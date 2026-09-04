#ifndef __INTERFACE_STOREPANEL_H_
#define __INTERFACE_STOREPANEL_H_
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
class CStoreSlot;
class CSlotScroll;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStorePanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStorePanel: public CWindow
{
	OBJECT_BASIC_METHODS(CStorePanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CButton> pClose;
	CPtr<CButton> pArrange;   // release save-format tag 4; posts CCmdUpdateStore
	CObj<CStoreSlot> pStoreSlot;
	CObj<CSlotScroll> pStoreSlotView;
	CObj<CComplexButtonFlash> pSMG;
	CObj<CComplexButtonFlash> pOthers;
	CObj<CComplexButtonFlash> pRifles;
	CObj<CComplexButtonFlash> pPistols;
	CObj<CComplexButtonFlash> pGrenades;
	CObj<CComplexButtonFlash> pColdSteel;
	CObj<CComplexButtonFlash> pPKWeapons;
	CObj<CComplexButtonFlash> pHeavyWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pClose); f.Add(4,&pArrange); f.Add(5,&pStoreSlot); f.Add(6,&pStoreSlotView); f.Add(7,&pSMG); f.Add(8,&pOthers); f.Add(9,&pRifles); f.Add(10,&pPistols); f.Add(11,&pGrenades); f.Add(12,&pColdSteel); f.Add(13,&pPKWeapons); f.Add(14,&pHeavyWeapon); return 0; }

protected:
	void UpdateButtons();

public:
	CStorePanel() {}
	CStorePanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
