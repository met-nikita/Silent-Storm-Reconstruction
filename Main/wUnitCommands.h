#ifndef __wUnitCommands_H_
#define __wUnitCommands_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiPosition.h"
#include "weActiveItem.h"
namespace NDb
{
	enum EShootMode;
}
namespace NRPG
{
	class IClipItem;
	class IWeaponItemInfo;
	class IInventoryItem;
	class IGrenadeItemInfo;
	enum EGrenadeMode;
}
namespace NWorld
{
class IItem;
class IObject;
class CUnit;
class CUnitServer;
class IPassageObject;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitCommandResult
{
	//// General
	UCR_OK,
	UCR_UNAVAILABLE,
	UCR_GENERAL_FAILURE,
	UCR_INVALID_COMMAND,
	//// Path
	UCR_NO_TARGET,
	UCR_NOT_ENOUGH_AP,
	UCR_PATH_NOT_FOUND,
	//// Condition
	UCR_NEED_RELOAD,
	UCR_NO_EQUIPMENT,
	UCR_WEAPON_JAMMED,
	UCR_CRITICALS_BAN,
	UCR_TARGET_OUT_OF_RANGE,
	//// Inventory
	UCR_INVENTORY_NO_PLACE,
	//// Skill / hero / passage  (retail PDB result codes; appended at END so existing ordinals are
	//// unchanged -- eResult IS raw-serialized via NGame::SActionInfo (mission save, DoDataVector) but an
	//// append is byte-safe because eResult is a per-frame-recomputed UI cache; NEVER reorder/insert existing)
	UCR_NEED_HIGHER_SKILL,            // clearing tool unusable / skill too low (disarm trap+mine CanDoIt)
	UCR_NOT_HERO,                     // can-talk-but-not-hero -- SILENT no-op (CExecTalk::CanDoIt)
	UCR_NOT_ALL_UNITS_NEAR_PASSAGE,   // unit not in the passage zone (CExecUsePassage::CanDoIt)
	UCR_PK_BAN,                       // crouched unit WEARING a Panzerklein may not look-around -- CanDo @0x3c1570
	                                  // (retail ordinal 0x15; here appended -> 16)
	UCR_CANT_HEAL,                    // heal target's CanHeal() failed -- CanDoFirstAid @0x3a2d40
	                                  // (retail ordinal 16; here appended -> 17)
	UCR_DOOR_LOCKED                   // locked door, no key and no charged picklock in hand --
	                                  // CExecOpenClose::CanDoIt @0x3bd290 (retail ordinal 17; here appended -> 18)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmd: public CObjectBase
{
public:
	virtual bool IsSkippable() const { return true; }
	// retail CCmd vtbl +0x18/+0x1c -- reserve / release the command's target-reservation lock
	// (CExecCannon::CanDoIt @0x3a2360 calls Unlock, probes ILockable::IsLocked, then Lock around the
	// entry gate). Base = no-op; CCmdCannon (@0x3b1d20/@0x60650), CCmdTakeCorpse (@0x3b1c90/@0x61260)
	// and CCmdMoveInventoryItem (@0x5f630/@0x5f9a0) override.
	virtual void Lock( CUnit *pBy ) {}
	virtual void Unlock() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdStartCombat: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdStartCombat);
public:
	ZDATA
	ZEND int operator&( CStructureSaver &f ) { return 0; }
	//
	CCmdStartCombat() {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdPath: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdPath);
public:
	ZDATA
	NAI::EFindPathParams eParams;
	NAI::SPosition ptDst;
	ENeedActiveItem needActiveItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bLeaveZone); f.Add(3,&eParams); f.Add(4,&ptDst); f.Add(5,&needActiveItem); f.Add(6,&bCanFindNotExact); return 0; }
	bool bLeaveZone = false;
	bool bCanFindNotExact = false;
	//
	CCmdPath() {}
	CCmdPath( const NAI::SPosition &_ptDst, 
		NAI::EFindPathParams _eParams = NAI::PF_DEFAULT, ENeedActiveItem _needActiveItem = ITEM_NO_MATTER ):
			ptDst(_ptDst), eParams( _eParams ), needActiveItem( _needActiveItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdLook: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdLook);
public:
	ZDATA
	NAI::SPosition ptDst;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptDst); return 0; }
	//
	CCmdLook() {}
	CCmdLook( const NAI::SPosition &_ptDst ):	ptDst(_ptDst) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdHeal: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdHeal);
public:
	ZDATA
	CPtr<CUnit> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); return 0; }
	//
	CCmdHeal() {}
	CCmdHeal( CUnit *_pTarget ):
		pTarget( _pTarget ) {}
	virtual bool IsSkippable() const { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdShootMode: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdShootMode);
public:
	ZDATA
	NDb::EShootMode eMode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eMode); return 0; }
	//
	CCmdShootMode() {}
	CCmdShootMode( NDb::EShootMode _eMode ): eMode( _eMode ) {}
	virtual bool IsSkippable() const { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdGrenadeMode: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdGrenadeMode);
public:
	ZDATA
	NRPG::EGrenadeMode eMode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eMode); return 0; }
	//
	CCmdGrenadeMode() {}
	CCmdGrenadeMode( NRPG::EGrenadeMode _eMode ): eMode( _eMode ) {}
	virtual bool IsSkippable() const { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdShootObject: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdShootObject);
public:
	ZDATA
	CPtr<CObjectBase> pTarget;
	NAI::EHitLocation eHL;
	int nExtraAttackAP;
		bool bOnlyPrepareToShoot = false;
	bool bCanBeReplacedByReload = false;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); f.Add(3,&eHL); f.Add(4,&nExtraAttackAP); f.Add(5,&bOnlyPrepareToShoot); f.Add(6,&bCanBeReplacedByReload); return 0; }
	//
	CCmdShootObject() {}
	CCmdShootObject( CObjectBase *_pTarget, int nExtraAP, NAI::EHitLocation _eHL = NAI::HL_ANY ):
		pTarget( _pTarget ), nExtraAttackAP(nExtraAP), eHL(_eHL) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdShootTile: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdShootTile);
public:
	ZDATA
	// retail @0x5eeb0: tag 2 = bCanBeReplacedByReload (the reload-replacement gate, same bool the
	// sibling CCmdShootObject carries at its tag 6), tag 3 = ptTarget (dev wrote ptTarget at tag 2)
	bool bCanBeReplacedByReload = false;
	CVec3 ptTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bCanBeReplacedByReload); f.Add(3,&ptTarget); return 0; }
	//
	CCmdShootTile() {}
	CCmdShootTile( const CVec3 &_ptTarget ):
		ptTarget( _ptTarget ) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdOpenClose: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdOpenClose);
public:
	ZDATA
	CPtr<IObject> pObject;
	bool bOpen;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObject); f.Add(3,&bOpen); return 0; }
	//
	CCmdOpenClose() {}
	CCmdOpenClose( IObject *_pObject, bool _bOpen ): pObject(_pObject), bOpen(_bOpen) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCannon: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdCannon);
public:
	ZDATA
	CPtr<IObject> pObject;
	CObj<CObjectBase> pLock;   // retail +0x10 (tag 3): the cannon-reservation token from ILockable::Lock
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObject); f.Add(3,&pLock); return 0; }   // retail @0x616d0
	//
	CCmdCannon() {}
	CCmdCannon( IObject *_pObject ): pObject(_pObject) {}
	//
	virtual void Lock( CUnit *pBy );          // retail @0x3b1d20: pLock = dynamic_cast<ILockable*>(pObject)->Lock(pBy)
	virtual void Unlock() { pLock = 0; }      // retail @0x60650: release the reservation token
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdExitPK: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdExitPK);
public:
	ZDATA
	ZEND int operator&( CStructureSaver &f ) { return 0; }
	//
	CCmdExitPK() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdExitCannon: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdExitCannon);
public:
	ZDATA
	CPtr<IObject> pCannon; // for internal use, set in UsingCannonState
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCannon); return 0; }
	//
	CCmdExitCannon() {}
	CCmdExitCannon( IObject *_pObject ): pCannon(_pObject) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdUsePassage: public CCmd
{
	OBJECT_BASIC_METHODS( CCmdUsePassage );
	ZDATA
public:
	CPtr<IPassageObject> pPassageObject;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPassageObject); return 0; }
	//
	CCmdUsePassage() {}
	CCmdUsePassage( IPassageObject *_pPassageObject ): pPassageObject( _pPassageObject ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTakeCorpse: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdTakeCorpse);
public:
	ZDATA
	CPtr<CUnit> pCorpse;
	CObj<CObjectBase> pLock;   // retail +0x10 (tag 3): the corpse-reservation token from ILockable::Lock
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCorpse); f.Add(3,&pLock); return 0; }   // retail @0x61350
	//
	CCmdTakeCorpse() {}
	CCmdTakeCorpse( CUnit *_pCorpse ): pCorpse(_pCorpse) {}
	//
	virtual void Lock( CUnit *pBy );          // retail @0x3b1c90: pLock = dynamic_cast<ILockable*>(pCorpse)->Lock(pBy)
	virtual void Unlock() { pLock = 0; }      // retail @0x61260: release the reservation token
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTakeCorpseOnDeploy: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdTakeCorpseOnDeploy);
public:
	ZDATA
	CPtr<CUnitServer> pCarrier;
	CPtr<CUnitServer> pCorpse;
	// retail @0x373510 serializes {2,3} only -- no bDead member (retail PDB size 20, ctor @0x370090 is 2-arg; W5)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCarrier); f.Add(3,&pCorpse); return 0; }
	//
	CCmdTakeCorpseOnDeploy() {}
	CCmdTakeCorpseOnDeploy( CUnitServer *_pCarrier, CUnitServer *_pCorpse ):
		pCarrier( _pCarrier ), pCorpse( _pCorpse ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdDropCorpse: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdDropCorpse);
public:
	ZDATA
	CPtr<CUnit> pCorpse;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCorpse); return 0; }
	//
	CCmdDropCorpse( CUnit *_pCorpse = 0 ): pCorpse( _pCorpse )  {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdReload: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdReload);
public:
	ZDATA
	// retail @0x3b1e30: slot to reload, -1 = active weapon (Jan03 carried an item ptr)
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSlot); return 0; }

	CCmdReload( int _nSlot = -1 ): nSlot(_nSlot) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdStrafe: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdStrafe);
public:
	ZDATA
	bool bState;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bState); return 0; }
		
	CCmdStrafe() {}
	CCmdStrafe( bool _bState ): bState( _bState ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdWishPose: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdWishPose);
public:
	ZDATA
	NAI::EPose pose;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pose); return 0; }
		
	CCmdWishPose() {}
	CCmdWishPose( NAI::EPose _pose ): pose(_pose) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdExplode: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdExplode);
public:
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTeleport: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdTeleport);
public:
	ZDATA
	NAI::SUnitPosition pos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pos); return 0; }
	//
	CCmdTeleport() {}
	CCmdTeleport( NAI::SUnitPosition &_pos ): pos(_pos) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Scripted fly-to-a-(fly-)position command (UnitFlyToWaypoint). Executed by CExecFly: a single
// CUnitAnimator::Fly move from the unit's current position to dst, then SetPosition(dst). dst is a
// 3D/fly position (NAI::MakeFlyPos). Retail NWorld::CCmdFly @0x3b2260 (id 0x71723160).
class CCmdFly: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdFly);
public:
	ZDATA
	NAI::SUnitPosition dst;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&dst); return 0; }
	//
	CCmdFly() {}
	CCmdFly( const NAI::SUnitPosition &_dst ): dst(_dst) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdSetGrenadeOnObject: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdSetGrenadeOnObject);
public:
	ZDATA
	CPtr<CObjectBase> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); return 0; }
	//
	CCmdSetGrenadeOnObject() {}
	CCmdSetGrenadeOnObject( CObjectBase *_pTarget ):
		pTarget( _pTarget ) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdUntrapObject: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdUntrapObject);
public:
	ZDATA
	CPtr<CObjectBase> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); return 0; }
	//
	CCmdUntrapObject() {}
	CCmdUntrapObject( CObjectBase *_pTarget ) : pTarget( _pTarget ) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdSetMineOnTile: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdSetMineOnTile);
public:
	ZDATA
	NAI::SPosition ptDst;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptDst); return 0; }
	//
	CCmdSetMineOnTile() {}
	CCmdSetMineOnTile( const NAI::SPosition &_ptDst ) : ptDst(_ptDst) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Inventory
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SItem
{
	// Enumerator ORDER matches the retail PDB (VACUUM=0 .. UNIT_ANYPLACE=6) -- eType is raw-serialized
	// (tag 3), so the numeric values are part of the save format. The dev tree had VACUUM appended last
	// (HAND=0..VACUUM=6), a byte-level divergence.
	enum EPlacement
	{
		VACUUM,			// retail 0 -- move target only: the item is removed from the world (DestroyItemInHand)
		HAND,			// 1
		SLOT,			// 2
		GROUND,			// 3
		BACKPACK,		// 4
		STORAGE,		// 5
		UNIT_ANYPLACE	// 6
	};

	ZDATA
	int nSlot;
	EPlacement eType;
	CTPoint<int> sPosition;
	CPtr<CUnit> pUnit;
	CPtr<IItem> pWorldItem;
	CPtr<IPlayer> pPlayer;
	CPtr<NRPG::IInventoryItem> pItem;
	CObj<CObjectBase> pLockItem;   // retail +0x20 (tag 9): the item-usage lock token from ILockable::Lock
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSlot); f.Add(3,&eType); f.Add(4,&sPosition); f.Add(5,&pUnit); f.Add(6,&pWorldItem); f.Add(7,&pPlayer); f.Add(8,&pItem); f.Add(9,&pLockItem); return 0; }   // retail @0x619c0

	// retail SItem::LockItem @0x3b1c30: acquire the item's usage lock for pBy (ILockable::Lock through
	// the IInventoryItem virtual base) and hold the token in pLockItem. Defined in wUnitCommands.cpp.
	void LockItem( CUnit *pBy );

	SItem() {}
	SItem( CUnit *_pUnit, EPlacement _eType, int _nSlot, NRPG::IInventoryItem* _pItem = 0 ): pUnit( _pUnit ), eType( _eType ), nSlot( _nSlot ), pItem( _pItem ) {}
	SItem( CUnit *_pUnit, EPlacement _eType, NRPG::IInventoryItem* _pItem = 0, IPlayer *_pPlayer = 0 ): pUnit( _pUnit ), eType( _eType ), pItem( _pItem ), pPlayer( _pPlayer ) {}
	SItem( CUnit *_pUnit, EPlacement _eType, const CTPoint<int> &_sPosition, NRPG::IInventoryItem* _pItem = 0 ): pUnit( _pUnit ), eType( _eType ), sPosition( _sPosition ), pItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdArrangeInventory: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdArrangeInventory);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCreateInventoryItem: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdCreateInventoryItem);
private:
	ZDATA
	CDBPtr<NDb::CRPGItem> pItem;
	// retail @0x20dbe0 tag 3: create a CLUE item (NRPG::CreateClueItem) instead of a plain one
	// (the lua UnitCreateItem 3rd "b[false]" param; the old dev comment misread it as bEquip)
	bool bClue = false;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); f.Add(3,&bClue); return 0; }

public:
	CCmdCreateInventoryItem() {}
	CCmdCreateInventoryItem( NDb::CRPGItem *_pItem, bool _bClue = false ): pItem( _pItem ), bClue( _bClue ) {}

	NDb::CRPGItem* GetItem() const { return pItem; }
	bool GetClue() const { return bClue; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdMoveInventoryItem: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdMoveInventoryItem);
private:
	ZDATA
	SItem sSource;
	SItem sTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSource); f.Add(3,&sTarget); return 0; }

public:
	CCmdMoveInventoryItem() {}
	CCmdMoveInventoryItem( const SItem &_sSource, const SItem &_sTarget ): sSource( _sSource ), sTarget( _sTarget ) {}

	const SItem& GetSource() { return sSource; }
	const SItem& GetTarget() { return sTarget; }
	//
	virtual void Lock( CUnit *pBy ) { sSource.LockItem( pBy ); }   // retail @0x5f630: lock the moved item
	virtual void Unlock() { sSource.pLockItem = 0; }               // retail @0x5f9a0: drop the token
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdLoadWeapon: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdLoadWeapon);
private:
	ZDATA
	SItem sClip;
	CPtr<NRPG::IWeaponItemInfo> pWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sClip); f.Add(3,&pWeapon); return 0; }

public:
	CCmdLoadWeapon() {}
	CCmdLoadWeapon( NRPG::IWeaponItemInfo *_pWeapon, const SItem &_sClip ): pWeapon( _pWeapon ), sClip( _sClip ) {}

	const SItem& GetClip() { return sClip; }
	NRPG::IWeaponItemInfo* GetWeapon() { return pWeapon; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdUnloadWeapon: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdUnloadWeapon);
private:
	ZDATA
	CPtr<NRPG::IWeaponItemInfo> pWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWeapon); return 0; }

public:
	CCmdUnloadWeapon() {}
	CCmdUnloadWeapon( NRPG::IWeaponItemInfo *_pWeapon ): pWeapon( _pWeapon ) {}

	NRPG::IWeaponItemInfo* GetWeapon() { return pWeapon; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdSetActiveItem: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdSetActiveItem);
public:
	ZDATA
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSlot); return 0; }
	//
	CCmdSetActiveItem() {}
	CCmdSetActiveItem( int _nSlot ): nSlot(_nSlot) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Create an item from a DB record AND slot it into a hand (lua CreateAndActivateItem). The analogue of
// CCmdCreateInventoryItem (which stows into the backpack) but targeting a hand slot, with activation.
class CCmdCreateAndActivateInventoryItem: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdCreateAndActivateInventoryItem);
private:
	ZDATA
	CDBPtr<NDb::CRPGItem> pItem;
	// retail @0x2fde50: 3=bNeedMove, 4=moveSource, 5=moveTarget, 6=nSlot (dev had nSlot at tag 3).
	// bNeedMove queues a stash move (moveSource -> moveTarget, usually active hand -> backpack)
	// AHEAD of the create+activate (retail CreateExecutor @0x3b37b0 prepends a CExecMoveInventoryItem).
	bool bNeedMove = false;
	SItem moveSource;
	SItem moveTarget;
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); f.Add(3,&bNeedMove); f.Add(4,&moveSource); f.Add(5,&moveTarget); f.Add(6,&nSlot); return 0; }

public:
	CCmdCreateAndActivateInventoryItem(): nSlot( 0 ) {}
	// retail value ctor @0x2fda10: (pItem, nSlot, bNeedMove, moveSource, moveTarget)
	CCmdCreateAndActivateInventoryItem( NDb::CRPGItem *_pItem, int _nSlot, bool _bNeedMove = false,
		const SItem &_moveSource = SItem(), const SItem &_moveTarget = SItem() ):
		pItem( _pItem ), nSlot( _nSlot ), bNeedMove( _bNeedMove ), moveSource( _moveSource ), moveTarget( _moveTarget ) {}

	NDb::CRPGItem* GetItem() const { return pItem; }
	int GetSlot() const { return nSlot; }
	bool GetNeedMove() const { return bNeedMove; }
	const SItem& GetMoveSource() const { return moveSource; }
	const SItem& GetMoveTarget() const { return moveTarget; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Exchange an inventory item into a hand slot, displacing whatever is there (lua UnitDrawWeapon /
// UnitSwitchToGrenade). sSource = the item to bring into the hand; sTarget carries the displaced
// (currently in-hand) item; nSlot = the target hand slot; bActivate makes that slot the active hand.
class CCmdExchangeInventoryItems: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdExchangeInventoryItems);
private:
	ZDATA
	SItem moveSource; // retail PDB names (moveSource@12/moveTarget@48); wire @0x2fdec0 = {2,3,4,5}
	SItem moveTarget;
	bool bActivate;
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&moveSource); f.Add(3,&moveTarget); f.Add(4,&bActivate); f.Add(5,&nSlot); return 0; }

public:
	CCmdExchangeInventoryItems(): bActivate( false ), nSlot( 0 ) {}
	CCmdExchangeInventoryItems( const SItem &_sSource, const SItem &_sTarget, bool _bActivate, int _nSlot ):
		moveSource( _sSource ), moveTarget( _sTarget ), bActivate( _bActivate ), nSlot( _nSlot ) {}

	const SItem& GetSource() const { return moveSource; }
	const SItem& GetTarget() const { return moveTarget; }
	bool GetActivate() const { return bActivate; }
	int GetSlot() const { return nSlot; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECollectSnipeAP
{
	CSAP_1AP,
	CSAP_10AP,
	CSAP_MAX,
	CSAP_ALL
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCollectSnipeAP: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdCollectSnipeAP);
public:
	ZDATA
	ECollectSnipeAP eAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eAP); return 0; }
	//
	CCmdCollectSnipeAP() {}
	CCmdCollectSnipeAP( ECollectSnipeAP _eAP ): eAP( _eAP ) {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdSnipeAttack: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdSnipeAttack);
public:
	ZDATA
	ZEND int operator&( CStructureSaver &f ) { return 0; }
	//
	CCmdSnipeAttack() {}
	virtual bool IsSkippable() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdEmpty: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdEmpty);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdContinue: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdContinue);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (dev CCmdNeedReload 0x52062170 REMOVED -- W5 serialization-convergence: the pair does not exist
// in retail in ANY form and had no creation site here either. Retail's "out of ammo" signal is the
// already-live ack path: CStateAttack::OnLButtonUp @0x1dbec0 -> SayAckForAll(IA_WEAPON_EMPTY) ->
// CWorld::PlayAck @0x361e50 -> CGlobalAck::OnLastPieceOfAmmo @0x338a20 -> CAckLastPieceOfAmmo.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdWeaponJammed: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdWeaponJammed);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdOrderConfirmation: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdOrderConfirmation);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdImpossibleToPerformAction: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdImpossibleToPerformAction);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdPlayAnimation: public CCmd
{
	OBJECT_BASIC_METHODS( CCmdPlayAnimation );
	ZDATA
public:
	int nDBAnimationID;
	bool bFreezeAfterLastFrame; // retail @0x2fd7b0: tag 4 (3 is a hole); Jan03 bCircled@3 was the pre-rename form
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nDBAnimationID); f.Add(4,&bFreezeAfterLastFrame); return 0; }
	//
	CCmdPlayAnimation() {}
	CCmdPlayAnimation( int _nDBAnimationID, bool _bFreezeAfterLastFrame ):
		nDBAnimationID( _nDBAnimationID ), bFreezeAfterLastFrame( _bFreezeAfterLastFrame ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdHide: public CCmd
{
	OBJECT_BASIC_METHODS( CCmdHide );
	ZDATA
	ZPARENT( CCmd )
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCmd *)this); return 0; }
	//
public:
	CCmdHide() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTakePerk: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdTakePerk);
	ZDATA_(CCmd)
	int nID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmd*)this); f.Add(2,&nID); return 0; }
	//
public:
	CCmdTakePerk() {}
	CCmdTakePerk( int _nID ): nID( _nID ) {}

	int GetID() { return nID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTalk: public CCmd
{
	OBJECT_BASIC_METHODS( CCmdTalk );
	ZDATA
	ZPARENT( CCmd )
public:
	CPtr<CUnit> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCmd *)this); f.Add(3,&pTarget); return 0; }
	//
	CCmdTalk() {}
	CCmdTalk( CUnit *_pTarget ): pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CCmdNotHeroWantsToTalk (id 0x50133140) -- payload-less command issued when a non-hero
// unit is ordered to talk to an NPC; its executor (CExecNotHeroWantsToTalk) fires the NPC-interaction
// voice ack. Bare CCmd, base-chain serialization only.
class CCmdNotHeroWantsToTalk: public CCmd
{
	OBJECT_BASIC_METHODS( CCmdNotHeroWantsToTalk );
	ZDATA
	ZPARENT( CCmd )
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCmd *)this); return 0; }
	//
public:
	CCmdNotHeroWantsToTalk() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
