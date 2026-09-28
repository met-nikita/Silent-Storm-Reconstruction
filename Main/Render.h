#ifndef __Render_H_
#define __Render_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetBits( const float *f ) { return *(const int*)f; }
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGradientMatrix
{
	float _11, _12, _13;
	float _21, _22, _23;
	float _d1, _d2, _d3;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CalcGradient( const SGradientMatrix &m, CVec4 *pRes, float f1, float f2, float f3 )
{
	pRes->x = m._11 * f1 + m._12 * f2 + m._13 * f3;
	pRes->y = m._21 * f1 + m._22 * f2 + m._23 * f3;
	pRes->z = 0;
	pRes->w = m._d1 * f1 + m._d2 * f2 + m._d3 * f3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void PrepareGradientMatrix( SGradientMatrix *pRes, const CVec3 &vA, const CVec3 &vB, const CVec3 &vC )
{
	float f1 = vB.x * vC.y - vC.x * vB.y;
	float f2 = vC.x * vA.y - vA.x * vC.y;
	float f3 = vA.x * vB.y - vB.x * vA.y;
	float fD = f1 + f2 + f3;
	float fD1 = 1 / fD;
	memset( pRes, 0, sizeof(*pRes ) );
	pRes->_11 = ( vB.y - vC.y ) * fD1;
	pRes->_12 = -( vA.y - vC.y ) * fD1;
	pRes->_13 = ( vA.y - vB.y ) * fD1;
	pRes->_21 = -( vB.x - vC.x ) * fD1;
	pRes->_22 = ( vA.x - vC.x ) * fD1;
	pRes->_23 = -( vA.x - vB.x ) * fD1;
	pRes->_d1 = f1 * fD1;
	pRes->_d2 = f2 * fD1;
	pRes->_d3 = f3 * fD1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SProjectedPoint
{
	CVec3 res, src;

	void Project()
	{
		res.z = 1 / src.z;
		res.x = src.x * res.z;
		res.y = src.y * res.z;
	}
	void Transform( const SHMatrix &xform, const CVec3 &_src )
	{
		src.x = xform._11 * _src.x + xform._12 * _src.y + xform._13 * _src.z + xform._14;
		src.y = xform._21 * _src.x + xform._22 * _src.y + xform._23 * _src.z + xform._24;
		src.z = xform._41 * _src.x + xform._42 * _src.y + xform._43 * _src.z + xform._44;
		Project();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// all points assumed to be projected & scaled to fit viewport
// rasterizer perform perspective division only
// rasterizers tries to conform DirectX rasterizing standard - points are checked by its centers
const float F_RASTERIZER_NEAR_PLANE = 0.01f;
const int N_F_RASTERIZER_NEAR_PLANE = 0x3c23d70a;//0.01f//*(int*)&F_RASTERIZER_NEAR_PLANE;
struct SFloatZIterator
{
	float fZ0, fZ, fZx, fZy;
	template<class T> void Init( T *, const CVec3 &vA, float fX, float fY )
	{
		fZx = fX;
		fZy = fY;
		fZ0 = vA.z - ( vA.x - 0.5f ) * fZx - ( vA.y - 0.5f ) * fZy;
	}
	void Start( int nY ) { fZ = nY * fZy + fZ0; }
	void Step() { fZ += fZy; }
	float GetZ( int nX ) const { return fZ + nX * fZx; }
	float GetDZ() const { return fZx; }
};
template<class T, class TZIterator = SFloatZIterator>
class CRasterizer
{
	// Retail uses this fixed-point edge walk for software shading, AI grids,
	// voxel queries and shadow visibility alike (v1.2 0x40d9b0/0x40eb40/
	// 0x4105d0/0x494050/0x578180). Only the depth iterator varies.
	void RasterTriangle( const CVec3 &vA, const CVec3 &vB, const CVec3 &vC )
	{
		RasterTriangleFixed( vA, vB, vC );
	}
	struct SFixedEdgeInfo
	{
		int nSY, nFY;
		int nDX, nX0;
	};
	void RasterTriangleLowFixed( const SFixedEdgeInfo &sLeft, const SFixedEdgeInfo &sRight,
		const SFixedEdgeInfo &sRight2, TZIterator *pZGradient,
		int nSY, int nFY2, int nFY, int nBack )
	{
		T *pThis = static_cast<T*>( this );
		pThis->ClipVertical( &nSY, &nFY2, &nFY );

		pZGradient->Start( nSY );
		int nLeftX = ( nSY - sLeft.nSY ) * sLeft.nDX + sLeft.nX0;
		int nRightX = ( nSY - sRight.nSY ) * sRight.nDX + sRight.nX0;
		int nY = nSY;
		for ( ; nY < nFY2; ++nY, nLeftX += sLeft.nDX, nRightX += sRight.nDX,
			pZGradient->Step() )
		{
			int nLeft = nLeftX >> 16;
			int nRight = nRightX >> 16;
			int nBackface;
			if ( nLeft > nRight )
			{
				swap( nLeft, nRight );
				nBackface = nBack ^ 1;
			}
			else if ( nRight > nLeft )
				nBackface = nBack;
			else
				continue;
			if ( nBackface && !pThis->DoRenderBackface() )
				continue;
			pThis->ClipHorizontal( &nLeft, &nRight );
			pThis->RasterSpan( nY, nLeft, nRight,
				pZGradient->GetZ( nLeft ), pZGradient->GetDZ(), nBackface );
		}

		nRightX = ( nY - sRight2.nSY ) * sRight2.nDX + sRight2.nX0;
		for ( ; nY < nFY; ++nY, nLeftX += sLeft.nDX, nRightX += sRight2.nDX,
			pZGradient->Step() )
		{
			int nLeft = nLeftX >> 16;
			int nRight = nRightX >> 16;
			int nBackface;
			if ( nLeft > nRight )
			{
				swap( nLeft, nRight );
				nBackface = nBack ^ 1;
			}
			else if ( nRight > nLeft )
				nBackface = nBack;
			else
				continue;
			if ( nBackface && !pThis->DoRenderBackface() )
				continue;
			pThis->ClipHorizontal( &nLeft, &nRight );
			pThis->RasterSpan( nY, nLeft, nRight,
				pZGradient->GetZ( nLeft ), pZGradient->GetDZ(), nBackface );
		}
	}
	void InitFixedEdge( SFixedEdgeInfo *pRes, const CVec3 &a, const CVec3 &dif, int nSY, int nFY )
	{
		const float fDX = dif.x / dif.y;
		pRes->nDX = Float2Int( fDX * 65536.0f );
		pRes->nX0 = Float2Int( ( a.x - ( a.y - 0.5f - nSY ) * fDX ) * 65536.0f ) + 0x8000;
		pRes->nSY = nSY;
		pRes->nFY = nFY;
	}
	bool CalcFixedZGradient( TZIterator *pInfo, const CVec3 &vA,
		const CVec3 &vCB, const CVec3 &vAC )
	{
		const float fArea = -vAC.x * vCB.y + vAC.y * vCB.x;
		if ( fArea == 0 )
			return false;
		const float fD = 1 / fArea;
		const float fZx = fD * ( -vCB.y * vAC.z + vAC.y * vCB.z );
		const float fZy = fD * ( vCB.x * vAC.z - vAC.x * vCB.z );
		pInfo->Init( static_cast<T*>( this ), vA, fZx, fZy );
		return true;
	}
	void RasterTriangleFixed( const CVec3 &vA, const CVec3 &vB, const CVec3 &vC )
	{
		const int nA = Float2Int( vA.y );
		const int nB = Float2Int( vB.y );
		const int nC = Float2Int( vC.y );
		if ( nA == nB && nA == nC )
			return;

		int nLeft = 0, nRight = 0;
		const CVec3 vEdge1( vB - vA ), vEdge2( vC - vB ), vEdge3( vA - vC );
		TZIterator zGrad;
		if ( !CalcFixedZGradient( &zGrad, vA, vEdge2, vEdge3 ) )
			return;
		SFixedEdgeInfo sLeft[2], sRight[2];

		if ( vEdge1.y > 0 )
			InitFixedEdge( &sLeft[nLeft++], vA, vEdge1, nA, nB );
		else if ( vEdge1.y < 0 )
			InitFixedEdge( &sRight[nRight++], vB, vEdge1, nB, nA );

		if ( vEdge2.y > 0 )
			InitFixedEdge( &sLeft[nLeft++], vB, vEdge2, nB, nC );
		else if ( vEdge2.y < 0 )
			InitFixedEdge( &sRight[nRight++], vC, vEdge2, nC, nB );

		if ( vEdge3.y > 0 )
			InitFixedEdge( &sLeft[nLeft++], vC, vEdge3, nC, nA );
		else if ( vEdge3.y < 0 )
			InitFixedEdge( &sRight[nRight++], vA, vEdge3, nA, nC );

		SFixedEdgeInfo *pLeft, *pRight;
		int nEdges, nBack;
		if ( nLeft == 1 )
		{
			pLeft = sLeft;
			pRight = sRight;
			nEdges = nRight;
			nBack = 0;
		}
		else if ( nRight == 1 )
		{
			pLeft = sRight;
			pRight = sLeft;
			nEdges = nLeft;
			nBack = 1;
		}
		else
			return;

		if ( nEdges > 1 )
		{
			if ( pRight[0].nSY > pRight[1].nSY )
				RasterTriangleLowFixed( pLeft[0], pRight[1], pRight[0], &zGrad,
					pLeft[0].nSY, pRight[1].nFY, pLeft[0].nFY, nBack );
			else
				RasterTriangleLowFixed( pLeft[0], pRight[0], pRight[1], &zGrad,
					pLeft[0].nSY, pRight[0].nFY, pLeft[0].nFY, nBack );
		}
		else if ( nEdges > 0 )
			RasterTriangleLowFixed( pLeft[0], pRight[0], pRight[0], &zGrad,
				pLeft[0].nSY, pRight[0].nFY, pLeft[0].nFY, nBack );
	}
	float ClipDistance( const SProjectedPoint &v, int nPlane ) const
	{
		// Retail v1.2 0x4946a0: near plane, then four guard planes.
		// Bound projected x/y to +/-1000 before the 16.16 edge walk.
		switch ( nPlane )
		{
		case 0: return v.src.z - F_RASTERIZER_NEAR_PLANE;
		case 1: return v.src.z - v.src.x * 0.001f;
		case 2: return v.src.z + v.src.x * 0.001f;
		case 3: return v.src.z - v.src.y * 0.001f;
		case 4: return v.src.z + v.src.y * 0.001f;
		default: __assume(0);
		}
	}
	void Intersect( SProjectedPoint *pRes, const SProjectedPoint &vA, const SProjectedPoint &vB, int nPlane )
	{
		float ffA = ClipDistance( vA, nPlane );
		float ffB = ClipDistance( vB, nPlane );
		float fKoef1 = 1 / (ffB - ffA), fA = fKoef1 * ffB, fB = -fKoef1 * ffA;
		pRes->src.x = vA.src.x * fA + vB.src.x * fB;
		pRes->src.y = vA.src.y * fA + vB.src.y * fB;
		pRes->src.z = vA.src.z * fA + vB.src.z * fB;
		pRes->Project();
	}
	void RenderClipped1( const SProjectedPoint &v1, const SProjectedPoint &v2, const SProjectedPoint &v3, int nPlane )
	{
		SProjectedPoint vI12;
		Intersect( &vI12, v1, v2, nPlane );
		SProjectedPoint vI13;
		Intersect( &vI13, v1, v3, nPlane );
		RasterClipped( v1, vI12, vI13, nPlane + 1 );
	}
	void RenderClipped2( const SProjectedPoint &v1, const SProjectedPoint &v2, const SProjectedPoint &v3, int nPlane )
	{
		SProjectedPoint vI13;
		Intersect( &vI13, v1, v3, nPlane );
		SProjectedPoint vI23;
		Intersect( &vI23, v2, v3, nPlane );
		RasterClipped( v1, vI23, vI13, nPlane + 1 );
		RasterClipped( v1, v2, vI23, nPlane + 1 );
	}
	void RasterClipped( const SProjectedPoint &v1, const SProjectedPoint &v2, const SProjectedPoint &v3, int nPlane )
	{
		unsigned int nTemp = 0;
		for ( ; nPlane < 5; ++nPlane )
		{
			float f1 = ClipDistance( v1, nPlane ), f2 = ClipDistance( v2, nPlane ), f3 = ClipDistance( v3, nPlane );
			nTemp = ( GetBits( &f1 ) < 0 ) * 4 | ( GetBits( &f2 ) < 0 ) * 2 | ( GetBits( &f3 ) < 0 );
			if ( nTemp )
				break;
		}
		if ( nPlane == 5 )
		{
			RasterTriangle( v1.res, v2.res, v3.res );
			return;
		}
		switch ( nTemp )
		{
		case 1:
			RenderClipped2( v1, v2, v3, nPlane );
			break;
		case 2:
			RenderClipped2( v3, v1, v2, nPlane );
			break;
		case 3:
			RenderClipped1( v1, v2, v3, nPlane );
			break;
		case 4:
			RenderClipped2( v2, v3, v1, nPlane );
			break;
		case 5:
			RenderClipped1( v2, v3, v1, nPlane );
			break;
		case 6:
			RenderClipped1( v3, v1, v2, nPlane );
			break;
		case 7:
			break;
		default:
			__assume(0);
			break;
		}
	}
public:
	void Raster( const SProjectedPoint &v1, const SProjectedPoint &v2, const SProjectedPoint &v3 )
	{
		RasterClipped( v1, v2, v3, 0 );
	}
	void RasterNoClip( const CVec3 &v1, const CVec3 &v2, const CVec3 &v3 )
	{
		RasterTriangle( v1, v2, v3 );
	}
	void RasterNoClipFixed( const CVec3 &v1, const CVec3 &v2, const CVec3 &v3 )
	{
		RasterTriangleFixed( v1, v2, v3 );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// special mapping to conform DirectX mapping standard
struct STextureMapping
{
	CVec4 ptDU, ptDV;

	void SetGradient( const CVec4 &srcU, const CVec4 &srcV )
	{
		ptDU.x = srcU.x; ptDU.y = srcU.y; ptDU.w = srcU.w + ( srcU.x + srcU.y ) * 0.5f - 0.5f;
		ptDV.x = srcV.x; ptDV.y = srcV.y; ptDV.w = srcV.w + ( srcV.x + srcV.y ) * 0.5f - 0.5f;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TElement>
class CArrayRasterizer: public CRasterizer<T>
{
public:
	CArray2D<TElement> res;
	//! set region with exclusive upper borders
	void SetRegion( const CTRect<int> &_region )
	{
		region = _region;
		res.SetSizes( region.Width(), region.Height() );
	}
	void SetTextureMapping( const CVec4 &_ptDU, const CVec4 &_ptDV )
	{
		texMapping.SetGradient( _ptDU, _ptDV );
	}
protected:
	CTRect<int> region; // with exclusive borders
	STextureMapping texMapping;
	
	void ClipVertical( int *pnSY, int *pnFY2, int *pnFY )
	{
		(*pnSY) = Max( *pnSY, region.y1 );
		(*pnFY) = Min( *pnFY, region.y2 );
		(*pnFY2) = Min( *pnFY2, region.y2 );
	}
	void ClipHorizontal( int *pnSX, int *pnFX )
	{
		(*pnSX) = Max( *pnSX, region.x1 );
		(*pnFX) = Min( *pnFX, region.x2 );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
