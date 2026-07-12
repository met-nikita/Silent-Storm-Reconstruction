#ifndef __aiStability_H_
#define __aiStability_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CModel;
}
namespace NWorld
{
	struct IVisObj;
	class IWorld;
	class IMine;
	class CUnitServer;
	class CObjectServerBase;
	class CDFrozenItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IAIMap;
bool CheckObjectStability( IAIMap *pMap, NDb::CModel *pModel, const SHMatrix &_pos, CObjectBase *pIgnore );
////////////////////////////////////////////////////////////////////////////////////////////////////
// The wreckage physics watchdog (retail aiStability.obj): a 3D grid of per-cell IAIMapTracker
// listeners over the map. Props, corpses, debris and mines register the bound they are RESTING on;
// when the AI map changes under a cell (octree change broadcast -> OnChange), anything whose
// support ("catchers") disappeared reacts: objects re-run CObjectServerBase::CheckStability,
// corpses re-drop (CUnitAnimator::BeDropped), frozen debris re-launches as dynamic debris, and
// mines whose floor went away explode.
// Retail IStabilityTrackers vftable @0x4b4840: +0x10 AddCorpse, +0x14 AddDebris, +0x18 AddObject,
// +0x1c AddMine, +0x20 EnlargeMap, +0x24 FinishConstruction (keep this virtual order).
////////////////////////////////////////////////////////////////////////////////////////////////////
class IStabilityTrackers: public CObjectBase
{
public:
	virtual void AddCorpse( NWorld::CUnitServer *pUnit, const SBound &b ) = 0;                       // retail @0xa5db0
	virtual void AddDebris( NWorld::CDFrozenItem *pDebris ) = 0;                                     // retail @0xa6770
	virtual void AddObject( NWorld::CObjectServerBase *pObj, NDb::CModel *pModel, const SHMatrix &m ) = 0; // retail @0xa6910
	virtual void AddMine( NWorld::IMine *pMine ) = 0;                                                // retail @0xa6840
	virtual void EnlargeMap( const CVec3 &ptMin, const CVec3 &ptMax ) = 0;                           // retail @0xa5190
	virtual void FinishConstruction() = 0;                                                           // retail @0xa6230
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NAI::CreateStabilityTrackers @0xa5980
IStabilityTrackers* CreateStabilityTrackers( NWorld::IWorld *pWorld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
