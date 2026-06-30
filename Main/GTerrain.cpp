#include "StdAfx.h"
#include "GScene.h"
#include "GSceneUtils.h"
#include "GMaterial.h"
#include "TerrainInfo.h"
#include "GTerrain.h"
#include "GTerrainTexture.h"
#include "Transform.h"
#include "GView.h"
#include "Grid.h"
#include "GMatShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\Misc\StrProc.h"
#include "GGrass.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLODCalcer::Recalc() 
{
	float fDist = fabs2( ptCenter - pCamera->GetValue() ); 
	value = fDist > sqr(4 * 8 * FP_GRID_STEP) ? 1 : 0; // 3 * 8 * GRID_STEP в квадрате
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcDer( const CVec3 &ptSrc, const CVec3 &ptNormal, CVec3 *ptRes )
{
	CVec3 vRes(ptSrc);
	vRes -= ptNormal * (ptNormal * vRes);
	Normalize( &vRes );
	*ptRes = vRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static SVertex* SetupMapVertex( SVertex *pvVertex, const CVec3 &vPoint, const CTRect<int> &nrRegion, const STerrainInfo *ptiInfo )
{
	pvVertex->pos.x = vPoint.x * FP_GRID_STEP;
	pvVertex->pos.y = vPoint.y * FP_GRID_STEP;
	pvVertex->pos.z = vPoint.z * FP_TERRAIN_H_SCALE;

	int nRX = vPoint.x;
	int nRY = vPoint.y;
	float fZ = ptiInfo->heightMap[nRY][nRX] * FP_TERRAIN_H_SCALE;
	float fXM = fZ, fXP = fZ, fYM = fZ, fYP = fZ;
	if ( nRX > 0 )
		fXM = ptiInfo->heightMap[nRY][nRX-1] * FP_TERRAIN_H_SCALE;
	if ( nRY > 0 )
		fYM = ptiInfo->heightMap[nRY-1][nRX] * FP_TERRAIN_H_SCALE;
	if ( nRX < ptiInfo->nWidth )
		fXP = ptiInfo->heightMap[nRY][nRX+1] * FP_TERRAIN_H_SCALE;
	if ( nRY < ptiInfo->nHeight )
		fYP = ptiInfo->heightMap[nRY+1][nRX] * FP_TERRAIN_H_SCALE;
	const float F_MUL = 1 / ( 2 * FP_GRID_STEP );
	CVec3 ptNormal( -( fXP - fXM ) * F_MUL, -( fYP - fYM ) * F_MUL, 1 );
	Normalize( &ptNormal );
	pvVertex->normal = ptNormal;

	pvVertex->normal = ptNormal;
	CalcDer( CVec3( 1, 0, 0 ), ptNormal, &pvVertex->texU );
	CalcDer( CVec3( 0, 1, 0 ), ptNormal, &pvVertex->texV );
	pvVertex->tex.u = (float)( vPoint.x - nrRegion.x1 ) / nrRegion.Width();
	pvVertex->tex.v = (float)( vPoint.y - nrRegion.y1 ) / nrRegion.Height();
	
	return pvVertex;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainModelPart::NeedUpdate()
{
	bool bI = pInfo.Refresh();
	bool bP = pPart.Refresh();
	return bI || bP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainModelPart::Recalc()
{
	pInfo.Refresh();
	pPart.Refresh();
	const STerrainInfo *ptiInfo = &pInfo->GetValue();
	const CTerrainPart *ptpPart = pPart->GetValue();

	pValue = new CObjectInfo;
	NGScene::CObjectInfo::SData objData;
	objData.verts.clear();
	objData.geometry.SetTriangles( ptpPart->faces );

	for ( vector<CVec3>::const_iterator iTemp = ptpPart->verts.begin(); iTemp != ptpPart->verts.end(); iTemp++ )
	{
		SVertex vVertex;
		objData.verts.push_back( *SetupMapVertex( &vVertex, *iTemp, nrRegion, ptiInfo ) );
	}
	pValue->Assign( objData );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainModelWallPart
////////////////////////////////////////////////////////////////////////////////////////////////////
static SVertex* SetupHoleVertex( SVertex *pvVertex, const CVec3 &vPos, const CVec2 &vTex )
{
	pvVertex->pos.x = vPos.x * FP_GRID_STEP;
	pvVertex->pos.y = vPos.y * FP_GRID_STEP;
	pvVertex->pos.z = vPos.z * FP_TERRAIN_H_SCALE;
	pvVertex->tex = vTex;
	pvVertex->texU = CVec3( 1, 0, 0 );
	pvVertex->texV = CVec3( 0, 1, 0 );
	return pvVertex;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainModelWallPart::Recalc()
{
	pPart.Refresh();
	const CTerrainPart *ptpPart = pPart->GetValue();
	
	pValue = new CObjectInfo;
	if ( ptpPart->faces.empty() )
		return;
	
	CObjectInfo::SData res;
	res.geometry.SetTriangles( ptpPart->faces );

	bool bUp = true;
	float fTexture = 0;
	CVec3 vLastPoint( ptpPart->verts.front() );
	for ( int nTemp = 0; nTemp < ptpPart->verts.size(); nTemp++ )
	{
		CVec2 vTexCoord;
		const CVec3 &vPoint = ptpPart->verts[nTemp];

		if ( bUp )
		{
			CVec2 vDelta( vPoint.x - vLastPoint.x, vPoint.y - vLastPoint.y );
			fTexture += fabs( vDelta ) / 8.0f;

			vTexCoord = CVec2( fTexture, 1 );
			vLastPoint = vPoint;
		}
		else
			vTexCoord = CVec2( fTexture, 0 );
		
		SVertex vVertex;
		res.verts.push_back( *SetupHoleVertex( &vVertex, vPoint, vTexCoord ) );

		bUp = !bUp;
	}

	const vector<CVec3> &verts = ptpPart->verts;
	for ( int nTemp = 0; nTemp < ptpPart->faces.size(); nTemp++ )
	{
		const STriangle &sTriangle = ptpPart->faces[nTemp];

		CVec3 vNormal = ( verts[sTriangle.i2] - verts[sTriangle.i1] ) ^ ( verts[sTriangle.i3] - verts[sTriangle.i2] );
		vNormal /= fabs( vNormal );
		res.verts[sTriangle.i1].normal = vNormal;
		res.verts[sTriangle.i2].normal = vNormal;
		res.verts[sTriangle.i3].normal = vNormal;
	}

	pValue->Assign( res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xF1871801, CLODCalcer );
REGISTER_SAVELOAD_CLASS( 0xF1871802, CTerrainModelPart );
REGISTER_SAVELOAD_CLASS( 0xF1871803, CTerrainModelWallPart );
