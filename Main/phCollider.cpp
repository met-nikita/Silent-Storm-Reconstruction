#include "StdAfx.h"
#include "phCollider.h"
#include "Bound.h"
#include "aiObject.h"
#include "aiMap.h"
#include "../dbformat/DataRPG.h"
#include "wTSFlags.h"

// wOSBase.obj @0x347bd0 -- breakable-glass collider gate (defined in wOSBase.cpp). Declared here so the
// friction collider can let a grenade/blast pass THROUGH a breakable pane (and break it) instead of
// colliding solidly. Forward-declared (not #include "wOSBase.h") to avoid pulling its heavy deps.
namespace NAI { struct SSourceInfo; }
namespace NWorld { bool CheckItemsBreakGlass( const NAI::SSourceInfo *pSrc ); }

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPhysCollider
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::CalcEnclosingBound( SBound *pRes, const vector<SSphere> &spheres, const vector<CVec3> &velocities )
{
	ASSERT( spheres.size() == velocities.size() );
	SBoundCalcer bc;
	for ( int k = 0; k < spheres.size(); ++k )
	{
		const SSphere &s = spheres[k];
		bc.Add( s.ptCenter, s.fRadius );
		bc.Add( s.ptCenter + velocities[k], s.fRadius );
	}
	bc.Make( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::SetBoundAndResolution( const SBound &b, float fLeng )
{
	PrepareCollider( b, fLeng );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::AddConvexHull( const SConvexHull &h )
{
	if ( h.points.empty() )
		return;
	if ( !ignoredObjects.empty() && ignoredObjects.find( h.src.pUserData.GetPtr() ) != ignoredObjects.end() )
		return;
	const SFBTransform &trans = h.trans;
	vector<STriangle> ts;
	h.tris.BuildTriangleList( &ts );
	SColliderUserInfo user( &h.src, h.nUserID );
	AddEntity( trans.forward, h.points, ts, user );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int nCollidedTris;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFrictionCollider
{
	bool bCollision, bTerrain;
	CVec3 ptClosest;
	float fMin;
	int nFloor;
	
	SFrictionCollider(): bCollision(false), fMin(1e30f) {}
	bool operator()( float fDist, const CVec3 &ptCollision, const SPlane &plane, const SColliderUserInfo &info )
	{
		// retail @0x286450 (NAI::SFrictionCollider::operator()): gate the whole collision on the breakable-glass
		// check (retail calls it unconditionally for EVERY contact here, incl. normal unit-movement sliding, not
		// just grenades). When a sphere strikes a pane flagged bBreakableByGrenade, CheckItemsBreakGlass breaks it
		// (bumps its destroy stage) and returns false -> we DON'T register the contact, so the sphere passes
		// through the glass. Every non-breakable contact returns true, so ordinary collisions are unchanged.
		if ( !NWorld::CheckItemsBreakGlass( info.pSrc ) )
			return false;
		bCollision = true;
		if ( fDist < fMin )
		{
			fMin = fDist;
			ptClosest = ptCollision;
			bTerrain = info.pSrc->pUserData == 0 && fabs( plane.n.z ) > 0.5f;
			nFloor = info.pSrc->nFloor;
		}
		return false;
	}
};
void CPhysCollider::CollideSphere( SSphere *pSphere, const CVec3 &_vel )
{
	SSphere &sphere = *pSphere;
	CVec3 vel(_vel);
	int nIteration = 0; // just for case :)
	while ( fabs2(vel) > 1e-8f && ++nIteration < 10 )
	{
		SFrictionCollider fc;
		Collide( sphere, vel, &fc ); 		
		if ( fc.bCollision )
		{
			nMinFloor = Min( nMinFloor, fc.nFloor );
			CVec3 realVel(vel);
			Normalize(&realVel);
			realVel = fc.fMin * realVel;

			sphere.ptCenter += realVel; // move sphere
			vel -= realVel;
			CVec3 normPlane = fc.ptClosest - sphere.ptCenter;
			Normalize(&normPlane);
			float scal = vel * normPlane;
			vel = vel - (vel * normPlane) * normPlane; // project on sliding plane
			if ( fc.bTerrain )
				vel = VNULL3;
			else
			{
				// friction
				float fSlow = Min( 0.4f * fabs(scal), fabs(vel) );
				//fSlow = 0;
				realVel = vel;
				Normalize(&realVel);
				vel -= fSlow * realVel;
			}
		}
		else
		{
			sphere.ptCenter += vel;
			vel = VNULL3;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::CollideSliding( const vector<SSphere> &spheres, 
	const vector<CVec3> &velocities, vector<CVec3> *pNewPositions )
{
	if ( spheres.empty() )
		return;
	ASSERT( spheres.size() == velocities.size() );
	pNewPositions->clear();
	nCollidedTris = 0;
	for ( int i=0; i<spheres.size(); ++i )
	{
		SSphere sphere( spheres[i] );
		CollideSphere( &sphere, velocities[i] );
		pNewPositions->push_back( sphere.ptCenter );
	}
/*	char buf[1024];
	sprintf( buf, "avrg tris/sphere %f\n", ((float)nCollidedTris) / spheres.size() );
	OutputDebugString( buf );*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInfoCollider
{
	bool bCollision;
	float fMin;
	CVec3 ptClosest;
	const SColliderUserInfo *pInfo;
	SPlane plane;

	SInfoCollider(): bCollision(false), fMin(1e30f) {}
	bool operator()( float fDist, const CVec3 &ptCollision, const SPlane &_plane, const SColliderUserInfo &info )
	{
		fMin = fMin; // keep compiler happy (skipping this line leads to odd internal compiler error on msvc6sp5
		bCollision = true;
		if ( fDist < fMin )
		{
			fMin = fDist;
			pInfo = &info;
			ptClosest = ptCollision;
			plane = _plane;
		}
		return false; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::CollideInfo( const SSphere &sphere, const CVec3 &vel, SCollisionPoint *pRes )
{
	SInfoCollider ic;
	Collide( sphere, vel, &ic );
	if ( !ic.bCollision )
		pRes->fDist = NCollider::F_NO_COLLISION;
	else
		*pRes = SCollisionPoint( ic.fMin, ic.ptClosest, ic.pInfo->pSrc, ic.pInfo->nUserID, ic.plane );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPhysCollider::CollideInfo( const vector<SSphere> &spheres, 
	const vector<CVec3> &velocities, vector<SCollisionPoint> *pRes )
{
	if ( spheres.empty() )
		return;
	ASSERT( spheres.size() == velocities.size() );
	pRes->resize( spheres.size() );
	nCollidedTris = 0;
	for ( int i=0; i<spheres.size(); ++i )
	{
		const SSphere &sphere = spheres[i];
		const CVec3 &vel = velocities[i];
		CollideInfo( sphere, vel, &(*pRes)[i] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static float CalcMax( const vector<SSphere> &spheres, const vector<CVec3> &velocities )
{
	float fMax = 0;
	for ( int i = 0; i < spheres.size(); ++i )
		fMax = Max( Max( sqr( spheres[i].fRadius ), fMax ), fabs2( velocities[i] ) );
	return sqrt( fMax );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PhysCollideSliding( IAIMap *pMap, const vector<SSphere> &spheres, const vector<CVec3> &velocities, 
	vector<CVec3> *pNewPositions, int nFlags, int *pnMinFloor )
{
	if ( spheres.empty() )
		return;
	CPhysCollider collider;
	float fMax = CalcMax( spheres, velocities );

	SBound bound;
	collider.CalcEnclosingBound( &bound, spheres, velocities );
	bound.Extend( fMax * 2 );
	pMap->PrepareCollider( &collider, bound, fMax * 2, nFlags );
	collider.CollideSliding( spheres, velocities, pNewPositions );
	if ( pnMinFloor )
		*pnMinFloor = collider.GetMinFloor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PhysCollideInfo( IAIMap *pMap, const vector<SSphere> &spheres, const vector<CVec3> &velocities, 
	vector<SCollisionPoint> *pRes, int nFlags )
{
	if ( spheres.empty() )
		return;
	CPhysCollider collider;
	float fMax = CalcMax( spheres, velocities );
	
	SBound bound;
	collider.CalcEnclosingBound( &bound, spheres, velocities );
	pMap->PrepareCollider( &collider, bound, fMax * 2, nFlags );
	collider.CollideInfo( spheres, velocities, pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
