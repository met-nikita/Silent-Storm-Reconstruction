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
	CPtr<CText> pName;
	CPtr<CButton> pClose;
	CPtr<CButton> pArrange;
	CObj<CBackPackSlot> pBackPack;
	CObj<CUnitModelShow> pUnitModelShow;
	CPtr<CComplexButton> pUnload;
	// retail operator& @0x1f01c0: tag 4 = pName (CText), 5..9 = pClose/pArrange/pBackPack/
	// pUnitModelShow/pUnload; the shipped binary has NO pRepair (v1.2 save record 0xB0521143
	// carries exactly tags 1..9 in this order). The old dev map read every widget one tag
	// early and pUnload got a CUnitModelShow ref -> null -> SetChecked crash on load.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pName); f.Add(5,&pClose); f.Add(6,&pArrange); f.Add(7,&pBackPack); f.Add(8,&pUnitModelShow); f.Add(9,&pUnload); return 0; }

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
