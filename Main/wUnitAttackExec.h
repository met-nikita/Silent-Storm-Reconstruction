#ifndef __WUNITATTACKEXEC_H_
#define __WUNITATTACKEXEC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wUnitCommands.h"
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
	bool CreateAttack( vector<NRPG::CAttackPortion> *pAttack, CUnitServer *pUnitTarget, bool bSpendAmmo = true ) const;

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
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShoot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecShoot: public CExecAttack
{
protected:
	ZDATA_(CExecAttack)
	int nExtraAP;
	int nToHit;
	bool bMissed;
	// --- retail timed-bullet pipeline state (replaces the Jan03 bComplete/vector<CAttackPortion> Attack
	//     synchronous-burst state). ALL transient (rebuilt per shot by SelectRay / driven by Segment);
	//     deliberately NOT serialized -> a mid-shot save restores an IDLE executor (no phantom gunshot),
	//     exactly the intent of the old "tag 9 DROPPED (transient Attack)" reconciliation. ---
	NRPG::CAttackPortion attack;         // single per-bullet portion (was the vector Attack)
	CRay ray;                            // firing ray (hit/miss baked in by PeekRay; CExecLaunchRocket also uses it)
	CVec3 ptAnimTarget;
	STime tNextBulletPrepare = 0;        // retail +0x24 -- 0 == no shot armed
	STime tNextBulletGo = 0;             // retail +0x11c
	bool  bShotInitiated = false;        // retail +0x28 -- a bullet is committed in flight
	int   nBulletPrepared = 0;           // retail +0x2c
	int   nBulletGone = 0;               // retail +0x118
	bool  bOnlyPrepareToShoot = false;   // retail +0x115 -- aim-and-hold selector (dormant: not yet threaded from the cmd)
	bool  bUpdateVision = true;          // retail +0x120 -- ctor default true
public:
	// save stream: DURABLE members only, kept at the retail tag numbers (CExecShoot::operator& @0x3b0ef0).
	// The transient timed-bullet/attack/ray state (retail tags 3-8,d,e) is DROPPED -> a stale/mid-shot save
	// loads as an idle executor (attack empty, tNext*=0 -> Segment early-returns): save-format-safe, and the
	// deliberate no-phantom-shot design of the old dropped-Attack is preserved. (retail tag 3 longBurstSnd is
	// elided: C3DSound is a wMisc.cpp-local class with no header surface / no EndSound in-tree.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecAttack*)this); f.Add(2,&nExtraAP); f.Add(9,&ptAnimTarget); f.Add(10,&nToHit); f.Add(11,&bMissed); f.Add(12,&bOnlyPrepareToShoot); f.Add(15,&bUpdateVision); return 0; }

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
	virtual void PerformAttack();                // @0x3a4480 -- fire ONE ranged attack from `attack`/`ray`
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
	NAI::ETileHitLocation eHL;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CExecShoot *)this); f.Add(2,&eHL); return 0; }

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
	bool bIsHitLocationShot;
	NAI::EHitLocation eHL;
	CPtr<CUnitServer> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecMelee*)this); f.Add(2,&bIsHitLocationShot); f.Add(3,&eHL); f.Add(4,&pTarget); return 0; }

public:
	CExecMeleeUnit() {}
	CExecMeleeUnit( CUnitServer *_pUS, CUnitServer *_pTarget, NAI::EHitLocation _eHL, int _nExtraAttackAP );

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
	void CheckToHitAndDelay( NDb::CRPGGrenade *pGrenade );
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
	CExecCannon( CUnitServer *_pUS, IObject *_pCannon, bool _bEnter );

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
	CExecCorpse( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bTake );

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
// CExecExchangeInventoryItems -- bring an item into a hand slot, displacing whatever is there
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecExchangeInventoryItems: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecExchangeInventoryItems);
	ZDATA_(CCommandExecute)
	CObj<CCmdExchangeInventoryItems> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecExchangeInventoryItems() {}
	CExecExchangeInventoryItems( CUnitServer *_pUS, CCmdExchangeInventoryItems *_pCmd );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false );
	virtual void Run();
};
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
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
