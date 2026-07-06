#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiUnitState.h"      // NAI::SAIUnitState: pEnemy / pPossibleEnemy
#include "aiPosition.h"       // NAI::SUnitPosition::GetCP, fabs( CVec3 )
#include "aiControl.h"        // IPlayer / commander access
#include "aiCommander.h"      // NAI::CAICommander::GetAIUnit (server -> AI unit)
#include "AILog.h"            // CAILogShot / CAILogCollectSnipeAP / CAILogCancelAction
#include "aiCombatLog.h"      // NAI::CAILog::operator<< (append + commit a record)
#include "wUnitServer.h"      // NWorld::CUnitServer: IsSniping / GetSnipingState / GetPlayer
#include "wUnitStates.h"      // NWorld::CUnitStateSniping: GetBaseSnipeAP / GetCollectedSnipeAP / GetTarget
#include "aiInventory.h"      // NAI::CAIInventory: GetBestWeaponForSnipe / IsCurrentItem
#include "aiWeapon.h"         // NAI::CAIFireArmsWeapon
#include "aiRouteMisc.h"      // NAI::GetNearestPlaces (flood the target's reachable places)
#include "wMain.h"            // NWorld::CWorld: GetGame / GetPathNetwork
#include "RPGGame.h"          // NRPG::IGame::CheckPositionVisibility
#include "..\DBFormat\DataRPG.h" // NDb::SM_Snipe
//
#include "aiActions.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The snipe state machine's per-turn actions: bank AP into the snipe pool (CollectAP), fire once the pool is
// full (Shot), abort when the enemy gets too close (Cancel). The fourth action, CAIBeginSnipeAction (the entry
// that decides to start sniping), stays deferred in aiActions.cpp -- its GetInfoInner inlines a snipe kill-zone
// scan the decode itself left opaque (see the note there). These three never activate until Begin lands (they
// all gate on CUnitServer::IsSniping), but they are complete + faithful reconstructions of the matched-release
// decode (oracle: decomp/src/s2_aisnipeaction.h: CanSnipe @0x4a4280, GetInfoInner @0x4a4490/@0x4a4650/
// @0x4a4720, Do @0x4a4c00/@0x4a4c90/@0x4a4df0). Every decode hook resolves to a real in-tree call.
//
// One documented dev<->release divergence: the release reads the snipe AP pool + the unit's current AP from
// the RPG skill block; the dev-native CUnitStateSniping caches the pool as nBaseAP (set at snipe start). The
// AP CAP is the unit's CURRENT AP (pU->GetAP()), read PLACE-INDEPENDENTLY exactly as the release does -- the
// release GetInfoInner @0x4a4490 ignores its `place` arg entirely. (Earlier this capped by place.nUnitAP.)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// NAI::CanSnipe @0x4a4280: sniping needs DISTANCE -- a live enemy far enough away (the CP-distance scaled by
// 1.6, the decode's __real_3fcccccd, must be >= 15) and any possible enemy >= 10; no live enemy means no.
static bool CanSnipe( IAIUnit *pU )
{
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) )
		return false;
	SAIUnitState *pState = pU->GetAIUnitState();
	if ( pState == 0 )
		return false;
	IAIUnit *pEnemy = pState->pEnemy.GetPtr();
	if ( !IsValid( pEnemy ) )
		return false;
	float fEnemy = fabs( pEnemy->GetPosition().GetCP() - pU->GetPosition().GetCP() ) * 1.6f;
	float fPossible = 65535.0f;
	IAIUnit *pPossible = pState->pPossibleEnemy.GetPtr();
	if ( IsValid( pPossible ) )
		fPossible = fabs( pPossible->GetPosition().GetCP() - pU->GetPosition().GetCP() ) * 1.6f;
	return !( fEnemy < 15.0f ) && !( fPossible < 10.0f );
}
// the AI-unit wrapper for a unit server (release NAI::GetAIUnit @0x4742c0): look it up via the server's
// player's AI commander (server -> GetPlayer -> GetCommander -> CAICommander::GetAIUnit). 0 if the server's
// side is not AI-commanded (e.g. a human-controlled target), exactly as the decode returns 0 in that case.
static IAIUnit *SnipeGetAIUnit( NWorld::CUnitServer *pServer )
{
	if ( !IsValid( pServer ) || pServer->GetPlayer() == 0 )
		return 0;
	CDynamicCast<CAICommander> pCommander( pServer->GetPlayer()->GetCommander() );
	if ( !IsValid( pCommander ) )
		return 0;
	return pCommander->GetAIUnit( pServer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIBeginSnipeAction::GetInfoInner @0x4a4810: decide to start sniping. Not already sniping; the best snipe
// weapon must exist + be live; CanSnipe; the to-hit against the state's enemy at HL_ANY must be positive (aim
// HL_HEAD when that also connects); and the KILL-ZONE SCAN must confirm the sniper can still SEE every place
// the target could flee to next turn (otherwise the target would just step out of view). The inline scan was
// hand-disassembled (the decode left it a hook): pWorld->GetGame()->CheckPositionVisibility from the sniper
// to each place NAI::GetNearestPlaces floods around the target.
//
// Two documented dev<->release elisions (build-validation scope): (1) the release GetToHit takes the chosen
// snipe weapon; the dev 3-arg GetToHit uses the unit's current weapon. (2) the release passes explicit
// sight-distance (IGame::GetUnitSightDistance) + FOV (CUnit::GetSightFOV) to a 4-arg CheckPositionVisibility;
// the dev CheckPositionVisibility is 2-arg (observer, target) and applies sight range/FOV internally, so the
// range/FOV computation is dropped. The enemy-flood budget (obscured in the decomp -- built from the enemy's
// move-table machinery) is taken as the enemy's full AP (GetMaxAP) at RUN pose -- "everywhere the target
// could run this turn".
void CAIBeginSnipeAction::GetInfoInner( const SPlaceWithAP &, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) || pUS->IsSniping() )
		return;                                          // must not be sniping already
	CAIInventory *pInv = pU->GetAIInventory();
	if ( !IsValid( pInv ) )
		return;
	pInfo->pWeapon = pInv->GetBestWeaponForSnipe();
	if ( !IsValid( pInfo->pWeapon ) )
		return;                                          // no live snipe weapon with ammo
	if ( !CanSnipe( pU ) )
		return;
	SAIUnitState *pState = pU->GetAIUnitState();
	if ( pState == 0 )
		return;
	IAIUnit *pEnemy = pState->pEnemy.GetPtr();
	if ( !IsValid( pEnemy ) || !IsValid( pEnemy->GetUnitServer() ) )
		return;
	// to-hit gate: must connect at HL_ANY; aim the head when that also connects.
	SUnitPosition selfPos = pU->GetUnitPosition();
	pInfo->hitLocation = HL_ANY;
	if ( pU->GetToHit( pEnemy, selfPos, HL_ANY ) <= 0 )
		return;
	if ( pU->GetToHit( pEnemy, selfPos, HL_HEAD ) > 0 )
		pInfo->hitLocation = HL_HEAD;
	// kill-zone scan: the sniper must still SEE every place the target could flee to next turn.
	NWorld::CWorld *pWorld = pUS->GetWorld();
	if ( !IsValid( pWorld ) )
		return;
	NRPG::IGame *pGame = pWorld->GetGame();
	IPathNetwork *pNet = pWorld->GetPathNetwork();
	if ( !IsValid( pGame ) || !IsValid( pNet ) )
		return;
	vector<SPathPlace> places;
	GetNearestPlaces( pEnemy->GetUnitServer(), pEnemy->GetPosition().p, pEnemy->GetMaxAP(), NAI::RUN, &places );
	for ( vector<SPathPlace>::const_iterator i = places.begin(); i != places.end(); ++i )
		if ( !pGame->CheckPositionVisibility( selfPos, SPosition( *i, pNet ) ) )
			return;                                      // a place the target could reach unseen -> cannot snipe
	pInfo->pTarget = pEnemy;
	pInfo->bCanDo = true;
}
void CAIBeginSnipeAction::Do( CAILog *pLog ) const                       // @0x4a4e80
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo )
		return;
	IAIUnit *pU = GetUnit();
	CAIFireArmsWeapon *pWeapon = info.pWeapon.GetPtr();
	CAIInventory *pInv = pU->GetAIInventory();
	if ( IsValid( pInv ) && !pInv->IsCurrentItem( pWeapon ) )    // switch to the snipe weapon if not in hand
		*pLog << new CAILogChangeWeapon( pU, pWeapon );
	*pLog << new CAILogChangeShootMode( pU, pWeapon, NDb::SM_Snipe );   // shoot-mode 5 = snipe
	*pLog << new CAILogBeginSnipe( pU, info.pTarget.GetPtr(), info.hitLocation );
	// (the release also flips the RPG-side "snipe started" mark here -- pU->GetUnitMission() vtbl+0x7c(1);
	//  elided like the cannon Undock mark: it only sets the SIMULATED is-sniping flag for same-think chaining,
	//  observationally inert under build-validation -- the real state flips when the logged snipe-aim executes.)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICollectSnipeAPAction::GetInfoInner @0x4a4490: while sniping, bank AP into the snipe pool -- collect
// min( pool - already-collected, AP available here ), doable while positive.
void CAICollectSnipeAPAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	pInfo->nAP = 0;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) || !pUS->IsSniping() )
		return;
	NWorld::CUnitStateSniping *pSnipe = pUS->GetSnipingState();
	if ( pSnipe == 0 )
		return;
	int nPool = pSnipe->GetBaseSnipeAP();
	int nCollected = pSnipe->GetCollectedSnipeAP();
	if ( nCollected >= nPool )
		return;
	int nAP = nPool - nCollected;
	// @0x4a4490 -- the release caps by the unit's CURRENT AP (its RPG AP skill), read PLACE-INDEPENDENTLY:
	// GetInfoInner never references its `place` arg (raw decode reads GetUnit()->GetRPG()+0x14+0x28). Oracle
	// s2_aisnipeaction.h: Min( nPool - nCollected, GetUnitAPAsInt(pU) ). Was: place.nUnitAP, which the per-place
	// SActionInfo cache memoizes separately and which diverges for non-current candidate places (move-cost reduced).
	int nCurAP = pU->GetAP();        // IAIUnit::GetAP() == the unit's current AP (same getter aiAction.cpp seeds nUnitAP with)
	if ( nCurAP < nAP )
		nAP = nCurAP;
	pInfo->nAP = nAP;
	if ( nAP > 0 )
		pInfo->bCanDo = true;
}
void CAICollectSnipeAPAction::Do( CAILog *pLog ) const                   // @0x4a4c00
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( info.bCanDo )
		*pLog << new CAILogCollectSnipeAP( GetUnit(), info.nAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISnipeShotAction::GetInfoInner @0x4a4650: sniping -> the shot is ready.
void CAISnipeShotAction::GetInfoInner( const SPlaceWithAP &, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	if ( pUS->IsSniping() )
		pInfo->bCanDo = true;
}
void CAISnipeShotAction::Do( CAILog *pLog ) const                        // @0x4a4c90
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo )
		return;
	IAIUnit *pU = GetUnit();
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	NWorld::CUnitStateSniping *pSnipe = IsValid( pUS ) ? pUS->GetSnipingState() : 0;
	// the stored snipe target (a unit server) resolved back to its AI-unit wrapper for the shot record.
	NWorld::CUnitServer *pTargetServer = pSnipe ? pSnipe->GetTarget() : 0;
	IAIUnit *pTarget = SnipeGetAIUnit( pTargetServer );
	if ( IsValid( pTarget ) )
		*pLog << new CAILogShot( pU, pTarget, HL_ANY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICancelSnipeAction::GetInfoInner @0x4a4720: sniping, but the snipe conditions no longer hold (enemies
// came too close) -> cancel.
void CAICancelSnipeAction::GetInfoInner( const SPlaceWithAP &, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	if ( pUS->IsSniping() && !CanSnipe( pU ) )
		pInfo->bCanDo = true;
}
void CAICancelSnipeAction::Do( CAILog *pLog ) const                      // @0x4a4df0
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( info.bCanDo )
		*pLog << new CAILogCancelAction( GetUnit() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
