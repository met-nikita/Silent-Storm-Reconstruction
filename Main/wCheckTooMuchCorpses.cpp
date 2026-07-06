#include "StdAfx.h"
#include "wUnitServer.h"
#include "aiPosition.h"
#include "..\Misc\2Darray.h"

// =====================================================================================
// wCheckTooMuchCorpses -- the corpse-density failsafe ("Too much corpses (%d) detected,
// possible fail, removing"). The visible map is bucketed into 4m cells; whenever a 3x3
// cell neighbourhood holds more than 15 visitor-set corpse objects, the oldest REMOVABLE
// ones are flagged out of the visitor set until the count is back at the cap of 15.
//
// Release free functions (compiland wCheckTooMuchCorpses.obj), reconstructed against the
// real engine types (the decomp answer-key's SCorpseHooks/SRefreshHooks abstraction
// is dropped -- every probe wires to a real call):
//   * corpse predicate            -> CDumbUnitServer::IsAddedToVisitor()
//   * "oldest"                     -> smallest CUnitServer::GetDeathTime()  (retail +0x228)
//   * "mark for removal"           -> CDumbUnitServer::MarkNotAddedToVisitors()
//                                     (retail bNotAddedToVisitors @+0x12c = 1); the
//                                     single-oldest path follows it with
//                                     CDumbUnitServer::Update() (bindGlobal.Update ->
//                                     guarded CSyncSrc<IVisObj>::Update on +0x30/+0x34).
//                                     The v1.1 bulk path wrote the byte alone; v1.2
//                                     @0x748140 fixed the asymmetry (full mark in both
//                                     paths) -- ported below.
//   * cell of a corpse            -> CUnit::GetPosition().GetCP()  (CUnit-base vtbl +0x44)
//   * REMOVABLE iff               -> !CanFight() (primary vtbl +0x44)
//                                 && !IsClueUnit() (CUnit-base vtbl +0x14, retail
//                                    @0x3c6900 == nClueCount>0)
//                                 && !IsEmptyPK() (CUnit-base vtbl +0x10)
//                                 && !IsValid( GetWearingDBPK() ) -- v1.2 @0x748340's
//                                    fourth gate: the worn Panzerklein must be absent or
//                                    dead (pPK==0 || nObjData bit31)
//                                 -- a live, quest-clue-carrying, empty-panzerklein or
//                                    live-PK-wearing object must not vanish. (The
//                                    answer-key doc-comment guessed "IsActive/IsHeld/
//                                    IsPossessed"; the real methods were resolved from
//                                    the Game.exe vtables.)
//
// PARITY SURFACE ONLY -- nothing calls these yet (behaviour-neutral). The release
// CDumbUnitServer::IsAddedToVisitor() also tested bNotAddedToVisitors, so a freshly
// marked corpse dropped out of the candidate scan on the next pass; the dev
// IsAddedToVisitor() currently returns only !bIsPKWhichIsWeared, so that skip is a no-op
// here until the method is converged -- left as-is on purpose (not touching the shared
// visitor path). Wiring the guarded CheckTooMuchCorpses() into CWorld::UpdateVisible is a
// deliberate follow-up so this first commit stays green.
// =====================================================================================

namespace NWorld
{

// NWorld::Push @0x347d40 -- append every element of src onto dst (per-element copy); backs
// the nine neighbourhood-bucket merges of the CArray2D TryRemoveCorpses overload below.
template< class T >
void Push( vector< T > &dst, const vector< T > &src )
{
	for ( int i = 0; i < (int)src.size(); ++i )
		dst.push_back( src[i] );
}

// NWorld::RemoveOldestCorpse @0x347c90 -- flag the visitor-set corpse with the smallest
// death-time. Faithfully keeps the original quirk: a zero death-time leaves the "best"
// tracker effectively unset (nBest stays 0), so later corpses keep replacing it.
void RemoveOldestCorpse( vector< CObj< CUnitServer > > &corpses )
{
	CUnitServer *pBest = 0;
	unsigned long nBest = 0;
	for ( int i = 0; i < (int)corpses.size(); ++i )
	{
		CUnitServer *pUS = corpses[i];
		if ( !pUS->IsAddedToVisitor() )
			continue;
		unsigned long nTs = pUS->GetDeathTime();		// retail +0x228
		if ( nBest == 0 || nTs < nBest )
		{
			pBest = pUS;
			nBest = nTs;
		}
	}
	if ( pBest != 0 )
	{
		pBest->MarkNotAddedToVisitors();				// retail byte @+0x12c = 1
		pBest->Update();								// bindGlobal.Update() -- guarded CSyncSrc<IVisObj>::Update
	}
}

// NWorld::TryRemoveCorpses @0x347d00 -- flag the n oldest; when the demand covers the
// whole list, flag everything. v1.2 @0x748140: the bulk branch now performs the FULL mark
// (byte + guarded vis-sync Update), exactly like RemoveOldestCorpse -- v1.1 wrote the byte
// alone here, never notifying the vis sync. (The CArray2D overload below calls this
// function, so it inherits the fix, matching the v1.2 @0x7481e0 de-inlining.)
void TryRemoveCorpses( vector< CObj< CUnitServer > > &corpses, int n )
{
	if ( n < (int)corpses.size() )
	{
		for ( int i = 0; i < n; ++i )
			RemoveOldestCorpse( corpses );
	}
	else
	{
		for ( int i = 0; i < (int)corpses.size(); ++i )
		{
			corpses[i]->MarkNotAddedToVisitors();
			corpses[i]->Update();   // v1.2: full mark (was byte-only in v1.1)
		}
	}
}

// NWorld::TryRemoveCorpses @0x347d80 -- gather a cell's 3x3 neighbourhood buckets into one
// list (the nine Push merges, rows nY-1..nY+1 x cols nX-1..nX+1) and remove n from it.
void TryRemoveCorpses( CArray2D< vector< CObj< CUnitServer > > > &buckets, int nX, int nY, int n )
{
	vector< CObj< CUnitServer > > gathered;
	for ( int dy = -1; dy <= 1; ++dy )
		for ( int dx = -1; dx <= 1; ++dx )
			Push( gathered, buckets[nY + dy][nX + dx] );
	TryRemoveCorpses( gathered, n );
}

// NWorld::FillQuantities @0x347f20 -- bucket every visitor-set corpse into 4m cells
// (cell = Float2Int((cp - ptMin) * 0.25 + 0.5)), counting all of them in `counts` and
// collecting the REMOVABLE ones (!CanFight && !IsClueUnit && !IsEmptyPK && no live worn
// PK -- the fourth gate is v1.2 @0x748340) in `buckets`.
// bResize sizes the grids on the first pass; the recount passes just clear them.
void FillQuantities( int nXSize, int nYSize, const CVec3 &ptMin,
					 const list< CObj< CUnitServer > > &units, bool bResize,
					 CArray2D< int > &counts,
					 CArray2D< vector< CObj< CUnitServer > > > &buckets )
{
	if ( bResize )
	{
		counts.SetSizes( nXSize, nYSize );
		buckets.SetSizes( nXSize, nYSize );
	}
	else
	{
		for ( int y = 0; y < nYSize; ++y )
			for ( int x = 0; x < nXSize; ++x )
				buckets[y][x].clear();
	}
	counts.FillZero();
	for ( list< CObj< CUnitServer > >::const_iterator it = units.begin(); it != units.end(); ++it )
	{
		CUnitServer *pUS = *it;
		if ( !pUS->IsAddedToVisitor() )
			continue;
		CVec3 cp = pUS->GetPosition().GetCP();
		int nX = Float2Int( ( cp.x - ptMin.x ) * 0.25f + 0.5f );
		int nY = Float2Int( ( cp.y - ptMin.y ) * 0.25f + 0.5f );
		counts[nY][nX] += 1;
		// v1.2 @0x748340: fourth removability gate -- a corpse still wearing a LIVE
		// Panzerklein is counted but never auto-removed (worn-PK record must be absent
		// or dead: pPK==0 || (nObjData & 0x80000000), i.e. !IsValid).
		if ( !pUS->CanFight() && !pUS->IsClueUnit() && !pUS->IsEmptyPK()
			 && !IsValid( pUS->GetWearingDBPK() ) )
			buckets[nY][nX].push_back( pUS );
	}
}

// NWorld::CheckTooMuchCorpses @0x348130 -- grid sizes = Float2Int(extent * 0.25 + 2.0), at
// least 3 per axis. Every interior cell whose 3x3 neighbourhood holds more than 15 corpses
// triggers TryRemoveCorpses(sum - 15) over the gathered neighbourhood buckets, then a
// recount.
void CheckTooMuchCorpses( const CVec3 &ptMin, const CVec3 &ptMax,
						 const list< CObj< CUnitServer > > &units )
{
	int nXSize = Float2Int( ( ptMax.x - ptMin.x ) * 0.25f + 2.0f );
	int nYSize = Float2Int( ( ptMax.y - ptMin.y ) * 0.25f + 2.0f );
	if ( nXSize < 3 )
		nXSize = 3;
	if ( nYSize < 3 )
		nYSize = 3;
	CArray2D< int > counts;
	CArray2D< vector< CObj< CUnitServer > > > buckets;
	FillQuantities( nXSize, nYSize, ptMin, units, true, counts, buckets );
	for ( int y = 1; y < nYSize - 1; ++y )
	{
		for ( int x = 1; x < nXSize - 1; ++x )
		{
			int nSum = 0;
			for ( int dy = -1; dy <= 1; ++dy )
				for ( int dx = -1; dx <= 1; ++dx )
					nSum += counts[y + dy][x + dx];
			if ( nSum > 15 )
			{
				DebugTrace( "Too much corpses (%d) detected, possible fail, removing\n", nSum );
				TryRemoveCorpses( buckets, x, y, nSum - 15 );
				FillQuantities( nXSize, nYSize, ptMin, units, false, counts, buckets );
			}
		}
	}
}

} // namespace NWorld
