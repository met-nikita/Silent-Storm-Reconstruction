#ifndef __INTERFACE_CHARACTERPANEL_H_
#define __INTERFACE_CHARACTERPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class IGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
class CInfoPanelCritical;   // iCriticalIcons.h (registered 0xB0241944); CObj serializes by id -> fwd-decl suffices
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCharacterPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCharacterPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CCharacterPanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CText> pLevel;
	CPtr<CImage> pClass;
	CPtr<CProgressBar> pLevelBar;
	//// Primary
	CPtr<CText> pEvasion;
	CPtr<CText> pStrength;
	CPtr<CText> pDexterity;
	CPtr<CText> pIntelligence;
	CPtr<CText> pActionPoints;
	CPtr<CText> pVitalityPoints;
	CPtr<CProgressBar> pEvasionBar;
	CPtr<CProgressBar> pStrengthBar;
	CPtr<CProgressBar> pDexterityBar;
	CPtr<CProgressBar> pIntelligenceBar;
	CPtr<CProgressBar> pActionPointsBar;
	CPtr<CProgressBar> pVitalityPointsBar;
	//// Secondary
	CPtr<CText> pHide;
	CPtr<CText> pSpot;
	CPtr<CText> pBurst;
	CPtr<CText> pMelee;
	CPtr<CText> pSnipe;
	CPtr<CText> pMedicine;
	CPtr<CText> pShooting;
	CPtr<CText> pThrowing;
	CPtr<CText> pInterrupt;
	CPtr<CText> pEngineering;
	CPtr<CProgressBar> pHideBar;
	CPtr<CProgressBar> pSpotBar;
	CPtr<CProgressBar> pBurstBar;
	CPtr<CProgressBar> pMeleeBar;
	CPtr<CProgressBar> pSnipeBar;
	CPtr<CProgressBar> pMedicineBar;
	CPtr<CProgressBar> pShootingBar;
	CPtr<CProgressBar> pThrowingBar;
	CPtr<CProgressBar> pInterruptBar;
	CPtr<CProgressBar> pEngineeringBar;
	//// Controls
	// release operator& @0x1b33d0: tags 39-42 are pClose(CHoverButton)/pPerks(CHoverFlashButton)/
	// pMedals(CHoverButton)/pBiography(CHoverButton). The Jan03 tree carried a "background" CButton
	// in slot 42 -- retail replaced it with the biography tab button (no pBackground member exists).
	CPtr<CHoverButton> pClose;
	CPtr<CHoverFlashButton> pPerks;
	CPtr<CHoverButton> pMedals;
	CPtr<CHoverButton> pBiography;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pLevel); f.Add(4,&pFullName); f.Add(5,&pClass); f.Add(6,&pLevelBar); f.Add(7,&pEvasion); f.Add(8,&pStrength); f.Add(9,&pDexterity); f.Add(10,&pIntelligence); f.Add(11,&pActionPoints); f.Add(12,&pVitalityPoints); f.Add(13,&pEvasionBar); f.Add(14,&pStrengthBar); f.Add(15,&pDexterityBar); f.Add(16,&pIntelligenceBar); f.Add(17,&pActionPointsBar); f.Add(18,&pVitalityPointsBar); f.Add(19,&pHide); f.Add(20,&pSpot); f.Add(21,&pBurst); f.Add(22,&pMelee); f.Add(23,&pSnipe); f.Add(24,&pMedicine); f.Add(25,&pShooting); f.Add(26,&pThrowing); f.Add(27,&pInterrupt); f.Add(28,&pEngineering); f.Add(29,&pHideBar); f.Add(30,&pSpotBar); f.Add(31,&pBurstBar); f.Add(32,&pMeleeBar); f.Add(33,&pSnipeBar); f.Add(34,&pMedicineBar); f.Add(35,&pShootingBar); f.Add(36,&pThrowingBar); f.Add(37,&pInterruptBar); f.Add(38,&pEngineeringBar); f.Add(39,&pClose); f.Add(40,&pPerks); f.Add(41,&pMedals); f.Add(42,&pBiography); f.Add(43,&pUnit); f.Add(44,&bLastPanelState); f.Add(45,&pEvasionIcon); f.Add(46,&pStrengthIcon); f.Add(47,&pDexterityIcon); f.Add(48,&pIntelligenceIcon); f.Add(49,&pActionPointsIcon); f.Add(50,&pVitalityPointsIcon); f.Add(51,&pHideIcon); f.Add(52,&pSpotIcon); f.Add(53,&pBurstIcon); f.Add(54,&pMeleeIcon); f.Add(55,&pSnipeIcon); f.Add(56,&pMedicineIcon); f.Add(57,&pShootingIcon); f.Add(58,&pThrowingIcon); f.Add(59,&pInterruptIcon); f.Add(60,&pEngineeringIcon); f.Add(61,&criticalIconsSet); return 0; }
	CPtr<CText> pFullName;
	CPtr<NGame::IUnitTracker> pUnit;
	bool bLastPanelState = false;
	CPtr<CImage> pEvasionIcon;
	CPtr<CImage> pStrengthIcon;
	CPtr<CImage> pDexterityIcon;
	CPtr<CImage> pIntelligenceIcon;
	CPtr<CImage> pActionPointsIcon;
	CPtr<CImage> pVitalityPointsIcon;
	CPtr<CImage> pHideIcon;
	CPtr<CImage> pSpotIcon;
	CPtr<CImage> pBurstIcon;
	CPtr<CImage> pMeleeIcon;
	CPtr<CImage> pSnipeIcon;
	CPtr<CImage> pMedicineIcon;
	CPtr<CImage> pShootingIcon;
	CPtr<CImage> pThrowingIcon;
	CPtr<CImage> pInterruptIcon;
	CPtr<CImage> pEngineeringIcon;
	vector<CObj<CInfoPanelCritical> > criticalIconsSet;

public:
	CCharacterPanel() {}
	CCharacterPanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
