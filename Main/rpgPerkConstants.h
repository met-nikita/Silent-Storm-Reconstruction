#ifndef __RPGPERKCONSTANTS_H_
#define __RPGPERKCONSTANTS_H_
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_PERK_CHEAP_ROTATE = 4;
const int N_PERK_LESS_INFLUENCE_OF_WOUNDS_FOR_TOHIT = 5;
const int N_PERK_FAST_WEAPON_ADAPTATION = 6;
const int N_PERK_CHEAP_CHANGE_POSE = 7;
const int N_PERK_CHEAP_SHORT_BURST = 10;
const int N_PERK_BEST_SHOOTING_POSE = 12;
const int N_PERK_CHEAP_SNAP_SHOT = 15;
const int N_PERK_LONGER_SHORT_BURST = 16;
const int N_PERK_LONG_BURST_AUTO_STOP = 18;
const int N_PERK_CHEAP_SHOOT_PREPARE = 22;
const int N_PERK_NO_FIRST_TURN_PENALTY = 23;
const int N_PERK_BETTER_SPOTTING = 25;
const int N_PERK_CHEAP_AIMED_SHOT = 31;
const int N_PERK_CHEAP_MELEE = 32;
const int N_PERK_CHEAP_RELOAD = 33;
const int N_PERK_PRONE_COSTS_CROUCH = 34;
const int N_PERK_CROUCH_COSTS_WALK = 35;
const int N_PERK_EXPLOSION_EVASION = 39;
const int N_PERK_BETTER_EVASION = 40;
const int N_PERK_CORPSE_TRACKER = 42;
const int N_PERK_CHEAP_THROW = 43;
const int N_PERK_BETTER_CRIT_DIFFICULTY = 44;   // "Better critical difficulty" (Param1=1.25 scales nCrticalDifficulty)
const int N_PERK_CRITICAL_RESISTANCE = 46;
const int N_PERK_REDUCED_CRITICAL_SEVERITY = 47;
const int N_PERK_DIFFICULT_TRAPS = 49;
const int N_PERK_PERFECT_ENGINEERING = 50;
const int N_PERK_DISARM_TRAPS = 51;
const int N_PERK_FASTER_TARGETING = 73;
const int N_PERK_HIDE_IN_NIGHT = 75;
const int N_PERK_ZEROING_BONUS = 85;
// CreateAttack perk gates (retail @0x6c2100; ids + params verified against the retail RPGPerks table)
const int N_PERK_RANGED_DMG_ADD = 26;           // "+ N Ranged Dmg" (Param1=5, flat add to both damage bounds)
const int N_PERK_BETTER_CRIT_CHANCE = 36;       // "Better critical chance" (Param1=0.1 -> nCrtical x1.1)
const int N_PERK_ALWAYS_MELEE_CRITICAL = 37;    // "Always melee critical" (nCrtical=100)
const int N_PERK_ALWAYS_CRITICAL = 38;          // "Always critical" (ranged nCrtical=100 when damage lands)
const int N_PERK_RAGE = 45;                     // "Rage" (Param1=0.5 VP-ratio threshold, Param2=1.5 damage mult)
const int N_PERK_MASTER_SNIPER = 54;            // "Master Sniper" (Param1=0.2 -> crit x1.2, Param2=1.5 critdiff mult)
const int N_PERK_MELEE_DMG_ADD = 55;            // "+ N Melee Dmg" (Param1=10, flat add to both damage bounds)
const int N_PERK_SLOW_ADAPTATION_BONUS = 71;    // "Slow Adaptation bonus" (Param1=2 cap mult, Param2=2 rate divisor)
const int N_PERK_UNAWARE_CRITICAL = 74;         // "Unaware critical difficulty" (Param1=0.25 -> crit x1.25 capped 100)
const int N_PERK_ADAPTATION_BONUS = 87;         // "Adaptation bonus" (Param1=1.5 cap mult)
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __RPGPERKCONSTANTS_H_
