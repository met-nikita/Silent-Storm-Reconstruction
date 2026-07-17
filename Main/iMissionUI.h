#ifndef __A5_MISSIONINTERFACE_PC_H__
#define __A5_MISSIONINTERFACE_PC_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iDesktopWindow.h"
#include "iPlayerSwitch.h"		// NUI::CPlayerSwitchUI (retail iPlayerSwitch.obj) -- CMissionUI serializes it (retail tag 25)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class IPlayerTracker;
}
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
class CText;
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
	// @0x218dc0): one CTrapIcon per entry of the active player's trapped-objects list
	// (own armed traps + spotted enemy mines), fixed texture 940, anchored at the trap pos z+0.6.
	list<CObj<CTrapIcon> > trapIconsList;
	// retail tag 9 (clueUnitIconsList @+0xfc): CClueIcon markers over heard-not-seen UNITS. This
	// fork covers that role with the CSoundIcon ear markers (clueIconsList -> retail soundIconsList,
	// tag 11), so this list is carried empty for save-format parity and never populated here.
	list<CObj<CClueIcon> > clueUnitIconsList;
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
	// retail tail members @0x218dc0 tags 25-26:
	CObj<CPlayerSwitchUI> pPlayerSwitchUI;			// the auto player-switch banner ("playerswitch" control)
	CPtr<NGame::IPlayerTracker> pLastActivePlayer;	// the last player the switch banner was shown for (retail Update @0x211d60)
	// retail NUI::CMissionUI::operator& @0x218dc0 -- 1=CDesktopWindow base, 2=pMission,
	// 3=sCameraScrollUpdate, 4=hitTextsList (dev hitsList; CHitTracker == retail CHitText, same
	// saveload id), 5=itemTextsList, 6=unitIconsList (dev enemyIconsList; CEnemyIcon == retail
	// CUnitIcon, same id), 7=hintIconsList, 8=clueItemIconsList, 9=clueUnitIconsList,
	// 10=trapIconsList, 11=soundIconsList (dev clueIconsList), tag 12 is a format HOLE,
	// 13=pTopBar, 14=pAckView (dev pAck), 15=pLogPanel, 16=pUnitPanel, 17=pPerksPanel,
	// 18=pStorePanel, 19=pMedalsPanel, 20=pInventory, 21=pCharacter, 22=pInventoryPanel,
	// 23=pBiographyPanel, 24=pCharacterPanel, 25=pPlayerSwitchUI, 26=pLastActivePlayer, 27=pPause;
	// then the OnSerialize(@0x219e60) post-pass.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pMission); f.Add(3,&sCameraScrollUpdate); f.Add(4,&hitsList); f.Add(5,&itemTextsList); f.Add(6,&enemyIconsList); f.Add(7,&hintIconsList); f.Add(8,&clueItemIconsList); f.Add(9,&clueUnitIconsList); f.Add(10,&trapIconsList); f.Add(11,&clueIconsList); f.Add(13,&pTopBar); f.Add(14,&pAck); f.Add(15,&pLogPanel); f.Add(16,&pUnitPanel); f.Add(17,&pPerksPanel); f.Add(18,&pStorePanel); f.Add(19,&pMedalsPanel); f.Add(20,&pInventory); f.Add(21,&pCharacter); f.Add(22,&pInventoryPanel); f.Add(23,&pBiographyPanel); f.Add(24,&pCharacterPanel); f.Add(25,&pPlayerSwitchUI); f.Add(26,&pLastActivePlayer); f.Add(27,&pPause); OnSerialize( f ); return 0; }

protected:
	void OnSerialize( CStructureSaver &f );		// retail @0x219e60: post-serialize "pause" text re-resolve
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
