#ifndef __MODMANAGER_H_
#define __MODMANAGER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModManager -- the engine's "mod manager": enumerates the mod sub-directories
// present next to the game, reads each mod's description.txt, and (Activate)
// rebuilds the database from the base game.db plus every selected mod's game.db.
//
// Reconstructed from .\release\ModManager.obj (Game.exe). CModManager holds only
// STATIC state + STATIC methods (every method is a free __cdecl that reads two
// file-scope globals living in the .obj):
//   * activatedMods    : vector<SModInfo>  (release global @0x9c6af8) -- active set
//   * nDataBaseVersion : int               (release global @0x9c6af4) -- bumped each Activate
// Both live as file-scope statics in ModManager.cpp; there is no instance state,
// so the class is a plain holder (not a CObjectBase, no class registration).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <string>
////////////////////////////////////////////////////////////////////////////////////////////////////
// SModInfo { szName = description.txt text, szDirectory = owning mod folder }.
// (Release layout: two basic_string<char> members; szName is declared first.)
struct SModInfo
{
	string szName;       // description.txt contents
	string szDirectory;  // mod folder
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CModManager
{
public:
	// GetBaseVersion @0x285b70 -- return nDataBaseVersion
	static int GetBaseVersion();
	// GetActiveMods  @0x285b80 -- return &activatedMods
	static vector<SModInfo> *GetActiveMods();
	// GetAvailableMods @0x285d70 -- clear *pMods, then FindFirstFileA(".\\*") and for
	// every FILE_ATTRIBUTE_DIRECTORY entry read its description.txt into *pMods.
	static void GetAvailableMods( vector<SModInfo> *pMods );
	// Activate(vector<SModInfo>&) @0x285e60 -- rebuild the DB from base + chosen mods.
	static bool Activate( const vector<SModInfo> &mods );
	// Activate(vector<string>&)   @0x286110 -- string overload -> SModInfo overload.
	static bool Activate( const vector<string> &dirs );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
