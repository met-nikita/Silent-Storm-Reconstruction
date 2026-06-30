#ifndef __aiRender_H_
#define __aiRender_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\2DArray.h"
#include "aiInterval.h"
#include "Transform.h"
#include "Render.h"
#include "Pool.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_INF = 1e30f;
const int N_F_INF = 0x7149f2ca;//1e30f//*(const int*)&F_INF;
struct SConvexHull;
class CFastRenderer: public CRasterizer<CFastRenderer>
{
public:
	struct SSource
	{
		const SSourceInfo *pSrc;
		int nUserID;

		SSource() {}
		SSource( const SSourceInfo *_pSrc, int _nUserID ): pSrc(_pSrc), nUserID(_nUserID) {}
	};
	struct SResult
	{
		union
		{
			float fDist[2];
			struct 
			{
				float fEnter, fExit;
			};
		};
		SSource *pSrc;
		SResult *pNext;

		const SSourceInfo& GetInfo() const { return *pSrc->pSrc; }
	};

	CArray2D<SResult*> resGrid;

	bool TestSphere( const CVec3 &ptCenter, float fR ) { return ts.IsIn( SSphere( ptCenter, fR ) ); }
	void GetPoints( vector<CVec3> *pEnters, vector<CVec3> *pExits ) const;
	void GetPoints( vector<CVec3> *pEnters, vector<CVec3> *pExits, int x, int y ) const;
	void GetDir( CVec3 *pRes, float x, float y ) const;
	void GetCoordsClamped( const CVec3 &v, float *pX, float *pY );
	//! Angle in radians; region is inclusive
	void InitParallel( const CVec2 &_ptOrigin, float fAngle, float fStep, const CTRect<int> &region );
	void InitParallel( const SHMatrix &cameraPos, float fHalfSize, int nHalfSize );
	//! fAngle is FOV in degrees
	void InitProjective( const SHMatrix &cameraPos, float fDistance, float fAngle, int nHalfSize, float fAspect = 1.0f );
	void InitProjective( const CVec3 &src, const CVec3 &dst, const CVec2 &halfSquare, int nHalfSize );
	void InitProjective( const CVec3 &src, const CVec3 &dst, float fHalfSquare, int nHalfSize );
	void TraceEntity( const vector<SConvexHull> &e, bool bTerrain );
	void TraceEntity( const SConvexHull &e, bool bTerrain );
	void SortIntervals();

private:
	CTRect<int> region; // with exclusive borders
	SSource *pCurrentSource;
	list<SSource> sources;
	SHMatrix transform, backForPoints;
	CTransformStack ts; // for bounding volume tests
	bool bPerspective, bUseInvertOrder;
	float fPerPixelShiftX, fPerPixelShiftY;
	CVec3 ptFrom;
	CArray2D<float> distMult;
	typedef CPool<SResult> TPool;
	TPool res;
	TPool::SIterator objectStart;

	bool DoRenderBackface() const { return true; }
	void ClipVertical( int *pnSY, int *pnFY2, int *pnFY )
	{
		(*pnSY) = Max( *pnSY, region.y1 );
		(*pnFY2) = Min( *pnFY2, region.y2 );
		(*pnFY) = Min( *pnFY, region.y2 );
	}
	void ClipHorizontal( int *pnSX, int *pnFX )
	{
		(*pnSX) = Max( *pnSX, region.x1 );
		(*pnFX) = Min( *pnFX, region.x2 );
	}
	SResult* AddResult()
	{
		SResult *pRes = res.Alloc();
		pRes->fDist[0] = F_INF;
		pRes->fDist[1] = F_INF;
		//pRes->nSourceIdx = nSourceIdx;
		pRes->pSrc = 0;
		return pRes;
	}
	void RasterSpan( int nY, int nLeft, int nRight, float fZ, float fDZ, int nBackface )
	{
		int y = nY - region.y1;
		SResult **pRow = &resGrid[y][0] - region.x1;
		SResult **p = pRow + nLeft, **pFinal = pRow + nRight;
		float *pDistMul = &distMult[y][0] + ( nLeft - region.x1 );
		for (; p < pFinal; ++p, fZ += fDZ, ++pDistMul )
		{
			float fDist = fZ;
			if ( bPerspective )
				fDist = pDistMul[0] / fZ;
			ASSERT( fDist < F_INF );
			SResult **pResGrid = p;
			SResult *pResult = *pResGrid;
			// check if there is place to add new point
			if ( pResult == 0 || pResult->pSrc != 0 || GetBits( &pResult->fDist[nBackface] ) != N_F_INF )
			{
				// has to add new intersection
				SResult *pNode = AddResult();
				pNode->pNext = pResult;
				*pResGrid = pNode;
				pResult = pNode;
			}
			// search for a place to store intersection info
			float *pPrev = &pResult->fDist[nBackface];
			for(;;)
			{
				pResult = pResult->pNext;
				if ( pResult == 0 || pResult->pSrc != 0 )
					break;
				float *pCur = &pResult->fDist[ nBackface ];
				if ( *pCur < fDist )
					break;
				*pPrev = *pCur;
				pPrev = pCur;
			}
			*pPrev = fDist;
		}
	}
	void SetRegion( const CTRect<int> &region );
	void RealTraceEntity( const SConvexHull &e );
	void CalcDistMul();
	void SetSource( const SSourceInfo *_pSrc, int _nUserID );
	void ConvertResults( bool bTerrain );
	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }

	friend class CRasterizer<CFastRenderer>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
