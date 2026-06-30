#ifndef __RPGPERK_H_
#define __RPGPERK_H_
//
#include "..\Misc\Set.h"
//
#include "..\DBFormat\DataPerk.h"
namespace NDb
{
	class CDBPerk;
}
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerk
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPerk: public CObjectBase
{
	OBJECT_BASIC_METHODS( CPerk );
	ZDATA
	CDBPtr<NDb::CDBPerk> pDBPerk;
	NWorld::CSet<CPerk> parents;
	bool bTaken;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pDBPerk); f.Add(3,&parents); f.Add(4,&bTaken); return 0; }
	//
	CPerk() {}
	CPerk( NDb::CDBPerk *_pDBPerk ): pDBPerk( _pDBPerk ), bTaken( false ) {}
	//
	NDb::CDBPerk* GetDBPerk() const { return pDBPerk; }
	void AddParent( CPerk *pParent );
	const NWorld::CSet<CPerk>& GetParents() const { return parents; }
	const bool IsTaken() const { return bTaken; }
	void Take() { bTaken = true; }
	const bool CanTake() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPerksTree
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPerksTree: public CObjectBase
{
	OBJECT_BASIC_METHODS( CPerksTree );
	ZDATA
		unordered_map< int, CObj<CPerk> > perks;
	int nPerkPoints;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&perks); f.Add(3,&nPerkPoints); return 0; }
	//
	void LoadTree( int nTreeID );
	CPerk* GetOrCreatePerk( NDb::CDBPerk *pDBPerk );
	//
public:
	CPerksTree( int nTreeID = -1 );
	//
	void Draw( string szFileName = "perks" );
	void GetAllPerks( vector< CPtr<CPerk> > *pPerks ) const;
	void GetTakenPerks( vector< CPtr<CPerk> > *pPerks ) const;
	void GetAvailablePerks( vector< CPtr<CPerk> > *pPerks ) const;
	bool HasPerk( int nPerkID, float *pParam1, float *pParam2, float *pParam3 );
	void TakePerk( int nPerkID );
	void TakeRandomPerks();		// release @0x2aeb60: spend every perk point on randomly-chosen available perks
	void AddPerkPoints( int nPoints );
	int GetPerkPoints() const { return nPerkPoints; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPerksTree* CreatePerksTree( int nID = -1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __RPGPERK_H_