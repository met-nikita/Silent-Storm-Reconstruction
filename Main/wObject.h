#ifndef __wObject_H_
#define __wObject_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "wOSBase.h"
#include "wMine.h"
#include "Locks.h"   // CLockable -- retail CCannon carries the usage-lock mixin (@+0xa0)
namespace NDb
{
	class CRPGGrenade;
	class CRPGEngGrenade;
}
namespace NRPG
{
	class IWeaponItem;
	class CAttackPortion;
	class CRPGArmor;
}
namespace NWorld
{
class CUnitServer;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectServer: public CObjectServerBase
{
	OBJECT_NOCOPY_METHODS(CObjectServer);
	ZDATA_(CObjectServerBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CObjectServerBase*)this); return 0; }
public:
	CObjectServer() {}
	// retail ctor @0x372090: (..., vCreateFlags, eTimeOfDay, bBorder)
	CObjectServer( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay, bool bBorder = false ) :
		CObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, vCreateFlags, eTimeOfDay, bBorder ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimObjectServer: public CAnimObjectServerBase
{
	OBJECT_NOCOPY_METHODS(CAnimObjectServer);
	ZDATA_(CAnimObjectServerBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); return 0; }
public:
	CAnimObjectServer() {}
	// retail ctor @0x372190: eTimeOfDay appended after vCreateFlags
	CAnimObjectServer( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay )
		: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags, eTimeOfDay ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindowDoor: public CAnimObjectServerBase, public IWindowDoor, public IMine
{
	OBJECT_NOCOPY_METHODS(CWindowDoor);
	struct SAttachedGrenade
	{
		ZDATA
		CDBPtr<NDb::CRPGGrenade> pGrenade;
		int nDC;
		CVec3 vPos;
		// retail SAttachedGrenade::operator& @0x383880 grew past the Jan03 form with tags 5/6/7: the placer's
		// explosive-perk damage modifiers (applied at door-trap detonation in GoBoom), plus the engineer-grenade
		// descriptor + the placer's eng skill for the eng-grenade door trap (populated by the eng SetTrap overload
		// @0x381f90, dispatched by GoBoom @0x381d60 via the eng AddGrenadeExplosion / world vtbl+0x120).
		SPerkMineModifiers sMineModifiers;
		CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;
		int nEngSkill;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGrenade); f.Add(3,&nDC); f.Add(4,&vPos); f.Add(5,&sMineModifiers); f.Add(6,&pEngGrenade); f.Add(7,&nEngSkill); return 0; }
		SAttachedGrenade() : nEngSkill( 0 ) {}   // sMineModifiers self-defaults {1,1,false}, pEngGrenade -> 0; old saves (tags 2-4) load clean
	};
	ZDATA_(CAnimObjectServerBase)
	bool bIsOpen;
	CPtr<NWorld::CUnitServer> pUser;
	SAttachedGrenade trap;
	bool bIsLocked;
	int nKeyID, nLockHardness;       // retail @220/@224: required-key id + lockpick difficulty (set when locking)
	bool bIsChest;                   // retail @228 (tag 8): chest-style container door (behavior port deferred)
	bool bIsTransparentIfOpen;       // retail @229 (tag 9): open door doesn't block vision (behavior port deferred)
	// retail @0x383790: {1 base, 2 bIsOpen, 3 pUser, 4 trap, 5 bIsLocked, 6 nKeyID, 7 nLockHardness,
	// 8 bIsChest, 9 bIsTransparentIfOpen}. The Jan03 wire put pAIHull@5, shifting the lock fields ->
	// loading a retail save read garbage into bIsLocked/nKeyID (18/21 doors on the base map came up
	// locked with unmatchable keys = "all doors inaccessible"). Retail has NO pAIHull member.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); f.Add(2,&bIsOpen); f.Add(3,&pUser); f.Add(4,&trap); f.Add(5,&bIsLocked); f.Add(6,&nKeyID); f.Add(7,&nLockHardness); f.Add(8,&bIsChest); f.Add(9,&bIsTransparentIfOpen); return 0; }
	// runtime hull cache, creation path only (NOT serialized; retail resolves hulls via
	// CUserHullsTracker -- dev equivalent is CAIMap::GetHull's src.pUserData scan)
	CPtr<CObjectBase> pAIHull;

public:
	// the save/load factory (OBJECT_NOCOPY_METHODS) constructs through this default ctor and operator& then fills
	// only the tags present in the save, so the new lock fields MUST default here too -- an old save (no tags 7/8)
	// would otherwise re-serialize indeterminate nKeyID/nLockHardness.
	CWindowDoor(): bIsLocked( false ), nKeyID( 0 ), nLockHardness( 0 ), bIsChest( false ), bIsTransparentIfOpen( false ) {}
	// retail ctor @0x3823f0: (..., vCreateFlags, eTimeOfDay, bOpen, bIsChest, bTransparentIfOpen) -- the two
	// trailing chest bools are a documented dev deferral (see CWorld::AddObject), eTimeOfDay ported per retail.
	CWindowDoor( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay, bool bOpen = false );

	void GoBoom( CUnitServer *pWho = 0 );
	// IMine
	virtual int GetMineDC();
	virtual CVec3 GetMinePos();
	virtual bool IsMineSet();
	virtual NDb::CRPGItem* DisarmMine();
	// IWindowDoor
	virtual bool IsBroken() const;
	virtual void OpenClose( bool bOpen, bool bAbruptly, CUnitServer *pWho = 0 );
	virtual bool IsOpen() const { return bIsOpen; }
	virtual CVec3 GetChangeStateDirection( bool bOpen ) const;
	virtual void LockDoor( bool bLock, int nKeyID, int nLockHardness );
	virtual bool IsLockedDoor() const { return bIsLocked; };
	int GetKeyID() const { return nKeyID; }                 // retail reads door+0xdc directly (same-module); accessor for the exec flow
	int GetLockHardness() const { return nLockHardness; }   // retail door+0xe0
	// IDynamicObject
	virtual bool Segment();
	virtual void Visit( IAIVisitor *p );
	//
	bool SetTrap( NDb::CRPGGrenade *pGrenade, int nDC, const SPerkMineModifiers *pMods = 0 );   // @0x381ee0 (pMods=0 for map-authored traps: no placer perks)
	bool SetTrap( NDb::CRPGEngGrenade *pEngGrenade, int nDC, const SPerkMineModifiers *pMods, int nEngSkill );   // @0x381f90: the engineer-grenade trap also records the placer's eng skill
	// 
	virtual int ProcessAttack( NWorld::IWorld *pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
		const CVec3 &vDir, NDb::CRPGArmor *pArmor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail PDB layout: CAnimObjectServerBase @0, ICannon @0x9c, CLockable @0xa0 (the usage lock a
// pending CCmdCannon reserves), pCurUnit @0xa8, pItem @0xac, fMinClearDistance @0xb0,
// ptCannonAttackOrig @0xb4. operator& @0x383970: 1=base, 2=pCurUnit, 3=pItem,
// 4=fMinClearDistance (4B), 5=ptCannonAttackOrig (12B), 6=CLockable sub-chunk.
class CCannon: public CAnimObjectServerBase, public ICannon, public CLockable
{
	OBJECT_NOCOPY_METHODS(CCannon);
	ZDATA_(CAnimObjectServerBase)
	CPtr<CUnit> pCurUnit;
	CObj<NRPG::IWeaponItem> pItem;
	float fMinClearDistance;      // retail +0xb0 (tag 4): from NDb::CGun::fMinClearDist (ctor @0x382610)
	CVec3 ptCannonAttackOrig;     // retail +0xb4 (tag 5): barrel muzzle offset, NDb::CGun AttackOriginX/Y/Z
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); f.Add(2,&pCurUnit); f.Add(3,&pItem); f.Add(4,&fMinClearDistance); f.Add(5,&ptCannonAttackOrig); f.Add(6,(CLockable*)this); return 0; }
public:
	CCannon(): fMinClearDistance(0), ptCannonAttackOrig(0,0,0) {}
	// retail ctor @0x382610: eTimeOfDay appended after vCreateFlags
	CCannon( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );

	NDb::CSkeleton* GetSkeleton() { return pSkeleton; }
	NAnimation::CSkeletonAnimator* GetSkeletonAnimator() { return pAnimator; }

	void SetCurrentUnit( CUnit *pUnit ) { pCurUnit = pUnit; }
	CVec3 GetPosition();
	float GetDirection();
	NRPG::IWeaponItem* GetItem() const { return pItem; }
	// retail CUnitServer::GetMinClearDistance @0x3bf6a0 reads cannon+0xb0 directly (same module);
	// CUnitServer::GetAttackOrigin @0x3bf8d0 adds cannon+0xb4..+0xbc to GetPosition().
	float GetMinClearDistance() const { return fMinClearDistance; }
	const CVec3& GetCannonAttackOrigin() const { return ptCannonAttackOrig; }

	// ICannon
	virtual bool IsBroken() const;
	virtual bool IsOccupied() const { return IsValid( pCurUnit ); }
	virtual CUnit* GetCurrentUnit() const { return pCurUnit; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x382b00 / @0x382930: both passage factories take the element's eTimeOfDay as the last argument
IPassageObject *CreatePassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID,
	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );
//
IPassageObject *CreateAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime,
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
