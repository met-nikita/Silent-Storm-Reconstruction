#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiGrid.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"   // NWorld::CCmd, CCmdEmpty, CCommand
//
#include "aiLogic.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release AI behaviour base - CAILogic bodies (structural port). Reconstructed from
// reconstruction/exports/engine.c (CAILogic::GetCommand/CheckCycling/CanGetCommand/ctor + accessors).
// WIP - NOT yet in Main.vcxproj. Supersedes the dev aiCompoundAction.h CAILogic at phase 7.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogic::CAILogic(): bFinished( false ), bHasPointOfInterest( false ), nPause( 0 )
{
}
CAILogic::CAILogic( IAIUnit *_pUnit ): pUnit( _pUnit ), bFinished( false ), bHasPointOfInterest( false ), nPause( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// accessors
IAIUnit*             CAILogic::GetUnit() const       { return pUnit; }
NWorld::CUnitServer* CAILogic::GetUnitServer() const { return IsValid( pUnit ) ? pUnit->GetUnitServer() : 0; }
NWorld::IWorld*      CAILogic::GetWorld() const      { return IsValid( GetUnitServer() ) ? GetUnitServer()->GetWorld() : 0; }
SAIState*            CAILogic::GetAIState() const     { return IsValid( pUnit ) ? pUnit->GetAIState() : 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// state
void CAILogic::Finish()                              { bFinished = true; }
// retail @0x62460: while the unit chain is ALIVE (AI unit -> server -> the server's RPG object all
// valid), the logic is NEVER finished -- bFinished only answers once that chain breaks (death/removal).
// The dev's bare `return bFinished;` let CheckForFinishedLogics retire a LIVING unit's logic on every
// self-Finish (the cycling guard, a spent route), after which nothing re-decided the unit until its
// threat state changed -- the "shoots a bit then stands around forever" starvation.
bool CAILogic::IsFinished()
{
	if ( IsValid( pUnit ) )
	{
		NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
		if ( IsValid( pUS ) && IsValid( pUS->GetRPG() ) )
			return false;
	}
	return bFinished;
}
bool CAILogic::HasCommands() const                   { return !commands.empty(); }
void CAILogic::ClearCommands()                       { commands.clear(); }
void CAILogic::Pause()                               { ++nPause; }
void CAILogic::Resume()                              { if ( nPause > 0 ) --nPause; }
bool CAILogic::IsActive() const                      { return nPause == 0; }
bool CAILogic::IsNeedToThink()                       { return false; }
void CAILogic::Think()                               {}
bool CAILogic::IsThinking()                          { return false; }
void CAILogic::StopThinking()                        {}
void CAILogic::GenerateCommand()                     {}   // base: no-op (@0x004621f0)
bool CAILogic::GetPointOfInterest( CVec3 *pOut ) const
{
	if ( bHasPointOfInterest && pOut )
		*pOut = vPointOfInterest;
	return bHasPointOfInterest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// command queue
void CAILogic::DoCommand( NWorld::CCommand *pCmd )    { if ( pCmd ) commands.push_back( pCmd ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// True if the unit is in a state where it can accept the next command this segment. @0x00462550
bool CAILogic::CanGetCommand() const
{
	if ( !IsValid( pUnit ) || nPause > 0 )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	// not while the unit is still performing an action / animating
	return !pUS->IsPerformingAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Pop the next world command (the commander consumes NWorld::CCommand - CCmdSetCommand wrappers the
// AI log records emit; the unit-level NWorld::CCmd is the wrapped payload). @0x00462c70
NWorld::CCommand* CAILogic::GetCommand()
{
	if ( !CanGetCommand() )
		return 0;
	NWorld::CUnitServer *pUS = IsValid( pUnit ) ? pUnit->GetUnitServer() : 0;
	if ( !IsValid( pUS ) )
		return 0;
	if ( pUS->IsPerformingAction() )
		return 0;
	// retail @0x462c70 KEEPALIVE (decomp-proven; supersedes the old "yield nothing" build-settle stub):
	// an IDLE unit whose server still holds an installed command (pCurrentCmd alive) gets a bare RESUME --
	// in realtime always, in turn-based only when the unit can afford it (person vtbl+0x60 HasEnoughAP).
	// The keepalive takes PRIORITY over the queued commands (retail's else-branch pops only when this
	// condition fails), and CheckCycling runs on it (the retail escape hatch: a genuinely wedged executor
	// accumulates same-place pulses -> 25% CCmdCancel at 16+). This is what re-runs a command that crossed
	// a turn boundary unaffordable (IsEndOfTurn's EOT-WITH-CMD ending): with the fresh turn's AP the resume
	// executes it. Without it the unit DEADLOCKED its whole turn (HasCommandToExecute true -> never thinks;
	// GetCommand null -> never acts; IsEndOfTurn false -> turn never ends -- the GFirst turn-2 stall).
	// Retail returns a raw CCmdEmpty that the commander converts to ONE CCmdSetCommand{unit, CCmdContinue};
	// the dev logic returns CCommand wrappers, so hand back the converted form directly (the drive detects
	// a bare-Continue inner and skips its usual second Continue).
	if ( pUS->HasCommand() && ( !pUS->GetWorld()->IsTurnBased() || pUS->HasEnoughAP() ) )
	{
		CheckCycling();
		return new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() );
	}
	if ( commands.empty() )
		GenerateCommand();
	if ( commands.empty() )
		return 0;
	NWorld::CCommand *pCmd = commands.front().Extract();
	commands.pop_front();
	if ( pCmd )
		CheckCycling();
	return pCmd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Anti-cycling, converged to retail @0x62a50 (SCyclingTracker::CheckCycling @0x62a10 inlined):
//  * the cycle key is the masked PLACE (IsSamePlace, mask 0x1feffff -- same tile ignoring the
//    direction/pose/moving bits) AND the LIVE server AP. The dev predecessor compared ONLY the AP, so in
//    real time (AP pinned at max) every multi-step route logic read "same" on 3 straight pops and died
//    mid-route -- the Defence strafe never reached its cover/attack spot ("crouches and gets stuck").
//  * escalation, not a kill switch: 16..30 repeats -> 25% chance per pop to CCmdCancel the server's
//    running command (unwedge the executor) + re-set the AP pool to the just-read value (retail person
//    vtbl+0x7c, the IUnitMission AP setter -- dev seam IAIUnit::SetAP, as CAIDefenceReaction uses);
//    only at >= 31 repeats log the fatal cycle and set bFinished. (For a LIVING unit bFinished no longer
//    retires the logic -- IsFinished @0x62460 masks it -- it only ends the TBS turn via base IsEndOfTurn.)
//  * the LIVE-AP read (retail reads the unit's current AP skill, which decreases as an action spends AP)
//    is kept from the earlier root-fix: a progressing shoot resets the counter instead of tripping it.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::CheckCycling()
{
	if ( !IsValid( pUnit ) )
		return;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	int nLiveAP = pUS->GetAP();
	SPlaceWithAP cur( pUnit->GetUnitPosition(), nLiveAP );
	bool bSamePlace = ( ( cur.place.pos.p.GetData() ^ cyclingTracker.place.place.pos.p.GetData() ) & 0x1feffff ) == 0;
	if ( bSamePlace && cur.nUnitAP == cyclingTracker.place.nUnitAP )
		++cyclingTracker.nSame;
	else
		cyclingTracker.Init( cur );
	if ( cyclingTracker.nSame >= 31 )        // retail: !(nSame < 0x1f)
	{
		DebugTrace( " AI : fatal cycling detected : logic canceled \n" );
		bFinished = true;
	}
	else if ( cyclingTracker.nSame > 15 )    // retail: nSame > 0xf
	{
		if ( ( random.Get() & 3 ) == 0 )     // retail: raw ISAAC draw & 3 == 0 (25%)
		{
			pUS->Do( new NWorld::CCmdCancel( pUS ) );
			pUnit->SetAP( nLiveAP, pUnit->GetMaxAP() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsEndOfTurn / GetCommand-gating are situation specific; base end-of-turn = finished or no AP. @0x00462750
bool CAILogic::IsEndOfTurn()
{
	if ( bFinished )
		return true;
	return IsValid( pUnit ) && pUnit->GetAP() <= 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x418350 -- retail wire (byte-walk-confirmed on slot-1 CAIRouteLogic instances): {2 nPause int4,
// 3 pUnit CPtr, 4 commands, 5 bFinished 1B, 6 cyclingTracker NESTED CHUNK, 7 bHasPointOfInterest 1B,
// 8 vPointOfInterest CVec3 raw12}. Tag 6 is ONE SCyclingTracker chunk (operator& @0x18410:
// {2 SPlaceWithAP{2 SUnitPosition{2 SPosition{1 place4,2 pNet4}, 4 bRun1}, 3 nUnitAP4}, 3 nSame4} --
// 33 bytes). The old dev double-write `Add(6,&place); Add(6,&nSame)` flattened one nesting level, so
// loading a retail save misread the whole subtree (wire-audit family: route logic 2.6 / combat
// logics 2.2.6 -- SIZE 33v4, RAWSZ 12v4, UNREAD bRun/nUnitAP, MISS place/pNet) and dev saves carried
// two tag-6 chunks retail never wrote.
int CAILogic::operator&( CStructureSaver &f )         // @0x00418350 / 0x00433820
{
	f.Add( 2, &nPause );
	f.Add( 3, &pUnit );
	f.Add( 4, &commands );
	f.Add( 5, &bFinished );
	f.Add( 6, &cyclingTracker );
	f.Add( 7, &bHasPointOfInterest );
	f.Add( 8, &vPointOfInterest );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
BASIC_REGISTER_CLASS( CAILogic )
