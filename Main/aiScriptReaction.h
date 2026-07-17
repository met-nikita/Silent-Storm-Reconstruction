#ifndef __AISCRIPTREACTION_H_
#define __AISCRIPTREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiReaction.h"   // NAI::CAIReaction (the reflex base)
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiScriptReaction (oracle: decomp/src/s2_aiscriptreaction.h; CreateAIScriptReaction @0xa37b0, Update
// @0xa3810 -- disasm-verified). The reaction installed on a script-controlled unit (UnitSetScriptLogic): its
// whole behaviour is to (re)install a fresh CAIScriptLogic each Update -- that per-turn reinstall is what
// makes the script logic's "OnUnitNeedCommand" hook fire once per turn. Retail-NEW; absent from the dev tree.
//
// TRANSIENT like the other dev reactions (BASIC_REGISTER_CLASS, cast-only): the dev CAIUnit does not
// serialize its reaction, so the release saveload id 0x52443161 + operator& are moot and not reproduced.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIScriptReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIScriptReaction );
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); return 0; }   // retail @0x3b900 (base chunk only)
public:
	CAIScriptReaction() {}
	CAIScriptReaction( IAIUnit *pUnit ): CAIReaction( pUnit ) {}
	//
	virtual void Update();   // @0xa3810
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIScriptReaction @0xa37b0 -- a live unit -> a new script reaction (the release factory's 2nd
// `bUnitAlive` arg is the caller-precomputed IsValid(pUnit), folded into IsValid here, like the other factories).
CAIReaction* CreateAIScriptReaction( IAIUnit *pUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AISCRIPTREACTION_H_
