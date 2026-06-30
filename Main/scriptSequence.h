#ifndef __SCRIPTSEQUENCE_H_
#define __SCRIPTSEQUENCE_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( c_BeginSequence );
DECLARE_SCRIPT_COMMAND( EndSequencePart );
DECLARE_SCRIPT_COMMAND( EndSequence );
DECLARE_SCRIPT_COMMAND( IsUIActionIDPresent );
DECLARE_SCRIPT_COMMAND( GetCamera );
DECLARE_SCRIPT_COMMAND( CameraMove );
DECLARE_SCRIPT_COMMAND( CameraSetClipping );
DECLARE_SCRIPT_COMMAND( CameraSequence );
DECLARE_SCRIPT_COMMAND( uiShowStore );
DECLARE_SCRIPT_COMMAND( uiShowTeamMngMenu );
DECLARE_SCRIPT_COMMAND( Floor );
// --- LUA convergence (PART A: cheap remainders) ---
DECLARE_SCRIPT_COMMAND( UpdateVisible );
DECLARE_SCRIPT_COMMAND( WantTurnBased );
DECLARE_SCRIPT_COMMAND( SlowSyncAIMap );
DECLARE_SCRIPT_COMMAND( c_StartGameEx );
DECLARE_SCRIPT_COMMAND( c_DelayGameStartEx );
DECLARE_SCRIPT_COMMAND( SetTimeOfDay );
// --- LUA convergence (PART B: CUICmd* family) ---
DECLARE_SCRIPT_COMMAND( Pause );
DECLARE_SCRIPT_COMMAND( CameraLock );
DECLARE_SCRIPT_COMMAND( SetLeaveZoneMode );
DECLARE_SCRIPT_COMMAND( PlayVideo );
DECLARE_SCRIPT_COMMAND( PlaySound );
DECLARE_SCRIPT_COMMAND( StopSound );
DECLARE_SCRIPT_COMMAND( Play3DSound );
DECLARE_SCRIPT_COMMAND( PlayEffect );
DECLARE_SCRIPT_COMMAND( StopEffect );
DECLARE_SCRIPT_COMMAND( SetupAmbientLight );
DECLARE_SCRIPT_COMMAND( SetAmbientEffect );
DECLARE_SCRIPT_COMMAND( FadeOut );
DECLARE_SCRIPT_COMMAND( FadeIn );
DECLARE_SCRIPT_COMMAND( ShowLoseDialog );
DECLARE_SCRIPT_COMMAND( ShowLeaveZoneDialog );
DECLARE_SCRIPT_COMMAND( ShowHint );
DECLARE_SCRIPT_COMMAND( AddHints );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTSEQUENCE_H_