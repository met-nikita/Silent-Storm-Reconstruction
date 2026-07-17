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
	// retail wraps the base as a NESTED chunk at tag 2 like every concrete reaction (its operator&
	// is ICF-folded with CAIScriptReaction::operator& @0x3b900: {2 = CAIReaction base{2 pUnit ref}}).
	// Dev inherited CAIReaction::operator& directly, so tag 2 was read as a bare 4-byte ptr while the
	// save carries the 6-byte nested chunk -> wire-audit RAWSZ 6v4 @2.2 (slot 9, byte-walked obj#37784).
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); return 0; }
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
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); f.Add( 3, &bWasCombat ); return 0; }   // retail @0x7f780
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
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); f.Add( 3, &pos ); return 0; }   // retail @0x95e20 (pos = 4-byte DataChunk)
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
