#include "StdAfx.h"
#include "GMemFormat.h"
#include "GfxBuffers.h"
#include "GGeometry.h"
#include "GfxRender.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
static void CalcDU( T *pRes, const CVec3 &vNormal )
{
	pRes->normal = vNormal;
	CVec3 vTexU = vNormal ^ CVec3(0.3f,0.3f,0.3f);
	if ( fabs2( vTexU ) < 0.01f )
		vTexU = vNormal ^ CVec3(0,1,0);
	Normalize( &vTexU );
	CVec3 vTexV = vTexU ^ vNormal;
	pRes->texU = vTexU;
	pRes->texV = vTexV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMemGeometry
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemGeometry::CMemGeometry( const vector<CVec3> &_points ) : points(_points)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemGeometry::Recalc()
{
	ASSERT( !IsValid( pValue ) );
	if ( NGfx::IsTnLDevice() )
	{
		NGfx::CBufferLock<NGfx::SGeomVecT1C1> geom( &pValue, points.size() );
		for ( int i = 0; i < points.size(); ++i )
		{
			NGfx::SGeomVecT1C1 v;
			v.pos = points[i];
			geom[i] = v;
		}
	}
	else
	{
		NGfx::CBufferLock<NGfx::SGeomVecFull> geom( &pValue, points.size() );
		for ( int i = 0; i < points.size(); ++i )
		{
			NGfx::SGeomVecFull v;
			v.pos = points[i];
			geom[i] = v;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMemObjectInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemObjectInfo::CMemObjectInfo( const vector<STriangle> &_tris, const vector<CVec3> &_points, 
	const vector<CVec3> &_normals ) : tris(_tris), points(_points), normals(_normals)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemObjectInfo::Recalc()
{
	pValue = new CObjectInfo;
	CObjectInfo::SData resData;
	resData.verts.resize( points.size() );
	for ( int i = 0; i < points.size(); ++i )
		resData.verts[i].pos = points[i];
	for ( int i = 0; i < normals.size(); ++i )
		CalcDU( &resData.verts[i], normals[i] );
	//
	resData.geometry.SetTriangles( tris );
	pValue->Assign( resData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02911170, CMemGeometry )
REGISTER_SAVELOAD_CLASS( 0x02911172, CMemObjectInfo )
