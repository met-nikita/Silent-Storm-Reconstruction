#ifndef __AIROUTE_H_
#define __AIROUTE_H_
//
#include "aiWaypoint.h"
//
class CMapWaypoint;
//
namespace NWorld
{
	class CUnitServer;
	class CUnitGroup;
	struct SObjectPlace;
}
//
namespace NAI
{
//
struct SCommand;
class CTask;
class IPathNetwork;
class CTaskSyncObject;
struct SPathPlace;
enum EAIManager;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRouteWaypoint
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRouteWaypoint: public CObjectBase
{
	OBJECT_BASIC_METHODS( CAIRouteWaypoint );
	ZDATA
public:
	string szName;
	NAI::SPosition pos;
	vector<NAI::SCommand> commands;
	CVec3 ptPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&szName); f.Add(3,&pos); f.Add(4,&commands); f.Add(5,&ptPos); return 0; }
	//
	CAIRouteWaypoint() {}
	CAIRouteWaypoint( IPathNetwork *pPathNetwork, CMapWaypoint *pMapWaypoint );
	CAIRouteWaypoint( IPathNetwork *pPathNetwork, CMapWaypoint *pMapWaypoint, bool b3DWaypoint ); // +offset/3D-aware (retail @0x496000); used by NWorld::CWaypointsHolder
	//
	const NWorld::SObjectPlace GetObjectPlace( int nAngle ) const;
	void RecalcPosition();   // retail @0x95f60 -- re-snap pos onto the nearest native cell of ptPos's network
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRoute
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRoute: public CObjectBase
{
	OBJECT_BASIC_METHODS( CAIRoute );
	ZDATA
	vector< CPtr<CAIRouteWaypoint> > waypoints;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&waypoints); return 0; }
	//
public:
	CAIRoute() {}
	CAIRoute( NWorld::CWorld *pWorld,	const vector< CPtr<CMapWaypoint> > &_mapWaypoints );
	CAIRoute( const vector< CPtr<CAIRouteWaypoint> > &_waypoints );
	//
	CTask *GetTask( NWorld::CUnitServer *pUS, 
		NWorld::CUnitGroup *_pUnitGroup, 
		const vector< CPtr<NAI::CTaskSyncObject> > &syncs, bool bCircled );
	void CreateSyncs( vector< CPtr<NAI::CTaskSyncObject> > *pSyncs );
	bool IsEmpty();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetUnitRoute( NWorld::CUnitServer *pUS, CAIRoute *pRoute, bool bCircled, NAI::EAIManager manager );
void SetGroupRoute( NWorld::CUnitGroup *pGroup, CAIRoute *pRoute, bool bCircled, NAI::EAIManager manager );
void SetUnitRoaming( NWorld::CUnitServer *pUS, const NAI::SPathPlace &p, int nAPradius, NAI::EAIManager manager );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __AIROUTE_H_