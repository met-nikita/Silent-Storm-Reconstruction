#ifndef __RPGATTACKMECH_H_
#define __RPGATTACKMECH_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CRPGArmor;
}
namespace NRPG
{
class IUnitMissionInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAttackType
{
	AT_NORMAL,
	AT_BLAST_WAVE,
	AT_CLICK_OF_DEATH
};
class CAttackPortion
{
public:
	ZDATA
	int nK; // "������������ �������" - ���� ����������� �����������
	int nDmgType;			// ��� ���������� damage-�
	int nDmgMin, nDmgMax;	// ���� �� ������
	int nCrtical;  // ����������� ���������
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
	CAttackPortion() : bAlwaysHumanCritical(false), fStructDmgModifier(1.0f), fPushCoeff(1.0f), bBypassPK(false), bNoBlowUp(false) {}
	CAttackPortion( int _nK, int _nDmgType, int _nDmgMin, int _nDmgMax,
		int _nCrtical, int nCritDifficulty = 0, IUnitMissionInfo *_pAttacker = 0,
		IUnitMissionInfo *_pTarget = 0, float _fDamageCoeff = 1,
		int _nUnconsciousProbability = 0, bool _bBackStab = false ):
			nK(_nK), nDmgType(_nDmgType), nDmgMin(_nDmgMin), nDmgMax(_nDmgMax), nCrtical(_nCrtical),
			nCrticalDifficulty(nCritDifficulty), pTarget(_pTarget), pAttacker(_pAttacker),
			fDamageCoeff( _fDamageCoeff ), atkType( AT_NORMAL ),
			nUnconsciousProbability( _nUnconsciousProbability ), bBackStab( _bBackStab ),
			bAlwaysHumanCritical(false), fStructDmgModifier(1.0f), fPushCoeff(1.0f), bBypassPK(false), bNoBlowUp(false) {}

	bool CanDealDmg( const NDb::CRPGArmor *pArmor ) const;
	bool IsArmorIgnored( const NDb::CRPGArmor *pArmor ) const;
	bool CanRicochet() const;
	int  CalcStructDmg( const NDb::CRPGArmor *pArmor ) const;
	void MakeClickOfDeath( const CRay &r );
};
float GetAPASubstraction( float fEnter, float fExit, const NDb::CRPGArmor *pArmor );
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAttackable
{
public:
	virtual int ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif