#ifndef __SCRIPTSCENARIO_H_
#define __SCRIPTSCENARIO_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( ScenarioGiveClue );
DECLARE_SCRIPT_COMMAND( ScenarioAddGoal );
DECLARE_SCRIPT_COMMAND( ScenarioSetGoalComplete );
DECLARE_SCRIPT_COMMAND( ScenarioSetTaskComplete );
DECLARE_SCRIPT_COMMAND( ShowObjectives );
DECLARE_SCRIPT_COMMAND( ScenarioOpenZone );
DECLARE_SCRIPT_COMMAND( ScenarioBlockZone );
DECLARE_SCRIPT_COMMAND( ExitToChapter );
DECLARE_SCRIPT_COMMAND( ClueShow );
DECLARE_SCRIPT_COMMAND( ClueIsFound );
DECLARE_SCRIPT_COMMAND( GetCurrentZoneAILevel );
DECLARE_SCRIPT_COMMAND( GetScenarioNumber );
DECLARE_SCRIPT_COMMAND( SetMaxCriticalSeverity );
// --- LUA convergence (PART B: CUICmd* family) ---
DECLARE_SCRIPT_COMMAND( BeginZone );
DECLARE_SCRIPT_COMMAND( SetTutorialMode );
// --- LUA convergence (zone re-entry / sub-zone transition) ---
DECLARE_SCRIPT_COMMAND( LeaveToSubZone );
DECLARE_SCRIPT_COMMAND( SetFirstMissionMode );
DECLARE_SCRIPT_COMMAND( EnableFeature );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTSCENARIO_H_