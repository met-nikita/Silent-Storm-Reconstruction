#ifndef __AIROUTEMISC_H_
#define __AIROUTEMISC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiPosition.h"      // NAI::SPathPlace, EPose, IPathNetwork
//
namespace NWorld { class CUnitServer; }
//
namespace NAI
{
class CMultiMovesTable;
class CTaskCommand;
class IAIUnit;
class CUnitArea;
class CPath;
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetNearestPlaces @0xa05d0 -- "every CROUCH/STAND place a unit can reach within an AP budget",
// the feeder of the choose-place place sources (CAICurrentPlaceSource::Prepare & friends) and the snipe
// kill-zone scan. Reconstructed from the matched-release decode (oracle: decomp/src/s2_routemisc.h),
// wired onto the in-tree path-wave machinery (NWorld::PrepareAllPaths over a CMultiMovesTable).
//
// The unit's wish pose is swapped to `nPose` around the whole flood (so the wave prices moves at the
// requested pose) and restored after; writing CRAWL also clears the run byte (Set/IsStrafing), faithful
// to the release. Reached places at the LAY(0) / INACTIVE(3) check-poses are dropped; the rest are kept
// unless impassable.
//
// TWO release-vs-dev faithfulness REFINEMENTS (the release evolved these path primitives; the dev tree
// has the predecessor form, so this wiring is functional + behaviour-CLOSE, not bit-exact):
//   (1) the release passed bNoDynamicLocks=true (ignore ALL transient unit-locks); the in-tree free
//       PrepareAllPaths accounts for the find-path units (minus self) -- so this version can EXCLUDE
//       places another unit transiently blocks. Bit-faithful needs the CMultiMovesTable member
//       PrepareAllPaths called with an EMPTY accountUnits list (+ the move-cost table).
//   (2) the release filtered with GetPassability (EPassable: drops AIP_NOT_PASSABLE + AIP_CANNOT_LAY);
//       the dev tree has the predecessor bool IsPassable, which lacks the lay-direction (CANNOT_LAY)
//       distinction. Bit-faithful needs the release per-place GetPassability(EPassable).
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetNearestPlaces( NWorld::CUnitServer *pServer, const SPathPlace &place, int nMaxCost, EPose nPose,
	vector<SPathPlace> *pRes, CMultiMovesTable *pTable = 0 );
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetCheckPosition @0xa1000 (oracle: decomp/src/s2_routemisc.h) -- the best spot to inspect `pos`
// from. Floods the unit's reachable places (current AP, RUN pose -- NO 28-AP floor, unlike GetSafePosition)
// keeping its own moves table, then over the reached places INSIDE `pArea` from which the unit can SEE `pos`
// (candidate turned toward it) keeps the one with the MAXIMUM wave cost (inspect from as far as the AP allows;
// ties keep the earlier place). The result carries the folded facing direction. False when the area is
// null/dead, the unit/server chain is dead, or no candidate qualifies. Consumed by CreateAICheckPositionLogic.
//
// Documented dev<->release elisions (build-validation scope, identical to aiDefenceReaction/aiSnipeAction):
//   * sight range/FOV are applied INTERNALLY by NRPG::IGame::CheckPositionVisibility (2-arg) -- the release's
//     explicit per-unit range/FOV args are elided;
//   * the AP budget is pUnit->GetAP() (current AP); the release's turn-vs-real-time min(cur,max) collapses to
//     current AP in turn-based combat (the only path this runs);
//   * GetNearestPlaces carries its own bNoDynamicLocks / EPassable predecessor refinements (see above) --
//     behaviour-close, not bit-exact.
bool GetCheckPosition( IAIUnit *pUnit, CUnitArea *pArea, const SPosition &pos, SPathPlace *pResult );
////////////////////////////////////////////////////////////////////////////////////////////////////
// The RouteAdd* command-list builders (oracle: decomp/src/s2_routemisc.h slice 2) -- tiny assemblers
// that append CTaskCommand steps to `cmds`. Reconstructed onto the reused CTaskCommand family. Consumed by
// the CreateAI*Logic factories (aiRouteLogic.cpp) and (future) RouteAddRoundUp.
//
// NAI::RouteAddLookAround @0xa2180 -- optionally a WALK pose change, then n glances (n = clamp(nCount-2,0,..)
// + random%span, at least 1), each a look in a random spoke followed by a 1..5s wait. ORIGINAL QUIRK: the
// spoke is random%7 so direction 7 (the NW spoke) can never be drawn.
void RouteAddLookAround( bool bAddPose, vector< CPtr<CTaskCommand> > &cmds, int nCount );
// NAI::RouteAddRoaming @0xa2420 -- one roam step, then a short look-around (nCount=2). The pose-change flag
// passes straight through (the route factory calls this with false, so roaming does not re-pose).
void RouteAddRoaming( const SPathPlace &center, int nRadius, bool bAddPose, vector< CPtr<CTaskCommand> > &cmds );
// NAI::RouteAddGoAndCheck @0xa24e0 -- face `pos`, RUN/WALK (bRun), goto it, optional look-around (nCount=6), WALK.
void RouteAddGoAndCheck( const SPosition &pos, bool bLookAround, bool bRun, vector< CPtr<CTaskCommand> > &cmds );
// NAI::RouteAddWalkToPosition @0xa2f20 -- face `pos`, WALK, goto it.
void RouteAddWalkToPosition( const SPosition &pos, vector< CPtr<CTaskCommand> > &cmds );
////////////////////////////////////////////////////////////////////////////////////////////////////
// The aiRouteMisc GEOMETRY BEDROCK (oracle: decomp/src/s2_routemisc.h, all disasm-verified) -- the
// release-new spatial query leaves the route AI reasons over: cone/crowd/visibility queries built on the
// landed GetNearestPlaces flood + in-tree path/vision seams. All net-new (absent from the dev tree); each
// reaches only calls the landed aiRouteMisc/aiMisc/aiDefenceReaction/aiSnipeAction legs already use --
// no pfn hooks. Dead code until a consumer (e.g. a future CAIFearReaction / RouteAddRoundUp) calls them.
//
// SHARED dev<->release elision (build-validation scope, identical to GetCheckPosition/aiSnipeAction/
// aiDefenceReaction): the release's visibility probe is a 4-arg CheckPositionVisibility carrying explicit
// per-unit sight RANGE (NRPG::CGame::GetUnitSightDistance) + FOV (NRPG::CUnit::GetSightFOV); the dev-native
// NRPG::IGame::CheckPositionVisibility(observerPos, targetPos) is 2-arg and internalizes range/FOV from the
// observer -- so the explicit range/FOV args are ELIDED everywhere below.
//
// NAI::IsNear @0x9fe30 -- the position's center point within fRadius of ANY of the points (squared compare).
bool IsNear( const SPosition &pos, const vector<CVec3> &points, float fRadius );
// NAI::IsNear @0xa0080 -- any point of the path near any of the points.
bool IsNear( IPathNetwork *pNet, const CPath &path, const vector<CVec3> &points, float fRadius );
// NAI::GetDirectedPos @0xa0130 -- the unit position at `place`, facing `target` (GetClosestDir folded &7
// into the place's direction bits). The SPosition analog seed for the visibility seers below.
SUnitPosition GetDirectedPos( const SPathPlace &place, const SPathPlace &target, IPathNetwork *pNet );
// NAI::GetAvgPos @0x9fed0 -- the normalized, distance-weighted average direction from `cp` toward every
// READY unit (live unit+server, CanFight) within 15m: sum normalize(cu - cp)*16.666666/(dist+1.6666666),
// then normalized (left zero when nothing contributes). Constants are disasm-verified literals.
CVec3 GetAvgPos( const vector< CPtr<IAIUnit> > &units, const CVec3 &cp );
// NAI::GetPlacesAtDirection @0xa0800 -- of all places the unit can reach within nMaxCost at `nPose`
// (GetNearestPlaces), keep those inside a forward cone along vDir (forward distance band [fMinDist,fMaxDist],
// perpendicular offset below nSpread%*fDot). When the cone catches nothing, the single farthest in-band
// place (max forward dot, FIRST on ties) is returned alone -- provided any candidate entered the band.
void GetPlacesAtDirection( NWorld::CUnitServer *pServer, const SPathPlace &place, CVec3 vDir, int nSpread,
	float fMinDist, float fMaxDist, int nMaxCost, EPose nPose, vector<SPathPlace> *pRes );
// NAI::GetFearPosition @0xa0b20 -- run AWAY from the crowd: vDir = -GetAvgPos(units, ownCP) fed to
// GetPlacesAtDirection from the unit's own place (spread 40, band [fMinDist, 4095.0], RUN pose); ONE cone
// place is picked uniformly with a raw ISAAC draw (random.Get(size)). False when the cone (and its
// farthest-place fallback) came up empty.
bool GetFearPosition( NWorld::CUnitServer *pServer, const vector< CPtr<IAIUnit> > &units, SPathPlace *pResult,
	float fMinDist, int nMaxCost );
// NAI::GetSafePosition @0xa0ca0 -- the reachable places at RUN within the unit's AP budget (clamped UP to
// at least 28), MINUS every place any live unit of `units` can SEE (each seer turned toward the candidate
// first). Returns the FIRST surviving place in wave order (no random pick); false when nothing survives.
bool GetSafePosition( NWorld::CUnitServer *pServer, const vector< CPtr<IAIUnit> > &units, SPathPlace *pResult );
// NAI::GetRestoreAPPoint @0xa01e0 -- where to catch a breath: FindPath from `from` to `target`, then walk
// the path BACKWARD and return the first point pEnemy can NOT see (the enemy virtually turned toward the
// point, GetDirectedPos). *pnVisible counts the seen probes (consecutive masked-duplicate tiles probed
// once, IsSamePlace mask 0x1feffff); the walk gives up false once it reaches 20. A null/dead/empty path is
// false. (No original bug here -- the release has the empty-points guard that GetCoveredPosition lacks.)
bool GetRestoreAPPoint( NWorld::CUnitServer *pServer, NWorld::CUnitServer *pEnemy, const SPosition &from,
	const SPosition &target, SPosition *pResult, int *pnVisible );
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetRoundUpPlaces @0xa1320 (oracle: decomp/src/s2_routemisc.h, all constants disasm-verified
// from Game.exe raw bytes) -- plan a FLANKING approach onto `enemyPos`. Outputs three places:
//   *pAttack = a random place reachable from the ENEMY's tile (wave budget min(dist*1.6,20)) whose check
//              pose matches and from which the enemy is visible (candidate turned toward him);
//   *pFlank  = a random reachable place off the own->attack path's 1/3 waypoint (budget min(dist*1.6,25))
//              on the attack side of the own->enemy axis (perp . offset > 0);
//   *pStop   = the first CM_INACTIVE waypoint of the own->flank path (a ladder/door transition) else *pFlank.
// Short range (dist < 8) defaults flank=stop=own / attack=enemy. With bCheckFriends it gathers the centre
// of every DOWNED ally (diplomacy DS_ALLY, NOT still combat-capable) near the own/enemy midpoint and fails
// the plan when the flank or flank->attack path passes within 4m of one.
//
// KEY ANSWER-KEY CORRECTION (resolved from the raw CUnitServer vtable, session-18/19 discipline): the decode
// hook `pfnGetAttackObject` (// vtbl+0x34) is a SEMANTIC MISLABEL. Primary CUnitServer vtable slot +0x34 is
// CUnitServer::GetWearingDBPK (@0x3c0240) -- the gate is "am I WEARING A PANZERKLEIN", NOT "have an attack
// target". A PK unit moves at WALK (else RUN) and is checked STANDing (else CROUCHing) -- the exact idiom the
// dev tree already uses at aiActionPlaceSource.cpp:318. Reproduced as IsValid(pServer->GetWearingDBPK()).
//
// ORIGINAL BUG (confirmed @0x4a1aa1-0x4a1b0a, fmul @0x4a1ab9): the lateral-projection scalar's u-term is
// multiplied by cpOwn.u (the unit's own WORLD u-coordinate) instead of n.u (the normalized own->enemy u-axis).
// The v/q terms are correct; only the u-term has the buggy substitution. (The decomp answer-key miscopies
// this as an UNSCALED u-term -- the raw bytes show the cpOwn.u multiply; this reconstruction is authoritative.)
//
// ONE documented ELISION (build-validation scope): the bCheckFriends control points use the +0x14c position
// component's CP[0] (vtbl+0x94), which has no dev accessor -- approximated by the unit centre
// (GetPosition().GetCenter()), a sub-metre offset that only affects the 4m friend-avoidance test.
bool GetRoundUpPlaces( NWorld::CUnitServer *pServer, const SUnitPosition &enemyPos, SPosition *pStop,
	SPosition *pFlank, SPosition *pAttack, bool bCheckFriends );
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddRoundUp @0xa2720 (oracle: decomp/src/s2_routemisc.h) -- the round-up command script: face
// the enemy, plan with GetRoundUpPlaces (bCheck ANDed with the difficulty option bAICheckCorpses), and on
// success append [face enemy; (wish-pose + goto stop unless on the own tile); pose + goto flank; if the enemy
// is alive and GetRestoreAPPoint finds a hidden spot: a 40% Wait(1) when <=3 path points were watched, goto
// restore, Wait(1); with NO worn PK a look toward the enemy; goto attack; look at enemy]. In ALL cases (also on
// plan failure) it tails Wait(2 + draw&1), RouteAddLookAround(false,6), ChangePose(WALK). Reuses the existing
// CTaskCommand family (the release strafe prefix on Goto + the ChangeWishPose/ChangePose split are elided -- see
// CreateAIStrafeToPositionLogic). bAvoidFriends = bCheck && GetGlobalGame()->pDifficulty->bAICheckCorpses.
void RouteAddRoundUp( NWorld::CUnitServer *pServer, NWorld::CUnitServer *pEnemy, const SUnitPosition &enemyPos,
	EPose nPose, vector< CPtr<CTaskCommand> > &cmds, bool bCheck );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIROUTEMISC_H_
