#ifndef __A5_SAVEMANAGER_H_
#define __A5_SAVEMANAGER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GPixelFormat.h"
#include "..\Misc\2DArray.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileStream;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMainLoop
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.x save magic (the Jan03 0x818B9F21 bumped byte-by-byte, +0x01010101). Retail also
// still accepts the old v1.0 value on read (dual check in CICLoad::Exec / GetSlotScreenShot),
// but this fork targets retail v1.x saves only, so only the v1.x magic is accepted/written.
const int N_SAVE_MAGIC_NUMBER = 0x828CA022;
// The old v1.0 magic. retail CICLoad::Exec @0x1f5fd0 does a DUAL check (nMagic != 0x818B9F21 &&
// nMagic != 0x828CA022 -> throw) and then parses BOTH identically -- the format is the same, only
// the magic was byte-bumped +0x01010101. Some autosaves/quicksaves in a v1.2 save dir still carry it.
const int N_SAVE_MAGIC_NUMBER_V0 = 0x818B9F21;
const int N_SAVE_SCREENSHOT_X = 320;
const int N_SAVE_SCREENSHOT_Y = 200;
const char S_SLOT_ACTIVE[] = "temp";
const char S_SAVE_FILENAME[] = "game.sav";
const char S_SLOT_QUICKSAVE[] = "quicksave";
const char S_INVALID_SAVE_CHARS[] = ".<>\\/|\"*^:?";
////////////////////////////////////////////////////////////////////////////////////////////////////
// SSaveFileHeader
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSaveFileHeader
{
	int nMagic;
	// retail v1.x repurposed the old checksum slot as the ACTIVE-MOD count: that many
	// mod-directory strings follow the header (CDataStream string format), and retail
	// CICLoad::Exec @0x1f5fd0 reads them and runs CModManager::Activate before the
	// CStructureSaver pass (CICSave @0x1f6390 writes GetActiveMods' szDirectory list).
	int nMods;

	NGfx::SPixel8888 sScreenShot[N_SAVE_SCREENSHOT_Y][N_SAVE_SCREENSHOT_X];
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveManager
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveManager
{
private:
	int nActiveSlotID;
	string szActiveProfile;

public:
	CSaveManager();

//// Profiles
	void CreateProfile( const string &szProfile ) const;
	void DeleteProfile( const string &szProfile ) const;
	void GetProfilesList( list<string> *pList ) const;

	const string& GetActiveProfile() const;
	void SetActiveProfile( const string &szProfile );

//// Slots
	void SaveSlot( const string &szName );
	void LoadSlot( const string &szName );
	void ClearSlot( const string &szName );
	void DeleteSlot( const string &szName );
	void PrepareSlot( const string &szName );
	void GetSlotsList( list<string> *pList ) const;

	void GetSlotTime( const string &szName, wstring *pTime );
	void GetSlotScreenShot( const string &szName, CArray2D<NGfx::SPixel8888> *pScreenShot );

	string GetSlotFilePath( const string &szName, const string &szFileName ) const;
};
////
CSaveManager* GetSaveManager();
////
// Release profile free-fns (iSaveManager.obj): the profile UI talks to these, not the CSaveManager
// methods.  Dir ops delegate to the manager; the ACTIVE profile lives in the NGlobal "game_profile"
// var (release divergence from the dev CSaveManager::szActiveProfile member).
void CreateProfile( const string &szProfile );      // @0x235fe0
void DeleteProfile( const string &szProfile );
void GetProfilesList( list<string> *pList );         // @0x2368e0
string GetActiveProfile();                           // @0x236e90 -- validates against the list
void SetActiveProfile( const string &szProfile );    // @0x235c70
void MakeDefaultProfile();                           // @0x236d70 -- ensure the default profile exists
bool IsValidCustomName( const string &szName );      // @0x235d00 -- typed save name not among the reserved slot names (DB 20241..20244)
////
void CreateDir( const string &szDir );
void RemoveDir( const string &szDir );
void CopyFiles( const string &szSource, const string &szTarget, const string &szMask );
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif