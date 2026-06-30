#ifndef __BSPCollider_H_
#define __BSPCollider_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "BSPTree.h"
#include "VolumeContainer.h"
namespace NCollider
{
struct SCollideParams
{
	CVec3 ptStart;
	CVec3 ptStop;
	SCollideParams( const CVec3 &v1, const CVec3 &v2 ) : ptStart(v1), ptStop(v2) {}
	bool operator==( const SCollideParams &cp ) const 
	{ 
		return ( ptStart == cp.ptStart && ptStop == cp.ptStop ); 
				//|| ( ptStop == cp.ptStart && ptStart == cp.ptStop ); 
	}
};
struct SCollideParamsHash
{
int operator()( const SCollideParams &a ) const { const int *p = (const int*)&a; return p[0] ^ p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5]; }
};
const float F_SELECTED_SPHERE_RADIUS = 0.31f;
externA5 unordered_map<CVec3, bool, SVec3Hash> colliderHash;
externA5 unordered_map<SCollideParams, bool, SCollideParamsHash> colliderHashMov;
externA5 int nRepeatedCalls;
externA5 int nTotalCalls;
externA5 int nRepeatedCallsMov;
externA5 int nTotalCallsMov;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DoesTriSphereIntersect( const CVec3 &a1, const CVec3 &b1, const CVec3 &c1,
	const CVec3 &ptCenter, float fR );
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUserData>
class CBSPCollider
{	
	struct SBSPEntity
	{
		CPtr<NAI::CBSPTree> pTree;
		SHMatrix pos, fwd;
		float fRMult;
		SBSPEntity( const CPtr<NAI::CBSPTree> &_pTree, const SHMatrix &_pos, const SHMatrix &_fwd ) 
			: pos(_pos), pTree(_pTree), fwd(_fwd) 
		{
			fRMult = Max( sqr( pos._11 ) + sqr( pos._21 ) + sqr( pos._31 ), sqr( pos._12 ) + sqr( pos._22 ) + sqr( pos._32 ) );
			fRMult = Max( fRMult, sqr( pos._13 ) + sqr( pos._23 ) + sqr( pos._33 ) );
			fRMult = sqrt( fRMult );	
		}
	};
	vector<SBSPEntity> entities;
	vector<TUserData> userData;
	CVolumeContainer volume;
public:
#ifdef _BSP_DEBUG
	vector<CVec3> drawPoints;
	vector<char> colors;
	bool bNeedDraw;
#endif
	void PrepareCollider( const SBound &b, float fLeng )
	{
		entities.clear();
		userData.clear();
		volume.Init( b.s.ptCenter - b.ptHalfBox, b.s.ptCenter + b.ptHalfBox, fLeng );
#ifdef _BSP_DEBUG
		bNeedDraw = false;
#endif
	}
	void AddEntity( const SHMatrix &pos, const SHMatrix &fwd, const CPtr<NAI::CBSPTree> &pTree, 
		const vector<CVec3> &_points, bool bTerrain, const TUserData &ud )
	{
		userData.push_back( ud );
		entities.push_back( SBSPEntity( pTree, pos, fwd ) );
		CVolumeContainer::SVolumeBounds bounds;
		volume.MakeVolume( &bounds, fwd, _points );
		if ( bTerrain )
			bounds.mn.z = 0;
		if ( volume.IsOut( bounds ) )
			return;
		volume.ClipVolume( &bounds );
		int nIndex = userData.size() - 1;
		volume.Add( bounds, nIndex );			
	}
	template<class TResultAnalyzer>
		void Collide( const SSphere &sphere, const CVec3 &vel, TResultAnalyzer *pResult )
	{
		CVec3 ptCollision;
		SBound b;
		CVec3 ptHalfBox( fabs(vel.x) * 0.5f, fabs(vel.y) * 0.5f, fabs(vel.z) * 0.5f );
		b.BoxExInit( sphere.ptCenter + vel * 0.5f, ptHalfBox );
		b.Extend( sphere.fRadius );
		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();
 
		/*for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SBSPEntity &e = entities[ fetchRes[i] ];*/
		for ( unsigned int i = 0; i < entities.size(); ++i )
		{
			SBSPEntity &e = entities[ i ];
			SHMatrix &pos = e.pos;
      float fR = sphere.fRadius * e.fRMult;
			CVec3 realCenter, realVel;
			pos.RotateHVector( &realCenter, sphere.ptCenter );
			pos.RotateVector( &realVel, vel );
			SPlane plane;
			if ( e.pTree->CollideCheck( realCenter, realCenter + realVel, fR, &ptCollision, &plane ) )
			{
				CVec3 ptRealCollision;
				e.fwd.RotateHVector( &ptRealCollision, ptCollision );
				float fDist = fabs( ptRealCollision - sphere.ptCenter );
				SPlane realPlane;
				e.fwd.RotateVector( &realPlane.n, plane.n );

				CVec3 ptSurelyOnPlane = - plane.n * plane.d;
				CVec3 ptRealSurelyOnPlane;
				e.fwd.RotateHVector( &ptRealSurelyOnPlane, ptSurelyOnPlane  );
				realPlane.d = - ptRealSurelyOnPlane * realPlane.n;

				float fSphDist = realPlane.GetDistanceToPoint( ptRealCollision );
				ptRealCollision -= realPlane.n * fSphDist; 
				//if ( (*pResult)( fDist2, ptRealCollision, realPlane, userData[ fetchRes[i] ] ) )
				if ( (*pResult)( fDist, ptRealCollision, realPlane, userData[ i ] ) )
					break;
			};
		}
	}
	template<class TResultAnalyzer>
		bool CollideBool( const SSphere &sphere, const CVec3 &vel, TResultAnalyzer *pResult )
	{
		/*if ( sphere.fRadius == F_SELECTED_SPHERE_RADIUS )
		{
			SCollideParams cp( sphere.ptCenter, sphere.ptCenter + vel );
			if ( colliderHashMov.find( cp ) != colliderHashMov.end() )
				++nRepeatedCallsMov;
			else
				colliderHashMov[ cp ] = true;
			++nTotalCallsMov;
		}*/
		SBound b;
		CVec3 ptHalfBox( fabs(vel.x) * 0.5f, fabs(vel.y) * 0.5f, fabs(vel.z) * 0.5f );
		b.BoxExInit( sphere.ptCenter + vel * 0.5f, ptHalfBox );
		b.Extend( sphere.fRadius );
		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();

		for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SBSPEntity &e = entities[ fetchRes[i] ];
			SHMatrix &pos = e.pos;
			float fR = sphere.fRadius * e.fRMult;
			CVec3 realCenter, realVel;
			pos.RotateHVector( &realCenter, sphere.ptCenter );
			pos.RotateVector( &realVel, vel );
			if ( e.pTree->CollideCheckNoImpact( realCenter, realCenter + realVel, fR ) )
			{
				if ( (*pResult)( userData[ fetchRes[i] ] ) )
					return true;
			}
		}
		return false;
	}
	template<class TResultAnalyzer>
		void Collide( const CVec3 &start, const CVec3 &stop, TResultAnalyzer *pResult )
	{
		SBound b;
		CVec3 ptCollision;

		b.BoxInit( start, stop );
		b.ptHalfBox.x = fabs( b.ptHalfBox.x ) * 1.01f;
		b.ptHalfBox.y = fabs( b.ptHalfBox.y ) * 1.01f;
		b.ptHalfBox.z = fabs( b.ptHalfBox.z ) * 1.01f;
		b.s.fRadius *= 1.01f;

		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();

		for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SBSPEntity &e = entities[ fetchRes[i] ];
			SHMatrix &pos = e.pos;
			CVec3 realStart, realStop;
			pos.RotateHVector( &realStart, start );
			pos.RotateHVector( &realStop, stop );
			SPlane plane;
			if ( e.pTree->CollideCheckZeroRad( realStart, realStop, &ptCollision, &plane ) )
			{
				float fDist = fabs( ptCollision - realStart );
				if ( (*pResult)( fDist, ptCollision, plane, userData[ fetchRes[i] ] ) )
					break;
			};
		}
	}
	void Collide( const SSphere &sphere, const CVec3 &vel )
	{
		Collide( sphere, vel, &skipAnalyzer );
	}
	template<class TResultAnalyzer>
		bool DoesIntersect( const CVec3 &ptCenter, float fRadius, TResultAnalyzer *pResult )
	{
		/*if ( fRadius == F_SELECTED_SPHERE_RADIUS ) 
		{
			if ( colliderHash.find( ptCenter ) != colliderHash.end() )
				++nRepeatedCalls;
			else
				colliderHash[ ptCenter ] = true;
			++nTotalCalls;
		}*/
		CVec3 ptCollision;
		SBound b;
		b.SphereInit( ptCenter, fRadius );
		volume.Fetch( b );
		const vector<int> &fetchRes = volume.GetFetchBuffer();
		for ( int i = 0; i < volume.GetFetchedNum(); ++i )
		{
			SBSPEntity &e = entities[ fetchRes[i] ];
			SHMatrix &pos = e.pos;
			float fR = fRadius * e.fRMult;
			CVec3 realCenter;
			pos.RotateHVector( &realCenter, ptCenter );
			if ( e.pTree->DoesIntersect( realCenter, fR ) )
			{
				if ( (*pResult)( userData[ fetchRes[i] ] ) )
					return true;
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
