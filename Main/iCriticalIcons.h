#ifndef __ICRITICALICONS_H_
#define __ICRITICALICONS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Interface.h"
#include "UICommCtrls.h"
#include "RPGUnitInfo.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// release compiland iCriticalIcons.obj: the per-critical status icon (CInfoPanelCritical), the
// mission-scripted-bleeding stand-in (CMissionCriticalBleedingFake) and the shared icon-vector
// refresher (UpdateCriticalIcons) used by BOTH the single-unit info panel (6 icons) and the
// character panel (12 icons). Extracted from iUnitPanel.cpp so iCharacterPanel.cpp can share it.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class IUnitTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelCritical: public CImage
{
	OBJECT_BASIC_METHODS(CInfoPanelCritical)
private:
	ZDATA_(CImage)
	CPtr<NRPG::ICriticalInfo> pCritical;
	////
	CObj<CToolTip> pToolTip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CImage*)this); f.Add(2,&pCritical); f.Add(3,&pToolTip); return 0; }

public:
	CInfoPanelCritical() {}
	CInfoPanelCritical( const SWindowInfo &sInfo );

	void Set( NRPG::ICriticalInfo *pCritical );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionCriticalBleedingFake
////////////////////////////////////////////////////////////////////////////////////////////////////
// A transient stand-in NRPG::ICriticalInfo the panels use to draw a permanent "bleeding" critical
// icon (a mission-scripted wound not backed by a real NRPG::CCritical). Every accessor is a
// hard-coded constant except the displayed value, carried as nValue.
// (release iCriticalIcons.obj; nValue@+12, sizeof 16). GetDifficultyClass returns -1 -- in the
// release it was COMDAT-folded with GetRemainingTime (both `return -1`), hence no distinct RVA.
class CMissionCriticalBleedingFake: public NRPG::ICriticalInfo
{
	OBJECT_BASIC_METHODS(CMissionCriticalBleedingFake)
private:
	ZDATA
	int nValue;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 1, &nValue ); return 0; }

public:
	CMissionCriticalBleedingFake(): nValue( 0 ) {}
	CMissionCriticalBleedingFake( int _nValue ): nValue( _nValue ) {}

	// never reached through CInfoPanelCritical::Set (C_BLEEDING uses the fixed icon, no variant)
	virtual bool IsTemporarily() const { return false; }
	virtual int GetRemainingTime() const { return -1; }
	virtual int GetDifficultyClass() const { return -1; }
	virtual float GetValue() const { return (float)nValue; }
	virtual NDb::ECritical GetCriticalType() const { return NDb::C_BLEEDING; }
	virtual NDb::ECriticalLocation GetCriticalLocation() const { return NDb::CL_ANY; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x1cda90: rebind the panel's icon vector from the unit's live criticals list. Appends
// the mission-scripted bleeding fake (unit GetBleeding > 0), skips PK statuses (no icon slot),
// hides the unused tail icons.
void UpdateCriticalIcons( NGame::IUnitTracker *pUnit, const vector<CObj<CInfoPanelCritical> > &criticalIconsSet );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__ICRITICALICONS_H_
