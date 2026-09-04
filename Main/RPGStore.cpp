#include "StdAfx.h"
//
#include "RPGStore.h"
#include "RPGItem.h"
#include "RPGUnit.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStore - release vendor stock model bodies. Reconstructed from the v1.1 raw disassembly.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
namespace
{
////////////////////////////////////////////////////////////////////////////////////////////////////
EStoreFilter WeaponFilter( NDb::CRPGWeaponType *pType )
{
	if ( !IsValid( pType ) )
		return FLT_OTHERS;

	switch ( pType->eStoreWeaponType )
	{
	case NDb::SWT_PISTOL:          return FLT_PISTOLS;
	case NDb::SWT_RIFLE:           return FLT_RIFLES;
	case NDb::SWT_SUB_MACHINE_GUN: return FLT_SUBMACHINEGUN;
	case NDb::SWT_HEAVY_WEAPON:    return FLT_HEAVYWEAPON;
	case NDb::SWT_COLD_STEEL:      return FLT_COLDSTEEL;
	case NDb::SWT_GRENADE:         return FLT_GRENADES;
	case NDb::SWT_PK_WEAPON:       return FLT_PKWEAPONS;
	default:                       return FLT_OTHERS;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail builds this association from every firearm's pInnerClip->pItem. Several guns can use the
// same clip, so prefer the category which is already selected and otherwise retain the first match.
EStoreFilter ClipFilter( NDb::CRPGItem *pItem, EStoreFilter eCurrent, EStoreFilter eDefault )
{
	EStoreFilter eResult = eDefault;
	bool bFound = false;
	CDBTable<NDb::CRPGWeapon> *pTable = NDatabase::GetTable<NDb::CRPGWeapon>();
	CDBIterator<NDb::CRPGWeapon> it( *pTable );
	while ( it.MoveNext() )
	{
		NDb::CRPGWeapon *pWeapon = it.Get();
		if ( !IsValid( pWeapon ) || !IsValid( pWeapon->pInnerClip ) ||
			!IsValid( pWeapon->pInnerClip->pItem ) || pWeapon->pInnerClip->pItem.GetPtr() != pItem )
			continue;

		EStoreFilter eCandidate = WeaponFilter( pWeapon->pWeaponType );
		if ( eCandidate == eCurrent )
			return eCandidate;
		if ( !bFound )
		{
			eResult = eCandidate;
			bFound = true;
		}
	}
	return eResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EStoreFilter GetItemFilter( NDb::CRPGItem *pItem, EStoreFilter eCurrent )
{
	if ( !IsValid( pItem ) || !IsValid( pItem->pSuccessor ) )
		return FLT_OTHERS;

	if ( NDb::CRPGWeapon *pWeapon = dynamic_cast<NDb::CRPGWeapon*>( pItem->pSuccessor.GetPtr() ) )
		return WeaponFilter( pWeapon->pWeaponType );
	if ( NDb::CRPGClip *pClip = dynamic_cast<NDb::CRPGClip*>( pItem->pSuccessor.GetPtr() ) )
		return ClipFilter( pClip->pItem, eCurrent, FLT_OTHERS );
	if ( NDb::CRPGGrenade *pGrenade = dynamic_cast<NDb::CRPGGrenade*>( pItem->pSuccessor.GetPtr() ) )
		return ClipFilter( pGrenade->pItem, eCurrent, FLT_GRENADES );
	if ( dynamic_cast<NDb::CRPGMeleeWeapon*>( pItem->pSuccessor.GetPtr() ) )
		return FLT_COLDSTEEL;
	return FLT_OTHERS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsItemBelongToSide( NDb::CRPGItem *pItem, NDb::CSide *pSide )
{
	// Retail treats an item with no CRPGStoreItem side restrictions as universal.
	if ( !IsValid( pSide ) )
		return true;
	bool bRestricted = false;
	CDBTable<NDb::CRPGStoreItem> *pTable = NDatabase::GetTable<NDb::CRPGStoreItem>();
	CDBIterator<NDb::CRPGStoreItem> it( *pTable );
	while ( it.MoveNext() )
	{
		NDb::CRPGStoreItem *pStoreItem = it.Get();
		if ( !IsValid( pStoreItem ) || pStoreItem->pItem.GetPtr() != pItem )
			continue;
		bRestricted = true;
		if ( pStoreItem->pSide.GetPtr() == pSide )
			return true;
	}
	return !bRestricted;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void TopUpRow( SStoreItem *pRow, int nQuantity )
{
	if ( !pRow || !IsValid( pRow->pRPGItem ) || !IsValid( pRow->pRPGItem->pSuccessor ) )
		return;
	for ( int nHave = (int)pRow->itemsList.size(); nHave < nQuantity; ++nHave )
		pRow->itemsList.push_back( CreateItem( pRow->pRPGItem->pSuccessor ) );
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0990: primary ctor. eFilter = FLT_OTHERS; one dirty flag (false) per category; empty stock;
// one fresh 10x13 dynamic-size CItemsMap per category. pPlayer is AddRef'd by the CPtr copy.
CStore::CStore( CGlobalPlayer *pPlayer ): pPlayer( pPlayer ), eFilter( FLT_OTHERS )
{
	flagsSet.resize( FLT_MAXVALUE, false );   // 8 dirty flags, all clear
	// itemsSet: left empty (no stock until Update).
	for ( int i = 0; i < FLT_MAXVALUE; ++i )
		itemsMapsSet.push_back( CObj<CItemsMap>( new CItemsMap( 10, 13, true ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0500: select the active category / items-map.
void CStore::SetFilter( EStoreFilter eFilter )
{
	this->eFilter = eFilter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0600: hand the caller the current per-category dirty flags, then reset them all to false.
void CStore::GetUpdateFlags( vector<bool> *pFlags )
{
	*pFlags = flagsSet;
	for ( int i = 0; i < (int)flagsSet.size(); ++i )
		flagsSet[ i ] = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0580: forward to the active grid (release vtbl +0x2c).
vector<SMapItem>* CStore::GetItems()
{
	return &itemsMapsSet[ eFilter ]->GetItems();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0540: forward to the active grid (release vtbl +0x18).
bool CStore::CanPlace( const CTPoint<int> &point, const IInventoryItem *item )
{
	return itemsMapsSet[ eFilter ]->CanPlace( point, item );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0550: forward to the active grid (release vtbl +0x1c).
bool CStore::FindPlace( const IInventoryItem *item, CTPoint<int> *pPos )
{
	return itemsMapsSet[ eFilter ]->FindPlace( item, pPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0560: forward to the active grid (release vtbl +0x24).
void CStore::SetSize( int newX, int newY )
{
	itemsMapsSet[ eFilter ]->SetSize( newX, newY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0570: forward to the active grid (release vtbl +0x28). The release returns CTPoint<int> by
// value (the decomp's out-param is the hidden struct-return pointer).
CTPoint<int> CStore::GetSize()
{
	return itemsMapsSet[ eFilter ]->GetSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0630: remove one unit of `item`.
//   1. ALWAYS first notify the active grid (vtbl +0x10 == CItemsMap::Take) -- this fires even when no
//      row holds the item and even when itemsSet is empty;
//   2. scan the stock rows in order; on the FIRST row whose item-list holds `item`, decrement
//      fQuantity by 1, erase that single node (the CObj releases the held ref) and return.
void CStore::Take( IInventoryItem *item )
{
	itemsMapsSet[ eFilter ]->Take( item );

	for ( vector<SStoreItem>::iterator row = itemsSet.begin(); row != itemsSet.end(); ++row )
	{
		for ( list<CObj<IInventoryItem> >::iterator it = row->itemsList.begin(); it != row->itemsList.end(); ++it )
		{
			if ( it->GetPtr() == item )
			{
				row->fQuantity -= 1.0f;
				row->itemsList.erase( it );
				return;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b0da0. Stock firearms form one row each. The weapon occupies the left side and freshly-created
// compatible clips fill the unused cells to its right. Equipped squad weapons use bForce=true, which
// suppresses the weapon itself and contributes only its ammunition to the vendor map.
void CStore::PlaceItem( SStoreItem *pRow, bool bWeapon, bool bForce, const CTPoint<int> &point )
{
	if ( !pRow || !IsValid( pRow->pRPGItem ) )
		return;

	EStoreFilter eItemFilter = GetItemFilter( pRow->pRPGItem, eFilter );
	CItemsMap *pMap = itemsMapsSet[ eItemFilter ];
	if ( !IsValid( pMap ) )
		return;

	int nBottom = 0;
	const vector<SMapItem> &placed = pMap->GetItems();
	for ( int i = 0; i < (int)placed.size(); ++i )
		nBottom = Max( nBottom, placed[i].sPos.y + placed[i].pItem->GetSize().y );

	NDb::CRPGWeapon *pWeapon = IsValid( pRow->pRPGItem->pSuccessor )
		? dynamic_cast<NDb::CRPGWeapon*>( pRow->pRPGItem->pSuccessor.GetPtr() ) : 0;
	for ( list<CObj<IInventoryItem> >::iterator it = pRow->itemsList.begin(); it != pRow->itemsList.end(); ++it )
	{
		IInventoryItem *pItem = it->GetPtr();
		if ( !IsValid( pItem ) )
			continue;

		if ( bWeapon && IsValid( pWeapon ) && IsValid( pWeapon->pInnerClip ) &&
			IsValid( pWeapon->pInnerClip->pItem ) )
		{
			const CTPoint<int> &sWeaponSize = pItem->GetSize();
			CTPoint<int> sMapSize = pMap->GetSize();
			pMap->SetSize( sMapSize.x, Max( sMapSize.y, nBottom + sWeaponSize.y ) );

			int nClipX = 0;
			if ( !bForce )
			{
				pMap->Place( CTPoint<int>( 0, nBottom ), pItem );
				nClipX = sWeaponSize.x;
			}

			NDb::CSide *pSide = IsValid( pPlayer ) ? pPlayer->pSide.GetPtr() : 0;
			NDb::CRPGItem *pClipItem = pWeapon->pInnerClip->pItem;
			const CTPoint<int> &sClipSize = pClipItem->sSize;
			if ( IsItemBelongToSide( pRow->pRPGItem, pSide ) && sClipSize.x > 0 && sClipSize.y > 0 )
			{
				const int nAcross = ( sMapSize.x - nClipX ) / sClipSize.x;
				const int nDown = sWeaponSize.y / sClipSize.y;
				for ( int y = 0; y < nDown; ++y )
					for ( int x = 0; x < nAcross; ++x )
					{
						CObj<IInventoryItem> pClip = CreateItem( pClipItem->pSuccessor );
						if ( IsValid( pClip ) )
							pMap->Place( CTPoint<int>( nClipX + x * sClipSize.x,
								nBottom + y * sClipSize.y ), pClip );
					}
			}
			nBottom += sWeaponSize.y;
		}
		else
		{
			CTPoint<int> sPos( point );
			if ( sPos.x == -1 || sPos.y == -1 )
				sPos = CTPoint<int>( -1, -1 );
			pMap->Place( sPos, pItem );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b1060: add a sold/dropped item as a non-regenerating row and place it in its category.
bool CStore::Place( const CTPoint<int> &point, IInventoryItem *item )
{
	if ( !IsValid( item ) )
		return true;

	SStoreItem row;
	row.nRating = 0;
	row.eType = SStoreItem::NONE;
	row.fQuantity = 1.0f;
	row.pRPGItem = item->GetDBItem();
	row.itemsList.push_back( item );
	itemsSet.push_back( row );

	EStoreFilter eItemFilter = GetItemFilter( row.pRPGItem, eFilter );
	CTPoint<int> sPos( point );
	if ( eItemFilter != eFilter || !itemsMapsSet[eFilter]->CanPlace( sPos, item ) )
		sPos = CTPoint<int>( -1, -1 );
	if ( eItemFilter != eFilter )
		flagsSet[eItemFilter] = true;
	PlaceItem( &itemsSet.back(), false, false, sPos );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b1200: an equipped weapon contributes compatible ammunition but is not itself sold.
void CStore::UpdateUnitItem( IInventoryItem *item )
{
	if ( !IsValid( item ) || !IsValid( item->GetDBItem() ) )
		return;
	NDb::CSide *pSide = IsValid( pPlayer ) ? pPlayer->pSide.GetPtr() : 0;
	if ( !IsItemBelongToSide( item->GetDBItem(), pSide ) )
		return;

	SStoreItem row;
	row.nRating = 0;
	row.eType = SStoreItem::NONE;
	row.fQuantity = 1.0f;
	row.pRPGItem = item->GetDBItem();
	row.itemsList.push_back( item );
	PlaceItem( &row, true, true, CTPoint<int>( -1, -1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x2b1350: unlock stock by campaign rating, regenerate only when that rating increases, rebuild
// all category maps, then add ammunition for the squad's two equipped weapons.
void CStore::Update( int nLevel, const vector< CObj<CUnit> > &units )
{
	// One-time migration of the predecessor's synthetic ammo rows. Retail ammo lives only in maps.
	for ( int i = 0; i < (int)itemsSet.size(); )
	{
		SStoreItem &row = itemsSet[i];
		if ( row.eType == SStoreItem::CONST_QUANTITY && row.nRating == -1 &&
			IsValid( row.pStoreItem ) && row.pStoreItem->pItem.GetPtr() != row.pRPGItem.GetPtr() )
			itemsSet.erase( itemsSet.begin() + i );
		else
			++i;
	}

	CDBTable<NDb::CRPGStoreItem> *pTable = NDatabase::GetTable<NDb::CRPGStoreItem>();
	CDBIterator<NDb::CRPGStoreItem> it( *pTable );
	while ( it.MoveNext() )
	{
		NDb::CRPGStoreItem *pStoreItem = it.Get();
		if ( !IsValid( pStoreItem ) || !IsValid( pStoreItem->pItem ) ||
			!IsValid( pStoreItem->pItem->pSuccessor ) )
			continue;
		if ( pStoreItem->pSide.GetPtr() != ( IsValid( pPlayer ) ? pPlayer->pSide.GetPtr() : 0 ) )
			continue;
		if ( nLevel < pStoreItem->nRating )
			continue;

		bool bFound = false;
		for ( int i = 0; i < (int)itemsSet.size(); ++i )
			if ( itemsSet[i].pStoreItem.GetPtr() == pStoreItem )
			{
				bFound = true;
				break;
			}
		if ( bFound )
			continue;

		SStoreItem row;
		row.eType = SStoreItem::REGEN_QUANTITY;
		row.nRating = pStoreItem->nRating;
		row.fQuantity = Max( 1.0f, pStoreItem->fQuantity );
		row.pRPGItem = pStoreItem->pItem;
		row.pStoreItem = pStoreItem;
		// Retail temporarily selects truncation for the float-to-int conversion here.
		TopUpRow( &row, int( row.fQuantity ) );
		itemsSet.push_back( row );
	}

	for ( int i = 0; i < (int)itemsSet.size(); ++i )
	{
		SStoreItem &row = itemsSet[i];
		if ( !IsValid( row.pStoreItem ) )
			continue;
		if ( row.eType == SStoreItem::REGEN_QUANTITY && row.nRating < nLevel )
		{
			row.fQuantity += float( nLevel - row.nRating ) * row.pStoreItem->fQuantity;
			row.nRating = nLevel;
		}
		if ( row.eType == SStoreItem::CONST_QUANTITY || row.eType == SStoreItem::REGEN_QUANTITY )
			TopUpRow( &row, int( row.fQuantity ) );
	}

	for ( int i = 0; i < (int)itemsMapsSet.size(); ++i )
		itemsMapsSet[i]->Clear( 10, 13 );
	for ( int i = 0; i < (int)itemsSet.size(); ++i )
		PlaceItem( &itemsSet[i], true, false, CTPoint<int>( -1, -1 ) );

	for ( int i = 0; i < (int)units.size(); ++i )
	{
		CUnit *pUnit = units[i];
		if ( !IsValid( pUnit ) || !IsValid( pUnit->GetInventory() ) )
			continue;
		for ( int nSlot = NDb::SLOT_1; nSlot <= NDb::SLOT_2; ++nSlot )
		{
			// Retail RTTI-filters both hand slots to IWeaponItem before UpdateUnitItem;
			// non-weapons must not contribute themselves to the store map.
			CDynamicCast<IWeaponItem> pWeapon( pUnit->GetInventory()->Get( (NDb::ESlot)nSlot ) );
			if ( IsValid( pWeapon ) )
				UpdateUnitItem( pWeapon.GetPtr() );
		}
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStore::HasMappedItems() const
{
	for ( int i = 0; i < (int)itemsMapsSet.size(); ++i )
		if ( IsValid( itemsMapsSet[i] ) && !itemsMapsSet[i]->GetItems().empty() )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0xb3120140, CStore )
