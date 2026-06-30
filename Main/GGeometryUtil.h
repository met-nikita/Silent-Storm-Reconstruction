#ifndef __GGeometryUtil_H_
#define __GGeometryUtil_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GGeometry.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
template<int N_SIZE>
struct SVxCache
{
	WORD nData[N_SIZE];
	int nPos;

	SVxCache() { memset( nData, 0xffff, sizeof(nData) ); nPos = 0; }
	void Push( WORD n ) { if ( IsIn( n ) ) return; nData[(++nPos)%N_SIZE] = n; }
	bool IsIn( WORD n ) const { for ( int k = 0; k < N_SIZE; ++k ) if ( nData[k] == n ) return true; return false; }
	int GetPos( WORD n ) const { for ( int k = 0; k < N_SIZE; ++k ) if ( nData[k] == n ) return (nPos - k + N_SIZE ) % N_SIZE; return -1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_VX_CACHE_SIZE = 16;
const int N_VX_CACHE_SIZE_EFF = N_VX_CACHE_SIZE - 4;
class CVertexCacheOptimizer
{
public:
	typedef CObjectInfo::SLightmapInfo CQuad;
private:
	vector<vector<int> > quadPerVertex; // list of incident quad for each vertex
	vector<int> freeLinks; // number of free (not used) quads incident to this vertex
	vector<char> isUsed; // true if quad already out
	SVxCache<N_VX_CACHE_SIZE_EFF> vxCache;

	void AddVertex( int nVertex, int nQuad )
	{
		if ( nVertex >= quadPerVertex.size() )
			quadPerVertex.resize( nVertex + 1 );
		quadPerVertex[nVertex].push_back( nQuad );
	}
	void MeasureEfficiency( const vector<CQuad> &quads );
	int SearchBest( const vector<CQuad> &quads );
public:
	void Optimize( vector<CQuad> *pQuads, vector<int> *pNewPlaces );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//const int N_VX_TRI_CACHE_SIZE = 10;//24;
class CTriVertexCacheOptimizer
{
private:
	vector<int> triPerVertex;
	vector<int> tpvIndex;
	vector<int> freeLinks;
	vector<char> isUsed;
	vector<int> cachePos;
	vector<int> cache;
	vector<int> outQueue, outPrevPos;
	int nVertices;
	bool bEvictedReferencedVertex;
	int nVCacheSize;

	void CountVertices( const vector<STriangle> &tris );
	void Init( const vector<STriangle> &tris );
	struct SBestSearch
	{
		int nBestIdx, nBestCached, nBestFreeLinks, nBestDistance;
		CTriVertexCacheOptimizer *pOptimizer;
		const vector<STriangle> &tris;
		SBestSearch( CTriVertexCacheOptimizer *pO, const vector<STriangle> &_tris ) : nBestIdx( -1 ), nBestCached( -1 ), pOptimizer(pO), tris(_tris) {}
		bool GetL( int *pnResDistance, int n, int nVertex, int nVCacheSize ) 
		{
			if ( n <= *pnResDistance )
				return false;
			if ( n + 1 >= nVCacheSize )
				return false;
			if ( n + pOptimizer->freeLinks[nVertex] * 3 + 1 < nVCacheSize )
			{
				*pnResDistance = n;
				return true;
			}
			int nFL = pOptimizer->CountNotCachedFL( tris, nVertex );
			ASSERT( nFL <= pOptimizer->freeLinks[nVertex] * 3 );
			if ( n + nFL + 1 >= nVCacheSize ) 
				return false; 
			*pnResDistance = n;
			return true;// - 0 * fl;// - 3 * fl;
		}
		bool HasFound() const { return nBestCached == 3; }
		void Try( int nTri )
		{
			const STriangle &q = tris[nTri];
			int nCached = 0;
			int nDistance = 0;
			int nFreeLinks = 0;
			//int nQDistance;
			int nVCacheSize = pOptimizer->nVCacheSize;

			nCached += pOptimizer->OutputVertex( q.i1 );
			nCached += pOptimizer->OutputVertex( q.i2 );
			nCached += pOptimizer->OutputVertex( q.i3 );

			if ( nCached == 3 )
			{
				pOptimizer->ReverseVertex();
				pOptimizer->ReverseVertex();
				pOptimizer->ReverseVertex();
				nBestIdx = nTri;
				nBestCached = nCached;
				return;
			}
			if ( nCached < nBestCached )
			{
				pOptimizer->ReverseVertex();
				pOptimizer->ReverseVertex();
				pOptimizer->ReverseVertex();
				return;
			}
			ASSERT( !pOptimizer->isUsed[nTri] );
			pOptimizer->isUsed[ nTri ] = 1;
			int nCachePos = pOptimizer->cache.size() - 1;
			int nQDistance;
			nQDistance = nCachePos - pOptimizer->cachePos[ q.i1 ];
			GetL( &nDistance, nQDistance, q.i1, nVCacheSize );

			nQDistance = nCachePos - pOptimizer->cachePos[ q.i2 ];
			GetL( &nDistance, nQDistance, q.i2, nVCacheSize );

			nQDistance = nCachePos - pOptimizer->cachePos[ q.i3 ];
			GetL( &nDistance, nQDistance, q.i3, nVCacheSize );

			nFreeLinks = Max( nFreeLinks, pOptimizer->freeLinks[ q.i1 ] );
			nFreeLinks = Max( nFreeLinks, pOptimizer->freeLinks[ q.i2 ] );
			nFreeLinks = Max( nFreeLinks, pOptimizer->freeLinks[ q.i3 ] );

			pOptimizer->ReverseVertex();
			pOptimizer->ReverseVertex();
			pOptimizer->ReverseVertex();
			pOptimizer->isUsed[ nTri ] = 0;

			if ( nCached == 2 )
				nCached = 1;

			if ( nCached > nBestCached )
			{
				nBestIdx = nTri;
				nBestCached = nCached;
				nBestFreeLinks = nFreeLinks;
				nBestDistance = nDistance;
			}
			else if ( nCached == nBestCached )
			{
				if ( nDistance > nBestDistance )
				{
					nBestIdx = nTri;
					nBestFreeLinks = nFreeLinks;
					nBestDistance = nDistance;
				}
				else if ( nDistance == nBestDistance )
				{
					if ( nFreeLinks < nBestFreeLinks )
					{
						nBestFreeLinks = nFreeLinks;
						nBestIdx = nTri;
					}
				}
			}
		}
	};
	int SearchBest( const vector<STriangle> &tris );
	void MeasureEfficiency( const vector<STriangle> &tris );
	bool OutputVertex( int n );
	void ReverseVertex();
	int CountNotCachedFL( const vector<STriangle> &tris, int nVertex );
	void OptimizeVertexOrder( vector<STriangle> &tris, vector<WORD> *pVertexReorder );
public:
	void Optimize( vector<STriangle> *pTris, vector<WORD> *pVertexReorder, int nVCacheSize );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdgeInfo
{
	int nU, nV, nXSize, nYSize;
};
inline bool operator==( const SEdgeInfo &a, const SEdgeInfo &b ) { return memcmp( &a, &b, sizeof(a) ) == 0; }
struct SEdgeInfoHash
{
	int operator()( const SEdgeInfo &e ) const { return e.nU ^ e.nV ^ e.nXSize ^ e.nYSize; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSquarePacker
{
	struct STriInfo
	{
		int n1, n2, n3, n4;
		bool bInverse, bInverse1;
		char nFirst, nFirst1;
		WORD nXSize, nYSize;
		STriInfo() {}
		STriInfo( int _n1, int _n2, int _n3, bool _bInverse, char _nFirst, WORD _nXSize, WORD _nYSize )
			: n1(_n1), n2(_n2), n3(_n3), bInverse(_bInverse), nFirst(_nFirst), nFirst1(0), bInverse1(false),nXSize(_nXSize), nYSize(_nYSize){}
	};
	vector<STriInfo> squares;
	CObjectInfo &info;
	float fTexelsPerMeter;
	typedef unordered_map<SEdgeInfo, int, SEdgeInfoHash> CEdgesHash;
	CEdgesHash edgesInfo;
	int nMaxSize;

	void PushTriangle( int n1, int n2, int n3, char nFirst );
public:
	CSquarePacker( int _nMaxSize, CObjectInfo &_info, float _fTexelPerMeter )
		: info(_info), fTexelsPerMeter(_fTexelPerMeter), nMaxSize(_nMaxSize)
	{
	}
	void AddTriangle( int n1, int n2, int n3 );
	void Build();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
