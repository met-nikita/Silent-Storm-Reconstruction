#include "StdAfx.h"
#include "GGeometryUtil.h"

/*#include <d3dx8.h>
#include "..\Misc\Win32Helper.h"
namespace NGfx
{
	externA5 NWin32Helper::com_ptr<IDirect3DDevice8> pDevice;
}*/
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVertexCacheOptimizer::MeasureEfficiency( const vector<CQuad> &quads )
{
	SVxCache<N_VX_CACHE_SIZE> cache;
	int nMisses = 0, nTotal = 0;
	for ( int k = 0; k < quads.size(); ++k )
	{
		const CQuad &q = quads[k];
		if ( !cache.IsIn( q.n1 ) )
			++nMisses;
		cache.Push( q.n1 );
		if ( !cache.IsIn( q.n2 ) )
			++nMisses;
		cache.Push( q.n2 );
		if ( !cache.IsIn( q.n3 ) )
			++nMisses;
		cache.Push( q.n3 );
		nTotal += 3;
		if ( q.n4 != 0xffff )
		{
			if ( !cache.IsIn( q.n4 ) )
				++nMisses;
			cache.Push( q.n4 );
			++nTotal;
		}
	}
	char szBuf[1024];
	sprintf( szBuf, "vcache efficiency %g\n", 1.0f - nMisses / (float)(nTotal) );
	OutputDebugString( szBuf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CVertexCacheOptimizer::SearchBest( const vector<CQuad> &quads )
{
	int nBestIdx = -1, nBestCached = -1, nBestFreeLinks;
	for ( int k = 0; k < quads.size(); ++k )
	{
		if ( isUsed[k] )
			continue;
		int nCached = 0;
		const CQuad &q = quads[k];
		if ( vxCache.IsIn( q.n1 ) )
			++nCached;
		if ( vxCache.IsIn( q.n2 ) )
			++nCached;
		if ( vxCache.IsIn( q.n3 ) )
			++nCached;
		if ( q.n4 != 0xffff && vxCache.IsIn( q.n4 ) )
			++nCached;
		if ( nCached >= nBestCached )
		{
			if ( nCached > nBestCached )
				nBestFreeLinks = 0x7fffffff;
			nBestCached = nCached;
			int nFreeLinks = Max( Max( freeLinks[q.n1], freeLinks[q.n2] ), freeLinks[q.n3] );
			if ( q.n4 != 0xffff )
				nFreeLinks = Max( nFreeLinks, freeLinks[q.n4] );
			if ( nFreeLinks < nBestFreeLinks )
			{
				nBestFreeLinks = nFreeLinks;
				nBestIdx = k;
			}
		}
	}
	ASSERT( nBestIdx >= 0 );
	return nBestIdx;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVertexCacheOptimizer::Optimize( vector<CQuad> *pQuads, vector<int> *pNewPlaces )
{
	//MeasureEfficiency( *pQuads );
	// prepare some data structures
	for ( int k = 0; k < pQuads->size(); ++k )
	{
		const CQuad &q = (*pQuads)[k];
		AddVertex( q.n1, k );
		AddVertex( q.n2, k );
		AddVertex( q.n3, k );
		if ( q.n4 != 0xffff )
			AddVertex( q.n4, k );
	}
	freeLinks.resize( quadPerVertex.size() );
	for ( int k = 0; k < freeLinks.size(); ++k )
		freeLinks[k] = quadPerVertex[k].size();
	// form result
	vector<CQuad> res;
	vector<int> &newPlaces = *pNewPlaces;
	newPlaces.resize( pQuads->size() );
	isUsed.resize( pQuads->size(), false );
	for ( int k = 0; k < pQuads->size(); ++k )
	{
		// search for a best candidate
		int nBest = SearchBest( *pQuads );
		ASSERT( !isUsed[nBest] );
		const CQuad &q = (*pQuads)[nBest];
		--freeLinks[q.n1];
		--freeLinks[q.n2];
		--freeLinks[q.n3];
		if ( q.nOrder1 & 4 )
		{
			vxCache.Push( q.n1 );
			vxCache.Push( q.n3 );
			vxCache.Push( q.n2 );
		}
		else
		{
			vxCache.Push( q.n1 );
			vxCache.Push( q.n2 );
			vxCache.Push( q.n3 );
		}
		if ( q.n4 != 0xffff )
		{
			--freeLinks[q.n4];
			if ( q.nOrder2 & 4 )
			{
				vxCache.Push( q.n4 );
				vxCache.Push( q.n3 );
				vxCache.Push( q.n2 );
			}
			else
			{
				vxCache.Push( q.n4 );
				vxCache.Push( q.n2 );
				vxCache.Push( q.n3 );
			}
		}
		isUsed[nBest] = true;
		newPlaces[k] = nBest;
		res.push_back( q );
	}
	//MeasureEfficiency( res );
	*pQuads = res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTriVertexCacheOptimizer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::CountVertices( const vector<STriangle> &tris )
{
	int nMax = 0;
	for ( int k = 0; k < tris.size(); ++k )
		nMax = Max( nMax, Max( (int)tris[k].i1, Max( (int)tris[k].i2, (int)tris[k].i3 ) ) );
	nVertices = nMax + 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::Init( const vector<STriangle> &tris )
{
	tpvIndex.resize( nVertices + 1 );
	freeLinks.resize( 0 );
	freeLinks.resize( nVertices, 0 );
	isUsed.resize( 0 );
	isUsed.resize( tris.size(), false );
	cachePos.resize( 0 );
	cachePos.resize( nVertices, 0 );
	cache.resize( 100, -1 );
	//nCachePos = cache.size() - 1;
	// calc freeLinks
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &t = tris[k];
		++freeLinks[ t.i1 ];
		++freeLinks[ t.i2 ];
		++freeLinks[ t.i3 ];
	}
	tpvIndex[0] = 0;
	for ( int k = 1; k < tpvIndex.size(); ++k )
		tpvIndex[k] = tpvIndex[k-1] + freeLinks[k-1];
	triPerVertex.resize( tpvIndex.back() );
	vector<int> writePtr( tpvIndex );
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &t = tris[k];
		triPerVertex[ writePtr[ t.i1 ]++ ] = k;
		triPerVertex[ writePtr[ t.i2 ]++ ] = k;
		triPerVertex[ writePtr[ t.i3 ]++ ] = k;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTriVertexCacheOptimizer::SearchBest( const vector<STriangle> &tris )
{
	SBestSearch bs( this, tris );
	for ( int nDistance = nVCacheSize - 1; nDistance >= 0; --nDistance )
	{
		int nVertex = cache[ cache.size() - nDistance - 1 ];
		if ( nVertex < 0 )
			continue;
		for ( int k = tpvIndex[nVertex]; k < tpvIndex[nVertex + 1]; ++k )
		{
			int nTri = triPerVertex[ k ];
			if ( isUsed[nTri] )
				continue;
			bs.Try( nTri );
			if ( bs.HasFound() )
				break;
		}
	}
	if ( bs.nBestIdx == -1 || bs.nBestCached == 0 )
	{
		// start over
		for ( int k = 0; k < tris.size(); ++k )
		{
			if ( isUsed[k] )
				continue;
			bs.Try( k );
		}
	}
	ASSERT( bs.nBestIdx != -1 );
	return bs.nBestIdx;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::MeasureEfficiency( const vector<STriangle> &tris )
{
	SVxCache<10> cache;
	int nMisses = 0, nTotal = 0;
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &q = tris[k];
		if ( !cache.IsIn( q.i1 ) )
			++nMisses;
		cache.Push( q.i1 );
		if ( !cache.IsIn( q.i2 ) )
			++nMisses;
		cache.Push( q.i2 );
		if ( !cache.IsIn( q.i3 ) )
			++nMisses;
		cache.Push( q.i3 );
		nTotal += 3;
	}
	char szBuf[1024];
	sprintf( szBuf, "vertices per triangle = %g\n", 3 * nMisses / (float)(nTotal) );
	OutputDebugString( szBuf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTriVertexCacheOptimizer::OutputVertex( int n )
{
	--freeLinks[n];
	int nQDistance;
	int nCachePos = cache.size() - 1;
	nQDistance = nCachePos - cachePos[ n ];
	outQueue.push_back( n );
	outPrevPos.push_back( cachePos[n] );
	if ( nQDistance >= nVCacheSize )
	{
		//if ( cachePos[ n ] != 0 )
		//	bEvictedReferencedVertex = true;
		cache.push_back( n );
		++nCachePos;
		cachePos[ n ] = nCachePos;
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::ReverseVertex()
{
	int n = outQueue.back();
	if ( outPrevPos.back() != cachePos[n] )
	{
		// was cached
		cachePos[n] = outPrevPos.back();
		ASSERT( cache.back() == n );
		cache.pop_back();
	}
	++freeLinks[n];
	outQueue.pop_back();
	outPrevPos.pop_back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTriVertexCacheOptimizer::CountNotCachedFL( const vector<STriangle> &tris, int nVertex )
{
	/*#ifdef _DEBUG
	int nChk = 0;
	for ( int k = tpvIndex[nVertex]; k < tpvIndex[nVertex+1]; ++k )
	{
	int nTri = triPerVertex[k];
	nChk += !isUsed[nTri];
	}
	ASSERT( freeLinks[nVertex] == nChk );
	#endif*/
	int nRes = 0;
	int nState = outQueue.size();
	for ( int k = tpvIndex[nVertex]; k < tpvIndex[nVertex+1]; ++k )
	{
		int nTri = triPerVertex[k];
		if ( isUsed[nTri] )
			continue;
		const STriangle &t = tris[nTri];
		nRes += !OutputVertex( t.i1 );
		nRes += !OutputVertex( t.i2 );
		nRes += !OutputVertex( t.i3 );
	}
	while ( outQueue.size() != nState )
		ReverseVertex();
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::OptimizeVertexOrder( vector<STriangle> &tris, vector<WORD> *pVertexReorder )
{
	vector<WORD> &position = *pVertexReorder;
	position.resize(0);
	position.resize( nVertices, 0xffff );
	WORD nPos = 0;
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &q = tris[k];
		if ( position[q.i1] == 0xffff )
			position[q.i1] = nPos++;
		if ( position[q.i2] == 0xffff )
			position[q.i2] = nPos++;
		if ( position[q.i3] == 0xffff )
			position[q.i3] = nPos++;
	}
	// clean up to avoid crashes
	for ( int k = 0; k < position.size(); ++k )
	{
		if ( position[k] == 0xffff )
			position[k] = nPos;
	}
	ASSERT( nPos <= position.size() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriVertexCacheOptimizer::Optimize( vector<STriangle> *pTris, vector<WORD> *pVertexReorder, int _nVCacheSize )
{
	nVCacheSize = _nVCacheSize;
	/*vector<STriangle> tt;
	for ( int y = 0; y < 8; ++y )
	{
	for ( int x = 0; x < 8; ++x )
	{
	tt.push_back( STriangle( (y)*9 + x  , (y)*9 + x+1, (y+1)*9+x ) );
	tt.push_back( STriangle( (y)*9 + x+1, (y+1)*9 + x, (y+1)*9+x+1 ) );
	}
	}
	*pTris = tt;*/
	if ( pTris->size() <= nVCacheSize / 3 )
	{
		CountVertices( *pTris );
		OptimizeVertexOrder( *pTris, pVertexReorder );
		return;
	}
//	DebugTrace( "%d tris\n", pTris->size() );
//	MeasureEfficiency( *pTris );
	// prepare some data structures
	CountVertices( *pTris );
	Init( *pTris );
	// form result
	vector<STriangle> res;
	for ( int k = 0; k < pTris->size(); ++k )
	{
		// search for a best candidate
		int nBest = SearchBest( *pTris );
		ASSERT( !isUsed[nBest] );
		const STriangle &q = (*pTris)[nBest];
		OutputVertex( q.i1 );
		OutputVertex( q.i2 );
		OutputVertex( q.i3 );
		isUsed[nBest] = true;
		res.push_back( q );
		//if ( bEvictedReferencedVertex )
		//	break;
	}
/*	if ( bEvictedReferencedVertex )
	{
//		OutputDebugString( "non trivial mesh, using d3dx optimizer\n" );
		int nTris = pTris->size();
		NWin32Helper::com_ptr<ID3DXMesh> pMesh;
		D3DXCreateMeshFVF( nTris, nVertices, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, NGfx::pDevice, pMesh.GetAddr() );
		STriangle *pBuf;
		pMesh->LockIndexBuffer( 0, (BYTE**)&pBuf );
		for ( int k = 0; k < nTris; ++k )
			pBuf[k] = (*pTris)[k];
		pMesh->UnlockIndexBuffer();
		vector<DWORD> dwAdj( nTris * 3 );
		vector<DWORD> dwRes( nTris );
		pMesh->ConvertPointRepsToAdjacency( 0, &dwAdj[0] );
		NWin32Helper::com_ptr<ID3DXBuffer> pReorder;
		pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, &dwAdj[0], 0, &dwRes[0], pReorder.GetAddr() );
		ASSERT( pReorder->GetBufferSize() == nVertices * 4 );
		pVertexReorder->resize( nVertices );
		DWORD *pReorderBuf = (DWORD*)pReorder->GetBufferPointer();
		for ( int k = 0; k < nVertices; ++k )
			(*pVertexReorder)[k] = pReorderBuf[k];
		res.resize( nTris );
		for ( int k = 0; k < res.size(); ++k )
			res[k] = (*pTris)[ dwRes[k] ];
	}
	else*/
	{
		*pTris = res;
//		MeasureEfficiency( *pTris );
		OptimizeVertexOrder( *pTris, pVertexReorder );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSquarePacker
////////////////////////////////////////////////////////////////////////////////////////////////////
// n1 - vertex with largest angle
void CSquarePacker::PushTriangle( int n1, int n2, int n3, char nFirst )
{
	const CVec3 &v1 = info.GetPositions()[ info.GetPositionIndices()[n1] ];
	const CVec3 &v2 = info.GetPositions()[ info.GetPositionIndices()[n2] ];
	const CVec3 &v3 = info.GetPositions()[ info.GetPositionIndices()[n3] ];
	// calc sizes
	float fSize21 = fabs( v2 - v1 );
	float fSize31 = fabs( v3 - v1 );
	// select largest size (&swap indices if needed)
	bool bInverse = false;
	if ( fSize31 > fSize21 )
	{
		swap( fSize21, fSize31 );
		swap( n2, n3 );
		bInverse = true;
	}
	// add to storage
	int nXSize = Max( Float2Int( fSize21 * fTexelsPerMeter ), 2 );
	int nYSize = Max( Float2Int( fSize31 * fTexelsPerMeter ), 2 );
	ASSERT( nYSize <= nXSize );
	nXSize = Min( nXSize, nMaxSize );
	nYSize = Min( nYSize, nXSize );
	if ( nXSize == nYSize && n2 > n3 )
	{
		swap( n2, n3 );
		bInverse = !bInverse;
	}
	SEdgeInfo edge;
	edge.nU = n2;
	edge.nV = n3;
	edge.nXSize = nXSize;
	edge.nYSize = nYSize;
	CEdgesHash::iterator i = edgesInfo.find( edge );
	if ( i != edgesInfo.end() )
	{
		STriInfo &tri = squares[i->second];
		if ( tri.n2 != n2 )
		{
			swap( n2, n3 );
			bInverse = !bInverse;
		}
		ASSERT( tri.n2 == n2 && tri.n3 == n3 );
		tri.n4 = n1;
		tri.nFirst1 = nFirst;
		tri.bInverse1 = bInverse;
		edgesInfo.erase( i );
		return;
	}
	if ( nXSize != nYSize )
		swap( edge.nU, edge.nV );
	edgesInfo[edge] = squares.size();
	squares.push_back( STriInfo( n1, n2, n3, bInverse, nFirst, nXSize, nYSize ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSquarePacker::AddTriangle( int n1, int n2, int n3 )
{
	// select largest angle
	const CVec3 &v1 = info.GetPositions()[ info.GetPositionIndices()[n1] ];
	const CVec3 &v2 = info.GetPositions()[ info.GetPositionIndices()[n2] ];
	const CVec3 &v3 = info.GetPositions()[ info.GetPositionIndices()[n3] ];
	float f1 = fabs2( v2 - v3 );
	float f2 = fabs2( v1 - v3 );
	float f3 = fabs2( v1 - v2 );
	if ( f2 > f1 )
	{
		if ( f2 > f3 )
			PushTriangle( n2, n3, n1, 2 );
		else
			PushTriangle( n3, n1, n2, 3 );
	}
	else
	{
		if ( f3 > f1 )
			PushTriangle( n3, n1, n2, 3 );
		else
			PushTriangle( n1, n2, n3, 1 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSquarePacker::Build()
{
	info.lmLODs.resize( N_LM_LODS );
	CObjectInfo::SLMLOD &lmBest = info.lmLODs[0];
	info.lmaps.resize( squares.size() );
	for ( int i = 0; i < info.lmLODs.size(); ++i )
		info.lmLODs[i].lmaps.resize( squares.size() );
	for ( int i = 0; i < squares.size(); ++i )
	{
		const STriInfo &tri = squares[i];
		CObjectInfo::SLightmapInfo &res = info.lmaps[i];
		res.n1 = tri.n1;
		res.n2 = tri.n2;
		res.n3 = tri.n3;
		res.nOrder1 = tri.nFirst;
		if ( tri.bInverse )
			res.nOrder1 |= 4;
		++info.nTris;
		if ( tri.nFirst1 )
		{
			res.n4 = tri.n4;
			res.nOrder2 = tri.nFirst1;
			if ( tri.bInverse1 )
				res.nOrder2 |= 4;
			++info.nTris;
		}
		else
		{
			res.n4 = 0xffff;
			res.nOrder2 = 0;
		}
		NRectPacker::SRect &r = lmBest.lmaps[i];
		r.nXSize = tri.nXSize;
		r.nYSize = tri.nYSize;
	}
	for ( int k = 1; k < info.lmLODs.size(); ++k )
	{
		CObjectInfo::SLMLOD &lmLOD = info.lmLODs[k];
		int nShift = k == info.lmLODs.size() - 1 ? 30 : k;
		for ( int i = 0; i < lmLOD.lmaps.size(); ++i )
		{
			const NRectPacker::SRect &src = lmBest.lmaps[i];
			NRectPacker::SRect &r = lmLOD.lmaps[i];
			r.nXSize = Max( 2, src.nXSize >> nShift );
			r.nYSize = Max( 2, src.nYSize >> nShift );
		}
	}
	for ( int k = 0; k < info.lmLODs.size(); ++k )
	{
		CObjectInfo::SLMLOD &lmLOD = info.lmLODs[k];
		NRectPacker::PackRects( &lmLOD.lmaps, &lmLOD.lmSize );
		//		ASSERT( lmLOD.lmSize.x <= nLMSize );
		//		ASSERT( lmLOD.lmSize.y <= nLMSize );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
