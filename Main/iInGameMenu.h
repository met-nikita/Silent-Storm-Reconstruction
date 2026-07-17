#ifndef __A5_INGAMEMENU_H_
#define __A5_INGAMEMENU_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICInGameMenu: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICInGameMenu);
private:
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	// retail threads bAllowRestart into CInGameMenuInterface::Initialize @0x1e9940: the "Restart
	// mission" button is enabled only when the menu was opened FROM A MISSION (the chapter/global
	// map callers leave it disabled -- there restart.sav belongs to the previous mission).
	bool bAllowRestart = false;
	bool bAllowSave = true;   // retail: the mission's bCanSave, forwarded to every save/load screen this menu opens

public:
	CICInGameMenu() {}
	CICInGameMenu( NRPG::CGlobalPlayer *pGlobalPlayer, bool bAllowRestart = false, bool bAllowSave = true );   // retail threads the mission's bCanSave down to the save/load screen

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
