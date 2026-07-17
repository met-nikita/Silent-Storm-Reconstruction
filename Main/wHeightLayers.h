#ifndef __WHEIGHTLAYERS_H_
#define __WHEIGHTLAYERS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// wHeightLayers -- NWorld::CHeightLayers, the per-floor terrain-height cache (retail-new world module,
// absent from the dev tree -- which replaced it -- reconstructed here as an additive parity shell).
// Each "floor" (an int key) owns an SHLayer holding a CArray2D<float> of heights; the class keeps a
// hash of those layers plus a desired->real floor remap, all seeded from one shared terrainLayer.
// Reconstructed against the matched-release decode (oracle: decomp/src/s2_heightlayers.h +
// s2_nworld_wheightlayers.h) over the real dev CArray2D / STerrainInfo / CFuncBase types.
//
// Owned by CWorld (+0x1a0, save tag 44); built by CreateRandom and sampled by CCamera::Update for the
// focus-height easing + eye lift-off. ComputeLayers is FULLY ported, step 3 included.
//
// PDB layout (x86): SHLayer 16 { CArray2D<float> heights }; IHeightLayers 12 (: CObjectBase, no data);
// CHeightLayers 72 (terrainLayer@12, layers@28, desired2realFloor@48, bHasTerrain@68); saveload id
// 0xa2313130. Vtable slot order is retail-exact (walked from the CHeightLayers vftable VA 0x8ca118).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\2DArray.h"   // CArray2D<float> -- complete type (SHLayer member)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainInfo;
template <class TResult> class CFuncBase;
class CStructureSaver;
namespace NAI { class IPathNetwork; }
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SHLayer -- one floor's height field. PDB: sizeof 16 { CArray2D<float> heights @0 }.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHLayer
{
	ZDATA
	CArray2D<float> heights;
	ZEND int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IHeightLayers -- the public face CreateHeightLayers returns. PDB: sizeof 12 (: CObjectBase, no own
// data). Pure-virtual read interface, implemented by the concrete CHeightLayers below.
////////////////////////////////////////////////////////////////////////////////////////////////////
class IHeightLayers: public CObjectBase
{
public:
	virtual SHLayer* GetLayer( int nFloor ) = 0;   // vtbl +0x10 @0x35d630
	// vtbl +0x14 @0x3726a0. ICF-folded to `lea eax,[ecx+0xc]; ret` (= &terrainLayer), so the PDB
	// misnames the slot CPlayerBase<>::GetPlayerUnits. 0-arg -- NOT a GetLayer overload.
	virtual SHLayer* GetTerrainLayer() = 0;        // vtbl +0x14 @0x3726a0
	virtual bool HasTerrain() const = 0;           // vtbl +0x18 @0x35e290
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers -- the per-floor height cache. Saveload-registered (id 0xa2313130).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeightLayers: public IHeightLayers
{
	OBJECT_BASIC_METHODS( CHeightLayers );
	ZDATA
	SHLayer terrainLayer;
	unordered_map<int, SHLayer> layers;
	unordered_map<int, int> desired2realFloor;
	bool bHasTerrain;
	ZEND int operator&( CStructureSaver &f );
public:
	CHeightLayers(): bHasTerrain( true ) {}
	//
	virtual SHLayer* GetLayer( int nFloor );
	virtual SHLayer* GetTerrainLayer() { return &terrainLayer; }
	virtual bool HasTerrain() const { return bHasTerrain; }
	// retail keeps these OFF the vtable (7 slots end at +0x18); ComputeLayers + GetLayer call them direct
	int GetRealFloor( int nFloor );                                        // @0x35d4f0
	SHLayer* GetCreateLayer( int nFloor );                                 // @0x35d670
	//
	void ComputeLayers( int nXCells, int nYCells, CFuncBase<STerrainInfo>* pTerrain, NAI::IPathNetwork* pPathNet );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateHeightLayers @0x35de90 -- the module's only public entry: build a height-layer cache and hand
// it back as IHeightLayers*.
////////////////////////////////////////////////////////////////////////////////////////////////////
IHeightLayers* CreateHeightLayers( int nXCells, int nYCells, CFuncBase<STerrainInfo>* pTerrain, NAI::IPathNetwork* pPathNet );
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WHEIGHTLAYERS_H_
