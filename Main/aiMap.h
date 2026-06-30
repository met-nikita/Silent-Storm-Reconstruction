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
	virtual bool CalcIntersection( const CVec3 &ptCenter, float fRadius, int s, CObjectBase *pIgnoreUser = 0 ) = 0;
	virtual void PrepareCollider( IPrepareCollider *pRes, const SBound &bound, float fElementSize,
		const int nMask, bool bSelect2DoorHulls = false ) = 0;
	virtual void AddTracker( IAIMapTracker *pTracker, const SBound &b, int nMask, bool bInformOnDoorFlip = false ) = 0;
	virtual void FlipDoorWindow( CObjectBase *pWhat, bool bOpen ) = 0;
};
void GetGeometry( list<SObjectInfo> *pRes, vector<SMassSphere> *pSpheres, int nAIGeometryID, bool *pbClosed = 0 );
void GetSpheres( NDb::CModel *pModel, vector<SMassSphere> *pRes, CVec3 *pMassCenter );
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIMap* CreateAIMap( NWorld::IWorld *pWorld );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif