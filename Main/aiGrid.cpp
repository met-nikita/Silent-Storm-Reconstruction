#include "StdAfx.h"
#include "aiGrid.h"
#include "aiPassCalcer.h"
#include "aiMovesCalcer.h"
#include "Grid.h"
#include "aiPMConst.h"
#include "aiGridSet.h"
#include "aiColourer.h"
#include "wTSFlags.h"
#include "Bound.h"
#include "wInterface.h"
#include "wMine.h"
#include "..\MiscDll\LogStream.h"
#include "aiLocker.h"
#include "aiJob.h"
#include "aiPassCalcJob.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_MAX_FORMATION_RADIUS = 2.0f;
const float F_MAX_FORMATION_RADIUS2 = 4.0f;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
ETransitionType GetTransitionType ( IPathNetwork *pNet, const SPathPlace &src, const SPathPlace &dst );
////////////////////////////////////////////////////////////////////////////////////////////////////
void Xor( CPointsContainer *pRes, const CPointsContainer &first, const CPointsContainer &second )
{
	CPointsContainer &ret = *pRes;
	ret.clear();
	// add all points in first group but not in second
	for ( CPointsContainer::const_iterator i = first.begin(); i != first.end(); ++i )
	{
		bool bFlag = true;
		for ( CPointsContainer::const_iterator j = second.begin(); j != second.end(); ++j )
			if ( *i == *j )
				bFlag = false;
		if ( bFlag )
			ret.push_back( *i );
	}
	// add all points in second group but not in first
	for ( CPointsContainer::const_iterator i = second.begin(); i != second.end(); ++i )
	{
		bool bFlag = true;
		for ( CPointsContainer::const_iterator j = first.begin(); j != first.end(); ++j )
			if ( *i == *j )
				bFlag = false;
		if ( bFlag )
			ret.push_back( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CheckEverythingOK( const CArray2D<CNodesLayer::STile> &tiles, int nLayer, const CNodesLayer::CTransitionsHash &transitions )
{
#ifdef _DEBUG
	for ( int y = 0; y < tiles.GetYSize(); ++y )
	{
		for ( int x = 0; x < tiles.GetXSize(); ++x )
		{
			CNodesLayer::STile &t = tiles[y][x];
			SPathPlace p( x, y, nLayer );
			bool b1 = ( t.nFlags & TF_HAS_INTERGRID );
			bool b2 = transitions.find( p ) != transitions.end();
			ASSERT( b1 == b2 );
			if ( b2 )
			{
				const CNodesLayer::STransitionSet &tr = transitions.find( p )->second;
				for ( list<CNodesLayer::SLink>::const_iterator i = tr.links.begin(); i != tr.links.end(); ++i )
				{
					const CNodesLayer::SLink &l = *i;	
					SPathPlace dst( l.dst );
				}
			}
		}
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayersSetTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLayersSetTracker: public NAI::IAIMapTracker
{
	OBJECT_BASIC_METHODS(CLayersSetTracker);
public:
	ZDATA
	CPtr<CLayersGroup> pLayersGroup;
	CTPoint<int> p;
	SBound bv;
	CPtr<IAIMap> pAIMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pLayersGroup); f.Add(3,&p); f.Add(4,&bv); f.Add(5,&pAIMap); return 0; }

	CLayersSetTracker() {}
	CLayersSetTracker( IAIMap *_pAIMap, CLayersGroup *_pLayersGroup, const CTPoint<int> &_p, const SBound &_bv )
		: pLayersGroup(_pLayersGroup), p(_p), bv(_bv), pAIMap(_pAIMap)
	{ 
		pAIMap->AddTracker( this, bv, NWorld::TS_PASS_BLOCKER, false );
	}
	virtual void OnChange() 
	{ 
		if ( !IsValid(pLayersGroup) )
			return;
		if ( pLayersGroup->squareLevel[p.y][p.x] == 0 )
			return;
		pLayersGroup->squareLevel[p.y][p.x] = 0;
		// mark colourer to be updated
		CTRect<unsigned char> r;
		for ( unsigned int i = 0; i < pLayersGroup->layers.size() ; ++i )
		{
			CPtr<CNodesLayer> pLayer = pLayersGroup->layers[i];
			if ( !IsValid(pLayer) )
				continue;
			r.x1 = p.x * N_RECALC_GRID_SIZE;
			r.y1 = p.y * N_RECALC_GRID_SIZE;
			r.x2 = Min( ( p.x + 1 ) * N_RECALC_GRID_SIZE, pLayer->tiles.GetXSize() );
			r.y2 = Min( ( p.y + 1 ) * N_RECALC_GRID_SIZE, pLayer->tiles.GetYSize() );
			pLayer->pColourer->AddZoneToRecalc( r );
			/*if ( pLayer->nLayer == 0 )
			{
				char buf[128];
				sprintf( buf, "Added recalc zone %d %d %d %d\n", r.x1, r.x2, r.y1, r.y2 );
				OutputDebugString( buf ); 
			}*/
		}
		CDynamicCast<IAIJobManager> pJobManager(pLayersGroup->pNet->GetAIJobManager());
		if (pJobManager)
		{
			SPathPlace where( p.x * N_RECALC_GRID_SIZE,  p.y * N_RECALC_GRID_SIZE, 0 );
			AddNewPassCalcerJob( pJobManager, pAIMap, pLayersGroup, where );
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLadderTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLadderTracker: public NAI::IAIMapTracker
{
	OBJECT_BASIC_METHODS(CLadderTracker);
public:
	ZDATA
	CPtr<CNodesLayer> pLayer;
	int nLadder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pLayer); f.Add(3,&nLadder); return 0; }

	CLadderTracker() {}
	CLadderTracker( IAIMap *pAIMap, CNodesLayer *_pLayer, int _nLadder, const SBound &_bv ): pLayer(_pLayer), nLadder(_nLadder)
	{
		pAIMap->AddTracker( this, _bv, NWorld::TS_PASS_BLOCKER, false );
	}
	virtual void OnChange() 
	{ 
		if ( !IsValid(pLayer) )
			return;
		pLayer->ladders[nLadder].bNeedRecalc = true;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayersGroup
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::RefreshSpot( const SPathPlace &p, IAIMap *pMap, char cLevel )
{
	if ( bFreeze )
		return;
	int nX, nY;
	int nLayer = p.GetLayer();
	if ( p.IsIntegral() )
	{
		nX = p.GetX() / N_RECALC_GRID_SIZE;
		nY = p.GetY() / N_RECALC_GRID_SIZE;
	}
	else
	{
		pNet->GetLayer( p.GetLayer() )->RefreshLadder( p.GetX(), pMap );
		return;
	}
	if ( squareLevel[nY][nX] < cLevel )
	{
		RecalcSquare( nX, nY, pMap, cLevel );
		squareLevel[nY][nX] = cLevel;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::RegisterFloorLayer( CNodesLayer *pLayer, int nFloor )
{
	for ( int x = 0; x < squareLevel.GetXSize(); ++x )
		for ( int y = 0; y < squareLevel.GetYSize(); ++y )
			squareLevel[y][x] = 0;
	int nIndex = ( nFloor - nFirstFloor ) * N_MAX_LAYERS_PER_FLOOR;
	CPtr<CNodesLayer> &pL = layers[ nIndex ];
	if ( !pL ) 
		pL = pLayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void KillBadHCMovesDir( CArray2D<CNodesLayer::STile> *pTiles, const CTPoint<int> &shift, 
		const CTRect<int> &region, char flag, char backFlag )
{
	int nMinX = Max( 0, - shift.x * 2 ), 
		  nMinY = Max( 0, - shift.y * 2 ); 
	nMinX = Max( nMinX, region.minx );	
	nMinY = Max( nMinY, region.miny );
	int nMaxX = pTiles->GetXSize() + Min( 0, - shift.x * 2 ), 
		  nMaxY = pTiles->GetYSize() + Min( 0, - shift.y * 2 ); 
	nMaxX = Min( nMaxX, region.maxx );	
	nMaxY = Min( nMaxY, region.maxy );
	CArray2D<CNodesLayer::STile> &tiles = *pTiles;
	for ( int x = nMinX; x < nMaxX; ++x )
	{
		for ( int y = nMinY; y < nMaxY; ++y )
		{
			CNodesLayer::STile &t1 = tiles[y][x];
			if ( ( t1.nMoveCrouch & backFlag ) && ( t1.nPassable & CP_INACTIVE ) )
			{
				if ( t1.nFlags & TF_IS_LADDER_UP )
					continue;
				CNodesLayer::STile &t2 = tiles[ y + shift.y ][ x + shift.x ];
				if ( ( t1.nMoveHC & flag ) && t2.nHeight < t1.nHeight )
					continue;
				t1.nMoveCrouch &= ~backFlag;
				t1.nMoveLay &= ~backFlag;
				if ( IsInArray( tiles, x - shift.x, y - shift.y ) )
				{
					CNodesLayer::STile &t3 = tiles[ y - shift.y ][ x - shift.x ];
					t3.nMoveLay &= ~flag;
					t3.nMoveCrouch &= ~flag;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void KillBadHCMoves( CArray2D<CNodesLayer::STile> *pTiles, const CTRect<int> &region )
{
	KillBadHCMovesDir( pTiles, CTPoint<int>( 0, 1 ), region, 1 << CD_U, 1 << CD_D );
	KillBadHCMovesDir( pTiles, CTPoint<int>( 0, -1 ), region, 1 << CD_D, 1 << CD_U );
	KillBadHCMovesDir( pTiles, CTPoint<int>( 1, 0 ), region, 1 << CD_R, 1 << CD_L );
	KillBadHCMovesDir( pTiles, CTPoint<int>( -1, 0 ), region, 1 << CD_L, 1 << CD_R );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CNodesLayer* CLayersGroup::GetRootFloorLayer( int nFloor )
{
	int nIndex = ( nFloor - nFirstFloor ) * N_MAX_LAYERS_PER_FLOOR;
	return layers[nIndex];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::RecalcSquare( int nX, int nY, IAIMap *pMap, char nLevel )
{
	ASSERT( pMap );
	if ( !pMap )
		return;
	int nMinX = nX * N_RECALC_GRID_SIZE, nMaxX = nMinX + N_RECALC_GRID_SIZE;
	int nMinY = nY * N_RECALC_GRID_SIZE, nMaxY = nMinY + N_RECALC_GRID_SIZE;
	nMaxX = Min( nMaxX, GetXSize() );
	nMaxY = Min( nMaxY, GetYSize() );
	CTRect<int> region( nMinX - 1, nMinY - 1, nMaxX + 1, nMaxY + 1 );
	STempArrayGroup<CNodesLayer::STile> tempArrays;
	tempArrays.region = region;
	CPtr<CCollider> pCollider;
	if ( TestSquare( nX, nY, 1 ) )
	{
    // copy to temp
		for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
		{
			for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
			{
				CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
				if ( !IsValid( pLayer ) )
					continue;
				CArray2D<CNodesLayer::STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
				nMinX = Max( 0, -region.minx );
				nMinY = Max( 0, -region.miny );
				nMaxX = Min( temp.GetXSize(), pLayer->tiles.GetXSize() - region.minx );
				nMaxY = Min( temp.GetYSize(), pLayer->tiles.GetYSize() - region.miny );
				for ( int y = nMinY; y < nMaxY; ++y )
				{
					for ( int x = nMinX; x < nMaxX; ++x )
						temp[y][x] = pLayer->tiles[y + region.miny][x + region.minx];
				}			
			}
		}
	}
	else
	{
		// pass calc
		//char buf[128];
		//sprintf( buf, "Recalcing spot %d %d, x = %d..%d, y = %d..%d \n", nX, nY, nMinX, nMaxX-1, nMinY, nMaxY-1 );
		//OutputDebugString( buf );
		CPassCalcer passCalcer( pNet, this, pMap, tempArrays, region );
		// mark ladder upper points as must be inactive: first for ladders which are not yet really created...
		for ( int j = 0; j < ladders.size(); ++j )
		{
			CVec2 shift2;
			SNotYetCreatedLadder &ladder = ladders[ j ];
			int x = ladder.nX, y = ladder.nY, upX = x, upY = y;
			switch ( (ELadderDirection)ladder.nRotation )
			{
				case LD_RIGHT: upX = x - 1; break;
				case LD_FRONT: upY = y - 1; break;
				case LD_LEFT: upX = x + 1; break;
				case LD_BACK: upY = y + 1; break;
				default: ASSERT(0);
			}				
			if ( upX > region.minx && upY > region.miny && upX < region.maxx - 1 && upY < region.maxy - 1 )
			{
				char buf[128];
				sprintf_s( buf, "Marking not created ladder %d at %d, %d\n", j, upX, upY );
				OutputDebugString( buf );
				passCalcer.MakeInactiveForLadder( x - region.minx, y - region.miny, upX - region.minx, upY - region.miny, 
					ladder.nHeight * F_LADDER_STEP, ladder.nFloor );
			}
		}
		// ...than for ladders which are already created
		for ( int i = 0; i < N_MAX_FLOORS; ++i )
		{
			CNodesLayer *pL = layers[ i * N_MAX_LAYERS_PER_FLOOR ];
			if ( !pL )
				continue;
			CVec2 shift2;
			for ( int j = 0; j < pL->ladders.size(); ++j )
			{
				CNodesLayer::SLadder &ladder = pL->ladders[ j ];
				int x = ladder.placeOnBottom.GetX(), y = ladder.placeOnBottom.GetY(), upX = x, upY = y;
				switch ( ladder.eDir )
				{
					case LD_RIGHT: upX = x - 1; break;
					case LD_FRONT: upY = y - 1; break;
					case LD_LEFT: upX = x + 1; break;
					case LD_BACK: upY = y + 1; break;
					default: ASSERT(0);
				}				
				if ( upX > region.minx && upY > region.miny && upX < region.maxx - 1 && upY < region.maxy - 1 )
				{
					//char buf[128];
					//sprintf( buf, "Marking already created ladder %d at %d, %d\n", j, upX, upY );
					//OutputDebugString( buf );
					passCalcer.MakeInactiveForLadder( x - region.minx, y - region.miny, upX - region.minx, upY - region.miny, 
						(ladder.GetHeight() - 1) * F_LADDER_STEP, i );
				}
			}
		}
		passCalcer.Calc();
		pCollider = passCalcer.GetCollider();
		squareLevel[nY][nX] = 1;
		SetTracker( nX, nY, pMap, tempArrays );
		if ( nLevel > 1 )
		{
			for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
			{
				for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
				{
					CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
					if ( !IsValid( pLayer ) )
						continue;
					CArray2D<CNodesLayer::STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
					for ( int y = 1; y < temp.GetYSize() - 1; ++y )
					{
						for ( int x = 1; x < temp.GetXSize() - 1; ++x )
						{
							CNodesLayer::STile &dst = pLayer->tiles[y + region.miny][x + region.minx];
							CNodesLayer::STile &src = temp[y][x];
							src.nLocks = dst.nLocks;
							src.nDynLocks = dst.nDynLocks;			
							dst = src;
						}
					}
				}
			}
		}
	}
	// moves calcing
	if ( nLevel > 1 )
	{
		for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
		{
			for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
			{
				CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
				if ( !IsValid( pLayer ) )
					continue;
				CMovesCalcer mc( pLayer, pMap, region, tempArrays, nLayer, nFloor, pCollider );
				mc.Calc();
			}
		}
	}
	// for each layer copy temp to destination
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
		{
			CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
			if ( !IsValid( pLayer ) )
				continue;
			CArray2D<CNodesLayer::STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
			for ( int y = 1; y < temp.GetYSize() - 1; ++y )
			{
				for ( int x = 1; x < temp.GetXSize() - 1; ++x )
				{
					CNodesLayer::STile &dst = pLayer->tiles[y + region.miny][x + region.minx];
					CNodesLayer::STile &src = temp[y][x];
					src.nLocks = dst.nLocks;
					src.nDynLocks = dst.nDynLocks;			
					SPathPlace p( x + region.minx, y + region.miny, pLayer->nLayer ); 
					if ( pLayer->transitions.find( p ) != pLayer->transitions.end() )
						src.nFlags |= TF_HAS_INTERGRID;
					else
						src.nFlags &= ~TF_HAS_INTERGRID;
					if ( src.nMoveStand != 0 )
						src.nFlags |= TF_STAND_PASSABLE;
					else
						src.nFlags &= ~TF_STAND_PASSABLE;
					dst = src;
					if ( dst.nPassable == 0 )
						dst.nMoveLay = dst.nMoveCrouch = dst.nMoveHC = dst.nMoveLay = 0;
				}
			}
			if ( nLevel > 1 )
				CheckEverythingOK( pLayer->tiles, pLayer->nLayer, pLayer->transitions );
			if ( pNet->IsGridReady() )
				KillBadHCMoves( &pLayer->tiles, region );
		}
	}
	// borders problem
	for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
		{
			CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
			if ( !IsValid( pLayer ) )
				continue;
			if ( nLevel > 1 )
				CheckEverythingOK( pLayer->tiles, pLayer->nLayer, pLayer->transitions );
			CArray2D<CNodesLayer::STile> &tiles = pLayer->tiles;
			for ( int y = 0; y < tiles.GetYSize(); ++y )
			{
				tiles[ y ][ 0 ].nMoveLay &= ~(char)( 8 + 16 + 32 );
				tiles[ y ][ 0 ].nMoveCrouch &= ~(char)( 8 + 16 + 32 );
				tiles[ y ][ 0 ].nMoveStand &= ~(char)( 8 + 16 + 32 );
				tiles[ y ][ 0 ].nMoveHC &= ~(char)( 8 + 16 + 32 );
				tiles[y][ tiles.GetXSize() - 1 ].nMoveLay &= ~(char)( 128 + 1 + 2 );
				tiles[y][ tiles.GetXSize() - 1 ].nMoveCrouch &= ~(char)( 128 + 1 + 2 );
				tiles[y][ tiles.GetXSize() - 1 ].nMoveStand &= ~(char)( 128 + 1 + 2 );
				tiles[y][ tiles.GetXSize() - 1 ].nMoveHC &= ~(char)( 128 + 1 + 2 );
			}
			for ( int x = 0; x < tiles.GetXSize(); ++x )
			{
				tiles[ 0 ][ x ].nMoveLay &= ~(char)( 32 + 64 + 128 );
				tiles[ tiles.GetYSize() - 1 ][ x ].nMoveLay &= ~(char)( 2 + 4 + 8 );
				tiles[ 0 ][ x ].nMoveCrouch &= ~(char)( 32 + 64 + 128 );
				tiles[ tiles.GetYSize() - 1 ][ x ].nMoveCrouch &= ~(char)( 2 + 4 + 8 );
				tiles[ 0 ][ x ].nMoveStand &= ~(char)( 32 + 64 + 128 );
				tiles[ tiles.GetYSize() - 1 ][ x ].nMoveStand &= ~(char)( 2 + 4 + 8 );
				tiles[ 0 ][ x ].nMoveHC &= ~(char)( 32 + 64 + 128 );
				tiles[ tiles.GetYSize() - 1 ][ x ].nMoveHC &= ~(char)( 2 + 4 + 8 );
			}
			if ( nLevel > 1 )
				CheckEverythingOK( pLayer->tiles, pLayer->nLayer, pLayer->transitions );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayersGroup::TestSquare( int nX, int nY, char nLevel )
{
	if ( squareLevel[nY][nX] < nLevel )
		return false;
	int nMinX, nMinY, nMaxX, nMaxY;
	nMinX = nX - 1; nMinY = nY - 1; nMaxX = nX + 2; nMaxY = nY + 2;
	nMinX = Max( 0, nMinX );
	nMinY = Max( 0, nMinY );
	nMaxX = Min( nMaxX, squareLevel.GetXSize() );
	nMaxY = Min( nMaxY, squareLevel.GetYSize() );
	for ( int y = nMinY; y < nMaxY; ++y )
	{
		for ( int x = nMinX; x < nMaxX; ++x )
		{
			if ( squareLevel[y][x] < nLevel )
				return false;
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator==( const SBound &a, const SBound &b )
{
	return memcmp( &a, &b, sizeof(SBound) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::SetTracker( int nX, int nY, IAIMap *pMap, const STempArrayGroup<STile> &tempArrays )
{
	CTPoint<int> ptMin(0,0), ptMax( N_RECALC_GRID_SIZE + 1, N_RECALC_GRID_SIZE + 1 );
	int nMinZ = 0, nMaxZ = 0xffff;
	//int nMinZ = 0xffff, nMaxZ = 0;
	int nShiftX = nX * N_RECALC_GRID_SIZE - 1, nShiftY = nY * N_RECALC_GRID_SIZE - 1;
	CVec2 pt11 = GetCPNoHeight( (float)(ptMin.x + nShiftX), (float)(ptMin.y + nShiftY) );
	CVec2 pt12 = GetCPNoHeight( (float)(ptMin.x + nShiftX), (float)(ptMax.y + nShiftY) );
	CVec2 pt22 = GetCPNoHeight( (float)(ptMax.x + nShiftX), (float)(ptMax.y + nShiftY) );
	CVec2 pt21 = GetCPNoHeight( (float)(ptMax.x + nShiftX), (float)(ptMin.y + nShiftY) );

	/*for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
	{
		for ( int nLayer = 0; nLayer < N_MAX_LAYERS_PER_FLOOR; ++nLayer )
		{
			CPtr<CNodesLayer> pLayer = layers[ nFloor * N_MAX_LAYERS_PER_FLOOR + nLayer ];
			if ( !IsValid( pLayer ) )
				continue;
			const CArray2D<STile> &temp = *tempArrays.GetArray( nFloor, nLayer );
			for ( int y = 1; y < temp.GetYSize() - 1; ++y )
			{
				for ( int x = 1; x < temp.GetXSize() - 1; ++x )
				{				
					if ( temp[y][x].nHeight < nMinZ )
						nMinZ = temp[y][x].nHeight;
					if ( temp[y][x].nHeight > nMaxZ )
						nMaxZ = temp[y][x].nHeight;
				}
			}
		}
	}*/

	float fMinZ = GetFHeight( nMinZ ) + F_TEST_SPHERE_RADIUS;
	float fMaxZ = GetFHeight( nMaxZ ) + F_HEIGHT - F_TEST_SPHERE_RADIUS;
	SBoundCalcer bc;
	bc.Add( CVec3( pt11.x, pt11.y, fMinZ ), F_TEST_SPHERE_RADIUS );
	bc.Add( CVec3( pt12.x, pt12.y, fMinZ ), F_TEST_SPHERE_RADIUS );
	bc.Add( CVec3( pt22.x, pt22.y, fMaxZ ), F_TEST_SPHERE_RADIUS );
	bc.Add( CVec3( pt21.x, pt21.y, fMaxZ ), F_TEST_SPHERE_RADIUS );
	SBound bv;
	bc.Make( &bv );
	CVec3 vDiagonal( fabs( pt22.x - pt11.x ), fabs( pt22.y - pt11.y ), Max( 0.0f, fMaxZ - fMinZ ) );
	float fNewRadius = fabs( vDiagonal ) * 0.5f + F_TEST_SPHERE_RADIUS;
	ASSERT( bv.s.fRadius >= fNewRadius );//fabs2(vDiagonal) <= sqr( bv.s.fRadius ) );
	bv.s.fRadius = fNewRadius;
	if ( IsValid( squareTrackers[nY][nX] ) )
	{
		CLayersSetTracker *pTracker = squareTrackers[nY][nX];
		if ( pTracker->bv == bv )
			return;
	}
#ifdef _DEBUG
	for ( int i = 0; i < layers.size(); ++i )
	{
		if ( !IsValid( layers[i] ) )
			continue;
		CNodesLayer *pLayer = layers[i];
		ASSERT( IsValid( pLayer->pColourer ) );
	}
#endif
	//char buf[128];
	//sprintf( buf, "Added tracker for X = %d..%d, Y = %d..%d,",  );
	//OutputDebugString( buf );
	squareTrackers[nY][nX] = new CLayersSetTracker( pMap, this, CTPoint<int>(nX,nY), bv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::BuildLayersGroup( int _nXSize, int _nYSize, const CVec2 &_ptOrigin, const CVec2 &_ptXDir, 
	CPathNetwork *_pNet )
{
	pNet = _pNet;
	ptOrigin = _ptOrigin;
	ptXDir = _ptXDir;
	nXSize = _nXSize;
	nYSize = _nYSize;
	squareLevel.SetSizes( 
		( nXSize + N_RECALC_GRID_SIZE - 1 ) / N_RECALC_GRID_SIZE,
		( nYSize + N_RECALC_GRID_SIZE - 1 ) / N_RECALC_GRID_SIZE );
	squareLevel.FillEvery( 0 );
	squareTrackers.SetSizes( squareLevel.GetXSize(), squareLevel.GetYSize() );
	layers.resize( N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR );
	for ( int i = 0; i < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++i )
		layers[ i ] = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SPoint CLayersGroup::GetPoint( const CVec3 &pos ) const
{
	float fShiftX = pos.x - ptOrigin.x;
	float fShiftY = pos.y - ptOrigin.y;
	const float fKoef = 1 / ( FP_GRID_STEP * FP_GRID_STEP );
	int nX = Float2Int( (  fShiftX * ptXDir.x + fShiftY * ptXDir.y ) * fKoef );
	int nY = Float2Int( ( -fShiftX * ptXDir.y + fShiftY * ptXDir.x ) * fKoef );
	nX = Max( nX, 0 ); nX = Min( nX, GetXSize() - 1 );
	nY = Max( nY, 0 ); nY = Min( nY, GetYSize() - 1 );
	return SPoint( nX, nY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersGroup::GetExactPoint( CVec2 *pRes, const CVec2 &pos ) const
{
	float fX = pos.x - ptOrigin.x;
	float fY = pos.y - ptOrigin.y;
	const float fKoef = 1 / ( FP_GRID_STEP * FP_GRID_STEP );
	pRes->x = (  fX * ptXDir.x + fY * ptXDir.y ) * fKoef;
	pRes->y = ( -fX * ptXDir.y + fY * ptXDir.x ) * fKoef;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayersGroup::IsInside( const CVec2 &ptTest ) const
{
	CVec2 ptLocal;
	GetExactPoint( &ptLocal, ptTest );
	return ptLocal.x >= 0 && ptLocal.y >= 0 && ptLocal.x <= GetXSize() && ptLocal.y <= GetYSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayersGroup::SIgnoreRect::IsInside( const CVec2 &ptTest ) const
{
	CVec2 ptShift( ptTest - ptOrigin );
	float fRes;
	const float fKoef = 1 / ( FP_GRID_STEP * FP_GRID_STEP );
	fRes = ( ptShift.x * ptXDir.x + ptShift.y * ptXDir.y ) * fKoef;
	if ( fRes < 1e-5 || fRes > ptSize.x - 1e-5 )
		return false;
	fRes = (-ptShift.x * ptXDir.y + ptShift.y * ptXDir.x ) * fKoef;
	if ( fRes < 1e-5 || fRes > ptSize.y - 1e-5 )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayersGroup::IsIgnored( float x, float y ) const
{
	for ( unsigned int i = 0; i < ignoreRects.size(); ++i )
	{
		if ( ignoreRects[i].IsInside( GetCPNoHeight( x, y ) ) )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNodesLayer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNodesLayer::GetSamePoints( const SPathPlace& p, vector<SPathPlace> *pRes )
{
	STile &t1 = tiles[ p.GetY() ][ p.GetX() ];
	if ( ( t1.nFlags & TF_HAS_SAME ) == 0 )
		return;
	for ( int i = 0; i < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++i )
	{
		CNodesLayer *pL = pGroup->layers[ i ];
		if ( !pL )
			continue;
		if ( pL == this )
			continue;
		STile &t2 = pL->tiles[ p.GetY() ][ p.GetX() ];
		if ( t2.nHeight == t1.nHeight && t2.nDisplacement == t1.nDisplacement )
			pRes->push_back( SPathPlace( p.GetX(), p.GetY(), pL->nLayer ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CNodesLayer::GetSamePoints( const SPathPlace& p, SPathPlace *places )
{
	STile &t1 = tiles[ p.GetY() ][ p.GetX() ];
	if ( ( t1.nFlags & TF_HAS_SAME ) == 0 )
		return 0;
	int nP = 0;
	for ( int i = 0; i < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++i )
	{
		CNodesLayer *pL = pGroup->layers[ i ];
		if ( !pL )
			continue;
		if ( pL == this )
			continue;
		STile &t2 = pL->tiles[ p.GetY() ][ p.GetX() ];
		if ( t2.nHeight == t1.nHeight && t2.nDisplacement == t1.nDisplacement )
		{
			places[ nP ] = SPathPlace( p.GetX(), p.GetY(), pL->nLayer );
			++nP;
		}
	}
	return nP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CNodesLayer::SetRoom( int x, int y, unsigned short nRoom ) 
{
	if ( ((unsigned)x) < rooms.GetXSize() && ((unsigned)y) < rooms.GetYSize() )
		rooms[y][x] = nRoom; 
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNodesLayer::BuildLayer( int nXSize, int nYSize, const CVec2 &_ptOrigin, const CVec2 &_ptXDir, 
	int _nFloor, int _nLayer, CPathNetwork *_pNet, CLayersGroup* _pGroup  )
{
	if ( !_pGroup )
	{
		pGroup = new CLayersGroup;
		pGroup->BuildLayersGroup( nXSize, nYSize, _ptOrigin, _ptXDir, _pNet );
		pGroup->nFirstFloor = _nFloor;
		pGroup->RegisterFloorLayer( this, _nFloor );
	}
	else
	{
		pGroup = _pGroup;
		pGroup->RegisterFloorLayer( this, _nFloor ); 
	}
	nLayer = _nLayer;
	nFloor = _nFloor;
	tiles.SetSizes( nXSize, nYSize ); 
	for ( int y = 0; y < nYSize; ++y )
	{
		for ( int x = 0; x < nXSize; ++x )
		{
			tiles[y][x].nPassable = 0; // // set of 1<<ECheckPose bits
			tiles[y][x].nMoveLay = 0;
			tiles[y][x].nMoveCrouch = 0;
			tiles[y][x].nMoveStand = 0; // move flags by direction by pose
			tiles[y][x].nFake = 0;
			tiles[y][x].nHeight = 0;
			tiles[y][x].nMoveHC = 0;
			tiles[y][x].nLocks = 0;
			tiles[y][x].nDynLocks = 0;
			tiles[y][x].nDisplacement = 0;
			tiles[y][x].nFlags = 0;
			tiles[y][x].nFlipper = 0;
		}
	}
	/*for ( int y = 1; y < nYSize - 1; ++y )
	{
		for ( int x = 1; x < nXSize - 1; ++x )
		{
			tiles[y][x].nMoveStand = (char) 0xff;
		}
	}*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNodesLayer::RefreshLadder( int nLadder, IAIMap *pMap )
{
	CNodesLayer::SLadder &ladder = ladders[ nLadder ];
	if ( !ladder.bNeedRecalc ) 
		return;
	CLadderCalcer calcer( this, pMap, &ladder, &ladder.pointPassable );
	calcer.Calc();
	ladder.bNeedRecalc = false;
	// ��������
	// �������� �������� �� ������� � ������ �����
	int nHeight = ladder.GetHeight();
	ladder.bConsistent = true;
	for ( int i = 0; i < nHeight - 3; ++i )
	{
		if ( !ladder.pointPassable[i] )
		{
			ladder.bConsistent = false;
			break;
		}
	}
	CVec3 ptBottom = pGroup->pNet->GetCP( ladder.placeOnBottom );
	CVec3 ptTop = pGroup->pNet->GetCP( ladder.placeOnTop );
	float fDiffZ = ptTop.z - ptBottom.z;
	float fNeedDiff = F_LADDER_STEP * nHeight;
	bool bBadUpperPoint = fDiffZ < fNeedDiff - 1 || fDiffZ > fNeedDiff + 1;
	if ( !bBadUpperPoint )
	{
		CNodesLayer* pLayer = pGroup->pNet->GetLayer( ladder.placeOnTop.GetLayer() );
		CNodesLayer::STile &t = pLayer->tiles[ ladder.placeOnTop.GetY() ][ ladder.placeOnTop.GetX() ];
		if ( ( t.nPassable & CP_INACTIVE ) == 0 )
			bBadUpperPoint = true;
		if ( t.nMoveCrouch == 0 && t.nMoveHC == 0 )
			bBadUpperPoint = true;
	}
	if ( bBadUpperPoint )
	{
		ladder.bConsistent = false;
		for ( int i = 0; i < nHeight; ++i )
			ladder.pointPassable[ i ] = false;
	}
	if ( ladder.bConsistent ) // �������� ��������� ���������
	{
		for ( int i = 0; i < nHeight / 2; ++i )
			ladder.pointOnUpperHalf[i] = false;
		for ( int i = nHeight / 2 + 1; i < nHeight; ++i )
			ladder.pointOnUpperHalf[i] = true;
	}
	else // �������� �������� ���������
	{
		int i;
		for ( i = 0; ladder.pointPassable[i]; ++i )
			ladder.pointOnUpperHalf[ i ] = true;
		int firstInpassable = i;
		for ( i = nHeight - 4; ladder.pointPassable[i]; --i )
			ladder.pointOnUpperHalf[ i ] = false;
		int lastInpassable = i;
		for ( i = firstInpassable; i < lastInpassable; ++i )
			ladder.pointPassable[ i ] = false; // ������������ ����� ������ �������� �������� �������
	}
	// create tracker
	if ( !IsValid(ladder.pTracker) )
	{
		SBound bv;
		// bv.BoxInit( xz ); // CRAP
		bv.SphereInit( CVec3(0,0,0), 1e6f ); // will work for sure
		ladder.pTracker = new CLadderTracker( pMap, this, nLadder, bv );
	}
	// add ladder transitions to pathnetwork
	// bottom
	ladderEntrances[ ladder.placeOnBottom ].bUpper = false;
	ladderEntrances[ ladder.placeOnBottom ].nLayerGroup = nLayer;
	ladderEntrances[ ladder.placeOnBottom ].nLadder = nLadder;
	// top
	CNodesLayer* pLayer = pGroup->pNet->GetLayer( ladder.placeOnTop.GetLayer() );
	pLayer->ladderEntrances[ ladder.placeOnTop ].bUpper = true;
	pLayer->ladderEntrances[ ladder.placeOnTop ].nLayerGroup = nLayer;
	pLayer->ladderEntrances[ ladder.placeOnTop ].nLadder = nLadder;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNodesLayer::AddNearPoints( const SSphere &s, vector<SPathPlace> *pRes, bool bTakeAll )
{
	float fLength = s.fRadius / FP_GRID_STEP;
	CVec2 p;
	pGroup->GetExactPoint( &p, CVec2( s.ptCenter.x, s.ptCenter.y ) );
	CTRect<int> r( Float2Int( p.x - fLength - 0.5f ), Float2Int( p.y - fLength - 0.5f ),
		Float2Int( p.x + fLength + 0.5f ), Float2Int( p.y + fLength + 0.5f ) );
	r.minx = Max( 0, r.minx );
	r.miny = Max( 0, r.miny );
	r.maxx = Min( tiles.GetXSize() - 1, r.maxx );
	r.maxy = Min( tiles.GetYSize() - 1, r.maxy );
	for ( int y = r.miny; y <= r.maxy; ++y )
	{
		for ( int x = r.minx; x <= r.maxx; ++x )
		{
			SPathPlace p;
			p.SetOnLayer( nLayer, x, y );
			pGroup->RefreshSpot( p, pGroup->pNet->pMap, 1 );
			CVec3 ptCP = GetCP( p );
			if ( fabs2( s.ptCenter - ptCP ) < sqr( s.fRadius ) )
			{
				CNodesLayer::STile &t1 = tiles[ y ][ x ];
				bool bAdd = true;
				if ( !bTakeAll )
				{
					for ( int nL = 0; nL < N_MAX_LAYERS_PER_FLOOR * N_MAX_FLOORS; ++nL )
					{
						CNodesLayer *pL = pGroup->layers[ nL ];
						if ( !pL )
							continue;
						if ( pL == this )
							continue;
						CNodesLayer::STile &t2 = pL->tiles[ y ][ x ];
						bool bBetter = ( t2.nPassable & t1.nPassable ) == t1.nPassable;
						if ( t2.nPassable == t1.nPassable )
							bBetter = pL->nLayer < nLayer;
						if ( t2.nHeight == t1.nHeight && t2.nDisplacement == t1.nDisplacement && bBetter )
						{
							bAdd = false;
							break;
						}
					}
				}
				if ( bAdd )
					pRes->push_back( p );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNodesLayer::IsValidDestination( const SPathPlace &p )
{
	ASSERT( p.GetLayer() == nLayer );
	pGroup->RefreshSpot( p, pGroup->pNet->pMap, 2 );
	const CNodesLayer::STile &t = tiles[p.GetY()][p.GetX()];
	if ( t.nPassable == 0 )
		return false;
	if ( t.nLocks != 0 )
		return false;
	if ( t.nMoveStand == 0 && t.nMoveCrouch == 0 && t.nMoveLay == 0 && t.nMoveHC == 0 )
	{
		SPathPlace transSearch( p.GetX(), p.GetY(), p.GetLayer() );
		if ( transitions.find( transSearch ) == transitions.end() )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CNodesLayer::RemoveAllTransitions( const CTRect<int> &rect )
{
	for ( CTransitionsHash::iterator i = transitions.begin(); i != transitions.end(); )
	{
		CTransitionsHash::iterator k = i;
		i++;
		const SPathPlace &p = k->first;
		ASSERT( p.IsIntegral() );
		/*if ( !p.nIntegral )
		{
			p = ladders[p.nX].placeOnBottom;
		}*/
		ASSERT( p.GetLayer() == nLayer );
		if ( p.GetX() >= rect.minx && p.GetX() < rect.maxx && p.GetY() >= rect.miny && p.GetY() < rect.maxy )
		{
			STile &t = tiles[ p.GetY() ][ p.GetX() ];
			t.nFlags &= ~TF_HAS_INTERGRID;
			t.nFlags &= ~TF_HAS_SAME;
			STransitionSet &s = k->second;
			for ( list<SLink>::iterator m = s.links.begin(); m != s.links.end(); ++m )
			{
				const SPathPlace &dst = m->dst;
				ASSERT( dst.GetLayer() != p.GetLayer() );
				CNodesLayer *pDstLayer = pGroup->pNet->layers[ dst.GetLayer() ];
				CTransitionsHash::iterator t = pDstLayer->transitions.find( dst );
				if ( t != pDstLayer->transitions.end() )
				{
					list<SLink>::iterator m = t->second.GetLink( p );
					if ( m != t->second.links.end() )
						t->second.links.erase( m );
					else
						ASSERT( 0 && "not matched link" );
					if ( t->second.links.empty() )
					{
						pDstLayer->transitions.erase( t );
						STile &t2 = pDstLayer->tiles[ dst.GetY() ][ dst.GetX() ];
						t2.nFlags &= ~TF_HAS_INTERGRID;
					}
				}
				else
					ASSERT( 0 && "not matched transition" );
			}
			transitions.erase( k );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPathNetwork
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathNetwork::CPathNetwork( IAIMap *_pMap, IAIJobManager *pManager )
: pMap(_pMap), bFreeze(false), pJobManager(pManager) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EDirection CPathNetwork::GetClosestDir( const SPathPlace &from, const SPathPlace &to )
{
	CVec2 ptFrom = GetCPNoHeight( from );
	CVec2 ptTo = GetCPNoHeight( to );
	CLayersGroup *pGroup = layers[from.GetLayer()]->pGroup;
	float x = ptTo.x - ptFrom.x;
	float y = ptTo.y - ptFrom.y;
	CVec2 local;
	local.x =   pGroup->ptXDir.x * x + pGroup->ptXDir.y * y;
	local.y = - pGroup->ptXDir.y * x + pGroup->ptXDir.x * y;
	if ( fabs(local.x) < 1e-5 && fabs(local.y) < 1e-5 )
		return (EDirection)to.GetDirection();
	float fAngle = NormalizeAngleInRadian( atan2( local.y, local.x ) );//+ FP_PI8 );
	int nDir = Float2Int( fAngle / FP_PI4 );
	nDir &= 7;
	return (EDirection)nDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EDirection CPathNetwork::GetClosestDir( int nLayer, float _fAngle )
{
	CLayersGroup *pGroup = layers[nLayer]->pGroup;
	float x = cos( _fAngle );
	float y = sin( _fAngle );
	CVec2 local;
	local.x =   pGroup->ptXDir.x * x + pGroup->ptXDir.y * y;
	local.y = - pGroup->ptXDir.y * x + pGroup->ptXDir.x * y;
	float fAngle = NormalizeAngleInRadian( atan2( local.y, local.x ) );//+ FP_PI8 );
	int nDir = Float2Int( fAngle / FP_PI4 );
	nDir &= 7;
	return (EDirection)nDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CPathNetwork::GetCP( const SPathPlace &src ) const
{
//	ASSERT( src.IsIntegral() );
	if ( src.IsFinal() ) // 3D/fly position: nLayer holds the altitude above the layer-0 ground cell
	{
		float fHeight = src.GetLayer() * F_3D_STEP;
		SPathPlace ground( ( src.GetData() & 0xffff ) | ( 0x3c01 << 16 ) ); // same x/y, integral, layer 0
		CVec3 cp = layers[0]->GetCP( ground );
		return CVec3( cp.x, cp.y, cp.z + fHeight );
	}
	CNodesLayer *pLayer = layers[src.GetLayer()];
	pLayer->pGroup->RefreshSpot( src, pMap, 1 );
	return pLayer->GetCP( src );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CPathNetwork::GetCPNoHeight( const SPathPlace &src ) const
{
//	ASSERT( src.IsIntegral() );
	CNodesLayer *pLayer = layers[ src.IsFinal() ? 0 : src.GetLayer() ]; // 3D/fly: ground cell on layer 0
	int nX, nY;
	if ( src.IsIntegral() )
	{
		nX = src.GetX();
		nY = src.GetY();
	}
	else
	{
		nX = layers[src.GetLayer()]->ladders[src.GetX()].placeOnBottom.GetX();
		nY = layers[src.GetLayer()]->ladders[src.GetX()].placeOnBottom.GetY();
	}
	return pLayer->GetCPNoHeight( src.GetX(), src.GetY() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CPathNetwork::GetDirection( const SPathPlace &src ) const
{
//	ASSERT( src.IsIntegral() );
	CNodesLayer *pLayer = layers[ src.IsFinal() ? 0 : src.GetLayer() ]; // 3D/fly: direction off layer 0
	return pLayer->GetDirection( src );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathNetwork::GetFloor( const SPathPlace &src ) const
{
	if ( src.IsFinal() ) // 3D/fly position: derive the floor from the encoded altitude (retail @0x3df00)
	{
		int n = Float2Int( src.GetLayer() * 0.04f + 0.5f );
		if ( n > 6 )
			return 4;
		if ( n < 0 )
			n = 0;
		return n - 3;
	}
	vector<SPathPlace> res;
	GetLayer( src.GetLayer() )->GetSamePoints( src, &res );
	int nFloor = GetLayer( src.GetLayer() )->GetFloor();
	//STile &t = GetLayer( src.GetLayer() )->tiles[ src.GetY() ][ src.GetX() ];
	for ( unsigned int i = 0; i < res.size(); ++i )
	{
		CNodesLayer::STile &t2 = GetLayer( res[i].GetLayer() )->tiles[ res[i].GetY() ][ res[i].GetX() ];
		int nF = GetLayer( res[i].GetLayer() )->GetFloor();
		if ( nFloor > nF && ( t2.nMoveStand != 0 || t2.nMoveHC != 0 ) )
			nFloor = nF;
	}
	return nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// true while any layer group still has a pending recolour job -- the script-visible "pass calcer is
// active" state (WaitForPassCalc loops on it). Retail CPathNetwork::HasPassCalcerJobs @0x3e4a0.
bool CPathNetwork::HasPassCalcerJobs() const
{
	for ( unsigned int i = 0; i < groups.size(); ++i )
		if ( IsValid( groups[i] ) && groups[i]->bHasRecolourJob )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetPassability( CArray2D<bool> *pRes, int nLayer )
{
	CNodesLayer *pLayer = layers[ nLayer ];
	pRes->SetSizes( pLayer->tiles.GetXSize(), pLayer->tiles.GetYSize() );
	for ( int y = 0; y < pLayer->tiles.GetYSize(); ++y )
	{
		for ( int x = 0; x < pLayer->tiles.GetXSize(); ++x )
		{
			SPathPlace p;
			p.SetOnLayer( nLayer, x, y );
			pLayer->pGroup->RefreshSpot( p, pMap, 1 );
			(*pRes)[y][x] = pLayer->tiles[y][x].nPassable != 0;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathNetwork::CreateLayersGroup( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFirstFloor )
{
	CLayersGroup* pGroup = new CLayersGroup;
	CVec2 ptXDir( cos(fAngle) * FP_GRID_STEP, sin(fAngle) * FP_GRID_STEP );
	pGroup->BuildLayersGroup( nXSize, nYSize, ptOrigin, ptXDir, this );
	pGroup->nFirstFloor = nFirstFloor;
	groups.push_back( pGroup );
	bColouringConstructed = false;
	return groups.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathNetwork::CreateLayer( int nXSize, int nYSize, const CVec2 &ptOrigin, float fAngle, int nFloor, CLayersGroup* pGroup ) 
{
	CVec2 ptXDir( cos(fAngle) * FP_GRID_STEP, sin(fAngle) * FP_GRID_STEP );
	return CreateLayer( nXSize, nYSize, ptOrigin, ptXDir, nFloor, pGroup );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathNetwork::CreateLayer( int nXSize, int nYSize, const CVec2 &ptOrigin, 
	const CVec2 &ptXDir, int nFloor, CLayersGroup* pGroup ) 
{
	CNodesLayer *pLayer = new CNodesLayer;
	int nLayer = layers.size();
	if ( nLayer >= 256 )
	{
		ASSERT(0);
		OutputDebugString("Warning! Layers number limit exceeded!\n");
		return -1;
	}
	pLayer->BuildLayer( nXSize, nYSize, ptOrigin, ptXDir, nFloor, nLayer, this, pGroup );
	layers.push_back( pLayer );
	pLayer->pColourer = new CMapColourer();
	pLayer->pColourer->SetLayer( pLayer, pMap, nLayer );
	bColouringConstructed = false;
	return nLayer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::FinishGridConstruction()
{
	pWaysCalcer = new CColouredWaysCalcer;
	for ( unsigned  int i = 0; i < groups.size(); ++i )
	{
		CLayersGroup *pGroup = groups[i];
		int nMaxX = pGroup->nXSize;
		int nMaxY = pGroup->nYSize;
		for ( int nX = 0; nX < nMaxX; nX += 8 )
		{
			for ( int nY = 0; nY < nMaxY; nY += 8 )
			{
				SPathPlace p( nX, nY, 0 ); // nLayer = 0 for this pathplace, because RefreshSpot doesn't
				                           // use layer number if pathplace is integral (i.e. not ladder)
				pGroup->RefreshSpot( p, pMap, 2 );
			}
		}
	}
	// ���������� ���� ���� ������, �.�. �� ������ ������� ����� ���� ������� ����� ����, �������
	// �������������� �� �� ����� �����������. 
	for ( unsigned int i = 0; i < groups.size(); ++i )
	{
		CLayersGroup *pGroup = groups[i];
		int nMaxX = pGroup->nXSize;
		int nMaxY = pGroup->nYSize;
		for ( int nX = 0; nX < nMaxX; nX += 8 )
		{
			for ( int nY = 0; nY < nMaxY; nY += 8 )
			{
				SPathPlace p( nX, nY, 0 ); 
				pGroup->RefreshSpot( p, pMap, 2 );
			}
		}
	}
	// finish ladders constructing
	for ( unsigned int i = 0; i < groups.size(); ++i )
		CreateLaddersInternal( groups[i] );
	// 
	for ( int i = 0; i < layers.size(); ++i )
		GetColourer(i)->ConstructColouring( this );
	for ( int i = 0; i < layers.size(); ++i )
		GetColourer(i)->AttachTransitions( this );
	bColouringConstructed = true;
	// First time kill bad HC moves
	for ( int i = 0; i < layers.size(); ++i )
	{
		CNodesLayer *pL = GetLayer( i );
		CTRect<int> region( 0, 0, pL->tiles.GetXSize(), pL->tiles.GetYSize() );
		KillBadHCMoves( &pL->tiles, region );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EDirection directionConvertTable[9] = 
{
	DOWNLEFT, DOWN, DOWNRIGHT, 
	LEFT, NONE, RIGHT,
	UPLEFT, UP, UPRIGHT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
EDirection CPathNetwork::GetDir( const SPathPlace &src, const SPathPlace &dst )
{
	SPathPlace src1;
	if ( src.IsIntegral() )
	{
		src1 = src;
	}
	else
	{
		src1 = layers[src.GetLayer()]->ladders[src.GetX()].placeOnBottom;
	}
	if ( !dst.IsIntegral() )
	{
		return (EDirection)dst.GetDirection();
	}
	if ( src1.GetLayer() == dst.GetLayer() )
	{
		ASSERT( ( src1.GetX() - dst.GetX() + 1 ) <= 2 && ( src1.GetY() - dst.GetY() + 1 ) <= 2 );
		return directionConvertTable[ (dst.GetY() - src1.GetY() + 1) * 3 + (dst.GetX() - src1.GetX() + 1) ];
	}
	else
	{
		return GetClosestDir( src, dst );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::SetOnLayer( SPosition *pRes, int nLayer, const CVec3 &pos )
{
	pRes->pNet = this;
	bool bRet = layers[nLayer]->pGroup->IsInside( CVec2( pos.x, pos.y ) );
	SPoint p = layers[nLayer]->pGroup->GetPoint( pos );
	pRes->p.SetOnLayer( nLayer, p.x, p.y );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::SetOnLayer( SUnitPosition *pRes, int nLayer, const CVec3 &pos )
{
	SetOnLayer( &pRes->pos, nLayer, pos );
	pRes->bRun = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::SetOnLayer( SObjectPosition *pRes, int nLayer, const CVec3 &pos )
{
	SetOnLayer( &pRes->pos, nLayer, pos );
	pRes->nFloor = GetLayer( nLayer )->nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::SetOnFloor( SPosition *pRes, int nFloor, const CVec3 &pos )
{
	float fBestDiff = 1e10;
	int nBestLayer;	
	for ( int i=0; i<layers.size(); ++i )
	{
		CNodesLayer *pCandidate = GetLayer( i );
	///	if ( pCandidate->nFloor != nFloor )
	///		continue;
		CLayersGroup *pGroup = pCandidate->pGroup;
		if ( !pGroup->IsInside( CVec2( pos.x, pos.y ) ) )
			continue;
		SetOnLayer( pRes, pCandidate->nLayer, pos );
		if ( pGroup->IsIgnored( pRes->p.GetX(), pRes->p.GetY() ) )
			continue;
		CVec3 ptResult = pRes->GetCP();
		float fDiffZ = fabs( pos.z - ptResult.z ); 
		if ( fDiffZ < fBestDiff )
		{
			fBestDiff = fDiffZ;
			nBestLayer = pCandidate->nLayer;
		}
	}
	if ( fBestDiff < 1000 )
	{
		SetOnLayer( pRes, nBestLayer, pos );
		return true;
	}
	// far away point may come here
	for ( int i=0; i<layers.size(); ++i )
	{
		CNodesLayer *pCandidate = layers[i];
		if ( pCandidate->nFloor != nFloor )
			continue;
		SetOnLayer( pRes, i, pos );
		return false;
	}
	ASSERT(0);
	// do not crash anyway
	SetOnLayer( pRes, 0, pos );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::HasLowerSame( const SPathPlace &p )
{
	CNodesLayer *pLayer = GetLayer( p.GetLayer() );
	CNodesLayer::STile &t = pLayer->tiles[ p.GetY() ][ p.GetX() ];
	if ( ( t.nFlags & TF_HAS_SAME ) == 0 )
		return false;
	CLayersGroup *pG = pLayer->pGroup;
	for ( int i = 0; i < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++i )
	{
		CNodesLayer *pL = pG->layers[ i ];
		if ( !pL )
			continue;
		if ( pL == pLayer )
			return false;
		CNodesLayer::STile &t2 = pL->tiles[ p.GetY() ][ p.GetX() ];
		if ( t2.nHeight == t.nHeight && t2.nDisplacement == t.nDisplacement )
			return true;
	}
	ASSERT(0);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::AddDirectTransitions( const CNodesLayer::CTransitionsHash &newtrans )
{
	// filter links and establish them
	for ( CNodesLayer::CTransitionsHash::const_iterator i = newtrans.begin(); i != newtrans.end(); ++i )
	{
		const SPathPlace &src = i->first;
		CNodesLayer *pSrc = layers[ src.GetLayer() ];
		CNodesLayer::STransitionSet res = i->second;
		if ( HasLowerSame( src ) )
			continue;
		// for every link establish 2 sided connections
		for ( list<CNodesLayer::SLink>::iterator k = res.links.begin(); k != res.links.end(); )
		{
			const SPathPlace &dst = k->dst;
			CNodesLayer *pDstLayer = layers[ dst.GetLayer() ];
			if ( !HasLowerSame( dst ) )
			{
				pDstLayer->transitions[ dst ].links.push_back( CNodesLayer::SLink( src, k->nHeight ) );
				CNodesLayer::STile &t = pDstLayer->tiles[ dst.GetY() ][ dst.GetX() ];
				t.nFlags |= TF_HAS_INTERGRID;
				++k;
			}
			else
				k = res.links.erase( k );
		}
		pSrc->transitions[src] = res;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::AddLink( const SPathPlace &p, CNodesLayer *pLayer, vector<SShowLink> *pLinks, 
	int nDir, const CTPoint<int> &shift, char nMoveFlags, SShowLink::EType type )
{
	if ( nMoveFlags & (1<<nDir) )
	{
		SPathPlace p1( p.GetX() + shift.x, p.GetY() + shift.y, p.GetLayer(), p.GetDirection(), p.GetPose(), p.IsMoving() );
		pLayer->pGroup->RefreshSpot( p1, pMap, 1 );
		if ( type != SShowLink::HEIGHT_CHANGE )
			pLinks->push_back( SShowLink( pLayer->GetCP(p), pLayer->GetCP(p1) + CVec3(0.01f,0.02f,0.01f), type ) );
		else // height change
		{
			pLinks->push_back( SShowLink( pLayer->GetCP(p), pLayer->GetCP(p1), type ) );
			// add second line if height change is in proper limits
			CNodesLayer::STile &t = pLayer->tiles[ p.GetY() ][ p.GetX() ];
			CNodesLayer::STile &t1 = pLayer->tiles[ p1.GetY() ][ p1.GetX() ];
			float fHDiff = GetFHeight( t1.nHeight ) - GetFHeight( t.nHeight );
			if ( fHDiff <= F_MAX_CLIMB_HEIGHT && fHDiff >= -F_MAX_CLIMB_HEIGHT )
				pLinks->push_back( SShowLink( pLayer->GetCP(p) + CVec3( 0.03f, 0, 0 ), pLayer->GetCP(p1) + CVec3( 0.03f, 0, 0 ), type ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetNetworkFragment(
	const SPosition &pos, bool bOneColorOnly, vector<SShowPoint> *pKnots, vector<SShowLink> *pLinks )
{
	ASSERT( pos.pNet == this );
	int nX = pos.p.GetX();
	int nY = pos.p.GetY();
	CTRect<int> region( nX - 8, nY - 8, nX + 8, nY + 8 );
	CNodesLayer *pLayer = layers[pos.p.GetLayer()];
	region.minx = Max( 0, region.minx );
	region.miny = Max( 0, region.miny );
	region.maxx = Min( pLayer->tiles.GetXSize() - 1, region.maxx );
	region.maxy = Min( pLayer->tiles.GetYSize() - 1, region.maxy );
	CMapColourer *pColourer = GetColourer( pos.p.GetLayer() );
	if ( bOneColorOnly && ( !pColourer->IsReady() ) )
		return; 
	WORD wColor;
	if ( bOneColorOnly )
		wColor = pColourer->GetPointColor( (unsigned char)pos.p.GetX(), (unsigned char)pos.p.GetY() );
	unsigned char cX, cY;
	if ( bOneColorOnly )
		pColourer->GetColorCenter( wColor, &cX, &cY );
	CLayersGroup *pGroup = pLayer->pGroup;
	CVec2 toX2 = 0.3f * ( pGroup->GetCPNoHeight( 1, 0 ) - pGroup->GetCPNoHeight( 0, 0 ) );
	CVec2 toY2 = 0.3f * ( pGroup->GetCPNoHeight( 0, 1 ) - pGroup->GetCPNoHeight( 0, 0 ) );
	for ( int y = region.miny; y <= region.maxy; ++y )
	{
		for ( int x = region.minx; x <= region.maxx; ++x )
		{
			if ( bOneColorOnly )
			{
				WORD wCurrColor = pColourer->GetPointColor( x, y );
				if ( wCurrColor != wColor ) 
					continue;
			}
			SPathPlace p( x, y, pLayer->nLayer ); // required for successful transitions search
			pLayer->pGroup->RefreshSpot( p, pMap, 2 );
			const CNodesLayer::STile &tile = pLayer->tiles[y][x];
			CVec3 pos = pLayer->GetCP( p );
			int nFlags = pLayer->GetLocks( x, y ) ? SShowPoint::LOCKED : 0;
			if ( tile.nPassable & CP_STAND )
				nFlags |= SShowPoint::CAN_STAND;
			if ( tile.nPassable & CP_LAY )
				nFlags |= SShowPoint::CAN_LAY;
			if ( tile.nPassable & CP_CROUCH )
				nFlags |= SShowPoint::CAN_CROUCH;

			if ( tile.nPassable & CP_INACTIVE )
			{
				//ASSERT( tile.nPassable == CP_INACTIVE);			// CRAP - temporary
				nFlags |= SShowPoint::BOW_LEGGED;
			}
			if ( nFlags == SShowPoint::LOCKED )
			{
				nFlags = 0;
			}
			if ( bOneColorOnly )
			{
			//	if ( ( y == cY ) && ( x == cX ) )
					nFlags |= SShowPoint::COLOR_CENTER;
			}
			if ( nFlags )
				pKnots->push_back( SShowPoint( pos, nFlags ) );
			if ( tile.nPassable ) 
			{
				if ( tile.nFlags & TF_HAS_SAME )
				{
					float nSM = 0;
					CVec3 thisPoint = pLayer->GetCP( p );
					vector<SPathPlace> sames;
					pLayer->GetSamePoints( p, &sames );
					for ( vector<SPathPlace>::iterator i = sames.begin(); i != sames.end(); ++i)
					{
						pLinks->push_back( SShowLink( thisPoint, thisPoint + CVec3( nSM, 0, 0.5 ), SShowLink::DIRECT ) );
						nSM += 0.03f;
					}
				}
				char nAllMoves = tile.nMoveStand & tile.nMoveLay & tile.nMoveCrouch;
				AddLink( p, pLayer, pLinks, CD_R, CTPoint<int>(1,0), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_RU, CTPoint<int>(1,1), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_U, CTPoint<int>(0,1), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_LU, CTPoint<int>(-1,1), nAllMoves, SShowLink::ANY_MOVE );

				AddLink( p, pLayer, pLinks, CD_L, CTPoint<int>(-1,0), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_LD, CTPoint<int>(-1,-1), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_D, CTPoint<int>(0,-1), nAllMoves, SShowLink::ANY_MOVE );
				AddLink( p, pLayer, pLinks, CD_RD, CTPoint<int>(1,-1), nAllMoves, SShowLink::ANY_MOVE );
				char nCurrentFlags = tile.nMoveStand & ( ~nAllMoves );
				if ( nCurrentFlags )
				{
					AddLink( p, pLayer, pLinks, CD_R, CTPoint<int>(1,0), nCurrentFlags, SShowLink::STAND_MOVE );
					AddLink( p, pLayer, pLinks, CD_RU, CTPoint<int>(1,1), nCurrentFlags, SShowLink::STAND_MOVE );
					AddLink( p, pLayer, pLinks, CD_U, CTPoint<int>(0,1), nCurrentFlags, SShowLink::STAND_MOVE );
					AddLink( p, pLayer, pLinks, CD_LU, CTPoint<int>(-1,1), nCurrentFlags, SShowLink::STAND_MOVE );
				}
				nCurrentFlags = tile.nMoveLay & ( ~nAllMoves );
				if ( nCurrentFlags )
				{
					AddLink( p, pLayer, pLinks, CD_R, CTPoint<int>(1,0), nCurrentFlags, SShowLink::CRAWL_MOVE );
					AddLink( p, pLayer, pLinks, CD_RU, CTPoint<int>(1,1), nCurrentFlags, SShowLink::CRAWL_MOVE );
					AddLink( p, pLayer, pLinks, CD_U, CTPoint<int>(0,1), nCurrentFlags, SShowLink::CRAWL_MOVE );
					AddLink( p, pLayer, pLinks, CD_LU, CTPoint<int>(-1,1), nCurrentFlags, SShowLink::CRAWL_MOVE );
				}
				nCurrentFlags = tile.nMoveCrouch & ( ~nAllMoves );
				if ( nCurrentFlags )
				{
					AddLink( p, pLayer, pLinks, CD_R, CTPoint<int>(1,0), nCurrentFlags, SShowLink::CROUCH_MOVE );
					AddLink( p, pLayer, pLinks, CD_RU, CTPoint<int>(1,1), nCurrentFlags, SShowLink::CROUCH_MOVE );
					AddLink( p, pLayer, pLinks, CD_U, CTPoint<int>(0,1), nCurrentFlags, SShowLink::CROUCH_MOVE );
					AddLink( p, pLayer, pLinks, CD_LU, CTPoint<int>(-1,1), nCurrentFlags, SShowLink::CROUCH_MOVE );
				}
				AddLink( p, pLayer, pLinks, CD_R, CTPoint<int>(1,0), tile.nMoveHC, SShowLink::HEIGHT_CHANGE );
				AddLink( p, pLayer, pLinks, CD_U, CTPoint<int>(0,1), tile.nMoveHC, SShowLink::HEIGHT_CHANGE );
				SPathPlace p1( 0, 0, pLayer->nLayer );
				SPathPlace p2( 1, 0, pLayer->nLayer );
				SPathPlace p3( 0, 1, pLayer->nLayer );
				CVec3 toX( toX2, 0 );
				CVec3 toY( toY2, 0 );
				CVec3 smallZ( 0, 0, 0.05f );
				if ( tile.nPassable & CP_LAY1 ) 
					pLinks->push_back( SShowLink( pLayer->GetCP(p) + toX + smallZ, pLayer->GetCP(p) - toX + smallZ, SShowLink::LAY_POSE_SHOW ) );
				if ( tile.nPassable & CP_LAY2 ) 
					pLinks->push_back( SShowLink( pLayer->GetCP(p) + toX + toY + smallZ, pLayer->GetCP(p) - toX - toY + smallZ, SShowLink::LAY_POSE_SHOW ) );
				if ( tile.nPassable & CP_LAY3 ) 
					pLinks->push_back( SShowLink( pLayer->GetCP(p) + toY + smallZ, pLayer->GetCP(p) - toY + smallZ, SShowLink::LAY_POSE_SHOW ) );
				if ( tile.nPassable & CP_LAY4 ) 
					pLinks->push_back( SShowLink( pLayer->GetCP(p) + toX - toY + smallZ, pLayer->GetCP(p) + toY - toX + smallZ, SShowLink::LAY_POSE_SHOW ) );
					
			}
			CNodesLayer::CTransitionsHash::iterator t = pLayer->transitions.find( p );
			if ( t != pLayer->transitions.end() )
			{
				for ( list<CNodesLayer::SLink>::iterator k = t->second.links.begin(); k != t->second.links.end(); ++k )
				{
					const SPathPlace &res = k->dst;
					CNodesLayer *pResLayer = layers[res.GetLayer()];
					pLinks->push_back( SShowLink( pLayer->GetCP( p ), pResLayer->GetCP( res ), SShowLink::DIRECT ) );
					ASSERT( p.GetLayer() != res.GetLayer() );
					// test
					CNodesLayer::CTransitionsHash::iterator test = pResLayer->transitions.find( res );
					ASSERT( test != pResLayer->transitions.end() );
					if ( test != pResLayer->transitions.end() )
						ASSERT( test->second.GetLink( p ) != test->second.links.end() );
				}
				////
			}
			CNodesLayer::CLadderHash::iterator l = pLayer->ladderEntrances.find( p );
			if ( l != pLayer->ladderEntrances.end() )
			{
				CNodesLayer::SLadderTransition &lt = l->second;
				if ( lt.bUpper )
				{
					CNodesLayer *pOther = GetLayer( lt.nLayerGroup );
					CNodesLayer::SLadder &ladder = pOther->ladders[ lt.nLadder ];
					SPathPlace &to = ladder.placeOnBottom;
					pLinks->push_back( SShowLink( pLayer->GetCP( p ), pOther->GetCP( to ), SShowLink::DIRECT ) );
				}
				else
				{
					CNodesLayer::SLadder &ladder = pLayer->ladders[ lt.nLadder ];
					SPathPlace &to = ladder.placeOnBottom;
					pLinks->push_back( SShowLink( pLayer->GetCP( p ), pLayer->GetCP( to ), SShowLink::DIRECT ) );
					for ( int ladStep = 0; ladStep < ladder.GetHeight(); ++ladStep )
					{
						SPathPlace p2;
						p2.SetOnLayer( pLayer->nLayer, lt.nLadder, ladStep, 0 );
						int nFlags = ladder.nLocks[ ladStep ] ? SShowPoint::LOCKED : 0 ;
						if ( ladder.pointPassable[ ladStep ] )
							nFlags |= ( SShowPoint::EVERY_POSE );
						CVec3 pos = GetCP( p2 );
						if ( nFlags )
						{
							pKnots->push_back( SShowPoint( pos, nFlags ) );
						}
						if ( ladStep < ladder.GetHeight() - 1 && ladder.pointPassable[ ladStep + 1 ] )
						{
							SPathPlace p3;
							p3.SetOnLayer( pLayer->nLayer, lt.nLadder, ladStep + 1, 0 );
							pLinks->push_back( SShowLink( pos, pLayer->GetCP( p3 ), SShowLink::ANY_MOVE ) );
						}
						if ( ladStep == ladder.GetHeight() - 4 && ladder.pointPassable[ ladStep ] )
						{
							pLinks->push_back( SShowLink( pos, GetCP( ladder.placeOnTop ), SShowLink::DIRECT ) );
						}
					}
				}
			}
			/*int nL, nLad;
			if ( pLayer->GetLadder( &nLad, &nL, p ) )
			{
				if ( nL == p.GetLayer() )
				{
					CNodesLayer::SLadder &l = pLayer->ladders[nLad];
					pLinks->push_back( SShowLink( GetCP( l.placeOnBottom ), GetCP( l.points[0]), SShowLink::DIRECT));
					pKnots->push_back( SShowPoint( GetCP( l.placeOnBottom ), SShowPoint::NORMAL ) );
					for ( int j = 0; j < l.points.size() - 1; ++j)
					{
						pLinks->push_back( SShowLink( GetCP( l.points[j] ), GetCP( l.points[j+1] ), SShowLink::DIRECT));
						pKnots->push_back( SShowPoint( GetCP( l.points[j] ), SShowPoint::NORMAL ) );
					}
					pLinks->push_back( SShowLink( GetCP( l.points.back() ), GetCP( l.end ), SShowLink::DIRECT));
					pKnots->push_back( SShowPoint( GetCP( l.points.back() ), SShowPoint::NORMAL ) );
					pKnots->push_back( SShowPoint( GetCP( l.end ), SShowPoint::NORMAL ) );
				}
			}*/
		}
	}
	if ( bOneColorOnly )
	{
		// draw part of nodes neighbours net
		vector<unsigned char> xCoords, yCoords;
		vector<int> layerNumbers;
		pColourer->GetColorNeighbourCenters( this, wColor, &xCoords, &yCoords, &layerNumbers, PM_ANY_MOVE );
		for ( unsigned int i = 0; i < xCoords.size(); ++i )
		{
			SPathPlace res( xCoords[ i ], yCoords[ i ], layerNumbers[ i ] );
			SPathPlace p( cX, cY, pLayer->nLayer );
			CNodesLayer *pResLayer = GetLayer( layerNumbers[ i ] );
			//if ( layerNumbers[ i ] != pLayer->nLayer ) 
			//	continue;
			pLinks->push_back( SShowLink( pLayer->GetCP( p ), pResLayer->GetCP( res ), 
				SShowLink::NEIGHBOUR_ZONE_ANY_MOVE ) );
		}
		xCoords.clear(); 
		yCoords.clear();
		layerNumbers.clear();
		pColourer->GetColorNeighbourCenters( this, wColor, &xCoords, &yCoords, &layerNumbers, PM_STAND_ONLY );
		for ( unsigned int i = 0; i < xCoords.size(); ++i )
		{
			SPathPlace res( xCoords[ i ], yCoords[ i ], layerNumbers[ i ] );
			SPathPlace p( cX, cY, pLayer->nLayer );
			CNodesLayer *pResLayer = GetLayer( layerNumbers[ i ] );
			//if ( layerNumbers[ i ] != pLayer->nLayer ) 
			//	continue;
			pLinks->push_back( SShowLink( pLayer->GetCP( p ), pResLayer->GetCP( res ), 
				SShowLink::NEIGHBOUR_ZONE_STAND_ONLY ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::PrintLayerInfo( int nLayer )
{
	char buf[80];
	if ( !bColouringConstructed )
		FinishGridConstruction();
	CNodesLayer *pLayer = GetLayer( nLayer );
	CLayersGroup *pGroup = pLayer->pGroup;
	for ( int i = 0; i < N_MAX_FLOORS; ++i )
	{
		for ( int j = 0; j < N_MAX_LAYERS_PER_FLOOR; ++j )
		{
			if ( pGroup->layers[ i * N_MAX_LAYERS_PER_FLOOR + j ] == pLayer )
			{
				sprintf_s( buf, " This layer lays on group address %p, floor %d, layer #%d\n", pGroup, i, j );
				csSystem << CC_WHITE << buf;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::PrintConsoleInfo( SPathPlace p )
{
	char buf[80];
	if ( !bColouringConstructed )
		FinishGridConstruction();
	CMapColourer *pC = GetColourer( p.GetLayer() );
	int color = pC->GetPointColor((unsigned char)p.GetX(), (unsigned char)p.GetY() );
	sprintf_s( buf, " color = %d, x = %d, y = %d \n", color, p.GetX(), p.GetY() );
	csSystem << CC_RED << buf;
	CNodesLayer *pLayer = GetLayer( p.GetLayer() );
	vector<SPathPlace> sames;
	pLayer->GetSamePoints( p, &sames );
	for ( vector<SPathPlace>::iterator i = sames.begin(); i != sames.end(); ++i )
	{
		SPathPlace pp = *i;
		pC = GetColourer( i->GetLayer() );
		color = pC->GetPointColor((unsigned char)i->GetX(), (unsigned char)i->GetY() );
		sprintf_s( buf, "trSame to l = %d, color = %d, x = %d, y = %d\n", i->GetLayer(), color, i->GetX(), i->GetY() );
		csSystem << CC_RED << buf;
	}	

	SPathPlace search( p.GetX(), p.GetY(), p.GetLayer() );
	CNodesLayer::CTransitionsHash::iterator t = pLayer->transitions.find( search );
	if ( t != pLayer->transitions.end() )
	{
		CNodesLayer::STransitionSet &trS = t->second;
		for ( list<CNodesLayer::SLink>::iterator i = trS.links.begin(); i != trS.links.end(); ++i )
		{
			SPathPlace pp = i->dst;
			pC = GetColourer( pp.GetLayer() );
			color = pC->GetPointColor((unsigned char)pp.GetX(), (unsigned char)pp.GetY() );
			sprintf_s( buf, "trLink to l = %d, color = %d, x = %d, y = %d\n", pp.GetLayer(), color, pp.GetX(), pp.GetY() );
			csSystem << CC_RED << buf;
		}
	}
	SNet *pNet;
	pC = GetColourer( p.GetLayer() );
	color = pC->GetPointColor((unsigned char)p.GetX(), (unsigned char)p.GetY() );
	pNet = &pC->zonesAnyMove;
	list<SNeighbour> &ns = pNet->nodes[color].neighbours;
	for (list<SNeighbour>::iterator i = ns.begin(); i != ns.end(); ++i) 
	{
		int nL = i->nLayer, nNN = i->wNodeNumber, nD = i->wDistance;
		sprintf_s( buf, "Global neighbour l=%d, clr=%d, distance=%d\n", nL, nNN, nD );
		csSystem << CC_GREEN << buf;
	}
	int wDistanceToCenter = pC->pointDistancesAnyMove.GetCost( CSquareMapCosts::SPosition((unsigned char)p.GetX(), (unsigned char)p.GetY() ) );
	sprintf_s( buf, "Distance to center = %d\n", wDistanceToCenter );
	csSystem << CC_RED << buf;
	CNodesLayer::STile &tile = pLayer->tiles[p.GetY()][p.GetX()];
	sprintf_s( buf, "Tile height = %d\n", tile.nHeight );
	csSystem << CC_RED << buf;
	sprintf_s( buf, "Tile is passable in inactive = %d\n", tile.nPassable & CP_INACTIVE );
	csSystem << CC_GREEN << buf;
	sprintf_s( buf, "Tile is passable in lay = %d\n", tile.nPassable & CP_LAY );
	csSystem << CC_GREEN << buf;
	CVec3 ptCP = GetCP( p );
	sprintf_s( buf, "Tile CP = %f %f %f\n", ptCP.x, ptCP.y, ptCP.z );
	csSystem << CC_RED << buf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::CreateAlternativeGrids( const SAlternativeGridInfo &root )
{
	CLayersGroup *pRoot = groups[ root.nLayersGroup ];
	for ( unsigned int i = 0; i < root.children.size(); ++i )
	{	
		CLayersGroup *pGroup = groups[ root.children[i].nLayersGroup ];
		for ( int nFloor = 0; nFloor < N_MAX_FLOORS; ++nFloor )
		{
			CVec2 ptOrigin( pGroup->ptOrigin );
			CVec2 ptXDir( pGroup->ptXDir );
			CVec2 ptRSize( pGroup->nXSize - 1, pGroup->nYSize - 1 );
			CLayersGroup::SIgnoreRect ignoreRect( ptOrigin, ptXDir, ptRSize );
			pRoot->ignoreRects.push_back( ignoreRect );
		}
		SAlternativeGridInfo nested = root.children[i];
		//nested.nLayersGroup = root.nLayersGroup;
		CreateAlternativeGrids( nested );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathNetwork::GetFloor( int nLayer ) const
{
	return layers[nLayer]->nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetNearPlaces( const SSphere &s, vector<SPathPlace> *pRes, bool bTakeAll )
{
	pRes->clear();
	for ( unsigned int i = 0; i < layers.size(); ++i )
		layers[i]->AddNearPoints( s, pRes, bTakeAll );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CPathNetwork::ForceLayersRecalc( const SSphere &s )
{
	for ( int i = 0; i < layers.size(); ++i )
		layers[i]->ForceRecalc( s );
	for ( int i = 0; i < layers.size(); ++i )
	{
		CTRect<int> r;
		layers[i]->GetRecalcRect( s, &r );
		r.minx *= N_RECALC_GRID_SIZE;
		++r.maxx;
		++r.maxy;
		r.maxx *= N_RECALC_GRID_SIZE;
		r.miny *= N_RECALC_GRID_SIZE;
		r.maxy *= N_RECALC_GRID_SIZE;
		r.maxx = min( r.maxx, layers[i]->tiles.GetXSize() );
		r.maxy = min( r.maxy, layers[i]->tiles.GetYSize() );
		GetColourer( i )->AddZoneToRecalc( CTRect<char>( r.minx, r.miny, r.maxx, r.maxy ) );
	}
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetLockAreaInternal( vector<SPathPlace> *pRes, const SPathPlace &p ) const
{
	CNodesLayer *pLayer = GetLayer( p.GetLayer() );
	// intergrid same
	pLayer->GetSamePoints( p, pRes );	
	// other intergrid locks
/*	CNodesLayer::CTransitionsHash &trHash = pLayer->transitions;
	CNodesLayer::CTransitionsHash::iterator i = trHash.find( SPathPlace( p.GetX(), p.GetY(), p.GetLayer() ) );
	if ( i != trHash.end() )
	{
		CNodesLayer::STransitionSet &transitions = i->second;
		for ( list<CNodesLayer::SLink>::iterator i = transitions.links.begin(); i != transitions.links.end(); ++i )
		{
			pRes->push_back( i->dst );
		}
	}*/
	pRes->push_back( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetLockAreaInternal( vector<SPathPlace> *pRes, const SPathPlace &p, int nDX, int nDY ) const
{
	CNodesLayer *pLayer = GetLayer( p.GetLayer() );
	// add "intergrid same" locks
	if ( p.GetX() + nDX < 0 || p.GetY() + nDY < 0 )
		return;
	if ( p.GetX() + nDX >= pLayer->tiles.GetXSize() || p.GetY() + nDY >= pLayer->tiles.GetYSize() )
		return;
	// intergrid-same locks end here
	SPathPlace pToAdd( p );
	pToAdd.SetXY( p.GetX() + nDX, p.GetY() + nDY );
	pRes->push_back( pToAdd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::GetLockArea( vector<SPathPlace> *pRes, const SPathPlace &p, bool bBigUnit ) const
{
	bool bManyTiles = bBigUnit;
	if ( p.GetPose() == CM_LAY )
		bManyTiles = true;
	if ( bManyTiles )
	{
		int nDX = nMoveShift[p.GetDirection()][0], nDY = nMoveShift[p.GetDirection()][1];
		GetLockAreaInternal( pRes, p );
		GetLockAreaInternal( pRes, p, nDX, nDY );
		GetLockAreaInternal( pRes, p, -nDX, -nDY );
		if ( nDX != 0 && nDY != 0 ) // diagonal 
		{
			GetLockAreaInternal( pRes, p, 0, nDY );
			GetLockAreaInternal( pRes, p, 0, -nDY );
			GetLockAreaInternal( pRes, p, nDX, 0 );
			GetLockAreaInternal( pRes, p, -nDX, 0 );
		}
	}
	else 
		GetLockAreaInternal( pRes, p );
	if ( !p.IsIntegral() && p.GetLadderStep() < 4 )
	{
		CNodesLayer::SLadder &ladder = GetLayer( p.GetLayer() )->ladders[ p.GetX() ];
		GetLockAreaInternal( pRes, ladder.placeOnBottom );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsLocked( const SPathPlace &p, bool bIgnoreBlockedDoors ) const
{
	int nX, nY;
	if ( p.IsIntegral() )
	{
		nX = p.GetX();
		nY = p.GetY();
		if ( layers[p.GetLayer()]->GetLocks( nX, nY ) != 0 )
			return true;
		if ( bIgnoreBlockedDoors )
			return false;
		int nFlipper = layers[p.GetLayer()]->tiles[nY][nX].nFlipper;
		if ( !nFlipper )
			return false;
		const SFlipper *pFlipper = GetFlipper( nFlipper - 1 );
		if ( pFlipper->nFixedFlags == 0 )
			return false;
		return !IsNotOnDoor( p );
	}
	else
	{
		return layers[p.GetLayer()]->ladders[p.GetX()].nLocks[ p.GetY() ] != 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsBigLockerLocked( const SPathPlace &p, EBigLockerType type ) const
{
	ASSERT( p.IsIntegral() );
	ASSERT( type == BL_PANZERKLEINE );
	CNodesLayer *pLayer = GetLayer( p.GetLayer() );
	int nMinX = p.GetX() - 1, nMinY = p.GetY() - 1, nMaxX = p.GetX() + 2, nMaxY = p.GetY() + 2;
	nMinX = Max( 0, nMinX );
	nMinY = Max( 0, nMinY );
	nMaxX = Min( pLayer->tiles.GetXSize(), nMaxX );
	nMaxY = Min( pLayer->tiles.GetYSize(), nMaxY );
	for ( int nX = nMinX; nX < nMaxX; ++nX )
		for ( int nY = nMinY; nY < nMaxY; ++nY )
			if ( pLayer->GetLocks( nX, nY ) != 0 )
				return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void LockUnlockChar( char *pLocks, bool bLock )
{
	if ( bLock ) 
		*pLocks = 1;
	else  
		*pLocks = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::LockUnlockPoint( const SPathPlace &p, bool bLock )
{
	char *pLocks;
	if ( p.IsIntegral() )
	{
		pLocks = &layers[p.GetLayer()]->tiles[p.GetY()][p.GetX()].nLocks;
		LockUnlockChar( pLocks, bLock );
	}
	else // not integral
	{
		CNodesLayer::SLadder &ladder = GetLayer( p.GetLayer()	)->ladders[ p.GetX() ];
		int nMaxStep = min( p.GetLadderStep() + 4, ladder.GetHeight() );
		for ( int nStep = p.GetLadderStep(); nStep < nMaxStep; ++nStep )
			LockUnlockChar( &ladder.nLocks[ nStep ], bLock );
	}
	/*char buf[128];
	if ( bLock )
		sprintf( buf, "Locking x: %d, y: %d, layer: %d\n", p.GetX(), p.GetY(), p.GetLayer() );
	else
		sprintf( buf, "Unlock' x: %d, y: %d, layer: %d\n", p.GetX(), p.GetY(), p.GetLayer() );
	OutputDebugString(buf);*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::Lock( CObjectBase *pUnit, const SPathPlace &p )
{
	CLocksHash::iterator i = lockedPlaces.find( pUnit );
	if ( i != lockedPlaces.end() )
		DisableLocks( pUnit );
	//if ( pUnit->IsDead() )
	//	return;
	vector<SPathPlace> res;
	bool bBigUnit = IsBigLocker( pUnit );
	GetLockArea( &res, p, bBigUnit );
	for ( unsigned int i = 0; i < res.size(); ++i )
	{
		const SPathPlace &l = res[i];
		LockUnlockPoint( l, true );
	}
	lockedPlaces[pUnit] = SLockInfo( res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::Lock( CObjectBase *pUnit, const vector<SPathPlace> &places )
{
	CLocksHash::iterator i = lockedPlaces.find( pUnit );
	for ( unsigned int i = 0; i < places.size(); ++i )
	{
		const SPathPlace &l = places[i];
		LockUnlockPoint( l, true );
	}
	lockedPlaces[pUnit] = SLockInfo( places );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::LockMovingObject( CObjectBase *pUnit, const SPathPlace &p1, const SPathPlace &p2 )
{
	CLocksHash::iterator i = lockedPlaces.find( pUnit );
	if ( i != lockedPlaces.end() )
		DisableLocks( pUnit );
	//if ( pUnit->IsDead() )
	//	return;
	vector<SPathPlace> res;
	bool bBigUnit = IsBigLocker( pUnit );
	GetLockArea( &res, p1, bBigUnit );
	GetLockArea( &res, p2, bBigUnit );
	for ( unsigned int i = 0; i < res.size(); ++i )
	{
		const SPathPlace &l = res[i];
		LockUnlockPoint( l, true );
	}
	lockedPlaces[pUnit] = SLockInfo( res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::Unlock( CObjectBase *pUnit )
{
	CLocksHash::iterator i = lockedPlaces.find( pUnit );
	if ( i != lockedPlaces.end() )
	{
		DisableLocks( pUnit );
		ASSERT( i->second.bLocked == false );
		lockedPlaces.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::DisableLocks( CObjectBase *pUnit )
{
	CLocksHash::iterator i = lockedPlaces.find( pUnit );
	if ( i != lockedPlaces.end() && i->second.bLocked == true )
	{
		vector<SPathPlace> &res = i->second.places;
		for ( unsigned int i = 0; i < res.size(); ++i )
		{
			const SPathPlace &l = res[i];
			LockUnlockPoint( l, false );
		}
		i->second.bLocked = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::LockUnlockDynamic( const SPathPlace &p, bool bLock, bool bBigUnit )
{
	vector<SPathPlace> res;
	GetLockArea( &res, p, bBigUnit );
	for ( vector<SPathPlace>::iterator i = res.begin(); i != res.end(); ++i )
	{
		if ( i->IsIntegral() )
		{
			char *pLocks = &layers[p.GetLayer()]->tiles[p.GetY()][p.GetX()].nDynLocks;
			LockUnlockChar( pLocks, bLock );
		}
		// no else, because we currently don't need dynamic locks for ladders
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::ClearDynamicLocks( CObjectBase *pUnit )
{
	CDynLocksHash::iterator i =	dynLockedPlaces.find( pUnit );
	bool bBigUnit = IsBigLocker( pUnit );
	if ( i != dynLockedPlaces.end() )
	{
		SDynLockInfo &info = i->second;
		if ( !info.bLocked )
			return;
		for ( vector<SPathPlace>::iterator j = info.points.begin(); j != info.points.end(); ++j )
		{
			LockUnlockDynamic( *j, false, bBigUnit );
		}
		info.bLocked = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::RestoreDynamicLocks( CObjectBase *pUnit )
{
	CDynLocksHash::iterator i =	dynLockedPlaces.find( pUnit );
	bool bBigUnit = IsBigLocker( pUnit );
	if ( i != dynLockedPlaces.end() )
	{
		SDynLockInfo &info = i->second;
		if ( info.bLocked )
			return;
		for ( vector<SPathPlace>::iterator j = info.points.begin(); j != info.points.end(); ++j )
		{
			LockUnlockDynamic( *j, true, bBigUnit );
		}
		info.bLocked = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::ChangeDynamicLocks( CObjectBase *pUnit, const vector<SPathPlace> &points )
{
	SPathPlace old;
	dynLockedPlaces[ pUnit ].points.clear();
	dynLockedPlaces[ pUnit ].bLocked = true;
	bool bBigUnit = IsBigLocker( pUnit );
	for ( vector<SPathPlace>::const_iterator j = points.begin(); j != points.end(); ++j )
	{
		if ( !j->IsIntegral() )
			continue;
		SPathPlace p( j->GetX(), j->GetY(), j->GetLayer() );
		if ( ! (p == old) )
		{
			LockUnlockDynamic( p, true, bBigUnit );
			old = p;
			dynLockedPlaces[ pUnit ].points.push_back( p );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::LockSelected( const list<CObjectBase*> &selected )
{
	for ( CLocksHash::iterator i = lockedPlaces.begin(); i != lockedPlaces.end(); ++i )
	{
		DisableLocks( i->first );
	}
	for ( list<CObjectBase*>::const_iterator k = selected.begin(); k != selected.end(); ++k )
	{
		CObjectBase *pLocker = *k;
		CLocksHash::iterator i = lockedPlaces.find( pLocker );
		if ( i != lockedPlaces.end() )
			Lock( i->first, i->second.places );
		else // trapped door
		{
			CDynamicCast<NWorld::IWindowDoor> pDoor(pLocker);
			if (pDoor)
			{
				SFlipper *pFlipper = GetFlipper( pLocker );
				pFlipper->nFixedFlags |= F_VISIBLE_TRAP;
			}
			else
			{
				CDynamicCast<NWorld::CMine> pMine(pLocker);
				if (pMine)
				{
					//mineTempLocks.push_back( pMine->GetPlace().p );
					//LockUnlockPoint( pMine->GetPlace().p, true );
				}
				else
				{
					//ASSERT(0);
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::UnlockSelected()
{
	for ( CLocksHash::iterator i = lockedPlaces.begin(); i != lockedPlaces.end(); ++i )
	{
		//if ( !i->second.bLocked )
			Lock( i->first, i->second.places );
	}
	for ( vector<SFlipper>::iterator it = flippers.begin(); it != flippers.end(); ++it )
		it->nFixedFlags &= ~F_VISIBLE_TRAP;
	for ( vector<SPathPlace>::iterator it = mineTempLocks.begin(); it != mineTempLocks.end(); ++it )
		LockUnlockPoint( *it, false );
	mineTempLocks.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CPathNetwork::GetWhoLocksThisPlace( const SPathPlace &p ) const
{
	for ( CLocksHash::const_iterator i = lockedPlaces.begin(); i != lockedPlaces.end(); ++i )
	{
		if ( i->second.bLocked )
		{
			const vector<SPathPlace> &places = i->second.places;
			for ( vector<SPathPlace>::const_iterator ip = places.begin(); ip != places.end(); ++ip )
				if ( ip->GetLayer() == p.GetLayer() && ip->GetX() == p.GetX() 
					&& ip->GetY() == p.GetY() && ip->IsIntegral() == p.IsIntegral() )
					return i->first;
		}
	}
	//ASSERT(0); 
	// nobody does? this function must be used only when we know truly that this place is locked, 
	// because it's expencive
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsValidDestination( const SPathPlace &p )
{
	// CRAP: 
	if ( p.GetLayer() >= layers.size() || p.GetLayer() < 0 )
	{
		ASSERT( 0 && "Never create move executors with default SPosition()! " );
		return false;
	}
	CNodesLayer *pLayer = layers[p.GetLayer()];
	if ( !p.IsIntegral() )
	{
		CNodesLayer::SLadder &ladder = pLayer->ladders[ p.GetX() ];
		if ( ladder.pointPassable[ p.GetLadderStep() ] == false )
			return false;
		return ladder.nLocks[ p.GetLadderStep() ] == 0;
	}
	pLayer->pGroup->RefreshSpot( p, pMap, 2 );
	const CNodesLayer::STile &t = pLayer->tiles[p.GetY()][p.GetX()];
	if ( t.nPassable == 0 )
		return false;
	if ( t.nLocks != 0 )
		return false;
	if ( t.nMoveStand == 0 && t.nMoveCrouch == 0 && t.nMoveLay == 0 && t.nMoveHC == 0 )
	{
		SPathPlace transSearch( p.GetX(), p.GetY(), p.GetLayer() );
		if ( pLayer->transitions.find( transSearch ) == pLayer->transitions.end() )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsTilePassable( const SPathPlace &p, const STile &t )
{
	switch ( p.GetPose() )
	{
		case CM_LAY:      
		{
			int dir = p.GetDirection() & 3;
			return ( t.nPassable & (CP_LAY1 << dir ) ) != 0;
		}
		case CM_CROUCH:   return (t.nPassable & CP_CROUCH) != 0;
		case CM_STAND:    return (t.nPassable & CP_STAND) != 0;
		case CM_INACTIVE: return (t.nPassable & CP_INACTIVE) != 0;
	}
	ASSERT( 0 );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsNotOnDoor( const SPathPlace &p ) const
{
	CNodesLayer *pLayer = layers[p.GetLayer()];
	pLayer->pGroup->RefreshSpot( p, pMap, 1 );
	if ( ( p.GetX() >= pLayer->tiles.GetXSize() ) || ( p.GetY() >= pLayer->tiles.GetYSize() ) )
		return false;
	const CNodesLayer::STile &t = pLayer->tiles[p.GetY()][p.GetX()];
	if ( t.nFlipper )
	{
		const SFlipper &fl = *GetFlipper( t.nFlipper - 1 );
		const unordered_map<SPathPlace, CNodesLayer::STile, SPathPlaceHash> *pHash;
		if ( fl.bOpen )
			pHash = &fl.locksOpen;
		else
			pHash = &fl.locksClosed;
		SPathPlace test( p.GetX(), p.GetY(), p.GetLayer() );
		unordered_map<SPathPlace, CNodesLayer::STile, SPathPlaceHash>::const_iterator it = pHash->find( test );
		if ( it != pHash->end() )
			return IsTilePassable( p, it->second );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsPassable( const SPathPlace &p )
{
	if ( !p.IsIntegral() )
	{
		// ladder...
		CNodesLayer::SLadder &ladder = GetLayer( p.GetLayer() )->ladders[ p.GetX() ];
		GetLayer( p.GetLayer() )->RefreshLadder( p.GetX(), pMap );
		ASSERT( p.GetY() < ladder.GetHeight() );
		return ladder.pointPassable[ p.GetLadderStep() ] && ( ladder.nLocks[ p.GetLadderStep() ] == 0 );
	}

	// Match release CPathNetwork::GetPassability( const SPathPlace & ): bounds-check the
	// layer, try to build the spot, then bounds-check p against the layer's tiles BEFORE
	// any tile read. RefreshSpot is gated by !bFreeze && !bHasRecolourJob, so while the
	// AI-grid pass-calc/recolour jobs are still pending it builds nothing and the layer's
	// tiles stay empty (GetXSize()==0). Reading them via GetLockArea -> GetSamePoints then
	// dereferences empty tiles -> AV (e.g. placing units on template 1192). The release
	// avoids this by bounds-checking p against the tiles size and returning not-passable.
	if ( p.GetLayer() >= layers.size() )
		return false;
	CNodesLayer *pPLayer = layers[ p.GetLayer() ];
	pPLayer->pGroup->RefreshSpot( p, pMap, 1 );
	if ( p.GetX() >= pPLayer->tiles.GetXSize() || p.GetY() >= pPLayer->tiles.GetYSize() )
		return false;

	static vector<SPathPlace> locked( 20 );
	locked.resize(0);
	GetLockArea( &locked, p, false );
	for ( unsigned int i = 0; i < locked.size(); ++i )
	{
		SPathPlace pLock( locked[i] );
		CNodesLayer *pLayer = layers[pLock.GetLayer()];
		pLayer->pGroup->RefreshSpot( pLock, pMap, 1 );
		const CNodesLayer::STile &t = pLayer->tiles[pLock.GetY()][pLock.GetX()];
		if ( t.nLocks != 0 )
			return false;
	}
	
	if ( !IsNotOnDoor( p ) )
		return false;

	CNodesLayer *pLayer = layers[p.GetLayer()];
	pLayer->pGroup->RefreshSpot( p, pMap, 1 );
	if ( ( p.GetX() >= pLayer->tiles.GetXSize() ) || ( p.GetY() >= pLayer->tiles.GetYSize() ) )
		return false;
	const CNodesLayer::STile &t = pLayer->tiles[p.GetY()][p.GetX()];
	return IsTilePassable( p, t );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsNativePassable( const SPathPlace &p )
{
	CNodesLayer *pLayer = layers[p.GetLayer()];
	pLayer->pGroup->RefreshSpot( p, pMap, 1 );
	const CNodesLayer::STile &t = pLayer->tiles[p.GetY()][p.GetX()];
	switch ( p.GetPose() )
	{
		case CM_LAY:      return (t.nPassable & CP_LAY) != 0;
		case CM_CROUCH:   return (t.nPassable & (CP_CROUCH | CP_INACTIVE)) != 0;
		case CM_STAND:    return (t.nPassable & CP_STAND) != 0;
		case CM_INACTIVE: return (t.nPassable & CP_INACTIVE) != 0;
	}
	ASSERT( 0 );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::UpdateColouring( const vector<SPathPlace> &lockers )
{
	if ( bFreeze )
		return false;
	// crap 
	for ( unsigned int i = 0; i < groups.size(); ++i )
	{
		CLayersGroup *pGroup = groups[i];
		if ( pGroup->bHasRecolourJob )
			return false;
	}
	CPointsContainer difference;
	Xor( &difference, oldAccountPoints, lockers );
	oldAccountPoints = lockers;	
	if ( !bColouringConstructed )
	{
		// finish all work with map colouring
		FinishGridConstruction();
	}
	else
	{
		for ( vector<SPathPlace>::const_iterator j = difference.begin(); j != difference.end(); ++j )
		{
			vector<SPathPlace> res;
			GetLockArea( &res, *j, false );
			for ( vector<SPathPlace>::iterator i = res.begin(); i != res.end(); ++i )
			{
				CMapColourer *colourer = GetColourer( i->GetLayer() );
				colourer->AddZoneToRecalc( CTRect<unsigned char>( i->GetX(), i->GetY(), i->GetX()+1, i->GetY()+1 ) );
			}
		}
		//OutputDebugString("[ COLOURER INFO ] Update colouring\n");
		for ( int i=0; i < layers.size(); ++i )
			GetColourer( i )->RecalcColouring( this, i );
		for ( int i=0; i < layers.size(); ++i )
			//GetColourer( i )->AttachTransitions( this );
			GetColourer( i )->RecalcTransitions( this );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::CreateLadder( int _nX, int _nY, int _nHeight, int _nRotation, int _nLayersGroup, int _nFloor )
{
	CLayersGroup::SNotYetCreatedLadder ladder;
	ladder.nX = _nX;
	ladder.nY = _nY;
	ladder.nHeight = _nHeight;
	ladder.nRotation = _nRotation;
	ladder.nFloor = _nFloor;
	CLayersGroup *pGroup = groups[ _nLayersGroup ];
	pGroup->ladders.push_back( ladder );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::CreateLaddersInternal( CLayersGroup *pGroup )
{
	for ( int i = 0; i < pGroup->ladders.size(); ++i )
	{
		CLayersGroup::SNotYetCreatedLadder &ladderNYC = pGroup->ladders[i];
		CNodesLayer::SLadder ladder;
		CNodesLayer *pFloorLayer = pGroup->GetRootFloorLayer( ladderNYC.nFloor );
		if ( !pFloorLayer )
		{
			OutputDebugString("��������� ��������!\n");
			continue;
		}
		int nLayer = pFloorLayer->nLayer;
		ladder.placeOnBottom = SPathPlace( ladderNYC.nX, ladderNYC.nY, nLayer );
		ladder.eDir = (ELadderDirection)ladderNYC.nRotation;
		ladder.pointPassable.resize( ladderNYC.nHeight + 1, true );
		ladder.nLocks.resize( ladderNYC.nHeight + 1, 0 );
		ladder.pointOnUpperHalf.resize( ladderNYC.nHeight + 1, false );
		ladder.bNeedRecalc = true;

		CVec2 shift2;
		switch ( ladder.eDir )
		{
			case LD_RIGHT: shift2 = CVec2( - 0.5f, 0 ); break;
			case LD_FRONT: shift2 = CVec2( 0, - 0.5f ); break;
			case LD_LEFT: shift2 = CVec2( 0.5f, 0 ); break;
			case LD_BACK: shift2 = CVec2( 0, 0.5f ); break;
			default: ASSERT(0);
		}
		CVec2 ptXDir = pGroup->ptXDir;
		CVec3 shift( shift2.x * ptXDir + shift2.y * CVec2( -ptXDir.y, ptXDir.x ), 0 );
		CVec3 upper = GetCP( ladder.placeOnBottom );
		upper.z += ladderNYC.nHeight * F_LADDER_STEP;
		SSphere sphere( upper, F_LADDER_STEP * 1.1f );
		vector<SPathPlace> places;
		char buf[128];
		sprintf_s( buf, "ATTEMPTING TO PLACE A LADDER TO %f %f %f\n", upper.x, upper.y, upper.z );
		OutputDebugString( buf );
		GetNearPlaces( sphere, &places, true );
		float fBestDist = 500;
		if ( !places.empty() ) 
		{
			float fCurrDist;
			SPathPlace bestPlace( places[0] );
			for ( int i = 0; i < places.size(); ++i )
			{
				SPathPlace test( places[i] );
				fCurrDist = fabs2( GetCP( places[i] ) - upper - shift );
				if ( fCurrDist > fBestDist )
					continue;
				CNodesLayer *pL = GetLayer( places[i].GetLayer() );
				pL->pGroup->RefreshSpot( test, pMap, 2 );
				CNodesLayer::STile &t = pL->tiles[ places[i].GetY() ][ places[i].GetX() ];
				int dir = (ladder.eDir * 2 + 4) & 7;
				bool canCr = t.nMoveCrouch & ( 1 << dir );
				bool canJump = t.nMoveHC & ( 1 << dir );
				if ( ( t.nPassable & CP_INACTIVE ) && ( canCr || canJump ) )
				{
					fBestDist = fCurrDist; 
					bestPlace = places[i];
				}
			}
			ladder.placeOnTop = SPathPlace( bestPlace.GetX(), bestPlace.GetY(), bestPlace.GetLayer() );
		}
		else
			OutputDebugString("CANNOT EVEN FIND A PLACE TO PLACE A LADDER!\n");
		if ( fBestDist < 100 )
		{
			pFloorLayer->ladders.push_back( ladder );
		}
		else // no place to store a ladder! an error?
		{
			//ASSERT(0);
			csSystem << CC_RED << "Possible design ERROR: cannot place a vertical ladder on floor " << ladderNYC.nFloor << endl;
			csSystem << "Ladder parameters: height " << ladderNYC.nHeight << " steps (one step = 0.625m), " << 
				" tile X = " << ladderNYC.nX << ", tile Y = " << ladderNYC.nY << endl;
			OutputDebugString("CANNOT PLACE A LADDER! \n");
		}
	}
	pGroup->ladders.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::FormationMoveTo( vector<SPosition> *pPlaces, const SPosition &to )
{
	if ( pPlaces->size() == 0 )
	{
		ASSERT(0);
		return;
	}
	vector<SPosition> &places = *pPlaces;
	if ( pPlaces->size() == 1 )
	{
		places[0].p = to.p;
		return;
	}
	CVec3 averPos( 0, 0, 0 );
	CVec3 desiredPos = GetCP( to.p );
	for ( unsigned int i = 0; i < pPlaces->size(); ++i )
	{
		averPos += GetCP( places[i].p );
	}
	averPos /= places.size();
	CVec3 posNew;
	for ( int i = 0; i < pPlaces->size(); ++i )
	{
		CVec3 toGroupCenter = GetCP( places[i].p ) - averPos;
		float distToGroupCenter = fabs2( toGroupCenter );
		float distToTarget = fabs2( desiredPos - averPos );
		if ( distToTarget < distToGroupCenter )
			toGroupCenter = toGroupCenter / 2;
		if ( distToGroupCenter > F_MAX_FORMATION_RADIUS2 )
			toGroupCenter = toGroupCenter / fabs( toGroupCenter ) * F_MAX_FORMATION_RADIUS;
		posNew = toGroupCenter + desiredPos;
		// find the nearest point
		SSphere sphere( posNew, 2.0f );
		vector<SPathPlace> placesToPut;
		GetNearPlaces( sphere, &placesToPut );
		float fBestDist = 1000, fCurrDist;
		SPathPlace best;
		for ( int j = 0; j < placesToPut.size(); ++j )
		{
			if ( !IsValidDestination( placesToPut[j] ) )
				continue;
			fCurrDist = fabs2( GetCP( placesToPut[j] ) - posNew );
			if ( fCurrDist < fBestDist )
			{
				fBestDist = fCurrDist; 
				best = placesToPut[j];
			}
		}
		if ( fBestDist < 1000 )
			places[ i ].p = best;
		else
			places[ i ].p = to.p;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathNetwork::SFlipper *CPathNetwork::GetFlipper( const CObjectBase *_pSrc )
{
	CObjectBase* pUnconstSrc = const_cast<CObjectBase*>(_pSrc);
	CPtr<CObjectBase> pSrc(pUnconstSrc);
	CFlippersHash::iterator i = flippersHash.find( pSrc );
	if ( i != flippersHash.end() )
		return &flippers[ i->second ];
	else
	{
		flippers.push_back( SFlipper() );
		flippersHash[ pSrc ] = flippers.size() - 1;
		flippers.back().nFlipper = flippers.size() - 1;
		flippers.back().nFixedFlags = 0;
		CDynamicCast<NWorld::IWindowDoor> pDoor( pSrc ); 
		ASSERT( pDoor );
		if ( pDoor )
			flippers.back().bOpen = pDoor->IsOpen();
		else
			flippers.back().bOpen = false;
		return &flippers.back();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::FlipperOpenClose( CObjectBase* flipper, bool bOpen )
{
	CPtr<CObjectBase> test( flipper );
	ASSERT( GetFlipper( test )->nFixedFlags == 0 );
	if ( GetFlipper( test )->nFixedFlags )
		return;
	GetFlipper( test )->bOpen = bOpen;
	pMap->FlipDoorWindow( flipper, bOpen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::LockUnlockFlipper( CObjectBase* flipper, bool bLock )
{
	CPtr<CObjectBase> test( flipper );
	if ( bLock )
		GetFlipper( test )->nFixedFlags |= F_LOCKED_DOOR;
	else
		GetFlipper( test )->nFixedFlags &= ~F_LOCKED_DOOR;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPathNetwork::IsBlockedByFlipper(
	const SPathPlace	&from, const SPathPlace	&to, CPtr<CObjectBase> *ppFlipper,
	bool *bIsNowOpen, bool *bBlocksInOpenState, bool *bBlocksInClosedState )
{
	// verify pose
	CNodesLayer *pLayer = layers[to.GetLayer()];
	*ppFlipper = 0;
	pLayer->pGroup->RefreshSpot( to, pMap, 1 );
	const CNodesLayer::STile &t = pLayer->tiles[ to.GetY() ][ to.GetX() ];
	if ( t.nFlipper )
	{
		SFlipper &flipper = *GetFlipper( t.nFlipper - 1 );
		for ( CFlippersHash::iterator i = flippersHash.begin(); i != flippersHash.end(); ++i )
		{
			if ( i->second == t.nFlipper - 1 )
			{
				*ppFlipper = i->first;
				break;
			}
		}
		char nPassableO, nPassableC;
		SPathPlace test( to.GetX(), to.GetY(), to.GetLayer() );
		*bIsNowOpen = flipper.bOpen;
		unordered_map<SPathPlace, CNodesLayer::STile, SPathPlaceHash>::iterator it;
		it = flipper.locksOpen.find( test );
		if ( it == flipper.locksOpen.end() )
			nPassableO = t.nPassable;
		else
		{
			CNodesLayer::STile &t1 = it->second;
			nPassableO = t.nPassable & t1.nPassable;
		}
		it = flipper.locksClosed.find( test );
		if ( it == flipper.locksClosed.end() )
			nPassableC = t.nPassable;
		else
		{
			CNodesLayer::STile &t1 = it->second;
			nPassableC = t.nPassable & t1.nPassable;
		}
		switch ( to.GetPose() )
		{
			case CM_LAY:      
				{
					int dir = to.GetDirection() & 3;
					*bBlocksInOpenState = ( nPassableO & (CP_LAY1 << dir ) ) == 0;
					*bBlocksInClosedState = ( nPassableC & (CP_LAY1 << dir ) ) == 0;
					break;
				}
			case CM_CROUCH:   
				*bBlocksInOpenState = ( nPassableO & (CP_CROUCH | CP_INACTIVE)) == 0;
				*bBlocksInClosedState = ( nPassableC & (CP_CROUCH | CP_INACTIVE)) == 0;
				break;
			case CM_STAND:    
				*bBlocksInOpenState = ( nPassableO & CP_STAND) == 0;
				*bBlocksInClosedState = ( nPassableC & CP_STAND) == 0;
				break;
			case CM_INACTIVE: 
				*bBlocksInOpenState = ( nPassableO & CP_INACTIVE) == 0;
				*bBlocksInClosedState = ( nPassableC & CP_INACTIVE) == 0;
				break;
		}
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathNetwork::Freeze( bool _bFreeze )
{
	bFreeze = _bFreeze;
//	char buf[256];
//	sprintf( buf, "Freeze %d\n", bFreeze );
//	OutputDebugString( buf );
	for ( vector< CObj<CNodesLayer> >::iterator it = layers.begin(); it != layers.end(); ++it )
		if ( IsValid( *it ) )
			(*it)->pGroup->bFreeze = bFreeze;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SPathPlace CPathNetwork::GetDeployPlace( const SPathPlace &start, int nDisplacement )
{
	SPathPlace res( start );
	CVec3 cp( GetCP( start ) );
	CNodesLayer &layer = *layers[0];
	SPathPlace plMin( 0, 0, 0 ), plMax( layer.tiles.GetXSize() - 1, layer.tiles.GetYSize() - 1, 0 );
	int nRealNum = nDisplacement + 4;
	CVec3 vDir = ( GetCP( plMin ) + GetCP( plMax ) ) / 2 - cp;
	float fAngle = atan2( vDir.y, vDir.x );
	if ( nDisplacement != 0 )
	{
		int nDY = nRealNum % 3 - 1, nDX = nRealNum / 3 - 1;
		// Clamp the spread-out place to the layer's tile bounds [plMin..plMax]. Without this a party
		// member deployed at the grid edge is shifted off-grid (e.g. y == tiles.GetYSize()), and
		// SetPosition's GetFloor then reads tiles[y][x] out of bounds (template 1192 mission-start AV).
		// Matches release NAI::CPathNetwork::GetDeployPlace @0043e4e0 (Min/Max clamp on both axes).
		int nNewX = Min( Max( (int)res.GetX() + nDX, (int)plMin.GetX() ), (int)plMax.GetX() );
		int nNewY = Min( Max( (int)res.GetY() + nDY, (int)plMin.GetY() ), (int)plMax.GetY() );
		res.SetXY( nNewX, nNewY );
	}
	EDirection dir = GetClosestDir( start.GetLayer(), fAngle );
	res.SetDirection( dir );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x00941130, CNodesLayer )
REGISTER_SAVELOAD_CLASS( 0x01041150, CPathNetwork )
REGISTER_SAVELOAD_CLASS( 0x72952150, CLayersSetTracker )
REGISTER_SAVELOAD_CLASS( 0x72952151, CLayersGroup )
REGISTER_SAVELOAD_CLASS( 0x01352160, CLadderTracker )
