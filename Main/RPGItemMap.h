#ifndef __RPGITEMMAP_H_
#define __RPGITEMMAP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "..\Misc\Geom.h"        // CTPoint
#include "..\Misc\2DArray.h"     // CArray2D
#include "RPGItemInfo.h"         // NRPG::IInventoryItem (GetSize)
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SMapItem - one stamped placement: an item plus the grid cell it was placed at. Layout-identical to
// the dev SBackPackItem (CTPoint<int> + CObj<IInventoryItem>), but the release CItemsMap carries its
// own distinct record type (PDB sizeof 12).
struct SMapItem
{
	ZDATA
	CTPoint<int>          sPos;     // top-left cell
	CObj<IInventoryItem>  pItem;    // the placed item (O-ref)
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &sPos ); f.Add( 3, &pItem ); return 0; }
	SMapItem() {}
	SMapItem( const CTPoint<int> &_sPos, IInventoryItem *_pItem ): sPos( _sPos ), pItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemsMap - the RPG inventory placement grid (saveload id 0xb3120130). A 2-D byte occupancy model
// behind an inventory window / backpack: a `placeMap` whose cells are 0 (free) or 1 (covered), plus
// `itemsSet`, the {cell, item} records stamped onto it. Items occupy an axis-aligned w*h footprint
// read from the item via IInventoryItem::GetSize() (.x = X-extent / columns, .y = Y-extent / rows;
// cells addressed row-major placeMap[row=y][col=x]).
//
// Release class, reconstructed from the matched-release decode (oracle: decomp/src/s2_itemsmap.h
// -- CanPlace @0x29eff0, SetSize @0x29f170, Clear @0x29f3e0, Place @0x29f480, Take @0x29f2b0, GetSize
// @0x29f150, GetItems @0x29f7a0, ctor() @0x29f740, ctor(x,y,dyn) @0x29f350, ctor(const&) @0x29f930,
// FindPlace @0x29f090). The release factored this grid model out of the dev CInventory (RPGInventory.cpp),
// which still does slot+backpackMap placement inline; CItemsMap is the standalone release form. It
// compiles + registers here; rewiring CInventory to USE it is a follow-up.
//
// The decode reached the CArray2D storage directly (pData/nXSize/Create); here the methods go through
// the real CArray2D<char> public API (operator[]/GetXSize/GetYSize/SetSizes/FillZero), which is
// layout- and behaviour-identical. The decode's one opaque seam -- the (-1,-1) "auto-find" virtual
// FindPlace -- is reconstructed in full (same first-fit scan as CInventory::FindPlace), so Place is
// self-contained with no hooks.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemsMap: public CObjectBase
{
	OBJECT_BASIC_METHODS( CItemsMap );   // New / Duplicate / MakeCopy(== the release copy ctor @0x29f930) / DestroyContents
	ZDATA
	bool             bDynamicSize;       // +0x0c
	CArray2D<char>   placeMap;           // +0x10  (data/pData/nXSize/nYSize)
	vector<SMapItem> itemsSet;           // +0x20  (placement records)
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &bDynamicSize ); f.Add( 3, &placeMap ); f.Add( 4, &itemsSet ); return 0; }

public:
	CItemsMap();                                            // @0x29f740 (1x1 grid, NOT zero-filled)
	CItemsMap( int x, int y, bool dynamic );                // @0x29f350 (x*y grid, zero-filled)
	// copy ctor (@0x29f930): compiler-generated memberwise copy deep-clones placeMap + itemsSet exactly.

	bool CanPlace( const CTPoint<int> &point, const IInventoryItem *item ) const;   // @0x29eff0
	void SetSize( int newX, int newY );                                            // @0x29f170
	void Clear( int newX, int newY );                                              // @0x29f3e0
	bool Place( CTPoint<int> point, IInventoryItem *item );                        // @0x29f480
	void Take( IInventoryItem *item );                                             // @0x29f2b0
	bool FindPlace( const IInventoryItem *item, CTPoint<int> *pPos ) const;        // @0x29f090
	CTPoint<int> GetSize() const;                                                  // @0x29f150
	const vector<SMapItem>& GetItems() const { return itemsSet; }                  // @0x29f7a0
	vector<SMapItem>& GetItems() { return itemsSet; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __RPGITEMMAP_H_
