#include "StdAfx.h"
#include "aiPosition.h"
#include "aiNearestPosition.h"
#include "aiGrid.h"
#include "aiMap.h"
#include "wTSFlags.h"
#include <float.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.1 0x47e3f0 / v1.2 0x47e420. An upward destination is not a fallback
// for a blocked spawn/teleport. The small forward preference breaks equidistant ties.
static float LookWhereMoveCriterium( const CVec3 &vDist, const CVec3 &facing,
	float fMaxFallDist, bool *pFall )
{
	*pFall = false;
	if ( vDist.z < -0.05f )
		return 1000;
	float fHorDist = vDist.x * vDist.x + vDist.y * vDist.y;
	float fPenalty = vDist.x * facing.x + vDist.y * facing.y + vDist.z * facing.z > 0 ? 0.04f : 0;
	if ( vDist.z < fMaxFallDist )
	{
		if ( fHorDist < 0.01f )
		{
			*pFall = true;
			return fHorDist + vDist.z * vDist.z * ( 0.2f * 0.2f );
		}
		return fHorDist + vDist.z * vDist.z + fPenalty;
	}
	return fHorDist + vDist.z * vDist.z * 4 + fPenalty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ENearestUnitPosResult LookWhereToMoveUnit( const SUnitPosition &from, SUnitPosition *pTo,
	float fMaxFallDist, float fHeight )
{
	*pTo = from; // Retail keeps the requested place when no candidate succeeds.
	CPathNetwork *pNet = from.pos.pNet;
	if ( !IsValid( pNet ) )
		return NPR_FAILED;
	CVec3 desired = from.GetCP();
	vector<SPathPlace> places;
	pNet->GetNearPlaces( SSphere( desired, 3 ), &places );
	int pose = from.pos.p.GetPose();
	if ( pose == CM_INACTIVE )
		pose = CM_CROUCH;
	float fAngle = from.GetDirection();
	CVec3 facing( cos( fAngle ) * 0.3f, sin( fAngle ) * 0.3f, 0.3f );
	float fMinDist = 1000;
	bool bFall = false;
	for ( int i = 0; i < places.size(); ++i )
	{
		places[i].SetPose( pose );
		places[i].SetDirection( from.GetDir() );
		// Retail uses the anchor query, not IsPassable's large/prone lock footprint.
		if ( pNet->GetPassability( places[i] ) != AIP_YES )
			continue;
		CNodesLayer *pLayer = pNet->GetLayer( places[i].GetLayer() );
		if ( !pLayer || pLayer->tiles[places[i].GetY()][places[i].GetX()].nMove[pose] == 0 )
			continue;
		CVec3 cp = pNet->GetCP( places[i] );
		CVec3 vDist( desired.x - cp.x, desired.y - cp.y, fHeight - cp.z );
		bool bCandidateFall;
		float fDist = LookWhereMoveCriterium( vDist, facing, fMaxFallDist, &bCandidateFall );
		if ( fDist < fMinDist )
		{
			fMinDist = fDist;
			bFall = bCandidateFall;
			pTo->pos.p = places[i];
		}
	}
	if ( fMinDist < 100 )
		return bFall ? NPR_FALL : NPR_MOVE;
	return NPR_FAILED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::SPosition GetNearestPosition( CVec3 ptPos, IPathNetwork *_pPathNetwork, bool bMustHaveLink, const CVec3 &ptLink, bool bNative )
{
	CDynamicCast<CPathNetwork> pPathNetwork(_pPathNetwork);
	SPosition invalidPlace;
	invalidPlace.SetNetwork( pPathNetwork );
	// Invalid physical positions must not reach the grid's Float2Int conversion.
	if ( !IsValid( pPathNetwork ) || !_finite( ptPos.x ) || !_finite( ptPos.y ) || !_finite( ptPos.z ) )
		return invalidPlace;
	float fMinDistance = 0xFFFF;

	SPosition sNearestPlace;
	bool bFound = false;
	float fRadius = 1.f;
	while ( !bFound )
	{
		SSphere sSphere( ptPos, fRadius );
		vector<SPathPlace> vNearestPlaces;
		pPathNetwork->GetNearPlaces( sSphere, &vNearestPlaces );
		for ( vector<SPathPlace>::iterator p = vNearestPlaces.begin(); p != vNearestPlaces.end(); ++p )
		{
			// retail RealGetNearestPosition @0x7e830: the NATIVE snap accepts every place that is not
			// hard-impassable -- GetPassability(p) != AIP_NOT_PASSABLE -- so fundamentally-walkable but
			// dynamically-blocked cells (AIP_DOOR under a closed door, AIP_LOCKED) still snap. Dev's
			// IsNativePassable (raw tile CP_* flags) rejected them, so a waypoint on a door threshold
			// (tutorial DoorsTraining, nFloor=1) snapped to a far/wrong-layer cell and its proximity
			// trigger could never fire. The non-native arm (IsPassable == AIP_YES) matches retail.
			if ( bNative ? ( pPathNetwork->GetPassability( *p ) != AIP_NOT_PASSABLE )
			             : pPathNetwork->IsPassable( *p ) )
			{
				SPosition sPlace;
				sPlace.p = *p;
				sPlace.SetNetwork( pPathNetwork );
				CVec3 ptTmpPosition = sPlace.GetCP();
				if ( fabs2( ptTmpPosition - ptPos ) < fMinDistance )
				{
					if ( bMustHaveLink )
					{
						IAIMap *pAIMap = pPathNetwork->GetAIMap();
						CRay ray;
						ray.ptOrigin = ptLink;
						ray.ptDir = ptTmpPosition + CVec3( 0, 0, 1.0f ) - ptLink;
						vector<SInterval> intersect;
						pAIMap->Trace( ray, &intersect, NWorld::TS_PASS_BLOCKER );
						bool bHasLink = true;
						for ( int it = 0; it < intersect.size(); ++it )
						{
							SInterval &il = intersect[it];
							if ( il.enter.fT < 0 || il.enter.fT > 1 )
								continue;
							bHasLink = false;
							break;
						}
						if ( !bHasLink )
							continue;
					}
					bFound = true;
					fMinDistance = fabs2( ptTmpPosition - ptPos );
					sNearestPlace = sPlace;
					if ( fMinDistance == 0 )
						break;
				}
			}
		}
		fRadius += 2;
		if ( fRadius > 15 )
		{
			ASSERT(0); // No position found near search place
			ASSERT( !vNearestPlaces.empty() );
			// Retail also returns at the radius cap when the candidate list is
			// empty. Do not grow the radius forever on an empty/bad grid.
			if ( !vNearestPlaces.empty() )
				invalidPlace.p = vNearestPlaces[0];
			return invalidPlace;
		}
	}
	return sNearestPlace;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x7ef80 NAI::GetNearestNativePosition == RealGetNearestPosition(pt, pNet, bNative=true,
// bMustHaveLink=false): same expanding-radius search but accepting native-grid-passable cells (the only
// difference from GetNearestPosition is the IsNativePassable test; no collider/link probe).
NAI::SPosition GetNearestNativePosition( CVec3 ptPos, IPathNetwork *_pPathNetwork )
{
	return GetNearestPosition( ptPos, _pPathNetwork, false, CVec3(), true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
