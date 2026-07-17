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
// which grid the forwarders dispatch to. This is DISTINCT from the UI's NUI::CStoreSlot
// (iStorePanel.cpp), which open-codes its own EFilter + placeMap; CStore is the standalone model the
// release factored out. Owned per-player: NRPG::CGlobalPlayer::pStore (save tag 3, retail
// CGlobalPlayer::operator& @0x29cba0), constructed in the side-seeded player ctor path.
//
// Reconstructed from the matched-release decode (oracle: decomp/src/s2_rpgstore.h). This landing
// covers the dependency-free surface, wired straight to the REAL CItemsMap API (no hooks): the ctor,
// the filter setter, the read-and-clear dirty-flag accessor, the five CItemsMap forwarders, and Take.
// DEFERRED (not present here): PlaceItem @0x2b0da0 is register-garbled / unrecoverable in the release
// decode (the answer key SKIPPED it), and Place / UpdateUnitItem / Update all funnel through it, so
// they are intentionally omitted until PlaceItem can be re-derived. Likewise the NRPG item-filter
// classification free fns (GetItemFilter / GetItemFiltersSet / IsItemBelongToSide) are a follow-up.
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
	// direct stock-row access for the CPlayer store flow (retail funnels stock maintenance through
	// CStore::Update/PlaceItem -- @0x2b0da0 is register-garbled/unrecovered in the oracle, so the
	// dev regen logic operates on the rows directly until PlaceItem can be re-derived).
	vector<SStoreItem>& ItemsSet() { return itemsSet; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __RPGSTORE_H_
