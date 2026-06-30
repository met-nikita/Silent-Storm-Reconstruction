#include "StdAfx.h"
#include "iMain.h"             // NMainLoop::IInterfaceBase / CInterfaceCommand / Command / CanRender / GetTime
#include "G2DView.h"
#include "GSceneUtils.h"       // NGScene::ClearScreen / NGScene::Flip
#include "Interface.h"
#include "UIInterface.h"       // NUI::CInterface
#include "Cursor.h"            // NUI::ICursor
#include "UICommCtrls.h"       // NUI::CVideoPlayer
#include "wInterface.h"        // NWorld::CCmdInterfaceEvent (mission-event branch)
#include "iMission.h"          // NGame::IMission
#include "iMainMenu.h"         // sibling module (CICMainMenu) -- the "exec mainmenu" target
#include "iIntroScreen.h"      // NGame::PlayVideoSequence (external entry point for PlayVideo)
#include "..\Input\Bind.h"     // NInput::CBind / SEvent / SetSection
#include "..\MiscDll\Commands.h"   // NGlobal::ProcessCommand / RegisterCmd / RegisterVar / VarBoolHandler / CmdHandler / CValue
#include "..\MiscDll\LogStream.h"  // csSystem / endl
#include "..\Misc\StrProc.h"   // NStr::TrimLeft/TrimRight / ToUnicode / ToAscii / SplitString
#include "..\FileIO\Streams.h" // CFileStream / CMemoryStream
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  iIntroScreen  --  the .seq sequence player driving the boot intro
//  ( start.cfg -> "sequence .\cfg\intro.seq" -> play JoWooD/Nival/Intro.bik -> "exec mainmenu" ).
//  Reconstructed from the release module .\release\iIntroScreen.obj (absent from
//  this predecessor tree).  CSequence is a structural twin of CInterMissionInterface.
//  VA = RVA + 0x400000.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequence  --  the intro / cut-scene video-sequence screen (saveload id 0xB3320180).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSequence: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CSequence);
	//
	NInput::CBind bindClose;        // "cancel"   -> ExecCommand(true)  : finish
	NInput::CBind bindSkip;         // "skip"     -> ExecCommand(false) : advance
	NInput::CBind bindExitGame;     // "exitgame" -> fires its own command
	//
	int           nEventID;
	string        szFileName;
	CPtr<NGame::IMission> pMission;
	int           nCounter;
	vector<string> commandsSet;     // the script lines walked by ExecCommand
	ZDATA
	CObj<NUI::ICursor>      pCursor;
	CObj<NUI::CInterface>   pInterface;
	CObj<NUI::CVideoPlayer> pVideoPlayer;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pVideoPlayer); return 0; }

public:
	CSequence();

	void Initialize( NGame::IMission *pMission, int nEventID, const string &szFileName );  // @0x1ed190
	void ExecCommand( bool bForce );                                                       // @0x1ecc00
	void RenderFrame();                                                                    // @0x1ecbd0

	virtual void Step();                                       // @0x1ed010
	virtual void OnGetFocus();                                 // @0x1ecb90 (empty)
	virtual bool ProcessEvent( const NInput::SEvent &eEvent ); // @0x1ecf20
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICPlaySequence  --  the queued main-loop command that builds + installs a CSequence.
// Transient (a CInterfaceCommand, not save-registered -- like CICInterMission).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICPlaySequence: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICPlaySequence);
	int    nEventID;
	bool   bExclusive;
	string szFileName;
	CPtr<NGame::IMission> pMission;

public:
	CICPlaySequence() {}
	CICPlaySequence( NGame::IMission *_pMission, int _nEventID, const string &_szFileName, bool _bExclusive ):
		nEventID( _nEventID ), bExclusive( _bExclusive ), szFileName( _szFileName ), pMission( _pMission ) {}

	virtual void Exec();                                       // @0x1ed580
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
// the "game_skipvideo" config global (CSequence::ExecCommand gates the "play" verb on it)
static bool g_bSkipVideo = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequence
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CSequence::CSequence():
	bindClose( "cancel" ), bindSkip( "skip" ), bindExitGame( "exitgame" ),
	nEventID( 0 ), nCounter( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CSequence::OnGetFocus()
{
	// empty in the binary (a `ret`)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NGame::CSequence::ProcessEvent( const NInput::SEvent &eEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( eEvent );                  // cursor sees the event first (result ignored)

	if ( pInterface->ProcessEvent( eEvent ) )
		return true;

	if ( bindSkip.ProcessEvent( eEvent ) )            // skip -> advance to the next command
	{
		ExecCommand( false );
		return true;
	}
	if ( bindClose.ProcessEvent( eEvent ) )           // close -> finish the sequence
	{
		ExecCommand( true );
		return true;
	}
	return bindExitGame.ProcessEvent( eEvent );        // exitgame fires its own bound command
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CSequence::Step()
{
	// The video finished -> let the command script move on.
	if ( !pVideoPlayer->IsPlaying() )
		ExecCommand( false );

	MarkNewDGFrame();
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		NGScene::ClearScreen( CVec3( 0, 0, 0 ) );      // release ClearScreenZBuffer() (absent) -> black clear
		pInterface->Draw( GetTime() );
		NGScene::Flip();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CSequence::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0, 0, 0 ) );          // release ClearScreenZBuffer() (absent) -> black clear
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CSequence::Initialize( NGame::IMission *_pMission, int _nEventID, const string &szFile )
{
	nEventID = _nEventID;
	pMission = _pMission;

	pCursor      = NUI::ICursor::Create( false, CVec2( -1, -1 ) );
	pInterface   = new NUI::CInterface( pCursor );
	pVideoPlayer = new NUI::CVideoPlayer( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ),
	                                                        "", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE ) );

	// Read the script file -> commandsSet (one string per line) -- the NGlobal::LoadConfig idiom.
	CMemoryStream sStream;
	try
	{
		CFileStream sFile;
		sFile.OpenRead( szFile.c_str() );
		sStream.WriteFrom( sFile );
		sStream << '\0';
	}
	catch( ... )
	{
		csSystem << "Can't open " << szFile << endl;
	}
	NStr::SplitString( (const char*)sStream.GetBuffer(), commandsSet, '\n' );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ExecCommand @0x1ecc00 -- walk the script from the NEXT line, executing the first
// real "play"/"exec" command found.  A forced finish (bForce) and the global
// game_skipvideo both DISABLE the "play" branch (forced finish skips past videos
// while still honouring "exec").  List exhausted -> notify the mission + pop.
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CSequence::ExecCommand( bool bForce )
{
	++nCounter;
	while ( nCounter < (int)commandsSet.size() )
	{
		string szLine = commandsSet[nCounter];
		if ( !szLine.empty() )
		{
			NStr::TrimLeft( szLine );
			NStr::TrimRight( szLine );
			if ( szLine.substr( 0, 1 ) != ";" && szLine.substr( 0, 2 ) != "//" )
			{
				string::size_type nSpace = szLine.find_first_of( ' ', 0 );
				if ( nSpace != string::npos )
				{
					string szVerb = szLine.substr( 0, nSpace );
					string szArgs = szLine.substr( nSpace );
					NStr::TrimLeft( szVerb ); NStr::TrimRight( szVerb );
					NStr::TrimLeft( szArgs ); NStr::TrimRight( szArgs );

					if ( !bForce && !g_bSkipVideo && szVerb == "play" )
					{
						pVideoPlayer->Stop();
						pVideoPlayer->Set( szArgs, NUI::CVideoPlayer::PLAY_WITH_SOUND );
						pVideoPlayer->Play( false );
						return;                            // a play command consumes this call
					}
					if ( szVerb == "exec" )
					{
						NGlobal::ProcessCommand( NStr::ToUnicode( szArgs ) );
						return;                            // an exec command consumes this call
					}
					// otherwise: unknown verb -> keep scanning
				}
			}
		}
		++nCounter;
	}

	// List exhausted -> finish the sequence. nEventID IS the queued UI-action id (release CCmdInterfaceEvent
	// carries the id directly now); CWorld::ExecuteCommand -> RemoveUIActionID(nEventID) unblocks WaitForUI.
	if ( IsValid( pMission ) )                             // boot: pMission == 0 -> skipped
		pMission->Command( new NWorld::CCmdInterfaceEvent( nEventID ) );
	NMainLoop::Command( 0 );                               // pop this modal screen
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICPlaySequence
////////////////////////////////////////////////////////////////////////////////////////////////////
void NGame::CICPlaySequence::Exec()
{
	CSequence *pSeq = new CSequence();
	pSeq->Initialize( pMission, nEventID, szFileName );
	if ( bExclusive )
		SetInterface( pSeq );                              // REPLACE the interface stack
	else
		PushInterface( pSeq );                             // PUSH on top
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Entry point for callers outside this file (PlayVideo's mission dispatch) -- CICPlaySequence is
// file-local, so route through this free function. See iIntroScreen.h.
void NGame::PlayVideoSequence( NGame::IMission *pMission, int nEventID, const string &szFileName, bool bExclusive )
{
	NMainLoop::Command( new NGame::CICPlaySequence( pMission, nEventID, szFileName, bExclusive ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CmdPlaySequence -- the "sequence <seqfile>" console command handler.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdPlaySequence( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( !paramsSet.empty() )
	{
		csSystem << "Sequence... " << NStr::ToAscii( paramsSet[0] ) << endl;
		NMainLoop::Command( new NGame::CICPlaySequence( 0, 0, NStr::ToAscii( paramsSet[0] ), true ) );
	}
	else
	{
		csSystem << "usage: " << szID << " <seqfile>" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3320180, CSequence );
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1edd40  NGame::iIntroScreenInit::iIntroScreenInit -- the module static-init ctor.
// Wrapped in namespace NGame so START_REGISTER (BasicChunk1.h) emits NGame::iIntroScreenInit
// (release-faithful) rather than ::iIntroScreenInit. Unqualified CmdPlaySequence / g_bSkipVideo
// still resolve to their file-static definitions at global scope; NGlobal::* stays fully
// qualified. Pure namespace placement -- no body/argument change, no double-registration.
namespace NGame {
START_REGISTER(iIntroScreen)
	REGISTER_CMD( "sequence", CmdPlaySequence )
	REGISTER_VAR_EX( "game_skipvideo", NGlobal::VarBoolHandler, &g_bSkipVideo, NGlobal::CValue( 0.f ), true )
FINISH_REGISTER
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
