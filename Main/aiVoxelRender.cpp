#include "StdAfx.h"
//
#include "aiObject.h"
#include "aiInterval.h"
//
#include "aiVoxelRender.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTVoxelRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TFinal, class TRes>
void CTVoxelRenderer<TFinal,TRes>::Init( const CVec3 &_vCenter, float _fCubeSize, int _nResolution )
{
	vCenter = _vCenter;
	fCubeSize = _fCubeSize;
	nResolution = _nResolution;
	voxels.SetSizes( nResolution, nResolution, nResolution );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TFinal, class TRes>
void CTVoxelRenderer<TFinal,TRes>::RealTraceEntity( const SConvexHull &e )
{
	// находим проекцию в CameraSpace
	static vector<CVec3> flatProjected;
	if ( e.points.size() > flatProjected.size() )
		flatProjected.resize( e.points.size() );
	//
	SHMatrix xform;
	Multiply( &xform, transform, e.trans.forward );
	for ( int i = 0; i < e.points.size(); ++i )
	{
		const CVec3 &src = e.points[i];
		CVec3 &dst = flatProjected[i];
		xform.RotateHVector( &dst, src );
	}
	// rasterize каждый треугольник ConvexHull-а
	const vector<SEdge> &edges = e.tris.edges;
	const vector<STriangle> &mesh = e.tris.mesh;
	for ( int i = 0; i < mesh.size(); ++i )
	{
		int i1, i2, i3;
		const STriangle &t = mesh[i];
		if ( t.i1 & 0x8000 )
		{
			i1 = edges[ t.i1 & 0x7fff ].wFinish; 
			i2 = edges[ t.i1 & 0x7fff ].wStart;
		}
		else
		{
			i1 = edges[ t.i1 & 0x7fff ].wStart; 
			i2 = edges[ t.i1 & 0x7fff ].wFinish;
		}
		if ( t.i2 & 0x8000 )
			i3 = edges[ t.i2 & 0x7fff ].wStart;
		else
			i3 = edges[ t.i2 & 0x7fff ].wFinish;
		//
		RasterNoClip( flatProjected[i1], flatProjected[i2], flatProjected[i3] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplVoxelRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplVoxelRenderer::AddObject( const SExplObject &object )
{
	SExplObject &dst = (*pObjects)[ SVoxelObjectKey( object.pUserData, object.nUserID ) ];
	dst = object;
	dst.nObjectID = (*pnObjectsEnd)++;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplVoxelRenderer::Init( const CVec3 &_vCenter, 
	float _fCubeSize, int _nResolution, CObjectsHash *_pObjects, int *_nObjectsEnd )
{
	CTParent::Init( _vCenter, _fCubeSize, _nResolution );
	pnObjectsEnd = _nObjectsEnd;
	nCurrentObjectID = 0;
	pObjects = _pObjects;
	voxels.FillEvery( SExplVoxel( 0, 0 ) );
	if ( pObjects->empty() )
	{
		ASSERT( *pnObjectsEnd == 0 );
		AddObject( SExplObject( 0, 0, 0, false ) );
		AddObject( SExplObject( 0, 0, 0, true ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplVoxelRenderer::TraceEntity( const SConvexHull &e, bool bTerrain )
{
	if ( !bTerrain )
	{
		CObjectsHash::iterator i = pObjects->find( SVoxelObjectKey( e.src.pUserData, e.nUserID ) );
		if ( i == pObjects->end() )
		{
			AddObject( SExplObject( e.src.pUserData, e.nUserID, e.src.pArmor, false ) );
			nCurrentObjectID = *pnObjectsEnd - 1;
		}
		else
			nCurrentObjectID = i->second.nObjectID;
	}
	else
		nCurrentObjectID = N_VOXEL_TERRAIN;
	//
	RealTraceEntity( e );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisionVoxelRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionVoxelRenderer::Init( const CVec3 &_vCenter, float _fCubeSize, int _nResolution, int _nSolidFillFlag )
{
	CTParent::Init( _vCenter, _fCubeSize, _nResolution );
	voxels.FillEvery( 0x20 );
	nSolidFillFlag = _nSolidFillFlag;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionVoxelRenderer::TraceEntity( const SConvexHull &e, bool bTerrain )
{
	// decide if fill is needed
	//e.src.pArmor
	if ( bTerrain || ( e.src.nTSFlags & nSolidFillFlag ) == 0 )
		cAdd[2] = (char)0x80;
	else
		cAdd[2] = (char)0x40;
	if ( GetAxis() == AXIS_Z && cAdd[2] == 0x40 )
	{
		cAdd[0] = -1;
		cAdd[1] = 1;
	}
	else
	{
		cAdd[0] = 0;
		cAdd[1] = 0;
	}
	//
	RealTraceEntity( e );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisionVoxelRenderer::FillSolid()
{
	for ( int z = 1; z < voxels.GetZSize() - 1; ++z )
	{
		for ( int y = 1; y < voxels.GetYSize() - 1; ++y )
		{
			int nInside = 0;
			for ( int x = 0; x < voxels.GetXSize(); ++x )
			{
				char nDelta = voxels[z][y][x] & 0x3f;
				nInside += nDelta - 0x20;
				voxels[z][y][x] &= 0x80 | 0x40;
				ASSERT( nInside >= 0 );
				if ( nInside > 0 )
					voxels[z][y][x] |= 0x40;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
