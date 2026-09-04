#ifndef __RPGSTORE_H_
#define __RPGSTORE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "rpgGlobal.h"     // NRPG::SStoreItem (reused), NRPG::CGlobalPlayer (pSide)
#include "RPGItemMap.h"    // NRPG::CItemsMap, NRPG::SMapItem, CTPoint, NRPG::IInventoryItem
//
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// EStoreFilter - the store's display / weapon-category filter. The eight values and their order are
// IDENTICAL to NUI::CStoreSlot::EFilter (iStorePanel.cpp), so the weapon-type classification the UI
// open-codes lines up with this model. FLT_MAXVALUE (== 8) is the category COUNT: a CStore keeps one
// dirty flag and one CItemsMap placement grid per category, indexed by the active eFilter.
enum EStoreFilter
{
	FLT_OTHERS,
	FLT_RIFLES,
	FLT_PISTOLS,
	FLT_GRENADES,
	FLT_COLDSTEEL,
	FLT_PKWEAPONS,
	FLT_HEAVYWEAPON,
	FLT_SUBMACHINEGUN,
	FLT_MAXVALUE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStore - the non-UI vendor stock model (release class, saveload id 0xb3120140; PDB sizeof 56,
// CObjectBase). It owns the priced stock rows (itemsSet, reusing the existing NRPG::SStoreItem) plus
// one CItemsMap placement grid per filter category (itemsMapsSet). The active filter (eFilter) selects
// which grid the forwarders dispatch to. NUI::CStoreSlot is only a view over the active map in the
// release; it owns no second placement grid. Owned per-player: NRPG::CGlobalPlayer::pStore (save tag 3, retail
// CGlobalPlayer::operator& @0x29cba0), constructed in the side-seeded player ctor path.
//
// Reconstructed from the matched-release decode plus raw v1.1 disassembly. In particular PlaceItem
// @0x2b0da0 was recovered from the raw instructions after the generated decompile lost its register
// provenance; it lays each weapon out with the compatible clip stock beside it.
class CStore: public CObjectBase
{
	OBJECT_BASIC_METHODS( CStore );
	ZDATA
	CPtr<CGlobalPlayer>         pPlayer;       // +0x0c  owning player (CPtr copy AddRefs)
	EStoreFilter               eFilter;        // +0x10  active category
	vector<bool>               flagsSet;       // +0x14  per-category "panel dirty" flags
	vector<SStoreItem>         itemsSet;       // +0x20  priced stock rows (reuses NRPG::SStoreItem)
	vector< CObj<CItemsMap> >  itemsMapsSet;   // +0x2c  one placement grid per category
	// Full release tag table (operator& @0x2b2a90): 2=pPlayer, 3=eFilter, 4=flagsSet
	// (DoDataVector<bool>), 5=itemsSet, 6=itemsMapsSet. flagsSet serializes through the framework's
	// dedicated std::vector<bool> path (BasicChunk1.h), which writes the exact retail
	// DoDataVector<bool> blob layout despite this build's bit-packed vector<bool>.
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pPlayer ); f.Add( 3, &eFilter ); f.Add( 4, &flagsSet ); f.Add( 5, &itemsSet ); f.Add( 6, &itemsMapsSet ); return 0; }

public:
	CStore() {}                                       // default (saveload New / DestroyContents)
	CStore( CGlobalPlayer *pPlayer );                 // @0x2b0990
	// copy ctor (@0x2b2870): compiler-generated memberwise copy is exact -- CPtr/CObj copy AddRefs,
	// the vectors deep-copy, and CObjectBase's own copy ctor resets the refcount (does NOT copy it).

	void SetFilter( EStoreFilter eFilter );           // @0x2b0500
	void GetUpdateFlags( vector<bool> *pFlags );      // @0x2b0600  (copy flags out, then clear them)
	// --- filter-map forwarders: dispatch onto itemsMapsSet[eFilter] (the active grid) ---
	vector<SMapItem>* GetItems();                                          // @0x2b0580 (vtbl +0x2c)
	bool CanPlace( const CTPoint<int> &point, const IInventoryItem *item ); // @0x2b0540 (vtbl +0x18)
	bool FindPlace( const IInventoryItem *item, CTPoint<int> *pPos );      // @0x2b0550 (vtbl +0x1c)
	void SetSize( int newX, int newY );                                   // @0x2b0560 (vtbl +0x24)
	CTPoint<int> GetSize();                                               // @0x2b0570 (vtbl +0x28)
	void Take( IInventoryItem *item );                                    // @0x2b0630 (vtbl +0x10)
	bool Place( const CTPoint<int> &point, IInventoryItem *item );          // @0x2b1060
	void Update( int nLevel, const vector< CObj<CUnit> > &units );          // @0x2b1350
	bool HasMappedItems() const;                                           // live-view initialization probe
	// Direct stock-row access is retained for diagnostics/save migration; normal store flow goes
	// through Update/Place/Take and the category maps.
	vector<SStoreItem>& ItemsSet() { return itemsSet; }

private:
	void PlaceItem( SStoreItem *pRow, bool bWeapon, bool bForce, const CTPoint<int> &point ); // @0x2b0da0
	void UpdateUnitItem( IInventoryItem *pItem );                              // @0x2b1200
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __RPGSTORE_H_
