#include "StdAfx.h"
#include "rpgobject.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataMap.h"
#include "RPGUnit.h"
#include "RPGAttackMech.h"
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObject: public IObject, public IAttackable
{
	OBJECT_BASIC_METHODS(CObject);
public:
	ZDATA
	int nVP;
	int nDestroyStages, nStage;
	int nMaxVP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nVP); f.Add(3,&nDestroyStages); f.Add(4,&nStage); f.Add(5,&nMaxVP); return 0; }

	CObject() : nVP(0), nDestroyStages(0), nStage(0) {}
	CObject( int nStages, NDb::CModel *pModel, int nStartStage );
	virtual int ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	virtual bool IsDead() const;
	virtual int GetDestroyStage();
	virtual void SetDestroyStage( int _nStage );
	virtual void Kill();
	virtual int GetHP() { return nVP; }   // luaObjectGetHP: current vitality points
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObject::CObject( int nStages, NDb::CModel *pModel, int nStartStage ): nDestroyStages(nStages), nStage(0) 
{
	nMaxVP = IsValid( pModel ) ? pModel->GetMaxVP() : 1;
	SetDestroyStage( nStartStage );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObject::SetDestroyStage( int _nStage )
{
	ASSERT( _nStage >= 0 && _nStage <= nDestroyStages );
	int nTmpStage = Clamp( _nStage, 0, nDestroyStages );
	nVP = nMaxVP - ( nDestroyStages > 0 ? ( nTmpStage * nMaxVP ) / nDestroyStages : 0 );
	nStage = nTmpStage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObject::ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
{
	if ( pAttack->atkType == AT_CLICK_OF_DEATH && nStage < nDestroyStages )
	{
		int nOldVP = nVP;
		++nStage;
		nVP = Max( 0, nMaxVP - Float2Int( nMaxVP * (nStage + 0.5f) ) );
		ASSERT( nVP <= nOldVP );
		return nOldVP - nVP;
	}
	if ( nMaxVP <= 0 )
		return 0;
	int nDmg = pAttack->CalcStructDmg(pArmor);
	ASSERT( nDmg >= 0 );
	nDmg = Max( 0, nDmg );
	nVP = Max( 0, nVP - nDmg );
	nStage = ( ( nMaxVP - nVP ) * nDestroyStages ) / nMaxVP;
	csRPG << "Shoot hit object, result damage = " << nDmg << ", remain " << nVP << " of " << nMaxVP << "\n";
	return nDmg;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObject::IsDead() const
{
	return nStage == nDestroyStages;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObject::GetDestroyStage()
{
	return nStage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObject::Kill()
{
	nStage = nDestroyStages;
	nVP = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IObject* CreateObject( int nStages, NDb::CModel *pModel, int nStartStage )
{
	return new CObject( nStages, pModel, nStartStage );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IObject *CreateObject( NDb::CObject *pDBObject, int nStartStage )
{
	ASSERT( IsValid( pDBObject ) );
	if ( !IsValid( pDBObject ) )
		return 0;
	//
	int nStages = pDBObject->GetStagesQuantity();
	ASSERT( nStages > 0 );
  if ( nStages > 0 )
		return NRPG::CreateObject( nStages - 1, pDBObject->pModels[0]->pModel, nStartStage );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*IObject* CreateTerrain()
{
	CObject *pRes = new CObject;
	pRes->pArmor = NDb::GetArmor(NDb::CRPGArmor::GROUND);
	pRes->nVP = -1;
	return pRes;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGArmor* GetTerrainArmor()
{
	return NDb::GetArmor(NDb::N_DEFAULT_ARMOR);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
REGISTER_SAVELOAD_CLASS_NM( 0x02781180, CObject, NRPG )
using namespace NRPG;
BASIC_REGISTER_CLASS( IObject )
