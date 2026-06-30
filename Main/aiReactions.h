#ifndef __AIREACTIONS_H_
#define __AIREACTIONS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Concrete AI reactions. Only the framework-driving ones are ported so far:
//   CAIEmptyReaction  - the null reaction; Update() does nothing.
//   CAINormalReaction - the default: enemy in view -> CAIAttackLogic; once combat is over ->
//                       CAIAfterCombatLogic; never-fought + no enemy -> idle. (CAINormalReaction::Update
//                       @0x0047f190; the scared/Defence/Assassin/Guard/Retreat/Fear escalations need the
//                       absent SAIUnitState threat tracker + event system and are deferred.)
// Fear/Assassin/Guard/Retreat/Defence/Script reactions exist in the release (each its own aiXxxReaction.obj)
// and plug in here once the threat tracker lands.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiReaction.h"
#include "aiPosition.h"   // SPathPlace (CAIRetreatReaction member)
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIEmptyReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIEmptyReaction );
public:
	CAIEmptyReaction() {}
	CAIEmptyReaction( IAIUnit *_pUnit ): CAIReaction( _pUnit ) {}
	virtual void Update() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAINormalReaction - size 0x14 = base + bWasCombat (release layout).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAINormalReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAINormalReaction );
	bool bWasCombat;                            // +0x10  was the unit in combat last think?
public:
	CAINormalReaction(): bWasCombat( false ) {}
	CAINormalReaction( IAIUnit *_pUnit ): CAIReaction( _pUnit ), bWasCombat( false ) {}
	virtual void Update();                       // @0x0047f190
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRetreatReaction - a unit falling back toward `pos` (a fear position). Update @0x00495ad0 (reconciled to
// the release): on reaching `pos` it becomes a Guard reaction; else it retreats from a live enemy / glances
// toward a suspected one / and (with no current logic) keeps moving to `pos` at a run. No bScared/Normal-revert
// (that was the dev predecessor). See aiReactions.cpp for the elisions + the SetReaction-during-Update lifetime.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRetreatReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIRetreatReaction );
	SPathPlace pos;                             // +0x10  the fall-back target
public:
	CAIRetreatReaction() {}
	CAIRetreatReaction( IAIUnit *_pUnit, const SPathPlace &_pos ): CAIReaction( _pUnit ), pos( _pos ) {}
	virtual void Update();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIRetreatReaction( IAIUnit *pUnit, const SPathPlace &pos );   // @0x00495a20
CAIReaction* CreateAIEmptyReaction( IAIUnit *pUnit );                            // @0x0043b780
CAIReaction* CreateAINormalReaction( IAIUnit *pUnit );                           // @0x0007efe0
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AIREACTIONS_H_
