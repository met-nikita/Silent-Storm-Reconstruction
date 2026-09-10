#include "stdafx.h"
//
#include "RPGUnitMission.h"
#include "RPGItemSet.h"
#include "RPGItem.h"
#include "RPGAttackMech.h"
#include "RPGUnit.h"          // NRPG::CUnit::pDefaultWeapon (FetchInventoryItems @0x55f90)
//
#include "..\DBFormat\DataRPG.h"
//
#include "aiUnit.h"
#include "aiWeapon.h"
#include "aiPosition.h"
#include "aiInventory.h"
//
#include "wUnitServer.h"
#include "wDebris.h"          // NWorld::CDFrozenItem (loot scorers)
#include "..\MiscDll\LogStream.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// the per-type probe of the unified items vector -- retail's dynamic_cast<T> inside GetItems<T>
// @0x57a00../GetItemsCount<T>/GetBestWeaponSimple<T> @0x57b00...
template< class T >
static T* CastInventoryItem( IAIInventoryItem *pItem )
{
	return CDynamicCast<T>( pItem );
}
// Retail's dynamic_cast<CAIFireArmsWeapon> can never yield a rocket launcher (the retail
// CAIRocketLauncherWeapon leaf is a SIBLING of CAIFireArmsWeapon under CAIFireArmsWeaponBase).
// The dev leaf derives from CAIFireArmsWeapon instead (see aiWeapon.h), so the firearms probe
// rejects rocket launchers by the same DB flag (bBazookaLogic) the leaf is created from.
template<>
CAIFireArmsWeapon* CastInventoryItem<CAIFireArmsWeapon>( IAIInventoryItem *pItem )
{
	CAIFireArmsWeapon *pWeapon = CDynamicCast<CAIFireArmsWeapon>( pItem );
	if ( pWeapon && pWeapon->IsRocketLauncher() )
		return 0;
	return pWeapon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::GetItems<T> @0x57a00 (FireArms) / @0x57bf0 (Throwing) / @0x57cf0 (Melee) /
// @0x57df0 (FireArmsBase) / @0x580a0 (Grenade) / @0x581a0 (RocketLauncher) / @0x582a0 (FirstAid):
// walk the unified vector in order, dynamic_cast each item, collect every success.
template< class T >
static void GetItemsOfType( const vector< CObj<IAIInventoryItem> > &items, vector< CObj<T> > *pRes )
{
	pRes->clear();
	for ( vector< CObj<IAIInventoryItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
	{
		if ( T *p = CastInventoryItem<T>( i->GetPtr() ) )
			pRes->push_back( p );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::GetItemsCount<T> @0x57ef0/@0x57f80/@0x58010/@0x58670/@0x58700/@0x58790 (retail
// materializes a GetItems<T> temp and takes its size; the direct count is result-identical).
template< class T >
static int GetItemsCountOfType( const vector< CObj<IAIInventoryItem> > &items )
{
	int nCount = 0;
	for ( vector< CObj<IAIInventoryItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
	{
		if ( CastInventoryItem<T>( i->GetPtr() ) )
			++nCount;
	}
	return nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::GetBestWeaponSimple<T> @0x57b00 (FireArms) / @0x58490 (RocketLauncher) / @0x583a0
// (Grenade) / @0x58580 (FirstAid): prefer the currently-held item when it is a LIVE T; otherwise
// the FIRST item of type T in inventory order (no liveness re-check on the fallback). When the
// current item is a dead T and no other T is held, the (dead) current is returned.
template< class T >
static T* GetBestWeaponSimpleT( const vector< CObj<IAIInventoryItem> > &items, IAIInventoryItem *pCurrent )
{
	T *pRes = CastInventoryItem<T>( pCurrent );
	if ( pRes == 0 || !IsValid( pRes ) )
	{
		for ( vector< CObj<IAIInventoryItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
		{
			if ( T *p = CastInventoryItem<T>( i->GetPtr() ) )
				return p;
		}
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetBestMeleeWeapon<T> @0x55850 (T = CAIThrowingWeapon) / @0x55960 (T = CAIMeleeWeapon): the
// strictly-highest average DB damage wins, the earlier index keeps ties; a missing/dead DB record
// contributes no score. Retail computes (int)((double)(nDmgMin + nDmgMax) * 0.5) with the FPU
// control word forced to round-toward-zero -- identical to C++ integer division by 2. There is NO
// current-item preference (the pre-convergence dev version preferred the equipped weapon; that
// never matched the decode).
template< class T >
static T* GetBestMeleeWeaponT( const vector< CObj<T> > &weapons )
{
	T *pBest = 0;
	int nBestScore = -1;
	for ( vector< CObj<T> >::const_iterator i = weapons.begin(); i != weapons.end(); ++i )
	{
		NDb::CRPGMeleeWeapon *pDB = (*i)->GetItem()->GetDBMeleeWeapon();
		if ( !IsValid( pDB ) )
			continue;
		int nScore = ( pDB->nDmgMin + pDB->nDmgMax ) / 2;
		if ( nBestScore < nScore )
		{
			pBest = i->GetPtr();
			nBestScore = nScore;
		}
	}
	return pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsFG42 @0x55630: is this RPG item the FG42 paratrooper rifle -- a CWeaponItem whose DB
// record id ([desc+0xc] == CDBRecord::nID) is 21, the FG42 row of the RPGWeapon table. The FG42
// (scoped auto-rifle) gets special treatment in IsItemNecessary's rifle-swap logic.
static bool IsFG42( NRPG::IInventoryItem *pItem )
{
	CDynamicCast<NRPG::CWeaponItem> pWeapon( pItem );
	if ( pWeapon )
		return pWeapon->GetDBWeapon()->GetRecordID() == 21;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56640: pOwner set; only for a live owner: rebuild the wrappers from the RPG bag, then make
// the owner's active RPG item's wrapper current.
CAIInventory::CAIInventory( IAIUnit *_pOwner ):	pOwner(_pOwner)
{
	ASSERT( IsValid( pOwner ) );
	if ( IsValid( pOwner ) )
	{
		FetchInventoryItems();
		//
		CPtr<NRPG::IInventoryItem> pItem = pOwner->GetUnitMission()->GetInventory()->GetActive();
		if ( IsValid( pItem ) )
		{
			CPtr<IAIInventoryItem> pInvItem = GetAIInventoryItem( pItem );
			ASSERT( IsValid( pInvItem ) );
			if ( IsValid( pInvItem ) )
				SetCurrentItem( pInvItem );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x556f0: the wrapper holding this RPG item (unified walk over IAIInventoryItem::GetInventoryItem,
// the retail vtbl+0x10 probe).
IAIInventoryItem* CAIInventory::GetAIInventoryItem( NRPG::IInventoryItem *pItem ) const
{
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) )
		return 0;
	for ( vector< CObj<IAIInventoryItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
	{
		if ( (*i)->GetInventoryItem() == pItem )
			return i->GetPtr();
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55b00: enumerate the owner's RPG inventory, order-preserving (backpack, then equip slots).
void CAIInventory::GetInventoryItems( list< CPtr<NRPG::IInventoryItem> > *pItems ) const
{
	pItems->clear();
	CPtr<NRPG::IInventory> pInventory = pOwner->GetUnitMission()->GetInventory();
	// backpack pass -- retail gathers the live backpack items, then keeps only those that are a
	// clip OR that the inventory can equip into the first weapon slot (the CanEquip(0, item)
	// vtbl+0x1c gate; Jan03 pushed everything, the gate is retail-new).
	const vector<NRPG::SBackPackItem> &vBackPackItems = pInventory->GetItems();
	vector<NRPG::SBackPackItem>::const_iterator i;
	for ( i = vBackPackItems.begin(); i != vBackPackItems.end(); ++i )
	{
		CPtr<NRPG::IInventoryItem> pItem = (*i).pItem;
		if ( !IsValid( pItem ) )
			continue;
		CDynamicCast<NRPG::CClipItem> pClip( pItem );
		if ( pClip || pInventory->CanEquip( NDb::SLOT_1, pItem ) )
			pItems->push_back( pItem );
	}
	// items from the equipment slots -- live items only, no backpack gate
	for ( int nSlot = 0; nSlot < NDb::N_SLOTS; ++nSlot )
	{
		CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)nSlot );
		if ( IsValid( pItem ) )
			pItems->push_back( pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55f90: rebuild the AI inventory from the owner's RPG inventory. Two AddItem passes (non-clip
// gear first so the weapon wrappers exist before their clips arrive), then the unit's innate
// default (bare-hands) melee weapon is added UNCONDITIONALLY from CUnit::pDefaultWeapon (the
// GetRPGUnit()+0x74 read at 0x45605e; AddItem ignores null).
void CAIInventory::FetchInventoryItems()
{
	list< CPtr<NRPG::IInventoryItem> > rpgItems;
	GetInventoryItems( &rpgItems );
	list< CPtr<NRPG::IInventoryItem> >::iterator i;
	for ( i = rpgItems.begin(); i != rpgItems.end(); ++i )
	{
		CDynamicCast<NRPG::CClipItem> pClip( *i );
		if ( !pClip )
			AddItem( *i );
	}
	for ( i = rpgItems.begin(); i != rpgItems.end(); ++i )
	{
		CDynamicCast<NRPG::CClipItem> pClip( *i );
		if ( pClip )
			AddItem( *i );
	}
	AddItem( pOwner->GetRPGUnit()->pDefaultWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x554f0: wrap a non-clip RPG item in its AI wrapper -- the dynamic-cast dispatch over the item
// type. A firearm splits on the DB bBazookaLogic flag (rocket-launcher leaf vs firearms leaf); a
// melee weapon splits on the DB bThrowing flag (throwing vs melee -- ONE wrapper, never both; the
// pre-convergence dev tree wrapped a knife as melee AND throwing, which never matched retail).
IAIInventoryItem* CAIInventory::CreateWeapon( NRPG::IInventoryItem *pItem )
{
	{
		CDynamicCast<NRPG::CWeaponItem> pFireArms( pItem );
		if ( pFireArms )
		{
			if ( pFireArms->GetDBWeapon()->bBazookaLogic )
				return CreateAIRocketLauncherWeapon( pOwner, pFireArms );
			return CreateAIFireArmsWeapon( pOwner, pFireArms );
		}
	}
	{
		CDynamicCast<NRPG::CGrenadeItem> pGrenade( pItem );
		if ( pGrenade )
			return CreateAIGrenadeWeapon( pGrenade );
	}
	{
		CDynamicCast<NRPG::CMeleeWeaponItem> pMelee( pItem );
		if ( pMelee )
		{
			if ( pMelee->GetDBMeleeWeapon()->bThrowing )
				return CreateAIThrowingWeapon( pMelee );
			return CreateAIMeleeWeapon( pMelee );
		}
	}
	{
		CDynamicCast<NRPG::CFirstAidItem> pFirstAid( pItem );
		if ( pFirstAid )
			return CreateAIFirstAid( pFirstAid );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55e20: a clip joins the suitable weapon's spares; anything else gets a fresh AI wrapper
// appended to the unified vector. Null/dead items are ignored (no duplicate check in retail).
void CAIInventory::AddItem( NRPG::IInventoryItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;
	CDynamicCast<NRPG::CClipItem> pClipItem( pItem );
	if ( pClipItem )
	{
		CObj<CAIFireArmsWeaponClip> pClip = CreateAIFireArmsWeaponClip( pClipItem );
		if ( IsValid( pClip ) )
		{
			CPtr<CAIFireArmsWeaponBase> pWeapon = GetSuitableWeapon( pClip );
			if ( IsValid( pWeapon ) )
				pWeapon->AddClip( pClip );
		}
	}
	else
	{
		CObj<IAIInventoryItem> pAIItem = CreateWeapon( pItem );
		if ( IsValid( pAIItem ) )
			items.push_back( pAIItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55a70: drop EVERY wrapper holding this RPG item; when the hand held it, the hand empties too.
void CAIInventory::RemoveItem( NRPG::IInventoryItem *pItem )
{
	for ( int i = 0; i < (int)items.size(); )
	{
		if ( items[i]->GetInventoryItem() == pItem )
		{
			items.erase( items.begin() + i );
			if ( IsValid( pCurrent ) && pCurrent->GetInventoryItem() == pItem )
				pCurrent = 0;
		}
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55db0: drop the wrapper itself. Retail does NOT touch pCurrent here (the CAILog* callers
// follow up with SetCurrentItem(0) themselves -- ModifyState @0x5c220/@0x5c7f0).
void CAIInventory::RemoveItem( IAIInventoryItem *pItem )
{
	for ( int i = 0; i < (int)items.size(); )
	{
		if ( items[i].GetPtr() == pItem )
			items.erase( items.begin() + i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// a5dll-only (no retail counterpart): the planner's RollBack inverse of RemoveItem(IAIInventoryItem*)
// for the CAILog* speculative-commit records -- re-append an existing live wrapper.
void CAIInventory::RestoreItem( IAIInventoryItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	if ( IsValid( pItem ) )
		items.push_back( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55750: dead items clear the hand.
void CAIInventory::SetCurrentItem( IAIInventoryItem *pItem )
{
	if ( IsValid( pItem ) )
		pCurrent = pItem;
	else
		pCurrent = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIInventoryItem* CAIInventory::GetCurrentItem() const
{
	return pCurrent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIInventory::IsCurrentItem( IAIInventoryItem *pItem ) const   // @0x55620
{
	ASSERT( IsValid( pItem ) );
	return pCurrent == pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// a5dll helper (aiUnit.cpp) -- the in-hand item when it is a live non-rocket-launcher firearm.
CAIFireArmsWeapon* CAIInventory::GetCurrentFireArms() const
{
	CPtr<CAIFireArmsWeapon> pRes = CDynamicCast<CAIFireArmsWeapon>( pCurrent ).GetPtr();
	if ( !IsValid( pRes ) || pRes->IsRocketLauncher() )
		return 0;
	else
		return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56300 -- the retail body IS GetBestWeaponSimple<CAIFireArmsWeapon> (its EH frame still names
// the template): the CURRENT item when it is a live firearm, else the first firearm in inventory
// order. Rocket launchers never match the probe. Used by the assassin/defence reactions.
CAIFireArmsWeapon* CAIInventory::GetFirstFireArms() const
{
	return GetBestWeaponSimpleT<CAIFireArmsWeapon>( items, pCurrent.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x55790: the first weapon (firearms base -- rocket launchers included) in inventory order that
// takes this clip.
CAIFireArmsWeaponBase* CAIInventory::GetSuitableWeapon( CAIFireArmsWeaponClip *pClip ) const
{
	ASSERT( IsValid( pClip ) );
	if ( !IsValid( pClip ) )
		return 0;
	for ( vector< CObj<IAIInventoryItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
	{
		CAIFireArmsWeaponBase *pWeapon = CastInventoryItem<CAIFireArmsWeaponBase>( i->GetPtr() );
		if ( pWeapon && pWeapon->IsSuitableClip( pClip ) )
			return pWeapon;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x560e0: the to-hit/damage scoring over GetItems<CAIFireArmsWeapon> (rocket launchers excluded).
CAIFireArmsWeapon* CAIInventory::GetBestFireArms( const NAI::SUnitPosition &pos, IAIUnit *pTarget,
	int nAP, int *pBestWeaponHitCover, int *pQuality,	NDb::EShootMode *eShootMode, int *nMaxToHit ) const
{
	CAIFireArmsWeapon *pBestWeapon = 0;
	int nShootMode = 0;
	*nMaxToHit = 0;
	*pBestWeaponHitCover = 0;
	*pQuality = -0xFFFF;
	vector< CObj<CAIFireArmsWeapon> > fireArms;
	GetItemsOfType( items, &fireArms );
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i  )
	{
		CPtr<NRPG::CWeaponItem> pWeaponItem( (*i)->GetItem() );
		int nHitCover = pOwner->GetCoverForFixedUnit( pos, pTarget->GetUnitServer(), pWeaponItem, NAI::HL_ANY );
		//
		for ( int nShootMode = ( int )NDb::SM_Snap; nShootMode != ( int )NDb::SM_Snipe; ++nShootMode )
		{
			if ( !pWeaponItem->IsShootModeSupported( ( NDb::EShootMode )nShootMode ) )
				continue;
			// retail @0x560e0: per-mode owner disable gate (IAIUnit vtbl+0x84 == CAIUnit::IsShootModeDisabled
			// @0xad590) -- sits between the support check and GetDamage; fed by the Defence-dance
			// Disable/EnableShootMode brackets (aiDefenceReaction.cpp Update @0x3b160).
			if ( pOwner->IsShootModeDisabled( ( NDb::EShootMode )nShootMode ) )
				continue;
			int nTmpMaxToHit;
			int nTmpQuality = (*i)->GetDamage( pos, pTarget, nHitCover, NAI::WALK, nAP, ( NDb::EShootMode )nShootMode, &nTmpMaxToHit );
			if ( nTmpQuality > *pQuality )
			{
				// retail @0x560e0: the out nMaxToHit is updated ONLY in the quality-winner branch (the
				// reported to-hit belongs to the CHOSEN weapon/mode); dev's unconditional max() inflated
				// nToHit with losing modes' values, pushing extra places over the ComparePlaces bar.
				pBestWeapon = i->GetPtr();
				*pQuality = nTmpQuality;
				*eShootMode = (NDb::EShootMode)nShootMode;
				*pBestWeaponHitCover = nHitCover;
				*nMaxToHit = nTmpMaxToHit;
			}
		}
	}
	return pBestWeapon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x567b0 = GetBestWeaponSimple<CAIGrenadeWeapon>. ptTarget is accepted for signature parity but
// UNUSED by the retail body too.
CAIGrenadeWeapon* CAIInventory::GetBestGrenade( CVec3 ptTarget ) const
{
	return GetBestWeaponSimpleT<CAIGrenadeWeapon>( items, pCurrent.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x567c0 = GetBestWeaponSimple<CAIRocketLauncherWeapon> (thunk to @0x58490).
CAIFireArmsWeapon* CAIInventory::GetBestRocketLaunchers() const
{
	return GetBestWeaponSimpleT<CAIRocketLauncherWeapon>( items, pCurrent.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x563b0: GetItems<CAIMeleeWeapon> + the avg-damage scorer @0x55960 (used by CAIMeleeAction).
CAIMeleeWeapon* CAIInventory::GetBestMeleeWeapon() const
{
	vector< CObj<CAIMeleeWeapon> > weapons;
	GetItemsOfType( items, &weapons );
	return GetBestMeleeWeaponT( weapons );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56310: GetItems<CAIThrowingWeapon> + the avg-damage scorer @0x55850 (used by CAIThrowKnifeAction).
CAIThrowingWeapon* CAIInventory::GetBestThrowingWeapon() const
{
	vector< CObj<CAIThrowingWeapon> > weapons;
	GetItemsOfType( items, &weapons );
	return GetBestMeleeWeaponT( weapons );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x567d0 = GetBestWeaponSimple<CAIFirstAid> (inlined copy @0x58580; used by CAIHealAction).
CAIFirstAid* CAIInventory::GetBestFirstAid() const
{
	return GetBestWeaponSimpleT<CAIFirstAid>( items, pCurrent.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb5b80 (IsCurrentClipFull) + @0xb6250 (HasAmmoForReload): the weapon is reload-eligible when
// its CURRENT clip is NOT at its instance capacity AND a live spare clip sits at the FRONT of the
// spares vector. A null/dead current clip counts as not-full. The release does NOT consult the
// spare's own ammo, nor require spare != current.
static bool NeedsReloadAndCan( CAIFireArmsWeaponBase *pWeapon )
{
	if ( !IsValid( pWeapon ) )
		return false;
	if ( pWeapon->IsCurrentClipFull() )
		return false;
	// HasAmmoForReload @0xb6250: a live spare clip at the FRONT of the spares vector (ammo not checked).
	return IsValid( pWeapon->GetNextClip() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56450 -- the release scans GetItems<CAIFireArmsWeaponBase> (rocket launchers INCLUDED) in
// inventory order and returns the FIRST reload-eligible weapon. There is NO current-item
// preference (used by CAIReloadAction). Retail types the result as the base; kept as
// CAIFireArmsWeapon* for the unowned aiActions.cpp caller (every dev base instance is one).
CAIFireArmsWeapon* CAIInventory::GetBestWeaponForReload() const
{
	vector< CObj<CAIFireArmsWeaponBase> > weapons;
	GetItemsOfType( items, &weapons );
	for ( vector< CObj<CAIFireArmsWeaponBase> >::const_iterator i = weapons.begin(); i != weapons.end(); ++i )
	{
		if ( NeedsReloadAndCan( i->GetPtr() ) )
			return static_cast<CAIFireArmsWeapon*>( i->GetPtr() );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56540 (used by CAIBeginSnipeAction): the first firearm whose weapon item supports the snipe
// shoot-mode (NDb::SM_Snipe) and has a non-empty current clip; inventory order, no current-item
// preference.
CAIFireArmsWeapon* CAIInventory::GetBestWeaponForSnipe() const
{
	vector< CObj<CAIFireArmsWeapon> > fireArms;
	GetItemsOfType( items, &fireArms );
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i )
	{
		CAIFireArmsWeapon *pWeapon = i->GetPtr();
		if ( !IsValid( pWeapon ) )
			continue;
		CPtr<NRPG::CWeaponItem> pWeaponItem( pWeapon->GetItem() );
		if ( !IsValid( pWeaponItem ) || !pWeaponItem->IsShootModeSupported( NDb::SM_Snipe ) )
			continue;
		CAIFireArmsWeaponClip *pClip = pWeapon->GetCurrentClip();
		if ( IsValid( pClip ) && !pClip->IsEmpty() )
			return pWeapon;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// the first frozen item in a list whose RPG item resolves to T (the GetItemOfType<T> @0x570d0.. of
// the release GetMostNecessaryItem). Helper for the loot type-priority cascade.
template<class T>
static NWorld::CDFrozenItem* LootFindByType( const list< CPtr<NWorld::CDFrozenItem> > &items )
{
	for ( list< CPtr<NWorld::CDFrozenItem> >::const_iterator i = items.begin(); i != items.end(); ++i )
	{
		NWorld::CDFrozenItem *pItem = i->GetPtr();
		if ( !IsValid( pItem ) )
			continue;
		CDynamicCast<T> pTyped( pItem->GetInvItem() );
		if ( pTyped )
			return pItem;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::IsItemNecessary @0x56810 (used by CAILootAction): is a ground item worth taking
// given what I already carry; *ppToDrop names the held item to drop to make room (0 if none).
// Decode (decomp + raw disasm of the count compares at 0x45693b/0x4569ae/0x4569e2 and of the
// rifle case at 0x456c76..0x456d8a):
//   - non-clips must pass the SAME CanEquip(slot 0, item) gate GetInventoryItems applies;
//   - grenades/throwing weapons (DB bThrowing): take while <= 3 are held; melee: while <= 1;
//     first aid: while none is held;
//   - clips: worth taking while the suitable held weapon has fewer than 4 spare clips;
//   - bazooka-logic weapons: take while fewer than 2 rocket launchers are held;
//   - other firearms: matched by ANIM HOLD-TYPE (CRPGWeapon::pAnimWeaponType->type; a held-weapon
//     map is built with WT_RLAUNCHER excluded), with per-type swap rules below. Comparisons are
//     CAIFireArmsWeaponBase::IsWorseThen @0xb5a60 (DB AIRating).
bool CAIInventory::IsItemNecessary( NWorld::CDFrozenItem *pFrozen, IAIInventoryItem **ppToDrop ) const
{
	*ppToDrop = 0;
	// retail fetches the RPG item straight away (no frozen-item validity gate); a null item bails.
	CPtr<NRPG::IInventoryItem> pRPG = pFrozen->GetInvItem();
	if ( !pRPG )
		return false;
	CDynamicCast<NRPG::CClipItem> pClipItem( pRPG );
	// gate @0x4568a4: anything that is not a clip must be equippable into the first weapon slot.
	if ( !pClipItem )
	{
		CPtr<NRPG::IInventory> pInventory = pOwner->GetUnitMission()->GetInventory();
		if ( !pInventory->CanEquip( NDb::SLOT_1, pRPG ) )
			return false;
	}
	{
		CDynamicCast<NRPG::CGrenadeItem> pGrenade( pRPG );        // grenade -- take while <= 3 held (disasm 0x45693b: cmp 3/setle)
		if ( pGrenade )
			return GetItemsCountOfType<CAIGrenadeWeapon>( items ) <= 3;
	}
	{
		CDynamicCast<NRPG::CMeleeWeaponItem> pMelee( pRPG );      // melee -- by the DB bThrowing split
		if ( pMelee )
		{
			if ( pMelee->GetDBMeleeWeapon()->bThrowing )          // throwing -- take while <= 3 held (0x4569ae)
				return GetItemsCountOfType<CAIThrowingWeapon>( items ) <= 3;
			return GetItemsCountOfType<CAIMeleeWeapon>( items ) <= 1;   // melee -- take while <= 1 held (0x4569e2: cmp eax,ebp==1)
		}
	}
	if ( pClipItem )                                              // clip -- suitable weapon short on spares
	{
		CObj<CAIFireArmsWeaponClip> pClip = CreateAIFireArmsWeaponClip( pClipItem );
		CPtr<CAIFireArmsWeaponBase> pWeapon = GetSuitableWeapon( pClip );
		return IsValid( pWeapon ) && pWeapon->GetClipCount() < 4;
	}
	{
		CDynamicCast<NRPG::CFirstAidItem> pFirstAid( pRPG );      // first aid -- keep 1
		if ( pFirstAid )
			return GetItemsCountOfType<CAIFirstAid>( items ) < 1;
	}
	CDynamicCast<NRPG::CWeaponItem> pWeap( pRPG );                // firearm
	if ( !pWeap )
		return false;
	if ( !pWeap->IsWorking() )                                    // 0x456bff: a broken weapon is never loot
		return false;
	NDb::CRPGWeapon *pDB = pWeap->GetDBWeapon();
	if ( pDB->bBazookaLogic )                                     // rocket launcher -- keep up to 2
		return GetItemsCountOfType<CAIRocketLauncherWeapon>( items ) < 2;
	// held-firearms map by anim hold-type (retail hash_map<int, CPtr<CAIFireArmsWeapon>>; the
	// last weapon of a type wins; WT_RLAUNCHER never enters the map).
	unordered_map< int, CPtr<CAIFireArmsWeapon> > heldByType;
	{
		vector< CObj<CAIFireArmsWeapon> > fireArms;
		GetItemsOfType( items, &fireArms );
		for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i )
		{
			int nHeldType = (int)(*i)->GetItem()->GetDBWeapon()->pAnimWeaponType->type;
			if ( nHeldType != (int)NDb::WT_RLAUNCHER )
				heldByType[nHeldType] = i->GetPtr();
		}
	}
	unordered_map< int, CPtr<CAIFireArmsWeapon> >::iterator it;
	switch ( pDB->pAnimWeaponType->type )
	{
	case NDb::WT_PISTOL:
		it = heldByType.find( (int)NDb::WT_PISTOL );
		if ( it != heldByType.end() )
		{
			CAIFireArmsWeapon *pHeld = it->second;
			*ppToDrop = pHeld;
			return pHeld->IsWorseThen( pWeap );
		}
		return true;                                              // no pistol held -> take it
	case NDb::WT_RIFLE:
		it = heldByType.find( (int)NDb::WT_RIFLE );
		if ( it != heldByType.end() )
		{
			CAIFireArmsWeapon *pHeld = it->second;
			*ppToDrop = pHeld;                                    // named even on the "keep mine" paths (0x456c85)
			if ( IsFG42( pHeld->GetItem() ) )                     // never drop the FG42
				return false;
			if ( IsFG42( pRPG ) )                                 // always take an FG42
				return true;
			if ( pHeld->GetItem()->GetDBWeapon()->bScope )        // scoped held: only a better scoped one wins
				return pHeld->IsWorseThen( pWeap ) && pDB->bScope;
			if ( pDB->bScope )                                    // unscoped held, scoped candidate: take
				return true;
			return pHeld->IsWorseThen( pWeap );
		}
		// no rifle held: an SMG or MG in hand covers the niche
		if ( heldByType.find( (int)NDb::WT_SUB_MACHINE_GUN ) != heldByType.end() )
			return false;
		if ( heldByType.find( (int)NDb::WT_MACHINE_GUN ) != heldByType.end() )
			return false;
		return true;
	case NDb::WT_SUB_MACHINE_GUN:
		it = heldByType.find( (int)NDb::WT_SUB_MACHINE_GUN );
		if ( it != heldByType.end() )
		{
			CAIFireArmsWeapon *pHeld = it->second;
			*ppToDrop = pHeld;
			return pHeld->IsWorseThen( pWeap );
		}
		if ( heldByType.find( (int)NDb::WT_RIFLE ) != heldByType.end() )
			return false;
		if ( heldByType.find( (int)NDb::WT_MACHINE_GUN ) != heldByType.end() )
			return false;
		return true;
	case NDb::WT_MACHINE_GUN:
		it = heldByType.find( (int)NDb::WT_MACHINE_GUN );
		if ( it != heldByType.end() )
		{
			CAIFireArmsWeapon *pHeld = it->second;
			*ppToDrop = pHeld;
			return pHeld->IsWorseThen( pWeap );
		}
		// no MG held: take it unless a rifle covers the niche; a held SMG is what gets dropped
		it = heldByType.find( (int)NDb::WT_SUB_MACHINE_GUN );
		if ( it != heldByType.end() )
			*ppToDrop = it->second;
		return heldByType.find( (int)NDb::WT_RIFLE ) == heldByType.end();
	default:
		return false;                                             // other hold types are never loot-worthy
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::GetMostNecessaryItem @0x55680: of the wanted items, the highest-priority by item
// type: firearm > clip > grenade > melee > first-aid (the release GetItemOfType<T> cascade).
NWorld::CDFrozenItem* CAIInventory::GetMostNecessaryItem( const list< CPtr<NWorld::CDFrozenItem> > &items ) const
{
	NWorld::CDFrozenItem *p;
	if ( ( p = LootFindByType<NRPG::CWeaponItem>( items ) ) != 0 ) return p;
	if ( ( p = LootFindByType<NRPG::CClipItem>( items ) ) != 0 ) return p;
	if ( ( p = LootFindByType<NRPG::CGrenadeItem>( items ) ) != 0 ) return p;
	if ( ( p = LootFindByType<NRPG::CMeleeWeaponItem>( items ) ) != 0 ) return p;
	if ( ( p = LootFindByType<NRPG::CFirstAidItem>( items ) ) != 0 ) return p;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x56fd0: any firearm/rocket launcher (the FireArmsBase probe), grenade, throwing or melee weapon.
bool CAIInventory::HasAnyWeapon() const
{
	return GetItemsCountOfType<CAIFireArmsWeaponBase>( items ) > 0
		|| GetItemsCountOfType<CAIGrenadeWeapon>( items ) > 0
		|| GetItemsCountOfType<CAIThrowingWeapon>( items ) > 0
		|| GetItemsCountOfType<CAIMeleeWeapon>( items ) > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x567e0: HasAnyWeapon minus the melee probe (any ranged/thrown weapon).
bool CAIInventory::HasAnyNotMeleeWeapon() const
{
	return GetItemsCountOfType<CAIFireArmsWeaponBase>( items ) > 0
		|| GetItemsCountOfType<CAIGrenadeWeapon>( items ) > 0
		|| GetItemsCountOfType<CAIThrowingWeapon>( items ) > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::DebugOutput() const
{
	csSystem << "Unit inventory:\n[\n";
	//
	vector< CObj<CAIFireArmsWeapon> > fireArms;
	GetItemsOfType( items, &fireArms );
	csSystem << "FireArms : " << ( int )fireArms.size();
	csSystem << "     [  ";
	vector< CObj<CAIFireArmsWeapon> >::const_iterator i;
	for ( i = fireArms.begin(); i != fireArms.end(); ++i )
		csSystem << "  " << (*i)->GetClipCount();
	csSystem << "   ]\n";
	//
	vector< CObj<CAIRocketLauncherWeapon> > rocketLaunchers;
	GetItemsOfType( items, &rocketLaunchers );
	csSystem << "RocketLaunchers : " << ( int )rocketLaunchers.size();
	csSystem << "     [  ";
	vector< CObj<CAIRocketLauncherWeapon> >::const_iterator j;
	for ( j = rocketLaunchers.begin(); j != rocketLaunchers.end(); ++j )
		csSystem << "  " << (*j)->GetClipCount();
	csSystem << "   ]\n";
	//
	csSystem << "Grenades : " << GetItemsCountOfType<CAIGrenadeWeapon>( items ) << "\n";
	csSystem << "]\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIInventory* CreateAIInventory( IAIUnit *pOwner )
{
	ASSERT( pOwner->GetUnitServer()->CanFight() );
	return new CAIInventory( pOwner );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52642102, CAIInventory );
