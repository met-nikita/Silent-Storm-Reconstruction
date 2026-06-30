#ifndef __A5_GLOBALMAP_UI_H__
#define __A5_GLOBALMAP_UI_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class ILight;
	class IGameView;
}
namespace NGame
{
	class IGlobalMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
class CFlashButton;
class CGlobalSector;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalMapUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CGlobalMapUI);
private:
	ZDATA_(CWindow)
	CPtr<NGame::IGlobalMap> pGlobal;
	////
	SCursorInfo sCursor;
	////
	CObj<CGlobalInfo> pInfo;
	////
	CObj<CImage> pBackground;
	CObj<CWindow> pMapView;
	CObj<CFlashButton> pReturn;
	CObj<CFlashButton> pBaseZone;
	vector<CObj<CGlobalSector> > sectorsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGlobal); f.Add(3,&sCursor); f.Add(4,&pInfo); f.Add(5,&pBackground); f.Add(6,&pMapView); f.Add(7,&pReturn); f.Add(8,&pBaseZone); f.Add(9,&sectorsSet); return 0; }

protected:
	void GetGlobalSectorInfo( const SGlobalSector &sSector, bool *pbVisible, bool *pRecommended );

public:
	CGlobalMapUI() {}
	CGlobalMapUI( const SWindowInfo &sInfo, NGame::IGlobalMap *pGlobal );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
