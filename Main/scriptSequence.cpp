#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wInterface.h"
#include "wUICommands.h"
#include "wUnitServer.h"
#include "..\DBFormat\DataCamera.h"
#include "..\DBFormat\DataSound.h"		// NDb::GetSound (PlaySound)
#include "..\DBFormat\DataFormat.h"		// NDb::GetTEffect (PlayEffect)
#include "..\DBFormat\DataLight.h"		// NDb::GetTAmbientLight (SetupAmbientLight)
#include "scriptPtr.h"
#include "scriptPosition.h"				// NScript::CLUAObjectPosition (Play3DSound/PlayEffect)
#include "rpgCheatConstants.h"
#include "rpgUnitMission.h"
#include "rpgUnit.h"
#include "rpgGlobal.h"					// NRPG::CGlobalGame::hintsSet (ShowHint dedup)
#include "wAnimation.h"
#include "..\MiscDll\LogStream.h"
#include "aiCommander.h"
#include "aiUnit.h"				// IAIUnit::De/ActivateCurrentControl (sequence route suspend/resume)
#include "aiMap.h"						// NAI::IAIMap::Sync (SlowSyncAIMap)
//
#include "scriptSequence.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NESTED-SEQUENCE DEPTH GUARD: the WORLD edge now nests through the ownerless-interrupt STACK itself,
// exactly like retail (StartSequence @0x375dd0 pushes one interrupt per c_BeginSequence, luaEndSequence
// @0x2f1a60 pops one via EndOfTurn @0x3776f0; IsSequence stays true until the last pop) -- the script
// commands below call those edges PER CALL, no guard. This guard now covers ONLY the per-unit block:
// the dev-only cheat/idle/cancel lines (CHEAT_SCRIPTSEQUENCE does not exist in retail Game.exe -- byte
// scan MISSING; it is the dev analog of retail's world-IsSequence probes) and the AI notifies. Retail
// runs GetAIUnits+OnSequenceStarted/Finished per CALL, but the dev CAIUnit carries extra AIM_SCRIPT
// control-stack handling inside those notifies, and running THAT per nested Begin/End (Common.l
// DialogPlayWithSequence, EBase's pkrepair OnOpenObject handler) CCmdCancel'ed the outer cutscene's
// live script routes -- a documented divergence until the control stack itself is converged.
// The depth is per-world (the weak CPtr detects a destroyed/replaced world and resets).
static CPtr<NWorld::CWorld> s_pSequenceWorld;
static int s_nSequenceDepth = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetSequenceCheat( NWorld::CWorld *pWorld, bool bOn )
{
	if ( !IsValid( s_pSequenceWorld ) || s_pSequenceWorld.GetPtr() != pWorld )
	{
		s_pSequenceWorld = pWorld;
		s_nSequenceDepth = 0;
	}
	if ( bOn )
	{
		if ( s_nSequenceDepth++ > 0 )
			return;                    // nested Begin: the outer sequence already holds the world
	}
	else
	{
		if ( s_nSequenceDepth > 0 )
			--s_nSequenceDepth;
		if ( s_nSequenceDepth > 0 )
			return;                    // nested End: the outer sequence is still running
	}
	// (the world edge -- StartSequence push / EndOfTurn pop -- rides PER CALL in the script commands
	// below, retail-1:1; this function is now only the depth-guarded per-unit block)
	vector< CPtr<NWorld::CUnit> > units;
	pWorld->GetAllUnits( &units );
	for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		CDynamicCast<NWorld::CUnitServer> pUS(*i);
		if (pUS)
		{
			pUS->GetUnitRPG()->GetRPGUnit()->SetCheat( NRPG::CHEAT_SCRIPTSEQUENCE, bOn );
			pUS->animator.SetBreathOnlyIdle( bOn );
			if ( bOn )
				pUS->OnTBSEvent( NWorld::TBS_CANCEL_ACTION );
			// retail luac_BeginSequence @0x2f1890 / EndSequence @0x2f1a60 run a per-unit AI notify
			// (GetAIUnits + CallAIFunc): CAIUnit::OnSequenceStarted @0xadec0 drops the combat logic,
			// SUSPENDS the unit's normal AI route (map guard/patrol/roaming logic -- without this the
			// bank robbers left the bank on their own before the camera got there) and clears any stale
			// sequence route; OnSequenceFinished @0xad3f0 drops the logic, ENDS the sequence's script
			// routes (a retail sequence route never outlives its sequence: routes[1] is deactivated,
			// never resumed) and RESUMES the unit's normal route (routes[0]->Activate). Script
			// AIM_SCRIPT controls assigned DURING the sequence stack on top and activate normally, so
			// scripted walks are unaffected. (The earlier De/ActivateCurrentControl pair mishandled the
			// End side: it re-woke whatever leftover SCRIPT control sat on top instead of the map logic.)
			CDynamicCast<NAI::CAICommander> pUnitCommander( pUS->GetPlayer()->GetCommander() );
			NAI::IAIUnit *pAIUnit = pUnitCommander ? pUnitCommander->GetAIUnit( pUS ) : 0;
			if ( IsValid( pAIUnit ) )
			{
				if ( bOn )
					pAIUnit->OnSequenceStarted();
				else
					pAIUnit->OnSequenceFinished();
			}
		}
	}
	// AI-convergence Stage 2: the tactical commander's DismissAllUnits (clear the combat rosters when a
	// sequence begins) is re-homed to the per-unit SetLogic(0) sweep above -- CAIUnit::OnSequenceStarted
	// (called for every unit in the bOn loop) already drops each unit's combat logic + suspends its route.
	// There is no per-player combat roster to clear anymore, so the extra pass is gone.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f1890 ("b[false]" -- NOT b[true]; the arg is the letterbox's skip-fade flag, Common.l
// DelayGameStart passes BeginSequence(true)): sequence-cheat all units, force real time, then queue the
// command WITH an id and return it -- lua BeginSequence() = WaitForUI(c_BeginSequence(b)) blocks until
// the letterbox fade-in completes (movieUI BorderShow @0x20e210 posts CCmdInterfaceEvent(id)).
BEGIN_SCRIPT_COMMAND( c_BeginSequence, "b[false]" );
	// retail @0x2f1890 order (decomp-proven): per-unit OnSequenceStarted notifies FIRST (so SetLogic
	// still works -- the world is not yet in sequence mode), THEN CTBSWorld::StartSequence (push the
	// ownerless interrupt; one per Begin, nesting stacks), THEN queue the letterbox command.
	SetSequenceCheat( pScript->pWorld, true );
	pScript->pWorld->StartSequence();
	return pScript->AddUICommandWithID( new NWorld::CUICmdBeginSequence( luaParams[ 0 ].b ) );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( EndSequencePart, "" );
	pScript->AddUICommand( new NWorld::CUICmdPartFinished() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f1a60 ("b[false]b[false]"): arg0 = bRestoreCamera (pop the BeginSequence pose back;
// false = the cutscene-end pose BECOMES the gameplay camera), arg1 = bSkipFade (immediate letterbox
// teardown). Queues PartFinished (plain) + the EndSequence command WITH an id and returns it -- lua
// StartGame() = WaitForUI(EndSequence()) blocks until the letterbox fade-out completes (BorderHide).
BEGIN_SCRIPT_COMMAND( EndSequence, "b[false]b[false]" );
	// retail luaEndSequence @0x2f1a60 order (decomp-proven): EndOfTurn FIRST (pop the ownerless top --
	// the !IsRealTime() branch re-fires OnPassControl to the RESUMED owned turn; an emptied stack
	// re-evaluates via StartNextPlayerTurn), then queue PartFinished + the EndSequence command, then
	// UpdateVisible, then per-unit OnSequenceFinished LAST (the world is out of sequence mode by the
	// time the notifies run, so SetLogic works -- retail's exact edge order).
	pScript->pWorld->EndOfTurn();
	pScript->AddUICommand( new NWorld::CUICmdPartFinished() );
	int nRet = pScript->AddUICommandWithID( new NWorld::CUICmdEndSequence( luaParams[ 0 ].b, luaParams[ 1 ].b ) );
	pScript->pWorld->UpdateVisible();
	SetSequenceCheat( pScript->pWorld, false );
	return nRet;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsUIActionIDPresent( id ): replaces the dev's type-based IsInterfaceAction. lua WaitForUI(id) (Common.l)
// polls this until the queued command (camera/dialog/sequence) finishes and its id is removed.
BEGIN_SCRIPT_COMMAND( IsUIActionIDPresent, "n" );
	if ( pScript->IsUIActionIDPresent( luaParams[ 0 ].n ) )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetCamera, "n" );
	luaPushCDBPtr<NDb::CDBCamera>( pState, NDb::GetDBCamera( luaParams[ 0 ].n ) );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// CameraMove( camera, time ): release migrated this to the id-queue. We keep the dev's single-target
// CUICmdMoveCamera (the release's CUICmdScriptMoveCamera is reserved for CameraSequence/the menu), but now
// queue it via AddUICommandWithID so it returns an id and lua CameraSet = WaitForUI(CameraMove(c,0)) waits
// correctly (the mission camera executor's Finished posts CCmdInterfaceEvent(id) -> RemoveUIActionID).
BEGIN_SCRIPT_COMMAND( CameraMove, "un" );
	CDBPtr<NDb::CDBCamera> pDBCamera = luaGetDBPtr<NDb::CDBCamera>( pScript->GetObject( 1 ) );
	if ( !IsValid( pDBCamera ) )
		return 0;
	//
	STime transitionTime = ( STime )luaParams[ 1 ].n;
	ICamera::SCameraPos pos( pDBCamera->vAnchor,
		pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	int nMoveID = pScript->AddUICommandWithID( new NWorld::CUICmdMoveCamera( pos, transitionTime ) );
	return nMoveID;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// CameraSetClipping( near, far ): set the camera near/far clip planes (CUICmdSetCameraClipDistance). No id
// (plain AddUICommand) -- the menu world's CMainMenuInterface applies it; missions apply it via the executor.
BEGIN_SCRIPT_COMMAND( CameraSetClipping, "nn" );
	pScript->AddUICommand( new NWorld::CUICmdSetCameraClipDistance( luaParams[ 0 ].f, luaParams[ 1 ].f ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// CameraSequence( {camera ids}, time ): play the camera through a list of DB-camera waypoints over `time`.
// Returns the queued command id (AddUICommandWithID); lua WaitForUI(id) waits for the fly to finish.
BEGIN_SCRIPT_COMMAND( CameraSequence, "tn" );
	Script::Object oTable = pScript->GetObject( 1 );
	int nCount = oTable.GetTableSize();
	vector<ICamera::SCameraPos> positions;
	for ( int i = 1; i <= nCount; ++i )
	{
		int nCameraID;
		{
			Script::AutoBlock block( *pScript );	// restore the lua stack each iteration (tables can be large)
			nCameraID = oTable.GetByTableIndex( i ).GetInteger();
		}
		CDBPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( nCameraID );
		if ( IsValid( pDBCamera ) )
			positions.push_back( ICamera::SCameraPos( pDBCamera->vAnchor,
				pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV ) );
	}
	STime transitionTime = ( STime )luaParams[ 1 ].n;
	return pScript->AddUICommandWithID( new NWorld::CUICmdScriptMoveCamera( positions, transitionTime ) );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( uiShowStore, "" );
	pScript->AddUICommand( new NWorld::CUICmdShowStore() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( uiShowTeamMngMenu, "" );
	pScript->AddUICommand( new NWorld::CUICmdShowTeamMng() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( Floor, "n" );
	pScript->AddUICommand( new NWorld::CUICmdSetFloor( luaParams[ 0 ].n ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ef460 (EndSequence's last step): refresh every player's visibility now.
BEGIN_SCRIPT_COMMAND( UpdateVisible, "" )
	pScript->pWorld->UpdateVisible( true );   // retail luaUpdateVisible @0x2ef460: force the sweep NOW (bForce), not deferred
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f15f0: the script requests turn-based mode (CWorld::ScriptWantTurnBased -- pWorld vtbl+0x1f0).
BEGIN_SCRIPT_COMMAND( WantTurnBased, "b" )
	pScript->pWorld->ScriptWantTurnBased( luaParams[ 0 ].b );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2e97a0: a script-driven full ("slow"/normal, ST_NORMAL) re-sync of the AI map, recomputing the
// passability geometry now (GetAIMap() vtbl+0x20 -> IAIMap::Sync vtbl+0x10 with ST_NORMAL=1).
BEGIN_SCRIPT_COMMAND( SlowSyncAIMap, "" )
	pScript->pWorld->GetAIMap()->Sync( NAI::IAIMap::ST_NORMAL );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f1010: start the game NOW -- DelayGameStart(0) clears the start-freeze, so the
// CWorld::StartFirstSegments warm-up loop stops segmenting and the mission proceeds.
BEGIN_SCRIPT_COMMAND( c_StartGameEx, "" )
	pScript->pWorld->DelayGameStart( 0 );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0f30: DELAY the game start -- DelayGameStart(1) sets the start-freeze, so
// StartFirstSegments keeps segmenting (running scripts each tick) until a c_StartGameEx unfreezes it.
BEGIN_SCRIPT_COMMAND( c_DelayGameStartEx, "" )
	pScript->pWorld->DelayGameStart( 1 );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ef260: queue a no-param "pause" command; the mission toggles its pause state
// when it processes it (CMission::ExecWorldCommands).
BEGIN_SCRIPT_COMMAND( Pause, "" )
	pScript->AddUICommand( new NWorld::CUICmdPause() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f1c80: show a one-shot UI hint, de-duplicated against the global game's hintsSet. The hint modal
// (NGame::CICShowHint) is gated at dispatch time on the "ui_showhints" option / tutorial mode; here we only
// resolve the record, remember it (so a re-show is a no-op), and queue the command -- AddUICommandWithID so the
// lua WaitForUI(id) helper can wait on the hint being dismissed.
BEGIN_SCRIPT_COMMAND( ShowHint, "n" )
	NDb::CUIHint *pHint = NDb::GetUIHint( luaParams[ 0 ].n );
	if ( !IsValid( pHint ) )
		return 0;
	NRPG::CGlobalGame *pGlobalGame = pScript->GetGlobalGame();
	if ( !IsValid( pGlobalGame ) )
		return 0;
	// already shown this hint? -> do nothing
	for ( vector< CDBPtr<NDb::CUIHint> >::iterator i = pGlobalGame->hintsSet.begin(); i != pGlobalGame->hintsSet.end(); ++i )
		if ( (*i).GetPtr() == pHint )
			return 0;
	pGlobalGame->hintsSet.push_back( pHint );
	return pScript->AddUICommandWithID( new NWorld::CUICmdShowHint( pHint ) );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ee440 ("n"): advance the sequential-hint cursor past the next n hints. luaAddHints loops
// CWorld::AddNextUIHint(true) n times -- a SILENT advance (the bool suppresses the hint modal; the sole
// retail caller passes true). Each step records the consumed hint in the global game's hintsSet and, once
// the hint sequence is exhausted, sets bNoMoreHints. No modal is popped (that is ShowHint's job).
BEGIN_SCRIPT_COMMAND( AddHints, "n" )
	int nCount = luaParams[ 0 ].n;
	for ( int i = 0; i < nCount; ++i )
		pScript->pWorld->AddNextUIHint( true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0310: queue a "lock/unlock camera" command from the bool (default true); the mission
// freezes the camera in place (true) or releases it (false) when it processes the command.
BEGIN_SCRIPT_COMMAND( CameraLock, "b[true]" )
	pScript->AddUICommand( new NWorld::CUICmdLockCamera( luaParams[ 0 ].b ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f14e0 ("bn[-1]"): block/allow leaving the zone. arg0 = may-leave (false blocks it), arg1 =
// "can't leave" message DB id. The mission records it (CMission::bLeaveBlockedByScript / nLeaveBlockReason,
// read by CanLeaveZone). See the iMissionInternal.h note re: the absent in-mission exit consumer.
BEGIN_SCRIPT_COMMAND( SetLeaveZoneMode, "bn[-1]" )
	pScript->AddUICommand( new NWorld::CUICmdLeaveZoneMode( luaParams[ 0 ].b, luaParams[ 1 ].n ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0e00 ("s"): play the named .seq cut-scene. Queues a CUICmdPlayVideo whose mission dispatch
// installs the iIntroScreen sequence player (NGame::CICPlaySequence). Returns the wait-id (WaitForUI).
BEGIN_SCRIPT_COMMAND( PlayVideo, "s" )
	return pScript->AddUICommandWithID( new NWorld::CUICmdPlayVideo( luaParams[ 0 ].s ) );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f06d0: queue a 2D play-sound command for the DB sound id; nil on a bad id. The command is
// kept alive (AddMiscObject) so the sound keeps playing, and is RETURNED so the script can StopSound it.
BEGIN_SCRIPT_COMMAND( PlaySound, "n" )
	NDb::CSound *pSound = NDb::GetSound( luaParams[ 0 ].n );
	if ( !IsValid( pSound ) )
	{
		pScript->PushNil();
		return 1;
	}
	NWorld::CUICmdPlaySound *pCmd = new NWorld::CUICmdPlaySound( pSound, false );
	pScript->AddUICommand( pCmd );
	pScript->AddMiscObject( pCmd );
	luaPushCObj( pState, pCmd );		// return the command -> StopSound's argument
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0d00: stop a running play-sound command -- release its live channel handle, which deletes
// the channel and stops the FMOD playback. (The retail re-queues the command for its soundsList removal;
// the dev's command-owns-the-channel model needs no re-queue -- releasing pChannel is the stop.)
BEGIN_SCRIPT_COMMAND( StopSound, "u" )
	CDynamicCast<NWorld::CUICmdPlaySound> pCmd( luaParams[ 0 ].p );
	if ( pCmd )
		pCmd->pChannel = 0;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0540: queue a positional (3D) play-sound command from a sound id + a lua position object;
// nil when the sound or position is missing. Returns the command so the script can StopSound it.
BEGIN_SCRIPT_COMMAND( Play3DSound, "nu" )
	CDynamicCast<CLUAObjectPosition> pPos( luaParams[ 1 ].p );
	NDb::CSound *pSound = pPos ? NDb::GetSound( luaParams[ 0 ].n ) : 0;
	if ( !pPos || !IsValid( pSound ) )
	{
		pScript->PushNil();
		return 1;
	}
	NWorld::CUICmdPlaySound *pCmd = new NWorld::CUICmdPlaySound( pSound, true );
	pCmd->vPos = pPos->ptPos;
	pScript->AddUICommand( pCmd );
	pScript->AddMiscObject( pCmd );
	luaPushCObj( pState, pCmd );		// return the command -> StopSound's argument
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0a80: queue a play-effect command from an effect id + a lua position object; nil when either is
// missing. Returns the command so the script can StopEffect it (kept playing via AddMiscObject).
BEGIN_SCRIPT_COMMAND( PlayEffect, "nu" )
	CDynamicCast<CLUAObjectPosition> pPos( luaParams[ 1 ].p );
	NDb::CTEffect *pEffect = pPos ? NDb::GetTEffect( luaParams[ 0 ].n ) : 0;
	if ( !pPos || !IsValid( pEffect ) )
	{
		pScript->PushNil();
		return 1;
	}
	NWorld::CUICmdPlayEffect *pCmd = new NWorld::CUICmdPlayEffect( pEffect, pPos->ptPos );
	pScript->AddUICommand( pCmd );
	pScript->AddMiscObject( pCmd );
	luaPushCObj( pState, pCmd );		// return the command -> StopEffect's argument
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0c00: stop a running play-effect command -- release its live particle handle, removing the effect.
BEGIN_SCRIPT_COMMAND( StopEffect, "u" )
	CDynamicCast<NWorld::CUICmdPlayEffect> pCmd( luaParams[ 0 ].p );
	if ( pCmd )
		pCmd->pHandle = 0;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0830: queue a "set ambient light" command for the DB light id; silently skipped on a bad id.
BEGIN_SCRIPT_COMMAND( SetupAmbientLight, "n" )
	NDb::CTAmbientLight *pLight = NDb::GetTAmbientLight( luaParams[ 0 ].n );
	if ( IsValid( pLight ) )
		pScript->AddUICommand( new NWorld::CUICmdSetAmbient( pLight, false ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f0940: queue a "set ambient effect" command from the DB effect id. NO null gate on the record
// (unlike SetupAmbientLight) -- a bad id (the campaign passes -1) queues a null effect, which the mission
// dispatch treats as "clear the current ambient effect". 941 = the real effect the campaign sets.
BEGIN_SCRIPT_COMMAND( SetAmbientEffect, "n" )
	NDb::CTEffect *pEffect = NDb::GetTEffect( luaParams[ 0 ].n );
	pScript->AddUICommand( new NWorld::CUICmdSetAmbientEffect( pEffect ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ed970 ("n"): set the world time-of-day (arg0 = TOD sentinel 6=day/5=night/0=anytime), then
// queue a null-light CUICmdSetAmbient(immediate) -- the mission dispatch recomputes the current ambient.
// CWorld::SetTimeOfDay rewrites the createFlags sentinel + re-lights every object (UpdateLight). Returns 1
// (faithful to the .obj -- the binding pushes nothing, so the input value is what lua sees back).
BEGIN_SCRIPT_COMMAND( SetTimeOfDay, "n" )
	pScript->pWorld->SetTimeOfDay( (NWorld::ETimeOfDay)luaParams[ 0 ].n );
	NWorld::CUICmdSetAmbient *pCmd = new NWorld::CUICmdSetAmbient();
	pCmd->bImmediate = true;
	pScript->AddUICommand( pCmd );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ef670: start a screen fade to the RGB colour (params 0..2, read as floats) over the duration
// (param 3 ms, default 1000); returns the queued wait-id so lua WaitForUI(id) can block on it.
BEGIN_SCRIPT_COMMAND( FadeOut, "n[0]n[0]n[0]n[1000]" )
	CVec3 vColor( luaParams[ 0 ].f, luaParams[ 1 ].f, luaParams[ 2 ].f );
	return pScript->AddUICommandWithID( new NWorld::CUICmdBeginFade( vColor, luaParams[ 3 ].n ) );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ef540: end the running screen fade (fade back in); returns the queued wait-id.
BEGIN_SCRIPT_COMMAND( FadeIn, "" )
	return pScript->AddUICommandWithID( new NWorld::CUICmdEndFade() );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f13d0: queue the "mission lost" dialog command carrying a record id; the mission posts a
// CICLoseMenu. AddUICommandWithID assigns a wait-id but the binding discards it (returns 0).
BEGIN_SCRIPT_COMMAND( ShowLoseDialog, "n[-1]" )
	pScript->AddUICommandWithID( new NWorld::CUICmdLoseDialog( luaParams[ 0 ].n ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2effb0: queue the "leave zone" dialog command. (The retail also runs a world leave-zone
// pre-step, IWorld vtbl+0x1e4, which is absent in the dev tree -- omitted.) The wait-id is discarded.
BEGIN_SCRIPT_COMMAND( ShowLeaveZoneDialog, "n" )
	pScript->AddUICommandWithID( new NWorld::CUICmdLeaveZoneDlg( luaParams[ 0 ].n ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}