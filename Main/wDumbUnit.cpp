#include "StdAfx.h"
#include "wDumbUnit.h"
#include "wUnitServer.h"     // NWorld::CUnitServer (Die's retail server arg -- cross-cast from this)
#include "wMain.h"
#include "wUICommands.h"     // BUG 5: NWorld::CUICmdUnitCamera (death-beauty auto-focus, ProcessAttack)
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "RPGGame.h"
#include "RPGItem.h"
#include "RPGUnit.h"
#include "RPGUnitMission.h"
#include "..\misc\RandomGen.h"
#include "GAnimation.h"
#include "InventoryUnit.h"
#include "wMisc.h"
#include "wObject.h"
#include "wAckBase.h"
#include "aiCollider.h"
#include "aiInterval.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataAI.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "aiMap.h"
#include "aiNearestPosition.h"   // NAI::GetNearestPosition (corpse logical-place re-snap @0x350960)
#include "aiLocker.h"
#include "GSceneUtils.h"
#include "..\MiscDll\Commands.h"
#include "..\Misc\EventsBase.h"
#include "eventUnit.h"
#include "RPGCritical.h" // for bleeding only
#include "wDecal.h"

namespace NWorld
{
static bool bEverybodyIsAlien = false;
static bool bIsGoldenShot = false;
bool bShowBlood = true;
static bool bFootball = false;   // retail "cheat_football" global: corpse-push always fires, with the 0.33/1.0 coeff split
static bool bForceHeadShot = false;   // retail @0x9c79d2 "cheat_heads_off": force the behead on any landed portion (console cheat, not saved)
static bool bAllowHeadshot = true;    // retail @0x982c79 "cheat_allow_headshot": gore sub-toggle ANDed with bShowBlood in the behead gate (default ON, saved)
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x34ef50 -- is the inventory's active item one to SHOW in the unit's hand? True iff there is a live active
// item whose DB record has bPlaceInHand set. Consumed by CExecMoveInventoryItem::AnimationFinished to decide the
// ActivateItem bHide arg (an item that is not "placed in hand" activates HIDDEN, using the unarmed animation set).
bool IsActiveItemToShow( NRPG::IInventoryInfo *pInv )
{
	NRPG::IInventoryItem *pItem = pInv->GetActive();
	return IsValid( pItem ) && pItem->GetDBItem()->bPlaceInHand;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LaunchItem( CWorld *pWorld, const CDumbUnitServer::SResItem &item, const CVec3 &vel, bool bFallFromBody, CObjectBase *pVisibilityParent, int nFloor )
{
	pWorld->AddDebris( item.pModel.GetPtr(), pWorld->GetAIMap(), item.ptCenter, item.q, vel, pWorld->GetTime(), bFallFromBody, pVisibilityParent, item.pItem, nFloor );
	// retail tail @0x74f594 (LaunchItem @0x34f500): pWorld->UpdateVisible(0) -- without this vision
	// recompute the in-flight CDItem is never LOS-revealed and stays hidden until physics-settle.
	pWorld->UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDumbUnitServer
////////////////////////////////////////////////////////////////////////////////////////////////////
CDumbUnitServer::CDumbUnitServer( CWorld *_pWorld, NRPG::IUnitMission *_pRPG, NDb::CModel *_pModel, const NAI::SUnitPosition &pos ): 
	animator( _pWorld->GetTime(), pos, _pModel->pSkeleton, _pWorld->GetAIMap(), _pRPG->GetRPGPers(), _pWorld ), 
	pWorld(_pWorld), bLocksTwoPlaces(false), bIsPKWhichIsWeared( false ), 
	bStrafe( false ), bJustUnhided( false ), nPrevFloor(1000), bHeadless( false )
{
	CPtr<NWorld::CDumbUnitServer> pHold(this);
	pRPG = _pRPG;
	pModel = _pModel;
	SetPositionCore( pos );
	wishPose = pos.GetPose();
	animator.SetWeaponAnimation( pRPG->GetWeaponType() );
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	if ( IsValid( pInventory->GetActive() ) )
		animator.SetActiveItem( true );
	animator.PlaceUnit( pos );
	pHold.Extract();
	bindGlobal.Link( pWorld->GetUnits(), this );
	bUndrawWeapon = false;
	bNoHeavyWeapon = false;
	// release scriptParticles / hand-attach members (ctor @0x7528d0). pHandModel/pHandEffect and
	// the two vectors default-construct empty/null; the PODs match the decoded ctor defaults.
	tBeginHandEffect = 0;
	bBloodyDeath = false;
	bCanHide = true;
	bTemporaryAimed = true;
	vPrevGetCorpseAIPosition = CVec3( -100.0f, -100.0f, -100.0f );
	bNotAddedToVisitors = false;
	bTrackSequence = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::IsEmptyPK() const
{
	return pRPG->GetRPGPers()->pPanzerklein;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::WearAsPK( bool bWear )
{
	if ( !IsEmptyPK() )
	{
		ASSERT(0);
		return false;
	}
	if ( bIsPKWhichIsWeared == bWear )
		return false;
	bIsPKWhichIsWeared = bWear;
	if ( !bWear )
	{
		PlaceOnPassablePlace();
		animator.SetPose( NAI::CRAWL );
		NAI::SUnitPosition pos = position;
		pos.SetPose( NAI::CRAWL );
		animator.PlaceUnit( pos );
	}
	else
	{
		pWorld->GetPathNetwork()->Unlock( this );
	}
	bindGlobal.Update();
	GetWorld()->UpdateVisible();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::GetUnitPositionForVisit( NAI::SUnitPosition *pPos )
{
	ASSERT( pPos != 0 );
	if ( pPos == 0 )
		return;
	//
	CUnit *pCarrier = animator.GetCorpseCarrier();
	if ( IsValid( pCarrier ) )
		*pPos = pCarrier->GetPosition();
	else
		*pPos = position;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDumbUnitServer::GetFloor()
{
	NAI::SUnitPosition pos;
	GetUnitPositionForVisit( &pos );
	int nFloor = pos.pos.GetFloor();
	// @0x34f610 -- retail only uses the dynamics floor for a plain (non-PK) unit; an
	// empty PK or a worn PK keeps its static position floor.
	if ( !CanFight() && !IsValid( animator.GetCorpseCarrier() ) )
		if ( !IsEmptyPK() && !IsWearingPK() )
			nFloor = animator.GetDynamicsFloor();
	return nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::IsAddedToVisitor()
{
	// @0x34ec20 -- retail also excludes a unit flagged OUT of the visitor set by the
	// wCheckTooMuchCorpses corpse-density failsafe (MarkNotAddedToVisitors). Converges
	// the dev no-op noted in wCheckTooMuchCorpses.cpp; behavior-neutral until
	// CheckTooMuchCorpses() is wired into CWorld::UpdateVisible.
	if ( bIsPKWhichIsWeared )
		return false;
	if ( bNotAddedToVisitors )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::Visit( IRenderVisitor *p )
{
	if ( !IsAddedToVisitor() )
		return;
	NAI::SUnitPosition pos;
	GetUnitPositionForVisit( &pos );
	int nFloor = GetFloor();
	//nFloor = 60; // invisible
	vector<IRenderVisitor::SBoundMesh> boundMeshes;
	// retail @0x352380: an empty PK shell never wears the cap; a downed unit loses it
	if ( IsEmptyPK() )
		GetItemsBindPlaces( &boundMeshes, pRPG, bUndrawWeapon, pRPG->GetRPGPers()->pPanzerklein, bNoHeavyWeapon, true );
	else if ( CanFight() || IsWearingPK() )
		GetItemsBindPlaces( &boundMeshes, pRPG, bUndrawWeapon, GetWearingDBPK(), bNoHeavyWeapon, !CanFight() );
	// release @0x752380 (UnitHoldItem -> SetHandModel): feed the script hand-held model to the render visitor on
	// the "Item" bind bone -- replace an existing "Item"-bound mesh's model if there is one, else add a new bound
	// mesh. (The release also carried a hand EFFECT here; the dev SBoundMesh has no effect field -> elided.)
	if ( IsValid( pHandModel ) )
	{
		bool bFound = false;
		for ( vector<IRenderVisitor::SBoundMesh>::iterator i = boundMeshes.begin(); i != boundMeshes.end(); ++i )
			if ( i->pszBindBone && strcmp( i->pszBindBone, "Item" ) == 0 )
			{
				i->pModel = pHandModel;
				bFound = true;
				break;
			}
		if ( !bFound )
			boundMeshes.push_back( IRenderVisitor::SBoundMesh( pHandModel, "Item" ) );
	}
	NGScene::CLightGroup *pGroup = p->MakeGroup();

	if ( bEverybodyIsAlien )
		p->StartAlienStyle(); // is hidden PK
	CUnit *pHead = dynamic_cast<CUnit*>(this);
	if ( pHead->GetWearingDBPK() && pHead->GetWearingDBPK()->bHasNoHead )
		pHead = 0;
	if ( bHeadless )
		pHead = 0;
	p->AddMesh( pModel, animator.GetSkeletonAnimator(), animator.GetSkeletonState(),
		boundMeshes, pGroup, nFloor, pHead, 0 );
	// release @0x752380 (AttachEffectToUnitBone): play each script-attached effect on the unit. The position
	// func is the root bone (the dev's own idiom -- cf. the hiding flare below) and the skeleton animator is
	// passed so the effect's glue-to-bone instances attach to the unit's bones.
	for ( vector<IRenderVisitor::SBoundEffect>::iterator i = attachedEffects.begin(); i != attachedEffects.end(); ++i )
		if ( IsValid( i->pEffect ) )
			p->AddParticleEffect( i->tBegin, i->pEffect, nFloor,
				new NAnimation::CAddBoneFilter( animator.GetSkeletonAnimator(), 0 ), animator.GetSkeletonAnimator() );
	if ( pRPG->IsHiding() ) // is hidden
	{
		p->AddFlare( 
			new NGScene::CExtractTranslation( new NAnimation::CAddBoneFilter( animator.GetSkeletonAnimator(), 0 ) ), 
			2, NDb::GetTexture(4369), nFloor, 0.3f, 0.3f
			);
		p->AddColorPostFilter( CVec4( 0,0,0,1 ) );
	}
	if ( bEverybodyIsAlien )
		p->FinishAlienStyle(); // is hidden PK
	nPrevFloor = nFloor;
	bTrackSequence = GetWorld()->IsSequence();   // @0x352380: render-path write-back for Segment's track-seq bug branch
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x6e96e0: append the effect to the unit's attached-effects list (Visit feeds it each frame). The
// release re-uses the last slot when one exists; a plain push_back is the same observable result.
void CDumbUnitServer::AttachEffect( STime tBegin, NDb::CEffect *pEffect )
{
	IRenderVisitor::SBoundEffect e;
	e.pEffect = pEffect;
	e.tBegin = tBegin;
	attachedEffects.push_back( e );
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::Visit( IAIVisitor *p )
{
	if ( !IsAddedToVisitor() )
		return;
	NAI::SUnitPosition pos;
	NDb::CRPGArmor *pArmor = pRPG->GetRPGArmor();
	if ( IsEmptyPK() )
		pArmor = pRPG->GetRPGPers()->pPanzerklein->pArmor;
	GetUnitPositionForVisit( &pos );
	pAIMapHull = p->AddAnimatedHull(
		pModel->pGeometry->pAIGeometry, pModel->pSkeleton, animator.GetSkeletonAnimator(),
		pArmor,
		GetFloor() /* @0x34f710 -- retail feeds the unit's own GetFloor (dynamics floor for corpses), not the raw path-net layer floor; equivalent for live units */,
		TS_UNITS|TS_FRAGMENTED|TS_PICK|TS_COVER|TS_WEAPON_BLOCKER );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x34fdd0: feed the unit's looping combat/engine sounds to the sound scene (CRenderSound syncs
// the union of GetActive()+GetUnits(), so live units are visited). Two CanFight()-guarded emits, each a
// root-bone-anchored looping 3D sound: (1) the worn PanzerKlein's engine loop, (2) a fixed hand effect.
void CDumbUnitServer::Visit( ISoundVisitor *p )
{
	if ( !CanFight() )
		return;
	NDb::CPanzerklein *pPK = GetWearingDBPK();
	if ( IsValid( pPK ) && IsValid( pPK->pEngineSound ) )
		p->Add3DSound( 0, pPK->pEngineSound,
			new NGScene::CExtractTranslation( new NAnimation::CAddBoneFilter( animator.GetSkeletonAnimator(), 0 ) ) );
	// release: hand-effect record 0x698 emits the fixed loop sound 0x4057 (bone-anchored to the root).
	if ( IsValid( pHandEffect ) && pHandEffect->GetRecordID() == 0x698 )
		p->Add3DSound( 0, NDb::GetSound( 0x4057 ),
			new NGScene::CExtractTranslation( new NAnimation::CAddBoneFilter( animator.GetSkeletonAnimator(), 0 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::GetBonePos( CVec3 *pRes, CQuat *pQuat, const char *pszBoneName )
{
	int nIndex = animator.GetSkeletonAnimator()->GetBoneIndex( pszBoneName );
	if ( nIndex < 0 )
		nIndex = 0; //return;
	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( animator.GetSkeletonAnimator(), nIndex );
	CDGPtr<CFuncBase<SFBTransform> > p( pFilter );
	p.Refresh();
	const SFBTransform &t = p->GetValue();
	*pRes = t.forward.GetTranslation();
	pQuat->FromEulerMatrix( t.forward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::TearOffItem( SResItem *pRes, NDb::ESlot slot, bool bPlaceNextSameItem )
{
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	pRes->pItem = pInventory->Get( slot );
	const char *pszBoneName = GetBoneName( slot, pRPG, bUndrawWeapon );
	bool bRes = false;
	if ( IsValid( pRes->pItem ) )
	{
		if ( pszBoneName && pszBoneName[0] != 0 && IsValid( pRes->pItem->GetDBItem()->pModel ) )
		{
			SRand rnd;
			pRes->pModel = pRes->pItem->GetDBItem()->pModel->CreateModel( &rnd );
			GetBonePos( &pRes->ptCenter, &pRes->q, pszBoneName );
			bRes = true;
		}

		pInventory->TakeOff( slot );

		if ( bPlaceNextSameItem && IsValid( pRes->pItem ) )
		{
			const vector<NRPG::SBackPackItem> &sItems = pInventory->GetItems();
			for ( int nTemp = 0; nTemp < sItems.size(); nTemp++ )
			{
				if ( sItems[nTemp].pItem->GetDBItem() == pRes->pItem->GetDBItem() )
				{
					CDynamicCast<NRPG::IGrenadeItem> pOldGrenade( pRes->pItem );
					CDynamicCast<NRPG::IGrenadeItem> pNewGrenade( sItems[nTemp].pItem );
					if ( IsValid( pOldGrenade ) && IsValid( pNewGrenade ) )
						pNewGrenade->SetMode( pOldGrenade->GetMode() );

					CObj<NRPG::IInventoryItem> pItem( sItems[nTemp].pItem );
					pInventory->Take( pItem );
					pInventory->Equip( slot, pItem );
					break;
				}
			}
		}
	}

	CPtr<NRPG::IInventoryItem> pItem = pInventory->GetActive();
	if ( IsValid( pItem ) )
	{
		NDb::EItemSubType subType = pItem->GetDBItem()->subType;
		int nPlace = pInventory->GetPlaceBySubType( subType );
		animator.ActivateItem( position, false, nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType() );
	}

	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::SetPositionCore( const NAI::SUnitPosition &dst )
{
	animator.SetPose( dst.GetPose() );
	position = dst;
	// retail @0x34f970: if this unit is carrying a corpse, drag the corpse's cached position with it so the
	// corpse's AI-map hull + last-known position (used by the AI corpse-pickup logic) track the carrier.
	CDumbUnitServer *pCorpse = GetCorpse();
	if ( pCorpse )
	{
		pCorpse->position = dst;
		pCorpse->vPrevGetCorpseAIPosition = dst.pos.GetCP();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::LockNextPlace( const NAI::SUnitPosition &dst )
{
	if ( CanFight() )
	{
		bLocksTwoPlaces = true;
		nextLock = dst.pos.p;
		pWorld->GetPathNetwork()->LockMovingObject( this, position.pos.p, dst.pos.p );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x34f430 -- where a stance change should land: mid-step in realtime (bLocksTwoPlaces) the unit is
// already committed to nextLock, so the pose-change CCmdPath must target it, not the tile being left.
NAI::SUnitPosition CDumbUnitServer::GetUnitSetPosePosition() const
{
	NAI::SUnitPosition res = position;
	if ( bLocksTwoPlaces && !pWorld->IsTurnBased() )
		res.pos.p = nextLock;
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::IsLocker()
{
	return CanFight() || ( IsEmptyPK() && !bIsPKWhichIsWeared ) || IsWearingPK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::SetPosition( const NAI::SUnitPosition &dst )
{
	bool bNeedUpdate = dst.pos.GetFloor() != position.pos.GetFloor();
	bool bRealMove = false;
	if ( position.GetPose() == dst.GetPose() && position.GetDir() == dst.GetDir() ) // the actual movement
			bRealMove = true;

	SetPositionCore( dst );
	if ( IsLocker() )
	{
		bLocksTwoPlaces = false;
		pWorld->GetPathNetwork()->Lock( this, position.pos.p );
		
		// check mines
		vector<NAI::SPathPlace> lockTiles;
		NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
		pNet->GetLockArea( &lockTiles, position.pos.p, NAI::IsBigLocker( this ) );
		vector<CVec3> points;
		for ( int k = 0; k < lockTiles.size(); ++k )
		{
			NAI::SPosition pos( lockTiles[k], pNet );
			points.push_back( pos.GetCP() );
		}
		vector<CPtr<CMine> > mines;
		GetWorld()->GetMineTracker()->GetMines( points, &mines );
		TouchedMines( mines );
	}
	//
	pWorld->UpdateVisible();

	if ( bRealMove )
		MakeStepSound( false );
	if ( bNeedUpdate )
	{
		bindGlobal.Update();
		CPtr<CDumbUnitServer> pCorpse = GetCorpse();
		if ( IsValid( pCorpse ) )
			pCorpse->Update();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::AddMiscObjects( vector<IVisObj*> *pRes )
{
	for ( list<CObj<IDynamicObject> >::const_iterator i = miscObjects.begin(); i != miscObjects.end(); ++i )
		pRes->push_back( CDynamicCast<IVisObj>( i->GetPtr() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::AttachMiscObject( CTimedObject *p )
{
	p->Attach( pWorld->GetUnits(), pWorld );
	miscObjects.push_back( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::PlaySound( NDb::CTSound *pSound )
{
	NDb::CSound *pS = NDb::GetSound( pSound );
	if ( pS )
		AttachMiscObject( Create3DSound( position.GetEyePosition(), pS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::DropItems( bool bDropHands, bool bDropCap, bool bDropBackPack )
{
	SResItem item;
	CPtr<NRPG::IInventoryInfo> pInventoryInfo = pRPG->GetInventoryInfo();
	// retail DropItems @0x3502c0 split the old Jan03 bHands flag into two independent flags -- bDropCap (the
	// uniform cap, dropped FIRST) and bDropHands (the in-hand/slot items) -- plus the unchanged bDropBackPack.
	// Every drop launches with bFallFromBody=true, the dying unit as the fog-gate visibility parent
	// (parent rides the CDItem so the gate survives physics-settle) and the unit's floor (GetFloor
	// @0x34f610, hoisted once @0x750309); LaunchItem's UpdateVisible(0) tail LOS-reveals the flying
	// item to any player that already sees the unit.
	const int nFloor = GetFloor();
	// drop the uniform cap
	if ( bDropCap )
	{
		NDb::CRPGUniform *pDBUniform = pInventoryInfo->GetUniform();
		SRand rnd;
		if ( pDBUniform && pDBUniform->pCapModel )
		{
			GetBonePos( &item.ptCenter, &item.q, GetBoneName( UIT_CAP, 0, IsWearingPK() ) );
			item.pModel = pDBUniform->pCapModel->CreateModel(&rnd);
			LaunchItem( pWorld, item, VNULL3, true, (CObjectBase*)this, nFloor );   // retail @0x750401 push 1
		}
	}
	// drop the in-hand / slotted items
	if ( bDropHands )
	{
		for ( int nSlot = 0; nSlot != NDb::N_SLOTS; ++nSlot )
			if ( TearOffItem( &item, (NDb::ESlot)nSlot ) )
			{
				CVec3 vInitial( random.GetFloat( -0.3f, 0.3f ), random.GetFloat( -0.3f, 0.3f ), 2 );
				LaunchItem( pWorld, item, vInitial, true, (CObjectBase*)this, nFloor );   // retail @0x7504f3
			}
	}
	// drop the backpack items
	if ( bDropBackPack )
	{
		const vector<NRPG::SBackPackItem> &sItems = pInventoryInfo->GetItems();
		for ( int n = 0; n < sItems.size(); ++n )
		{
			item.pItem = sItems[n].pItem;
			const char *pszBoneName = GetBoneName( UIT_BACKPACK, item.pItem );
			if ( IsValid( item.pItem ) && pszBoneName &&
				pszBoneName[0] != 0 && IsValid( item.pItem->GetDBItem()->pModel ) )
			{
				SRand rnd;
				item.pModel = item.pItem->GetDBItem()->pModel->CreateModel( &rnd );
				GetBonePos( &item.ptCenter, &item.q, pszBoneName );
				pRPG->GetInventory()->Take( item.pItem );
				CVec3 vInitial( random.GetFloat( -0.3f, 0.3f ), random.GetFloat( -0.3f, 0.3f ), 2 );
				LaunchItem( pWorld, item, vInitial, true, (CObjectBase*)this, nFloor );   // retail @0x750688
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::Hide( bool bHide, bool bThrowEvent )
{
	// retail @0x34fb10: you cannot RE-hide until bCanHide is re-armed (OnNewPlayerFastTurn). One unhide disarms it.
	if ( bHide && !bCanHide )
		return;
	bool wasHiding = GetUnitRPG()->IsHiding();   // capture BEFORE SetHiding
	GetUnitRPG()->SetHiding( bHide );
	if ( !bHide )
	{
		bCanHide = false;      // disarm re-hiding until the next fast-turn re-arms it
		bJustUnhided = true;
		if ( bThrowEvent && wasHiding )   // reveal event only when actually leaving hiding (voluntary/death paths pass false)
			NGlobal::ThrowEvent( CEventOnUnitUnhide( this ) );
	}
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::EnableHide()
{
	// retail @0x34ed00: re-arm hiding on the false->true transition (the world UINeedUpdate UI dirty-bit is a
	// cosmetic no-op absent from this build, so it is elided).
	if ( !bCanHide )
		bCanHide = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::FallAsIfDead( const CVec3 &ptDir, bool bDropItemsFromBackPack, bool bPlayDeathAnim )
{
	DropItems( !IsWearingPK(), true, bDropItemsFromBackPack );   // retail @0x350770 always drops the cap here (bDropCap=true)
	//
	if ( GetUnitRPG()->IsHiding() )
		Hide( false, false );   // retail FallAsIfDead @0x350770: a dying unit unhides WITHOUT throwing the reveal event
	//
	// retail @0x350770 (disasm 0x7507c3): a falling corpse-CARRIER lets go of the corpse it hauls --
	// vtbl+0x30 GetCorpse() -> CUnitAnimator::BeDropped @0x33b1b0 + the corpse's vis-sync Update.
	// Without this the carried body stays glued to the dead carrier's back forever.
	CDumbUnitServer *pCarried = GetCorpse();
	if ( IsValid( pCarried ) )
	{
		// retail @0x7507cc pushes the corpse pointer (the GetCorpse result) as BeDropped's server arg;
		// the carried corpse is always a CUnitServer (narrowed to CDumbUnitServer* by the GetCorpse vtable).
		pCarried->animator.BeDropped( static_cast<CUnitServer*>( pCarried ) );
		pCarried->Update();
	}
	//
	if ( !IsWearingPK() )
	{
		// retail @0x350770 gates the death animation on the 3rd arg -- a combat (non-script) knock-out
		// plays NOTHING here; its ragdoll fall arrives from the ProcessAttack corpse-push (@0x350e20).
		// Retail passes a VNULL3 direction (disasm 0x7507ff..0x750818: the CRay is filled from ::VNULL3);
		// the push direction only ever enters via the bPlayDeath=false entries.
		if ( bPlayDeathAnim )
			animator.Die( position, VNULL3, true, CDynamicCast<CUnitServer>( this ) );   // retail passes the dying server @0x75081e
		pWorld->GetPathNetwork()->Unlock( this );
	}
	else
	{
		animator.SetPose( NAI::CRAWL );
		NAI::SUnitPosition animPos = position;
		animPos.pos.p.SetPose( NAI::CRAWL );
		animator.PlaceUnit( animPos );
	}
	bindGlobal.Update();
	pWorld->UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::KillUnit( const CVec3 &ptDir )
{
	if ( IsDead() )
		return;
	//
	bool bUnconscious = IsUnconscious();
	Die(); // UnitServer callback
	//
	if ( !bUnconscious )
	{
		FallAsIfDead( ptDir, true, true );   // retail @0x350cd0: killed -> drop backpack + play death anim
		PlaySound( pRPG->GetRPGPers()->pSoundDeath );
	}
	else
		DropItems( false, false, true );     // retail @0x350cd0: unconscious->killed drops only the backpack
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::MakeUnconscious( const CVec3 &ptDir, bool bFromScript )
{
	if ( !CanFight() )
		return;
	//
	OnUnitMadeUnconscious( bFromScript ); // UnitServer callback
	//
	FallAsIfDead( ptDir, false, bFromScript );   // retail @0x350dc0: play the death anim only for a scripted knock-out
	if ( !bFromScript )
		PlaySound( pRPG->GetRPGPers()->pSoundDeath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::BlowUp()
{
	SRand rnd;
	DropItems( true, !IsUnconscious(), true );   // retail @0x350890: an already-unconscious unit blowing up keeps its cap
	CVec3 vPos = GetUnitPosition().GetCP() + CVec3(0,0,1);
	pWorld->CreateBloodyMess( vPos, CVec3(0,0,0), this, 30 );
	pWorld->CreateParticle( vPos, QNULL, NDb::GetTEffect( 822 )->GetEffect( &rnd ) );
	RemoveFromWorld();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x350e20 -- the seam between the two ProcessAttack contracts: this is IAttackable's
// 5-arg form, and it forwards to IUnitMission's 4-arg form (which has no CVec3) on pRPG.
// retail @0x350e20 calls AddHitLocator/CreateBloodyMess on this->pWorld (member 44, disasm
// `mov ecx,[esi+0x24]`) but forwards the INCOMING world to pRPG -- hence _pWorld, which must not
// shadow the pWorld member the body below uses.
int CDumbUnitServer::ProcessAttack( NWorld::IWorld *_pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	if ( !IsValid(this) )
		return 0;
	if ( bIsGoldenShot )
	{
		BlowUp();
		return 0;
	}
	SRand rnd;
	int nWasVP = pRPG->GetTotalVP();
	// push the per-mission RPG game down to the target's mission so the combat critical clamp can honor
	// CGame::nMaxCriticalSeverity (luaSetMaxCriticalSeverity). Retail cached the game in the mission ctor;
	// this dev fork dropped that ctor arg, so we (re)bind it here at the attack entry.
	pRPG->SetGame( pWorld->GetGame() );
	int nRes = pRPG->ProcessAttack( _pWorld, nUserID, pAttack, pArmor );
	// retail gib gate @0x750ef1..0x750f4c: bShowBlood && nRes > 120 && CanBlowUp() (unit
	// vtbl+0x28, @0x3c0420) && !pAttack->bNoBlowUp -- and a still-living unit DIES first
	// (Die(1,0) @0x750f25: the FULL death path with a FORCED slow-mo beauty cam) before the gib.
	// Only a Panzerklein pilot's melee portion carries bNoBlowUp=false (CreateAttack
	// @0x6c264c), so bullets/explosion portions never gib a unit through this path.
	if ( bShowBlood && nRes > 120 && CanBlowUp() && !pAttack->bNoBlowUp )
	{
		if ( !IsDead() )
			Die( true );   // bDeathBeauty=true, bRemove=false (retail push 0; push 1 @0x750f25)
		BlowUp();
		return 0;
	}

	NRPG::CUnit *pRPGUnit = pRPG->GetRPGUnit();
	int nHalfVP = Float2Int( 0.5f * pRPGUnit->Skills( NDb::ST_VP ).GetMaxValue() );
	int nCurrentVP = pRPG->GetTotalVP();
	if ( nCurrentVP < nHalfVP )
	{
		float fStrength = ( Min( nWasVP, nHalfVP ) - nCurrentVP ) / 10.0f;
		NRPG::SCritical suffer( NDb::CL_ANY, NDb::C_BLEEDING, -1, fStrength );
		pRPG->ApplyCritical( suffer );
	}

	if ( !IsDead() )
	{
		CVec3 ptHit;
		pWorld->GetAIMap()->GetUnitHLPos( &ptHit, pWorld->GetAIMap()->GetHull(this), NAI::HL_HEAD );
		// retail @0x350e20: CHitLocator(nRes, bPK, ptHit, unit) -- bPK = the RPG damage receiver was a
		// Panzerklein (CReceivedDmg.type == RD_PK). Dev's ProcessAttack still returns a plain int, so
		// the receiver type is derived here: pers-is-PK, or the worn PK ricocheted/blocked (-1 while
		// wearing -- the human dodge path is gated on !GetPanzerklein(), so no overlap).
		bool bPK = IsValid( pRPG->GetRPGPers()->pPanzerklein ) ||
			( IsValid( pRPG->GetPanzerklein() ) && nRes < 0 );
		pWorld->AddHitLocator( new CHitLocator( nRes, bPK, ptHit, CDynamicCast<CUnit>( (CObjectBase*)this ) ) );
		PlaySound( pRPG->GetRPGPers()->pSoundHit );

		if ( nRes > 5 )
		{
			pWorld->GetAIMap()->GetUnitHLPos( &ptHit, pWorld->GetAIMap()->GetHull(this), nUserID );
			pWorld->CreateBloodyMess( ptHit, pAttack->rTtrajectory.ptDir, this, 1 );
		}
	}

	vector<NDb::ECritical> criticals;
	pRPG->GetLastCriticals( &criticals );

	// retail @0x7511dc: the whole behead section is additionally gated on CanBlowUp() (unit
	// vtbl+0x28) -- a quest-clue corpse or a PK (worn/empty) never loses its head. Two entry
	// branches (@0x7511ec / @0x751217): the "cheat_heads_off" FORCED behead (any landed portion
	// on a living, conscious, not-yet-headless unit -- regardless of hit location/VP/gore flags;
	// its failure falls through into the regular branch) OR the regular gore path, which ANDs the
	// "cheat_allow_headshot" sub-toggle with bShowBlood (censored builds keep blood but disable
	// decapitations via a config setvar).
	if ( CanBlowUp() )
	{
		const bool bForced = bForceHeadShot && !IsDead() && !pRPG->IsUnconscious() && !bHeadless;
		const bool bAllowed = bAllowHeadshot && bShowBlood && !IsDead() && nUserID == NAI::HL_HEAD
			&& !criticals.empty() && !bHeadless && nCurrentVP <= 0;
		if ( bForced || bAllowed )
		{
			CVec3 vVel = pAttack->rTtrajectory.ptDir;
			if ( fabs2( vVel ) > 0 )
			{
				Normalize( &vVel );
				vVel *= 2;
			}
			else
				vVel = CVec3( 2, 0, 1 );
			CWorld *pWorld = GetWorld();
			CVec3 vHeadPos;
			CQuat qPos;
			GetBonePos( &vHeadPos, &qPos, "Head" );
			//pWorld->AddDebris( dynamic_cast<CUnit*>(this), pWorld->GetAIMap(), vHeadPos, qPos, vVel, pWorld->GetTime() );
			AttachMiscObject( CreateDParticles( vHeadPos, qPos, NDb::GetTEffect( 821 )->GetEffect( &rnd ), GetFloor() ) );
			bHeadless = true;
			// ORIGINAL BUG (confirmed retail @0x350e20): the behead flag is written as a BYTE into the
			// LOW BYTE of animator.fDeathFall (`*(char*)&fDeathFall = 1` -> the float becomes the 1.4e-45
			// denormal, which is still > 0.0f), so a beheaded unit's Die @0x33bb90 takes the falling-death
			// SetMove branch with a ~zero fall height. Reproduced bit-exactly (the guards keep reading the
			// dev bHeadless mirror, exactly as they read the byte in retail).
			*(unsigned char *)&animator.fDeathFall = 1;
			// retail @0x7512b9: RPG-kill BEFORE KillUnit -- required on the forced path (a full-VP
			// unit must become RPG-dead or the pRPG->IsDead() reconciliation below won't fire).
			pRPG->Kill();
			KillUnit( pAttack->rTtrajectory.ptDir );
		}
	}

	bool bDied = false;   // retail @0x350e20: set by BOTH the kill and the knock-out dispatch (beauty-cam gate)
	if ( pRPG->IsDead() && !IsDead() )
	{
		KillUnit( pAttack->rTtrajectory.ptDir );
		bDied = true;
	}
	else if ( pRPG->IsUnconscious() && !IsUnconscious() )
	{
		MakeUnconscious( pAttack->rTtrajectory.ptDir );
		bDied = true;
	}
	else
	{
		animator.Wound();

		NRPG::SUnitInfo sInfo;
		GetUnitRPG()->GetInfo( NAI::WALK, &sInfo );
		OnSuffersDamage( float(sInfo.nHP) / sInfo.nMaxHP );
	}

	// retail @0x350e20 step 8 (the CORPSE PUSH -- was missing here entirely): push the downed body
	// with the shot direction via the clipless animator Die (bPlayDeath=false). This is the ONLY
	// ragdoll fall a combat knock-out ever gets (MakeUnconscious/FallAsIfDead play nothing), and the
	// push a fresh corpse gets on top of its death clip. Skipping it left KO'd units FROZEN standing
	// in their last pose -- and a later KillUnit on an unconscious unit takes the DropItems-only
	// branch, freezing the corpse for good (the reported frozen-corpse-at-old-place bug).
	// Gates (disasm 0x75138c..0x7513fa): persona !IsAlive; GetPushCorpseCoeff(pAttack) > 0 (or
	// cheat_football) -- NOT fabs2(dir) > 0; not a worn PK shell; persona not a panzerklein; not
	// being carried (animator.bIsCarried @+103).
	// ‼️ retail @0x75138c stores the GetPushCorpseCoeff return (fstp [esp+0x10], dropped by Ghidra)
	// and, non-football, multiplies the normalized direction by IT (0x75144f..0x751458) -- the
	// weapon-scaled fling. The previous flat fCoeff = 1.0f was the "same force for every weapon" bug.
	{
		float fCoeff = pAttack->GetPushCorpseCoeff();   // retail @0x28f860 over the bullet's fPushCoeff
		bool bDown = IsDead() || IsUnconscious();
		if ( bDown && ( bFootball || fCoeff > 0 ) && !IsWearingPK() &&
		     pRPG->GetRPGPers()->pPanzerklein == 0 && !animator.bIsCarried )
		{
			CVec3 vPush = pAttack->rTtrajectory.ptDir;
			Normalize( &vPush );
			if ( bFootball )
				fCoeff = pAttack->GetPushCorpseCoeff() <= 0 ? 0.33f : 1.0f;   // retail 0x75141d..0x75144f
			vPush *= fCoeff;
			// BUG 5 (auto-focus): retail ProcessAttack @0x350e20 posts CUICmdUnitCamera(dyingUnit,
			// PR_UNIT_DIED_BEAUTY, useSloMo=true, prob=(coeff<=0.5?1:2*coeff+0.5), null) for a killed/KO'd
			// pushed corpse -- the slow-mo death "beauty shot". CDynamicCast<CUnit> because CUnit is a SIBLING
			// base of CUnitServer (not a parent of this CDumbUnitServer), so `this` needs a cross-cast.
			if ( bDied )
			{
				float t = fCoeff <= 0.5f ? 1.0f : ( fCoeff + fCoeff + 0.5f );
				NWorld::CUnit *pDeadCUnit = CDynamicCast<NWorld::CUnit>( this );
				GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( pDeadCUnit,
					NWorld::PR_UNIT_DIED_BEAUTY, true, t, 0 ) );
			}
			// Impact point = the HIT-LOCATION bone's world position (nUserID is the EHitLocation of
			// this portion). CParticleSkeleton::Init's falloff (2-|particle-impact|) then puts the
			// strongest impulse where the shot landed -> a head shot flings the head, a leg shot the
			// legs, instead of every corpse tilting forward off its feet (the generic-GetCP fallback).
			const char *pszHitBone;
			switch ( nUserID )
			{
				case NAI::HL_HEAD:  pszHitBone = "Head";     break;
				case NAI::HL_RHAND: pszHitBone = "R_UArm";   break;
				case NAI::HL_LHAND: pszHitBone = "L_UArm";   break;
				case NAI::HL_RLEG:  pszHitBone = "R_Thigh";  break;
				case NAI::HL_LLEG:  pszHitBone = "L_Thigh";  break;
				default:            pszHitBone = "Spine";    break;   // HL_BODY / HL_ANY
			}
			CVec3 vImpact;
			CQuat qImpact;
			GetBonePos( &vImpact, &qImpact, pszHitBone );
			animator.Die( position, vPush, false, CDynamicCast<CUnitServer>( this ), &vImpact );   // retail @0x751511
		}
	}

	if ( IsDead() )
		return nRes;

	for ( int k = 0; k < criticals.size(); ++k )
		ProcessCritical( criticals[k] );
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::Segment()
{
	// ORIGINAL BUG (confirmed retail @0x350960): inside the corpse-like gate, TWO branches re-push the vis-binding
	// (Update()) on a cached-vs-live mismatch but NEVER write the new value back here -- the write-back lives in the
	// render path (Visit @0x352380). So a corpse-like unit that is never rendered re-pushes every tick. Faithful --
	// do NOT add the write-backs here. Branch (A) is the floor change; branch (B) is the track-sequence flag, gated
	// on CWorld::IsSequence @0x376ff0 (the interrupt/real-time predicate, delegated to CTBSWorld::IsSequence and
	// exposed as an IWorld default-no-op virtual). bTrackSequence is written back in Visit above.
	// Retail's full corpse gate (disasm @0x350960): !CanFight && not a Panzerklein pers && no corpse
	// carrier. Inside it, BEFORE the Update branches, the corpse hit-location refresh: when the physical
	// body drifted > sqrt(2) from the last snapshot (or the points were never filled), re-snap the logical
	// place to the body (NAI::GetNearestPosition @0x7ef50) and rebuild the 6 corpseHLpos ray points from
	// the AI-map hull (CAIMap::GetUnitHLPos @0x65ee0, retail fill order 1,0,3,2,5,4). These points feed
	// CGame::IsCorpseVisible @0x298da0 -- the tracker's corpse-sighting probe.
	if ( !CanFight() && pRPG->GetRPGPers()->pPanzerklein == 0
	     && !IsValid( animator.GetCorpseCarrier() ) )
	{
		CVec3 ptReal;
		GetRealUnitPosition( &ptReal );
		if ( fabs2( ptReal - vPrevGetCorpseAIPosition ) > 2.0f || corpseHLpos.empty() )
		{
			position.pos = NAI::GetNearestPosition( ptReal, position.pos.GetNetwork() );
			vPrevGetCorpseAIPosition = ptReal;
			corpseHLpos.resize( 6 );
			NAI::IAIMap *pMap = pWorld->GetAIMap();
			CObjectBase *pHull = pMap->GetHull( this );
			static const int order[6] = { 1, 0, 3, 2, 5, 4 };   // retail fill order @0x750ac0..0x750b4f
			for ( int k = 0; k < 6; ++k )
				pMap->GetUnitHLPos( &corpseHLpos[k], pHull, order[k] );
		}
		if ( GetFloor() != nPrevFloor )
			Update();
		if ( bTrackSequence != GetWorld()->IsSequence() )
			Update();
	}
	bJustUnhided = false;
	CallSegment( &miscObjects );
	ProcessSteps( GetWorld()->GetTime()->GetValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// performs move from game logic`s point of view
// returns false if movement should be aborted after this transition
void CDumbUnitServer::DoGameMove( const NAI::SUnitPosition &dst )
{
///	char buf[200];
//	NWorld::CDumbUnitServer *pAddr = this;
//	sprintf( buf, "DoGameMove : unitid %d, x=%d, y=%d, l=%d, p=%d\n", pAddr, dst.pos.p.GetX(), dst.pos.p.GetY(), dst.pos.p.GetLayer(), dst.pos.p.GetPose());
//	OutputDebugString( buf);

	SetPosition( dst );
	AttachMiscObject( CreateDGrassEvent( dst.GetCP() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ECanMoveRes CDumbUnitServer::CheckPassable( const NAI::SUnitPosition &dst )
{
	// @0x34efd0 -- query the granular EPassable verdict for dst, optionally re-check the whole lock
	// footprint (big lockers, or a move INTO the LAY pose: dst nPose==CM_LAY, the 0xC0-masked top pose
	// bits == 0 -- disasm-confirmed at 0x74f024, and the footprint place is dst, NOT position.pos.p),
	// then re-lock the unit's own place and map EPassable -> ECanMoveRes:
	//   AIP_NOT_PASSABLE/AIP_CANNOT_LAY -> CMR_NOT_PASSABLE, AIP_LOCKED -> CMR_LOCKED,
	//   AIP_DOOR -> CMR_DOOR, AIP_YES -> CMR_YES.
	NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
	pNet->Unlock( this );
	NAI::EPassable e = pNet->GetPassability( dst.pos.p );
	if ( e == NAI::AIP_YES &&
	     ( NAI::IsBigLocker( this ) || dst.pos.p.GetPose() == NAI::CM_LAY ) )
		e = NAI::BigLockerPassableState( pNet, dst.pos.p );
	if ( IsLocker() )
	{
		if ( bLocksTwoPlaces )
			pNet->LockMovingObject( this, position.pos.p, nextLock );
		else
			pNet->Lock( this, position.pos.p );
	}
	switch ( e )
	{
		case NAI::AIP_NOT_PASSABLE:
		case NAI::AIP_CANNOT_LAY:  return CMR_NOT_PASSABLE;
		case NAI::AIP_LOCKED:      return CMR_LOCKED;
		case NAI::AIP_DOOR:        return CMR_DOOR;
		default:                   return CMR_YES;   // AIP_YES
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ECanMoveRes CDumbUnitServer::CanDoGameMove( const NAI::SUnitPosition &dst )
{
	// @0x34f100 -- res = CheckPassable(dst); if passable, res = (ECanMoveRes)(CanMove()==0)
	// i.e. CMR_YES iff both hold, else CMR_CANNOT_MOVE(1); otherwise the CheckPassable verdict.
	ECanMoveRes res = CheckPassable( dst );
	if ( res == CMR_YES && !pRPG->CanMove() )
		res = CMR_CANNOT_MOVE;
	return res;
	//CanSpendAP( pWorld->GetMoveActionType( position, dst, animator.IsCarryingCorpse() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::PlaceOnPassablePlace()
{
	if ( !IsLocker() )
		return;
	bool bBigUnit = NAI::IsBigLocker( this );
	NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
	pNet->Unlock( this );
	bool bPassable = bBigUnit? NAI::IsBigLockerPassable( pNet, position.pos.p ) : pNet->IsPassable( position.pos.p );
	if ( bPassable )
	{
		bLocksTwoPlaces = false;
		pNet->Lock( this, position.pos.p );
		return;
	}
	CVec3 cp = position.GetCP();
	float fRadius = 1;
	int nStartFloor = pNet->GetFloor( position.pos.p.GetLayer() );
	for( int i = 0; i < 4; ++i, fRadius *= 2 )
	{
		vector<NAI::SPathPlace> places;
		pNet->GetNearPlaces( SSphere( cp, fRadius ), &places );
		float fBest = 1e38f;
		NAI::SPathPlace best;
		for ( int k = 0; k < places.size(); ++k )
		{
			places[k].SetPose( NAI::CM_STAND );
			bool bPassable = bBigUnit? NAI::IsBigLockerPassable( pNet, places[k] ) : pNet->IsPassable( places[k] );
			if ( bPassable && pNet->GetFloor( places[k].GetLayer() ) <= nStartFloor )
			{
				NAI::SPosition pos( position.pos );
				pos.p = places[k];
				float fDist = fabs2( cp - pos.GetCP() );
				if ( fDist < fBest )
				{
					fBest = fDist;
					best = places[k];
				}
			}
		}
		if ( fBest != 1e38f )
		{
			NAI::SPathPlace p = best;
			p.SetPose( position.pos.p.GetPose() );
			if ( IsEmptyPK() )
				p.SetPose( NAI::CRAWL );
			p.SetDirection( position.pos.p.GetDirection() );
			NAI::SUnitPosition newPos( position );
			newPos.pos.p = p;
			animator.PlaceUnit( newPos );
			SetPosition( newPos );
			return;			
		}
	}
	ASSERT( 0 ); // no suitable place was found
	OutputDebugString( "impossible to place unit correctly\n" );
	bLocksTwoPlaces = false;
	pNet->Lock( this, position.pos.p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x351630: retail is CreateFlash(bLeft, bFirstBullet). bLeft picks the L_/R_Weapon barrel bone on a dual-mount
// (headless) PK; bFirstBullet gates the burst sound so it emits ONCE at the start of a burst (short -> pSoundBurst,
// long -> pSoundCycleBurst) instead of the Jan03 GetNBullets()==1 gate. Retail returns the created
// C3DSound* -- CExecShoot retains it in its SLongBurstSnd slot (save tag 3) and EndSound()s it when
// the long burst stops.
C3DSound* CDumbUnitServer::CreateFlash( bool bLeft, bool bFirstBullet )
{
	NAnimation::SBonePose barrel;
	bool bTest = false;
	NRPG::IWeaponItem *pRPGWeapon;
	if ( animator.GetCannon() )
	{
		NRPG::IWeaponItem *pCannonItem = animator.GetCannon()->GetItem();
		if ( !pCannonItem )
			return 0;
		pRPGWeapon = pCannonItem;
	}
	else {
		CDynamicCast<NRPG::IWeaponItem> pW(pRPG->GetInventory()->GetActive());
		if (pW)
			pRPGWeapon = pW;
		else
			return 0;
	}
	NDb::CRPGWeapon *pWeapon = pRPGWeapon->GetDBWeapon();
	ASSERT( pWeapon );
	bTest = animator.GetBarrelPos( animator.GetCannon() ? 0 : pWeapon->GetModel()->pGeometry, &barrel, bLeft );
	ASSERT( bTest );
	if ( pWeapon->pShotEffect )
	{
		CQuat rndX( random.GetFloat( - FP_PI / 8, FP_PI / 8 ), CVec3(1,0,0) );
		SRand rnd;
		AttachMiscObject( CreateDParticles( barrel.pos, barrel.rot * rndX, pWeapon->pShotEffect->GetEffect( &rnd ), GetFloor() ) );
	}
	NDb::CSound *pSound = 0;
	NDb::CAISound *pAISound = 0;
	switch ( pRPGWeapon->GetShootMode() )
	{
		case NDb::SM_Snap:
		case NDb::SM_Aimed:
		case NDb::SM_Careful:
		case NDb::SM_Snipe:
			pSound = pWeapon->pSound;
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pAISound;
			break;
		case NDb::SM_ShortBurst:
			if ( !bFirstBullet )   // burst sound emits once, at the first bullet
				return 0;
			pSound = pWeapon->pSoundBurst;
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pBurstAISound;
			break;
		case NDb::SM_LongBurst:
			if ( !bFirstBullet )
				return 0;
			pSound = pWeapon->pSoundCycleBurst;   // long burst -> the cycling burst sound
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pBurstAISound;
			break;
	}
	return pWorld->MakeAISound( pAISound, this, 0, pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDumbUnitServer::GetActionAP( NRPG::EAction action ) const
{
	return pRPG->GetActionAP( GetUnitPosition().GetPose(), action ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDumbUnitServer::GetAP() const
{
	return pRPG->GetAP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::CanSpendAP( int nAP ) const
{
	if ( pWorld->IsRealTime() )
		return true;
	return pRPG->CanSpendAP( nAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::SpendAP( int nAP )
{
	if ( pWorld->IsRealTime() )
		return;
	pRPG->SpendAP( nAP );
	//NRPG::IUnitMission *pRPG = GetRPGUnitMission();
/*	int nRet = pRPG->GetActionAP( GetUnitPosition().GetPose(), action, nMaxRequiredAP );
	if ( !pWorld->IsRealTime() )
		pRPG->SpendAP( GetUnitPosition().GetPose(), action, nMaxRequiredAP );
	else
		nRet = nMaxRequiredAP;
	return nRet;*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::DoAction( NRPG::EAction action )
{
	SpendAP( GetActionAP( action ) );
	pRPG->RegisterAction( action );
	// retail @0x34edb0: only a RUNNING move counts toward nMoveInLastTurn (the shooting move-penalty);
	// side-step costs 2, diagonal 1. Walk/crouch/crawl moves no longer inflate it.
	if ( position.GetPose() == NAI::RUN )
	{
		if ( action == NRPG::AC_MOVE_SIDE )
			pRPG->AddMoveInLastTurn( 2 );
		else if ( action == NRPG::AC_MOVE_DIAGONAL )
			pRPG->AddMoveInLastTurn( 1 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGArmor* CDumbUnitServer::GetArmor()
{
	CVec3 point = position.GetCP();
	point.z += 0.2f;
	return pWorld->GetArmor( point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CAISound *CDumbUnitServer::GetStepAISound()
{
	int nID = 0;
	if ( IsStrafing() )
	{
		nID = 7;
	}
	else
	{
		switch ( position.GetPose() )
		{
		case NAI::CRAWL:
			nID = 3; break;
		case NAI::CROUCH:
			nID = 4; break;
		case NAI::WALK:
			nID = 5; break;
		case NAI::RUN:
			nID = GetUnitRPG()->HasPerk( 0x29 ) ? 5 : 6; break;   // @0x34ee90: quiet-run perk 0x29 -> walk-step AI sound
		}
	}
	return NDb::GetAISound( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CSound* CDumbUnitServer::GetStepSound( NDb::CRPGArmor *pArmor )
{
	if ( position.GetPose() == NAI::CRAWL )
		return 0;

	SRand rand;
	if ( pArmor && pArmor->pSoundStep )
		return pArmor->pSoundStep->GetSound( &rand )->pSound;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::MakeStepSound( bool bSound )
{
	NDb::CAISound *pAISound = GetStepAISound();
	NDb::CSound *pSound = 0;

	NDb::CRPGArmor *pArmor = GetArmor();

	if ( bSound )
		pSound = GetStepSound( pArmor );
	int nAISoundType = 0;
	if ( pArmor )
		nAISoundType = pArmor->nAISoundType;

	pWorld->MakeAISound( pAISound, this, nAISoundType, pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::ProcessSteps( const STime tCurrent )
{
	STime tStep;
	while ( animator.GetStepTime( &tStep ) && tCurrent >= tStep )
	{
		MakeStepSound( true );
		animator.DropStep();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::GetRealUnitPosition( CVec3 *pRes )
{
	if ( animator.GetHipPos( pRes ) )
		return;

	*pRes = position.GetCP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::InitAsCorpse( bool bDead ) 
{ 
	GetUnitRPG()->InitAsCorpse( bDead );
	bindGlobal.Update(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CDumbUnitServer::GetMaxFallDist( float extraDrop ) const
{
	float fMaxFall = 3.0f;
	NAI::IAIMap *pMap = GetWorld()->GetAIMap();
	vector<NAI::SInterval> intersect;
	CRay ray;
	ray.ptOrigin = position.GetCP();
	ray.ptOrigin.z = extraDrop + 0.2f;   // retail @0x34f1c0: z is REPLACED by extraDrop+0.2 (x/y stay from CP)
	ray.ptDir = CVec3( 0, 0, -1.0f );
	pMap->Trace( ray, &intersect, NWorld::TS_PASS_BLOCKER	);
	for ( int k = 0; k < intersect.size(); ++k )
	{
		NAI::SInterval &interv = intersect[k];
		if ( interv.enter.fT < 0 )
			continue;
		fMaxFall = interv.enter.fT;
		break;
	}
	return fMaxFall;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void BloodHandler( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bool bPrev = bShowBlood;
	NGlobal::VarBoolHandler( szID, sValue, pContext );
	if ( bShowBlood != bPrev )
		NGlobal::ThrowEvent( *((CShowBloodUpdated*)0) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
START_REGISTER(wDumbUnit)
	REGISTER_VAR_EX( "i_am_an_alien", NGlobal::VarBoolHandler, &bEverybodyIsAlien, 0, false )
	REGISTER_VAR_EX( "cheat_golden_shot", NGlobal::VarBoolHandler, &bIsGoldenShot, 0, false )
	// retail registers cheat_blood with default 1.0 (@0x752cb5) -- a 0 default broke the options
	// "default" button (ResetVar turned blood OFF) and the checkbox init (registry read 0).
	REGISTER_VAR_EX( "cheat_blood", BloodHandler, &bShowBlood, 1, true )
	REGISTER_VAR_EX( "cheat_heads_off", NGlobal::VarBoolHandler, &bForceHeadShot, 0, false )   // retail @0x752d13
	REGISTER_VAR_EX( "cheat_football", NGlobal::VarBoolHandler, &bFootball, 0, false )   // retail console var (Game.exe @0x4c8a96)
	REGISTER_VAR_EX( "cheat_allow_headshot", NGlobal::VarBoolHandler, &bAllowHeadshot, 1, true )   // retail @0x752dc9 (default ON, saved)
FINISH_REGISTER
