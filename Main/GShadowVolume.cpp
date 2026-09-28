#include "StdAfx.h"
#include "DG.h"
#include "GfxBuffers.h"
#include "Transform.h"
#include "GGeometry.h"
#include "DiscretePos.h"
#include "GCombiner.h"
#include "GShadowVolume.h"
#include "..\Misc\2DArray.h"
#include "Render.h"
//#include "GMaterial.h"
//#include "GScene.h"
//#include "GSceneUtils.h"
//#include "MemObject.h"
//#include "GSceneInternal.h"
//#include "GMemBuilder.h"
//#include "GMemFormat.h"
//extern CGScene *pCurrentScene;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_OCCLUDE_BUFFER_WIDTH = 128;
const int N_OCCLUDE_BUFFER_HEIGHT = 128;
const float FP_OCCLUDE_NEARCLIP = 0.01f;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ELineDispos
{
	SAME,
	PARALLEL,
	INTERSECT,
	NOTINTERSECT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 vCamDirs[6] = 
{
	CVec3( 1, 0, 0 ),
	CVec3( 0, 1, 0 ),
	CVec3( 0, 0, 1 ),
	CVec3(-1, 0, 0 ),
	CVec3( 0,-1, 0 ),
	CVec3( 0, 0,-1 )
};
static vector< CObj<CObjectBase> > nodes;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHZBuffer : public IHZBuffer
{
	OBJECT_BASIC_METHODS(CHZBuffer);
	vector<CArray2D<unsigned short> > depthBuffer;
	float fZBufScale;
public:
	void Initialize( int nXSize, int nYSize, float fScale )
	{
		fZBufScale = fScale;
		int nSize = Max( nXSize, nYSize );
		int nLevels = 0;
		while ( (nSize>>nLevels) > 4 )
			++nLevels;
		depthBuffer.resize( nLevels + 1 );
		depthBuffer[0].SetSizes( nXSize, nYSize );
		for ( int k = 1; k <= nLevels; ++k )
		{
			int nX = ( nXSize + (1<<k) - 1 ) >> k;
			int nY = ( nYSize + (1<<k) - 1 ) >> k;
			depthBuffer[k].SetSizes( nX, nY );
		}
	}
	void BuildHZ()
	{
		for ( int k = 1; k < depthBuffer.size(); ++k )
		{
			const CArray2D<unsigned short> &src = depthBuffer[ k - 1 ];
			CArray2D<unsigned short> &dst = depthBuffer[ k ];
			int nXSize = Min( src.GetXSize() / 2, dst.GetXSize() );
			int nYSize = Min( src.GetYSize() / 2, dst.GetYSize() );
			for ( int y = 0; y < nYSize; ++y )
			{
				for ( int x = 0; x < nXSize; ++x )
				{
					dst[y][x] = Min(
						src[y*2  ][x*2  ], Min(
						src[y*2  ][x*2+1], Min(
						src[y*2+1][x*2  ], 
						src[y*2+1][x*2+1] ) ) );
				}
			}
			if ( nYSize * 2 < src.GetYSize() )
			{
				ASSERT( nYSize * 2 + 1 == src.GetYSize() );
				for ( int y = nYSize; y < dst.GetYSize(); ++y )
				{
					for ( int x = 0; x < nXSize; ++x )
						dst[y][x] = Min(
							src[y*2][x*2  ],
							src[y*2][x*2+1] );
				}
			}
			else
			{
				for ( int y = nYSize; y < dst.GetYSize(); ++y )
				{
					for ( int x = 0; x < nXSize; ++x )
						dst[y][x] = 0xffff;
				}
			}
			if ( nXSize * 2 < src.GetXSize() )
			{
				ASSERT( nXSize * 2 + 1 == src.GetXSize() );
				for ( int x = nXSize; x < dst.GetXSize(); ++x )
				{
					for ( int y = 0; y < nYSize; ++y )
						dst[y][x] = Min(
							src[y*2  ][x*2],
							src[y*2+1][x*2] );
				}
			}
			else
			{
				for ( int x = nXSize; x < dst.GetXSize(); ++x )
				{
					for ( int y = 0; y < nYSize; ++y )
						dst[y][x] = 0xffff;
				}
			}
			for ( int x = nXSize; x < dst.GetXSize(); ++x )
			{
				for ( int y = nYSize; y < dst.GetYSize(); ++y )
					dst[y][x] = 0xffff;
			}
		}
	}
	bool IsVisible( const CTRect<float> &r, unsigned short nCheck ) const
	{
		int nX1 = Float2Int( ( r.x1 + 1 ) * 0.5f * depthBuffer[0].GetXSize() - 0.5f );
		int nX2 = Float2Int( ( r.x2 + 1 ) * 0.5f * depthBuffer[0].GetXSize() - 0.5f );
		int nY1 = Float2Int( ( r.y1 + 1 ) * 0.5f * depthBuffer[0].GetYSize() - 0.5f );
		int nY2 = Float2Int( ( r.y2 + 1 ) * 0.5f * depthBuffer[0].GetYSize() - 0.5f );
		int nMax = Max( nX2 - nX1, nY2 - nY1 ), nLevel = 0;
		while ( (nMax>>nLevel) > 3 && nLevel < depthBuffer.size() - 1 )
			++nLevel;
		const CArray2D<unsigned short> &l = depthBuffer[nLevel];
		nX1 = Max( (nX1 >> nLevel)-0, 0 );
		nX2 = Min( (nX2 >> nLevel)+0, l.GetXSize() - 1 );
		nY1 = Max( (nY1 >> nLevel)-0, 0 );
		nY2 = Min( (nY2 >> nLevel)+0, l.GetYSize() - 1 );
		for ( int y = nY1; y <= nY2; ++y )
		{
			for ( int x = nX1; x <= nX2; ++x )
			{
				if ( nCheck > l[y][x] )
					return true;
			}
		}
		return false;
	}
	bool IsVisible( const SSphere &s, CTransformStack *pTS ) const
	{
		CTRect<float> r;
		if ( !pTS->GetCoverRect( &r, s.ptCenter, s.fRadius ) )
			return false;
		const SHMatrix &m = pTS->Get().forward;
		const CVec3 &v = s.ptCenter;
		float fDist = m.wx * v.x + m.wy * v.y + m.wz * v.z + m.ww;
		if ( fDist < s.fRadius )
			return true;
		const float fCheck = fZBufScale / ( fDist - s.fRadius );
		if ( fCheck > 65535.0f )
			return true;
		return IsVisible( r, (unsigned short)int( fCheck ) );
	}
	CArray2D<unsigned short>& GetBase() { return depthBuffer[0]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Visible part generator
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFixedZIterator
{
	int nZ0, nZ, nDZx, nDZy;
	template<class T> void Init( T *pRender, const CVec3 &vA, float fZx, float fZy )
	{
		// Retail v1.2 0x5753e0: 8 fractional bits, anchored at a nearby pixel
		// before subtracting integer gradients (avoids distant-origin rounding).
		const float fScale = pRender->GetZBufferScale() * 256.0f;
		nDZx = Float2Int( fZx * fScale );
		nDZy = Float2Int( fZy * fScale );
		const int nX = Float2Int( vA.x - 0.5f ), nY = Float2Int( vA.y - 0.5f );
		const float fZ = vA.z - ( vA.x - 0.5f - nX ) * fZx - ( vA.y - 0.5f - nY ) * fZy;
		nZ0 = int( unsigned( Float2Int( fZ * fScale ) ) - unsigned( nDZx ) * nX - unsigned( nDZy ) * nY );
	}
	void Start( int nY ) { nZ = int( unsigned( nZ0 ) + unsigned( nDZy ) * nY ); }
	void Step() { nZ = int( unsigned( nZ ) + unsigned( nDZy ) ); }
	int GetZ( int nX ) const { return int( unsigned( nZ ) + unsigned( nDZx ) * nX ) >> 8; }
	int GetDZ() const { return nDZx >> 8; }
};
class CPartsRender: public CRasterizer<CPartsRender, SFixedZIterator>
{
	CObj<CHZBuffer> pHZBuffer;
	CArray2D<unsigned short> indexBuffer;
	vector<int> refCount;
	int nWidth, nHeight;
	int nCurrentID;
	float fZBufScale;

	bool DoRenderBackface() const { return false; }
	void ClipVertical( int *pnSY, int *pnFY2, int *pnFY )
	{
		*pnSY = Max( *pnSY, 0 );
		*pnFY2 = Min( *pnFY2, nHeight );
		*pnFY = Min( *pnFY, nHeight );
	}
	void ClipHorizontal( int *pnSX, int *pnFX )
	{
		*pnSX = Max( *pnSX, 0 );
		*pnFX = Min( *pnFX, nWidth );
	}
	void RasterSpan( int nY, int nLeft, int nRight, int nZ, int nDZ, int nBackface )
	{
		CArray2D<unsigned short> &depth = pHZBuffer->GetBase();
		for ( int x = nLeft; x < nRight; ++x, nZ = int( unsigned( nZ ) + unsigned( nDZ ) ) )
		{
			// Raw 0x5782a5: signed depth comparison, word store; zero depth has
			// no previous owner to debit. Do not clamp spans to the init limit.
			if ( nZ > depth[nY][x] )
			{
				if ( depth[nY][x] != 0 )
					--refCount[indexBuffer[nY][x]];
				++refCount[nCurrentID];
				indexBuffer[nY][x] = (unsigned short)nCurrentID;
				depth[nY][x] = (unsigned short)nZ;
			}
		}
	}
public:
	CPartsRender( int _nWidth, int _nHeight, float fScale ):
		nWidth( _nWidth ), nHeight( _nHeight ), fZBufScale( fScale )
	{
		pHZBuffer = new CHZBuffer;
		pHZBuffer->Initialize( nWidth, nHeight, fZBufScale );
		indexBuffer.SetSizes( nWidth, nHeight );
	}
	int GetWidth() const { return nWidth; }
	int GetHeight() const { return nHeight; }
	float GetZBufferScale() const { return fZBufScale; }
	void InitZBuffer( const SHMatrix &proj, float fRadius )
	{
		CArray2D<unsigned short> &depth = pHZBuffer->GetBase();
		const float fScale = ( 1.0f / fRadius ) * fZBufScale;
		for ( int y = 0; y < nHeight; ++y )
		{
			const float fY = ( y + 0.5f - nHeight * 0.5f ) * ( 2.0f / nHeight );
			const float fDX = 2.0f / nWidth;
			float fX = ( 0.5f - nWidth * 0.5f ) * fDX;
			const float fY2 = fY * fY + 1.0f;
			for ( int x = 0; x < nWidth; ++x, fX += fDX )
			{
				const float fLeng = sqrt( fX * fX + fY2 );
				depth[y][x] = (unsigned short)Min( Float2Int( fLeng * fScale ), 65535 );
				indexBuffer[y][x] = 0;
			}
		}
	}
	void FastInitZBuffer()
	{
		CArray2D<unsigned short> &depth = pHZBuffer->GetBase();
		for ( int y = 0; y < nHeight; ++y )
			for ( int x = 0; x < nWidth; ++x )
			{
				depth[y][x] = 0;
				indexBuffer[y][x] = 0;
			}
	}
	void SetCurrentID( int n ) { nCurrentID = n; }
	void SetRefsNumber( int n ) { refCount.assign( n, 0 ); }
	void SetRefs( int n, int nVal ) { refCount[n] = nVal; }
	int GetRefs( int n ) { return refCount[n]; }
	CHZBuffer* GetHZBuffer() { return pHZBuffer; }
	CHZBuffer* BuildHZ()
	{
		pHZBuffer->BuildHZ();
		return pHZBuffer;
	}
	friend class CRasterizer<CPartsRender, SFixedZIterator>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CountParts( const list<SRenderPartSet> &l )
{
	int nRes = 0;
	for ( list<SRenderPartSet>::const_iterator i = l.begin(); i != l.end(); ++i )
		nRes += i->pParts->size();
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCompareRPS
{
	CVec3 vZ;
	explicit SCompareRPS( const CVec3 &_vZ ) : vZ(_vZ) {}
	bool operator()( const SRenderPartSet &a, const SRenderPartSet &b ) const
	{
		if ( a.nFloorMask != b.nFloorMask )
			return a.nFloorMask > b.nFloorMask;
		return a.pGeometry->pVertices->GetBound().s.ptCenter * vZ <
			b.pGeometry->pVertices->GetBound().s.ptCenter * vZ;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void RenderStuff( CPartsRender &pr, IRender *pRender, CTransformStack *pTS, list<SRenderPartSet> &listParts,
	float fMinAverageArea, float fMinBoundSizeSquared, CHZBuffer *pHZ )
{
	SHMatrix sRes;
	sRes = pTS->Get().forward;
	sRes.x = ( sRes.x * 0.5f + sRes.w * 0.5f ) * pr.GetWidth();
	sRes.y = ( sRes.y * 0.5f + sRes.w * 0.5f ) * pr.GetHeight();
	listParts.sort( SCompareRPS( CVec3( sRes.wx, sRes.wy, sRes.wz ) ) );

	pr.SetRefsNumber( CountParts( listParts ) + 1 );
	int nIDCounter = 0;
	int nFloorMask = 0;
	bool bFirst = true;
	for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
	{
		SRenderPartSet &rps = *i;
		if ( rps.nFloorMask != nFloorMask )
		{
			if ( pHZ )
				pHZ->BuildHZ();
			nFloorMask = rps.nFloorMask;
			bFirst = false;
		}
		const vector<SSphere> &bounds = rps.pGeometry->pVertices->GetBounds();
		for ( int nPart = 0; nPart < rps.pParts->size(); ++nPart )
		{
			pr.SetCurrentID( ++nIDCounter );
			pr.SetRefs( nIDCounter, 0 );
			
			if ( !rps.parts.IsSet( nPart ) )
				continue;
			if ( !rps.castShadow.IsSet( nPart ) )
				continue;
			IPart *pPart = rps.GetPart( nPart );
			if ( !IsValid( pPart ) )
				continue;
			if ( pPart->fAverageTriArea < fMinAverageArea ||
				( fMinBoundSizeSquared > 0 && fabs2( pPart->vBVMax - pPart->vBVMin ) < fMinBoundSizeSquared ) )
			{
				// Small geometry remains a visibility candidate, but cannot occlude others.
				rps.castShadow.Reset( nPart );
				continue;
			}
			if ( pHZ && !bFirst && !pHZ->IsVisible( bounds[nPart], pTS ) )
				continue;
			
			vector<CVec3> points;
			vector<STriangle> tris;
			TransformPart( pPart, &points, &tris );
			
			static vector<SProjectedPoint> verticesSet;
			if ( points.size() > verticesSet.size() )
				verticesSet.resize( points.size() );
			for ( int nVert = 0; nVert < points.size(); nVert++ )
			{
				const CVec3 &src = points[nVert];
				verticesSet[nVert].Transform( sRes, src );
			}
			
			for ( int i = 0; i < tris.size(); ++i )
			{
				const STriangle &tri = tris[i];
				const SProjectedPoint &v1 = verticesSet[ tri.i1 ];
				const SProjectedPoint &v2 = verticesSet[ tri.i2 ];
				const SProjectedPoint &v3 = verticesSet[ tri.i3 ];
				pr.Raster( v1, v3, v2 ); // reverse triangle order because directX is using negative Oy direction
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GeneratePartList( IRender *pRender, const CVec3 &vCenter, float fRadius, 
	list<SRenderPartSet> *pRes, IRender::EDepthType eType, const SGroupSelect &mask )
{
	CPartsRender pr( N_OCCLUDE_BUFFER_WIDTH, N_OCCLUDE_BUFFER_HEIGHT, 10000.0f );
	nodes.clear();

	for ( int nTemp = 0; nTemp < 6; nTemp++ )
	{
		SHMatrix cameraTransf;
		MakeMatrix( &cameraTransf, vCenter, vCamDirs[nTemp] );

		CTransformStack sTransform;
		sTransform.MakeProjective( CVec2( N_OCCLUDE_BUFFER_WIDTH, N_OCCLUDE_BUFFER_HEIGHT ), 90, 0.1f, fRadius );
		sTransform.SetCamera( cameraTransf );
	
		list<SRenderPartSet> listParts;
		pRender->FormPartList( &sTransform, &listParts, eType, mask );
		pr.InitZBuffer( sTransform.GetProjection().forward, fRadius );
		RenderStuff( pr, pRender, &sTransform, listParts, 0, 0, 0 );

		CObj<CHZBuffer> pHZ = pr.BuildHZ();

		int nID = 0;
		for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
		{
			SRenderPartSet &rps = *i, *pDst = 0;
			for ( list<SRenderPartSet>::iterator k = pRes->begin(); k != pRes->end(); ++k )
			{
				if ( k->pNode == rps.pNode )
				{
					pDst = &(*k);
					break;
				}
			}
			if ( !pDst )
				pDst = &*pRes->insert( pRes->end(), SRenderPartSet( rps.pNode, rps.pParts, rps.pGeometry, rps.nFloorMask ) );
			const vector<SSphere> &bounds = rps.pGeometry->pVertices->GetBounds();
			for ( int k = 0; k < rps.pParts->size(); ++k )
			{
				nID++;
				bool bIsVisible = false;
				if ( rps.parts.IsSet(k) && !rps.castShadow.IsSet( k ) )
					bIsVisible = pHZ->IsVisible( bounds[k], &sTransform );
				bIsVisible |= pr.GetRefs( nID ) != 0;
				if ( bIsVisible )
					pDst->parts.Set( k );
			}
		}
		//if ( nTemp == 0 )
		//{
		//	nodes.clear();
		//	SGroupInfo sInfo( 0, 0xFEFFFFFF );
		//	CPtr<IMaterial> pMaterial = CreateMaterial( CVec3( 1, 0.3f, 0.3f ) );
		//	
		//	CTransformStack sStack;
		//	sStack.Init();
		//	CPtr<CCFBTransform> pTransform = new CCFBTransform( sStack.Get() );
		//	
		//	for( int nTempX = 0; nTempX < pr.depthBuffer.GetXSize(); nTempX++ )
		//	{
		//		for( int nTempY = 0; nTempY < pr.depthBuffer.GetYSize(); nTempY++ )
		//		{
		//			if ( pr.depthBuffer[nTempY][nTempX].nIndex != 0 )
		//			{
		//				CVec4 vRes, vSrc( 
		//					( ( nTempX + 0.5f ) / pr.GetWidth() - 0.5f ) * 2, 
		//					( ( nTempY + 0.5f ) / pr.GetHeight() - 0.5f ) * 2, 
		//					pr.depthBuffer[nTempY][nTempX].fDepth, 1 );
		//				pTS->Get().backward.RotateHVector( &vRes, vSrc );
		//				ASSERT( vRes.w != 0 );
		//				CVec3 vPos( vRes.x / vRes.w, vRes.y / vRes.w, vRes.z / vRes.w );
		//				CObj<CMemObject> pModelBuilder( new CMemObject );
		//				pModelBuilder->CreateSphere( vPos, 0.06f, 0 );
		//				
		//				nodes.push_back( pCurrentScene->CreatePart( CreateObjectInfo( pModelBuilder, CVec4(1,1,1,1) ), pMaterial, pTransform, sInfo ) );
		//				//nodes.push_back( pCurrentScene->CreatePart( modelBuilder.CreateObjectInfo(), pMaterial, pTransform, sInfo ) );
		//			}
		//		}
		//	}
		//}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeInvisibleElementsList( IRender *pRender, CTransformStack *pTS, 
	const SGroupSelect &_mask, const CVec2 &screenSize, CIgnorePartsHash *pIgnore,
	CObj<IHZBuffer> *pHZBuffer )
{
	// retail @0x176d50: width x/2 clamped [4,400], height y/2 clamped [4,300]
	CPartsRender pr( Min( 400, Max( 4, (int)screenSize.x / 2 ) ), Min( 300, Max( 4, (int)screenSize.y / 2 ) ), 40000.0f );
	list<SRenderPartSet> listParts;
	pRender->FormPartList( pTS, &listParts,IRender::DT_STATIC, _mask );
	pr.FastInitZBuffer();
	RenderStuff( pr, pRender, pTS, listParts, 0, 0.25f, pr.GetHZBuffer() );
	
	CHZBuffer *pHZ = pr.BuildHZ();
	*pHZBuffer = pHZ;

	int nID = 0;
	for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
	{
		SRenderPartSet &rps = *i;
		CIgnorePartsHash::iterator res = pIgnore->end();
		const vector<SSphere> &bounds = rps.pGeometry->pVertices->GetBounds();
		for ( int k = 0; k < rps.pParts->size(); ++k )
		{
			++nID;
			//if ( !rps.castShadow.IsSet(k) )
			//	continue;
			//if ( !rps.parts.IsSet( k ) )
			//	continue;
			bool bIsVisible = false;
			if ( rps.parts.IsSet(k) && !rps.castShadow.IsSet( k ) )
				bIsVisible = pHZ->IsVisible( bounds[k], pTS );
			bIsVisible |= pr.GetRefs( nID ) != 0;

			if ( !bIsVisible )
			{
				if ( res == pIgnore->end() )
				{
					(*pIgnore)[ rps.pNode.GetPtr() ].Clear();
					res = pIgnore->find( rps.pNode.GetPtr() );
				}
				res->second.Set( k );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1770f0: the HSR_DYNAMIC occlusion pass -- same marking loop as
// MakeInvisibleElementsList, but reuses a function-static 400x300 rasterizer and its persistent
// HZ buffer (retail: static CPartsRender(400,300,40000) owning a CObj<CHZBuffer>) instead of
// allocating screen-sized ones every call; screenSize is unused (retail keeps the parameter).
void MakeInvisibleElementsListFast( IRender *pRender, CTransformStack *pTS,
	const SGroupSelect &_mask, const CVec2 &screenSize, CIgnorePartsHash *pIgnore,
	CObj<IHZBuffer> *pHZBuffer )
{
	static CPartsRender pr( 400, 300, 40000.0f );
	list<SRenderPartSet> listParts;
	pRender->FormPartList( pTS, &listParts, IRender::DT_STATIC, _mask );
	pr.FastInitZBuffer();
	RenderStuff( pr, pRender, pTS, listParts, 0.19f, 0.25f, pr.GetHZBuffer() );

	CHZBuffer *pHZ = pr.BuildHZ();
	*pHZBuffer = pHZ;

	int nID = 0;
	for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
	{
		SRenderPartSet &rps = *i;
		CIgnorePartsHash::iterator res = pIgnore->end();
		const vector<SSphere> &bounds = rps.pGeometry->pVertices->GetBounds();
		for ( int k = 0; k < rps.pParts->size(); ++k )
		{
			++nID;
			bool bIsVisible = false;
			if ( rps.parts.IsSet(k) && !rps.castShadow.IsSet( k ) )
				bIsVisible = pHZ->IsVisible( bounds[k], pTS );
			bIsVisible |= pr.GetRefs( nID ) != 0;

			if ( !bIsVisible )
			{
				if ( res == pIgnore->end() )
				{
					(*pIgnore)[ rps.pNode.GetPtr() ].Clear();
					res = pIgnore->find( rps.pNode.GetPtr() );
				}
				res->second.Set( k );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Shadow volumes generator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShadowVolumeBuilder
{
	enum EE
	{
		N_MAX_POINTS = 10
	};
	struct SPoly
	{
		CVec3 points[N_MAX_POINTS];
		int nSize;

		SPoly(): nSize(0) {}
		int GetSize() const { return nSize; }
		bool IsEmpty() const { return GetSize() < 3; }
		void AddVertex( const CVec3 &v ) { ASSERT( nSize < N_MAX_POINTS ); points[nSize++] = v; }
	};

	CVec3 vCenter;
	float fRadius, fHullRadius;
	vector<CVec3> &resPoints;
	vector<STriangle> &resTris;

	typedef unordered_map<CVec3, int, SVec3Hash> CPointHash;
	CPointHash pointHash;
	vector<STriangle> tris;

	int AddPoint( const CVec3 &a );
	int AddBackPoint( const CVec3 &a );
	void AddBackTriangle( int nPlane, const SPoly &poly );
	void AddEdge( int n1, int n2 );
	float CalcPointNorm( const CVec3 &p1 );
public:
	CShadowVolumeBuilder( const CVec3 &_vCenter, float _fRadius, vector<CVec3> *_pResPoints, vector<STriangle> *_pResTris )
		: vCenter(_vCenter), fRadius(_fRadius), resPoints(*_pResPoints), resTris(*_pResTris), fHullRadius(_fRadius * FP_SQRT_3)
	{ 
		resPoints.resize( 0 );
		resTris.resize( 0 );
	}
	void AddTriangle( const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 );
	void BuildResult();
	float GetHullRadius() const { return fHullRadius; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int CShadowVolumeBuilder::AddPoint( const CVec3 &a )
{
	CPointHash::iterator i = pointHash.find( a );
	if ( i != pointHash.end() )
		return i->second;
	int nRes = resPoints.size();
	pointHash[a] = nRes;
	resPoints.push_back( a );
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CShadowVolumeBuilder::AddBackPoint( const CVec3 &a )
{
	CVec3 v = a - vCenter;
	float fLeng = fabs(v.x) + fabs(v.y) + fabs(v.z) + 1e-20f;
	v = vCenter + v * ( fRadius * FP_SQRT_3 / fLeng );
	return AddPoint( v );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcMiddle( CVec3 *pRes, const CVec3 &a, float fA, const CVec3 &b, float fB )
{
	float f1 = 1 / ( fB - fA );
	fA *= f1;
	fB *= f1;
	pRes->x = ( a.x * fB - b.x * fA );
	pRes->y = ( a.y * fB - b.y * fA );
	pRes->z = ( a.z * fB - b.z * fA );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShadowVolumeBuilder::AddEdge( int n1, int n2 )
{
	int nIndices[10];
	CVec3 points[10];
	points[0] = resPoints[n1];
	points[1] = resPoints[n2];
	int nSize = 2;
	for ( int nPlane = 0; nPlane < 3; ++nPlane )
	{
		for ( int k = 0; k < nSize - 1; ++k )
		{
			float fCur  = points[k  ].m[ nPlane ] - vCenter.m[ nPlane ];
			float fNext = points[k+1].m[ nPlane ] - vCenter.m[ nPlane ];
			float fTest = fCur * fNext;
			if ( fTest < 0 )
			{
				for ( int m = nSize; m > k+1; --m )
					points[m] = points[m-1];
				CalcMiddle( &points[k+1], points[k], fCur, points[k+2], fNext );
				nSize++;
				break;
			}
		}
	}
	nIndices[0] = n2;
	nIndices[1] = n1;
	for ( int k = 0; k < nSize; ++k )
		nIndices[k + 2] = AddBackPoint( points[k] );
	for ( int k = 2; k < nSize + 2; ++k )
		resTris.push_back( STriangle( nIndices[0], nIndices[k-1], nIndices[k] ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShadowVolumeBuilder::AddBackTriangle( int nPlane, const SPoly &poly )
{
	if ( nPlane == 3 )
	{
		int nIndices[ N_MAX_POINTS ];
		// project points onto octahedron
		for ( int k = 0; k < poly.GetSize(); ++k )
			nIndices[k] = AddBackPoint( poly.points[k] );
		for ( int k = 2; k < poly.GetSize(); ++k )
			resTris.push_back( STriangle( nIndices[0], nIndices[k-1], nIndices[k] ) );
		return;
	}
	SPoly pos, neg;
	const CVec3 *pPrev = &poly.points[ poly.GetSize() - 1 ];
	float fPrev = pPrev->m[ nPlane ] - vCenter.m[ nPlane ];
	for ( int k = 0; k < poly.GetSize(); ++k )
	{
		const CVec3 *pCur = &poly.points[ k ];
		float fCur = pCur->m[ nPlane ] - vCenter.m[ nPlane ];
		if ( fCur > 0 )
		{
			if ( fPrev < 0 )
			{
				CVec3 vCenter;
				CalcMiddle( &vCenter, *pCur, fCur, *pPrev, fPrev );
				pos.AddVertex( vCenter );
				neg.AddVertex( vCenter );
			}
			pos.AddVertex( *pCur );
		}
		else if ( fCur < 0 )
		{
			if ( fPrev > 0 )
			{
				CVec3 vCenter;
				CalcMiddle( &vCenter, *pCur, fCur, *pPrev, fPrev );
				pos.AddVertex( vCenter );
				neg.AddVertex( vCenter );
			}
			neg.AddVertex( *pCur );
		}
		else
		{
			pos.AddVertex( *pCur );
			neg.AddVertex( *pCur );
		}
		fPrev = fCur;
		pPrev = pCur;
	}
	if ( !pos.IsEmpty() )
		AddBackTriangle( nPlane + 1, pos );
	if ( !neg.IsEmpty() )
		AddBackTriangle( nPlane + 1, neg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	int nStart, nFinish;

	SEdge( int _nStart, int _nFinish ): nStart(_nStart), nFinish(_nFinish) {}
};
inline bool operator==( const SEdge &a, const SEdge &b ) { return a.nStart == b.nStart && a.nFinish == b.nFinish; }
struct SEdgeHash
{
	int operator()( const SEdge &a ) const { return ( a.nStart << 10 ) ^ a.nFinish; }
};
typedef unordered_map<SEdge, int, SEdgeHash> CEdgesHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdgeTracker
{
	CEdgesHash edges;

	void AddEdge( int n1, int n2 )
	{
		SEdge e( n1, n2 ), eBack( n2, n1 );
		CEdgesHash::iterator k = edges.find( eBack );
		if ( k != edges.end() )
		{
			--k->second;
			return;
		}
		CEdgesHash::iterator i = edges.find( e );
		if ( i == edges.end() )
			edges[e] = 1;
		else
			++i->second;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShadowVolumeBuilder::BuildResult()
{
	// add fronts
	for ( int k = 0; k < tris.size(); ++k )
		resTris.push_back( tris[k] );
	// add back covers
	for ( int k = 0; k < tris.size(); ++k )
	{
		SPoly p;
		const STriangle &t = tris[k];
		p.AddVertex( resPoints[t.i2] );
		p.AddVertex( resPoints[t.i1] );
		p.AddVertex( resPoints[t.i3] );
		AddBackTriangle( 0, p );
	}
	// add edges
	SEdgeTracker edges;
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &t = tris[k];
		edges.AddEdge( t.i1, t.i2 );
		edges.AddEdge( t.i2, t.i3 );
		edges.AddEdge( t.i3, t.i1 );
	}
	for ( CEdgesHash::iterator i = edges.edges.begin(); i != edges.edges.end(); ++i )
	{
		for ( int k = 0; k < i->second; ++k )
			AddEdge( i->first.nStart, i->first.nFinish );
		for ( int k = -1; k >= i->second; --k )
			AddEdge( i->first.nFinish, i->first.nStart );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CShadowVolumeBuilder::CalcPointNorm( const CVec3 &p1 )
{
	CVec3 v = p1 - vCenter;
	return fabs(v.x) + fabs(v.y) + fabs(v.z);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShadowVolumeBuilder::AddTriangle( const CVec3 &p1, const CVec3 &p2, const CVec3 &p3 )
{
	CVec3 vNormal( ( p2 - p1 ) ^ ( p3 - p1 ) );
	float fTest = vNormal * vCenter;
	if ( ( vNormal * p1 >= fTest ) && ( vNormal * p2 >= fTest ) && ( vNormal * p3 >= fTest ) )
		return;

	float f1 = CalcPointNorm( p1 );
	float f2 = CalcPointNorm( p2 );
	float f3 = CalcPointNorm( p3 );
	if ( f1 > fRadius * FP_SQRT_3 && f2 > fRadius * FP_SQRT_3 && f3 > fRadius * FP_SQRT_3 )
		return;
	fHullRadius = Max( fHullRadius, f1 );
	fHullRadius = Max( fHullRadius, f2 );
	fHullRadius = Max( fHullRadius, f3 );
	int n1 = AddPoint( p1 );
	int n2 = AddPoint( p2 );
	int n3 = AddPoint( p3 );
	tris.push_back( STriangle( n1, n2, n3 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeShadowVolumes( IRender *pRender, CTransformStack *pTS, const CVec3 &vCenter, 
	float fRadius, vector<STriangle> *pTris, 
	vector<CVec3> *pVertices, IRender::EDepthType eType, const SGroupSelect &mask,
	float *pHullRadius,
	CFilterPartsHash *pIgnore )
{
	CShadowVolumeBuilder shadowBuilder( vCenter, fRadius, pVertices, pTris );
	list<SRenderPartSet> listParts;

	if ( pIgnore )
		pIgnore->clear();
	if ( eType == IRender::DT_STATIC )
	{
		GeneratePartList( pRender, vCenter, fRadius, &listParts, eType, mask );
		if ( pIgnore )
		{
			for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
			{
				SRenderPartSet &rps = *i;
				// if there are skipped parts, fill them with 1
				if ( !rps.parts.IsEmpty() )
				{
					CPartFlags &r = (*pIgnore)[ i->pNode ];
					r = rps.parts;
					r.Invert();
				}
			}
		}
	}
	else
		pRender->FormPartList( pTS, &listParts, eType, mask );

	//int nVertCount = 0;
	//int nCoverTriCount = 0;
	//list<SEdge> partEdges;
	//pList->clear();
	//pVertices->clear();
	//pCoverFaces->clear();
	for ( list<SRenderPartSet>::iterator i = listParts.begin(); i != listParts.end(); i++ )
	{
		SRenderPartSet &rps = *i;
		for ( int k = 0; k < rps.pParts->size(); ++k )
		{
			if ( !rps.parts.IsSet( k ) )
				continue;
			IPart *pPart = rps.GetPart( k );
			if ( !IsValid( pPart ) )
				continue;

			vector<CVec3> points;
			vector<STriangle> tris;
			TransformPart( pPart, &points, &tris );

			for( int nTriIndex = 0; nTriIndex != tris.size(); nTriIndex++ )
			{
				const STriangle &tri = tris[nTriIndex];
				const CVec3 &p1 = points[ tri.i1 ];
				const CVec3 &p2 = points[ tri.i2 ];
				const CVec3 &p3 = points[ tri.i3 ];
				shadowBuilder.AddTriangle( p1, p2, p3 );
			}
		}
	}
	shadowBuilder.BuildResult();
	*pHullRadius = shadowBuilder.GetHullRadius();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
BASIC_REGISTER_CLASS( CHZBuffer )
