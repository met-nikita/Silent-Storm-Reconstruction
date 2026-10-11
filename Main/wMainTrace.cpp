#include "StdAfx.h"
#include "wMainTrace.h"
#include "wMain.h"
#include "aiMap.h"
#include "wOSBase.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool TraceTile( IWorld *pWorld, const CRay &ray, NAI::SPosition *pRes, int nMaxFloor )
{
	if ( !CDynamicCast<CWorld>(pWorld) )
		return false;
	CVec3 ptDst;
	vector<NAI::SInterval> intervals;
	// retail NWorld::Trace @0x37d760 (the cursor pick; this function's sole caller is
	// CMission::TraceCursor) traces TS_PICK|TS_PASS_BLOCKER for the tile result -- Jan03's
	// TS_PASS_BLOCKER-only set missed clickable walkable surfaces whose hull carries only
	// TS_PICK (e.g. a lower-floor surface), so a click on one fell through past the gate
	// and resolved to the wrong (current-floor) tile. Interval order stays nearest-first
	// (CAIMap::Trace sorts unconditionally), matching retail's first-in-gate tile pick.
	pWorld->GetAIMap()->Trace( ray, &intervals, TS_PICK|TS_PASS_BLOCKER, NAI::CFloorsSet(), NAI::IAIMap::STH_SPLIT_TERR_HG );
	for ( int k = 0; k < intervals.size(); ++k )
	{
		NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		ptDst = ray.ptOrigin + interv.enter.fT * ray.ptDir;
		if ( pWorld->GetPathNetwork()->SetOnFloor( pRes, interv.pSrc->nFloor, ptDst )
			&& pRes->GetFloor() <= nMaxFloor )
			return true;
	}
	// retail Trace v1.2 0x77dccd/0x77ded4: no actual hit resolving to a tile means no move target.
	// Do not intersect an imaginary z=1 plane and snap that off-map point onto layer zero.
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceObjects( IWorld *pWorld, const CRay &ray, vector<CObjectBase*> *pRes, int nMaxFloor )
{
	if ( !CDynamicCast<CWorld>(pWorld) )
		return;
	vector<NAI::SInterval> intervals;
	pWorld->GetAIMap()->Trace( ray, &intervals, TS_PICK|TS_PASS_BLOCKER );
	NAI::SInterval *pBest = 0;
	float fBest = 1e30f;
	for ( int k = 0; k < intervals.size(); ++k )
	{
		NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		// Retail NWorld::Trace @0x77d760 ranks interactives ahead of scenery.
		// A cut-away building hull can be nearer than the visible unit behind
		// it. Do not stop at that hull before the caller can classify the pick.
		// Pure vision/non-solid surfaces retain retail's unpenalized priority.
		float fT = interv.enter.fT;
		int nTS = interv.pSrc->nTSFlags;
		if ( !( nTS & TS_VISION ) || ( nTS & TS_VISION_SOLID ) )
		{
			CDynamicCast<CUnit> pUnit( interv.pSrc->pUserData );
			CDynamicCast<CObjectServerBase> pObject( interv.pSrc->pUserData );
			CDynamicCast<IItem> pItem( interv.pSrc->pUserData );
			if ( !pUnit && !( pObject && pObject->IsTargetable() ) && !pItem )
				fT += 1.0f;
		}
		if ( fT < fBest )
		{
			fBest = fT;
			pBest = &interv;
		}
	}
	if ( pBest )
		pRes->push_back( pBest->pSrc->pUserData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool TraceRay( IWorld *pWorld, const CRay &ray, int nMaxFloor, CObjectBase **ppUserData, int *pUserID, CVec3 *pPoint )
{
	CFWContext world( &pCurrentWorld, (CWorld*)pWorld );
	vector<NAI::SInterval> intervals;
	pWorld->GetAIMap()->Trace( ray, &intervals, TS_FRAGMENTED );
	NAI::SInterval *pBest = 0;
	float fBest = 1e30f;
	for ( int k = 0; k < intervals.size(); ++k )
	{
		NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		if ( interv.enter.fT < fBest )
		{
			fBest	= interv.enter.fT;
			pBest = &interv;
		}
	}
	if ( !pBest )
		return false;
	else
	{
		*ppUserData = pBest->pSrc->pUserData;
		*pUserID = pBest->nUserID;
		*pPoint = ray.Get( fBest );
		return true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
