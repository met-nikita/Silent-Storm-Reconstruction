#include "StdAfx.h"
#include "aiPosition.h"
#include "aiNearestPosition.h"
#include "aiGrid.h"
#include "aiMap.h"
#include "wTSFlags.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::SPosition GetNearestPosition( CVec3 ptPos, IPathNetwork *_pPathNetwork, bool bMustHaveLink, const CVec3 &ptLink, bool bNative )
{
	CDynamicCast<CPathNetwork> pPathNetwork(_pPathNetwork);
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
			if ( !vNearestPlaces.empty() )
			{
				SPosition sPlace;
				sPlace.p = vNearestPlaces[0];
				sPlace.SetNetwork( pPathNetwork );
				return sPlace;
			}
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
