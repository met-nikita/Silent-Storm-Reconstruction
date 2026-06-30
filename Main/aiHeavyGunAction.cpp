#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiUnitState.h"      // NAI::SAIUnitState (complete): GetKnownEnemies / pEnemy / pPossibleEnemy
#include "aiPosition.h"
#include "aiPath.h"
#include "aiMap.h"
#include "AILog.h"             // CAILogShot
#include "aiCombatLog.h"       // CAILogUseCannon / CAILogExitCannon (release log records)
#include "wMain.h"             // NWorld::CWorld: GetCannons / GetPathNetwork / GetAIMap
#include "wUnitServer.h"       // NWorld::CUnitServer: GetWorld / GetUnitRPG / GetAIMapHull / Get/SetWishPose
#include "wMainPath.h"         // NWorld::FindPath
#include "wMainMoves.h"        // NWorld::GetMoveActionType
#include "wObject.h"           // NWorld::CCannon / ICannon
#include "wUnitAttack.h"       // NWorld::CanAttackWithCannon
#include "wUnitCommands.h"     // NWorld::UCR_OK / UCR_NEED_RELOAD
#include "rpgUnitMission.h"    // NRPG::IUnitMission::GetActionAP + NRPG::AC_APPROACH_CANNON
#include "RPGItem.h"           // NRPG::IWeaponItem::SetShootMode / HasAmmo
#include "..\DBFormat\DataRPG.h" // NDb::EShootMode
//
#include "aiActions.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The heavy-gun (stationary cannon) action set: dock with the best reachable gun, fire it at the
// current enemy, abandon it when it stops being useful. Reconstructed from the matched-release decode
// (oracle: decomp/src/s2_aiheavygunaction.h: @0x52760/@0x529e0/@0x52c00 + the helpers @0x51f20..
// @0x525b0). Every decode hook resolves to a real in-tree call (the cannon subsystem -- CCannon/ICannon,
// IAIUnit::GetCannon, CanAttackWithCannon, the AI-map aim query, the cannon-approach path-AP -- already
// exists; only the world cannon enumeration was added, CWorld::GetCannons). One documented faithfulness
// gap: the release's RPG-side mark in Undock::Do (IUnitMission vtbl+0x7c) is elided -- the ExitCannon
// record clears the docked cannon so the action cannot re-fire, making the mark observationally inert.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// IsWorkingCannon @0x51f20: not occupied, not broken, and its weapon item has ammo.
static bool HG_IsWorkingCannon( NWorld::CCannon *pC )
{
	return !pC->IsOccupied() && !pC->IsBroken() && IsValid( pC->GetItem() ) && pC->GetItem()->HasAmmo();
}
// CannonCanHit (GetDamageableUnits core @0x525b0): the enemy's AI-map aim point fed to CanAttackWithCannon.
// UCR_OK -> 1 (damageable), UCR_NEED_RELOAD -> 2 (abort the whole scan), else 0.
static int HG_CannonCanHit( NWorld::CCannon *pC, IAIUnit *pEnemy, NWorld::CWorld *pWorld )
{
	if ( !IsValid( pEnemy ) || !IsValid( pEnemy->GetUnitServer() ) )
		return 0;
	CVec3 ptAim;
	pWorld->GetAIMap()->GetUnitHLPos( &ptAim, pWorld->GetAIMap()->GetHull(pEnemy->GetUnitServer()), HL_BODY );
	NWorld::EUnitCommandResult e = NWorld::CanAttackWithCannon( pC, ptAim );
	if ( e == NWorld::UCR_OK )
		return 1;
	if ( e == NWorld::UCR_NEED_RELOAD )
		return 2;
	return 0;
}
// GetDamageableUnits @0x525b0: of the unit's known enemies, those the cannon can engage; an
// UCR_NEED_RELOAD answer aborts the scan (the gun must reload first).
static void HG_GetDamageableUnits( IAIUnit *pU, NWorld::CCannon *pC, NWorld::CWorld *pWorld,
	vector<IAIUnit*> *pRes )
{
	pRes->clear();
	SAIUnitState *pState = pU->GetAIUnitState();
	if ( pState == 0 )
		return;
	const vector< CPtr<IAIUnit> > &known = pState->GetKnownEnemies();
	for ( int i = 0; i < (int)known.size(); ++i )
	{
		int n = HG_CannonCanHit( pC, known[i].GetPtr(), pWorld );
		if ( n == 2 )
			break;
		if ( n == 1 )
			pRes->push_back( known[i].GetPtr() );
	}
}
// shared: is the state's most-dangerous enemy among the damageable units?
static bool HG_EnemyDamageable( IAIUnit *pU, NWorld::CCannon *pC, NWorld::CWorld *pWorld )
{
	vector<IAIUnit*> dmg;
	HG_GetDamageableUnits( pU, pC, pWorld, &dmg );
	SAIUnitState *pState = pU->GetAIUnitState();
	IAIUnit *pEnemy = pState ? pState->pEnemy.GetPtr() : 0;
	for ( int i = 0; i < (int)dmg.size(); ++i )
		if ( dmg[i] == pEnemy )
			return true;
	return false;
}
// the CanUseCannon @0x51fa0 tail: FindPath to the cannon's approaches (wish pose forced to RUN); the
// path AP + the use-cannon action cost must fit the AP available at `place`.
static bool HG_CanReachCannon( IAIUnit *pU, const SPlaceWithAP &place, NWorld::CCannon *pC )
{
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	NWorld::CWorld *pWorld = pUS->GetWorld();
	if ( !IsValid( pWorld ) )
		return false;
	IPathNetwork *pNet = pWorld->GetPathNetwork();
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return false;
	vector<SPathPlace> approaches;
	pC->GetApproaches( &approaches, pNet );
	if ( approaches.empty() )
		return false;
	NAI::EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( NAI::RUN );
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, pU->GetPosition().p, approaches, pUS );
	pUS->SetWishPose( nOldWish );
	if ( !IsValid( pPath ) || pPath->points.empty() )
		return false;
	// walk the path summing per-move action AP, then add the use-cannon action cost.
	SUnitPosition cur = pU->GetUnitPosition();
	int nPathAP = 0;
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
	{
		SUnitPosition next( cur );
		next.pos.p = *i;
		NRPG::EAction act = NWorld::GetMoveActionType( pNet, cur, next, false );
		nPathAP += pRPG->GetActionAP( (NAI::EPose)cur.GetPose(), act );
		cur = next;
	}
	int nUseAP = pRPG->GetActionAP( (NAI::EPose)pU->GetUnitPosition().GetPose(), NRPG::AC_APPROACH_CANNON );
	return nPathAP + nUseAP <= place.nUnitAP;
}
// CanUseCannon @0x51fa0: live, unoccupied, working cannon within 10m of `place`; no live enemy within
// 5m of the gun, no suspect within 3m; and the unit can reach it with enough AP. `pfDist` returns the
// place->gun distance for the dock ranking.
static bool HG_CanUseCannon( IAIUnit *pU, const SPlaceWithAP &place, NWorld::CCannon *pC, float *pfDist )
{
	if ( !IsValid( pU ) || !IsValid( pC ) || !HG_IsWorkingCannon( pC ) )
		return false;
	CVec3 gun = pC->GetPosition();
	float fDist = fabs( place.place.GetCP() - gun );
	*pfDist = fDist;
	if ( fDist > 10.0f )
		return false;
	SAIUnitState *pState = pU->GetAIUnitState();
	if ( pState == 0 )
		return false;
	float fEnemy = 65535.0f, fPossible = 65535.0f;
	IAIUnit *pEnemy = pState->pEnemy.GetPtr();
	if ( IsValid( pEnemy ) )
		fEnemy = fabs( pEnemy->GetPosition().GetCP() - gun );
	IAIUnit *pPossible = pState->pPossibleEnemy.GetPtr();
	if ( IsValid( pPossible ) )
		fPossible = fabs( pPossible->GetPosition().GetCP() - gun );
	if ( !( fEnemy > 5.0f ) || !( fPossible > 3.0f ) )
		return false;
	return HG_CanReachCannon( pU, place, pC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIDockWithHGAction::GetInfoInner @0x52760: not already docked; scan every cannon -- an unoccupied,
// usable gun whose damageable set contains the state's enemy competes by distance, nearest wins.
void CAIDockWithHGAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) )
		return;
	if ( IsValid( pU->GetCannon() ) )
		return;   // already manning one
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	NWorld::CWorld *pWorld = pUS->GetWorld();
	if ( !IsValid( pWorld ) )
		return;
	vector<NWorld::CCannon*> cannons;
	pWorld->GetCannons( &cannons );
	float fBest = 65535.0f;
	for ( int i = 0; i < (int)cannons.size(); ++i )
	{
		NWorld::CCannon *pC = cannons[i];
		if ( !IsValid( pC ) )
			continue;
		float fDist = 0.0f;
		if ( !HG_CanUseCannon( pU, place, pC, &fDist ) )
			continue;
		if ( !HG_EnemyDamageable( pU, pC, pWorld ) )
			continue;
		if ( fDist < fBest )
		{
			fBest = fDist;
			pInfo->pGun = pC;
			pInfo->bCanDo = true;
		}
	}
}
void CAIDockWithHGAction::Do( CAILog *pLog ) const                       // @0x52dd0
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( info.bCanDo && IsValid( info.pGun ) )
		*pLog << new CAILogUseCannon( GetUnit(), info.pGun.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUndockFromHGAction::GetInfoInner @0x529e0: manning a gun that is no longer worth it (broken, out of
// position, or unable to hit the enemy) -> climb off.
void CAIUndockFromHGAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CCannon *pC = pU->GetCannon();
	if ( !IsValid( pC ) )
		return;   // not manning one
	pInfo->pGun = pC;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	NWorld::CWorld *pWorld = IsValid( pUS ) ? pUS->GetWorld() : 0;
	if ( HG_IsWorkingCannon( pC ) && IsValid( pWorld ) )
	{
		float fDist = 0.0f;
		if ( HG_CanUseCannon( pU, place, pC, &fDist ) && HG_EnemyDamageable( pU, pC, pWorld ) )
			return;   // still a good gun -- stay on it
	}
	pInfo->bCanDo = true;
}
void CAIUndockFromHGAction::Do( CAILog *pLog ) const                     // @0x52340
{
	if ( !IsValid( pLog ) )
		return;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CCannon *pC = pU->GetCannon();   // reads the DOCKED cannon directly (not the cached info)
	if ( !IsValid( pC ) )
		return;
	*pLog << new CAILogExitCannon( pU, pC );
	// (release also marks the RPG side here via IUnitMission vtbl+0x7c(1); elided -- the ExitCannon record
	// clears the docked cannon, so GetInfoInner cannot re-fire, making the mark observationally inert.)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIShootFromHGAction::GetInfoInner @0x52c00: manning a live, working gun usable from here that can hit
// the state's enemy -> fire.
void CAIShootFromHGAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CCannon *pC = pU->GetCannon();
	if ( !IsValid( pC ) )
		return;
	pInfo->pGun = pC;
	if ( !HG_IsWorkingCannon( pC ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	NWorld::CWorld *pWorld = IsValid( pUS ) ? pUS->GetWorld() : 0;
	if ( !IsValid( pWorld ) )
		return;
	float fDist = 0.0f;
	if ( !HG_CanUseCannon( pU, place, pC, &fDist ) )
		return;
	if ( !HG_EnemyDamageable( pU, pC, pWorld ) )
		return;
	SAIUnitState *pState = pU->GetAIUnitState();
	if ( pState == 0 )
		return;
	pInfo->pTarget = pState->pEnemy.GetPtr();
	pInfo->bCanDo = true;
}
void CAIShootFromHGAction::Do( CAILog *pLog ) const                      // @0x52470
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo )
		return;
	IAIUnit *pU = GetUnit();
	NWorld::CCannon *pC = pU->GetCannon();
	if ( !IsValid( pC ) )
		return;
	if ( IsValid( pC->GetItem() ) )
		pC->GetItem()->SetShootMode( (NDb::EShootMode)4 );   // the cannon's firing mode before the shot
	*pLog << new CAILogShot( pU, info.pTarget.GetPtr(), HL_ANY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
