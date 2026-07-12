#ifndef __wObject_H_
#define __wObject_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "wOSBase.h"
#include "wMine.h"
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
	CObjectServer( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, const vector<int> &vCreateFlags, bool bBorder = false ) :
		CObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, vCreateFlags, bBorder ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimObjectServer: public CAnimObjectServerBase
{
	OBJECT_NOCOPY_METHODS(CAnimObjectServer);
	ZDATA_(CAnimObjectServerBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); return 0; }
public:
	CAnimObjectServer() {}
	CAnimObjectServer( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags )
		: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags ) {}
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
	CPtr<CObjectBase> pAIHull;
	bool bIsLocked;
	int nKeyID, nLockHardness;       // retail @220/@224: required-key id + lockpick difficulty (set when locking)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); f.Add(2,&bIsOpen); f.Add(3,&pUser); f.Add(4,&trap); f.Add(5,&pAIHull); f.Add(6,&bIsLocked); f.Add(7,&nKeyID); f.Add(8,&nLockHardness); return 0; }

public:
	// the save/load factory (OBJECT_NOCOPY_METHODS) constructs through this default ctor and operator& then fills
	// only the tags present in the save, so the new lock fields MUST default here too -- an old save (no tags 7/8)
	// would otherwise re-serialize indeterminate nKeyID/nLockHardness.
	CWindowDoor(): bIsLocked( false ), nKeyID( 0 ), nLockHardness( 0 ) {}
	CWindowDoor( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, bool bOpen = false );

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
	int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCannon: public CAnimObjectServerBase, public ICannon
{
	OBJECT_NOCOPY_METHODS(CCannon);
	ZDATA_(CAnimObjectServerBase)
	CPtr<CUnit> pCurUnit;
	CObj<NRPG::IWeaponItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimObjectServerBase*)this); f.Add(2,&pCurUnit); f.Add(3,&pItem); return 0; }
public:
	CCannon() {}
	CCannon( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags );

	NDb::CSkeleton* GetSkeleton() { return pSkeleton; }
	NAnimation::CSkeletonAnimator* GetSkeletonAnimator() { return pAnimator; }

	void SetCurrentUnit( CUnit *pUnit ) { pCurUnit = pUnit; }
	CVec3 GetPosition();
	float GetDirection();
	NRPG::IWeaponItem* GetItem() const { return pItem; }

	// ICannon
	virtual bool IsBroken() const;
	virtual bool IsOccupied() const { return IsValid( pCurUnit ); }
	virtual CUnit* GetCurrentUnit() const { return pCurUnit; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IPassageObject *CreatePassageObject( CWorld *pWorld, const SObjectPlace &pos,	
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, 
	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags );
//
IPassageObject *CreateAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime,
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
