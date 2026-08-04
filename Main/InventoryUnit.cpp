#include "StdAfx.h"
//#include "RPGGame.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RPGUnit.h"	// NRPG::CUnit::pHeadInfo (retail cap-vs-hair gate @0x21db20)
#include "LSHead.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
//#include "GAnimation.h"
#include "InventoryUnit.h"
#include "..\Misc\RandomGen.h"
//#include "wInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int nItemPlaces2UnitItemTypes[ NDb::N_ITEM_PLACES ] = 
{
	UIT_BELT_L1,
	UIT_BELT_R1,
	UIT_BELT_M1,
	UIT_BELT_MEDIUM_L1,
	UIT_BELT_MEDIUM_R1,
	UIT_BELT_MEDIUM_L2,
	UIT_BELT_MEDIUM_R2,
	UIT_WAIST_BELT_L1,
	UIT_WAIST_BELT_R1,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszItemEffectors[N_UNIT_ITEM_TYPES] = 
{
	0, 0, 
	"Slot5",
	"Slot3",
	"Slot4",
	"Slot9",
	"Slot6",
	"Slot8",
	"Slot7",
	"Slot2",
	"Slot1",
	"Cap",
	"BackPack",
	"L_Weapon",
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GetBoneName( EUnitItemType type, NRPG::IInventoryItem *pItem, bool bIsPK )
{
	const char *pszBoneName = 0;
	switch ( type )
	{
	case UIT_WEAPON_HEAVY:
	{
		CDynamicCast<NRPG::IWeaponItemInfo> pWeapon(pItem);
		if (pWeapon)
		{
			NDb::CAnimWeaponType* pWType = pWeapon->GetDBWeapon()->pAnimWeaponType;
			ASSERT(pWType);
			if (!pWType)
				return "";
			switch (pWType->type)
			{
			case NDb::WT_RIFLE:
				pszBoneName = "Rifle";
				break;
			case NDb::WT_SUB_MACHINE_GUN:
				pszBoneName = "SubMachineGun";
				break;
			case NDb::WT_MACHINE_GUN:
				pszBoneName = "MachineGun";
				break;
			case NDb::WT_RLAUNCHER:
				pszBoneName = "RocketLauncher";
				break;
			default:
				ASSERT(0);
				return "";
			}
		}
	}
			break;
		case UIT_HAND:
			if ( !bIsPK )
				pszBoneName = "Item";
			else
				pszBoneName = "R_Weapon";
			break;
		case UIT_BELT_L1:
		case UIT_BELT_R1:
		case UIT_BELT_M1:
		case UIT_BELT_MEDIUM_L1:
		case UIT_BELT_MEDIUM_R1:
		case UIT_BELT_MEDIUM_L2:
		case UIT_BELT_MEDIUM_R2:
		case UIT_WAIST_BELT_L1:
		case UIT_WAIST_BELT_R1:
		case UIT_CAP:
		case UIT_BACKPACK:
		case UIT_PK_LEFT_HAND:
			pszBoneName = pszItemEffectors[type];
			break;
	}
	return pszBoneName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AttachItem( vector<IRenderVisitor::SBoundMesh> *pRes, SRand *pRnd, 
	EUnitItemType type, NDb::CTRndModel *pModel, bool bIsPK, NRPG::IInventoryItem *pItem = 0 )
{
	if ( !pModel )
		return;
	const char *pszBoneName = GetBoneName( type, pItem, bIsPK );
	pRes->push_back( IRenderVisitor::SBoundMesh( pModel->CreateModel( pRnd ), pszBoneName ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GetBoneName( NDb::ESlot slot, NRPG::IUnitMissionInfo *pRPG, bool bUndrawWeapon )
{
	CPtr<NRPG::IInventoryInfo> pInventory = pRPG->GetInventoryInfo();
	NDb::CRPGUniform *pDBUniform = pInventory->GetUniform();
	NRPG::IInventoryItem *pItem = pInventory->Get( slot );
	NRPG::IInventoryItem *pActiveItem = pInventory->GetActive();
	if ( !pItem || !pDBUniform )
		return "";
	if ( pItem->GetDBItem()->subType == NDb::SUBTYPE_HEAVY )
		return GetBoneName( UIT_WEAPON_HEAVY, pItem );
	if ( pItem == pActiveItem && !bUndrawWeapon )
		return GetBoneName( UIT_HAND, pItem );
	for ( int i = 0; i < NDb::N_ITEM_PLACES; ++i )
	{
		if ( pDBUniform->subTypes[i] == pItem->GetDBItem()->subType )
		{
			EUnitItemType uit = (EUnitItemType)nItemPlaces2UnitItemTypes[i];
			return GetBoneName( uit, pItem );
		}
	}
	return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetItemsBindPlaces( vector<IRenderVisitor::SBoundMesh> *pRes, NRPG::IUnitMissionInfo *pRPG,
	bool bUndrawWeapon, NDb::CPanzerklein *pPanzerklein, bool bNoHeavyWeapon, bool bNoCap, bool bNoItems )
{
	pRes->clear();
	SRand rnd;
	bool bIsPK = IsValid( pPanzerklein );
	CPtr<NRPG::IInventoryInfo> pInventory = pRPG->GetInventoryInfo();
	NDb::CRPGUniform *pDBUniform = pInventory->GetUniform();
	// retail @0x21db20: a PK poses in ITS pers uniform (not the wearer's), and a headless PK
	// showing items never wears the cap
	if ( bIsPK )
	{
		pDBUniform = pPanzerklein->pPers->pUniform;
		if ( !bNoItems && pPanzerklein->bHasNoHead )
			bNoCap = true;
	}
	// cap (retail: a live head with a hair model keeps the cap off)
	if ( pDBUniform && !bNoCap )
	{
		NRPG::CUnit *pRPGUnit = pRPG->GetRPGUnit();
		bool bHair = pRPGUnit && IsValid( pRPGUnit->pHeadInfo ) && IsValid( pRPGUnit->pHeadInfo->GetHair() );
		if ( pDBUniform->pCapModel && !bHair )
			AttachItem( pRes, &rnd, UIT_CAP, pDBUniform->pCapModel, bIsPK );
	}
	// retail: a body-mode pose (dialog/portrait) carries at most the cap
	if ( bNoItems )
		return;
	if ( pDBUniform && pDBUniform->pBackpackModel )
		AttachItem( pRes, &rnd, UIT_BACKPACK, pDBUniform->pBackpackModel, bIsPK );
	if ( bIsPK )
	{
		NDb::CRPGItem *pLeftHandItem = pPanzerklein->pLeftHandItem;
		AttachItem( pRes, &rnd, UIT_PK_LEFT_HAND, pLeftHandItem->pModel, bIsPK );
	}
	bool bHeavy = false;
	unordered_map< int, int > slotItems; // slot number or -1 for active weapon
	NRPG::IInventoryItem *pActiveIItem = pInventory->GetActive();
	NDb::CRPGItem *pActiveItem = 0;
	if ( pActiveIItem )
		pActiveItem = pActiveIItem->GetDBItem();
	if ( pActiveItem )
	{
		if ( bIsPK )
		{
			if ( pActiveItem->subType != NDb::SUBTYPE_GRENADE_SMALL && 
						pActiveItem->subType != NDb::SUBTYPE_GRENADE_LARGE )
				AttachItem( pRes, &rnd, UIT_HAND, pActiveItem->pModel, bIsPK, pActiveIItem );
			else
				pActiveItem = 0;
		}
		else
		{
			if ( pActiveItem->subType == NDb::SUBTYPE_HEAVY )
			{
				if ( !bNoHeavyWeapon )
					AttachItem( pRes, &rnd, UIT_WEAPON_HEAVY, pActiveItem->pModel, bIsPK, pActiveIItem );
				bHeavy = true;
			}
			else if ( !bUndrawWeapon )
				AttachItem( pRes, &rnd, UIT_HAND, pActiveItem->pModel, bIsPK, pActiveIItem );
		}
	}
	if ( !pActiveItem && bIsPK )
	{
		for ( int i = 0; i < NDb::N_SLOTS; ++i )
		{
			NRPG::IInventoryItem *pIItem = pInventory->Get( (NDb::ESlot)i );
			if ( !pIItem )
				continue;
			NDb::CRPGItem *pItem = pIItem->GetDBItem();
			if ( pItem->subType != NDb::SUBTYPE_GRENADE_SMALL && pItem->subType != NDb::SUBTYPE_GRENADE_LARGE )
			{
				AttachItem( pRes, &rnd, UIT_HAND, pItem->pModel, bIsPK, pIItem );
				break;
			}
		}
	}
	for ( int i = 0; i < NDb::N_SLOTS; ++i )
	{
		NRPG::IInventoryItem *pIItem = pInventory->Get( (NDb::ESlot)i );
		if ( !pIItem )
			continue;
		NDb::CRPGItem *pItem = pIItem->GetDBItem();
		if ( pInventory->GetActiveSlot() == i )
			continue;
		if ( pItem->subType == NDb::SUBTYPE_HEAVY && !bHeavy )
		{
			if ( !bNoHeavyWeapon )
				AttachItem( pRes, &rnd, UIT_WEAPON_HEAVY, pItem->pModel, bIsPK, pIItem );
			bHeavy = true;
		}
		else
			slotItems[ pItem->subType ] = i;
	}
	if ( pActiveItem && pActiveItem->subType != NDb::SUBTYPE_HEAVY )
		slotItems[ pActiveItem->subType ] = -1;
	if ( !pDBUniform )
		return;
	// do not use any item twice
	vector<char> itemsUsed( pInventory->GetItems().size(), 0 );
	bool slotsUsed[ NDb::N_SLOTS + 1 ] = { false, false, false };

	for ( int i = 0; i < NDb::N_ITEM_PLACES; ++i )
	{
		EUnitItemType uit = (EUnitItemType)nItemPlaces2UnitItemTypes[i];
		NDb::EItemSubType subType = pDBUniform->subTypes[i];
		ASSERT( subType != NDb::SUBTYPE_HEAVY );
		if ( subType == NDb::SUBTYPE_NONE )
		{
			if ( pDBUniform->fixedModels[i] )
				AttachItem( pRes, &rnd, uit, pDBUniform->fixedModels[i], bIsPK );
			continue;
		}
		if ( pActiveItem && pActiveItem->subType == subType )
		{
			if ( bUndrawWeapon )
				AttachItem( pRes, &rnd, uit, pActiveItem->GetItemModel( false, pDBUniform ), bIsPK );
			else if ( subType == NDb::SUBTYPE_PISTOL || subType == NDb::SUBTYPE_KNIFE )
				AttachItem( pRes, &rnd, uit, pActiveItem->GetItemModel( true, pDBUniform ), bIsPK );
			else
			{
				NDb::CRPGItem *pResultItem = 0;
				int nMaxPriority = -1;
				int nResultIt = -1;
				const vector<NRPG::SBackPackItem> &bpItems = pInventory->GetItems();
				for ( int it = 0; it < bpItems.size(); ++it )
				{
					if ( itemsUsed[ it ] )
						continue;
					NDb::CRPGItem *pI = bpItems[it].pItem->GetDBItem();
					if ( pI == pActiveItem )
					{
						if ( slotsUsed[ 0 ] )
							continue;
						slotsUsed[ 0 ] = true;
						pResultItem = pActiveItem;
						break;
					}
					else if ( pI->subType == subType && pI->nSubTypePriority > nMaxPriority )
					{
						pResultItem = pI;
						nResultIt = it;
						nMaxPriority = pI->nSubTypePriority;
					}
				}
				if ( pResultItem )
				{
					if ( nResultIt >= 0 )
						itemsUsed[ nResultIt ] = 1;
					AttachItem( pRes, &rnd, uit, pResultItem->GetItemModel( false, pDBUniform ), bIsPK );
				}
			}
		}
		else
		{
			if ( slotItems.find(subType) != slotItems.end() )
			{
				NDb::CRPGItem *pRPGItem;
				if ( slotItems[subType] >= 0 )
					pRPGItem = pInventory->Get( (NDb::ESlot)slotItems[subType] )->GetDBItem();
				else
					pRPGItem = pActiveItem;
				if ( slotsUsed[ slotItems[subType] + 1 ] )
					continue;
				slotsUsed[ slotItems[subType] + 1 ] = true;
				AttachItem( pRes, &rnd, uit, pRPGItem->GetItemModel( false, pDBUniform ), bIsPK );
			}
			else
			{
				NDb::CRPGItem *pResultItem = 0;
				int nMaxPriority = -1;
				int nResultIt = -1;
				const vector<NRPG::SBackPackItem> &bpItems = pInventory->GetItems();
				for ( int it = 0; it < bpItems.size(); ++it )
				{
					if ( itemsUsed[ it ] )
						continue;
					NDb::CRPGItem *pI = bpItems[it].pItem->GetDBItem();
					if ( pI->subType == subType && pI->nSubTypePriority > nMaxPriority )
					{
						pResultItem = pI;
						nResultIt = it;
						nMaxPriority = pI->nSubTypePriority;
					}
				}
				if ( pResultItem )
				{
					if ( nResultIt >= 0 )
						itemsUsed[ nResultIt ] = 1;
					AttachItem( pRes, &rnd, uit, pResultItem->GetItemModel( false, pDBUniform ), bIsPK );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
