#ifndef __AIVOXELRENDER_H_
#define __AIVOXELRENDER_H_
//
#include "Transform.h"
#include "Render.h"
#include "..\Misc\2DArray.h"
//
namespace NDb
{
	class CRPGArmor;
}
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_VOXEL_TERRAIN = 1;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SConvexHull;
enum EAxis
{
	AXIS_X,
	AXIS_Y,
	AXIS_Z
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTVoxelRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TFinal, class TRes>
class CTVoxelRenderer : public CRasterizer<TFinal>
{
private:
	CVec3 vCenter;
	float fCubeSize;
	int nResolution;
	SHMatrix transform; // world space ==> camera space
	CTransformStack ts;
	EAxis axis;

protected:
	TRes *pBaseVoxel;
	int nDeltaZ, nDeltaY, nDeltaX;
private:
	//
	bool DoRenderBackface() const { return true; }
	void ClipVertical( int *pnSY, int *pnFY2, int *pnFY )
	{
		(*pnSY) = Max( *pnSY, 0 );
		(*pnFY2) = Min( *pnFY2, nResolution );
		(*pnFY) = Min( *pnFY, nResolution );
	}
	//
	void ClipHorizontal( int *pnSX, int *pnFX )
	{
		(*pnSX) = Max( *pnSX, 0 );
		(*pnFX) = Min( *pnFX, nResolution );
	}
	//
	void RasterSpan( int nY, int nLeft, int nRight, float fZ, float fDZ, int nBackface )
	{
		TRes *pVoxel = pBaseVoxel + nY * nDeltaY + nLeft * nDeltaX;
		float fWZ = fZ;
		float fShift = -0.5f;// + nBackface - 0.5f;

		for ( int nX = nLeft; nX < nRight; ++nX, fWZ += fDZ, pVoxel += nDeltaX )
		{
			int nZ = Float2Int( fShift + fWZ * nResolution );
#ifdef _DEBUG
			if ( nZ >= 0 && nZ < nResolution )
				ASSERT( pVoxel + nZ * nDeltaZ >= &voxels[0][0][0] && pVoxel + nZ * nDeltaZ < &voxels[0][0][0] + nResolution * nResolution * nResolution );
#endif
			((TFinal*)this)->OutputVoxel( pVoxel, nZ, nDeltaZ, nBackface );
				//switch ( axis )
				//{
				//case AXIS_X: pVoxel[ nZ * nDeltaZ ].nObject |= 1; break;
				//case AXIS_Y: pVoxel[ nZ * nDeltaZ ].nObject |= 2; break;
				//case AXIS_Z: pVoxel[ nZ * nDeltaZ ].nObject |= 4; break;
				//}
				//ASSERT( &voxels[nWX][nWY][nWZ] == pVoxel + nX * nDeltaX + nY * nDeltaY + nZ * nDeltaZ );
		}
	}
	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }
protected:
	void RealTraceEntity( const SConvexHull &e );
	EAxis GetAxis() const { return axis; }
	int GetResolution() const { return nResolution; }
public:
	CArray3D<TRes> voxels;
	//
	bool TestSphere( const CVec3 &ptCenter, float fR ) { return ts.IsIn( SSphere( ptCenter, fR ) ); }
	void GetPoints( vector<CVec3> *pEnters ) const {}
	void Init( const CVec3 &_vCenter, float _fCubeSize, int _nResolution );
	void InitParallel( EAxis _axis )
	{
		axis = _axis;
		pBaseVoxel = &voxels[0][0][0];
		ts.MakeParallel( fCubeSize, fCubeSize, -fCubeSize * 0.5f, fCubeSize * 0.5f );
		SHMatrix cam;
		switch ( axis )
		{
		case AXIS_X:
			MakeMatrix( &cam, vCenter, CVec3(1,0,0) );
			nDeltaX = -voxels.GetXSize();
			nDeltaY = 1;
			nDeltaZ = voxels.GetXSize() * voxels.GetYSize();
			pBaseVoxel = pBaseVoxel + (nResolution - 1) * voxels.GetXSize();
			//nWX = Float2Int( fShift + fWZ * ( nResolution - 1 ) );
			//nWZ = nY - region.y1;
			//nWY = nResolution - nX + region.x1;
			break;
		case AXIS_Y:
			MakeMatrix( &cam, vCenter, CVec3(0,1,0) );
			nDeltaX = voxels.GetXSize() * voxels.GetYSize();
			nDeltaY = 1;
			nDeltaZ = voxels.GetXSize();
			pBaseVoxel = pBaseVoxel;
			//nWY = Float2Int( fShift + fWZ * ( nResolution - 1 ) );
			//nWZ = nY - region.y1;
			//nWX = nX - region.x1;
			break;
		case AXIS_Z:
			MakeMatrix( &cam, vCenter, CVec3(0,0,1) );
			nDeltaX = -voxels.GetXSize();
			nDeltaY = -voxels.GetXSize() * voxels.GetYSize();
			nDeltaZ = 1;
			pBaseVoxel = pBaseVoxel 
				+ ( nResolution - 1 ) * voxels.GetXSize() * voxels.GetYSize() 
				+ ( nResolution - 1 ) * voxels.GetXSize();
			//nWZ = Float2Int( fShift + fWZ * ( nResolution - 1 ) );
			//nWX = nResolution - nY + region.y1;
			//nWY = nResolution - nX + region.x1;
			break;
		}
		//voxels[nWX][nWY][nWZ] (it was wrong order, I know) is replaced with
		//pBaseVoxel[ nDeltaX * nX + nDeltaY * nY + nDeltaZ * Float2Int( fShift + fWZ * ( nResolution - 1 ) ) ];
		ts.SetCamera( cam );
		transform = ts.Get().forward;
		transform.x = transform.x * 0.5f + transform.w * 0.5f; // [0,1] range instead of [-1,1]
		transform.y = transform.y * 0.5f + transform.w * 0.5f; // [0,1] range
		transform.x *= nResolution;
		transform.y *= nResolution;
	}
	friend class CRasterizer<TFinal>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVoxelObjectKey
{
	CObjectBase *pUser;
	int nUserID;
	SVoxelObjectKey() : pUser(0), nUserID(0) {}
	SVoxelObjectKey( CObjectBase *_pUser, int _nUserID ) : pUser(_pUser), nUserID(_nUserID) {}
	bool operator==( const SVoxelObjectKey &a ) const { return pUser == a.pUser && nUserID == a.nUserID; }
};
struct SVoxelObjectHash
{
	int operator()( const SVoxelObjectKey &k ) const { return ((int)k.pUser) ^ k.nUserID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unsigned short ushort;
struct SExplVoxel
{
	ZDATA
	ushort nObject;
	ushort nIndex;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nObject); f.Add(3,&nIndex); return 0; }
	//
	SExplVoxel() {}
	SExplVoxel( unsigned short _nObject, unsigned short _nIndex ):
	nObject( _nObject ), nIndex( _nIndex ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplVoxelRenderer : public CTVoxelRenderer<CExplVoxelRenderer, SExplVoxel>
{
	typedef CTVoxelRenderer<CExplVoxelRenderer, SExplVoxel> CTParent;
public:
	//
	struct SExplObject
	{
		ZDATA
		bool bTerrain;
		int nUserID;
		CPtr<CObjectBase> pUserData;
		CDBPtr<NDb::CRPGArmor> pArmor;
		int nObjectID;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bTerrain); f.Add(3,&nUserID); f.Add(4,&pUserData); f.Add(5,&pArmor); f.Add(6,&nObjectID); return 0; }
		//
		SExplObject() {}
		SExplObject( CObjectBase *_pUserData, int _nUserID, NDb::CRPGArmor *_pArmor, bool _bTerrain ):
			nUserID( _nUserID ), pUserData( _pUserData ), pArmor( _pArmor ), 
			bTerrain( _bTerrain ) {}
	};
	typedef unordered_map<SVoxelObjectKey, SExplObject, SVoxelObjectHash> CObjectsHash;

private:
	int nCurrentObjectID;
	CObjectsHash *pObjects; // объект с индексом 0 - зарезервирован, 1 - terrain
	int *pnObjectsEnd;
	//
	void AddObject( const SExplObject &object );
	__forceinline void OutputVoxel( SExplVoxel *pRes, int nZ, int nDeltaZ, int nBackface ) 
	{ 
		if ( nZ >= 0 && nZ < GetResolution() )
			pRes[ nZ * nDeltaZ ].nObject = nCurrentObjectID; 
	}

	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }
public:
	void Init( const CVec3 &_vCenter, float _fCubeSize, int _nResolution, CObjectsHash *_objects, int *_nObjectsEnd );
	//
	void TraceEntity( const vector<SConvexHull> &e, bool bTerrain )	{	ASSERT(0); }
	void TraceEntity( const SConvexHull &e, bool bTerrain );
	//
	friend class CTParent;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVisionVoxelRenderer : public CTVoxelRenderer<CVisionVoxelRenderer, char>
{
	typedef CTVoxelRenderer<CVisionVoxelRenderer, char> CTParent;
public:
	//

private:
	int nSolidFillFlag;
	char cAdd[3];
	__forceinline void OutputVoxel( char *pRes, int nZ, int nDeltaZ, int nBackface ) 
	{
		if ( nZ < 0 )
			nZ = 0;
		int nMax = GetResolution();
		if ( nZ >= nMax )
			nZ = nMax - 1;
		char &nRes = pRes[ nZ * nDeltaZ ];
		nRes |= cAdd[2];
		nRes += cAdd[ nBackface ];
	}

	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }
public:
	void Init( const CVec3 &_vCenter, float _fCubeSize, int _nResolution, int _nSolidFillFlag );
	//
	void TraceEntity( const vector<SConvexHull> &e, bool bTerrain )	{	ASSERT(0); }
	void TraceEntity( const SConvexHull &e, bool bTerrain );
	void FillSolid();
	//
	friend class CTParent;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __AIVOXELRENDER_H_