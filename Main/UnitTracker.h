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
	int nTargetAP;
	CVec3 vTargetPos;
	NAI::SPosition sTarget;
	CObj<NAI::CPath> pPath;
	CObj<NUI::CWindow> pPathAPText;
	vector< CObj<CObjectBase> > nodesSet;
	vector< CObj<NGScene::CLightGroup> > groupsSet;
	vector< CObj<CObjectBase> > digidNodesSet;
	vector< CObj<NGScene::CLightGroup> > digidGroupsSet;

	CPtr<NWorld::CUnit> pLastEnemyUnit;
	list<CPtr<NWorld::CUnit> > enemiesList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&bActive); f.Add(5,&nFloor); f.Add(6,&bSelected); f.Add(7,&bHilighted); f.Add(8,&pSelection); f.Add(9,&bPathVisible); f.Add(10,&bHilightTarget); f.Add(11,&bPathDigitsVisible); f.Add(12,&nTargetAP); f.Add(13,&vTargetPos); f.Add(14,&sTarget); f.Add(15,&pPath); f.Add(16,&pPathAPText); f.Add(17,&nodesSet); f.Add(18,&groupsSet); f.Add(19,&digidNodesSet); f.Add(20,&digidGroupsSet); f.Add(21,&pLastEnemyUnit); f.Add(22,&enemiesList); return 0; }

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

	void SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly = false );
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

	void GetVisibleEnemiesList( list<CPtr<NWorld::CUnit> > *pEnemies ) const;
	NWorld::CUnit* GetNextVisibleEnemy();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
