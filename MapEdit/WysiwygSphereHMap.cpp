#include "StdAfx.h"
#include "WysiwygSphereHMap.h"
#include "..\Main\Grid.h"
#include "..\Main\METerrain.h"
#include "TerrainUndo.h"

namespace NWysiwyg
{
const CTRect<float> RECT_DEF(1e6,1e6,-1e6,-1e6);
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CTRect<float> GetMaxRect( const CTRect<float> &r1, const CTRect<float> &r2 )
{
	CTRect<float> r;
	r.minx = Min( r1.minx, r2.minx );
	r.miny = Min( r1.miny, r2.miny );
	r.maxx = Max( r1.maxx, r2.maxx );
	r.maxy = Max( r1.maxy, r2.maxy );
	return r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSphereHeightMap::CSphereHeightMap( CWysiwygTerrain *pTerr, const CVec2 &ptTerrainPos )
: pTerrain(pTerr), bValid(false)
{
	ptWorldCenter = FP_GRID_STEP * ptTerrainPos;
	ptCenter.x = Float2Int( ptTerrainPos.x );
	ptCenter.y = Float2Int( ptTerrainPos.y );

	CMETerrainInfo *pInfo = pTerrain->GetTerrain();

	if ( pInfo && ptCenter.x > 0 && ptCenter.y > 0 && 
		ptCenter.x < pInfo->info.heightMap.GetXSize() && 
		ptCenter.y < pInfo->info.heightMap.GetYSize() )
	{
		bValid = true;
		hm = pInfo->info.heightMap;
	}
	fLastHDelta = 0;
	fLastRadius = 0;
	rTotal = RECT_DEF;
	bLastRectValid = false;
	bTotalValid = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSphereHeightMap::OnMove( const CVec2 &ptDelta )
{
	if ( !bValid )
		return;
	float fHDelta = -0.02f * ptDelta.y / FP_TERRAIN_H_SCALE;
	float fRadius = 0.03f * fabs( ptDelta.x );
	if ( fabs( fHDelta - fLastHDelta ) < FP_EPSILON && ( fRadius < 1 || fabs( fRadius - fLastRadius ) < FP_EPSILON ) )
		return;
	fLastHDelta = fHDelta;
	fLastRadius = fRadius;

	CMETerrainInfo *pInfo = pTerrain->GetTerrain();
	pInfo->info.heightMap = hm;
	CTRect<float> r;
	if ( fRadius > 1 )
	{
		if ( fabs( fHDelta ) > 0.01f )
		{
			r = MakeSphere( fRadius, fHDelta * FP_TERRAIN_H_SCALE );
		}
		else
			return;
	}
	else
	{
		float fH = hm[ptCenter.y][ptCenter.x] + fHDelta;
		fH = Max( 0.0f, fH );
		pInfo->info.heightMap[ptCenter.y][ptCenter.x] = fH;
		r = CTRect<float>( ptWorldCenter.x-FP_GRID_STEP , ptWorldCenter.y-FP_GRID_STEP, ptWorldCenter.x+FP_GRID_STEP, ptWorldCenter.y+FP_GRID_STEP );
	}
	UpdateTotalRect( r );
	CTRect<float> rUpdate = bLastRectValid ? GetMaxRect( r, rLast ) : r;
	pTerrain->InvalidateGeometry( rUpdate );
	pTerrain->SetModified();
	rLast = r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTRect<float> CSphereHeightMap::MakeSphere( float fRadius, float fHeight )
{
	const float fSphereR = (sqr( fRadius ) + sqr( fHeight )) / (2 * fHeight);
	const float fSphereR2 = sqr( fSphereR );
	const int nRadius = ceil( fRadius );
	int minx = Max( 0, ptCenter.x - nRadius );
	int miny = Max( 0, ptCenter.y - nRadius );
	int maxx = Min( hm.GetXSize(), ptCenter.x + nRadius );
	int maxy = Min( hm.GetYSize(), ptCenter.y + nRadius );

	CMETerrainInfo *pInfo = pTerrain->GetTerrain();
	CArray2D<unsigned short> &hmap = pInfo->info.heightMap;
	float fSign = Sign( fHeight ) / FP_TERRAIN_H_SCALE;
	float z0 = fabs( fHeight ) - fabs( fSphereR );
	for ( int x = minx; x < maxx; ++x )
		for ( int y = miny; y < maxy; ++y )
		{
			if ( sqr(x - ptCenter.x) + sqr(y - ptCenter.y) > fRadius * fRadius )
				continue;
			const float fDelta = z0 + sqrt( fSphereR2 - sqr(x - ptCenter.x) - sqr(y - ptCenter.y) );
			const float fH = hm[y][x] + fSign * Max( 0.0f, fDelta );
			hmap[y][x] = Max( 0.0f, fH );
		}
	//
	--minx;
	--miny;
	++maxx;
	++maxy;
	return CTRect<float>( minx * FP_GRID_STEP, miny * FP_GRID_STEP, maxx * FP_GRID_STEP, maxy * FP_GRID_STEP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSphereHeightMap::UpdateTotalRect( const CTRect<float> &r )
{
	rTotal = GetMaxRect( rTotal, r );
	bTotalValid = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSphereHeightMap::Cancel()
{
	if ( !bValid )
		return;
	CMETerrainInfo *pInfo = pTerrain->GetTerrain();
	pInfo->info.heightMap = hm;

	if ( bTotalValid )
		pTerrain->InvalidateGeometry( rTotal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSphereHeightMap::FillUndoInfo( CTerrainUndo *pUndo )
{
	if ( !bValid || !bTotalValid )
		return;
	CMETerrainInfo *pInfo = pTerrain->GetTerrain();
	pUndo->SetHeightMapOp( hm, pInfo->info.heightMap, rTotal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IHeightEditor* CreateSphereEditor( CWysiwygTerrain *pTerr, const CVec2 &ptTilePos )
{
	return new CSphereHeightMap( pTerr, ptTilePos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
