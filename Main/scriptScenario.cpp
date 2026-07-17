#include "stdafx.h"
//
#include "A5Script.h"
#include "scriptCommon.h"
#include "..\Misc\RandomGen.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "wUICommands.h"
#include "rpgGlobal.h"
#include "..\MiscDll\LogStream.h"		// csSystem (ScenarioSetGoal/TaskComplete warning)
#include "wMain.h"        // NWorld::CWorld::GetGame (SetMaxCriticalSeverity)
#include "RPGGame.h"      // NRPG::IGame::SetMaxCriticalSeverity
#include "aiCommander.h"  // NAI::CAICommander (LeaveToSubZone: skip AI players)
#include "aiMisc.h"       // NAI::IsAIPlayer (retail @0x73df0 -- the human/AI probe LeaveToSubZone uses)
#include "wUnitServer.h"  // NWorld::CUnitServer (LeaveToSubZone)
#include "iObjectivesMenu.h"  // NGame::CICObjectives (ShowObjectives)
#include "iShowObjectives.h"  // NGame::CICShowObjectives -- the retail framed objectives modal
//
#include "scriptScenario.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaSetMaxCriticalSeverity @0x2ee0e0 ("n"): set the per-mission critical-DC cap. World->GetRPGGame
// (retail vtbl+0x1c) then game->SetMaxCriticalSeverity (vtbl+0x48; the setter clamps n<1 -> 300). The cap is
// honored in CUnitMission::ApplyCritical: a rolled combat critical's severity is clamped down to it.
BEGIN_SCRIPT_COMMAND( SetMaxCriticalSeverity, "n" )
	int n = luaParams[ 0 ].n;
	NRPG::IGame *pGame = pScript->pWorld->GetGame();
	if ( IsValid( pGame ) )
		pGame->SetMaxCriticalSeverity( n );
	else
		csSystem << CC_RED << "Script warning: " << CC_GREY << "SetMaxCriticalSeverity: no RPG game" << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ScenarioGiveClue, "sb[true]b[false]" )
	string szClueName = luaParams[ 0 ].s;
	bool bTake = luaParams[ 1 ].b;
	bool bImmediately = luaParams[ 2 ].b;
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
	{
		CPtr<NScenario::CScenarioClue> pClue = pTracker->GetClueByName( szClueName );
		if ( IsValid( pClue ) )
		{
			if ( bTake )
				pTracker->CheatTakeClue( pClue, bImmediately );
			else
				pTracker->CheatDestroyClue( pClue, bImmediately );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2edac0 ("n"): append a script goal (DB ScenarioGoals id) to the global game's current zone.
BEGIN_SCRIPT_COMMAND( ScenarioAddGoal, "n" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	NRPG::CGlobalGame *pGG = pScript->GetGlobalGame();
	if ( IsValid( pTracker ) && IsValid( pGG ) )
		pTracker->AddScriptGoal( pGG->pCurrentZone, luaParams[ 0 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2edbd0 ("nb[true]"): set a script goal's completion state in the current zone; warn on failure.
// NOTE (build-validation scope): goals are STORED + queryable, but the dev has no objectives-journal UI to
// DISPLAY them -- so this currently has no player-visible effect. The display is a separate absent subsystem.
BEGIN_SCRIPT_COMMAND( ScenarioSetGoalComplete, "nb[true]" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	NRPG::CGlobalGame *pGG = pScript->GetGlobalGame();
	bool bDone = false;
	if ( IsValid( pTracker ) && IsValid( pGG ) )
		bDone = pTracker->ScriptGoalSetComplete( pGG->pCurrentZone, luaParams[ 0 ].n, luaParams[ 1 ].b );
	if ( !bDone )
		csSystem << CC_RED << "Script warning: " << "Could not complete goal " << luaParams[ 0 ].n << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2edd40 ("nnb[true]"): set a script goal's task[idx] completion state; warn on failure. (Same
// display caveat as ScenarioSetGoalComplete.) The warning prints the task index then the goal id (retail order).
BEGIN_SCRIPT_COMMAND( ScenarioSetTaskComplete, "nnb[true]" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	NRPG::CGlobalGame *pGG = pScript->GetGlobalGame();
	bool bDone = false;
	if ( IsValid( pTracker ) && IsValid( pGG ) )
		bDone = pTracker->ScriptTaskSetComplete( pGG->pCurrentZone, luaParams[ 0 ].n, luaParams[ 1 ].n, luaParams[ 2 ].b );
	if ( !bDone )
		csSystem << CC_RED << "Script warning: " << "Could not complete task " << luaParams[ 1 ].n << ", GoalID=" << luaParams[ 0 ].n << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// Open the OBJECTIVES/JOURNAL modal that renders the goals/tasks stored by ScenarioAddGoal /
// SetGoalComplete / SetTaskComplete (otherwise invisible). NMainLoop::Command is deferred (queued, not
// run inline), so pushing the CIC from the script step is safe.
BEGIN_SCRIPT_COMMAND( ShowObjectives, "" )
	NRPG::CGlobalGame *pGG = pScript->GetGlobalGame();
	if ( IsValid( pGG ) )
		// retail framed objectives modal (CGlobalGame has no mission accessor -> null mission; the rows render
		// from the goal/task strings and the backdrop is a fresh B&W capture of the current frame). Exec no-ops
		// when pCurrentZone is null (off-zone), matching retail GetGoalsFromZone's bail on a null current zone.
		NMainLoop::Command( new NGame::CICShowObjectives( 0, pGG->pCurrentZone ) );
	else
		csSystem << CC_RED << "Script warning: " << CC_GREY << "ShowObjectives: no global game" << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ScenarioOpenZone, "s" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
	{
		string szZoneName = luaParams[ 0 ].s;
		CPtr<NScenario::CScenarioZone> pZone = pTracker->GetZoneByName( szZoneName );
		if ( IsValid( pZone ) )
			pTracker->OpenZone( pZone );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ScenarioBlockZone, "s" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
	{
		string szZoneName = luaParams[ 0 ].s;
		CPtr<NScenario::CScenarioZone> pZone = pTracker->GetZoneByName( szZoneName );
		if ( IsValid( pZone ) )
			pTracker->BlockZone( pZone );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ExitToChapter, "" )
	pScript->AddUICommand( new NWorld::CUICmdContinueChapter() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ClueShow, "s" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
	{
		CPtr<NScenario::CScenarioClue> pClue = pTracker->GetClueByName( luaParams[ 0 ].s );
		if ( IsValid( pClue ) )
			pScript->AddUICommand( new NWorld::CUICmdShowClue( pClue ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ClueIsFound, "s" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
	{
		CPtr<NScenario::CScenarioClue> pClue = pTracker->GetClueByName( luaParams[ 0 ].s );
		if ( IsValid( pClue ) && pTracker->IsClueFound( pClue ) )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetCurrentZoneAILevel, "" )
	CPtr<NScenario::CScenarioZone> pZone = pScript->GetGlobalGame()->pCurrentZone;
	if ( IsValid( pZone ) )
		pScript->PushNumber( pZone->GetDifficulty() );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ed5e0: push the active scenario id, or nil when there is no live tracker.
BEGIN_SCRIPT_COMMAND( GetScenarioNumber, "" )
	CPtr<NScenario::CScenarioTracker> pTracker = pScript->GetScenarioTracker();
	if ( IsValid( pTracker ) )
		pScript->PushNumber( pTracker->GetScenarioID() );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eded0: queue a "begin zone" command carrying the destination zone name; the mission
// resolves the named scenario zone and posts a CICBeginMission when it processes the command.
BEGIN_SCRIPT_COMMAND( BeginZone, "s" )
	pScript->AddUICommand( new NWorld::CUICmdBeginZone( luaParams[ 0 ].s ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2edfd0: queue a tutorial-mode toggle; the mission sets CMission::bTutorialMode when it processes
// it (the ShowHint dispatch gate reads it -- in tutorial mode hints always show, even with the option off).
BEGIN_SCRIPT_COMMAND( SetTutorialMode, "b" )
	pScript->AddUICommand( new NWorld::CUICmdTutorialMode( luaParams[ 0 ].b ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaLeaveToSubZone @0x2ed740 ("n"): trigger the in-mission transition to passage-zone id n. Retail
// picks the first non-AI player's first live unit and calls IWorld vtbl+0x12c (CWorld::UsePassageObject):
// n<1 -> exit to the chapter map (CUICmdContinueChapter); else match the template carrying passage-zone n and
// queue a CUICmdLoadTemplate. The dev already has the full worker (CWorld::UsePassageObject, wMain.cpp:2272),
// so this binding just resolves a controllable unit to drive it.
BEGIN_SCRIPT_COMMAND( LeaveToSubZone, "n" )
	int nPassageZoneID = luaParams[ 0 ].n;
	NWorld::CWorld *pWorld = pScript->pWorld;
	if ( IsValid( pWorld ) )
	{
		for ( NWorld::IPlayer *pIPlayer = pWorld->GetNextPlayerForScript( 0 );
			  pIPlayer != 0; pIPlayer = pWorld->GetNextPlayerForScript( pIPlayer ) )
		{
			// skip AI-commanded players -- the passage is used by the human/script player (retail
			// luaLeaveToSubZone @0x2ed740 skips via NAI::IsAIPlayer @0x73df0; the human's retail
			// CSequenceCommander IS a CAICommander, so the raw cast would wrongly skip him)
			if ( NAI::IsAIPlayer( pIPlayer ) )
				continue;
			CDynamicCast<NWorld::CPlayer> pPlayer( pIPlayer );
			if ( !pPlayer )
				continue;
			vector< CPtr<NWorld::CUnitServer> > units;
			pPlayer->GetUnits( &units );
			for ( int i = 0; i < units.size(); ++i )
			{
				if ( IsValid( units[ i ] ) )
				{
					pWorld->UsePassageObject( units[ i ], nPassageZoneID );
					return 0;
				}
			}
		}
	}
	csSystem << CC_RED << "LeaveToSubZone: no appropriate player found or no alive units" << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaSetFirstMissionMode @0x2ee200 ("b"): enter/exit "first mission" HUD mode. The mission closes the
// medals/biography panels now and latches CMission::bSpecialFirstMissionMode (future SetPanelState keeps them off).
BEGIN_SCRIPT_COMMAND( SetFirstMissionMode, "b" )
	pScript->AddUICommand( new NWorld::CUICmdFirstMissionMode( luaParams[ 0 ].b ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaEnableFeature @0x2ee310 ("s"): enable a named mission feature. Only "reenter" is defined ->
// queues CUICmdEnableFeature(0) -> CMission::bEnableFeatureReenter = true (allow re-entering this zone).
BEGIN_SCRIPT_COMMAND( EnableFeature, "s" )
	if ( string( luaParams[ 0 ].s ) == "reenter" )
		pScript->AddUICommand( new NWorld::CUICmdEnableFeature( 0 ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}