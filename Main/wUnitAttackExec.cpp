#include "StdAfx.h"
#include "wUnitAttack.h"
#include "wUnitMove.h"
#include "wUnitServer.h"
#include "Grid.h"
#include "wMain.h"
#include "wUnitCommands.h"
#include "wUICommands.h"      // BUG 5: NWorld::CUICmdUnit -- auto-focus camera producer on shoot/grenade
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
#include "..\DBFormat\DataMap.h"	// NDb::EDiplomacyState / DS_ENEMY (CExecHeal::CanDoIt diplomacy gate)
#include "RPGCritical.h"
#include "wUnitAttack.h"
#include "wUnitAttackExec.h"
#include "aiNearestPosition.h"
#include "aiMisc.h"            // NAI::IsAIPlayer (GetActionType AI-silent gate)
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
// retail @0x3a1db0: reach gate for a melee swing -- F_MELEE_DISTANCE, doubled when the unit
// carries a reach extender (docked PK cannon).
bool CanMeleeAttack( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget )
{
	float fReach = F_MELEE_DISTANCE;
	if ( IsValid( pUS ) && IsValid( pUS->animator.GetCannon() ) )
		fReach += fReach;
	return IsWithinHumanReach( from.GetCP(), ptTarget, fReach );
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
			return UCR_CANT_HEAL;   // retail CanDoFirstAid @0x3a2d40 returns UCR_CANT_HEAL on the CanHeal-fail path
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
	// @0x3a31c0 -- retail CanDropCorpse sweeps a 0.9375 (0x3f700000) forward step, NOT FP_GRID_STEP (0.625).
	// Jan03 used FP_GRID_STEP here; the shipped binary widened the corpse-drop probe -- FOLLOW THE DECOMP.
	vels.push_back( CVec3( cos(fDir) * 0.9375f, sin(fDir) * 0.9375f, 0 ) );
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

	// Engineer/PK grenades (satchel charges) gate on a required demolition perk + the unit's
	// ENGINEERING skill (retail @0x3ac8d0, disasm 0x7ac93d: GetUnit -> skills[8] -> the CDynamicSkill
	// effective-value read -- NOT the to-hit calcer's throwing-derived nSkill, which mis-gated eng
	// throws regardless of eng skill). Ordinary grenades have no eng-grenade DB record ->
	// GetDBEngGrenade() is null -> no gate, so this fires only for engineer grenades.
	CDynamicCast<NRPG::CGrenadeItem> pGrenadeConcrete( pGrenade );
	if ( IsValid( pGrenadeConcrete ) )
	{
		NDb::CRPGEngGrenade *pEngDB = pGrenadeConcrete->GetDBEngGrenade();
		if ( pEngDB )
		{
			if ( pEngDB->nRequiredPerkID > 0 && !pUS->GetUnitRPG()->HasPerk( pEngDB->nRequiredPerkID ) )
				return UCR_NO_EQUIPMENT;
			if ( pUS->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_ENGINEERING ) < pEngDB->nSkillReq )
				return UCR_NEED_HIGHER_SKILL;
		}
	}

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
			SItem sItem;
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
			SItem sItem;
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

			// retail @0x3aa1c0: a unit with a disabled hand (C_IDLE_HAND critical) cannot be handed an item.
			if ( pUSTarget->GetUnitRPG()->HasCritical( NDb::C_IDLE_HAND ) )
				return UCR_CRITICALS_BAN;

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
	// retail @0x3a81b1: throw along the unit's FACING -- vel = ( cos(dir)*1.5, sin(dir)*1.5, 0.2 );
	// @0x7a81f1: bFallFromBody=false (a deliberate throw-out, phys case *ThrowOut), the unit rides
	// as the visibility parent (@0x7a81f0) and the drop lands on the unit's floor (GetFloor @0x7a81ea).
	float fDir = pUS->GetPosition().GetDirection();
	LaunchItem( pUS->GetWorld(), item, CVec3( cos(fDir) * 1.5f, sin(fDir) * 1.5f, 0.2f ), false, (CObjectBase*)pUS, pUS->GetFloor() );
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
			SItem sItem;
			pUS->GetTBSPlayer()->GetInHandItem( &sItem );
			pItem = sItem.pItem;

			// retail @0x7aa8fc..0x7aa956: the hand is cleared with a FRESH SItem( pUS, VACUUM ) --
			// eType(+4)=VACUUM(0) and pUnit(+0x10)=pUS are the only fields written, the four smart
			// pointers are zeroed by the ctor and nSlot/sPosition are left alone. It is NOT the
			// fetched SItem with pItem nulled.
			pUS->GetTBSPlayer()->SetInHandItem( SItem( pUS, SItem::VACUUM ) );
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
			// retail @0x7aace3: SItem::SItem(const SItem&) @0x5f920 COPIES sSource, then overwrites
			// only pItem(+0x1c) and pUnit(+0x10). Copying sSource is what carries the item's ORIGIN
			// (eType/nSlot/sPosition) into the hand -- CExecMoveInventoryItem::GetActionType
			// @0x3a7990 reads sHandItem.eType back out and compares it to the drop destination.
			// (Retail assigns pUnit from the entry-level CDynamicCast<CUnitServer>(cmd source unit);
			// sSource.pUnit is the same object in every case reachable here.)
			SItem sItem( sSource );
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
				// @0x3aada1 -- retail fires CEventOnItemGiven( receiver, replacedItem, newItem ) so the
				// good/bad-item-given voice acks can compare the swap (CAckGoodItemGiven/CAckBadItemGiven).
				NGlobal::ThrowEvent( CEventOnItemGiven( pUSTarget, pOldItem, pItem ) );

				pInventory->TakeOff( (NDb::ESlot)sTarget.nSlot );

				// retail @0x7aadd4: copy-construct from sTARGET, then overwrite pUnit + pItem. The
				// displaced item's origin is the slot it was just taken out of, so the hand records
				// eType = SLOT / nSlot = sTarget.nSlot -- GetActionType @0x3a7990 then classifies a
				// subsequent move of it as AC_ITEM_SLOT.
				SItem sInfo( sTarget );
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

				// retail @0x7ab039: copy-construct from sTARGET (eType = BACKPACK), overwrite pUnit +
				// pItem, and additionally copy sPosition(+8,+0xc) from the DISPLACED item's own grid
				// entry (@0x7ab070/0x7ab07b read the list node's SBackPackItem::sPos) -- so the hand
				// records the exact backpack cell the item came out of, not the drop position.
				SItem sInfo( sTarget );
				sInfo.pUnit = pUSTarget;
				sInfo.pItem = pOldItem;
				sInfo.sPosition = intersectedItemslist.front().sPos;
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
	// (the Jan03 mission-side StartAttack() burst cursor is GONE in retail -- the exec's own
	// nBulletGone/nBulletPrepared counters are the burst state)
	Start();
	StartAction( pUS->GetWorld(), NORMAL );
	UpdateCamera();   // BUG 5: retail CExecAttack::Run @0x3a1460 tail -- post the arbitrated auto-focus camera
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// BUG 5: retail CExecAttack::UpdateCamera @0x3a3e70 -- the base attack focuses on the shooter with no second
// framing point (tile / object / melee shots have no unit target).
void CExecAttack::UpdateCamera()
{
	if ( IsValid( pUS ) && IsValid( pUS->GetWorld() ) )
		pUS->GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( pUS, NWorld::PR_UNIT_ACTION, false, 1.0f, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecAttack::TimeLabelReached()
{
	// @0x3a2650 -- retail: OnLabel is now VOID (arms the timed-bullet schedule / ends the shot). Two changes
	// from Jan03, both disasm-confirmed at 0x7a2650:
	//   (1) NextBullet() is DROPPED -- retail does NOT advance the RPG bullet cursor per label; the timed
	//       pipeline tracks bullets via nBulletGone/nBulletPrepared + SelectRay's per-bullet CreateAttack.
	//       (The one-shot CExecMelee*/CExecThrowKnife OnLabel overrides call NextBullet() themselves.)
	//   (2) the Jan03 "!bRes -> StartAttack() reset burst state" re-arm is GONE; instead, on cancel we Finish
	//       the executor here. Return !bAttackCanceled (consumed by CUnitServer::Segment's bCallTimeLabel).
	OnLabel();
	if ( bAttackCanceled )
		Finished();
	return !bAttackCanceled;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecAttack::Cancel()
{
	bAttackCanceled = true;
//	StopAction();
//	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecAttack::CreateAttack( vector<NRPG::CAttackPortion> *pAttack, CUnitServer *pUnitTarget, bool bSpendAmmo,
	bool bAdaptWeapon ) const
{
	bool bBackStab = false;
	if ( IsValid( pUnitTarget ) )
	{
		bBackStab = !pUnitTarget->IsUnitAudible( pUS ) && !pUnitTarget->IsUnitVisible( pUS );
		if ( bBackStab )
			csSystem << CC_RED << "Backstab attack" << endl;
	}
	CPtr<NRPG::IUnitMission> pTarget = IsValid( pUnitTarget ) ? pUnitTarget->GetUnitRPG() : 0;
	return pUS->GetUnitRPG()->CreateAttack( pAttack, bSpendAmmo, false, pTarget, bBackStab, bAdaptWeapon );
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
// @0x3a8b40 -- retail timed-bullet OnLabel (VOID). Jan03's synchronous "PerformShot drains the Attack
// vector" is GONE. On the FIRST label it ARMS the schedule (tNextBulletPrepare/Go) and flashes the first
// bullet (firing it instantly when the weapon has zero bullet-delay); thereafter, once the shot is done
// (bAttackCanceled -- set by OnBulletGo/Segment when the burst ends) it ENDS the shot (AC_END_SHOOT +
// CheckShotResult), else it keeps the aim/attack animation running. Per-bullet firing is driven by Segment.
void CExecShoot::OnLabel()
{
	const NAI::SUnitPosition &position = pUS->GetPosition();

	// ---- arm the schedule + flash/launch the first bullet (once; skipped in aim-and-hold mode) ----
	if ( tNextBulletPrepare == 0 && !bOnlyPrepareToShoot )
	{
		STime delay = GetBulletDelay();
		bool bFlash = ( nBulletPrepared <= 0 ) || CheckBurst( nBulletPrepared, false );
		if ( bFlash )
		{
			CreateFlash( nBulletPrepared == 0 );
			++nBulletPrepared;
		}
		if ( delay == 0 )
			OnBulletGo();   // zero-delay weapon: the bullet departs immediately
		STime t = GetNextBulletTime( pUS->GetWorld()->GetTime()->GetValue() );
		tNextBulletPrepare = t;
		tNextBulletGo = t + delay;
	}

	if ( bAttackCanceled )
	{
		// ---- END the shot ---- (retail @0x3a8b40: stop a still-held long-burst SFX first, then
		//   AC_END_SHOOT + CheckShotResult. The retail weapon-empty notify / UpdateVision world
		//   sinks remain deferred -- see the header.)
		if ( IsValid( longBurstSnd.pLongBurstSnd ) && GetShootMode() == NDb::SM_LongBurst )
			longBurstSnd.pLongBurstSnd->EndSound();
		pUS->DoAction( NRPG::AC_END_SHOOT );
		CheckShotResult();
	}
	else
	{
		// ---- CONTINUE: keep the aim/attack animation running while bullets are still scheduled ----
		STime delay = GetBulletDelay();
		if ( delay == 0 || tNextBulletPrepare > pUS->GetWorld()->GetTime()->GetValue() )
			pUS->animator.Attack( position, rayInfo.GetRay(), true, false );   // retail: rayInfo.GetRay()
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a4480 -- retail fires ONE ranged attack per call (no args), from this->attack + this->ray (Jan03 looped
// a vector<CAttackPortion>). Trail model/speed come from the equipped weapon's DB record. (Retail also reports
// the spawned bullet to IUnitMissionForMedals::AddWaitForBullet -- DEFERRED: the concrete CUnitMission does not
// derive that interface in-tree, so the cross-cast never resolves; the whole medal-session subsystem is unwired.)
void CExecShoot::PerformAttack()
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
	// ORIGINAL BUG (confirmed disasm @0x7a4480): retail's GetWeaponItem()==null path zeroes the weapon-record
	// base then falls through into the same GetDBWeapon reads -> deref of a null record. Never reached in
	// practice (PerformAttack only runs with a weapon equipped); the null-guard keeps the flow observable.
	if ( pWeapon && pWeapon->GetDBWeapon()->pTrailEffect )
	{
		SRand sRand;
		fTrailSpeed = pWeapon->GetDBWeapon()->fTrailSpeed;
		pTrailEffect = pWeapon->GetDBWeapon()->pTrailEffect->CreateModel( &sRand );
	}

	CRay rTempRay( rayInfo.GetRay() );
	rTempRay.ptOrigin += rayInfo.vDir * pUS->GetMinClearDistance();
	pUS->GetWorld()->PerformRangedAttack( attack, rTempRay, ignores, pUS->GetWorld()->GetTime()->GetValue(), pTrailEffect, fTrailSpeed );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a1fa0 -- retail CheckBurst(nFired, bDoAction): may the burst keep firing? (return TRUE == continue) and,
// when bDoAction, spend the burst AP for this round. NOTE the return polarity is INVERTED vs the Jan03
// CheckBurst(bool bComplete) form (which returned "should stop"). The first bullet (nFired==0) never spends
// burst AP here.
// v1.2 @0x7a2280: the mode dispatch becomes a full switch. NEW: the single-shot modes SM_Snap/SM_Aimed/
// SM_Careful -- `return false` in v1.1 -- keep firing until the weapon's bullets-per-shot count is exhausted
// (nFired < GetBulletsPerShot()), so one trigger pull fires all pellets of a ShotsInOne weapon. ShortBurst/
// LongBurst keep the v1.1 length + burst-state + AC_BURST AP logic verbatim; Snipe still never bursts.
bool CExecShoot::CheckBurst( int nFired, bool bDoAction )
{
	switch ( GetShootMode() )
	{
	case NDb::SM_Snap:
	case NDb::SM_Aimed:
	case NDb::SM_Careful:
		return nFired < GetBulletsPerShot();   // v1.2: multi-bullet single shot
	case NDb::SM_ShortBurst:
		if ( GetShortBurstLength() <= nFired )
			return false;   // short burst has fired its full length
		break;
	case NDb::SM_LongBurst:
		break;
	default:
		return false;       // SM_Snipe: never a burst
	}

	if ( CanDoIt( pUS->GetPosition() ) != UCR_OK )
		return false;       // can no longer shoot from here

	if ( nFired >= 1 )
	{
		int nAP = pUS->GetActionAP( NRPG::AC_BURST );
		if ( !pUS->CanSpendAP( nAP ) )
			return false;   // not enough AP for the next burst round
		if ( bDoAction )
			pUS->DoAction( NRPG::AC_BURST );
	}
	return true;
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
// @0x3a8290 -- retail Start: reset the timed-bullet bookkeeping, then either aim-and-hold (bOnlyPrepareToShoot)
// or run a full shot (unhide, careful-AP, select the ray, spend AP, play the fire anim / free-shot OnLabel).
void CExecShoot::Start()
{
	nBulletGone = 0;
	nBulletPrepared = 0;
	bMissed = true;
	nToHit = 0;

	if ( bOnlyPrepareToShoot )
	{
		// aim-and-hold: raise/aim only (no AP, no scream). Latch bAttackCanceled so the first OnLabel ENDs
		// immediately without arming a bullet. animator.Attack(.., bInstant=false, bNoShoot=true).
		SelectRay();
		bShotInitiated = true;
		pUS->animator.Attack( pUS->GetPosition(), rayInfo.GetRay(), false, true );
		bAttackCanceled = true;
		return;
	}

	csRPG << CC_WHITE << "Shoot begin :\n";
	// (BUG 5: the auto-focus camera is posted from CExecAttack::Run -> UpdateCamera (retail @0x3a1460 tail),
	// NOT here in Start -- so it fires once per attack Run and carries the shot target, for unit shots only.)
	CheckUnhide();
	CalculateExtraAP();
	SelectRay();
	bShotInitiated = true;
	SpendAP();
	if ( !IsAccidental() )
	{
		Scream();
		pUS->animator.Attack( pUS->GetPosition(), rayInfo.GetRay(), false, false );
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
// @0x3a1ee0 -- the equipped weapon's shoot mode (SM_Snap when there is no valid weapon).
NDb::EShootMode CExecShoot::GetShootMode() const
{
	NRPG::CWeaponItem *pWeapon = pUS->GetUnitRPG()->GetRPGUnit()->GetWeaponItem();
	if ( IsValid( pWeapon ) )
		return pWeapon->GetShootMode();
	return NDb::SM_Snap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Short-burst bullet count = nRoF/6 (floored) + the LONGER_SHORT_BURST perk bonus -- the exact Jan03 CheckBurst
// inline formula. 0 when there is no valid weapon.
int CExecShoot::GetShortBurstLength() const
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::CWeaponItem *pWeapon = pRPG->GetRPGUnit()->GetWeaponItem();
	float fPerkBullets = 0;
	pRPG->HasPerk( NRPG::N_PERK_LONGER_SHORT_BURST, &fPerkBullets );
	if ( IsValid( pWeapon ) )
		return ( int )( pWeapon->GetDBWeapon()->nRoF / 6.f + fPerkBullets );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a20e0 -- advance a bullet timestamp by one inter-bullet period = round(60000/BPM) ms (the disasm computes
// t -= round(-60000.0f/rec[+0x9c]), and +0x9c is nBPM -- bullets per MINUTE. NOT nRoF, which is the full-burst
// bullet COUNT: dividing by nRoF (~12-30) gave multi-second gaps between burst bullets). Unchanged t on a
// missing weapon/record.
// v1.2 @0x7a23f0: a multi-bullet shot (ShotsInOne > 1) has NO inter-bullet interval -- the timestamp is
// returned unchanged, so all pellets share one time (Segment's catch-up loop fires them in a single tick).
STime CExecShoot::GetNextBulletTime( STime t ) const
{
	NRPG::IWeaponItem *pWeapon = pUS->GetUnitRPG()->GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		NDb::CRPGWeapon *pDB = pWeapon->GetDBWeapon();
		if ( pDB && pDB->nShotsInOne > 1 )
			return t;   // v1.2: all bullets of the shot go at once
		if ( pDB && pDB->nBPM != 0 )
			t += ( STime )( int )( 60000.0f / ( float )pDB->nBPM + 0.5f );
	}
	return t;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 @0x7a21c0 (NEW helper) -- bullets fired by ONE trigger pull: the weapon DB's ShotsInOne field
// clamped to >= 1 (1 when there is no valid weapon/record, matching the retail rec[+0xb0]>1 ? rec[+0xb0] : 1).
int CExecShoot::GetBulletsPerShot() const
{
	NRPG::IWeaponItem *pWeapon = pUS->GetUnitRPG()->GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		NDb::CRPGWeapon *pDB = pWeapon->GetDBWeapon();
		if ( pDB && pDB->nShotsInOne > 1 )
			return pDB->nShotsInOne;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a4240 -- muzzle flash for one shot. bLeft alternates the barrel bone on a dual-mount (headless) PK
// (retail gates it on GetWearingDBPK()->bHasNoHead and the prepared-bullet parity); bFirstBullet gates the
// once-per-burst sound. Retail retention: the returned C3DSound* is kept in longBurstSnd ONLY when the
// slot was empty; a held burst sound is never replaced mid-burst -- a later one-shot is EndSound()ed.
void CExecShoot::CreateFlash( bool bFirstBullet )
{
	bool bLeft = false;
	NDb::CPanzerklein *pPK = pUS->GetWearingDBPK();
	if ( IsValid( pPK ) && pPK->bHasNoHead )
		bLeft = ( nBulletPrepared & 1 ) != 0;
	C3DSound *pSnd = pUS->CreateFlash( bLeft, bFirstBullet );
	if ( !IsValid( longBurstSnd.pLongBurstSnd ) )
		longBurstSnd.pLongBurstSnd = pSnd;      // slot empty: retain (retail CObj acquire)
	else if ( pSnd )
		pSnd->EndSound();                       // slot full: end the one-shot, do not retain
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a40d0 -- retail reveals a concealed shooter before firing (weapon fSilencer >= 1.0 gated by a
// hidden-state predicate). STUBBED: CRPGWeapon::fSilencer now exists (it feeds the shot's SAISound), but
// the posProvider hidden-predicate (vtbl[0xc]) has no counterpart and the dev has no conceal-on-shoot
// flow -> safe no-op until that subsystem lands.
void CExecShoot::CheckUnhide()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a26f0 -- one bullet departs the barrel: fire it (PerformAttack), then decide whether to chamber the next
// burst round (SelectRay) or latch the cancel and stop. A short burst clears a pending mid-burst cancel (it
// always finishes its full length); any other mode stops on a pending cancel.
void CExecShoot::OnBulletGo()
{
	if ( bShotInitiated )
	{
		++nBulletGone;
		PerformAttack();
		bShotInitiated = false;
	}

	NDb::EShootMode mode = GetShootMode();
	if ( mode == NDb::SM_ShortBurst )
	{
		if ( bAttackCanceled )
			bAttackCanceled = false;
	}
	else if ( bAttackCanceled )
		return;

	if ( !IsAttackCanceled() && CheckBurst( nBulletGone, true ) )
	{
		SelectRay();
		bShotInitiated = true;
		return;
	}

	bAttackCanceled = true;   // nothing more to fire -> OnLabel ENDs the shot next tick
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a8d20 -- per-tick timed-bullet driver. Samples world-time ONCE; as it crosses tNextBulletPrepare it
// chambers/flashes the next bullet, and as it crosses tNextBulletGo it fires (OnBulletGo). Overrides the
// CCommandExecute::Segment no-op; CUnitServer::Segment invokes it every tick.
// v1.2 @0x7a9160: the go branch is a catch-up LOOP with a NEW abort guard on bAttackCanceled -- v1.1 fired at
// most ONE bullet per tick; v1.2 fires EVERY bullet whose time has come (and, with the v1.2 GetNextBulletTime
// no-advance for ShotsInOne > 1 weapons, all pellets of a multi-bullet shot depart in this same tick --
// bAttackCanceled, latched by OnBulletGo when the shot ends, is the loop's only brake). Prepare unchanged.
void CExecShoot::Segment()
{
	if ( tNextBulletPrepare == 0 )
		return;   // no shot armed

	STime now = pUS->GetWorld()->GetTime()->GetValue();

	if ( tNextBulletPrepare <= now )
	{
		if ( nBulletPrepared < 1 || CheckBurst( nBulletPrepared, false ) )
		{
			CreateFlash( nBulletPrepared == 0 );
			++nBulletPrepared;
		}
		tNextBulletPrepare = GetNextBulletTime( tNextBulletPrepare );
	}

	while ( tNextBulletGo <= now && !bAttackCanceled )   // reuses the same sampled `now`; v1.2 catch-up loop
	{
		OnBulletGo();
		tNextBulletGo = GetNextBulletTime( tNextBulletGo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootTile
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecShootTile::CExecShootTile( CUnitServer *_pUS, const CVec3 &_ptTarget ):
	CExecShoot(_pUS, 0)
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
void CExecShootTile::SelectRay() // false, when it is the last shot
{
	// retail SelectRay builds a SINGLE `attack` portion into the member (was the vector Attack); NO CheckBurst
	// here (that moved to the OnBulletGo/Segment pipeline). cover+to-hit+peek inlined from the retail solver
	// NRPG::AttackPointRanged (absent in-tree) using the dev helpers it wraps.
	vector<NRPG::CAttackPortion> tmp;
	CreateAttack( &tmp, 0, true, nBulletGone == 0 );   // retail @0x3a4720: adapt on the shot's first bullet only
	if ( !tmp.empty() )
	{
		attack = tmp[0];
		int nTmpToHit;
		bool bTmpMissed;
		CPtr<CWorld> pWorld = pUS->GetWorld();
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( pUS->GetPosition() ),
			attack, pUS, ptAnimTarget, pUS->GetMinClearDistance() );
		float fHit = NRPG::CheckTileToHit( pUS, ptAnimTarget,
			GetExtraAP(), NAI::THL_LOWER, pCover, pWorld->IsFirstTurn(), &nTmpToHit, nBulletGone );   // retail: no eHL member, tile always THL_LOWER; the exec's burst cursor is the bullet index
		CRay r;
		NRPG::PeekRay( pCover, &r, fHit, &bTmpMissed );
		// retail @0x3a4720 tail (NRPG::AttackPointRanged @0x2b62b0): the solver rebuilds the FULL
		// SAttackRayInfo carrier (12-arg ctor @0x291330 semantics) and stores it in the member.
		rayInfo = NRPG::SAttackRayInfo( attack, r.ptOrigin, r.ptDir, pUS, pUS->GetPosition(),
			nBulletGone, GetExtraAP(), !bTmpMissed, pUS->GetMinClearDistance(), 30.0f, 0 );
		rayInfo.bFirstTurn = pWorld->IsFirstTurn();
		rayInfo.pIgnore = pUS;   // retail: the shooter's own hull is the in-flight ignore object
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
// BUG 5: retail CExecShootUnit::UpdateCamera @0x3a47c0 -- a unit shot frames the shooter against the shot
// TARGET (the two-point best-point framing), unlike the base tile/object attack.
void CExecShootUnit::UpdateCamera()
{
	if ( IsValid( pUS ) && IsValid( pUS->GetWorld() ) )
		pUS->GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( pUS, NWorld::PR_UNIT_ACTION, false, 1.0f, pTarget.GetPtr() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecShootUnit::SelectRay() // false, when it is the last shot
{
	// retail SelectRay builds a SINGLE `attack` portion into the member (was the vector Attack); NO CheckBurst
	// here (moved to the OnBulletGo/Segment pipeline). cover+to-hit+peek inlined from NRPG::AttackObjectRanged.
	vector<NRPG::CAttackPortion> tmp;
	CreateAttack( &tmp, pTarget, true, nBulletGone == 0 );   // retail @0x3a49b0: adapt on the shot's first bullet only
	if ( !tmp.empty() )
	{
		attack = tmp[0];
		int nTmpToHit;
		bool bTmpMissed;
		CPtr<CWorld> pWorld = pUS->GetWorld();
		vector<int> accessibleHLs;
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCovers( pUS->GetAttackOrigin(), attack, pUS, pTarget, eHL, pUS->GetMinClearDistance() );
		float fHit = NRPG::CheckToHit( pUS, pTarget, GetExtraAP(), eHL, accessibleHLs, pCover, pWorld->IsFirstTurn(), &nTmpToHit, nBulletGone );
		CRay r;
		NRPG::PeekRay( pCover, &r, fHit, &bTmpMissed );
		// retail @0x3a49b0 tail (NRPG::AttackObjectRanged @0x2b6560): rebuild the full SAttackRayInfo
		// carrier (12-arg ctor @0x291330 semantics; pTarget rides the trailing ctor arg).
		rayInfo = NRPG::SAttackRayInfo( attack, r.ptOrigin, r.ptDir, pUS, pUS->GetPosition(),
			nBulletGone, GetExtraAP(), !bTmpMissed, pUS->GetMinClearDistance(), 30.0f, pTarget.GetPtr() );
		rayInfo.bFirstTurn = pWorld->IsFirstTurn();
		rayInfo.pIgnore = pUS;   // retail: the shooter's own hull is the in-flight ignore object
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
		// add interrupt
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
	// @0x3a8da0 -- retail fires CEventOnAttackAtUnit (consumed by CAIEventTrackerImpl::OnAttack /
	// aiThreatTracker) after the lua hook and before CancelSnipe. Was missing in a5dll (dead listener).
	NGlobal::ThrowEvent( CEventOnAttackAtUnit( pUS, pTarget ) );
	//
	pUS->CancelSnipe();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecShootUnit::IsAttackCanceled()
{
	// @0x3a4ae0 -- already-canceled short-circuit: the binary returns early on bAttackCanceled
	// BEFORE consuming the burst auto-stop RNG roll. (a5dll previously fell through to the roll.)
	if ( bAttackCanceled )
		return true;
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
	// @0x3a2180 -- retail composes this with NWorld::CanMeleeAttack @0x3a1db0; two
	// behaviours were missing vs the Jan03 copy:
	//   * a prone (CRAWL) attacker cannot melee -> UCR_UNAVAILABLE;
	//   * the melee reach DOUBLES when the unit carries a reach extender (a docked
	//     Panzerklein cannon); otherwise it stays F_MELEE_DISTANCE.
	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	if ( from.GetPose() == NAI::CRAWL )
		return UCR_UNAVAILABLE;

	if ( !CanMeleeAttack( pUS, from, ptTarget ) )	// reach (+PK-cannon extender), @0x3a1db0
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
	{
		NDb::SAISound sound = { NDb::GetAISound( 19 ), 0, 1.0f };   // retail @0x3a4ed0: no silencer on melee
		pUS->GetWorld()->MakeAISound( sound, pUS, 0 );
	}
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
void CExecMeleeTile::OnLabel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	CWorld *pWorld = pUS->GetWorld();
	const NAI::SUnitPosition &position = pUS->GetPosition();
	bool bComplete = true;
	// (the Jan03 mission-side bullet cursor is GONE in retail; melee/knife to-hit ignores the
	// bullet index, so nothing replaces the old NextBullet() advance here)

	CRay ray;
	ray.ptOrigin = position.GetEyePosition();
	ray.ptDir = ptTarget - ray.ptOrigin;
	Normalize( &ray.ptDir );
	vector<NRPG::CAttackPortion> attack;
	bComplete = CreateAttack( &attack, 0 );
	ASSERT( bComplete );
	PerformAttack( attack, ray );
	// @0x3a50a0 -- decode sets bAttackCanceled (+0x18) unconditionally before return.
	// It suppresses CExecAttack::TimeLabelReached's burst-reset branch
	// (`if(!bRes && !bAttackCanceled) StartAttack()`): without it a completed
	// melee-tile swing re-triggers StartAttack(), a behavioral divergence.
	bAttackCanceled = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMeleeUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3a51b0: base chain + hlInfo = {_eHL, empty set} (SelectTargetHLs fills the set at
// Start) + pTarget; the aim point seeds from the valid target's ground point, else the attacker's.
// (The Jan03 bIsHitLocationShot derivation is GONE -- a called shot is simply eHL != HL_ANY.)
CExecMeleeUnit::CExecMeleeUnit( CUnitServer *_pUS, CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP ):
	CExecMelee(_pUS, _nExtraAttackAP), hlInfo(_eHL), pTarget(_pTarget)
{
	if ( IsValid( pTarget ) )
		ptTarget = pTarget->GetPosition().GetCP();
	else
		ptTarget = _pUS->GetPosition().GetCP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 @0x7a1840 -- the hit-location resolution shared by Start and the v1.2 CanDoIt probe. Both
// callees already returned bool in v1.1 (Start @0x3a1640 ignored them); v1.2's factoring added the
// early-out: on SelectTargetHLs failure GetUnitHLPos is NOT called, so *pRes keeps its old value.
bool CExecMeleeUnit::SelectTargetHLPos( const NAI::SUnitPosition &pos, CVec3 *pRes, NRPG::STargetHLInfo *pInfo ) const
{
	CWorld *pWorld = pUS->GetWorld();
	if ( !NRPG::SelectTargetHLs( pUS, pos, pInfo, pTarget, pInfo->eHL ) )
		return false;
	return pWorld->GetAIMap()->GetUnitHLPos( pRes, pWorld->GetAIMap()->GetHull(pTarget), pInfo->eHL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 NEW override @0x7a2510 (v1.1 had no CExecMeleeUnit::CanDoIt; base @0x3a2180 unchanged):
// probe the hit-location resolution on a stack COPY of hlInfo (the member stays untouched, the
// probed aim point is discarded) -- a valid target with NO resolvable hit location can't be meleed.
// NOTE retail order: the probe runs BEFORE the base's bIgnoreTarget/CRAWL/reach checks.
EUnitCommandResult CExecMeleeUnit::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( IsValid( pTarget ) )
	{
		NRPG::STargetHLInfo probeInfo = hlInfo;
		CVec3 ptProbe;
		if ( !SelectTargetHLPos( from, &ptProbe, &probeInfo ) )
			return UCR_TARGET_OUT_OF_RANGE;
	}
	return CExecMelee::CanDoIt( from, bIgnoreTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMeleeUnit::Start()
{
	// v1.2 @0x7a18a0 (the v1.1 Start @0x3a1640 with its prologue split into the helper): resolve
	// hlInfo + the aim point, then the unchanged CExecMelee tail (DoAction(AC_MELEE) + SpendAP +
	// animator.CloseAttack toward ptTarget). Helper failure -> ptTarget keeps the ctor's seed.
	// (The Jan03 HL_ANY random resolve moved into NRPG::SelectTargetHLs -- where retail DROPS the
	// pick, see the ORIGINAL BUG note there; the accessible-HL set now persists in hlInfo for
	// OnLabel's to-hit instead of being recomputed.)
	SelectTargetHLPos( pUS->GetPosition(), &ptTarget, &hlInfo );
	CExecMelee::Start();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMeleeUnit::OnLabel()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	CWorld *pWorld = pUS->GetWorld();
	bool bComplete = true;

	// (the Jan03 mission-side bullet cursor is GONE in retail; melee to-hit ignores the bullet
	// index, so nothing replaces the old NextBullet() advance here)
	csRPG << CC_WHITE << "Melee " << pRPG->GetName() << "\n{\n";
	vector<NRPG::CAttackPortion> attack;
	bComplete = CreateAttack( &attack, pTarget );
	ASSERT( bComplete );
	if ( !attack.empty() )
	{
		// retail @0x3a94c0 hands &hlInfo (as resolved by Start's SelectTargetHLs and SERIALIZED with
		// the executor) into NRPG::AttackObjectRanged @0x2b6560, whose unit branch feeds hlInfo.eHL +
		// hlInfo.accessibleHLs into the to-hit -- the Jan03 bIsHitLocationShot branch and the
		// accessible-HL recompute are GONE (a called shot already narrowed the set in SelectTargetHLs;
		// after a mid-swing load the set comes from the save, not from a fresh AIMap query).
		// cover+to-hit+peek inlined from AttackObjectRanged, matching the CExecShootUnit::SelectRay port.
		CVec3 ptAttackPos = NRPG::GetMeleeAttackPos( pUS, ptTarget );
		CObj<NRPG::CCoverInfo> pCover = pWorld->GetGame()->CalcCovers( ptAttackPos, attack[0], pUS, pTarget, hlInfo.eHL, 0 );
		//
		int nToHit;
		float fHit = NRPG::CheckToHit( pUS, pTarget,
			GetExtraAP(), hlInfo.eHL, hlInfo.accessibleHLs, pCover, pWorld->IsFirstTurn(), &nToHit );
		CRay ray;
		bool bIsMiss;
		if ( NRPG::PeekRay( pCover, &ray, fHit, &bIsMiss ) )
		{
			PerformAttack( attack, ray, fHit );
			//PerformAttack( attack, ray, bIsMiss, fHit );
			// @0x3a9677 -- retail's ONLY throw here is CEventOnUnitSuccessfulMelee( attacker ) (typeid
			// VA 0x98273c), consumed by CAckSuccessfulMeleeAttack (the "landed a melee hit" voice ack).
			// The earlier CEventOnAttackAtUnit( pUS, pTarget ) throw was a mis-decode -- retail never
			// fires that event at the melee OnLabel site (single-unit payload, disasm confirmed).
			NGlobal::ThrowEvent( CEventOnUnitSuccessfulMelee( pUS ) );
		}
	}
	csRPG << "}\n";
	// @0x3a94c0 -- retail OnLabel unconditionally sets bAttackCanceled = true here (its primary
	// observable side effect): marks the attack consumed so the base CExecAttack::TimeLabelReached()
	// does NOT re-issue StartAttack()/reset burst state (lines 898-900). Jan03 OnLabel omitted it.
	bAttackCanceled = true;
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
// retail @0x3a1700 takes the grenade ITEM (not the CRPGGrenade record) -- the fuse fields are
// read through the which-record branch, so an engineer grenade works too.
void CExecThrowGrenade::CheckToHitAndDelay( NRPG::IGrenadeItem *pGrenade )
{
	// check ToHit and find the point where the grenade is actually thrown
	int nToHit = pToHitCalcer->GetToHit();
	pToHitCalcer->Log();

	// find the actual grenade explosion delay
	int nRandom = random.Get(100);
	if ( nRandom > nToHit )
	{
		// we miss
			// find where the grenade will actually fly
		float fD = 0.2f * fabs( grenadeParams.vel );
		grenadeParams.vel.x += random.GetFloat( -fD, +fD );
		grenadeParams.vel.y += random.GetFloat( -fD, +fD );
		grenadeParams.vel.z += random.GetFloat( -fD, +fD );
	}
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << " \tCheck:" << nRandom;

	// convert flight time into a delay
	const int nMaxDelay = NRPG::GetGrenadeRecMaxDelay( pGrenade );
	grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float(nMaxDelay) );
	grenadeParams.fT = nMaxDelay - grenadeParams.fT;
	csRPG << " True delay: " << grenadeParams.fT;
	nRandom = random.Get(100);
	if ( nRandom >= 99 || nRandom >= pToHitCalcer->GetSkill() )
	{
		// change the delay time
		grenadeParams.fT *= random.GetFloat( 0.5f, 2.f );
		grenadeParams.fT = Clamp( grenadeParams.fT, 0.f, float(nMaxDelay) * 0.75f );
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
	CheckToHitAndDelay( pGrenade );

	pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), !pUS->IsAIUnit() );

/*	{
		NDb::EItemSubType subType = pInventory->GetActive()->GetDBItem()->subType;
		int nPlace = pInventory->GetPlaceBySubType( subType );
		pUS->animator.ActivateItem( pUS->GetPosition(), false, nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType() );
	}*/

	// retail @0x764140: BOTH records go to the world -- CWorld::ThrowGrenade dispatches an
	// engineer grenade (null regular record) to the contact-fused eng grenade server.
	pUS->GetWorld()->ThrowGrenade( grenadeParams.ptStart, grenadeParams.vel,
		pUS->animator.GetTimeLabel1(), grenadeParams.fT, item.pModel,
		pGrenade->GetDBGrenade(), pUS, pGrenade->GetDBEngGrenade() );
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
	// (Jan03 StartAttack() burst-cursor reset dropped -- the mission cursor is gone in retail)
	//
	pUS->DoAction( NRPG::AC_THROW_GRENADE );
	pUS->animator.ThrowGrenade( position, grenadeParams.ptOriginalTarget, grenadeParams.nSide );
	StartAction( pUS->GetWorld(), NORMAL );
	// BUG 5 (auto-focus): retail CExecThrowGrenade::Run @0x3acda0 posts the arbitrated auto-focus as its LAST
	// action (after StartAction): CUICmdUnitCamera(thrower, PR_UNIT_ACTION, false, 1.0, null).
	if ( IsValid( pUS ) && IsValid( pUS->GetWorld() ) )
		pUS->GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( pUS, NWorld::PR_UNIT_ACTION, false, 1.0f, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecThrowGrenade::TimeLabelReached()
{
	// @0x3acf60 -- retail decode is exactly { ThrowGrenade(); return false; }.
	// The pRPG->NextBullet() advance was burst-gun cruft (cf. CExecShoot label path
	// @wUnitAttackExec.cpp:896) and is absent from the grenade executor's decode --
	// a grenade is a single throw, not a burst, so nBullet must not be advanced here.
	ThrowGrenade();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecLaunchRocket
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecLaunchRocket::CExecLaunchRocket( CUnitServer *_pUS, const CVec3 &_ptTarget ):
	CExecShoot(_pUS, 0), type(NORMAL), tRocket(0)
{
	ptAnimTarget = _ptTarget;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecLaunchRocket::CExecLaunchRocket( CUnitServer *_pUS, EType _type )
	: CExecShoot(_pUS, 0), type(_type), tRocket(0)
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
	CRay r;
	if ( IsAccidental() )
		pUS->GetBarrelDir( &r );
	else
	{
		r.ptOrigin = pUS->GetAttackOrigin();
		r.ptDir = ptAnimTarget - r.ptOrigin;
		Normalize( &r.ptDir );
	}
	rayInfo.vOrigin = r.ptOrigin;   // the rocket ray rides the inherited SAttackRayInfo carrier (retail +0x70)
	rayInfo.vDir = r.ptDir;
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
	CRay r( rayInfo.GetRay() );
	if ( !IsAccidental() )
	{
		int nToHit = pToHitCalcer->GetToHit();
		CObj<NRPG::CCoverInfo> pCover = pUS->GetWorld()->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( pUS->GetPosition() ),
			attack[0], pUS, ptAnimTarget, pUS->GetMinClearDistance() );
		//
		if ( !NRPG::PeekRayForRocket( pCover, &r, random.Get( 1, 100 ) <= nToHit ) )
		{
			ASSERT( 0 );
			return;
		}
	}
	//
	CVec3 speed = r.ptDir * fSpeed;
	r.ptOrigin += r.ptDir * pUS->GetMinClearDistance();
	rayInfo.vOrigin = r.ptOrigin;   // persist the launched ray back into the carrier (retail keeps rayInfo current)
	rayInfo.vDir = r.ptDir;
	STime tThrow = pUS->animator.GetTimeLabel1();
	if ( IsAccidental() )
		tThrow = pUS->GetWorld()->GetTime()->GetValue();
	pUS->GetWorld()->LaunchRocket( r.ptOrigin, speed, tThrow,
		pToHitCalcer->GetMaxDistance() * FP_GRID_STEP, pModel, attack.front(), pRocket, pUS, pEffect );
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a1f30 CExecShoot::GetBulletDelay -- the per-shot label->launch delay (ms), read from the weapon's DB
// anim-weapon record (0 when any link in the weapon/record chain is missing/destroyed).
int CExecShoot::GetBulletDelay() const
{
	NRPG::IWeaponItem *pWeapon = pUS->GetUnitRPG()->GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		NDb::CRPGWeapon *pDB = pWeapon->GetDBWeapon();
		if ( pDB && IsValid( pDB->pAnimWeaponType ) )
			return pDB->pAnimWeaponType->nBulletDelay;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a21d0 -- retail ARMS the launch here (schedules tRocket) instead of firing immediately; the actual
// LaunchRocket + END_SHOOT happen in Segment once game time reaches tRocket. (Jan03 did it all inline here:
// NextBullet/LaunchRocket/DoAction/CreateFlash -- retail keeps only CreateFlash + the arm.)
void CExecLaunchRocket::OnLabel()
{
	if ( tRocket == 0 )
	{
		pUS->CreateFlash( false, false );   // @0x3a21d0 launch rocket -> right barrel, single sound
		tRocket = pUS->GetWorld()->GetTime()->GetValue() + GetBulletDelay();
	}
	// OnLabel is now VOID (was `return false`). It does NOT set bAttackCanceled, so the label re-fires each tick
	// -- benign: this body no-ops once tRocket is armed, and CExecLaunchRocket::Segment fires the rocket + latches
	// bAttackCanceled + Finished(). No NextBullet() (rocket ammo is spent in LaunchRocket->CreateAttack(true)).
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a58a0 -- per-tick: once game time reaches the armed launch time, fire exactly once (LaunchRocket +
// END_SHOOT), latch via bAttackCanceled (the "already launched" flag), and finish. Driven each tick by
// CUnitServer::Segment's pExec->Segment().
void CExecLaunchRocket::Segment()
{
	STime tNow = pUS->GetWorld()->GetTime()->GetValue();
	if ( tRocket != 0 && tNow >= tRocket )
	{
		if ( !bAttackCanceled )
		{
			LaunchRocket();
			if ( !IsAccidental() )
				pUS->DoAction( NRPG::AC_END_SHOOT );
		}
		bAttackCanceled = true;
		Finished();
	}
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

		// @0x3a59d0 -- retail ADDS the 1.45f back-off offset (fadd @0x7a5a89/0x7a5a97; 0x3fb9999a==1.45f);
	// Jan03 SUBTRACTED. The decomp is authoritative -> drop the PK 1.45m FORWARD along the facing
	// direction, not behind. (cp + 1.45*(cos a, sin a, 0).)
	CVec3 ptPKPos( ptPos.x + 1.45f * cos(fAngle), ptPos.y + 1.45f * sin(fAngle), ptPos.z );
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
	// @0x3a22b0 -- retail grew real validity gates + a reach test; the Jan03/dev version was a bare
	// `return UCR_OK`. `from` and `bIgnoreTarget` are accepted but UNUSED (faithful: the disasm reads
	// neither).
	if ( !IsValid( pUS ) )
		return UCR_GENERAL_FAILURE;
	if ( !IsValid( pCmd->pPassageObject ) )
		return UCR_GENERAL_FAILURE;
	// Reach test: retail reads the passage zone id (IPassageObject vtbl+0x14 == GetPassageZoneID) and
	// asks a unit-side component (pUS+0x2c vtbl+0x1e8) "is the unit in that passage zone", returning
	// UCR_OK when near else UCR_NOT_ALL_UNITS_NEAR_PASSAGE. That component virtual is opaque/unnameable
	// in this tree; CanPass is the in-tree equivalent reach predicate -- the same gate
	// CWorld::UsePassageObject applies before actually using the passage (wMain.cpp:2325).
	return pCmd->pPassageObject->CanPass( pUS ) ? UCR_OK : UCR_NOT_ALL_UNITS_NEAR_PASSAGE;
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
CExecCannon::CExecCannon( CUnitServer *_pUS, IObject *_pCannon, bool _bEnter, CCmdCannon *_pCmd ):
	CCommandExecute(_pUS), pCannon(_pCannon), bEnter(_bEnter), pCmd(_pCmd)   // retail @0x3a5e50
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

	// retail @0x3a2360 lock gate: drop our own reservation, probe whether SOMEONE ELSE holds the
	// cannon, then re-acquire it through the command; a foreign lock fails the executor. (Retail
	// probes/locks with two views of the owning unit; dev uses the unit's CUnit for BOTH so the
	// stored handle and the probe identity agree.)
	if ( IsValid( pCmd ) )
	{
		CDynamicCast<ILockable> pL( pCannon );
		if ( pL )
		{
			CUnit *pBy = pUS.GetPtr();   // NWorld::CUnit -- the lock identity is the world unit (CCmd::Lock signature)
			pCmd->Unlock();
			bool bForeignLock = pL->IsLocked( CastToObjectBase( pBy ) );
			pCmd->Lock( pBy );
			if ( bForeignLock )
				return UCR_GENERAL_FAILURE;
		}
	}
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
CExecCorpse::CExecCorpse( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bTake, CCmdTakeCorpse *_pCmd ):
	CCommandExecute(_pUS), pDeadUnit(_pCorpse), bTake(_bTake), pCmd(_pCmd)   // retail @0x3a6160
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

	// retail @0x3a61d0 tail lock gate: drop our reservation, probe the corpse's CLockable base for
	// a FOREIGN holder, re-acquire through the command; a foreign lock fails the executor. (The
	// drop path carries no command -- Unlock/Lock skip; the probe still runs and reads our own
	// take-time token as "not locked".)
	CUnit *pBy = pUS.GetPtr();   // NWorld::CUnit -- the lock identity is the world unit (CCmd::Lock signature)
	if ( IsValid( pCmd ) )
		pCmd->Unlock();
	bool bForeignLock = pDeadUnit->IsLocked( CastToObjectBase( pBy ) );
	if ( IsValid( pCmd ) )
		pCmd->Lock( pBy );
	return bForeignLock ? UCR_GENERAL_FAILURE : UCR_OK;
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
	// @0x3a62e0 -- ASSERTs replaced by retail carry-state guard; bCorpseInPK cached;
	// PK branch now FALLS THROUGH to StartAction (Jan03 early-return dropped).
	if ( GetState() == FINISHED )
		return;
	if ( bTake && pUS->IsCarryingCorpse() )		// can't take while already carrying
		return;
	if ( !bTake && !pUS->IsCarryingCorpse() )	// nothing to drop
		return;
	bCorpseInPK = pDeadUnit->IsWearingPK();		// cache [+0x24] -- read by TimeLabelReached/Cancel
	if ( bCorpseInPK )
	{
		// SpillPK: eject the corpse from its shell, lay it dead, re-seat the empty PK.
		CUnitServer *pPK = pDeadUnit->GetWearingPK();
		pPK->SetPosition( pDeadUnit->GetPosition() );
		pPK->WearAsPK( false );
		pDeadUnit->FlipPanzerklein( 0 );
		// retail @0x3a62e0 (SpillPK): Die(pos, VNULL3, bPlayDeath=FALSE) -- the spilled pilot corpse
		// drops as a pure ragdoll, no death clip and no synthetic (1,0) push direction.
		pDeadUnit->animator.Die( pDeadUnit->GetPosition(), VNULL3, false, pDeadUnit );
		pUS->GetWorld()->GetPathNetwork()->Unlock( pDeadUnit );
	}
	else if ( bTake )
	{
		pUS->DoAction( NRPG::AC_TAKE_CORPSE );
		pUS->animator.TakeCorpse( pUS->GetPosition() );
	}
	else
	{
		pUS->animator.DropCorpse( pUS->GetPosition() );
		NDb::SAISound sound = { NDb::GetAISound( 27 ), 0, 1.0f };   // retail @0x3a62e0: no silencer on corpse drop
		pUS->GetWorld()->MakeAISound( sound, pUS, 0 );
	}
	StartAction( pUS->GetWorld(), SKIPPABLE );		// ALL branches reach this
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecCorpse::TimeLabelReached()
{
	// @0x3a64d0 -- leading StopAction(); branch on cached bCorpseInPK; take-branch
	// validity/carrier re-check that abandons the carry pose (DropCorpse) when the
	// corpse became invalid or got taken by someone else during the animation.
	StopAction();
	if ( bCorpseInPK )
		return false;
	if ( bTake )
	{
		if ( !IsValid( pDeadUnit ) || pDeadUnit->GetCorpseCarrier() )
		{
			pUS->animator.DropCorpse( pUS->GetPosition() );
			return false;
		}
		pDeadUnit->animator.BeTaken( pUS, &pUS->animator, pDeadUnit );
		pUS->SetState( new CUnitStateCorpseCarrier( pUS, pDeadUnit ) );
	}
	else
		pUS->SetState( new CUnitStateNormal( pUS ) );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCorpse::Cancel()
{
	// @0x3a6610 -- reversal now gated on !bCorpseInPK; CalmCorpse dropped (retail
	// dropped it); StopAction() reordered ahead of AlignTime/PlaceUnit.
	// NOTE: retail gates each SetState on GetStateType()==8 (corpse-carrier state-
	// machine id); a5dll has no GetStateType -> kept unconditional SetState (FLAGGED).
	if ( !bCorpseInPK )
	{
		if ( bTake )
		{
			pUS->animator.DropCorpse( pUS->GetPosition() );
			pDeadUnit->animator.BeDropped( pDeadUnit );
			pUS->SetState( new CUnitStateNormal( pUS ) );
		}
		else
		{
			pUS->animator.TakeCorpse( pUS->GetPosition() );
			pDeadUnit->animator.BeTaken( pUS, &pUS->animator, pDeadUnit );
			pUS->SetState( new CUnitStateCorpseCarrier( pUS, pDeadUnit ) );
		}
	}

	StopAction();
	pUS->animator.AlignTime( 50 );
	pUS->animator.PlaceUnit( pUS->GetPosition() );
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTakeCorpseOnDeploy
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecTakeCorpseOnDeploy::CExecTakeCorpseOnDeploy( CUnitServer *_pUS, CUnitServer *_pCorpse ):
	CCommandExecute(_pUS), pDeadUnit(_pCorpse), n( 0 )
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
	// @0x3a2760 -- retail: the Jan03 pre-corpse bookkeeping (SetState Unconscious/Death +
	// pDeadUnit->InitAsCorpse(bDead)) is NOT done here; it was moved upstream (unit is already
	// a corpse by deploy time). Retail also forces the carrier to WALK pose before laying the
	// corpse. See src/s2_cexectakecorpseondeploy.h header note (disasm-verified).
	ASSERT( IsValid( pDeadUnit ) );
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pDeadUnit ) || !IsValid( pUS ) )
		return;
	// retail loads both units into REGISTERS at entry and never re-reads the members
	// (disasm @0x3a2760). The calls below (SetPosition / animator Init*) can reach
	// CUnitServer::CancelAction / CheckCmdExecState, which null CUnitServer::pExec and so
	// DELETE this executor mid-Run -- re-reading this->pUS afterwards is a use-after-free
	// (AV in the CUnitStateCorpseCarrier ctor on base-zone deploy). Strong locals reproduce
	// the retail register caching and pin both units.
	CPtr<CUnitServer> pCarrier = pUS;
	CPtr<CUnitServer> pCorpse = pDeadUnit;
	//
	pCorpse->GetWorld()->GetPathNetwork()->Unlock( pCorpse );
	pCorpse->animator.InitAsCorpse( pCorpse->GetPosition() );
	int nCorpseDir = (int)( pCarrier->GetPosition().GetDir() ) - 4;
	if ( nCorpseDir < 0 )
		nCorpseDir += 8;
	else if ( nCorpseDir > 7 )
		nCorpseDir -= 8;
	NAI::SUnitPosition pos = pCarrier->GetPosition();
	pos.pos.p.SetDirection( nCorpseDir );
	// carrier WALK-pose gate (@0x3a2760: GetPose @0x4911a0 cmp eax,2; SetPose @0x491120)
	if ( pos.GetPose() != NAI::WALK )
	{
		pos.SetPose( NAI::WALK );
		pCarrier->SetPosition( pos );
		pCarrier->animator.PlaceUnit( pCarrier->GetPosition() );
	}
	pCorpse->SetPosition( pos );
	pCorpse->GetWorld()->GetPathNetwork()->Unlock( pCorpse );
	//
	pCarrier->animator.InitAsCorpseCarrier( pCarrier->GetPosition() );
	pCarrier->SetState( new CUnitStateCorpseCarrier( pCarrier, pCorpse ) );
	pCorpse->animator.BeTaken( pCarrier, &pCarrier->animator, pCorpse );
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
	// @0x3a6840 -- retail adds a diplomacy gate absent in Jan03: a medic must not "heal" an
	// enemy. Taken only when a real target is considered (bIgnoreTarget==false) and BOTH the
	// medic and the patient are live objects; pTarget->GetDiplomacyState(pUS)==DS_ENEMY (==0).
	if ( !bIgnoreTarget && IsValid( pUS ) && IsValid( pTarget ) &&
	     pTarget->GetDiplomacyState( pUS ) == NDb::DS_ENEMY )
		return UCR_GENERAL_FAILURE;

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

	// @0x3a68b0 -- retail distinguishes plain first-aid from power-armour REPAIR. The gate is on the
	// TARGET being a Panzerklein (worn PK, or an empty PK suit); only then is the medic's ACTIVE
	// inventory item probed, driving the finer "dedicated repair kit" flag (a CFirstAidItem whose
	// first-aid DB record id == 0x1a). DoAction picks AC_REPAIR_PK vs AC_FIRSTAID by the PK gate;
	// StartHealing takes (bPanzerklein=gate, bRepairKit). (disasm-verified arg wiring.)
	bool bTargetIsPK = pTarget->IsWearingPK() || pTarget->IsEmptyPK();
	bool bRepairKit  = false;
	if ( bTargetIsPK )
	{
		CDynamicCast<NRPG::CFirstAidItem> pKit( pUS->GetUnitRPG()->GetInventory()->GetActive() );
		if ( IsValid( pKit ) )
		{
			// @0x3a68b0 -- retail null-guards GetDBFirstAid()'s RETURN (test eax; je) before reading the id.
			NDb::CRPGFirstAid *pRec = pKit->GetDBFirstAid();
			if ( pRec && pRec->GetRecordID() == 0x1a )   // 0x1a == PK repair-kit record
				bRepairKit = true;
		}
	}

	CVec3 ptTarget;
	pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), -1 );
	pUS->animator.StartHealing( pUS->GetPosition(), NAI::GetBlowHeight( pUS->GetPosition(), ptTarget ), bTargetIsPK, bRepairKit );
	StartAction( pUS->GetWorld(), SKIPPABLE );
	pUS->DoAction( bTargetIsPK ? NRPG::AC_REPAIR_PK : NRPG::AC_FIRSTAID );

	NDb::SAISound sound = { NDb::GetAISound( 20 ), 0, 1.0f };   // retail @0x3a68b0: no silencer on first aid
	pUS->GetWorld()->MakeAISound( sound, pUS, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecHeal::AnimationFinished()
{
	StopAction();
	Finished();

	// @0x3a6ac0 -- inside the same target-is-Panzerklein gate as Run: for a dedicated repair kit
	// install the welder hand effect (NDb::GetEffect(0x698), stamped with the current world time),
	// and always (in gate) set the custom idle animation (0x1183 repair / 0x1181 plain PK heal).
	if ( pTarget->IsWearingPK() || pTarget->IsEmptyPK() )
	{
		bool bRepairKit = false;
		CDynamicCast<NRPG::CFirstAidItem> pKit( pUS->GetUnitRPG()->GetInventory()->GetActive() );
		if ( IsValid( pKit ) )
		{
			// @0x3a6ac0 -- null-guard the DB record before reading its id (matches the disasm test eax; je).
			NDb::CRPGFirstAid *pRec = pKit->GetDBFirstAid();
			if ( pRec && pRec->GetRecordID() == 0x1a )
				bRepairKit = true;
		}

		if ( bRepairKit )
			pUS->SetHandEffect( NDb::GetEffect( 0x698 ), pUS->GetWorld()->GetTime()->GetValue() );
		pUS->animator.SetCustomIdleAnimation( NDb::GetAnimation( bRepairKit ? 0x1183 : 0x1181 ) );
	}
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
	// @0x3a2450 -- retail gates FIRST on the active item being an undestroyed grenade in SETTRAP
	// mode (IGrenadeItemInfo vtbl[0] = GetMode() == GM_SETTRAP == 1, the oracle's
	// kSetTrapRequiredGrenadeType); a missing / throw-mode grenade -> UCR_UNAVAILABLE, evaluated
	// BEFORE the target check. Mirrors the in-tree gate at wUnitAttack.cpp:516.
	CDynamicCast<NRPG::IGrenadeItem> pGrenade( pUS->GetUnitRPG()->GetInventory()->GetActive() );
	if ( !IsValid( pGrenade ) || pGrenade->GetMode() != NRPG::GM_SETTRAP )
		return UCR_UNAVAILABLE;

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
			// retail CExecSetTrap::TimeLabelReached @0x3a99e0: seed the placer's explosive-perk mods {1,1,0}+Fill
			// and hand them to the trapped door; a plain grenade arms via the 3-arg SetTrap @0x381ee0, an ENGINEER
			// grenade via the 4-arg overload @0x381f90 with the placer's effective ST_ENGINEERING value (retail
			// inlines CDynamicSkill: bFreezedToMax ? nMaxValue : min(nMaxValue,nValue) on skills[8] -- dev's
			// Skills() operator int) and GetGrenadeTrapDC( 0 ).
			NDb::CRPGGrenade *pRPGGrenade = pGrenade->GetDBGrenade();
			NDb::CRPGEngGrenade *pRPGEngGrenade = pGrenade->GetDBEngGrenade();
			SPerkMineModifiers mods;
			mods.Fill( pRPG->GetRPGUnit() );
			if ( pRPGGrenade )
			{
				if ( pTarget->SetTrap( pRPGGrenade, pRPG->GetGrenadeTrapDC( pRPGGrenade ), &mods ) )
				{
					CUnitServer::SResItem item;
					pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), true );
					pUS->AddToVisibleTraps( pTarget );
				}
			}
			else if ( pRPGEngGrenade )
			{
				int nEngSkill = pRPG->GetRPGUnit()->Skills( NDb::ST_ENGINEERING );
				if ( pTarget->SetTrap( pRPGEngGrenade, pRPG->GetGrenadeTrapDC( 0 ), &mods, nEngSkill ) )
				{
					CUnitServer::SResItem item;
					pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot(), true );
					pUS->AddToVisibleTraps( pTarget );
				}
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
// Spend one charge off the mine-clearing tool and, if that emptied it, take it off the unit.
//
// Retail INLINES this identically into both disarm executors -- CExecDisarmMine::TimeLabelReached
// @0x3a8770 (disasm @0x7a880b..0x7a8892) and CExecDisarmTrap::TimeLabelReached @0x3a84f0 -- factored
// here because it is byte-for-byte the same sequence in each. Decoded from raw disasm:
//
//   mov edx,[edi] / mov ecx,edi / call [edx+0xc]        ; pTool->SpendOneCharge()  (IToolItem slot 3)
//   mov eax,[edi+4] / mov ecx,[eax+0x14]                ; vbtable[5] -> IItemContainerInfo vbase
//   mov edx,[ecx+edi+4] / lea ecx,[ecx+edi+4]
//   call [edx+8] / test al,al / je <roll>               ; pTool->IsEmpty()         (slot 2)
//   mov eax,[ebp] / mov ecx,ebp / call [eax+0x24]       ; pInventory->GetActiveSlot()  (slot 9)
//   push eax / mov ecx,ebp / call [eax+0x48]            ; pInventory->TakeOff(slot)    (slot 18)
//   <AddRef>                                            ; CObj<> temp over the taken-off item
//   mov eax,[esi+0x14] / mov byte [eax+0xbc],0          ; animator.bActiveItem      = false
//   mov dword [eax+0xd4],0                              ; animator.eAnimationWeaponType = WT_DEFAULT (0)
//   mov ecx,[eax+0x30] / test ecx,ecx / je .. / test byte [ecx+7],0x80 / jne ..
//   mov eax,[eax+0x34] / push eax / call 0x6cf930       ; bindGlobal.Update() (IsValid(pSync)-guarded)
//   <ReleaseObj>                                        ; CObj<> temp dies HERE, after the reset+Update
//
// ‼️ ORDERING (the whole point of this fix -- read off the raw disasm, not inferred):
//   1. SpendOneCharge is UNCONDITIONAL and happens on the ATTEMPT, *before* the CanClear skill roll.
//      A failed disarm still costs a charge.
//   2. The IsEmpty gate runs immediately after the spend and does NOT block the action -- the roll at
//      @0x7a88ae runs either way. Emptiness only removes the tool; it never cancels the disarm.
// Field offsets resolved from the PDB: pUS+0xbc = animator(+88)+100 = bActiveItem (byte store),
// pUS+0xd4 = animator(+88)+124 = eAnimationWeaponType (dword store, WT_DEFAULT == 0),
// pUS+0x30/+0x34 = bindGlobal{pSync,nID} (CSyncSrcBind<IVisObj>).
//
// (Retail hoists pRPG->GetInventory() to the top of TimeLabelReached, outside both the tool-validity
// check and this branch; it is a plain member accessor with no side effects, so fetching it here where
// it is actually used is behaviourally identical.)
static void SpendMineClearingTool( CUnitServer *pUS, NRPG::IUnitMission *pRPG, NRPG::IToolItem *pTool )
{
	pTool->SpendOneCharge();
	if ( !pTool->IsEmpty() )
		return;
	// The tool is spent out: retail takes it off the active slot and drops the returned CObj<> temp,
	// which frees the empty tool -- but only after the animator reset and the sync update below.
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	CObj<NRPG::IInventoryItem> pSpent = pInventory->TakeOff( ( NDb::ESlot )pInventory->GetActiveSlot() );
	pUS->animator.SetActiveItem( false );
	pUS->animator.SetWeaponAnimation( NDb::WT_DEFAULT );
	pUS->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecDisarmTrap::CExecDisarmTrap( CUnitServer *_pUS, CWindowDoor *_pTarget ) : 
	CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecDisarmTrap::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	// @0x3a6e50 -- retail tests the TOOL first (UCR_UNAVAILABLE when none equipped,
	// UCR_NEED_HIGHER_SKILL when the unit can't use it) BEFORE the target tests, unlike
	// the Jan03 target-first form this used to carry. FOLLOW THE DECOMP.
	NRPG::IToolItem *pTool = GetMineClearingTool( pUS );
	if ( !IsValid( pTool ) )
		return UCR_UNAVAILABLE;
	if ( !pTool->CanBeUsed( pUS->GetUnitRPG()->GetRPGUnit() ) )
		return UCR_NEED_HIGHER_SKILL;

	if ( IsValid( pTarget ) && !bIgnoreTarget )
	{
		if ( pUS->GetTBSPlayer()->CanSeeTrap( pTarget ) && pTarget->IsMineSet() )
			return UCR_OK;
		return UCR_GENERAL_FAILURE;
	}
	return UCR_NO_TARGET;
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
			// @0x7a858c -- the charge is spent on the ATTEMPT, BEFORE the CanClear roll below, and the
			// emptiness check that follows it does not gate the roll. See SpendMineClearingTool.
			SpendMineClearingTool( pUS, pRPG, pTool );
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
			CMine *pRes = new CMine( pUS->GetWorld(), pCmd->ptDst.GetCP(), pRPGMine, pRPG->GetMineDC( pRPGMine ), pCmd->ptDst.GetFloor(), pUS );   // thread the placer for save (tag 12) + blast attribution
			// retail @0x3a9c00: scale the placed mine's explosion damage by the placer's explosive perks
			// (structure/AE damage + always-human-critical). Stored on the mine; applied at detonation (GoBoom).
			SPerkMineModifiers mods;
			mods.Fill( pRPG->GetRPGUnit() );
			pRes->SetPerkModifiers( mods );
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
		// check whether we have the special tool
		NRPG::IToolItem *pTool = GetMineClearingTool( pUS );
		if ( !IsValid( pTool ) )
			return UCR_GENERAL_FAILURE;
		// @0x3a70e0 -- RETAIL skill gate (absent from Jan03, which fell straight to UCR_OK):
		// the clearing tool must be usable by this unit's RPG unit (skill high enough).
		// Decomp: tool->CanBeUsed( pUS->GetUnitRPG()->GetRPGUnit() ); false -> NEED_HIGHER_SKILL.
		if ( !pTool->CanBeUsed( pUS->GetUnitRPG()->GetRPGUnit() ) )
			return UCR_NEED_HIGHER_SKILL;
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
			// @0x7a880b -- the charge is spent on the ATTEMPT, BEFORE the CanClear roll below, and the
			// emptiness check that follows it does not gate the roll. See SpendMineClearingTool.
			SpendMineClearingTool( pUS, pRPG, pTool );
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
// @0x3a7280 -- release ctor also records hitLocation (3rd param). The called-shot 3rd arg is
// deferred (see header note / FLAGGED); default to HL_BODY so the tag-3 save round-trips cleanly.
CExecSnipeAim::CExecSnipeAim( CUnitServer *_pUnitServer, CUnitServer *_pTarget ):
	CCommandExecute(_pUnitServer), pTarget(_pTarget), hitLocation(NAI::HL_BODY)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecSnipeAim::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const 
{ 
	if ( !pUS->CanSnipe() )
		return UCR_GENERAL_FAILURE;

	// @0x3a72e0 -- release wraps the cannon / working-weapon dispatch in a live-target guard:
	// a null or zombie (nObjData & 0x80000000) target short-circuits to UCR_NO_TARGET before the
	// dispatch. IsValid(pTarget) == pTarget!=0 && !pTarget->IsRefInvalid() -- the exact decode.
	if ( IsValid( pTarget ) )
	{
		if ( GetActionType( pUS ) == AT_CANNON )
		{
			if ( bIgnoreTarget )
				return UCR_NO_TARGET;

			CVec3 ptTarget;
			pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull(pTarget), NAI::HL_BODY );
			return CanAttackWithCannon( pUS->animator.GetCannon(), ptTarget );
		}

		return HasWorkingWeapon( pUS );
	}

	return UCR_NO_TARGET;
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

	// @0x3a19a0 -- release derives the snipe-ray END POINT from the target's AIMap hit-location
	// world pos (GetUnitHLPos at hitLocation, HL_ANY clamped to HL_BODY) MINUS the attack origin,
	// not Jan03's pTarget->GetCP() - pUS->GetCP(). hitLocation stays HL_BODY here (called-shot
	// deferred), so this reduces to the body-centre ray for the un-called case.
	CRay ray;
	ray.ptOrigin = pUS->GetAttackOrigin( pUS->GetPosition() );
	pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ray.ptDir, pUS->GetWorld()->GetAIMap()->GetHull(pTarget),
		hitLocation == NAI::HL_ANY ? NAI::HL_BODY : hitLocation );
	ray.ptDir = ray.ptDir - ray.ptOrigin;

	StartAction( pUS->GetWorld(), SKIPPABLE );

	// @0x3a19a0 -- release order: when not already sniping, PREPARE (if not aimed) THEN Snipe THEN
	// enter the sniping state; the already-sniping path just (re)aims. Jan03 Snipe'd once before the
	// IsSniping test. animator.IsAiming() == (bAimed||bAimedStrafe) is the decode's not-aimed guard.
	if ( !pUS->IsSniping() )
	{
		if ( !pUS->animator.IsAiming() )
			pUS->DoAction( NRPG::AC_PREPARE );
		pUS->animator.Snipe( pUS->GetPosition(), ray );
		pUS->SetState( new CUnitStateSniping( pUS, pTarget, 0 ) );	// 4th hitLocation arg deferred (FLAGGED)
	}
	else
	{
		pUS->animator.Snipe( pUS->GetPosition(), ray );
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
	// @0x3a8a50 -- retail caches GetResiduaryAP() and ADDS the (res < start) clamp Jan03 lacked
	if ( !pUS->IsSniping() )
		return UCR_INVALID_COMMAND;
	//
	const int nRes = GetResiduaryAP();
	if ( nRes < 1 )
		return UCR_GENERAL_FAILURE;
	//
	const int nStart = GetStartAP();
	if ( nStart < 1 || nRes < nStart )
		return UCR_NOT_ENOUGH_AP;
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecCollectSnipeAP::GetStartAP() const
{
	// @0x3a8aa0 -- retail returns the fixed 1AP/10AP amounts UNCLAMPED; the residuary cap (Min)
	// applies ONLY to the CSAP_MAX / CSAP_ALL paths. (CSAP_PRECISE -> this->nAP is a release-only
	// mode deliberately omitted from this build; see AILog.cpp -- so it falls through to 0.)
	switch( eAP )
	{
		case CSAP_1AP:
			return 1;
		case CSAP_10AP:
			return 10;
		case CSAP_MAX:
		{
			int nCap = pUS->GetUnitRPG()->GetSkillValue( NDb::ST_AP ) -
				pUS->GetUnitRPG()->GetActionAP( NAI::WALK, NRPG::AC_SHOOT );
			return Min( GetResiduaryAP(), nCap );
		}
		case CSAP_ALL:
		{
			int nCap = pUS->GetUnitRPG()->GetSkillValue( NDb::ST_AP );
			return Min( GetResiduaryAP(), nCap );
		}
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCollectSnipeAP::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );
	pUS->CollectSnipeAP( GetStartAP() );
	Finished();		// @0x3a1b40 -- retail sets state=FINISHED inline; one-shot must reap itself
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecThrowKnife
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecThrowKnife::CExecThrowKnife( CUnitServer *_pUS, const CVec3 &_ptTarget, CUnitServer *_pTarget ):
	CExecAttack(_pUS), ptTarget(_ptTarget), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a7470 -- targeted-unit ctor (RETAIL ADDITION; Jan03 had only the point ctor). Records eHL and
// derives the aim point from the target unit's hit location instead of taking an explicit ptTarget.
// The exact retail aim helper (pUS->GetWorld()->...->vtbl[0x28](&ptTarget, pTarget->ObjBase, eHL)) is
// not mapped in-tree; reproduced with the proven AIMap hit-location idiom (cf. CExecShootUnit ctor).
CExecThrowKnife::CExecThrowKnife( CUnitServer *_pUS, NAI::EHitLocation _eHL, CUnitServer *_pTarget ):
	CExecAttack(_pUS), pTarget(_pTarget), eHL(_eHL)
{
	if ( IsValid( pTarget ) )
	{
		NAI::EHitLocation eHitLoc = ( eHL == NAI::HL_ANY ) ? NAI::HL_BODY : eHL;
		pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull( pTarget ), eHitLoc );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecThrowKnife::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	if ( !HasThrowingKnife( pUS ) )
		return UCR_NO_EQUIPMENT;

	if ( bIgnoreTarget )
		return UCR_NO_TARGET;

	// @0x3a7540 -- RETAIL cover/LOS gate (absent in Jan03): build the attack portion and reject the
	// throw when cover/line-of-sight blocks it, matching the other ranged CExec*::CanDoIt (CExecShootUnit/
	// CExecShootTile). Decomp: CExecAttack::CreateAttack(this,&portion,pTarget,false,true) then NRPG::CanShoot
	// over the portion (free-point on ptTarget when pTarget==0, else object pTarget+eHL).
	CWorld *pWorld = pUS->GetWorld();
	vector<NRPG::CAttackPortion> attack;
	if ( !CreateAttack( &attack, pTarget, false ) || attack.empty() )
		return UCR_GENERAL_FAILURE;
	CObj<NRPG::CCoverInfo> pCover;
	if ( pTarget )
		pCover = pWorld->GetGame()->CalcCovers( pUS->GetAttackOrigin( from ), attack[0], pUS, pTarget, eHL, pUS->GetMinClearDistance() );
	else
		pCover = pWorld->GetGame()->CalcCoversForTile( pUS->GetAttackOrigin( from ), attack[0], pUS, ptTarget, pUS->GetMinClearDistance() );
	if ( !NRPG::CanShoot( pCover ) )
		return UCR_GENERAL_FAILURE;

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
void CExecThrowKnife::OnLabel()
{
	// (the Jan03 mission-side bullet cursor is GONE in retail; the knife to-hit ignores the
	// bullet index, so nothing replaces the old NextBullet() advance here)
	ThrowKnife();
	// @0x3ab700 -- retail marks the attack canceled after the throw label fires so CExecAttack::TimeLabelReached
	// finishes the executor for this one-shot throw (the decomp writes [esi+0x18]=1). Jan03 omitted this.
	// NOTE: the retail "knife gone / torn-off" feedback sound is deferred.
	bAttackCanceled = true;
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
		// we miss
		// find where the knife will actually fly
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
// retail @0x3ab3c0: create the DB item (clue variant when bClue) and stash it into the backpack --
// SYNCHRONOUS free function, no executor/AP/animation. NO_PLACE falls back to the ground drop.
void CreateInventoryItemForUnit( CUnitServer *pUS, CCmdCreateInventoryItem *pCmd )
{
	CObj<NRPG::IInventoryItem> pItem;
	if ( pCmd->GetClue() )
		pItem = NRPG::CreateClueItem( pCmd->GetItem() );
	else
		pItem = NRPG::CreateItem( pCmd->GetItem()->pSuccessor );
	if ( !IsValid( pItem ) || !IsValid( pUS ) )		// retail @0x3ab3c0 guard
		return;
	CObj<CCmdMoveInventoryItem> pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::BACKPACK, CTPoint<int>( -1, -1 ) ) );
	if ( CanMoveInventoryItem( pUS, pFake ) == UCR_INVENTORY_NO_PLACE )
		pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::GROUND ) );

	MoveInventoryItem( pUS, pFake );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecCreateInventoryItem::Run()
{
	// retail @0x3ab770: delegate to the shared direct insert, then latch FINISHED
	CreateInventoryItemForUnit( pUS, pCmd );
	Finished();		// @0x3ab770 -- retail Run latches state=FINISHED (mov [esi+0x10],1) right after the create+move; without it CSimpleExecQueue::Run sees RUNNING (wUnitQueue.cpp L57) and never pops this executor -> the unit's command queue hangs
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
// @0x3ab790 -- match the retail decode (re-read get_decomp.py 0x3ab790). The binary does NOT vacate
// the slot first and does NOT call Inventory::Activate: "activate" is achieved purely by moving the
// freshly created item from the GROUND into the hand SLOT (nSlot) -- the SLOT-destination move equips
// it. On UCR_INVENTORY_NO_PLACE the item is dropped on the GROUND (disasm fallback builds GROUND<->pItem,
// mirroring CExecCreateInventoryItem::Run, SLOT instead of BACKPACK). The prior vacate pre-step, the
// BACKPACK fallback and the explicit Activate were predecessor guesswork that diverged from the binary.
void CExecCreateAndActivateInventoryItem::Run()
{
	int nSlot = pCmd->GetSlot();
	// create the item from the DB record and slot it directly into the hand (nSlot)
	CObj<NRPG::IInventoryItem> pItem = NRPG::CreateItem( pCmd->GetItem()->pSuccessor );
	CObj<CCmdMoveInventoryItem> pFake = new CCmdMoveInventoryItem(
		SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::SLOT, nSlot, pItem ) );
	// slot full -> drop the new item on the ground instead (matches the @0x3ab790 NO_PLACE branch)
	if ( CanMoveInventoryItem( pUS, pFake ) == UCR_INVENTORY_NO_PLACE )
		pFake = new CCmdMoveInventoryItem( SItem( 0, SItem::GROUND, pItem ), SItem( pUS, SItem::GROUND ) );
	MoveInventoryItem( pUS, pFake );
	Finished();		// @0x3ab790 -- latch state=FINISHED (mov [edi+0x10],1) after the create+move, exactly like
					// the sibling CExecCreateInventoryItem::Run; without it CSimpleExecQueue::Run sees RUNNING
					// and never pops the executor -> the unit command queue hangs. (adversarial-verify catch)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CExecExchangeInventoryItems REMOVED -- see wUnitAttackExec.h; the exchange is composed in
//  CreateExecutor from registered execs like retail @0x3b37b0)
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

	// @0x3abc90 -- retail opens Run by registering the classified reach action (and
	// spending its AP) BEFORE touching the inventory (disasm: DoAction(GetActionType())).
	pUS->DoAction( GetActionType() );

	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	////
	NDb::ESlot slot = (NDb::ESlot)pInventory->GetActiveSlot();
	NRPG::IInventoryItem* pItem = pInventory->Get( slot );
	////
	NDb::EItemSubType subType = NDb::SUBTYPE_NONE;
	if ( IsValid( pItem ) )
		subType = pItem->GetDBItem()->subType;

	SItem sInfo;
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

	// @0x3abc90 -- retail starts this move/deactivate action as SKIPPABLE, not NOBLOCK
	// (disasm: CCommandExecute::StartAction(this, pUS->GetWorld(), SKIPPABLE)).
	StartAction( pUS->GetWorld(), SKIPPABLE );
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
	// retail @0x3a1b70: activate the newly-active item HIDDEN unless its DB record marks it bPlaceInHand
	// (IsActiveItemToShow). Clear the undraw flag first so a shown item actually renders (retail zeroes the
	// target unit's bUndrawWeapon immediately before ActivateItem).
	bool bHide = !NWorld::IsActiveItemToShow( pInventory );
	pUSTarget->SetUndrawItem( false );
	if ( subType == NDb::SUBTYPE_HEAVY || subType == NDb::SUBTYPE_MINE_DETECTOR )
		pUSTarget->animator.ActivateItem( pUSTarget->GetPosition(), true, true, NDb::BELT_M1, pRPG->GetWeaponType(), bHide );
	else
	{
		int nPlace = pInventory->GetPlaceBySubType( subType );
		pUSTarget->animator.ActivateItem( pUSTarget->GetPosition(), false,
			nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType(), bHide );
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
// @0x3a7990 -- classify the queued inventory move into the NRPG::EAction whose reach
// animation should play (AC_NONE when no body animation / AP is needed). Retail-only:
// Jan03 has no CExecMoveInventoryItem::GetActionType, so the disasm @0x7a7990 is the sole
// authority. Reads the REAL CCmdMoveInventoryItem source/target SItem members.
NRPG::EAction CExecMoveInventoryItem::GetActionType() const
{
	// AI-driven inventory moves are instantaneous (no reach animation).
	if ( NAI::IsAIPlayer( pUS->GetPlayer() ) )
		return NRPG::AC_NONE;

	const SItem &src = pCmd->GetSource();
	const SItem &dst = pCmd->GetTarget();

	// Pull off the ground into a hand / generic unit place -> a "take" pickup.
	if ( src.eType == SItem::GROUND &&
		( dst.eType == SItem::UNIT_ANYPLACE || dst.eType == SItem::HAND ) )
		return NRPG::AC_ITEM_TAKE;

	// Dropping an item onto the ground needs no reach animation.
	if ( dst.eType == SItem::GROUND )
		return NRPG::AC_NONE;

	if ( src.eType == SItem::HAND )
	{
		// Retail re-fetches the in-hand item and compares its ORIGINAL drag placement
		// (sHandItem.eType) against the destination. Now that GetInHandItem returns the full SItem
		// and MoveInventoryItem stores the drag origin into the hand, that compare is exact again:
		//   @0x7a7990-> if (sInfo.eType == dst.eType) AC_NONE          -- putting it straight back
		//               else if (sInfo.eType == SLOT) AC_ITEM_SLOT     -- it came out of a slot
		//               else if (dst.eType != SLOT)   AC_ITEM_TAKE
		//               else                          AC_ITEM_SLOT
		//
		// ORIGINAL BUG (confirmed by disasm @0x3a7990): retail DISCARDS GetInHandItem's bool and reads
		// sInfo.eType regardless. SItem's default ctor initialises only the five smart pointers, so
		// when the fetch fails eType is uninitialised stack -- the compare below is then a read of
		// indeterminate memory. Latent in practice: this branch runs only for src.eType == HAND, i.e.
		// a command whose source IS the hand, so the fetch succeeds. Reproduced 1:1 (an `if (!...)`
		// guard here would be a silent behaviour change on the very path retail leaves undefined).
		// NB this arm is newly reachable: before the SItem migration this code read only sInfo.pUnit,
		// which the CPtr ctor always zeroes.
		SItem sInfo;
		pUS->GetTBSPlayer()->GetInHandItem( &sInfo );
		CUnit *pInfoUnit = sInfo.pUnit.GetPtr();
		CUnit *pDstUnit  = dst.pUnit.GetPtr();
		if ( pInfoUnit == pDstUnit || pInfoUnit == 0 || pDstUnit == 0 )
		{
			if ( sInfo.eType == dst.eType )
				return NRPG::AC_NONE;
			if ( sInfo.eType == SItem::SLOT )
				return NRPG::AC_ITEM_SLOT;
			if ( dst.eType != SItem::SLOT )
				return NRPG::AC_ITEM_TAKE;
			return NRPG::AC_ITEM_SLOT;
		}
		// in-hand item held by a different unit -> a cross-unit transfer
	}
	else if ( src.pUnit.GetPtr() == dst.pUnit.GetPtr() ||
		src.pUnit.GetPtr() == 0 || dst.pUnit.GetPtr() == 0 )
		return NRPG::AC_NONE;

	return NRPG::AC_ITEM_TRANSFER;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a7b40 -- start-AP for the move == the unit's AP cost of the classified action
// (disasm: return CDumbUnitServer::GetActionAP(pUS, GetActionType())).
int CExecMoveInventoryItem::GetStartAP() const
{
	return pUS->GetActionAP( GetActionType() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecPlayAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3a7cc0 — retail stores arg3 into bFreezeAfterLastFrame (+0x1c), NOT the Jan03 bCircled.
CExecPlayAnimation::CExecPlayAnimation( CUnitServer *_pUS, int _nDBAnimationID, bool _bFreezeAfterLastFrame ):
	CCommandExecute( _pUS ), nDBAnimationID( _nDBAnimationID ), bFreezeAfterLastFrame( _bFreezeAfterLastFrame )
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
// @0x3a7d10 — retail AnimationFinished is BYTE-FOR-BYTE identical to Cancel @0x3a7d40:
// it StopAction()+Finished() unconditionally and NEVER replays the clip.
//   ORIGINAL BUG (confirmed @0x3a7d10 disasm): a "circled" play-animation command does NOT loop;
//   the Jan03 replay branch was dropped in the retail build.
void CExecPlayAnimation::AnimationFinished()
{
	StopAction();
	Finished();
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
	// @0x3a2580 -- one-shot command: retail sets state=FINISHED at the end (decomp +0x10=1).
	// Without Finished() the unit command Segment loop keeps state==RUNNING and re-invokes
	// Run() every tick, replaying the OnTalk lua callback + the dialog. Jan03's Run() omitted
	// it; the decomp wins. Finished() only flips state (no executor free) -> re-entrancy-safe.
	NScript::luaCallFunction( "OnTalk", "pp", pUS.GetBarePtr(), pTarget.GetBarePtr() );
	int nDialogID = pTarget->GetDialog();
	if ( nDialogID > 0 )
		PlayDialog( pUS->GetWorld(), nDialogID );
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecTalk::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget )
{
	// @0x3a1cf0 -- retail splits the two failure paths (Jan03 collapsed both to
	// UCR_GENERAL_FAILURE). CanTalk()==false -> UCR_GENERAL_FAILURE; CanTalk() but
	// !IsHero() -> UCR_NOT_HERO, which the result classifier treats as a SILENT no-op
	// (no error log/sound), unlike the generic UCR_GENERAL_FAILURE feedback.
	// ORIGINAL BUG (confirmed @0x3a1cf0 disasm): non-hero talk yields the distinct
	// UCR_NOT_HERO, not UCR_GENERAL_FAILURE.
	if ( !pTarget->CanTalk() )
		return UCR_GENERAL_FAILURE;
	return pUS->GetUnitRPG()->GetRPGUnit()->IsHero() ? UCR_OK : UCR_NOT_HERO;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecNotHeroWantsToTalk
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecNotHeroWantsToTalk::Run()
{
	// @0x3b56f0 -- fire the "non-hero wants to talk" event (consumed by CAckNPCInteraction), then finish,
	// exactly like the sibling one-shot executors (CExecOrderConfirmation etc.).
	NGlobal::ThrowEvent( CEventOnNotHeroWantsToTalk( pUS ) );
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x50133145, CExecNotHeroWantsToTalk )
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
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
REGISTER_SAVELOAD_CLASS( 0x71983141, CExecPanzerklein )
REGISTER_SAVELOAD_CLASS( 0x71983142, CExecSetTrap )
REGISTER_SAVELOAD_CLASS( 0x71983143, CExecDisarmTrap )
REGISTER_SAVELOAD_CLASS( 0x71983144, CExecSetMine )
REGISTER_SAVELOAD_CLASS( 0x71983145, CExecDisarmMine )
REGISTER_SAVELOAD_CLASS( 0xB3120160, CExecCreateInventoryItem )
REGISTER_SAVELOAD_CLASS( 0xA0123140, CExecCreateAndActivateInventoryItem )
