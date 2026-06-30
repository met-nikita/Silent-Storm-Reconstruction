#include "StdAfx.h"
#include "RPGGame.h"
#include "RPGAttackMech.h"
#include "BuildingGrid.h"
#include "..\MiscDll\LogStream.h"
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilding: public CObjectBase, public IAttackable
{
	OBJECT_BASIC_METHODS(CBuilding);
	ZDATA
	CObj<NBuilding::CBuildingGrid> pGrid;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGrid); return 0; }
public:
	CBuilding( NBuilding::CBuildingGrid *_pGrid = 0 ): pGrid(_pGrid) {}
	virtual int ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	virtual bool IsDead() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuilding::ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
{
	NBuilding::SPoint3 pt;
	NBuilding::GetPieceHashCoords( nUserID, &pt );

	int nDmg = pAttack->CalcStructDmg(pArmor);
	if ( pGrid->DamageSpot( pt, nDmg, true ) )   // retail: weapon/grenade/explosion hits feed the destruction FX
	{
		//csRPG << "Shoot destroy bulding node!\n";
		return nDmg;
	}
	//csRPG << "Shoot hit bulding, result damage = " << nDmg << ", remain " << pGrid->GetHP(pt) << "\n";
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CreateBuilding( NBuilding::CBuildingGrid *pGrid )
{
	return new CBuilding( pGrid );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x019c1140, CBuilding )
