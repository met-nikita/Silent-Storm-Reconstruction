#include "StdAfx.h"
#include "iMain.h"
#include "GView.h"
#include "Gfx.h"
#include "SWTexture.h"
#include "GScene.h"
#include "G2DView.h"
#include "A5Script.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\BasicShare.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "iSaveManager.h"
#include "iExitMenu.h"
#include "Interface.h"
#include "iInterMission.h"
#include "GResource.h"
#include "iLoading.h"   // NGame::ShowLoadingScreen / TermLoadingScreen -- loading-screen lifecycle on the load path
#include "ModManager.h" // CModManager -- retail saves carry the active-mod list in the header (CICLoad/CICSave)
////////////////////////////////////////////////////////////////////////////////////////////////////
void DumpMemoryStats() {}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMainLoop
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static STime currentTime;
static bool bAppIsActive = false;
static list< CPtr<CInterfaceCommand> > cmds;
static list< CObj<IInterfaceBase> > interfaces;
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowLogo()
{
	CObj<NGScene::I2DGameView> p2DScene = NGScene::CreateNew2DView();

	NGScene::ClearScreen( CVec3( 0, 0, 0 ) );
	NDb::CTexture *pLogo = NDb::GetTexture( 1744 );
	if ( pLogo )
	{
		CVec2 vSize = p2DScene->GetViewportSize();
		CTRect<int> window( 0, 0, Float2Int(vSize.x), Float2Int(vSize.y) );
		CRectLayout rl;

		p2DScene->StartNewFrame();
		// quad size = logo dims * (vp/1024, vp/768) = the full viewport (baked; no layout scale)
		rl.AddRect( 0, 0, pLogo->nWidth * vSize.x / 1024.0f, pLogo->nHeight * vSize.y / 768.0f,
			CRectLayout::STextureCoord( CTRect<float>( 0, pLogo->nHeight, pLogo->nWidth, 0 ) ) );
		p2DScene->CreateDynamicRects( pLogo,  rl, CTPoint<int>( 0, 0 ), window );
		p2DScene->Flush();
	}
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowSplash( NDb::CUIContainer *pUI, const CArray2D<NGfx::SPixel8888> &sScreenShot )
{
	CPtr<NUI::ICursor> pCursor = NUI::ICursor::Create( false );
	CObj<NUI::CInterface> pInterface = new NUI::CInterface( pCursor );

	NUI::LoadTemplate( pInterface, pUI );

	CPtr<NUI::CWindow> pScreenShotBase = NUI::GetUIWindow<NUI::CWindow>( pInterface, "screenshot" );
	CObj<NUI::CScreenShot> pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pScreenShotBase, NUI::SPoint( 0, 0 ), pScreenShotBase->GetSize(), "screenshot", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE ) );
	pScreenShot->Set( sScreenShot );
	pScreenShot->SetMode( NUI::CScreenShot::COLOR, CVec4( 0.75f, 0.75f, 0.75f, 1 ) );

	pInterface->Step( 0 );
	pInterface->Draw( 0 );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MakeScreenShot()
{
	CreateDir( "screenshots" );

	char szName[1024];
	SYSTEMTIME sTime;
	GetLocalTime( &sTime );
	sprintf( szName, "screenshots\\ScrnShot_%2.2d%2.2d%2.2d_%2.2d%2.2d%2.2d.bmp", sTime.wDay, sTime.wMonth, sTime.wYear % 100, sTime.wHour, sTime.wMinute, sTime.wSecond );

	CFileStream f;
	try
	{
		f.OpenWrite( szName );

		CArray2D<NGfx::SPixel8888> data;
		NGfx::MakeScreenShot( &data, true );

		BITMAPFILEHEADER head;
		BITMAPINFOHEADER info;
		Zero( head );
		Zero( info );
		head.bfType = 0x4d42;
		head.bfSize = sizeof( head ) + sizeof( info ) + data.GetXSize() * data.GetYSize() * 4;
		head.bfOffBits = sizeof( head ) + sizeof( info );
		info.biSize = sizeof( info );
		info.biWidth = data.GetXSize();
		info.biHeight = data.GetYSize();
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biCompression = BI_RGB;
		info.biSizeImage = 0;
		info.biXPelsPerMeter = 1;
		info.biYPelsPerMeter = 1;
		info.biClrUsed = 0;
		info.biClrImportant = 0;

		f.Write( &head, sizeof( head ) );
		f.Write( &info, sizeof( info ) );
		for ( int y = data.GetYSize() - 1; y >=0; --y )
			f.Write( &data[y][0], data.GetXSize() * 4 );
	}
	catch(...)
	{
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInterfaceCommand
void CInterfaceCommand::ResetStack()
{
	interfaces.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceCommand::SetInterface( IInterfaceBase *pNewInterface )
{
	ASSERT( IsValid( pNewInterface ) );
	interfaces.clear();
	interfaces.push_back( pNewInterface );
	pNewInterface->OnGetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInterfaceBase* CInterfaceCommand::GetInterface() const
{
  if ( interfaces.empty() )
    return 0;
  return interfaces.back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceCommand::PushInterface( IInterfaceBase *pNewInterface )
{
	ASSERT( IsValid( pNewInterface ) );
	// retail @0x1f5230: the covered top gets OnLostFocus; the NEW interface gets NO focus call --
	// its Initialize just ran (the old dev OnGetFocus-on-push had no retail counterpart)
	if ( !interfaces.empty() )
		interfaces.back()->OnLostFocus();
	interfaces.push_back( pNewInterface );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceCommand::PopInterface()
{
	if ( interfaces.empty() )
		return;
	interfaces.back()->OnLostFocus();	// retail @0x1f50a0: the removed top loses focus first
	interfaces.pop_back();
	if ( !interfaces.empty() )
		interfaces.back()->OnGetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInterfaceObject
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime IInterfaceObject::GetTime()
{
	return currentTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInterfaceBase
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IInterfaceBase::CanRender()
{
	if ( bAppIsActive && NGScene::Is3DActive() )
		return true;
	Sleep( 10 );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICExitModal
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICContainer::Exec()
{
	for ( int nTemp = 0; nTemp < cmdsSet.size(); nTemp++ )
		cmdsSet[nTemp]->Exec();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICExitModal
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICExitModal::Exec() 
{
	PopInterface(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICLoad
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICLoad::Exec()
{
	if ( strcspn( szName.c_str(), S_INVALID_SAVE_CHARS ) != szName.length() )
	{
		csSystem << "Invalid save file name \"" << szName << "\"" << endl;
		return;
	}

	CSaveManager *pSaveManager = GetSaveManager();

	if ( !bSilent )
	{
		// retail @0x1f5fd0: the ONLY paint on the load path is one loading-screen frame. The old
		// dev GetSlotScreenShot + ShowSplash(350) pair was CUT in retail -- keeping it stacked the
		// splash's "Loading..." label over the loading screen's own PLEASE WAIT/LOADING pair.
		NGame::ShowLoadingScreen( 0 );
	}

#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenRead( pSaveManager->GetSlotFilePath( szName, S_SAVE_FILENAME ).c_str() );

		SSaveFileHeader sHeader;
		sFile.Read( &sHeader, sizeof(SSaveFileHeader) );

		if ( sHeader.nMagic != N_SAVE_MAGIC_NUMBER && sHeader.nMagic != N_SAVE_MAGIC_NUMBER_V0 )
			throw L"Invalid save file";	// retail CICLoad::Exec @0x1f5fd0 dual magic check (same format)

		// retail @0x1f5fd0: the header's second int counts the save's ACTIVE-MOD directory
		// strings that follow; activate that mod set (DB rebuild) before deserializing. An
		// unavailable mod aborts the load without touching the running state.
		vector<string> modDirs( sHeader.nMods );
		for ( int nTemp = 0; nTemp < sHeader.nMods; nTemp++ )
			sFile.ReadString( modDirs[nTemp] );
		if ( !CModManager::Activate( modDirs ) )
			throw L"Save mods unavailable";

		interfaces.clear();
		CSharedHolder hold;
		CStructureSaver sSaver( sFile, CStructureSaver::READ );
		sSaver.Add( 2, &interfaces );
		SerializeShared( &sSaver );
		ASSERT( !interfaces.empty() );
		SaveLoadDiag( "LOAD-DESERIALIZE-COMPLETE (%d interfaces)\n", (int)interfaces.size() );
		// dev-only in-place-resume hook: OnSnapshotRestored -> CMission::OnSnapshotRestored ->
		// CWorld::RestoreRuntimeCaches (rebuild building shells + camera height source, reconnect the
		// AI commanders -- runtime refs this fork's shallower serialization does not restore; without
		// it the AI Segment crashed on null pWorld). Retail's only post-deserialize call here is
		// front->OnLoad(bSilent) (vtbl+0x18, CICLoad::Exec @0x1f5fd0 -> CMission::OnLoad @0x1fb8e0 =
		// loading-bar counters only) -- retail restores NO world state here and in particular never
		// restarts the turn; the deserialized TBS state resumes as saved.
		for ( list< CObj<IInterfaceBase> >::iterator i = interfaces.begin(); i != interfaces.end(); ++i )
			if ( IsValid( *i ) )
				(*i)->OnSnapshotRestored();
	}
#ifndef _DEBUG
	catch(...)
	{
		ASSERT( 0 && "Loading failed!" );
		SaveLoadDiag( "LOAD-FAILED (exception caught in CICLoad)\n" );
		return;
	}
#endif

	pSaveManager->LoadSlot( szName );
	SaveLoadDiag( "LOAD-SLOT-DONE (interfaces restored; entering main loop)\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICSave
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSave::Exec()
{
	if ( strcspn( szName.c_str(), S_INVALID_SAVE_CHARS ) != szName.length() )
	{
		csSystem << "Invalid save file name \"" << szName << "\"" << endl;
		return;
	}

	StepApp( true, true, false );
	CArray2D<NGfx::SPixel8888> sScreenShot;
	NGfx::MakeScreenShot( &sScreenShot, true );

	ShowSplash( NDb::GetUIContainer( 349 ), sScreenShot );

	CSaveManager *pSaveManager = GetSaveManager();

	pSaveManager->SaveSlot( szName );

#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenWrite( pSaveManager->GetSlotFilePath( szName, S_SAVE_FILENAME ).c_str() );

		SSaveFileHeader sHeader;
		sHeader.nMagic = N_SAVE_MAGIC_NUMBER;
		// retail @0x1f6390: the second header int = active-mod count; the mods' directory
		// strings follow the header so the loader can re-activate the same DB set.
		vector<SModInfo> &activeMods = *CModManager::GetActiveMods();
		sHeader.nMods = activeMods.size();

		CDGPtr<NGScene::CBilinearTexture> pTexture = new NGScene::CBilinearTexture( sScreenShot, N_SAVE_SCREENSHOT_X, N_SAVE_SCREENSHOT_Y );
		pTexture.Refresh();
		CObj<NGScene::CSWTextureData> pData = pTexture->GetValue();
		CArray2D<NGfx::SPixel8888> &sScreenShot320x200 = pData->mips.front();

		for ( int nTempY = 0; nTempY < N_SAVE_SCREENSHOT_Y; nTempY++ )
			for ( int nTempX = 0; nTempX < N_SAVE_SCREENSHOT_X; nTempX++ )
				sHeader.sScreenShot[nTempY][nTempX] = sScreenShot320x200[nTempY][nTempX];

		sFile.Write( &sHeader, sizeof(SSaveFileHeader) );
		for ( int nTemp = 0; nTemp < activeMods.size(); nTemp++ )
			sFile.WriteString( activeMods[nTemp].szDirectory );

		CStructureSaver sSaver( sFile, CStructureSaver::WRITE );
		sSaver.Add( 2, &interfaces );
		SerializeShared( &sSaver );
	}
#ifndef _DEBUG
	catch(...)
	{
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICSaveFile -- retail @0x1f6a80: write the whole interface stack into temp\<name> as a raw
// headerless compressed stream (dev: uncompressed WRITE; the reader is symmetric).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSaveFile::Exec()
{
	// (no S_INVALID_SAVE_CHARS check: that guard is for user-typed SLOT names -- the raw snapshot
	// name is a code constant like "restart.sav", whose '.' the slot rule would falsely reject)
	CSaveManager *pSaveManager = GetSaveManager();
	pSaveManager->PrepareSlot( S_SLOT_ACTIVE );
	const string szPath = pSaveManager->GetSlotFilePath( S_SLOT_ACTIVE, szName );
	// retail @0x1f6a80: clear + delete the stale snapshot before writing
	::SetFileAttributesA( szPath.c_str(), FILE_ATTRIBUTE_NORMAL );
	::DeleteFileA( szPath.c_str() );

#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenWrite( szPath.c_str() );

		CStructureSaver sSaver( sFile, CStructureSaver::WRITE );
		sSaver.Add( 2, &interfaces );
		SerializeShared( &sSaver );
	}
#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't write snapshot " << szName << endl;
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICLoadFile -- retail @0x1f6830: replace the whole interface stack from temp\<name>. No header,
// no LoadSlot bookkeeping.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICLoadFile::Exec()
{
	// (no slot-name character check -- see CICSaveFile::Exec)
	CSaveManager *pSaveManager = GetSaveManager();

#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenRead( pSaveManager->GetSlotFilePath( S_SLOT_ACTIVE, szName ).c_str() );

		interfaces.clear();
		CSharedHolder hold;
		CStructureSaver sSaver( sFile, CStructureSaver::READ );
		sSaver.Add( 2, &interfaces );
		SerializeShared( &sSaver );
		ASSERT( !interfaces.empty() );

		// the snapshot resumes in place (no Initialize): let each interface rebuild its
		// runtime-only caches (building shells, camera terrain height source, ...)
		for ( list< CObj<IInterfaceBase> >::iterator i = interfaces.begin(); i != interfaces.end(); ++i )
			if ( IsValid( *i ) )
				(*i)->OnSnapshotRestored();
	}
#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't load snapshot " << szName << endl;
		return;
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICProfile
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICProfile::Exec()
{
	CSaveManager *pSaveManager = GetSaveManager();

	pSaveManager->SetActiveProfile( szProfile );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NMainLoop
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bWireFrame = false;
static bool bShowFPSStats = false;
static NInput::CBind cWireframe( "wireframe" ), bindExit( "exitgame" ), cLoad( "load" ), cSave( "save" ), cScreenShot( "screenshot" ), cDump( "memorystats" );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ProcessStandardEvents( const NInput::SEvent &eEvent )
{
	if ( bindExit.ProcessEvent( eEvent ) )
#ifdef _MAPEDIT
		Command( 0 );
#else
		Command( new NGame::CICExitMenu() );
#endif
	else if ( cWireframe.ProcessEvent( eEvent ) )
		NGScene::SetWireframe( bWireFrame = !bWireFrame );
	else if ( cLoad.ProcessEvent( eEvent ) )
		Command( new NMainLoop::CICLoad( string( S_SLOT_QUICKSAVE ) ) );
	else if ( cSave.ProcessEvent( eEvent ) )
		Command( new NMainLoop::CICSave( string( S_SLOT_QUICKSAVE ) ) );
	else if ( cScreenShot.ProcessEvent( eEvent ) )
	{
		if ( MakeScreenShot() )
			csGame << "ScreenShot created." << endl;
		else
			csGame << CC_RED << "ScreenShot create failed." << endl;
	}
	else if ( cDump.ProcessEvent( eEvent ) )
		DumpMemoryStats();

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool ProcessInterfaceCmds()
{
	while ( !cmds.empty() )
	{
		CPtr<CInterfaceCommand> pCmd = cmds.front();
		cmds.pop_front();
		if ( !IsValid( pCmd ) )
			return false;
		pCmd->Exec();
	}
	return !interfaces.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool StepApp( bool bActive, bool bSetGamma, bool bInput )
{
	if ( bActive )
		NGfx::CheckBackBufferSize();
	bAppIsActive = bActive;
	NGfx::SetGamma( bSetGamma );
	if ( !ProcessInterfaceCmds() )
		return false;
	ASSERT( IsValid( interfaces.back() ) );

	if ( bInput )
	{
		// commands processing
		NInput::SEvent event;
		while ( NInput::GetEvent( &event ) )
		{
			if ( !interfaces.back()->ProcessEvent( event ) )
				ProcessStandardEvents( event );
			if ( !ProcessInterfaceCmds() )
				return false;
		}
		interfaces.back()->ProcessEvent( event );
		if ( !ProcessInterfaceCmds() )
			return false;

		currentTime = event.mMessage.tTime;
	}

	if ( bActive )
		NGScene::LoadPrecached();

	interfaces.back()->Step();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoneInterface()
{
	interfaces.clear();
	NGame::TermLoadingScreen();   // release DoneInterface @0x1f5110: clear the list, then tear down the loading screen (pairs the boot InitLoadingScreen)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Command( CInterfaceCommand *pCmd )
{
	cmds.push_back( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1f4e40
bool HaveInterfaceCommand()
{
	return !cmds.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetInterfaceStackDepth()
{
	return interfaces.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void LoadSavedGame( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() < 1 )
	{
		csSystem << szID << " \"name\"" << endl;
		return;
	}

	string szName( NStr::ToAscii( szParams[0].c_str() ) );

	csSystem << "loading slot " << szName << "..." << endl;
	Command( new CICLoad( szName ) );
	csSystem << "done." << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SaveGame( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() < 1 )
	{
		csSystem << szID << " \"name\"" << endl;
		return;
	}

	string szName( NStr::ToAscii( szParams[0].c_str() ) );

	csSystem << "saving slot " << szName << "..." << endl;
	Command( new CICSave( szName ) );
	csSystem << "done." << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetProfile( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() < 1 )
	{
		csSystem << szID << " \"name\"" << endl;
		return;
	}

	string szName( NStr::ToAscii( szParams[0].c_str() ) );

	csSystem << "profile set to " << szName << endl;
	Command( new CICProfile( szName ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMain)
	REGISTER_CMD( "load", LoadSavedGame )
	REGISTER_CMD( "save", SaveGame )
	REGISTER_CMD( "profile", SetProfile )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace 
////////////////////////////////////////////////////////////////////////////////////////////////////
