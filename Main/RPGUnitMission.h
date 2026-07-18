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
	class IWorld;   // ProcessAttack/CalcStructDmg thread the world down to the difficulty multipliers
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
class CGlobalGame;
class CCritical;
struct SCritical;
struct SDiplomacy;
struct SFirstAid;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECriticalState
{
	CS_ENABLED = 0,
	CS_DISABLED, // execution is impossible
	CS_REROLL // must choose another critical
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
	virtual bool CheckIC() = 0;	// Return true if he managed to dodge
	virtual void Kill() = 0;
	virtual bool IsDead() const = 0;
	virtual IInventory* GetInventory() const = 0;
	// retail @0x6c2100 takes a 6th bool (ret 0x18): bAdaptWeapon gates the weapon-familiarity tail
	// (CUnit::UseWeapon) -- the shoot execs pass nBulletGone==0 (adapt once per shot), melee passes true.
	virtual bool CreateAttack( vector<CAttackPortion> *pRes, bool bSpendAmmo,
		bool bAnonymous = true, IUnitMissionInfo *pTarget = 0, bool bBackStab = false,
		bool bAdaptWeapon = true ) = 0;
	// NOTE: this is a SECOND, genuinely different ProcessAttack contract -- IUnitMission does not
	// derive from IAttackable, and retail's IUnitMission form takes NO CVec3 (PDB: "class
	// NRPG::CReceivedDmg __thiscall NRPG::CUnitMission::ProcessAttack(class NWorld::IWorld *,int,
	// class NRPG::CAttackPortion *,class NDb::CRPGArmor *)") where IAttackable's takes one. Only
	// pWorld is added here, and only so it reaches CalcStructDmg @0x28f960. CDumbUnitServer::
	// ProcessAttack @0x350e20 is the seam between the two contracts. On the int return see the
	// IAttackable banner in RPGAttackMech.h -- it is retail's CReceivedDmg::nDmg, -1 sentinel and all.
	virtual int ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
		NDb::CRPGArmor *pArmor ) = 0;
	// retail @0x2c5a70/@0x2c5a80: the Jan03 bSitting Seat()/Stand() pair became a ref-counted
	// nMotionless (several motionless sources may overlap); CanMove() == (nMotionless == 0).
	virtual void ApplyMotionless() = 0;
	virtual void ReleaseMotionless() = 0;
	virtual bool CanMove() const = 0;
	virtual void Reload() = 0;
	virtual bool LoadWeapon( IWeaponItemInfo *pWeapon, IClipItem *pClip ) = 0;
	virtual bool UnloadWeapon( IWeaponItemInfo *pWeapon ) = 0;
	// (Jan03 StartAttack/NextBullet/GetNBullets are GONE in retail -- the bullet index is a
	// parameter threaded through the to-hit chain; the shoot exec owns the burst cursor.)
	virtual void AddLastCritical( NDb::ECritical eCA ) = 0; // for Criticals
	virtual void GetLastCriticals( vector<NDb::ECritical> *pResCritical ) = 0;
	virtual bool HasCritical( NDb::ECritical eCritical, CCritical** ppCritical = 0 ) const = 0;
	virtual void UseTwoHanded( bool bUse ) = 0;
	virtual bool CanUseTwoHanded() const = 0;
	// (Jan03 Blind/Deaf are GONE in retail -- CBlindCritical/CDeafCritical @0x294da0/@0x294e00
	// only record themselves via AddLastCritical; no perception members exist.)
	virtual int GetRPGPersID() const = 0;
	virtual void SetCannonItem( IWeaponItem *pItem ) = 0;
	virtual IWeaponItem* GetCannonItem() const = 0;
	virtual IWeaponItem* GetWeaponItem() const = 0;
	// retail @0x2c2ee0 signature: the SAISound descriptor (record + tile type + silencer) replaced
	// the old (CAISound*, nAISoundType) pair.
	virtual bool CanHearSound( const CVec3 &ptSoundPosition, const CVec3 &ptListenerPosition,
		const NDb::SAISound &sound, IUnitMission *pSource ) = 0;
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
	// retail CUnitMission::GetMineSpotRange @0x2c0340 (release vtbl+0x168, right after CanSeeMine):
	// the distance out to which this unit spots a mine of detection class nDC. Retail CanSeeMine
	// @0x2bec30 is a pure delegate (fDistance <= GetMineSpotRange(nDC)), and CUnitServer::
	// UpdateVisible @0x3c4450 gathers mines with GetMineSpotRange(0) (@0x7c4bca, arg 0) instead of a
	// hardcoded constant. Appended NON-PURE at the interface tail (sess19 pattern -- dev<->release
	// vtable order is name-dispatched) so non-mission implementors keep building; CUnitMission
	// overrides with the retail formula.
	virtual float GetMineSpotRange( int nDC ) { return 0; }
	virtual bool CanClear( int nDC, int nSkillModif ) = 0;
	virtual int GetUnhideProbability( IUnitMission *pTarget, float fDistance ) const = 0;
	// release-new (RVA 0x2bff30): the percent chance this unit HEARS pSource at distance fDist for `sound` --
	// the probability-returning sibling of CanHearSound. Appended NON-PURE at the END of the vtable (the sess19
	// IAIUnit::GetHideProbability pattern) so the dev<->release vtable order is irrelevant and other IUnitMission
	// implementors keep building; CUnitMission overrides it. Consumed by CanHearSound (@0x2c2ee0) and the
	// assassin reaction.
	virtual int GetHearingProbability( IUnitMission *pSource, float fDist, const NDb::SAISound &sound, bool *pAudible ) { return 0; }
	// @0x34edb0 (DoAction) relocates the move-in-last-turn accounting here out of RegisterAction; default no-op
	// so non-tracking impls (CFakeRPGUnit) need not override.
	virtual void AddMoveInLastTurn( int n ) {}
	// retail CUnitMission carries the campaign CGlobalGame (for pDifficulty); this fork binds it in the
	// CUnitServer ctor. Appended NON-PURE at the vtable tail (dev<->release order is name-dispatched) so
	// other IUnitMission implementors keep building; CUnitMission overrides it.
	virtual void SetGlobalGame( CGlobalGame *p ) {}
	// Getter counterpart: the release reads the campaign difficulty record through the world's global
	// game (e.g. the called-shots gate pDifficulty->bHeadshotShouldKill in the to-hit paths). Appended
	// NON-PURE at the vtable tail like SetGlobalGame; CUnitMission overrides it.
	virtual CGlobalGame* GetGlobalGame() const { return 0; }
	// retail vtbl+0x184 AddVPBoost @0x2c3400 (temporary VP drug boost); the healer FAE_BOOST_VP path
	// (wUnitStates.cpp) dispatches through IUnitMission. Appended NON-PURE at the vtable tail like the
	// two above; CUnitMission overrides it.
	virtual void AddVPBoost( float fStrength, int nDuration ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetCubesArea( const CVec3 &ptPos, vector<CVec3> *pCubes );
int GetHLPenalty( NAI::EHitLocation hl );
float GetVPPenalty( int nVP, int nHealedVP, int nMaxVP );
float GetHeadshotMultiplier( NAI::EHitLocation hl );   // @0x2b6ca0 (defined in RPGToHit.cpp; shared with GetMeleeToHit)
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
// retail RPGUnitGetToHit @0x2b4ae0 takes the bullet index as a PARAMETER (the mission-side
// Jan03 nBullet cursor does not exist in retail); callers pass their burst-loop index.
int GetToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	const NAI::SPosition &posTarget, NAI::EHitLocation eHL, int nExtraAP, const NWorld::CUnit *pTarget,
	const vector<int> &accessibleHLs, int nHitCover, bool bFirstRound,
	const CVec3 &ptIllumination = CVec3(1,1,1), bool bBackstab = false, int nBullet = 0 );
int GetTileToHit( const NWorld::CUnit *pAttacker, NAI::EPose curPose, int nDistance, const CVec3 &ptAttacker,
	CVec3 ptTilePos, NAI::ETileHitLocation eHitLocation, int nExtraAP, int nHitCover, bool bFirstRound,
	const CVec3 &ptIllumination = CVec3(1,1,1), int nBullet = 0 );
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