#ifndef __CHAPTERMAP_UI_H__
#define __CHAPTERMAP_UI_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CDBScenarioZone;
}
namespace NGame
{
	class IChapterMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTeamMarker;
class CFlashButton;
class CChapterSector;
class CDescriptionText;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChapterMapUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CChapterMapUI);
private:
	ZDATA_(CWindow)
	CPtr<NGame::IChapterMap> pChapter;
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pChapter); f.Add(3,&sCursor); f.Add(4,&pInfo); f.Add(5,&fXK); f.Add(6,&fYK); f.Add(7,&fPassedPathLen); f.Add(8,&vCurrentPos); f.Add(9,&vTargetPos); f.Add(10,&sLastUpdateTime); f.Add(11,&pBackground); f.Add(12,&pMapView); f.Add(13,&pTeamMarker); f.Add(14,&pShowGlobal); f.Add(15,&pTextLeft); f.Add(16,&pTextRight); f.Add(17,&sectorsSet); return 0; }

protected:
	NDb::CDBScenarioZone* FindScenarioZone( const SChapterSector &sSector, bool *pRecommended );

public:
	CChapterMapUI() {}
	CChapterMapUI( const SWindowInfo &sInfo, NGame::IChapterMap *pChapter );

	void SetTarget( const CVec2 &_vTargetPos );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
