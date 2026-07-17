#include "StdAfx.h"
#include "RPGAttackMech.h"
#include "..\DBFormat\DataRPG.h"
// complete types CalcStructDmg needs: IWorld::GetGlobalGame, CGlobalGame::pDifficulty,
// CDBDifficulty::f{Enemy,Our}DamageMult, IUnitMissionInfo::IsAIPlayer.
#include "..\DBFormat\DataDifficulty.h"
#include "wInterface.h"
#include "rpgGlobal.h"
#include "RPGUnitInfo.h"
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
// retail @0x28f860: a sub-threshold push (fPushCoeff < 0.18) is suppressed to zero; otherwise the
// corpse-push coefficient is fPushCoeff + 0.1, capped at 1.5. fPushCoeff itself is non-zero ONLY
// for firearm bullets (CWeaponItem::CreateNewAttackPortion) -- every other retail attack source
// passes 0, so only bullets fling corpses.
float CAttackPortion::GetPushCorpseCoeff() const
{
	if ( fPushCoeff < 0.18f )
		return 0;
	return Min( fPushCoeff + 0.1f, 1.5f );
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
// retail @0x28f960 (raw disasm authoritative: Ghidra aliases the running float through param_2).
// Dev lacked the pMaterial gate, fStructDmgModifier (the member existed, nothing read it),
// nAccumulated, and the difficulty multipliers. The multipliers are gated on the material being
// HUMAN_BODY, so they scale flesh damage only -- never structural damage to objects/buildings.
int CAttackPortion::CalcStructDmg( NWorld::IWorld *pWorld, const NDb::CRPGArmor *pArmor, int nAccumulated ) const
{
	ASSERT( pArmor != 0 );
	if ( pArmor == 0 || !IsValid( pArmor->pMaterial ) )
		return 0;

	// Old way
/*	int nDmg = GetDmg2Armor( nDmgType, pArmor->pMaterial->nDR );
	int nResDmg = !nDmg ? nDmg : ( pArmor->pMaterial->nVP / nDmg );*/

	// retail does NOT null-check the CGlobalGame between the two derefs -- reproduced exactly.
	NDb::CDBDifficulty *pDifficulty = 0;
	if ( pWorld )
		pDifficulty = pWorld->GetGlobalGame()->pDifficulty;

	// New way
	int nAPA = Max( nK / 10, 0 );
	int nDmg;
	if ( nDmgMax <= nDmgMin )
		nDmg = nDmgMin;
	else
		nDmg = random.Get( nDmgMin, nDmgMax );
	int nResDmg = nDmg + Min( nAPA - pArmor->pMaterial->nThreshold, 0 );
	nResDmg = Max( nResDmg, 0 );
	float fRes = fStructDmgModifier * fDamageCoeff * nResDmg;
	if ( nAccumulated != 0 )
		fRes -= nAccumulated;
	if ( pArmor->pMaterial->GetRecordID() == NDb::CRPGMaterial::HUMAN_BODY && IsValid( pAttacker ) && pDifficulty )
	{
		if ( pAttacker->IsAIPlayer() )
			fRes *= pDifficulty->fEnemyDamageMult;
		else
			fRes *= pDifficulty->fOurDamageMult;
	}
	// saturate before the fistp so a runaway float cannot land on x87's integer-indefinite and flip
	// negative. Float2Int is fld/fistp = retail's round-to-nearest-even (no fldcw truncate present).
	return Float2Int( Max( 0.0f, Min( fRes, 1879048192.0f /* 0x70000000 */ ) ) );
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

