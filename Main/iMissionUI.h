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
class CSoundIcon;	// the heard-not-seen "ear" marker (retail NUI::CSoundIcon)
class CClueIcon;	// the discovered CLUE-item marker (retail NUI::CClueIcon, item flavor @0x211410)
class CHintIcon;	// the discovered HINT-item marker (retail NUI::CHintIcon @0x2114a0)
class CTrapIcon;	// the known trap/mine marker (retail NUI::CTrapIcon @0x211530, texture 940)
class CHitTracker;
class CTopBar;
class CLogPanel;
class CUnitPanel;
class CPerksPanel;
class CStorePanel;
class CInventoryPanel;
class CCharacterPanel;
class CMedalsPanel;
class CBiographyPanel;
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
	NInput::CBind bindMedals, bindBiography;	// retail ProcessEvent @0x20f7a0 routes all four tabs
	NInput::CBind bindPoseSubMenu, bindWeaponModeSubMenu, bindGrenadeModeSubMenu;

	ZDATA_(CDesktopWindow)
	CPtr<NGame::IMission> pMission;
	////
	list<CObj<CItemText> > itemTextsList;
	list<CObj<CEnemyIcon> > enemyIconsList;
	// retail CMissionUI::UpdateVisibleUnits @0x213e70 keeps a SECOND marker list next to the
	// unit icons: the "ear" markers over heard-not-seen units (dev CSoundIcon, textures 666-674).
	list<CObj<CSoundIcon> > clueIconsList;
	// retail CMissionUI::UpdateVisibleItems @0x2130c0 rebuilds TWO more overlay lists next to the
	// ground-item labels (retail members +0xf8 hintIconsList / +0xfc clueItemIconsList; serialized
	// as tags 7/8 in retail operator& @0x218dc0): CHintIcon over every discovered NRPG::IHintItem
	// (gated by "ui_showhints"), CClueIcon over every discovered NRPG::IClueItem (ungated).
	list<CObj<CHintIcon> > hintIconsList;
	list<CObj<CClueIcon> > clueItemIconsList;
	// retail CMissionUI::UpdateTrappedObjects @0x214990 (serialized as tag 10 in retail operator&
	// @0x218dc0; dev tag 23): one CTrapIcon per entry of the active player's trapped-objects list
	// (own armed traps + spotted enemy mines), fixed texture 940, anchored at the trap pos z+0.6.
	list<CObj<CTrapIcon> > trapIconsList;
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
	// retail CMissionUI hosts the medals + biography sub-panels too (@0x214d40 TEMPLATELOAD:
	// "medalspanel"/"biographypanel"; serialized in retail operator& @0x218dc0)
	CObj<CMedalsPanel> pMedalsPanel;
	CObj<CBiographyPanel> pBiographyPanel;
	list<CObj<CHitTracker> > hitsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pMission); f.Add(3,&itemTextsList); f.Add(4,&enemyIconsList); f.Add(5,&sCameraScrollUpdate); f.Add(6,&pPause); f.Add(7,&pTopBar); f.Add(8,&pAck); f.Add(9,&pLogPanel); f.Add(10,&pUnitPanel); f.Add(11,&pPerksPanel); f.Add(12,&pStorePanel); f.Add(13,&pInventory); f.Add(14,&pCharacter); f.Add(15,&pInventoryPanel); f.Add(16,&pCharacterPanel); f.Add(17,&hitsList); f.Add(18,&clueIconsList); f.Add(19,&pMedalsPanel); f.Add(20,&pBiographyPanel); f.Add(21,&hintIconsList); f.Add(22,&clueItemIconsList); f.Add(23,&trapIconsList); return 0; }

protected:
	void UpdateHits( const STime &sTime );
	void UpdateItems( NGScene::I2DGameView *pView );
	void UpdateEnemies();
	void UpdateClues();
	void UpdateTraps();		// retail CMissionUI::UpdateTrappedObjects @0x214990
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
