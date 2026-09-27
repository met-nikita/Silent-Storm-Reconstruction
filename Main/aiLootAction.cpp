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
#include "wMain.h"
#include "wMainPath.h"
#include "wUnitAttack.h"
#include "aiPath.h"
#include "aiMoveAction.h"
//
#include "aiActions.h"
namespace NWorld { bool IsWithinHumanReach( const CVec3 &ptFrom, const CVec3 &ptTarget, float fPlaneDist ); }
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The after-combat loot action: pick up the most necessary item the player knows is on the ground (and
// drop what must go to make room for it). Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_ailootaction.h: GetInfoInner @0x63210 / Do @0x63a80). The item-necessity scoring
// (CAIInventory::IsItemNecessary / GetMostNecessaryItem) is reconstructed in aiInventory.cpp.
//
// Use the shared human-reach search to validate the destination and batch other
// useful items reachable from its endpoint. Drop lists include the old gun's spares.
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
			pToDrop->GetInventoryItemWithClips( &pInfo->itemsToDrop[ CPtr<NWorld::CDFrozenItem>( pItem ) ] );
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
	vector<SPathPlace> places;
	NWorld::GetHumanReachPlaces( pUS, pInfo->pItem->GetPos(), &places, 0.625f );
	if ( places.empty() )
		return;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	CPtr<CPath> path = NWorld::FindPath( pNet, pUS, pU->GetPosition().p, places, pUS,
		false, PF_DEFAULT, false, true, true );
	if ( !IsValid( path ) || path->points.empty() )
		return;
	CVec3 ptDest = GetUnitPos( path->points.back(), pNet ).GetCP();
	for ( list< CPtr<NWorld::CDFrozenItem> >::const_iterator it = wanted.begin(); it != wanted.end(); ++it )
		if ( *it != pInfo->pItem && NWorld::IsWithinHumanReach( ptDest, (*it)->GetPos(), 0.625f ) )
			pInfo->otherItem.push_back( *it );
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
	EPose pose = pU->GetUnitServer()->IsWearingPK() ? WALK : wishPose;
	*pLog << new CAILogPickUpItem( pU, info.pItem, pose );
	for ( list< CPtr<NWorld::CDFrozenItem> >::const_iterator it = info.otherItem.begin(); it != info.otherItem.end(); ++it )
	{
		IAIInventoryItem *pToDrop = 0;
		// Previous records have already updated the simulated inventory.
		if ( !IsValid( *it ) || !pU->GetAIInventory()->IsItemNecessary( *it, &pToDrop ) )
			continue;
		const vector< CPtr<NRPG::IInventoryItem> > &otherDrops = info.itemsToDrop[*it];
		for ( int i = 0; i < (int)otherDrops.size(); ++i )
			if ( IsValid( otherDrops[i] ) )
				*pLog << new CAILogDropItem( pU, otherDrops[i] );
		*pLog << new CAILogPickUpItem( pU, *it, pose );
	}
}
}
