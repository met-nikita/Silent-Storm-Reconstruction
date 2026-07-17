#include "StdAfx.h"
#include "GGeometry.h"
#include "Bound.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "GGeometryUtil.h"

namespace NGfx
{
	externA5 int nVCacheSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void FilterTrinagles( vector<STriangle> *pRes, const vector<WORD> &filter )
{
	int nTarget = 0;
	for ( int k = 0; k < pRes->size(); ++k )
	{
		const STriangle &src = (*pRes)[k];
		STriangle &res = (*pRes)[nTarget];
		res.i1 = filter[ src.i1 ];
		res.i2 = filter[ src.i2 ];
		res.i3 = filter[ src.i3 ];
		nTarget += (res.i1 != res.i2) & (res.i1 != res.i3) & (res.i2 != res.i3);
	}
	pRes->resize( nTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MergePositions( vector<WORD> *pMatches, vector<CVec3> *pPositions )
{
	vector<CVec3> mergedPositions;
	vector<CVec3> &positions = *pPositions;
	vector<WORD> &posIndices = *pMatches;
	posIndices.resize( positions.size() );
	mergedPositions.reserve( pPositions->size() );
	typedef unordered_map<CVec3,int,SVec3Hash> CPosHash;
	CPosHash posHash;
	for ( int k = 0; k < positions.size(); ++k )
	{
		int nRes;
		CPosHash::iterator i = posHash.find( positions[k] );
		if ( i == posHash.end() )
		{
			nRes = mergedPositions.size();
			mergedPositions.push_back( positions[k] );
			posHash[ positions[k] ] = nRes;
		}
		else
			nRes = i->second;
		posIndices[k] = nRes;
	}
	positions = mergedPositions;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SPolygonIndices
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPolygonIndices::GetTriangles( vector<STriangle> *pRes ) const
{
	pRes->resize( GetTrianglesCount() );
	if ( pRes->empty() )
		return;
	STriangle *pTri = &(*pRes)[0];

	int i1 = 0, i2 = 2;
	int nPoly = 1;
	int nPolys = polys.size();
	while ( nPoly < nPolys )
	{
		ASSERT( polys[nPoly] != polys[nPoly-1] );
		pTri->i1 = indices[ i1 ];
		pTri->i2 = indices[ i2 - 1 ];
		pTri->i3 = indices[ i2 ];
		++pTri;
		if ( ++i2 == polys[nPoly] )
		{
			i1 = i2;
			i2 = i1 + 2;
			++nPoly;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPolygonIndices::SetTriangles( const vector<STriangle> &tris )
{
	int nTris = tris.size();

	indices.resize( nTris * 3 );
	polys.resize( nTris + 1 );
	polys[0] = 0;
	if ( nTris == 0 )
		return;
	int ind = -1;
	const STriangle *pTri = &tris[0];
	for ( int i = 0; i < nTris; ++i, ++pTri )
	{
		polys[i+1] = (i+1) * 3;
		indices[ ++ind ] = pTri->i1;
		indices[ ++ind ] = pTri->i2;
		indices[ ++ind ] = pTri->i3;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPolygonIndices::AddTriangles( const vector<STriangle> &tris, int nBaseIndex )
{
	ASSERT( !polys.empty() );
	int nTris = tris.size();
	if ( nTris == 0 )
		return;
	int nBasePoly = polys.size();
	int ind = indices.size() - 1;
	polys.resize( nBasePoly + nTris );
	indices.resize( ind + 1 + nTris * 3 );
	const STriangle *pTri = &tris[0];
	for ( int i = 0; i < nTris; ++i, ++pTri )
	{
		indices[ ++ind ] = pTri->i1 + nBaseIndex;
		indices[ ++ind ] = pTri->i2 + nBaseIndex;
		indices[ ++ind ] = pTri->i3 + nBaseIndex;
		polys[ i + nBasePoly ] = ind + 1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPolygonIndices::Filter( const vector<WORD> &posIndices )
{
	vector<WORD> findices;
	vector<WORD> fpolys;
	fpolys.push_back(0);
	for ( int k = 1; k < polys.size(); ++k )
	{
		WORD nPrev = posIndices[ indices[ polys[k] - 1 ] ];
		for ( int i = polys[k-1]; i < polys[k]; ++i )
		{
			WORD nCurPos = posIndices[ indices[i] ];
			if ( nCurPos != nPrev )
				findices.push_back( posIndices[ indices[i] ] );
			nPrev = nCurPos;
		}
		if ( findices.size() - fpolys.back() < 3 )
			findices.resize( fpolys.back() );
		else
			fpolys.push_back( findices.size() );
	}
	indices = findices;
	polys = fpolys;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectInfo 
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRefTracker
{
	vector<int> temp;

	void SetSize( int n )
	{
		temp.resize( n );
		for ( int k = 0; k < temp.size(); ++k )
			temp[k] = -1;
	}
	int GetRef( int nIndex, int nPos )
	{
		int &nRef = temp[ nIndex ];
		if ( nRef == -1 )
		{
			nRef = nPos;
			return nPos;
		}
		return nRef;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::EstablishRefs()
{
	SRefTracker rt, rtPos;
	rt.SetSize( positions.size() );
	vertRefPositions.resize( verts.size() );
	for ( int k = 0; k < vertRefPositions.size(); ++k )
		vertRefPositions[k] = rt.GetRef( posIndices[k], k );

	rt.SetSize( verts.size() );
	rtPos.SetSize( positions.size() );
	lmRefVertices.resize( lmaps.size() * 4 );
	lmRefPositions.resize( lmaps.size() * 4 );
	for ( int k = 0; k < lmaps.size(); ++k )
	{
		const SLightmapInfo &lm = lmaps[k];
		lmRefVertices[k*4 + 0] = rt.GetRef( lm.n1, k*4 + 0 );
		lmRefPositions[k*4 + 0] = rtPos.GetRef( posIndices[ lm.n1 ], k*4 + 0 );
		lmRefVertices[k*4 + 1] = rt.GetRef( lm.n2, k*4 + 1 );
		lmRefPositions[k*4 + 1] = rtPos.GetRef( posIndices[ lm.n2 ], k*4 + 1 );
		lmRefVertices[k*4 + 2] = rt.GetRef( lm.n3, k*4 + 2 );
		lmRefPositions[k*4 + 2] = rtPos.GetRef( posIndices[ lm.n3 ], k*4 + 2 );
		if ( lm.n4 != 0xffff )
		{
			lmRefVertices[k*4 + 3] = rt.GetRef( lm.n4, k*4 + 3 );
			lmRefPositions[k*4 + 3] = rtPos.GetRef( posIndices[ lm.n4 ], k*4 + 3 );
		}
		else
		{
			lmRefVertices[k*4 + 3] = 0;
			lmRefPositions[k*4 + 3] = 0;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCompoundPosKey
{
	CVec3 v;
	SRealVertexWeight w;

	SCompoundPosKey() {}
	SCompoundPosKey( const CVec3 &_v, const SRealVertexWeight &_w ) : v(_v), w(_w) {}
	bool operator== ( const SCompoundPosKey &k ) const { return v == k.v && w == k.w; }
};
struct CalcCompoundKeyHash
{
	int operator()( const SCompoundPosKey &k ) const { SVec3Hash v; return v(k.v); }
};
void CObjectInfo::MergePositions()
{
	// merge positions
	if ( weights.size() != positions.size() )
	{
		NGScene::MergePositions( &posIndices, &positions );
		return;
	}
	vector<CVec3> mergedPositions;
	vector<SRealVertexWeight> mergedWeights;
	mergedPositions.reserve( positions.size() );
	mergedWeights.reserve( weights.size() );
	posIndices.resize( positions.size() );
	typedef unordered_map<SCompoundPosKey,int,CalcCompoundKeyHash> CPosHash;
	CPosHash posHash;
	for ( int k = 0; k < positions.size(); ++k )
	{
		int nRes;
		SCompoundPosKey key( positions[k], weights[k] );
		CPosHash::iterator i = posHash.find( key );
		if ( i == posHash.end() )
		{
			nRes = mergedPositions.size();
			mergedPositions.push_back( positions[k] );
			mergedWeights.push_back( weights[k] );
			posHash[ key ] = nRes;
		}
		else
			nRes = i->second;
		posIndices[k] = nRes;
	}
	positions = mergedPositions;
	weights = mergedWeights;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::AssignGeometry( const SData &data )
{
	// initialize stuff
	positions.resize( data.verts.size() );
	verts.resize( data.verts.size() );
	weights.resize( data.weights.size() );
	for ( int k = 0; k < data.weights.size(); ++k )
	{
		SRealVertexWeight &res = weights[k];
		const SVertexWeight &src = data.weights[k];
		for ( int i = 0; i < 4; ++i )
		{
			res.fWeights[i] = src.fWeights[i];
			res.cBoneIndices[i] = src.cBoneIndices[i];
			ASSERT( src.fWeights[i] >= 0 && src.fWeights[i] <= 1 );
			res.nWeights[i] = Clamp( Float2Int( src.fWeights[i] * 256 ), 0, 255 );
		}
		// sort them, we are not in a hurry :)
		for ( int i = 1; i < 4; ++i )
		{
			for ( int k = i - 1; k >= 0; --k )
			{
				if ( res.fWeights[k] < res.fWeights[k+1] )
				{
					swap( res.fWeights[k], res.fWeights[k+1] );
					swap( res.nWeights[k], res.nWeights[k+1] );
					swap( res.cBoneIndices[k], res.cBoneIndices[k+1] );
				}
				else
					break;
			}
		}
		ASSERT( res.fWeights[0] >= res.fWeights[1] && res.fWeights[1] >= res.fWeights[2] && res.fWeights[2] >= res.fWeights[3] );
	}
	for ( int k = 0; k < data.verts.size(); ++k )
	{
		const SVertex &v = data.verts[k];
		SUVInfo &res = verts[k];
		positions[k] = v.pos;
		res.tex.nU = Float2Int( v.tex.u * N_VERTEX_TEX_SIZE );
		res.tex.nV = Float2Int( v.tex.v * N_VERTEX_TEX_SIZE );
		// release @0x5217a0: SVertex already stores normal/texU/texV PRE-packed as
		// SCompactVector, so these are straight DWORD copies (the predecessor packed
		// raw CVec3s here via CalcCompactVector; producers pack now).
		res.normal = v.normal;
		res.texU = v.texU;
		res.texV = v.texV;
	}
	geometry = data.geometry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
static void Reorder( vector<T> *pRes, const vector<int> &newPlaces )
{
	vector<T> tmp( pRes->size() );
	for ( int k = 0; k < tmp.size(); ++k )
		tmp[k] = (*pRes)[ newPlaces[k] ];
	*pRes = tmp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::AssignLM( const SData &data )
{
	AssignGeometry( data );

	MergePositions();
	//if ( geometry.polys.empty() )
	//	geometry.polys.push_back(0);
	//geometry.Filter( posIndices );
	// calc lightmaps
	vector<STriangle> tris;
	/*data.*/geometry.GetTriangles( &tris );
	CSquarePacker packer( N_MAX_LIGHTMAP_SIDE, *this, F_MAX_LM_RESOLUTION );
	for ( int k = 0; k < tris.size(); ++k )
		packer.AddTriangle( tris[k].i1, tris[k].i2, tris[k].i3 );
	nTris = 0;
	packer.Build();

	CVertexCacheOptimizer vxOptimize;
	vector<int> newPlaces;
	vxOptimize.Optimize( &lmaps, &newPlaces );
	for ( int k = 0; k < lmLODs.size(); ++k )
		Reorder( &lmLODs[k].lmaps, newPlaces );
	//
	EstablishRefs();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::Assign( const SData &data )
{
	SData optimizedData;
	if ( data.geometry.GetTrianglesCount() > 0  )
	{
		// optimize for vertex cache
		vector<STriangle> tris;
		data.geometry.GetTriangles( &tris );
		CTriVertexCacheOptimizer vxOptimize;
		vector<WORD> vxReorder;
		vxOptimize.Optimize( &tris, &vxReorder, NGfx::nVCacheSize );
		FilterTrinagles( &tris, vxReorder );
		optimizedData.geometry.SetTriangles( tris );
		optimizedData.verts.resize( data.verts.size() );
		for ( int k = 0; k < data.verts.size(); ++k )
			optimizedData.verts[ vxReorder[k] ] = data.verts[k];
		if ( !data.weights.empty() )
		{
			ASSERT( data.weights.size() == data.verts.size() );
			optimizedData.weights.resize( data.weights.size() );
			for ( int k = 0; k < data.weights.size(); ++k )
				optimizedData.weights[ vxReorder[k] ] = data.weights[k];
		}
	}
	lmaps.clear();
	lmLODs.clear();
	AssignGeometry( optimizedData );
	MergePositions();
	nTris = geometry.GetTrianglesCount();
	EstablishRefs();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::AssignFast( const SData &data )
{
	AssignGeometry( data );
	lmLODs.clear();
	nTris = geometry.GetTrianglesCount();
	posIndices.resize( verts.size() );
	for ( int k = 0; k < posIndices.size(); ++k )
		posIndices[k] = k;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetLMTriangles( vector<STriangle> *pRes ) const
{
	pRes->resize(0);
	for ( int k = 0; k < lmaps.size(); ++k )
	{
		int nOffset = k * 4;
		const SLightmapInfo &lm = lmaps[k];
		if ( lm.nOrder1 & 4 )
			pRes->push_back( STriangle( nOffset + 0, nOffset + 2, nOffset + 1 ) );
		else
			pRes->push_back( STriangle( nOffset + 0, nOffset + 1, nOffset + 2 ) );
		if ( lm.nOrder2 == 0 )
			continue;
		if ( lm.nOrder2 & 4 )
			pRes->push_back( STriangle( nOffset + 3, nOffset + 2, nOffset + 1 ) );
		else
			pRes->push_back( STriangle( nOffset + 3, nOffset + 1, nOffset + 2 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetLMVerticesTriangles( vector<STriangle> *pRes ) const
{
	GetLMTriangles( pRes );
	FilterTrinagles( pRes, lmRefVertices );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetLMPositionTriangles( vector<STriangle> *pRes ) const
{
	GetLMTriangles( pRes );
	FilterTrinagles( pRes, lmRefPositions );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetVxVerticesTriangles( vector<STriangle> *pRes ) const
{
	if ( lmaps.empty() )
	{
		geometry.GetTriangles( pRes );
		return;
	}
	pRes->resize(0);
	for ( int k = 0; k < lmaps.size(); ++k )
	{
		const SLightmapInfo &lm = lmaps[k];
		if ( lm.nOrder1 & 4 )
			pRes->push_back( STriangle( lm.n1, lm.n3, lm.n2 ) );
		else
			pRes->push_back( STriangle( lm.n1, lm.n2, lm.n3 ) );
		if ( lm.nOrder2 == 0 )
			continue;
		if ( lm.nOrder2 & 4 )
			pRes->push_back( STriangle( lm.n4, lm.n3, lm.n2 ) );
		else
			pRes->push_back( STriangle( lm.n4, lm.n2, lm.n3 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetVxPositionTriangles( vector<STriangle> *pRes ) const
{
	GetVxVerticesTriangles( pRes );
	if ( !vertRefPositions.empty() )
		FilterTrinagles( pRes, vertRefPositions );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::GetPosTriangles( vector<STriangle> *pRes ) const
{
	GetVxVerticesTriangles( pRes );
	if ( !posIndices.empty() )
		FilterTrinagles( pRes, posIndices );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::CalcBound( SBound *pRes )
{
	if ( verts.empty() )
		pRes->BoxInit( VNULL3, VNULL3 );
	else
		::CalcBound( pRes, positions, SGetSelf<CVec3>() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectInfo::CalcBound( SSphere *pRes )
{
	::CalcBound( pRes, positions, SGetSelf<CVec3>() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
