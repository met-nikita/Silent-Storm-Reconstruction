#include "StdAfx.h"
#include "aiStability.h"
#include "aiMap.h"
#include "..\DBFormat\DataFormat.h"
#include "wTSFlags.h"
#include "wInterface.h"
#include "wOSBase.h"
#include "wUnitServer.h"
#include "wDebris.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
const float FP_STABLE_HEIGHT = 0.15f;
bool CheckObjectStability( IAIMap *pMap, NDb::CModel *pModel, const SHMatrix &_pos, CObjectBase *pIgnore )
{
	if ( !pModel )
		return true;

	CVec3 massCenter;
	vector<SMassSphere> massSpheres;
	NAI::GetSpheres( pModel, &massSpheres, &massCenter );
	if ( massSpheres.empty() )
		return true;
	const SHMatrix &fwd = _pos;
	vector<SSphere> spheres;
	CVec3 tmp;
	for ( int i = 0; i < massSpheres.size(); ++i )
	{
		fwd.RotateHVector( &tmp, massSpheres[i].ptCenter );
		spheres.push_back( SSphere( tmp, massSpheres[i].fRadius ) );
	}
	fwd.RotateHVector( &massCenter, massCenter );
	vector<CVec3> stable;
	for ( int i = 0; i < spheres.size(); ++i )
	{
		if ( pMap->CalcIntersection( spheres[i].ptCenter, spheres[i].fRadius, NWorld::TS_ITEM_BLOCKER, pIgnore ) ) //TERRAINS | NWorld::TS_OBJECTS
			stable.push_back( spheres[i].ptCenter );
	}
	if ( spheres.size() < 4 )
		return ( spheres.size() == stable.size() );
	if ( stable.size() <= spheres.size() / 2 )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The stability-tracker grid (retail aiStability.obj). See aiStability.h for the subsystem overview.
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::GetObjSpheres @0xa55c0: the model's collision spheres (NAI::GetSpheres @0x67050,
// local space) transformed into world space by the placement matrix; radius copied through.
static bool GetObjSpheres( NDb::CModel *pModel, const SHMatrix &m, vector<SSphere> *pRes )
{
	CVec3 massCenter;
	vector<SMassSphere> massSpheres;
	GetSpheres( pModel, &massSpheres, &massCenter );
	if ( massSpheres.empty() )
		return false;
	CVec3 tmp;
	for ( int i = 0; i < massSpheres.size(); ++i )
	{
		m.RotateHVector( &tmp, massSpheres[i].ptCenter );
		pRes->push_back( SSphere( tmp, massSpheres[i].fRadius ) );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::GetIdx @0xa52a0: which grid cell a coordinate falls into.
static int GetIdx( float fV, float fMin, float fMax, int n )
{
	int nIdx = Float2Int( ( fV - fMin ) * (float)n / ( fMax - fMin ) );
	if ( nIdx >= n - 1 )
		nIdx = n - 1;
	if ( nIdx < 0 )
		nIdx = 0;
	return nIdx;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::IsStableMine @0xa5300: trace straight down from the mine (IAIMap vtbl+0x18, mask
// 0x8000 = TS_ITEM_BLOCKER); stable while the nearest hit (t >= 0) stays within 0.1 of the mine's
// own level.
static bool IsStableMine( NWorld::IMine *pMine, IAIMap *pMap )
{
	CVec3 pt = pMine->GetMinePos();
	CRay ray;
	ray.ptOrigin = pt;
	ray.ptDir = CVec3( 0, 0, -1 );
	vector<SInterval> hits;
	pMap->Trace( ray, &hits, NWorld::TS_ITEM_BLOCKER );
	float fMin = 1e10f;
	for ( int i = 0; i < hits.size(); ++i )
	{
		float fT = hits[i].enter.fT;
		if ( !( fT < 0 ) )
			fMin = Min( fMin, fT );
	}
	float fHitZ = fMin * ray.ptDir.z + pt.z;
	return fHitZ >= pt.z - 0.1f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SStabObject<T> (44 bytes; ctors @0xa6b70/@0xa6ef0, operator& @0xa8bb0 tags 2/3/4): one
// tracked object with the hull set it was resting on when registered (the "catchers", collected
// via IAIMap vtbl+0x4c SelectHullPointers with include 0x8000 / exclude 0x20000).
template<class T>
struct SStabObject
{
	CPtr<T> pObj;
	vector< CPtr<CObjectBase> > catchers;
	SBound b;
	//
	int operator&( CStructureSaver &f ) { f.Add(2,&pObj); f.Add(3,&catchers); f.Add(4,&b); return 0; }
	SStabObject() {}
	SStabObject( T *_pObj, IAIMap *pMap, const SBound &_b ): pObj(_pObj), b(_b)
	{
		pMap->SelectHullPointers( &catchers, b, NWorld::TS_ITEM_BLOCKER, NWorld::TS_LOCKED_EXTRA );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::CStabilityTracker (96 bytes, save id 0x71812130): one grid cell, registered with the
// AI map octree as an IAIMapTracker over its bound (mask 0x8000, bInformOnDoorFlip) so map changes
// under the cell fire OnChange.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStabilityTracker: public IAIMapTracker
{
	OBJECT_BASIC_METHODS( CStabilityTracker );
	ZDATA
	vector< SStabObject<NWorld::CUnitServer> > corpses;
	vector< SStabObject<NWorld::CDFrozenItem> > debrises;
	vector< SStabObject<NWorld::CObjectServerBase> > objects;
	CPtr<NWorld::IWorld> pWorld;
	SBound b;
	vector< SStabObject<NWorld::IMine> > mines;
	// retail operator& @0xa8a00: tags 2 corpses, 3 debrises, 4 objects, 5 pWorld, 6 b, 7 mines;
	// nDebugNumber is NOT saved.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&corpses); f.Add(3,&debrises); f.Add(4,&objects); f.Add(5,&pWorld); f.Add(6,&b); f.Add(7,&mines); return 0; }
	int nDebugNumber;
	//
	IAIMap* GetMap() const { return pWorld->GetAIMap(); }
	// retail CStabilityTracker::IsInstable<T> @0xa6c80: unstable when any recorded catcher no
	// longer shows up in the current catcher query over the object's bound.
	template<class T>
	bool IsInstable( const SStabObject<T> &so )
	{
		vector< CPtr<CObjectBase> > now;
		GetMap()->SelectHullPointers( &now, so.b, NWorld::TS_ITEM_BLOCKER, NWorld::TS_LOCKED_EXTRA );
		for ( int i = 0; i < so.catchers.size(); ++i )
		{
			bool bFound = false;
			for ( int k = 0; k < now.size(); ++k )
			{
				if ( now[k] == so.catchers[i] )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
				return true;
		}
		return false;
	}
public:
	CStabilityTracker(): nDebugNumber(0) {}
	// retail ctor @0xa78f0: store the world + cell bound, then self-register with the AI map as an
	// IAIMapTracker over the bound: map->AddTracker(this, b, 0x8000, bInformOnDoorFlip=true).
	CStabilityTracker( NWorld::IWorld *_pWorld, const SBound &_b, int _nDebugNumber )
		: pWorld(_pWorld), b(_b), nDebugNumber(_nDebugNumber)
	{
		GetMap()->AddTracker( this, b, NWorld::TS_ITEM_BLOCKER, true );
	}
	//
	void AddCorpse( NWorld::CUnitServer *pUnit, const SBound &bound )
	{
		corpses.push_back( SStabObject<NWorld::CUnitServer>( pUnit, GetMap(), bound ) );
	}
	void AddDebris( NWorld::CDFrozenItem *pDebris, const SBound &bound )
	{
		debrises.push_back( SStabObject<NWorld::CDFrozenItem>( pDebris, GetMap(), bound ) );
	}
	void AddObject( NWorld::CObjectServerBase *pObj, const SBound &bound )
	{
		objects.push_back( SStabObject<NWorld::CObjectServerBase>( pObj, GetMap(), bound ) );
	}
	void AddMine( NWorld::IMine *pMine, const SBound &bound )
	{
		mines.push_back( SStabObject<NWorld::IMine>( pMine, GetMap(), bound ) );
	}
	// retail CStabilityTracker::OnChange @0xa59a0 (IAIMapTracker vtbl+0x10) -- the payoff.
	virtual void OnChange()
	{
		{
			// objects: unstable -> the object re-runs its own CheckStability (which Kills it when
			// unsupported); TRUE keeps it tracked, FALSE drops it.
			vector< SStabObject<NWorld::CObjectServerBase> > keep;
			for ( int i = 0; i < objects.size(); ++i )
			{
				SStabObject<NWorld::CObjectServerBase> &so = objects[i];
				if ( !IsInstable( so ) )
				{
					keep.push_back( so );
					continue;
				}
				NWorld::CObjectServerBase *pObj = so.pObj;
				if ( pObj != 0 && IsValid( pObj ) && pObj->CheckStability() )
					keep.push_back( so );
			}
			objects = keep;
		}
		{
			// corpses: unstable -> re-drop the ragdoll (CUnitAnimator::BeDropped @0x33b1b0);
			// removed from tracking either way (BeStopped re-registers it when it rests again).
			// Retail guards on a server byte +0xbf ("drop already initiated") that is never written
			// anywhere in the retail image -- effectively always clear, so no dev counterpart.
			vector< SStabObject<NWorld::CUnitServer> > keep;
			for ( int i = 0; i < corpses.size(); ++i )
			{
				SStabObject<NWorld::CUnitServer> &so = corpses[i];
				if ( !IsInstable( so ) )
				{
					keep.push_back( so );
					continue;
				}
				NWorld::CUnitServer *pObj = so.pObj;
				if ( pObj != 0 && IsValid( pObj ) )
					pObj->animator.BeDropped( pObj );
			}
			corpses = keep;
		}
		{
			// debris: unstable -> the debris controller re-launches the frozen item as dynamic
			// debris (world vtbl GetDebris -> ActivateDebris(item, map, aim-time node)).
			vector< SStabObject<NWorld::CDFrozenItem> > keep;
			for ( int i = 0; i < debrises.size(); ++i )
			{
				SStabObject<NWorld::CDFrozenItem> &so = debrises[i];
				if ( !IsInstable( so ) )
				{
					keep.push_back( so );
					continue;
				}
				NWorld::CDebrisController *pCtrl = pWorld->GetDebris();
				if ( pCtrl != 0 && so.pObj != 0 && IsValid( so.pObj ) )
					pCtrl->ActivateDebris( so.pObj.GetPtr(), GetMap(), pWorld->GetAimTime() );
			}
			debrises = keep;
		}
		{
			// mines: unstable -> recheck the floor straight down; a mine whose ground dropped away
			// GOES BOOM; removed unless the ground check still passes.
			vector< SStabObject<NWorld::IMine> > keep;
			for ( int i = 0; i < mines.size(); ++i )
			{
				SStabObject<NWorld::IMine> &so = mines[i];
				if ( !IsInstable( so ) )
				{
					keep.push_back( so );
					continue;
				}
				if ( so.pObj == 0 || !IsValid( so.pObj ) )
					continue;
				if ( IsStableMine( so.pObj, GetMap() ) )
					keep.push_back( so );
				else
					NWorld::GoBoom( so.pObj );
			}
			mines = keep;
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::CStabilityTrackers (108 bytes, save id 0x71812131): the grid container. Everything
// registered before FinishConstruction queues into the nc* lists; FinishConstruction sizes the
// grid over the accumulated world box and distributes them.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStabilityTrackers: public IStabilityTrackers
{
	OBJECT_BASIC_METHODS( CStabilityTrackers );
	// retail CArray3D<CObj<CStabilityTracker>> (operator& @0xa8860: tags 2 cells, 3..6 dims).
	struct SGrid
	{
		ZDATA
		vector< CObj<CStabilityTracker> > cells;
		int nXSize, nYSize, nZSize, nXYSize;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&cells); f.Add(3,&nXSize); f.Add(4,&nYSize); f.Add(5,&nZSize); f.Add(6,&nXYSize); return 0; }
		SGrid(): nXSize(0), nYSize(0), nZSize(0), nXYSize(0) {}
	};
	// retail CStabilityTrackers::SObject (36 bytes; operator& @0xa7fd0 tags 2/3/4): a pre-
	// construction queued object registration.
	struct SObject
	{
		ZDATA
		CPtr<NWorld::CObjectServerBase> pObj;
		SBound b;
		int nID;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObj); f.Add(3,&b); f.Add(4,&nID); return 0; }
		SObject(): nID(0) {}
		SObject( NWorld::CObjectServerBase *_pObj, const SBound &_b, int _nID ): pObj(_pObj), b(_b), nID(_nID) {}
	};
	ZDATA
	SGrid trackers;
	CPtr<NWorld::IWorld> pWorld;
	CVec3 ptMin, ptMax;
	bool bConstructed;
	vector<SObject> ncObjects;
	vector< CPtr<NWorld::CDFrozenItem> > ncDebrises;
	vector< CPtr<NWorld::IMine> > ncMines;
	// retail operator& @0xa7e50: tags 2 trackers, 3 pWorld, 4 ptMin, 5 ptMax, 6 bConstructed,
	// 7 ncObjects, 8 ncDebrises, 9 ncMines.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&trackers); f.Add(3,&pWorld); f.Add(4,&ptMin); f.Add(5,&ptMax); f.Add(6,&bConstructed); f.Add(7,&ncObjects); f.Add(8,&ncDebrises); f.Add(9,&ncMines); return 0; }
	//
	IAIMap* GetMap() const { return pWorld->GetAIMap(); }
	// retail CStabilityTrackers::GetTracker @0xa54a0.
	CStabilityTracker* GetTracker( const CVec3 &pt )
	{
		int nX = GetIdx( pt.x, ptMin.x, ptMax.x, trackers.nXSize );
		int nY = GetIdx( pt.y, ptMin.y, ptMax.y, trackers.nYSize );
		int nZ = GetIdx( pt.z, ptMin.z, ptMax.z, trackers.nZSize );
		return trackers.cells[ trackers.nXYSize * nZ + trackers.nXSize * nY + nX ];
	}
	// retail AddObject(server, bound, id) @0xa6090.
	void AddObjectBound( NWorld::CObjectServerBase *pObj, const SBound &b, int nID )
	{
		GetTracker( b.s.ptCenter )->AddObject( pObj, b );
	}
	// retail AddDebrisAfterConstruction @0xa5e60 / AddMineAfterConstruction @0xa5f80: stamp a
	// 0.5-radius unit box at the object's position and file it in the owning grid cell.
	void AddDebrisAfterConstruction( NWorld::CDFrozenItem *pDebris )
	{
		SBound b;
		b.SphereInit( pDebris->GetPos(), 0.5f );
		GetTracker( b.s.ptCenter )->AddDebris( pDebris, b );
	}
	void AddMineAfterConstruction( NWorld::IMine *pMine )
	{
		SBound b;
		b.SphereInit( pMine->GetMinePos(), 0.5f );
		GetTracker( b.s.ptCenter )->AddMine( pMine, b );
	}
public:
	// retail CStabilityTrackers::CStabilityTrackers(IWorld*) @0xa6f70 / CreateStabilityTrackers
	// @0xa5980: empty grid + the inverted sentinel box the first EnlargeMap collapses onto.
	CStabilityTrackers(): bConstructed(false)
	{
		ptMin = CVec3( 1e10f, 1e10f, 1e10f );
		ptMax = CVec3( -1e10f, -1e10f, -1e10f );
	}
	CStabilityTrackers( NWorld::IWorld *_pWorld ): pWorld(_pWorld), bConstructed(false)
	{
		ptMin = CVec3( 1e10f, 1e10f, 1e10f );
		ptMax = CVec3( -1e10f, -1e10f, -1e10f );
	}
	// retail AddCorpse @0xa5db0 (vtbl+0x10; post-construction only).
	virtual void AddCorpse( NWorld::CUnitServer *pUnit, const SBound &b )
	{
		if ( !bConstructed )
			return;
		GetTracker( b.s.ptCenter )->AddCorpse( pUnit, b );
	}
	// retail AddDebris @0xa6770 / AddMine @0xa6840: pre-construction they only queue up.
	virtual void AddDebris( NWorld::CDFrozenItem *pDebris )
	{
		if ( bConstructed )
			AddDebrisAfterConstruction( pDebris );
		else
			ncDebrises.push_back( pDebris );
	}
	virtual void AddMine( NWorld::IMine *pMine )
	{
		if ( bConstructed )
			AddMineAfterConstruction( pMine );
		else
			ncMines.push_back( pMine );
	}
	// retail AddObject(server, model, matrix) @0xa6910 (vtbl+0x18): the bound is the envelope of
	// the model's world-space collision spheres; before construction it queues into ncObjects
	// (and grows the map), after it registers directly.
	virtual void AddObject( NWorld::CObjectServerBase *pObj, NDb::CModel *pModel, const SHMatrix &m )
	{
		vector<SSphere> spheres;
		if ( !GetObjSpheres( pModel, m, &spheres ) )
			return;
		SBoundCalcer calc;
		for ( int i = 0; i < spheres.size(); ++i )
			calc.Add( spheres[i].ptCenter, spheres[i].fRadius );
		SBound b;
		calc.Make( &b );
		int nID = pObj->GetDBObjectID();
		if ( !bConstructed )
		{
			EnlargeMap( calc.ptMin, calc.ptMax );
			ncObjects.push_back( SObject( pObj, b, nID ) );
		}
		else
			AddObjectBound( pObj, b, nID );
	}
	// retail EnlargeMap @0xa5190 (vtbl+0x20; before construction only).
	virtual void EnlargeMap( const CVec3 &_ptMin, const CVec3 &_ptMax )
	{
		if ( bConstructed )
			return;
		ptMin.Minimize( _ptMin );
		ptMax.Maximize( _ptMax );
	}
	// retail FinishConstruction @0xa6230 (vtbl+0x24): the grid is round(extent/2) clamped to
	// [1,16]x[1,16]x[1,8] cells, each cell bound inflated by 1.0 per half-axis; the queued nc*
	// lists distribute and clear.
	virtual void FinishConstruction()
	{
		if ( ptMax.x < ptMin.x )
		{
			// retail falls back to the path network's bounds (net+0xb4) for an empty map; the dev
			// IPathNetwork exposes no bounds -- unreachable in practice (CAIMap::InsertHull grows
			// the box for every static hull), so degrade to a unit box.
			ptMin = CVec3( 0, 0, 0 );
			ptMax = CVec3( 1, 1, 1 );
		}
		int nX = Float2Int( ( ptMax.x - ptMin.x ) * 0.5f );
		int nY = Float2Int( ( ptMax.y - ptMin.y ) * 0.5f );
		int nZ = Float2Int( ( ptMax.z - ptMin.z ) * 0.5f );
		if ( nX > 16 ) nX = 16;
		if ( nY > 16 ) nY = 16;
		if ( nZ > 8 ) nZ = 8;
		if ( nX < 2 ) nX = 1;
		if ( nY < 2 ) nY = 1;
		if ( nZ < 2 ) nZ = 1;
		trackers.nXSize = nX;
		trackers.nYSize = nY;
		trackers.nZSize = nZ;
		trackers.nXYSize = nX * nY;
		trackers.cells.resize( nX * nY * nZ );
		float fDX = ( ptMax.x - ptMin.x ) / (float)nX;
		float fDY = ( ptMax.y - ptMin.y ) / (float)nY;
		float fDZ = ( ptMax.z - ptMin.z ) / (float)nZ;
		int nDebug = 0;
		for ( int z = 0; z < nZ; ++z )
		{
			for ( int y = 0; y < nY; ++y )
			{
				for ( int x = 0; x < nX; ++x )
				{
					CVec3 lo( ptMin.x + x * fDX, ptMin.y + y * fDY, ptMin.z + z * fDZ );
					CVec3 hi( ptMin.x + ( x + 1 ) * fDX, ptMin.y + ( y + 1 ) * fDY, ptMin.z + ( z + 1 ) * fDZ );
					SBound b;
					b.BoxInit( lo, hi );
					b.Extend( 1.0f );
					++nDebug;
					trackers.cells[ trackers.nXYSize * z + trackers.nXSize * y + x ] =
						new CStabilityTracker( pWorld, b, nDebug );
				}
			}
		}
		bConstructed = true;
		for ( int i = 0; i < ncObjects.size(); ++i )
			AddObjectBound( ncObjects[i].pObj, ncObjects[i].b, ncObjects[i].nID );
		ncObjects.clear();
		for ( int i = 0; i < ncDebrises.size(); ++i )
		{
			if ( ncDebrises[i] != 0 && IsValid( ncDebrises[i].GetPtr() ) )
				AddDebrisAfterConstruction( ncDebrises[i] );
		}
		ncDebrises.clear();
		for ( int i = 0; i < ncMines.size(); ++i )
		{
			if ( ncMines[i] != 0 && IsValid( ncMines[i].GetPtr() ) )
				AddMineAfterConstruction( ncMines[i] );
		}
		ncMines.clear();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IStabilityTrackers* CreateStabilityTrackers( NWorld::IWorld *pWorld )
{
	return new CStabilityTrackers( pWorld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x71812130, CStabilityTracker )
REGISTER_SAVELOAD_CLASS( 0x71812131, CStabilityTrackers )
