#include "StdAfx.h"
#include "BSchemaViewer.h"
#include "MakeBuilding.h"
#include "BuildingGrid.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "BuildingSchema.h"
#include "RodJunction.h"
#include "rod.h"
#include "MemObject.h"
#include "Transform.h"
#include "Grid.h"
#include "..\Misc\BasicShare.h"
#include "BuildingInfo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	externA5 CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSchemaObject: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSchemaObject);
public:
	vector<CObj<CObjectBase> > pNodes;
	vector<CObj<NGScene::CPolyline> > pLines;
	vector<CObj<NGScene::CText> > pStrings;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* ViewBuildingSchema( NGScene::IGameView *pScene, NBuilding::CSolidAndWallMap *pSWMap, int nBuildingID, NBuilding::CBuildingGrid *pBGrid, const SFBTransform &pos )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pBInfo = pLoader->GetValue();
	if ( !pBInfo )
		return 0;
	return ViewBuildingSchema( pScene, pSWMap, pBInfo, pBGrid, pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* ViewBuildingSchema( NGScene::IGameView *pScene, NBuilding::CSolidAndWallMap *pSWMap, NBuilding::CBuildInfo *pBInfo, NBuilding::CBuildingGrid *pBGrid, const SFBTransform &pos )
{
	/*
	for ( int i = 0; i < 30; ++i )
	{
		NBuilding::CBuildingSchema schema;
		NBuilding::MakeBuildingSchema( &schema, pBGrid, pBInfo );
	}
	return 0;
*/

	CPtr<NBuilding::CBuildingSchema> pSchema = new NBuilding::CBuildingSchema;
	NBuilding::MakeBuildingSchema( pSchema, pBGrid, pBInfo, pSWMap );
	pSchema->Start();
	int nIterations = 0;
	if ( nIterations )
	{
		for ( int i = 0; i < nIterations; ++i )
		{
			if ( i > 0 )
				pSchema->Reset();
			pSchema->Recalc( pBGrid );
		}
	}
	else
	{
		pSchema->Recalc( 0 );
	}


	const vector<NBuilding::CJunction> &juncs = pSchema->GetJuncs();
	const vector<NBuilding::CRod> &rods = pSchema->GetRods();

	CPtr<CMemObject> pSphere = new CMemObject;
	pSphere->CreateSphere( VNULL3, 0.15f, 1 );

	CSchemaObject *pObj = new CSchemaObject;
	float fAlpha = 1;
	float fMaxPressure = 0;
	float fWeight = 0;
	for ( vector<NBuilding::CJunction>::const_iterator pJ = juncs.begin(); pJ != juncs.end(); ++pJ )
	{
		const CVec3 ptPos( pJ->ptJ.x * FP_GRID_STEP, pJ->ptJ.y * FP_GRID_STEP, pJ->ptJ.z * 0.625f );
		NBuilding::CJunction::EStability st = pJ->GetStability();
		fMaxPressure = Max( fMaxPressure, pJ->GetPressure() );
		fWeight += pJ->GetWeight();

		CVec4 color;
		int nHP = sqrt( pBGrid->GetHP( NBuilding::SPoint3( pJ->ptJ.x, pJ->ptJ.y, pJ->ptJ.z ) ) );
		if ( pJ->IsBroken( nHP ) && !pJ->IsGround() && !pJ->IsCellarWall() )
			color = CVec4( 0.9f, 0.9f, 0, fAlpha );
		else
			switch ( st )
			{
				case NBuilding::CJunction::FREE:
					color = CVec4( 1.0f, 1.0f, 1.0f, fAlpha );
					break;
				case NBuilding::CJunction::STABLE:
					color =  pJ->IsFilled() ? CVec4( 0.2f, 0.1f, 0.0f, fAlpha ) : CVec4( 0.3f, 0.2f, 0.1f, fAlpha );
					if ( pJ->IsGround() )
						color = CVec4( 0, 0, 0, fAlpha );
					if ( pJ->IsCellarWall() )
						color = CVec4( 0.30f, 0.35f, 0.30f, fAlpha );
					break;
				case NBuilding::CJunction::UNKNOWN:
					color = CVec4( 0.6f, 0.6f, 0.6f, fAlpha );
					break;
				case NBuilding::CJunction::UNSTABLE:
					color =  CVec4( 0.7f, 0.1f, 0.1f, fAlpha );
					break;
	//			case NBuilding::CJunction::UNSTABLE_NOTFREE:
	//				pSphere->color =  CVec3( 0.7f, 0.5f, 0.1f );
	//				break;
				case NBuilding::CJunction::UNKNOWN_MOMENT:
					color =  CVec4( 1.0f, 1.0f, 0.1f, fAlpha );
					break;
				default:
					color =  CVec4( 1.0f, 0.3f, 1.0f, fAlpha );
					break;
			}
		pObj->pNodes.push_back( pScene->CreateMesh( pSphere, color, pos * MakeTransform( ptPos )  ) );
		//pObj->pStrings.push_back( p2DScene->CreateText( new NGScene::CCWString( L"AbCd" ), new NGScene::CCTRect( CTRect<int>( 5, 5, 50, 50 ) ), new NGScene::CCInt( 0 ) ) );
	}
	//
	float fMax = -1;
	float fMin = 0;
	for ( vector<NBuilding::CRod>::const_iterator pR = rods.begin(); pR != rods.end(); ++pR )
	{
		const NBuilding::CRod &rod = *pR;
		float ml = pSchema->GetJunction( pR->GetJunction( NBuilding::RS_LEFT ) )->GetMoment( &rod );
		float mr = pSchema->GetJunction( pR->GetJunction( NBuilding::RS_RIGHT ) )->GetMoment( &rod );
		fMax = Max( fMax, Max( fabs(ml), fabs(mr) ) );
		//fMin = Max( fMin, Max( ml, mr ) );
	}
	//
	char buf[512];
	sprintf( buf, "Min moment = %f \tMax Moment = %f\tMax pressure=%f  num juncs=%d, num rods=%d, weight=%f\n", 
		fMin, fMax, fMaxPressure, juncs.size(), rods.size(), fWeight );
	OutputDebugString( buf );
	//
	fMax = sqrt( fMax );
	fMin = sqrt( fMin );
	float fDenom = fMax - fMin;
	float fScale = fDenom > FP_EPSILON ? 5.0f / (fMax - fMin) : 1;
	//
	CVec4 lcr( 0.05f, 0.25f, 0.3f, fAlpha );
	for ( vector<NBuilding::CRod>::const_iterator pR = rods.begin(); pR != rods.end(); ++pR )
	{
		const NBuilding::CRod &rod = *pR;
		const NBuilding::CIVec3 ptL = pR->GetLeftPt();
		const NBuilding::CIVec3 ptR = pR->GetRightPt();
		float ml = pSchema->GetJunction( pR->GetJunction( NBuilding::RS_LEFT ) )->GetMoment( &rod );
		float mr = pSchema->GetJunction( pR->GetJunction( NBuilding::RS_RIGHT ) )->GetMoment( &rod );

		vector<CVec3> points;
		points.push_back( CVec3( ptL.x * FP_GRID_STEP, ptL.y * FP_GRID_STEP, ptL.z * 0.625f ) );
		points.push_back( CVec3( ptR.x * FP_GRID_STEP, ptR.y * FP_GRID_STEP, ptR.z * 0.625f ) );
		CPtr<CMemObject> pCylinder = new CMemObject;
		const float fM = Max( fabs(ml), fabs(mr) );
		float dr = 0.004f * sqrt( fM ) * fScale;
		pCylinder->CreateCylinder( points[0], points[1], 0.03f + dr, 1 );
		bool bBroken = pSchema->GetJunction( pR->GetJunction( NBuilding::RS_LEFT ) )->IsLinkBroken( &rod );
		CVec4 color = bBroken ? CVec4( 1, 1, 1, fAlpha ) : lcr;
		color.w = fAlpha;
		pObj->pNodes.push_back( pScene->CreateMesh( pCylinder, color, pos ) );
		//pObj->pLines.push_back( pScene->CreatePolyline( points, fScale * Max( ml, mr ) * lcr ) );
	}
	return pObj;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
