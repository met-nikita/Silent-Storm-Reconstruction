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
// release factored out (additive -- nothing references it yet).
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
	// flagsSet (release tag 4) is INTENTIONALLY not serialized: with this build's std::vector<bool>
	// (bit-packed) the saveload framework's DoVector takes &data[i], which is ill-formed for the bit
	// proxy and would not compile. flagsSet is a transient dirty-flag cache, so dropping it from the
	// save image is behaviour-neutral for this additive (never-saved-yet) class.
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pPlayer ); f.Add( 3, &eFilter ); f.Add( 5, &itemsSet ); f.Add( 6, &itemsMapsSet ); return 0; }

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
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __RPGSTORE_H_
