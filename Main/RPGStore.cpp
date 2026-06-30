#include "StdAfx.h"
//
#include "RPGStore.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStore - release vendor stock model bodies. Reconstructed from the matched-release decode
// (oracle: decomp/src/s2_rpgstore.h). The dependency-free surface is wired straight to the REAL
// CItemsMap API (RPGItemMap.cpp), so the forwarders and Take carry no hooks. PlaceItem @0x2b0da0 and
// the Place / UpdateUnitItem / Update path that funnels through it are register-garbled in the release
// decode (the answer key SKIPPED PlaceItem as unrecoverable) and are deferred -- intentionally absent.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
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
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0xb3120140, CStore )
