#ifndef __WMAINWAYPOINTS_H_
#define __WMAINWAYPOINTS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// NWorld::CWaypointsHolder -- the name-keyed registry of route waypoints (release compiland
// wMainWaypoints). In the RETAIL build CWorld multiply-inherits this holder (base @0x64) and the
// `waypoints' map + AddWaypoint/LoadWaypoints/GetWaypoint live HERE instead of inline in CWorld.
// This dev tree keeps that registry inline in CWorld (wMain.h `waypoints', operator& tag 34), so the
// holder is landed as an ADDITIVE, UNWIRED parity surface: it compiles and is save-serializable on its
// own, but CWorld is NOT re-based onto it. Re-basing CWorld onto this holder and relocating the
// serialized `waypoints' map out of CWorld::operator& tag 34 into the holder is a world save-graph
// change and is DEFERRED (it would cross the "do not change CWorld's tag set / member layout" rule).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiRoute.h"   // NAI::CAIRouteWaypoint (complete), CMapWaypoint + NAI::IPathNetwork fwd decls
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// IWorldWaypoints -- retail interface (PDB size 20, virtual CObjectBase base; GetAIViewerWaypoints at
// vtbl+4, GetPathNetwork at vtbl+8) that CWaypointsHolder derives from. Consumed engine-wide as
// CPtr<IWorldWaypoints> (e.g. CAIViewer::pWaypoints, retail DoPtr<IWorldWaypoints>).
////////////////////////////////////////////////////////////////////////////////////////////////////
class IWorldWaypoints: public virtual CObjectBase
{
public:
	virtual void GetAIViewerWaypoints( vector<CVec3> *pOrigins, vector<CVec3> *pPoints ) = 0; // vtbl+4
	virtual NAI::IPathNetwork* GetPathNetwork() = 0;                                          // vtbl+8
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointsHolder (PDB size 40, single `waypoints' hash_map @8). Abstract here: GetPathNetwork stays
// pure, because in retail the concrete owner (CWorld) supplies it -- the dev tree keeps the registry in
// CWorld, so nothing constructs a standalone holder (the re-base is deferred). The 4 methods below are
// faithfully reconstructed; the only seam without a dev source (the b3DWaypoint map field) is stubbed.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWaypointsHolder: public IWorldWaypoints
{
	ZDATA
	unordered_map< string, CObj<NAI::CAIRouteWaypoint> > waypoints;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &waypoints ); return 0; }
	//
	CWaypointsHolder() {}
	// Clone the map waypoint, offset the copy by the world origin (ptShift), lower-case its name and
	// register it; duplicate names keep the FIRST. retail @0x77de70.
	void AddWaypoint( CMapWaypoint *pWaypoint, const CVec3 &ptShift );
	// Every existing-on-map waypoint. retail @0x77e000.
	void LoadWaypoints( const list< CObj<CMapWaypoint> > &_waypoints, const CVec3 &ptShift );
	// Lookup by (case-insensitive) name; logs + returns the (possibly invalid) slot. retail @0x77e040.
	NAI::CAIRouteWaypoint* GetWaypoint( const string &szName );
	// For the AI debug viewer: per live waypoint, its map origin + its resolved net point. retail @0x77ddb0.
	virtual void GetAIViewerWaypoints( vector<CVec3> *pOrigins, vector<CVec3> *pPoints );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __WMAINWAYPOINTS_H_
