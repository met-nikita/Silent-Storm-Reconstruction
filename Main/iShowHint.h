#ifndef __A5_SHOWHINT_H_
#define __A5_SHOWHINT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// LUA convergence (hint machinery) -- release iShowHint.obj. The "show hint" modal screen + the queued
// main-loop command that posts it. Reconstructed 1:1 on the iLoseFake CIC-menu template (CICLoseMenu /
// CLoseMenuInterface / CLoseMenuUI): a NMainLoop::CInterfaceCommand whose Exec builds the IInterfaceBase
// screen, awards the hint's XP reward to the party, and pushes the screen. Retail RVAs (VA = RVA+0x400000):
//   NGame::CICShowHint::Exec               @0x23ab00
//   NGame::CShowHintInterface::Initialize  @0x23a6e0  (container id 322 = mov ecx,0x142 before GetUIContainer)
//   NUI::CShowHintUI::ProcessMessage       @0x23abd0
//   NUI::CShowHintView::ProcessMessage     @0x23acc0
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CScreenshotTexture;
}
namespace NDb
{
	class CUIHint;
}
namespace NRPG
{
	class CGlobalGame;
}
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class IMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowHint -- release-new (iShowHint): queues the "show hint" modal screen (CShowHintInterface). On Exec it
// awards the hint's XP reward to the whole party and pushes the screen. The retail fetches the global game from
// the mission (IMission vtbl+0x130, absent in dev) so the dev passes it in at dispatch time (where it's in scope).
class CICShowHint: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICShowHint);
private:
	CPtr<IMission> pMission;
	int nEventID;									// the wait-id the screen posts back on close (WaitForUI)
	CDBPtr<NDb::CUIHint> pHint;
	CPtr<NRPG::CGlobalGame> pGlobalGame;			// dev delta: the retail gets this from the mission
	CObj<NGScene::CScreenshotTexture> pScreenShotTexture;

public:
	CICShowHint(): nEventID( 0 ) {}
	CICShowHint( IMission *pMission, int nEventID, NDb::CUIHint *pHint, NRPG::CGlobalGame *pGlobalGame,
		NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
