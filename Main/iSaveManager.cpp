#include "StdAfx.h"
#include "iMain.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"
#include "..\MiscDll\Commands.h"
#include "..\FileIO\Streams.h"
#include "Interface.h"     // NUI umbrella -- GetDBString (IsValidCustomName's reserved-name lookups)
#include "iSaveManager.h"
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMainLoop
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const char S_SAVE_TEMPLATE[] = "save\\";
const char S_SAVE_SLOTTEMPLATE[] = "save\\%s\\%s\\";
const char S_SAVE_PROFILETEMPLATE[] = "save\\%s\\";
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveManager* GetSaveManager()
{
	static CSaveManager sSaveManager;
	return &sSaveManager;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveManager::CSaveManager(): 
	nActiveSlotID( 0 ), szActiveProfile( "default" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::CreateProfile( const string &szProfile ) const
{
	CreateDir( NStr::Format( S_SAVE_PROFILETEMPLATE, szProfile.c_str() ) );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::DeleteProfile( const string &szProfile ) const
{
	RemoveDir( NStr::Format( S_SAVE_PROFILETEMPLATE, szProfile.c_str() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::GetProfilesList( list<string> *pList ) const
{
	string szProfilesDir( string( S_SAVE_TEMPLATE ) + "*.*" );

	_finddata_t sFindData;
	int nHandle = _findfirst( szProfilesDir.c_str(), &sFindData );
	int nRet = nHandle;
	while ( nRet != -1 )
	{
		if ( sFindData.attrib & _A_SUBDIR )
			pList->push_back( sFindData.name );

		nRet = _findnext( nHandle, &sFindData );
	}

	_findclose( nHandle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& CSaveManager::GetActiveProfile() const
{
	return szActiveProfile;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::SetActiveProfile( const string &szProfile )
{
	szActiveProfile = szProfile;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::SaveSlot( const string &szName )
{
	string szSource( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), S_SLOT_ACTIVE ) );
	string szTarget( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) );

	if ( szSource == szTarget )
	{
		CreateDir( szTarget );
		return;
	}

	RemoveDir( szTarget );
	////
	CreateDir( szTarget );
	CreateDir( szSource );

	CopyFiles( szSource, szTarget, "*.*" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::LoadSlot( const string &szName )
{
	string szSource( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) );
	string szTarget( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), S_SLOT_ACTIVE ) );

	if ( szSource == szTarget )
	{
		CreateDir( szTarget );
		return;
	}

	RemoveDir( szTarget );
	////
	CreateDir( szSource );
	CreateDir( szTarget );

	CopyFiles( szSource, szTarget, "*.*" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::ClearSlot( const string &szName )
{
	string szSource( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) );
	RemoveDir( szSource );
	CreateDir( szSource );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::DeleteSlot( const string &szName )
{
	string szSource( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) );
	RemoveDir( szSource );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::PrepareSlot( const string &szName )
{
	string szSource( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) );
	CreateDir( szSource );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::GetSlotsList( list<string> *pList ) const
{
	string szSource( NStr::Format( S_SAVE_PROFILETEMPLATE, szActiveProfile.c_str() ) );
	string szSourceMask( szSource + "*.*" );

	_finddata_t sFindData;
	int nHandle = _findfirst( szSourceMask.c_str(), &sFindData );
	int nRet = nHandle;
	while ( nRet != -1 )
	{
		string szName( sFindData.name );
		if ( ( sFindData.attrib & _A_SUBDIR ) && ( szName.compare( "." ) != 0 ) && ( szName.compare( ".." ) != 0 ) && ( szName.compare( "temp" ) != 0 ) )
			pList->push_back( sFindData.name );

		nRet = _findnext( nHandle, &sFindData );
	}

	_findclose( nHandle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::GetSlotTime( const string &szName, wstring *pTime )
{
	struct _stat sStat;
	int nRet = _stat( GetSlotFilePath( szName, S_SAVE_FILENAME ).c_str(), &sStat );
	if ( nRet == -1 )
	{
		ASSERT( 0 );
		return;
	}

	struct tm *pLocalTime = localtime( &sStat.st_mtime );

	WCHAR wcBuffer[MAX_PATH];
	wcsftime( wcBuffer, MAX_PATH, L"%d/%m/%y", pLocalTime );
	*pTime = wstring( wcBuffer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveManager::GetSlotScreenShot( const string &szName, CArray2D<NGfx::SPixel8888> *pScreenShot )
{
#ifndef _DEBUG
	try
#endif
	{
		CFileStream sFile;
		sFile.OpenRead( GetSlotFilePath( szName, S_SAVE_FILENAME ).c_str() );

		SSaveFileHeader sHeader;
		sFile.Read( &sHeader, sizeof(SSaveFileHeader) );

		if ( sHeader.nMagic != N_SAVE_MAGIC_NUMBER )
			throw L"Invalid save file";

		pScreenShot->SetSizes( N_SAVE_SCREENSHOT_X, N_SAVE_SCREENSHOT_Y );
		for ( int nTempY = 0; nTempY < N_SAVE_SCREENSHOT_Y; nTempY++ )
			for ( int nTempX = 0; nTempX < N_SAVE_SCREENSHOT_X; nTempX++ )
				(*pScreenShot)[nTempY][nTempX] = sHeader.sScreenShot[nTempY][nTempX];
	}
#ifndef _DEBUG
	catch(...)
	{
		ASSERT( 0 && "Loading failed!" );
		return;
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CSaveManager::GetSlotFilePath( const string &szName, const string &szFileName ) const
{
	return string( NStr::Format( S_SAVE_SLOTTEMPLATE, szActiveProfile.c_str(), szName.c_str() ) + szFileName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateDir( const string &szDir )
{
	int nPos = 0, nLastPos = 0;

	do
	{
		nPos = szDir.find_first_of( '\\', nLastPos );
		CreateDirectory( szDir.substr( 0, nPos ).c_str(), NULL );

		nLastPos = nPos + 1;
	} while( nPos != string::npos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RemoveDir( const string &szDir )
{
	string szSourcePath( szDir + "*.*" );

	_finddata_t sFindData;
	int nHandle = _findfirst( szSourcePath.c_str(), &sFindData );
	int nRet = nHandle;
	while ( nRet != -1 )
	{
		string szName( sFindData.name );
		if ( ( szName.compare( "." ) != 0 ) && ( szName.compare( ".." ) != 0 ) )
		{
			if ( sFindData.attrib & _A_SUBDIR )
				RemoveDir( szDir + sFindData.name );
			else
			{
				if ( !DeleteFile( string( szDir + sFindData.name ).c_str() ) )
					csSystem << "Can't delete file " << sFindData.name << endl;
			}
		}

		nRet = _findnext( nHandle, &sFindData );
	}

	_findclose( nHandle );

	if ( !RemoveDirectory( szDir.c_str() ) )
		csSystem << "Can't delete directory " << szDir << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CopyFiles( const string &szSource, const string &szTarget, const string &szMask )
{
	string szSourcePath( szSource + szMask );

	_finddata_t sFindData;
	int nHandle = _findfirst( szSourcePath.c_str(), &sFindData );
	int nRet = nHandle;
	while ( nRet != -1 )
	{
		string sSourceFile( szSource + sFindData.name );
		string sTargetFile( szTarget + sFindData.name );
		CopyFile( sSourceFile.c_str(), sTargetFile.c_str(), FALSE );

		nRet = _findnext( nHandle, &sFindData );
	}

	_findclose( nHandle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Profile free-fns (release iSaveManager.obj).  The profile UI talks to these.  Dir ops delegate to
// the CSaveManager; the ACTIVE profile lives in the NGlobal "game_profile" var (validated against the
// on-disk list), the release divergence from CSaveManager::szActiveProfile.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateProfile( const string &szProfile )
{
	GetSaveManager()->CreateProfile( szProfile );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DeleteProfile( const string &szProfile )
{
	GetSaveManager()->DeleteProfile( szProfile );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetProfilesList( list<string> *pList )
{
	GetSaveManager()->GetProfilesList( pList );
	// CSaveManager::GetProfilesList returns every save\* subdir incl. the "." / ".." dir entries that
	// _findfirst yields -- drop them so they don't show up as bogus profiles.
	for ( list<string>::iterator iProfile = pList->begin(); iProfile != pList->end(); )
	{
		if ( *iProfile == "." || *iProfile == ".." )
			iProfile = pList->erase( iProfile );
		else
			iProfile++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeDefaultProfile()
{
	list<string> profilesList;
	GetProfilesList( &profilesList );
	if ( profilesList.empty() )
	{
		NGlobal::ResetVar( "game_profile" );
		CreateProfile( NStr::ToAscii( NGlobal::GetVar( "game_profile" ).GetString() ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetActiveProfile()
{
	string szActive = NStr::ToAscii( NGlobal::GetVar( "game_profile" ).GetString() );

	list<string> profilesList;
	GetProfilesList( &profilesList );

	bool bFound = false;
	for ( list<string>::const_iterator iProfile = profilesList.begin(); iProfile != profilesList.end(); iProfile++ )
		if ( *iProfile == szActive )
			bFound = true;

	if ( !bFound )
	{
		if ( profilesList.empty() )
			MakeDefaultProfile();
		else
			NGlobal::SetVar( "game_profile", NGlobal::CValue( NStr::ToUnicode( profilesList.front() ) ) );
	}

	return NStr::ToAscii( NGlobal::GetVar( "game_profile" ).GetString() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveProfile( const string &szProfile )
{
	NGlobal::SetVar( "game_profile", NGlobal::CValue( NStr::ToUnicode( szProfile ) ) );
	GetSaveManager()->SetActiveProfile( szProfile );    // keep the slot system's active profile in sync
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NMainLoop::IsValidCustomName @0x235d00 (iSaveManager.obj): a typed save-slot name is valid
// unless it equals one of the four RESERVED localized slot names, DB strings 20241..20244 (each
// compared as ToAscii(GetDBString(id)) with plain string equality -- disasm 0x635d1c..0x635e4b).
// The empty-name check is the caller's job (CSaveView::SaveSlot tests empty separately).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsValidCustomName( const string &szName )
{
	if ( szName == NStr::ToAscii( NUI::GetDBString( 20241 ) ) )
		return false;
	if ( szName == NStr::ToAscii( NUI::GetDBString( 20242 ) ) )
		return false;
	if ( szName == NStr::ToAscii( NUI::GetDBString( 20243 ) ) )
		return false;
	if ( szName == NStr::ToAscii( NUI::GetDBString( 20244 ) ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
