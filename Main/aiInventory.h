#ifndef __AIINVENTORY_H_
#define __AIINVENTORY_H_
//
namespace NDB
{
	enum EShootMode;
}
//
namespace NRPG
{
	class IUnitMission;
	class CAttackPortion;
	class IInventoryItem;
	class CGrenadeItem;
}
//
namespace NWorld { class CDFrozenItem; }
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class CAIFireArmsWeaponBase;
class CAIFireArmsWeapon;
class CAIFireArmsWeaponClip;
class CAIRocketLauncherWeapon;
class CAIGrenadeWeapon;
class CAIMeleeWeapon;
class CAIThrowingWeapon;
class CAIFirstAid;
class IAIInventoryItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory -- the AI-side mirror of a unit's RPG inventory. Retail data model (PDB layout,
// 32 bytes): { CPtr<IAIUnit> pOwner @0xc; vector< CObj<IAIInventoryItem> > items @0x10;
// CObj<IAIInventoryItem> pCurrent @0x1c } -- ONE unified item vector; every consumer filters it by
// item type with a dynamic_cast probe (GetItems<T> @0x57a00.., GetItemsCount<T>,
// GetBestWeaponSimple<T> @0x57b00..). Wire (operator& @0x58820, byte-walked in every retail
// mission save): {2: pOwner, 3: items vector, 4: pCurrent}. The former dev model scattered the
// items across SIX typed vectors serialized at tags 3..9, so loading a retail save stuffed the
// whole unified tag-3 list into `fireArms` (type confusion for every non-firearm item), read the
// pCurrent pointer chunk (tag 4) as the grenade vector, and left tags 5-9 forever absent (pCurrent
// and the melee/throwing/first-aid state silently defaulted).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIInventory: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAIInventory);
	ZDATA
	CPtr<IAIUnit> pOwner;
	vector< CObj<IAIInventoryItem> > items;   // unified item list (retail @0x10)
	CObj<IAIInventoryItem> pCurrent;          // the simulated "in hand" item (retail @0x1c)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pOwner); f.Add(3,&items); f.Add(4,&pCurrent); return 0; }  // retail @0x58820
	//
	void GetInventoryItems( list< CPtr<NRPG::IInventoryItem> > *pItems ) const;   // @0x55b00
	void FetchInventoryItems();                                                   // @0x55f90
	IAIInventoryItem* CreateWeapon( NRPG::IInventoryItem *pItem );                // @0x554f0
	CAIFireArmsWeaponBase* GetSuitableWeapon( CAIFireArmsWeaponClip *pClip ) const; // @0x55790
	IAIInventoryItem* GetAIInventoryItem( NRPG::IInventoryItem *pItem ) const;    // @0x556f0
	//
public:
	CAIInventory() {}
	CAIInventory( IAIUnit *_pOwner );   // @0x56640
	//
	// the retail item mutators: a clip routes into the suitable weapon's spares, anything else
	// gets an AI wrapper appended to `items`.
	void AddItem( NRPG::IInventoryItem *pItem );      // @0x55e20
	void RemoveItem( NRPG::IInventoryItem *pItem );   // @0x55a70 (also empties the hand if it held it)
	void RemoveItem( IAIInventoryItem *pItem );       // @0x55db0 (does NOT touch pCurrent)
	// a5dll-only (no retail counterpart): the planner's RollBack inverse of RemoveItem(IAIInventoryItem*),
	// used by the CAILog* speculative-commit records to re-add an existing wrapper.
	void RestoreItem( IAIInventoryItem *pItem );
	//
	void SetCurrentItem( IAIInventoryItem *pItem );   // @0x55750
	IAIInventoryItem* GetCurrentItem() const;
	CAIFireArmsWeapon* GetCurrentFireArms() const;    // a5dll helper (aiUnit.cpp); pCurrent-only, no vector reads
	// @0x56300 == GetBestWeaponSimple<CAIFireArmsWeapon>: the CURRENT item when it is a live
	// firearm, else the first firearm in inventory order. Rocket launchers never match (retail
	// leaf is a sibling; here the probe rejects IsRocketLauncher()).
	CAIFireArmsWeapon* GetFirstFireArms() const;
	bool IsCurrentItem( IAIInventoryItem *pItem ) const;   // @0x55620
	//
	CAIFireArmsWeapon* GetBestFireArms( const NAI::SUnitPosition &pos, IAIUnit *pTarget,
		int nAP, int *pBestWeaponHitCover, int *pQuality,	NDb::EShootMode *shootMode, int *nMaxToHit ) const;  // @0x560e0
	CAIGrenadeWeapon* GetBestGrenade( CVec3 ptTarget ) const;      // @0x567b0 (ptTarget unused in retail too)
	CAIFireArmsWeapon* GetBestRocketLaunchers() const;             // @0x567c0 = GetBestWeaponSimple<CAIRocketLauncherWeapon>
	CAIMeleeWeapon* GetBestMeleeWeapon() const;                    // @0x563b0 (avg-damage scoring @0x55960)
	CAIThrowingWeapon* GetBestThrowingWeapon() const;              // @0x56310 (avg-damage scoring @0x55850)
	CAIFirstAid* GetBestFirstAid() const;                          // @0x567d0 = GetBestWeaponSimple<CAIFirstAid>
	// @0x56450: the FIRST firearm-base (rocket launchers included) whose current clip is not at
	// RPG capacity and that has a live spare at the front of its spares -- pure inventory order,
	// no current-item preference (used by CAIReloadAction). Retail types the result as the base;
	// kept as CAIFireArmsWeapon* for the unowned aiActions.cpp caller (every dev instance is one).
	CAIFireArmsWeapon* GetBestWeaponForReload() const;
	// @0x56540: the first firearm that supports the snipe shoot-mode and has a non-empty current
	// clip (used by CAIBeginSnipeAction).
	CAIFireArmsWeapon* GetBestWeaponForSnipe() const;
	// the loot scorers (@0x56810 / @0x55680, used by CAILootAction). IsItemNecessary: is a ground
	// item worth picking up given what I carry (and, if so, *ppToDrop = the held item to drop to
	// make room). GetMostNecessaryItem: of a wanted list, the highest-priority by item type.
	bool IsItemNecessary( NWorld::CDFrozenItem *pFrozen, IAIInventoryItem **ppToDrop ) const;
	NWorld::CDFrozenItem* GetMostNecessaryItem( const list< CPtr<NWorld::CDFrozenItem> > &items ) const;
	bool HasAnyWeapon() const;          // @0x56fd0: any firearm/rocket launcher, grenade, throwing or melee weapon
	bool HasAnyNotMeleeWeapon() const;  // @0x567e0: same minus the melee probe
	//
	void DebugOutput() const;           // a5dll-only debug helper
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIInventory* CreateAIInventory( IAIUnit *pOwner );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif
