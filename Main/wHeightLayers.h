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
// SCOPE OF THIS SHELL:
//   * The accessors (GetLayer/GetRealFloor/GetCreateLayer/HasTerrain), the ctor, and ComputeLayers
//     steps 1-2 (the shared terrainLayer data product) are faithful.
//   * ComputeLayers step 3 -- the path-network wall rasterization + per-floor beta-spline smoothing --
//     reads a release NAI::CPathNetwork passage/geometry/cell substructure that the dev predecessor
//     NAI::CPathNetwork (vector<CObj<CNodesLayer>> layers/groups, no passages) does not carry. It is a
//     DEFERRED, documented no-op: no per-floor layers are populated, so GetLayer/GetRealFloor degrade
//     to the shared terrainLayer. Behaviour-neutral and call-path-safe -- nothing in the dev tree
//     calls CreateHeightLayers.
//
// PDB layout (x86): SHLayer 16 { CArray2D<float> heights }; IHeightLayers 12 (: CObjectBase, no data);
// CHeightLayers 72 (terrainLayer@12, layers@28, desired2realFloor@48, bHasTerrain@68); saveload id
// 0xa2313130. IHeightLayers has no dev consumer, so its vtable slot order is functional, not byte-exact.
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
	virtual SHLayer* GetLayer( int nFloor ) = 0;
	virtual int GetRealFloor( int nFloor ) = 0;
	virtual SHLayer* GetCreateLayer( int nFloor ) = 0;
	virtual bool HasTerrain() const = 0;
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
	virtual int GetRealFloor( int nFloor );
	virtual SHLayer* GetCreateLayer( int nFloor );
	virtual bool HasTerrain() const { return bHasTerrain; }
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
