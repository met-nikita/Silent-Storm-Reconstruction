#include "StdAfx.h"
#include "wHeightLayers.h"
#include "TerrainInfo.h"   // STerrainInfo (heightMap) + CFuncBase<STerrainInfo>::GetValue
#include "Grid.h"          // FP_TERRAIN_H_SCALE (1.0f/64.0f)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SHLayer::operator& -- one CArray2D<float> chunk (Do2DArrayData raw block for the POD float).
////////////////////////////////////////////////////////////////////////////////////////////////////
int SHLayer::operator&( CStructureSaver &f )
{
	f.Add( 2, &heights );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers::operator& -- terrainLayer + the two int-keyed floor hashes + the terrain flag.
// (operator& was not in the lifted release set, so the retail chunk tags are unrecoverable; the class
// is retail-new and absent from the dev game.db, so sequential tags are authored -- save/load is
// symmetric, and no live dev structure stores a CHeightLayers, so this registrar is inert until
// exercised.)
////////////////////////////////////////////////////////////////////////////////////////////////////
int CHeightLayers::operator&( CStructureSaver &f )
{
	f.Add( 2, &terrainLayer );
	f.Add( 3, &layers );
	f.Add( 4, &desired2realFloor );
	f.Add( 5, &bHasTerrain );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers::GetRealFloor @0x35d4f0 -- resolve a desired floor to a real (existing) one and cache.
// Cached in desired2realFloor -> mapped real; else the largest layer key <= nFloor, baselined at the
// first layer key in iteration order (faithful to the binary's bucket-order walk), then cached.
////////////////////////////////////////////////////////////////////////////////////////////////////
int CHeightLayers::GetRealFloor( int nFloor )
{
	unordered_map<int, int>::iterator it = desired2realFloor.find( nFloor );
	if ( it != desired2realFloor.end() )
		return it->second;

	int nResult = 0;
	unordered_map<int, SHLayer>::iterator k = layers.begin();
	if ( k != layers.end() )
	{
		nResult = k->first;                 // baseline: first-iterated layer key
		for ( ; k != layers.end(); ++k )
		{
			int nKey = k->first;
			if ( nKey > nResult && nKey <= nFloor )
				nResult = nKey;
		}
	}
	desired2realFloor[ nFloor ] = nResult;
	return nResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers::GetLayer @0x35d630 -- empty cache -> shared terrainLayer; else the resolved real floor.
////////////////////////////////////////////////////////////////////////////////////////////////////
SHLayer* CHeightLayers::GetLayer( int nFloor )
{
	if ( layers.empty() )
		return &terrainLayer;
	return &layers[ GetRealFloor( nFloor ) ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers::GetCreateLayer @0x35d670 -- find-or-create the layer for nFloor, seeding a fresh one
// from terrainLayer and recording desired2realFloor[nFloor] = nFloor.
////////////////////////////////////////////////////////////////////////////////////////////////////
SHLayer* CHeightLayers::GetCreateLayer( int nFloor )
{
	unordered_map<int, SHLayer>::iterator it = layers.find( nFloor );
	if ( it != layers.end() )
		return &it->second;

	SHLayer &fresh = layers[ nFloor ];
	fresh.heights = terrainLayer.heights;        // CArray2D<float>::operator= -- deep copy
	desired2realFloor[ nFloor ] = nFloor;
	return &layers[ nFloor ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightLayers::ComputeLayers @0x35d6f0 -- (re)build the shared terrainLayer from the static terrain,
// then (DEFERRED) rasterize the path network into per-floor layers.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightLayers::ComputeLayers( int nXCells, int nYCells, CFuncBase<STerrainInfo>* pTerrain, NAI::IPathNetwork* pPathNet )
{
	// 1. size the terrain layer to the (nXCells+1)x(nYCells+1) knot grid (SetSizes no-ops if unchanged
	//    -- the dev CArray2D::SetSizes is exactly the release "delete + Create only on a size change").
	terrainLayer.heights.SetSizes( nXCells + 1, nYCells + 1 );

	if ( pTerrain == 0 || pTerrain->IsRefInvalid() )
	{
		// 2a. no terrain -> zero the height field, drop the flag.
		terrainLayer.heights.FillZero();
		bHasTerrain = false;
	}
	else
	{
		// 2b. scaled (1/64) copy of the terrain heightMap over the overlapping region. (The binary
		//     AddRefs / lazy-recalcs / Releases the funcbase around this read; that DG plumbing has no
		//     effect on the data product and is elided -- the module has no live caller.)
		const STerrainInfo &info = pTerrain->GetValue();
		const CArray2D<unsigned short> &hm = info.heightMap;
		int nDstX = terrainLayer.heights.GetXSize(), nDstY = terrainLayer.heights.GetYSize();
		int nSrcX = hm.GetXSize(), nSrcY = hm.GetYSize();
		int nMinX = nDstX < nSrcX ? nDstX : nSrcX;
		int nMinY = nDstY < nSrcY ? nDstY : nSrcY;
		for ( int x = 0; x < nMinX; ++x )
			for ( int y = 0; y < nMinY; ++y )
				terrainLayer.heights[y][x] = (float)hm[y][x] * FP_TERRAIN_H_SCALE;
	}

	// 3. DEFERRED no-op: the retail path-network wall rasterization + per-floor beta-spline smoothing
	//    reads a release NAI::CPathNetwork passage/geometry/cell substructure absent from the dev
	//    predecessor (vector<CObj<CNodesLayer>> layers/groups, no passages). Landing it is a structural
	//    rewrite, not a POD fill. Behaviour-neutral: `layers` stays empty so the per-floor cache
	//    degrades to the shared terrainLayer. See decomp/src/s2_heightlayers.h SComputeLayersHooks.
	(void)pPathNet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateHeightLayers @0x35de90 -- factory: build a height-layer cache, return its IHeightLayers face.
////////////////////////////////////////////////////////////////////////////////////////////////////
IHeightLayers* CreateHeightLayers( int nXCells, int nYCells, CFuncBase<STerrainInfo>* pTerrain, NAI::IPathNetwork* pPathNet )
{
	CHeightLayers *pRes = new CHeightLayers();
	pRes->ComputeLayers( nXCells, nYCells, pTerrain, pPathNet );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NWorld
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0xa2313130, CHeightLayers )
