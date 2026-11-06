#include "StdAfx.h"
#include "wMainTrace.h"
#include "wMain.h"
#include "aiMap.h"
#include "wOSBase.h"
#include "wMisc.h"
#include "iMission.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 NWorld::Trace @0x77e200 (sole caller CMission::TraceCursor 0x5ff813). One ranked pass
// over ONE trace fills the whole cursor record: the move tile, the cursor point and the hovered object.
// bModifier is CMission's "modifier" bind (Shift/Ctrl): while held, TS_PICK-only hulls do not take part.
void Trace( IWorld *pWorld, IPlayer *pPlayer, const CRay &ray, int nMaxFloor, STraceResult *pRes, bool bModifier )
{
	// 0x77e22d: the three latches only; pObject is left as is (GetTraceObject checks bObjectSet)
	pRes->bTileSet = false;
	pRes->bPointSet = false;
	pRes->bObjectSet = false;
	if ( !CDynamicCast<CWorld>(pWorld) )
		return;
	// 0x77e244..0x77e2c2
	IPlayer::CUnitSet myUnits;
	list< CPtr<CUnit> > visibleList;
	vector<IVisObj*> soundsList;
	list< CPtr<CObjectBase> > visibleObjects;
	if ( pPlayer )
	{
		pPlayer->GetUnits( &myUnits );
		pPlayer->GetVisible( &visibleList );
		pPlayer->GetSounds( &soundsList );
		pPlayer->GetVisibleObjects( &visibleObjects );
	}
	// 0x77e2c5..0x77e317: 0x100 while the modifier is held, 0x180 otherwise; terrain groups united
	vector<NAI::SInterval> intervals;
	pWorld->GetAIMap()->Trace( ray, &intervals, bModifier ? TS_PASS_BLOCKER : TS_PICK|TS_PASS_BLOCKER,
		NAI::CFloorsSet(), NAI::IAIMap::STH_UNION_TERR_HG );
	const NAI::SInterval *pBest = 0;
	float fBest = 1e30f;
	for ( int k = 0; k < intervals.size(); ++k )
	{
		const NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		CObjectBase *pUserData = interv.pSrc->pUserData;
		// 0x77e39e..0x77e42e: a heard-unit marker (retail IAISound, dev CDMesh) takes part only while
		// its unit is not ours, not visible and still audible; otherwise the interval is dropped for
		// the point, the tile and the object alike.
		CUnit *pHeard = dynamic_cast<CUnit*>( GetDMeshUnit( pUserData ) );
		if ( pHeard
			&& ( find( myUnits.begin(), myUnits.end(), pHeard ) != myUnits.end()
				|| find( visibleList.begin(), visibleList.end(), pHeard ) != visibleList.end()
				|| find( soundsList.begin(), soundsList.end(), CDynamicCast<IVisObj>( pUserData ) ) == soundsList.end() ) )
			continue;
		// 0x77e434..0x77e4fa: scenery that blocks vision (or does not take part in vision) is ranked
		// 1.0 farther, so a unit/targetable object/item just behind a cut-away hull or a grating wins.
		float fT = interv.enter.fT;
		int nTS = interv.pSrc->nTSFlags;
		if ( !( nTS & TS_VISION ) || ( nTS & TS_VISION_SOLID ) )
		{
			CDynamicCast<CUnit> pUnit( pUserData );
			CDynamicCast<CObjectServerBase> pObject( pUserData );
			CDynamicCast<IItem> pItem( pUserData );
			if ( !pUnit && !( pObject && pObject->IsTargetable() ) && !pItem )
				fT += 1.0f;
		}
		if ( fT < fBest )
		{
			fBest = fT;
			pBest = &interv;
		}
		// 0x77e4fe..0x77e581: the tile is the first interval (trace order, unpenalized enter) that
		// resolves onto a floor at or below the cut
		if ( !pRes->bTileSet )
		{
			CVec3 ptDst = ray.Get( interv.enter.fT );
			if ( pWorld->GetPathNetwork()->SetOnFloor( &pRes->sTile, interv.pSrc->nFloor, ptDst )
				&& pRes->sTile.GetFloor() <= nMaxFloor )
				pRes->bTileSet = true;
		}
	}
	if ( !pBest )
		return;
	// 0x77e5ca..0x77e6d1: does the ranked interval give a hovered object
	CObjectBase *pUserData = pBest->pSrc->pUserData;
	bool bTake = false;
	if ( CUnit *pUnit = dynamic_cast<CUnit*>( pUserData ) )
		bTake = find( visibleList.begin(), visibleList.end(), pUnit ) != visibleList.end();
	else if ( IObject *pObject = dynamic_cast<IObject*>( pUserData ) )
		bTake = pObject->IsTargetable();
	else if ( dynamic_cast<IItem*>( pUserData ) )
		bTake = find( visibleObjects.begin(), visibleObjects.end(), pUserData ) != visibleObjects.end();
	else if ( dynamic_cast<IMine*>( pUserData ) )
		bTake = true;
	else
		bTake = GetDMeshUnit( pUserData ) != 0;   // the marker, already gated above
	if ( bTake )
	{
		pRes->pObject = pUserData;
		pRes->bObjectSet = true;
	}
	// 0x77e6d5: the cursor point is the ranked interval at its unpenalized enter
	pRes->bPointSet = true;
	pRes->vPoint = ray.Get( pBest->enter.fT );
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
