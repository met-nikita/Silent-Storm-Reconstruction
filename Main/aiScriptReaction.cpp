#include "StdAfx.h"
//
#include "aiUnit.h"          // NAI::IAIUnit
#include "aiScriptLogic.h"   // NAI::CreateAIScriptLogic
//
#include "aiScriptReaction.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiScriptReaction -- the script-controlled unit's reaction. Reconstructed from the matched-release decode
// (oracle decomp/src/s2_aiscriptreaction.h, Update disasm-verified @0xa3810). See aiScriptReaction.h.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3810 -- a live unit -> (re)install a fresh script logic. Unconditional, faithful to the decode (no
// "already a script logic?" guard, unlike CAINormalReaction's attack-logic check): the commander calls this
// once per unit per AI turn (ChooseLogic), so the per-turn reinstall resets the logic's state -> the
// "OnUnitNeedCommand" hook fires once per turn.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIScriptReaction::Update()
{
	IAIUnit *pU = GetUnit();
	if ( IsValid( pU ) )
		SetLogic( CreateAIScriptLogic( pU ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa37b0 -- a live unit -> a new script reaction.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIScriptReaction( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return 0;
	return new CAIScriptReaction( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x52443161, CAIScriptReaction )
