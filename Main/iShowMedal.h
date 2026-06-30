#ifndef __A5_SHOWMEDAL_H_
#define __A5_SHOWMEDAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release iShowMedal.obj convergence -- the queued main-loop command that posts the modal "show awarded
// medal" screen. Reconstructed 1:1 on the in-tree siblings iShowClue.h/.cpp + iShowHint.h/.cpp (identical
// CIC -> IInterfaceBase -> CWindow modal lifecycle), the payload type-substituted for the medal screen:
// the owning side + the awarded medal DB record + a wide player name + the frozen-backdrop screenshot.
// Retail RVAs (VA = RVA + 0x400000):
//   NGame::CICShowMedal::Exec               @0x23c100  (guards IsValid(pMedal) before building the screen)
//   NGame::CICShowMedal::CICShowMedal       @0x23c060  (payload ctor)
//   NGame::CShowMedalInterface::Initialize  @0x23bcf0
//   NUI::CShowMedalUI::ProcessMessage       @0x23c180
// CShowMedalInterface + CShowMedalUI are .cpp-local (defined + implemented + save-registered in
// iShowMedal.cpp); only the transient command is declared here, matching CICShowClue / CICShowHint.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CScreenshotTexture;
}
namespace NDb
{
	class CSide;
	class CMedal;
}
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICShowMedal: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICShowMedal);
private:
	wstring wsName;
	CPtr<NDb::CSide> pSide;
	CPtr<NDb::CMedal> pMedal;
	CObj<NGScene::CScreenshotTexture> pScreenShotTexture;

public:
	CICShowMedal() {}
	CICShowMedal( NDb::CSide *pSide, const wstring &wsName, NDb::CMedal *pMedal,
		NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
