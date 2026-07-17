#include "StdAfx.h"
#include "DG.h"
#include "GGeometry.h"
#include "..\Misc\BasicShare.h"
#include "Grid.h"
#include "GScene.h"
#include "GObjectInfo.h"
#include "GPixelFormat.h"
#include "GFileSkin.h"
#include "aiObject.h"
#include "aiObjectLoader.h"

inline bool operator==( const SPlane &a, const SPlane &b ) { return a.n == b.n && a.d == b.d; }
namespace NAI
{
	externA5 CBasicShare<int, CLoadGeometryInfo> shareAIModel;
	externA5 CBasicShare<int, CFileSkinPointsLoad> shareSkinPoints;
}
using namespace NBuilding;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
static CBasicShare<SPartKey, CObjectInfoPiecesLoader, SPartHash> shareObjInfoPieces(122);
static vector<STriangle> trisBuf;
static SVertex zeroVertex;
////////////////////////////////////////////////////////////////////////////////////////////////////
// poly utils
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadVertexHash
{
	int operator()( const SLoadVertex &v ) const { return SVec3Hash()( v.pos ); }
};
bool operator==( const SLoadVertex &a, const SLoadVertex &b ) { return memcmp( &a, &b, sizeof(a) ) == 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// �������� �������������
const int N_STEPS_PER_METER = 4096;
const int N_TEXTURE_PRECISION = 65536;
const int N_VECTOR_PRECISION = 16384;
static void AlignPositionToGrid( CVec3 *p )
{
	p->x = Float2Int( p->x * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
	p->y = Float2Int( p->y * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
	p->z = Float2Int( p->z * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
}
static void AlignVector( CVec3 *p )
{
	p->x = Float2Int( p->x * N_VECTOR_PRECISION ) * ( 1.0f / N_VECTOR_PRECISION );
	p->y = Float2Int( p->y * N_VECTOR_PRECISION ) * ( 1.0f / N_VECTOR_PRECISION );
	p->z = Float2Int( p->z * N_VECTOR_PRECISION ) * ( 1.0f / N_VECTOR_PRECISION );
}
static void AlignPlaneToGrid( SPlane *p )
{
	p->n.x = Float2Int( p->n.x * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
	p->n.y = Float2Int( p->n.y * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
	p->n.z = Float2Int( p->n.z * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
	p->d = Float2Int( p->d * N_STEPS_PER_METER ) * ( 1.0f / N_STEPS_PER_METER );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void StripOppositeFacingPolygons( SFacesVector *pRes, const SFacesVector &src )
{
	// merge by positions
	unordered_map<CVec3, int, SVec3Hash> posHash;
	vector<int> posIndices( src.points.size(), -1 );
	for ( int k = 0; k < src.points.size(); ++k )
	{
		CVec3 vPos( src.points[k].pos );
		AlignPositionToGrid( &vPos );
		unordered_map<CVec3, int, SVec3Hash>::iterator i = posHash.find( vPos );
		if ( i != posHash.end() )
			posIndices[k] = i->second;
		else
		{
			posHash[ vPos ] = k;
			posIndices[k] = k;
		}
	}
	
	const SPolygonIndices &srcpolys = src.polys;

	// normalize polygon indices (start with lowest index)
	vector<WORD> indices( srcpolys.indices.size() );
	for ( int k = 1; k < srcpolys.polys.size(); ++k )
	{
		// peek lowest
		int nFirst = srcpolys.polys[ k - 1 ];
		int nStart = nFirst;
		WORD nLowest = posIndices[ srcpolys.indices[ nStart ] ];
		for ( int i = nStart + 1; i < srcpolys.polys[ k ]; ++i )
		{
			WORD nTest = posIndices[ srcpolys.indices[i] ];
			if ( nTest < nLowest )
			{
				nLowest = nTest;
				nStart = i;
			}
		}
		int nTarget = nFirst;
		for ( int i = nStart; i < srcpolys.polys[ k ]; ++i )
			indices[ nTarget++ ] = posIndices[ srcpolys.indices[ i ] ];
		for ( int i = nFirst; i < nStart; ++i )
			indices[ nTarget++ ] = posIndices[ srcpolys.indices[ i ] ];
	}

	// copy polygons one by one and check for every if it has peer
	vector<char> takePolygon( srcpolys.polys.size(), true );
	for ( int k = 2; k < srcpolys.polys.size(); ++k )
	{
		for ( int kTest = 1; kTest < k; ++kTest )
		{
			if ( !takePolygon[kTest] )
				continue;
			if (
				indices[ srcpolys.polys[ k - 1 ] ] == indices[ srcpolys.polys[ kTest - 1 ] ] && 
				srcpolys.polys[ k ] - srcpolys.polys[ k - 1 ] == srcpolys.polys[ kTest ] - srcpolys.polys[ kTest - 1 ] )
			{
				bool bIsSame = true;
				int nIndices = srcpolys.polys[ k ] - srcpolys.polys[ k - 1 ];
				for ( int t = 1; t < nIndices; ++t )
				{
					if ( indices[ srcpolys.polys[ k - 1 ] + t ] != indices[ srcpolys.polys[ kTest ] - t ] )
					{
						bIsSame = false;
						break;
					}
				}
				if ( bIsSame )
				{
					takePolygon[ k ] = false;
					takePolygon[ kTest ] = false;
					break;
				}
			}
		}
	}

	// output result
	pRes->points = src.points;
	pRes->polys.Clear();
	for ( int k = 1; k < srcpolys.polys.size(); ++k )
	{
		if ( !takePolygon[ k ] )
			continue;
		for ( int i = srcpolys.polys[k - 1]; i < srcpolys.polys[k]; ++i )
			pRes->polys.indices.push_back( srcpolys.indices[i] );
		pRes->polys.polys.push_back( pRes->polys.indices.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSector
{
	int nStart, nFinish;
	float fStart, fFinish; // angles
};
struct SRemoveInternal
{
	//SFacesVector &src;
	const vector<SLoadVertex> &points;
	SPolygonIndices &src;
	vector<int> connectionsPerPoint, connectionOffset;
	vector<SSector> sectors;
	vector<char> isInternal;
	vector<int> collapse1, collapse2;
	vector<float> originalDirection;
	vector<WORD> mergeTable;
	int nTotalSectors;
	CVec3 vX, vY, vNormal;

	void CountSectors()
	{
		for ( int k = 1; k < src.polys.size(); ++k )
		{
			for ( int i = src.polys[k-1]; i < src.polys[k]; ++i )
			{
				int nCur = src.indices[ i ];
				++connectionsPerPoint[ nCur ];
			}
		}
	}
	void CalcOffsets()
	{
		nTotalSectors = 0;
		for ( int k = 0; k < connectionsPerPoint.size(); ++k )
		{
			connectionOffset[ k ] = nTotalSectors;
			nTotalSectors += connectionsPerPoint[ k ];
		}
	}
	float GetAngle( const CVec3 &vDif )
	{
		return atan2( vDif * vY, vDif * vX );
	}
	void CalcSectors()
	{
		sectors.resize( nTotalSectors );
		vector<int> outCount( points.size(), 0 );
		// 
		for ( int k = 1; k < src.polys.size(); ++k )
		{
			int nStart = src.polys[k-1];
			int nFinish = src.polys[k];
			if ( nFinish - nStart < 3 )
				continue;
			int nPrev2 = src.indices[ nFinish - 2 ];
			int nPrev1 = src.indices[ nFinish - 1 ];
			for ( int i = nStart; i < nFinish; ++i )
			{
				int nCur = src.indices[ i ];
				SSector &s = sectors[ connectionOffset[nPrev1] + outCount[ nPrev1 ]++ ];
				s.nStart = nPrev2;
				s.nFinish = nCur;
				s.fStart = GetAngle( points[ nPrev2 ].pos - points[ nPrev1 ].pos );
				s.fFinish = GetAngle( points[ nCur ].pos - points[ nPrev1 ].pos );
				nPrev2 = nPrev1;
				nPrev1 = nCur;
			}
		}
		for ( int k = 0; k < connectionsPerPoint.size(); ++k )
			ASSERT( outCount[k] == connectionsPerPoint[k] );
	}
	bool FindInternalPoints()
	{
		isInternal.resize( points.size(), 0 );
		collapse1.resize( points.size(), -1 );
		collapse2.resize( points.size(), -1 );
		vector<char> hasNext( nTotalSectors, 0 ), hasPrev( nTotalSectors, 0 );
		bool bHasInternal = false;
		for ( int i = 0; i < isInternal.size(); ++i )
		{
			int nOffset = connectionOffset[ i ];
			// find peers
			for ( int k = 0; k < connectionsPerPoint[i]; ++k )
			{
				//float f = sectors[ nOffset + k ].fStart;
				int n = sectors[ nOffset + k ].nStart;
				for ( int z = 0; z < connectionsPerPoint[i]; ++z )
				{
					if ( hasNext[ nOffset + z ] )
						continue;
					//if ( sectors[ nOffset + z ].fFinish == f )
					if ( sectors[ nOffset + z ].nFinish == n )
					{
						hasNext[ nOffset + z ] = 1;
						hasPrev[ nOffset + k ] = 1;
						break;
					}
				}
			}
			// check if whole range or exactly half
			float fStart;
			int nStartCount = 0, nStartIdx;
			for ( int k = 0; k < connectionsPerPoint[i]; ++k )
			{
				if ( hasPrev[ nOffset + k ] )
					continue;
				nStartIdx = sectors[ nOffset + k ].nStart;
				fStart = sectors[ nOffset + k ].fStart;
				++nStartCount;
			}
			if ( nStartCount > 1 )
				continue;
			if ( nStartCount == 1 )
			{
				// check if it is a half point
				float fFinish;
				int nFinishIdx;
				for ( int k = 0; k < connectionsPerPoint[i]; ++k )
				{
					if ( hasNext[ nOffset + k ] )
						continue;
					nFinishIdx = sectors[ nOffset + k ].nFinish;
					fFinish = sectors[ nOffset + k ].fFinish;
					break;
				}
				float fDelta = fFinish - fStart;
				fDelta = fDelta + FP_2PI * 10;
				fDelta = fmod( fDelta, FP_2PI );
				if ( fabs( fDelta - FP_PI ) > 0.001f )
					continue;
				collapse1[i] = nStartIdx;
				collapse2[i] = nFinishIdx;
			}
			if ( connectionsPerPoint[i] != 0 )
			{
				isInternal[i] = true;
				bHasInternal = true;
			}
		}
		return bHasInternal;
	}
	void CalcOriginalDirection()
	{
		originalDirection.resize( nTotalSectors );
		for ( int k = 0; k < connectionsPerPoint.size(); ++k )
		{
			int nOffset = connectionOffset[ k ];
			const CVec3 &v = points[ k ].pos;
			for ( int i = 0; i < connectionsPerPoint[k]; ++i )
			{
				int nIdx = nOffset + i;
				const SSector &sector = sectors[ nIdx ];
				const CVec3 &vStart = points[ sector.nStart ].pos;
				const CVec3 &vFinish = points[ sector.nFinish ].pos;
				originalDirection[nIdx] = ( ( vFinish - v ) ^ ( vStart - v ) ) * vNormal;
			}
		}
	}
	bool CanCollapse( int nFrom, int nTo )
	{
		CVec3 vTo = points[ mergeTable[ nTo ] ].pos;
		for ( int k = 0; k < connectionsPerPoint.size(); ++k )
		{
			if ( mergeTable[ k ] == nFrom )
			{
				int nOffset = connectionOffset[k];
				for ( int i = 0; i < connectionsPerPoint[k]; ++i )
				{
					int nIdx = nOffset + i;
					const SSector &sector = sectors[ nIdx ];
					int nStart = mergeTable[ sector.nStart ];
					if ( nStart == nFrom )
						nStart = nTo;
					int nFinish = mergeTable[ sector.nFinish ];
					if ( nFinish == nFrom )
						nFinish = nTo;
					const CVec3 &vStart = points[ nStart ].pos;
					const CVec3 &vFinish = points[ nFinish ].pos;
					CVec3 vN = ( vFinish - vTo ) ^ ( vStart - vTo );
					float f = vN * vNormal;
					if ( originalDirection[ nIdx ] == 0 && f != 0 )
						return false;
					if ( f * originalDirection[ nIdx ] < 0 )
						return false;
				}
			}
		}
		return true;
	}
	void PerformMerge( int nFrom, int nTo )
	{
		mergeTable[ nFrom ] = mergeTable[ nTo ];
		for ( int m = 0; m <mergeTable.size(); ++m )
			mergeTable[m] = mergeTable[ mergeTable[m] ];
		isInternal[ nFrom ] = false;
	}
	void Merge()
	{
		mergeTable.resize( points.size() );
		for ( int k = 0; k < mergeTable.size(); ++k )
			mergeTable[k] = k;
		vector<int> possibleMerge( points.size(), 0 );
		int nPossibleMerge = 1;
		for(;;)
		{
			bool bHasMerged = false;
			for ( int k = 0; k < isInternal.size(); ++k )
			{
				if ( isInternal[k] ) 
				{
					if ( collapse1[k] >= 0 )
					{
						// border point
						int nTarget = mergeTable[ collapse1[k] ];
						if ( CanCollapse( k, nTarget ) )
						{
							if ( collapse1[ nTarget ] == k )
								collapse1[ nTarget ] = collapse2[k];
							if ( collapse2[ nTarget ] == k )
								collapse2[ nTarget ] = collapse2[k];
							bHasMerged = true;
							PerformMerge( k, nTarget );
							break;
						}
						nTarget = mergeTable[ collapse2[k] ];
						if ( CanCollapse( k, nTarget ) )
						{
							if ( collapse1[ nTarget ] == k )
								collapse1[ nTarget ] = collapse1[k];
							if ( collapse2[ nTarget ] == k )
								collapse2[ nTarget ] = collapse1[k];
							bHasMerged = true;
							PerformMerge( k, nTarget );
							break;
						}
					}
					else
					{
						// true internal point
						// find possible merges
						++nPossibleMerge;
						for ( int i = 0; i < mergeTable.size(); ++i )
						{
							if ( mergeTable[i] != k )
								continue;
							int nOffset = connectionOffset[ i ];
							for ( int z = 0; z < connectionsPerPoint[i]; ++z )
							{
								const SSector &sector = sectors[ nOffset + z ];
								possibleMerge[ mergeTable[ sector.nStart ] ] = nPossibleMerge;
								possibleMerge[ mergeTable[ sector.nFinish ] ] = nPossibleMerge;
							}
						}
						// check merges
						possibleMerge[ k ] = 0;
						for ( int i = 0; i < possibleMerge.size(); ++i )
						{
							if ( possibleMerge[i] != nPossibleMerge )
								continue;
							if ( CanCollapse( k, i ) )
							{
								bHasMerged = true;
								PerformMerge( k, i );
								break;
							}
						}
					}
				}
			}
			if ( !bHasMerged )
				break;
		}
	}
	SRemoveInternal( const vector<SLoadVertex> &_points, SPolygonIndices *pRes, const SPlane &plane ) 
		: points(_points), src(*pRes), connectionsPerPoint( _points.size(), 0 ), connectionOffset( _points.size() ) 
	{
		CVec3 vTest( 1, 0, 0 );
		if ( fabs( vTest * plane.n ) > 0.7f )
			vTest = CVec3(0,1,0);
		vX = vTest ^ plane.n;
		Normalize( &vX );
		AlignVector( &vX );
		vY = vX ^ plane.n;
		Normalize( &vY );
		AlignVector( &vY );
		vNormal = plane.n;

		vector<STriangle> tris;
		src.GetTriangles( &tris );
		src.SetTriangles( tris );
	}
	bool Process()
	{
		CountSectors();
		CalcOffsets();
		CalcSectors();
		if ( !FindInternalPoints() )
			return false;
		CalcOriginalDirection();
		Merge();
		src.Filter( mergeTable );
		return true;
	}
};
static bool RemoveInternalPoints( const vector<SLoadVertex> &_points, SPolygonIndices *pRes, const SPlane &plane )
{
	SRemoveInternal r( _points, pRes, plane );
	return r.Process();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcCross( SLoadVertex *pRes, const SLoadVertex &vCur, const SLoadVertex &vNxt, float fCurDot, float fNextDot )
{
	float fScale = fNextDot / ( fNextDot - fCurDot );
	float f1Scale = 1.0f - fScale;
	pRes->pos     = fScale * vCur.pos + f1Scale * vNxt.pos;//vCur.pos + fScale * (vNxt.pos - vCur.pos);
	pRes->normal  = fScale * vCur.normal + f1Scale * vNxt.normal;
	pRes->texU    = fScale * vCur.texU + f1Scale * vNxt.texU;
	pRes->texV    = fScale * vCur.texV + f1Scale * vNxt.texV;
	pRes->tex     = fScale * vCur.tex + f1Scale * vNxt.tex;//vCur.tex + fScale * (vNxt.tex - vCur.tex);
	pRes->dwColor = NGfx::GetDWORDColor( fScale * NGfx::GetCVec4Color( vCur.dwColor ) + f1Scale * NGfx::GetCVec4Color( vNxt.dwColor ) );
	Normalize( &pRes->normal );
	Normalize( &pRes->texU );
	Normalize( &pRes->texV );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SClipHelper
{
	SFacesVector *pRes;
	unordered_map< SLoadVertex, int, SLoadVertexHash > pointHash;
	SClipHelper( SFacesVector *_pRes, const SFacesVector &src ) : pRes(_pRes)
	{
		pRes->points = src.points;
		pRes->polys.Clear();
	}
	void FormPolygon() 
	{ 
		if ( pRes->polys.indices.size() - pRes->polys.polys.back() > 2 )
			pRes->polys.polys.push_back( pRes->polys.indices.size() ); 
		else
			pRes->polys.indices.resize( pRes->polys.polys.back() );
	}
	void AddCross( const SLoadVertex &vPrev, const SLoadVertex &vCur, float fPrevDot, float fCurDot )
	{
		SLoadVertex vCross;
		CalcCross( &vCross, vPrev, vCur, fPrevDot, fCurDot );
		unordered_map< SLoadVertex, int, SLoadVertexHash >::iterator i = pointHash.find( vCross );
		if ( i == pointHash.end() )
		{
			int n = pRes->points.size();
			pRes->points.push_back( vCross );
			pointHash[ vCross ] = n;
			pRes->polys.indices.push_back( n );
		}
		else
			pRes->polys.indices.push_back( i->second );
	}
	void AddPoint( int nIdx ) { pRes->polys.indices.push_back( nIdx ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Clip( SFacesVector *pRes, const SFacesVector &src, const SPlane &plane )
{
	SClipHelper ch( pRes, src );
	for ( int k = 1; k < src.polys.polys.size(); ++k )
	{
		int nStart = src.polys.polys[ k - 1 ], nFinish = src.polys.polys[ k ];
		const SLoadVertex *pPrev = &src.points[ src.polys.indices[ nFinish - 1 ] ];
		float fCurDot, fPrevDot = plane.d - pPrev->pos * plane.n;
		for ( int i = nStart; i < nFinish; ++i )
		{
			int nIdx = src.polys.indices[ i ];
			fCurDot = plane.d - src.points[ nIdx ].pos * plane.n;
			if ( fCurDot <= 0 )
			{
				if ( fPrevDot > 0 )
					ch.AddCross( *pPrev, src.points[ nIdx ], fPrevDot, fCurDot );
				ch.AddPoint( nIdx );
			}
			else
			{
				if ( fPrevDot < 0 )
					ch.AddCross( *pPrev, src.points[ nIdx ], fPrevDot, fCurDot );
			}
			fPrevDot = fCurDot;
			pPrev = &src.points[ nIdx ];
		}
		ch.FormPolygon();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// strip not used and merge same
static void OptimizeVertices( SFacesVector *pRes, const SFacesVector &src )
{
	pRes->points.resize(0);
	pRes->polys.Clear();
	vector<int> posIndices( src.points.size(), -1 );
	unordered_map< SLoadVertex, int, SLoadVertexHash > pointHash;
	for ( int k = 1; k < src.polys.polys.size(); ++k )
	{
		for ( int i = src.polys.polys[ k - 1 ]; i < src.polys.polys[ k ]; ++i )
		{
			int n = src.polys.indices[ i ];
			if ( posIndices[ n ] == -1 )
			{
				SLoadVertex vertex( src.points[n] );
				AlignPositionToGrid( &vertex.pos );
				AlignVector( &vertex.normal );
				AlignVector( &vertex.texU );
				AlignVector( &vertex.texV );
				vertex.tex.u = Float2Int( vertex.tex.u * N_TEXTURE_PRECISION ) * ( 1.0f / N_TEXTURE_PRECISION );
				vertex.tex.v = Float2Int( vertex.tex.v * N_TEXTURE_PRECISION ) * ( 1.0f / N_TEXTURE_PRECISION );

				unordered_map< SLoadVertex, int, SLoadVertexHash >::iterator i = pointHash.find( vertex );
				if ( i == pointHash.end() )
				{
					int nIndex = pRes->points.size();
					pointHash[ vertex ] = nIndex;
					posIndices[ n ] = nIndex;
					pRes->points.push_back( vertex );
				}
				else
					posIndices[ n ] = i->second;
			}
			pRes->polys.indices.push_back( posIndices[ n ] );
		}
		pRes->polys.polys.push_back( pRes->polys.indices.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPlaneHash
{
	int operator()( const SPlane &a ) const { return SVec3Hash()( a.n ) ^ ((int*)&a.d)[0]; }
};
static void SeparateOnPerPlaneFaces(unordered_map<SPlane, SPolygonIndices, SPlaneHash> *pRes, const SFacesVector &src )
{
	for ( int k = 1; k < src.polys.polys.size(); ++k )
	{
		int nStart = src.polys.polys[ k - 1 ], nFinish = src.polys.polys[ k ];
		if ( nFinish - nStart < 3 )
			continue;
		int nPrev1 = nFinish - 1, nPrev2 = nFinish - 2;
		for ( int i = nStart; i < nFinish; ++i )
		{
			SPlane p;
			const CVec3 &v1 = src.points[ src.polys.indices[nPrev2] ].pos;
			const CVec3 &v2 = src.points[ src.polys.indices[nPrev1] ].pos;
			const CVec3 &v3 = src.points[ src.polys.indices[i] ].pos;
			if ( p.Set( v1, v2, v3 ) )
			{
				AlignPlaneToGrid( &p );
				unordered_map<SPlane, SPolygonIndices, SPlaneHash>::iterator k = pRes->find( p );
				SPolygonIndices *pDst;
				if ( k == pRes->end() )
				{
					pDst = &(*pRes)[ p ];
					pDst->Clear();
				}
				else
					pDst = &k->second;
				for ( int i = nStart; i < nFinish; ++i )
					pDst->indices.push_back( src.polys.indices[ i ] );
				pDst->polys.push_back( pDst->indices.size() );
				break;
			}
			nPrev2 = nPrev1;
			nPrev1 = i;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddFaces( SFacesVector *pRes, const SFacesVector &src )
{
	int nShift = pRes->points.size();
	pRes->points.resize( pRes->points.size() + src.points.size() );
	for ( int k = 0; k < src.points.size(); ++k )
		pRes->points[ nShift + k ] = src.points[ k ];
	for ( int k = 1; k < src.polys.polys.size(); ++k )
	{
		for ( int i = src.polys.polys[ k - 1 ]; i < src.polys.polys[ k ]; ++i )
			pRes->polys.indices.push_back( src.polys.indices[ i ] + nShift );
		pRes->polys.polys.push_back( pRes->polys.indices.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddFaces( SFacesVector *pRes, const SPolygonIndices &src )
{
	for ( int k = 1; k < src.polys.size(); ++k )
	{
		for ( int i = src.polys[ k - 1 ]; i < src.polys[ k ]; ++i )
			pRes->polys.indices.push_back( src.indices[ i ] );
		pRes->polys.polys.push_back( pRes->polys.indices.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ConvertVertices( vector<SVertex> *pRes, const vector<SLoadVertex> &_src )
{
	pRes->resize( _src.size() );
	for ( int k = 0; k < _src.size(); ++k )
	{
		NGScene::SVertex &dst = (*pRes)[k];
		const SLoadVertex &src = _src[k];
		dst.pos = src.pos;
		// release SVertex stores normal/texU/texV packed (SCompactVector). Release's
		// SLoadVertex (PDB, 32B) is ALREADY compact so release copies verbatim; this
		// tree's SLoadVertex still carries raw CVec3s (dev resource-file format), so
		// pack at this load boundary -- same packed values either way.
		NGfx::CalcCompactVector( &dst.normal, src.normal );
		dst.tex = src.tex;
		NGfx::CalcCompactVector( &dst.texU, src.texU );
		NGfx::CalcCompactVector( &dst.texV, src.texV );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Assign( CObjectInfo *pRes, const SFacesVector &src )
{
	CObjectInfo::SData objData;
	objData.geometry = src.polys;
	ConvertVertices( &objData.verts, src.points );
	pRes->Assign( objData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfoLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileRequest* CObjectInfoLoader::CreateRequest()
{
	return new CFileRequest( "Geometries", GetKey() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfoLoader::RecalcValue( CFileRequest *pRequest )
{
	CFileRequest &req = *pRequest;
	vector<SLoadVertexWeight> wghts;
	vector<SLoadVertex> vertices;
	pValue = new CObjectInfo;
	try
	{
		NGScene::CObjectInfo::SData objData;
		CStructureSaver saver( *req.GetStream(), CStructureSaver::READ );
		saver.Add( 1, &vertices );
		saver.Add( 2, &objData.geometry.indices );
		saver.Add( 3, &objData.geometry.polys );
		saver.Add( 5, &wghts );
		if ( !vertices.empty() )
			ConvertWeights( &objData.weights, wghts, vertices.size() );
		ConvertVertices( &objData.verts, vertices );
		pValue->Assign( objData );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGeometryConverter
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddPiece( NGScene::CObjectInfo::SData *pObj, const NAI::CGeometryInfo::SPiece &p )
{
	NGScene::CObjectInfo::SData &objData = *pObj;
	p.edges.BuildTriangleList( &trisBuf );
	int nVerts = p.points.size(), nBase = objData.verts.size();
	objData.geometry.AddTriangles( trisBuf, nBase );
	objData.verts.resize( nBase + nVerts );
	for ( int k = 0; k < nVerts; ++k )
	{
		SVertex &dst = objData.verts[ k + nBase ];
		dst = zeroVertex;
		dst.pos = p.points[k];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIGeometryConverter::Recalc()
{
	const NAI::CGeometryInfo *pAIGData = pAIGeom->GetValue();
	pValue = new CObjectInfo;
	if ( pAIGData == 0 )
		return;
	const NAI::CGeometryInfo &g = *pAIGData;
	NGScene::CObjectInfo::SData objData;

	vector<STriangle> tris;
	objData.geometry.SetTriangles( tris );
	if ( bTakeAll )
	{
		for ( NAI::CGeometryInfo::CPieceMap::const_iterator i = g.pieces.begin(); i != g.pieces.end(); ++i )
		{
			const NAI::CGeometryInfo::SPiece &p = i->second;
			AddPiece( &objData, p );
		}
	}
	else
	{
		for ( int k = 0; k < parts.size(); ++k )
		{
			NAI::CGeometryInfo::CPieceMap::const_iterator i = g.pieces.find( parts[k] );
			if ( i != g.pieces.end() )
				AddPiece( &objData, i->second );
		}
	}
	pValue->AssignFast( objData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIGeometryConverter::SetKey( int _nID ) 
{
	pAIGeom = NAI::shareAIModel.Get( _nID );
	parts.clear();
	bTakeAll = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGeometryConverter::CAIGeometryConverter( int _nID, const vector<int> &_parts )
	: parts(_parts)
{
	pAIGeom = NAI::shareAIModel.Get( _nID );
	bTakeAll = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISkinObjectConverter
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAISkinObjectConverter::Recalc()
{
	const NAI::CFileSkinPoints *pAIGData = pAIGeom->GetValue();
	pValue = new CObjectInfo;
	if ( pAIGData == 0 )
		return;
	const NAI::CFileSkinPoints &g = *pAIGData;
	NGScene::CObjectInfo::SData objData;

	vector<STriangle> tris;
	objData.geometry.SetTriangles( tris );
	for ( NAI::CFileSkinPoints::CBodypartsHash::const_iterator i = g.parts.begin(); i != g.parts.end(); ++i )
	{
		const NAI::CFileSkinPoints::SBodypart &p = i->second;
		ASSERT( p.weights.size() == p.points.size() );
		p.edges.BuildTriangleList( &trisBuf );
		int nVerts = p.points.size(), nBase = objData.verts.size();
		objData.geometry.AddTriangles( trisBuf, nBase );
		objData.verts.resize( nBase + nVerts );
		objData.weights.resize( nBase + nVerts );
		for ( int k = 0; k < nVerts; ++k )
		{
			SVertex &dst = objData.verts[ k + nBase ];
			dst = zeroVertex;
			dst.pos = p.points[k];
			objData.weights[ k + nBase ] = p.weights[k];
		}
	}
	pValue->AssignFast( objData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAISkinObjectConverter::SetKey( int _nID )
{
	pAIGeom = NAI::shareSkinPoints.Get( _nID );
	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfoPiecesLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileRequest* CObjectInfoPiecesLoader::CreateRequest()
{
	return new CFileRequest( "Geometries", GetKey() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfoPiecesLoader::RecalcValue( CFileRequest *pRequest )
{
	pValue = new CObjectInfoPieces;
	try
	{
		CFileRequest &req = *pRequest;
		CStructureSaver saver( *req.GetStream(), CStructureSaver::READ );
		saver.Add( 4, &pValue->faces );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWallObjectInfoClipper
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallObjectInfoClipper::ClipWall()
{
	SPlane clipLeft, clipRight;
	bool bClipLeft, bClipRight;
	CPieceMap &faces = pSrc->GetValue()->faces;
	short nClipL = (clipInfo.nClip >> 8) & 0xff;
	short nClipR = clipInfo.nClip & 0xff;

	bClipLeft  = GetClipPlane( &clipLeft, nClipL, true );
	bClipRight = GetClipPlane( &clipRight, nClipR, false );
	vector<int> visibleParts;
	UnpackParts( &visibleParts, clipInfo.dwParts, clipInfo.nSubBlockID );
	//
	SFacesVector res, res1;
	res.points.reserve( 1024 );
	res.polys.Clear();
	for ( int k = 0; k < visibleParts.size(); ++k )
	{
		CPieceMap::const_iterator it = faces.find( visibleParts[k] );
		if ( it != faces.end() )
			AddFaces( &res, it->second );
	}
	if ( bClipLeft )
	{
		Clip( &res1, res, clipLeft );
		if ( bClipRight )
			Clip( &res, res1, clipRight );
		else
			res = res1;
	}
	else if ( bClipRight )
	{
		Clip( &res1, res, clipRight );
		res = res1;
	}
	unordered_map<SPlane,SPolygonIndices,SPlaneHash> perPlane;
	StripOppositeFacingPolygons( &res1, res );
	OptimizeVertices( &res, res1 );
	SeparateOnPerPlaneFaces( &perPlane, res );
	res1.points = res.points;
	res1.polys.Clear();
	for (unordered_map<SPlane,SPolygonIndices,SPlaneHash>::iterator i = perPlane.begin(); i != perPlane.end(); ++i )
	{
		if ( RemoveInternalPoints( res.points, &i->second, i->first ) )
			RemoveInternalPoints( res.points, &i->second, i->first );
		AddFaces( &res1, i->second );
	}
	StripOppositeFacingPolygons( &res, res1 );
	OptimizeVertices( &res1, res );
	Assign( pValue, res1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallObjectInfoClipper::GetClipPlane( SPlane *pPlane, short nClip, bool bLeft )
{
	const float F_INV_SQRT2 = 1.0f / SQRT_2;

	bool  bPerpen  = nClip & 0x1;
	bool  bTop     = (nClip >> 3) & 0x1;
	int   nInWidth = (nClip >> 1) & 0x3;
	if ( 0 == nInWidth ) // ��� �������� ��������� ������
		return false;
	float fInWidth = ID2Width( nInWidth );
	short nSrcWidthLen = clipInfo.nClip >> 16;
	int   nWidth   = nSrcWidthLen & 0xff;
	float fWidth   = ID2Width( nWidth );
	float fLength  = FP_GRID_STEP * (nSrcWidthLen >> 8);

	// ���������������� ������
	if ( bPerpen )
	{
		// ���������� ������� -> ���� ��������� ��� 45 ����.
		if ( nInWidth == nWidth )
		{
			if ( bLeft )
			{
				if ( bTop)
					*pPlane = SPlane( CVec3( F_INV_SQRT2, -F_INV_SQRT2, 0 ), 0 );
				else
					*pPlane = SPlane( CVec3( F_INV_SQRT2, F_INV_SQRT2, 0 ), 0 );
			}
			else
			{
				float d = -F_INV_SQRT2 * fLength;
				if ( bTop)
					*pPlane = SPlane( -CVec3( F_INV_SQRT2, -F_INV_SQRT2, 0 ), d );
				else
					*pPlane = SPlane( -CVec3( F_INV_SQRT2, F_INV_SQRT2, 0 ), d );
			}
			return true;
		}
		// ������ ������� -> ���� ��������� ����� ���� ���������
		else if ( nInWidth > nWidth )
		{
			if ( bLeft )
				*pPlane = SPlane( CVec3( 1, 0, 0 ), 0.5f * fInWidth );
			else
				*pPlane = SPlane( CVec3( -1, 0, 0 ), -(fLength - 0.5f * fInWidth) );
			return true;
		}
		return false;
	}
	// ������������ ������
	/*
	// ���� ��������� ����� ������, �� �� �������
	if ( fInWidth < fWidth )
	return false;
	*/
	if ( bLeft )
	{
		pPlane->n = CVec3( 1.0f, 0, 0 );
		//pPlane->d = 0.5f * fInWidth;
		pPlane->d = 0;
	}
	else
	{
		pPlane->n = CVec3( -1.0f, 0, 0 );
		//pPlane->d = -(fLength - 0.5f * fInWidth);
		pPlane->d = -fLength;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallObjectInfoClipper::SetKey( const SClipShare &key ) 
{
	clipInfo = key; 
	pSrc = shareObjInfoPieces.Get( clipInfo.src );
	pSrc.Refresh();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallObjectInfoClipper::Recalc()
{
	pSrc.Refresh();
	if ( !IsValid( pSrc->GetValue() ) )
		return;
	pValue = new CObjectInfo;
	if ( !pSrc->GetValue()->faces.empty() )
		ClipWall();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSolidObjectInfoClipper
////////////////////////////////////////////////////////////////////////////////////////////////////
// �������� ������� �� ���������
void CSolidObjectInfoClipper::ClipSolid()
{
	CPieceMap &faces = pSrc->GetValue()->faces;
	vector<int> visibleParts;
	UnpackParts( &visibleParts, clipInfo.dwParts, clipInfo.nSubBlockID );

	SFacesVector res, res1;
	res.points.reserve( 1024 );
	res.polys.Clear();
	for ( int k = 0; k < visibleParts.size(); ++k )
	{
		CPieceMap::const_iterator it = faces.find( visibleParts[k] );
		if ( it != faces.end() )
			AddFaces( &res, it->second );
	}
	StripOppositeFacingPolygons( &res1, res );
	OptimizeVertices( &res, res1 );
	Assign( pValue, res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidObjectInfoClipper::SetKey( const SClipShare &key ) 
{
	clipInfo = key; 
	pSrc = shareObjInfoPieces.Get( clipInfo.src );
	pSrc.Refresh();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidObjectInfoClipper::Recalc()
{
	pSrc.Refresh();
	if ( !IsValid( pSrc->GetValue() ) )
		return;
	pValue = new CObjectInfo;
	if ( !pSrc->GetValue()->faces.empty() )
		ClipSolid();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02741123, CObjectInfoLoader )
REGISTER_SAVELOAD_CLASS( 0x02741130, CObjectInfoPiecesLoader )
REGISTER_SAVELOAD_CLASS( 0x02741131, CWallObjectInfoClipper )
REGISTER_SAVELOAD_CLASS( 0x00571120, CSolidObjectInfoClipper )
REGISTER_SAVELOAD_CLASS( 0x020a2171, CAIGeometryConverter )
REGISTER_SAVELOAD_CLASS( 0x023a2110, CAISkinObjectConverter )