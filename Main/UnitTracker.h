#ifndef __IMISSION_UNITTRACKER_H_
#define __IMISSION_UNITTRACKER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class CPath;
}
namespace NGame
{
	class IGlobalGame;
}
namespace NWorld
{
	class CUnit;
}
namespace NGScene
{
	class CPolyline;
	class CCFBTransform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitTracker: public IUnitTracker
{
	OBJECT_BASIC_METHODS(CUnitTracker);
private:
	ZDATA
	CPtr<IMission> pMission;
	CPtr<NWorld::CUnit> pUnit;

	bool bActive;

	int nFloor;
	bool bSelected;
	bool bHilighted;
	CObj<CObjectBase> pSelection;
	
	bool bPathVisible;
	bool bHilightTarget;
	bool bPathDigitsVisible;
	bool bTrackRealTime;	// retail tag 12 (@0x32e160, member @0x27): the real-time mode the path overlay was built
							// for; Update @0x32d520 re-latches it and HidePath()s across a TB/RT switch
	int nTargetAP;
	CVec3 vTargetPos;
	NAI::SPosition sTarget;
	CObj<NAI::CPath> pPath;
	vector< CObj<CObjectBase> > nodesSet;
	vector< CObj<NGScene::CLightGroup> > groupsSet;
	vector< CObj<CObjectBase> > digidNodesSet;
	vector< CObj<NGScene::CLightGroup> > digidGroupsSet;

	CPtr<NWorld::CUnit> pLastEnemyUnit;
	list<CPtr<NWorld::CUnit> > enemiesList;
	// retail tag 23 (@0x32e160, member @0x7c): per-skill baseline map -- GetSkillChanges @0x32c110
	// reports (current - baseline) and seeds the baseline on first probe; SyncAllSkills @0x32c180
	// re-baselines every watched skill to its current value.
	unordered_map<int,int> skillsChanges;
	// retail NGame::CUnitTracker::operator& @0x32e160 -- 2=pMission, 3=pUnit, 4=bActive, 5=nFloor,
	// 6=bSelected, 7=bHilighted, 8=pSelection, 9=bPathVisible, 10=bHilightTarget,
	// 11=bPathDigitsVisible, 12=bTrackRealTime, 13=nTargetAP, 14=vTargetPos, 15=sTarget, 16=pPath,
	// 17=nodesSet, 18=groupsSet, 19=digidNodesSet, 20=digidGroupsSet, 21=pLastEnemyUnit,
	// 22=enemiesList, 23=skillsChanges. (The dev-only pPathAPText label -- a format violation at dev
	// tag 16 -- was dropped from the wire earlier and removed entirely, with CProjectedText, in W5.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&bActive); f.Add(5,&nFloor); f.Add(6,&bSelected); f.Add(7,&bHilighted); f.Add(8,&pSelection); f.Add(9,&bPathVisible); f.Add(10,&bHilightTarget); f.Add(11,&bPathDigitsVisible); f.Add(12,&bTrackRealTime); f.Add(13,&nTargetAP); f.Add(14,&vTargetPos); f.Add(15,&sTarget); f.Add(16,&pPath); f.Add(17,&nodesSet); f.Add(18,&groupsSet); f.Add(19,&digidNodesSet); f.Add(20,&digidGroupsSet); f.Add(21,&pLastEnemyUnit); f.Add(22,&enemiesList); f.Add(23,&skillsChanges); return 0; }

protected:
	void ShowPath();
	void HidePath();
	void ShowPathDigits();
	void HidePathDigits();

	void ShowSelection();
	void HideSelection();

	void SmoothPathLine( const vector<NWorld::SPathPoint> &points, vector<NWorld::SPathPoint> *pRes );
	float SmoothIteration( vector<NWorld::SPathPoint> *pRes );

public:
	CUnitTracker() {}
	CUnitTracker( IMission *pMission, NWorld::CUnit *pUnit );

	NWorld::EUnitCommandResult SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly = false );   // retail @0x32b670
	NAI::SPosition GetTargetPosition() const;

	bool IsPathComplete() const;
	void CancelPath();

	bool IsActive() const;
	void SetActive( bool bState );

	bool IsSelected() const;
	void SetSelected( bool bState );

	bool IsHilighted() const;
	void SetHilighted( bool bState );

	void Update();
	
	NWorld::CUnit* GetUnit() const { return pUnit; }

	NDb::EDiplomacyState GetUnitDiplomacy( NWorld::CUnit *pUnit ) const;

	// retail skill-change tracking over skillsChanges (tag 23):
	int GetSkillChanges( int nSkill );		// retail @0x32c110: current - baseline (first probe seeds the baseline, returns 0)
	void SyncAllSkills();					// retail @0x32c180: re-baseline every watched skill to its current value

	void GetVisibleEnemiesList( list<CPtr<NWorld::CUnit> > *pEnemies ) const;
	NWorld::CUnit* GetNextVisibleEnemy();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
