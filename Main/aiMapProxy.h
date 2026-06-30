#ifndef __aiMapProxy_H_
#define __aiMapProxy_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiGrid.h"
#include "aiMap.h"
#include <utility>

namespace NAI 
{
externA5 char cTranslations[][2];

enum EPathfinderMode
{
	PM_STAND_ONLY,
	PM_ANY_MOVE,
	PM_NUM_MODES
};

class CNodesLayerProxy : public CObjectBase // Proxy class which gives a "SquareMap" interface to a CNodesLayer
{
	OBJECT_BASIC_METHODS(CNodesLayerProxy);
	friend class CMapColourer;
	ZDATA
	CPtr<CNodesLayer> pLayer;
	CPtr<IAIMap> pMap;
	EPathfinderMode mode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pLayer); f.Add(3,&pMap); f.Add(4,&mode); return 0; }
	bool IsValidPoint( const CNodesLayer::STile &t ) const
	{
		return ( ( t.nLocks == 0 ) && ( t.nPassable != 0 || ( t.nFlags & TF_STAND_PASSABLE ) ) );
	}

	bool IsPassablePoint( const CNodesLayer::STile &t ) const
	{			
		return ( t.nFlags & TF_STAND_PASSABLE );
	}
public:
	CNodesLayerProxy(): pLayer(0), pMap(0), mode( PM_STAND_ONLY ) {}
	CNodesLayerProxy(CNodesLayer* _pLayer, IAIMap* _pMap): 
		pLayer(_pLayer), pMap(_pMap), mode( PM_STAND_ONLY ) {}

	void SetPathfinderMode( EPathfinderMode _mode ) { mode = _mode; }

	void GetSizes( int* pX, int* pY ) const
	{
		ASSERT(pLayer);
		*pX = pLayer->tiles.GetXSize();
		*pY = pLayer->tiles.GetYSize();
	}
	
	bool HasSameTransition( unsigned char cX, unsigned char cY ) const
	{
		return pLayer->tiles[ cY ][ cX ].nFlags & TF_HAS_SAME;
	}

	bool IsTransitionPoint( unsigned char cX, unsigned char cY ) const
	{
		CNodesLayer::STile &t = pLayer->tiles[ cY ][ cX ];
#ifdef _DEBUG
		bool b1 = ( t.nFlags & TF_HAS_INTERGRID );
		SPathPlace p( cX, cY, pLayer->nLayer ); 
		bool b2 = pLayer->transitions.find( p ) != pLayer->transitions.end();
		ASSERT( b1 == b2 );
		if ( b2 )
		{
			CNodesLayer::STransitionSet &trH = pLayer->transitions[ p ];
			vector<SPathPlace> links;
			for ( list<CNodesLayer::SLink>::iterator l = trH.links.begin(); l != trH.links.end(); ++l )
				links.push_back( l->dst );
			bool b3 = !trH.links.empty();
//			ASSERT( b3 );
		}
#endif
		return ( t.nFlags & ( TF_HAS_INTERGRID | TF_HAS_SAME ) ) != 0;
	}

	bool IsValidPoint( unsigned char cX, unsigned char cY ) const
	{
		CNodesLayer::STile &t = pLayer->tiles[ cY ][ cX ];
		if ( ( t.nLocks == 0 ) && ( t.nPassable != 0 || ( t.nFlags & TF_STAND_PASSABLE ) ) )
			return true;
		return false;//IsTransitionPoint( cX, cY );
	}

	bool IsPassablePoint( unsigned char cX, unsigned char cY ) const
	{			
		CNodesLayer::STile &t = pLayer->tiles[ cY ][ cX ];
		if ( t.nFlags & TF_STAND_PASSABLE )
			return true;
		return false;//IsTransitionPoint( cX, cY );
	}
 
	unsigned char GetOneMoveCost( unsigned char cX, unsigned char cY, EDirection direction ) const
	{
		unsigned char modifier = ( direction & 1 )? 3 : 2;
		// now mode = PM_ANY_MOVE
		ASSERT( pLayer );
		SPathPlace p( cX, cY, pLayer->nLayer ); 
		CNodesLayer::STile &t = pLayer->tiles[ cY ][ cX ];
		unsigned char 
			cNewX = cX + cTranslations[direction][0],
			cNewY = cY + cTranslations[direction][1];
		CNodesLayer::STile &t2 = pLayer->tiles[ cNewY ][ cNewX ];
		/*if ( !IsValidPoint( cX, cY ) )
		{
			if ( !IsValidPoint( cNewX, cNewY ) )
				return modifier;
			else
				return 0;	
		}
		if ( !IsPassablePoint( cX, cY ) )
		{
			if ( !IsValidPoint( t2 ) )
				return 0;
			if ( !IsPassablePoint( cNewX, cNewY ) ) 
				return modifier;
			if ( mode == PM_STAND_ONLY )
				return 0;
		}*/
		int oppositeDir = ( direction + 4 ) & 7;
		if ( t.nLocks || t2.nLocks )
			return 0;
		if ( t.nFlipper || t2.nFlipper )
		{
			SPathPlace test1( cX, cY, pLayer->nLayer, 0, CM_STAND, 0 );
			if ( pLayer->GetNetwork()->IsLocked( test1 ) )
				return 0;
			SPathPlace test2( cNewX, cNewY, pLayer->nLayer, 0, CM_STAND, 0 );
			if ( pLayer->GetNetwork()->IsLocked( test2 ) )
				return 0;
		}
		if ( ( t.nMoveStand & (1<<direction) ) && ( t2.nMoveStand & (1<<oppositeDir) ) )
			return modifier;
		if ( mode == PM_STAND_ONLY )
			return 0;
		if ( ( t.nMoveCrouch & (1<<direction) ) && ( t2.nMoveCrouch & (1<<oppositeDir) ) )
			return modifier * 2;
		if ( ( t.nMoveLay & (1<<direction) ) && ( t2.nMoveLay & (1<<oppositeDir) ) )
			return modifier * 3;
		if ( ( t.nMoveHC & (1<<direction) ) && ( t2.nMoveHC & (1<<oppositeDir) ) )
		{
			float fHDiff = GetFHeight(t2.nHeight) - GetFHeight(t.nHeight);
			if ( ( fHDiff > -F_MAX_CLIMB_HEIGHT ) && ( fHDiff < F_MAX_CLIMB_HEIGHT ) )
			{
				return modifier * 5;
				/*if ( fHDiff < 1.0f )
					return 5 * modifier;
				if ( fHDiff < 1.5f )
					return 10 * modifier;
				if ( fHDiff < 2.0f )
					return 12 * modifier;
				return 15 * modifier;*/
			}
		}
		return 0;
	}

	bool IsPassable( unsigned char cX, unsigned char cY, EDirection direction ) const
	{
		return GetOneMoveCost( cX, cY, direction ) != 0;
	}

	typedef CTPoint<unsigned char> SPosition;
	template<class TFunction>
		void ForEachMove( const SPosition &pos, TFunction &f ) const
	{
		int nMaxX, nMaxY;
		GetSizes( &nMaxX, &nMaxY );
		bool 
			left = ( pos.x > 0 ),
			right = ( pos.x < nMaxX - 1 ),
			down = ( pos.y > 0),
			up = ( pos.y < nMaxY - 1 );
		if ( left )
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, LEFT );
			if ( cCost )
				f( SPosition( pos.x - 1, pos.y ), cCost );
		}
		if ( up ) 
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, UP );
			if ( cCost )
				f( SPosition( pos.x, pos.y + 1 ), cCost );
		}
		if ( right ) 
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, RIGHT );
			if ( cCost )
				f( SPosition( pos.x + 1, pos.y ), cCost );
		}
		if ( down )
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, DOWN );
			if ( cCost )
				f( SPosition( pos.x, pos.y - 1 ), cCost );
		}

		if ( left && up )
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, UPLEFT );
			if ( cCost )
				f( SPosition( pos.x - 1, pos.y + 1 ), cCost );
		}
		if ( left && down )
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, DOWNLEFT );
			if ( cCost )
				f( SPosition( pos.x - 1, pos.y - 1 ), cCost );
		}
		if ( right && up )
		{
			unsigned char cCost = GetOneMoveCost( pos.x, pos.y, UPRIGHT );
			if ( cCost )
				f( SPosition( pos.x + 1, pos.y + 1 ), cCost );
		}
		if ( right && down )
		{
			unsigned char cCost = GetOneMoveCost( pos.x , pos.y , DOWNRIGHT );
			if ( cCost )
				f( SPosition( pos.x + 1, pos.y - 1 ), cCost );
		}
	}
	
};

}

#endif