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
	CPtr<CButton> pArrange;   // release save-format tag 4 (inserted before pStoreSlot); dead-in-dev
	CObj<CStoreSlot> pStoreSlot;
	CObj<CSlotScroll> pStoreSlotView;
	//// release retypes these 8 to CObj<CComplexButtonFlash>; kept as CObj<CComplexButton> (polymorphic
	//// base) -- CComplexButtonFlash now exists+registered so release Flash-button saves load; the
	//// Flash-construction (create CComplexButtonFlash) is the deferred behavior half.
	CObj<CComplexButton> pSMG;
	CObj<CComplexButton> pOthers;
	CObj<CComplexButton> pRifles;
	CObj<CComplexButton> pPistols;
	CObj<CComplexButton> pGrenades;
	CObj<CComplexButton> pColdSteel;
	CObj<CComplexButton> pPKWeapons;
	CObj<CComplexButton> pHeavyWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pClose); f.Add(4,&pArrange); f.Add(5,&pStoreSlot); f.Add(6,&pStoreSlotView); f.Add(7,&pSMG); f.Add(8,&pOthers); f.Add(9,&pRifles); f.Add(10,&pPistols); f.Add(11,&pGrenades); f.Add(12,&pColdSteel); f.Add(13,&pPKWeapons); f.Add(14,&pHeavyWeapon); return 0; }

protected:
	void UpdateButtons();

public:
	CStorePanel() {}
	CStorePanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
