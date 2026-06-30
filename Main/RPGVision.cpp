#include "StdAfx.h"
#include "RPGVision.h"
#include "aiRender.h"
#include "DG.h"
#include "wTSFlags.h"
#include "aiVoxelRender.h"
#include "..\DBFormat\DataRPG.h"
#include "aiMap.h"
#include "grid.h"
#include "..\Misc\2darray.h"

const int N_HALFSIZE = 64;

namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_VISION_CUBE_RES_POW2 = 5;
const int N_VISION_CUBE_RESOLUTION = 1 << N_VISION_CUBE_RES_POW2;
const float F_STEP = FP_GRID_STEP / 4;
const float F_VISION_CUBE_SIZE = F_STEP * N_VISION_CUBE_RESOLUTION;
// ���������� ������ ������� ������������� �� �������������
const int N_TILES_HALF_TRANSP = 1;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVisionTracker;
class CVisionCube : public NAI::IAIMapTracker
{
	OBJECT_BASIC_METHODS(CVisionCube);
public:
	ZDATA
	CVec3 vCenter;
	CPtr<NAI::IAIMap> pAIMap;
	CArray2D<DWORD> solid, transp;
	CPtr<CVisionTracker> pParent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vCenter); f.Add(3,&pAIMap); f.Add(4,&solid); f.Add(5,&transp); f.Add(6,&pParent); return 0; }
	bool bCalced;

	CVisionCube() { Zero( bCalced ); }
	CVisionCube( NAI::IAIMap *_pMap, CVisionTracker *_pTracker, const CVec3 &_vCenter ): pAIMap(_pMap), pParent(_pTracker), vCenter(_vCenter)
	{ 
		SBound b;
		float f = F_VISION_CUBE_SIZE;
		b.BoxExInit( vCenter, CVec3( f, f, f ) );
		pAIMap->AddTracker( this, b, NWorld::TS_VISION|NWorld::TS_VISION_SOLID, true );
		Zero( bCalced );
	}
	virtual void OnChange();
	void Recalc();
	bool IsEmpty() const { return solid.GetXSize() < N_VISION_CUBE_RESOLUTION; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisionTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVisionQuery
{
	CVec3 vFrom, vWhat;
	bool operator ==( const SVisionQuery &a ) const { return vFrom == a.vFrom && vWhat == a.vWhat; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVisionQueryHash
{
	int operator()( const SVisionQuery &a ) const { SVec3Hash h; return h( a.vFrom ) + h( a.vWhat ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVisionTracker : public IVisionTracker
{
	OBJECT_NOCOPY_METHODS( CVisionTracker );
	typedef unordered_map<SVisionQuery, bool, SVisionQueryHash> CVisionHash;
	ZDATA
		CVisionHash visionCache;
	CArray3D<CObj<CVisionCube> > grid;
	CPtr<NAI::IAIMap> pAIMap;
	// release save-format tags 5/6/7: grass-height grid + per-point vision cache + vision koef
	// (dead-in-dev; behavior deferred). CVisionHash/CArray2D default-construct empty.
	CArray2D<char> grassHeight;
	CVisionHash pointVisionCache;
	float fVisionKoef = 1.0f;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&visionCache); f.Add(3,&grid); f.Add(4,&pAIMap); f.Add(5,&grassHeight); f.Add(6,&pointVisionCache); f.Add(7,&fVisionKoef); return 0; }
private:
	int nGCx, nGCy, nGCz;
	CPtr<CVisionCube> pCachedCube;

	bool TraverseLine( const CTPoint3<int> &_p1, const CTPoint3<int> &_p2, const CTPoint3<int> &_s, const CTPoint3<int> &_a,  
		int nXIdx, int nYIdx, int nZIdx, int nTranspLimit );
	bool IsVisible( const CTPoint3<int> p1, const CTPoint3<int> &p2, int nLimit );
	bool IsVisible( const CVec3 &vFrom, const CVec3 &vWhat, float fLimit );
	void FlushCubeCache() { nGCx = 0x7fffffff; nGCy = 0; nGCz = 0; pCachedCube = 0; }
	CVisionCube* FetchCube( int x, int y, int z );
	EVoxelVisionState GetVisionCached( int x, int y, int z );
public:
	CVisionTracker() { FlushCubeCache(); }
	CVisionTracker( NAI::IAIMap *_pAIMap );
	virtual bool IsCubeVisible( const CVec3 &ptFrom, const CVec3 &ptTarget, const CVec3 &ptForward );
	virtual EVoxelVisionState GetVision( int x, int y, int z )
	{
		FlushCubeCache();
		return GetVision( x, y, z );
	}
	virtual void GetCoord( const CVec3 &vPoint, CTPoint3<int> *pRes );
	virtual void GetCenter( const CTPoint3<int> &p, CVec3 *pRes );
	void FlushVisionCache() { visionCache.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisionCube
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionCube::Recalc()
{
	if ( bCalced )
		return;
//	int nResolution = 10;
	int nResolution = N_VISION_CUBE_RESOLUTION + 2;
	float fWidth = F_STEP * nResolution;//0.5f;
	NAI::CVisionVoxelRenderer renderer;
	renderer.Init( vCenter, fWidth, nResolution, NWorld::TS_VISION_SOLID );
	pAIMap->TraceVisionGrid( &renderer, NWorld::TS_VISION );
	//
	solid.SetSizes( N_VISION_CUBE_RESOLUTION, N_VISION_CUBE_RESOLUTION );
	transp.SetSizes( N_VISION_CUBE_RESOLUTION, N_VISION_CUBE_RESOLUTION );
	solid.FillEvery( 0 );
	transp.FillEvery( 0 );
	DWORD dwTotal = 0;
	for ( int x = 1; x < renderer.voxels.GetXSize() - 1; ++x )
	{
		for ( int y = 1; y < renderer.voxels.GetYSize() - 1; ++y )
		{
			DWORD &dwSolid = solid[y-1][x-1];
			DWORD &dwTransp = transp[y-1][x-1];
			for ( int z = 1; z < renderer.voxels.GetZSize() - 1; ++z )
			{
				DWORD dwBit = 1 << ( z - 1 );
				char cTest = renderer.voxels[x][y][z];
				if ( cTest & 0x80 )
				{
					dwSolid |= dwBit;
					dwTotal |= dwBit;
				}
				if ( cTest & 0x40 )
				{
					dwTransp |= dwBit;
					dwTotal |= dwBit;
				}
			}
		}
	}
	if ( dwTotal == 0 )
	{
		// save memory on empty cube
		solid.Clear();
		transp.Clear();
	}
	bCalced = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionCube::OnChange() 
{ 
	Zero( bCalced );
	if ( IsValid( pParent ) )
		pParent->FlushVisionCache();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisionTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CVisionTracker::CVisionTracker( NAI::IAIMap *_pAIMap ): pAIMap(_pAIMap)
{
	FlushCubeCache();
	// here comes hotstepper
	grid.SetSizes( 32, 32, 4 );
	for ( int z = 0; z < grid.GetZSize(); ++z )
	{
		for ( int y = 0; y < grid.GetYSize(); ++y )
		{
			for ( int x = 0; x < grid.GetXSize(); ++x )
			{
				float f = F_VISION_CUBE_SIZE;
				float fShift = F_VISION_CUBE_SIZE * 0.5f;
				CVec3 vCenter( x * f + fShift, y * f + fShift, z * f + fShift );
				grid[z][y][x] = new CVisionCube( pAIMap, this, vCenter );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVisionCube* CVisionTracker::FetchCube( int gx, int gy, int gz )
{
	CVisionCube *pRes;
	nGCx = gx; nGCy = gy; nGCz = gz; pCachedCube = 0;

	if ( gx < 0 || gx >= grid.GetXSize() )
		return 0;
	if ( gy < 0 || gy >= grid.GetYSize() )
		return 0;
	if ( gz < 0 || gz >= grid.GetZSize() )
		return 0;
	pRes = grid[gz][gy][gx];
	pRes->Recalc();
	if ( pRes->IsEmpty() )
		pCachedCube = 0;
	else
		pCachedCube = pRes;
	return pCachedCube;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
__forceinline EVoxelVisionState CVisionTracker::GetVisionCached( int x, int y, int z )
{
	int gx = x >> N_VISION_CUBE_RES_POW2;
	int gy = y >> N_VISION_CUBE_RES_POW2;
	int gz = z >> N_VISION_CUBE_RES_POW2;
	CVisionCube *pRes;
	if ( nGCx != gx || nGCy != gy || nGCz != gz )
		pRes = FetchCube( gx, gy, gz );
	else
		pRes = pCachedCube;
	if ( !pRes )
		return VVS_NONE;
	int bx = x & ( N_VISION_CUBE_RESOLUTION - 1 );
	int by = y & ( N_VISION_CUBE_RESOLUTION - 1 );
	DWORD dwTest = 1 << ( z & ( N_VISION_CUBE_RESOLUTION - 1 ) );
	if ( pRes->solid[by][bx] & dwTest )
		return VVS_SOLID;
	if ( pRes->transp[by][bx] & dwTest )
		return VVS_TRANSPARENT;
	return VVS_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionTracker::GetCoord( const CVec3 &vPoint, CTPoint3<int> *pRes )
{
	pRes->x = Float2Int( vPoint.x / F_STEP - 0.5f );
	pRes->y = Float2Int( vPoint.y / F_STEP - 0.5f );
	pRes->z = Float2Int( vPoint.z / F_STEP - 0.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionTracker::GetCenter( const CTPoint3<int> &p, CVec3 *pRes )
{
	pRes->x = ( p.x + 0.5f ) * F_STEP;
	pRes->y = ( p.y + 0.5f ) * F_STEP;
	pRes->z = ( p.z + 0.5f ) * F_STEP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVisionTracker::TraverseLine( const CTPoint3<int> &_p1, const CTPoint3<int> &_p2, const CTPoint3<int> &_s, const CTPoint3<int> &_a,  
	int nXIdx, int nYIdx, int nZIdx, int nTranspLimit )
{
	FlushCubeCache();
	CTPoint3<int> cur( _p1 );
	int x2 =_p2.m[nXIdx];
	int sx = _s.m[nXIdx], sy = _s.m[nYIdx], sz = _s.m[nZIdx];
	int ax = _a.m[nXIdx], ay = _a.m[nYIdx], az = _a.m[nZIdx];

	int yd = ay - (ax >> 1);
	int zd = az - (ax >> 1);
	int n = 0;
	for (;;)
	{
		EVoxelVisionState vs = GetVisionCached( cur.x, cur.y, cur.z );
		if ( vs == VVS_SOLID )
			return false;
		if ( vs == VVS_TRANSPARENT )
		{
			nTranspLimit -= ( (n * 128) / ( n + 16 ) );
			if ( nTranspLimit < 0 )
				return false;
		}

		if ( cur.m[nXIdx] == x2 )
			return true;

		if (yd >= 0 )
		{
			cur.m[nYIdx] += sy;
			yd -= ax;
			if ( GetVisionCached( cur.x, cur.y, cur.z ) == VVS_SOLID )
				return false;
		}

		if (zd >= 0)
		{
			cur.m[nZIdx] += sz;
			zd -= ax;
			if ( GetVisionCached( cur.x, cur.y, cur.z ) == VVS_SOLID )
				return false;
		}

		cur.m[nXIdx] += sx;
		yd += ay;
		zd += az;
		++n;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVisionTracker::IsVisible( const CTPoint3<int> p1, const CTPoint3<int> &p2, int nDistance )
{
	if ( nDistance == 0 )
		return true;
	int dx, dy, dz;
	dx = p2.x - p1.x;
	dy = p2.y - p1.y;
	dz = p2.z - p1.z;

	int nTranspLimit = -Float2Int( (128 * N_TILES_HALF_TRANSP * 1.45f) * log( nDistance  / ( N_SIGHTDISTANCE / F_STEP ) ) );
	CTPoint3<int> a( abs(dx) << 1, abs(dy) << 1, abs(dz) << 1 );
	CTPoint3<int> s( Sign(dx), Sign(dy), Sign(dz) );

	if (a.x >= Max(a.y, a.z) )            // x dominant
	{
		if ( a.x == 0 )
			return true;
		nTranspLimit = (nTranspLimit * a.x) / nDistance;
		return TraverseLine( p1, p2, s, a, 0, 1, 2, nTranspLimit );
	}
	else if (a.y >= a.z )            // y dominant 
	{
		nTranspLimit = (nTranspLimit * a.y) / nDistance;
		return TraverseLine( p1, p2, s, a, 1, 2, 0, nTranspLimit );
	}
	else
	{
		nTranspLimit = (nTranspLimit * a.z) / nDistance;
		return TraverseLine( p1, p2, s, a, 2, 0, 1, nTranspLimit );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVisionTracker::IsVisible( const CVec3 &_vFrom, const CVec3 &_vWhat, float fDistance )
{
	CTPoint3<int> vFrom, vWhat;
	GetCoord( _vFrom, &vFrom );
	GetCoord( _vWhat, &vWhat );
	return IsVisible( vFrom, vWhat, Float2Int( fDistance * ( 1 / F_STEP ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVisionTracker::IsCubeVisible( const CVec3 &vFrom, const CVec3 &ptTarget, const CVec3 &ptForward )
{
	SVisionQuery q;
	q.vFrom = vFrom;
	q.vWhat = ptTarget;
	CVec3 ptDif = ptTarget - vFrom;
	if ( ptDif * ptForward < -1e-5f )
		return false;
	float fDistance = fabs( ptDif );
	if ( fDistance >= N_SIGHTDISTANCE )
		return false;
	CVisionHash::iterator i = visionCache.find( q );
	if ( i != visionCache.end() )
		return i->second;
	//CObj<CVisionCube> pCube = GetCube( ptFrom );
	int nCount = 0;
	nCount += IsVisible( vFrom, ptTarget, fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(-0.2f, -0.2f, -0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(0.2f, -0.2f, -0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(-0.2f, 0.2f, -0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(0.2f, 0.2f, -0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(-0.2f, -0.2f, 0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(0.2f, -0.2f, 0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(-0.2f, 0.2f, 0.2f), fDistance );
	nCount += IsVisible( vFrom, ptTarget + CVec3(0.2f, 0.2f, 0.2f), fDistance );
	bool bRes = nCount >= 4;
	// if cache has grown too large truncate it
	if ( visionCache.size() > 30000 )
		visionCache.clear();
	visionCache[q] = bRes;
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IVisionTracker* CreateVisionTracker( NAI::IAIMap *pAIMap )
{
	return new CVisionTracker( pAIMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x026b1180, CVisionCube )
REGISTER_SAVELOAD_CLASS( 0x00413190, CVisionTracker )