#include "StdAfx.h"
#include "TerrainInfo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// STerrainHole
////////////////////////////////////////////////////////////////////////////////////////////////////
int STerrainHole::operator&( CStructureSaver &f )
{
	f.Add( 1, &nHeight );
	f.Add( 2, &bVisible );
	f.Add( 4, &vPolygon );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// STerrainInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
int STerrainInfo::operator&( CStructureSaver &f )
{
	f.Add( 1, &nWidth );
	f.Add( 2, &nHeight );
	f.Add( 3, &holes );
	f.Add( 4, &typeMap );
	f.Add( 5, &heightMap ); 
	f.Add( 6, &grass );
	f.Add( 7, &color );
	f.Add( 8, &spots );
	f.Add( 9, &nMaxGrassLayerID );
	f.Add( 10, &soundmap );
	f.Add( 11, &avrgColor );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_TERRA_STEP = 8;
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainInfoHolder::CTerrainInfoHolder( const STerrainInfo &info ) 
{ 
	value = info;
	int nX = info.nWidth;
	int nY = info.nHeight;
	nX = ceil( (float)nX / N_TERRA_STEP );
	nY = ceil( (float)nY / N_TERRA_STEP );
	geometryGrid.SetSizes( nX, nY );
	textureGrid.SetSizes( nX, nY );
	grassGrid.SetSizes( nX, nY );
	for ( int y = 0; y < nY; ++y )
		for ( int x = 0; x < nX; ++x )
		{
			geometryGrid[y][x] = new CVersioningBase;
			textureGrid[y][x] = new CVersioningBase;
			grassGrid[y][x] = new CVersioningBase;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVersioningBase* CTerrainInfoHolder::GetRegion( const CArray2D<CObj<CVersioningBase> > &grid, const CTRect<int> &sRegion )
{
	ASSERT( sRegion.Width() <= N_TERRA_STEP && sRegion.Height() <= N_TERRA_STEP );
	int x = sRegion.minx / N_TERRA_STEP;
	int y = sRegion.miny / N_TERRA_STEP;
	if ( x >= grid.GetXSize() || y >= grid.GetYSize() )
	{
		ASSERT(0);
		return 0;
	}
	return grid[y][x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainInfoHolder::UpdateRegion( CArray2D<CObj<CVersioningBase> > *pGrid, const CTRect<int> &sRegion )
{
	int minX = Max( 0, int(sRegion.minx / N_TERRA_STEP) );
	int minY = Max( 0, int(sRegion.miny / N_TERRA_STEP) );
	int maxX = Min( pGrid->GetXSize(), 1 + int(sRegion.maxx / N_TERRA_STEP) );
	int maxY = Min( pGrid->GetYSize(), 1 + int(sRegion.maxy / N_TERRA_STEP) );

	for ( int y = minY; y < maxY; ++y )
		for ( int x = minX; x < maxX; ++x )
		{
			(*pGrid)[y][x]->Updated();
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainInfoHolder::UpdateRegionGeometry( const CTRect<int> &sRegion ) 
{ 
	UpdateRegion( &geometryGrid, sRegion ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainInfoHolder::UpdateRegionTexture( const CTRect<int> &sRegion ) 
{ 
	UpdateRegion( &textureGrid, sRegion ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainInfoHolder::UpdateRegionGrass( const CTRect<int> &sRegion ) 
{ 
	UpdateRegion( &grassGrid, sRegion );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVersioningBase* CTerrainInfoHolder::GetRegionGeometry( const CTRect<int> &sRegion ) 
{ 
	return GetRegion( geometryGrid, sRegion ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVersioningBase* CTerrainInfoHolder::GetRegionTexture( const CTRect<int> &sRegion ) 
{ 
	return GetRegion( textureGrid, sRegion ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVersioningBase* CTerrainInfoHolder::GetRegionGrass( const CTRect<int> &sRegion ) 
{ 
	return GetRegion( grassGrid, sRegion ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
STerrainInfo& CTerrainInfoHolder::GetWritableInfo()
{
	return value;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTerrainInfoHolder::operator&( CStructureSaver &f ) 
{ 
	f.Add(1,&value);
	f.Add(2,&geometryGrid);
	f.Add(3,&textureGrid);
	f.Add(4,&grassGrid);
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0x10461600, CCTerrainInfo );
REGISTER_SAVELOAD_CLASS( 0x10461601, CTerrainPart );
REGISTER_SAVELOAD_CLASS( 0xA0562160, CTerrainInfoHolder );
