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
// IChapterMap
////////////////////////////////////////////////////////////////////////////////////////////////////
class IChapterMap: public NMainLoop::IInterfaceBase
{
public:
	virtual NUI::ICursor* GetCursor() const = 0;
	virtual NUI::CInterface* GetInterface() const = 0;

	virtual NRPG::CGlobalGame* GetGlobalGame() const = 0;
	virtual NRPG::CGlobalPlayer* GetGlobalPlayer() const = 0;
	virtual NSound::ISoundScene* GetSoundScene() const = 0;
	virtual NDb::CChapterMap* GetChapterMap() const = 0;
	virtual CPtrFuncBase<CChapterInfo>* GetChapterInfo() const = 0;
};
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