#ifndef __RPGITEMINFO_H_
#define __RPGITEMINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Locks.h"   // ILockable -- retail IInventoryItem carries it as a virtual base

namespace NDb
{
	enum ESlot;
	enum ESkillType;
	enum EItemSubType;
	enum EShootMode;
	enum EWeaponType;
	class CRPGItem;
	class CRPGAmmo;
	class CRPGClip;
	class CRPGGrenade;
	class CRPGUniform;
	class CRPGWeapon;
	class CRPGFirstAid;
	class CRPGMeleeWeapon;
	class CRPGMineDetector;
	class CRPGMine;
	class CRPGTool;
	class CRPGKey;
	class CRPGPicklock;
	class CRPGEngGrenade;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
class CUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Base item interface (weight/quantity)
class IItem: virtual public CObjectBase
{
public:
	virtual int GetWeight() const = 0;
	virtual int GetQuantity() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail PDB: IInventoryItem has TWO virtual bases -- IItem AND ILockable (every inventory item is
// lockable; the implementation lives in CInventoryItem's CLockable base). SItem::LockItem @0x3b1c30
// calls ILockable::Lock statically through this interface.
class IInventoryItem: virtual public IItem, virtual public ILockable
{
public:
	virtual NDb::CRPGItem *GetDBItem() const = 0;
	virtual const CTPoint<int>& GetSize() const = 0;

	virtual NDb::EWeaponType GetWeaponType() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::IClueItem / NRPG::IHintItem -- empty marker interfaces (PDB: both are
// `: virtual public IInventoryItem`, size 36 = just the virtual-base machinery, no members).
// CMissionUI::UpdateVisibleItems @0x2130c0 RTTI-classifies every discovered ground item's inventory
// item against them (RTDynamicCast pair @0x6137b7: IHintItem then IClueItem): an IClueItem gets a
// CClueIcon marker (UNGATED), otherwise an IHintItem gets a CHintIcon marker gated by the
// "ui_showhints" option -- a clue item always wins when both would match.
class IClueItem: virtual public IInventoryItem
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IHintItem: virtual public IInventoryItem
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::IDummyItem (ctor @0x2a9a40) -- same empty marker shape: tags the inert wrapper
// CreateDummyItem @0x2a2220 builds; CInventory::CanEquip @0x29d660 rejects it by this cast.
class IDummyItem: virtual public IInventoryItem
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IItemContainerInfo: virtual public IInventoryItem
{
public:
	virtual int GetIncQuantity() const = 0;
	virtual int GetMaxIncQuantity() const = 0;
	// retail IItemContainerInfo slot 2. Proved by vftable slot-walk (ICF hides the per-class symbol):
	// ??_7CToolItem@NRPG@@6BIItemContainerInfo@1@@ @rva 0x4c3090 +0x008 -> 0x2a6950, an adjustor thunk
	// (`sub ecx,[ecx-4]; sub ecx,0xc; jmp 0x6a4780`) into the ONE shared body @0x2a4780, which the PDB
	// names NRPG::CItemContainer<NRPG::CAmmoItem>::IsEmpty and mangles `...::IsEmpty(void)const`.
	// CPicklockItem/CFirstAidItem slot 2 reach the same body through their own thunks -- ONE body for
	// every container, hence it lives on CItemContainer<T>, not per-class. Body @0x2a4780 is exactly
	// `return GetIncQuantity() < 1;` (re-dispatched through the same vbtable[5] the caller used).
	virtual bool IsEmpty() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBackPackItem
{
	ZDATA
	CTPoint<int> sPos;
	CObj<IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sPos); f.Add(3,&pItem); return 0; }

	SBackPackItem() {}
	SBackPackItem( const CTPoint<int> &_sPos, IInventoryItem* _pItem ): sPos( _sPos ), pItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Information about inventory
class IInventoryInfo: public CObjectBase
{
public:
	virtual bool CanPlace( const CTPoint<int> &sPos, const IInventoryItem *pItem, bool bCheckSpace = true ) const = 0;
	virtual bool FindPlace( const IInventoryItem *pItem, CTPoint<int> *pPos ) const = 0;
	virtual const vector<SBackPackItem>& GetItems() const = 0;

	virtual bool CanEquip( NDb::ESlot where, const IInventoryItem *pWhat ) const = 0;
	virtual IInventoryItem *Get( NDb::ESlot where ) const = 0;
	virtual int GetActiveSlot() const = 0;
	virtual IInventoryItem *GetActive() const = 0;

	// retail IInventoryInfo has NO GetHandItem: the PDB vtable is exactly CanPlace(4), FindPlace(5),
	// GetItems(6), CanEquip(7), Get(8), GetActiveSlot(9), GetActive(10), GetPlaceBySubType(11),
	// GetUniform(12) -- corroborated by GetCluesFromPers @0x302a20 dispatching Get at +0x20 (slot 8)
	// and GetItems at +0x18 (slot 6). The hand item lives on CUnitServer/CPlayer, not the inventory.
	virtual int GetPlaceBySubType( NDb::EItemSubType subType ) const = 0;
	virtual NDb::CRPGUniform* GetUniform() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAmmoItem: virtual public IItem
{
public:
	virtual NDb::CRPGAmmo *GetDBAmmo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IClipItem: virtual public IItemContainerInfo
{
public:
	virtual NDb::CRPGAmmo* GetDBAmmo() const = 0;
	virtual NDb::CRPGClip* GetDBClip() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ������
class CClipItem;
class CAttackPortion;
struct SWeaponInfo
{
	int nRoF;
	int nQuality;
	int nShotAP, nTargetingAP;
	int nRecoil;
	int nMinRange, nMaxRange;
	int nDmgMin, nDmgMax;
	int nArmorPiercingAbility;
	float fScopeFactor;
};
class IWeaponItemInfo: virtual public IInventoryItem
{
public:
	virtual bool CanLoad( IClipItem *pClip ) const = 0;
	virtual bool CanReload( IInventoryInfo *pInventory ) const = 0;

	virtual IClipItem *GetInnerClip() const  = 0;
	virtual int GetAmmoQuantity() const = 0;
	virtual int GetShootAP() const = 0;
	virtual void GetInfo( SWeaponInfo *pInfo ) const = 0;
	virtual NDb::ESkillType GetSkillIndex() const = 0;
	virtual NDb::EShootMode GetShootMode() const = 0;
	virtual bool IsShootModeSupported( NDb::EShootMode eMode ) const = 0;

	virtual NDb::CRPGWeapon* GetDBWeapon() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EGrenadeMode
{
	GM_THROW,
	GM_SETTRAP
};
////
class IGrenadeItemInfo: virtual public IInventoryItem
{
public:
	virtual EGrenadeMode GetMode() const = 0;
	virtual NDb::CRPGGrenade *GetDBGrenade() const = 0;
	// An ENGINEER grenade leaves GetDBGrenade() null and carries its record here instead
	// (CGrenadeItem::GetDBEngGrenade, retail @0x2a3c70) -- consumers must check both.
	virtual NDb::CRPGEngGrenade *GetDBEngGrenade() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail IFirstAidItem (PDB: size 0x30, `: virtual public IItemContainerInfo` -- the identical shape to
// IToolItem/IPicklockItem, NOT the `virtual public IInventoryItem` this used to carry). A medkit IS a
// CItemContainer<CSimpleCharge>, so its remaining doses come through IItemContainerInfo.
// ‼️ It declares exactly ONE own virtual: ??_7CFirstAidItem@NRPG@@6BIFirstAidItem@1@@ @rva 0x4c2ff8 is
// {+0x000 GetDBFirstAid, +0x004 ??_R4...IItemContainer...} -- slot 1 is already the next base's RTTI
// marker, so there is NO second own slot. The `virtual bool IsEmpty() = 0;` that used to sit here was a
// dev-only invention; IsEmpty is IItemContainerInfo slot 2 (see above), reached through the vbase.
class IFirstAidItem: virtual public IItemContainerInfo
{
public:
	virtual NDb::CRPGFirstAid* GetDBFirstAid() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMeleeWeaponItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMeleeWeapon* GetDBMeleeWeapon() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMineDetectorItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMineDetector* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMineItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMine* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail IToolItem (PDB: size 0x30, `: virtual public IItemContainerInfo` -- the identical shape to
// IPicklockItem/IFirstAidItem): a tool IS a CItemContainer<CSimpleCharge>, so its remaining charges
// come through IItemContainerInfo (GetIncQuantity), not an own member. CToolItem::CanBeUsed
// @0x2a0be0 gates on exactly that vbase accessor (vbtable-dispatched GetIncQuantity() < 1).
class IToolItem: virtual public IItemContainerInfo
{
public:
	virtual NDb::CRPGTool* GetDBItemInfo() const = 0;
	virtual bool CanBeUsed( NRPG::CUnit *pUnit ) const = 0;
	virtual int GetSkillModifForMineCleaning() const = 0;
	// retail IToolItem slot 3 -- the slot whose absence made tools infinite. Proved by slot-walk:
	// ??_7CToolItem@NRPG@@6BIToolItem@1@@ @rva 0x4c30d8 is
	//   {+0x000 GetDBItemInfo (ICF-folded onto NAI::CAIJob::GetParentJob -- both are `return member;`),
	//    +0x004 CToolItem::CanBeUsed @0x2a0be0, +0x008 CToolItem::GetSkillModifForMineCleaning @0x2a0d70,
	//    +0x00c CToolItem::SpendOneCharge @0x2a6f90}.
	// The two disarm executors call it as `call [vptr+0xc]` (@0x7a880b / @0x7a856b). Mangled
	// `...::SpendOneCharge(void)` -- void, non-const. Unique to IToolItem: IPicklockItem @0x4c3068 and
	// IFirstAidItem @0x4c2ff8 each have only their one GetDB* slot and spend by calling the concrete
	// CItemContainer<CSimpleCharge>::SpendCharge @0x2a6fa0 directly instead.
	virtual void SpendOneCharge() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IKeyItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGKey* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail IPicklockItem (ctor @0x2a75a0): the lockpick's interface base -- charge counting comes
// through IItemContainerInfo (a picklock IS a CItemContainer<CSimpleCharge>).
class IPicklockItem: virtual public IItemContainerInfo
{
public:
	virtual NDb::CRPGPicklock* GetDBPicklock() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif