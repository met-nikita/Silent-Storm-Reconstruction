#include "StdAfx.h"
#include "GShadowMap.h"
//#include "GfxRender.h"
#include "GfxUtils.h"
#include "GfxBuffers.h"
#include "RectLayout.h"
#include "GRects.h"
#include "Transform.h"
#include "Bound.h"
#include "GInit.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 Unhomogen( const CVec4 &a ) { ASSERT( a.w != 0 ); return CVec3(a.x/a.w, a.y/a.w, a.z/a.w ); }
namespace NGeometry
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPolygon
{
	vector<CVec4> vertices;
	CVec4 vPlane;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPolyhedron
{
	vector<SPolygon> facets;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	CVec4 vStart, vEnd;
	SEdge() {}
	SEdge( const CVec4 _s, const CVec4 &_e ) : vStart(_s), vEnd(_e) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcIntersection( CVec4 *p, const CVec4 &_a, float fa, const CVec4 &_b, float fb )
{
	CVec4 a(_a), b(_b);
	float aw = a.w, bw = b.w;
	if ( aw == bw )
	{
		float f1 = 1 / (fb - fa);
		*p = a * (fb*f1) - b * (fa*f1);
		return;
	}
	if ( aw == 0 )
	{
		if ( fa == 0 )
		{
			*p = a;
			return;
		}
		*p = b - (fb / fa) * a;
		return;
	}
	if ( bw == 0 )
	{
		if ( fb == 0 )
		{
			*p = b;
			return;
		}
		*p = a - (fa / fb) * b;
		return;
	}
	a.x *= bw; a.y *= bw; a.z *= bw; a.w *= bw; fa *= bw;
	b.x *= aw; b.y *= aw; b.z *= aw; b.w *= aw; fb *= aw;
	float f1 = 1 / (fb - fa);
	*p = a * (fb*f1) - b * (fa*f1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Split( SPolyhedron *pRes, const SPlane &p )
{
	SPolyhedron src = *pRes;
	list<SEdge> edges;
	pRes->facets.resize(0);
	for ( int k = 0; k < src.facets.size(); ++k )
	{
		SPolygon d;
		SPolygon &s = src.facets[k];
		for ( int k = 0; k < s.vertices.size(); ++k )
		{
			if ( s.vertices[k].w < 0 )
				s.vertices[k] = -s.vertices[k];
		}
		int iPrev = s.vertices.size() - 1;
		float fPrevSide = p.vec4 * s.vertices[iPrev], fSide;
		CVec4 vEnter, vExit;
		bool bWasIntersected = false;
		for ( int i = 0; i < s.vertices.size(); iPrev = i, fPrevSide = fSide, ++i )
		{
			const CVec4 &vPrev = s.vertices[ iPrev ];
			const CVec4 &v = s.vertices[ i ];
			fSide = p.vec4 * v;
			if ( fPrevSide < 0 )
			{
				if ( fSide < 0 )
					continue;
				bWasIntersected = true;
				if ( fSide > 0 )
				{
					CalcIntersection( &vEnter, vPrev, fPrevSide, v, fSide );
					d.vertices.push_back( vEnter );
				}
				else
					vEnter = v;
				d.vertices.push_back( v );
			}
			else
			{
				if ( fSide < 0 )
				{
					ASSERT( fPrevSide >= 0 );
					if ( fPrevSide > 0 )
					{
						CalcIntersection( &vExit, v, fSide, vPrev, fPrevSide );
						d.vertices.push_back( vExit );
					}
					else
						vExit = vPrev;
				}
				else
					d.vertices.push_back( v );
			}
		}
		if ( !d.vertices.empty() )
		{
			d.vPlane = s.vPlane;
			pRes->facets.push_back( d );
		}
		if ( bWasIntersected )
			edges.push_back( SEdge( vEnter, vExit ) );
	}
	if ( !edges.empty() )
	{
		SPolygon onPlane;
		onPlane.vertices.push_back( edges.back().vEnd );
		edges.pop_back();
		while ( !edges.empty() )
		{
			CVec4 v = onPlane.vertices.back();
			bool bFound = false;
			for ( list<SEdge>::iterator i = edges.begin(); i != edges.end(); ++i )
			{
				if ( v == i->vStart )
				{
					bFound = true;
					onPlane.vertices.push_back( i->vEnd );
					edges.erase( i );
					break;
				}
			}
			ASSERT( bFound );
			if ( !bFound )
				break;
		}
		onPlane.vPlane = -p.vec4;
		pRes->facets.push_back( onPlane );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 Get3DDif( const CVec4 &_v1, const CVec4 &_v2 )
{
	CVec4 v1(_v1), v2(_v2);
	if ( v1.w < 0 )
		v1 = -v1;
	if ( v2.w < 0 )
		v2 = -v2;
	return CVec3( 
		v1.x * v2.w - v2.x * v1.w,  
		v1.y * v2.w - v2.y * v1.w,  
		v1.z * v2.w - v2.z * v1.w );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetPlane( SPolygon *pRes, const CVec4 &a, const CVec4 &b, const CVec4 &c )
{
	CVec3 ab = Get3DDif( b, a ), ac = Get3DDif( c, a );
	CVec3 vNormal = ab ^ ac;
	float fDist = a.x * vNormal.x + a.y * vNormal.y + a.z * vNormal.z;
	if ( a.w < 0 )
		fDist = -fDist;
	pRes->vPlane.Set( vNormal * fabs(a.w), fDist );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SPolygon GetTriangle( const CVec4 &a, const CVec4 &b, const CVec4 &c )
{
	SPolygon r;
	r.vertices.push_back( a );
	r.vertices.push_back( b );
	r.vertices.push_back( c );
	SetPlane( &r, a, b, c );
	return r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SPolygon GetQuad( const CVec4 &a, const CVec4 &b, const CVec4 &c, const CVec4 &d )
{
	SPolygon r;
	r.vertices.push_back( a );
	r.vertices.push_back( b );
	r.vertices.push_back( c );
	r.vertices.push_back( d );
	SetPlane( &r, a, b, c );
	return r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHashedHedron
{
	vector<CVec4> pts;
	struct SEdge
	{
		int nStart, nEnd;
		SEdge() {}
		SEdge( int _s, int _e ) : nStart(_s), nEnd(_e) {}
	};
	struct SPolygon
	{
		vector<int> edges;
		CVec4 vPlane;
	};
	vector<SEdge> edges;
	vector<SPolygon> polys;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static int AddPoint( SHashedHedron *pRes, const CVec4 &v )
{
	for ( int k = 0; k < pRes->pts.size(); ++k )
	{
		if ( pRes->pts[k] == v )
			return k;
	}
	pRes->pts.push_back( v );
	return pRes->pts.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int AddEdge( SHashedHedron *pRes, int nStart, int nEnd )
{
	for ( int k = 0; k < pRes->edges.size(); ++k )
	{
		SHashedHedron::SEdge &e = pRes->edges[k];
		if ( e.nStart == nStart && e.nEnd == nEnd )
			return k;
		if ( e.nStart == nEnd && e.nEnd == nStart )
			return k|0x80000000;
	}
	pRes->edges.push_back( SHashedHedron::SEdge( nStart, nEnd ) );
	return pRes->edges.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Assign( SHashedHedron *pRes, const SPolyhedron &src )
{
	pRes->polys.resize( src.facets.size() );
	for ( int k = 0; k < src.facets.size(); ++k )
	{
		const SPolygon &p = src.facets[k];
		SHashedHedron::SPolygon &d = pRes->polys[k];
		d.vPlane = p.vPlane;
		int nPrev = AddPoint( pRes, p.vertices[ p.vertices.size() - 1 ] ), nTek;
		for ( int i = 0; i < p.vertices.size(); nPrev = nTek, ++i )
		{
			nTek = AddPoint( pRes, p.vertices[i] );
			d.edges.push_back( AddEdge( pRes, nPrev, nTek ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsClosed( const SHashedHedron &src )
{
	vector<int> count;
	count.resize( src.edges.size(), 0 );
	for ( int k = 0; k < src.polys.size(); ++k )
	{
		for ( int i = 0; i < src.polys[k].edges.size(); ++i )
		{
			int nEdge = src.polys[k].edges[i];
			if ( nEdge & 0x80000000 )
				--count[ nEdge&0x7fffffff ];
			else
				++count[nEdge];
		}
	}
	bool bRes = true;
	for ( int i = 0; i < count.size(); ++i )
		bRes &= count[i] == 0;
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Assign( SPolyhedron *pRes, const SHashedHedron &src )
{
	pRes->facets.resize( src.polys.size() );
	for ( int k = 0; k < src.polys.size(); ++k )
	{
		const SHashedHedron::SPolygon &s = src.polys[k];
		SPolygon &d = pRes->facets[k];
		d.vPlane = s.vPlane;
		for ( int i = 0; i < s.edges.size(); ++i )
		{
			int nEdge = s.edges[i];
			if ( nEdge & 0x80000000 )
				d.vertices.push_back( src.pts[ src.edges[ nEdge&0x7fffffff ].nEnd ] );
			else
				d.vertices.push_back( src.pts[ src.edges[ nEdge ].nStart ] );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeShadowOccludersHull( NGeometry::SPolyhedron *pRes, const NGeometry::SPolyhedron &src, const CVec3 &ptDir )
{
	NGeometry::SHashedHedron h;
	NGeometry::Assign( &h, src );
	ASSERT( NGeometry::IsClosed( h ) );

	vector<int> up( h.polys.size() );
	for ( int k = 0; k < h.polys.size(); ++k )
	{
		const CVec4 vPlane = h.polys[k].vPlane;
		up[k] = vPlane * CVec4(ptDir,0) < 0;
	}

	vector<int> rPolys( h.edges.size(), 0 ), lPolys( h.edges.size(), 0 );
	for ( int k = 0; k < h.polys.size(); ++k )
	{
		NGeometry::SHashedHedron::SPolygon &p = h.polys[k];
		for ( int i = 0; i < p.edges.size(); ++i )
		{
			if ( p.edges[i] & 0x80000000 )
				lPolys[ p.edges[i] & 0x7fffffff ] = k;
			else
				rPolys[ p.edges[i] ] = k;
		}
	}

	CVec4 vInf( -ptDir, 0 );
	for ( int k = 0; k < h.edges.size(); ++k )
	{
		CVec4 vStart = h.pts[ h.edges[k].nStart ];
		CVec4 vEnd = h.pts[ h.edges[k].nEnd ];
		if ( up[ rPolys[k] ] != up[ lPolys[k] ] )
		{
			if ( up[ rPolys[k] ] )
				pRes->facets.push_back( NGeometry::GetTriangle( vStart, vEnd, vInf ) );
			else
				pRes->facets.push_back( NGeometry::GetTriangle( vEnd, vStart, vInf ) );
		}
	}
	for ( int k = 0; k < up.size(); ++k )
	{
		if ( up[k] )
			continue;
		pRes->facets.push_back( src.facets[k] );
	}
	NGeometry::SHashedHedron hTest;
	NGeometry::Assign( &hTest, *pRes );
	ASSERT( NGeometry::IsClosed( hTest ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeShadowMatrix( CTransformStack *pRes, const CTransformStack &ts, const CVec3 &ptDir, float fMaxHeight )
{
	CVec4 vC, v00, v01, v10, v11, vC1;
	ts.Get().backward.RotateHVector( &vC, CVec4(0,0,1,0) );
	ts.Get().backward.RotateHVector( &v00, CVec4(-1,-1,1,1) );
	ts.Get().backward.RotateHVector( &v01, CVec4(-1, 1,1,1) );
	ts.Get().backward.RotateHVector( &v11, CVec4( 1, 1,1,1) );
	ts.Get().backward.RotateHVector( &v10, CVec4( 1,-1,1,1) );
	ts.Get().backward.RotateHVector( &vC1, CVec4( 0, 0,1,1) );
	NGeometry::SPolyhedron hedron, occluderHedron;
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v00, v10 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v10, v11 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v11, v01 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v01, v00 ) );
	hedron.facets.push_back( NGeometry::GetQuad( v00, v01, v11, v10 ) );
	NGeometry::Split( &hedron, SPlane( CVec3(0,0,1), 0 ) );
	NGeometry::Split( &hedron, SPlane( CVec3(0,0,-1), fMaxHeight * 1.05f ) );

	MakeShadowOccludersHull( &occluderHedron, hedron, ptDir );
	NGeometry::Split( &occluderHedron, SPlane( CVec3(0,0,-1), fMaxHeight ) );
	// limit for safety by some large area
	NGeometry::Split( &occluderHedron, SPlane( CVec3(1,0,0), 1000 ) );
	NGeometry::Split( &occluderHedron, SPlane( CVec3(0,1,0), 1000 ) );
	NGeometry::Split( &occluderHedron, SPlane( CVec3(-1,0,0), 1000 ) );
	NGeometry::Split( &occluderHedron, SPlane( CVec3(0,-1,0), 1000 ) );
	if ( occluderHedron.facets.empty() )
	{
//		ASSERT(0);
		pRes->MakeParallel( 1, 1 );
		return;
	}

	// create special ts where original data is spaced better
	CTransformStack tsBetter;
	CVec3 vCamera( Unhomogen( vC ) ), vDest( Unhomogen( vC1 ) );
	float fDist = fabs( vDest - vCamera );
	tsBetter.MakeProjective( 1, 90, fDist * 0.5f, fDist * 1.5f );
	SHMatrix betterCam;
	MakeMatrix( &betterCam, vCamera - (vDest - vCamera ) * 0.5f, vDest - vCamera );
	tsBetter.SetCamera( betterCam );

	CTransformStack test;
	CVec4 vBetterPos;
	bool bInvertZ = false;
	tsBetter.Get().forward.RotateHVector( &vBetterPos, CVec4(ptDir,0) );
	if ( fabs( vBetterPos.w ) > 0.01f )
	{
		bInvertZ = vBetterPos.z > 0;
		vBetterPos /= vBetterPos.w;
		CVec3 vBP( vBetterPos.x, vBetterPos.y, vBetterPos.z );

		// calc center of target area
		const SHMatrix &mToBetter = tsBetter.Get().forward;
		SBoundCalcer bc;
		for ( int k = 0; k < hedron.facets.size(); ++k )
		{
			const NGeometry::SPolygon &p = hedron.facets[k];
			for ( int i = 0; i < p.vertices.size(); ++i )
			{
				CVec4 vRes;
				mToBetter.RotateHVector( &vRes, p.vertices[i] );
				bc.Add( Unhomogen( vRes ) );
			}
		}
		SSphere s;
		bc.Make( &s );
		SHMatrix camera;
		//MakeMatrix( &camera, CVec3(0,0,0), ptDir );
		MakeMatrix( &camera, vBP, s.ptCenter - vBP );//-vBP );
		test.MakeProjective( 1, 90, 0.1f, 1e4f );//MakeParallexl( 1, 1, -1000, 1000 );
		test.SetCamera( camera );
	}
	else
	{
		test.MakeParallel( 1, 1 );
		SHMatrix camera;
		//MakeMatrix( &camera, CVec3(0,0,0), ptDir );
		MakeMatrix( &camera, CVec3(0,0,0), CVec3(vBetterPos.x, vBetterPos.y, vBetterPos.z ) );
		test.SetCamera( camera );
	}
	test.Push44( tsBetter.Get().forward );

	// fit ts to target
	SHMatrix mRes = test.Get().forward;
	if ( bInvertZ )
		mRes.z = mRes.w - mRes.z;
	else
		mRes.x = -mRes.x;
	CVec2 vMin( 1e30f, 1e30f ), vMax( -1e30f, -1e30f );
	// calc bbox of all point projections
	for ( int k = 0; k < hedron.facets.size(); ++k )
	{
		const NGeometry::SPolygon &p = hedron.facets[k];
		for ( int i = 0; i < p.vertices.size(); ++i )
		{
			CVec4 v;
			mRes.RotateHVector( &v, p.vertices[i] );
			CVec2 vProj( v.x / v.w, v.y / v.w );
			vMin.Minimize( vProj );
			vMax.Maximize( vProj );
		}
	}
	float fXScale = 2 / ( vMax.x - vMin.x );
	float fYScale = 2 / ( vMax.y - vMin.y );
	mRes.x = mRes.x * fXScale - mRes.w * ( vMin.x + vMax.x ) * 0.5f * fXScale;
	mRes.y = mRes.y * fYScale - mRes.w * ( vMin.y + vMax.y ) * 0.5f * fYScale;
	// calc z range
	float fZMax = -1e30f, fZMin = 1e30f;
	for ( int k = 0; k < occluderHedron.facets.size(); ++k )
	{
		const NGeometry::SPolygon &p = occluderHedron.facets[k];
		for ( int i = 0; i < p.vertices.size(); ++i )
		{
			CVec4 v;
			mRes.RotateHVector( &v, p.vertices[i] );
			ASSERT( v.w > 0 );
			float fZ = v.z / v.w;
			fZMax = Max( fZMax, fZ );
			fZMin = Min( fZMin, fZ );
		}
	}
	float fZScale = 1 / ( fZMax - fZMin );
	mRes.z = mRes.z * fZScale - mRes.w * fZMin * fZScale;
	pRes->Make( mRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeSceneGeometryBound( SBound *pRes, const CTransformStack &ts, float fMaxHeight )
{
	CVec4 vC, v00, v01, v10, v11, vC1;
	ts.Get().backward.RotateHVector( &vC, CVec4(0,0,1,0) );
	ts.Get().backward.RotateHVector( &v00, CVec4(-1,-1,1,1) );
	ts.Get().backward.RotateHVector( &v01, CVec4(-1, 1,1,1) );
	ts.Get().backward.RotateHVector( &v11, CVec4( 1, 1,1,1) );
	ts.Get().backward.RotateHVector( &v10, CVec4( 1,-1,1,1) );
	ts.Get().backward.RotateHVector( &vC1, CVec4( 0, 0,1,1) );
	NGeometry::SPolyhedron hedron, occluderHedron;
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v00, v10 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v10, v11 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v11, v01 ) );
	hedron.facets.push_back( NGeometry::GetTriangle( vC, v01, v00 ) );
	hedron.facets.push_back( NGeometry::GetQuad( v00, v01, v11, v10 ) );
	NGeometry::Split( &hedron, SPlane( CVec3(0,0,1), 0 ) );
	NGeometry::Split( &hedron, SPlane( CVec3(0,0,-1), fMaxHeight * 1 ) );
	SBoundCalcer bc;
	for ( int i = 0; i < hedron.facets.size(); ++i )
	{
		const NGeometry::SPolygon &p = hedron.facets[i];
		for ( int k = 0; k < p.vertices.size(); ++k )
			bc.Add( Unhomogen( p.vertices[k] ) );
	}
	bc.Make( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShadowMapsShare
////////////////////////////////////////////////////////////////////////////////////////////////////
CShadowMapsShare shadowMapsShare;
CShadowMapsShare::CShadowMapsShare()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShadowMapsShare::Refresh()
{
	if ( IsValid( pDepthShadow ) )
		return;
	nDepthResolution = GetDepthTexResolution();
	nCLSkyTextures = NGScene::GetCLSkyTexturesNumber();
	pDepthShadow = NGfx::MakeTexture( nDepthResolution, nDepthResolution, 1, NGfx::SPixel8888::ID, NGfx::TARGET, NGfx::CLAMP );
	int n = N_DEFAULT_RT_RESOLUTION;
	pParticleLM = NGfx::MakeTexture( n, n, 1, NGfx::SPixel8888::ID, NGfx::TARGET, NGfx::CLAMP );
	for ( int k = 0; k < nCLSkyTextures; ++k )
		pLMDepthBuffers[k] = NGfx::MakeTexture( n, n, 1, NGfx::SPixel8888::ID, NGfx::TARGET, NGfx::CLAMP );
	if ( CanCacheLighting() )
	{
		n = GetCubeDepthResolution();
		pCubeDepth = NGfx::MakeCubeTexture( n, 1, NGfx::SPixel8888::ID, NGfx::TARGET );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawBorder( NGfx::CRenderContext *pRC, int nSize )
{
	CRectLayout borderLayout;
	borderLayout.AddRect( 0, 0, CTRect<float>( 0, 0, nSize, 1 ) );
	borderLayout.AddRect( 0, 0, CTRect<float>( 0, 0, 1, nSize ) );
	borderLayout.AddRect( 0, nSize - 1, CTRect<float>( 0, 0, nSize, 1 ) );
	borderLayout.AddRect( nSize - 1, 0, CTRect<float>( 0, 0, 1, nSize ) );
	NGfx::C2DQuadsRenderer qr( *pRC, CVec2( nSize, nSize ), NGfx::QRM_DEPTH_NONE|NGfx::QRM_NOCOLOR );
	RenderRectLayout( &qr, 0, borderLayout );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetAndClearRT( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, int nWriteMask, int nSize )
{
	pRC->SetTextureRT( pTex );
	//pRC->ClearBuffers( 0 );//0xffffffff );
	CRectLayout blackSquare;
	blackSquare.AddRect( 0, 0, CTRect<float>( 0, 0, nSize, nSize), NGfx::SPixel8888(0,0,0,0) );
	ASSERT( nWriteMask );
	pRC->SetColorWrite( (NGfx::EColorWriteMask)nWriteMask );
	NGfx::C2DQuadsRenderer qr( *pRC, CVec2( nSize, nSize ), NGfx::QRM_OVERWRITE|NGfx::QRM_SOLID );
	RenderRectLayout( &qr, 0, blackSquare );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}