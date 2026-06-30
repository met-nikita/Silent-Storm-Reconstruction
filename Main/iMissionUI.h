#ifndef __A5_MISSIONINTERFACE_PC_H__
#define __A5_MISSIONINTERFACE_PC_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iDesktopWindow.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckIcon;
class CItemText;
class CEnemyIcon;
class CHitTracker;
class CTopBar;
class CLogPanel;
class CUnitPanel;
class CPerksPanel;
class CStorePanel;
class CInventoryPanel;
class CCharacterPanel;
class CHoverButton;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMissionUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CMissionUI);
private:
	NInput::CBind bindCancel, bindShowItems;
	NInput::CBind bindPerks, bindStore, bindInventory, bindCharacter;
	NInput::CBind bindPoseSubMenu, bindWeaponModeSubMenu, bindGrenadeModeSubMenu;

	ZDATA_(CDesktopWindow)
	CPtr<NGame::IMission> pMission;
	////
	list<CObj<CItemText> > itemTextsList;
	list<CObj<CEnemyIcon> > enemyIconsList;
	////
	STime sCameraScrollUpdate;
	////
	CPtr<CText> pPause;   // the HUD "PAUSE" indicator (game.db missionUI container 123, control "pause") is a
	                      // UI_TEXT control (StringID 20992), so it is a CText -- NOT a CImage. GetUIWindow<CImage>
	                      // dynamic_cast-failed and returned a throwaway dummy, leaving the real (Visible=1) text
	                      // un-hideable -> "PAUSE" stuck on-screen. Update() hides this per-frame unless paused.
	CObj<CTopBar> pTopBar;
	CPtr<CAckIcon> pAck;
	CObj<CLogPanel> pLogPanel;
	CObj<CUnitPanel> pUnitPanel;
	CObj<CPerksPanel> pPerksPanel;
	CObj<CStorePanel> pStorePanel;
	CObj<CHoverButton> pInventory;
	CObj<CHoverButton> pCharacter;
	CObj<CInventoryPanel> pInventoryPanel;
	CObj<CCharacterPanel> pCharacterPanel;
	list<CObj<CHitTracker> > hitsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pMission); f.Add(3,&itemTextsList); f.Add(4,&enemyIconsList); f.Add(5,&sCameraScrollUpdate); f.Add(6,&pPause); f.Add(7,&pTopBar); f.Add(8,&pAck); f.Add(9,&pLogPanel); f.Add(10,&pUnitPanel); f.Add(11,&pPerksPanel); f.Add(12,&pStorePanel); f.Add(13,&pInventory); f.Add(14,&pCharacter); f.Add(15,&pInventoryPanel); f.Add(16,&pCharacterPanel); f.Add(17,&hitsList); return 0; }

protected:
	void UpdateHits( const STime &sTime );
	void UpdateItems( NGScene::I2DGameView *pView );
	void UpdateEnemies();
	void UpdateCameraScroll( const STime &sTime );
	CAckEvent* PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent );

public:
	CMissionUI();
	CMissionUI( const SWindowInfo &sInfo, NGame::IMission *pMission );

	NGame::CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd );

	bool ProcessEvent( const NInput::SEvent &sEvent );
	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
