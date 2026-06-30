#include "StdAfx.h"
//
#include "RPGItemMap.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemsMap - release inventory placement-grid bodies. Reconstructed from the matched-release decode
// (decomp/src/s2_itemsmap.h). The grid logic is faithful to the disassembly; the item footprint
// is read live from IInventoryItem::GetSize() (the engine's vtable accessor the decode externalized),
// and the CArray2D<char> storage is touched through its real public API. Every ORIGINAL behaviour is
// preserved (notably Place performs NO bounds check -- callers validate with CanPlace first).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f740: default ctor -- a minimal 1x1 grid, empty item set. NB: the single cell is NOT
// zero-filled (the decode omits the fill loop the sized ctor runs); reproduced faithfully.
CItemsMap::CItemsMap(): bDynamicSize( false ), placeMap( 1, 1 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f350: sized ctor -- x*y grid, zero-filled, empty item set.
CItemsMap::CItemsMap( int x, int y, bool dynamic ): bDynamicSize( dynamic ), placeMap( x, y )
{
	placeMap.FillZero();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29eff0: const -- true iff the item's footprint fits at `point` and every covered cell is free.
// 1. reject a negative origin; 2. inclusive bounds fit (w + x <= nXSize, h + y <= nYSize);
// 3. scan the covered w*h block, any non-zero cell => not placeable. A non-positive extent scans
// nothing and is trivially placeable.
bool CItemsMap::CanPlace( const CTPoint<int> &point, const IInventoryItem *item ) const
{
	if ( point.x < 0 || point.y < 0 )
		return false;

	const CTPoint<int> &sz = item->GetSize();
	if ( sz.x + point.x > placeMap.GetXSize() )
		return false;
	if ( sz.y + point.y > placeMap.GetYSize() )
		return false;

	for ( int r = 0; r < sz.y; ++r )
		for ( int c = 0; c < sz.x; ++c )
			if ( placeMap[point.y + r][point.x + c] != 0 )
				return false;   // a covered cell is already occupied
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f170: resize the grid to (newX,newY), PRESERVING the overlapping top-left sub-rectangle and
// zero-filling the rest. Even an identical size still snapshots/zeroes/copies (a content no-op,
// faithfully reproduced). SetSizes reallocates only when the size actually differs; FillZero clears
// the whole (possibly new) buffer; then the overlap is copied back from the snapshot.
void CItemsMap::SetSize( int newX, int newY )
{
	CArray2D<char> old( placeMap );        // snapshot current contents (deep copy)
	placeMap.SetSizes( newX, newY );
	placeMap.FillZero();
	int rows = ( newY < old.GetYSize() ) ? newY : old.GetYSize();
	int cols = ( newX < old.GetXSize() ) ? newX : old.GetXSize();
	for ( int y = 0; y < rows; ++y )
		for ( int x = 0; x < cols; ++x )
			placeMap[y][x] = old[y][x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f3e0: drop every placed item and (re)size + zero-fill the grid. Unlike SetSize, NO contents
// are preserved. Swapping in an empty vector destroys every record (each per-element CObj releases
// its held item) and frees the buffer; SetSizes reallocates only if the size differs.
void CItemsMap::Clear( int newX, int newY )
{
	{ vector<SMapItem> empty; itemsSet.swap( empty ); }
	placeMap.SetSizes( newX, newY );
	placeMap.FillZero();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f480: stamp the item's w*h footprint occupied at `point` and record the placement.
// 1. a (-1,-1) point auto-finds a free cell via FindPlace; if none, return false untouched;
// 2. stamp every covered cell occupied (non-positive extent stamps nothing);
// 3. append a {point,item} record (the CObj AddRefs the new item). NB: NO bounds check before
// stamping -- callers validate with CanPlace first (faithful; out-of-range would overrun the grid).
bool CItemsMap::Place( CTPoint<int> point, IInventoryItem *item )
{
	if ( point.x == -1 && point.y == -1 )
	{
		if ( !FindPlace( item, &point ) )
			return false;
	}

	const CTPoint<int> &sz = item->GetSize();
	for ( int r = 0; r < sz.y; ++r )
		for ( int c = 0; c < sz.x; ++c )
			placeMap[point.y + r][point.x + c] = (char)1;

	itemsSet.push_back( SMapItem( point, item ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f2b0: remove `item` from the grid -- find its record, un-stamp its w*h footprint back to free,
// then erase the record. The footprint is the item's CURRENT GetSize() (read live, not the one
// captured at Place time). If no record matches, no-op (the dev ASSERTed; the release simply returns).
void CItemsMap::Take( IInventoryItem *item )
{
	for ( vector<SMapItem>::iterator it = itemsSet.begin(); it != itemsSet.end(); ++it )
	{
		if ( it->pItem.GetPtr() != item )
			continue;

		const CTPoint<int> &sz = item->GetSize();
		int baseY = it->sPos.y, baseX = it->sPos.x;
		for ( int r = 0; r < sz.y; ++r )
			for ( int c = 0; c < sz.x; ++c )
				placeMap[baseY + r][baseX + c] = 0;

		itemsSet.erase( it );   // the erased record's CObj releases the item
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f090: first-fit auto-placement -- scan the grid row-major for the first cell the item fits at,
// write it to *pPos and return true; false if nowhere fits. (Same scan as CInventory::FindPlace; this
// is the sibling virtual the decode left as an opaque seam, reconstructed in full.)
bool CItemsMap::FindPlace( const IInventoryItem *item, CTPoint<int> *pPos ) const
{
	for ( int y = 0; y < placeMap.GetYSize(); ++y )
		for ( int x = 0; x < placeMap.GetXSize(); ++x )
		{
			CTPoint<int> p( x, y );
			if ( CanPlace( p, item ) )
			{
				*pPos = p;
				return true;
			}
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x29f150: grid dimensions as a CTPoint (x = nXSize cols, y = nYSize rows).
CTPoint<int> CItemsMap::GetSize() const
{
	return CTPoint<int>( placeMap.GetXSize(), placeMap.GetYSize() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0xb3120130, CItemsMap )
