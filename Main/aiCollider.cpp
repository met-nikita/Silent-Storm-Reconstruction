#include "StdAfx.h"
#include "aiCollider.h"
#include "Bound.h"
#include "aiObject.h"
#include "aiMap.h"
#include "../dbformat/DataRPG.h"
#include "wTSFlags.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCollider
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCollider::SetBoundAndResolution( const SBound &b, float fLeng )
{
	PrepareCollider( b, fLeng );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CCollider::AddConvexHull @0042ad40: build the whole-hull bound from forward-transformed
// points, then feed the precomputed CPrecalcSpheres (or bake them on the fly if the hull has none)
// into the precalc broad-phase.
void CCollider::AddConvexHull( const SConvexHull &h )
{
	if ( h.points.empty() )
		return;
	// release @0042ad40: the precalc collider only holds OBJECTS (pUserData != 0). Terrain
	// (pUserData == 0) is excluded here - its walkability comes from the height grid the move/pass
	// calcers build directly, not from the collider. Adding it would voxelize the whole terrain patch
	// per collider-prepare, which is the load-time CPU sink the profiler caught in GeneratePrecalcSpheres.
	if ( h.src.pUserData == 0 )
		return;
	if ( !ignoredObjects.empty() && ignoredObjects.find( h.src.pUserData.GetPtr() ) != ignoredObjects.end() )
		return;
	SColliderUserInfo user( &h.src, h.nUserID );
	SBoundCalcer bc;
	for ( unsigned int i = 0; i < h.points.size(); ++i )
	{
		CVec3 pt;
		h.trans.forward.RotateHVector( &pt, h.points[i] );
		bc.Add( pt );
	}
	SBound bound;
	bc.Make( &bound );
	if ( h.precalc.empty() )
	{
		vector<STriangle> tris;
		h.tris.BuildTriangleList( &tris );
		vector<CPtr<CPrecalcSpheres> > tmp;
		GeneratePrecalcSpheres( &tmp, h.points, tris );
		AddEntity( h.trans.backward, h.trans.forward, tmp, false, user, bound );
	}
	else
		AddEntity( h.trans.backward, h.trans.forward, h.precalc, false, user, bound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBoolCollider
{
	bool bCollision;
	
	SBoolCollider(): bCollision(false) {}
	bool operator()( const SColliderUserInfo &info )
	{ 
		bCollision = true;
		return true;
	}
};
void CCollider::CollideCheck( const vector<SSphere> &spheres, 
	const vector<CVec3> &velocities, vector<char> *pRes )
{
	if ( spheres.empty() )
		return;
	ASSERT( spheres.size() == velocities.size() );
	pRes->resize( spheres.size() );
	SBoolCollider bc;
	for ( unsigned int i = 0; i < spheres.size(); ++i )
		(*pRes)[i] = CollideBool( spheres[i], velocities[i], &bc );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NB: the free NAI::CollideInfo(IAIMap*,...) impact helper was removed - the release deleted it and
// routes those callers through CPhysCollider/PhysCollideInfo (phCollider).
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
BASIC_REGISTER_CLASS(CCollider);
