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
	UCR_PK_BAN,                       // crouching, fight-capable unit may not look-and-move -- CanDo @0x3c1570
	                                  // (retail ordinal 0x13; here appended -> 16)
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
	CVec3 ptTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptTarget); return 0; }
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObject); return 0; }
	//
	CCmdCannon() {}
	CCmdCannon( IObject *_pObject ): pObject(_pObject) {}
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCorpse); return 0; }
	//
	CCmdTakeCorpse() {}
	CCmdTakeCorpse( CUnit *_pCorpse ): pCorpse(_pCorpse) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTakeCorpseOnDeploy: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdTakeCorpseOnDeploy);
public:
	ZDATA
	CPtr<CUnitServer> pCarrier;
	CPtr<CUnitServer> pCorpse;
	bool bDead;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCarrier); f.Add(3,&pCorpse); f.Add(4,&bDead); return 0; }
	//
	CCmdTakeCorpseOnDeploy() {}
	CCmdTakeCorpseOnDeploy( CUnitServer *_pCarrier, CUnitServer *_pCorpse, bool _bDead ): 
		pCarrier( _pCarrier ), pCorpse( _pCorpse ), bDead( _bDead ) {}
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
	CPtr<NRPG::IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); return 0; }
		
	CCmdReload( NRPG::IInventoryItem *_pItem = 0 ): pItem(_pItem) {}
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
	enum EPlacement
	{
		HAND,
		SLOT,
		GROUND,
		BACKPACK,
		STORAGE,
		UNIT_ANYPLACE,
		VACUUM			// move target only: the item is removed from the world (DestroyItemInHand)
	};

	ZDATA
	int nSlot;
	EPlacement eType;
	CTPoint<int> sPosition;
	CPtr<CUnit> pUnit;
	CPtr<IItem> pWorldItem;
	CPtr<IPlayer> pPlayer;
	CPtr<NRPG::IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSlot); f.Add(3,&eType); f.Add(4,&sPosition); f.Add(5,&pUnit); f.Add(6,&pWorldItem); f.Add(7,&pPlayer); f.Add(8,&pItem); return 0; }

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
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); return 0; }

public:
	CCmdCreateInventoryItem() {}
	CCmdCreateInventoryItem( NDb::CRPGItem *_pItem ): pItem( _pItem ) {}

	NDb::CRPGItem* GetItem() const { return pItem; }
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
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); f.Add(3,&nSlot); return 0; }

public:
	CCmdCreateAndActivateInventoryItem(): nSlot( 0 ) {}
	CCmdCreateAndActivateInventoryItem( NDb::CRPGItem *_pItem, int _nSlot ): pItem( _pItem ), nSlot( _nSlot ) {}

	NDb::CRPGItem* GetItem() const { return pItem; }
	int GetSlot() const { return nSlot; }
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
	SItem sSource;
	SItem sTarget;
	bool bActivate;
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSource); f.Add(3,&sTarget); f.Add(4,&bActivate); f.Add(5,&nSlot); return 0; }

public:
	CCmdExchangeInventoryItems(): bActivate( false ), nSlot( 0 ) {}
	CCmdExchangeInventoryItems( const SItem &_sSource, const SItem &_sTarget, bool _bActivate, int _nSlot ):
		sSource( _sSource ), sTarget( _sTarget ), bActivate( _bActivate ), nSlot( _nSlot ) {}

	const SItem& GetSource() const { return sSource; }
	const SItem& GetTarget() const { return sTarget; }
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
class CCmdNeedReload: public CCmd
{
	OBJECT_BASIC_METHODS(CCmdNeedReload);
};
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
	bool bCircled;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nDBAnimationID); f.Add(3,&bCircled); return 0; }
	//
	CCmdPlayAnimation() {}
	CCmdPlayAnimation( int _nDBAnimationID, bool _bCircled ): 
		nDBAnimationID( _nDBAnimationID ), bCircled( _bCircled ) {}
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
}
#endif
