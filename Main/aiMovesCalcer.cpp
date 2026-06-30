#include "StdAfx.h"
#include "aiMovesCalcer.h"
#include "Grid.h"
#include "aiPMConst.h"
#include "aiCollider.h"
#include "aiMap.h"
#include "wTSFlags.h"
#include "wInterface.h"
#include "aiDoorCollider.h"
namespace NAI
{
//
const char N_FLAG_NATIVE    = 1;
const char N_FLAG_MUST_CALC = 2;
const char N_FLAG_IGNORED   = 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMovesCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CMovesCalcer::CMovesCalcer( CNodesLayer *_pLayer, IAIMap *_pMap, const CTRect<int> &_region,
	STempArrayGroup<STile> &_tempArrays, int _nGroupLayer, int _nGroupFloor, CCollider *_pCollider )
: pLayer(_pLayer), pMap(_pMap), region(_region), tempArrays(_tempArrays), pNet(_pLayer->pGroup->pNet), 
	nGroupLayer(_nGroupLayer), nGroupFloor( _nGroupFloor ), pCollider(_pCollider)
{
	ASSERT( IsValid( pMap ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMovesCalcer::TestMove( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, 
	const CTRect<int> &region, float fHeightDiffLimit,
	int nSphereHeight, float fHeight, char nFwdFlag, char nBackFlag,
	CArray2D<char> *pTested )
{
	const CVec2 &ptXDir = pLayer->pGroup->ptXDir;
	CArray2D<STile> &temp = *pRes;
	CArray2D<char> &tested = *pTested;
	int nStartX = 1, nStartY = 1, nFinishX = temp.GetXSize() - 1, nFinishY = temp.GetYSize() - 1;
	nStartX -= shift.x > 0;
	nStartY -= shift.y > 0;
	nFinishX += shift.x < 0;
	nFinishY += shift.y < 0;
	bool bDiagonal = shift.x && shift.y;
	CVec2 ptMove = shift.x * ptXDir + shift.y * CVec2( -ptXDir.y, ptXDir.x );
	for ( int y = nStartY; y < nFinishY; ++y )
	{
		for ( int x = nStartX; x < nFinishX; ++x )
		{
			if ( tested[y][x] )
				continue;
			STile &tile = temp[y][x];
			if ( ( (flags[y][x] | flags[y+shift.y][x+shift.x]) & N_FLAG_NATIVE ) == 0 )
				continue;
			if ( tile.nPassable == CP_INACTIVE && bDiagonal )
				continue;
			STile &tileDest = temp[y+shift.y][x+shift.x];
			if ( tileDest.nPassable == CP_INACTIVE && bDiagonal )
				continue;
			float fSrcHeight = GetFHeight( tile.nHeight );
			float fDstHeight = GetFHeight( tileDest.nHeight );
			if ( fabs( fSrcHeight - fDstHeight ) >= fHeightDiffLimit )
				continue;
			CVec2 displSrc( F_DISPLACEMENT_X[ tile.nDisplacement ], F_DISPLACEMENT_Y[ tile.nDisplacement ] );
			CVec2 displDst( F_DISPLACEMENT_X[ tileDest.nDisplacement ], F_DISPLACEMENT_Y[ tileDest.nDisplacement ] );
			int nH1 = sphereHeight[y][x];
			int nH2 = sphereHeight[y + shift.y][x + shift.x];
			if ( ( nH1 & 7 ) >= nSphereHeight && ( nH2 & 7 ) >= nSphereHeight )
			{
				CVec2 cp = pLayer->GetCPNoHeight( x + region.minx, y + region.miny );
				float fRadius = F_TEST_SPHERE_RADIUS;
				SSphere sph( CVec3(cp.x, cp.y, fSrcHeight + fHeight), fRadius );
				CVec3 vel( ptMove + displDst - displSrc, fDstHeight - fSrcHeight );
				SDoorColliderAnalyzer analyzer;
				pCollider->CollideBool( sph, vel, &analyzer );
				if ( analyzer.IsCollided() )
					tested[y][x] = 1;
				else
				{
					tested[y][x] = 0;
					switch ( nSphereHeight )
					{
						case 2:
							tile.nMoveCrouch |= nFwdFlag;
							tileDest.nMoveCrouch |= nBackFlag;
							break;
						case 3:
							tile.nMoveStand |= nFwdFlag;
							tileDest.nMoveStand |= nBackFlag;
							break;
						default:
							ASSERT( 0 );
							break;
					}
					if ( analyzer.pSrc )
					{
						// door collision
						STile &tileF = GetFlipperTile( x, y, analyzer, tile, &tile.nFlipper );
						STile &tileB = GetFlipperTile( x + shift.x, y + shift.y, analyzer, tileDest, &tileDest.nFlipper );
						switch ( nSphereHeight )
						{
							case 2:
								tileF.nMoveCrouch &= ~nFwdFlag;
								tileB.nMoveCrouch &= ~nBackFlag;
								break;
							case 3:
								tileF.nMoveStand &= ~nFwdFlag;
								tileB.nMoveStand &= ~nBackFlag;
								break;
						}
					}
				}			
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMovesCalcer::STile &CMovesCalcer::GetFlipperTile(
	int x, int y, const SDoorColliderAnalyzer &analyzer, const STile &t, unsigned char *pNFlipper )
{
	if ( !IsInArray( pLayer->tiles, x + region.minx, y + region.miny ) )
	{
		*pNFlipper = 0;
		return fake;
	}
	SPathPlace p( x + region.minx, y + region.miny, pLayer->nLayer );
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
void CMovesCalcer::TestMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, 
	const CTRect<int> &region, float fHeightDiffLimit,
	char nFwdFlag, char nBackFlag )
{
	bool bDiagonal = shift.x && shift.y;
	CArray2D<char> tested;
	tested.SetSizes( pRes->GetXSize(), pRes->GetYSize() );
	tested.FillZero();
	CArray2D<STile> &temp = *pRes;
	for ( int y = 0; y < temp.GetYSize(); ++y )
	{
		for ( int x = 0; x < temp.GetXSize(); ++x )
		{
			STile &t = temp[y][x];
			if ( bDiagonal && (t.nPassable & CP_INACTIVE) )
			{
				t.nMoveLay &= ~nFwdFlag;
				t.nMoveLay &= ~nBackFlag;
			}
			if ( !(( t.nPassable & CP_LAY ) || ( t.nPassable & CP_INACTIVE )) )
				t.nMoveLay = 0;
			if ( t.nMoveLay & nFwdFlag )
			{
				if ( IsInArray( temp, x + shift.x, y + shift.y ) )
				{
					STile &t2 = temp[y + shift.y][x + shift.x];
					if ( !(( t2.nPassable & CP_LAY ) || ( t2.nPassable & CP_INACTIVE )) )
					{
						t.nMoveLay &= ~nFwdFlag;
						t2.nMoveLay = 0;
					}
					if ( ( t2.nPassable & CP_LAY ) == 0 && bDiagonal )
					{
						t.nMoveLay &= ~nFwdFlag;
						t2.nMoveLay &= ~nBackFlag;
					}
					if ( !(( t.nPassable & CP_LAY ) || ( t2.nPassable & CP_LAY )) )
					{
						t.nMoveLay &= ~nFwdFlag;
						t2.nMoveLay &= ~nBackFlag;
					}
				}
				else  // not in array
          t.nMoveLay &= ~nFwdFlag;
			}
			if ( !( t.nMoveLay & nFwdFlag ) )
				tested[y][x] = 1;
		}
	}
	TestMove( pCollider, pRes, shift, region, fHeightDiffLimit, 2,
		F_CHECK_HEIGHT_MOVE + F_TEST_SPHERE_STEP_MOVE, nFwdFlag, nBackFlag, &tested );
	TestMove( pCollider, pRes, shift, region, fHeightDiffLimit, 3,
		F_CHECK_HEIGHT_MOVE + F_TEST_SPHERE_STEP_MOVE*2, nFwdFlag, nBackFlag, &tested );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMovesCalcer::TestHCMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, 
	const CTRect<int> &region, float fHeightDiffLimit,
	char nFwdFlag, char nBackFlag )
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
			if ( ( (flags[y][x] | flags[y+shift.y][x+shift.x]) & N_FLAG_NATIVE ) == 0 )
				continue;
			STile &tileSrc = temp[y][x];
			STile &tileDst = temp[y+shift.y][x+shift.x];
			if ( tileSrc.nDisplacement != tileDst.nDisplacement )
				continue;
			float fSrcHeight = GetFHeight( tileSrc.nHeight );
			float fDstHeight = GetFHeight( tileDst.nHeight );
			float fDiff = fabs( fSrcHeight - fDstHeight );
			if ( fDiff < fHeightDiffLimit )
				continue;
			if ( fDiff < F_MIN_CLIMB_HEIGHT )
				continue;
			CVec2 cp;
			float fStartHeight, fFinishHeight;
			if ( fSrcHeight > fDstHeight )
			{
				if ( (tileSrc.nPassable & CP_INACTIVE ) == 0 || (tileDst.nPassable & CP_CROUCH) == 0 )
					continue;
				cp = pLayer->GetCPNoHeight( x + shift.x + region.minx, y + shift.y + region.miny );
				fStartHeight = fSrcHeight + F_CHECK_HEIGHT + F_TEST_SPHERE_STEP;
				fFinishHeight = fDstHeight + F_CHECK_HEIGHT;
			}
			else
			{
				if ( (tileDst.nPassable & CP_INACTIVE ) == 0 || (tileSrc.nPassable & CP_STAND) == 0 )
					continue;
				cp = pLayer->GetCPNoHeight( x + region.minx, y + region.miny );
				fStartHeight = fDstHeight + F_CHECK_HEIGHT + F_TEST_SPHERE_STEP;
				fFinishHeight = fSrcHeight + F_CHECK_HEIGHT;
			}
			SSphere sph( CVec3(cp.x, cp.y, fStartHeight), F_HC_TEST_SPHERE_RADIUS );
			CVec3 vel( 0, 0, fFinishHeight - fStartHeight );
			SDoorColliderAnalyzer analyzer;
			pCollider->CollideBool( sph, vel, &analyzer );
			if ( !analyzer.IsCollided() )
			{
				tileSrc.nMoveHC |= nFwdFlag;
				tileDst.nMoveHC |= nBackFlag;
				if ( analyzer.pSrc )
				{
					// door collision
					STile &tileF = GetFlipperTile( x, y, analyzer, tileSrc, &tileSrc.nFlipper );
					STile &tileB = GetFlipperTile( x + shift.x, y + shift.y, analyzer, tileDst, &tileDst.nFlipper );
					tileF.nMoveHC &= ~nFwdFlag;
					tileB.nMoveHC &= ~nBackFlag;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! try to map src points to dest region
// src/dest squares are exclusive
void CMovesCalcer::CreateGrid2GridCandidates( IAIMap *pMap, 
	const CTRect<int> &src, CNodesLayer *pSrc, CArray2D<STile> &srcTiles, CTPoint<int> &srcOrigin,
	const CTRect<int> &dest, CNodesLayer *pDst, CArray2D<STile> &dstTiles, CTPoint<int> &dstOrigin,
	vector<SSphere> *pSpheres, vector<CVec3> *pMoves, vector<SPossibleTransition> *pPossibilities,
	bool bDoRefresh ) 
{
	bool bSameGroup = pSrc->pGroup == pDst->pGroup;
	for ( int nY = src.miny; nY < src.maxy; ++nY )
	{
		for ( int nX = src.minx; nX < src.maxx; ++nX )
		{
			const STile &srcTile = srcTiles[nY - srcOrigin.y][nX - srcOrigin.x];
			// it is passable
			if ( ( srcTile.nPassable & CP_LAY ) == 0 )
				continue;
			// check if it fits to region
			CVec2 test = pSrc->GetCPNoHeight( nX, nY );
			CVec2 res;
			if ( pDst->pGroup == pSrc->pGroup )
			{
				res.x = nX;
				res.y = nY;		
			}
			else
				pDst->pGroup->GetExactPoint( &res, test );
			int nDestX = ((int)(res.x + 10000) ) - 10000;
			if ( nDestX < dest.minx - 1 || nDestX >= dest.maxx )
				continue;
			int nDestY = ((int)(res.y + 10000) ) - 10000;
			if ( nDestY < dest.miny - 1 || nDestY >= dest.maxy )
				continue;
			int nMinX = Max( nDestX, dest.minx );
			int nMinY = Max( nDestY, dest.miny );
			// cut improbable cases right away
			//if ( abs( ((int)srcTile.nHeight) - ((int)dstTiles[nMinY-dstOrigin.y][nMinX-dstOrigin.x].nHeight) ) > GetIHeight( 1.5f ) )
			//	continue;
			// examine every possible link
			int nMaxX = Min( nDestX + 1, dest.maxx - 1 );
			int nMaxY = Min( nDestY + 1, dest.maxy - 1 );
			float fSrcHeight = GetFHeight( srcTile.nHeight );
			CVec3 ptSrc( test.x, test.y, fSrcHeight );
			for ( int nLocalY = nMinY; nLocalY <= nMaxY; ++nLocalY )
			{
				for ( int nLocalX = nMinX; nLocalX <= nMaxX; ++nLocalX )
				{
					// make sure it has level 1 data
					if ( bDoRefresh )
					{
						SPathPlace p;
						p.SetOnLayer( pDst->nLayer, nLocalX, nLocalY );
						pDst->pGroup->RefreshSpot( p, pMap, 1 );
					}
					//
					const STile &destTile = dstTiles[nLocalY-dstOrigin.y][nLocalX-dstOrigin.x];
					if ( !( destTile.nPassable & CP_LAY ) )
						continue;
					if ( bSameGroup )
					{
						if ( srcTile.nDisplacement == 0 && destTile.nDisplacement == 0 )
							continue;
						if ( nLocalX != nX || nLocalY != nY )
							continue;
						if ( srcTile.nDisplacement == destTile.nDisplacement )
							continue;
					}
					// check if height does not differ too much (fall into 30 degrees test
					CVec2 destCP = pDst->GetCPNoHeight( nLocalX, nLocalY );
					float fDist = fabs2( destCP - test );
					float fDestHeight = GetFHeight( destTile.nHeight );
					float fDH = fabs( fSrcHeight - fDestHeight );
					if ( fDH * fDH * 3 > fDist + 0.01f )
						continue;
					SPossibleTransition trans;
					Zero( trans );
					trans.from.SetOnLayer( pSrc->nLayer, nX, nY );
					trans.to.SetOnLayer( pDst->nLayer, nLocalX, nLocalY );
					// depending on distance either instant transition or direct one
					if ( sqr(fDH) + sqr( fDist ) < sqr( 0.01f ) && bSameGroup )
						continue;
					else
					{
						// add to possible transitions list
						// cannot use GetCP() because results from temp[][] are not transfered to tiles yet
						CVec3 ptDest = CVec3( destCP, fDestHeight );
						pSpheres->push_back( SSphere( ptSrc + CVec3(0,0,F_CHECK_HEIGHT_MOVE), F_TEST_SPHERE_RADIUS ) );
						pMoves->push_back( ptDest - ptSrc );
						pPossibilities->push_back( trans );
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! place current layer knots onto from position
void CMovesCalcer::NormalizeTransitions( vector<SPossibleTransition> *pRes )
{
	int nLayer = pLayer->nLayer;
	for ( int i = 0; i < pRes->size(); ++i )
	{
		SPossibleTransition &t = (*pRes)[i];
		ASSERT( t.from.GetLayer() != t.to.GetLayer() );
		if ( t.to.GetLayer() == nLayer )
			swap( t.from, t.to );
		ASSERT( t.from.GetLayer() == nLayer );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMovesCalcer::MarkNativePoints( CArray2D<STile> *pRes )
{
	CArray2D<STile> &temp = *pRes;
	int nLayer = pLayer->nLayer;
	int nMinX = max( 1, - region.minx );
	int nMinY = max( 1, - region.miny );
	int nMaxX = min( temp.GetXSize() - 1, pLayer->tiles.GetXSize() - region.minx );
	int nMaxY = min( temp.GetYSize() - 1, pLayer->tiles.GetYSize() - region.miny );
	for ( int x = nMinX; x < nMaxX; ++x )
	{
		for ( int y = nMinY; y < nMaxY; ++y )
		{
			STile &tile = temp[y][x];
			if ( pNet->HasLowerSame( SPathPlace( x + region.minx, y + region.miny, nLayer ) ) )
				continue;
			flags[y][x] |= N_FLAG_NATIVE;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMovesCalcer::MarkSame( CArray2D<STile> *pRes )
{
	CArray2D<STile> &temp = *pRes;
	CLayersGroup *pGroup = pLayer->pGroup;
	int nMinX = max( 0, - region.minx );
	int nMinY = max( 0, - region.miny );
	int nMaxX = min( temp.GetXSize(), pLayer->tiles.GetXSize() - region.minx );
	int nMaxY = min( temp.GetYSize(), pLayer->tiles.GetYSize() - region.miny );
	for ( int nFl = 0; nFl < N_MAX_FLOORS; ++nFl )	
	{
		for ( int i = 0; i < N_MAX_LAYERS_PER_FLOOR; ++i )
		{
			CNodesLayer *pLayerDst = pGroup->layers[ i + nFl * N_MAX_LAYERS_PER_FLOOR ];
			if (!pLayerDst)
				continue;
			if ( pLayerDst == pLayer )
				return;
			CArray2D<STile> &temp2 = *tempArrays.GetArray( nFl, i );
			for ( int x = nMinX; x < nMaxX; ++x )
			{
				for ( int y = nMinY; y < nMaxY; ++y )
				{
					STile &t1 = temp[ y ][ x ];
					STile &t2 = temp2[ y ][ x ];
					int nDiff = t1.nHeight - t2.nHeight;
					if ( nDiff > GetIHeight( 0.05f ) || nDiff < -GetIHeight( 0.05f ) )
						continue;
					if ( t1.nDisplacement == t2.nDisplacement ) // match found
					{
						t1.nFlags |= TF_HAS_SAME;
						t2.nFlags |= TF_HAS_SAME;
						t2.nHeight = t1.nHeight;
						if ( t1.nMoveStand || t2.nMoveStand )
						{
							t1.nFlags |= TF_STAND_PASSABLE;
							t2.nFlags |= TF_STAND_PASSABLE;
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMovesCalcer::Calc()
{
	int nLayer = pLayer->nLayer;
	sphereHeight.SetSizes( region.Width(), region.Height() );
	flags.SetSizes( region.Width(), region.Height() );
	sphereHeight.FillZero();
	flags.FillZero();
	// remove all existing intergrid links (direct and height changing ones)
	CTRect<int> exactRegion( region.minx + 1, region.miny + 1, region.maxx - 1, region.maxy - 1 );
	pLayer->RemoveAllTransitions( exactRegion );
	// calc sphere heights from passability flags and points borders too
	CVec3 ptMin(1e38f,1e38f,1e38f), ptMax(-1e38f,-1e38f,-1e38f);
	CArray2D<STile> &temp = *tempArrays.GetArray( nGroupFloor, nGroupLayer );
	for ( int y = 0; y < temp.GetYSize(); ++y )
	{
		for ( int x = 0; x < temp.GetXSize(); ++x )
		{
			STile &tile = temp[y][x];
			char cRes = 0;
			if ( tile.nPassable & CP_LAY )
				cRes = 1;
			if ( tile.nPassable & CP_INACTIVE )
				cRes = 2+8;
			if ( tile.nPassable & CP_CROUCH )
				cRes = 2;
			if ( tile.nPassable & CP_STAND )
				cRes = 3;
			if ( cRes )
			{
				CVec2 cp = pLayer->GetCPNoHeight( x + region.minx, y + region.miny );
				CVec3 ptTest( cp.x, cp.y, GetFHeight( tile.nHeight ) );
				ptMin.Minimize( ptTest );
				ptMax.Maximize( ptTest );
			}
			sphereHeight[y][x] = cRes;
		}
	}
	// check if there was at least one passable point in the region
	if ( ptMin.x == 1e38f )
		return;
	// mark native points with appropriate flag
	MarkNativePoints( &temp );
	// initialize collider
	// extend region to match all possible traces
	if ( !IsValid( pCollider ) )
	{
		SBound test;
		ptMax.z += 2 + 3;
		ptMin.z -= 3;
		test.BoxInit( ptMin, ptMax );
		test.Extend( F_TEST_SPHERE_RADIUS + 1 );
		pCollider = new NAI::CCollider;
		pMap->PrepareCollider( pCollider, test, F_TEST_SPHERE_RADIUS * 2, NWorld::TS_PASS_BLOCKER, true );
	}
	NAI::CCollider &collider = *pCollider;
	// for each direction run a sphere on heights up to minimum height of pair
	// fill passability info depending on sphere run tests
	TestMoves( &collider, &temp, CTPoint<int>( 1,0), region, 
		F_NORMAL_HEIGHT_DIFF_LIMIT, 1<<CD_R,  1<<CD_L );
	TestMoves( &collider, &temp, CTPoint<int>( 1,1), region, 
		F_DIAGONAL_HEIGHT_DIFF_LIMIT, 1<<CD_RU, 1<<CD_LD );
	TestMoves( &collider, &temp, CTPoint<int>( 0,1), region, 
		F_NORMAL_HEIGHT_DIFF_LIMIT, 1<<CD_U,  1<<CD_D );
	TestMoves( &collider, &temp, CTPoint<int>(-1,1), region, 
		F_DIAGONAL_HEIGHT_DIFF_LIMIT, 1<<CD_LU, (char)(1<<CD_RD) );
	// check HC transitions
	TestHCMoves( &collider, &temp, CTPoint<int>(1,0), region, 
		F_NORMAL_HEIGHT_DIFF_LIMIT, 1<<CD_R,  1<<CD_L );
	TestHCMoves( &collider, &temp, CTPoint<int>(0,1), region, 
		F_NORMAL_HEIGHT_DIFF_LIMIT, 1<<CD_U,  1<<CD_D );
	// calc intergrid transitions
	vector<SSphere> spheres;
	vector<CVec3> moves;
	vector<SPossibleTransition> possibilities;
	// map current layer onto other
	for ( int nTestLayer = 0; nTestLayer < nLayer; ++nTestLayer )
	{
		CNodesLayer *pOuter = pNet->GetLayer( nTestLayer );
    //if ( pOuter->pGroup == pLayer->pGroup )
		//	continue; // crap - must think about different displacements
		CreateGrid2GridCandidates( pMap,
			CTRect<int>( region.minx + 1, region.miny + 1, region.maxx - 1, region.maxy - 1 ),
			pLayer, temp, CTPoint<int>(region.minx, region.miny),
			CTRect<int>( 0, 0, pOuter->tiles.GetXSize(), pOuter->tiles.GetYSize() ),
			pOuter, pOuter->tiles, CTPoint<int>(0,0),
			&spheres, &moves, &possibilities, true );
	}
	// map other layers onto current
	for ( int nTestLayer = nLayer + 1; nTestLayer < pNet->GetNumLayers(); ++nTestLayer )
	{
		CNodesLayer *pOuter = pNet->GetLayer( nTestLayer );
		CTRect<int> cover;
    if ( pOuter->pGroup == pLayer->pGroup )
			cover = region;
		else
		{
			// determine rect on pOuter that covers current region
			SPoint test;
			CVec2 corner, ptMin(1e38f, 1e38f), ptMax(-1e38f,-1e38f);
			pOuter->pGroup->GetExactPoint( &corner, pLayer->GetCPNoHeight( region.minx, region.miny ) );
			ptMin.Minimize( corner ); ptMax.Maximize( corner );
			pOuter->pGroup->GetExactPoint( &corner, pLayer->GetCPNoHeight( region.minx, region.maxy ) );
			ptMin.Minimize( corner ); ptMax.Maximize( corner );
			pOuter->pGroup->GetExactPoint( &corner, pLayer->GetCPNoHeight( region.maxx, region.maxy ) );
			ptMin.Minimize( corner ); ptMax.Maximize( corner );
			pOuter->pGroup->GetExactPoint( &corner, pLayer->GetCPNoHeight( region.maxx, region.miny ) );
			ptMin.Minimize( corner ); ptMax.Maximize( corner );
			cover.minx = Float2Int( ptMin.x );
			cover.miny = Float2Int( ptMin.y );
			cover.maxx = Float2Int( ptMax.x ) + 1;
			cover.maxy = Float2Int( ptMax.y ) + 1;
		}
		// clip
		cover.maxx = Min( cover.maxx, pOuter->tiles.GetXSize() );
		cover.maxy = Min( cover.maxy, pOuter->tiles.GetYSize() );
		cover.minx = Max( cover.minx, 0 );
		cover.miny = Max( cover.miny, 0 );
		// for each potential contact recalc 1st level data
		for ( int nY = cover.miny; nY < cover.maxy; ++nY )
		{
			for ( int nX = cover.minx; nX < cover.maxx; ++nX )
			{
				SPathPlace p;
				p.SetOnLayer( pOuter->nLayer, nX, nY );
				if ( pOuter->pGroup != pLayer->pGroup )
					pOuter->pGroup->RefreshSpot( p, pMap, 1 );
			}
		}
		CreateGrid2GridCandidates( pMap,
			CTRect<int>( cover.minx, cover.miny, cover.maxx, cover.maxy ),
			pOuter, pOuter->tiles, CTPoint<int>(0,0),
			CTRect<int>( region.minx + 1, region.miny + 1, region.maxx - 1, region.maxy - 1 ),
			pLayer, temp, CTPoint<int>(region.minx, region.miny),
			&spheres, &moves, &possibilities, false );
	}
	// analyse collision results and if found links add them
	NormalizeTransitions( &possibilities );
	// search for normal transitions
	vector<char> result;
	collider.CollideCheck( spheres, moves, &result );
	// store results
	vector<int> linkHeight;
	linkHeight.resize( spheres.size() );
	for ( int i = 0; i < linkHeight.size(); ++i )
		linkHeight[i] = !result[i];
	// test higher transforms
	for ( int k = 1; k < 3; ++k )
	{
		for ( int i = 0; i < spheres.size(); ++i )
			spheres[i].ptCenter.z += F_TEST_SPHERE_STEP_MOVE;
		collider.CollideCheck( spheres, moves, &result );
		for ( int i = 0; i < linkHeight.size(); ++i )
		{
			if ( linkHeight[i] == k )
				linkHeight[i] += !result[i];
		}
	}
	// store links info into hash map
	for ( int i = 0; i < linkHeight.size(); ++i )
	{
		if ( linkHeight[i] == 0 )
			continue;
		CNodesLayer::SLink lnk( possibilities[i].to, linkHeight[i] );
		SPathPlace from( possibilities[i].from ); 
		transitions[ from ].links.push_back( lnk );
		ASSERT( from.GetX() >= region.minx && from.GetY() >= region.miny && from.GetX() < region.maxx && from.GetY() < region.maxy );
	}
	// add found transitions to network
	pNet->AddDirectTransitions( transitions );
	MarkSame( &temp );
	// LAY re-checking - убираем мувы, которые могут потребовать ползать, чтобы просто развернуться
	for ( int y = 0; y < temp.GetYSize(); ++y )
	{
		for ( int x = 0; x < temp.GetXSize(); ++x )
		{
			if ( ! ( temp[ y ][ x ].nPassable & CP_LAY ) )
				continue;
			if ( ( temp[ y ][ x ].nPassable & CP_CROUCH ) || ( temp[ y ][ x ].nPassable & CP_STAND ) )
				continue;
			// here we can lay only. can we turn everywhere?
			bool 
				b1 = temp[ y ][ x ].nPassable & CP_LAY1,
				b2 = temp[ y ][ x ].nPassable & CP_LAY2,
				b3 = temp[ y ][ x ].nPassable & CP_LAY3,
				b4 = temp[ y ][ x ].nPassable & CP_LAY4;
			if ( !(b1 && b2 && b3 && b4) )
				temp[y][x].nPassable |= CP_CROUCH;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLadderCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CLadderCalcer::CLadderCalcer( CNodesLayer *_pLayer, IAIMap *_pMap, CNodesLayer::SLadder *_pLadder, vector<char> *_pRes )
: pMap(_pMap), pLayer(_pLayer), pLadder(_pLadder), pNet( _pLayer->pGroup->pNet ), pRes( _pRes )
{
	ASSERT( IsValid( pMap ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLadderCalcer::Calc()
{
	// initialize collider
	CPtr<NAI::CCollider> pColliderPass = new NAI::CCollider;
	NAI::CCollider &colliderPass = *pColliderPass;
	SBound test;
	CVec3 ptShift;
	switch ( pLadder->eDir )
	{
		case LD_RIGHT: ptShift = CVec3( - F_LADDER_SHIFT, 0, 0 ); break;
		case LD_FRONT: ptShift = CVec3( 0, - F_LADDER_SHIFT, 0 ); break;
		case LD_LEFT: ptShift = CVec3( F_LADDER_SHIFT, 0, 0 ); break;
		case LD_BACK: ptShift = CVec3( 0, F_LADDER_SHIFT, 0 ); break;
		default: ASSERT(0);
	}
	CVec3 ptMin( pNet->GetCP( pLadder->placeOnBottom ) + ptShift ), ptMax( ptMin ), ptCurrent( ptMin );
	int nHeight = pLadder->GetHeight();
	ptMax.z += F_LADDER_STEP * nHeight;
	test.BoxInit( ptMin, ptMax );
	test.Extend( F_TEST_SPHERE_RADIUS + 1 );
	pMap->PrepareCollider( &colliderPass, test, F_TEST_SPHERE_RADIUS * 2, NWorld::TS_PASS_BLOCKER );
	for ( int i = 0; i < nHeight; ++i )
	{
		bool bIntersect = colliderPass.DoesIntersect( ptCurrent, F_LADDER_TEST_RADIUS );
		(*pRes)[ i ] = bIntersect;
		ptCurrent.z += F_LADDER_STEP;
	}
	vector<char> collideRes;
	collideRes.resize( nHeight - 1 );
	CVec3 move( 0, 0, F_LADDER_STEP );
	for ( int i = 0; i < nHeight - 1; ++i )
	{
		collideRes[i] = !(*pRes)[ i ];
	}
	for ( int i = 0; i < nHeight - 3; ++i )
	{
		(*pRes)[i] = !( collideRes[i] && collideRes[ i + 1 ] && collideRes[ i + 2 ] );
	}
	(*pRes)[ nHeight - 1 ] = (*pRes)[ nHeight - 2 ] = (*pRes)[ nHeight - 3 ] = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
