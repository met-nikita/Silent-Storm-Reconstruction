#include "StdAfx.h"
#include "aiMoves.h"
#include "aiMultiMoves.h"
#include "aiMovesEnumerator.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAlways
{
	bool operator()( const SPathPlace&, const SPathPlace& ) const { return true; }
	bool IsFinal( const SPathPlace& ) const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//				CMultiMovesTable
////////////////////////////////////////////////////////////////////////////////////////////////////
CPath * CMultiMovesTable::ConstructPath( const SPathPlace &dst )
{
	CMultiMovesTable::CMovesHash::iterator k = data.find( dst );
	if ( k == data.end() )
		return 0;
	list<SPathPlace> points;
	SPathPlace cur( dst );
	points.push_front( cur );
	while ( cur.GetData() != src.GetData() )
	{
		CMultiMovesTable::CMovesHash::iterator k = data.find( cur );
		ASSERT ( k != data.end() );
		cur = k->second.parentPos;
		points.push_front( cur );
	}
	CPath *pRes = new CPath;
	pRes->pNet = pNet;
	for ( list<SPathPlace>::iterator i = points.begin(); i != points.end(); ++i )
		pRes->AddNewPoint( *i );

	pRes->Improve( false, dst, pNet, PF_DEFAULT ); // CRAP - dst is not needed
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiMovesTable::PrepareAllPaths( list<SPathPlace> *pRes, 
		IPathNetwork *_pNet, const SPathPlace &_src, const int *pCosts, int nPriceLimit, 
		const list<CObjectBase*> &accountUnits, bool bBigUnit, 
		bool bCheckSuicide, bool bMoveOnly )
{
	src = _src;
	pNet = _pNet;
#ifdef _DEBUG
	for ( int i = 0; i < N_MOVE_TYPES; ++i )
	{
		if ( pCosts[i] > N_PF_MAXWEIGHT - 1 )
		{
			ASSERT(0);
			return;
		}
	}
#endif
	pNet->LockSelected( accountUnits );
	SAlways constraints;
	CMovesEnumerator enumerator( pNet, pCosts, bCheckSuicide, bMoveOnly, bBigUnit );
	CWaveCountCostsMajor<CMovesEnumerator, SPathPlace, WORD, SAlways, CMultiMovesTable, 16>
		counter( this, &constraints, &enumerator, src );
	counter.Count( nPriceLimit );
	pNet->UnlockSelected();
	if ( pRes == 0 )
		return;
	pRes->clear();
	for ( CMultiMovesTable::CMovesHash::iterator k = data.begin(); k != data.end(); ++k )
	{
		pRes->push_back( k->first );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}