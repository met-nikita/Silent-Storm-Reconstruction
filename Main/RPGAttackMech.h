#ifndef __RPGATTACKMECH_H_
#define __RPGATTACKMECH_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CRPGArmor;
	class CDBDifficulty;
}
namespace NWorld
{
	class IWorld;
}
namespace NRPG
{
class IUnitMissionInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAttackType
{
	AT_NORMAL,
	AT_BLAST_WAVE,
	AT_CLICK_OF_DEATH,
	AT_FRAGMENT   // retail =3: grenade splinters (ExplodeFragments @0x356a80 stamps every fragment portion; disasm 0x756bf3 mov ebx,3)
};
class CAttackPortion
{
public:
	ZDATA
	int nK; // "kinetic energy" - measure of penetration ability
	int nDmgType;			// type of damage dealt
	int nDmgMin, nDmgMax;	// damage to units
	int nCrtical;  // critical hit probability
	int nCrticalDifficulty;
	CRay rTtrajectory;
	CPtr<IUnitMissionInfo> pAttacker;
	CPtr<IUnitMissionInfo> pTarget;
	float fDamageCoeff;
	EAttackType atkType;
	int nUnconsciousProbability;
	bool bBackStab;
	// release-added damage-modifier members (serialized tags 14-18, operator& @0x347a90). The
	// release DROPPED rTtrajectory from serialization (trajectory moved to CExecShoot::rayInfo)
	// and renumbered pAttacker..bBackStab 9-14 -> 8-13. rTtrajectory is KEPT as a member (dev
	// combat code still uses it) but no longer serialized. These 5 are dead-in-dev (the release
	// damage pipeline sets them; this predecessor doesn't) -> default-init, behavior deferred.
	bool bAlwaysHumanCritical;
	float fStructDmgModifier;
	float fPushCoeff;
	bool bBypassPK;
	bool bNoBlowUp;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nK); f.Add(3,&nDmgType); f.Add(4,&nDmgMin); f.Add(5,&nDmgMax); f.Add(6,&nCrtical); f.Add(7,&nCrticalDifficulty); f.Add(8,&pAttacker); f.Add(9,&pTarget); f.Add(10,&fDamageCoeff); f.Add(11,&atkType); f.Add(12,&nUnconsciousProbability); f.Add(13,&bBackStab); f.Add(14,&bAlwaysHumanCritical); f.Add(15,&fStructDmgModifier); f.Add(16,&fPushCoeff); f.Add(17,&bBypassPK); f.Add(18,&bNoBlowUp); return 0; }
	// retail defaults: the inlined default ctor at the probe/knife/melee-tile sites writes
	// fPushCoeff(+0x64)=0 and bNoBlowUp(+0x69)=1; the out-of-line 12-arg ctor @0x28faa0 takes
	// fPushCoeff as its 3rd argument and also sets bNoBlowUp=true. The previous dev defaults
	// (1.0f/false) gave EVERY attack a full-strength corpse push regardless of the weapon.
	CAttackPortion() : bAlwaysHumanCritical(false), fStructDmgModifier(1.0f), fPushCoeff(0.0f), bBypassPK(false), bNoBlowUp(true) {}
	CAttackPortion( int _nK, int _nDmgType, float _fPushCoeff, int _nDmgMin, int _nDmgMax,
		int _nCrtical, int nCritDifficulty = 0, IUnitMissionInfo *_pAttacker = 0,
		IUnitMissionInfo *_pTarget = 0, float _fDamageCoeff = 1,
		int _nUnconsciousProbability = 0, bool _bBackStab = false ):
			nK(_nK), nDmgType(_nDmgType), nDmgMin(_nDmgMin), nDmgMax(_nDmgMax), nCrtical(_nCrtical),
			nCrticalDifficulty(nCritDifficulty), pTarget(_pTarget), pAttacker(_pAttacker),
			fDamageCoeff( _fDamageCoeff ), atkType( AT_NORMAL ),
			nUnconsciousProbability( _nUnconsciousProbability ), bBackStab( _bBackStab ),
			bAlwaysHumanCritical(false), fStructDmgModifier(1.0f), fPushCoeff(_fPushCoeff), bBypassPK(false), bNoBlowUp(true) {}

	float GetPushCorpseCoeff() const;
	bool CanDealDmg( const NDb::CRPGArmor *pArmor ) const;
	bool IsArmorIgnored( const NDb::CRPGArmor *pArmor ) const;
	bool CanRicochet() const;
	// retail @0x28f960. pWorld reaches the campaign difficulty multipliers; nAccumulated is the
	// damage this attack already dealt (CObject's stage loop subtracts it; single-shot callers pass 0).
	int  CalcStructDmg( NWorld::IWorld *pWorld, const NDb::CRPGArmor *pArmor, int nAccumulated ) const;
	void MakeClickOfDeath( const CRay &r );
};
float GetAPASubstraction( float fEnter, float fExit, const NDb::CRPGArmor *pArmor );
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAttackable
{
public:
	// retail: CReceivedDmg ProcessAttack( IWorld*, int, CAttackPortion*, const CVec3&, CRPGArmor* ),
	// IAttackable's only virtual (slot 0); arg order proven at PerformThrowingAttackPortion @0x290780.
	// vDir is the attack trajectory direction -- retail's replacement for CAttackPortion::rTtrajectory,
	// which retail's CAttackPortion does not have. Return stays int: it IS retail's CReceivedDmg::nDmg
	// (-1 sentinel included). The `type` half is unread here and not uniform per implementor -- see dossier.
	virtual int ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
		const CVec3 &vDir, NDb::CRPGArmor *pArmor ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif