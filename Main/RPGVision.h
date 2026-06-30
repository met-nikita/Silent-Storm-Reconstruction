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
	virtual bool IsCubeVisible( const CVec3 &ptFrom, const CVec3 &ptTarget, const CVec3 &ptForward ) = 0;
	virtual EVoxelVisionState GetVision( int x, int y, int z ) = 0;
	virtual void GetCoord( const CVec3 &vPoint, CTPoint3<int> *pRes ) = 0;
	virtual void GetCenter( const CTPoint3<int> &p, CVec3 *pRes ) = 0;
};
IVisionTracker* CreateVisionTracker( NAI::IAIMap *pAIMap );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif