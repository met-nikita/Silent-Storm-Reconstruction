#ifndef _aiWaveSearch_H_
#define _aiWaveSearch_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 vector<SSphere> sphereParticles;	// test sphere visualization
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TPosition, class TCost>
struct SMoveInfo
{
	TPosition parentPos;
	TCost cost;
	SMoveInfo( const TPosition &_parentPos, const TCost &_cost): cost(_cost), parentPos(_parentPos) {}
	SMoveInfo() {}   // default constructor needed by CArray2D
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSquareMapCosts
{
public:
	typedef CTPoint<unsigned char> SPosition;
	typedef SMoveInfo<SPosition, WORD> SMove;
private:
	ZDATA
	CArray2D<SMove> moves;
public:	
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&moves); return 0; }
	CSquareMapCosts() {}
	CSquareMapCosts( unsigned char _cSizeX, unsigned char _cSizeY ) { moves.SetSizes( _cSizeX, _cSizeY ); }
	SMove& operator[]( const SPosition &pt ) { return moves[ pt.y ][ pt.x ]; }
	WORD GetCost( const SPosition &pt ) { return moves[ pt.y ][ pt.x ].cost; }
	void MakeInfinity(void)
	{
		// do nothing - we'll make it infinite in other point
	}
	void SetSizes( unsigned char cMaxX, unsigned char cMaxY ) { moves.SetSizes( cMaxX, cMaxY ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TMap, class TPosition, class TCost, class TConstraints, class TCostsTable, size_t MAX_MOVE_COST>
class CWaveCountCostsMajor
{
	TCostsTable *table;
	const TConstraints *constraints;
	TMap *pMap;
	TPosition position;

	typedef vector<TPosition> TPositionVector;
	TPositionVector queue[MAX_MOVE_COST+1];
	int currentPrior;
	TPosition current;
	TCost currentCost;
	TCost maxCost;  // maxCost == 0 means that maxCost is not defined

public:
	TPosition firstReached;
	void SetConstraints( const TConstraints *_constr) { constraints = _constr; }
	void SetMap( TMap *_pMap) { pMap = _pMap; }
	void SetPosition( const TPosition& _position) { position = _position; }

	CWaveCountCostsMajor
	(TCostsTable* _table, const TConstraints *_constraints, 
	 TMap* _map, const TPosition& _position): 
	table(_table), constraints(_constraints), pMap(_map), position(_position), currentPrior(0) {}

	void operator()( const TPosition &pos, TCost cost )
	{
		ASSERT( cost <= MAX_MOVE_COST );
		TCost totalCost = currentCost + cost;
		if ( table->GetCost( pos ) <= totalCost ) 
			return; 

		if ( !(*constraints)( current, pos ) ) 
			return;

		if ( pMap->MustDraw( pos ) )
		{
			CVec3 ptPos = pMap->GetCP( pos );
			sphereParticles.push_back( SSphere( ptPos, 0.1f ) );
		}

		// Добавляем, определяя заодно новую стоимость
		if  (( !maxCost ) || ( totalCost < maxCost ))
		{
			(*table)[ pos ].parentPos  = current;
			(*table)[ pos ].cost = totalCost;
			int nQueue = cost + currentPrior; 
			if ( nQueue > MAX_MOVE_COST ) 
				nQueue -= ( MAX_MOVE_COST + 1 );

			queue[ nQueue ].push_back( pos ); 
#ifdef _DEBUG
			if ( ( queue[ nQueue ].size() & 127 ) == 0 )
			{
				char buf[50];			
				sprintf( buf, "WaveSearch buffer: queue size is %d\n", queue[ nQueue ].size() );
				OutputDebugString( buf );
			}
#endif
		}
	}

	bool Count( TCost _maxCost = 0 )
	{
		sphereParticles.clear();
		// initializing
		for ( int i = 0; i < MAX_MOVE_COST+1; ++i )
			queue[i].clear();
		maxCost = _maxCost;
		
		// somehow make table have infinite values
		table->MakeInfinity();

		// adding first TPosition
		(*table)[position] = SMoveInfo<TPosition, TCost>(position, 0); // parent is the same TPosition and cost is zero
		queue[0].push_back( position );
		currentPrior = 0;

		// Main loop
		while (1)
		{
			// pop next TPosition as current
			//	get next non-empty queue as current prior; if no any, return
			int oldPrior = currentPrior;
			while ( queue[currentPrior].empty() ) 
			{				
				currentPrior++;
				if (currentPrior > MAX_MOVE_COST)
					currentPrior -= (MAX_MOVE_COST+1);
				if (oldPrior == currentPrior) 
					return false; // no non-empty queues
			}
			current = queue[currentPrior].back();
			queue[currentPrior].pop_back();
			
			if ( constraints->IsFinal( current ) )
			{
				firstReached = current;
				return true; // final point reached
			}

		  currentCost = (*table)[current].cost;
			// for each move in TMap beginning with this TPosition, apply operator() ...
			pMap->ForEachMove(current, *this);
		}
	}
};
///////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif