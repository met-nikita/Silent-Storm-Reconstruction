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
#include "..\Misc\HPTimer.h"       // NHPTimer::UpdateHPTimerFrequency -- the per-frame TSC recalibration
#include "..\Main\iSaveManager.h" // CRAP, to start from mission
#include "..\Main\Sound.h"
#include "..\Main\WinInputConv.h" // Win32->NInput bridge: replays WM_KEYDOWN/WM_CHAR (OS auto-repeat)
#include "..\FileIO\BasicChunk1.h"  // [HARNESS] g_bSaveLoadDiag / SaveLoadDiag
#include "..\MiscDll\LogStream.h"   // [HARNESS] g_bHarnessLog (console-log tee)
#include "..\Main\A5Script.h"       // [HARNESS] ProcessCommand (console/lua entry for the command channel)
#include <dbghelp.h>                 // [HARNESS] post-load crash backtrace (SymFromAddr / StackWalk64)
#pragma comment(lib, "dbghelp.lib")
////////////////////////////////////////////////////////////////////////////////////////////////////
// [HARNESS] Unhandled-exception filter: on a post-load AV (the "silent close"), log a symbolic
// backtrace to _saveload.log so the driver can map the fault to a function. Installed only in harness
// runs (needs a PDB next to Game.exe -- CMake emits one for optimized configs). Writes directly to the
// file (no engine calls) so it survives a corrupted heap. Terminates after logging (unattended runs).
static LONG WINAPI HarnessCrashFilter( EXCEPTION_POINTERS *pEP )
{
	FILE *pF = fopen( "_saveload.log", "ab" );
	if ( pF )
	{
		fprintf( pF, "CRASH code=0x%08X addr=0x%p\n",
			(unsigned)pEP->ExceptionRecord->ExceptionCode, pEP->ExceptionRecord->ExceptionAddress );
		if ( pEP->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
			pEP->ExceptionRecord->NumberParameters >= 2 )
			fprintf( pF, "  access=%s data-addr=0x%08X\n",
				pEP->ExceptionRecord->ExceptionInformation[0] ? "WRITE" : "READ",
				(unsigned)pEP->ExceptionRecord->ExceptionInformation[1] );
		HANDLE hProc = GetCurrentProcess();
		SymSetOptions( SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES );
		if ( SymInitialize( hProc, NULL, TRUE ) )
		{
			CONTEXT ctx = *pEP->ContextRecord;
			STACKFRAME64 sf; memset( &sf, 0, sizeof(sf) );
			sf.AddrPC.Offset = ctx.Eip;    sf.AddrPC.Mode    = AddrModeFlat;
			sf.AddrFrame.Offset = ctx.Ebp; sf.AddrFrame.Mode = AddrModeFlat;
			sf.AddrStack.Offset = ctx.Esp; sf.AddrStack.Mode = AddrModeFlat;
			for ( int n = 0; n < 40; ++n )
			{
				if ( !StackWalk64( IMAGE_FILE_MACHINE_I386, hProc, GetCurrentThread(), &sf, &ctx,
						NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL ) )
					break;
				DWORD64 addr = sf.AddrPC.Offset;
				if ( !addr )
					break;
				IMAGEHLP_MODULE64 mod; mod.SizeOfStruct = sizeof(mod);
				const char *szMod = SymGetModuleInfo64( hProc, addr, &mod ) ? mod.ModuleName : "?";
				char symBuf[ sizeof(SYMBOL_INFO) + 512 ];
				SYMBOL_INFO *pSym = (SYMBOL_INFO*)symBuf;
				pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
				pSym->MaxNameLen = 500;
				DWORD64 disp = 0;
				IMAGEHLP_LINE64 line; line.SizeOfStruct = sizeof(line); DWORD lineDisp = 0;
				bool bLine = !!SymGetLineFromAddr64( hProc, addr, &lineDisp, &line );
				if ( SymFromAddr( hProc, addr, &disp, pSym ) )
					fprintf( pF, "  #%02d %s!%s +0x%llX%s%s:%lu\n", n, szMod, pSym->Name,
						(unsigned long long)disp,
						bLine ? "  " : "", bLine ? line.FileName : "", bLine ? line.LineNumber : 0 );
				else
					fprintf( pF, "  #%02d %s +0x%llX  [0x%llX]\n", n, szMod,
						(unsigned long long)( addr - SymGetModuleBase64( hProc, addr ) ),
						(unsigned long long)addr );
			}
		}
		fprintf( pF, "CRASH-END\n" );
		fclose( pF );
	}
	return EXCEPTION_EXECUTE_HANDLER;   // terminate cleanly after logging
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//void DumpMemoryStats() {}

// ============================================================================================
// [HARNESS] Frame-polled command channel -- a minimal RTC protocol between an external driver and
// the running game. Once per frame (when g_bHarnessLog is on) the main loop reads ONE command line
// from ".\_harness_cmd.txt" (raw system-ANSI bytes so Cyrillic slot names round-trip), clears the
// file, executes it, and acks into _saveload.log; engine output goes to _console.log (the tee).
// Verbs (extend freely -- this is the protocol foundation):
//   console <text>   run a console command / var / "@lua" (the global ProcessCommand entry)
//   load <slot>      queue a save-slot load (slot name = raw ANSI)
//   quit             request a clean shutdown
// The driver (gen/_loadtest.py in s2_scratch) writes _harness_cmd.txt and reads the logs. Sweep the
// whole harness by grepping "[HARNESS]".
// ============================================================================================
static bool HarnessPoll()   // returns false to request main-loop exit
{
	FILE *pF = fopen( "_harness_cmd.txt", "rb" );
	if ( !pF )
		return true;
	char szBuf[2048];
	size_t n = fread( szBuf, 1, sizeof(szBuf) - 1, pF );
	fclose( pF );
	remove( "_harness_cmd.txt" );
	szBuf[n] = 0;
	while ( n && ( szBuf[n-1] == '\n' || szBuf[n-1] == '\r' || szBuf[n-1] == ' ' || szBuf[n-1] == '\t' ) )
		szBuf[--n] = 0;
	if ( n == 0 )
		return true;
	string sCmd( szBuf );
	SaveLoadDiag( "[harness] cmd: %s\n", sCmd.c_str() );
	if ( sCmd == "quit" )
		return false;
	else if ( sCmd.compare( 0, 8, "console " ) == 0 )
		ProcessCommand( NStr::ToUnicode( sCmd.substr( 8 ) ) );
	else if ( sCmd.compare( 0, 5, "load " ) == 0 )
		NMainLoop::Command( new NMainLoop::CICLoad( sCmd.substr( 5 ) ) );
	else
		SaveLoadDiag( "[harness] unknown cmd\n" );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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
	string szLoadSlot( NMainLoop::S_SLOT_QUICKSAVE );
	NStr::SplitStringWithMultipleBrackets( lpCmdLine, szParams, ' ' );
	string szCfg( "start.cfg" );
	for ( int i = 0; i < szParams.size(); ++i )
	{
		if ( szParams[i] == "-fullscreen" )
			NGlobal::SetVar( "gfx_fullscreen", 1 );
		else if ( szParams[i] == "-windowed" )
			NGlobal::SetVar( "gfx_fullscreen", 0 );
		else if ( szParams[i] == "-harness" )   // [HARNESS] enable the command channel + console tee WITHOUT auto-loading
		{
			g_bSaveLoadDiag = true;
			g_bHarnessLog = true;
			remove( "_saveload.log" );
			remove( "_console.log" );
			SaveLoadDiag( "BOOT harness (no auto-load)\n" );
		}
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
			NGlobal::SetVar( "gfx_anisotropic_filter", 2 );   // retail @0x409f65: level 2 (1 = off)
		else if ( szParams[i] == "-bannp2" )
			NGlobal::SetVar( "gfx_fix_ban_np2", 1 );
		else if ( szParams[i] == "-nvrulez" )
			NGlobal::SetVar( "gfx_fix_nv_np2_hack", 1 );
		else if ( szParams[i] == "-dxtoff" )
			NGlobal::SetVar( "gfx_texture_usedxt", 0 );

		if ( szParams[i] == "-nosound" )
		{
			NGlobal::SetVar( "sound_mode", 0 );   // retail WinMain @0x9810 sets both (sound_mode is what SetModeFromConfig reads)
			NGlobal::SetVar( "sound_init", 0 );
		}

		if ( szParams[i] == "-noai" )
			NGlobal::SetVar( "game_noai", 1 );

		if ( szParams[i] == "-load" )
			bDoLoad = true;
		// -loadslot: unattended save-load test harness. Read the target slot NAME (raw bytes,
		// system-ANSI/CP1251 -- so Cyrillic folder names round-trip) from ".\_loadslot.txt", auto-load
		// it instead of the menu, and enable the SaveLoadDiag object trace (-> ".\_saveload.log").
		if ( szParams[i] == "-loadslot" )
		{
			try
			{
				CFileStream fSlot;
				fSlot.OpenRead( "_loadslot.txt" );
				int nLen = fSlot.GetSize();
				string sSlot;
				sSlot.resize( nLen );
				if ( nLen > 0 )
					fSlot.Read( &sSlot[0], nLen );
				while ( !sSlot.empty() && ( sSlot[sSlot.size()-1] == '\n' || sSlot[sSlot.size()-1] == '\r' || sSlot[sSlot.size()-1] == ' ' || sSlot[sSlot.size()-1] == '\t' ) )
					sSlot.resize( sSlot.size() - 1 );
				if ( !sSlot.empty() )
				{
					szLoadSlot = sSlot;
					bDoLoad = true;
					g_bSaveLoadDiag = true;   // [HARNESS]
					g_bWireAudit = true;      // [HARNESS] per-load wire-divergence audit -> _wireaudit.log
					g_bHarnessLog = true;     // [HARNESS] tee engine console output to _console.log
					remove( "_saveload.log" );   // fresh trace each run
					remove( "_console.log" );
					remove( "_wireaudit.log" );
					SaveLoadDiag( "BOOT loadslot=[%s]\n", szLoadSlot.c_str() );
				}
			}
			catch ( ... ) {}
		}
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
		NMainLoop::Command( new NMainLoop::CICLoad( szLoadSlot ) );
	else
		NMainLoop::Command( new CICInterMission( szCfg ) );
	if ( g_bHarnessLog )
		SetUnhandledExceptionFilter( HarnessCrashFilter );   // [HARNESS] symbolic backtrace on post-load AV
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
		// retail WinMain @0x9810 calls this every frame right here (@0x40a70b, immediately after
		// StepApp): it re-derives the RDTSC->seconds scale against a rolling QPC window. Dev only ever
		// calibrated once, in the HPTimer static ctor, so fProcFreq1 was frozen at whatever clock the
		// CPU happened to be running at during boot -- everything on NHPTimer (the sound mixer's
		// timing, the window-message stamps) then drifts as SpeedStep/turbo move the TSC ratio.
		// Self-throttling: only recalibrates once the 50ms reference window has elapsed.
		NHPTimer::UpdateHPTimerFrequency();
		if ( g_bHarnessLog && !HarnessPoll() )   // [HARNESS] frame-polled command channel
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
