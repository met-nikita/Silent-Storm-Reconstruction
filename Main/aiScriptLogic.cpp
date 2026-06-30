#include "StdAfx.h"
//
#include "aiUnit.h"          // NAI::IAIUnit
#include "wUnitServer.h"     // NWorld::CUnitServer (GetPlayer)
#include "wInterface.h"      // NWorld::IPlayer::GetCommander, NWorld::CCommander::HasCommands
#include "scriptCallLUA.h"   // NScript::luaCallFunction
#include "eventPlayer.h"     // NWorld::CEventOnNewPlayerTurn (complete -- regOnNewTurn ctor + handler)
#include "eventWorld.h"      // NWorld::CEventOnSegment       (complete -- regOnSegment ctor + handler)
//
#include "aiScriptLogic.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiScriptLogic -- the script-driven unit's AI logic. Reconstructed from the matched-release decode (oracle
// decomp/src/s2_aiscriptlogic.h; the field offsets / branch logic disasm-verified @0xa30c0..0xa3470).
// See aiScriptLogic.h for the architecture + the transient/event notes.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Both ctors must init the CEventRegister members (they have no usable default ctor -- it ASSERTs). The
// default ctor (the BASIC_REGISTER_CLASS create path) seeds nCommandSegment=0 (release @0xa3270); the
// unit-bound ctor seeds nCommandSegment=-5 so the very first GenerateCommand's "+5" gate starts armed
// (release @0xa3360).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIScriptLogic::CAIScriptLogic()
	: regOnNewTurn( this, &CAIScriptLogic::OnNewTurn ),
	  regOnSegment( this, &CAIScriptLogic::OnSegment ),
	  bEndOfTurn( false ), nSegment( 0 ), nCommandSegment( 0 ), bCommandGiven( false )
{
}
CAIScriptLogic::CAIScriptLogic( IAIUnit *pUnit )
	: CAILogic( pUnit ),
	  regOnNewTurn( this, &CAIScriptLogic::OnNewTurn ),
	  regOnSegment( this, &CAIScriptLogic::OnSegment ),
	  bEndOfTurn( false ), nSegment( 0 ), nCommandSegment( -5 ), bCommandGiven( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa30e0 / @0xa30f0 -- the two event handlers.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIScriptLogic::OnNewTurn( const NWorld::CEventOnNewPlayerTurn & ) { bEndOfTurn = false; }
void CAIScriptLogic::OnSegment( const NWorld::CEventOnSegment & )       { ++nSegment; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa30c0 -- the logic ends the turn once it has surrendered, else it defers to the base.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIScriptLogic::IsEndOfTurn()
{
	return bEndOfTurn || CAILogic::IsEndOfTurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3140 -- ask the script for commands, then surrender if it stays silent.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIScriptLogic::GenerateCommand()
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// First call of this window: fire the "OnUnitNeedCommand" lua hook so the mission script can command the
	// unit, and remember the segment we asked on.
	if ( !bEndOfTurn && !bCommandGiven )
	{
		NScript::luaCallFunction( "OnUnitNeedCommand", "p", CastToObjectBase( pUS ) );
		nCommandSegment = nSegment;
		bCommandGiven = true;
	}
	// Five segments after asking: if the unit's player still has nothing queued, surrender the turn. Either
	// way re-arm bCommandGiven so the next window asks again (the reset is unconditional here -- @0xa323d).
	if ( nCommandSegment + 5 <= nSegment )
	{
		NWorld::IPlayer *pPlayer = pUS->GetPlayer();
		NWorld::CCommander *pCommander = IsValid( pPlayer ) ? pPlayer->GetCommander() : 0;
		if ( pCommander == 0 || !pCommander->HasCommands() )
			bEndOfTurn = true;
		bCommandGiven = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3470 -- a live unit with a live unit server -> a new script logic.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIScriptLogic( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return 0;
	return new CAIScriptLogic( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
BASIC_REGISTER_CLASS( CAIScriptLogic )
