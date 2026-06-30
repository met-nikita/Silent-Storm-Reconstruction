#ifndef __aiWayConstraints_H_
#define __aiWayConstraints_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiGrid.h"
#include "aiMoves.h"
#include "aiColourer.h"

namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLayerColorConstraints
{
	CPathNetwork* pNet;
	vector< CArray2D<bool> > areasToCheck;
	bool bHasCheckZones;
	bool bMoveOnly;
	bool bDoNotCheck;
	const int *pCosts;
	int minCost;
	int bestPose;
	int CostOfPose( int nPose ) const 
	{ 
		if ( nPose == CM_INACTIVE )
			return pCosts[ MT_MOVE_CROUCH ];																
		return pCosts[ 4 - 2 * nPose ]; 
	}
public:
	unordered_map<SPathPlace, SPathPlace, SPathPlaceHash> finalPoints;
	CLayerColorConstraints( CPathNetwork* _pNet, bool _bMoveOnly, const int *_pCosts ): 
			pNet( _pNet ), bMoveOnly( _bMoveOnly ), pCosts( _pCosts ), bDoNotCheck( false ), bHasCheckZones( false )
	{
		minCost = Min( pCosts[ MT_MOVE_STAND ], pCosts[ MT_MOVE_CRAWL ] );
		minCost = Min( pCosts[ MT_MOVE_CROUCH ], minCost );
		if ( minCost == pCosts[ MT_MOVE_STAND ] )
			bestPose = CM_STAND;
		if ( minCost == pCosts[ MT_MOVE_CROUCH ] )
			bestPose = CM_CROUCH;
		if ( minCost == pCosts[ MT_MOVE_CRAWL ] )
			bestPose = CM_LAY;
		if ( !bDoNotCheck )
		{
			const vector<CObj<CNodesLayer> > &layers = pNet->GetLayers();
			areasToCheck.resize( layers.size() );
			for ( unsigned int nL = 0; nL < layers.size(); ++nL )
			{
				CNodesLayer *pL = layers[ nL ];
				areasToCheck[ nL ].SetSizes( (pL->tiles.GetXSize() >> 3) + 1, (pL->tiles.GetYSize() >> 3) + 1 );
				areasToCheck[ nL ].FillZero();
			}
		}
	}
	
	void DoNotCheck() { bDoNotCheck = true; }
	void AddCheckArea( const SZone &zone )
	{
		CArray2D<bool> &areas = areasToCheck[ zone.nLayer ];
		CTPoint<unsigned char> center(	zone.GetCenter( pNet ) );
//		char buf[100];
//		sprintf( buf, " Adding check areas around %d, %d\n", center.x, center.y );
//		OutputDebugString( buf );
		int nMinX = Max( (center.x >> 3) - 1, 0 );
		int nMinY = Max( (center.y >> 3) - 1, 0 );
		int nMaxX = Min( (center.x >> 3) + 2, areas.GetXSize() );
		int nMaxY = Min( (center.y >> 3) + 2, areas.GetYSize() );
		for ( int x = nMinX; x < nMaxX; ++x )
			for ( int y = nMinY; y < nMaxY; ++y )
				areas[ y ][ x ] = true;
		bHasCheckZones = true;
	}

	void AddWay( const CUpperNetWay& way )
	{
#ifdef _DEBUG
		OutputDebugString(" Adding way \n");
#endif
		for ( CUpperNetWay::const_iterator i = way.begin(); i != way.end(); ++i )
		{
			const SZone &zone = *i;
			AddCheckArea( zone );
#ifdef _DEBUG
			char buf[100];
			sprintf( buf, " Neighs of layer #%d color #%d adding...\n", i->nLayer, i->wColor );
			OutputDebugString( buf );
#endif
		}
	}

	bool IsClear() const { return !bHasCheckZones; }
	bool IsFinal( const SPathPlace &dst ) const
	{ 
		if ( dst.GetPose() != CM_STAND && dst.GetPose() != CM_CROUCH )
			return finalPoints.find( dst ) != finalPoints.end();
		SPathPlace _dst( dst );
		_dst.SetDirection( 0 );
		return finalPoints.find( _dst ) != finalPoints.end();
	}
	SPathPlace GetFinalParent( const SPathPlace& dst )
	{
		SPathPlace _dst( dst );
		if ( _dst.GetPose() == CM_STAND || _dst.GetPose() == CM_CROUCH )
			_dst.SetDirection( 0 );
		ASSERT ( finalPoints.find( _dst ) != finalPoints.end() );
		return finalPoints[ _dst ];
	}

	void AddFinalPoint( const SPathPlace& _position, const SPathPlace& _parent ) 
	{ 
		if ( pNet->IsPassable( _parent ) )
		{
			finalPoints[ _position ] = _parent;
			/*char buf[128];
			const SPathPlace *i = &_position;
			OutputDebugString( "Final point added: " );
			if ( i->IsIntegral() )
				sprintf( buf, "x:%d, y:%d, l:%d, p:%d, d:%d, m:%d\n", 
					i->GetX(), i->GetY(), i->GetLayer(), i->GetPose(), i->GetDirection(), i->IsMoving() );
			else
				sprintf( buf, "ladder# %d, step:%d, l:%d, d:%d\n", i->GetX(), i->GetY(), i->GetLayer(), i->GetDirection() );
			OutputDebugString( buf );*/
		}
		/*else
		{
			int placeBreakpointHere = 0;
		}*/
	}

	bool operator()(const SPathPlace &src, const SPathPlace &dst) const
	{ 
//		if ( IsFinal( dst ) )
//			return true;
		if ( bDoNotCheck )
			return true;
		if ( !dst.IsIntegral() )
			return true;
		unsigned char cX = (unsigned char)dst.GetX();
		unsigned char cY = (unsigned char)dst.GetY();
		int nLayerDst = dst.GetLayer(),
		    nLayerSrc = src.GetLayer();
		if ( !areasToCheck[ nLayerDst ][ cY >> 3 ][ cX >> 3 ] )
			return false;
		if ( dst.GetPose() != CM_LAY || bestPose == CM_LAY )
			return true;
		// if moving in dst pose is not better
		if  ( ( CostOfPose( src.GetPose() ) <= CostOfPose( dst.GetPose() ) ) &&
					( src.GetPose() != dst.GetPose() ) )
		{
			CNodesLayer 
				*pLayerDst = pNet->GetLayer(nLayerDst),
				*pLayerSrc = pNet->GetLayer(nLayerSrc);

			CNodesLayer::STile &tSrc = pLayerSrc->tiles[ src.GetY() ][ src.GetX() ];
			char cSrcFlags = tSrc.nMove[ src.GetPose() ];
			CNodesLayer::STile &tDst = pLayerDst->tiles[ cY ][ cX ];
			char cDstFlags = tDst.nMove[ dst.GetPose() ];
			// if has no sense to become crawling or crouching because can go
			if ( ( dst.GetPose() == bestPose ) && ( ( cSrcFlags & cDstFlags ) == cDstFlags ) )
				return false;
			if ( ( dst.GetPose() != bestPose ) && ( cSrcFlags & ( 1 << dst.GetDirection() ) ) )
				return false;

			// if has no sense to become crawling or crouching because can't crowl or croach
			if ( !( cDstFlags & ( 1 << dst.GetDirection() ) ) )  
				return false;
		} 
		if ( ( CostOfPose( dst.GetPose() ) > minCost ) && ( src.GetPose() == dst.GetPose() ) )
		{
			// always change pose to best when begin moving. The only exception is when unit CANNOT do it
			if ( bMoveOnly ) return true;
			CNodesLayer* pLayerSrc = pNet->GetLayer(nLayerSrc);
			CNodesLayer::STile &tSrc = pLayerSrc->tiles[ src.GetY() ][ src.GetX() ];
			// CRAP { code that sometimes we must change pose to make intergrid transitions
			char cFlags = tSrc.nMove[ bestPose ];
			if ( cFlags & ( 1 << dst.GetDirection() ) ) // can move in that direction using more normal pose
				return false;
			return true;
			// } EOCRAP
		}
		return true;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif