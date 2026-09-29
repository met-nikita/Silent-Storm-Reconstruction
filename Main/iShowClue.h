#ifndef __A5_SHOWCLUE_H_
#define __A5_SHOWCLUE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
	class CScreenShot;
}
namespace NScenario
{
	class CScenarioClue;
}
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class IMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICShowClue: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICShowClue);
private:
	int nEventID = 0;
	CPtr<IMission> pMission;
	CPtr<NUI::CScreenShot> pScreenShot;
	CPtr<NRPG::CGlobalGame> pGame;
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	CPtr<NScenario::CScenarioClue> pClue;

public:
	CICShowClue() {}
	CICShowClue( IMission *pMission, int nEventID, NRPG::CGlobalGame *pGame, NRPG::CGlobalPlayer *pPlayer, NScenario::CScenarioClue *pClue, NUI::CScreenShot *pScreenShot = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
