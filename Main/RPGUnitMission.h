#ifndef __RPGMISSION_H_
#define __RPGMISSION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GSkeleton.h"
#include "RPGUnitInfo.h"
#include "Grid.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_MELEE_DISTANCE = (float)(FP_GRID_STEP * SQRT_2 + 0.01f);
namespace NAI
{
	class IAIMap;
	enum EPose;
	enum EHitLocation;
}
namespace NWorld
{
	class CUnit;
	class CUnitServer;
}
namespace NDb
{
	class CRPGWeapon;
	class CRPGPers;
	class CAISound;
	struct SAISound;   // (release-new) GetHearingProbability's sound descriptor (DataAI.h)
	class CModel;
	class CPanzerklein;
	struct SToHitConstants;
	struct SAISoundConstants;
	struct SInterruptsConstants;
	class CRPGMine;
	enum ECriticalLocation;
	enum ECritical;
}
namespace NRPG
{
struct SUnitInfo; // data about any unit that can be shown in interface
class CAttackPortion;
class IInventory;
class IInventoryInfo;
class IInventoryItem;
class IClipItem;
class IWeaponItem;
class CUnit;
class IGame;
class CCritical;
struct SCritical;
struct SDiplomacy;
struct SFirstAid;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECriticalState
{
	CS_ENABLED = 0,
	CS_DISABLED, // ���������� ����������
	CS_REROLL // ���� ������� ������ ��������
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSnipeAP
{
	ZDATA
	int nAP;
	CPtr<IUnitMissionInfo> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nAP); f.Add(3,&pTarget); return 0; }
	SSnipeAP( int _nAP = 0, IUnitMissionInfo *_pTarget = 0 ): nAP(_nAP), pTarget(_pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// mission time RPG information handler
class CDynamicSkill;
class IUnitMission: public IUnitMissionInfo
{
public:
	// the combat critical clamp reads the per-mission RPG game's nMaxCriticalSeverity; the owning unit-
	// server pushes the game in (retail passed it to the mission ctor). Default no-op for non-mission impls.
	virtual void SetGame( IGame *pGame ) {}
	virtual void SpendAP( int nAP ) = 0;
	virtual void RegisterAction( EAction action ) = 0;
	virtual const SSnipeAP& GetSavedAP() const = 0;
	virtual void SaveAP( const SSnipeAP &ap ) = 0;
	virtual void StartNewTurn( const CVec3 &ptCP ) = 0;
	virtual bool CheckIC() = 0;	// Return true ec�� �� ����� ����������
	virtual void Kill() = 0;
	virtual bool IsDead() const = 0;
	virtual IInventory* GetInventory() const = 0;
	virtual bool CreateAttack( vector<CAttackPortion> *pRes, bool bSpendAmmo, 
		bool bAnonymous = true, IUnitMissionInfo *pTarget = 0, bool bBackStab = false ) = 0;
	virtual int ProcessAttack( int nUserID, CAttackPortion *pAttack, NDb::CRPGArmor *pArmor ) = 0;
	virtual void Seat() = 0;
	virtual void Stand() = 0;
	virtual bool CanMove() const = 0;
	virtual void Reload() = 0;
	virtual bool LoadWeapon( IWeaponItemInfo *pWeapon, IClipItem *pClip ) = 0;
	virtual bool UnloadWeapon( IWeaponItemInfo *pWeapon ) = 0;
	virtual void StartAttack() = 0;					// Burst
	virtual void NextBullet() = 0;					// Burst
	virtual int  GetNBullets() const = 0;		// Burst
	virtual void AddLastCritical( NDb::ECritical eCA ) = 0; // for Criticals
	virtual void GetLastCriticals( vector<NDb::ECritical> *pResCritical ) = 0;
	virtual bool HasCritical( NDb::ECritical eCritical, CCritical** ppCritical = 0 ) const = 0;
	virtual void UseTwoHanded( bool bUse ) = 0;
	virtual bool CanUseTwoHanded() const = 0;
	virtual void Blind( bool bBlind ) = 0;
	virtual void Deaf( bool bDeaf ) = 0;
	virtual int GetRPGPersID() const = 0;
	virtual void SetCannonItem( IWeaponItem *pItem ) = 0;
	virtual IWeaponItem* GetCannonItem() const = 0;
	virtual IWeaponItem* GetWeaponItem() const = 0;
	virtual bool CanHearSound( const CVec3 &ptSoundPosition, const CVec3 &ptListenerPosition,
		NDb::CAISound *pSound, int nAISoundType, IUnitMission *pSource ) = 0;
	virtual NDb::SToHitConstants *GetToHitConstants() = 0;
	virtual NDb::SAISoundConstants *GetAISoundConstants() = 0;
	virtual NDb::SInterruptsConstants *GetInterruptsConstants() = 0;
	virtual int GetHealedVP() const = 0;
	virtual int GetTotalVP() const = 0;
	virtual int GetLastActionTimes() const = 0;
	virtual int GetMoveInLastTurn() const = 0;
	virtual void ApplyCritical( const NRPG::SCritical &critical ) = 0;
	virtual bool RemoveCritical( NDb::ECritical eCritical ) = 0;
	virtual void SuspendCriticals( int nTurns ) = 0;
	virtual void MakeDirectDamage( int nDmg ) = 0; // CRAP
	virtual void EnableCriticals() = 0;
	virtual void DisableCriticals() = 0;
	virtual void DisableCritical( NDb::ECritical eC, ECriticalState eState  ) = 0;
	virtual int  CheckInterrupt( const IUnitMission *pEnemy, bool bIsMutual, bool bWasShot ) = 0;
	virtual void BulletHit() = 0;
	virtual bool GetAck( int *pAckID, IUnitMissionInfo ** ppAttacker ) = 0;
	virtual float GetXP( int nHowManyPerson ) const = 0;
	virtual void StartRealTime() = 0;
	virtual const SDiplomacy& GetDiplomacy() const = 0;
	virtual void SetDiplomacy( const SDiplomacy &dip ) = 0;
	virtual bool IsUnconscious() = 0;
	virtual void InitAsCorpse( bool bDead ) = 0;
	virtual int GetFallDamage( float fHDiff ) = 0;
	virtual NDb::CPanzerklein *GetPanzerklein() = 0;
	virtual void SetPanzerklein( NDb::CPanzerklein *pPK, CDynamicSkill *_pPanzerkleinVP, IInventory *_pPKInventory ) = 0; 
	virtual void DoRegenerations() = 0;
	virtual void SetHiding( bool _bHiding ) = 0;
	virtual void HealVP( const SFirstAid &fa ) = 0;
	virtual void HealCriticals( int nDC ) = 0;
	virtual bool HasPerk( int nPerkID, 
		float *pParam1 = 0, float *pParam2 = 0, float *pParam3 = 0 ) const = 0;
	virtual int GetGrenadeTrapDC( NDb::CRPGGrenade *pGrenade ) = 0;
	virtual int GetMineDC( NDb::CRPGMine *pMine ) = 0;
	virtual bool CanSeeMine( float fDistance, int nDC ) = 0;
	virtual bool CanClear( int nDC, int nSkillModif ) = 0;
	virtual int GetUnhideProbability( IUnitMission *pTarget, float fDistance ) const = 0;
	// release-new (RVA 0x2bff30): the percent chance this unit HEARS pSource at distance fDist for `sound` --
	// the probability-returning sibling of CanHearSound. Appended NON-PURE at the END of the vtable (the sess19
	// IAIUnit::GetHideProbability pattern) so the dev<->release vtable order is irrelevant and other IUnitMission
	// implementors keep building; CUnitMission overrides it. Dead until the AI hearing query / assassin reaction.
	virtual int GetHearingProbability( IUnitMission *pSource, float fDist, const NDb::SAISound &sound, bool *pAudible ) { return 0; }
	// @0x34edb0 (DoAction) relocates the move-in-last-turn accounting here out of RegisterAction; default no-op
	// so non-tracking impls (CFakeRPGUnit) need not override.
	virtual void AddMoveInLastTurn( int n ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetCubesArea( const CVec3 &ptPos, vector<CVec3> *pCubes );
int GetHLPenalty( NAI::EHitLocation hl );
float GetVPPenalty( int nVP, int nHealedVP, int nMaxVP );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Weapon-class dispatch of the to-hit/cover pipeline. Was file-local to RPGUnitMission.cpp; the
// release shares it across the RPGToHit users (RealCalcTileCovers @0x2b4700 picks the melee-swing
// cover path on TH_MELEE). Values match the retail GetToHitType @0x2b3790 returns.
enum EToHitType
{
	TH_MELEE,
	TH_THROWING,
	TH_SHOOT,
	TH_RLAUNCHER,
	TH_DEFAULT,
};
// NRPG::GetToHitType @0x2b3790: the held-item class of the attacker. NOTE: TH_MELEE means a SWUNG
// melee weapon only -- a throwable knife reports TH_THROWING (and bare hands fall back to the
// default melee weapon -> TH_MELEE).
EToHitType GetToHitType( const NWorld::CUnit *pAttacker );
////////////////////////////////////////////////////////////////////////////////////////////////////
// To-hit dispatch (release migration, session 25): free fns that replace the dev virtual
// IUnitMissionInfo::Get*ToHit interface. Each RTTI-casts the firing unit to its CUnitServer, picks the
// EToHitType from the held weapon and builds the matching ToHitCalcer (defined in RPGUnitMission.cpp;
// friends of CUnitMission). bNight is wired false here (CWorld::IsNight absent -- documented elision).
int GetToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	const NAI::SPosition &posTarget, NAI::EHitLocation eHL, int nExtraAP, const NWorld::CUnit *pTarget,
	const vector<int> &accessibleHLs, int nHitCover, bool bFirstRound,
	const CVec3 &ptIllumination = CVec3(1,1,1), bool bBackstab = false );
int GetTileToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	CVec3 ptTilePos, NAI::ETileHitLocation eHitLocation, int nExtraAP, int nHitCover, bool bFirstRound,
	const CVec3 &ptIllumination = CVec3(1,1,1) );
int GetGrenadeToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	bool bFirstRound, CVec3 ptTilePos, const CVec3 &ptIllumination = CVec3(1,1,1) );
int GetRLauncherToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	CVec3 ptTilePos, NAI::ETileHitLocation eHitLocation, int nExtraAP, bool bFirstRound,
	const CVec3 &ptIllumination = CVec3(1,1,1) );
extern void DumpCritical( CCritical *p );
extern void DumpStats( CUnit *p, int nHealedVP );
extern const string& GetCLName( NDb::ECriticalLocation cl );
extern const string& GetHLName( NAI::EHitLocation cl );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif