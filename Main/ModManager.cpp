#include "StdAfx.h"
#include "ModManager.h"
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
// CModManager::Activate(vector<SModInfo>&) @0x285e60   [DEFERRED -- sibling BLOCKED]
//   Faithful body re-activates the DB for a chosen mod set: probe each mod's
//   description.txt, then ClearHoldQueue / NGScene::CloseAllResources /
//   NGScene::ClearResourceDirs / ClearDatabaseTables, load the base game.db
//   (NDatabase::Serialize+Import) + NGScene::AddResourceDir, layer every mod's
//   game.db, NDb::BuildMapLinks, ++nDataBaseVersion, activatedMods = mods.
//   NGScene::ClearResourceDirs and ClearDatabaseTables are release-only and ABSENT
//   from the dev source, so this path is stubbed until they are reconstructed.
//   Nothing in the dev tree calls Activate yet -> the stub is behaviour-neutral.
bool CModManager::Activate( const vector<SModInfo> &mods )
{
	// TODO(convergence): wire ClearHoldQueue / NGScene::{CloseAllResources,
	// ClearResourceDirs, AddResourceDir} / ClearDatabaseTables / NDatabase load /
	// NDb::BuildMapLinks once the absent release-only helpers are reconstructed.
	(void)mods;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager::Activate(vector<string>&) @0x286110   [DEFERRED -- forwards to stub]
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
