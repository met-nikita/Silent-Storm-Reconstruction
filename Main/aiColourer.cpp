#include "StdAfx.h"
#include "aiColourer.h"
#include "Grid.h"
#include "aiWayConstraints.h"

namespace NAI
{
char cTranslations[][2] = { {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
enum EApproximateCosts
{
	AC_LADDER_STEP = 2,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsLocallyPassable( CNodesLayerProxy* pMap, int nX, int nY, EDirection direction )
{
	bool flag;
	switch ( direction ) 
	{
		case RIGHT:
			flag = ( nX & 7 ) != 7; break;
		case UPRIGHT:
			flag = ( ( nY & 7 ) != 7 ) && ( ( nX & 7 ) != 7); break;
		case UP:			
			flag = ( nY & 7 ) != 7; break;
		case UPLEFT:		
			flag = ( ( nY & 7 ) != 7 ) && ( nX & 7 ); break;
		case LEFT:
			flag = ( nX & 7 )!=0; break;
		case DOWNLEFT:
			flag = ( nX & 7 ) && ( nY & 7 ); break;
		case DOWN:
			flag = ( nY & 7 ) != 0; break;
		case DOWNRIGHT:
			flag = ( ( nX & 7 ) != 7 ) && ( nY & 7 ); break;
		default:
			ASSERT( 0 );
			return true;
	}
	if ( !flag ) return false;
	return pMap->IsPassable( nX, nY, direction ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SNetNode
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void SNetNode::AddNeighbour( WORD wNode, int wDistance, int nLayer ) 
{ 
	for ( list<SNeighbour>::iterator i = neighbours.begin(); i != neighbours.end(); ++i )
		if ( ( i->wNodeNumber == wNode ) && ( i->nLayer == nLayer ) ) 
		{
			if ( wDistance < i->wDistance )
				i->wDistance = wDistance;
			return; 
		}
	SNeighbour pN( wNode, wDistance, nLayer ); 
	neighbours.push_front( pN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SNet
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void SNet::Clear() 
{ 
	for ( vector<SNetNode>::iterator i = nodes.begin(); i != nodes.end(); ++i ) 
		(i->neighbours).clear(); 			
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SZone
////////////////////////////////////////////////////////////////////////////////////////////////////
SZone::SZone( const CPathNetwork* pNet, const SPathPlace& point )
{
	nLayer = point.GetLayer();
	CMapColourer *colourer = pNet->GetColourer( nLayer );
	if ( point.IsIntegral() )
		wColor = colourer->GetPointColor( (unsigned char)point.GetX(), (unsigned char)point.GetY() );
	else // ladder
	{
		int nLadder = point.GetX();
		CNodesLayer::SLadder &ladder = pNet->GetLayer( nLayer )->ladders[ nLadder ];
		bool bUpperHalf = ladder.pointOnUpperHalf[ point.GetLadderStep() ];
		wColor = EC_LADDER_COLOR + nLadder * 2 + ( bUpperHalf? 1 : 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTPoint<unsigned char> SZone::GetCenter( const CPathNetwork *pNet ) const
{
	CMapColourer *colourer = pNet->GetColourer( nLayer );
	unsigned char cX, cY;
	if ( wColor < EC_LADDER_COLOR )
		colourer->GetColorCenter( wColor, &cX, &cY );
	else
	{
		int nLadder = ( wColor - EC_LADDER_COLOR ) / 2;
		CNodesLayer::SLadder &ladder = pNet->GetLayer( nLayer )->ladders[ nLadder ];
		SZone z( pNet, ladder.placeOnBottom );
		return z.GetCenter( pNet );
	}
	return CTPoint<unsigned char>( cX, cY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 SZone::GetCP( const CPathNetwork *pNet ) const
{
	CMapColourer *colourer = pNet->GetColourer( nLayer );
	unsigned char cX, cY;
	if ( wColor < EC_LADDER_COLOR )
		colourer->GetColorCenter( wColor, &cX, &cY );
	else
	{
		int nLadder = ( wColor - EC_LADDER_COLOR ) / 2;
		CNodesLayer::SLadder &ladder = pNet->GetLayer( nLayer )->ladders[ nLadder ];
		SZone z( pNet, ladder.placeOnBottom );
		return z.GetCP( pNet );
	}
	SPathPlace p( cX, cY, nLayer );
	return pNet->GetCP( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMapColourer
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CMapColourer::Get8x8SquareOfColor( WORD wColor, unsigned char *pX, unsigned char *pY )
{
	*pX = localColorInfos[ wColor ].wAverageX >> 3;
	*pY = localColorInfos[ wColor ].wAverageY >> 3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int CMapColourer::CountDistance( int X1, int Y1, int X2, int Y2 ) const
{
	int a = abs( X2 - X1 ),
	    b = abs( Y2 - Y1 );
	return a + b + max( a, b );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline WORD CMapColourer::GetNextEmptyColor( WORD wCurrent ) 
{ 
	do wCurrent++; 
	while ( (wCurrent < nLocalColors) && (localColorCounts[wCurrent] > 0) ); 
	return wCurrent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void KillBadHCMoves( CArray2D<CNodesLayer::STile> *pTiles, const CTRect<int> &region );
// function above can be found in aiGrid.cpp
void CMapColourer::ConstructColouring( CPathNetwork* pNet )
{
	int nMaxX, nMaxY;
	pMap->GetSizes( &nMaxX, &nMaxY ); 
	nLocalColors = 0;
	localColorRedirects.clear();
	localColorCounts.clear();

	if ( nMaxX != pointColors.GetXSize() || nMaxY != pointColors.GetYSize() )
	{
		pointColors.SetSizes( nMaxX, nMaxY );
		pointDistancesAnyMove.SetSizes( nMaxX, nMaxY );
		pointDistancesStandOnly.SetSizes( nMaxX, nMaxY );
	}

	CNodesLayer *pLayer = pMap->pLayer;
	IAIMap *pAIMap = pMap->pMap;
	for ( int nX = 0; nX < nMaxX; ++nX )
	{
		for ( int nY = 0; nY < nMaxY; ++nY )
		{
			pointDistancesAnyMove[ CSquareMapCosts::SPosition( nX, nY ) ].cost = 65535;
			pointDistancesStandOnly[ CSquareMapCosts::SPosition( nX, nY ) ].cost = 65535;
		}
	}
	CNodesLayer *pL = pMap->pLayer;
	CTRect<int> region( 0, 0, pL->tiles.GetXSize(), pL->tiles.GetYSize() );
	KillBadHCMoves( &pL->tiles, region );

	InvestigateSquareMap( 0, 0, nMaxX, nMaxY );

	pMap->SetPathfinderMode( PM_STAND_ONLY );
	CreateZonesMap( &zonesStandOnly, 0 );
	pMap->SetPathfinderMode( PM_ANY_MOVE );
	CreateZonesMap( &zonesAnyMove, &pLayer->ladders );

	zonesToRecalc.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::InvestigateSquareMap( const vector< CTRect<unsigned char> > &areas )
{
	int nMaxX, nMaxY;
	pMap->GetSizes( &nMaxX, &nMaxY );
	CArray2D<bool> mustInvestigate( (nMaxX + 7) >> 3, (nMaxY + 7) >> 3 );
	int nX, nY;
	for ( nX = 0; nX < ( (nMaxX + 7) >> 3 ); ++nX )
		for ( nY = 0; nY < ( (nMaxY + 7) >> 3 ); ++nY )
			mustInvestigate[ nY ][ nX ] = false;
	for ( vector< CTRect<unsigned char> >::const_iterator i = areas.begin(); i != areas.end(); ++i )
	{
		for ( nX = (i->minx >> 3); nX < ((i->maxx + 7) >> 3); ++nX )
			for ( nY = (i->miny >> 3); nY < ((i->maxy + 7) >> 3); ++nY )
				mustInvestigate[ nY ][ nX ] = true;
	}
	for ( nX = 0; nX < ( (nMaxX + 7) >> 3 ); ++nX )
		for ( nY = 0; nY < ( (nMaxY + 7) >> 3 ); ++nY )
		{
			int
				nMaxInvX = Min( nMaxX, ( nX<<3 ) + 8 ),
				nMaxInvY = Min( nMaxY, ( nY<<3 ) + 8 );
			if ( mustInvestigate[ nY ][ nX ] )
				InvestigateSquareMap( nX<<3, nY<<3, nMaxInvX , nMaxInvY );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::InvestigateSquareMap( int nMinX, int nMinY, int nMaxX, int nMaxY )
{
	// First time passing the map, colouring it and setting colour redirects
	WORD wCurrColor = 65535;
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );

	pMap->SetPathfinderMode( PM_STAND_ONLY );

	for ( int i = nMinX; i < nMaxX; ++i )
		for ( int j = nMinY; j < nMaxY; ++j )
		{
			// LOCAL COLOURING
			bool bRPass, bDPass;
			bRPass = IsLocallyPassable( pMap, i, j, LEFT );
			bDPass = IsLocallyPassable( pMap, i, j, DOWN );
			// new color
			if ( !bRPass && !bDPass ) 
			{
				if ( nLocalColors == 0 ) 
					wCurrColor = 0;
				else 
				{
					if ( wCurrColor == 65535 ) 
						wCurrColor = localColorCounts[0]? GetNextEmptyColor(0) : 0;
					else	wCurrColor = GetNextEmptyColor( wCurrColor );
				}
				if ( wCurrColor >= nLocalColors )
				{				
					localColorRedirects.push_back( nLocalColors );
					localColorCounts.push_back( 0 );
					nLocalColors++;
				}
#ifdef _DEBUG
				int nDebugX, nDebugY;
				for ( nDebugX = 0; nDebugX < nSizeX; ++nDebugX )
				{
					for ( nDebugY = 0; nDebugY < nSizeY; ++nDebugY )
					{
						if ( ( nDebugX >= nMinX ) && ( nDebugX < nMaxX ) 
							&& ( nDebugY >= nMinY ) && ( nDebugY < nMaxY ) )
							continue;
						ASSERT( pointColors[ nDebugY ][ nDebugX ] != wCurrColor );
					}
				}
#endif
				localColorRedirects[ wCurrColor ] = wCurrColor;
				pointColors[ j ][ i ]=wCurrColor;				
			}
			// one old color
			if ( bRPass && !bDPass )
				pointColors[ j ][ i ]=pointColors[ j ][ i - 1 ];
			if ( !bRPass&& bDPass )
				pointColors[ j ][ i ]=pointColors[ j - 1 ][ i ];
			// two old colors
			if ( bRPass && bDPass )
			{
					WORD	color1 = pointColors[ j - 1 ][ i ],
								color2 = pointColors[ j ][ i - 1 ];
					pointColors[ j ][ i ] = color1;
					if ( color1 != color2 )
					{
						WORD color = min( localColorRedirects[ color1 ], localColorRedirects[ color2 ] );
						localColorRedirects[ color1 ] = 
							localColorRedirects[ color2 ] = 
								color; // now theese 3 colors are the same
					}
			}
		}

	// simplifying color redirects tree 
	for ( wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor )
		while ( localColorRedirects[ wCurrColor ]!= localColorRedirects[localColorRedirects[ wCurrColor ]] )
				localColorRedirects[ wCurrColor ] = localColorRedirects[ localColorRedirects[ wCurrColor ] ];

	localColorInfos.resize( nLocalColors );
	for ( WORD i=0; i<nLocalColors; ++i )
		localColorInfos[ i ].wAverageX = localColorInfos[ i ].wAverageY = localColorCounts[ i ] = 0;
				
	// second time passing the map, applying redirects, 
	// counting squares of local colours, and filling in color correspondencies
	for ( unsigned char i = 0; i < nSizeX; ++i )
		for ( unsigned char j = 0; j < nSizeY; ++j )
		{
			// LOCAL COLOR:
			// applying local redirect
			wCurrColor = pointColors[ j ][ i ];
			if ( wCurrColor == 65535 )
				continue;
			wCurrColor = localColorRedirects[ wCurrColor ];
			pointColors[ j ][ i ] = wCurrColor;

			// counting local color area size & average (center) coords
			localColorCounts[ wCurrColor ]++;

			ASSERT( localColorCounts[ wCurrColor ] < 65 );
			localColorInfos[ wCurrColor ].wAverageX += i;
			localColorInfos[ wCurrColor ].wAverageY += j;
		}

	// Now finding out central point of each local-coloured zone
	for ( WORD wCurrColor=0; wCurrColor<nLocalColors ; ++wCurrColor )
	{
		if ( localColorCounts[ wCurrColor ] )
		{
			localColorInfos[ wCurrColor ].wAverageX /= localColorCounts[ wCurrColor ];
			localColorInfos[ wCurrColor ].wAverageY /= localColorCounts[ wCurrColor ];

			// center of a color is not of this color - select any point as a central one
			if ( pointColors
					[ localColorInfos[ wCurrColor ].wAverageY ]
 					[ localColorInfos[ wCurrColor ].wAverageX ] != wCurrColor ) 
			{
				unsigned char cX, cY, cMaxX, cMaxY;
				Get8x8SquareOfColor( wCurrColor, &cX, &cY );
				int nBestDist=256, nBestX, nBestY;
				cMaxX = Min( ( cX << 3 ) + 8, nSizeX );
				cMaxY = Min( ( cY << 3 ) + 8, nSizeY );
				for ( int i = cX << 3; i < cMaxX; ++i )
					for ( int j = cY << 3; j < cMaxY; ++j )
						if ( pointColors[ j ][ i ] == wCurrColor )
						{
							int nDist = CountDistance(
								localColorInfos[ wCurrColor ].wAverageX,
								localColorInfos[ wCurrColor ].wAverageY, i, j );
							if ( nDist < nBestDist )
							{
								nBestDist = nDist; 
								nBestX = i; 
								nBestY = j;
							}
						}
				ASSERT( nBestDist < 256 );
				localColorInfos[ wCurrColor ].wAverageX = nBestX;
				localColorInfos[ wCurrColor ].wAverageY = nBestY;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::CalcDistanceTableForColor( 
	WORD wCurrColor, EPathfinderMode mode, CWaysCounter *counter )
{
	// do not calculate neighbours for non-native and non-passable (i.e. fake) colors
	WORD wCenterX = localColorInfos[ wCurrColor ].wAverageX;
	WORD wCenterY = localColorInfos[ wCurrColor ].wAverageY;
	if ( !pMap->IsValidPoint((unsigned char)wCenterX, (unsigned char)wCenterY ) )
		return;
	if ( mode == PM_STAND_ONLY )
		if ( !pMap->IsPassablePoint((unsigned char)wCenterX, (unsigned char)wCenterY ) )
			return;	
	
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );

	CTRect<unsigned char> rect;
	Get8x8SquareOfColor( wCurrColor, &rect.minx, &rect.miny );
	// make current color distances infinite
	rect.minx <<= 3;
	rect.miny <<= 3;
	rect.maxx = Min( rect.minx + 8, nSizeX );
	rect.maxy = Min( rect.miny + 8, nSizeY );
	SRectColorConstraint<unsigned char> constraints( rect.minx, rect.miny, rect.maxx, rect.maxy, &pointColors, wCurrColor );
	pMap->SetPathfinderMode( mode );
	counter->SetConstraints( &constraints );
	counter->SetPosition( CTPoint<unsigned char>
		((unsigned char)localColorInfos[wCurrColor].wAverageX,
			(unsigned char)localColorInfos[wCurrColor].wAverageY ) );
	counter->Count();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::CreateZonesMap( SNet *pZones, vector<CNodesLayer::SLadder> *pLadders )
{
	SNet &zones = *pZones;
	// перестраиваем карту, если она уже была построена
	zones.Clear();
	if (zones.nodes.size()<nLocalColors)
		zones.nodes.resize(nLocalColors);
	for (WORD wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor)
		zones.nodes[wCurrColor].wColor = wCurrColor;

	// определяем структуры, ищущие путь
	EPathfinderMode mode;
	CSquareMapCosts *pTable;
	if ( pZones == &zonesStandOnly )
	{
		mode = PM_STAND_ONLY;
		pTable = &pointDistancesStandOnly;
	}
	else
	{
		mode = PM_ANY_MOVE;
		pTable = &pointDistancesAnyMove;
	}
	// для каждого цвета, который реально существует, ищем пути к соседям
	SRectColorConstraint<unsigned char> constraints(0, 0, 24, 24);
	CWaysCounter counter( pTable, &constraints, pMap, CTPoint<unsigned char>(0, 0) );
	for (WORD wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor)
	{
		if (localColorCounts[wCurrColor]) // colour valid
			CalcDistanceTableForColor( wCurrColor, mode, &counter );
	}
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );
	FindAdjacentColours( 0, 0, nSizeX, nSizeY, mode, pLadders );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddNeighborToProceedList( const SNeighbour &n, SZone &start, const SZone &best,
	CZonesToDistInfo &distances, WORD bestdist )
{
	WORD newdist = bestdist + n.wDistance;
	ASSERT( newdist >= bestdist );
	SZone newnode( n.wNodeNumber, n.nLayer );
	SDistanceInfo &di = distances[ newnode ];
	if ((di.isProceeded)&&(di.distance <= newdist)) 
		return;
	di.distance = newdist;			
	di.parent = best;
	if (di.isInList)
		return;
	di.next = start;
	di.prev.MakeNull();
	di.isProceeded = true;
	di.isInList = true;
	if ( !start.IsNull() )
		distances[ start ].prev = newnode;
	start = newnode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CalcWay( CPathNetwork *pPathNet, CUpperNetWay *pWay, const SZone &src, 
	const SZone &dst, bool bStandOnly, SZone *pStart, CZonesToDistInfo *pDistances );
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColouredWaysCalcer::CalcBestWays( CPathNetwork *pPathNet, CLayerColorConstraints *pWays, const SZone &src, 
	const vector<SZone> &dst, const vector<CVec3> &dstCP, bool bStandOnly )
{
	int firstNotFound = -1;
	if ( bMustChange || pPathNet != pPrevNet || bStandOnly != bPrevStandOnly	|| !( src == prevSrc ) )
	{
		bMustChange = false;
		pPrevNet = pPathNet;
		bPrevStandOnly = bStandOnly;
		prevSrc = src;
		distances.clear();
		// Записываем начало пути в таблицу расстояний
		start = src;
		distances[src].distance = 0;
		distances[src].parent.MakeNull();
		distances[src].prev.MakeNull();
		distances[src].next.MakeNull();
		distances[src].isProceeded = distances[src].isInList = true;
	}
	// Начинаем поиск
	for ( int i = 0; i < dst.size(); ++i )
	{
		CUpperNetWay way;
		// проверим, не просчитали ли мы уже эту зону
		CZonesToDistInfo::iterator calced = distances.find( dst[i] );
		if ( calced != distances.end() )
		{
			SZone best = dst[i];
			while ( !distances[best].parent.IsNull() )
			{				
				way.push_back(best);
				best = distances[best].parent;
			}
			way.push_back(src);
			pWays->AddWay( way );
			continue;
		}
		bool bThisWayCalced = CalcWay( pPathNet, &way, src, dst[ i ], bStandOnly, &start, &distances );
		if ( bThisWayCalced )
		{
			//OutputDebugString("[ GLOBAL WAY SEARCH ] Found way \n");
			pWays->AddWay( way );
		}
		else
		{
			firstNotFound = i;
			break;
			// если функция вернула false, значит, в таблицу попали все расстояния до всех зон, что были достижимы.
			// поэтому расчитывать ее дальше не имеет смысла
		}
	}
	if ( firstNotFound == -1 )
		return;
	//OutputDebugString("[ GLOBAL WAY SEARCH ] Way not found! \n");
	// хотя бы один расчет пути вернул false.
	// следовательно, надо искать "ближайшее приближение". 
	for ( int i = firstNotFound; i < dst.size(); ++i )
	{
		CUpperNetWay way;
		CZonesToDistInfo::iterator calced = distances.find( dst[i] );
		SZone best;
		if ( calced == distances.end() ) // зона действительно недостижима
		{
			float bestDist = 1e8;
			float fKoeff = ( FP_GRID_STEP / 32 );
			for ( CZonesToDistInfo::iterator z = distances.begin(); z != distances.end(); ++z )
			{
				const SZone &that = z->first;
				float curLinDist = fabs2( dstCP[i] - that.GetCP( pPathNet ) );
				int moveCost = z->second.distance;
				if ( curLinDist > bestDist * bestDist )
					continue;
				float curDist = sqrt( curLinDist ) + fKoeff * moveCost;
				if ( curDist < bestDist )
				{
					bestDist = curDist;
					best = that;
				}
			}
		}
		else 
			best = dst[i];
		// best found, add way
		while ( !distances[best].parent.IsNull() )
		{				
			way.push_back(best);
			best = distances[best].parent;
		}
		way.push_back(src);
		pWays->AddWay( way );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CalcWay( CPathNetwork *pPathNet, CUpperNetWay *pWay, const SZone &src, 
	const SZone &dst, bool bStandOnly, SZone *pStart, CZonesToDistInfo *pDistances )
{
	CUpperNetWay &way = *pWay;
#ifdef _DEBUG
	// some basic defence
	SNet *pNet, *pNetDst;
	if (bStandOnly)
	{
		pNet = &pPathNet->GetColourer(src.nLayer)->zonesStandOnly;
		pNetDst = &pPathNet->GetColourer(dst.nLayer)->zonesStandOnly;
	}
	else
	{
		pNet = &pPathNet->GetColourer(src.nLayer)->zonesAnyMove;
		pNetDst = &pPathNet->GetColourer(dst.nLayer)->zonesAnyMove;
	}
	ASSERT(pNet);
	ASSERT(pNetDst);
	if ( src.wColor < EC_LADDER_COLOR && dst.wColor < EC_LADDER_COLOR )
		ASSERT((src.wColor < pNet->nodes.size())&&(dst.wColor < pNetDst->nodes.size()));
#endif

	CZonesToDistInfo &distances = *pDistances;
	
	SZone &start = *pStart;
	while ( !start.IsNull() ) // list is not empty
	{
		// find the "best" node
		SZone best = start, current = start;
		WORD bestdist = distances[best].distance;
		while (1)
		{
			current = distances[current].next;
			if ( current.IsNull() ) 
				break;
			if (distances[current].distance < bestdist)
			{
				best = current;
				bestdist = distances[best].distance;
			}
		}
		//char buf[128];
		//sprintf( buf, "Zone l = %d  clr = %d\n", best.nLayer, best.wColor );
		//OutputDebugString( buf );
		// if this is the needed node, construct path...
		if ( best == dst )
		{
			way.clear();
			while ( !distances[best].parent.IsNull() )
			{				
				way.push_back(best);
				best = distances[best].parent;
			}
			way.push_back(src);
			return true;
		}
		// pop the best node from the list
		SZone prev = distances[best].prev, next = distances[best].next;
		if (best == start) 
		{
			start = next;
			if ( start.IsNull() ) start = prev;  // if no next
		}
		if ( !prev.IsNull() ) 
			distances[prev].next = next;
		if ( !next.IsNull() ) 
			distances[next].prev = prev;	
		distances[best].isInList = false;
		// for each successor...
		SNet *pNet;
		if ( bStandOnly ) 
			pNet = &pPathNet->GetColourer(best.nLayer)->zonesStandOnly;
		else 
			pNet = &pPathNet->GetColourer(best.nLayer)->zonesAnyMove;
		if ( best.wColor >= EC_LADDER_COLOR ) // it's a ladder
		{
			int nLadder = (best.wColor - EC_LADDER_COLOR) / 2;
			bool bUpper = best.wColor & 1;
			if ( bUpper ) // upper half of a ladder 
			{
				// add transition to up
				SNeighbour &upper = pNet->ladderUpperPoints[ nLadder ];
				WORD wDistance = upper.wDistance * 2;
				AddNeighborToProceedList( upper, start, best, distances, bestdist );
				// add transition to lower half of the ladder
				if ( pNet->ladderConsistent[ nLadder ] )
				{
					SNeighbour n( best.wColor - 1, wDistance, best.nLayer );
					AddNeighborToProceedList( n, start, best, distances, bestdist );
				}
			}
			else // lower half of a ladder
			{
				// add transition to down
				SNeighbour &lower = pNet->ladderLowerPoints[ nLadder ];
				WORD wDistance = lower.wDistance * 2;
				AddNeighborToProceedList( lower, start, best, distances, bestdist );
				// add transition to upper half of the ladder
				if ( pNet->ladderConsistent[ nLadder ] )
				{
					SNeighbour n( best.wColor + 1, wDistance, best.nLayer );
					AddNeighborToProceedList( n, start, best, distances, bestdist );
				}
			}
		}
		else // generic node
		{
			list<SNeighbour> &ns = pNet->nodes[best.wColor].neighbours;
			for (list<SNeighbour>::iterator i = ns.begin(); i != ns.end(); ++i) 
			{
				AddNeighborToProceedList( *i, start, best, distances, bestdist );
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::ClearColor( EPathfinderMode mode, CPathNetwork* pNet, WORD wColor, int nLayer ) 
{ 
	list<SNeighbour> *pMyNeighbours; 
	if ( mode == PM_STAND_ONLY )
		pMyNeighbours = &zonesStandOnly.nodes[wColor].neighbours;
	else
		pMyNeighbours = &zonesAnyMove.nodes[wColor].neighbours;
	// удаляем у соседей не с данного лэйера ссылки на данный цвет
	for ( list<SNeighbour>::iterator i = pMyNeighbours->begin(); i != pMyNeighbours->end(); ++i )
	{
		if ( i->nLayer == nLayer )
			continue;
		CMapColourer *pOther = pNet->GetColourer( i->nLayer );
		list<SNeighbour> *pNeighbours;
		if ( i->wNodeNumber >= EC_LADDER_COLOR )
			continue;
		if ( mode == PM_STAND_ONLY )
			pNeighbours = &pOther->zonesStandOnly.nodes[ i->wNodeNumber ].neighbours;
		else
			pNeighbours = &pOther->zonesAnyMove.nodes[ i->wNodeNumber ].neighbours;
		for ( list<SNeighbour>::iterator j = pNeighbours->begin(); j != pNeighbours->end(); ++j )
		{
			if ( ( j->wNodeNumber == wColor ) && ( j->nLayer == nLayer ) )
			{
				pNeighbours->erase( j );
				break;
			}
		}
	}
	// случай, когда этот цвет полностью очищается
	if ( localColorCounts[ wColor ] == 0 )
	{
		pMyNeighbours->clear();
		return;
	}
	list<SNeighbour> temp;
	// случай, когда очищаются только ссылки на соседей, которые очищаются полностью
	for ( list<SNeighbour>::iterator i = pMyNeighbours->begin(); i != pMyNeighbours->end(); ++i )
	{
		if ( i->nLayer != nLayer )
			continue; // delete
		WORD wAnotherColor = i->wNodeNumber;
		if ( wAnotherColor >= EC_LADDER_COLOR )
			continue;
		if ( localColorCounts[ wAnotherColor ] == 0 ) 
			continue; // delete
		temp.push_back( *i );
	}
	*pMyNeighbours = temp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapColourer::IsColorInArea( WORD wColor, const vector< CTRect<unsigned char> > &areas )
{
	unsigned char cAX = (unsigned char)localColorInfos[wColor].wAverageX;
	unsigned char cAY = (unsigned char)localColorInfos[wColor].wAverageY;
	for ( vector< CTRect<unsigned char> >::const_iterator i = areas.begin(); i != areas.end(); ++i )
	{
		unsigned char 
			cMaxX = ( (i->maxx + 7) >> 3) << 3, 
			cMaxY = ( (i->maxy + 7) >> 3) << 3,
			cMinX = (i->minx >> 3) << 3, 
			cMinY = (i->miny >> 3) << 3;
		ASSERT( cMaxX >= i->maxx );
		ASSERT( cMaxY >= i->maxy );
		ASSERT( cMinX <= i->minx );
		ASSERT( cMinY <= i->miny );
		if (( cAX < cMaxX )&&( cAX >= cMinX )&&( cAY < cMaxY )&&( cAY >= cMinY ))
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPointTouchingArea( unsigned char cAX, unsigned char cAY, const vector< CTRect<unsigned char> > &areas )
{
	for ( vector< CTRect<unsigned char> >::const_iterator i = areas.begin(); i != areas.end(); ++i )
	{
		unsigned char 
			cMaxX = ( ((i->maxx + 7) >> 3) + 1 )<<3, 
			cMaxY = ( ((i->maxy + 7) >> 3) + 1 )<<3,
			cMinX = (i->minx >> 3) << 3, 
			cMinY = (i->miny >> 3) << 3;
		if ( cMinX > 0 ) 
			cMinX -= 8;
		if ( cMinY > 0 ) 
			cMinY -= 8;
		ASSERT( cMaxX >= i->maxx );
		ASSERT( cMaxY >= i->maxy );
		ASSERT( cMinX <= i->minx );
		ASSERT( cMinY <= i->miny );
		if (( cAX < cMaxX )&&( cAX >= cMinX )&&( cAY < cMaxY )&&( cAY >= cMinY ))
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapColourer::IsColorTouchingArea( WORD wColor, const vector< CTRect<unsigned char> > &areas )
{
	unsigned char cAX = (unsigned char)localColorInfos[wColor].wAverageX;
	unsigned char cAY = (unsigned char)localColorInfos[wColor].wAverageY;
	return IsPointTouchingArea( cAX, cAY, areas );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::RecalcColouring( CPathNetwork *pNet, int nCurrentLayer )
{
	if ( zonesToRecalc.empty() )
		return;
/*	DebugTrace("Recalc %d zones\n", zonesToRecalc.size());
	for ( int i = 0; i < zonesToRecalc.size(); ++i ) 
	{
		DebugTrace("zone %d-%d %d-%d\n", zonesToRecalc[i].x1, zonesToRecalc[i].x2, zonesToRecalc[i].y1, zonesToRecalc[i].y2 );
	}*/
	pNet->GetWaysCalcer()->ForceUpdate();
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );
	
	// clear colors in this area
	for (WORD wColor = 0; wColor < nLocalColors; ++wColor)
	{
		if (localColorCounts[wColor]) // colour valid
		{
			if ( IsColorInArea( wColor, zonesToRecalc ) )
			{
				localColorCounts[ wColor ] = 0;
				ClearColor( PM_STAND_ONLY, pNet, wColor, nCurrentLayer );
				ClearColor( PM_ANY_MOVE, pNet, wColor, nCurrentLayer );
			}
		}
	}
	for (WORD wColor = 0; wColor < nLocalColors; ++wColor)
	{
		if (localColorCounts[wColor]) // colour valid
		{
			if ( IsColorTouchingArea( wColor, zonesToRecalc ) )
			{
				ClearColor( PM_STAND_ONLY, pNet, wColor, nCurrentLayer );
				ClearColor( PM_ANY_MOVE, pNet, wColor, nCurrentLayer );
			}
		}
	}
	// clearing...
	for ( unsigned char cX = 0; cX < nSizeX; ++cX )
	{
		for ( unsigned char cY = 0; cY < nSizeY; ++cY )
		{
			WORD wColor = pointColors[ cY ][ cX ];
			if ( localColorCounts[ wColor ] == 0 )
			{
				pointColors[ cY ][ cX ] = 65535;
				pointDistancesAnyMove[ CSquareMapCosts::SPosition( cX, cY ) ].cost = 65535;
				pointDistancesStandOnly[ CSquareMapCosts::SPosition( cX, cY ) ].cost = 65535;
			}
		}
	}
	CNodesLayer *pLayer = pMap->pLayer;
	// refreshing layer spots
	/*char buf[128];
	if ( pLayer->nLayer == 9)
	{
		sprintf( buf, "Recalc started on layer %d\n", pLayer->nLayer );
		OutputDebugString( buf ); 
	}*/
	for ( vector< CTRect<unsigned char> >::iterator i = zonesToRecalc.begin(); i != zonesToRecalc.end(); ++i )
	{
		unsigned char 
			cMinX = i->minx, cMaxX = i->maxx,
			cMinY = i->miny, cMaxY = i->maxy;
		/*if ( pLayer->nLayer == 9)
		{
			sprintf( buf, "Recalc zone %d %d %d %d\n", cMinX, cMaxX, cMinY, cMaxY );
			OutputDebugString( buf ); 
		}*/
		IAIMap *pAIMap = pMap->pMap;
		for ( unsigned char cX = cMinX; cX < cMaxX; ++cX )
		{
			for ( unsigned char cY = cMinY; cY < cMaxY; ++cY )
			{
				SPathPlace p( cX, cY, nLayer );
				pLayer->pGroup->RefreshSpot( p, pAIMap, 2 );
			}
		}
	}
	// recolor this area
	CArray2D<bool> mustInvestigate( (nSizeX + 7) >> 3, (nSizeY + 7) >> 3 );
	{
		unsigned char cX, cY;
		for ( cX = 0; cX < ( (nSizeX + 7) >> 3 ); ++cX )
			for ( cY = 0; cY < ( (nSizeY + 7) >> 3 ); ++cY )
				mustInvestigate[ cY ][ cX ] = false;
		for ( vector< CTRect<unsigned char> >::const_iterator i = zonesToRecalc.begin(); i != zonesToRecalc.end(); ++i )
		{
			for ( cX = (i->minx >> 3); cX < ((i->maxx + 7) >> 3); ++cX )
				for ( cY = (i->miny >> 3); cY < ((i->maxy + 7) >> 3); ++cY )
					mustInvestigate[ cY ][ cX ] = true;
		}
		for ( cX = 0; cX < ( (nSizeX + 7) >> 3 ); ++cX )
		{
			for ( cY = 0; cY < ( (nSizeY + 7) >> 3 ); ++cY )
			{
				unsigned char 
					cMaxInvX = Min( nSizeX, ( cX<<3 ) + 8 ),
					cMaxInvY = Min( nSizeY, ( cY<<3 ) + 8 );
				if ( mustInvestigate[ cY ][ cX ] )
					InvestigateSquareMap( cX<<3, cY<<3, cMaxInvX , cMaxInvY );
			}
		}
	}

	// repair nodes network for new colors
	if (zonesStandOnly.nodes.size()<nLocalColors)
	{
		zonesStandOnly.nodes.resize(nLocalColors);
		for (WORD wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor)
			zonesStandOnly.nodes[wCurrColor].wColor = wCurrColor;
	}
	if (zonesAnyMove.nodes.size()<nLocalColors)
	{
		zonesAnyMove.nodes.resize(nLocalColors);
		for (WORD wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor)
			zonesAnyMove.nodes[wCurrColor].wColor = wCurrColor;
	}

	// определяем структуры, ищущие путь
	SRectColorConstraint<unsigned char> constraints(0, 0, 24, 24);
	CWaysCounter counterStandOnly(&pointDistancesStandOnly, &constraints, pMap, CTPoint<unsigned char>(0, 0) );
	CWaysCounter counterAnyMove(&pointDistancesAnyMove, &constraints, pMap, CTPoint<unsigned char>(0, 0) );

	for (WORD wCurrColor = 0; wCurrColor < nLocalColors; ++wCurrColor)
	{
		if (localColorCounts[wCurrColor]) // colour valid
		{
			if ( IsColorInArea( wCurrColor, zonesToRecalc ) )
			{
				CalcDistanceTableForColor( wCurrColor, PM_ANY_MOVE, &counterAnyMove );
				CalcDistanceTableForColor( wCurrColor, PM_STAND_ONLY, &counterStandOnly );
			}
		}
	}

	for ( vector< CTRect<unsigned char> >::const_iterator i = zonesToRecalc.begin(); i != zonesToRecalc.end(); ++i )
	{
		unsigned char cMinBlockX = i->minx >> 3;
		unsigned char cMaxBlockX = ( i->maxx + 7 ) >> 3;
		unsigned char cMinBlockY = i->miny >> 3;
		unsigned char cMaxBlockY = ( i->maxy + 7 ) >> 3;
		if (cMinBlockY > 0 ) 
			cMinBlockY--;
		if (cMinBlockX > 0 ) 
			cMinBlockX--;
		if (cMaxBlockY < ((nSizeY + 7) >> 3) ) 
			cMaxBlockY++;
		if (cMaxBlockX < ((nSizeX + 7) >> 3) ) 
			cMaxBlockX++;
		for ( unsigned char cX = cMinBlockX; cX < cMaxBlockX; ++cX )
			for ( unsigned char cY = cMinBlockY; cY < cMaxBlockY; ++cY )
				mustInvestigate[ cY ][ cX ] = true;
	}
	// для каждой пары соприкасающихся цветов в MustInvestigate добавляем связь
	unsigned char cX, cY;
	for ( cX = 0; cX < ( (nSizeX + 7) >> 3 ); ++cX )
	{
		for ( cY = 0; cY < ( (nSizeY + 7) >> 3 ); ++cY )
		{
			if ( mustInvestigate[ cY ][ cX ] )
			{
				unsigned char 
					cMaxInvX = Min( nSizeX, ( cX<<3 ) + 8 ),
					cMaxInvY = Min( nSizeY, ( cY<<3 ) + 8 );
				FindAdjacentColours( cX<<3, cY<<3, cMaxInvX, cMaxInvY, PM_ANY_MOVE, &pLayer->ladders );
				FindAdjacentColours( cX<<3, cY<<3, cMaxInvX, cMaxInvY, PM_STAND_ONLY, 0 );
			}
		}
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::FindAdjacentColours( unsigned char cMinX, unsigned char cMinY, 
	unsigned char cMaxX, unsigned char cMaxY, 
	EPathfinderMode mode, vector<CNodesLayer::SLadder> *pLadders  )
{
	int nX, nY;
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );
	WORD wColor1, wColor2;
	CSquareMapCosts *pTable;
	SNet *pNet;
	pMap->SetPathfinderMode( mode );
	if ( mode == PM_ANY_MOVE )
	{
		pTable = &pointDistancesAnyMove;
		pNet = &zonesAnyMove;
	}
	else
	{
		pTable = &pointDistancesStandOnly;
		pNet = &zonesStandOnly;
	}
	CSquareMapCosts &table = *pTable;
	for ( nX = cMinX; nX < cMaxX; ++nX )
	{
		for ( nY = cMinY; nY < cMaxY; ++nY )
		{
			wColor1 = pointColors[ nY ][ nX ];
			// LEFT
			if ( nX > 0 )
			{
				wColor2 = pointColors[ nY ][ nX - 1 ];
				if ( wColor1 != wColor2 )
				{
					WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, LEFT );
					if ( wOneMoveDist )
					{
/*#ifdef _DEBUG
						bool bValid1 = pMap->IsValidPoint( nX, nY );
						bool bValid2 = pMap->IsValidPoint( nX - 1, nY );
						if ( bValid2 != bValid1 )
							__debugbreak();
						if ( mode == PM_STAND_ONLY )
						{
							bool bStPass1 = pMap->IsPassablePoint( nX, nY );
							bool bStPass2 = pMap->IsPassablePoint( nX - 1, nY );
							if ( bStPass1 != bStPass2 )
								__debugbreak();
						}
#endif*/
						WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
						WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX - 1, nY ) );
						WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
						pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
						pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
					}
				}
			}
			// DOWN
			if ( nY > 0 )
			{
				wColor2 = pointColors[ nY - 1 ][ nX ];
				if ( wColor1 != wColor2 )
				{
					WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, DOWN );
					if ( wOneMoveDist )
					{
/*#ifdef _DEBUG
						bool bValid1 = pMap->IsValidPoint( nX, nY );
						bool bValid2 = pMap->IsValidPoint( nX, nY - 1 );
						if ( bValid2 != bValid1 )
							__debugbreak();
						if ( mode == PM_STAND_ONLY )
						{
							bool bStPass1 = pMap->IsPassablePoint( nX, nY );
							bool bStPass2 = pMap->IsPassablePoint( nX, nY - 1 );
							if ( bStPass1 != bStPass2 )
								__debugbreak();
						}
#endif*/
						WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
						WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX , nY - 1 ) );
						WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
						pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
						pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
					}
				}
			}
			// DOWNLEFT
			if ( nX > 0 && nY > 0 )
			{
				wColor2 = pointColors[ nY - 1 ][ nX - 1 ];
				if ( wColor1 != wColor2 )
				{
					WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, DOWNLEFT );
					if ( wOneMoveDist )
					{
						WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
						WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX - 1, nY - 1 ) );
						WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
						pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
						pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
					}
				}
			}
			// DOWNRIGHT
			if ( nX < nSizeX - 1 && nY > 0 )
			{
				wColor2 = pointColors[ nY - 1 ][ nX + 1 ];
				if ( wColor1 != wColor2 )
				{
					WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, DOWNRIGHT );
					if ( wOneMoveDist )
					{
							WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
							WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX + 1, nY - 1 ) );
							WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
							pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
							pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
					}
				}
			}
		}
	}
	// RIGHT EDGE
	nX = cMaxX - 1;
	for ( nY = cMinY; nY < cMaxY; ++nY )
	{
		wColor1 = pointColors[ nY ][ nX ];
		// RIGHT
		if ( nX < nSizeX - 1 )
		{
			wColor2 = pointColors[ nY ][ nX + 1 ];
			if ( wColor1 != wColor2 )
			{
				WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, RIGHT );
				if ( wOneMoveDist )
				{
					WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
					WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX + 1, nY ) );
					WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
					pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
					pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
				}
			}
		}
		// UPRIGHT
		if ( nX < nSizeX - 1 && nY < nSizeY - 1 )
		{
			wColor2 = pointColors[ nY + 1 ][ nX + 1 ];
			if ( wColor1 != wColor2 )
			{
				WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, UPRIGHT );
				if ( wOneMoveDist )
				{
					WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
					WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX + 1, nY + 1 ) );
					WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
					pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
					pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
				}
			}
		}
	}
	// UPPER EDGE
	nX = cMaxX - 1;
	for ( nY = cMinY; nY < cMaxY; ++nY )
	{
		wColor1 = pointColors[ nY ][ nX ];
		// UP
		if ( nY < nSizeY - 1 )
		{
			wColor2 = pointColors[ nY + 1 ][ nX ];
			if ( wColor1 != wColor2 )
			{
				WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, UP );
				if ( wOneMoveDist )
				{
					WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
					WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY + 1 ) );
					WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
					pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
					pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
				}
			}
		}
		// UPLEFT
		if ( nX > 0 && nY < nSizeY - 1 )
		{
			wColor2 = pointColors[ nY + 1 ][ nX - 1 ];
			if ( wColor1 != wColor2 )
			{
				WORD wOneMoveDist = pMap->GetOneMoveCost( nX, nY, UPLEFT );
				if ( wOneMoveDist )
				{
					WORD wDistance1 = pTable->GetCost( CSquareMapCosts::SPosition( nX, nY ) );
					WORD wDistance2 = pTable->GetCost( CSquareMapCosts::SPosition( nX - 1, nY + 1 ) );
					WORD wDistance = wDistance1 + wOneMoveDist + wDistance2;
					pNet->nodes[wColor1].AddNeighbour( wColor2, wDistance, nLayer );
					pNet->nodes[wColor2].AddNeighbour( wColor1, wDistance, nLayer );
				}
			}
		}
	}
	// add ladders
	// в этом месте мы добавляем переход с нижней точки на лестницу, если он есть.
	// переход с верхней половины лестницы будет добавлен позже, в момент AttachTransitions
	// т.к. сейчас еще не факт, что тот слой вообще посчитан
	if ( !pLadders )
		return;
	vector<CNodesLayer::SLadder> &ladders = *pLadders;
	if ( pNet->ladderConsistent.size() < ladders.size() )
	{
		pNet->ladderConsistent.resize( ladders.size() );
		pNet->ladderUpperPoints.resize( ladders.size() );
		pNet->ladderLowerPoints.resize( ladders.size() );
	}
	for ( int i = 0; i < ladders.size(); ++i )
	{
//		OutputDebugString("[ LADDER ] Attaching ladder down\n");
		CNodesLayer::SLadder &ladder = ladders[i];
		unsigned char cX = (unsigned char)ladder.placeOnBottom.GetX();
		unsigned char cY = (unsigned char)ladder.placeOnBottom.GetY();
		if ( cX < cMinX || cX >= cMaxX || cY < cMinY || cY >= cMaxY )
			continue;
		pMap->pLayer->RefreshLadder( i, pMap->pMap );
		WORD wColor = GetPointColor( cX, cY );
		WORD wLadderColor = EC_LADDER_COLOR + i * 2;
		WORD wDistance = pTable->GetCost( CSquareMapCosts::SPosition( cX, cY ) ) +
			ladder.GetHeight() / 4 * AC_LADDER_STEP;
		pNet->nodes[ wColor ].AddNeighbour( wLadderColor, wDistance, nLayer );
		pNet->ladderConsistent[ i ] = ladder.bConsistent;
		pNet->ladderLowerPoints[ i ] = SNeighbour( wColor, wDistance, nLayer );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// returns the way length from point to its color center
WORD CMapColourer::GetDistanceToCenter( unsigned char cX, unsigned char cY, EPathfinderMode mode ) 
{
	ASSERT( IsReady() );
	WORD wCurrColor = pointColors[cY][cX];
	CSquareMapCosts *pTable;
	if ( mode == PM_STAND_ONLY )
		pTable = &pointDistancesStandOnly;
	else
		pTable = &pointDistancesAnyMove;
	WORD wRet = (*pTable)[ CTPoint<unsigned char>(cX, cY) ].cost;
	return wRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::AttachColourers(	
	unsigned char cX, unsigned char cY, CMapColourer* pOther, 
	unsigned char cOtherX, unsigned char cOtherY, unsigned char cTransitionCost, EPathfinderMode mode )
{
	if ( !IsReady() )
		return;
	if ( !pOther->IsReady() ) 
		return;
 
	WORD wDistance1 = GetDistanceToCenter( cX, cY, mode );
	if ( wDistance1 == 65535 )
		return;
	WORD wDistance2 = pOther->GetDistanceToCenter( cOtherX, cOtherY, mode );
	if ( wDistance2 == 65535 )
		return;
	WORD wDistance = wDistance1 + wDistance2 + cTransitionCost;
	WORD wCurrColor = pointColors[cY][cX],
		wAnother = pOther->pointColors[cOtherY][cOtherX];
/*#ifdef _DEBUG
	bool bValid1 = pMap->IsValidPoint( cX, cY );
	bool bValid2 = pOther->pMap->IsValidPoint( cOtherX, cOtherY );
	if ( bValid2 != bValid1 ) 
		__debugbreak();
	if ( mode == PM_STAND_ONLY )
	{
		bool bStPass1 = pMap->IsPassablePoint( cX, cY );
		bool bStPass2 = pOther->pMap->IsPassablePoint( cOtherX, cOtherY );
		if ( bStPass1 != bStPass2 )
			__debugbreak();
	}
#endif*/
	if ( mode == PM_ANY_MOVE )
	{
		zonesAnyMove.nodes[wCurrColor].AddNeighbour( wAnother, wDistance, pOther->nLayer );
		pOther->zonesAnyMove.nodes[wAnother].AddNeighbour( wCurrColor, wDistance, nLayer );
	}
	else // mode == PM_STAND_ONLY
	{
		zonesStandOnly.nodes[wCurrColor].AddNeighbour( wAnother, wDistance, pOther->nLayer );
		pOther->zonesStandOnly.nodes[wAnother].AddNeighbour( wCurrColor, wDistance, nLayer );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::AttachSameTransitions( CPathNetwork* pNet,
	const SPathPlace &search, EPathfinderMode mode )
{
	unsigned char cX = (unsigned char)search.GetX();
	unsigned char cY = (unsigned char)search.GetY();
	SPathPlace sames[32];
	int nP = pNet->GetLayer( nLayer )->GetSamePoints( search, sames );
	for ( int i = 0; i < nP; ++i )
	{
		CMapColourer *pAnother = pNet->GetColourer( sames[i].GetLayer() );
		AttachColourers( cX, cY, pAnother, (unsigned char)sames[i].GetX(), (unsigned char)sames[i].GetY(), 0, mode);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::AttachTransitions( CPathNetwork* pNet,
	const SPathPlace &p, const CNodesLayer::STransitionSet &trSet, EPathfinderMode mode )
{
	unsigned char cX = (unsigned char)p.GetX();
	unsigned char cY = (unsigned char)p.GetY();
	for ( list<CNodesLayer::SLink>::const_iterator l = trSet.links.begin(); l != trSet.links.end(); ++l )
	{
		const SPathPlace &dst = l->dst;
		SPathPlace search( p.GetX(), p.GetY(), p.GetLayer() );
		CNodesLayer *pLayer1 = pNet->GetLayers()[ dst.GetLayer() ];
		CNodesLayer::STile &t1 = pLayer1->tiles[dst.GetY()][dst.GetX()];
		CMapColourer *pAnother = pNet->GetColourer( dst.GetLayer() );
		CVec3 searchCP = pNet->GetCP( search ),
					dstCP = pNet->GetCP( dst );
		float fDist = fabs2( searchCP - dstCP );
		float fHDiff = GetFHeight( l->nHeight );
		if ( ( fHDiff > F_MAX_CLIMB_HEIGHT ) || ( fHDiff < - F_MAX_CLIMB_HEIGHT ) )
			continue;
		if ( fDist > FP_GRID_STEP * FP_GRID_STEP ) 
			AttachColourers( cX, cY, pAnother, (unsigned char)dst.GetX(), (unsigned char)dst.GetY(), 3, mode);
		else
			AttachColourers( cX, cY, pAnother, (unsigned char)dst.GetX(), (unsigned char)dst.GetY(), 2, mode);
	}	
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::AttachLadderTransition( CPathNetwork* pNet, int nLadder, const CNodesLayer::SLadder &ladder )
{
	int nOtherLayer = ladder.placeOnTop.GetLayer();
	CMapColourer *pAnother = pNet->GetColourer( nOtherLayer );
	if ( !pAnother->IsReady() )
		return;
	unsigned char cX = (unsigned char)ladder.placeOnTop.GetX();
	unsigned char cY = (unsigned char)ladder.placeOnTop.GetY();
	WORD wColor = pAnother->GetPointColor( cX, cY );
	WORD wDistance = AC_LADDER_STEP * ladder.GetHeight() / 2;
	WORD wDistance1 = pAnother->GetDistanceToCenter( cX, cY, PM_ANY_MOVE ); 
	zonesAnyMove.ladderUpperPoints[ nLadder ] = SNeighbour( wColor, wDistance + wDistance1, nOtherLayer );
	WORD wLadderColor = EC_LADDER_COLOR + nLadder * 2 + 1;
	pAnother->zonesAnyMove.nodes[ wColor ].AddNeighbour( wLadderColor, wDistance + wDistance1, nLayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::RecalcTransitions( CPathNetwork* pNet )
{
	RecalcTransitions( pNet, PM_STAND_ONLY );
	RecalcTransitions( pNet, PM_ANY_MOVE );
	for ( int i = 0; i < pMap->pLayer->ladders.size(); ++i )
	{
//		OutputDebugString("[ LADDER ] Attaching ladder up\n");
		AttachLadderTransition( pNet, i, pMap->pLayer->ladders[i] );
	}
/*
 	Ladders try:
	for ( int i = 0; i < zonesAnyMove.ladderLowerPoints.size(); ++i )
	{
		int nColor = zonesAnyMove.ladderLowerPoints[i].wNodeNumber;
		int x = localColorInfos[ nColor ].wAverageX;
		int y = localColorInfos[ nColor ].wAverageY;
		char buf[128];
		sprintf( buf, "[ LADDER ] Layer %d has ladder transition lower point %d, x %d, y %d\n",
			pMap->pLayer->nLayer, nColor, x, y );
		OutputDebugString( buf );
	}
	for ( int i = 0; i < zonesAnyMove.ladderUpperPoints.size(); ++i )
	{
		char buf[128];
		int nColor = zonesAnyMove.ladderUpperPoints[i].wNodeNumber;
		int nUpLayer = zonesAnyMove.ladderUpperPoints[i].nLayer;
		CMapColourer *pC = pNet->GetColourer( nUpLayer );
		int x = pC->localColorInfos[ nColor ].wAverageX;
		int y = pC->localColorInfos[ nColor ].wAverageY;
		sprintf( buf, "[ LADDER ] Layer %d has ladder transition upper point to layer %d color %d, x %d, y %d\n",
			pMap->pLayer->nLayer, nUpLayer, nColor, x, y );
		OutputDebugString( buf );
	}
	for ( vector<SNetNode>::iterator it = zonesAnyMove.nodes.begin(); it != zonesAnyMove.nodes.end(); ++it )
	{
		SNetNode &node = *it;
		for ( list<SNeighbour>::iterator lit = node.neighbours.begin(); lit != node.neighbours.end(); ++lit )
		{
			SNeighbour &ne = *lit;
			if ( ne.wNodeNumber > EC_LADDER_COLOR )
			{
				char buf[128];
				sprintf( buf, "[ LADDER ] Layer %d has ladder transition to layer %d\n", pMap->pLayer->nLayer, ne.nLayer );
				OutputDebugString( buf );
			}
		}
	}*/
	zonesToRecalc.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::RecalcTransitions( CPathNetwork* pNet, EPathfinderMode mode )
{
	const CNodesLayer::CTransitionsHash &tr = pMap->pLayer->transitions;
	for (CNodesLayer::CTransitionsHash::const_iterator k = tr.begin(); k != tr.end(); ++k )
	{
		unsigned char cX = (unsigned char)k->first.GetX();
		unsigned char cY = (unsigned char)k->first.GetY();
		if ( IsPointTouchingArea( cX, cY, zonesToRecalc ) )
			AttachTransitions( pNet, k->first, k->second, mode );
	}
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );
	
	int nMaxCellX = ((nSizeX + 7) >> 3) + 1, nMaxCellY = ((nSizeY + 7) >> 3) + 1;
	CArray2D<bool> mustInvestigate( nMaxCellX, nMaxCellY );
	unsigned char cX, cY;
	for ( cX = 0; cX < nMaxCellX; ++cX )
		for ( cY = 0; cY < nMaxCellY; ++cY )
			mustInvestigate[ cY ][ cX ] = false;
	for ( vector< CTRect<unsigned char> >::const_iterator i = zonesToRecalc.begin(); i != zonesToRecalc.end(); ++i )
	{
		int nMinNowX = Max( 0, (i->minx >> 3) - 1 ),
				nMinNowY = Max( 0, (i->miny >> 3) - 1 );
		int nMaxNowX = Min( nMaxCellX, (i->maxx >> 3) + 2 ),
				nMaxNowY = Min( nMaxCellY, (i->maxy >> 3) + 2 );
		for ( cX = nMinNowX; cX < nMaxNowX; ++cX )
			for ( cY = nMinNowY; cY < nMaxNowY; ++cY )
				mustInvestigate[ cY ][ cX ] = true;
	}
	for ( cX = 0; cX < nMaxCellX; ++cX )
	{
		for ( cY = 0; cY < nMaxCellY; ++cY )
		{
			if ( mustInvestigate[ cY ][ cX ] )
			{
				unsigned char 
					cMaxInvX = Min( nSizeX, ( cX<<3 ) + 8 ),
					cMaxInvY = Min( nSizeY, ( cY<<3 ) + 8 );
				unsigned char cSmallX, cSmallY;
				for ( cSmallY = cY << 3; cSmallY < cMaxInvY; ++cSmallY )
				{
					for ( cSmallX = cX << 3; cSmallX < cMaxInvX; ++cSmallX )
					{
						if ( pMap->HasSameTransition( cSmallX, cSmallY ) )
						{
							AttachSameTransitions( pNet, SPathPlace( cSmallX, cSmallY, nLayer ), PM_STAND_ONLY );
							AttachSameTransitions( pNet, SPathPlace( cSmallX, cSmallY, nLayer ), PM_ANY_MOVE );
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::AttachTransitions(	CPathNetwork* pNet )
{
	const CNodesLayer::CTransitionsHash &tr = pMap->pLayer->transitions;
	for (CNodesLayer::CTransitionsHash::const_iterator k = tr.begin(); k != tr.end(); ++k )
		AttachTransitions( pNet, k->first, k->second, PM_STAND_ONLY );
	for (CNodesLayer::CTransitionsHash::const_iterator k = tr.begin(); k != tr.end(); ++k )
		AttachTransitions( pNet, k->first, k->second, PM_ANY_MOVE );
	for ( int i = 0; i < pMap->pLayer->ladders.size(); ++i )
		AttachLadderTransition( pNet, i, pMap->pLayer->ladders[i] );
	int nSizeX, nSizeY;
	pMap->GetSizes( &nSizeX, &nSizeY );
	for ( int nX = 0; nX < nSizeX; ++nX )
	{
		for ( int nY = 0; nY < nSizeY; ++nY )
		{
			if ( pMap->HasSameTransition( nX, nY ) )
			{
				AttachSameTransitions( pNet, SPathPlace( nX, nY, nLayer ), PM_STAND_ONLY );
				AttachSameTransitions( pNet, SPathPlace( nX, nY, nLayer ), PM_ANY_MOVE );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapColourer::GetColorNeighbourCenters( CPathNetwork *pNet, WORD wColor, 
	vector<unsigned char> *cX, vector<unsigned char> *cY, vector<int> *layers, EPathfinderMode mode )
{
	list<SNeighbour> *myNeighbours;
	if ( mode == PM_ANY_MOVE ) 
		myNeighbours = &zonesAnyMove.nodes[wColor].neighbours;
	else
		myNeighbours = &zonesStandOnly.nodes[wColor].neighbours;
	for ( list<SNeighbour>::iterator i = myNeighbours->begin(); i != myNeighbours->end(); ++i )
	{
		WORD wCurrColor = i->wNodeNumber;
		CMapColourer *pOther = pNet->GetColourer( i->nLayer );
		if ( wCurrColor < EC_LADDER_COLOR )
		{
			ASSERT( pOther->localColorCounts[ wCurrColor ] );
			cX->push_back( (unsigned char)pOther->localColorInfos[ wCurrColor ].wAverageX );
			cY->push_back( (unsigned char)pOther->localColorInfos[ wCurrColor ].wAverageY );
			layers->push_back( i->nLayer );
		}
	}
}

}
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x70142160, CMapColourer );
REGISTER_SAVELOAD_CLASS( 0x70142161, CNodesLayerProxy );
REGISTER_SAVELOAD_CLASS( 0x72452120, CColouredWaysCalcer );