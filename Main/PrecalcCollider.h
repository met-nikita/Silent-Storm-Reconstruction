#ifndef __PrecalcCollider_H_
#define __PrecalcCollider_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "aiObject.h"
#include "VolumeContainer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPrecalcCollider - the release's collision engine, sibling of CBSPCollider (BSPCollider.h).
// Same CVolumeContainer broad-phase, same query interface; only the per-entity payload differs:
// each entity holds a CPtr<NAI::CPrecalcSpheres> voxel grid instead of a CPtr<CBSPTree>, and the
// narrow phase calls IsSphereCollided / IsMovingSphereCollided instead of the BSP CollideCheck*.
// Reconstructed verbatim from the release Game.exe (NCollider::CPrecalcCollider<NAI::SColliderUserInfo>:
// SEntity ctor @0042b3e0, PrepareCollider @0042b530, AddEntity @0042b6d0, CollideBool @0047c3e0,
// DoesIntersect @0059b440). The grid is boolean, so this collider intentionally has NO impact-returning
// Collide<T> (CCollider::CollideInfo handles impacts separately).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NCollider
{
bool DoesTriSphereIntersect( const CVec3 &a1, const CVec3 &b1, const CVec3 &c1,
	const CVec3 &ptCenter, float fR );
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUserData>
class CPrecalcCollider
{
	struct SEntity
	{
		SBound bound;
		CPtr<NAI::CPrecalcSpheres> pData;
		SHMatrix pos, fwd;
		SEntity( const CPtr<NAI::CPrecalcSpheres> &_pData, const SHMatrix &_pos, const SHMatrix &_fwd,
			const SBound &_bound )
			: bound(_bound), pData(_pData), pos(_pos), fwd(_fwd)
		{
			// pad the broad-phase bound by the test-sphere radius (matches SEntity ctor @0042b3e0)
			bound.ptHalfBox.x += NAI::F_TEST_SPHERE_RADIUS;
			bound.ptHalfBox.y += NAI::F_TEST_SPHERE_RADIUS;
			bound.ptHalfBox.z += NAI::F_TEST_SPHERE_RADIUS;
			bound.s.fRadius = fabs( bound.ptHalfBox );
		}
	};
	vector<SEntity> entities;
	vector<TUserData> userData;
	CVolumeContainer volume;
public:
	void PrepareCollider( const SBound &b, float fLeng )
	{
		entities.clear();
		userData.clear();
		volume.Init( b.s.ptCenter - b.ptHalfBox, b.s.ptCenter + b.ptHalfBox, fLeng );
	}
	void AddEntity( const SHMatrix &pos, const SHMatrix &fwd,
		vector<CPtr<NAI::CPrecalcSpheres> > spheres, bool bTerrain, const TUserData &ud,
		const SBound &bound )
	{
		// one entity per precalc-sphere Z-block; all share the whole-hull broad-phase volume.
		for ( unsigned int i = 0; i < spheres.size(); ++i )
		{
			userData.push_back( ud );
			entities.push_back( SEntity( spheres[i], pos, fwd, bound ) );
			CVolumeContainer::SVolumeBounds bounds;
			volume.MakeVolume( &bounds, bound );
			if ( bTerrain )
				bounds.mn.z = 0;
			if ( volume.IsOut( bounds ) )
				return;
			volume.ClipVolume( &bounds );
			int nIndex = entities.size() - 1;
			volume.Add( bounds, nIndex );
		}
	}
	template<class TResultAnalyzer>
		bool CollideBool( const SSphere &sphere, const CVec3 &vel, TResultAnalyzer *pResult )
	{
		SBound b;
		CVec3 ptHalfBox( fabs(vel.x) * 0.5f, fabs(vel.y) * 0.5f, fabs(vel.z) * 0.5f );
		b.BoxExInit( sphere.ptCenter + vel * 0.5f, ptHalfBox );
		b.Extend( sphere.fRadius );
		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();
		for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SEntity &e = entities[ fetchRes[i] ];
			if ( ::DoesIntersect( b, e.bound ) )
			{
				CVec3 realCenter, realVel;
				e.pos.RotateHVector( &realCenter, sphere.ptCenter );
				e.pos.RotateVector( &realVel, vel );
				if ( e.pData->IsMovingSphereCollided( realCenter, realCenter + realVel ) )
				{
					if ( (*pResult)( userData[ fetchRes[i] ] ) )
						return true;
				}
			}
		}
		return false;
	}
	template<class TResultAnalyzer>
		bool DoesIntersect( const CVec3 &ptCenter, float fRadius, TResultAnalyzer *pResult )
	{
		SBound b;
		b.SphereInit( ptCenter, fRadius );
		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();
		for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SEntity &e = entities[ fetchRes[i] ];
			if ( e.bound.IsInside( ptCenter ) )
			{
				CVec3 realCenter;
				e.pos.RotateHVector( &realCenter, ptCenter );
				if ( e.pData->IsSphereCollided( realCenter ) )
				{
					if ( (*pResult)( userData[ fetchRes[i] ] ) )
						return true;
				}
			}
		}
		return false;
	}
	bool DoesIntersect( const CVec3 &ptCenter, float fR )
	{
		return DoesIntersect( ptCenter, fR, &skipAnalyzer );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
