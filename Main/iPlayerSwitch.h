#ifndef __A5_I_PLAYERSWITCH_H__
#define __A5_I_PLAYERSWITCH_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Interface.h"		// NUI::CWindow, SWindowInfo, SEvent, CLoader, GetDBString, CText
#include "iCommonUI.h"		// NUI::CHoverButton
#include "iMission.h"		// NGame::IMission, IPlayerTracker, NWorld::IPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayerSwitchUI -- in-mission "active player changed" banner (release-new iPlayerSwitch.obj). A
// CWindow that owns a title label (pText) and a single OK CHoverButton (pOk), plus the pause flag
// (bPause) it must restore. Show() pops the banner up, builds the title from the active player's name,
// remembers the current pause state and forces the game paused; OK / Enter / any notify restores the
// previous pause state and hides it. Reconstructed 1:1 from Game.exe:
//   ctor @0x22e4a0, OnOK @0x22e470, Show @0x22e4e0, ProcessMessage @0x22e5b0, operator& @0x22ea70.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayerSwitchUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CPlayerSwitchUI)
private:
	ZDATA_(CWindow)
	bool bPause;
	CPtr<NGame::IMission> pMission;
	CObj<CText> pText;
	CObj<CHoverButton> pOk;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bPause); f.Add(3,&pMission); f.Add(4,&pText); f.Add(5,&pOk); return 0; }

public:
	CPlayerSwitchUI() {}
	CPlayerSwitchUI( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void OnOK();
	void Show();

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
