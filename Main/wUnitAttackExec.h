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
	virtual bool OnLabel() = 0;
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
	bool bComplete;
	CRay ray;
	float fRayToHit;
	CVec3 ptAnimTarget;
	vector<NRPG::CAttackPortion> Attack;
public:
	// tag 9 (&Attack) DROPPED: Attack is the TRANSIENT per-burst fire queue; the release CExecShoot::operator&
	// (@0x3b0ef0) does NOT persist it (its tag 9 is ptAnimTarget, already our tag 8). Persisting it means a save
	// taken mid-burst restores a pending bullet that the first post-load OnLabel->PerformShot fires -> a phantom
	// gunshot (PerformRangedAttack SFX+projectile). Leaving it out makes Attack default empty on load, so
	// PerformShot no-ops until the unit is re-commanded. (Empty after a completed burst, so save-format-safe.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CExecAttack*)this); f.Add(2,&nExtraAP); f.Add(3,&nToHit); f.Add(4,&bMissed); f.Add(5,&bComplete); f.Add(6,&ray); f.Add(7,&fRayToHit); f.Add(8,&ptAnimTarget); return 0; }

private:
	void Scream();

protected:
	int GetExtraAP() const { return nExtraAP; }
	void CalculateExtraAP();
	void PerformShot();
	void SpendAP();

public:
	CExecShoot() {}
	CExecShoot( CUnitServer *_pUS, int _nExtraAP );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual int GetActionAP() const;
	virtual void Start();
	virtual bool CheckBurst( bool bComplete );
	virtual void PerformAttack( const vector<NRPG::CAttackPortion> &attack, const CRay &ray, float fHit = 1.0f );
	virtual bool OnLabel();
	virtual void PrepareShot() {}
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
	virtual void PrepareShot();
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
	virtual void PrepareShot();
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

	virtual bool OnLabel();
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
	virtual bool OnLabel();
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CExecShoot *)this); f.Add(3,&type); return 0; }

protected:
	void LaunchRocket();

public:
	CExecLaunchRocket() {}
	CExecLaunchRocket( CUnitServer *_pUS, const CVec3 &_ptTarget );
	CExecLaunchRocket( CUnitServer *_pUS, EType _type );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void Start();
	virtual bool OnLabel();
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
	bool bDead;
	int n;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&pDeadUnit); f.Add(4,&bDead); f.Add(5,&n); return 0; }
	//
public:
	CExecTakeCorpseOnDeploy() {}
	CExecTakeCorpseOnDeploy( CUnitServer *_pUS, CUnitServer *_pCorpse, bool _bDead );
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CCommandExecute *)this); f.Add(2,&pTarget); return 0; }

public:
	CExecSnipeAim() {}
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

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual void Start();
	virtual bool OnLabel();
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
	bool bCircled;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); f.Add(3,&nDBAnimationID); f.Add(5,&bFreezeAfterLastFrame); return 0; }
	bool bFreezeAfterLastFrame = false;
	//
public:
	CExecPlayAnimation() {}
	CExecPlayAnimation( CUnitServer *_pUS, int _nDBAnimationID, bool _bCircled );
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
