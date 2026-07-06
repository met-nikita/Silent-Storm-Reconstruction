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
		rl.scale.x = vSize.x / 1024.0f;
		rl.scale.y = vSize.y / 768.0f;
		rl.AddRect( 0, 0, CRectLayout::STextureCoord( CTRect<float>( 0, pLogo->nHeight, pLogo->nWidth, 0 ) ) );
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
	interfaces.push_back( pNewInterface );
	pNewInterface->OnGetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceCommand::PopInterface()
{
	if ( interfaces.empty() )
		return;
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
		NGame::ShowLoadingScreen( 0 );   // @0x1f5fd0: paint one loading frame at the top of the !bSilent path (release: xor ecx,ecx -> pct=0)
		CArray2D<NGfx::SPixel8888> sScreenShot;
		pSaveManager->GetSlotScreenShot( szName, &sScreenShot );
		ShowSplash( NDb::GetUIContainer( 350 ), sScreenShot );
	}

#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenRead( pSaveManager->GetSlotFilePath( szName, S_SAVE_FILENAME ).c_str() );

		SSaveFileHeader sHeader;
		sFile.Read( &sHeader, sizeof(SSaveFileHeader) );

		if ( sHeader.nMagic != N_SAVE_MAGIC_NUMBER )
			throw L"Invalid save file";
//		if ( sHeader.nChecksum != CalcSaveCheckSum() )
//			throw L"Save file corrupted";

		interfaces.clear();
		CSharedHolder hold;
		CStructureSaver sSaver( sFile, CStructureSaver::READ );
		sSaver.Add( 2, &interfaces );
		SerializeShared( &sSaver );
		ASSERT( !interfaces.empty() );
	}
#ifndef _DEBUG
	catch(...)
	{
		ASSERT( 0 && "Loading failed!" );
		return;
	}
#endif

	pSaveManager->LoadSlot( szName );
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
//		sHeader.nChecksum = CalcSaveCheckSum();

		CDGPtr<NGScene::CBilinearTexture> pTexture = new NGScene::CBilinearTexture( sScreenShot, N_SAVE_SCREENSHOT_X, N_SAVE_SCREENSHOT_Y );
		pTexture.Refresh();
		CObj<NGScene::CSWTextureData> pData = pTexture->GetValue();
		CArray2D<NGfx::SPixel8888> &sScreenShot320x200 = pData->mips.front();

		for ( int nTempY = 0; nTempY < N_SAVE_SCREENSHOT_Y; nTempY++ )
			for ( int nTempX = 0; nTempX < N_SAVE_SCREENSHOT_X; nTempX++ )
				sHeader.sScreenShot[nTempY][nTempX] = sScreenShot320x200[nTempY][nTempX];

		sFile.Write( &sHeader, sizeof(SSaveFileHeader) );

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
