#include "StdAfx.h"
//
#include "..\DBFormat\DataRPG.h" // NDb::EShootMode (aiInventory.h decls reference it)
#include "aiUnit.h"
#include "aiInventory.h"      // NAI::CAIInventory: IsItemNecessary / GetMostNecessaryItem
#include "aiWeapon.h"         // NAI::CAIFireArmsWeapon (the IAIInventoryItem to drop)
#include "aiPosition.h"       // NAI::SPosition::GetCP, fabs( CVec3 )
#include "AILog.h"            // CAILogDropItem / CAILogPickUpItem
#include "aiCombatLog.h"      // NAI::CAILog::operator<<
#include "wUnitServer.h"      // NWorld::CUnitServer: GetPlayer / GetWorld / CanDo
#include "wDebris.h"          // NWorld::CDFrozenItem: GetPos / GetInvItem
#include "wUnitCommands.h"    // NWorld::CCmdMoveInventoryItem / SItem / UCR_OK
#include "wInterface.h"       // NWorld::IPlayer::GetVisibleObjects
//
#include "aiActions.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The after-combat loot action: pick up the most necessary item the player knows is on the ground (and
// drop what must go to make room for it). Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_ailootaction.h: GetInfoInner @0x63210 / Do @0x63a80). The item-necessity scoring
// (CAIInventory::IsItemNecessary / GetMostNecessaryItem) is reconstructed in aiInventory.cpp.
//
// Documented dev<->release elisions (build-validation scope): the release additionally requires a reachable
// human-reach PATH to the chosen item (NWorld::GetHumanReachPlaces @0x393610 -- a Ghidra-mangled CNodesLayer
// flood with no clean dev primitive + no CVec3->SPathPlace accessor) and, having walked it, loots other
// items within human-reach of the path's end in the same trip; that pathing-reachability gate + the
// otherItem batching are elided here. The pick-up record carries no wishPose (the dev CAILogPickUpItem ctor
// predates it). The core decision -- which item to take + what to drop -- is faithful.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// CAILootAction::GetInfoInner @0x63210. The SPlaceWithAP arg is unused -- the decode always runs from the
// unit's current position, whatever candidate place the choose-place job is scoring.
void CAILootAction::GetInfoInner( const SPlaceWithAP &, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	pInfo->pItem = 0;
	pInfo->otherItem.clear();
	pInfo->itemsToDrop.clear();
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	NWorld::IPlayer *pPlayer = pUS->GetPlayer();
	CAIInventory *pInv = pU->GetAIInventory();
	if ( pPlayer == 0 || !IsValid( pInv ) )
		return;
	// every known world object that is a frozen item the inventory wants (IsItemNecessary also names what
	// to drop to make room for it).
	list< CPtr<CObjectBase> > known;
	pPlayer->GetVisibleObjects( &known );
	list< CPtr<NWorld::CDFrozenItem> > wanted;
	for ( list< CPtr<CObjectBase> >::iterator i = known.begin(); i != known.end(); ++i )
	{
		CDynamicCast<NWorld::CDFrozenItem> pItem( i->GetPtr() );
		if ( !pItem )
			continue;
		IAIInventoryItem *pToDrop = 0;
		if ( !pInv->IsItemNecessary( pItem, &pToDrop ) )
			continue;
		wanted.push_back( CPtr<NWorld::CDFrozenItem>( pItem ) );
		if ( IsValid( pToDrop ) )
			pInfo->itemsToDrop[ CPtr<NWorld::CDFrozenItem>( pItem ) ].push_back( pToDrop->GetInventoryItem() );
	}
	// keep the wanted items within 4m whose GROUND -> BACKPACK move the unit would accept (i.e. that fit).
	CVec3 ptOwn = pU->GetPosition().GetCP();
	for ( list< CPtr<NWorld::CDFrozenItem> >::iterator it = wanted.begin(); it != wanted.end(); )
	{
		NWorld::CDFrozenItem *pItem = it->GetPtr();
		if ( !IsValid( pItem ) || fabs( pItem->GetPos() - ptOwn ) > 4.0f )
		{
			it = wanted.erase( it );
			continue;
		}
		NWorld::SItem src( (NWorld::CUnit*)0, NWorld::SItem::GROUND, pItem->GetInvItem() );
		src.pWorldItem = pItem;
		NWorld::SItem dst( (NWorld::CUnit*)pUS, NWorld::SItem::BACKPACK, pItem->GetInvItem() );
		dst.sPosition.x = -1;
		dst.sPosition.y = -1;
		CObj<NWorld::CCmd> pCmd( new NWorld::CCmdMoveInventoryItem( src, dst ) );
		if ( pUS->CanDo( pCmd.GetPtr() ) != NWorld::UCR_OK )
		{
			it = wanted.erase( it );
			continue;
		}
		++it;
	}
	pInfo->pItem = pInv->GetMostNecessaryItem( wanted );
	if ( !IsValid( pInfo->pItem ) )
		return;
	pInfo->bCanDo = true;
}
void CAILootAction::Do( CAILog *pLog ) const                            // @0x63a80
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pItem ) )
		return;
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	// drop the items that make room for the chosen item, then pick it up.
	vector< CPtr<NRPG::IInventoryItem> > &drops = info.itemsToDrop[ info.pItem ];
	for ( int i = 0; i < (int)drops.size(); ++i )
		if ( IsValid( drops[i] ) )
			*pLog << new CAILogDropItem( pU, drops[i].GetPtr() );
	*pLog << new CAILogPickUpItem( pU, info.pItem, wishPose );
	// (the release loots additional items within human-reach of the walked path's end in the same trip --
	//  otherItem, tied to the elided GetHumanReachPlaces pathing -- omitted here; see the file header.)
}
}
