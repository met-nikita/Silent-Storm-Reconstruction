#include "StdAfx.h"
#include "..\Main\GInit.h"
#include "WinFrame.h"
#include "..\Main\iMain.h"
#include "..\Input\Bind.h"
#include "..\ADOImport\BasicDB.h"
#include "..\DBFormat\DataMap.h"		// NDb::BuildMapLinks (post-load DB relation build)
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Main\GResource.h" // CRAP �� ��������� �������, ������-�� ������ ���� ��������� ������
#include "..\Main\iInterMission.h" // CRAP, to start from mission
#include "..\Main\iLoading.h"      // NGame::InitLoadingScreen / TermLoadingScreen -- loading-screen UI built once at boot
#include "..\Main\iSaveManager.h" // CRAP, to start from mission
#include "..\Main\Sound.h"
#include "..\Main\WinInputConv.h" // Win32->NInput bridge: replays WM_KEYDOWN/WM_CHAR (OS auto-repeat)
////////////////////////////////////////////////////////////////////////////////////////////////////
//void DumpMemoryStats() {}

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
#ifdef _DEBUG
  int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	//tmpFlag |= _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF;
  tmpFlag = _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF;
  _CrtSetDbgFlag( tmpFlag );
	//_CrtSetBreakAlloc( 114 );
#else
	srand( GetTickCount() );
#endif // _DEBUG
	NGScene::AddResourceDir( ".\\res" );
	NGScene::RunResourceLoadingThread();
  // load game database
	try
	{
		CFileStream f;
		f.OpenRead( "game.db" );
		NDatabase::Serialize( f, CStructureSaver::READ );
	}
	catch (...)
	{
		ASSERT( 0 ); // game.db not found
		MessageBox( 0, "File game.db not found", "Error", MB_OK );
		return 0;
	}

	// NOTE: no explicit NDb::BuildMapLinks() here (it is APPEND-ONLY -- calling it twice duplicates
	// skeleton-anim/debris/uniform-look/per-pers-inventory links). Every load path already covers it:
	// a v1/Steam columnar game.db has its links built by NDatabase::Serialize itself (gated internal
	// BuildMapLinks(false), ADOImport\BasicDB.cpp), and a v0 dev-format game.db carries the links
	// serialized in its records (DataImport runs BuildMapLinks before exporting; CSkeleton::pAnimations
	// is a serialized member). The unconditional call that used to sit here double-pushed on both.

	// init subsystems
	if ( !NWinFrame::InitApplication( hInstance, "Silent Storm", "Silent Storm" ) )
		return 0;
	if ( !NGfx::Init3D( NWinFrame::GetWnd() ) )
	{
		ASSERT(0); // DX8 not found
		MessageBox( 0, "Failed to initialize Direct3D8", "Error", MB_OK );
		return 0;
	}
	if ( !NSound::InitSound( NWinFrame::GetWnd() ) )
	{
		ASSERT(0); // FMod not found
		MessageBox( 0, "Failed to initialize FMod", "Error", MB_OK );
		return 0;
	}
	if ( !NInput::InitInput( NWinFrame::GetWnd() ) )
	{
		ASSERT(0); // DX8input not found
		MessageBox( 0, "Failed to initialize DirectInput8", "Error", MB_OK );
		return 0;
	}

	// Load config & process params
	NGlobal::LoadConfig( ".\\cfg\\autoexec.cfg" );

	vector<string> szParams;
	bool bDoLoad = false;
	NStr::SplitStringWithMultipleBrackets( lpCmdLine, szParams, ' ' );
	string szCfg( "start.cfg" );
	for ( int i = 0; i < szParams.size(); ++i )
	{
		if ( szParams[i] == "-fullscreen" )
			NGlobal::SetVar( "gfx_fullscreen", 1 );
		else if ( szParams[i] == "-320" )
			NGlobal::SetVar( "gfx_resolution", 320 );
		else if ( szParams[i] == "-400" )
			NGlobal::SetVar( "gfx_resolution", 400 );
		else if ( szParams[i] == "-640" )
			NGlobal::SetVar( "gfx_resolution", 640 );
		else if ( szParams[i] == "-800" )
			NGlobal::SetVar( "gfx_resolution", 800 );
		else if ( szParams[i] == "-1024" )
			NGlobal::SetVar( "gfx_resolution", 1024 );
		else if ( szParams[i] == "-1280" )
			NGlobal::SetVar( "gfx_resolution", 1280 );
		else if ( szParams[i] == "-1600" )
			NGlobal::SetVar( "gfx_resolution", 1600 );
		else if ( szParams[i] == "-nops" )
			NGlobal::SetVar( "gfx_nopixelshaders", 1 );
		else if ( szParams[i] == "-novs" )
			NGlobal::SetVar( "gfx_novertexshaders", 1 );
		else if ( szParams[i] == "-gfxvalidate" )
			NGlobal::SetVar( "gfx_validate", 1 );
		else if ( szParams[i] == "-aniso" )
			NGlobal::SetVar( "gfx_anisotropic_filter", 1 );
		else if ( szParams[i] == "-bannp2" )
			NGlobal::SetVar( "gfx_fix_ban_np2", 1 );
		else if ( szParams[i] == "-nvrulez" )
			NGlobal::SetVar( "gfx_fix_nv_np2_hack", 1 );
		else if ( szParams[i] == "-dxtoff" )
			NGlobal::SetVar( "gfx_texture_usedxt", 0 );

		if ( szParams[i] == "-nosound" )
			NGlobal::SetVar( "sound_init", 0 );

		if ( szParams[i] == "-noai" )
			NGlobal::SetVar( "game_noai", 1 );

		if ( szParams[i] == "-load" )
			bDoLoad = true;
		if ( szParams[i] == "-cfg" )
		{
			if ( i + 1 < szParams.size() )
				szCfg = szParams[++i];
		}
	}
	//
	if ( !NGScene::SetModeFromConfig() )
	{
		ASSERT(0); // no mode found
		MessageBox( 0, "Failed to set display mode", "Error", MB_OK );
		return 0;
	}
	//
	if ( !NSound::SetModeFromConfig() )
	{
		ASSERT(0);
		MessageBox( 0, "Failed to set sound mode", "Error", MB_OK );
		return 0;
	}
	//
	// Build the loading-screen UI once at boot, BEFORE the first interface command is queued. Mirrors
	// release NMainLoop::InitInterface @0x1f5800, whose first unconditional statement is InitLoadingScreen()
	// (iMain.c:699-700), run before the bLoad?CICLoad:CICInterMission build. Builds the three file-scope
	// iLoading globals (cursor + a SEPARATE CInterface + CLoadingUI) so the load paths paint a splash instead
	// of a black screen. Paired with NGame::TermLoadingScreen() in NMainLoop::DoneInterface (called at shutdown
	// below). Safe: this CInterface is never pushed onto NMainLoop::interfaces; it is only Step+Drawn inside
	// ShowLoadingScreen, so ShowWindow(SHOW) here does not overlay the menu queued just below.
	NGame::InitLoadingScreen();
	if ( bDoLoad )
		NMainLoop::Command( new NMainLoop::CICLoad( string( NMainLoop::S_SLOT_QUICKSAVE ) ) );
	else
		NMainLoop::Command( new CICInterMission( szCfg ) );
	SWinToInputMessageConverter sWinInputConv;
	for (;;)
	{
		NWinFrame::PumpMessages();
		bool bActive = NWinFrame::IsAppActive();
		NInput::PumpMessages( bActive );
		// Re-emit the coalesced Win32 keyboard stream (WM_KEYDOWN/WM_CHAR, OS auto-repeated)
		// as NInput messages, exactly as the retail main loop does (WinMain @0x9810: right
		// after NInput::PumpMessages, before StepApp) -- this is what gives held keys repeat.
		sWinInputConv.Do();
		if ( NWinFrame::IsExit() )
			break;
		if ( !NMainLoop::StepApp( bActive, bActive ) )
			break;
		if ( !bActive )
			Sleep( 40 );
	}
	//
	NGlobal::SaveConfig( ".\\cfg\\config.cfg" );
	NMainLoop::DoneInterface();
	NGfx::Done3D();
	NInput::DoneInput();
	NSound::DoneSound();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
