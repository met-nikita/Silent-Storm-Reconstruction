#ifndef __AIASSASSINREACTION_H_
#define __AIASSASSINREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiReaction.h"    // NAI::CAIReaction (the reflex base)
#include "aiPosition.h"    // NAI::SUnitPosition / SPathPlace / EPose
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiAssassinReaction (oracle: decomp/src/s2_aiassassinreaction.h, all disasm-verified @0x1ab00..0x1b500).
// The sneak-attack reaction: a unit that holds a short arm (pistol/SMG), is not engaged and has a distant
// live enemy hides, then creeps -- via places the enemy cannot hear it running to -- until it is close
// enough to strike, at which point it hands itself back to the Normal reaction (which fires the attack).
// Any break in the plan (the enemy dies, the hide is lost, no quiet approach exists) also gives up to Normal.
// Retail-NEW (no Jan03 counterpart); absent from the dev tree.
//
// TRANSIENT like the other dev reactions (CAINormalReaction/CAIDefenceReaction/CAIGuardReaction): registered
// with BASIC_REGISTER_CLASS (cast-only), NOT saveload-serialized. The release IS saveload-registered
// (REGISTER_SAVELOAD_CLASS id 0x23069400 + operator& @0x1b860), but the dev CAIUnit does NOT serialize its
// reaction (aiUnit.cpp:65 -- pReaction is transient, rebuilt each think), so the id/operator& are moot here
// and not reproduced (the same simplification CAIDefenceReaction/CAIGuardReaction make). pEnemy is a CPtr
// weak ref re-picked from the threat tracker each pass; bJustStarted suppresses the "broke out of hiding"
// give-up on the construction pass.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIAssassinReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIAssassinReaction );
	CPtr<IAIUnit> pEnemy;       // +0x10  the tracked victim (re-picked from SAIUnitState when it dies / can't fight)
	bool          bJustStarted; // +0x14  first pass after construction (suppresses the broke-out-of-hiding give-up)
public:
	CAIAssassinReaction(): bJustStarted( true ) {}
	CAIAssassinReaction( IAIUnit *pUnit );   // @0x1b3a0 (pEnemy null, bJustStarted true)
	//
	virtual void Update();   // @0x1b500 -- the hide-and-creep dance
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIAssassinReaction @0x1ade0 -- a live unit -> a new reaction. (The release factory carries a
// precomputed bUnitAlive arg; folded into the default + the IsValid(pUnit) gate, like CreateAIGuardReaction.)
CAIReaction* CreateAIAssassinReaction( IAIUnit *pUnit, bool bUnitAlive = true );
// NAI::CanUseAssassinReaction @0x1ab00 -- fight-capable, not already in a panzerklein, a pistol/SMG in hand,
// a live known-or-suspected enemy more than 5 units away, the difficulty assassin-chance d100 passes, and the
// unit is already hidden or the engine accepts a hide command.
bool CanUseAssassinReaction( IAIUnit *pUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIASSASSINREACTION_H_
