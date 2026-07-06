#include "StdAfx.h"
#include "wDumbUnit.h"
#include "wMain.h"
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
#include "aiSignal.h"
#include "aiMap.h"
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
void LaunchItem( CWorld *pWorld, const CDumbUnitServer::SResItem &item, const CVec3 &vel, CObjectBase *pVisibilityParent )
{
	pWorld->AddDebris( item.pModel.GetPtr(), pWorld->GetAIMap(), item.ptCenter, item.q, vel, pWorld->GetTime(), item.pItem, pVisibilityParent );
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
	if ( IsEmptyPK() )
		GetItemsBindPlaces( &boundMeshes, pRPG, bUndrawWeapon, pRPG->GetRPGPers()->pPanzerklein, bNoHeavyWeapon );
	else if ( CanFight() || IsWearingPK() )
		GetItemsBindPlaces( &boundMeshes, pRPG, bUndrawWeapon, GetWearingDBPK(), bNoHeavyWeapon );
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
bool CDumbUnitServer::IsLocker()
{
	return CanFight() || ( IsEmptyPK() && !bIsPKWhichIsWeared ) || IsWearingPK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::SetPosition( const NAI::SUnitPosition &dst )
{
	bool bNeedUpdate = dst.pos.GetFloor() != position.pos.GetFloor();
	bool bRealMove = false;
	if ( position.GetPose() == dst.GetPose() && position.GetDir() == dst.GetDir() ) // ���������� �����������
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
	// drop the uniform cap
	if ( bDropCap )
	{
		NDb::CRPGUniform *pDBUniform = pInventoryInfo->GetUniform();
		SRand rnd;
		if ( pDBUniform && pDBUniform->pCapModel )
		{
			GetBonePos( &item.ptCenter, &item.q, GetBoneName( UIT_CAP, 0, IsWearingPK() ) );
			item.pModel = pDBUniform->pCapModel->CreateModel(&rnd);
			// the cap has NO inventory item, so it must be fog-gated explicitly (retail @0x34ade0's
			// separate CObjectBase* visibility-parent, which DropItems @0x3502c0 passes for every
			// drop) -- else an unseen unit's death shows a floating helmet. The parent rides on the
			// flying CDItem (+0x2c) so the gate also survives physics-settle: Segment @0x34b4b0
			// re-derives it and AddFrozenItem @0x34b100 keeps the frozen cap on the fog-gated list
			// AND in visibleItems (the LOS/vision candidate list), so gaining LOS reveals it.
			// INTENTIONALLY BETTER THAN RETAIL: retail publishes the flying cap into
			// visibleDynamicItems and waits for the NEXT VISION UPDATE to LOS-reveal it (a small but
			// visible delay); a dev fog-gated CDItem is worse still -- in-flight debris is never a
			// vision candidate (GetVisibleItems only walks the frozen list), so it stayed hidden
			// until physics-settle. When the dying unit is already visible to the HUMAN player we
			// route the cap to the ungated show list so it renders the same frame; unseen units keep
			// the gated (hidden-until-settle) path so a helmet can't float in the fog of war.
			// The gate MUST test visibility to the human/scenario-player-0 (the mission's view
			// player -- same resolution KillEmAll @0x2fd180 uses), NOT GetCurrentPlayer(): the TBS
			// current player is the ACTING player (the AI during its turn/fast-turn, NULL in plain
			// real time), and a player's visible list unconditionally contains its OWN units
			// (CPlayerBaseVision::UpdateVisible), so gating on it counted every AI victim as "seen"
			// and dropped its cap ungated into the fog of war.
			bool bSeen = false;
			CPlayer *pHuman = pWorld->GetPlayerByID( 0 );
			if ( IsValid( pHuman ) )
			{
				list< CPtr<CUnit> > visibleList;
				pHuman->GetVisible( &visibleList );
				CPtr<CUnit> pSelf = CDynamicCast<CUnit>( (CObjectBase*)this );
				if ( IsValid( pSelf ) )
					bSeen = find( visibleList.begin(), visibleList.end(), pSelf ) != visibleList.end();
			}
			LaunchItem( pWorld, item, VNULL3, bSeen ? (CObjectBase*)0 : (CObjectBase*)this );
		}
	}
	// drop the in-hand / slotted items
	if ( bDropHands )
	{
		for ( int nSlot = 0; nSlot != NDb::N_SLOTS; ++nSlot )
			if ( TearOffItem( &item, (NDb::ESlot)nSlot ) )
			{
				CVec3 vInitial( random.GetFloat( -0.3f, 0.3f ), random.GetFloat( -0.3f, 0.3f ), 2 );
				LaunchItem( pWorld, item, vInitial );
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
				LaunchItem( pWorld, item, vInitial );
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
		pCarried->animator.BeDropped();
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
			animator.Die( position, VNULL3, true );
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
int CDumbUnitServer::ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
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
	int nRes = pRPG->ProcessAttack( nUserID, pAttack, pArmor );
	if ( bShowBlood && nRes > 120 )
	{
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
		pWorld->AddHitLocator( new CHitLocator( nRes, ptHit ) );
		PlaySound( pRPG->GetRPGPers()->pSoundHit );

		if ( nRes > 5 )
		{
			pWorld->GetAIMap()->GetUnitHLPos( &ptHit, pWorld->GetAIMap()->GetHull(this), nUserID );
			pWorld->CreateBloodyMess( ptHit, pAttack->rTtrajectory.ptDir, this, 1 );
		}
	}

	vector<NDb::ECritical> criticals;
	pRPG->GetLastCriticals( &criticals );

	if ( bShowBlood && !IsDead() && nUserID == NAI::HL_HEAD && !criticals.empty() && !bHeadless && nCurrentVP <= 0 )
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
		KillUnit( pAttack->rTtrajectory.ptDir );
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
		GetWorld()->GetAISignalManager()->Add( NAI::CreateAIHitSignal( pAttack->pAttacker, pAttack->pTarget ) );
	}

	// retail @0x350e20 step 8 (the CORPSE PUSH -- was missing here entirely): push the downed body
	// with the shot direction via the clipless animator Die (bPlayDeath=false). This is the ONLY
	// ragdoll fall a combat knock-out ever gets (MakeUnconscious/FallAsIfDead play nothing), and the
	// push a fresh corpse gets on top of its death clip. Skipping it left KO'd units FROZEN standing
	// in their last pose -- and a later KillUnit on an unconscious unit takes the DropItems-only
	// branch, freezing the corpse for good (the reported frozen-corpse-at-old-place bug).
	// Gates (disasm 0x751313..0x751359): persona !IsAlive; direction (or cheat_football); not a worn
	// PK shell; persona not a panzerklein; not being carried (animator.bIsCarried @+103).
	// (retail calls NRPG::CAttackPortion::GetPushCorpseCoeff here for its return value only --
	//  dev's equivalent is the plain fPushCoeff member read below; no side effects to preserve)
	{
		CVec3 vPush = pAttack->rTtrajectory.ptDir;
		bool bDown = IsDead() || IsUnconscious();
		if ( bDown && ( bFootball || fabs2( vPush ) > 0 ) && !IsWearingPK() &&
		     pRPG->GetRPGPers()->pPanzerklein == 0 && !animator.bIsCarried )
		{
			Normalize( &vPush );
			float fCoeff = 1.0f;
			if ( bFootball )
				fCoeff = pAttack->fPushCoeff <= 0 ? 0.33f : 1.0f;   // retail GetPushCorpseCoeff(pAttack)
			vPush *= fCoeff;
			// retail posts a CUICmdUnitCamera(PR_UNIT_DIED_BEAUTY, t=(coeff<=0.5 ? 1 : 2*coeff+0.5)) when
			// bDied -- dev has no arbitrated camera-command class (see wUnitMove.cpp GLUE FIX); omitted.
			animator.Die( position, vPush, false );
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
	if ( !CanFight() )
	{
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
// long -> pSoundCycleBurst) instead of the Jan03 GetNBullets()==1 gate. (The retail returns the created C3DSound* for
// the long-burst sound-retention; this dev fork dropped that member, so we keep the void MakeAISound.)
void CDumbUnitServer::CreateFlash( bool bLeft, bool bFirstBullet )
{
	NAnimation::SBonePose barrel;
	bool bTest = false;
	NRPG::IWeaponItem *pRPGWeapon;
	if ( animator.GetCannon() )
	{
		NRPG::IWeaponItem *pCannonItem = animator.GetCannon()->GetItem();
		if ( !pCannonItem )
			return;
		pRPGWeapon = pCannonItem;
	}
	else {
		CDynamicCast<NRPG::IWeaponItem> pW(pRPG->GetInventory()->GetActive());
		if (pW)
			pRPGWeapon = pW;
		else
			return;
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
				return;
			pSound = pWeapon->pSoundBurst;
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pBurstAISound;
			break;
		case NDb::SM_LongBurst:
			if ( !bFirstBullet )
				return;
			pSound = pWeapon->pSoundCycleBurst;   // long burst -> the cycling burst sound
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pBurstAISound;
			break;
	}
	pWorld->MakeAISound( pAISound, this, 0, pSound );
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
	REGISTER_VAR_EX( "cheat_blood", BloodHandler, &bShowBlood, 0, true )
	REGISTER_VAR_EX( "cheat_football", NGlobal::VarBoolHandler, &bFootball, 0, false )   // retail console var (Game.exe @0x4c8a96)
FINISH_REGISTER
