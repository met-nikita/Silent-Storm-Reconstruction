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
class IMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICInGameMenu: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICInGameMenu);
private:
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	CPtr<IMission> pMission;
	// retail threads bAllowRestart into CInGameMenuInterface::Initialize @0x1e9940: the "Restart
	// mission" button follows the mission's saved bCanRestart flag. Mission initialization arms it;
	// chapter/global map callers leave it disabled (restart.sav belongs to the previous mission).
	bool bAllowRestart = false;
	bool bAllowSave = true;   // retail: the mission's bCanSave, forwarded to every save/load screen this menu opens

public:
	CICInGameMenu() {}
	CICInGameMenu( NRPG::CGlobalPlayer *pGlobalPlayer, bool bAllowRestart = false, bool bAllowSave = true, IMission *pMission = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
