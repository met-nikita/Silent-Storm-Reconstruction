#ifndef _aiSimpleWaveSearch_H_
#define _aiSimpleWaveSearch_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TMap, class TPosition, class TCost, class TConstraints, class TCostsTable, 
			size_t MAX_BUFFER, size_t MAX_MOVE_COST>
class CWaveCountCosts
{
	TCostsTable* table;
	const TConstraints *constraints;
	const TMap* pMap;
	TPosition position;

	TPosition queue[MAX_MOVE_COST+1][MAX_BUFFER];
	int currentPrior;
	int queueMaximums[MAX_MOVE_COST+1];

	TPosition current;
	TCost currentCost;

public:
	void SetConstraints( const TConstraints *_constr) { constraints = _constr; }
	void SetMap( const TMap *_pMap) { pMap = _pMap; }
	void SetPosition( const TPosition& _position) { position = _position; }

	CWaveCountCosts
	(TCostsTable *_table, const TConstraints *_constraints, 
	 const TMap *_map, const TPosition& _position): 
	table(_table), constraints(_constraints), pMap(_map), position(_position), currentPrior(0) {}

	void operator()( const TPosition &pos, TCost cost )
	{
		if ( !(*constraints)( pos ) ) 
			return; 
		TCost totalCost = currentCost + cost;
		if ( table->GetCost(pos) <= totalCost ) 
			return; 
		// Добавляем, определяя заодно новую стоимость
		(*table)[pos].parentPos  = current;
		(*table)[pos].cost = totalCost;
		int nQueue = cost + currentPrior; 
		if (nQueue > MAX_MOVE_COST) 
			nQueue -= (MAX_MOVE_COST+1);
		
		// This assertion will fail if buffer size was not enough
		ASSERT(queueMaximums[nQueue] < MAX_BUFFER);

		queue[nQueue][queueMaximums[nQueue]] = pos;
		queueMaximums[nQueue]++;	
	}

	void Count( )
	{
		// initializing
		ZeroMemory( queueMaximums, (MAX_MOVE_COST+1)*sizeof(int) );
		
		// somehow make table have infinite values
		table->MakeInfinity();

		// adding first TPosition
		(*table)[position] = SMoveInfo<TPosition, TCost>(position, 0); // parent is the same TPosition and cost is zero
		queue[0][0]= position;
		queueMaximums[0]++;
		currentPrior = 0;

		// Main loop
		while (1)
		{
			// pop next TPosition as current
			//	get next non-empty queue as current prior; if no any, return
			int oldPrior = currentPrior;
			while (!queueMaximums[currentPrior]) 
			{				
				currentPrior++;
				if (currentPrior > MAX_MOVE_COST) currentPrior -= (MAX_MOVE_COST+1);
				if (oldPrior == currentPrior) 
					return; // no non-empty queues
			}
			queueMaximums[currentPrior]--;
			current = queue[currentPrior][queueMaximums[currentPrior]];

		  currentCost = (*table)[current].cost;
			// for each move in TMap beginning with this TPosition, apply operator() ...
			pMap->ForEachMove(current, *this);
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif