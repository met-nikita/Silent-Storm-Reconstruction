#include "StdAfx.h"
#include "..\DBFormat\DataRPG.h"
#include "RPGUnit.h"
#include "RPGUnitInfo.h"
#include "RPGCritical.h"
#include "RPGAllCriticals.h"
#include "RPGUnitMission.h"
#include "RPGItemSet.h"
#include "rpgPerkConstants.h"   // N_PERK_LESS_INFLUENCE_OF_WOUNDS_FOR_TOHIT (CVPCritical @0x295280)
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
	if ( modifiers.empty() ) return 0; return modifiers.front()->fMul;
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
// retail @0x295280: percentage of THEORETICAL max VP (healed VP folded in), the wound-tolerance
// perk 5 (+25 pts), the wounded-IC perk 8 (IC x(Param2+1) below its threshold), the nCheats&0x80
// harsher-wounds table, and FIVE degraded skills (AP/INTERRUPT/MELEE/SHOOTING/THROWING).
bool CVPCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	CDynamicSkill &vp = pRPGUnit->Skills(NDb::ST_VP);

	const int nEff = vp;                                     // frozen -> cap, else min(cap, value)
	const int nDenom = vp.GetTheoreticalMax();               // nBaseValue + nXPValue
	int nPercentage = int( float( pRPGUnit->nHealedVP + nEff ) * 100.0f / float( nDenom ) );

	if ( pRPGUnit->HasPerk( N_PERK_LESS_INFLUENCE_OF_WOUNDS_FOR_TOHIT ) )
		nPercentage += 25;

	float fP1 = 0, fP2 = 0;
	if ( pRPGUnit->HasPerk( 8, &fP1, &fP2 ) && float( nPercentage ) < fP1 * 100.0f )
		PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_IC), SSkillModifyInfo( fP2 + 1.0f, 0 ) ) );

	const bool bCheat = pRPGUnit->IsCheatEnabled( 0x80 );    // harsher-wounds cheat bit
	if ( nPercentage > 75 )
		return false;

	float mAP, mInt, mMelee, mShoot, mThrow;
	if ( bCheat )
	{
		if ( nPercentage < 26 )      { mAP = 0.7f;  mInt = 0.25f; mMelee = 0.4f;  mShoot = 0.4f;  mThrow = 0.4f;  }
		else if ( nPercentage < 51 ) { mAP = 0.8f;  mInt = 0.5f;  mMelee = 0.6f;  mShoot = 0.6f;  mThrow = 0.6f;  }
		else                         { mAP = 0.9f;  mInt = 0.75f; mMelee = 0.8f;  mShoot = 0.8f;  mThrow = 0.8f;  }
	}
	else
	{
		if ( nPercentage < 26 )      { mAP = 0.8f;  mInt = 0.5f;  mMelee = 0.5f;  mShoot = 0.5f;  mThrow = 0.5f;  }
		else if ( nPercentage < 51 ) { mAP = 0.9f;  mInt = 0.75f; mMelee = 0.75f; mShoot = 0.75f; mThrow = 0.75f; }
		else                         { mAP = 0.95f; mInt = 0.9f;  mMelee = 0.9f;  mShoot = 0.9f;  mThrow = 0.9f;  }
	}

	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_AP),        SSkillModifyInfo( mAP,    0 ) ) );
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_INTERRUPT), SSkillModifyInfo( mInt,   0 ) ) );
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_MELEE),     SSkillModifyInfo( mMelee, 0 ) ) );
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_SHOOTING),  SSkillModifyInfo( mShoot, 0 ) ) );
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_THROWING),  SSkillModifyInfo( mThrow, 0 ) ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//                      CLASS CAPCritical
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAPCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission )
{
	ASSERT( pRPGUnit );
	CDynamicSkill &ap = pRPGUnit->Skills(NDb::ST_AP);
 	PushModifier( new CSkillModifier( &ap, SSkillModifyInfo( 1.0f/critical.fValue, 0 ) ) );
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
 		PushModifier( new CSkillModifier( &s, SSkillModifyInfo( 1.0f/critical.fValue, 0 ) ) );
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
	PushModifier( new CSkillModifier( &pRPGUnit->Skills(NDb::ST_AP), SSkillModifyInfo( 1.0f/critical.fValue, 0 ) ) );
	pRPGMission->ApplyMotionless();   // retail @0x295d60 -- the nMotionless ref-count replaces the Jan03 bSitting
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMotionlessCritical::RemoveModifiers()
{
	CCritical::RemoveModifiers();
	if ( IsValid( pRPGMission ) )
		pRPGMission->ReleaseMotionless();  // retail @0x294eb0
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
// retail @0x294da0: bind + AddLastCritical ONLY -- the shipped build has NO Blind(true/false)
// toggle (the Jan03 fLightPerception member does not exist in retail).
bool CBlindCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBlindCritical::RemoveModifiers()  // retail @0x294fb0
{
	CCritical::RemoveModifiers();
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
// retail @0x294e00: bind + AddLastCritical ONLY -- no Deaf(true/false) toggle (the Jan03
// fSoundPerception member does not exist in retail).
bool CDeafCritical::SetModifiers( CUnit *pRPGUnit, IUnitMission *_pRPGMission )
{
	ASSERT( pRPGUnit && _pRPGMission );
	pRPGMission = _pRPGMission;
	pRPGMission->AddLastCritical( GetCriticalType() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeafCritical::RemoveModifiers()  // retail @0x295010
{
	CCritical::RemoveModifiers();
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
