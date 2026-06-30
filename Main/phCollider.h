#ifndef __phCollider_H_
#define __phCollider_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SuperCollider.h"
#include "aiCollidersCommon.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPhysCollider: public NCollider::CUserCollider<SColliderUserInfo>, public IPrepareCollider
{
	int nMinFloor;
	unordered_map<CPtr<CObjectBase>,bool,SPtrHash> ignoredObjects;

	void CollideSphere( SSphere *pSphere, const CVec3 &vel );
public:
	CPhysCollider() : nMinFloor(100) {}
	virtual void AddConvexHull( const SConvexHull &h );
	virtual void SetBoundAndResolution( const SBound &b, float fLeng );
	void CalcEnclosingBound( SBound *pRes, const vector<SSphere> &spheres, const vector<CVec3> &velocities );
	void CollideSliding( const vector<SSphere> &spheres, const vector<CVec3> &velocities, vector<CVec3> *pNewPositions );
	void CollideInfo( const vector<SSphere> &spheres, const vector<CVec3> &velocities, vector<SCollisionPoint> *pRes );
	void CollideInfo( const SSphere &sphere, const CVec3 &vel, SCollisionPoint *pRes );
	int GetMinFloor() const { return nMinFloor; }
	void AddIgnoredUserData( CObjectBase *p ) { ignoredObjects[p]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIMap;
void PhysCollideSliding( IAIMap *pMap, const vector<SSphere> &spheres, const vector<CVec3> &velocities, 
	vector<CVec3> *pNewPositions, int nFlags, int *pnMinFloor );
void PhysCollideInfo( IAIMap *pMap, const vector<SSphere> &spheres, const vector<CVec3> &velocities, 
	vector<SCollisionPoint> *pRes, int nFlags );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif