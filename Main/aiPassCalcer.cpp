#include "StdAfx.h"
#include "aiPassCalcer.h"
#include "aiGrid.h"
#include "aiCollider.h"
#include "Grid.h"
#include "aiHeight.h"
#include "aiRender.h"
#include "aiMap.h"
#include "wTSFlags.h"
#include "aiGridSet.h"
#include "aiPMConst.h"
#include "wInterface.h"
#include "aiDoorCollider.h"
#include "..\MiscDll\LogStream.h"
namespace NAI
{

const bool B_3x3_RENDER = false;

const char N_FLAG_NATIVE    = 1;
const char N_FLAG_MUST_CALC = 2;
const char N_FLAG_IGNORED   = 4;
const char N_FLAG_NO_CALC		= 8;
const char N_FLAG_ADDITION	= 16;
const char N_FLAG_LADDER		= 32;
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
static bool IsInsideArray( int x, int y, const CArray2D<T> &arr )
{
	if ( B_3x3_RENDER )
		return x > 0 && y > 0 && x <= arr.GetXSize() - 2 && y <= arr.GetYSize() - 2;
	else
		return x > 1 && y > 1 && x <= arr.GetXSize() - 1 && y <= arr.GetYSize() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasGoodDiff( const CFastRenderer::SResult *pRes, float fH )
{
	for ( ; pRes; pRes = pRes->pNext )
	{
		float fDiff = fH - pRes->fExit;
		if ( fDiff > -F_MIN_CLIMB_HEIGHT && fDiff < F_MIN_CLIMB_HEIGHT )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPassCalcer::IsInactivePoint( int x, int y, float fH, const CArray2D<char> &flags )
{
	if ( B_3x3_RENDER )
	{
		for ( int dX = -1; dX < 2; ++dX )
		{
			for ( int dY = -1; dY < 2; ++dY )
			{
				CFastRenderer::SResult *pRes = render.resGrid[ y + dY ][ x + dX ];
				if ( !HasGoodDiff( pRes, fH ) )
					return true;
			}
		}
		return false;
	}
	else
	{
		CFastRenderer::SResult *pRes = render.resGrid[ y ][ x ];
		return !HasGoodDiff( pRes, fH );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPassCalcer::IsInactivePoint( int x, int y, const CArray2D<unsigned short> &tempH, const CArray2D<char> &flags )
{
	if ( !IsInsideArray( x, y, tempH ) )
		return false;
	if ( flags[y][x] & N_FLAG_LADDER )
		return true;
	float fH = GetFHeight( tempH[ y ][ x ] );
	return IsInactivePoint( x, y, fH, flags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MarkNeighbors( CArray2D<char> *pRes, const CTPoint<int> &shift, bool bMarkAllMustCalc = false )
{
	CArray2D<char> &flags = *pRes;
	int nMinX = Max( -shift.x, 0 );
	int nMinY = Max( -shift.y, 0 );
	int nMaxX = Min( flags.GetXSize() - shift.x, flags.GetXSize() );
	int nMaxY = Min( flags.GetYSize() - shift.y, flags.GetYSize() );
	for ( int y = nMinY; y < nMaxY; ++y )
	{
		for ( int x = nMinX; x < nMaxX; ++x )
		{
			if ( bMarkAllMustCalc )
				flags[y][x] |= N_FLAG_NATIVE;
			if ( flags[ y ][ x ] & N_FLAG_NATIVE ) 
				flags[ y + shift.y ][ x + shift.x ] |= N_FLAG_MUST_CALC;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::MarkInactiveEnlargements( CArray2D<char> *pRes, const CTPoint<int> &shift, const CArray2D<unsigned short> &tempH,
	const CFastRenderer &render )
{
	CArray2D<char> &flags = *pRes;
	int nMinX = Max( -shift.x, 0 );
	int nMinY = Max( -shift.y, 0 );
	int nMaxX = Min( flags.GetXSize() - shift.x, flags.GetXSize() );
	int nMaxY = Min( flags.GetYSize() - shift.y, flags.GetYSize() );
	// mark inactive, non-native points' neighbours as must calc too
	for ( int y = nMinY; y < nMaxY; ++y )
	{
		for ( int x = nMinX; x < nMaxX; ++x )
		{
			if ( flags[ y + shift.y ][ x + shift.x ] & N_FLAG_NATIVE ) 
				continue;
			if ( ( flags[ y ][ x ] & N_FLAG_MUST_CALC ) == 0 )
				continue;
			if ( !IsInactivePoint( x, y, tempH, flags ) )
				continue;
			flags[ y + shift.y ][ x + shift.x ] |= N_FLAG_ADDITION;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void FinishMarking( const CFastRenderer &render, CArray2D<char> *pRes )
{
	CArray2D<char> &flags = *pRes;
	for ( int y = 0; y < flags.GetYSize(); ++y )
	{
		for ( int x = 0; x < flags.GetXSize(); ++x )
		{
			if ( flags[y][x] & N_FLAG_ADDITION )
				flags[y][x] |= N_FLAG_MUST_CALC;
			CFastRenderer::SResult *p = render.resGrid[y][x];
			if ( !p )
				flags[y][x] &= ~N_FLAG_MUST_CALC;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPassCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CPassCalcer::	CPassCalcer( CPathNetwork *_pNet, CLayersGroup *_pGroup, IAIMap *_pMap,	STempArrayGroup<CNodesLayer::STile> &_tempArrays, const CTRect<int> &_region )
: pMap(_pMap), region(_region), pGroup(_pGroup), pNet( _pNet ), tempArrays( _tempArrays )
{
	ASSERT( IsValid( pMap ) );
	ASSERT( IsValid( pNet ) );
	tempArrays.region = region;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::CutBorders( int nFloor, int nLayer )
{
	int nTilesXSize, nTilesYSize;
	if ( B_3x3_RENDER )
	{
		nTilesXSize = pGroup->GetXSize() * 3;
		nTilesYSize = pGroup->GetYSize() * 3;
	}
	else
	{
		nTilesXSize = pGroup->GetXSize();
		nTilesYSize = pGroup->GetYSize();
	}
	CArray2D<char> &flags = *tempFlags.GetArray( nFloor, nLayer );
	for ( int nX = region.minx; nX < 0; ++nX )
	{
		for ( int nY = 0; nY < flags.GetYSize(); ++nY )
		{
			flags[nY][nX-region.minx] = 0;
			if ( B_3x3_RENDER )
			{
				flags[nY][nX-region.minx+1] = 0;
				flags[nY][nX-region.minx+2] = 0;
			}
		}
	}
	for ( int nX = nTilesXSize; nX < region.maxx; ++nX )
	{
		for ( int nY = 0; nY < flags.GetYSize(); ++nY )
		{
			flags[nY][nX-region.minx] = 0;
			if ( B_3x3_RENDER )
			{
				flags[nY][nX-region.minx+1] = 0;
				flags[nY][nX-region.minx+2] = 0;
			}
		}
	}
	for ( int nY = region.miny; nY < 0; ++nY )
	{
		for ( int nX = 0; nX < flags.GetXSize(); ++nX )
		{
			flags[nY-region.miny][nX] = 0;
			if ( B_3x3_RENDER )
			{
				flags[nY-region.miny+1][nX] = 0;
				flags[nY-region.miny+2][nX] = 0;
			}
		}
	}
	for ( int nY = nTilesYSize; nY < region.maxy; ++nY )
	{
		for ( int nX = 0; nX < flags.GetXSize(); ++nX )
		{
			flags[nY-region.miny][nX] = 0;
			if ( B_3x3_RENDER )
			{
				flags[nY-region.miny+1][nX] = 0;
				flags[nY-region.miny+2][nX] = 0;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_HEIGHT_MARGIN = 0.4f;
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPassCalcer::MarkIgnored( int nFloor, int nLayer, const CArray2D<STile> & temp )
{
	CNodesLayer *pLayer = pGroup->layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
	CArray2D<char> &flags = *tempFlags.GetArray( nFloor, nLayer );
	const vector<CLayersGroup::SIgnoreRect> &ignoreRects = pGroup->ignoreRects;
	int nCount = 0;
	for ( int y = 0; y < temp.GetYSize(); ++y )
	{
		for ( int x = 0; x < temp.GetXSize(); ++x )
		{
			CVec2 ptTest = pGroup->GetCPNoHeight( x + region.minx, y + region.miny );
			for ( int k = 0; k < ignoreRects.size(); ++k )
			{
				if ( ignoreRects[k].IsInside( ptTest ) )
				{
					if ( B_3x3_RENDER )
					{
						for ( int dY = 0; dY < 3; ++dY )
							for ( int dX = 0; dX < 3; ++dX )
								flags[y * 3 + dY ][x * 3 + dX ] |= N_FLAG_IGNORED;
					}
					else
						flags[y][x] |= N_FLAG_IGNORED;
				}
				else
					++nCount;
			}
		}
	}
	return nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::KillNonNativeLayMoves( CArray2D<STile> *pRes, const CTPoint<int> &shift, 
	char nFwdFlag, char nBackFlag, const CArray2D<char> &flags )
{
	CArray2D<STile> &temp = *pRes;
	int nStartX = 1, nStartY = 1, nFinishX = temp.GetXSize() - 1, nFinishY = temp.GetYSize() - 1;
	nStartX -= shift.x > 0;
	nStartY -= shift.y > 0;
	nFinishX += shift.x < 0;
	nFinishY += shift.y < 0;
	for ( int y = nStartY; y < nFinishY; ++y )
	{
		for ( int x = nStartX; x < nFinishX; ++x )
		{
			if ( B_3x3_RENDER )
			{
				if ( (flags[y*3+1][x*3+1] | flags[y*3+1+shift.y*3][x*3+1+shift.x*3]) & ( N_FLAG_NATIVE | N_FLAG_ADDITION ) )
					continue;
			}
			else
			{
				if ( (flags[y][x] | flags[y+shift.y][x+shift.x]) & ( N_FLAG_NATIVE | N_FLAG_ADDITION ) )
					continue;
			}
			STile &tSrc = temp[y][x];
			STile &tDst = temp[y+shift.y][x+shift.x];
			tSrc.nMoveLay &= ~nFwdFlag;
			tDst.nMoveLay &= ~nBackFlag;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::TestLayMovesInDirection( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CArray2D<STile> *pResPrev,
	const CTPoint<int> &shift, float fHDiffLimit, const CArray2D<SDoorColliderAnalyzer> &checkSphOk, char nFwdFlag, 
	char nBackFlag, int nLayer )
{
	const CVec2 &ptXDir = pGroup->ptXDir;
	CArray2D<STile> &temp = *pRes;
	int nStartX = 1, nStartY = 1, nFinishX = temp.GetXSize() - 1, nFinishY = temp.GetYSize() - 1;
	nStartX -= shift.x > 0;
	nStartY -= shift.y > 0;
	nFinishX += shift.x < 0;
	nFinishY += shift.y < 0;
	CVec2 ptMove = shift.x * ptXDir + shift.y * CVec2( -ptXDir.y, ptXDir.x );
	float fStep = fabs( ptMove );// Grid step or sqrt(2)*grid step
	for ( int y = nStartY; y < nFinishY; ++y )
	{
		for ( int x = nStartX; x < nFinishX; ++x )
		{
			STile &tSrc = temp[y][x];
			STile &tDst = temp[y+shift.y][x+shift.x];
			if ( pResPrev )
			{
				const CArray2D<STile> &prev = *pResPrev;
				if ( tSrc.nHeight == prev[y][x].nHeight && 
						 tDst.nHeight == prev[y+shift.y][x+shift.x].nHeight )
					continue;
			}
			float fSrcHeight = GetFHeight( tSrc.nHeight );
			float fDstHeight = GetFHeight( tDst.nHeight );
			float fDiffH = fabs( fSrcHeight - fDstHeight );
			if ( fDiffH >= fHDiffLimit )
				continue;
			CVec2 displSrc( F_DISPLACEMENT_X[ tSrc.nDisplacement ], F_DISPLACEMENT_Y[ tSrc.nDisplacement ] );
			CVec2 displDst( F_DISPLACEMENT_X[ tDst.nDisplacement ], F_DISPLACEMENT_Y[ tDst.nDisplacement ] );
			CVec2 cp = pGroup->GetCPNoHeight( x + region.minx, y + region.miny );
			CVec3 ptCenter( cp + displSrc, fSrcHeight + F_CHECK_HEIGHT_MOVE + 0.05f ), 
				vel( ptMove + displDst - displSrc, fDstHeight - fSrcHeight );
			SDoorColliderAnalyzer analyzer;
			if ( fDiffH > 0.05f )
			{
				// тут надо по идее строить перпендикуляр, чтобы правильно ставить сферы
        // пока будет Crap, заключающийся просто в том, что мы их просто подымем повыше
				// на ту высоту, на которой они уже не задевают исходные точки
				float fHypotenuse = fabs( vel );
				float fCosInv = fHypotenuse / fStep;
				ptCenter.z = fSrcHeight + F_CHECK_HEIGHT_MOVE * fCosInv + 0.15f;
				if ( pCollider->DoesIntersect( ptCenter, F_TEST_SPHERE_RADIUS, &analyzer ) )
					continue;	
			}
			else
			{
				if ( checkSphOk[y][x].IsCollided() )
					continue;
				analyzer = checkSphOk[y][x];
			}
			SSphere sph( ptCenter, F_TEST_SPHERE_RADIUS );			
			pCollider->CollideBool( sph, vel, &analyzer );
			if ( !analyzer.IsCollided() )
			{
				tSrc.nMoveLay |= nFwdFlag;
				tDst.nMoveLay |= nBackFlag;
				if ( analyzer.pSrc )
				{
					STile &tileF = GetFlipperTile( x, y, nLayer, analyzer, tSrc, &tSrc.nFlipper );
					tileF.nMoveLay &= ~nFwdFlag;
					STile &tileB = GetFlipperTile( x + shift.x, y + shift.y, nLayer, analyzer, tDst, &tDst.nFlipper );
					tileB.nMoveLay &= ~nBackFlag;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::TestLayMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CArray2D<STile> *pResPrev, 
	const CArray2D<char> &flags, const CArray2D<SDoorColliderAnalyzer> &checkSphOk, int nLayer )
{
	TestLayMovesInDirection( pCollider, pRes, pResPrev, CTPoint<int>(1,0), F_NORMAL_HEIGHT_DIFF_LIMIT, 
		checkSphOk, 1<<CD_R, 1<<CD_L, nLayer );
	TestLayMovesInDirection( pCollider, pRes, pResPrev, CTPoint<int>(1,1), F_DIAGONAL_HEIGHT_DIFF_LIMIT,
		checkSphOk, 1<<CD_RU, 1<<CD_LD, nLayer );
	TestLayMovesInDirection( pCollider, pRes, pResPrev, CTPoint<int>(0,1), F_NORMAL_HEIGHT_DIFF_LIMIT, 
		checkSphOk, 1<<CD_U, 1<<CD_D, nLayer );
	TestLayMovesInDirection( pCollider, pRes, pResPrev, CTPoint<int>(-1,1), F_DIAGONAL_HEIGHT_DIFF_LIMIT, 
		checkSphOk, 1<<CD_LU, (char)(1<<CD_RD), nLayer );

	CArray2D<STile> &temp = *pRes;
	// Now test lay poses according to theese lay moves
	for ( int y = 1; y < temp.GetYSize()-1; ++y )
	{
		for ( int x = 1; x < temp.GetXSize()-1; ++x )
		{
			STile &tile = temp[y][x];
			if ( ! ( tile.nPassable & CP_LAY ) )
				continue;
			// test laypose 1
			// here and later x * 17 is x | x * 16, that is the direction flag and the opposite direction flag
			if ( ( tile.nMoveLay & ( 1 * 17 ) ) == 1 * 17 )
				tile.nPassable |= CP_LAY1;
			else
				tile.nPassable &= ~CP_LAY1;
			// test laypose 2
			if ( ( tile.nMoveLay & ( 2 * 17 ) ) == 2 * 17 )
				tile.nPassable |= CP_LAY2;
			else
				tile.nPassable &= ~CP_LAY2;
			// test laypose 3
			if ( ( tile.nMoveLay & ( 4 * 17 ) ) == 4 * 17 )
				tile.nPassable |= CP_LAY3;
			else
				tile.nPassable &= ~CP_LAY3;
			// test laypose 4
			if ( ( tile.nMoveLay & ( 8 * 17 ) ) == 8 * 17 )
				tile.nPassable |= CP_LAY4;
			else
				tile.nPassable &= ~CP_LAY4;
		}
	}
	// Now "kill" lay moves between non-natives
	KillNonNativeLayMoves( pRes, CTPoint<int>(1,0), 1<<CD_R, 1<<CD_L, flags );
	KillNonNativeLayMoves( pRes, CTPoint<int>(1,1), 1<<CD_RU, 1<<CD_LD, flags );
	KillNonNativeLayMoves( pRes, CTPoint<int>(0,1), 1<<CD_U, 1<<CD_D, flags );
	KillNonNativeLayMoves( pRes, CTPoint<int>(-1,1), 1<<CD_LU, (char)(1<<CD_RD), flags );//*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPassCalcer::GetNFloor( CFastRenderer::SResult *p )
{
	if ( p->pSrc->pSrc->pUserData == 0 ) // it's a terrain
		return Max( 0, - pGroup->nFirstFloor );// nFirstFloor can be negative for maps with subterranean layers
	else
		return p->pSrc->pSrc->nFloor + Max( 0, - pGroup->nFirstFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPassCalcer::CalcColliderSize( int nFloor, int nLayer, float *pMinH, float *pMaxH, CVec2 *pPMin, CVec2 *pPMax )
{
	int nSpotsCount = 0;
	CArray2D<unsigned short> &tempH = *tempHeights.GetArray( nFloor, nLayer );
	CArray2D<char> &flags = *tempFlags.GetArray( nFloor, nLayer );
	float &fMinHeight = *pMinH, &fMaxHeight = *pMaxH;
	CVec2 &ptMin = *pPMin, &ptMax = *pPMax;
	for ( int y = 0; y < tempH.GetYSize(); ++y )
	{
		for ( int x = 0; x < tempH.GetXSize(); ++x )
		{
			CVec2 cp;
			if ( B_3x3_RENDER )
				cp = pGroup->GetCPNoHeight( x/3 + region.minx, y/3 + region.miny );
			else
				cp = pGroup->GetCPNoHeight( x + region.minx, y + region.miny );
			ptMin.Minimize( cp );
			ptMax.Maximize( cp );
			unsigned short &nHeight = tempH[y][x];
			float fHeight = GetFHeight( nHeight );
			fMinHeight = Min( fMinHeight, fHeight );
			fMaxHeight = Max( fMaxHeight, fHeight );
			if ( ( flags[y][x] & N_FLAG_MUST_CALC ) == 0 )
				continue;
			++nSpotsCount;
		}
	}
	return nSpotsCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CPassCalcer::InactivePointHeight( int x, int y, const CArray2D<unsigned short> &tempH )
{
	if ( B_3x3_RENDER )
	{
		unsigned short nHeight = 0;
		for ( int dX = -1; dX < 2; ++dX )
			for ( int dY = -1; dY < 2; ++dY )
				if ( tempH[ y + dY ][ x + dX ] > nHeight )
					nHeight = tempH[ y + dY ][ x + dX ];
		return GetFHeight( nHeight );
	}
	else
		return GetFHeight( tempH[y][x] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasFlag( int x, int y, const CArray2D<char> &flags, char flag )
{
	if ( !B_3x3_RENDER )
		return flags[ y ][ x ] & flag;

	if ( !IsInsideArray( x, y, flags ) )
		return flags[ y ][ x ] & flag;
	for ( int dX = -1; dX < 2; ++dX )
	{
		for ( int dY = -1; dY < 2; ++dY )
		{
			if ( flags[ y + dY ][ x + dX ] & flag )
				return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int N_DISPLACEMENT_X[] = { 1, 1, 2, 1, 0, 0, 2, 2, 0 };
static int N_DISPLACEMENT_Y[] = { 1, 2, 1, 0, 1, 2, 2, 0, 0 };
////////////////////////////////////////////////////////////////////////////////////////////////////
CPassCalcer::STile &CPassCalcer::GetFlipperTile(
	int x, int y, int nLayer, const SDoorColliderAnalyzer &analyzer, const STile &t, unsigned char *pNFlipper )
{
	CNodesLayer *pL = pNet->GetLayer( nLayer );
	if ( !IsInArray( pL->tiles, x + region.minx, y + region.miny ) )
	{
		*pNFlipper = 0;
		return fake;
	}
	SPathPlace p( x + region.minx, y + region.miny, nLayer );
	CPtr<CObjectBase> iHateVCPP( analyzer.pSrc );
	CPathNetwork::SFlipper &flipper = *pNet->GetFlipper( iHateVCPP );
	*pNFlipper = flipper.nFlipper + 1;
	typedef unordered_map<SPathPlace, STile,SPathPlaceHash> CFHash;
	CFHash *pHash;
	if ( analyzer.bInClosed )
		pHash = &flipper.locksClosed;
	else
		pHash = &flipper.locksOpen;
	CFHash::iterator it = pHash->find( p );
	if ( it != pHash->end() )
		return it->second;
	else
	{
		STile &ft = (*pHash)[ p ];
		ft = t;
		ft.nMoveLay = ft.nMoveCrouch = ft.nMoveStand = ft.nMoveHC = (char)255;
		return ft;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPassCalcer::CalcPoseInPoint( int x, int y, NAI::CCollider *pCollider, const CArray2D<unsigned short> &tempH, 
		CArray2D<STile> *pTemp, const CArray2D<char> &flags, int nLayer, int nDis, bool bLastLayer )
{
	NAI::CCollider &collider = *pCollider;
	CArray2D<STile> &temp = *pTemp;
	int nY, nX;
	if ( B_3x3_RENDER )
	{
		nY = y*3 + N_DISPLACEMENT_Y[nDis];
		nX = x*3 + N_DISPLACEMENT_X[nDis];
	}
	else
	{
		nY = y;
		nX = x;
	}
	if ( !HasFlag( nX, nY, flags, N_FLAG_MUST_CALC ) )
		return 0;
	STile &tile = temp[y][x];
	tile.nHeight = tempH[ nY ][ nX ];	
	CVec2 cp = pGroup->GetCPNoHeight( x + region.minx, y + region.miny ) + 
		CVec2( F_DISPLACEMENT_X[nDis], F_DISPLACEMENT_Y[nDis] );
	CVec3 ptTest( cp.x, cp.y, GetFHeight( tempH[ nY ][ nX ] ) );
	ptTest.z += F_CHECK_HEIGHT;
	SDoorColliderAnalyzer analyzer;
	bool bWorkWithFlipperFinished = false;
	bool bCollides = collider.DoesIntersect( ptTest, F_TEST_SPHERE_RADIUS, &analyzer );
	if ( analyzer.pSrc && (!bCollides) ) // no inactive points in flipping objects
	{
		STile &tileF = GetFlipperTile( x, y, nLayer, analyzer, tile, &tile.nFlipper );
		tileF.nDisplacement = nDis;
		bWorkWithFlipperFinished = true;
	}
	if ( flags[ nY ][ nX ] & N_FLAG_LADDER )
		tile.nFlags |= TF_IS_LADDER_UP;
	if ( IsInactivePoint( nX, nY, tempH, flags ) || bCollides )
	{
		if ( nDis )
			return 0;
		if ( bCollides && !bLastLayer ) // над нами на данном этаже еще есть что-то, а значит считать инэктив здесь бесполезно
			return 0;
		if ( !HasFlag( nX, nY, flags, N_FLAG_NATIVE ) )
			return 0;
		float fTestZ = InactivePointHeight( nX, nY, tempH ), fDZ = 0, fLimitZ = F_MAX_HEIGHT, fHeightForZ = F_MAX_HEIGHT;
		// find maximal height where it is reasonable to search for a place
		for ( CFastRenderer::SResult *p = render.resGrid[ nY ][ nX ]; p; p = p->pNext )
		{
			//if ( p->fExit > fTestZ + F_TEST_SPHERE_RADIUS )
			if ( p->fExit > ptTest.z )
			{
				fLimitZ = Min( fLimitZ, float( p->fExit - (F_CHECK_HEIGHT - F_TEST_SPHERE_RADIUS ) ) );
				fHeightForZ = Min( fHeightForZ, p->fExit );
			}
		}
		// do search for a place
		if ( !bCollides )
			fDZ -= F_SPECIAL_POINT_TEST_STEP;
		bool bContinueCount = false;
		for(;;)
		{
			fDZ += F_SPECIAL_POINT_TEST_STEP;
			ptTest.z = fTestZ + fDZ + F_TEST_SPHERE_RADIUS; 
			if ( fDZ > F_MAX_SPECIAL_POINT_HEIGHT )
				return 0;
			if ( ptTest.z >= fLimitZ )
			{
				ptTest.z = fHeightForZ + F_CHECK_HEIGHT;
				analyzer.Clear();
				bCollides = collider.DoesIntersect( ptTest, F_TEST_SPHERE_RADIUS, &analyzer );
				if ( bCollides )
					return 0;
				// we can place "same", and we do not collide -> place same
				tile.nHeight = GetIHeight( fHeightForZ );
				bContinueCount = true;
				break;
			}
			analyzer.Clear();
			bool bCollidesNow = 
				collider.DoesIntersect( ptTest, F_TEST_SPHERE_RADIUS, &analyzer ) ||
				collider.DoesIntersect( ptTest + CVec3( 0, 0, F_TEST_SPHERE_STEP ), F_TEST_SPHERE_RADIUS, &analyzer );
			if ( bCollidesNow )
				continue;
			// Special point is found!
			tile.nHeight = GetIHeight( fTestZ + fDZ );
			tile.nPassable = CP_INACTIVE;
			break;
		}
		if ( !bContinueCount ) 
			return 2;
	}
	tile.nPassable = CP_LAY;
	ptTest.z += F_TEST_SPHERE_STEP;
	bCollides = bCollides || collider.DoesIntersect( ptTest, F_TEST_SPHERE_RADIUS, &analyzer );
	if ( analyzer.pSrc && !bCollides && !bWorkWithFlipperFinished ) 
	{
		STile &tileF = GetFlipperTile( x, y, nLayer, analyzer, tile, &tile.nFlipper );
		tileF.nDisplacement = nDis;
		tileF.nPassable = CP_LAY;
		bWorkWithFlipperFinished = true;
	}
	if ( bCollides )
		return 1;
	tile.nPassable |= CP_CROUCH;
	ptTest.z += F_TEST_SPHERE_STEP;
	bCollides = collider.DoesIntersect( ptTest, F_TEST_SPHERE_RADIUS, &analyzer );
	if ( analyzer.pSrc && !bCollides && !bWorkWithFlipperFinished ) 
	{
		STile &tileF = GetFlipperTile( x, y, nLayer, analyzer, tile, &tile.nFlipper );
		tileF.nDisplacement = nDis;
		tileF.nPassable = CP_LAY | CP_CROUCH;
		bWorkWithFlipperFinished = true;
	}
	if ( bCollides )
		return 2;
	tile.nPassable |= CP_STAND;
	return 3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MarkCandidatesToDisplace( CArray2D<char> *pSpH, const CArray2D<CPassCalcer::STile> &tiles )
{
	ASSERT( B_3x3_RENDER );
	CArray2D<char> &sphereHeight = *pSpH;
	for ( int y = 1; y < tiles.GetYSize() - 1; ++y )
	{
		for ( int x = 1; x < tiles.GetXSize() - 1; ++x )
		{
			if ( ( tiles[y][x].nPassable & CP_INACTIVE ) == 0 )
				continue;
			if ( tiles[y][x].nFlags & TF_IS_LADDER_UP )
			{
				sphereHeight[y][x] = 3;
				sphereHeight[y-1][x] = 3;
				sphereHeight[y+1][x] = 3;
				sphereHeight[y][x-1] = 3;
				sphereHeight[y][x+1] = 3;
			}
			sphereHeight[y][x] = 0;
			bool b1 = tiles[y-1][x].nPassable & CP_CROUCH;
			bool b2 = tiles[y+1][x].nPassable & CP_CROUCH;
			bool b3 = tiles[y][x-1].nPassable & CP_CROUCH;
			bool b4 = tiles[y][x+1].nPassable & CP_CROUCH;
			int nH = tiles[y][x].nHeight;
			int nDH1 = abs( tiles[y-1][x].nHeight - nH );
			int nDH2 = abs( tiles[y+1][x].nHeight - nH );
			int nDH3 = abs( tiles[y][x-1].nHeight - nH );
			int nDH4 = abs( tiles[y][x+1].nHeight - nH );
			int nMinH = GetIHeight( F_MIN_CLIMB_HEIGHT );
			if ( b1 && b2 && ( nDH1 >= nMinH || nDH2 >= nMinH ) )
			{
				sphereHeight[y][x] = 3;
				sphereHeight[y-1][x] = 3;
				sphereHeight[y+1][x] = 3;
			}
			if ( b3 && b4 && ( nDH3 >= nMinH || nDH4 >= nMinH ) )
			{
				sphereHeight[y][x] = 3;
				sphereHeight[y][x-1] = 3;
				sphereHeight[y][x+1] = 3;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::CalcDisplacements(
	NAI::CCollider *pCollider, const CArray2D<unsigned short> &tempH, const CArray2D<unsigned short> *pTempPrevH,
	CArray2D<STile> *pTemp, const CArray2D<STile> *pTempPrev, const CArray2D<char> &flags,
	const CArray2D<char> &sphereHeight, const CArray2D<char> *pSphereHeightPrev, int nLayer )
{
	ASSERT( B_3x3_RENDER );
	CArray2D<STile> &temp = *pTemp;
	for ( int y = 0; y < pTemp->GetYSize(); ++y )
	{
		for ( int x = 0; x < pTemp->GetXSize(); ++x )
		{
			temp[y][x].nDisplacement = 0;
			int nBest = sphereHeight[y][x] * 2 + 1;
			if ( nBest >= 6 || ( flags[ y*3 + 1 ][ x*3 + 1 ] & N_FLAG_MUST_CALC ) == 0 )
				continue;
			bool bSameAsPrevLayer;
			if ( pSphereHeightPrev ) 
				bSameAsPrevLayer = ( sphereHeight[y][x] == (*pSphereHeightPrev)[y][x] );
			else
				bSameAsPrevLayer = 0;
			if ( bSameAsPrevLayer )
			{
				const CArray2D<unsigned short> &tempPrevH = *pTempPrevH;
				for ( int i = 0; i < 3; ++i )
				{
					for ( int j = 0; j < 3; ++j )
					{
						if ( tempH[ y*3 +j ][ x*3 + i ] != tempPrevH[ y*3 +j ][ x*3 + i ] )
						{
							bSameAsPrevLayer = false;
							break;
						}
					}
				}
				if ( bSameAsPrevLayer )
				{
					const CArray2D<STile> &tempPrev = *pTempPrev;
					temp[y][x].nPassable = tempPrev[y][x].nPassable;
					temp[y][x].nDisplacement = tempPrev[y][x].nDisplacement;
					temp[y][x].nHeight = tempPrev[y][x].nHeight;
					continue;
				}
			}
			int nBestD = 0, nBestFlags = temp[y][x].nPassable;
			unsigned short nBestHeight = temp[y][x].nHeight;
			for ( int nDisplacement = 1; nDisplacement < 9; ++nDisplacement )
			{
				temp[y][x].nPassable = 0;
				int nH = CalcPoseInPoint( x, y, pCollider, tempH, pTemp, flags, nLayer, nDisplacement, false ) * 2 
					+ ( (nDisplacement < 5) ? 1 : 0 );
				if ( nH > nBest )				
				{
					nBestD = nDisplacement;
					nBest = nH;
					nBestFlags = temp[y][x].nPassable;
					nBestHeight = temp[y][x].nHeight;
				}
				if ( nH >= 6 )
					break;
			}
			temp[y][x].nPassable = nBestFlags;
			ASSERT( !( nBestFlags & CP_INACTIVE ) || !( nBestFlags & CP_LAY ) );
			temp[y][x].nDisplacement = nBestD;
			temp[y][x].nHeight = nBestHeight;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsDoor( CFastRenderer::SResult *p )
{
	return ( p->pSrc->pSrc->nTSFlags & ( NWorld::TS_STATE_OPEN |  NWorld::TS_STATE_CLOSED ) ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::CreateAdditionalLayers()
{
	float fBadHeight = 0;
	CVec2 ptBadHeightCP;
	bool bBadFloor = false;
	ZeroMemory( nLayersMustHave, N_MAX_FLOORS );
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		nIntersectsOnFloor[ nFloor ].SetSizes( render.resGrid.GetXSize(), render.resGrid.GetYSize() );
		nIntersectsOnFloor[ nFloor ].FillZero();
	}
	for ( int i = 0; i < render.resGrid.GetXSize(); ++i )
	{
		for ( int j = 0; j < render.resGrid.GetYSize(); ++j )
		{
			bool bHasOpen = false, bHasClosed = false;
			bool bIgnore = false;
			for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
			{
				CNodesLayer *pLayer = pGroup->layers[ nFloor * N_MAX_LAYERS_PER_FLOOR ];
				if ( !pLayer )
					continue;
				CArray2D<char> &flags = *tempFlags.GetArray( nFloor, 0 );
				if ( flags[j][i] & N_FLAG_IGNORED )
				{
					bIgnore = true;
					break;
				}
			}
			if ( bIgnore )
				continue;
			int nMaxH[ N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR ];
			for ( int nF = 0; nF < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++nF )
				nMaxH[ nF ] = 0;
			for ( CFastRenderer::SResult *p = render.resGrid[ j ][ i ]; p; p = p->pNext )
			{
				//выясняем, "считается" ли это пересечение
				bool bHasIntersect = false;
				if ( p->pSrc->pSrc->nTSFlags & NWorld::TS_STATE_OPEN )
				{
					bHasOpen = true;
					if ( bHasClosed )
						bHasIntersect = true;
				}
				else if ( p->pSrc->pSrc->nTSFlags & NWorld::TS_STATE_CLOSED )
				{
					bHasClosed = true;
					if ( bHasOpen )
						bHasIntersect = true;
				}
				else 
					bHasIntersect = true;
				if ( !bHasIntersect )
					continue;

				// это пересечение, выясняем, не слишком ли близко к следующему
				CFastRenderer::SResult *pNext = p->pNext;
				while ( pNext && pNext->fEnter < p->fExit )
					pNext = pNext->pNext;

				if ( pNext && !IsDoor( pNext ) )
				{
					float fDiff = pNext->fEnter - p->fExit;
					if ( fDiff < F_MIN_LAYER_WIDTH )
						continue;
				}
						
				// выбираем индекс этажа
				int nFloorIndex = GetNFloor( p );		
				if ( nFloorIndex < 0 )
				{
					bBadFloor = true;
					nFloorIndex = 0;
				}
				if ( nFloorIndex > 7 )
				{
					bBadFloor = true;
					nFloorIndex = 7;
				}
				// проверка на отрицательные высоты
				if ( p->fExit < fBadHeight )
				{
					fBadHeight = p->fExit;
					if ( B_3x3_RENDER )
					{
						ptBadHeightCP.x = pGroup->ptOrigin.x + FP_GRID_STEP / 3 * ( i + region.x1 * 3 - 1 );
						ptBadHeightCP.y = pGroup->ptOrigin.y + FP_GRID_STEP / 3 * ( j + region.y1 * 3 - 1 );
					}
					else
					{
						ptBadHeightCP.x = pGroup->ptOrigin.x + FP_GRID_STEP * ( i + region.x1 );
						ptBadHeightCP.y = pGroup->ptOrigin.y + FP_GRID_STEP * ( j + region.y1 );
					}
				}
				// заполнение массива высот в данной точке
				char &nLayerCur = nIntersectsOnFloor[ nFloorIndex ][j][i];
				char &nLayerMax = nLayersMustHave[ nFloorIndex ];
				int &fMaxH = nMaxH[ nFloorIndex * N_MAX_LAYERS_PER_FLOOR + nLayerCur ];
				
				fMaxH = GetIHeight( p->fExit );
				++nLayerCur;
				// при необходимости, создание нового слоя
				if ( nLayerCur > nLayerMax )
				{
					nLayerMax = nLayerCur;
					if ( nLayerMax > N_MAX_LAYERS_PER_FLOOR )
						continue;
					if ( pGroup->layers[ nFloorIndex * N_MAX_LAYERS_PER_FLOOR + nLayerMax - 1 ] )
						continue;
					int nNewLayer = pNet->CreateLayer( pGroup->nXSize, pGroup->nYSize, pGroup->ptOrigin, pGroup->ptXDir, 
						p->pSrc->pSrc->nFloor, pGroup );
					if ( nNewLayer == -1 ) // layer cannot be created - too much layers?
						break;
					CNodesLayer *pNewLayer = pNet->GetLayer( nNewLayer );
					pGroup->layers[ nFloorIndex * N_MAX_LAYERS_PER_FLOOR + nLayerMax - 1 ] = pNewLayer;
				}
			}
			int nF, nFUp;
			// "дописываем" те высоты, для которых не нашлось пересечения
			for ( nFUp = 1; nFUp < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++nFUp )
			{
				if ( nMaxH[nFUp] < nMaxH[nFUp-1] )
				{
					nMaxH[nFUp] = nMaxH[nFUp-1];
					int nNowFloor = nFUp / N_MAX_LAYERS_PER_FLOOR;
					if ( nIntersectsOnFloor[ nNowFloor ][j][i] == 0 )
						nIntersectsOnFloor[ nNowFloor ][j][i] = 1;
				}
			}
			// копируем массив высот в данной точке в таблицу высот
			for ( nF = 0; nF < N_MAX_FLOORS ; ++nF )
			{
				for ( int nL = 0; nL < N_MAX_LAYERS_PER_FLOOR; ++nL )
				{
					CArray2D<unsigned short> &tempH = *tempHeights.GetArray( nF, nL );
					int nIndex = nF * N_MAX_LAYERS_PER_FLOOR + nL;
					tempH[j][i] = nMaxH[ nIndex ];			 
					if ( nIndex == 0 || nMaxH[ nIndex ] != nMaxH[ nIndex - 1 ] )
					{
						CArray2D<char> &flags = *tempFlags.GetArray( nF, nL );
						flags[j][i] |= N_FLAG_NATIVE;
					}
				}
			}
		}
	}
	if ( fBadHeight < 0 )
	{
		char buf[128];
		sprintf( buf, "[[ ERROR ]] DETECTED HEIGHT = %f < 0 at x = %f, y = %f\n", fBadHeight, ptBadHeightCP.x, ptBadHeightCP.y );
		csSystem << CC_RED << buf;
		OutputDebugString( buf ); 
	}
	if ( bBadFloor )
		OutputDebugString("[[ ERROR ]] DETECTED FLOOR < 0\n");
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::MarkLadderUps()
{
	for ( int i = 0; i < ladders.size(); ++i )
	{
		bool bMarked = false;
		SLadderInfo &li = ladders[i];
		CArray2D<unsigned short> &tempHLow = *tempHeights.GetArray( li.nNativeFloor - pGroup->nFirstFloor, 0 );
		float fUpperH;
		if ( B_3x3_RENDER )
			fUpperH = li.fHeight + GetFHeight( tempHLow[ li.y * 3 + 1 ][ li.x * 3 + 1 ] );
		else
			fUpperH = li.fHeight + GetFHeight( tempHLow[ li.y ][ li.x ] );
		for ( int nF = li.nNativeFloor - pGroup->nFirstFloor; nF < N_MAX_FLOORS; ++nF )
		{
			for ( int nL = 0; nL < N_MAX_LAYERS_PER_FLOOR; ++nL )
			{
				CNodesLayer *pLayer = pGroup->layers[ nF * N_MAX_LAYERS_PER_FLOOR + nL ];
				if ( !pLayer )
					continue;
				CArray2D<unsigned short> &tempH = *tempHeights.GetArray( nF, nL );
				bool bHas = false;
				if ( B_3x3_RENDER )
				{
					for ( int dY = 0; dY < 3; ++dY )
					{
						for ( int dX = 0; dX < 3; ++dX )
						{
							float fH = GetFHeight( tempH[ li.upY * 3 + dY ][ li.upX * 3 + dX ] );
							float fDiff = fabs( fH - fUpperH );
							if ( fDiff < F_LADDER_STEP )
								bHas = true;
						}
					}
				}
				else
				{
					float fH = GetFHeight( tempH[ li.upY ][ li.upX ] );
					float fDiff = fabs( fH - fUpperH );
					bHas = fDiff < F_LADDER_STEP;
				}
				if ( bHas ) 
				{
					CArray2D<char> &flags = *tempFlags.GetArray( nF, nL );
					if ( B_3x3_RENDER )
					{
						for ( int dY = 0; dY < 3; ++dY )
							for ( int dX = 0; dX < 3; ++dX )
								flags[ li.upY * 3 + dY ][ li.upX * 3 + dX ] |= N_FLAG_LADDER;
					}
					else
						flags[ li.upY ][ li.upX ] |= N_FLAG_LADDER;
					bMarked = true;
				}
			}
		}
		//ASSERT( bMarked );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcer::Calc()
{
	// trace all grids with +1 on bounds size
	const CVec2 &ptXDir = pGroup->ptXDir;
	const CVec2 &ptOrigin = pGroup->ptOrigin;
	vector<char> mustCalc;
	mustCalc.resize( N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR );
	float fAngle = atan2( ptXDir.y, ptXDir.x );
	if ( B_3x3_RENDER )
	{
		CTRect<int> regionBy3( region );
		regionBy3.x1 *= 3;
		regionBy3.y1 *= 3;
		regionBy3.x2 *= 3;
		regionBy3.y2 *= 3;
		regionBy3.x1 -= 1;
		regionBy3.y1 -= 1;
		regionBy3.x2 += 1;
		regionBy3.y2 += 1;
		render.InitParallel( ptOrigin, fAngle, FP_GRID_STEP / 3, regionBy3 );
	}
	else
	{
		render.InitParallel( ptOrigin, fAngle, FP_GRID_STEP, region );
	}
	pMap->TraceGrid( &render, NWorld::TS_PASS_BLOCKER, 
		IAIMap::STH_SORT_INTERVALS, CFloorsSet(), IAIMap::STH_SPLIT_TERR_HG, true );
	sphereHeights.region = tempArrays.region;
	tempFlags.region.SetRect( 0, 0, render.resGrid.GetXSize(), render.resGrid.GetYSize() );
	tempHeights.region.SetRect( 0, 0, render.resGrid.GetXSize(), render.resGrid.GetYSize() );
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		CNodesLayer *pLayer = pGroup->layers[ nFloor * N_MAX_LAYERS_PER_FLOOR ];
		if ( !pLayer )
			continue;
		CArray2D<STile> &temp = *tempArrays.GetArray( nFloor, 0 );
		CArray2D<char> &flags = *tempFlags.GetArray( nFloor, 0 );
		flags.FillZero();
		if ( !pGroup->ignoreRects.empty() )
			MarkIgnored( nFloor, 0, temp );
	}
	CreateAdditionalLayers();
	MarkLadderUps();
	float fMinHeight = 1e10f, fMaxHeight = -1e10f;
	CVec2 ptMin( 1e10f, 1e10f ), ptMax( -1e10f, -1e10f );
	// for every layer 
	bool bMustCalcAll = true;
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
		{
			mustCalc[nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer] = false;
			CNodesLayer *pLayer = pGroup->layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
			if ( !pLayer )
				continue;
			if ( pLayer->nFloor != nFloor + pGroup->nFirstFloor )
			{
				ASSERT(0);
				OutputDebugString("Something wrong with new passability system.\n");
			}
			sphereHeights.GetArray( nFloor, nLayer )->FillZero();
			CArray2D<char> &flags = *tempFlags.GetArray( nFloor, nLayer );

			// mark all 4 neighbors of native knots as must calcs
			MarkNeighbors( &flags, CTPoint<int>(  0,  0 ), bMustCalcAll );
			for ( int y = 0; y < flags.GetYSize(); ++y )
			{
				for ( int x = 0; x < flags.GetXSize(); ++x )
				{
					if ( flags[y][x] & N_FLAG_IGNORED )
						flags[y][x] = 0;
				}
			}
			bMustCalcAll = false; // calc only one total layer
			int nShift = B_3x3_RENDER ? 3 : 1;
			MarkNeighbors( &flags, CTPoint<int>(  nShift,  0 ) );
			MarkNeighbors( &flags, CTPoint<int>(  0,  nShift ) );
			MarkNeighbors( &flags, CTPoint<int>( -nShift,  0 ) );
			MarkNeighbors( &flags, CTPoint<int>(  0, -nShift ) );
			CArray2D<unsigned short> &tempH = *tempHeights.GetArray( nFloor, nLayer );
			MarkInactiveEnlargements( &flags, CTPoint<int>(  nShift,  0 ), tempH, render );
			MarkInactiveEnlargements( &flags, CTPoint<int>(  -nShift,  0 ), tempH, render );
			MarkInactiveEnlargements( &flags, CTPoint<int>(  0,  nShift ), tempH, render );
			MarkInactiveEnlargements( &flags, CTPoint<int>(  0, -nShift ), tempH, render );

			FinishMarking( render, &flags);

			CutBorders( nFloor, nLayer );
			CArray2D<STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
			int nSpotsCount = CalcColliderSize( nFloor, nLayer, &fMinHeight, &fMaxHeight, &ptMin, &ptMax );
			if ( nSpotsCount == 0 )
				mustCalc[nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer] = false;
			else
				mustCalc[nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer] = true;
		}
	}
	if ( ptMin.x > 1e8 ) // no points found
		return;
	pCollider = new NAI::CCollider;
	NAI::CCollider &collider = *pCollider;
	SBound test;
	test.BoxInit( CVec3( ptMin.x, ptMin.y, fMinHeight ), CVec3( ptMax.x, ptMax.y, fMaxHeight + F_MAX_SPECIAL_POINT_HEIGHT ) );
	test.Extend( F_TEST_SPHERE_RADIUS + 1 );
	pMap->PrepareCollider( &collider, test, F_TEST_SPHERE_RADIUS * 2, NWorld::TS_PASS_BLOCKER, true );
	int nPrevFloor = -1, nPrevLayer;
	CArray2D<SDoorColliderAnalyzer> checkSphOk;
	CArray2D<STile> &tempToKnowSize = *tempArrays.GetArray( 0, 0 );
	checkSphOk.SetSizes( tempToKnowSize.GetXSize(), tempToKnowSize.GetYSize() );
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
		{
			if ( !mustCalc[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ] )
				continue;
			CArray2D<STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
			CArray2D<char> &flags = *tempFlags.GetArray( nFloor, nLayer );
			CArray2D<char> &sphereHeight = *sphereHeights.GetArray( nFloor, nLayer );
			CArray2D<unsigned short> &tempH = *tempHeights.GetArray( nFloor, nLayer );
			CArray2D<char> *pSphereHeightPrev = 0;
			CArray2D<unsigned short> *pTempPrevH = 0;
			CArray2D<STile> *pTempPrev = 0;

			if ( nPrevFloor != -1 )
			{
				pSphereHeightPrev = sphereHeights.GetArray( nPrevFloor, nPrevLayer );
				pTempPrevH = tempHeights.GetArray( nPrevFloor, nPrevLayer );
				pTempPrev = tempArrays.GetArray( nPrevFloor, nPrevLayer );
			}
			int nRealLayer = pGroup->layers[ nFloor*N_MAX_LAYERS_PER_FLOOR + nLayer ]->nLayer;
			for ( int y = 0; y < temp.GetYSize(); ++y )
			{
				for ( int x = 0; x < temp.GetXSize(); ++x )
				{
					int nX, nY;
					if ( B_3x3_RENDER )
					{ 
						nX = x * 3 + 1;
						nY = y * 3 + 1;
					}
					else
					{ 
						nX = x;
						nY = y;
					}
					bool bLastLayer = (nLayer == N_MAX_LAYERS_PER_FLOOR - 1);
					if ( !bLastLayer )
					{
						CArray2D<unsigned short> &tempHNext = *tempHeights.GetArray( nFloor, nLayer + 1 );
						if ( tempHNext[ nY ][ nX ] == tempH[ nY ][ nX ] )
							bLastLayer = true;
					}
					//if ( ( nPrevFloor < 0 ) || 
					//		tempH[ nY ][ nX ] != (*pTempPrevH)[ nY ][ nX ] )
						sphereHeight[y][x] = CalcPoseInPoint( x, y, pCollider, tempH, &temp, flags, nRealLayer, 0, bLastLayer );
					/*else
					{
						sphereHeight[y][x] = (*pSphereHeightPrev)[y][x];
						
						if ( HasFlag( nX, nY, flags, N_FLAG_MUST_CALC ) )
						{
							// Find first same layer, and copy theese points
							for ( int nSameFloor = 0; nSameFloor < N_MAX_FLOORS; ++nSameFloor )
							{
								for ( int nSameLayer = 0; nSameLayer < N_MAX_LAYERS_PER_FLOOR; ++nSameLayer )
								{
									if ( !mustCalc[ nSameFloor * N_MAX_LAYERS_PER_FLOOR + nSameLayer ] )
										continue;
									CArray2D<unsigned short> &tempSameH = *tempHeights.GetArray( nSameFloor, nSameLayer );
									if ( tempH[ nY ][ nX ] != tempSameH[ nY ][ nX ] )
										continue;
									CArray2D<STile> &tempSame = *tempArrays.GetArray( nSameFloor, nSameLayer );
									temp[y][x] = tempSame[y][x];
									temp[y][x].nMoveLay = 0;
									break;
								}
							}
						}
					}*/
				}
			}
			if ( B_3x3_RENDER )
			{
				MarkCandidatesToDisplace( &sphereHeight, temp );
				CalcDisplacements( &collider, tempH, pTempPrevH, &temp, pTempPrev, 
					flags, sphereHeight, pSphereHeightPrev, nRealLayer );
			}
			for ( int x = 0; x < temp.GetXSize(); ++x )
			{
				for ( int y = 0; y < temp.GetYSize(); ++y )
				{
					if ( ( !pTempPrevH ) || 
						temp[y][x].nHeight != (*pTempPrev)[y][x].nHeight )
					{
						STile &tSrc = temp[y][x];
						float fSrcHeight = GetFHeight( tSrc.nHeight );
						CVec2 displSrc( F_DISPLACEMENT_X[ tSrc.nDisplacement ], F_DISPLACEMENT_Y[ tSrc.nDisplacement ] );
						CVec2 cp = pGroup->GetCPNoHeight( x + region.minx, y + region.miny );
						CVec3 ptCenter( cp + displSrc, fSrcHeight + F_CHECK_HEIGHT_MOVE + 0.05f ); 
						checkSphOk[y][x].Clear();
						pCollider->DoesIntersect( ptCenter, F_TEST_SPHERE_RADIUS, &checkSphOk[y][x] );
					}
					// else checkSphOk is already set
				}
			}
			TestLayMoves( &collider, &temp, pTempPrev, flags, checkSphOk, nRealLayer );
			nPrevFloor = nFloor;
			nPrevLayer = nLayer;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
