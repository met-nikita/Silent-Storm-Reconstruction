#ifndef __INTERFACE_UNITPANEL_H_
#define __INTERFACE_UNITPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
class CAckEvent;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitsTabBar;
class CUnitIconsBar;
class CLevelSwitchBar;
class CInfoPanelSingleUnit;
class CInfoPanelMultipleUnits;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitPanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CButton> pEndOfTurn;
	CPtr<CButton> pStartOfTurn;
	CObj<CUnitIconsBar> pUnitIconsBar;
	CObj<CLevelSwitchBar> pLevelSwitchBar;
	////
	CPtr<CImage> pBackgroundSingleUnit;
	CPtr<CImage> pBackgroundMultipleUnits;
	CObj<CUnitsTabBar> pUnitsTabBar;
	CObj<CInfoPanelSingleUnit> pInfoPanelSingleUnit;
	CObj<CInfoPanelMultipleUnits> pInfoPanelMultipleUnits;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pEndOfTurn); f.Add(4,&pStartOfTurn); f.Add(5,&pUnitIconsBar); f.Add(6,&pLevelSwitchBar); f.Add(7,&pBackgroundSingleUnit); f.Add(8,&pBackgroundMultipleUnits); f.Add(9,&pUnitsTabBar); f.Add(10,&pInfoPanelSingleUnit); f.Add(11,&pInfoPanelMultipleUnits); f.Add(12,&pBackgroundEmpty); return 0; }
	CPtr<CImage> pBackgroundEmpty;

public:
	CUnitPanel() {}
	CUnitPanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	// retail CUnitPanel::PlayAckEvent @0x2563e0 -> the single-unit face's ack animation (the bottom-left
	// 3D portrait turns to the camera to deliver the bark, temporarily swapping in a non-selected speaker)
	void PlayAckEvent( const STime &sTime, CAckEvent *pEvent );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
