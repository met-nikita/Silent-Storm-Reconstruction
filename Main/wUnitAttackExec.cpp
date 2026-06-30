#include "StdAfx.h"
#include "wUnitAttack.h"
#include "wUnitMove.h"
#include "wUnitServer.h"
#include "Grid.h"
#include "wMain.h"
#include "wUnitCommands.h"
#include "RPGItem.h"
#include "RPGItemSet.h" // CRAP
#include "RPGUnitMission.h"
#include "RPGGame.h"
#include "aiMap.h"
#include "aiCollider.h"
#include "phCollider.h"	// PhysCollideInfo (impact path; the free NAI::CollideInfo was removed)
#include "..\misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"
#include "wObject.h"
#include "wUnitStates.h"
#include "wAckBase.h"
#include "RPGToHit.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataAI.h"
#include "RPGCritical.h"
#include "wUnitAttack.h"
#include "wUnitAttackExec.h"
#include "aiNearestPosition.h"
#include "rpgPerkConstants.h"
#include "wUnitQueue.h"
#include "wDialog.h"
#include "scriptCallLUA.h"
#include "..\Misc\EventsBase.h"
#include "eventUnit.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsWithinHumanReach( const CVec3 &ptFrom, const CVec3 &ptTarget, float fPlaneDist )
{
	if ( sqr( ptFrom.x - ptTarget.x ) + sqr( ptFrom.y - ptTarget.y ) > sqr( fPlaneDist ) )
		return false;
	if ( ptTarget.z < ptFrom.z - 0.5 || ptTarget.z > ptFrom.z + 2 )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult HasWorkingWeapon( CUnitServer *pUS )
{
	NRPG::IInventoryItem *pItem = pUS->GetUnitRPG()->GetWeaponItem();
	CDynamicCast<NRPG::IWeaponItem> pW(pItem);
	if ( !IsValid( pW ) )
		return UCR_UNAVAILABLE;
	if ( !pW->HasAmmo() )
		return UCR_NEED_RELOAD;
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	bool bRPGCanUse = pW->GetDBWeapon()->pWeaponType->bTwoHanded ? pRPG->CanUseTwoHanded() : true;
	if ( !bRPGCanUse )
		return UCR_GENERAL_FAILURE;
	if ( !pW->IsWorking() )
		return UCR_WEAPON_JAMMED;
	//
	vector<NRPG::CAttackPortion> attack;
	pUS->GetUnitRPG()->CreateAttack( &attack, false );
	if ( attack.empty() )
		return UCR_GENERAL_FAILURE;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasGrenade( CUnitServer *pUS )
{
	NRPG::IInventoryItem *pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
	if ( dynamic_cast<NRPG::IGrenadeItem*>( pItem ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasThrowingKnife( CUnitServer *pUS )
{
	NRPG::IInventoryItem *pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
	CDynamicCast<NRPG::IMeleeWeaponItem> pMelee(pItem);
	if (pMelee)
	{
		if ( pMelee->GetDBMeleeWeapon()->bThrowing )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasBazookaAndRockets( CUnitServer *pUS )
{
	NRPG::IInventoryItem *pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
	CDynamicCast<NRPG::IWeaponItem> pWeapon(pItem);
	if (pWeapon)
	{
		if ( pWeapon->GetDBWeapon()->bBazookaLogic && pWeapon->HasAmmo() )
			return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult HasActiveUsableFirstAid( CUnitServer *pUS )
{
	CDynamicCast<NRPG::CFirstAidItem> pFA( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	if ( !IsValid( pFA ) )
		return UCR_UNAVAILABLE;
	if ( pFA->IsEmpty() )
		return UCR_NO_EQUIPMENT;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CanAttackWithCannon( CCannon *pCannon, const CVec3 &ptTarget )
{
	ASSERT(pCannon);
	if ( !pCannon )
		return UCR_UNAVAILABLE;

	CVec3 pos = pCannon->GetPosition();
	float fAngle = pCannon->GetDirection();
	CVec3 dir = ptTarget - pos;
	float fHAngle = SignumNormalizeAngleInRadian( atan2( dir.y, dir.x ) - fAngle );
	if ( fabs(fHAngle) > FP_PI8 )
		return UCR_TARGET_OUT_OF_RANGE;

	float fVAngleSin = dir.z / fabs(dir);
	if ( fabs(fVAngleSin) > sin(FP_PI8) )
		return UCR_TARGET_OUT_OF_RANGE;

	if ( !pCannon->GetItem()->HasAmmo() )
		return UCR_NEED_RELOAD;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CanDoFirstAid( CUnitServer *pUS, const NAI::SUnitPosition &from, CUnitServer *pTarget )
{
	EUnitCommandResult eResult = HasActiveUsableFirstAid( pUS );
	if ( eResult != UCR_OK )
		return eResult;

	if ( IsValid( pTarget ) )
	{
		if ( pTarget->IsDead() || pTarget->IsUnconscious() )
			return UCR_INVALID_COMMAND;

		CVec3 ptTarget = pTarget->GetPosition().GetCP();
		if ( !IsWithinHumanReach( from.GetCP(), ptTarget, F_HEAL_DISTANCE ) )
			return UCR_TARGET_OUT_OF_RANGE;

		if ( !pUS->GetUnitRPG()->GetRPGUnit()->CanHeal( pTarget->GetRPG()->GetRPGUnit() ) )
			return UCR_GENERAL_FAILURE;
	}

	if ( !IsValid( pTarget ) )
		return UCR_NO_TARGET;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanPickUpCorpse( CUnitServer *pUS, const NAI::SUnitPosition &from, CUnitServer *pTarget )
{
	CVec3 ptTarget = pTarget->GetPosition().GetCP();
	return fabs2( from.GetCP() - ptTarget ) < 4; // CRAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanDropCorpse( CUnitServer *pUS )
{
	const NAI::SUnitPosition &from = pUS->GetPosition();
	CVec3 center = from.GetCP();
	center.z += 0.9f;
	vector<SSphere> spheres;
	vector<CVec3> vels;
	vector<NAI::SCollisionPoint> ress;
	float fDir = from.GetDirection();
	vels.push_back( CVec3( cos(fDir) * FP_GRID_STEP, sin(fDir) * FP_GRID_STEP, 0 ) );
	spheres.push_back( SSphere( center, 0.45f ) );
	NAI::PhysCollideInfo( pUS->GetWorld()->GetAIMap(), spheres, vels, &ress, NWorld::TS_ITEM_BLOCKER );
	return (ress[0].fDist == NAI::FP_NO_COLLISION);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CheckParabolaIntersect( NAI::CCollider *pCollider, const float fTFly, const CVec3 &from, const CVec3 &to )
{
	CVec3 gravity(0,0,-F_GRAVITY);
	
	vector<char> results;
	vector<SSphere> spheres;
	vector<CVec3> vels;	
	
	CVec3 startVel = (to - from) / fTFly - 0.5f * fTFly * gravity;
	CVec3 endVel = (to - from) / fTFly + 0.5f * fTFly * gravity;
	float fTStart = 0;//1.f / fabs(startVel);
	float fTEnd = fTFly - 1.0f / fabs(endVel);
	if ( fTEnd < 0.01f )
		return false;
	for ( float fT = fTStart; fT < fTEnd; fT += 0.1f )
	{
		SSphere s;
		s.ptCenter = from + startVel * fT + gravity * fT * fT * 0.5f;
		s.fRadius = F_GRENADE_SPHERE_RADIUS;
		spheres.push_back( s );
		//sphereParticles.push_back( spheres.back() );
	}
	if ( spheres.empty() )
		return false;
	for ( int i = 0; i < spheres.size() - 1; ++i )
		vels.push_back( spheres[i+1].ptCenter - spheres[i].ptCenter );
	vels.push_back( from + startVel * fTEnd + gravity * fTEnd * fTEnd * 0.5f - spheres.back().ptCenter );
	//sphereParticles.push_back( SSphere(from + startVel * fTEnd + gravity * fTEnd * fTEnd * 0.5f, F_GRENADE_SPHERE_RADIUS ) );
	
	pCollider->CollideCheck( spheres, vels, &results );
	for ( int i = 0; i < results.size(); ++i )
	{
		if ( results[i] )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool FindGrenadeParams( CWorld *pWorld, const NAI::SUnitPosition &pos, const CVec3 &to, float fMaxVel, SGrenadeParams *pRes ) 
{
	CVec3 from = pos.GetCenter();
	
	vector<CVec3> boundPoints;
	boundPoints.push_back( from );
	boundPoints.push_back( to );
	
//	float fMaxVel = 14.0f; // CRAP, maximum velocity, should take it from RPG skills
	float a = sqr(F_GRAVITY) * 0.25f;
	float b = F_GRAVITY * (to.z - from.z) - sqr(fMaxVel);
	float c = fabs2(to - from);
	float d = sqr(b) - 4 * a * c;
	if ( d < 0 )
	{
		// cannot throw there - too far
		return false;
	}
	float fTFlyMin = 0.5f * (- b - sqrt(d)) / a;
	float fTFlyMax = 0.5f * (- b + sqrt(d)) / a;
	ASSERT( fTFlyMin > 0 );
	ASSERT( fTFlyMax > 0 );
	fTFlyMin = sqrt(fTFlyMin);
	fTFlyMax = sqrt(fTFlyMax);
	
	// upper point of all trajectories
	float fTUp = (to.z - from.z) / F_GRAVITY / fTFlyMax + fTFlyMax * 0.5f;
	if ( fTUp > 0 && fTUp < fTFlyMax )
		boundPoints.push_back( CVec3( to.x, to.y, from.z + F_GRAVITY * fTUp * fTUp * 0.5f ) );
	
	float fAngle = pos.GetDirection();
	float fX = F_GRENADE_CHECK_SIDE * cos(fAngle), fY = F_GRENADE_CHECK_SIDE * sin(fAngle);
	CVec3 rightSide(fY,-fX,0);
	CVec3 leftSide(-fY,fX,0);
	boundPoints.push_back( from + leftSide );
	boundPoints.push_back( from + rightSide );
	
	SBound bound;
	CalcBound( &bound, boundPoints, SGetSelf<CVec3>() );
	bound.Extend( Max( F_GRENADE_CHECK_RADIUS, F_GRENADE_SPHERE_RADIUS ) );
	CPtr<NAI::CCollider> pCollider = new NAI::CCollider;
	NAI::CCollider &collider = *pCollider;
	pWorld->GetAIMap()->PrepareCollider( &collider, bound, F_GRENADE_SPHERE_RADIUS, TS_PASS_BLOCKER );//TS_TERRAINS|TS_OBJECTS );
	
	vector<SSphere> spheres;
	vector<CVec3> vels;
	vector<char> results;
	
	spheres.push_back( SSphere( from, F_GRENADE_CHECK_RADIUS ) );
	vels.push_back( rightSide );
	spheres.push_back( SSphere( from, F_GRENADE_CHECK_RADIUS ) );
	vels.push_back( leftSide );
	collider.CollideCheck( spheres, vels, &results );
	
	vector<CVec3> startPoints;
	startPoints.push_back( from );
	startPoints.push_back( from + rightSide ); // right side
	startPoints.push_back( from + leftSide ); // left side
	
	bool bFound = false;
	int nSide = 0;
	float fTFly;
	for ( fTFly = fTFlyMin; fTFly < fTFlyMax + F_GRENADE_DELAY_STEP; fTFly += F_GRENADE_DELAY_STEP )
	{
		int nSPSize = startPoints.size();
		for ( int k = 0; k < nSPSize; ++k )
		{
			if ( k > 0 && results[k - 1] )
				continue;
			from = startPoints[k];
			bFound = !CheckParabolaIntersect( &collider, Min(fTFly, fTFlyMax), from, to );
			if ( bFound ) 
			{
				nSide = k;
				break;
			}
		}
		if ( bFound )
			break;
	}
	if ( bFound )
	{
		SGrenadeParams &gp = *pRes;
		gp.ptOriginalTarget = to;
		gp.ptStart = from;
		gp.vel = (to - from) / fTFly - 0.5f * fTFly * CVec3(0,0,-F_GRAVITY);
		gp.fT = fTFly; 
		gp.nSide = nSide;
	}
	return bFound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CanUnitThrowGrenade( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IGrenadeItem *pGrenade )
{
	SGrenadeParams sParams;

	CPtr<NRPG::CGrenadeToHitCalcer> pToHitCalcer;
	int nDistance = fabs( ptTarget - from.GetCP() ) / FP_GRID_STEP;
	pToHitCalcer = new NRPG::CGrenadeToHitCalcer( pUS, from.GetPose(),
		nDistance, from.GetCP(), pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptTarget, pGrenade );	// AI evaluates an inventory grenade -> pass it explicitly (it is not the active item)
/*	float fGrenadeMaxDistance = pToHitCalcer->GetGrenadeMaxDistance();

	if ( nDistance > fGrenadeMaxDistance )
		return UCR_TARGET_OUT_OF_RANGE;*/
	if ( !FindGrenadeParams( pUS->GetWorld(), from, ptTarget, pToHitCalcer->GetMaxGrenadeVelocity(), &sParams ) )
		return UCR_TARGET_OUT_OF_RANGE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Script-driven instant grenade throw (retail NWorld::UnitThrowGrenade @0x3ac740, called by the lua
// UnitGrenadeToUnit / UnitGrenadeToWaypoint handlers). Unlike the AI/player CExecThrowGrenade path it
// uses NO inventory grenade, AP spend or wind-up animation: it spawns a temp grenade item from the DB
// record, computes the ballistic params (skill-based max velocity + to-hit scatter + fuse delay, exactly
// as CExecThrowGrenade::CheckToHitAndDelay) and hands the flight to CWorld::ThrowGrenade.
void UnitThrowGrenade( CUnitServer *pUS, NDb::CRPGGrenade *pGrenade, const CVec3 &ptTarget, int /*nReserved*/ )
{
	if ( !IsValid( pUS ) || !IsValid( pGrenade ) )
		return;

	// the script supplies a grenade DB record, not an equipped item -- make a transient one to throw
	CObj<NRPG::IInventoryItem> pItem = NRPG::CreateItem( pGrenade );
	CDynamicCast<NRPG::IGrenadeItem> pGrenadeItem( pItem.GetPtr() );
	if ( !IsValid( pGrenadeItem ) )
		return;

	const NAI::SUnitPosition position = pUS->GetPosition();

	int nDistance = fabs( ptTarget - position.GetCP() ) / FP_GRID_STEP;
	CPtr<NRPG::CGrenadeToHitCalcer> pToHitCalcer = new NRPG::CGrenadeToHitCalcer( pUS, position.GetPose(),
		nDistance, position.GetCP(), pUS->GetWorld()->IsFirstTurn(), false, CVec3( 1, 1, 1 ), ptTarget, pGrenadeItem.GetPtr() );	// transient script grenade is not equipped/active -> pass it explicitly

	SGrenadeParams grenadeParams;
	if ( !FindGrenadeParams( pUS->GetWorld(), position, ptTarget, pToHitCalcer->GetMaxGrenadeVelocity(), &grenadeParams ) )
		return;

	// to-hit scatter + fuse delay -- mirrors CExecThrowGrenade::CheckToHitAndDelay (`random` there is an
	// executor member, so use a local SRand, the dev's local-rng idiom elsewhere in this file).
	SRand random;
	int nToHit = pToHitCalcer->GetToHit();
	int nRandom = random.Get( 100 );
	if ( nRandom > nToHit )
	{
		float fD = 0.2f * fabs( grenadeParams.vel );
		grenadeParams.vel.x += random.GetFloat( -fD, +fD );
		grenadeParams.vel.y += random.GetFloat( -fD, +fD );
		grenadeParams.vel.z += random.GetFloat( -fD, +fD );
	}
	grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float( pGrenade->nMaxDelay ) );
	grenadeParams.fT = pGrenade->nMaxDelay - grenadeParams.fT;
	nRandom = random.Get( 100 );
	if ( nRandom >= 99 || nRandom >= pToHitCalcer->GetSkill() )
	{
		grenadeParams.fT *= random.GetFloat( 0.5f, 2.f );
		grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float( pGrenade->nMaxDelay ) * 0.75f );
	}

	// the flying grenade mesh comes from the grenade's item record (null model still explodes)
	NDb::CModel *pModel = 0;
	if ( IsValid( pGrenade->pItem ) && IsValid( pGrenade->pItem->pModel ) )
	{
		SRand rndModel;
		pModel = pGrenade->pItem->pModel->CreateModel( &rndModel );
	}

	pUS->GetWorld()->ThrowGrenade( grenadeParams.ptStart, grenadeParams.vel,
		pUS->GetWorld()->GetTime()->GetValue(), grenadeParams.fT, pModel, pGrenade, pUS );
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CanUnitThrowKnife( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IMeleeWeaponItem *pMelee )
{
	CPtr<NRPG::CThrowKnifeTileToHitCalcer> pToHitCalcer;
	int nDistance = fabs( ptTarget - from.GetCP() ) / FP_GRID_STEP;
	pToHitCalcer = new NRPG::CThrowKnifeTileToHitCalcer( pUS, from.GetPose(),
		nDistance, from.GetCP(), 100.f, pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptTarget, 0 );

 	float fKnifeMaxDistance = pToHitCalcer->GetKnifeMaxDistance();

	if ( nDistance > fKnifeMaxDistance )
		return UCR_TARGET_OUT_OF_RANGE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CanUnitLaunchRocket( CUnitServer *pUS, 
	const NAI::SUnitPosition &from, const CVec3 &ptTarget, int nExtraAP, NRPG::IWeaponItem *pBazooka )
{
	float fDistance = fabs( ptTarget - from.GetCP() );
	int nDistance =  fDistance / FP_GRID_STEP;
	//
	if ( nDistance < 3 )
		return UCR_TARGET_OUT_OF_RANGE;
	//
	CPtr<NRPG::CRLauncherToHitCalcer> pToHitCalcer =
		new NRPG::CRLauncherToHitCalcer( pUS, from.GetPose(), nDistance,
			from.GetCP(), 100.f, nExtraAP, pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptTarget );
	//
	if ( nDistance >= pToHitCalcer->GetMaxDistance() )
		return UCR_TARGET_OUT_OF_RANGE;
	//
	vector<NRPG::CAttackPortion> attack;
	pUS->GetUnitRPG()->CreateAttack( &attack, false );
	if ( attack.empty() )
		return UCR_GENERAL_FAILURE;
	//
	CObj<NRPG::CCoverInfo> pCover = pUS->GetWorld()->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( from ), 
		attack[0], pUS, ptTarget, pUS->GetMinClearDistance() );
	if ( !NRPG::CanShoot( pCover ) )
		return UCR_GENERAL_FAILURE;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsPointInRect( int nX, int nY, const CTRect<int> &sRect )
{
	if ( ( nX >= sRect.x1 ) && ( nX <= sRect.x2 ) && ( nY >= sRect.y1 ) && ( nY <= sRect.y2 ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsRectsIntersect( const CTRect<int> &s1, const CTRect<int> &s2 )
{
	if ( IsPointInRect( s1.x1, s1.y1, s2 ) || IsPointInRect( s1.x2, s1.y1, s2 ) || IsPointInRect( s1.x2, s1.y2, s2 ) || IsPointInRect( s1.x1, s1.y2, s2 ) )
		return true;
	if ( IsPointInRect( s2.x1, s2.y1, s1 ) || IsPointInRect( s2.x2, s2.y1, s1 ) || IsPointInRect( s2.x2, s2.y2, s1 ) || IsPointInRect( s2.x1, s2.y2, s1 ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult CanMoveInventoryItem( CUnitServer *pUS, CCmdMoveInventoryItem *pMoveItem )
{
	CObj<NRPG::IInventoryItem> pItem;

	const SItem &sSource = pMoveItem->GetSource();
	const SItem &sTarget = pMoveItem->GetTarget();

	switch( sSource.eType )
	{
	case SItem::HAND:
		{
			IPlayer::SItemInfo sItem;
			if ( !pUS->GetTBSPlayer()->GetInHandItem( &sItem ) )
				return UCR_INVALID_COMMAND;

			pItem = sItem.pItem;
			break;
		}
	case SItem::SLOT:
		{
			ASSERT( IsValid( sSource.pUnit ) );
			CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
			ASSERT( IsValid( pUSSource ) );
			ASSERT( !pUSSource->IsDead() );
			NRPG::IInventory *pInventory = pUSSource->GetUnitRPG()->GetInventory();

			pItem = pInventory->Get( (NDb::ESlot)sSource.nSlot );
			if ( !IsValid( pItem ) )
				return UCR_INVALID_COMMAND;

			break;
		}
	case SItem::BACKPACK:
		{
			ASSERT( IsValid( sSource.pUnit ) );
			CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
			ASSERT( IsValid( pUSSource ) );
			ASSERT( !pUSSource->IsDead() );
			pItem = sSource.pItem;
			break;
		}
	case SItem::GROUND:
		{
			if ( IsValid( sSource.pUnit ) )
			{
				CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
				ASSERT( IsValid( pUSSource ) );
				ASSERT( pUSSource->IsDead() );
				NRPG::IInventory *pDeadInventory = pUSSource->GetUnitRPG()->GetInventory();
				if ( sSource.nSlot >= 0 )
				{
					pItem = pDeadInventory->Get( (NDb::ESlot)sSource.nSlot );
					if ( !IsValid( pItem ) )
						return UCR_INVALID_COMMAND;
				}
				else
					pItem = sSource.pItem;
			}
			else
				pItem = sSource.pItem;

			break;
		}
	case SItem::STORAGE:
		{
			ASSERT( IsValid( sSource.pPlayer ) );

			list<CPtr<NRPG::IInventoryItem> > itemsSet;
			sSource.pPlayer->GetStoreItems( &itemsSet );

			if ( find( itemsSet.begin(), itemsSet.end(), sSource.pItem ) != itemsSet.end() )
				pItem = sSource.pItem;
			else
				return UCR_GENERAL_FAILURE;

			break;
		}
	case SItem::UNIT_ANYPLACE:
		ASSERT( 0 );
		return UCR_INVALID_COMMAND;
	}

	ASSERT( IsValid( pItem ) );

	switch( sTarget.eType )
	{
	case SItem::HAND:
		{
			IPlayer::SItemInfo sItem;
			if ( pUS->GetTBSPlayer()->GetInHandItem( &sItem ) )
				return UCR_GENERAL_FAILURE;
			break;
		}
	case SItem::SLOT:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			if( !IsValid( pUSTarget ) || pUSTarget->IsDead() )
				return UCR_GENERAL_FAILURE;

			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();
			if ( !pInventory->CanEquip( (NDb::ESlot)sTarget.nSlot, pItem ) )
				return UCR_GENERAL_FAILURE;

			break;
		}
	case SItem::BACKPACK:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			if( !IsValid( pUSTarget ) || pUSTarget->IsDead() )
				return UCR_GENERAL_FAILURE;

			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();
			CTPoint<int> sTargetPosition = sTarget.sPosition;
			if ( ( sTargetPosition.x == -1 ) && ( sTargetPosition.y == -1 ) )
			{
				if ( !pInventory->FindPlace( pItem, &sTargetPosition ) )
					return UCR_INVENTORY_NO_PLACE;
			}
			const CTPoint<int> &sTargetSize = pItem->GetSize();
			CTRect<int> sTargetRect( sTargetPosition.x, sTargetPosition.y, sTargetPosition.x + sTargetSize.x - 1, sTargetPosition.y + sTargetSize.y - 1 );

			list<NRPG::SBackPackItem> intersectedItemslist;
			const vector<NRPG::SBackPackItem> &itemsSet = pInventory->GetItems();
			for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
			{
				const CTPoint<int> &sSize = itemsSet[nTemp].pItem->GetSize();
				const CTPoint<int> &sPosition = itemsSet[nTemp].sPos;
				CTRect<int> sItemRect( sPosition.x, sPosition.y, sPosition.x + sSize.x - 1, sPosition.y + sSize.y - 1 );

				if ( IsRectsIntersect( sTargetRect, sItemRect ) )
					intersectedItemslist.push_back( itemsSet[nTemp] );
			}

			if ( intersectedItemslist.size() > 1 )
				return UCR_GENERAL_FAILURE;

			if ( !pInventory->CanPlace( sTargetPosition, pItem, false ) )
				return UCR_GENERAL_FAILURE;

			break;
		}
	case SItem::GROUND:
		{
			break;
		}
	case SItem::STORAGE:
		{
			break;
		}
	case SItem::UNIT_ANYPLACE:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			if( !IsValid( pUSTarget ) || pUSTarget->IsDead() )
				return UCR_GENERAL_FAILURE;

			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();
			for ( int nTemp = 0; nTemp < NDb::N_SLOTS; nTemp++ )
			{
				if ( !IsValid( pInventory->Get( (NDb::ESlot)nTemp ) ) && pInventory->CanEquip( (NDb::ESlot)nTemp, pItem ) )
					return UCR_OK;
			}

			CTPoint<int> sTargetPosition;
			if ( pInventory->FindPlace( pItem, &sTargetPosition ) )
				return UCR_OK;

			return UCR_INVENTORY_NO_PLACE;
		}
	}

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void LaunchItem( CUnitServer *pUS, NRPG::IInventoryItem *pItem )
{
	CVec3 shift;
	shift.x = random.GetFloat( - FP_GRID_STEP * 0.5f, FP_GRID_STEP * 0.5f );
	shift.y = random.GetFloat( - FP_GRID_STEP * 0.5f, FP_GRID_STEP * 0.5f );
	shift.z = 1.5f;
	CDumbUnitServer::SResItem item;
	item.ptCenter = pUS->GetPosition().GetCP() + shift;
	item.q = QNULL;
	item.pItem = pItem;
	SRand rnd;
	item.pModel = pItem->GetDBItem()->pModel->CreateModel( &rnd );
	LaunchItem( pUS->GetWorld(), item, CVec3( 0, 3, 3 ) );
	//pUS->GetWorld()->AddFrozenItem( pUS->GetPosition().GetCP() + shift, QNULL, pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddItemToUnit( CUnitServer *pUS, NDb::CRPGItem *_pItem )
{
	if ( !IsValid(_pItem) )
		return;
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	NRPG::IInventoryItem *pItem = NRPG::CreateItem( _pItem->pSuccessor );
	CTPoint<int> invPos;
	if ( pInventory->FindPlace( pItem, &invPos ) )
		pInventory->Place( invPos, pItem );
	else
		LaunchItem( pUS, pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MoveInventoryItem( CUnitServer *pUS, CCmdMoveInventoryItem *pMoveItem )
{
	CObj<NRPG::IInventoryItem> pItem;

	SItem sSource = pMoveItem->GetSource();
	SItem sTarget = pMoveItem->GetTarget();

	switch( sSource.eType )
	{
	case SItem::HAND:
		{
			IPlayer::SItemInfo sItem;
			pUS->GetTBSPlayer()->GetInHandItem( &sItem );
			pItem = sItem.pItem;

			sItem.pItem = 0;
			pUS->GetTBSPlayer()->SetInHandItem( sItem );
			break;
		}
	case SItem::SLOT:
		{
			ASSERT( IsValid( sSource.pUnit ) );
			CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
			ASSERT( IsValid( pUSSource ) );
			ASSERT( !pUSSource->IsDead() );
			NRPG::IInventory *pInventory = pUSSource->GetUnitRPG()->GetInventory();
			pItem = pInventory->TakeOff( (NDb::ESlot)sSource.nSlot );
			break;
		}
	case SItem::BACKPACK:
		{
			ASSERT( IsValid( sSource.pUnit ) );
			CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
			ASSERT( IsValid( pUSSource ) );
			ASSERT( !pUSSource->IsDead() );
			NRPG::IInventory *pInventory = pUSSource->GetUnitRPG()->GetInventory();
			pItem = sSource.pItem;
			pInventory->Take( sSource.pItem );
			break;
		}
	case SItem::GROUND:
		{
			if ( IsValid( sSource.pUnit ) )
			{
				CDynamicCast<CUnitServer> pUSSource( sSource.pUnit );
				ASSERT( IsValid( pUSSource ) );
				ASSERT( pUSSource->IsDead() );
				NRPG::IInventory *pDeadInventory = pUSSource->GetUnitRPG()->GetInventory();
				if ( sSource.nSlot >= 0 )
				{
					NDb::ESlot eSlot = (NDb::ESlot)sSource.nSlot;
					ASSERT( pDeadInventory->Get( eSlot ) == sSource.pItem );
					pItem = pDeadInventory->TakeOff( eSlot );
				}
				else
				{
					pItem = sSource.pItem;
					pDeadInventory->Take( sSource.pItem );
				}
				pUSSource->Update();
			}
			else
			{
				pItem = sSource.pItem;
				pUS->GetWorld()->RemoveFrozenItem( sSource.pItem );
			}

			sSource.pUnit = pUS;
			break;
		}
	case SItem::STORAGE:
		{
			ASSERT( IsValid( sSource.pPlayer ) );

			pItem = sSource.pItem;

			vector<CPtr<NRPG::IInventoryItem> > itemsSet;
			sSource.pPlayer->TakeStoreItem( sSource.pItem );

			break;
		}
	case SItem::UNIT_ANYPLACE:
		ASSERT( 0 );
		return;
	}

	ASSERT( IsValid( pItem ) );

	switch( sTarget.eType )
	{
	case SItem::HAND:
		{
			IPlayer::SItemInfo sItem;
			sItem.pItem = pItem;
			sItem.pUnit = sSource.pUnit;
			pUS->GetTBSPlayer()->SetInHandItem( sItem );
			break;
		}
	case SItem::SLOT:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			ASSERT( IsValid( pUSTarget ) );
			ASSERT( !pUSTarget->IsDead() );
			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();

			CObj<NRPG::IInventoryItem> pOldItem = pInventory->Get( (NDb::ESlot)sTarget.nSlot );
			if ( IsValid( pOldItem ) )
			{
				pInventory->TakeOff( (NDb::ESlot)sTarget.nSlot );

				IPlayer::SItemInfo sInfo;
				sInfo.pUnit = pUSTarget;
				sInfo.pItem = pOldItem;
				pUSTarget->GetTBSPlayer()->SetInHandItem( sInfo );
			}

			pInventory->Equip( (NDb::ESlot)sTarget.nSlot, pItem );
			break;
		}
	case SItem::BACKPACK:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			ASSERT( IsValid( pUSTarget ) );
			ASSERT( !pUSTarget->IsDead() );
			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();

			CTPoint<int> sTargetPosition = sTarget.sPosition;
			if ( ( sTargetPosition.x == -1 ) && ( sTargetPosition.y == -1 ) )
				pInventory->FindPlace( pItem, &sTargetPosition );

			const CTPoint<int> &sTargetSize = pItem->GetSize();
			CTRect<int> sTargetRect( sTargetPosition.x, sTargetPosition.y, sTargetPosition.x + sTargetSize.x - 1, sTargetPosition.y + sTargetSize.y - 1 );

			list<NRPG::SBackPackItem> intersectedItemslist;
			const vector<NRPG::SBackPackItem> &itemsSet = pInventory->GetItems();
			for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
			{
				const CTPoint<int> &sSize = itemsSet[nTemp].pItem->GetSize();
				const CTPoint<int> &sPosition = itemsSet[nTemp].sPos;
				CTRect<int> sItemRect( sPosition.x, sPosition.y, sPosition.x + sSize.x - 1, sPosition.y + sSize.y - 1 );

				if ( IsRectsIntersect( sTargetRect, sItemRect ) )
					intersectedItemslist.push_back( itemsSet[nTemp] );
			}

			if ( intersectedItemslist.size() > 1 )
				break;

			if ( !intersectedItemslist.empty() )
			{
				CObj<NRPG::IInventoryItem> pOldItem = intersectedItemslist.front().pItem;
				pInventory->Take( pOldItem );

				IPlayer::SItemInfo sInfo;
				sInfo.pUnit = pUSTarget;
				sInfo.pItem = pOldItem;
				pUSTarget->GetTBSPlayer()->SetInHandItem( sInfo );
			}

			pInventory->Place( sTargetPosition, pItem );
			break;
		}
	case SItem::GROUND:
		LaunchItem( pUS, pItem );
		break;
	case SItem::STORAGE:
		{
			ASSERT( IsValid( sTarget.pPlayer ) );
			sTarget.pPlayer->PlaceStoreItem( pItem );
			break;
		}
	case SItem::UNIT_ANYPLACE:
		{
			ASSERT( IsValid( sTarget.pUnit ) );
			CDynamicCast<CUnitServer> pUSTarget( sTarget.pUnit );
			ASSERT( IsValid( pUSTarget ) );
			ASSERT( !pUSTarget->IsDead() );
			NRPG::IInventory *pInventory = pUSTarget->GetUnitRPG()->GetInventory();

			int pValidSlots[] = { NDb::SLOT_1, NDb::SLOT_2 };

			bool bComplete = false;
			for ( int nTemp = 0; nTemp < ARRAY_SIZE( pValidSlots ); nTemp++ )
			{
				NDb::ESlot eSlot = (NDb::ESlot)pValidSlots[nTemp];
				if ( !IsValid( pInventory->Get( eSlot ) ) && pInventory->CanEquip( eSlot, pItem ) )
				{
					bComplete = true;
					pInventory->Equip( eSlot, pItem );
					break;
				}
			}

			if ( !bComplete )
			{
				CTPoint<int> sTargetPosition;
				if ( pInventory->FindPlace( pItem, &sTargetPosition ) )
				{
					bComplete = true;
					pInventory->Place( sTargetPosition, pItem );
				}
			}

			ASSERT( bComplete );
			break;
		}
	case SItem::VACUUM:
		// destroy the item: it was already taken off its source above, so just drop the last
		// reference (pItem is a CObj) -- the item is removed from the world. (DestroyItemInHand)
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecAttack
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecAttack::CExecAttack( CUnitServer *_pUS ):
	CCommandExecute(_pUS), bAttackCanceled( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecAttack::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	ASSERT( pUS->CanSpendAP( GetStartAP() ) );
	//
	bAttackCanceled = false;
	pUS->GetUnitRPG()->StartAttack();
	Start();
	StartAction( pUS->GetWorld(), NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecAttack::TimeLabelReached() // ���� false, �� �����
{
	pUS->GetUnitRPG()->NextBullet();
	bool bRes = OnLabel();						// ���� ������� ��������� NextBullet(), �� ���� �� ������� �������� �� �����
	if ( !bRes && !bAttackCanceled )			// ������ ���������� ���� ���� �� ���� �����! ������ �� � ���!
		pUS->GetUnitRPG()->StartAttack(); // reset burts state
	return bRes && !bAttackCanceled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecAttack::Cancel()
{
	bAttackCanceled = true;
//	StopAction();
//	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecAttack::CreateAttack( vector<NRPG::CAttackPortion> *pAttack, CUnitServer *pUnitTarget, bool bSpendAmmo ) const
{
	bool bBackStab = false;
	if ( IsValid( pUnitTarget ) )
	{
		bBackStab = !pUnitTarget->IsUnitAudible( pUS ) && !pUnitTarget->IsUnitVisible( pUS );
		if ( bBackStab )
			csSystem << CC_RED << "Backstab attack" << endl;
	}
	CPtr<NRPG::IUnitMission> pTarget = IsValid( pUnitTarget ) ? pUnitTarget->GetUnitRPG() : 0;
	return pUS->GetUnitRPG()->CreateAttack( pAttack, bSpendAmmo, false, pTarget, bBackStab );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShoot
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecShoot::CExecShoot( CUnitServer *_pUS, int _nExtraAP ): 
	CExecAttack(_pUS), nExtraAP(_nExtraAP)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecShoot::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( GetActionType( pUS ) == AT_CANNON )
	{
		if ( bIgnoreTarget )
			return UCR_NO_TARGET;

		return CanAttackWithCannon( pUS->animator.GetCannon(), ptAnimTarget );
	}

	if ( from.pos.p.GetPose() == NAI::CM_INACTIVE )
		return UCR_GENERAL_FAILURE;
	return HasWorkingWeapon( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::PerformShot()
{
	if ( !Attack.empty() )
	{
		Attack.front().rTtrajectory = ray;
		PerformAttack( Attack, ray, fRayToHit );
		pUS->CreateFlash();
		Attack.clear();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecShoot::OnLabel()
{
	bool bBurstComplete = bComplete;
	const NAI::SUnitPosition &position = pUS->GetPosition();

	if ( IsAttackCanceled() )
	{
		bAttackCanceled = true;
	}
	else
	{
		PerformShot();
		if ( !bComplete && !bAttackCanceled )
			PrepareShot();
	}

	if ( bBurstComplete || bAttackCanceled )
	{
		pUS->DoAction( NRPG::AC_END_SHOOT );
		CheckShotResult();
	}
	else
	{
		pUS->animator.Attack( position, ray, true );
	}

	return !bBurstComplete || bAttackCanceled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::PerformAttack( const vector<NRPG::CAttackPortion> &attack, const CRay &ray, float fHit )
{
	vector<NRPG::IAttackable*> ignores;
	CCannon *pCannon = pUS->animator.GetCannon();
	if ( pCannon )
		ignores.push_back( pCannon );
	else
		ignores.push_back( pUS );

	float fTrailSpeed = 0;
	CPtr<NDb::CModel> pTrailEffect = 0;
	NRPG::IWeaponItem *pWeapon = pUS->GetUnitRPG()->GetWeaponItem();
	if ( pWeapon && pWeapon->GetDBWeapon()->pTrailEffect )
	{
		SRand sRand;
		fTrailSpeed = pWeapon->GetDBWeapon()->fTrailSpeed;
		pTrailEffect = pWeapon->GetDBWeapon()->pTrailEffect->CreateModel( &sRand );
	}

	CRay rTempRay( ray ); // CRAP
	rTempRay.ptOrigin += ray.ptDir * pUS->GetMinClearDistance();
	for ( vector<NRPG::CAttackPortion>::const_iterator i = attack.begin(); i != attack.end(); ++i )
		pUS->GetWorld()->PerformRangedAttack( *i, rTempRay, ignores, pUS->GetWorld()->GetTime()->GetValue(), pTrailEffect, fTrailSpeed );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecShoot::CheckBurst( bool bComplete )
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::CWeaponItem *pWeapon = pRPG->GetRPGUnit()->GetWeaponItem();
	float fPerkBullets = 0;
	pRPG->HasPerk( NRPG::N_PERK_LONGER_SHORT_BURST, &fPerkBullets );
	bool bBurstLastBullet = pRPG->GetNBullets() && 
		pRPG->GetNBullets() >= ( int ) ( pWeapon->GetDBWeapon()->nRoF / 6.f + fPerkBullets );
	//
	if ( !bComplete || bBurstLastBullet )
	{
		if ( CanDoIt( pUS->GetPosition() ) != UCR_OK )
			return true;
		if ( pUS->GetUnitRPG()->GetNBullets() > 1 )
		{
			if ( !pUS->CanSpendAP( pUS->GetActionAP( NRPG::AC_BURST ) ) )
				return true;
			pUS->DoAction( NRPG::AC_BURST );
		}
	}
	return bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecShoot::GetActionAP() const 
{ 
	if ( IsAccidental() )
		return 0;
	NRPG::CWeaponItem *pWeapon = pUS->GetUnitRPG()->GetRPGUnit()->GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		NDb::EShootMode ShootMode = pWeapon->GetShootMode();
		if ( ShootMode == NDb::SM_Careful || ShootMode == NDb::SM_LongBurst )
		{
			NRPG::SUnitInfo Info;
			pUS->GetInfo( &Info );
			return max( Info.nAP,  GetStartAP() );
		} 
	}

	return GetStartAP(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::Scream()
{
	CPtr<NRPG::CWeaponItem> pWeapon = pUS->GetUnitRPG()->GetRPGUnit()->GetWeaponItem();
	if ( IsValid( pWeapon ) && pWeapon->GetShootMode() == NDb::SM_LongBurst )
	{
		NGlobal::ThrowEvent( CEventOnUnitLongBurst( pUS ) );
		if ( pUS->GetPosition().GetPose() != NAI::CRAWL )
		{
			if ( IsValid( pWeapon->GetDBWeapon() ) && 
				pWeapon->GetDBWeapon()->pAnimWeaponType->type == NDb::WT_MACHINE_GUN )
			{
					pUS->GetWorld()->MakeSound( pUS->GetPosition().GetCP(), NDb::GetSound( 4060 ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::Start()
{
	bComplete = true;
	bMissed = true;
	nToHit = 0;
	//
	csRPG << CC_WHITE << "Shoot begin :\n";
	CalculateExtraAP();
	PrepareShot();
	SpendAP();
	if ( !IsAccidental() )
	{
		Scream();
		pUS->animator.Attack( pUS->GetPosition(), ray );
	}
	else
		OnLabel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecShoot::GetStartAP() const
{
	if ( IsAccidental() )
		return 0;
	if ( pUS->animator.IsAiming() )
		return pUS->GetActionAP( NRPG::AC_SHOOT ) + nExtraAP;
	else
		return pUS->GetActionAP( NRPG::AC_PREPARE_AND_SHOOT ) + nExtraAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::CalculateExtraAP()
{
	nExtraAP = 0;
	NRPG::CWeaponItem *pWeapon = pUS->GetUnitRPG()->GetRPGUnit()->GetWeaponItem();
	if ( IsValid( pWeapon ) && pWeapon->GetShootMode() == NDb::SM_Careful )
		nExtraAP = pUS->GetCarefulShotExtraAP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShoot::SpendAP()
{
	if ( IsAccidental() )
		return;
	if ( pUS->animator.IsAiming() )
		pUS->DoAction( NRPG::AC_SHOOT );
	else
		pUS->DoAction( NRPG::AC_PREPARE_AND_SHOOT );
	pUS->SpendAP( nExtraAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootTile
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecShootTile::CExecShootTile( CUnitServer *_pUS, const CVec3 &_ptTarget ): 
	CExecShoot(_pUS, 0), eHL( NAI::THL_LOWER )
{
	ptAnimTarget = _ptTarget;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecShootTile::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	EUnitCommandResult result = CExecShoot::CanDoIt( from );
	if ( result != UCR_OK )
		return result;
	//
	CWorld *pWorld = pUS->GetWorld();
	vector<NRPG::CAttackPortion> attack;
	pUS->GetUnitRPG()->CreateAttack( &attack, false );
	CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( from ), 
		attack[0], pUS, ptAnimTarget, pUS->GetMinClearDistance() );
	if ( !NRPG::CanShoot( pCover ) )
		return UCR_GENERAL_FAILURE;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShootTile::PrepareShot() // false, ����� ��������� �������
{
	bComplete = CreateAttack( &Attack, 0, true );
	bComplete = CheckBurst( bComplete );
	//
	if ( !Attack.empty() )
	{
		int nTmpToHit;
		bool bTmpMissed;
		CPtr<CWorld> pWorld = pUS->GetWorld();
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( pUS->GetPosition() ), 
			Attack[0], pUS, ptAnimTarget, pUS->GetMinClearDistance() );
		fRayToHit = NRPG::CheckTileToHit( pUS, ptAnimTarget, 
			GetExtraAP(), eHL, pCover, pWorld->IsFirstTurn(), &nTmpToHit );
		NRPG::PeekRay( pCover, &ray, fRayToHit, &bTmpMissed );
		bMissed &= bTmpMissed;
		nToHit = max( nToHit, nTmpToHit );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecShootUnit::CExecShootUnit( CUnitServer *_pUS, NWorld::CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP ):
		CExecShoot( _pUS, _nExtraAttackAP ), pTarget(_pTarget), eHL(_eHL)
{ 
	if ( IsValid( pTarget ) )
	{
		if ( eHL == NAI::HL_ANY )
			pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptAnimTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), NAI::HL_BODY );
		else
			pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptAnimTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), eHL );
	}
	else
	{
		ptAnimTarget = pUS->GetPosition().GetCP();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecShootUnit::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	bIgnoreTarget = bIgnoreTarget || !IsValid( pTarget );
	EUnitCommandResult result = CExecShoot::CanDoIt( from, bIgnoreTarget );
	if ( result != UCR_OK )
		return result;
	if ( bIgnoreTarget )
		return UCR_NO_TARGET;
	//
	CWorld *pWorld = pUS->GetWorld();
	vector<NRPG::CAttackPortion> attack;
	pUS->GetUnitRPG()->CreateAttack( &attack, false );
	CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCovers( pUS->GetAttackOrigin( from ), attack[0], pUS, pTarget, eHL, pUS->GetMinClearDistance() );
	if ( !NRPG::CanShoot( pCover ) )
		return UCR_GENERAL_FAILURE;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShootUnit::PrepareShot() // false, ����� ��������� �������
{
	bComplete = CreateAttack( &Attack, pTarget );
	bComplete = CheckBurst( bComplete );
	//
	if ( !Attack.empty() )
	{
		int nTmpToHit;
		bool bTmpMissed;
		CPtr<CWorld> pWorld = pUS->GetWorld();
		vector<int> accessibleHLs; // ���������� ������ ��� ���������� Melee ToHit
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCovers( pUS->GetAttackOrigin(), Attack[0], pUS, pTarget, eHL, pUS->GetMinClearDistance() );
		fRayToHit = NRPG::CheckToHit( pUS, pTarget, GetExtraAP(), eHL, accessibleHLs, pCover, pWorld->IsFirstTurn(), &nTmpToHit );
		NRPG::PeekRay( pCover, &ray, fRayToHit, &bTmpMissed );
		bMissed &= bTmpMissed;
		nToHit = max( nToHit, nTmpToHit );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShootUnit::CheckShotResult()
{
	csRPG << CC_WHITE << "}\n";
	CWorld *pWorld = pUS->GetWorld();
	//
	if ( !bMissed )
	{
		if ( nToHit > 30 )
			pWorld->GetGlobalAck()->OnTargetHit( pUS );
		else
			pWorld->GetGlobalAck()->OnHardTargetHit( pUS );
	}
	else 
	{
		if ( nToHit > 60 )
			pWorld->GetGlobalAck()->OnTargetMissed( pUS );
		// ��������� interrupt
		CDynamicCast<CUnitServer> pUnit( pTarget );
		if ( IsValid(pUnit) )
		{
			SInterruptInfo info;
			info.AddEvent( pTarget, pUS, true );
			pWorld->CheckInterrupt( &info );
		}
	}
	// retail CExecShootUnit::CheckShotResult (wUnitAttackExec.c:8205): fire the lua hook for the unit that was
	// shot at (target may be null/invalid -> nil), regardless of hit/miss, before clearing the snipe state.
	NScript::luaCallFunction( "OnShotAtUnit", "p", IsValid( pTarget ) ? pTarget.GetBarePtr() : 0 );
	//
	pUS->CancelSnipe();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecShootUnit::IsAttackCanceled()
{
	CPtr<NRPG::IUnitMission> pRPG = pUS->GetUnitRPG();
	CPtr<NRPG::IWeaponItem> pWeapon = pRPG->GetWeaponItem();
	bool bCanStopBurst = pRPG->HasPerk( NRPG::N_PERK_LONG_BURST_AUTO_STOP ) ||
		random.Check( pRPG->GetSkillValue( NDb::ST_BURST ) );
	if ( !IsValid( pTarget ) )
		return true;
	if ( IsValid( pWeapon ) && pWeapon->GetShootMode() == NDb::SM_LongBurst && 
		!pTarget->CanFight() && bCanStopBurst )
			return true;
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMelee
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecMelee::CExecMelee( CUnitServer *_pUS, int _nExtraAP ): 
	CExecAttack(_pUS), nExtraAP(_nExtraAP)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecMelee::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	if ( !IsWithinHumanReach( from.GetCP(), ptTarget, F_MELEE_DISTANCE ) )
		return UCR_TARGET_OUT_OF_RANGE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecMelee::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_MELEE ) + nExtraAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMelee::Start()
{
	const NAI::SUnitPosition &position = pUS->GetPosition();
	pUS->DoAction( NRPG::AC_MELEE );
	pUS->SpendAP( nExtraAP );
	pUS->animator.CloseAttack( position, NAI::GetBlowHeight( position, ptTarget ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMelee::PerformAttack( const vector<NRPG::CAttackPortion> &attack, const CRay &ray, float fHit )
{
	vector<NRPG::IAttackable*> ignores;
	CCannon *pCannon = pUS->animator.GetCannon();
	if ( pCannon )
		ignores.push_back( pCannon );
	else
		ignores.push_back( pUS );

	for ( vector<NRPG::CAttackPortion>::const_iterator i = attack.begin(); i != attack.end(); ++i )
	{
		pUS->GetWorld()->GetGame()->ProcessMeleeAttackPortion( *i, ray, ignores );
	}

	if ( !attack.empty() )
		pUS->GetWorld()->MakeAISound( NDb::GetAISound( 19 ), pUS, 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMeleeTile
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecMeleeTile::CExecMeleeTile( CUnitServer *_pUS, const CVec3 &_ptTarget ):
	CExecMelee( _pUS, 0 )
{
	ptTarget = _ptTarget;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecMeleeTile::OnLabel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	CWorld *pWorld = pUS->GetWorld();
	const NAI::SUnitPosition &position = pUS->GetPosition();
	bool bComplete = true;

	CRay ray;
	ray.ptOrigin = position.GetEyePosition();
	ray.ptDir = ptTarget - ray.ptOrigin;
	Normalize( &ray.ptDir );
	vector<NRPG::CAttackPortion> attack;
	bComplete = CreateAttack( &attack, 0 );
	ASSERT( bComplete );
	PerformAttack( attack, ray );
	return !bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMeleeUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecMeleeUnit::CExecMeleeUnit( CUnitServer *_pUS, CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP ):
	CExecMelee(_pUS, _nExtraAttackAP), pTarget(_pTarget), eHL(_eHL) 
{ 
	bIsHitLocationShot = (eHL != NAI::HL_ANY);

	if ( IsValid( pTarget ) )
		ptTarget = pTarget->GetPosition().GetCP();
	else
		ptTarget = _pUS->GetPosition().GetCP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMeleeUnit::Start()
{
	CWorld *pWorld = pUS->GetWorld();
	if ( NAI::HL_ANY == eHL )
	{
		// �������� ���� ����� ����
		vector<int> hls;
		pWorld->GetAIMap()->GetAccessibleUnitHL( &hls, pUS->GetPosition().GetCenter(), pWorld->GetAIMap()->GetHull(pTarget), F_MELEE_DISTANCE );
		if ( !hls.empty() )
		{
			static SRand rnd;
			eHL = (NAI::EHitLocation)hls[rnd.Get( hls.size() )];
		}
		else
			ASSERT( 0 );
	}
	pWorld->GetAIMap()->GetUnitHLPos( &ptTarget, pWorld->GetAIMap()->GetHull(pTarget), eHL );
	CExecMelee::Start();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecMeleeUnit::OnLabel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	CWorld *pWorld = pUS->GetWorld();
	const NAI::SUnitPosition &position = pUS->GetPosition();
	bool bComplete = true;
	
	pUS->GetUnitRPG()->NextBullet();
	csRPG << CC_WHITE << "Melee " << pRPG->GetName() << "\n{\n";
	vector<NRPG::CAttackPortion> attack;
	bComplete = CreateAttack( &attack, pTarget );
	ASSERT( bComplete );
	if ( !attack.empty() )
	{
		vector<int> accessibleHLs; // ���������� ������ ��� ���������� Melee ToHit
		//
		if ( bIsHitLocationShot )//-1 == eHL )//pCmd->nSpentAP )
			accessibleHLs.push_back( eHL );
		else
			pWorld->GetAIMap()->GetAccessibleUnitHL( &accessibleHLs, position.GetCenter(), pWorld->GetAIMap()->GetHull(pTarget), F_MELEE_DISTANCE );

		CVec3 ptAttackPos = NRPG::GetMeleeAttackPos( pUS, ptTarget );
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCovers( ptAttackPos, attack[0], pUS, pTarget, eHL, 0 );
		//
		int nToHit;
		float fHit = NRPG::CheckToHit( pUS, pTarget, 
			GetExtraAP(), eHL, accessibleHLs, pCover, pWorld->IsFirstTurn(), &nToHit );
		CRay ray;
		bool bIsMiss;
		if ( NRPG::PeekRay( pCover, &ray, fHit, &bIsMiss ) )
			PerformAttack( attack, ray, fHit );
			//PerformAttack( attack, ray, bIsMiss, fHit );
	}
	csRPG << "}\n";
	return !bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecThrowGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecThrowGrenade::CExecThrowGrenade( CUnitServer *_pUS, const CVec3 &_ptTarget ): 
	CCommandExecute(_pUS), ptTarget(_ptTarget)
{
	CDynamicCast<NRPG::IGrenadeItem> pGrenade( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	ASSERT( IsValid(pGrenade) );
	int nDistance = fabs( ptTarget - pUS->GetPosition().GetCP() ) / FP_GRID_STEP;
	pToHitCalcer = new NRPG::CGrenadeToHitCalcer( pUS, pUS->GetPosition().GetPose(),
		nDistance, pUS->GetPosition().GetCP(), pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecThrowGrenade::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	CDynamicCast<NRPG::IGrenadeItem> pGrenade( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	if ( !IsValid( pGrenade ) )
		return UCR_INVALID_COMMAND;

	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	if ( pGrenade->GetMode() != NRPG::GM_THROW )
		return UCR_GENERAL_FAILURE;

	return CanUnitThrowGrenade( pUS, from, ptTarget, pGrenade );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecThrowGrenade::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_THROW_GRENADE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecThrowGrenade::CheckToHitAndDelay( NDb::CRPGGrenade *pGrenade )
{
	// ��������� ToHit � ������� ����� � ������� ������������� ������ �������
	int nToHit = pToHitCalcer->GetToHit();
	pToHitCalcer->Log();

	// ������� �������������� �������� ������ �������
	int nRandom = random.Get(100);
	if ( nRandom > nToHit )
	{
		// �������������
			// ���� ���� ������������� ������� �������
		float fD = 0.2f * fabs( grenadeParams.vel );
		grenadeParams.vel.x += random.GetFloat( -fD, +fD );
		grenadeParams.vel.y += random.GetFloat( -fD, +fD );
		grenadeParams.vel.z += random.GetFloat( -fD, +fD );
	}
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << " \tCheck:" << nRandom;

	// ��������������� ����� ����� � ��������
	grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float(pGrenade->nMaxDelay) );
	grenadeParams.fT = pGrenade->nMaxDelay - grenadeParams.fT;
	csRPG << " True delay: " << grenadeParams.fT;
	nRandom = random.Get(100);
	if ( nRandom >= 99 || nRandom >= pToHitCalcer->GetSkill() ) 
	{
		// ������ ����� ��������
		grenadeParams.fT *= random.GetFloat( 0.5f, 2.f );
		grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float(pGrenade->nMaxDelay) * 0.75f );
	}
	csRPG << " Used delay: " << grenadeParams.fT << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecThrowGrenade::ThrowGrenade()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	CUnitServer::SResItem item;
	CDynamicCast<NRPG::IGrenadeItem> pGrenade( pInventory->GetActive() );
	if ( !IsValid( pGrenade ) )
		return;

	FindGrenadeParams( pUS->GetWorld(), pUS->GetPosition(), ptTarget, pToHitCalcer->GetMaxGrenadeVelocity(), &grenadeParams );
	CheckToHitAndDelay( pGrenade->GetDBGrenade() );

	pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), !pUS->IsAIUnit() );

/*	{
		NDb::EItemSubType subType = pInventory->GetActive()->GetDBItem()->subType;
		int nPlace = pInventory->GetPlaceBySubType( subType );
		pUS->animator.ActivateItem( pUS->GetPosition(), false, nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType() );
	}*/

	pUS->GetWorld()->ThrowGrenade( grenadeParams.ptStart, grenadeParams.vel, 
		pUS->animator.GetTimeLabel1(), grenadeParams.fT, item.pModel, 
		pGrenade->GetDBGrenade(), pUS ); 
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecThrowGrenade::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	ASSERT( pUS->CanSpendAP( GetStartAP() ) );

	if ( !FindGrenadeParams( pUS->GetWorld(), pUS->GetPosition(), ptTarget, pToHitCalcer->GetMaxGrenadeVelocity(), &grenadeParams ) )
	{
		Failed();
		return;
	}
	CWorld *pWorld = pUS->GetWorld();
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	const NAI::SUnitPosition position = pUS->GetPosition();
	pRPG->StartAttack();
	//
	pUS->DoAction( NRPG::AC_THROW_GRENADE );
	pUS->animator.ThrowGrenade( position, grenadeParams.ptOriginalTarget, grenadeParams.nSide );
	StartAction( pUS->GetWorld(), NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecThrowGrenade::TimeLabelReached()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	pRPG->NextBullet();
	ThrowGrenade();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecLaunchRocket
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecLaunchRocket::CExecLaunchRocket( CUnitServer *_pUS, const CVec3 &_ptTarget ):
	CExecShoot(_pUS, 0), type(NORMAL)
{
	ptAnimTarget = _ptTarget;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecLaunchRocket::CExecLaunchRocket( CUnitServer *_pUS, EType _type )
	: CExecShoot(_pUS, 0), type(_type)
{
	ptAnimTarget = CVec3(0,0,0); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecLaunchRocket::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !HasBazookaAndRockets( pUS ) )
		return UCR_NO_EQUIPMENT;

	if ( type != NORMAL )
		return UCR_OK;

	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	CDynamicCast<NRPG::IWeaponItem> pWeapon( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	return CanUnitLaunchRocket( pUS, from, ptAnimTarget, nExtraAP, pWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecLaunchRocket::Start()
{
	if ( IsAccidental() )
		pUS->GetBarrelDir( &ray );
	else
	{
		ray.ptOrigin = pUS->GetAttackOrigin();
		ray.ptDir = ptAnimTarget - ray.ptOrigin;
		Normalize( &ray.ptDir );
	}
	//
	if ( CanDoIt( pUS->GetPosition() ) == UCR_OK )
		CExecShoot::Start();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecLaunchRocket::LaunchRocket( )
{
	vector<NRPG::CAttackPortion> attack;
	pUS->GetUnitRPG()->CreateAttack( &attack, true );
	if ( attack.empty() )
		return;
	//
	NRPG::IWeaponItem *pWeapon = pUS->GetUnitRPG()->GetWeaponItem();
	NRPG::IClipItem *pRocket = pUS->GetUnitRPG()->GetWeaponItem()->GetInnerClip();
	NDb::CRPGItem *pRPGRocket = pRocket->GetDBItem();
	float fSpeed = pWeapon->GetDBWeapon()->fTrailSpeed;
	SRand rnd;
	CPtr<NDb::CModel> pModel = pWeapon->GetDBWeapon()->pTrailEffect->CreateModel( &rnd );
	CPtr<NDb::CEffect> pEffect;
	if ( pWeapon->GetDBWeapon()->pTrailParticle )
		pEffect = pWeapon->GetDBWeapon()->pTrailParticle->GetEffect( &rnd );
	CPtr<NRPG::CRLauncherToHitCalcer> pToHitCalcer;
	int nDistance = fabs( ptAnimTarget - pUS->GetPosition().GetCP() ) / FP_GRID_STEP;
	pToHitCalcer = new NRPG::CRLauncherToHitCalcer( pUS,
		pUS->GetPosition().GetPose(), nDistance, pUS->GetPosition().GetCP(), 100.f, nExtraAP,
		pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptAnimTarget );
	//
	if ( !IsAccidental() )
	{
		int nToHit = pToHitCalcer->GetToHit();
		CObj<NRPG::CCoverInfo> pCover = pUS->GetWorld()->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( pUS->GetPosition() ), 
			attack[0], pUS, ptAnimTarget, pUS->GetMinClearDistance() );
		//
		if ( !NRPG::PeekRayForRocket( pCover, &ray, random.Get( 1, 100 ) <= nToHit ) )
		{
			ASSERT( 0 );
			return;
		}
	}
	//
	CVec3 speed = ray.ptDir * fSpeed;
	ray.ptOrigin += ray.ptDir * pUS->GetMinClearDistance();
	STime tThrow = pUS->animator.GetTimeLabel1();
	if ( IsAccidental() )
		tThrow = pUS->GetWorld()->GetTime()->GetValue();
	pUS->GetWorld()->LaunchRocket( ray.ptOrigin, speed, tThrow, 
		pToHitCalcer->GetMaxDistance() * FP_GRID_STEP, pModel, attack.front(), pRocket, pUS, pEffect );
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecLaunchRocket::OnLabel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	pRPG->NextBullet();
	LaunchRocket();
	if ( !IsAccidental() )
		pUS->DoAction( NRPG::AC_END_SHOOT );
	pUS->CreateFlash();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecPanzerklein
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecPanzerklein::CExecPanzerklein( CUnitServer *_pUS, CCmdTakeCorpse *_pCmd ): 
	CCommandExecute(_pUS), pCmd(_pCmd)
{
	if ( pUS->IsWearingPK() )
		action = NRPG::AC_LEAVE_PK;
	else
		action = NRPG::AC_ENTER_PK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecPanzerklein::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	// ... here we check some conditions about PK

	if ( !IsValid( pCmd ) )
	{
		if ( bIgnoreTarget )
			return UCR_NO_TARGET;

		if ( action == NRPG::AC_ENTER_PK )
			return UCR_INVALID_COMMAND;
	}
	else if ( action == NRPG::AC_ENTER_PK )
	{
		CDynamicCast<CUnitServer> pPK = pCmd->pCorpse;
		NRPG::CDynamicSkill *pVP = &pPK->GetRPG()->GetRPGUnit()->Skills( NDb::ST_VP );
		if ( *pVP < 0 )
			return UCR_UNAVAILABLE;
	}

	return UCR_OK; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecPanzerklein::GetStartAP() const
{
	return pUS->GetActionAP( action );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecPanzerklein::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	pUS->DoAction( action );
  /*
  pUS->animator.Panzerklein( pUS->GetPosition(), action == AC_ENTER_PK );
	*/
	StartAction( pUS->GetWorld(), SKIPPABLE );
	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecPanzerklein::TimeLabelReached()
{
	if ( action == NRPG::AC_LEAVE_PK )
	{
		CUnitServer *pPK = pUS->GetWearingPK();
		CVec3 ptPos = pUS->GetPosition().GetCP();
		float fAngle = pUS->GetPosition().GetDirection();
		pUS->GetWorld()->GetPathNetwork()->Unlock( pUS );
		pUS->FlipPanzerklein( 0 );

		CVec3 ptPKPos( ptPos.x - 1.45f * cos(fAngle), ptPos.y - 1.45f * sin(fAngle), ptPos.z );
		NAI::SPosition pos = NAI::GetNearestPosition( ptPKPos, pPK->GetWorld()->GetPathNetwork(), true, ptPos + CVec3( 0, 0, 1.0f ) );
		NAI::SUnitPosition posPK;
		posPK.pos = pos;
		posPK.SetPose( NAI::CRAWL );
		posPK.pos.p.SetDirection( pUS->GetPosition().GetDir() );
		posPK.bRun = false;
		pPK->SetPosition( posPK );
		pPK->WearAsPK( false );
	}
	else
	{
		CDynamicCast<CUnitServer> pPK = pCmd->pCorpse;
		if ( !pPK->WearAsPK( action == NRPG::AC_ENTER_PK ) )
			return false;
		NAI::SUnitPosition pos = pPK->GetPosition();
		pos.bRun = false;
		pos.pos.p.SetPose( NAI::CM_STAND );
		pUS->SetPosition( pos );
		pUS->FlipPanzerklein( pPK );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecUsePassage
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecUsePassage::CExecUsePassage( CUnitServer *_pUS, CCmdUsePassage *_pCmd ):
	CCommandExecute(_pUS), pCmd( _pCmd )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecUsePassage::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecUsePassage::GetStartAP() const
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecUsePassage::Run()
{
	CDynamicCast<NWorld::IPassageObject> pPassage(pCmd->pPassageObject);
	if (pPassage)
		pPassage->UsePassageObject( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecCannon::CExecCannon( CUnitServer *_pUS, IObject *_pCannon, bool _bEnter ): 
	CCommandExecute(_pUS), pCannon(_pCannon), bEnter(_bEnter)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecCannon::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !bEnter )
		return UCR_OK;
	if ( !IsValid( pCannon ) )
		return UCR_GENERAL_FAILURE;

	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	CDynamicCast<ICannon> pC( pCannon );
	if ( !pC || pC->IsBroken() || pC->IsOccupied() )
		return UCR_GENERAL_FAILURE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecCannon::GetStartAP() const
{
	if ( bEnter )
		return pUS->GetActionAP( NRPG::AC_APPROACH_CANNON );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCannon::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	CDynamicCast<CCannon> pOS( pCannon );
	ASSERT( pOS );
	if ( bEnter )
	{
		pUS->DoAction( NRPG::AC_APPROACH_CANNON );
		pUS->GetUnitRPG()->SetCannonItem( pOS->GetItem() );
		CDynamicCast<NRPG::CWeaponItem> pWeaponItem( pOS->GetItem() );
		ASSERT( pWeaponItem );
		pUS->animator.SetWeaponName( pWeaponItem->GetDBWeapon()->szAnimName.c_str() );
		pUS->animator.EnterCannon( pUS->GetPosition(), pOS );
		pOS->SetCurrentUnit( pUS );
		pUS->SetState( new CUnitStateUsingCannon( pUS, pOS ) );
	}
	else
	{
		pUS->GetUnitRPG()->SetCannonItem(0);
		pUS->animator.LeaveCannon( pUS->GetPosition() );
		pOS->SetCurrentUnit(0);
		pUS->SetState( new CUnitStateNormal( pUS ) );
	}
	StartAction( pUS->GetWorld(), SKIPPABLE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCorpse
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecCorpse::CExecCorpse( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bTake ): 
	CCommandExecute(_pUS), pDeadUnit(_pCorpse), bTake(_bTake)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecCorpse::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	if ( !IsValid( pDeadUnit ) )
		return UCR_INVALID_COMMAND;
	if ( bTake && pDeadUnit->GetCorpseCarrier() )
		return UCR_GENERAL_FAILURE;
	if ( !bTake && pDeadUnit->GetCorpseCarrier() != pUS )
		return UCR_GENERAL_FAILURE;
	if ( !bTake && !CanDropCorpse( pUS ) )
		return UCR_GENERAL_FAILURE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecCorpse::GetStartAP() const 
{ 
	if ( bTake )
		return pUS->GetActionAP( NRPG::AC_TAKE_CORPSE ); 
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCorpse::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	ASSERT( pUS->CanSpendAP( GetStartAP() ) );
	if ( GetState() == FINISHED ) 
		return;
	if ( pDeadUnit->IsWearingPK() ) 
	{
		CUnitServer *pPK = pDeadUnit->GetWearingPK();
		pPK->SetPosition( pDeadUnit->GetPosition() );
		pPK->WearAsPK( false );
		pDeadUnit->FlipPanzerklein( 0 );
		CVec2 ptDir( 1, 0 );
		pDeadUnit->animator.Die( pDeadUnit->GetPosition(), CVec3( ptDir.x, ptDir.y, 0 ) );
		pUS->GetWorld()->GetPathNetwork()->Unlock( pDeadUnit );
		return;
	}
	if ( bTake )
	{
		pUS->DoAction( NRPG::AC_TAKE_CORPSE );
		OutputDebugString("Taking corpse in Run()\n");
		pUS->animator.TakeCorpse( pUS->GetPosition() );
	}
	else
	{
		pUS->animator.DropCorpse( pUS->GetPosition() );
		NDb::CAISound *pAISound = NDb::GetAISound( 27 );
		pUS->GetWorld()->MakeAISound( pAISound, pUS, 0, 0 );
	}
	StartAction( pUS->GetWorld(), SKIPPABLE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecCorpse::TimeLabelReached()
{
	ASSERT( pDeadUnit );
	if ( pDeadUnit->IsWearingPK() ) 
		return false;
	if ( bTake )
	{
		pDeadUnit->animator.BeTaken( pUS, &pUS->animator );
		pUS->SetState( new CUnitStateCorpseCarrier( pUS, pDeadUnit ) );
	}
	else
		pUS->SetState( new CUnitStateNormal( pUS ) );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCorpse::Cancel()
{
	if ( bTake )
	{
		pUS->animator.DropCorpse( pUS->GetPosition() );
		pDeadUnit->animator.BeDropped();
		pDeadUnit->animator.CalmCorpse();
		pUS->SetState( new CUnitStateNormal( pUS ) );
	}
	else
	{
		OutputDebugString("Taking corpse in Cancel()\n");
		pUS->animator.TakeCorpse( pUS->GetPosition() );
		pDeadUnit->animator.BeTaken( pUS, &pUS->animator );
		pUS->SetState( new CUnitStateCorpseCarrier( pUS, pDeadUnit ) );
	}

	pUS->animator.AlignTime( 50 );
	pUS->animator.PlaceUnit( pUS->GetPosition() );

	StopAction();
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTakeCorpseOnDeploy
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecTakeCorpseOnDeploy::CExecTakeCorpseOnDeploy( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bDead ):
	CCommandExecute(_pUS), pDeadUnit(_pCorpse), bDead( _bDead ), n( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecTakeCorpseOnDeploy::CanDoIt( const NAI::SUnitPosition &from, 
	bool bIgnoreTarget ) const
{
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecTakeCorpseOnDeploy::GetStartAP() const
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecTakeCorpseOnDeploy::Run()
{
	ASSERT( IsValid( pDeadUnit ) );
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pDeadUnit ) || !IsValid( pUS ) )
		return;
	//
	if ( !bDead )
		pDeadUnit->SetState( new CUnitStateUnconscious( pDeadUnit ) );
	else
		pDeadUnit->SetState( new CUnitStateDeath( pDeadUnit ) );
	pDeadUnit->GetWorld()->GetPathNetwork()->Unlock( pDeadUnit );
	pDeadUnit->InitAsCorpse( bDead );
	pDeadUnit->animator.InitAsCorpse( pDeadUnit->GetPosition() );
	int nCorpseDir = (int)( pUS->GetPosition().GetDir() ) - 4;
	if ( nCorpseDir < 0 )
		nCorpseDir += 8;
	else if ( nCorpseDir > 7 )
		nCorpseDir -= 8;
	NAI::SUnitPosition pos = pUS->GetPosition();
	pos.pos.p.SetDirection( nCorpseDir );
	pDeadUnit->SetPosition( pos );
	pDeadUnit->GetWorld()->GetPathNetwork()->Unlock( pDeadUnit );
	//
	pUS->animator.InitAsCorpseCarrier( pUS->GetPosition() );
	pUS->SetState( new CUnitStateCorpseCarrier( pUS, pDeadUnit ) );
	pDeadUnit->animator.BeTaken( pUS, &pUS->animator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecTakeCorpseOnDeploy::TimeLabelReached()
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecHeal
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecHeal::CExecHeal( CUnitServer *_pUS, CUnitServer *_pTarget ): 
	CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecHeal::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	return CanDoFirstAid( pUS, from, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecHeal::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_FIRSTAID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecHeal::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	CVec3 ptTarget;
	pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), -1 );
	pUS->animator.StartHealing( pUS->GetPosition(), NAI::GetBlowHeight( pUS->GetPosition(), ptTarget ) );
	StartAction( pUS->GetWorld(), SKIPPABLE );
	pUS->DoAction( NRPG::AC_FIRSTAID );

	NDb::CAISound *pAISound = NDb::GetAISound( 20 );
	pUS->GetWorld()->MakeAISound( pAISound, pUS, 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecHeal::AnimationFinished()
{
	StopAction();
	Finished();
	//
	pUS->SetState( new CUnitStateHealer( pUS, pTarget ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecSetTrap::CExecSetTrap( CUnitServer *_pUS, CWindowDoor *_pTarget ): 
	CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecSetTrap::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !IsValid( pTarget ) )
		return UCR_NO_TARGET;
	if ( pUS->GetTBSPlayer()->CanSeeTrap( pTarget ) && pTarget->IsMineSet() )
		return UCR_GENERAL_FAILURE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecSetTrap::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_TRAP_OBJECT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecSetTrap::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	pUS->DoAction( NRPG::AC_TRAP_OBJECT );
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), NORMAL );
//	NDb::CAISound *pAISound = NDb::GetAISound( 20 ); // click
//	pUS->GetWorld()->MakeAISound( pAISound, pUS, 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecSetTrap::TimeLabelReached()
{
	if ( IsValid(pTarget) )
	{
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		NRPG::IInventory *pInventory = pRPG->GetInventory();
		NRPG::IInventoryItem *pItem = pInventory->GetActive();
		CDynamicCast<NRPG::IGrenadeItem> pGrenade(pItem);
		if (pGrenade)
		{
			NDb::CRPGGrenade *pRPGGrenade = pGrenade->GetDBGrenade();
			if ( pTarget->SetTrap( pRPGGrenade, pRPG->GetGrenadeTrapDC( pRPGGrenade ) ) )
			{
				CUnitServer::SResItem item;
				pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), true );
				pUS->AddToVisibleTraps( pTarget );
			}
		}
		else
			ASSERT(0);
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecDisarmTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
static NRPG::IToolItem* GetMineClearingTool( CUnitServer *pUS )
{
	ASSERT( IsValid( pUS ) );
	if ( IsValid( pUS ) )
	{		
		CPtr<NRPG::IInventoryInfo> pInventory = pUS->GetUnitRPG()->GetInventoryInfo();
		if ( IsValid( pInventory ) )
		{
			NDb::ESlot active = ( NDb::ESlot )pInventory->GetActiveSlot();
			CDynamicCast<NRPG::IToolItem> pTool( pInventory->Get( active ) );
			if ( IsValid( pTool ) && pTool->GetDBItemInfo()->bCanUseForMineCleaning )
				return pTool.GetPtr();
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecDisarmTrap::CExecDisarmTrap( CUnitServer *_pUS, CWindowDoor *_pTarget ) : 
	CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecDisarmTrap::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !IsValid( pTarget ) || bIgnoreTarget )
		return UCR_NO_TARGET;
	if ( !pUS->GetTBSPlayer()->CanSeeTrap( pTarget ) || !pTarget->IsMineSet() )
		return UCR_GENERAL_FAILURE;
	if ( !IsValid( GetMineClearingTool( pUS ) ) )
		return UCR_GENERAL_FAILURE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecDisarmTrap::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_DISARM_TRAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecDisarmTrap::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	pUS->DoAction( NRPG::AC_DISARM_TRAP );
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecDisarmTrap::TimeLabelReached()
{
	if ( IsValid(pTarget) )
	{
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		CPtr<NRPG::IToolItem> pTool = GetMineClearingTool( pUS );
		ASSERT( IsValid( pTool ) );
		if ( IsValid( pTool ) )
		{
			if ( !pRPG->CanClear( pTarget->GetMineDC(), pTool->GetSkillModifForMineCleaning() ) )
			{
				pTarget->GoBoom( pUS );
				// retail fires OnMineDiffusion( object, result ) in BOTH branches: 1 = detonated (clear failed),
				// 2 = disarmed. Retail's mine path uses "pb" (bool) which the dev marshaller can't deliver -- use
				// "pi" like the retail trap path so the script's OnMineDiffusion(object,result) gets the result.
				NScript::luaCallFunction( "OnMineDiffusion", "pi", pTarget.GetBarePtr(), 1 );
			}
			else
			{
				AddItemToUnit( pUS, pTarget->DisarmMine() );
				NScript::luaCallFunction( "OnMineDiffusion", "pi", pTarget.GetBarePtr(), 2 );
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetMine
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IMineItem* CExecSetMine::GetMine() const
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	NRPG::IInventoryItem *pItem = pInventory->GetActive();
	CDynamicCast<NRPG::IMineItem> pMine(pItem);
	if (pMine)
		return pMine;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecSetMine::GetMinesNearTarget( vector<CPtr<CMine> > *pRes ) const
{
	vector<CVec3> places;
	places.push_back( pCmd->ptDst.GetCP() );
	pUS->GetWorld()->GetMineTracker()->GetMines( places, pRes );
	return !pRes->empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecSetMine::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !GetMine() )
		return UCR_NO_EQUIPMENT;
	vector<CPtr<CMine> > res;
	GetMinesNearTarget( &res );
	for ( int k = 0; k < res.size(); ++k )
	{
		if ( pUS->GetTBSPlayer()->CanSeeTrap( res[k] ) )
			return UCR_GENERAL_FAILURE;
	}
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecSetMine::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_SET_MINE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecSetMine::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	pUS->DoAction( NRPG::AC_SET_MINE );
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecSetMine::TimeLabelReached()
{
	vector<CPtr<CMine> > res;
	if ( GetMinesNearTarget( &res ) )
	{
		for ( int k = 0; k < res.size(); ++k )
			res[k]->GoBoom( pUS );
	}
	else
	{
		NRPG::IMineItem *pMine = GetMine();
		if ( pMine )
		{
			NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
			NDb::CRPGMine *pRPGMine = pMine->GetDBItemInfo();
			CMine *pRes = new CMine( pUS->GetWorld(), pCmd->ptDst.GetCP(), pRPGMine, pRPG->GetMineDC( pRPGMine ), pCmd->ptDst.GetFloor() );
			pUS->AddToVisibleTraps( pRes );
			NRPG::IInventory *pInventory = pRPG->GetInventory();
			CUnitServer::SResItem item;
			pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), true );
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecDisarmMine
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecDisarmMine::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !IsValid( pTarget ) || bIgnoreTarget )
		return UCR_NO_TARGET;
	CDynamicCast<CMine> pMine(pTarget);
	if (pMine)
	{
		if ( !pUS->GetTBSPlayer()->CanSeeObject( pTarget ) || !pUS->GetTBSPlayer()->CanSeeTrap( pTarget ) )
			return UCR_GENERAL_FAILURE;
		// ��������� ����-�� � ��� ����������� tool
		if ( !IsValid( GetMineClearingTool( pUS ) ) )
			return UCR_GENERAL_FAILURE;
	}
	else
		return UCR_GENERAL_FAILURE;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecDisarmMine::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_DISARM_MINE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecDisarmMine::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	pUS->DoAction( NRPG::AC_DISARM_MINE );
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecDisarmMine::TimeLabelReached()
{
	if ( IsValid(pTarget) )
	{
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		CPtr<NRPG::IToolItem> pTool = GetMineClearingTool( pUS );
		ASSERT( IsValid( pTool ) );
		if ( IsValid( pTool ) )
		{
			if ( !pRPG->CanClear( pTarget->GetMineDC(), pTool->GetSkillModifForMineCleaning() ) )
			{
				pTarget->GoBoom( pUS );
				// OnMineDiffusion( object, 1=detonated ); see CExecDisarmTrap above for the "pi"-vs-"pb" note.
				NScript::luaCallFunction( "OnMineDiffusion", "pi", pTarget.GetBarePtr(), 1 );
			}
			else
			{
				AddItemToUnit( pUS, pTarget->DisarmMine() );
				NScript::luaCallFunction( "OnMineDiffusion", "pi", pTarget.GetBarePtr(), 2 );  // 2 = disarmed
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSnipeAim
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecSnipeAim::CExecSnipeAim( CUnitServer *_pUnitServer, CUnitServer *_pTarget ): 
	CCommandExecute(_pUnitServer), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecSnipeAim::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const 
{ 
	if ( !pUS->CanSnipe() )
		return UCR_GENERAL_FAILURE;

	if ( GetActionType( pUS ) == AT_CANNON ) 
	{
		if ( bIgnoreTarget )
			return UCR_NO_TARGET;

		CVec3 ptTarget;
		pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), NAI::HL_BODY );
		return CanAttackWithCannon( pUS->animator.GetCannon(), ptTarget );
	}

	return HasWorkingWeapon( pUS );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecSnipeAim::GetStartAP() const 
{ 
	if ( !pUS->animator.IsAiming() )
		return pUS->GetUnitRPG()->GetActionAP( pUS->GetPosition().GetPose(), NRPG::AC_PREPARE );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecSnipeAim::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	CRay ray;
	ray.ptOrigin = 	pUS->GetAttackOrigin( pUS->GetPosition() );
	ray.ptDir = pTarget->GetPosition().GetCP() - pUS->GetPosition().GetCP();

	StartAction( pUS->GetWorld(), SKIPPABLE );
	pUS->animator.Snipe( pUS->GetPosition(), ray );

	if ( !pUS->IsSniping()  )
	{
		if ( !pUS->animator.IsAiming() )
			pUS->DoAction( NRPG::AC_PREPARE );
		pUS->SetState( new CUnitStateSniping( pUS, pTarget, 0 ) );	
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecSnipeAim::AnimationFinished()
{
	StopAction();
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCollectSnipeAP
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecCollectSnipeAP::CExecCollectSnipeAP ( CUnitServer *_pUnitServer, ECollectSnipeAP _eAP ): 
	CCommandExecute(_pUnitServer), eAP( _eAP )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecCollectSnipeAP::GetResiduaryAP() const
{
	NRPG::SSnipeAP snipeAP = pUS->GetUnitRPG()->GetSavedAP();
	int nSnipeSkill = pUS->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_SNIPE );
	return Max( 0, nSnipeSkill - snipeAP.nAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecCollectSnipeAP::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !pUS->IsSniping() )
		return UCR_INVALID_COMMAND;
	//
	if ( GetResiduaryAP() <= 0 )
		return UCR_GENERAL_FAILURE;
	//
	if ( GetStartAP() <= 0 )
		return UCR_NOT_ENOUGH_AP;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecCollectSnipeAP::GetStartAP() const
{
	int nRes = 0;
	switch( eAP )
	{
		case CSAP_1AP:
			nRes = 1; break;
		case CSAP_10AP:
			nRes = 10; break;
		case CSAP_MAX:
		{
			nRes = pUS->GetUnitRPG()->GetSkillValue( NDb::ST_AP ) -	
				pUS->GetUnitRPG()->GetActionAP( NAI::WALK, NRPG::AC_SHOOT ); 
			break;
		}
		case CSAP_ALL:
			nRes = pUS->GetUnitRPG()->GetSkillValue( NDb::ST_AP ); break;
	}
	//
	return Min( nRes, GetResiduaryAP() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCollectSnipeAP::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	pUS->CollectSnipeAP( GetStartAP() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecThrowKnife
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecThrowKnife::CExecThrowKnife( CUnitServer *_pUS, const CVec3 &_ptTarget, CUnitServer *_pTarget ): 
	CExecAttack(_pUS), ptTarget(_ptTarget), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecThrowKnife::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !HasThrowingKnife( pUS ) )
		return UCR_NO_EQUIPMENT;

	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	CDynamicCast<NRPG::IMeleeWeaponItem> pMelee( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	return CanUnitThrowKnife( pUS, from, ptTarget, pMelee );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecThrowKnife::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_THROW_KNIFE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecThrowKnife::Start()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	const NAI::SUnitPosition position = pUS->GetPosition();
	pUS->DoAction( NRPG::AC_THROW_KNIFE );
	pUS->animator.ThrowKnife( position, ptTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecThrowKnife::OnLabel()
{
	ThrowKnife();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecThrowKnife::ThrowKnife()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	CUnitServer::SResItem item;
	CDynamicCast<NRPG::IMeleeWeaponItem> pMelee( pInventory->GetActive() );
	if ( !IsValid( pMelee ) )
		return;
	vector<NRPG::CAttackPortion> attack;
	CreateAttack( &attack, 0, false );
	if ( attack.empty() )
		return;
	pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), true );

	int nToHit;
	int nDistance = fabs( ptTarget - pUS->GetPosition().GetCP() ) / FP_GRID_STEP;
	float fMaxDist;
	if ( pTarget )
	{
		CPtr<NRPG::CThrowKnifeToHitCalcer> pToHitCalcer =
			new NRPG::CThrowKnifeToHitCalcer( pUS, pUS->GetPosition().GetPose(),
			nDistance, pUS->GetPosition().GetCP(), 100.f, pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), 0, false );
		nToHit = pToHitCalcer->GetToHit();
		pToHitCalcer->Log();
		fMaxDist = pToHitCalcer->GetKnifeMaxDistance() * FP_GRID_STEP;
	}
	else
	{
		CPtr<NRPG::CThrowKnifeTileToHitCalcer> pToHitCalcer =
			new NRPG::CThrowKnifeTileToHitCalcer( pUS, pUS->GetPosition().GetPose(),
			nDistance, pUS->GetPosition().GetCP(), 100.f, pUS->GetWorld()->IsFirstTurn(), false, CVec3(1,1,1), ptTarget, 0 );
		nToHit = pToHitCalcer->GetToHit();
		pToHitCalcer->Log();
		fMaxDist = pToHitCalcer->GetKnifeMaxDistance() * FP_GRID_STEP;
	}

	float fSpeed = 5.f;
	CVec3 speed = ptTarget - item.ptCenter;
	Normalize(&speed);
	speed *= fSpeed;
	if ( random.Get( 1, 100 ) > nToHit )
	{
		// �������������
		// ���� ���� ������������� ������� �����
		float fD = 0.15f * fabs( fSpeed );
		speed.x += random.GetFloat( -fD, +fD );
		speed.y += random.GetFloat( -fD, +fD );
		speed.z += random.GetFloat( -fD, +fD );
	}
	pUS->GetWorld()->ThrowKnife( item.ptCenter, speed, 
		pUS->animator.GetTimeLabel1(), fMaxDist, item.pModel, attack.front(), item.pItem, pUS ); 
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMoveItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecCreateInventoryItem::CExecCreateInventoryItem( CUnitServer *_pUS, CCmdCreateInventoryItem *_pCmd ):
	CCommandExecute(_pUS), pCmd(_pCmd)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecCreateInventoryItem::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	if ( !IsValid( pCmd->GetItem() ) )
		return UCR_INVALID_COMMAND;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCreateInventoryItem::Run()
{
	CObj<NRPG::IInventoryItem> pItem = NRPG::CreateItem( pCmd->GetItem()->pSuccessor );
	CObj<CCmdMoveInventoryItem> pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::BACKPACK, CTPoint<int>( -1, -1 ) ) );
	if ( CanMoveInventoryItem( pUS, pFake ) == UCR_INVENTORY_NO_PLACE )
		pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::GROUND ) );

	MoveInventoryItem( pUS, pFake );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCreateAndActivateInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecCreateAndActivateInventoryItem::CExecCreateAndActivateInventoryItem( CUnitServer *_pUS, CCmdCreateAndActivateInventoryItem *_pCmd ):
	CCommandExecute(_pUS), pCmd(_pCmd)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecCreateAndActivateInventoryItem::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	if ( !IsValid( pCmd->GetItem() ) )
		return UCR_INVALID_COMMAND;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirrors CExecCreateInventoryItem::Run but slots the new item into a hand (nSlot) and activates it,
// vacating the slot to the backpack first if it is occupied. The per-step draw/stow ANIMATION that
// CExecMoveInventoryItem/CExecSetActiveItem play is elided (build-validation scope): the inventory
// STATE is faithful (the proven MoveInventoryItem / Activate primitives), the animation is not replayed.
void CExecCreateAndActivateInventoryItem::Run()
{
	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	int nSlot = pCmd->GetSlot();

	// vacate the target hand slot (move the current item to the backpack) so the new item can be slotted
	if ( IsValid( pInventory->Get( (NDb::ESlot)nSlot ) ) )
	{
		CObj<CCmdMoveInventoryItem> pVacate = new CCmdMoveInventoryItem(
			SItem( pUS, SItem::SLOT, nSlot ), SItem( pUS, SItem::BACKPACK, CTPoint<int>( -1, -1 ) ) );
		if ( CanMoveInventoryItem( pUS, pVacate ) == UCR_OK )
			MoveInventoryItem( pUS, pVacate );
	}

	// create the item from the DB record and slot it into the (now-free) hand
	CObj<NRPG::IInventoryItem> pItem = NRPG::CreateItem( pCmd->GetItem()->pSuccessor );
	CObj<CCmdMoveInventoryItem> pFake = new CCmdMoveInventoryItem(
		SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::SLOT, nSlot, pItem ) );
	if ( CanMoveInventoryItem( pUS, pFake ) == UCR_INVENTORY_NO_PLACE )
		pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::BACKPACK, CTPoint<int>( -1, -1 ) ) );
	MoveInventoryItem( pUS, pFake );

	pInventory->Activate( (NDb::ESlot)nSlot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecExchangeInventoryItems
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecExchangeInventoryItems::CExecExchangeInventoryItems( CUnitServer *_pUS, CCmdExchangeInventoryItems *_pCmd ):
	CCommandExecute(_pUS), pCmd(_pCmd)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecExchangeInventoryItems::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	if ( !IsValid( pCmd->GetSource().pItem ) )
		return UCR_INVALID_COMMAND;

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bring sSource into hand slot nSlot, displacing the in-hand item to the backpack, then (bActivate)
// make the slot active. Same animation elision as CExecCreateAndActivateInventoryItem above.
void CExecExchangeInventoryItems::Run()
{
	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	int nSlot = pCmd->GetSlot();
	NRPG::IInventoryItem *pSource = pCmd->GetSource().pItem.GetPtr();
	if ( !IsValid( pSource ) )
		return;

	// 1) vacate the target hand slot (the displaced item goes to the backpack)
	if ( IsValid( pInventory->Get( (NDb::ESlot)nSlot ) ) )
	{
		CObj<CCmdMoveInventoryItem> pVacate = new CCmdMoveInventoryItem(
			SItem( pUS, SItem::SLOT, nSlot ), SItem( pUS, SItem::BACKPACK, CTPoint<int>( -1, -1 ) ) );
		if ( CanMoveInventoryItem( pUS, pVacate ) == UCR_OK )
			MoveInventoryItem( pUS, pVacate );
	}

	// 2) move the source item from the backpack into the now-free hand slot
	CObj<CCmdMoveInventoryItem> pEquip = new CCmdMoveInventoryItem(
		SItem( pUS, SItem::BACKPACK, pSource ), SItem( pUS, SItem::SLOT, nSlot, pSource ) );
	if ( CanMoveInventoryItem( pUS, pEquip ) == UCR_OK )
		MoveInventoryItem( pUS, pEquip );

	// 3) make the slot the active hand
	if ( pCmd->GetActivate() )
		pInventory->Activate( (NDb::ESlot)nSlot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMoveItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecMoveInventoryItem::CExecMoveInventoryItem( CUnitServer *_pUS, CCmdMoveInventoryItem *_p ):
	CCommandExecute(_pUS), pCmd(_p)
{
	pUSTarget = dynamic_cast<CUnitServer*>( pCmd->GetTarget().pUnit.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecMoveInventoryItem::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	return CanMoveInventoryItem( pUS, pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMoveInventoryItem::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	////
	NDb::ESlot slot = (NDb::ESlot)pInventory->GetActiveSlot();
	NRPG::IInventoryItem* pItem = pInventory->Get( slot );
	////
	NDb::EItemSubType subType = NDb::SUBTYPE_NONE;
	if ( IsValid( pItem ) )
		subType = pItem->GetDBItem()->subType;

	IPlayer::SItemInfo sInfo;
	NDb::EItemSubType subTypeNext = NDb::SUBTYPE_NONE;
	if ( ( pCmd->GetSource().eType == SItem::HAND ) && pUS->GetTBSPlayer()->GetInHandItem( &sInfo ) )
		subTypeNext = sInfo.pItem->GetDBItem()->subType;

	NDb::ESlot slotTarget;
	if ( IsValid( pUSTarget ) )
		slotTarget = (NDb::ESlot)pUSTarget->GetUnitRPG()->GetInventory()->GetActiveSlot();

	MoveInventoryItem( pUS, pCmd );

	bool bReadyToFinish = true;

	bool bActive = pUS->animator.IsActiveItem();
	if ( ( pCmd->GetSource().eType != SItem::SLOT || pCmd->GetSource().nSlot != slot ) )
		pUS->SetUndrawItem( !bActive );
	else
		bReadyToFinish = false;

	bool bActiveForTarget = false;
	if ( IsValid( pUSTarget ) )
	{
		bActiveForTarget = pUSTarget->animator.IsActiveItem();
		if ( pCmd->GetTarget().eType != SItem::SLOT || pCmd->GetTarget().nSlot != slotTarget )
			pUSTarget->SetUndrawItem( !bActiveForTarget );
		else
			bReadyToFinish = false;
	}

	if ( bReadyToFinish )
	{
		Finished();
		return;
	}

	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	pUS->animator.SetWeaponAnimation( bActive ? pRPG->GetWeaponType() : NDb::WT_DEFAULT );
	pUS->SetUndrawItem( !bActive );

	if ( !IsValid( pUSTarget ) )
	{
		Finished();
		return;
	}

	if ( !pUSTarget->animator.CanActivateItem() )
	{
		NRPG::IUnitMission *pRPGTarget = pUSTarget->GetUnitRPG();
		pUSTarget->animator.SetWeaponAnimation( bActiveForTarget ? pRPGTarget->GetWeaponType() : NDb::WT_DEFAULT );
		pUSTarget->SetUndrawItem( !bActiveForTarget );
		Finished();
		return;
	}

	StartAction( pUS->GetWorld(), NOBLOCK );
	bTwoHeavy = IsTwoHeavy( subType, subTypeNext );
	nStage = 1;
	if ( !BeginDeactivatingItem( pUS, subType ) )
		AnimationFinished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecMoveInventoryItem::TimeLabelReached()
{
	if ( !nStage )
		return false;
	if ( nStage == 1 )
		pUS->SetUndrawItem( true, bTwoHeavy );
	else
		pUSTarget->SetUndrawItem( false, false );
	return nStage == 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMoveInventoryItem::AnimationFinished()
{
	if ( !nStage )
		return;

	NRPG::IUnitMission *pRPG = pUSTarget->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	NDb::ESlot slot = (NDb::ESlot)pInventory->GetActiveSlot();
	if ( nStage == 2 || !pInventory->Get(slot) )
	{
		Finished();
		return;
	}
	nStage = 2;
	NDb::EItemSubType subType = pInventory->Get(slot)->GetDBItem()->subType;
	if ( subType == NDb::SUBTYPE_HEAVY || subType == NDb::SUBTYPE_MINE_DETECTOR )
		pUSTarget->animator.ActivateItem( pUSTarget->GetPosition(), true, true, NDb::BELT_M1, pRPG->GetWeaponType() );
	else
	{
		int nPlace = pInventory->GetPlaceBySubType( subType );
		pUSTarget->animator.ActivateItem( pUSTarget->GetPosition(), false, 
			nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMoveInventoryItem::Cancel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	bool bActive = pInventory->GetActive() != 0;
	pUS->SetUndrawItem( !bActive );
	pUS->animator.SetWeaponAnimation( bActive ? pRPG->GetWeaponType() : NDb::WT_DEFAULT );
	pUS->animator.SetActiveItem( bActive );
	pUS->animator.AlignTime( 50 );
	pUS->animator.PlaceUnit( pUS->GetPosition() );
	nStage = 0;
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecPlayAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecPlayAnimation::CExecPlayAnimation( CUnitServer *_pUS, int _nDBAnimationID, bool _bCircled ):
	CCommandExecute( _pUS ), nDBAnimationID( _nDBAnimationID ), bCircled( _bCircled )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecPlayAnimation::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecPlayAnimation::Run()
{
	StartAction( pUS->GetWorld(), NORMAL );
	pUS->animator.PlayCustomAnimation( pUS->GetPosition(), nDBAnimationID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecPlayAnimation::AnimationFinished()
{
	if ( bCircled )
		pUS->animator.PlayCustomAnimation( pUS->GetPosition(), nDBAnimationID );
	else
	{
		StopAction();
		Finished();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecPlayAnimation::Cancel()
{
	StopAction();
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTalk
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecTalk::CExecTalk( CUnitServer *_pUS, CUnitServer *_pTarget ):
	CCommandExecute( _pUS ), pTarget( _pTarget )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecTalk::Run()
{
	NScript::luaCallFunction( "OnTalk", "pp", pUS.GetBarePtr(), pTarget.GetBarePtr() );
	int nDialogID = pTarget->GetDialog();
	if ( nDialogID > 0 )
		PlayDialog( pUS->GetWorld(), nDialogID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecTalk::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	if ( pTarget->CanTalk() && pUS->GetUnitRPG()->GetRPGUnit()->IsHero() )
		return UCR_OK;
	else
		return UCR_GENERAL_FAILURE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x00122173, CExecShootTile )
REGISTER_SAVELOAD_CLASS( 0x00122175, CExecCannon )
REGISTER_SAVELOAD_CLASS( 0x00222190, CExecThrowGrenade )
REGISTER_SAVELOAD_CLASS( 0x00422170, CExecShootUnit )
REGISTER_SAVELOAD_CLASS( 0x00422171, CExecMeleeTile )
REGISTER_SAVELOAD_CLASS( 0x00422172, CExecMeleeUnit )
REGISTER_SAVELOAD_CLASS( 0x10422130, CExecOpenClose )
REGISTER_SAVELOAD_CLASS( 0x00422110, CExecCorpse )
REGISTER_SAVELOAD_CLASS( 0x00622140, CExecHeal )
REGISTER_SAVELOAD_CLASS( 0x53042170, CExecSnipeAim )
REGISTER_SAVELOAD_CLASS( 0x10662170, CExecThrowKnife )
REGISTER_SAVELOAD_CLASS( 0x01122132, CExecMoveInventoryItem )
REGISTER_SAVELOAD_CLASS( 0x50372121, CExecCollectSnipeAP )
REGISTER_SAVELOAD_CLASS( 0x50372000, CExecLaunchRocket )
REGISTER_SAVELOAD_CLASS( 0x51892150, CExecUsePassage )
REGISTER_SAVELOAD_CLASS( 0x52492170, CExecTakeCorpseOnDeploy )
REGISTER_SAVELOAD_CLASS( 0x50112152, CExecPlayAnimation )
REGISTER_SAVELOAD_CLASS( 0x51922130, CExecTalk )