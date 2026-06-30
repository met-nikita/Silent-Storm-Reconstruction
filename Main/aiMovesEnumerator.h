#ifndef __aiMovesEnumerator_H_
#define __aiMovesEnumerator_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiPosition.h"
namespace NAI 
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMovesEnumerator
{
	IPathNetwork *pNet;
	bool bCheckSuicide, bMoveOnly;
	const int *pCosts;
	vector<SMove> pfMoves;
	vector<char> nDynLocks;
	bool bBigUnit;
public:
	CMovesEnumerator( IPathNetwork* _pNet, const int *_pCosts, bool _bCheckSuicide, bool _bMoveOnly, bool _bBigUnit ):
			pNet(_pNet), bCheckSuicide(_bCheckSuicide), bMoveOnly(_bMoveOnly), pCosts(_pCosts), bBigUnit(_bBigUnit),
			pfMoves(100), nDynLocks(100) {}

	bool MustDraw( const SPathPlace &p )
	{
		return false;
	}
	CVec3 GetCP( const SPathPlace &p ) 
	{ 
		NAI::SPosition pos; 
		pos.p = p;
		pos.SetNetwork( pNet );
		return pos.GetCP();
	}

	template<class TFunction>
		void ForEachMove(const SPathPlace& pos, TFunction& f)
	{
		int nMovesCount;
		GetNonStandartMoves( pNet, pos, bCheckSuicide, bMoveOnly, &pfMoves, &nMovesCount, &nDynLocks );
		for ( int i = 0; i < nMovesCount; ++i )
		{
			EMoveType type = pfMoves[i].type;
			WORD wCost = WORD( pCosts[ type ] );
			if ( type == MT_TURN ) 
				wCost = 0;
			else
			{
				wCost += nDynLocks[ i ];
				if ( wCost > 16 )
					wCost = 16;
			}
			if ( bBigUnit )
			{
				if ( pNet->IsBigLockerLocked( pfMoves[i].dest, BL_PANZERKLEINE ) )
					continue;
			}
			f( pfMoves[i].dest, wCost );
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif