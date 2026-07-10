#include "StdAfx.h"
#include "wUICommands.h"
#include "..\DBFormat\DataSound.h"		// complete NDb::CSound for CUICmdPlaySound's CDBPtr saveload factory
//
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Every CUICmd gets a unique, monotonically increasing id at construction (release: file-scope counter).
// The CScript id-queue tracks queued ids so lua WaitForUI(id)/IsUIActionIDPresent(id) can wait on a command.
static int g_nUniqCmdID = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmd::CUICmd()
{
	nID = ++g_nUniqCmdID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmd::CUICmd( int /*nPriority*/ )	// priority arg ignored (legacy non-camera subclasses); see header
{
	nID = ++g_nUniqCmdID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0xB1011160, CUICmd )
REGISTER_SAVELOAD_CLASS( 0xB1011161, CUICmdTurn )
REGISTER_SAVELOAD_CLASS( 0xB1011162, CUICmdUnit )
REGISTER_SAVELOAD_CLASS( 0x02973150, CUICmdAIUnitWillMove )		// AI "unit will move" hint (release id, $E93 @0x4a94c0)
REGISTER_SAVELOAD_CLASS( 0x51402130, CUICmdMoveCamera )
REGISTER_SAVELOAD_CLASS( 0x53102130, CUICmdPlayDialog )
REGISTER_SAVELOAD_CLASS( 0x50412160, CUICmdContinueChapter )
REGISTER_SAVELOAD_CLASS( 0x51312180, CUICmdLoadTemplate )
REGISTER_SAVELOAD_CLASS( 0xB1122080, CUICmdShowStore )
REGISTER_SAVELOAD_CLASS( 0xB1122081, CUICmdShowTeamMng )
REGISTER_SAVELOAD_CLASS( 0x52022180, CUICmdPlayAck )
REGISTER_SAVELOAD_CLASS( 0x52022200, CUICmdSetFloor )
REGISTER_SAVELOAD_CLASS( 0x52622200, CUICmdShowClue )
REGISTER_SAVELOAD_CLASS( 0x53115170, CUICmdPartFinished )
REGISTER_SAVELOAD_CLASS( 0x53115171, CUICmdBeginSequence )
REGISTER_SAVELOAD_CLASS( 0x53115172, CUICmdEndSequence )
REGISTER_SAVELOAD_CLASS( 0x53115173, CUICmdPause )			// LUA convergence PART B
REGISTER_SAVELOAD_CLASS( 0xA0623200, CUICmdLockCamera )		// LUA convergence PART B (release id; 0xA0623201 = ClipDistance)
REGISTER_SAVELOAD_CLASS( 0x53115174, CUICmdBeginZone )		// LUA convergence PART B (fresh id)
REGISTER_SAVELOAD_CLASS( 0x53115175, CUICmdPlaySound )		// LUA convergence PART B (fresh id)
REGISTER_SAVELOAD_CLASS( 0x53115176, CUICmdPlayEffect )		// LUA convergence PART B (fresh id)
REGISTER_SAVELOAD_CLASS( 0x53115177, CUICmdSetAmbient )		// LUA convergence PART B (fresh id)
REGISTER_SAVELOAD_CLASS( 0xB3122180, CUICmdBeginFade )		// LUA convergence PART B (release id)
REGISTER_SAVELOAD_CLASS( 0x53115178, CUICmdEndFade )		// LUA convergence PART B (fresh id)
REGISTER_SAVELOAD_CLASS( 0xB3523170, CUICmdLoseDialog )		// LUA convergence PART B (release id)
REGISTER_SAVELOAD_CLASS( 0xB3130120, CUICmdLeaveZoneDlg )	// LUA convergence PART B (release id)
REGISTER_SAVELOAD_CLASS( 0xB3212180, CUICmdShowHint )		// LUA convergence (release id; hint machinery)
REGISTER_SAVELOAD_CLASS( 0xB3327132, CUICmdTutorialMode )	// LUA convergence (release id; hint machinery)
REGISTER_SAVELOAD_CLASS( 0xB3621120, CUICmdLeaveZoneMode )	// LUA convergence (release id; SetLeaveZoneMode)
REGISTER_SAVELOAD_CLASS( 0xB3327130, CUICmdPlayVideo )		// LUA convergence (release id; PlayVideo)
REGISTER_SAVELOAD_CLASS( 0xA1023140, CUICmdSetAmbientEffect )	// LUA convergence (release id)
REGISTER_SAVELOAD_CLASS( 0x53115179, CUICmdFirstMissionMode )	// LUA convergence (fresh id; SetFirstMissionMode)
REGISTER_SAVELOAD_CLASS( 0x5311517A, CUICmdEnableFeature )		// LUA convergence (fresh id; EnableFeature reenter)
// release-new camera commands. NOTE: the release reuses 0xB1011161 (dev CUICmdTurn) for CUICmdCameraLocator
// and 0x51402130 (dev CUICmdMoveCamera) for CUICmdScriptMoveCamera. We keep CUICmdTurn/CUICmdMoveCamera (the
// release removed them), so these two carry FRESH non-colliding ids; their layout + operator& are byte-exact.
// CUICmdSetCameraClipDistance uses its real release id (0xA0623201, unused in dev).
REGISTER_SAVELOAD_CLASS( 0xB1011163, CUICmdCameraLocator )
REGISTER_SAVELOAD_CLASS( 0x51402133, CUICmdScriptMoveCamera )
REGISTER_SAVELOAD_CLASS( 0x53115181, CUICmdUnitCamera )		// fresh id: retail 0xB1011162 = kept dev CUICmdUnit; layout/operator& byte-exact
REGISTER_SAVELOAD_CLASS( 0xA0623201, CUICmdSetCameraClipDistance )