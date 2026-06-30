#ifndef __INTERFACE_BIOGRAPHYPANEL_H_
#define __INTERFACE_BIOGRAPHYPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class IMission;
}
namespace NWorld
{
	class CUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBiographyPanel -- the "biography" tab of the merc character UI (release iBiographyPanel.obj,
// registered 0xB3218140). A CWindow-derived panel that owns the tab buttons (close / perks / medals /
// character), a CText body showing the selected merc's biography prose, and a CScrollWindow<CText> that
// wraps it. Keeps non-owning refs to the current mission and the tracked unit. Modelled on the sibling
// CCharacterPanel. Functions: ctor @0x1a4f10; Draw @0x1a4f70; ProcessMessage @0x1a51a0; operator& @0x1a6170.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBiographyPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CBiographyPanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<NWorld::CUnit> pUnit;
	CObj<CHoverButton> pClose;
	CObj<CHoverFlashButton> pPerks;
	CObj<CHoverButton> pMedals;
	CObj<CHoverButton> pCharacter;
	CObj<CMLText> pDescription;                  // multi-line (tag-processed, GetRealSize) biography body
	CObj<CScrollWindow<CMLText> > pDescriptionView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pClose); f.Add(5,&pPerks); f.Add(6,&pMedals); f.Add(7,&pCharacter); f.Add(8,&pDescription); f.Add(9,&pDescriptionView); return 0; }

public:
	CBiographyPanel() {}
	CBiographyPanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
