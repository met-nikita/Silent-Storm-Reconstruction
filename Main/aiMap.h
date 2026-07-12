#ifndef __AIMAP_H_
#define __AIMAP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiInterval.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class IWorld;
	class CUnit;
};
namespace NDb
{
	class CModel;
	class CRPGArmor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSPTree;
class CExplVoxelRenderer;
class CVisionVoxelRenderer;
class CFastRenderer;
class IPrepareCollider;
class IStabilityTrackers;   // aiStability.h
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFloorsSet
{
protected:
	vector<int> floors;
	friend class CAIMap;
public:
	CFloorsSet() {}
	CFloorsSet( int nFloor ) { floors.push_back( nFloor ); }
	CFloorsSet( const vector<int> &_floors ): floors(_floors) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectInfo
{
	vector<CVec3> points;
	vector<STriangle> tris;
	int nPieceID;
	int nArmorID;
	int nTSFlags;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIMapTracker: public CObjectBase
{
public:
	virtual void OnChange() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSPTree;
class IAIMap: public CObjectBase
{
public:
	enum ESplitTerrainHGroups
	{
		STH_SPLIT_TERR_HG,
		STH_UNION_TERR_HG
	};
	enum ESort
	{
		STH_NOSORT,
		STH_SORT_INTERVALS
	};
	enum ESyncType
	{
		ST_FAST,
		ST_NORMAL
	};
	virtual void Sync( ESyncType st = ST_NORMAL ) = 0;
	virtual void GetEntities( list<SObjectInfo> *pRes, int nMask, const CFloorsSet &hg = CFloorsSet() ) = 0;
	virtual void Trace( const CRay &, vector<SInterval> *pIntersections, int nMask, const CFloorsSet &hg = CFloorsSet(), ESplitTerrainHGroups shg = STH_UNION_TERR_HG ) = 0;
	virtual void TraceGrid( CFastRenderer *pRes, int nMask, ESort sort = STH_NOSORT, const CFloorsSet &hg = CFloorsSet(),
		ESplitTerrainHGroups shg = STH_UNION_TERR_HG, bool bSelect2DoorHulls = false ) = 0;
	virtual void TraceVoxelGrid( CExplVoxelRenderer *pRes, int nMask, const CFloorsSet &hg = CFloorsSet(),
		bool bSelect2DoorHulls = false ) = 0;
	virtual void TraceVisionGrid( CVisionVoxelRenderer *pRes, int nMask, const CFloorsSet &hg = CFloorsSet(),
		bool bSelect2DoorHulls = false ) = 0;
	//virtual void TraceUnit( const CRay &, vector<SInterval> *pIntersections, CObjectBase *pTarget ) = 0;
	//virtual void TraceUnit( CFastRenderer *pRes, CObjectBase *pTarget ) = 0;
	virtual void GetUnitHLPos( CVec3 *pRes, CObjectBase *pHull, int nUserID ) = 0;
	virtual void GetAccessibleUnitHL( vector<int> *pRes, const CVec3 &ptFrom, CObjectBase *pHull, float fMaxDistance ) = 0;
	virtual CObjectBase* GetHull( CObjectBase *pUser ) = 0;
	// retail NAI::CAIMap::GetObjectBound @0x465800 (IAIMap vtbl+0x2c): the union bound of EVERY hull
	// registered for `pSrc` (retail: CUserHullsTracker::GetHulls + SBoundCalcer over each hull's
	// bound). Zeroes *pRes and returns false when the object has no hulls. Consumer:
	// CUnitServer::UpdateVisible's mine-LOS probe pull-back (@0x7c4d53).
	virtual bool GetObjectBound( SBound *pRes, CObjectBase *pSrc ) = 0;
	virtual bool CalcIntersection( const CVec3 &ptCenter, float fRadius, int s, CObjectBase *pIgnoreUser = 0 ) = 0;
	virtual void PrepareCollider( IPrepareCollider *pRes, const SBound &bound, float fElementSize,
		const int nMask, bool bSelect2DoorHulls = false ) = 0;
	virtual void AddTracker( IAIMapTracker *pTracker, const SBound &b, int nMask, bool bInformOnDoorFlip = false ) = 0;
	virtual void FlipDoorWindow( CObjectBase *pWhat, bool bOpen ) = 0;
	// retail IAIMap vtbl+0x48: the wreckage stability grid owned by this map (see aiStability.h).
	virtual IStabilityTrackers* GetStabilityTrackers() = 0;
	// retail IAIMap vtbl+0x4c (NAI::CAIMap::SelectHullPointers @0x67840/@0x673b0): collect every
	// hull whose cached node bound intersects b -- the same door-state visibility gate as the other
	// hull queries, an include mask (must overlap) and an exclude mask (must not), NO floor filter.
	// Pushes the CConvexHull objects themselves (pointer identity is what the stability trackers
	// diff); the output vector is NOT cleared first.
	virtual void SelectHullPointers( vector< CPtr<CObjectBase> > *pRes, const SBound &b, int nIncludeMask, int nExcludeMask ) = 0;
};
void GetGeometry( list<SObjectInfo> *pRes, vector<SMassSphere> *pSpheres, int nAIGeometryID, bool *pbClosed = 0 );
void GetSpheres( NDb::CModel *pModel, vector<SMassSphere> *pRes, CVec3 *pMassCenter );
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIMap* CreateAIMap( NWorld::IWorld *pWorld );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif