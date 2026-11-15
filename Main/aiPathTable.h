#ifndef __aiPathTable_H_
#define __aiPathTable_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Misc\RetailHashOrder.h"

namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathPlaceTable
{
public:
	typedef SMoveInfo<SPathPlace, WORD> SMove;
	typedef unordered_map<SPathPlace, SMove, SPathPlaceHash> CMovesHash;
	ZDATA
	CMovesHash data;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&data); return 0; }
	SMove& operator[]( const SPathPlace &_pt ) { return data[ _pt ]; }
	WORD GetCost( const SPathPlace &pt ) 
	{ 
		unordered_map<SPathPlace, SMove, SPathPlaceHash>::const_iterator i = data.find( pt );
		if ( i == data.end() ) 
			return 65535;
		return (i->second).cost;
	}
	void MakeInfinity(void) { data.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Transient path-search workspace. Saved reachable-area tables keep their layout.
// Equal-distance fallback destinations are resolved by retail's STLport order.
class CPathSearchTable : public CPathPlaceTable
{
	NRetail::CIntHashOrder order;
public:
	SMove& operator[]( const SPathPlace &pt )
	{
		std::pair<CMovesHash::iterator, bool> entry = data.emplace( pt, SMove() );
		if ( entry.second )
			order.Insert( pt.GetData() );
		return entry.first->second;
	}
	void MakeInfinity()
	{
		CPathPlaceTable::MakeInfinity();
		order.Clear(); // Retail retains the grown bucket count between searches.
	}
	const std::vector<int>& GetSearchOrder() const { return order.GetKeys(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif
