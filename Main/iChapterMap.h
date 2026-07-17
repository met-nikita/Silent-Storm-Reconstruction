#ifndef __ICHAPTERMAP_H_
#define __ICHAPTERMAP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
#include "ChapterInfo.h"
namespace NDb
{
	class CChapterMap;
}
namespace NUI
{
	class ICursor;
	class CInterface;
}
namespace NRPG
{
	class CGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: there is NO NGame::IChapterMap here, and there is none in retail either -- it is absent from
// Game.pdb entirely (checked every one of the 10509 UDTs; the only survivals are the stale mangled
// names in the legacy Main/FastDebug.def + Main/ReleaseDll.def export tables, snapshots from before
// the fold -- neither .def is referenced by CMakeLists.txt, which only feeds third_party defs to
// lib.exe). It was a Jan03-era artifact this fork kept. Retail instead derives
// NGame::CChapterMap from NGame::CMissionBase (PDB: CChapterMap size 296, base CMissionBase size
// 264 -> own members from 0x108) and folds the old IChapterMap accessors into the common mission
// interface, so every holder just keeps a CPtr<NGame::IMission> (CChapterMapUI::operator& @0x1af170
// tag 2 is a CPtr<NGame::IMission>). See iChapterMap.cpp for the reparent + IMission::GetChapterMap /
// GetChapterInfo (retail mission vtbl+0x14c/+0x150) in iMission.h.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICBeginChapter: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICBeginChapter);
private:
	int nTemplateID;
	CPtr<NRPG::CGlobalGame> pGlobalGame;

public:
	CICBeginChapter() {}
	CICBeginChapter( int nTemplateID, NRPG::CGlobalGame *_pGlobalGame );
	void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICContinueChapter: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICContinueChapter);
private:
	CPtr<NRPG::CGlobalGame> pGlobalGame;

public:
	CICContinueChapter() {}
	CICContinueChapter( NRPG::CGlobalGame *_pGlobalGame );
	void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif