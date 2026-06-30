#pragma once
#include "RPGUnit.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Retail-new (wExplosionPerks.obj): per-unit explosive-perk modifiers.
// Filled from the placing/throwing unit's RPG perks, then handed to the mine /
// grenade / explosion code via SetPerkModifiers. The owning unit must be valid
// (alive / not scheduled for deletion); dead or missing units leave the struct
// untouched. Callers seed sensible defaults (e.g. SPerkMineModifiers{1,1,false})
// before Fill, which only overrides on a present perk. Perk ids:
//   0x30 -> structure-damage modifier, 0x35 -> always-human-critical flag,
//   0x5e -> area-effect-damage modifier.
struct SPerkMineModifiers
{
	float fStructureDmgModifier;
	float fAEDmgModifier;
	bool  bAlwaysHumanCritical;

	// neutral defaults: multipliers of 1 (no change), no forced crit. Fill() only overrides on a present perk, so a
	// thrower with no explosive perks (or a thrower-less environmental blast) leaves the explosion damage untouched.
	SPerkMineModifiers() : fStructureDmgModifier( 1.0f ), fAEDmgModifier( 1.0f ), bAlwaysHumanCritical( false ) {}
	void Fill( NRPG::CUnit *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NWorld
