#ifndef __RPGDIPLOMACY_H_
#define __RPGDIPLOMACY_H_
//
namespace NDb
{
	enum EDiplomacyState;
}
//
namespace NRPG
{
//
class IUnitMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacy
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDiplomacy
{
	ZDATA
	DWORD nDiplomacy;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nDiplomacy); return 0; }

	SDiplomacy( DWORD _nDiplomacy = 0 );
	//
	NDb::EDiplomacyState GetDiplomacyState( int nPlayer ) const;
	void SetDiplomacyState( int nPlayer, NDb::EDiplomacyState state );
	DWORD GetDiplomacy() const { return nDiplomacy; }
	void SetDiplomacy( DWORD _nDiplomacy ) { nDiplomacy = _nDiplomacy; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalDiplomacy 
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalDiplomacy: public CObjectBase
{
	OBJECT_BASIC_METHODS( CGlobalDiplomacy );
	ZDATA
	vector<SDiplomacy> diplomacy;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&diplomacy); return 0; }
	void MakeSelfAFriend();
public:
	//
	CGlobalDiplomacy();
	//
	NDb::EDiplomacyState GetDiplomacyState( int nWho, int nToWhom ) const;
	void SetDiplomacyState( int nPlayer1, 
		const vector< CPtr<NRPG::IUnitMission> > &player1Units, int nPlayer2, NDb::EDiplomacyState state );
	const SDiplomacy& GetPlayerDiplomacy( int nPlayer ) const;
	void SetPlayerDiplomacy( int nPlayer, 
		const vector< CPtr<NRPG::IUnitMission> > &playerUnits, DWORD nDiplomacy );
	//
	void LoadDiplomacy( int nVariantID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalDiplomacy *CreateGlobalDiplomacy();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __RPGDIPLOMACY_H_