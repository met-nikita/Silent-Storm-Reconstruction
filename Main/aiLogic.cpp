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
IAIState*            CAILogic::GetAIState() const     { return IsValid( pUnit ) ? pUnit->GetAIState() : 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// state
void CAILogic::Finish()                              { bFinished = true; }
bool CAILogic::IsFinished()                          { return bFinished; }
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
	// build-settle @0x00462c70: the release yields an empty/keepalive command while the unit is still
	// performing an action; that empty is a CCommand-level keepalive (not the CCmd CCmdEmpty). For now
	// defer to the commander's own IsPerformingAction gate and yield nothing.
	if ( pUS->IsPerformingAction() )
		return 0;
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
// Anti-cycling: if the unit keeps returning to the same place+AP, count it and bail after a threshold.
// @0x00462a50 (the full version logs to the AI console). Faithful intent below.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::CheckCycling()
{
	const int N_CYCLE_LIMIT = 3;
	if ( !IsValid( pUnit ) )
		return;
	SPlaceWithAP cur( pUnit->GetUnitPosition(), pUnit->GetAP() );
	// build-settle: add SPathPlace equality to the place check (release uses an inline same-place test)
	if ( cur.nUnitAP == cyclingTracker.place.nUnitAP )
	{
		if ( ++cyclingTracker.nSame >= N_CYCLE_LIMIT )
			Finish();
	}
	else
		cyclingTracker.Init( cur );
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
int CAILogic::operator&( CStructureSaver &f )         // @0x00418350 / 0x00433820
{
	f.Add( 2, &nPause );
	f.Add( 3, &pUnit );
	f.Add( 4, &commands );
	f.Add( 5, &bFinished );
	f.Add( 6, &cyclingTracker.place );  f.Add( 6, &cyclingTracker.nSame );
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
