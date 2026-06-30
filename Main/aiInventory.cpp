#include "stdafx.h"
//
#include "RPGUnitMission.h"
#include "RPGItemSet.h"
#include "RPGItem.h"
#include "RPGAttackMech.h"
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
// CAIInventory
////////////////////////////////////////////////////////////////////////////////////////////////////
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
template< class T >
static IAIInventoryItem* FindAIInventoryItem( NRPG::IInventoryItem *pItem, const vector< CObj<T> > &items )
{
	ASSERT( IsValid( pItem ) );
	if ( IsValid( pItem ) )
	{
		for ( vector< CObj<T> >::const_iterator i = items.begin(); i != items.end(); ++i )
		{
			if ( CDynamicCast<NRPG::IInventoryItem>( (*i)->GetItem() ).GetPtr() == pItem )
				return *i;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIInventoryItem* CAIInventory::GetAIInventoryItem( NRPG::IInventoryItem *pItem ) const
{
	IAIInventoryItem *pRes = 0;
	ASSERT( IsValid( pItem ) );
	if ( IsValid( pItem ) )
	{
		pRes = FindAIInventoryItem( pItem, fireArms );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, grenades );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, rocketLaunchers );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, meleeWeapons );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, throwingWeapons );
		if ( !pRes )
			pRes = FindAIInventoryItem( pItem, firstAids );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::GetInventoryItems( list< CPtr<NRPG::IInventoryItem> > *pItems ) const
{
	pItems->clear();
	CPtr<NRPG::IInventory> pInventory = pOwner->GetUnitMission()->GetInventory();
	// ���� �� ��������
	const vector<NRPG::SBackPackItem> &vBackPackItems = pInventory->GetItems();
	vector<NRPG::SBackPackItem>::const_iterator i;
	for ( i = vBackPackItems.begin(); i != vBackPackItems.end(); ++i )
	{
		CPtr<NRPG::IInventoryItem> pItem = (*i).pItem;
		ASSERT( IsValid( pItem ) );
		if ( IsValid( pItem ) )
			pItems->push_back( pItem );
	}
	// ���� �� ������
	for ( int nSlot = 0; nSlot < NDb::N_SLOTS; ++nSlot )
	{
		CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)nSlot );
		if ( IsValid( pItem ) )
			pItems->push_back( pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::FetchInventoryItems()
{
	list< CPtr<NRPG::IInventoryItem> > items;
	GetInventoryItems( &items );
	for ( list< CPtr<NRPG::IInventoryItem> >::iterator i = items.begin(); i != items.end(); ++i  )
	{
		ASSERT( IsValid( *i ) );
		if ( IsValid( *i ) )
		{
			CDynamicCast<NRPG::CWeaponItem> pFireArms(*i);
			if (pFireArms)
			{
				CPtr<CAIFireArmsWeapon> pWeapon = CreateAIFireArmsWeapon( pOwner, pFireArms );
				if ( pWeapon->IsRocketLauncher() )
					AddRocketLaunchers( pWeapon );
				else
					AddFireArms( pWeapon );
			}
			CDynamicCast<NRPG::CGrenadeItem> pGrenade(*i);
			if (pGrenade)
				AddGrenade( CreateAIGrenadeWeapon( pGrenade ) );
			// melee weapons (default/knife/katana); a knife (WT_KNIFE) is also a throwable weapon.
			CDynamicCast<NRPG::CMeleeWeaponItem> pMelee(*i);
			if ( pMelee )
			{
				AddMeleeWeapon( CreateAIMeleeWeapon( pMelee ) );
				if ( pMelee->GetWeaponType() == NDb::WT_KNIFE )
					AddThrowingWeapon( CreateAIThrowingWeapon( pMelee ) );
			}
			CDynamicCast<NRPG::CFirstAidItem> pFirstAid(*i);
			if ( pFirstAid )
				AddFirstAid( CreateAIFirstAid( pFirstAid ) );
		}
	}
	//
	for ( list< CPtr<NRPG::IInventoryItem> >::iterator i = items.begin(); i != items.end(); ++i  )
	{
		ASSERT( IsValid( *i ) );
		if ( IsValid( *i ) )
		{
			CDynamicCast<NRPG::CClipItem> pClip(*i);
			if ( pClip )
				AddClip( CreateAIFireArmsWeaponClip( pClip ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddClip( CAIFireArmsWeaponClip *pClip )
{
	ASSERT( IsValid( pClip ) );
	CPtr<CAIFireArmsWeaponClip> pHolder = pClip;
	CPtr<CAIFireArmsWeapon> pWeapon = GetSuitableWeapon( pClip );
	if ( IsValid(pWeapon) )
		pWeapon->AddClip( pClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static void AddItemToInventory( T *pItem, vector< CObj<T> > *pVector )
{
	ASSERT( IsValid( pItem ) );
	ASSERT( find( pVector->begin(), pVector->end(), pItem ) == pVector->end() );
	if ( IsValid( pItem ) )
		pVector->push_back( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
void RemoveItemFromInventory( T *pItem, vector< CObj<T> > *pVector )
{
	bool bCurrent = false;
	ASSERT( IsValid( pItem ) );
	ASSERT( find( pVector->begin(), pVector->end(), pItem ) != pVector->end() );
	pVector->erase( remove( pVector->begin(), pVector->end(), pItem ), pVector->end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddFireArms( CAIFireArmsWeapon *pWeapon )
{
	bool bRocketLauncher = pWeapon->IsRocketLauncher();
	ASSERT( !bRocketLauncher );
	if ( !bRocketLauncher )
		AddItemToInventory( pWeapon, &fireArms );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveFireArms( CAIFireArmsWeapon *pWeapon )
{
	RemoveItemFromInventory( pWeapon, &fireArms );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddRocketLaunchers( CAIFireArmsWeapon *pLauncher )
{
	bool bRocketLauncher = pLauncher->IsRocketLauncher();
	ASSERT( bRocketLauncher );
	if ( bRocketLauncher )
		AddItemToInventory( pLauncher, &rocketLaunchers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveRocketLaunchers( CAIFireArmsWeapon *pLauncher )
{
	RemoveItemFromInventory( pLauncher, &rocketLaunchers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddGrenade( CAIGrenadeWeapon *pGrenade )
{
	AddItemToInventory( pGrenade, &grenades );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::RemoveGrenade( CAIGrenadeWeapon *pGrenade )
{
	RemoveItemFromInventory( pGrenade, &grenades );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddMeleeWeapon( CAIMeleeWeapon *pWeapon )
{
	AddItemToInventory( pWeapon, &meleeWeapons );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddThrowingWeapon( CAIThrowingWeapon *pWeapon )
{
	AddItemToInventory( pWeapon, &throwingWeapons );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::AddFirstAid( CAIFirstAid *pFirstAid )
{
	AddItemToInventory( pFirstAid, &firstAids );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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
bool CAIInventory::IsCurrentItem( IAIInventoryItem *pItem ) const
{
	ASSERT( IsValid( pItem ) );
	return pCurrent == pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetCurrentFireArms() const
{
	CPtr<CAIFireArmsWeapon> pRes = CDynamicCast<CAIFireArmsWeapon>( pCurrent ).GetPtr();
	if ( !IsValid( pRes ) || pRes->IsRocketLauncher() )
		return 0;
	else
		return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory::GetFirstFireArms @0x56300 -- the first firearm the unit carries. The release scans a single
// unified items vector and dynamic-casts to CAIFireArmsWeapon (so it could surface a rocket launcher first); the
// dev tree keeps firearms and rocket launchers in separate typed vectors, so the first of `fireArms` is the
// first non-RL firearm (behaviour-close -- the assassin gate that consumes this rejects RLs by hold-type anyway).
// Dead until the assassin reaction.
CAIFireArmsWeapon* CAIInventory::GetFirstFireArms() const
{
	return fireArms.empty() ? 0 : fireArms[0].GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetCurrentRocketLauncher() const
{
	CPtr<CAIFireArmsWeapon> pRes = CDynamicCast<CAIFireArmsWeapon>( pCurrent ).GetPtr();
	if ( !IsValid( pRes ) || !pRes->IsRocketLauncher() )
		return 0;
	else
		return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeWeapon* CAIInventory::GetCurrentGrenade() const
{
	return CDynamicCast<CAIGrenadeWeapon>( pCurrent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CAIFireArmsWeapon* FindSuitableWeapon( CAIFireArmsWeaponClip *pClip, const vector< CObj<CAIFireArmsWeapon> > &weapons )
{
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = weapons.begin(); i != weapons.end(); ++i  )
	{
		if ( (*i)->IsSuitableClip( pClip ) )
			return *i;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetSuitableWeapon( CAIFireArmsWeaponClip *pClip ) const
{
	CAIFireArmsWeapon *pRes = 0;
	ASSERT( IsValid( pClip ) );
	if ( IsValid( pClip ) )
	{
		pRes = FindSuitableWeapon( pClip, fireArms );
		if ( !pRes )
			pRes = FindSuitableWeapon( pClip, rocketLaunchers );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetBestFireArms( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
	int nAP, int *pBestWeaponHitCover, int *pQuality,	NDb::EShootMode *eShootMode, int *nMaxToHit ) const
{
	CAIFireArmsWeapon *pBestWeapon = 0;
	int nShootMode = 0;
	*nMaxToHit = 0;
	*pBestWeaponHitCover = 0;
	*pQuality = -0xFFFF;
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i  )
	{
		CPtr<NRPG::CWeaponItem> pWeaponItem( (*i)->GetItem() );
		int nHitCover = pOwner->GetCoverForFixedUnit( pos, pTarget->GetUnitServer(), pWeaponItem, NAI::HL_ANY );
		//
		for ( int nShootMode = ( int )NDb::SM_Snap; nShootMode != ( int )NDb::SM_Snipe; ++nShootMode )
		{
			if ( !pWeaponItem->IsShootModeSupported( ( NDb::EShootMode )nShootMode ) )
				continue;
			//
			int nTmpMaxToHit;
			int nTmpQuality = (*i)->GetDamage( pos, pTarget, nHitCover, NAI::WALK, nAP, ( NDb::EShootMode )nShootMode, &nTmpMaxToHit );
			*nMaxToHit = max( *nMaxToHit, nTmpMaxToHit );
			if ( nTmpQuality > *pQuality )
			{
				pBestWeapon = *i;
				*pQuality = nTmpQuality;
				*eShootMode = (NDb::EShootMode)nShootMode;
				*pBestWeaponHitCover = nHitCover;
			}
		}
	}
	return pBestWeapon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeWeapon* CAIInventory::GetBestGrenade( CVec3 ptTarget ) const
{
	if ( grenades.empty() )
		return 0;
	else
	{
		CPtr<CAIGrenadeWeapon> pGrenade = GetCurrentGrenade();
		if ( !IsValid( pGrenade ) )
			pGrenade = grenades.front();
		return pGrenade;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon* CAIInventory::GetBestRocketLaunchers() const
{
	if ( rocketLaunchers.empty() )
		return 0;
	else
	{
		CPtr<CAIFireArmsWeapon> pLauncher = GetCurrentRocketLauncher();
		if ( !IsValid( pLauncher ) )
			pLauncher = rocketLaunchers.front();
		return pLauncher;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CAIInventory::GetBestMeleeWeapon / GetBestThrowingWeapon (used by CAIMeleeAction /
// CAIThrowKnifeAction @0x0041c3d0 / @0x0041c210). Prefer the equipped one; else the first available.
CAIMeleeWeapon* CAIInventory::GetBestMeleeWeapon() const
{
	if ( meleeWeapons.empty() )
		return 0;
	CDynamicCast<CAIMeleeWeapon> pCurrent( GetCurrentItem() );
	if ( IsValid( pCurrent ) )
		return pCurrent;
	return meleeWeapons.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIThrowingWeapon* CAIInventory::GetBestThrowingWeapon() const
{
	if ( throwingWeapons.empty() )
		return 0;
	CDynamicCast<CAIThrowingWeapon> pCurrent( GetCurrentItem() );
	if ( IsValid( pCurrent ) )
		return pCurrent;
	return throwingWeapons.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release GetBestFirstAid @0x004567d0: the equipped first-aid if any, else the first available.
CAIFirstAid* CAIInventory::GetBestFirstAid() const
{
	if ( firstAids.empty() )
		return 0;
	CDynamicCast<CAIFirstAid> pCurrent( GetCurrentItem() );
	if ( IsValid( pCurrent ) )
		return pCurrent;
	return firstAids.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// true if the firearm's current clip is spent and a different, non-empty spare clip is available.
static bool NeedsReloadAndCan( CAIFireArmsWeapon *pWeapon )
{
	if ( !IsValid( pWeapon ) )
		return false;
	CAIFireArmsWeaponClip *pCur  = pWeapon->GetCurrentClip();
	CAIFireArmsWeaponClip *pNext = pWeapon->GetNextClip();
	const bool bNeeds = !IsValid( pCur ) || pCur->IsEmpty();
	const bool bCan   = IsValid( pNext ) && pNext != pCur && !pNext->IsEmpty();
	return bNeeds && bCan;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed: the release CAIInventory::GetBestWeaponForReload (used by CAIReloadAction @0x0041c0d0).
// Prefer reloading the equipped firearm; otherwise the first firearm with a spent clip + a usable spare.
CAIFireArmsWeapon* CAIInventory::GetBestWeaponForReload() const
{
	CPtr<CAIFireArmsWeapon> pCurrent = GetCurrentFireArms();
	if ( NeedsReloadAndCan( pCurrent ) )
		return pCurrent;
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i )
		if ( NeedsReloadAndCan( *i ) )
			return *i;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed: the release CAIInventory::GetBestWeaponForSnipe (@0x56540, used by CAIBeginSnipeAction).
// The first firearm whose weapon item supports the snipe shoot-mode (NDb::SM_Snipe) and has a non-empty
// current clip. (Decode: GetItems<CAIFireArmsWeapon> -> weaponItem->CanSetShootMode(SM_Snipe=5) && current
// clip not empty -> return the first match.)
CAIFireArmsWeapon* CAIInventory::GetBestWeaponForSnipe() const
{
	for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i )
	{
		CAIFireArmsWeapon *pWeapon = *i;
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
// the first frozen item in a list whose RPG item resolves to T (the GetItemOfType<T> of the release
// GetMostNecessaryItem). Helper for the loot type-priority cascade.
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
// Reconstructed: CAIInventory::IsItemNecessary (@0x56810). Is a ground item worth taking given what I
// already carry; *ppToDrop names the held item to drop to make room (0 if none). From the matched-release
// decode: per-type keep-counts + the firearm "keep the best per store-weapon-type slot" rule.
// Documented simplifications (build-validation scope -- the dev tree lacks the release helpers): the clip
// case approximates the release's owner-RPG clip-need predicate (IUnitMission slot, absent) with "I carry
// a firearm"; throwing knives fold into the melee case (no dev throwing/melee RPG-type split); the FG42
// special-case + the release's secondary store-slot fallback (RIFLE<->SMG<->PK) are omitted.
bool CAIInventory::IsItemNecessary( NWorld::CDFrozenItem *pFrozen, IAIInventoryItem **ppToDrop ) const
{
	*ppToDrop = 0;
	if ( !IsValid( pFrozen ) )
		return false;
	NRPG::IInventoryItem *pRPG = pFrozen->GetInvItem();
	if ( !IsValid( pRPG ) )
		return false;
	{
		CDynamicCast<NRPG::CClipItem> pClip( pRPG );          // clip -- useful while I carry a firearm
		if ( pClip )
			return !fireArms.empty();
	}
	{
		CDynamicCast<NRPG::CGrenadeItem> pGren( pRPG );        // grenade -- keep up to 3
		if ( pGren )
			return (int)grenades.size() < 3;
	}
	{
		CDynamicCast<NRPG::CMeleeWeaponItem> pMelee( pRPG );   // melee (+ throwing) -- keep one
		if ( pMelee )
			return (int)meleeWeapons.size() < 1;
	}
	{
		CDynamicCast<NRPG::CWeaponItem> pWeap( pRPG );         // firearm
		if ( pWeap )
		{
			NDb::CRPGWeapon *pDB = pWeap->GetDBWeapon();
			if ( pDB == 0 )
				return false;
			if ( pDB->bBazookaLogic )                          // rocket launcher -- keep up to 2
				return (int)rocketLaunchers.size() < 2;
			NDb::EStoreWeaponType slot = IsValid( pDB->pWeaponType ) ? pDB->pWeaponType->eStoreWeaponType
			                                                        : NDb::SWT_OTHER;
			for ( vector< CObj<CAIFireArmsWeapon> >::const_iterator i = fireArms.begin(); i != fireArms.end(); ++i )
			{
				CAIFireArmsWeapon *pMine = *i;
				if ( !IsValid( pMine ) || !IsValid( pMine->GetItem() ) )
					continue;
				NDb::CRPGWeapon *pMineDB = pMine->GetItem()->GetDBWeapon();
				if ( pMineDB == 0 || !IsValid( pMineDB->pWeaponType ) )
					continue;
				if ( pMineDB->pWeaponType->eStoreWeaponType == slot )
				{
					// already carry a firearm of this slot -> take the candidate only if mine is worse
					// (IsWorseThen == lower nQuality), and drop mine to make room.
					*ppToDrop = pMine;
					return pMineDB->nQuality < pDB->nQuality;
				}
			}
			return true;                                       // no firearm of this slot yet -> take it
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed: CAIInventory::GetMostNecessaryItem (@0x55680). Of the wanted items, the highest-priority
// by item type: firearm > clip > grenade > melee > first-aid (the release GetItemOfType<T> cascade).
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
bool CAIInventory::HasAnyWeapon() const
{
	return !fireArms.empty() || !grenades.empty() || !rocketLaunchers.empty() || !meleeWeapons.empty() || !throwingWeapons.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIInventory::DebugOutput() const
{
	csSystem << "Unit inventory:\n[\n";
	//
	csSystem << "FireArms : " << ( int )fireArms.size();
	csSystem << "     [  ";
	vector< CObj<CAIFireArmsWeapon> >::const_iterator i;
	for ( i = fireArms.begin(); i != fireArms.end(); ++i )
		csSystem << "  " << (*i)->GetClipCount();
	csSystem << "   ]\n";
	//
	csSystem << "RocketLaunchers : " << ( int )rocketLaunchers.size();
	csSystem << "     [  ";
	for ( i = rocketLaunchers.begin(); i != rocketLaunchers.end(); ++i )
		csSystem << "  " << (*i)->GetClipCount();
	csSystem << "   ]\n";
	//
	csSystem << "Grenades : " << ( int )grenades.size() << "\n";
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