#ifndef __IUNIT_ICON_BAR_H_
#define __IUNIT_ICON_BAR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIconBarSet;
class CComplexButton;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitIconsBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitIconsBar: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitIconsBar)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CIconBarSet> pIconBar;
	NGame::EActionIconsSet eLastSet;
	vector<CPtr<CComplexButton> > iconsSet;
	// retail +0x98/+0x9c (operator& @0x253bb0 tags 6/7): Draw's refresh-on-change trackers --
	// the last-seen GetUnitsWorldState() value (retail PDB member name kept) and the last-seen
	// selection set. Retail leaves nTrackWeaponModeChanges UNINITIALIZED in both ctors
	// (@0x252420/@0x2536f0) -- reproduced: no init here or in the ctor init lists.
	int nTrackWeaponModeChanges;
	vector<CPtr<NGame::IUnitTracker> > trackSelectionChanges;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pIconBar); f.Add(4,&eLastSet); f.Add(5,&iconsSet); f.Add(6,&nTrackWeaponModeChanges); f.Add(7,&trackSelectionChanges); return 0; }

public:
	CUnitIconsBar() {}
	CUnitIconsBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void SetIconBar( CIconBarSet *pIcons );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
