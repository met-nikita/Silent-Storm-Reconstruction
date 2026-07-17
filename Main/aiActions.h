#ifndef __AIACTIONS_H_
#define __AIACTIONS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the 19 concrete CAIAction subclasses (structural port).
//
// SUPERSEDES the dev aiAttackAction.h (CAIShootAction/CAIThrowGrenadeAction/CAILaunchRocketAction in the
// OLD form: Do(IAILogContainer*), named SAIShootActionInfo, no cache) and aiMoveAction's action role.
// Each release action: derives CAIAction (aiActionBase.h), declares a nested `struct SInfo` (first member
// `bool bCanDo`), owns `SActionInfo<Self,SInfo> info`, and implements GetInfoInner(place,SInfo*) +
// Do(CAILog*) + ComparePlaces + CanDo (via info) + ResetInfoHash (via info).
//
// WIP - NOT yet in Main.vcxproj; folded over dev aiAttackAction.h at the phase-7 swap.
//
// The full repertoire (CAIAttackLogic ctor @0x00420f30) - reconstruction status per action:
//   DONE here (faithful, adapted from dev aiAttackAction.cpp + vtable_action.txt):
//     CAIShootAction, CAIThrowGrenadeAction, CAILaunchRocketAction
//   CONTRACT-ONLY (bodies in phase 3b cont. - decompile @ vtable_action.txt slot5/6/7):
//     CAIReloadAction, CAIMeleeAction, CAIThrowKnifeAction, CAILootAction, CAIHealAction,
//     CAIMoveToEnemyAction, CAIBeginSnipeAction, CAICollectSnipeAPAction, CAISnipeShotAction,
//     CAICancelSnipeAction, CAIDockWithHGAction, CAIUndockFromHGAction, CAIShootFromHGAction,
//     CAITerrorPKAction, CAIWearPKAction, CAILeavePKAction
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiActionBase.h"      // CAIAction, SActionInfo, SPlaceWithAP
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb { enum EShootMode; }
namespace NWorld { class CCommand; }
namespace NWorld { class CDFrozenItem; }
namespace NWorld { class CUnitServer; }
namespace NWorld { class CCannon; }
namespace NRPG { class IInventoryItem; }
namespace NAI
{
class CAIFireArmsWeapon;
class CAIFireArmsWeaponBase;
class CAIGrenadeWeapon;
class CAIFirstAid;
class CAIMeleeWeapon;
class CAIThrowingWeapon;
struct SAIUnitGroup;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIShootAction - fire the best firearm at the current enemy from a place.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIShootAction: public CAIAction
{
	OBJECT_BASIC_METHODS( CAIShootAction );
	ZDATA
	ZPARENT( CAIAction );
public:
	// Layout matches the release CAIShootAction::SInfo (Game.pdb, size 0x1c): bools packed at +0/+1,
	// nToHit/fCover/shootMode/hitLocation, the weapon, and nAPToSpend (the shot budget Do() spends down).
	struct SInfo
	{
		bool bCanDo;                // +0x00
		bool bKillTargetCertainly;  // +0x01
		int  nToHit;                // +0x04
		float fCover;               // +0x08
		NDb::EShootMode shootMode;  // +0x0c
		EHitLocation hitLocation;   // +0x10
		CPtr<CAIFireArmsWeapon> pWeapon;   // +0x14
		int  nAPToSpend;            // +0x18
		SInfo(): bCanDo( false ), bKillTargetCertainly( false ), nToHit( 0 ), fCover( 0 ), shootMode( (NDb::EShootMode)0 ), hitLocation( HL_ANY ), nAPToSpend( 0 ) {}
		// retail @0x20200: {2 bCanDo(1B), 3 bKillTargetCertainly(1B), 4 nToHit, 5 fCover, 6 shootMode,
		// 7 hitLocation, 8 pWeapon ref, 9 nAPToSpend} = 42B in the save. Without this operator& the
		// SActionInfo hash-map value fell into the POD DataChunk path (raw 28B memcpy) -> wire-audit
		// SIZE 42v28 @3.3.2; byte-walked slot 4 obj#69920.
		int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &bKillTargetCertainly ); f.Add( 4, &nToHit ); f.Add( 5, &fCover ); f.Add( 6, &shootMode ); f.Add( 7, &hitLocation ); f.Add( 8, &pWeapon ); f.Add( 9, &nAPToSpend ); return 0; }
	};
private:
	SActionInfo<CAIShootAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIShootAction() {}
	CAIShootAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	//
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowGrenadeAction - throw the best grenade at the nearest viable enemy group.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIThrowGrenadeAction: public CAIAction
{
	OBJECT_BASIC_METHODS( CAIThrowGrenadeAction );
	ZDATA
	ZPARENT( CAIAction );
public:
	// Layout matches the release CAIThrowGrenadeAction::SInfo (Game.pdb, size 0x1c): bCanDo/bBadGroupHealth
	// packed at +0/+1, the target group size + throw point, the grenade, and the AP to spend.
	struct SInfo
	{
		bool bCanDo;             // +0x00
		bool bBadGroupHealth;    // +0x01
		int  nTargetSize;        // +0x04
		CVec3 ptTarget;          // +0x08  throw point (group centre from GetNearestGroup)
		CPtr<CAIGrenadeWeapon> pGrenade;   // +0x14
		int  nAPToSpend;         // +0x18
		SInfo(): bCanDo( false ), bBadGroupHealth( false ), nTargetSize( 0 ), nAPToSpend( 0 ) {}
		// retail @0x1fe10: {2 bCanDo(1B), 3 bBadGroupHealth(1B), 4 nTargetSize, 5 ptTarget(12B raw),
		// 6 pGrenade ref, 7 nAPToSpend} = 38B (wire-audit SIZE 38v28 @3.3.2; slot 4 obj#69921).
		int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &bBadGroupHealth ); f.Add( 4, &nTargetSize ); f.Add( 5, &ptTarget ); f.Add( 6, &pGrenade ); f.Add( 7, &nAPToSpend ); return 0; }
	};
private:
	SActionInfo<CAIThrowGrenadeAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIThrowGrenadeAction() {}
	CAIThrowGrenadeAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	//
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILaunchRocketAction - fire a rocket launcher at the nearest viable enemy group.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILaunchRocketAction: public CAIAction
{
	OBJECT_BASIC_METHODS( CAILaunchRocketAction );
	ZDATA
	ZPARENT( CAIAction );
public:
	// Layout matches the release CAILaunchRocketAction::SInfo (Game.pdb, size 0x18): same shape as the
	// grenade info (bCanDo/bBadGroupHealth, target size + point, weapon). The release weapon type is
	// CAIRocketLauncherWeapon; the dev firearm (what GetBestRocketLaunchers returns) is kept pending that
	// type's reconstruction. No bNeedReload field - an empty launcher is just not do-able (the reload
	// action handles reloading), so the old pre-reload path is dropped.
	struct SInfo
	{
		bool bCanDo;             // +0x00
		bool bBadGroupHealth;    // +0x01
		int  nTargetSize;        // +0x04
		CVec3 ptTarget;          // +0x08
		CPtr<CAIFireArmsWeapon> pWeapon;   // +0x14 (release: CAIRocketLauncherWeapon)
		SInfo(): bCanDo( false ), bBadGroupHealth( false ), nTargetSize( 0 ) {}
		// retail @0x20600: {2 bCanDo(1B), 3 bBadGroupHealth(1B), 4 nTargetSize, 5 ptTarget(12B raw),
		// 6 pWeapon ref} = 32B (wire-audit SIZE 32v24 @3.3.2).
		int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &bBadGroupHealth ); f.Add( 4, &nTargetSize ); f.Add( 5, &ptTarget ); f.Add( 6, &pWeapon ); return 0; }
	};
private:
	SActionInfo<CAILaunchRocketAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAILaunchRocketAction() {}
	CAILaunchRocketAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	//
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Support actions (reload/heal/melee/knife/loot/move). Each follows the same shape; CanDo/ResetInfoHash
// delegate to `info`. Verified from reconstruction/exports/actions_support.c + vtable_action.txt.
////////////////////////////////////////////////////////////////////////////////////////////////////
// Common boilerplate for an action with one weapon/item member in SInfo.
#define AI_ACTION_DECL( Cls ) \
	OBJECT_BASIC_METHODS( Cls ); \
	ZDATA ZPARENT( CAIAction ); \
	SActionInfo<Cls, SInfo> info; \
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; } \
	public: \
	Cls() {} \
	Cls( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {} \
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const; \
	virtual void Do( CAILog *pLog ) const; \
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const; \
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); } \
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIReloadAction - reload the best firearm that needs it. (-> currentPlaceSource)
class CAIReloadAction: public CAIAction
{
public:
	// retyped from the release-only CAIFireArmsWeaponBase to the concrete dev CAIFireArmsWeapon (the dev
	// firearm IS the reload target; this lets CAILogReloadWeapon take it directly).
	// retail @0x18be0: {2 bCanDo(1B), 3 pWeapon ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<CAIFireArmsWeapon> pWeapon; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pWeapon ); return 0; } };
private:
	AI_ACTION_DECL( CAIReloadAction )
};
// CAIHealAction - use a first-aid kit when hurt enough. (-> currentPlaceSource)
class CAIHealAction: public CAIAction
{
public:
	// retail @0x18fa0: {2 bCanDo(1B), 3 pFirstAid ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<CAIFirstAid> pFirstAid; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pFirstAid ); return 0; } };
private:
	AI_ACTION_DECL( CAIHealAction )
};
// CAIMeleeAction - melee the enemy if in reach. (-> enemyPlaceSource)
class CAIMeleeAction: public CAIAction
{
public:
	// retail @0x20d70: {2 bCanDo(1B), 3 pWeapon ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<CAIMeleeWeapon> pWeapon; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pWeapon ); return 0; } };
private:
	AI_ACTION_DECL( CAIMeleeAction )
};
// CAIThrowKnifeAction - throw a knife at the enemy. (-> attackPlaceSource)
class CAIThrowKnifeAction: public CAIAction
{
public:
	// retail @0x209d0: {2 bCanDo(1B), 3 pWeapon ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<CAIThrowingWeapon> pWeapon; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pWeapon ); return 0; } };
private:
	AI_ACTION_DECL( CAIThrowKnifeAction )
};
// CAIMoveToEnemyAction - advance toward the enemy (Do is empty; the place-move is logged by DoAction).
class CAIMoveToEnemyAction: public CAIAction
{
public:
	// retail (folded 1-byte SInfo serializer, cf. @0x27b80 shape): {2 bCanDo(1B)} = 3B
	// (wire-audit SIZE 3v1 @3.3.2 x26).
	struct SInfo { bool bCanDo; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); return 0; } };
private:
	AI_ACTION_DECL( CAIMoveToEnemyAction )
};
// CAILootAction - pick up necessary items / drop redundant ones. SInfo carries the chosen item, the
// loot/drop lists. GetInfoInner @0x00463210 reconstructed in aiLootAction.cpp.
class CAILootAction: public CAIAction
{
public:
	struct SInfo
	{
		bool bCanDo;
		CPtr<NWorld::CDFrozenItem> pItem;   // the chosen (most-necessary) item to loot
		list< CPtr<NWorld::CDFrozenItem> > otherItem;   // retail PDB: nstl::list<CPtr<CDFrozenItem>> (was CObj here)
		unordered_map< CPtr<NWorld::CDFrozenItem>, vector< CPtr<NRPG::IInventoryItem> >, SPtrHash > itemsToDrop;
		SInfo(): bCanDo( false ) {}
		// retail @0x19b30: {2 bCanDo(1B), 3 pItem ref, 4 otherItem list, 5 itemsToDrop hash-map}.
		// Without this the whole SInfo (48B of std containers!) was raw-memcpy'd -> wire-audit
		// SIZE 13v48 @4.3.2; byte-walked slot 4 obj#70032 (13B = 02 02 00 | 03 08 +4B | 04 00 | 05 00).
		int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pItem ); f.Add( 4, &otherItem ); f.Add( 5, &itemsToDrop ); return 0; }
	};
private:
	// NOT AI_ACTION_DECL: retail CAILootAction carries EPose wishPose @+0x10 (ctor param: attack/guard
	// pass RUN -- decomp @0x20f30/@0x4e7ed -- after-combat passes WALK) and serializes it at tag 3
	// (operator& @0x19840: 2=base, 3=wishPose 4B, 4=actionInfo).
	OBJECT_BASIC_METHODS( CAILootAction );
	ZDATA ZPARENT( CAIAction );
	EPose wishPose;
	SActionInfo<CAILootAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &wishPose ); f.Add( 4, &info ); return 0; }
	public:
	CAILootAction(): wishPose( RUN ) {}
	CAILootAction( IAIUnit *_pUnit, EPose pose ): CAIAction( _pUnit ), wishPose( pose ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
#undef AI_ACTION_DECL
////////////////////////////////////////////////////////////////////////////////////////////////////
// Specialized actions: snipe state machine, half-grown (HG) vehicle, panzerklein (PK). Heavy + game
// specific (GetInfoInner 199..1107 B). Declared here to complete the 19-action contract; ComparePlaces
// faithful, GetInfoInner/Do deferred to the build-settle (addresses in aiActions.cpp). SInfo is minimal
// (bCanDo) pending the deferred GetInfoInner bodies. Verified from reconstruction/exports/actions_special.c.
////////////////////////////////////////////////////////////////////////////////////////////////////
// SInfo wire (retail, folded 1-byte serializer shape): {2 bCanDo(1B)} = 3B (wire-audit SIZE 3v1 @3.3.2).
#define AI_ACTION_DECL_MIN( Cls ) \
	public: struct SInfo { bool bCanDo; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); return 0; } }; \
	private: \
	OBJECT_BASIC_METHODS( Cls ); \
	ZDATA ZPARENT( CAIAction ); \
	SActionInfo<Cls, SInfo> info; \
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; } \
	public: \
	Cls() {} \
	Cls( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {} \
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const; \
	virtual void Do( CAILog *pLog ) const; \
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const; \
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); } \
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
// snipe state machine: aim (Begin) -> accumulate AP over turns (CollectAP) -> fire (Shot) / abort (Cancel).
// CollectAP / Shot / Cancel are reconstructed in aiSnipeAction.cpp; Begin's GetInfoInner kill-zone scan
// (inlined @0x004a49b0..) remains deferred -- the decode itself left it a hook (see aiSnipeAction.cpp).
// CAIBeginSnipeAction - decide to start sniping the current enemy. SInfo carries the chosen snipe weapon,
// the locked target, and the aimed hit location.
class CAIBeginSnipeAction: public CAIAction
{
public:
	struct SInfo
	{
		bool bCanDo;
		CPtr<CAIFireArmsWeapon> pWeapon;
		CPtr<IAIUnit> pTarget;
		EHitLocation hitLocation;
		SInfo(): bCanDo( false ), hitLocation( HL_ANY ) {}
		// retail @0x2ab30: {2 bCanDo(1B), 3 pWeapon ref, 4 pTarget ref, 5 hitLocation} = 21B
		// (wire-audit SIZE 21v16 @3.3.2).
		int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pWeapon ); f.Add( 4, &pTarget ); f.Add( 5, &hitLocation ); return 0; }
	};
private:
	OBJECT_BASIC_METHODS( CAIBeginSnipeAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAIBeginSnipeAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIBeginSnipeAction() {}
	CAIBeginSnipeAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
class CAISnipeShotAction:      public CAIAction { AI_ACTION_DECL_MIN( CAISnipeShotAction ) };
class CAICancelSnipeAction:    public CAIAction { AI_ACTION_DECL_MIN( CAICancelSnipeAction ) };
// CAICollectSnipeAPAction - bank AP into the snipe pool over turns. SInfo carries the AP to collect this turn.
class CAICollectSnipeAPAction: public CAIAction
{
public:
	// retail @0x27b80: {2 bCanDo(1B), 3 nAP} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; int nAP; SInfo(): bCanDo( false ), nAP( 0 ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &nAP ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAICollectSnipeAPAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAICollectSnipeAPAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAICollectSnipeAPAction() {}
	CAICollectSnipeAPAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
// half-grown vehicle (mounted gun): dock / undock / shoot-from
// CAIDockWithHGAction - dock with the best reachable stationary cannon. SInfo carries the chosen gun.
// The SInfo CPtr<NWorld::CCannon> pGun members (here + CAIUndockFromHGAction + CAIShootFromHGAction)
// instantiate CPtrBase<NWorld::CCannon,CObjectBase::SRef> (VA = RVA + 0x400000):
//   @0x00423300 AddRef   @0x00423330 SetObject
class CAIDockWithHGAction: public CAIAction
{
public:
	// retail @0x29f40: {2 bCanDo(1B), 3 pGun ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<NWorld::CCannon> pGun; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pGun ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAIDockWithHGAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAIDockWithHGAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIDockWithHGAction() {}
	CAIDockWithHGAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
// CAIUndockFromHGAction - abandon the manned cannon when it stops being worth it. SInfo carries the gun.
class CAIUndockFromHGAction: public CAIAction
{
public:
	// retail (ICF-folded with CAIDockWithHGAction's @0x29f40, identical layout): {2 bCanDo(1B),
	// 3 pGun ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<NWorld::CCannon> pGun; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pGun ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAIUndockFromHGAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAIUndockFromHGAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIUndockFromHGAction() {}
	CAIUndockFromHGAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
// CAIShootFromHGAction - fire the manned cannon at the current enemy. SInfo carries the gun + target.
class CAIShootFromHGAction: public CAIAction
{
public:
	// retail @0x2a440: {2 bCanDo(1B), 3 pGun ref, 4 pTarget ref} = 15B (wire-audit SIZE 15v12 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<NWorld::CCannon> pGun; CPtr<IAIUnit> pTarget; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pGun ); f.Add( 4, &pTarget ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAIShootFromHGAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAIShootFromHGAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIShootFromHGAction() {}
	CAIShootFromHGAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
// panzerklein (mech suit): terror (intimidate) / wear (enter) / leave (exit)
// CAITerrorPKAction - in a dying panzerklein, walk into the nearest enemy group (terror weapon works
// by proximity). SInfo carries the rampage destination (custom, beyond the minimal {bCanDo}).
class CAITerrorPKAction: public CAIAction
{
public:
	// retail @0x291f0: {2 bCanDo(1B), 3 place (nested SUnitPosition operator& @0x91790)} = 22B in
	// slot 4 (wire-audit SIZE 22v16 @3.3.2; byte-walked: 3 = {2 SPosition{1 place 4B, 2 pNet ref}, 4 bRun 1B}).
	struct SInfo { bool bCanDo; SUnitPosition place; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &place ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAITerrorPKAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAITerrorPKAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAITerrorPKAction() {}
	CAITerrorPKAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
// CAIWearPKAction - climb INTO a free panzerklein suit. SInfo carries the chosen suit's unit server.
class CAIWearPKAction: public CAIAction
{
public:
	// retail @0x2a770: {2 bCanDo(1B), 3 pPK ref} = 9B (wire-audit SIZE 9v8 @3.3.2).
	struct SInfo { bool bCanDo; CPtr<NWorld::CUnitServer> pPK; SInfo(): bCanDo( false ) {} int operator&( CStructureSaver &f ) { f.Add( 2, &bCanDo ); f.Add( 3, &pPK ); return 0; } };
private:
	OBJECT_BASIC_METHODS( CAIWearPKAction );
	ZDATA ZPARENT( CAIAction );
	SActionInfo<CAIWearPKAction, SInfo> info;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAIAction*)this ); f.Add( 3, &info ); return 0; }
public:
	CAIWearPKAction() {}
	CAIWearPKAction( IAIUnit *_pUnit ): CAIAction( _pUnit ), info( this ) {}
	void GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const;
	virtual void Do( CAILog *pLog ) const;
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const;
	virtual bool CanDo( const SPlaceWithAP &place ) { return info.CanDo( place ); }
	virtual void ResetInfoHash() { info.ResetInfoHash(); }
};
class CAILeavePKAction:        public CAIAction { AI_ACTION_DECL_MIN( CAILeavePKAction ) };
#undef AI_ACTION_DECL_MIN
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AIACTIONS_H_
