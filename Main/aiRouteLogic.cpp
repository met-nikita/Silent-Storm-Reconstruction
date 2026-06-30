#include "StdAfx.h"
//
#include "aiUnit.h"            // NAI::IAIUnit (GetUnitServer / GetUnitPosition / GetAP / GetHideProbability)
#include "aiUnitState.h"       // NAI::SAIUnitState::pEnemy (CreateAICheckPositionLogic no-enemy gate)
#include "aiTaskCommander.h"   // NAI::CTaskCommand + CTaskCommandGoto / CTaskCommandChangePose (reused command family)
#include "aiMisc.h"            // NAI::HasPath
#include "wUnitServer.h"       // NWorld::CUnitServer: GetUnitPosition / HasCommand / GetWorld
#include "wUnitCommands.h"     // NWorld::CCmd / CCmdSetCommand (the CCmd->CCommand wrap) / CCmdLook
#include "wMain.h"             // NWorld::CWorld::GetPathNetwork
#include "aiActionBase.h"      // NAI::SPlaceWithAP (complete) -- before aiMoveAction.h (C2036 guard)
#include "aiMoveAction.h"      // NAI::GetPos
#include "aiRouteMisc.h"       // NAI::RouteAddLookAround / RouteAddRoaming
#include "../DBFormat/DataMap.h" // NDb::EDiplomacyState / DS_ENEMY (the Hide diplomacy gate)
#include "aiEvent.h"           // NAI::CreateAIPossibleEnemyEvent / CreateAIHelpCalledEvent (the alarm's per-unit events)
#include "aiState.h"           // NAI::IAIState::GetAllyAIPlayer
#include "aiPlayer.h"          // NAI::IAIPlayer::GetUnits / GetNearestUnit
//
#include "aiRouteLogic.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiRouteLogic -- the route-following AI logic. Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_routelogic.h). The release CAIRouteLogic is the dev CTask walker re-parented as a
// CAILogic; this reconstruction reuses the existing CTaskCommand command family and adds only the logic
// wrapper + the strafe factory. See aiRouteLogic.h for the architecture.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandLookToPosition::Do @0x99840 -- if the unit's current place is valid, turn toward the stored
// target place (the path network's closest spoke from here to there) and queue a CCmdLook. Mirrors the dev
// CTaskCommandChangeDirection::Do (GetPosition -> SetDirection -> queue a CCmd) but resolves the direction
// toward a target place via IPathNetwork::GetClosestDir and emits CCmdLook rather than a PF_USE_DIR path.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandLookToPosition::Do()
{
	if ( !IsValid( pUnitServer ) )
		return;
	SUnitPosition cur = pUnitServer->GetPosition();
	// a real (non-sentinel) current place: pose bits 30-31 not both set (the decode's 0xc0000000 gate)
	if ( ( (unsigned)cur.pos.p.GetData() & 0xc0000000u ) != 0xc0000000u )
	{
		IPathNetwork *pNet = pUnitServer->GetWorld()->GetPathNetwork();
		cur.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( cur.pos.p, pos ) & 7 ) );
		DoCommand( new NWorld::CCmdLook( cur.pos ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTaskCommandLookToPosition::operator&( CStructureSaver &f )
{
	f.Add( 2, (CTaskCommand*)this );
	f.Add( 3, &pos );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandHide::Do @0x99b40 -- queue a hide if: the unit can fight, is not already hidden, scenario
// player 0 (the human player) is the unit's ENEMY, and the engine accepts a probe hide command.
//
// IMPORTANT -- the decomp decode (src/s2_routelogic.h:596-609) MISLABELS the two world reaches as
// pfnFindCover/pfnCoverBlocked. Resolving the IWorld vtable from raw bytes shows vtbl+0x170 == GetPlayerByID
// and vtbl+0xbc == GetDiplomacyState (and the CUnit base's vtbl+0x34 == GetPlayer, GetDiplomacyState's 2nd
// arg) -- there is NO world cover-finder. The "cover" is purely a stance toggle done by the in-engine
// CExecHide handler (wUnitExec.cpp); this command only decides WHEN to hide. The gate value DS_ENEMY(0) was
// confirmed from the GetDiplomacyState disasm (== dev NDb DataMap.h DS_ENEMY=0).
//
// Documented elision (build-validation scope): the release queues CCmdHide(!IsHiding) carrying an explicit
// bState; the dev CCmdHide is the predecessor (no bState) and CExecHide::Run toggles on the live IsHiding
// state. Since this command only proceeds when NOT hidden, the toggle hides -- behaviourally identical here,
// so the bState is not modelled (it would change a live command's serialization -> deferred).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandHide::Do()
{
	if ( !IsValid( pUnitServer ) || !pUnitServer->CanFight() )   // server ready (vtbl+0x44 == CanFight)
		return;
	if ( pUnitServer->IsHiding() )                               // already hidden (vtbl+0xc == IsHiding) -> nothing to do
		return;
	NWorld::CWorld *pWorld = pUnitServer->GetWorld();
	NWorld::CPlayer *pPlayer0 = pWorld->GetPlayerByID( 0 );      // release GetPlayerByID(0,0); dev predecessor is single-arg
	if ( !IsValid( pPlayer0 ) )
		return;
	// hide only if the human player (scenario player 0) is this unit's ENEMY (release gate == DS_ENEMY)
	if ( pWorld->GetDiplomacyState( pPlayer0, pUnitServer->GetPlayer() ) != NDb::DS_ENEMY )
		return;
	// probe a hide command (CanDo -> CExecHide::CanDoIt: no visible enemy may currently see us); if accepted,
	// queue the real hide. CanDo consumes/deletes the zero-ref probe, per its interface contract.
	if ( pUnitServer->CanDo( new NWorld::CCmdHide() ) == NWorld::UCR_OK )
		DoCommand( new NWorld::CCmdHide() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTaskCommandHide::operator&( CStructureSaver &f )
{
	f.Add( 2, (CTaskCommand*)this );   // Hide carries no own fields (decode size 32 == base)
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandAlarm::Do @0x99990 -- raise the garrison: every ALLY within 5 m of the alarming unit learns of the
// enemy (AddPossibleEnemy via CreateAIPossibleEnemyEvent), and the alarming unit raises its own help flag
// (CreateAIHelpCalledEvent). IAIUnit has no Notify slot, so each per-unit event is delivered by event->Modify(state)
// synchronously -- the identical SAIUnitState mutation. With the active Populate-preserve the injected possibleEnemy
// survives the next think, so the alert sticks across the garrison.
void CTaskCommandAlarm::Do()
{
	IAIUnit *pU = IsValid( pUnitServer ) ? GetAIUnit( pUnitServer ) : 0;   // base CTaskCommand::pUnitServer = alarming unit
	if ( !IsValid( pU ) || pU->IsDead() )
		return;
	IAIUnit *pEnemyAI = IsValid( pEnemy ) ? GetAIUnit( pEnemy ) : 0;
	if ( !IsValid( pEnemyAI ) || pEnemyAI->IsDead() )
		return;
	IAIState *pSt = pU->GetAIState();
	if ( pSt == 0 )
		return;
	IAIPlayer *pAllyPlayer = pSt->GetAllyAIPlayer();
	if ( pAllyPlayer == 0 )
		return;
	CVec3 pos = pU->GetPosition().GetCP();
	// release GetUnitsAtRange(pos, 5, allies, exclude=NULL): the ally AI-player roster IS the garrison; self included.
	vector< CPtr<IAIUnit> > &units = *pAllyPlayer->GetUnits();
	for ( int i = 0; i < (int)units.size(); ++i )
	{
		IAIUnit *a = units[i].GetPtr();
		if ( !IsValid( a ) || a->IsDead() || !IsValid( a->GetUnitServer() ) )
			continue;
		if ( fabs( a->GetPosition().GetCP() - pos ) >= 5.0f )   // within 5 m
			continue;
		SAIUnitState *as = a->GetAIUnitState();
		if ( as == 0 )
			continue;
		CObj<IAIEvent> e = CreateAIPossibleEnemyEvent( pEnemyAI );   // == ally AddPossibleEnemy(enemy)
		if ( IsValid( e ) )
			e->Modify( as );
	}
	// the alarming unit itself raises the help flag (release: pU->Notify(CreateAIHelpCalledEvent()))
	SAIUnitState *my = pU->GetAIUnitState();
	CObj<IAIEvent> h = CreateAIHelpCalledEvent();
	if ( my != 0 && IsValid( h ) )
		h->Modify( my );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTaskCommandAlarm::operator&( CStructureSaver &f )
{
	f.Add( 2, (CTaskCommand*)this );
	f.Add( 3, &pEnemy );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIRouteLogic::operator&( CStructureSaver &f )
{
	f.Add( 2, (CAILogic*)this );
	f.Add( 3, &bCircled );
	f.Add( 4, &routeCommands );
	f.Add( 5, &nCurrentCommand );
	f.Add( 6, &bContinueCommand );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIRouteLogic::AddCommand( CTaskCommand *pCmd )
{
	routeCommands.push_back( CObj<CTaskCommand>( pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x98f80: a dead unit/server finishes everything. Out of range -> finished iff the server has no live
// command. In range -> the step's own IsEndOfCommand, but only while not resuming (bContinueCommand) and
// while the server is idle (no live command). The release reads "pCurrentCmd alive"; the dev-native
// CUnitServer::HasCommand() is that live-command probe.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIRouteLogic::IsCurrentCommandFinished()
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( !IsValid( pUS ) )
		return true;
	bool bLiveCmd = pUS->HasCommand();
	if ( !IsCommandInRange() )
		return !bLiveCmd;
	bool bRes = false;
	if ( !bContinueCommand && !bLiveCmd )
		bRes = routeCommands[nCurrentCommand]->IsEndOfCommand();
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9a170: advance past finished steps (OnCommandFinished / OnTaskStarted bracketing, wrap when circled),
// pull the next world command from the current step into the logic's queue (wrapped CCmd->CCommand), and
// finish the logic once an uncircled route runs out. Routes shorter than 2 commands never circle. The
// CAILogic base calls this from GetCommand() whenever its command queue is empty.
//   (dev<->release: the release step lifecycle calls OnCommandStarted; the dev CTaskCommand exposes
//    OnTaskStarted as the per-step start hook -- mapped by name.)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIRouteLogic::GenerateCommand()
{
	int n = (int)routeCommands.size();
	if ( n < 2 )
		bCircled = false;
	if ( nCurrentCommand < n )
	{
		bool bAdvance = nCurrentCommand == -1 || IsCurrentCommandFinished();
		if ( bAdvance )
		{
			if ( IsCommandInRange() )
				routeCommands[nCurrentCommand]->OnCommandFinished();
			++nCurrentCommand;
			if ( bCircled && nCurrentCommand == n )
				nCurrentCommand = 0;
			if ( IsCommandInRange() )
				routeCommands[nCurrentCommand]->OnTaskStarted();
		}
		bContinueCommand = false;
		if ( IsCommandInRange() )
		{
			NWorld::CCmd *pCmd = routeCommands[nCurrentCommand]->GetCommand();
			if ( pCmd )
				DoCommand( new NWorld::CCmdSetCommand( GetUnitServer(), pCmd ) );
		}
	}
	if ( !bCircled && nCurrentCommand >= n )
		Finish();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x98c50: the base end-of-turn wins; an unstarted non-empty route is not yet the end; otherwise the
// current step decides.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIRouteLogic::IsEndOfTurn()
{
	if ( CAILogic::IsEndOfTurn() )
		return true;
	if ( !routeCommands.empty() && nCurrentCommand == -1 )
		return false;
	if ( IsCommandInRange() )
		return routeCommands[nCurrentCommand]->IsEndOfUnitTurn();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x98d10 (PauseInner): pausing rewinds to the nearest preceding Goto step and marks the logic to
// continue it on resume (so an interrupted route re-walks the leg). Chains the base pause (nPause++).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIRouteLogic::Pause()
{
	CAILogic::Pause();
	int j = nCurrentCommand;
	while ( j >= 0 && j < (int)routeCommands.size() )
	{
		if ( dynamic_cast<CTaskCommandGoto*>( routeCommands[j].GetPtr() ) != 0 )
			break;
		--j;
	}
	if ( j < 0 || j >= (int)routeCommands.size() )
		return;
	bContinueCommand = true;
	if ( IsCommandInRange() )
		routeCommands[nCurrentCommand]->OnCommandFinished();
	nCurrentCommand = j;
	routeCommands[j]->OnTaskStarted();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9a680
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIRouteLogic* CreateAIRouteLogic( IAIUnit *pUnit, const vector< CPtr<CTaskCommand> > &cmds, bool bCircled )
{
	if ( !IsValid( pUnit ) || cmds.empty() )
		return 0;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	CAIRouteLogic *pLogic = new CAIRouteLogic( pUnit );
	pLogic->SetCircled( bCircled );
	for ( int i = 0; i < (int)cmds.size(); ++i )
	{
		if ( IsValid( cmds[i] ) )
		{
			cmds[i]->SetUnitServer( pUS );   // wire the unit's server into the step (decode NewRouteLogic)
			pLogic->AddCommand( cmds[i].GetPtr() );
		}
	}
	return pLogic;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9b8d0: strafe to a position. Gated on a path to it at the wish pose; queues [wish-pose, end-pose,
// goto]. Not circled.
//
// TWO documented dev<->release divergences (build-validation scope), both rooted in the dev tree being
// the PREDECESSOR command family:
//   1. STRAFE PREFIX -- NOW RESTORED. The release CRouteCommandGoto (@0x99280) prefixes a CCmdStrafe so the
//      unit side-steps while keeping its facing, unless it is running or is inside a Panzerklein. The gate
//      was NOT the assumed (absent) GetAttackObject -- it is CUnitServer::GetWearingDBPK (slot +0x34, present
//      in the dev), i.e. "am I wearing a PK". CTaskCommandGoto now carries a bStrafe flag and emits
//      CCmdStrafe(bStrafe) before CCmdPath under that gate (see CTaskCommandGoto::Do); the Strafe factory
//      passes bStrafe=true, the Move factory false.
//   2. ChangeWishPose ~= ChangePose. The release splits the pose step into ChangeWishPose (wish pose only)
//      and ChangePose (wish pose + in-place re-pose). The dev tree has only CTaskCommandChangePose; it is
//      reused for the wish-pose step too (it additionally emits a PF_USE_POSE re-pose path -- harmless for a
//      movement pose).
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIStrafeToPositionLogic( IAIUnit *pUnit, const SPosition &pos, EPose wishPose, EPose endPose )
{
	if ( !IsValid( pUnit ) )
		return 0;
	if ( !HasPath( pUnit, pUnit->GetUnitPosition().pos.p, pos.p, wishPose, false ) )
		return 0;
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangePose( wishPose ) );   // release: ChangeWishPose (see divergence #2)
	cmds.push_back( new CTaskCommandChangePose( endPose ) );
	cmds.push_back( new CTaskCommandGoto( pos, true ) );        // strafe to cover (retail @0x99280: CCmdStrafe before CCmdPath)
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The composite route-logic factories (oracle: decomp/src/s2_routelogic.h ~880-1148). Each assembles a
// CTaskCommand step list (reusing the existing command family + the release-new CTaskCommandLookToPosition,
// and the RouteAdd* builders for the randomized look-around / roam steps) and wraps it in a CAIRouteLogic.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace {
// RouteUnitAndServerAlive (decode @0x9a8e0): the unit is live and its server is live. (IsValid covers both
// the null and the destroyed-bit checks the decode's pfnIsAlive/pfnIsServerAlive do.)
inline bool RouteUnitAndServerAlive( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	return IsValid( pUnit->GetUnitServer() );
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9a920: face `place`, wait 3 seconds. Not circled.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAILookToPositionLogic( IAIUnit *pUnit, const SPathPlace &place )
{
	if ( !RouteUnitAndServerAlive( pUnit ) )
		return 0;
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandLookToPosition( place ) );
	cmds.push_back( new CTaskCommandWait( 3 ) );
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9aad0: gated on a path to `place` at movePose; queues [look at it, wish-pose for the move, goto (to the
// place's world position), end-pose]. Not circled. The HasPath `from` is the unit's current place (the decode
// hook supplied it implicitly). This is the MOVE variant, so its goto passes bStrafe=false (face-and-walk);
// only the ChangeWishPose~=ChangePose divergence remains (see the strafe factory's divergence note above).
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIMoveToPositionLogic( IAIUnit *pUnit, const SPathPlace &place, EPose movePose, EPose endPose, bool bCanFindNotExactPath )
{
	if ( !RouteUnitAndServerAlive( pUnit ) )
		return 0;
	if ( !HasPath( pUnit, pUnit->GetUnitPosition().pos.p, place, movePose, bCanFindNotExactPath ) )
		return 0;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	SPosition go = GetPos( place, pUS->GetWorld()->GetPathNetwork() );
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandLookToPosition( place ) );
	cmds.push_back( new CTaskCommandChangePose( movePose ) );   // release: ChangeWishPose (see strafe divergence #2)
	cmds.push_back( new CTaskCommandGoto( go, false ) );        // move-to-position: face-and-walk (not strafe), cf. @0x99280
	cmds.push_back( new CTaskCommandChangePose( endPose ) );
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateAIAlarmLogic @0x9b3d0 -- a scared, supported unit RUNs to a place beside its nearest ally, then the
// CTaskCommandAlarm step raises the garrison. Gated on a runnable path to that place; else 0 (caller falls back to
// the normal retreat/guard). The ally-roster + 5 m filter substitutes the absent SAIState::GetNearestUnit/GetUnitsAtRange.
IAILogic* CreateAIAlarmLogic( IAIUnit *pUnit, IAIUnit *pEnemy )
{
	if ( !RouteUnitAndServerAlive( pUnit ) || !IsValid( pEnemy ) )
		return 0;
	IAIState *pSt = pUnit->GetAIState();
	if ( pSt == 0 )
		return 0;
	IAIPlayer *pAllyPlayer = pSt->GetAllyAIPlayer();
	if ( pAllyPlayer == 0 )
		return 0;
	float fDist = 0.f;
	IAIUnit *pAlly = pAllyPlayer->GetNearestUnit( pUnit, &fDist );   // release GetNearestUnit(pos,allies,active,exclude=self)
	if ( !IsValid( pAlly ) || !IsValid( pAlly->GetUnitServer() ) || !IsValid( pAlly->GetUnitServer()->GetWorld() ) )
		return 0;
	IPathNetwork *pNet = pAlly->GetUnitServer()->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return 0;
	SSphere s;
	s.ptCenter = pAlly->GetPosition().GetCP();
	s.fRadius = 2.0f;   // a spot beside the ally
	vector<SPathPlace> places;
	pNet->GetNearPlaces( s, &places );
	if ( places.empty() )
		return 0;
	SPathPlace place = places[ random.Get( places.size() ) ];
	if ( !HasPath( pUnit, pUnit->GetUnitPosition().pos.p, place, RUN, false ) )   // gate: reachable at a run
		return 0;
	SPosition go = GetPos( place, pNet );
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangePose( RUN ) );          // EPose RUN = 3
	cmds.push_back( new CTaskCommandGoto( go, false ) );          // run to the ally (face-and-walk)
	cmds.push_back( new CTaskCommandLookToPosition( place ) );
	// NOTE: retail's route has a Wait(1) before the Alarm, but the dev's flag-based CTaskCommandWait::IsEndOfUnitTurn
	// depends on OnNewTurn() which CAIRouteLogic never propagates (the route path, unlike the old CTask path, doesn't
	// call it) -- inside a route the Wait would never complete in TBS, so the Alarm would never fire. Drop the cosmetic
	// 1-turn pause; the Alarm fires immediately on arrival (after the look). The garrison alert is identical.
	cmds.push_back( new CTaskCommandAlarm( pEnemy->GetUnitServer() ) );   // raise the garrison on arrival
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9ad50: change to `pose`, then a randomized look-around (RouteAddLookAround, nCount=6). Circled.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAILookRoundLogic( IAIUnit *pUnit, EPose pose )
{
	if ( !RouteUnitAndServerAlive( pUnit ) )
		return 0;
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangePose( pose ) );
	RouteAddLookAround( false, cmds, 6 );
	if ( cmds.empty() )
		return 0;
	return CreateAIRouteLogic( pUnit, cmds, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9aec0: one roam step + a short look-around (RouteAddRoaming, no re-pose), looping. Circled.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIRoamingLogic( IAIUnit *pUnit, const SPathPlace &center, int nRadius )
{
	if ( !RouteUnitAndServerAlive( pUnit ) )
		return 0;
	vector< CPtr<CTaskCommand> > cmds;
	RouteAddRoaming( center, nRadius, false, cmds );
	if ( cmds.empty() )
		return 0;
	return CreateAIRouteLogic( pUnit, cmds, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9b7b0: a single hide command. Not circled. Only unit liveness is checked here (no server gate,
// faithfully); the server/diplomacy/already-hidden gating happens inside CTaskCommandHide::Do.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIHideLogic( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return 0;
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandHide() );
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9afb0: inspect `pos` (a noise/clue spot). An optional hide prefix (a hide-probability roll). If
// GetCheckPosition finds a reachable check spot inside `pArea` (and HasPath reaches it at RUN) -> look at `pos`,
// take `pose`, maybe hide again, then goto the spot. OTHERWISE, only when the unit has NO current enemy -> look
// at `pos`, crouch unless in a panzerklein, and glance in a jittered network direction toward `pos`. Anything
// queued gets a 6-second wait. Not circled. Reconstructed from the matched-release decode
// (decomp/src/s2_routelogic.h:1043-1081) + disasm_func 0x9afb0.
//
// TWO answer-key MISLABELS corrected from the raw vtable (the session-18 discipline: resolve load-bearing
// reaches from scan_vtables + disasm, not the pfn* label):
//   * the answer-key's pfnUnitStanding (// vtbl+0x20) is actually CAIUnit::IsInPK (slot 8) -- so the gate is
//     "crouch UNLESS the unit is busy in a panzerklein", NOT "crouch if standing". Reproduced via the in-tree
//     IsInPK idiom IsValid(pUS->GetWearingDBPK()) (same as aiDefenceReaction.cpp).
//   * the answer-key's pfnUnitHasEnemy (// v+0x74 -> +0x88) is GetAIUnitState() then the validity of
//     state.pEnemy (SAIUnitState +0x88 == "most dangerous known enemy") -- the same enemy-read idiom
//     aiDefenceReaction.cpp uses.
//
// Documented elisions (build-validation scope): the jitter random(-1,2) (range [-1,2), i.e. -1/0/+1) is
// transcribed as (int)random.Get(3)-1 -- one ISAAC draw, identical distribution -- because the in-tree
// CRandomGenerator::Get(min,max) takes UNSIGNED args (random.Get(-1,2) would be garbage). GetCheckPosition + the
// goto carry aiRouteMisc's documented path-primitive refinements. Dead code until SetLogic'd -> no live change.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAICheckPositionLogic( IAIUnit *pUnit, CUnitArea *pArea, const SPosition &pos, EPose pose )
{
	if ( !RouteUnitAndServerAlive( pUnit ) || pArea == 0 )
		return 0;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	vector< CPtr<CTaskCommand> > cmds;
	// optional hide prefix (hide-probability roll)
	if ( random.Get( 0, 99 ) < (unsigned)pUnit->GetHideProbability() )
		cmds.push_back( new CTaskCommandHide() );
	SPathPlace checkPlace( (int)0xfdffffff );   // SPathPlace(int) ctor (nData is private) -- the invalid sentinel
	if ( GetCheckPosition( pUnit, pArea, pos, &checkPlace ) )
	{
		if ( HasPath( pUnit, pUnit->GetUnitPosition().pos.p, checkPlace, RUN, false ) )
		{
			cmds.push_back( new CTaskCommandLookToPosition( pos.p ) );
			cmds.push_back( new CTaskCommandChangePose( pose ) );
			if ( random.Get( 0, 99 ) < (unsigned)pUnit->GetHideProbability() )
				cmds.push_back( new CTaskCommandHide() );
			SPosition go = GetPos( checkPlace, pUS->GetWorld()->GetPathNetwork() );
			cmds.push_back( new CTaskCommandGoto( go ) );
		}
	}
	else
	{
		// only when the unit has no current enemy (GetAIUnitState()->pEnemy invalid)
		SAIUnitState *pState = pUnit->GetAIUnitState();
		bool bHasEnemy = pState != 0 && IsValid( pState->pEnemy.GetPtr() );
		if ( !bHasEnemy )
		{
			cmds.push_back( new CTaskCommandLookToPosition( pos.p ) );
			// crouch to inspect UNLESS the unit is busy in a panzerklein (release vtbl+0x20 == IsInPK)
			if ( !IsValid( pUS->GetWearingDBPK() ) )
				cmds.push_back( new CTaskCommandChangePose( CROUCH ) );
			IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
			int nDir = (int)pNet->GetClosestDir( pUnit->GetUnitPosition().pos.p, pos.p ) + ( (int)random.Get( 3 ) - 1 );
			if ( nDir <= 0 )
				nDir = 0;
			else if ( nDir > 7 )
				nDir = 7;
			cmds.push_back( new CTaskCommandChangeDirection( nDir ) );
		}
	}
	if ( cmds.empty() )
		return 0;
	cmds.push_back( new CTaskCommandWait( 6 ) );
	return CreateAIRouteLogic( pUnit, cmds, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x9a710: "go check it out". An optional hide prefix (a hide-probability roll), then the RouteAddRoundUp
// flanking script against the enemy position -- the round-up is told the enemy is SILENT when it is not
// currently audible (so it includes the friend-avoidance sweep). A logic is built only if the round-up queued
// at least one command, then a trailing 6s wait. Not circled. Reconstructed from the matched-release decode
// (decomp/src/s2_routelogic.h:1010-1035) + disasm_func 0x9a710.
//
// Seam resolution (the one open hook): pfnIsAudible @0x49a80c (`add ecx,0x168; call 0x6fd7f0`) is
// NWorld::CAudibleSet<CUnitServer>::IsAudible(CUnitServer const*) -- a MEMBERSHIP query (is the enemy server in
// my audibleUnits list, wUnitSounds.h:116), NOT a CanHearSound/hearing-probability roll. CUnitServer inherits
// CAudibleSet, so it is the direct base call pUS->IsAudible(enemyServer). GetHideProbability (sess19),
// CTaskCommandHide (sess18), CreateRCWait, CreateAIRouteLogic are all already in-tree.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAICheckForEnemyLogic( IAIUnit *pUnit, const SUnitPosition &enemyPos, EPose pose, IAIUnit *pEnemy )
{
	if ( !RouteUnitAndServerAlive( pUnit ) )
		return 0;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	vector< CPtr<CTaskCommand> > cmds;
	// optional hide prefix (hide-probability roll)
	if ( random.Get( 0, 99 ) < (unsigned)pUnit->GetHideProbability() )
		cmds.push_back( new CTaskCommandHide() );
	int nBefore = (int)cmds.size();
	bool bAudible = false;
	NWorld::CUnitServer *pEnemyUS = 0;
	if ( pEnemy != 0 )
	{
		bAudible = pUS->IsAudible( pEnemy->GetUnitServer() );   // CAudibleSet membership query (the enemy's sound)
		if ( IsValid( pEnemy ) )                                // the decode's pfnIsAlive(pEnemy) before passing the server
			pEnemyUS = pEnemy->GetUnitServer();
	}
	RouteAddRoundUp( pUS, pEnemyUS, enemyPos, pose, cmds, !bAudible );
	if ( (int)cmds.size() > nBefore )
	{
		cmds.push_back( new CTaskCommandWait( 6 ) );
		return CreateAIRouteLogic( pUnit, cmds, false );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x51443110, CAIRouteLogic )
REGISTER_SAVELOAD_CLASS( 0x52443111, CTaskCommandLookToPosition )
REGISTER_SAVELOAD_CLASS( 0x23068341, CTaskCommandHide )
REGISTER_SAVELOAD_CLASS( 0x23062480, CTaskCommandAlarm )   // release CRouteCommandAlarm id (save-game parity)
