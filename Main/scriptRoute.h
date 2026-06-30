#ifndef __SCRIPTROUTE_H_
#define __SCRIPTROUTE_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( CreateRoute );
DECLARE_SCRIPT_COMMAND( UnitSetRoute );
DECLARE_SCRIPT_COMMAND( GroupSetRoute );
DECLARE_SCRIPT_COMMAND( UnitMoveToWaypoint );
DECLARE_SCRIPT_COMMAND( UnitFlyToWaypoint );
DECLARE_SCRIPT_COMMAND( UnitSetToWaypoint );
DECLARE_SCRIPT_COMMAND( RecalcWaypointPos );
DECLARE_SCRIPT_COMMAND( UnitRoaming )
// --- LUA convergence (PART A: AI reaction logic setters) ---
DECLARE_SCRIPT_COMMAND( UnitSetNormalLogic );
DECLARE_SCRIPT_COMMAND( UnitSetGuardLogic );
DECLARE_SCRIPT_COMMAND( UnitSetRetreatLogic );
DECLARE_SCRIPT_COMMAND( UnitSetFearLogic );
DECLARE_SCRIPT_COMMAND( UnitSetPanicLogic );
DECLARE_SCRIPT_COMMAND( UnitSetCivilianLogic );
DECLARE_SCRIPT_COMMAND( UnitSetScriptLogic );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTROUTE_H_