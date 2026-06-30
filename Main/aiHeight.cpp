#include "StdAfx.h"
#include "aiHeight.h"
#include "aiMap.h"
#include "aiRender.h"
#include "aiGridSet.h"
#include "Grid.h"
#include "wTSFlags.h"

externA5 vector<SSphere> sphereParticles;	// CRAP test sphere visualization

namespace NAI
{
const float HEIGHT_MAP_SAMPLE_SIZE = FP_GRID_STEP * 0.333333f;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int Wrap( int n, int nSize )
{
	return ( n + (nSize << 5 ) ) % nSize;//n & (nSize - 1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeightMapBlockInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlockInfo::Init( const CTRect<int> &rect )
{
	ptShift.x = rect.left;
	ptShift.y = rect.top;
	int nXSize = rect.Width() + 1;//GetNextPow2( rect.Width() + 1 );
	int nYSize = rect.Height() + 1;//GetNextPow2( rect.Height() + 1 );
	height.SetSizes( nXSize, nYSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlockInfo::ShowSpheres()
{
	int nXSize = height.GetXSize();
	int nYSize = height.GetYSize();
	int nX = Wrap( ptShift.x, nXSize );
	int nY = Wrap( ptShift.y, nYSize );
	sphereParticles.clear();
	/*for ( int a = 0; a < nXSize; ++a )
	{
		for ( int b = 0; b < nYSize; ++b )
		{*/
	for ( int a = nXSize/2 - 3; a <= nXSize/2 + 3; ++a )
	{
		for ( int b = nYSize/2 - 3; b <= nYSize/2 + 3; ++b )
		{
			int aa = Wrap( (a + nX), nXSize );
			int bb = Wrap( (b + nY), nYSize );
			SSphere s;
			s.ptCenter.x = ( ptShift.x + a ) * HEIGHT_MAP_SAMPLE_SIZE;
			s.ptCenter.y = ( ptShift.y + b ) * HEIGHT_MAP_SAMPLE_SIZE;
			s.ptCenter.z = height[bb][aa];
			s.fRadius = 0.1f;
			sphereParticles.push_back( s );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CHeightMapBlockInfo::Move( CTPoint<int> &ptDest, CTRect<int> *pWasted1, CTRect<int> *pWasted2 )
{
	int nShiftX = ptDest.x - ptShift.x;//(int)( (newCorner.x - corner.x) / HEIGHT_MAP_SAMPLE_SIZE );
	int nShiftY = ptDest.y - ptShift.y;//(int)( (newCorner.y - corner.y) / HEIGHT_MAP_SAMPLE_SIZE );
	
	int nXSize = height.GetXSize();
	int nYSize = height.GetYSize();
	
	ptShift = ptDest;
	int nX = Wrap( ptShift.x, nXSize );
	int nY = Wrap( ptShift.y, nYSize );

	if ( abs(nShiftX) > nXSize - 1 || abs(nShiftY) > nYSize - 1 )
	{
		pWasted1->SetRect( ptDest.x, ptDest.y, ptDest.x + nXSize, ptDest.y + nYSize );
		pWasted2->SetRectEmpty();
		return;
	}
	int nMinX, nMaxX, nMinY, nMaxY;
	if ( nShiftX >= 0 )
	{
		nMinX = nXSize - nShiftX;
		nMaxX = nXSize;
	}
	else
	{
		nMinX = 0;
		nMaxX = - nShiftX;
	}
	if ( nShiftY >= 0 )
	{
		nMinY = nYSize - nShiftY;
		nMaxY = nYSize;
	}
	else
	{
		nMinY = 0;
		nMaxY = - nShiftY;
	}
	pWasted1->SetRect( nMinX + ptShift.x, ptShift.y, nMaxX + ptShift.x, ptShift.y + nYSize );
	nMinX = Max( 0, -nShiftX );
	nMaxX = Min( nXSize, nXSize - nShiftX );
	pWasted2->SetRect( nMinX + ptShift.x, nMinY + ptShift.y, nMaxX + ptShift.x, nMaxY + ptShift.y );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlockInfo::Move( int nDestX, int nDestY )
{
	ptShift.x = nDestX;
	ptShift.y = nDestY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlockInfo::CalcCoords( float fX, float fY, float *pfCoeff )
{
	int nXSize = height.GetXSize();
	int nYSize = height.GetYSize();
	fX = fX / HEIGHT_MAP_SAMPLE_SIZE;
	fY = fY / HEIGHT_MAP_SAMPLE_SIZE;

	if ( fX < 0 )
		fX = 0;
	if ( fY < 0 )
		fY = 0;
	int nX1, nX2, nY1, nY2;
	nX1 = (int)fX;
	nY1 = (int)fY;
	nX1 = Max( nX1, ptShift.x );
	nY1 = Max( nY1, ptShift.y );
	nX1 = Min( nX1, ptShift.x + nXSize - 1 );
	nY1 = Min( nY1, ptShift.y + nYSize - 1 );
	nX2 = nX1;
	nY2 = nY1;
	if ( nX1 == ptShift.x + nXSize - 1 )
		--nX1;
	else
		++nX2;
	if ( nY1 == ptShift.y + nYSize - 1 )
		--nY1;
	else
		++nY2;
	//
	pfCoeff[4] = fX - nX1;
	pfCoeff[5] = fY - nY1;
	nX1 = Wrap( nX1, nXSize );
	nX2 = Wrap( nX2, nXSize );
	nY1 = Wrap( nY1, nYSize );
	nY2 = Wrap( nY2, nYSize );
	pfCoeff[0] = height[nY1][nX1];
	pfCoeff[1] = height[nY1][nX2];
	pfCoeff[2] = height[nY2][nX1];
	pfCoeff[3] = height[nY2][nX2];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CHeightMapBlockInfo::GetHeight( float fX, float fY )
{
	float fRes[6];
	CalcCoords( fX, fY, fRes );
	float h1 = fRes[0] * (1 - fRes[4]) + fRes[1] * fRes[4];
	float h2 = fRes[2] * (1 - fRes[4]) + fRes[3] * fRes[4];
	return ( h1 * (1 - fRes[5]) + h2 * fRes[5] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CHeightMapBlockInfo::GetNormal( float fX, float fY )
{
	float fRes[6];
	CalcCoords( fX, fY, fRes );
	CVec3 v1( HEIGHT_MAP_SAMPLE_SIZE, 0, fRes[1] - fRes[0] );
	CVec3 v2( 0, HEIGHT_MAP_SAMPLE_SIZE, fRes[2] - fRes[0] );
	CVec3 v3 = v1 ^ v2;
	Normalize(&v3);
	return v3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CalcGridPlace( float f )
{
	return Float2Int( f / HEIGHT_MAP_SAMPLE_SIZE );
}
void CalcHeightMapSize( CTRect<int> *pRes, const CVec3 &ptCenter, float fSize )
{
	pRes->SetRect( 
		CalcGridPlace( ptCenter.x - fSize ) - 1, CalcGridPlace( ptCenter.y - fSize ) - 1, 
		CalcGridPlace( ptCenter.x + fSize ) + 1, CalcGridPlace( ptCenter.y + fSize ) + 1 
		);
	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcHeightMap( NAI::IAIMap *pMap, CHeightMapBlockInfo *pRes, const CTRect<int> &rect, const SHeightCalcInfo &hInfo )
{
	if ( rect.IsRectEmpty() )
		return;
	int nXSize = pRes->height.GetXSize();
	int nYSize = pRes->height.GetYSize();
	ASSERT( rect.left >= pRes->ptShift.x );
	ASSERT( rect.right <= pRes->ptShift.x + nXSize );
	ASSERT( rect.top >= pRes->ptShift.y );
	ASSERT( rect.bottom <= pRes->ptShift.y + nYSize );
	NAI::CFastRenderer render;
	render.InitParallel( CVec2(0,0), 0, HEIGHT_MAP_SAMPLE_SIZE, rect );
	pMap->TraceGrid( &render, NWorld::TS_PASS_BLOCKER, IAIMap::STH_NOSORT, CFloorsSet() );
	for ( int y = rect.top; y < rect.bottom; ++y )
	{
		for ( int x = rect.left; x < rect.right; ++x )
		{
			float fRes = 0;
			for ( CFastRenderer::SResult *p = render.resGrid[y-rect.top][x-rect.left]; p; p = p->pNext )
				fRes = Max( fRes, p->fExit );
			pRes->height[ Wrap( y, nYSize ) ][ Wrap( x, nXSize ) ] = fRes;
		}
	}
	for ( list<CVec3>::const_iterator i = hInfo.points.begin(); i != hInfo.points.end(); ++i )
	{
		const CVec3 &p = *i;
		int nX = Float2Int( p.x / HEIGHT_MAP_SAMPLE_SIZE );
		int nY = Float2Int( p.y / HEIGHT_MAP_SAMPLE_SIZE );
		int x1 = Max( nX - 1, rect.minx );
		int x2 = Min( nX + 2, rect.maxx );
		int y1 = Max( nY - 1, rect.miny );
		int y2 = Min( nY + 2, rect.maxy );
		for ( nX = x1; nX < x2; ++nX )
			for ( nY = y1; nY < y2; ++nY )
				pRes->height[ Wrap( nY, nYSize ) ][ Wrap( nX, nXSize ) ] = p.z;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcHeightMap( NAI::IAIMap *pMap, CHeightMapBlockInfo *pRes, const SHeightCalcInfo &hInfo )
{
	CTRect<int> r;
	r.SetRect( 
		pRes->ptShift.x, pRes->ptShift.y, 
		pRes->ptShift.x + pRes->height.GetXSize(), pRes->ptShift.y + pRes->height.GetYSize() );
	CalcHeightMap( pMap, pRes, r, hInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsDoor( CFastRenderer::SResult *p )
{
	return ( p->pSrc->pSrc->nTSFlags & ( NWorld::TS_STATE_OPEN |  NWorld::TS_STATE_CLOSED ) ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcHeightMap( NAI::IAIMap *pMap, CHeightMapBlockInfo *pRes, const float fH, const list<CVec3> &specialPoints )
{
	CTRect<int> rect;
	int nXSize = pRes->height.GetXSize();
	int nYSize = pRes->height.GetYSize();
	rect.SetRect( pRes->ptShift.x, pRes->ptShift.y, pRes->ptShift.x + nXSize, pRes->ptShift.y + nYSize );

	NAI::CFastRenderer render;
	render.InitParallel( CVec2(0,0), 0, HEIGHT_MAP_SAMPLE_SIZE, rect );
	pMap->TraceGrid( &render, NWorld::TS_PASS_BLOCKER, IAIMap::STH_NOSORT, CFloorsSet() );

	// Get info about source for every special point
	unordered_map<int, bool> hSources;
	for ( list<CVec3>::const_iterator i = specialPoints.begin(); i != specialPoints.end(); ++i )
	{
		int nX = Float2Int( i->x / HEIGHT_MAP_SAMPLE_SIZE );
		int nY = Float2Int( i->y / HEIGHT_MAP_SAMPLE_SIZE );
		float fBestDiff = 100;
		int pI = 0;
		if ( nY < rect.top || nX < rect.left || nY - rect.top >= rect.bottom || nX - rect.left >= rect.right )
			continue; // бывает при "переставлении" юнита очень далеко
		for ( CFastRenderer::SResult *p = render.resGrid[nY-rect.top][nX-rect.left]; p; p = p->pNext )
		{
			if ( IsDoor( p ) )
				continue;
			float fDiff = fabs( i->z - p->fExit );
			if ( fDiff < fBestDiff )
			{
				CObjectBase *pObj = p->pSrc->pSrc->pUserData;
				pI = reinterpret_cast<int>( pObj );
				//pI = reinterpret_cast<int>( p->pSrc->pSrc );
				fBestDiff = fDiff;
			}		
		}
		hSources[ pI ] = true;
	}
	// 
	for ( int y = rect.top; y < rect.bottom; ++y )
	{
		for ( int x = rect.left; x < rect.right; ++x )
		{
			float fRes = 0;
			float fBestDiff = 100;
			for ( CFastRenderer::SResult *p = render.resGrid[y-rect.top][x-rect.left]; p; p = p->pNext )
			{
				if ( IsDoor( p ) )
					continue;
				CObjectBase *pObj = p->pSrc->pSrc->pUserData;
				int pInfo = reinterpret_cast<int>( pObj );
				//int pInfo = reinterpret_cast<int>( p->pSrc->pSrc );
				if ( hSources.find( pInfo ) != hSources.end() )
				{
					pRes->height[ Wrap( y, nYSize ) ][ Wrap( x, nXSize ) ] = p->fExit;
					fBestDiff = fabs( fH - p->fExit );
					break;
				}
			}
			if ( fBestDiff > 1 )
			{
				if ( !specialPoints.empty() )
					pRes->height[ Wrap( y, nYSize ) ][ Wrap( x, nXSize ) ] = fH;
				else
				{
					for ( CFastRenderer::SResult *p = render.resGrid[y-rect.top][x-rect.left]; p; p = p->pNext )
					{
						if ( IsDoor( p ) )
							continue;
						float fDiff = fabs( fH - p->fExit );
						if ( p->fExit > fH )
							fDiff *= 3;
						if ( fDiff < fBestDiff )
						{
							fRes = p->fExit;
							fBestDiff = fDiff;
						}
					}
					pRes->height[ Wrap( y, nYSize ) ][ Wrap( x, nXSize ) ] = fRes;
				}
			}
		}
	}
	for ( list<CVec3>::const_iterator i = specialPoints.begin(); i != specialPoints.end(); ++i )
	{
		const CVec3 &p = *i;
		int nX = Float2Int( p.x / HEIGHT_MAP_SAMPLE_SIZE );
		int nY = Float2Int( p.y / HEIGHT_MAP_SAMPLE_SIZE );
		int x1 = Max( nX - 1, rect.minx );
		int x2 = Min( nX + 2, rect.maxx );
		int y1 = Max( nY - 1, rect.miny );
		int y2 = Min( nY + 2, rect.maxy );
		//if ( nX < rect.minx || nY < rect.miny || nX >= rect.maxx || nY >= rect.maxy )
		//	continue;
		//for ( nX = x1; nX < x2; ++nX )
			//for ( nY = y1; nY < y2; ++nY )
				pRes->height[ Wrap( nY, nYSize ) ][ Wrap( nX, nXSize ) ] = p.z;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHeightNode
{
	float h;
	int level;
};
struct SHeightNodeRef
{
	union
	{
		struct
		{
			unsigned char x, y; // unsigned is important
		};
		short n;
	};
	bool operator==( const SHeightNodeRef &a ) const { return a.n == n; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static int delta2[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
static int delta3[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
void CheckGradient( CHeightMapBlockInfo *pRes, float fSampleSize )
{
	if ( fSampleSize == 0 )
		fSampleSize = HEIGHT_MAP_SAMPLE_SIZE;
	float LEVEL_DIFF = (float)(fSampleSize / SQRT_3 * 0.5f);
	int nXSize = pRes->height.GetXSize();
	int nYSize = pRes->height.GetYSize();
	int nX = Wrap( pRes->ptShift.x, nXSize );
	int nY = Wrap( pRes->ptShift.y, nYSize );

	CArray2D<SHeightNode> array;
	vector< vector<SHeightNodeRef> > levels;
	array.SetSizes( nXSize, nYSize );

	int nSamples = Max( nXSize, nYSize );
	levels.resize( nSamples + 3 );
	
	// finding max
	float fMaxHeight = -1e10f;
	for ( int y = 0; y < nYSize; ++y )
	{
		int yy = Wrap( y + nY, nYSize );
		for ( int x = 0; x < nXSize; ++x )
		{
			int xx = Wrap( x + nX, nXSize );
			float h = array[y][x].h = pRes->height[yy][xx];
			if ( h > fMaxHeight )
				fMaxHeight = h;
		}
	}

	int nCenterX = Wrap( nXSize/2 + nX , nXSize );
	int nCenterY = Wrap( nYSize/2 + nY , nYSize );
	float fCenterH = pRes->height[ nCenterY ][ nCenterX ];
	float fCenterRaise = 0;
	int nCenterLevel = int( ( fMaxHeight - fCenterH ) / LEVEL_DIFF );
	SHeightNodeRef refCenter;
	refCenter.x = nXSize/2;
	refCenter.y = nYSize/2;

	SHeightNodeRef ref;
	for ( ref.y = 0; ref.y < nYSize; ++ref.y )
	{
		for ( ref.x = 0; ref.x < nXSize; ++ref.x )
		{
			int nLevel = int( (fMaxHeight - array[ref.y][ref.x].h) / LEVEL_DIFF );
			if ( nLevel > levels.size() - 3 )
				nLevel = levels.size() - 3;
			array[ref.y][ref.x].level = nLevel;
			levels[ nLevel ].push_back( ref );
		}
	}
	// we dont have to check last level
	for ( int nLevelI = 0; nLevelI < levels.size() - 3; ++nLevelI )
	{
		vector<SHeightNodeRef> &curLevel = levels[ nLevelI ];
		for ( int i = 0; i != curLevel.size(); ++i )
		{
			SHeightNodeRef &curRef = curLevel[ i ];
			int nLevel = array[curRef.y][curRef.x].level;
			if ( nLevel == -1 )
				continue;
			ASSERT( nLevelI == nLevel );
			array[curRef.y][curRef.x].level = -1;
			unsigned char ciX = curRef.x, ciY = curRef.y;
			for ( int nDir = 2; nDir <= 3; ++nDir )
			{
				int (*pDelta)[2] = nDir == 2 ? delta2 : delta3;
				float hMin = array[ciY][ciX].h - LEVEL_DIFF * nDir;
				for ( int k = 0; k < 4; ++k )
				{
					ref.x = ciX + pDelta[k][0];
					ref.y = ciY + pDelta[k][1];
					if ( ref.x >= nXSize || ref.y >= nYSize )
						continue;
					if ( array[ref.y][ref.x].level != -1 && array[ref.y][ref.x].h < hMin )
					{
						if ( ref == refCenter )
						{
							// raise all lower levels
							SHeightNode &centerNode = array[ref.y][ref.x];
							float fCurrentAdd = hMin - centerNode.h;
							float fCurrentCenterH = centerNode.h;
							int nLevelAdd = nLevel + nDir - centerNode.level;
//							ASSERT( nLevelAdd <= 0 );
							fCenterRaise += fCurrentAdd;
							SHeightNodeRef ref2;
							for ( ref2.y = 0; ref2.y < nYSize; ++ref2.y )
							{
								for ( ref2.x = 0; ref2.x < nXSize; ++ref2.x )
								{
									SHeightNode &n = array[ref2.y][ref2.x];
									if ( n.h <= fCurrentCenterH )
									{
										n.h += fCurrentAdd;
										pRes->height[ Wrap( ref2.y + nY, nYSize ) ][ Wrap( ref2.x + nX, nXSize ) ] += fCurrentAdd;
										int nNewLevel = int( (fMaxHeight - n.h) / LEVEL_DIFF );
										if ( nNewLevel > levels.size() - 3 )
											nNewLevel = levels.size() - 3;
										if ( nNewLevel != n.level )
										{
											ASSERT( nNewLevel >= nLevel );
											n.level = nNewLevel;
											levels[ nNewLevel ].push_back( ref2 );
										}
									}
								}
							}
							nCenterLevel += nLevelAdd;
							//ASSERT( fCenterRaise == pRes->height[ nCenterY ][ nCenterX ] - fCenterH );
						}
						else
						{
							if ( nLevel + nDir < levels.size() - 3)
							{
								ASSERT( nLevel + nDir <= array[ref.y][ref.x].level);
								array[ref.y][ref.x].level = nLevel + nDir;
								levels[ nLevel + nDir ].push_back( ref );
							}
							array[ref.y][ref.x].h = pRes->height[ Wrap( ref.y + nY, nYSize ) ][ Wrap( ref.x + nX, nXSize ) ] = hMin;
						}
					}
				}
			}
		}
	}
	for ( int y = 0; y < nYSize; ++y )
	{
		int yy = Wrap( y + nY, nYSize );
		for ( int x = 0; x < nXSize; ++x )
		{
			int xx = Wrap( x + nX, nXSize );
			pRes->height[yy][xx] -= fCenterRaise;
		}
	}
	float fFinalCenterRaise = 	fCenterH - pRes->height[ nCenterY ][ nCenterX ];
	ASSERT( fFinalCenterRaise < 0.00001 && fFinalCenterRaise > -0.00001 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
BASIC_REGISTER_CLASS( CHeightMapBlockInfo )