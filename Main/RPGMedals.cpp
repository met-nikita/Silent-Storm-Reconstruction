#include "StdAfx.h"
#include "RPGMedals.h"
#include "..\MiscDll\LogStream.h"   // CLogStream / csSystem / EConsoleColor (CC_RED=1, CC_GREEN=2)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetMedalPoints @ RPGMedals.obj (release VA 0x6ac1e0)
//
// switch(eCase) over the 19-entry jump table. Each recognised case logs one
// "<colour><label>" line to csSystem then returns its point value. The constant
// cases ignore fAmount; the five scaled cases (13/14/15/17 -> *1.1, 16 -> *1.2)
// multiply the caller magnitude by a fixed double factor. The original default
// branch reads an unresolved engine global (unreachable for any valid case id);
// faithfully reconstructed as 0.0f -- no value is fabricated and no line is logged.
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetMedalPoints( EMedalPointCases eCase, float fAmount )
{
	switch( eCase )
	{
	case MPC_ATTACK_ENEMY:
		csSystem << CC_GREEN << "Attacked enemy";
		return 1.0f;
	case MPC_ATTACK_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Attacked enemy in PK";
		return 2.0f;
	case MPC_HIT_ENEMY:
		csSystem << CC_GREEN << "Hit enemy";
		return 3.0f;
	case MPC_HIT_PK:
		csSystem << CC_GREEN << "Hit enemy PK";
		return 2.0f;
	case MPC_HIT_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Hit enemy in PK";
		return 5.0f;
	case MPC_HIT_ALLY:
		csSystem << CC_RED << "Hit ally";
		return -5.0f;
	case MPC_CRITICAL_HIT_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Critical hit enemy in PK";
		return 10.0f;
	case MPC_CRITICAL_HIT_ALLY:
		csSystem << CC_RED << "Critical hit ally enemy";
		return -15.0f;
	case MPC_KILLED_ENEMY:
		csSystem << CC_GREEN << "Killed enemy";
		return 15.0f;
	case MPC_DISABLED_ENEMY_PK:
		csSystem << CC_GREEN << "Disabled enemy PK";
		return 20.0f;
	case MPC_KILLED_ENEMY_NOT_DISABLED_PK:
		csSystem << CC_GREEN << "Killed enemy w/o disabling PK";
		return 40.0f;
	case MPC_KILLED_ALLY:
		csSystem << CC_RED << "Killed ally";
		return -50.0f;
	case MPC_DISABLED_ALLY_PK:
		csSystem << CC_RED << "Disabled ally PK";
		return -30.0f;
	case MPC_HEALED_CRITICAL:
		csSystem << CC_GREEN << "Healed critical";
		return (float)( fAmount * 1.1 );
	case MPC_HEALED_WOUND:
		csSystem << CC_GREEN << "Healed wounds";
		return (float)( fAmount * 1.1 );
	case MPC_PICK_LOCK:
		csSystem << CC_GREEN << "Picked a lock";
		return (float)( fAmount * 1.1 );
	case MPC_DISARM_TRAP:
		csSystem << CC_GREEN << "Disarmed trap";
		return (float)( fAmount * 1.2 );
	case MPC_NOTICE_TRAP:
		csSystem << CC_GREEN << "Noticed trap";
		return (float)( fAmount * 1.1 );
	case MPC_CLUE_GAINED:
		csSystem << CC_GREEN << "Gained clue";
		return 40.0f;
	default:
		return 0.0f;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
