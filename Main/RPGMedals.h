#ifndef __RPGMEDALS_H_
#define __RPGMEDALS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// RPGMedals -- medal-progress point scoring (release-new RPGMedals.obj compiland).
//
// NRPG::GetMedalPoints maps a single combat/utility event (EMedalPointCases) to the
// point value credited toward a unit's medal progress (see NRPG::CMedalsGainer in
// RPGUnit.h). It is a pure lookup: thirteen cases return a fixed constant, five
// "scaled" cases multiply the caller-supplied magnitude by a fixed factor, and the
// out-of-range default returns 0. Every recognised case also emits one coloured
// console line to the global csSystem naming the event.
//
// Currently a leaf: its natural callers (CMedalsGainer::AddMedalPoints and the
// rpgAttackSession subsystem) are deferred pending NDb::CSide::medals, so nothing
// invokes this yet. Landed for release parity of the RPGMedals compiland surface.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Event kinds scored by GetMedalPoints. Order is load-bearing: these are the raw
// switch indices 0..18 of the release jump table (RPGMedals.obj @VA 0x6ac1e0).
enum EMedalPointCases
{
	MPC_ATTACK_ENEMY,					// 0
	MPC_ATTACK_ENEMY_IN_PK,				// 1
	MPC_HIT_ENEMY,						// 2
	MPC_HIT_PK,							// 3
	MPC_HIT_ENEMY_IN_PK,				// 4
	MPC_HIT_ALLY,						// 5
	MPC_CRITICAL_HIT_ENEMY_IN_PK,		// 6
	MPC_CRITICAL_HIT_ALLY,				// 7
	MPC_KILLED_ENEMY,					// 8
	MPC_DISABLED_ENEMY_PK,				// 9
	MPC_KILLED_ENEMY_NOT_DISABLED_PK,	// 10
	MPC_KILLED_ALLY,					// 11
	MPC_DISABLED_ALLY_PK,				// 12
	MPC_HEALED_CRITICAL,				// 13  scaled
	MPC_HEALED_WOUND,					// 14  scaled
	MPC_PICK_LOCK,						// 15  scaled
	MPC_DISARM_TRAP,					// 16  scaled
	MPC_NOTICE_TRAP,					// 17  scaled
	MPC_CLUE_GAINED						// 18
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Returns the medal-progress points for one event. fAmount is the caller magnitude,
// used ONLY by the scaled cases (13..17); every constant case ignores it.
float GetMedalPoints( EMedalPointCases eCase, float fAmount );
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RPGMEDALS_H_
