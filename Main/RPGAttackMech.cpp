#include "StdAfx.h"
#include "RPGAttackMech.h"
#include "..\DBFormat\DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
float GetAPASubstraction( float fEnter, float fExit, const NDb::CRPGArmor *pArmor )
{
	fEnter = Max( fEnter, 0.0f );
	fExit = Max( fExit, 0.0f );
	float fRet = (fExit - fEnter) * pArmor->pMaterial->fDensity;
	return fRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetDmg2Armor( int nDmg, int nArmor )
{
	if ( nArmor < 0 || nDmg < 0 )
		return 0;
	NDb::CRPGDmgToArmor *pAr = NDb::GetDBDmg2Armor(nDmg);
	if ( !pAr )
		return 0;
	return pAr->armors[nArmor];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttackPortion::CanDealDmg( const NDb::CRPGArmor *pArmor ) const
{
	ASSERT( pArmor != 0 );
	if ( pArmor == 0 )
		return false;
	//
	return GetDmg2Armor( nDmgType, pArmor->pMaterial->nDR ) > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttackPortion::IsArmorIgnored( const NDb::CRPGArmor *pArmor ) const
{
	ASSERT( pArmor != 0 );
	if ( pArmor == 0 )
		return false;
	//
	int nDmg = GetDmg2Armor( nDmgType, pArmor->pMaterial->nDR );
	return nDmg < 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttackPortion::CanRicochet() const
{
	return !(nDmgType == 0 || nDmgType == 5);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAttackPortion::CalcStructDmg( const NDb::CRPGArmor *pArmor ) const
{
	ASSERT( pArmor != 0 );
	if ( pArmor == 0 )
		return 0;

	// Old way
/*	int nDmg = GetDmg2Armor( nDmgType, pArmor->pMaterial->nDR );
	int nResDmg = !nDmg ? nDmg : ( pArmor->pMaterial->nVP / nDmg );*/

	// New way
	int nAPA = Max( nK / 10, 0 );
	int nDmg;
	if ( nDmgMax <= nDmgMin )
		nDmg = nDmgMin;
	else
		nDmg = random.Get( nDmgMin, nDmgMax );
	int nResDmg = nDmg + Min( nAPA - pArmor->pMaterial->nThreshold, 0 );
	nResDmg = Max( nResDmg, 0 );
	return nResDmg * fDamageCoeff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAttackPortion::MakeClickOfDeath( const CRay &ray )
{
	nK = 1000000;
	nDmgType = 0;
	nDmgMin = 10000000; nDmgMax = 10000001;
	nCrtical = 0;
	nCrticalDifficulty = 0;
	rTtrajectory = ray;
	pAttacker = 0;
	pTarget = 0;
	fDamageCoeff = 1e6;
	atkType = AT_CLICK_OF_DEATH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////

