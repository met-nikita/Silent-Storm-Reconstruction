#ifndef __CHAPTERMAP_UI_H__
#define __CHAPTERMAP_UI_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iDesktopWindow.h"		// NUI::CDesktopWindow -- the CChapterMapUI base (retail @0x1af170 tag 1)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CDBScenarioZone;
}
namespace NGame
{
	class IMission;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTeamMarker;
class CFlashButton;
class CChapterSector;
class CDescriptionText;
class CLogPanel;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail NUI::CChapterMapUI derives from CDesktopWindow (PDB: NUI::CChapterMapUI size 240, base
// NUI::CDesktopWindow size 140 -- base NUI::CWindow), and its operator& @0x1af170 emits the base as
// tag 1 via CallObjectSerialize<NUI::CDesktopWindow>. This fork previously kept it a plain CWindow
// and faked the base chunk with an SBaseChunk shim that nested the CWindow at inner tag 1 -- byte-
// compatible for tag 1.1, but it dropped CDesktopWindow's OWN tags 2/3/4 (pClientWindow,
// pActiveEvent, pNextAckEvent -- retail CDesktopWindow::operator& @0x1aeee0), which the wire audit
// flagged UNREAD on slots 5/10. Reparented onto the fork's real CDesktopWindow (iDesktopWindow.h),
// whose operator& already matches @0x1aeee0 tag-for-tag, so the whole base chunk now round-trips.
class CChapterMapUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CChapterMapUI);
private:
	ZDATA_(CDesktopWindow)
	// retail @0x1af170 tag 2 is a CPtr<NGame::IMission> (PDB-typed). Retail has no NGame::IChapterMap
	// at all -- NGame::CChapterMap simply IS a CMissionBase, and the chapter accessors live on the
	// common mission interface (IMission::GetChapterMap / GetChapterInfo, retail mission vtbl+0x14c /
	// +0x150 -- see iMission.h), so every holder here just keeps the mission pointer.
	CPtr<NGame::IMission> pChapter;
	////
	SCursorInfo sCursor;
	////
	CObj<CChapterInfo> pInfo;
	////
	float fXK;
	float fYK;
	float fPassedPathLen;
	CVec2 vCurrentPos, vTargetPos;
	STime sLastUpdateTime;
	////
	CObj<CImage> pBackground;
	CObj<CWindow> pMapView;
	CObj<CTeamMarker> pTeamMarker;
	CObj<CFlashButton> pShowGlobal;
	CObj<CDescriptionText> pTextLeft;
	CObj<CDescriptionText> pTextRight;
	vector<CObj<CChapterSector> > sectorsSet;
	// retail base tag 18: the "special frame" counter. Draw @0x1aac20 does nWaitForSpecialFrame++ and
	// drains the scenario tracker's just-found clues ONLY on the second frame (== 2), so the clue
	// popups fire once, just after the map's first frame is on screen. The dev Draw used to drain
	// every frame (relying solely on IsJustFound/SetJustFound to self-limit).
	int nWaitForSpecialFrame;
	// retail base tag 19: the chapter map's own log panel, built on EVENT_TEMPLATELOAD from the
	// loader's "logpanel" control with STREAM_GAME (decomp @0x1abd20).
	CObj<CLogPanel> pLogPanel;
	// retail NUI::CChapterMapUI::operator& @0x1af170 -- 19 tags, tag 1 = the CDesktopWindow base
	// chunk (emitted non-polymorphically via CallObjectSerialize<NUI::CDesktopWindow>).
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CDesktopWindow*)this);		// retail CDesktopWindow base chunk (@0x1aeee0)
		f.Add(2,&pChapter); f.Add(3,&sCursor); f.Add(4,&pInfo);
		f.Add(5,&fXK); f.Add(6,&fYK); f.Add(7,&fPassedPathLen); f.Add(8,&vCurrentPos); f.Add(9,&vTargetPos); f.Add(10,&sLastUpdateTime);
		f.Add(11,&pBackground); f.Add(12,&pMapView); f.Add(13,&pTeamMarker); f.Add(14,&pShowGlobal); f.Add(15,&pTextLeft); f.Add(16,&pTextRight);
		f.Add(17,&sectorsSet);
		f.Add(18,&nWaitForSpecialFrame); f.Add(19,&pLogPanel);
		return 0;
	}

protected:
	NDb::CDBScenarioZone* FindScenarioZone( const SChapterSector &sSector, bool *pRecommended );

public:
	CChapterMapUI() {}
	CChapterMapUI( const SWindowInfo &sInfo, NGame::IMission *pChapter );

	void SetTarget( const CVec2 &_vTargetPos );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
