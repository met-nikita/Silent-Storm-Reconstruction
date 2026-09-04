#include "StdAfx.h"
//
namespace NWorld { class CWorld; }          // aiRoute.h's CAIRoute(NWorld::CWorld*,..) ctor needs it fwd-declared
#include "mapBuild.h"
#include "aiPosition.h"
#include "aiRoute.h"
#include "..\Misc\StrProc.h"          // NStr::ToLower
#include "..\MiscDll\LogStream.h"     // csSystem / CC_RED / endl
//
#include "wMainWaypoints.h"
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointsHolder::AddWaypoint  retail @0x77de70
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointsHolder::AddWaypoint( CMapWaypoint *pWaypoint, const CVec3 &ptShift )
{
	// Clone (CObjectBase::MakeCopy via the public Duplicate()) so the world-origin shift never mutates
	// the shared map data, then offset the copy by ptShift -- offset-map support.
	CObj<CMapWaypoint> pCopy = pWaypoint->Duplicate();
	pCopy->pos.ptPos += ptShift;
	//
	string szLowerName = pCopy->pName->szName;
	NStr::ToLower( szLowerName );
	//
	CObj<NAI::CAIRouteWaypoint> &slot = waypoints[ szLowerName ];
	if ( !IsValid( slot ) )
	{
		// Retail forwards CMapWaypoint+0x48 so explicitly 3D editor points are fly-snapped.
		slot = new NAI::CAIRouteWaypoint( GetPathNetwork(), pCopy, pCopy->b3DWaypoint );
	}
	else
	{
		csSystem << CC_RED << "There are more then one waypoint with name " << pCopy->pName->szName << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointsHolder::LoadWaypoints  retail @0x77e000
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointsHolder::LoadWaypoints( const list< CObj<CMapWaypoint> > &_waypoints, const CVec3 &ptShift )
{
	for ( list< CObj<CMapWaypoint> >::const_iterator
		i = _waypoints.begin(); i != _waypoints.end(); ++i )
	{
		if ( (*i)->bExists )
			AddWaypoint( *i, ptShift );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointsHolder::GetWaypoint  retail @0x77e040
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CAIRouteWaypoint* CWaypointsHolder::GetWaypoint( const string &szName )
{
	string szLowerName = szName;
	NStr::ToLower( szLowerName );
	NAI::CAIRouteWaypoint *pRes = waypoints[ szLowerName ];
	if ( !IsValid( pRes ) )
		csSystem << CC_RED << "waypoint \"" << szName.c_str() << "\" not found " << endl;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointsHolder::GetAIViewerWaypoints  retail @0x77ddb0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointsHolder::GetAIViewerWaypoints( vector<CVec3> *pOrigins, vector<CVec3> *pPoints )
{
	vector<CVec3>().swap( *pOrigins );
	vector<CVec3>().swap( *pPoints );
	for ( unordered_map< string, CObj<NAI::CAIRouteWaypoint> >::iterator
		i = waypoints.begin(); i != waypoints.end(); ++i )
	{
		NAI::CAIRouteWaypoint *pW = i->second;
		if ( !IsValid( pW ) )
			continue;
		pOrigins->push_back( pW->ptPos );      // retail vOrigin @0x2c (the map origin)
		pPoints->push_back( pW->pos.GetCP() ); // retail GetCP of pos @0x18 (resolved net point)
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
// IWorldWaypoints is consumed engine-wide as CPtr<NWorld::IWorldWaypoints> (retail
// DoPtr<IWorldWaypoints>), so register the cast-only specializations the release way. It is
// deliberately NOT a REGISTER_SAVELOAD_CLASS: the retail holder has no standalone save id -- it is
// serialized only as a CWorld base -- and re-basing CWorld onto it is the deferred save-graph leg.
BASIC_REGISTER_CLASS( NWorld::IWorldWaypoints );
