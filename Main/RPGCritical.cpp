#include "StdAfx.h"
#include "..\DBFormat\DataRPG.h"
#include "RPGUnit.h"
#include "RPGUnitInfo.h"
#include "RPGCritical.h"
#include "RPGAllCriticals.h"
#include "RPGUnitMission.h"
#include "RPGItemSet.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
CCritical::CCritical( const SCritical &crit ): critical(crit), nTurn(0)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCritical::NextTurn()
{
	if ( critical.nDuration >= 0 && ++nTurn > critical.nDuration )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CCritical::GetRemainingTime() const
{
	return critical.nDuration - nTurn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CCritical::GetModifier() const 
{ 
	if ( modifiers.empty() ) return 0; return modifiers.front()->Get(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CCritical::Merge( CCritical *pCritical ) const
{
	const SCritical &c = pCritical->critical;
	if ( c.eCritical != critical.eCritical || c.eCl != critical.eCl )
		return OTHER;
	int nRTime = GetRemainingTime();
	int nCRTime = pCritical->GetRemainingTime();
	bool bTime = !IsTemporarily() ? true : ( pCritical->IsTemporarily() ? false : nRTime > nCRTime );
	if ( bTime && critical.fValue > c.fValue && critical.nDC > c.nDC )
		return WEAKER;
	if ( nRTime == nCRTime && fabs( critical.fValue - c.fValue ) < FP_EPSILON )
		return MERGED;
	if ( !IsTemporarily() || !pCritical->IsTemporarily() )
		pCritical->critical.nDuration = -1;
	else
	{
		pCritical->critical.nDuration = Max( nRTime, nCRTime );
		pCritical->nTurn = 0;
	}
	pCritical->critical.fValue = Max( critical.fValue, c.fValue );
	pCritical->critical.nDC = Max( critical.nDC, c.nDC );
	return MERGED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCritical* CreateCritical( const SCritical &critical )
{
	switch ( critical.eCritical )
	{
		case NDb::C_VP:
			return new CVPCritical( critical );
		case NDb::C_AP_REDUCTION:
			return new CAPCritical( critical );
		case NDb::C_WEAPONSKILL_REDUCTION:
			return new CWeaponSkillCritical( critical );
		case NDb::C_DEATH:
			return new CDeathCritical( critical );
		case NDb::C_MOTIONLESS:
			return new CMotionlessCritical( critical );
		case NDb::C_ACCIDENTAL_SHOT:
			return new CAccidentalShotCritical( critical );
		case NDb::C_LOST_WEAPON:
			return new CLostWeaponCritical( critical );
		case NDb::C_IDLE_HAND:
			return new CIdleHandCritical( critical );
		case NDb::C_DAMAGE_WEAPON:
			return new CDamageWeaponCritical( critical );
		case NDb::C_BLIND:
			return new CBlindCritical( critical );
		case NDb::C_STUN:
			return new CStunCritical( critical );
		case NDb::C_PATIENT:
			return new CPatientCritical( critical );
		case NDb::C_DEAF:
			return new CDeafCritical( critical );
		case NDb::C_BLEEDING:
			return new CBleedingCritical( critical );
		case NDb::C_PANZERKLEIN_AXIS:
		case NDb::C_PANZERKLEIN_ALLIES:
		case NDb::C_PANZERKLEIN_TERRORS:
		case NDb::C_PANZERKLEIN_BROKEN:
		case NDb::C_PANZERKLEIN_AXIS_SOLDIER:
		case NDb::C_PANZERKLEIN_AXIS_ENGINEER:
		case NDb::C_PANZERKLEIN_ALLIES_SCOUT:
		case NDb::C_PANZERKLEIN_ALLIES_SNIPER:
		case NDb::C_PANZERKLEIN_TERRORS_MEDIC:
		case NDb::C_PANZERKLEIN_TERRORS_HWG:
			return new CPanzerkleinCritical( critical );
	}
	ASSERT( 0 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CVPCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVPCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	CDynamicSkill &vp = pRPGUnit->Skills(NDb::ST_VP);
	CDynamicSkill &ap = pRPGUnit->Skills(NDb::ST_AP);
	CDynamicSkill &interrupt = pRPGUnit->Skills(NDb::ST_INTERRUPT);
	CDynamicSkill &melee = pRPGUnit->Skills(NDb::ST_MELEE);

	const int nPercentage = 100.0f * vp / vp.GetMaxValue();
	//
	if ( nPercentage > 75 )
		return false;
	else if ( nPercentage > 50 )
	{
		PushModifier( new CSkillModifier( &ap, 0.95f ) );
		PushModifier( new CSkillModifier( &interrupt, 0.9f ) );
		PushModifier( new CSkillModifier( &melee, 0.9f ) );
	}
	else if ( nPercentage > 25 )
	{
		PushModifier( new CSkillModifier( &ap, 0.9f ) );
		PushModifier( new CSkillModifier( &interrupt, 0.75f ) );
		PushModifier( new CSkillModifier( &melee, 0.75f ) );
	}
	else
	{
		PushModifier( new CSkillModifier( &ap, 0.8f ) );
		PushModifier( new CSkillModifier( &interrupt, 0.5f ) );
		PushModifier( new CSkillModifier( &melee, 0.5f ) );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CAPCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAPCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	CDynamicSkill &ap = pRPGUnit->Skills(NDb::ST_AP);
 	PushModifier( new CSkillModifier( &ap, 1.0f/critical.fValue ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CWeaponSkillCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
static NDb::ESkillType weaponSkills[] = 
{
	NDb::ST_MELEE,
	NDb::ST_SHOOTING,
	NDb::ST_THROWING,
	NDb::ST_BURST,
	NDb::ST_SNIPE,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponSkillCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	//
	for ( int i = 0; i < ARRAY_SIZE( weaponSkills ); ++i )
	{
		CDynamicSkill &s = pRPGUnit->Skills( weaponSkills[i] );
 		PushModifier( new CSkillModifier( &s, 1.0f/critical.fValue ) );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CDeathCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDeathCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	pRPGUnit->Skills(NDb::ST_VP) -= pRPGUnit->Skills(NDb::ST_VP);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CMotionlessCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMotionlessCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_AP), 1.0f/critical.fValue ) );
	pRPGMission->Seat();
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMotionlessCritical::RemoveModifiers()
{
	CCritical::RemoveModifiers();
	if ( IsValid( pRPGMission ) )
		pRPGMission->Stand();
	pRPGMission = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CLostWeaponCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLostWeaponCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGMission );
	pRPGMission->AddLastCritical( GetCriticalType() );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CIdleHandCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CIdleHandCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	pRPGMission->UseTwoHanded( false );
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CIdleHandCritical::RemoveModifiers()
{
	CCritical::RemoveModifiers();
	if ( IsValid( pRPGMission ) )
		pRPGMission->UseTwoHanded( true );
	pRPGMission = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CLostAccidentalShot
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAccidentalShotCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGMission );
	pRPGMission->AddLastCritical( GetCriticalType() );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CDamageWeaponCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDamageWeaponCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGMission );
	CDynamicCast<CWeaponItem> pW(pRPGMission->GetInventory()->GetActive());
	if (pW)
	{
		pRPGMission->AddLastCritical( GetCriticalType() );
		pW->Damage();
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CBlindCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBlindCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	pRPGMission->Blind( true );
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBlindCritical::RemoveModifiers()
{
	CCritical::RemoveModifiers();
	if ( IsValid( pRPGMission ) )
		pRPGMission->Blind( false );
	pRPGMission = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CStunCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStunCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CDeafCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDeafCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	pRPGMission->Deaf( true );
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeafCritical::RemoveModifiers()
{
	CCritical::RemoveModifiers();
	if ( IsValid( pRPGMission ) )
		pRPGMission->Deaf( false );
	pRPGMission = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0xA28C1160, CVPCritical );
REGISTER_SAVELOAD_CLASS( 0xA0812160, CAPCritical );
REGISTER_SAVELOAD_CLASS( 0xA0912140, CWeaponSkillCritical );
REGISTER_SAVELOAD_CLASS( 0xA1812120, CDeathCritical );
REGISTER_SAVELOAD_CLASS( 0xA2112180, CMotionlessCritical );
REGISTER_SAVELOAD_CLASS( 0xA2412180, CLostWeaponCritical );
REGISTER_SAVELOAD_CLASS( 0xA2412181, CIdleHandCritical );
REGISTER_SAVELOAD_CLASS( 0xA2412182, CAccidentalShotCritical );
REGISTER_SAVELOAD_CLASS( 0xA2512150, CDamageWeaponCritical );
REGISTER_SAVELOAD_CLASS( 0xA2812130, CBlindCritical );
REGISTER_SAVELOAD_CLASS( 0xA0132160, CStunCritical );
REGISTER_SAVELOAD_CLASS( 0x50642130, CPatientCritical );
REGISTER_SAVELOAD_CLASS( 0x50842160, CDeafCritical );
REGISTER_SAVELOAD_CLASS( 0x71502140, CPanzerkleinCritical );
REGISTER_SAVELOAD_CLASS( 0x52512140, CBleedingCritical );
