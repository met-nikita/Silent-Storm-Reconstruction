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
	// retail CPlayerTracker +0x4C (PDB): the player's OWN camera -- a full owned ICamera object,
	// save tag 10 (operator& @0x289270 CallObjectSerialize<CObj<ICamera>>). Created by the ctor via
	// the mission factory (@0x287d70 -> CMissionBase::CreateCamera @0x1a2390). This is the camera
	// the player drives in normal play (CMissionBase::GetCamera selector @0x1a1ee0); the mission's
	// pCamera (base tag 18) is the separate cinematic camera. (The old dev 32-byte SCameraPos here
	// mis-read the retail save's 4-byte object-ref chunk.)
	CObj<ICamera> pCamera;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsName); f.Add(3,&pMission); f.Add(4,&pGlobalPlayer); f.Add(5,&pPlayer); f.Add(6,&pCommander); f.Add(7,&unitsSet); f.Add(8,&turnSelectionSaveSet); f.Add(9,&selectedUnits); f.Add(10,&pCamera); return 0; }

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

	ICamera* GetCamera() const;					// retail @0x2877f0 (IPlayerTracker vtbl+0x18)
	void SetCamera( ICamera *pCamera );			// retail @0x287d40 (IPlayerTracker vtbl+0x1c)

	NWorld::IPlayer* GetPlayer() const;
	NWorld::CCommander* GetCommander() const;
	NRPG::CGlobalPlayer* GetGlobalPlayer() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
