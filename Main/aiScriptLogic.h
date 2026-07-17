#ifndef __AISCRIPTLOGIC_H_
#define __AISCRIPTLOGIC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiLogic.h"             // NAI::CAILogic (the command-driven AI logic base)
#include "..\Misc\EventsBase.h"  // NGlobal::CEventRegister (regOnNewTurn / regOnSegment)
#include "eventPlayer.h"         // NWorld::CEventOnNewPlayerTurn (COMPLETE: the CEventRegister member's dtor
#include "eventWorld.h"          // NWorld::CEventOnSegment       typeid's the event, so every includer that
                                 // instantiates CAIScriptLogic's special members needs the complete types)
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiScriptLogic (oracle: decomp/src/s2_aiscriptlogic.h; CreateAIScriptLogic @0xa3470, ctor @0xa3360,
// GenerateCommand @0xa3140, IsEndOfTurn @0xa30c0, OnNewTurn @0xa30e0, OnSegment @0xa30f0 -- all disasm-
// verified). The AI logic of a script-controlled unit: each time the commander asks it for a command it
// fires the Lua hook "OnUnitNeedCommand" (so the mission script can issue the unit's commands), then gives
// the script five world segments to queue something on the unit's player before surrendering the turn.
// (CAIScriptReaction re-installs a fresh one each turn, which is what makes the hook fire once per turn.)
//
// The release subscribes two NGlobal events: OnNewTurn(CEventOnPassControl) clears bEndOfTurn at turn start,
// OnSegment(CEventOnSegment) counts segments. The dev's per-turn event is NWorld::CEventOnNewPlayerTurn (the
// same one CAIAfterCombatLogic subscribes); the per-segment CEventOnSegment was absent and is reconstructed
// (eventWorld.h, thrown from CWorld::Segment). The CEventRegister members are runtime subscriptions, NOT
// serialized -- exactly like CAIAfterCombatLogic::regOnNewTurn.
//
// TRANSIENT like the dev combat logics (CAIAttackLogic/CAIAfterCombatLogic): BASIC_REGISTER_CLASS, not
// saveload-serialized (the reaction rebuilds it each turn). The release saveload id 0x51253122 + operator&
// are moot under the dev's rebuild-each-turn model and not reproduced.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIScriptLogic: public CAILogic
{
	OBJECT_BASIC_METHODS( CAIScriptLogic );
	NGlobal::CEventRegister< CAIScriptLogic, NWorld::CEventOnNewPlayerTurn > regOnNewTurn;  // clears bEndOfTurn
	NGlobal::CEventRegister< CAIScriptLogic, NWorld::CEventOnSegment >       regOnSegment;  // counts segments
	bool bEndOfTurn;       // +0x70  the script gave up / had no command this turn -> surrender
	int  nSegment;         // +0x74  world segments elapsed (OnSegment bumps it)
	int  nCommandSegment;  // +0x78  the segment the "OnUnitNeedCommand" hook last fired on
	bool bCommandGiven;    // +0x7c  the hook fired this window (re-armed every 5 segments)
public:
	// retail @0xa3710: 2=CAILogic base, 3=bEndOfTurn, 4=nSegment, 5=nCommandSegment, 6=bCommandGiven
	// (PDB offsets +0x70/74/78/7c; Ghidra's shifted names resolved via layout). Convergence W2.
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAILogic*)this ); f.Add( 3, &bEndOfTurn ); f.Add( 4, &nSegment ); f.Add( 5, &nCommandSegment ); f.Add( 6, &bCommandGiven ); return 0; }
private:
public:
	CAIScriptLogic();
	CAIScriptLogic( IAIUnit *pUnit );
	//
	// the two NGlobal event handlers (NOT the base no-arg OnNewTurn()/OnSegment() vtable slots -- declaring
	// these const& overloads hides the inherited no-arg names so &CAIScriptLogic::OnNewTurn is unambiguous,
	// matching CAIAfterCombatLogic; the base no-op virtuals still fill the vtable slots).
	void OnNewTurn( const NWorld::CEventOnNewPlayerTurn &event );   // @0xa30e0: bEndOfTurn = false
	void OnSegment( const NWorld::CEventOnSegment &event );         // @0xa30f0: ++nSegment
	//
	virtual bool IsEndOfTurn();       // @0xa30c0: bEndOfTurn || CAILogic::IsEndOfTurn()
	virtual void GenerateCommand();   // @0xa3140: fire the hook, surrender after 5 idle segments
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIScriptLogic @0xa3470 -- a live unit with a live unit server -> a new script logic.
IAILogic* CreateAIScriptLogic( IAIUnit *pUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AISCRIPTLOGIC_H_
