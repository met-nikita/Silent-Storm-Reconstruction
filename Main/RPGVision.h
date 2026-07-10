#ifndef __RPGVision_H_
#define __RPGVision_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class IAIMap;
	class IPathNetwork;
}
namespace NWorld
{
	class CUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
class CTPoint3
{
public:
	union
	{
		struct { T x, y, z; };
		struct { T m[3]; };
	};

	CTPoint3() {}
	CTPoint3( const T &_x, const T &_y, const T &_z ) : x(_x), y(_y), z(_z) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
const int N_SIGHTDISTANCE = 20;
inline float GetMaxSightDistance() { return 40.0f; }   // retail NRPG::GetMaxSightDistance @0x2ba2e0 (imm 0x42200000)
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EVoxelVisionState
{
	VVS_NONE,
	VVS_TRANSPARENT,
	VVS_SOLID
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IVisionTracker : public CObjectBase
{
public:
	// LEGACY 3-arg form (Jan03): hardcoded N_SIGHTDISTANCE + 3D half-space + 9-ray majority. Remaining
	// consumers after the legacy-range convergence (GetVisibilityArea/CanSee now use the real per-unit
	// range/FOV): the dev-only 2-arg CheckPositionVisibility (aiSignal, gutted layer) and CGame::IsVisible
	// (mines/multi-point item probes -- retail per-DC radius is a documented residual).
	virtual bool IsCubeVisible( const CVec3 &ptFrom, const CVec3 &ptTarget, const CVec3 &ptForward ) = 0;
	// retail CVisionTracker::IsCubeVisible @0x2c9160 (vision vtbl+0x10) -- THE perception leaf: the
	// MakeVisionQuery @0x2c8040 gate (2D FOV cone off cos(FOV/2), height-stretched effective range
	// capped at 2x -- NRPG::GetSightDistance @0x2c7f50) then the CACHED 9-point jittered-ray majority
	// (> 3 of 9; cache key {from,to,effRange,cosHalfFOV}). The retail 4-arg CGame::CheckPositionVisibility
	// @0x298fa0 calls THIS per occupied cube (disasm: vision vtbl+0x10 -- the earlier "single ray
	// IsPointVisible" reading of that leaf was wrong).
	virtual bool IsCubeVisible( const CVec3 &ptFrom, const CVec3 &ptTarget, const CVec3 &ptForward,
		float fRange, float fCosHalfFOV ) = 0;
	// retail CVisionTracker::IsPointVisible @0x2c9440 (vision vtbl+0x14) -- the SINGLE-RAY point test with
	// the same MakeVisionQuery gates (retail consumer: CGame::CanSeeCenter @0x298750).
	virtual bool IsPointVisible( const CVec3 &ptFrom, const CVec3 &ptTarget, const CVec3 &ptForward,
		float fRange, float fCosHalfFOV ) = 0;
	// retail CVisionTracker::IsWithinSightRange @0x2c7fa0 (vision vtbl+0x28): the cheap pre-cull used by
	// GetVisibilityArea @0x298840 -- dist^2 < eff^2 where eff is the same height-stretched, 2x-capped
	// effective range MakeVisionQuery uses.
	virtual bool IsWithinSightRange( const CVec3 &ptFrom, const CVec3 &ptTarget, float fRange ) = 0;
	virtual EVoxelVisionState GetVision( int x, int y, int z ) = 0;
	virtual void GetCoord( const CVec3 &vPoint, CTPoint3<int> *pRes ) = 0;
	virtual void GetCenter( const CTPoint3<int> &p, CVec3 *pRes ) = 0;
};
IVisionTracker* CreateVisionTracker( NAI::IAIMap *pAIMap );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif