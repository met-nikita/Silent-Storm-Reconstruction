#ifndef __IMISSION_PLAYERTRACKER_H_
#define __IMISSION_PLAYERTRACKER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	enum EDiplomacyState;
}
namespace NRPG
{
	class CGlobalGame;
}
namespace NGame
{
	class IGlobalGame;
	class IUnitTracker;
	class CUnitTracker;
}
namespace NWorld
{
	class IWorld;
	class IPlayer;
	class CCommander;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayerTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayerTracker: public IPlayerTracker
{
	OBJECT_BASIC_METHODS(CPlayerTracker);
private:
	ZDATA
	wstring wsName;
	CPtr<IMission> pMission;
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	////
	CPtr<NWorld::IPlayer> pPlayer;
	CPtr<NWorld::CCommander> pCommander;
	vector< CObj<CUnitTracker> > unitsSet;
	vector< CPtr<IUnitTracker> > turnSelectionSaveSet;
	////
	vector< CPtr<IUnitTracker> > selectedUnits;
	////
	ICamera::SCameraPos sCamPlacement;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsName); f.Add(3,&pMission); f.Add(4,&pGlobalPlayer); f.Add(5,&pPlayer); f.Add(6,&pCommander); f.Add(7,&unitsSet); f.Add(8,&turnSelectionSaveSet); f.Add(9,&selectedUnits); f.Add(10,&sCamPlacement); return 0; }

public:
	CPlayerTracker() {}
	CPlayerTracker( IMission *pMission, NRPG::CGlobalPlayer *_pGlobalPlayer, const wstring &wsName );

	void AddUnit( NRPG::CUnit *pMerc );
	void RemoveUnit( IUnitTracker *pUnit );

	bool IsPlayerLoser();
	bool IsPlayerWinner();
	bool IsUnitVisible( NWorld::CUnit *pUnit ) const;

	NDb::EDiplomacyState GetUnitDiplomacy( NWorld::CUnit *pUnit ) const;
	void GetVisibleEnemyList( list<CPtr<NWorld::CUnit> > *pEnemies ) const;

	void GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;
	void GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;

	int CountSelected();
	void Select( NWorld::CUnit *pUnit, bool bAdditive );
	void Select( int nDir );
	void SelectNext();
	void SelectPrev();

	void Activate();
	void Deactivate();
	
	void Update( bool bActive );

	const ICamera::SCameraPos& GetCamera() const;
	void SetCamera( const ICamera::SCameraPos &sPosition );

	NWorld::IPlayer* GetPlayer() const;
	NWorld::CCommander* GetCommander() const;
	NRPG::CGlobalPlayer* GetGlobalPlayer() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
