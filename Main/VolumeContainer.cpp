#include "StdAfx.h"
#include "VolumeContainer.h"
namespace NCollider
{
vector<int> fetchBufferArray;
SSkipColliderAnalyzer skipAnalyzer;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVolumeContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::Init( const CVec3 &_ptMin, const CVec3 &ptMax, float fRegularSize )
{
	ptMin = _ptMin;
	ASSERT( ptMax.x > ptMin.x && ptMax.y > ptMin.y && ptMax.z > ptMin.z );
	float fSize = Max( ptMax.x - ptMin.x, Max( ptMax.y - ptMin.y, ptMax.z - ptMin.z ) );
	int nDepth = 1;
	while ( fSize > fRegularSize && nDepth < 6 )
	{
		fSize /= 2;
		++nDepth;
	}
	volumeData.resize( nDepth );
	for ( int i = 0; i < nDepth; ++i )
	{
		int nSide = 1 << i;
		volumeData[i].tris.resize( nSide * nSide * nSide );
		for ( int k = 0; k < volumeData[i].tris.size(); ++k )
			volumeData[i].tris[k].resize(0);
	}
	int nSide = 1 << (nDepth - 1);
	fSize1x = nSide / ( ptMax.x - ptMin.x );
	fSize1y = nSide / ( ptMax.y - ptMin.y );
	fSize1z = nSide / ( ptMax.z - ptMin.z );
	fetchMark.clear();
	nLastFetch = 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::GetCoords( SIntCoords *pRes, const CVec3 &src )
{
	pRes->x = Float2Int( ( src.x - ptMin.x ) * fSize1x - 0.5f );
	pRes->y = Float2Int( ( src.y - ptMin.y ) * fSize1y - 0.5f );
	pRes->z = Float2Int( ( src.z - ptMin.z ) * fSize1z - 0.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
vector<int> CVolumeContainer::fetchBufferArray;
void CVolumeContainer::Fetch( const SBound &bound )
{
	if ( volumeData.size() == 1 )
	{
		pFetched = &volumeData[0].tris[0];
		nFetched = pFetched->size();
		return;
	}
	pFetched = & fetchBufferArray;
	fetchBufferArray.resize( Max( fetchBufferArray.size(), fetchMark.size() ) );
	nFetched = 0;
	//SIntCoords mn, mx;
	CVec3 ptMin( bound.s.ptCenter - bound.ptHalfBox );
	CVec3 ptMax( bound.s.ptCenter + bound.ptHalfBox );
	SVolumeBounds b;
	GetCoords( &b.mn, ptMin );
	GetCoords( &b.mx, ptMax );
	ClipVolume( &b );
	int nDepth = volumeData.size();
	for ( int nLevel = 0; nLevel < nDepth; ++nLevel )
	{
		SLevelContainers &level = volumeData[nLevel];
		int nShift = nDepth - 1 - nLevel;
		for ( int nZ = b.mn.z >> nShift; nZ <= (b.mx.z >> nShift); ++nZ )
		{
			for ( int nY = b.mn.y >> nShift; nY <= (b.mx.y >> nShift); ++nY )
			{
				for ( int nX = b.mn.x >> nShift; nX <= (b.mx.x >> nShift); ++nX )
				{
					int nIdx = ( nZ << (nLevel+nLevel) ) + ( nY << nLevel ) + nX;
					vector<int> &data = level.tris[nIdx];
					for ( int i = 0; i < data.size(); ++i )
					{
						int nRes = data[i];
						if ( fetchMark[nRes] != nLastFetch )
						{
							fetchMark[nRes] = nLastFetch;
							fetchBufferArray[nFetched++] = nRes;
						}
					}
				}
			}
		}
	}
	++nLastFetch;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::MakeVolume( SVolumeBounds *pRes, const SIntCoords &a, const SIntCoords &b, const SIntCoords &c )
{
	pRes->mn.x = Min( a.x, Min( b.x, c.x ) );
	pRes->mn.y = Min( a.y, Min( b.y, c.y ) );
	pRes->mn.z = Min( a.z, Min( b.z, c.z ) );
	pRes->mx.x = Max( a.x, Max( b.x, c.x ) );
	pRes->mx.y = Max( a.y, Max( b.y, c.y ) );
	pRes->mx.z = Max( a.z, Max( b.z, c.z ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::MakeVolume( SVolumeBounds *pRes, const SHMatrix &pos, const vector<CVec3> &coords )
{
	ASSERT( coords.size() > 0 );
	SIntCoords pt;
	CVec3 ptReal;
	pos.RotateHVector( &ptReal, coords[0] );
	GetCoords( &pt, ptReal );
	pRes->mn.x = pRes->mx.x = pt.x;
	pRes->mn.y = pRes->mx.y = pt.y;
	pRes->mn.z = pRes->mx.z = pt.z;
	for ( int i = 1; i < coords.size(); ++i )
	{
		pos.RotateHVector( &ptReal, coords[i] );
		GetCoords( &pt, ptReal );
		if ( pt.x > pRes->mx.x )
			pRes->mx.x = pt.x;
		if ( pt.x < pRes->mn.x )
			pRes->mn.x = pt.x;
		if ( pt.y > pRes->mx.y )
			pRes->mx.y = pt.y;
		if ( pt.y < pRes->mn.y )
			pRes->mn.y = pt.y;
		if ( pt.z > pRes->mx.z )
			pRes->mx.z = pt.z;
		if ( pt.z < pRes->mn.z )
			pRes->mn.z = pt.z;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Build the integer broad-phase volume that bounds an SBound's AABB (release @0072ec00). The two
// opposite AABB corners (center -/+ halfBox) are mapped to grid cells and combined component-wise;
// the explicit Min/Max guards against GetCoords reordering. Used by CPrecalcCollider::AddEntity.
void CVolumeContainer::MakeVolume( SVolumeBounds *pRes, const SBound &b )
{
	SIntCoords mn, mx;
	GetCoords( &mn, b.s.ptCenter - b.ptHalfBox );
	GetCoords( &mx, b.s.ptCenter + b.ptHalfBox );
	pRes->mn.x = Min( mn.x, mx.x );  pRes->mx.x = Max( mn.x, mx.x );
	pRes->mn.y = Min( mn.y, mx.y );  pRes->mx.y = Max( mn.y, mx.y );
	pRes->mn.z = Min( mn.z, mx.z );  pRes->mx.z = Max( mn.z, mx.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVolumeContainer::IsOut( const SVolumeBounds &t )
{
	int nMax = ( 1 << ( volumeData.size() - 1 ) ) - 1;
	return t.mx.x < 0 || t.mx.y < 0 || t.mx.z < 0 || t.mn.x > nMax || t.mn.y > nMax || t.mn.z > nMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::ClipVolume( SVolumeBounds *pRes )
{
	int nMax = ( 1 << ( volumeData.size() - 1 ) ) - 1;
	pRes->mn.x = Max( 0, pRes->mn.x ); pRes->mn.y = Max( 0, pRes->mn.y ); pRes->mn.z = Max( 0, pRes->mn.z );
	pRes->mx.x = Min( nMax, pRes->mx.x ); pRes->mx.y = Min( nMax, pRes->mx.y ); pRes->mx.z = Min( nMax, pRes->mx.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVolumeContainer::Add( const SVolumeBounds &b, int nData )
{
	int nD = b.GetSize();
	int nDepth = volumeData.size();
	int nLevel = nDepth - 1;
	while ( nLevel > 0 && nD > 1 )
	{
		nLevel--;
		nD >>= 1;
	}
	int nShift = nDepth - 1 - nLevel;
	for ( int nZ = b.mn.z >> nShift; nZ <= (b.mx.z >> nShift); ++nZ )
	{
		for ( int nY = b.mn.y >> nShift; nY <= (b.mx.y >> nShift); ++nY )
		{
			for ( int nX = b.mn.x >> nShift; nX <= (b.mx.x >> nShift); ++nX )
			{
				int nIdx = ( nZ << (nLevel+nLevel) ) + ( nY <<nLevel ) + nX;
				volumeData[nLevel].tris[nIdx].push_back( nData );
			}
		}
	}
	if ( nData >= fetchMark.size() )
		fetchMark.resize( nData + 1, 0 );
}
}