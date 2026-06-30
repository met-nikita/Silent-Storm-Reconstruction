#include "StdAfx.h"
#include "aiTerrain.h"
#include "Grid.h"
#include "aiObject.h"
#include "TerrainInfo.h"
#include "BSPTree.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainGeometry
void CTerrainGeometry::Recalc()
{
	pPart.Refresh();
	CTerrainPart *ptpPart = pPart->GetValue();

	vector<CVec3> verts( ptpPart->verts.size() );
	for ( int i = 0; i < ptpPart->verts.size(); i++ )
	{
		verts[i].x = ptpPart->verts[i].x * FP_GRID_STEP;
		verts[i].y = ptpPart->verts[i].y * FP_GRID_STEP;
		verts[i].z = ptpPart->verts[i].z * FP_TERRAIN_H_SCALE;
	}

	pValue = new CGeometryInfo;
	pValue->AddPiece( -1, verts, ptpPart->faces, 0, vector<NAI::SJunction>(), false );
	pValue->CalcBound();
	// terrain gets no baked precalc grid (release PrecalcCollideInfo skips terrain); collision is
	// generated on the fly when a terrain hull is added to a collider.
	pValue->PrecalcCollideInfo( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x014c1112, CTerrainGeometry )
