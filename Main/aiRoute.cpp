#include "stdafx.h"
//
#include "mapBuild.h"
#include "aiPosition.h"
#include "aiTaskCommander.h"
#include "aiWaypoint.h"
#include "aiControl.h"
#include "aiCommander.h"
#include "aiUnit.h"
#include "..\DBFormat\DataMap.h"
#include "wUnitCommands.h"
#include "wUnitGroup.h"
#include "wUnitServer.h"
#include "wMain.h"
#include "wOSBase.h"
#include "aiNearestPosition.h"
//
#include "aiRoute.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRouteWaypoint
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIRouteWaypoint::CAIRouteWaypoint( IPathNetwork *pPathNetwork, CMapWaypoint *pMapWaypoint )
{
	ASSERT( IsValid( pPathNetwork ) );
	ASSERT( IsValid( pMapWaypoint ) );
	if ( !IsValid( pPathNetwork ) || !IsValid( pMapWaypoint ) )
		return;
	//
	szName = pMapWaypoint->pName->szName;
	// RETAIL PARITY (marker-2 root): this 2-arg ctor is what fills the SCRIPT-facing registry
	// (CWorld::AddWaypoint -> CWorld::GetWaypoint -> GetWaypointPos / PlaceTemplate /
	// UnitSetToWaypoint / WaitForUnitWaypoint). Retail has ONE registry (CWaypointsHolder,
	// AddWaypoint @0x77de70) and its only waypoint ctor (@0x496000) snaps onto the nearest
	// NATIVE cell -- RealGetNearestPosition @0x7e830 bNative arm, accepting every
	// GetPassability != AIP_NOT_PASSABLE place (AIP_DOOR/AIP_LOCKED included), nearest by
	// plain 3D distance (no extra floor tiebreak in retail). The dev-legacy NON-native snap
	// here (IsPassable == AIP_YES only) rejected dynamically-blocked cells, so a waypoint on
	// a closed-door threshold (tutorial "DoorsTraining", floor 1 -> its cell is AIP_DOOR)
	// snapped to some far/other-layer AIP_YES cell and the lua proximity trigger
	// GetDistance( GetPos(hero), GetWaypointPos(wp) ) <= 2 could never fire. The earlier
	// native-arm filter fix (aiNearestPosition.cpp) never applied to script waypoints
	// because this ctor did not take the native arm.
	pos = GetNearestNativePosition( pMapWaypoint->pos.ptPos, pPathNetwork );
	commands = pMapWaypoint->commands;
	ptPos = pMapWaypoint->pos.ptPos;
}
//////////////////////////////////////////////////////////////////////////////////////
// retail @0x496000 -- the offset-map / 3D-aware waypoint ctor used by NWorld::CWaypointsHolder
// (release wMainWaypoints). Same as the 2-arg form except it snaps onto the nearest NATIVE cell
// (GetNearestNativePosition, like RecalcPosition) instead of GetNearestPosition, and -- when
// b3DWaypoint is set -- onto a 3D-fly cell. The dev CMapWaypoint has no b3DWaypoint field (adding it
// would change the map save format), so the only caller (CWaypointsHolder::AddWaypoint) always passes
// b3D == false; the 3D-fly snap branch is deferred to the native snap until the 3D-waypoint map seam lands.
CAIRouteWaypoint::CAIRouteWaypoint( IPathNetwork *pPathNetwork, CMapWaypoint *pMapWaypoint, bool b3DWaypoint )
{
	ASSERT( IsValid( pPathNetwork ) );
	ASSERT( IsValid( pMapWaypoint ) );
	if ( !IsValid( pPathNetwork ) || !IsValid( pMapWaypoint ) )
		return;
	//
	szName = pMapWaypoint->pName->szName;
	ptPos = pMapWaypoint->pos.ptPos;                         // retail vOrigin @0x2c
	pos = GetNearestNativePosition( ptPos, pPathNetwork );   // retail b3D==false branch
	commands = pMapWaypoint->commands;
	(void)b3DWaypoint; // 3D-fly snapping deferred (see note above)
}
//////////////////////////////////////////////////////////////////////////////////////	
const NWorld::SObjectPlace CAIRouteWaypoint::GetObjectPlace( int nAngle ) const
{
	NWorld::SObjectPlace res;
	res.ptPos = ptPos;
	res.ptScale = CVec3( 1, 1 ,1 );
	res.fAngle = nAngle / 180.f * PI;
	res.nFloor = pos.GetFloor();
	return res;
}
//////////////////////////////////////////////////////////////////////////////////////
// retail @0x95f60: re-snap the waypoint's stored world origin (ptPos) onto the nearest NATIVE passable
// cell of its current path network, overwriting pos. SPosition's CPtr<CPathNetwork> handles the net
// AddRef/Release on assignment, so the plain assignment matches retail's manual refcount swap.
void CAIRouteWaypoint::RecalcPosition()
{
	IPathNetwork *pNet = pos.GetNetwork();
	if ( pNet )
		pos = GetNearestNativePosition( ptPos, pNet );
}
//////////////////////////////////////////////////////////////////////////////////////
// CAIRoute
//////////////////////////////////////////////////////////////////////////////////////	
CAIRoute::CAIRoute( const vector< CPtr<CAIRouteWaypoint> > &_waypoints ):
	waypoints( _waypoints )
{
}
//////////////////////////////////////////////////////////////////////////////////////	
CAIRoute::CAIRoute( NWorld::CWorld *pWorld,	
	const vector< CPtr<CMapWaypoint> > &_mapWaypoints )
{
	for ( vector< CPtr<CMapWaypoint> >::const_iterator 
		i = _mapWaypoints.begin(); i != _mapWaypoints.end(); ++i )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pWorld->GetWaypoint( (*i)->pName->szName );
		if ( IsValid( pWaypoint ) )
			waypoints.push_back( pWaypoint );
	}
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAIRoute::IsEmpty()
{
	return waypoints.empty();
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAIRoute::CreateSyncs( vector< CPtr<NAI::CTaskSyncObject> > *pSyncs )
{
	ASSERT( pSyncs != 0 );
	if ( pSyncs == 0 )
		return;
	//
	for ( int i = 0; i < waypoints.size(); ++i )
		pSyncs->push_back( new NAI::CTaskSyncObject() );
}
//////////////////////////////////////////////////////////////////////////////////////	
CTask *CAIRoute::GetTask( NWorld::CUnitServer *pUS, NWorld::CUnitGroup *_pUnitGroup, 
	const vector< CPtr<NAI::CTaskSyncObject> > &syncs, bool bCircled )
{
	bool bGroupRoute = IsValid( _pUnitGroup ) && _pUnitGroup->units.GetSize() > 1;
	if ( bGroupRoute && syncs.size() != waypoints.size() )
		return 0;
	//
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pUS ) )
		return 0;
	//
	CPtr<NWorld::CUnitGroup> pUnitGroup = _pUnitGroup;
	if ( !IsValid( pUnitGroup ) )
	{
		pUnitGroup = new NWorld::CUnitGroup();
		pUnitGroup->units.Add( pUS );
	}
	ASSERT( IsValid( pUnitGroup ) );
	ASSERT( pUnitGroup->units.IsContain( pUS ) );
	if ( !IsValid( pUnitGroup ) || !pUnitGroup->units.IsContain( pUS ) )
		return 0;
	//
	bool bCircledTask = bCircled && waypoints.size() > 1;
	CTask *pTask = new CTask( pUS, bCircledTask );
	ASSERT( IsValid( pTask ) );
	if ( !IsValid( pTask ) )
		return 0;
	//
	int nSync = 0;
	for ( vector< CPtr<CAIRouteWaypoint> >::iterator 
		i = waypoints.begin(); i != waypoints.end(); ++i, ++nSync )
	{
		// ��������� Waypoint
		int nIndex = -1;
		vector< NAI::SPosition > unitPlaces;
		for ( int i = 0; i < pUnitGroup->units.GetSize(); ++i )
		{
			if ( pUnitGroup->units[ i ] == pUS )
				nIndex = i;
			unitPlaces.push_back( pUnitGroup->units[ i ]->GetPosition().pos );
		}
		pUS->GetWorld()->GetPathNetwork()->FormationMoveTo( &unitPlaces, (*i)->pos );
		//
		pTask->AddCommand( new CTaskCommandGoto( unitPlaces[ nIndex ] ) );
		// ��������� ��������
		for ( vector<NAI::SCommand>::iterator 
			c = (*i)->commands.begin(); c != (*i)->commands.end(); ++c )
		{
			CTaskCommand *pCommand = 0;
			switch ( (*c).cmd )
			{
				case CMD_POSE:
					pCommand = new CTaskCommandChangePose( (*c).pose );
					break;
				case CMD_DIR:
					pCommand = new CTaskCommandChangeDirection( (*c).dir );
					break;
				case CMD_WAIT:
					pCommand = new CTaskCommandWait( (*c).time );
					break;
			}
			ASSERT( IsValid( pCommand ) );
			if ( IsValid( pCommand ) )
				pTask->AddCommand( pCommand );
		}
		//
		if ( bGroupRoute )
			pTask->AddCommand( new CTaskCommandSync( syncs[ nSync ].GetPtr() ) );
	}
	//
	return pTask;
}
//////////////////////////////////////////////////////////////////////////////////////	
void SetUnitRoute( NWorld::CUnitServer *pUS, CAIRoute *pRoute, bool bCircled, NAI::EAIManager manager )
{
	CPtr<CAIRoute> pHolder = pRoute;
	ASSERT( IsValid( pUS ) );
	ASSERT( IsValid( pRoute ) );
	if ( !IsValid( pUS ) || !IsValid( pRoute ) )
		return;
	//
	CDynamicCast<NAI::CAICommander> pAICommander( pUS->GetPlayer()->GetCommander() );
	if ( IsValid( pAICommander ) )
	{
		vector< CPtr<NAI::CTaskSyncObject> > syncs;
		CPtr<NAI::CTask> pTask = pRoute->GetTask( pUS, 0, syncs, bCircled );
		CPtr<NAI::IAIControl> pAIControl = 
			NAI::CreateAITaskControl( pAICommander, pTask, NAI::AI_CONTROL_INTERRUPTABLE, manager );
		pAICommander->GetAIUnit( pUS )->AssignControl( pAIControl );
	}
}
//////////////////////////////////////////////////////////////////////////////////////
void SetGroupRoute( NWorld::CUnitGroup *pGroup, CAIRoute *pRoute, bool bCircled, NAI::EAIManager manager )
{
	CPtr<CAIRoute> pHolder = pRoute;
	ASSERT( IsValid( pGroup ) );
	ASSERT( IsValid( pRoute ) );
	if ( !IsValid( pGroup ) || !IsValid( pRoute ) )
		return;
	//
	vector< CPtr<NAI::CTaskSyncObject> > syncs;
	pRoute->CreateSyncs( &syncs );
	//
	for ( int i = 0; i < pGroup->units.GetSize(); ++i )
	{
		CPtr<NWorld::CUnitServer> pUS = pGroup->units[ i ];
		CPtr<NAI::CTask> pTask = pRoute->GetTask( pUS, pGroup, syncs, bCircled );
		CDynamicCast<NAI::CAICommander> pAICommander( pUS->GetPlayer()->GetCommander() );
		if ( IsValid( pAICommander ) )
		{
			CPtr<NAI::IAIControl> pAIControl = 
				NAI::CreateAITaskControl( pAICommander, pTask, NAI::AI_CONTROL_INTERRUPTABLE, manager );
			pAICommander->GetAIUnit( pUS )->AssignControl( pAIControl );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////	
void SetUnitRoaming( NWorld::CUnitServer *pUS, 
	const NAI::SPathPlace &p, int nAPradius, NAI::EAIManager manager )
{
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pUS ) )
		return;
	//
	CDynamicCast<NAI::CAICommander> pAICommander( pUS->GetPlayer()->GetCommander() );
	if ( IsValid( pAICommander ) )
	{
		CPtr<NAI::CTask> pTask = new NAI::CTask( pUS, true );
		pTask->AddRoaming( p, nAPradius );
		NAI::IAIControl *pAIControl = 
			CreateAITaskControl( pAICommander, pTask, AI_CONTROL_INTERRUPTABLE, manager );
		pAICommander->GetAIUnit( pUS )->AssignControl( pAIControl );
	}
}
//////////////////////////////////////////////////////////////////////////////////////	
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52402110, CAIRoute );
REGISTER_SAVELOAD_CLASS( 0x52402111, CAIRouteWaypoint );