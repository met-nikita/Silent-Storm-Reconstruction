#ifndef __aiCollidersCommon_H_
#define __aiCollidersCommon_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataRPG.h"
namespace NAI
{
struct SConvexHull;
const float FP_NO_COLLISION = NCollider::F_NO_COLLISION;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSourceInfo;
struct SColliderUserInfo
{
	const SSourceInfo *pSrc;
	int nUserID;

	SColliderUserInfo( const SSourceInfo *_pSrc, int _nUserID ): pSrc(_pSrc), nUserID(_nUserID) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCollisionPoint
{
	float fDist;
	CVec3 pt;
	const SSourceInfo *pSrc;
	int nUserID;
	SPlane plane;

	SCollisionPoint() {}
	SCollisionPoint( float _fDist, const CVec3 &_pt, const SSourceInfo *_pSrc, int _nUserID, const SPlane &_plane )
		: fDist(_fDist), pt(_pt), pSrc(_pSrc), nUserID(_nUserID), plane(_plane) {}
	int operator&( CStructureSaver &f ) { ASSERT(0&&"This struct could not be serialized!"); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPrepareCollider
{
public:
	virtual void SetBoundAndResolution( const SBound &b, float fLeng ) = 0;
	virtual void AddConvexHull( const SConvexHull &h ) = 0;
};
}
#endif