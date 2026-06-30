#ifndef __INTERFACE_MEDALSPANEL_H_
#define __INTERFACE_MEDALSPANEL_H_
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
// release-new sibling widgets, defined in iMedalsPanel.cpp (forward-declared here exactly like
// iPerksPanel.h forward-declares CPerksPanelView):
class CMedalsPanelView;   // CObj member below + the rotating-medal preview
class CMedalsPanelItem;   // SetSelected() argument + the medal-list row
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel -- the "medals" tab of the merc character UI (release iMedalsPanel.obj, registered
// 0xB3414140). A CWindow-derived panel that owns the tab buttons (perks / biography / character and the
// self "medals" close button), a CListView of CMedalsPanelItem rows inside a CScrollWindow, and a
// CMedalsPanelView 3D preview of the selected medal. Keeps non-owning refs to the current mission and
// the tracked unit. Modelled 1:1 on the siblings CBiographyPanel / CPerksPanel; layout + save tags from
// the matched Game.exe + PDB (NUI::CMedalsPanel, size 164). Functions (VA = RVA + 0x400000):
//   ctor( SWindowInfo, IMission* ) @0x1f9da0 ; SetSelected @0x1f9740 ; Generate @0x1fa680 ;
//   Draw @0x1fa8e0 ; ProcessMessage @0x1f9ff0.
//
// DEFERRED SEAM: the awarded-medal source NRPG::CMedalsGainer::GetGainedMedals (and the per-unit
// "current award" flash) require NDb::CSide::medals, a release-new DB column ABSENT from this tree
// (see RPGUnit.h CMedalsGainer). Those two gainer accessors are landed as documented STUBS that return
// an empty list / false, so the panel compiles and runs behaviour-neutral (no rows, no flash) until the
// DB-schema convergence lands. The panel structure / dispatch / preview math are otherwise exact.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMedalsPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CMedalsPanel)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<NWorld::CUnit> pUnit;
	////
	CObj<CButton> pClose;
	CObj<CHoverFlashButton> pPerks;
	CObj<CHoverButton> pBiography;
	CObj<CHoverButton> pCharacter;
	CObj<CListView> pMedals;
	CObj<CMedalsPanelView> pMedalModelView;
	CObj<CScrollWindow<CListView> > pMedalsView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pClose); f.Add(5,&pPerks); f.Add(6,&pBiography); f.Add(7,&pCharacter); f.Add(8,&pMedals); f.Add(9,&pMedalModelView); f.Add(10,&pMedalsView); return 0; }

private:
	void SetSelected( CMedalsPanelItem *pItem );
	void Generate( NWorld::CUnit *pUnit );

public:
	CMedalsPanel() {}
	CMedalsPanel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
