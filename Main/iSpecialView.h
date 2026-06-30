#ifndef __A5_SPECIALVIEW_H__
#define __A5_SPECIALVIEW_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Module iSpecialView (.\release\iSpecialView.obj) -- the X-COM / global "map" special view: a
// CDesktopWindow (CXComMapUI) that hosts a free-spinnable 3D "earth" globe (CEarthView, a CModel)
// framed by six rotate/zoom slide buttons (CEarthControl + CSlideButton). Retail-only rework of the
// CGlobalMapUI predecessor (iGlobalMapUI.*, RETAINED -- do not merge).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class IGameView;
}
namespace NGame
{
	class IMission;
}
namespace NRPG
{
	class CGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlideButton -- a CHoverButton whose held/released edges drive a scalar fValue "slider".
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlideButton: public CHoverButton
{
	OBJECT_BASIC_METHODS(CSlideButton)
private:
	ZDATA_(CHoverButton)
	bool bLastState;
	bool bActive;
	float fValue;
	STime sLastTime;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&bLastState); f.Add(3,&bActive); f.Add(4,&fValue); f.Add(5,&sLastTime); return 0; }

public:
	CSlideButton() {}
	CSlideButton( const SWindowInfo &sInfo );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView -- a CModel-derived rotatable globe ( NDb::GetRPGItem(0x1E7) ) free-spun by mouse drag.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEarthView: public CModel
{
	OBJECT_NOCOPY_METHODS(CEarthView);
private:
	ZDATA_(CModel)
	bool bButtonDown;
	float fAngleX;
	float fLastAngleX;
	float fAngleY;
	float fLastAngleY;
	SPoint sLastPoint;
	CTimeCounter sTimer;
	CObj<CObjectBase> pMouseCapture;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CModel*)this); f.Add(2,&bButtonDown); f.Add(3,&fAngleX); f.Add(4,&fLastAngleX); f.Add(5,&fAngleY); f.Add(6,&fLastAngleY); f.Add(7,&sLastPoint); f.Add(8,&sTimer); f.Add(9,&pMouseCapture); return 0; }

protected:
	void UpdateMatrix();

public:
	CEarthView() {}
	CEarthView( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthControl -- a CWindow container framing the globe with six rotate/zoom CSlideButtons.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEarthControl: public CWindow
{
	OBJECT_NOCOPY_METHODS(CEarthControl);
private:
	ZDATA_(CWindow)
	CPtr<CEarthView> pView;
	CPtr<CButton> pRXPlus;
	CPtr<CButton> pRYPlus;
	CPtr<CButton> pRXMinus;
	CPtr<CButton> pRYMinus;
	CPtr<CButton> pZoomPlus;
	CPtr<CButton> pZoomMinus;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pView); f.Add(3,&pRXPlus); f.Add(4,&pRYPlus); f.Add(5,&pRXMinus); f.Add(6,&pRYMinus); f.Add(7,&pZoomPlus); f.Add(8,&pZoomMinus); return 0; }

public:
	CEarthControl() {}
	CEarthControl( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CXComMapUI -- the CDesktopWindow strategic-map screen hosting CEarthView + CEarthControl.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CXComMapUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CXComMapUI);
private:
	ZDATA_(CDesktopWindow)
	CPtr<NGame::IMission> pGlobal;
	SCursorInfo sCursor;
	CObj<CGlobalInfo> pInfo;
	CObj<CWindow> pMapView;
	CObj<CEarthView> pEarth;
	CObj<CEarthControl> pEarthControl;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pGlobal); f.Add(3,&sCursor); f.Add(4,&pInfo); f.Add(5,&pMapView); f.Add(6,&pEarth); f.Add(7,&pEarthControl); return 0; }

protected:
	void GetGlobalSectorInfo( const SGlobalSector &sSector, bool *pbVisible, bool *pRecommended );

public:
	CXComMapUI() {}
	CXComMapUI( const SWindowInfo &sInfo, NGame::IMission *pGlobal );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
