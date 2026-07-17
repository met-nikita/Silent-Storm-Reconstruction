#include "StdAfx.h"
#include "wHeightLayers.h"
#include "TerrainInfo.h"   // STerrainInfo (heightMap) + CFuncBase<STerrainInfo>::GetValue
#include "Grid.h"          // FP_TERRAIN_H_SCALE (1.0f/64.0f) + FP_INV_GRID_STEP
#include "aiGrid.h"        // ComputeLayers step 3: CPathNetwork/CNodesLayer/CLayersGroup/STile + GetFHeight
#include "BetaSpline.h"    // CBetaSpline::Value(CArray2D<float>&,int,int) @0xb94c0 -- the step-3 smoother
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
// Tags VERIFIED against retail @0x35e5f0 (vtbl+0x0c): 2=terrainLayer, 3=layers, 4=desired2realFloor,
// 5=bHasTerrain. A retail save's chunk reads {2:21338 3:128094 4:1224 5:1} -- tag 3 carries the real
// per-floor layers, so CWorld tag 44 restores a populated cache.
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

	// 3. @0x35d95a: rasterize every layer's tile grid into its floor's height field, carry the result
	//    upward through the floors, then beta-spline-smooth each one. This is what makes the field
	//    building-aware -- without it `layers` stays empty and GetLayer degrades to terrainLayer.
	NAI::CPathNetwork *pNet = dynamic_cast<NAI::CPathNetwork*>( pPathNet );
	if ( pNet == 0 || pNet->IsRefInvalid() )
		return;

	int nMinFloor = 100, nMaxFloor = -100;                    // @0x35d976 / @0x35d97e

	const vector< CObj<NAI::CNodesLayer> > &netLayers = pNet->GetLayers();
	for ( vector< CObj<NAI::CNodesLayer> >::const_iterator it = netLayers.begin(); it != netLayers.end(); ++it )
	{
		NAI::CNodesLayer *pLayer = *it;
		if ( pLayer == 0 )
			continue;
		NAI::CLayersGroup *pGroup = pLayer->pGroup;           // CNodesLayer +0x5c
		if ( pGroup == 0 )
			continue;
		// ptXDir is (cos,sin) * FP_GRID_STEP (aiGrid.cpp:1078); normalizing divides the 0.625 out, so
		// FP_INV_GRID_STEP * FP_GRID_STEP == 1 -- one tile step IS one terrain cell.
		float nc = pGroup->ptXDir.x, ns = pGroup->ptXDir.y;
		const float fLen2 = nc * nc + ns * ns;
		if ( fLen2 != 0.0f )
		{
			const float fK = 1.0f / sqrt( fLen2 );
			nc *= fK;
			ns *= fK;
		}
		const float fOrgX = FP_INV_GRID_STEP * pGroup->ptOrigin.x;
		const float fOrgY = FP_INV_GRID_STEP * pGroup->ptOrigin.y;

		for ( int i = 0; i < pLayer->tiles.GetXSize(); ++i )
			for ( int j = 0; j < pLayer->tiles.GetYSize(); ++j )
			{
				const NAI::STile &tile = pLayer->tiles[j][i];
				// Retail rotates by the TRANSPOSE of CLayersGroup::GetCPNoHeight (aiGrid.h:210) --
				// both sin signs inverted. Identical while a group's theta is 0. Reproduced 1:1.
				const float fx = (float)i, fy = (float)j, fx5 = fx + 0.5f, fy5 = fy + 0.5f;
				const float fX1 = fOrgX + fx  * nc + fy  * ns, fY1 = fOrgY + fy  * nc - fx  * ns;
				const float fX2 = fOrgX + fx5 * nc + fy5 * ns, fY2 = fOrgY + fy5 * nc - fx5 * ns;

				SHLayer *pHL = GetCreateLayer( tile.nFloor );
				if ( tile.nFloor <= nMinFloor ) nMinFloor = tile.nFloor;
				if ( tile.nFloor >= nMaxFloor ) nMaxFloor = tile.nFloor;
				const float fH = NAI::GetFHeight( tile.nHeight );

				// Float2Int is fld/fistp (round-to-nearest-even) -- a C cast would truncate. Both
				// bounds are strict at 0, so row/column 0 is never written.
				int nX = Float2Int( fX1 ), nY = Float2Int( fY1 );
				if ( nX > 0 && nX < pHL->heights.GetXSize() && nY > 0 && nY < pHL->heights.GetYSize() )
					pHL->heights[nY][nX] = Max( pHL->heights[nY][nX], fH );
				nX = Float2Int( fX2 );
				nY = Float2Int( fY2 );
				if ( nX > 0 && nX < pHL->heights.GetXSize() && nY > 0 && nY < pHL->heights.GetYSize() )
					pHL->heights[nY][nX] = Max( pHL->heights[nY][nX], fH );
			}
	}

	if ( layers.empty() )                                     // @0x35dc11 -- gates the merge AND the smooth
		return;

	// 3b @0x35dc0d: carry each real floor's field up into the next distinct one.
	SHLayer *pPrev = &layers[ GetRealFloor( nMinFloor ) ];
	for ( int f = nMinFloor + 1; f <= nMaxFloor; ++f )
	{
		SHLayer *pCur = &layers[ GetRealFloor( f ) ];
		if ( pCur == pPrev )
			continue;
		for ( int x = 0; x < pCur->heights.GetXSize(); ++x )
			for ( int y = 0; y < pCur->heights.GetYSize(); ++y )
				pCur->heights[y][x] = Max( pCur->heights[y][x], pPrev->heights[y][x] );
		pPrev = pCur;
	}

	// 3c @0x35dcd7: smooth each distinct real floor, double-buffered through a copy.
	// (Retail emits Init(1,1) first @0x35dcea; Init re-derives every field, so (0.5,10) is the live shape.)
	CBetaSpline spline;
	spline.Init( 0.5f, 10.0f );
	SHLayer *pDone = 0;
	for ( int f = nMinFloor; f <= nMaxFloor; ++f )
	{
		SHLayer *pHL = &layers[ GetRealFloor( f ) ];
		if ( pHL == pDone )
			continue;
		CArray2D<float> tmp = pHL->heights;
		for ( int x = 0; x < pHL->heights.GetXSize(); ++x )
			for ( int y = 0; y < pHL->heights.GetYSize(); ++y )
				pHL->heights[y][x] = spline.Value( tmp, x, y );
		pDone = pHL;
	}
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
