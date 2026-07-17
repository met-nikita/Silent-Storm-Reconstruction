#ifndef __WUNITATTACKEXEC_H_
#define __WUNITATTACKEXEC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wUnitCommands.h"
#include "RPGBullet.h"   // NRPG::SAttackRayInfo -- CExecShoot::rayInfo (retail save tag 8)
#include "wMisc.h"       // NWorld::C3DSound -- CExecShoot::SLongBurstSnd slot (retail save tag 3)
#include "RPGToHit.h"    // NRPG::STargetHLInfo -- CExecMeleeUnit::hlInfo (retail save tag 2)
namespace NRPG
{
	class CGrenadeToHitCalcer;
	class IToolItem;
}
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_GRENADE_DELAY_STEP = 0.1f;
const float F_GRENADE_SPHERE_RADIUS = 0.25f;
//
const float F_GRENADE_CHECK_SIDE = FP_GRID_STEP;
const float F_GRENADE_CHECK_RADIUS = 0.3f;
const float F_GRENADE_CHECK_HEIGHT = 1.2f;
//
const float F_GRAVITY = 10.f;
const float F_HEAL_DISTANCE = 0.8f;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGrenadeParams
{
	CVec3 ptOriginalTarget;
	CVec3 ptStart;
	CVec3 vel;
	float fT;
	int   nSide;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsWithinHumanReach( const CVec3 &ptFrom, const CVec3 &ptTarget, float fPlaneDist );
// NWorld::CanMeleeAttack @0x3a1db0: pure reach gate for a melee swing from `from` at ptTarget --
// F_MELEE_DISTANCE, doubled when the unit carries a reach extender (docked PK cannon). No pose/AP
// checks here (CExecMelee::CanDoIt @0x3a2180 layers those); the composite tile to-hit (@0x2b54a0)
// calls this straight and maps a miss to the -1 "no percentage" sentinel.
bool CanMeleeAttack( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecAttack
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecAttack: public CCommandExecute
{
protected:
	ZDATA_(CCommandExecute)
	bool bAttackCanceled;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bAttackCanceled); return 0; }

protected:
	// retail @0x3a3f30 takes a 4th bool: bAdaptWeapon, forwarded as the mission CreateAttack's 6th arg
	// (weapon-familiarity tail). Shoot execs pass nBulletGone==0 (adapt once per shot); melee/knife true.
	bool CreateAttack( vector<NRPG::CAttackPortion> *pAttack, CUnitServer *pUnitTarget, bool bSpendAmmo = true,
		bool bAdaptWeapon = true ) const;

public:
	CExecAttack( CUnitServer *_pUS = 0 );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const = 0;
	virtual void Start() = 0;
	// retail @0x3a8b40: OnLabel returns VOID and merely ARMS the timed-bullet schedule / ends the shot;
	// TimeLabelReached (still bool, consumed by CUnitServer::Segment) derives its return from bAttackCanceled.
	virtual void OnLabel() = 0;
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void Cancel();
	// BUG 5: retail CExecAttack::UpdateCamera @0x3a3e70 (slot +0x44) -- post the arbitrated auto-focus camera
	// for this attack. Base = shooter, no target (tile/object/melee); CExecShootUnit overrides with pTarget.
	virtual void UpdateCamera();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShoot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecShoot: public CExecAttack
{
public:
	// retail nested SLongBurstSnd (PDB CExecShoot+0x20): the retained long-burst firing sound.
	// Serialized as CExecShoot tag 3 (CallObjectSerialize<SLongBurstSnd> @0x3b0ef0, inner tag 2 =
	// the CObj<C3DSound>). The dtor ends a still-held sound (retail SLongBurstSnd::~ @0x3a1010 area).
	struct SLongBurstSnd
	{
		ZDATA
		CObj<C3DSound> pLongBurstSnd;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pLongBurstSnd); return 0; }
		~SLongBurstSnd() { if ( IsValid( pLongBurstSnd ) ) pLongBurstSnd->EndSound(); }
	};
protected:
	ZDATA_(CExecAttack)
	int nExtraAP;
	int nToHit;
	bool bMissed;
	// --- retail timed-bullet pipeline state (replaces the Jan03 bComplete/vector<CAttackPortion> Attack
	//     synchronous-burst state). Rebuilt per shot by SelectRay / driven by Segment, and (W3
	//     serialization convergence) saved 1:1 at the retail tags 3-8/13/14 @0x3b0ef0. ---
	NRPG::CAttackPortion attack;         // single per-bullet portion (was the vector Attack), retail +0x30, tag 7
	NRPG::SAttackRayInfo rayInfo;        // firing ray + solver carrier (retail +0x70, tag 8; was the dev CRay ray)
	SLongBurstSnd longBurstSnd;          // retail +0x20, tag 3 -- retained long-burst SFX
	CVec3 ptAnimTarget;
	STime tNextBulletPrepare = 0;        // retail +0x24 -- 0 == no shot armed
	STime tNextBulletGo = 0;             // retail +0x11c
	bool  bShotInitiated = false;        // retail +0x28 -- a bullet is committed in flight
	int   nBulletPrepared = 0;           // retail +0x2c
	int   nBulletGone = 0;               // retail +0x118
	bool  bOnlyPrepareToShoot = false;   // retail +0x115 -- aim-and-hold selector (dormant: not yet threaded from the cmd)
	bool  bUpdateVision = true;          // retail +0x120 -- ctor default true
public:
	// full retail tag table (CExecShoot::operator& @0x3b0ef0): 1=base, 2=nExtraAP, 3=longBurstSnd,
	// 4=tNextBulletPrepare, 5=bShotInitiated, 6=nBulletPrepared, 7=attack, 8=rayInfo, 9=ptAnimTarget,
	// 10=nToHit, 11=bMissed, 12=bOnlyPrepareToShoot, 13=nBulletGone, 14=tNextBulletGo, 15=bUpdateVision.
	// (The former "DURABLE members only" hedge is REVERSED -- W3 serialization convergence.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecAttack*)this); f.Add(2,&nExtraAP); f.Add(3,&longBurstSnd); f.Add(4,&tNextBulletPrepare); f.Add(5,&bShotInitiated); f.Add(6,&nBulletPrepared); f.Add(7,&attack); f.Add(8,&rayInfo); f.Add(9,&ptAnimTarget); f.Add(10,&nToHit); f.Add(11,&bMissed); f.Add(12,&bOnlyPrepareToShoot); f.Add(13,&nBulletGone); f.Add(14,&tNextBulletGo); f.Add(15,&bUpdateVision); return 0; }

private:
	void Scream();

protected:
	int  GetExtraAP() const { return nExtraAP; }
	void CalculateExtraAP();
	void SpendAP();
	int  GetBulletDelay() const;                 // @0x3a1f30 -- per-shot label->launch delay (ms) from the weapon DB
	NDb::EShootMode GetShootMode() const;        // @0x3a1ee0 -- the equipped weapon's shoot mode (SM_Snap if none)
	int  GetShortBurstLength() const;            // short-burst bullet count (nRoF/6 + LONGER_SHORT_BURST perk)
	int  GetBulletsPerShot() const;              // v1.2 @0x7a21c0 (NEW helper) -- weapon DB ShotsInOne clamped to >= 1
	STime GetNextBulletTime( STime t ) const;    // @0x3a20e0 -- advance a bullet timestamp by one inter-bullet period
	void OnBulletGo();                           // @0x3a26f0 -- a bullet departs: fire it, then continue/stop the burst
	void CreateFlash( bool bFirstBullet );       // @0x3a4240 -- muzzle flash (dev CreateFlash is arg-less; bFirstBullet unused)
	void CheckUnhide();                          // @0x3a40d0 -- reveal-on-shoot (stub: no dev DB field / hidden-predicate)

public:
	CExecShoot() {}
	CExecShoot( CUnitServer *_pUS, int _nExtraAP );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual int GetActionAP() const;
	virtual void Start();
	virtual void Segment();                      // @0x3a8d20 -- per-tick timed-bullet driver (overrides CCommandExecute::Segment)
	virtual bool CheckBurst( int nFired, bool bDoAction );   // @0x3a1fa0 -- may the burst keep firing (+ optionally spend burst AP)
	virtual void PerformAttack();                // @0x3a4480 -- fire ONE ranged attack from `attack`/`rayInfo`
	virtual void OnLabel();                      // @0x3a8b40 -- arm the timed schedule / end the shot (void)
	virtual void SelectRay() {}                  // @0x3a4720/@0x3a49b0 -- pick the firing ray+portion (Jan03 PrepareShot renamed; Tile/Unit override)
	virtual void CheckShotResult() {}
	virtual bool IsAttackCanceled() { return false; }
	virtual bool IsAccidental() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootTile
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecShootTile: public CExecShoot
{
	OBJECT_BASIC_METHODS(CExecShootTile);
private:
	ZDATA_( CExecShoot )
	// retail @0x3b1120: CExecShootTile adds NO data members (sizeof == sizeof CExecShoot); the Jan03
	// ETileHitLocation eHL member is GONE -- the tile to-hit always evaluates THL_LOWER (SelectRay).
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CExecShoot *)this); return 0; }

public:
	CExecShootTile() {}
	CExecShootTile( CUnitServer *_pUS, const CVec3 &_ptTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void SelectRay();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecShootUnit: public CExecShoot
{
	OBJECT_BASIC_METHODS(CExecShootUnit);
private:
	ZDATA_(CExecShoot)
	CPtr<CUnitServer> pTarget;
	NAI::EHitLocation eHL;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecShoot*)this); f.Add(2,&pTarget); f.Add(3,&eHL); return 0; }

public:
	CExecShootUnit() {}
	CExecShootUnit( CUnitServer *_pUS, NWorld::CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void UpdateCamera();   // BUG 5: retail CExecShootUnit::UpdateCamera @0x3a47c0 -- focus with the shot target
	virtual void SelectRay();
	virtual void CheckShotResult();
	virtual bool IsAttackCanceled();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMelee
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecMelee: public CExecAttack
{
protected:
	ZDATA_(CExecAttack)
	CVec3 ptTarget;
	int nExtraAP;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecAttack*)this); f.Add(2,&ptTarget); f.Add(3,&nExtraAP); return 0; }

protected:
	int GetExtraAP() const { return nExtraAP; }

public:
	CExecMelee() {}
	CExecMelee( CUnitServer *_pUS, int _nExtraAP );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Start();
	virtual void PerformAttack( const vector<NRPG::CAttackPortion> &attack, const CRay &ray, float fHit = 1.0f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMeleeTile
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecMeleeTile: public CExecMelee
{
	OBJECT_BASIC_METHODS(CExecMeleeTile);
public:
	CExecMeleeTile() {}
	CExecMeleeTile( CUnitServer *_pUS, const CVec3 &_ptTarget );

	virtual void OnLabel();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMeleeUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecMeleeUnit: public CExecMelee
{
	OBJECT_BASIC_METHODS(CExecMeleeUnit);
private:
	ZDATA_(CExecMelee)
	// retail members (PDB: hlInfo @+0x2c, pTarget @+0x3c): the Jan03 pair {bool bIsHitLocationShot;
	// NAI::EHitLocation eHL} was replaced by ONE NRPG::STargetHLInfo -- eHL plus the accessible-HL
	// set SelectTargetHLs resolves at Start, serialized together so a mid-swing save restores the
	// exact OnLabel to-hit inputs (no recompute after load).
	NRPG::STargetHLInfo hlInfo;
	CPtr<CUnitServer> pTarget;
	// retail wire (operator& @0x3b0d90): 1 = CExecMelee base, 2 = hlInfo (CallObjectSerialize ->
	// inner tags 2 eHL / 3 accessibleHLs @0x3b0e10), 3 = pTarget. (Jan03 had 2=bIsHitLocationShot,
	// 3=eHL, 4=pTarget.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecMelee*)this); f.Add(2,&hlInfo); f.Add(3,&pTarget); return 0; }

	// v1.2 @0x7a1840 -- the v1.1 Start prologue (@0x3a1640) factored into a bool helper, shared with
	// the NEW v1.2 CanDoIt override: resolve *pInfo via NRPG::SelectTargetHLs (accessibility measured
	// from `pos`), then the aim point on the target hull into *pRes. false = no accessible hit
	// location resolves (or the target has no valid hull); *pRes is then left UNTOUCHED (v1.2 delta:
	// v1.1 always overwrote ptTarget via GetUnitHLPos).
	bool SelectTargetHLPos( const NAI::SUnitPosition &pos, CVec3 *pRes, NRPG::STargetHLInfo *pInfo ) const;

public:
	CExecMeleeUnit() {}
	CExecMeleeUnit( CUnitServer *_pUS, CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP );

	// v1.2 NEW override @0x7a2510 (base CExecMelee::CanDoIt @0x3a2180 unchanged): a valid target with
	// no resolvable hit location is rejected UCR_TARGET_OUT_OF_RANGE up front.
	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void Start();
	virtual void OnLabel();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecThrowGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecThrowGrenade: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecThrowGrenade);
	ZDATA_(CCommandExecute)
	CVec3 ptTarget;
	SGrenadeParams grenadeParams;
	CPtr<NRPG::CGrenadeToHitCalcer> pToHitCalcer;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&ptTarget); f.Add(3,&grenadeParams); f.Add(4,&pGrenade); f.Add(5,&pNextSameItem); f.Add(6,&bUpdateVision); return 0; }
	CPtr<NRPG::IGrenadeItem> pGrenade;
	CObj<NRPG::IInventoryItem> pNextSameItem;
	bool bUpdateVision = false;

protected:
	void CheckToHitAndDelay( NRPG::IGrenadeItem *pGrenade );   // retail @0x3a1700 takes the ITEM
	void ThrowGrenade();

public:
	CExecThrowGrenade() {}
	CExecThrowGrenade( CUnitServer *_pUS, const CVec3 &_ptTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecLaunchRocket
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecLaunchRocket: public CExecShoot
{
	OBJECT_BASIC_METHODS(CExecLaunchRocket);
public:
	enum EType
	{
		NORMAL,
		ACCIDENTAL,
		TEST
	};
private:
	ZDATA
	ZPARENT( CExecShoot );
	EType type;
	STime tRocket;   // @+0x128 armed launch time (0 = not armed). TRANSIENT -- deliberately NOT serialized so a
	                 // mid-flight save cannot restore an armed rocket (phantom launch), matching the dropped Attack.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CExecShoot *)this); f.Add(3,&type); return 0; }

protected:
	void LaunchRocket();

public:
	CExecLaunchRocket(): tRocket(0) {}
	CExecLaunchRocket( CUnitServer *_pUS, const CVec3 &_ptTarget );
	CExecLaunchRocket( CUnitServer *_pUS, EType _type );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void Start();
	virtual void OnLabel();
	virtual void Segment();   // @0x3a58a0 -- deferred rocket launch (fires once game time reaches tRocket)
	virtual bool IsAccidental() const { return type == ACCIDENTAL; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecPanzerklein
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecPanzerklein: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecPanzerklein);
private:
	ZDATA_(CCommandExecute)
	CObj<CCmdTakeCorpse> pCmd;
	NRPG::EAction action;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); f.Add(3,&action); return 0; }

public:
	CExecPanzerklein() {}
	CExecPanzerklein( CUnitServer *_pUS, CCmdTakeCorpse *_pCmd = 0 );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCannon: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCannon);
private:
	ZDATA_(CCommandExecute)
	bool bEnter;
	CPtr<IObject> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bEnter); f.Add(3,&pCannon); f.Add(4,&pCmd); return 0; }
	CObj<CCmdCannon> pCmd;

public:
	CExecCannon() {}
	// retail ctor @0x3a5e50 takes the reserving command as the 4th arg (the CanDoIt lock gate
	// @0x3a2360 needs it); the exit path (bEnter=false) passes none.
	CExecCannon( CUnitServer *_pUS, IObject *_pCannon, bool _bEnter, CCmdCannon *_pCmd = 0 );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecUsePassage
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecUsePassage: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecUsePassage);
	ZDATA
	ZPARENT( CCommandExecute )
	CPtr<CCmdUsePassage> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&pCmd); return 0; }
public:
	//
	CExecUsePassage() {}
	CExecUsePassage( CUnitServer *_pUS, CCmdUsePassage *_pCmd );
	//
	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCorpse
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCorpse: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCorpse);
private:
	ZDATA_(CCommandExecute)
	bool bTake;
	CPtr<CUnitServer> pDeadUnit;
		CPtr<CCmdTakeCorpse> pCmd;
	bool bCorpseInPK = false;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bTake); f.Add(3,&pDeadUnit); f.Add(4,&pCmd); f.Add(5,&bCorpseInPK); return 0; }

public:
	CExecCorpse() {}
	// retail ctor @0x3a6160 takes the reserving command as the 4th arg (the CanDoIt lock gate
	// @0x3a61d0 needs it); the drop path (CCmdDropCorpse) passes none.
	CExecCorpse( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bTake, CCmdTakeCorpse *_pCmd = 0 );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void Cancel();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTakeCorpseOnDeploy
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecTakeCorpseOnDeploy: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecTakeCorpseOnDeploy);
private:
	ZDATA
	ZPARENT( CCommandExecute );
	CPtr<CUnitServer> pDeadUnit;
	int n;
	// retail CExecTakeCorpseOnDeploy::operator& DROPPED the dead-stored bDead (never read here -- InitAsCorpse was
	// moved upstream) and renumbered n tag5->tag4. Serialized tags now 2 base / 3 pDeadUnit / 4 n (retail-matching).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&pDeadUnit); f.Add(4,&n); return 0; }
	//
public:
	CExecTakeCorpseOnDeploy() {}
	CExecTakeCorpseOnDeploy( CUnitServer *_pUS, CUnitServer *_pCorpse );
	//
	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecHeal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecHeal: public CCommandExecute
{
	OBJECT_NOCOPY_METHODS(CExecHeal);
private:
	ZDATA_(CCommandExecute)
	CPtr<CUnitServer> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pTarget); return 0; }
	//
public:
	CExecHeal() {}
	CExecHeal( CUnitServer *_pUS, CUnitServer *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual void AnimationFinished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSetTrap: public CCommandExecute
{
	OBJECT_NOCOPY_METHODS(CExecSetTrap);
private:
	ZDATA_(CCommandExecute)
	CPtr<CWindowDoor> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pTarget); return 0; }
	//
public:
	CExecSetTrap() {}
	CExecSetTrap( CUnitServer *_pUS, CWindowDoor *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecDisarmTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecDisarmTrap: public CCommandExecute
{
	OBJECT_NOCOPY_METHODS(CExecDisarmTrap);
private:
	ZDATA_(CCommandExecute)
	CPtr<CWindowDoor> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pTarget); return 0; }
	//
public:
	CExecDisarmTrap() {}
	CExecDisarmTrap( CUnitServer *_pUS, CWindowDoor *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetMine
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSetMine: public CCommandExecute
{
	OBJECT_NOCOPY_METHODS(CExecSetMine);
private:
	ZDATA_(CCommandExecute)
	CObj<CCmdSetMineOnTile> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
	//
	NRPG::IMineItem* GetMine() const;
	bool GetMinesNearTarget( vector<CPtr<CMine> > *pRes ) const;
public:
	CExecSetMine() {}
	CExecSetMine( CUnitServer *_pUS, CCmdSetMineOnTile *_pCmd ) : CCommandExecute(_pUS), pCmd(_pCmd) {}

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecDisarmMine
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecDisarmMine: public CCommandExecute
{
	OBJECT_NOCOPY_METHODS(CExecDisarmMine);
private:
	ZDATA_(CCommandExecute)
	CPtr<CMine> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pTarget); return 0; }
	//
public:
	CExecDisarmMine() {}
	CExecDisarmMine( CUnitServer *_pUS, CMine *_pTarget ) : CCommandExecute(_pUS), pTarget(_pTarget) {}

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSnipeAim
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSnipeAim: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecSnipeAim);
private:
	ZDATA_( CCommandExecute )
	CPtr<CUnitServer> pTarget;
	// @0x3a7280/@0x3b0260 -- release added a 4-byte hitLocation member at +0x1c (after
	// pTarget@+0x18) holding the snipe's called-shot hit location; operator& serializes it
	// as tag 3 (DataChunk, 4 bytes). Mirrors CUnitStateSniping's deferred-called-shot note:
	// this predecessor snipe has no called-shot machinery, so it stays HL_BODY (un-called) --
	// SAVE-FORMAT member only; behavior deferred (3-arg ctor / caller threading FLAGGED).
	NAI::EHitLocation hitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CCommandExecute *)this); f.Add(2,&pTarget); f.Add(3,&hitLocation); return 0; }

public:
	CExecSnipeAim() : hitLocation(NAI::HL_BODY) {}
	CExecSnipeAim( CUnitServer *_pUnitServer, CUnitServer *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual void AnimationFinished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCollectSnipeAP
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCollectSnipeAP: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCollectSnipeAP );
private:
	ZDATA_( CCommandExecute )
	ECollectSnipeAP eAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CCommandExecute *)this); f.Add(2,&eAP); return 0; }
	//
	int GetResiduaryAP() const;
public:
	CExecCollectSnipeAP () {}
	CExecCollectSnipeAP ( CUnitServer *pUnitServer, ECollectSnipeAP eAP );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	int GetAPToCollect() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecThrowKnife
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecThrowKnife: public CExecAttack
{
	OBJECT_BASIC_METHODS(CExecThrowKnife);
private:
	ZDATA_(CCommandExecute)
	CVec3 ptTarget;
	CPtr<CUnitServer> pTarget;
		CObj<NRPG::IInventoryItem> pNextSameItem;
	NAI::EHitLocation eHL = NAI::HL_ANY;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&ptTarget); f.Add(3,&pTarget); f.Add(4,&pNextSameItem); f.Add(5,&eHL); return 0; }

protected:
	void ThrowKnife();

public:
	CExecThrowKnife() {}
	CExecThrowKnife( CUnitServer *_pUS, const CVec3 &_ptTarget, CUnitServer *_pTarget = 0 );
	// @0x3a7470 -- targeted-unit ctor: aim point computed from pTarget's hit location (eHL).
	CExecThrowKnife( CUnitServer *_pUS, NAI::EHitLocation _eHL, CUnitServer *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void Start();
	virtual void OnLabel();
	virtual int GetStartAP() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecArrangeInventory
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecArrangeInventory: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecArrangeInventory);
private:
	ZDATA_(CCommandExecute)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); return 0; }
public:
	CExecArrangeInventory() {}
	CExecArrangeInventory( CUnitServer *pUS ): CCommandExecute(pUS) {}
	virtual void Run()
	{
		pUS->GetUnitRPG()->GetInventory()->ArrangeItems();
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CreateInventoryItemForUnit @0x3ab3c0: SYNCHRONOUS backpack insert (no executor, no
// unit command, no AP) -- lua UnitCreateItem @0x2fbfd0 calls it directly; CExecCreateInventoryItem::
// Run @0x3ab770 delegates to it.
void CreateInventoryItemForUnit( CUnitServer *pUS, CCmdCreateInventoryItem *pCmd );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMoveInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCreateInventoryItem: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCreateInventoryItem);
	ZDATA_(CCommandExecute)
	CObj<CCmdCreateInventoryItem> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecCreateInventoryItem() {}
	CExecCreateInventoryItem( CUnitServer *_pUS, CCmdCreateInventoryItem *_pCmd );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMoveInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecMoveInventoryItem: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecMoveInventoryItem);
	ZDATA_(CCommandExecute)
	int nStage;
	bool bTwoHeavy;
	CObj<CCmdMoveInventoryItem> pCmd;
	CPtr<CUnitServer> pUSTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&nStage); f.Add(3,&bTwoHeavy); f.Add(4,&pCmd); f.Add(5,&pUSTarget); return 0; }
public:
	CExecMoveInventoryItem() {}
	CExecMoveInventoryItem( CUnitServer *_pUS, CCmdMoveInventoryItem *_p );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void AnimationFinished();
	virtual void Cancel();
	virtual int GetStartAP() const;        // @0x3a7b40 retail-only override -> per-action AP cost
private:
	// @0x3a7990 retail-only: classify the queued move into its reach-animation action.
	NRPG::EAction GetActionType() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCreateAndActivateInventoryItem -- create an item from a DB record and slot it into a hand
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCreateAndActivateInventoryItem: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCreateAndActivateInventoryItem);
	ZDATA_(CCommandExecute)
	CObj<CCmdCreateAndActivateInventoryItem> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecCreateAndActivateInventoryItem() {}
	CExecCreateAndActivateInventoryItem( CUnitServer *_pUS, CCmdCreateAndActivateInventoryItem *_pCmd );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CExecExchangeInventoryItems REMOVED -- retail-absent; retail CreateExecutor @0x3b37b0 composes
//  the exchange from a CExecQueue of registered execs, so it serializes cleanly mid-exchange)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecPlayAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecPlayAnimation: public CCommandExecute
{
	OBJECT_BASIC_METHODS( CExecPlayAnimation );
	ZDATA
	ZPARENT( CCommandExecute );
	int nDBAnimationID;
	// @0x3a7cc0 — Jan03 `bool bCircled` removed: retail has no such member; +0x1c is bFreezeAfterLastFrame.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&nDBAnimationID); f.Add(5,&bFreezeAfterLastFrame); return 0; }
	bool bFreezeAfterLastFrame = false;
	//
public:
	CExecPlayAnimation() {}
	CExecPlayAnimation( CUnitServer *_pUS, int _nDBAnimationID, bool _bFreezeAfterLastFrame );
	//
	virtual void Run();
	virtual void AnimationFinished();
	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
	virtual void Cancel();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTalk
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecTalk: public CCommandExecute
{
	OBJECT_BASIC_METHODS( CExecTalk );
	ZDATA
	ZPARENT( CCommandExecute );
	CPtr<CUnitServer> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&pTarget); return 0; }
	//
public:
	CExecTalk() {}
	CExecTalk( CUnitServer *_pUS, CUnitServer *_pTarget );
	//
	virtual void Run();
	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecNotHeroWantsToTalk
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3b56f0 -- one-shot executor issued when a NON-hero unit is told to talk to an NPC: it
// throws CEventOnNotHeroWantsToTalk (which drives the CAckNPCInteraction voice bark) and finishes.
class CExecNotHeroWantsToTalk: public CCommandExecute
{
	OBJECT_BASIC_METHODS( CExecNotHeroWantsToTalk );
	ZDATA
	ZPARENT( CCommandExecute );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); return 0; }
	//
public:
	CExecNotHeroWantsToTalk() {}
	CExecNotHeroWantsToTalk( CUnitServer *_pUS ): CCommandExecute( _pUS ) {}
	//
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
