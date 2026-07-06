#include "StdAfx.h"
#include "ModManager.h"
#include "DG.h"						// ClearHoldQueue
#include "GResource.h"				// NGScene::{CloseAllResources,ClearResourceDirs,AddResourceDir}
#include "..\ADOImport\BasicDB.h"	// NDatabase::{ClearDatabaseTables,Serialize} + CFileStream (via BasicChunk1.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager -- mod enumeration / activation. Reconstructed from
// .\release\ModManager.obj (Game.exe). All methods are static; state is file-scope.
////////////////////////////////////////////////////////////////////////////////////////////////////
// File-scope statics (release globals nDataBaseVersion @0x9c6af4, activatedMods @0x9c6af8).
static int nDataBaseVersion = 0;
static vector<SModInfo> activatedMods;
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetModInfo (file-local helper @0x285be0)
//   Open "<dir>\\description.txt" for read; on success build an SModInfo whose
//   szName is the file text and szDirectory is the folder, and push it onto *pMods.
//   On any failure (open / GetFileSize / ReadFile fails) do nothing -- so a folder
//   without a description.txt produces no entry.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetModInfo( vector<SModInfo> *pMods, const string &szDir )
{
	string szPath = szDir + "\\description.txt";
	HANDLE hFile = CreateFileA( szPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
	                            0, OPEN_EXISTING, 0, 0 );
	if ( hFile == INVALID_HANDLE_VALUE )
		return;
	DWORD nSize = GetFileSize( hFile, 0 );
	if ( nSize == INVALID_FILE_SIZE )
	{
		CloseHandle( hFile );
		return;
	}
	SModInfo info;
	char *pBuffer = new char[nSize + 1];
	DWORD nRead = 0;
	if ( !ReadFile( hFile, pBuffer, nSize, &nRead, 0 ) )
	{
		CloseHandle( hFile );
		delete[] pBuffer;
		return;
	}
	pBuffer[nSize] = 0;
	info.szDirectory = szDir;     // owning folder
	info.szName = pBuffer;        // description text (up to first NUL)
	pMods->push_back( info );     // deep-copies both strings
	CloseHandle( hFile );
	delete[] pBuffer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::GetBaseVersion @0x285b70
int CModManager::GetBaseVersion()
{
	return nDataBaseVersion;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::GetActiveMods @0x285b80
vector<SModInfo> *CModManager::GetActiveMods()
{
	return &activatedMods;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::GetAvailableMods @0x285d70
void CModManager::GetAvailableMods( vector<SModInfo> *pMods )
{
	pMods->clear();
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA( ".\\*", &findData );
	if ( hFind == INVALID_HANDLE_VALUE )
		return;
	do
	{
		if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			string szName = findData.cFileName;
			GetModInfo( pMods, szName );  // adds an entry only if description.txt exists
		}
	}
	while ( FindNextFileA( hFind, &findData ) );
	FindClose( hFind );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::Activate(vector<SModInfo>&) @0x285e60 -- re-activate the DB for a chosen mod set.
//   Faithful reconstruction (disasm-verified against the release, the Ghidra decomp garbles the
//   per-mod loop into a fake tail-call): probe every mod's description.txt FIRST (any failure ->
//   return false with the live DB untouched), then tear down (ClearHoldQueue / CloseAllResources /
//   ClearResourceDirs / ClearDatabaseTables), reload the base "game.db" (cwd-relative, exactly the
//   startup path Game/Main.cpp uses) + AddResourceDir(".\res"), then per mod load
//   "<dir>\game.db" and AddResourceDir(<dir>) -- a mod whose game.db fails to load is SKIPPED
//   (release swallows the exception, catch funclet @0x686083) but its dir is still registered.
//   Footer: ++nDataBaseVersion, activatedMods = mods, return true.
bool CModManager::Activate( const vector<SModInfo> &mods )
{
	// release @0x685e7b: nothing active and nothing requested -> nothing to do
	if ( activatedMods.empty() && mods.empty() )
		return true;
	// release @0x685ec0-0x685f2a: probe "<dir>\description.txt" for EVERY mod before any teardown;
	// any failure -> return false without having touched the database
	for ( size_t i = 0; i < mods.size(); ++i )
	{
		string szPath = mods[i].szDirectory + "\\description.txt";
		HANDLE hFile = CreateFileA( szPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
		                            0, OPEN_EXISTING, 0, 0 );
		if ( hFile == INVALID_HANDLE_VALUE )
			return false;
		CloseHandle( hFile );
	}
	// teardown (release @0x685f2c-0x685f3b)
	ClearHoldQueue();							// release @0xd7c50
	NGScene::CloseAllResources();				// release @0x157f20
	NGScene::ClearResourceDirs();				// release @0x157950
	NDatabase::ClearDatabaseTables();			// release @0x3570
	// base reload (release @0x685f40-0x685fb6): Open("game.db","rb",2) + Serialize(READ). The dev
	// Serialize's v1 columnar path performs the record rebuild the release does in the separate
	// NDatabase::Import(true) @0x402e90 call, so no explicit Import is needed here (same idiom as
	// the startup load in Game/Main.cpp). Release catch funclet @0x6860dd on failure.
	try
	{
		CFileStream f;
		f.OpenRead( "game.db" );
		NDatabase::Serialize( f, CStructureSaver::READ );
	}
	catch (...)
	{
		MessageBox( 0, "File game.db not found", "Error", MB_OK );
		return false;
	}
	NGScene::AddResourceDir( ".\\res" );		// release @0x685fb1: literal ".\res"
	// per-mod overlay (release @0x685fbb-0x6860a3): layer "<dir>\game.db" over the database, then
	// register <dir> as a resource dir. A throwing load is swallowed (release funclet @0x686083)
	// and the loop continues -- the failed mod's dir is STILL added, exactly like the release.
	for ( size_t i = 0; i < mods.size(); ++i )
	{
		try
		{
			CFileStream f;
			f.OpenRead( ( mods[i].szDirectory + "\\game.db" ).c_str() );
			NDatabase::Serialize( f, CStructureSaver::READ );
		}
		catch (...)
		{
		}
		NGScene::AddResourceDir( mods[i].szDirectory.c_str() );
	}
	// footer (release @0x6860a8-0x6860c5). The release calls NDb::BuildMapLinks(&status) @0x424150
	// ONCE here; the dev NDatabase::Serialize already runs NDb::BuildMapLinks(false) after every v1
	// columnar load (ADOImport\BasicDB.cpp), and BuildMapLinks is append-only (push_back, no clear),
	// so re-calling it here would duplicate the skeleton-anim / debris / uniform-look / pers-item
	// links -- intentionally omitted.
	++nDataBaseVersion;
	activatedMods = mods;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::Activate(vector<string>&) @0x286110
//   String overload: build a temp SModInfo set (only dirs that actually carry a
//   description.txt -- GetModInfo silently drops the rest), then forward to the
//   SModInfo overload. Returns whatever that overload returns.
bool CModManager::Activate( const vector<string> &dirs )
{
	vector<SModInfo> tmp;
	for ( size_t i = 0; i < dirs.size(); ++i )
		GetModInfo( &tmp, dirs[i] );
	return Activate( tmp );
}
