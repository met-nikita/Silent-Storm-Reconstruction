#include "StdAfx.h"
//
#include "aiUnit.h"            // NAI::IAIUnit
#include "aiUnitState.h"       // NAI::SAIUnitState (pEnemy / pPossibleEnemy)
#include "aiActionBase.h"      // NAI::SPlaceWithAP (complete) -- before aiMoveAction.h (C2036 guard)
#include "aiMoveAction.h"      // NAI::GetUnitPos
#include "aiMisc.h"            // NAI::GetAPForMove
#include "aiRouteMisc.h"       // NAI::GetNearestPlaces
#include "aiRouteLogic.h"      // NAI::CreateAIStrafeToPositionLogic
#include "aiCombatLogic.h"     // NAI::CreateAIDefenceLogic
#include "aiInventory.h"       // NAI::CAIInventory::HasAnyWeapon
#include "wUnitServer.h"       // NWorld::CUnitServer (GetWorld/GetWearingDBPK/CanFight/wish-pose)
#include "wMain.h"             // NWorld::CWorld::GetPathNetwork / GetGame
#include "wMainPath.h"         // NWorld::FindPath
#include "aiPath.h"            // NAI::CPath, NAI::PF_DEFAULT
#include "RPGGame.h"           // NRPG::IGame::CheckPositionVisibility
//
#include "aiDefenceReaction.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiDefenceReaction -- the take-cover reaction. Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_aidefencereaction.h). See aiDefenceReaction.h for the architecture + the transient-
// reaction note. Every reach resolves to a real in-tree call; the documented elisions are noted inline.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
namespace {
// IsSamePlace(a,b,0x1feffff) inlined (the dev tree has no shared masked form). Same tile ignoring the
// direction/pose/moving bits -- the release's mask, used throughout the route/place layer.
inline bool SamePlace( const SPathPlace &a, const SPathPlace &b )
{
	return ( ( a.GetData() ^ b.GetData() ) & 0x1feffff ) == 0;
}
// A real (non-sentinel) place: pose bits 30-31 not both set (catches the 0xfdffffff / 0xffffffff sentinels).
// The release's IsValidPoint(net,place) is a deeper net-validity probe; the cover/attack places come from the
// path network so the sentinel check is the meaningful guard here (it rejects the unplanned-pair case).
inline bool IsValidPlace( const SPathPlace &p )
{
	return ( (unsigned)p.GetData() & 0xc0000000u ) != 0xc0000000u;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsUnitSeePosFromPos @0x3a270 -- can `pUnit` (placed at `from`, turned toward `target`) see `target`?
// The release builds the sight cone explicitly (the unit's RPG sight range + GetSightFOV, vision CanSee);
// the dev-native equivalent is NRPG::IGame::CheckPositionVisibility(observerPos, targetPos), which applies
// sight range / FOV internally -- so the explicit range/FOV are elided (the same simplification snipe uses).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsUnitSeePosFromPos( IAIUnit *pUnit, const SUnitPosition &target, const SUnitPosition &from )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) || !IsValid( pUS->GetWorld() ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	NRPG::IGame *pGame = pUS->GetWorld()->GetGame();
	if ( pNet == 0 || pGame == 0 )
		return false;
	SUnitPosition seer = from;
	seer.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( seer.pos.p, target.pos.p ) & 7 ) );
	return pGame->CheckPositionVisibility( seer, target.pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsLocked @0x3a480 -- a dead server counts as locked; else locked iff some OTHER live object holds the
// place (the server's own lock does not count).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsLocked( NWorld::CUnitServer *pUS, const SPathPlace &place )
{
	if ( !IsValid( pUS ) )
		return true;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	CObjectBase *pOwner = pNet->GetWhoLocksThisPlace( place );
	return IsValid( pOwner ) && pOwner != (CObjectBase*)pUS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CanUseDefenceReactionInner @0x3a4f0 -- the full plausibility probe for a cover/attack pair.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanUseDefenceReactionInner( IAIUnit *pUnit, const SUnitPosition &cover, const SUnitPosition &attack )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	if ( IsValid( pUS->GetWearingDBPK() ) )   // IsUnitBusy == IsInPK: busy in a panzerklein
		return false;
	if ( !pUS->CanFight() )
		return false;
	// [pfnPosCompBusy elided: a unit-component vtbl-0xc predicate (the decode labelled it "busy"; the same
	//  slot reads as the hidden flag elsewhere) -- semantic uncertain, omitted as a permissive plausibility gate]
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	if ( !IsValidPlace( cover.pos.p ) || !IsValidPlace( attack.pos.p ) )
		return false;
	// the unit must carry a firearm. The release wants a HOLSTERED LONG ARM (model hold type 1 or 3); the
	// GetFirstFireArms + hold-type accessor is absent in the dev tree, so approximated by "has any weapon".
	if ( !IsValid( pUnit->GetAIInventory() ) || !pUnit->GetAIInventory()->HasAnyWeapon() )
		return false;
	// the pop-out move must cost something (the cover/attack pair must differ)
	if ( GetAPForMove( pUnit, cover.pos.p, attack.pos.p, CROUCH ) <= 0 )
		return false;
	if ( IsLocked( pUS, cover.pos.p ) || IsLocked( pUS, attack.pos.p ) )
		return false;
	SAIUnitState *pState = pUnit->GetAIUnitState();
	if ( pState == 0 )
		return false;
	SUnitPosition own = pUnit->GetUnitPosition();
	IAIUnit *pEnemy = pState->pEnemy.GetPtr();
	bool bEnemy = IsValid( pEnemy );
	// standing on the attack spot already is only meaningful vs a live enemy
	if ( SamePlace( own.pos.p, attack.pos.p ) && !bEnemy )
		return false;
	if ( bEnemy )
	{
		SUnitPosition enemyPos = pEnemy->GetUnitPosition();
		// full cover: the enemy can't see the cover AND the cover can't see the enemy
		if ( IsUnitSeePosFromPos( pEnemy, cover, enemyPos ) )
			return false;
		if ( IsUnitSeePosFromPos( pUnit, enemyPos, cover ) )
			return false;
	}
	IAIUnit *pPossible = pState->pPossibleEnemy.GetPtr();
	if ( IsValid( pPossible ) )
	{
		// a merely suspected enemy we can already see voids the plan
		SUnitPosition possiblePos = pPossible->GetUnitPosition();
		if ( IsUnitSeePosFromPos( pUnit, possiblePos, own ) )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetCoveredPosition @0x3a830 -- wave 18 AP at CROUCH from `from`, keep the CROUCH places the enemy
// cannot see (each turned toward the enemy) as path targets, FindPath there with the wish pose forced to
// CROUCH, output the reached place (*pCover = path.back()), then walk the path BACKWARD for the last CROUCH
// waypoint from which the unit CAN see the enemy (*pAttack, turned toward it).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetCoveredPosition( IAIUnit *pUnit, const SUnitPosition &from, IAIUnit *pEnemy,
	SUnitPosition *pCover, SUnitPosition *pAttack )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) || IsValid( pUS->GetWearingDBPK() ) )   // alive + not busy in a PK
		return false;
	if ( !IsValid( pEnemy ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	SUnitPosition enemyPos = pEnemy->GetUnitPosition();
	SUnitPosition ownPos = pUnit->GetUnitPosition();
	vector<SPathPlace> places;
	GetNearestPlaces( pUS, from.pos.p, 18, CROUCH, &places );
	vector<SPathPlace> targets;
	for ( int i = 0; i < (int)places.size(); ++i )
	{
		if ( SamePlace( places[i], pUnit->GetUnitPosition().pos.p ) )
			continue;
		SUnitPosition cand = GetUnitPos( places[i], pNet );
		if ( (int)cand.pos.p.GetPose() != CM_CROUCH )   // CROUCH places only
			continue;
		if ( IsUnitSeePosFromPos( pEnemy, cand, enemyPos ) )   // the enemy must NOT see it
			continue;
		cand.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( cand.pos.p, enemyPos.pos.p ) & 7 ) );
		targets.push_back( cand.pos.p );
	}
	if ( targets.empty() )
		return false;
	NWorld::CUnit *pComp = static_cast<NWorld::CUnit*>( pUS );
	EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( CROUCH );
	CPtr<NAI::CPath> path = NWorld::FindPath( pNet, pComp, ownPos.pos.p, targets, pComp,
		false, NAI::PF_DEFAULT, false, false, false );
	pUS->SetWishPose( nOldWish );
	if ( nOldWish == CRAWL )
		pUS->SetStrafe( false );
	if ( !IsValid( path ) )
		return false;
	const vector<SPathPlace> &pts = path->points;
	// ORIGINAL BUG (confirmed @0x3aba2): unlike GetRestoreAPPoint, GetCoveredPosition has NO empty-points
	// guard on the returned path -- pts.back() is read unconditionally. Reproduced faithfully (a valid path
	// from FindPath is non-empty in practice, so it never triggers).
	*pCover = GetUnitPos( pts[pts.size() - 1], pNet );
	for ( int i = (int)pts.size() - 1; i >= 0; --i )
	{
		if ( SamePlace( pts[i], pCover->pos.p ) )
			continue;
		if ( (int)pts[i].GetPose() != CM_CROUCH )
			continue;
		SUnitPosition cand = GetUnitPos( pts[i], pNet );
		if ( !IsUnitSeePosFromPos( pUnit, enemyPos, cand ) )   // the unit CAN see the enemy from here
			continue;
		cand.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( cand.pos.p, enemyPos.pos.p ) & 7 ) );
		*pAttack = cand;
		return true;
	}
	return false;
}
}   // anonymous namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3b4d0: positions default to the 0xfdffffff sentinel, both AP fields to 0xffff; with a live known enemy
// the ctor immediately plans (GetCoveredPosition into the members) and prices the pop-out (GetAPForMove).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIDefenceReaction::CAIDefenceReaction( IAIUnit *pUnit, CAIReaction *pPrevReaction )
	: CAIReaction( pUnit ), nAPForTakeCover( 0xffff ), nPrevAP( 0xffff ), bJustStarted( true ),
	  pPrevReaction( pPrevReaction )
{
	coveredPos.pos.p = SPathPlace( (int)0xfdffffff );
	attackPos.pos.p = SPathPlace( (int)0xfdffffff );
	SAIUnitState *pState = GetAIUnitState();
	IAIUnit *pEnemy = ( pState != 0 ) ? pState->pEnemy.GetPtr() : 0;
	if ( IsValid( pEnemy ) )
	{
		if ( GetCoveredPosition( pUnit, pUnit->GetUnitPosition(), pEnemy, &coveredPos, &attackPos ) )
			nAPForTakeCover = GetAPForMove( pUnit, coveredPos.pos.p, attackPos.pos.p, CROUCH );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// the pop-out's poor-AP arm: strafe to cover, then clamp the AP pool to what remains after getting there
// (never below 0). The AP-pool restore is the release's IUnitMission AP setter -> dev IAIUnit::SetAP.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIDefenceReaction::StrafeToCover( IAIUnit *pU, int nAP )
{
	int nCost = GetAPForMove( pU, pU->GetUnitPosition().pos.p, coveredPos.pos.p, CROUCH );
	SetLogic( CreateAIStrafeToPositionLogic( pU, coveredPos.pos, CROUCH, CROUCH ) );
	int nLeft = nAP - nCost;
	pU->SetAP( nLeft < 0 ? 0 : nLeft, pU->GetMaxAP() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3b160. The plan no longer works and there is a previous reaction -> reinstall it. Otherwise run the
// cover dance on the AP budget:
//   at the cover  -> (low AP: keep the pool) strafe to the attack spot;
//   at the attack -> with fresh AP above the pop-out price install the defence logic, else duck back;
//   en route rich -> strafe to the attack spot (> 2x the price);
//   en route poor -> strafe to cover, clamping the pool to what remains.
//
// ELIDED: the release brackets the dance with DisableShootMode/EnableShootMode (kinds 2,4) -- those IAIUnit
// vtbl 0x7c/0x80 methods are absent from the dev tree, so the shoot-mode toggling is omitted (the unit may
// auto-fire during the cover approach -- a behaviour refinement, not part of the core decision).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIDefenceReaction::Update()
{
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	bool bOK = CanUseDefenceReactionInner( pU, coveredPos, attackPos );
	CAIReaction *pPrev = pPrevReaction.GetPtr();
	if ( !bOK && IsValid( pPrev ) )
	{
		pU->SetReaction( pPrev );   // hand the unit back to the fall-back reaction
		return;
	}
	int nAP = pU->GetAP();
	SUnitPosition own = pU->GetUnitPosition();
	bool bAtCover = GetAPForMove( pU, own.pos.p, coveredPos.pos.p, own.GetPose() ) == 0;
	own = pU->GetUnitPosition();
	bool bAtAttack = GetAPForMove( pU, own.pos.p, attackPos.pos.p, own.GetPose() ) == 0;
	if ( bAtCover )
	{
		if ( nAP < 2 * nAPForTakeCover )
			pU->SetAP( nAP, pU->GetMaxAP() );
		SetLogic( CreateAIStrafeToPositionLogic( pU, attackPos.pos, CROUCH, CROUCH ) );
	}
	else if ( bAtAttack )
	{
		if ( nAP > nAPForTakeCover && nAP != nPrevAP )
		{
			nPrevAP = nAP;
			SetLogic( CreateAIDefenceLogic( pU, attackPos.pos.p, nAPForTakeCover ) );
		}
		else
			StrafeToCover( pU, nAP );
	}
	else if ( nAP > 2 * nAPForTakeCover )
		SetLogic( CreateAIStrafeToPositionLogic( pU, attackPos.pos, CROUCH, CROUCH ) );
	else
		StrafeToCover( pU, nAP );
	bJustStarted = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3af10
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIDefenceReaction( IAIUnit *pUnit, CAIReaction *pPrevReaction )
{
	if ( !IsValid( pUnit ) || !IsValid( pPrevReaction ) )
		return 0;
	return new CAIDefenceReaction( pUnit, pPrevReaction );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3add0
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanUseDefenceReaction( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	SAIUnitState *pState = pUnit->GetAIUnitState();
	if ( pState == 0 )
		return false;
	IAIUnit *pEnemy = pState->pEnemy.GetPtr();
	if ( !IsValid( pEnemy ) )
		return false;
	SUnitPosition cover, attack;
	cover.pos.p = SPathPlace( (int)0xfdffffff );
	attack.pos.p = SPathPlace( (int)0xfdffffff );
	if ( !GetCoveredPosition( pUnit, pUnit->GetUnitPosition(), pEnemy, &cover, &attack ) )
		return false;
	return CanUseDefenceReactionInner( pUnit, cover, attack );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
BASIC_REGISTER_CLASS( CAIDefenceReaction )
