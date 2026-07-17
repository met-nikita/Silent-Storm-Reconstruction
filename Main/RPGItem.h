#ifndef __RPGITEM_H_
#define __RPGITEM_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "RPGItemInfo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CRPGClip;
	class CPanzerklein;
	class CRPGEngGrenade;   // fwd for CGrenadeItem::pDBEngGrenade (release save-format tag 4)
	class CRPGWeaponType;
}
namespace NRPG
{
class CUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Base interface for item transfer
class IJoinSplit: virtual public CObjectBase
{
public:
	virtual bool Join( IJoinSplit *pItem ) = 0;
	virtual IJoinSplit *Split( int nQuantityToGet = 1 ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IItemContainer: virtual public IItemContainerInfo
{
public:
	virtual IJoinSplit* SplitItem( int nQ ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// For storing items
class IInventory: public IInventoryInfo
{
public:
	virtual void Take( IInventoryItem *pItem ) = 0;
	virtual bool Place( const CTPoint<int> &sPos, IInventoryItem *pItem ) = 0;
	virtual void ArrangeItems() = 0;

	virtual bool Equip( NDb::ESlot where, IInventoryItem *pWhat ) = 0;
	virtual IInventoryItem *TakeOff( NDb::ESlot where ) = 0;
	virtual bool Activate( NDb::ESlot where ) = 0;

	// retail IInventory has NO SetHandItem: the PDB vtable runs Take(13), Place(14), ArrangeItems(15),
	// CanTakeOff(16), Equip(17), TakeOff(18), Activate(19), SetPanzerklein(20) -- corroborated by
	// CExecLoadWeapon::LoadClip @0x3949a0 dispatching Take at +0x34 (slot 13) and TakeOff at +0x48
	// (slot 18). Setting the hand goes through CUnitServer::SetHandItem @0x387b30 instead.
	virtual void SetPanzerklein( NDb::CPanzerklein *pPK, IInventory *pPKInventory ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IWeaponItem: virtual public IWeaponItemInfo
{
public:
	virtual bool HasAmmo() const = 0;
	virtual void CreateNewAttackPortion( vector<CAttackPortion> *pRes, bool bSpendAmmo ) = 0;
	virtual bool IsWorking() const = 0;

	virtual bool SetShootMode( NDb::EShootMode eShootMode ) = 0;
	virtual bool IsShootModeSupported( NDb::EShootMode eMode ) const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IGrenadeItem: virtual public IGrenadeItemInfo
{
public:
	virtual void SetMode( EGrenadeMode eMode ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventory *CreateInventory( CUnit *pOwner );
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CreateItem( CDBRecord *pItem );
IInventoryItem *CreateClueItem( NDb::CRPGItem *pDBItem );
IInventoryItem *CreateHintItem();	// retail @0x2a21a0: the fixed db item 0x1b6 wrapped in CSimpleItem<IHintItem>
IInventoryItem *CreateDummyItem( NDb::CRPGItem *pDBItem );	// retail @0x2a2220: inert wrapper for a record with no live pSuccessor
IWeaponItem *CreateWeaponItem( NDb::CRPGWeapon *pDBWeapon );
IInventoryItem *CreateMeleeWeaponItem( NDb::CRPGMeleeWeapon *pDBMeleeWeapon );
IInventoryItem *CreateClipItem( NDb::CRPGClip *pDBClip, NDb::CRPGAmmo *pDBAmmo = 0, int nAmmoQuantity = - 1 );
IInventoryItem *CreateGrenadeItem( NDb::CRPGGrenade *pDBGrenade );
IInventoryItem *CreateUniformItem( NDb::CRPGUniform *pDBUniform );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Field accessors over WHICHEVER record a grenade item carries -- the regular CRPGGrenade or (for
// an ENGINEER grenade, where GetDBGrenade() is null) the CRPGEngGrenade. Retail reads every such
// field through this which-record branch (to-hit calcer Prepare @0x2b88d0 / FillWeaponInfo
// @0x2b7ca0, throw exec @0x3a1700, item tooltip); both records expose the same
// pWeaponType/nQuality/nMaxDelay/pItem fields.
NDb::CRPGWeaponType *GetGrenadeRecWeaponType( IGrenadeItemInfo *pGrenade );
int GetGrenadeRecQuality( IGrenadeItemInfo *pGrenade );
int GetGrenadeRecMaxDelay( IGrenadeItemInfo *pGrenade );
NDb::CRPGItem *GetGrenadeRecItem( IGrenadeItemInfo *pGrenade );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif