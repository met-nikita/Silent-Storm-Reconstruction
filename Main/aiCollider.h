#ifndef __aiCollider_H_
#define __aiCollider_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PrecalcCollider.h"
#include "aiCollidersCommon.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCollider is the release's boolean collider over precomputed CPrecalcSpheres (was CBSPCollider).
// Impact-returning queries (CollideInfo) live on CPhysCollider:CUserCollider; here we keep only the
// boolean checks (CollideBool/DoesIntersect inherited, CollideCheck wraps CollideBool).
class CCollider : public NCollider::CPrecalcCollider<SColliderUserInfo>, public IPrepareCollider, public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CCollider);
	unordered_map<CPtr<CObjectBase>,bool,SPtrHash> ignoredObjects;
public:
	virtual void AddConvexHull( const SConvexHull &h );
	virtual void SetBoundAndResolution( const SBound &b, float fLeng );
	void CollideCheck( const vector<SSphere> &spheres, const vector<CVec3> &velocities, vector<char> *pRes );
	void AddIgnoredUserData( CObjectBase *p ) { ignoredObjects[p]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
