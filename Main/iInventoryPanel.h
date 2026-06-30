#ifndef __INTERFACE_INVENTORYPANEL_H_
#define __INTERFACE_INVENTORYPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class IMissionGame;
	class IUnitTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBackPackSlot;
class CUnitModelShow;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInventoryPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInventoryPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CInventoryPanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<NGame::IUnitTracker> pUnit;
	////
	CPtr<CButton> pClose;
	CPtr<CButton> pArrange;
	CObj<CBackPackSlot> pBackPack;
	CObj<CUnitModelShow> pUnitModelShow;
	CPtr<CComplexButton> pUnload;
	CPtr<CComplexButton> pRepair;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pClose); f.Add(5,&pArrange); f.Add(6,&pBackPack); f.Add(7,&pUnitModelShow); f.Add(8,&pUnload); f.Add(9,&pRepair); return 0; }

public:
	CInventoryPanel() {}
	CInventoryPanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
