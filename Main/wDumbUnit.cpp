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
////////////////////////////////////////////////////////////////////////////////////////////////////
void LaunchItem( CWorld *pWorld, const CDumbUnitServer::SResItem &item, const CVec3 &vel )
{
	pWorld->AddDebris( item.pModel.GetPtr(), pWorld->GetAIMap(), item.ptCenter, item.q, vel, pWorld->GetTime(), item.pItem );
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
	if ( !CanFight() && !IsValid( animator.GetCorpseCarrier() ) )
		nFloor = animator.GetDynamicsFloor();
	return nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::IsAddedToVisitor()
{
	if ( bIsPKWhichIsWeared )
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
		pWorld->GetPathNetwork()->GetFloor( pos.pos.p.GetLayer() ), 
		TS_UNITS|TS_FRAGMENTED|TS_PICK|TS_COVER|TS_WEAPON_BLOCKER );
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
void CDumbUnitServer::DropItems( bool bHands, bool bBackPack )
{
	SResItem item;
	CPtr<NRPG::IInventoryInfo> pInventoryInfo = pRPG->GetInventoryInfo();
	// ����������� ������
	if ( bHands )
	{
		NDb::CRPGUniform *pDBUniform = pInventoryInfo->GetUniform();
		SRand rnd;
		if ( pDBUniform )
		{
			if ( pDBUniform->pCapModel )
			{
				// � � ������ ������� �������
				GetBonePos( &item.ptCenter, &item.q, GetBoneName( UIT_CAP, 0, IsWearingPK() ) );
				item.pModel = pDBUniform->pCapModel->CreateModel(&rnd);
				LaunchItem( pWorld, item );
			}
		}
		// item in hand goes away
		for ( int nSlot = 0; nSlot != NDb::N_SLOTS; ++nSlot )
			if ( TearOffItem( &item, (NDb::ESlot)nSlot ) )
			{
				CVec3 vInitial( random.GetFloat( -0.3f, 0.3f ), random.GetFloat( -0.3f, 0.3f ), 2 );
				LaunchItem( pWorld, item, vInitial );
			}
	}
	// ���������� ��� �� �������
	if ( bBackPack )
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
void CDumbUnitServer::Hide( bool bHide )
{
	GetUnitRPG()->SetHiding( bHide );
	if ( !bHide )
	{
		bJustUnhided = true;
		NGlobal::ThrowEvent( CEventOnUnitUnhide( this ) );
	}
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::FallAsIfDead( const CVec3 &ptDir, bool bDropItemsFromBackPack )
{
	DropItems( !IsWearingPK(), bDropItemsFromBackPack );
	//
	if ( GetUnitRPG()->IsHiding() )
		Hide( false );
	//
	if ( !IsWearingPK() )
	{
		animator.Die( position, ptDir );
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
		FallAsIfDead( ptDir, true );
		PlaySound( pRPG->GetRPGPers()->pSoundDeath );
	}
	else
		DropItems( false, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::MakeUnconscious( const CVec3 &ptDir, bool bFromScript )
{
	if ( !CanFight() )
		return;
	//
	OnUnitMadeUnconscious( bFromScript ); // UnitServer callback
	//
	FallAsIfDead( ptDir, false );
	if ( !bFromScript )
		PlaySound( pRPG->GetRPGPers()->pSoundDeath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDumbUnitServer::BlowUp()
{
	SRand rnd;
	DropItems( true, true );
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

	if ( pRPG->IsDead() && !IsDead() )
		KillUnit( pAttack->rTtrajectory.ptDir );
	else if ( pRPG->IsUnconscious() && !IsUnconscious() )
		MakeUnconscious( pAttack->rTtrajectory.ptDir );
	else
	{		
		animator.Wound();

		NRPG::SUnitInfo sInfo;
		GetUnitRPG()->GetInfo( NAI::WALK, &sInfo );
		OnSuffersDamage( float(sInfo.nHP) / sInfo.nMaxHP );
		GetWorld()->GetAISignalManager()->Add( NAI::CreateAIHitSignal( pAttack->pAttacker, pAttack->pTarget ) );
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
	// �������� ���������� ���� ��� �����
	if ( !CanFight() )
	{
		if ( GetFloor() != nPrevFloor )
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
bool CDumbUnitServer::CheckPassable( const NAI::SUnitPosition &dst )
{
	pWorld->GetPathNetwork()->Unlock( this );
	bool bPassable = pWorld->GetPathNetwork()->IsPassable( dst.pos.p );
	if ( IsLocker() )
	{
		if ( bLocksTwoPlaces )
			pWorld->GetPathNetwork()->LockMovingObject( this, position.pos.p, nextLock );
		else
			pWorld->GetPathNetwork()->Lock( this, position.pos.p );
	}
	return bPassable;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDumbUnitServer::CanDoGameMove( const NAI::SUnitPosition &dst )
{
	if ( !CheckPassable( dst ) || !pRPG->CanMove() )
		return false;
	return true;
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
void CDumbUnitServer::CreateFlash()
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
	bTest = animator.GetBarrelPos( animator.GetCannon() ? 0 : pWeapon->GetModel()->pGeometry, &barrel );
	ASSERT( bTest );
	if ( pWeapon->pShotEffect )
	{
		CQuat rndX( random.GetFloat( - FP_PI / 8, FP_PI / 8 ), CVec3(1,0,0) );
		SRand rnd;
		AttachMiscObject( CreateDParticles( barrel.pos, barrel.rot * rndX, pWeapon->pShotEffect->GetEffect( &rnd ), GetFloor() ) );
	}
	//AttachMiscObject( new CDFlash( barrel.pos, CVec3(0.3f,0.3f,0.3f), 2.5f, 3 ) );
	NDb::CSound *pSound = 0;
	NDb::CAISound *pAISound = 0;
	bool bDoPlay = true;
	switch ( pRPGWeapon->GetShootMode() )
	{
		case NDb::SM_Snap:
		case NDb::SM_Aimed:
		case NDb::SM_Careful:
		case NDb::SM_Snipe:
			pSound = pWeapon->pSound; // AttachMiscObject // barrel.pos
			pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pAISound;
			break;
		case NDb::SM_ShortBurst:
		case NDb::SM_LongBurst:
			if ( pRPG->GetNBullets() == 1 )
			{
				pSound = pWeapon->pSoundBurst; // AttachMiscObject( // barrel.pos
				pAISound = pRPGWeapon->GetDBWeapon()->pWeaponType->pBurstAISound;
			}
			else
				bDoPlay = false;
			break;
	}
	//
	if ( bDoPlay )
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
			nID = 6; break;
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
float CDumbUnitServer::GetMaxFallDist() const
{
	float fMaxFall = 3.0f;
	NAI::IAIMap *pMap = GetWorld()->GetAIMap();
	vector<NAI::SInterval> intersect;
	CRay ray;
	ray.ptOrigin = position.GetCP();
	ray.ptOrigin.z += 0.2f;
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
FINISH_REGISTER
