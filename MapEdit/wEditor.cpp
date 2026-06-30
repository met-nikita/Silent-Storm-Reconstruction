#include "StdAfx.h"
#include "wEditor.h"
#include "..\Main\aiMap.h"
#include "..\Main\BuildingGrid.h"
#include "..\Main\BuildingInfo.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\Transform.h"
#include "..\Main\MemObject.h"
#include "IWysiwyg.h"
#include "WysiwygSpotSel.h"
#include "WysiwygSubTemplateSel.h"
#include "..\Main\MakeBuilding.h"
#include "..\Main\Grid.h"
#include "..\Main\wBuilding.h"
#include "..\Main\wTerrain.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\Main\MELayers.h"
#include "MEParams.h"
#include "MEUserSettings.h"
#include "..\Main\aiWaypoint.h"
#include "..\Main\MapBuild.h"
#include "..\Main\MapBuildTerrain.h"
#include "..\Main\aiGrid.h"

const float WALL_HEIGHT = 2.5f;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
extern CBasicShare<int, NAI::CWaypointLoader> shareWaypoints;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsLayerVisible( int nLayerID )
{
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	return find( layers.begin(), layers.end(), nLayerID ) != layers.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool operator==( const SMapHole &a, const SMapHole &b )
{
	return a.nFloor == b.nFloor && a.nHeight == b.nHeight && a.bVisible == b.bVisible && a.polygonsList == b.polygonsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool operator==( const SMapWall &a, const SMapWall &b )
{
	return a.nHeightMin == b.nHeightMin && a.nHeightMax == b.nHeightMax && a.vBeg == b.vBeg && a.vEnd == b.vEnd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEdtorMesh
////////////////////////////////////////////////////////////////////////////////////////////////////
CEdtorMesh::CEdtorMesh( CSyncSrc<IVisObj> *pShow, NDb::CModel *_pModel, const SFBTransform &_m, NDb::CFinalElement *p )
: pModel(_pModel), m(_m), pFin(p)
{
	bindGlobal.Link( pShow, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdtorMesh::Visit( IRenderVisitor *p )
{
	if ( !IsValid( pFin ) || IsValid( pFin->pVariant ) )
		p->AddMesh( pModel, m, 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdtorMesh::Visit( IAIVisitor *p )
{
	if ( !IsValid( pFin ) || IsValid( pFin->pVariant ) )
		p->AddHull( pModel->pGeometry->pAIGeometry, m, pModel->pRPGArmor, 0, TS_PICK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMemMesh
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemMesh::CMemMesh( CSyncSrc<IVisObj> *pShow, CMemObject *_pObject, const SFBTransform &_m, int _nUserID, const CVec4 &cr )
: pObject(_pObject), m(_m), nUserID(_nUserID), color( cr )
{
	bindGlobal.Link( pShow, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemMesh::Visit( IRenderVisitor *p )
{
	if ( pObject->IsPolyLine() )
		p->AddPolyline( pObject->GetPoints(), CVec3( color.x, color.y, color.z ) );
	else
		p->AddMesh( pObject, color, m );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemMesh::Visit( IAIVisitor *p )
{
	p->AddHull( pObject, m, 0, 0, TS_PICK, nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsTerrainVisible()
{
	return IsLayerVisible( NBuilding::MakeFragmentID( LID_HEIGHTS, 0 ) ) 
		|| IsLayerVisible( NBuilding::MakeFragmentID( LID_TILES, 0 ) ) 
		|| IsLayerVisible( NBuilding::MakeFragmentID( LID_TERRCOLOR, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEditorTerrain
////////////////////////////////////////////////////////////////////////////////////////////////////
CEditorTerrain::CEditorTerrain( CSyncSrc<IVisObj> *pShow, CTerrainInfoHolder *pTerrainInfo, 
	NAI::IAIMap *_pAIMap, CFuncBase<STime> *pTime, int nDefaultFloor, 
	const list<SMapHole> &holesList, const list<SMapWall> &wallsList, int _nCutFloor  )
	:CTerrain( pShow, pTerrainInfo, _pAIMap, pTime, nDefaultFloor, holesList, wallsList ), nCutFloor(_nCutFloor), pAIMap(_pAIMap)
{
	bVisible = true;//IsTerrainVisible() && nCutFloor >= 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorTerrain::Visit( IAIVisitor *pVisitor )
{
	if ( IsTerrainVisible() && nCutFloor >= 0 )
		CTerrain::Visit( pVisitor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorTerrain::Visit( IRenderVisitor *pVisitors )
{
	if ( IsTerrainVisible() && nCutFloor >= 0 )
		CTerrain::Visit( pVisitors );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorTerrain::Update( int _nCutFloor, bool bForceUpdate )
{
	nCutFloor = _nCutFloor;
	bool bVis = IsTerrainVisible() && nCutFloor >= 0;
	if ( bVis != bVisible || bForceUpdate)
	{
		bVisible = bVis;
		bindGlobal.Update();
		CTerrain::Update( bVisible );
		pAIMap->Sync();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEditorWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
CEditorWorld::CEditorWorld() 
{ 
	pTime = new CCTime(0); 
	bTerrainAlign = false;
	bUpdateTerrain = true;
	bShowInfo = true;
	bHolesUpdated = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SFBTransform CEditorWorld::GetTerrainTransform( float x, float y )
{
	if ( bTerrainAlign )
	{
//		pTerrainInfo.Refresh();
		CDGPtr<CFuncBase<STerrainInfo> > pInfo = pTerrainInfo;
		pInfo.Refresh();

		return MakeTransform( CVec3(0,0,GetMeterHeightCheck(pInfo->GetValue(), x, y ) ) );
	}
	else
		return posTerrain;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SFBTransform CEditorWorld::GetTerrainTransform( const SFBTransform &posObj )
{
	if ( bTerrainAlign )
	{
		CVec3 pt = VNULL3;
		posObj.forward.RotateHVector( &pt, pt );
//		pTerrainInfo.Refresh();
		CDGPtr<CFuncBase<STerrainInfo> > pInfo = pTerrainInfo;
		pInfo.Refresh();

		return MakeTransform( CVec3(0,0,GetMeterHeightCheck(pInfo->GetValue(), pt.x, pt.y ) ) );
	}
	else
		return posTerrain;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CEdtorMesh* CEditorWorld::AddFrozen( const SFBTransform &pos, NDb::CModel *pModel, NDb::CFinalElement *pFin )
{
	return *frozen.insert( frozen.end(), new CEdtorMesh( pShow, pModel, GetTerrainTransform(pos) * pos, pFin ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddUnits()
{
	units.clear();
	//
	for ( int i = 0; i < pVar->pUnits.size(); ++i )
	{
		NDb::CUnit *pUnit = pVar->pUnits[i];
		if ( !IsValid( pUnit ) || !IsValid( pUnit->pMonster ) || !IsValid( pUnit->pMonster->pModel ) )
			continue;
		CVec2 ptPos( pUnit->ptPos.x, pUnit->ptPos.y );
		ptPos *= FP_GRID_STEP;
		units.push_back( IEditorUnit::Create( this, pUnit, GetTerrainTransform(ptPos.x, ptPos.y) ) );
		nMaxFloor = Max( nMaxFloor, pUnit->nFloor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddBuilding( const SMapBuilding &info )
{
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pBInfo = pBuildInfo->GetValue();
	NBuilding::BuildingHP( pBInfo, info.pGrid, info.pSWMap );
	buildings.push_back( new CBuilding( pShow, info, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddMemObject( list<CObj<CMemMesh> > *pRes, ::CMemObject *pObject, const SFBTransform &pos, const CVec4 &color, int nUserID, bool bIgnoreTerr )
{
	pRes->push_back( new CMemMesh( pShow, pObject, bIgnoreTerr ? pos : pos * GetTerrainTransform(pos), nUserID, color ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CEditorWorld::CreateMemObject( ::CMemObject *pObject, const SFBTransform &pos, const CVec4 &color, int nUserID, bool bIgnoreTerr )
{
	return new CMemMesh( pShow, pObject, bIgnoreTerr ? pos : pos * GetTerrainTransform(pos), nUserID, color );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::CreateTerrain( bool bResetPins )
{
	ASSERT( IsValid( pVar ) );
	SMapInfo mapInfo;
	SRand rand;
	vector<int> flags;

	mapInfo.terrain.nWidth = pVar->pTemplate->nWidth;
	mapInfo.terrain.nHeight = pVar->pTemplate->nHeight;
	/*
	if ( LoadRootTerrain( pVar->GetRecordID(), &mapInfo.terrain, &rand, flags ) )
	{
		pTerrainInfo = new CTerrainInfoHolder( mapInfo.terrain );
		CDGPtr<CFuncBase<STerrainInfo> > pInfo = pTerrainInfo;
		pInfo.Refresh();
//		pTerrainInfo.Refresh();
		float fDZ = GetMeterHeightCheck(pInfo->GetValue(), pVar->pTemplate->nWidth * FP_GRID_STEP / 2, pVar->pTemplate->nHeight * FP_GRID_STEP / 2 );
		posTerrain = MakeTransform( CVec3(0,0,fDZ) );
	}
	else
	{
		pTerrainInfo = new CTerrainInfoHolder();
		posTerrain = MakeTransform( VNULL3 );
	}
	*/
	pTerrainInfo = new CTerrainInfoHolder( mapInfo.terrain );
	RebuildTerrInfo( bResetPins );

	CDGPtr<CFuncBase<STerrainInfo> > pInfo = pTerrainInfo;
	pInfo.Refresh();
	float fDZ = GetMeterHeightCheck(pInfo->GetValue(), pVar->pTemplate->nWidth * FP_GRID_STEP / 2, pVar->pTemplate->nHeight * FP_GRID_STEP / 2 );
	posTerrain = MakeTransform( CVec3(0,0,fDZ) );

	pTerrain = new NWorld::CEditorTerrain( pShow, pTerrainInfo, pAIMap, pTime, mapInfo.nBaseTerrainFloor, sMapInfo.holesList, sMapInfo.wallsList, pBuildingGrid->GetCutFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::CreateRandom( int nVariantID, const vector<string> &params, bool bBuildingStability, 
	const list< CPtr<NScenario::CScenarioClue> > &clues, int nMobsLevel, CObj<CPostWorldCreateInfo> *pPostInfo, SRandomSeed sSeed, bool bLeanAndMean )
{
	pShow = new NWorld::CWorldSyncSrc;
	pShowUnits = new NWorld::CWorldSyncSrc;
	pAIMap = NAI::CreateAIMap( this );
	pPathNetwork = NAI::CreateNodesNetwork( pAIMap, 0 );
	pBuildingGrid = new NBuilding::CBuildingGrid;
	pSWMap = NBuilding::MakeSWMap( nVariantID, pBuildingGrid->GetSeed() );
	pBuildInfo = NGScene::shareBuildings.Get( nVariantID );
	pBuildInfo.Refresh();
	
  CDBTable<NDb::CTemplVariant> *pVarTable = NDatabase::GetTable<NDb::CTemplVariant>();
  if ( !pVarTable )
		return;
	pVar = pVarTable->GetRecord( nVariantID );
	if ( !pVar || !pVar->pTemplate )
		return;

	CreateTerrain( true );
	//pAIMap->AddTerrain( this, pPathNetwork, nRootLayer );
	pPathNetwork->CreateLayer( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight, VNULL2, 0, 0 );
	
	NBuilding::CBuildInfo *pBInfo = pBuildInfo->GetValue();
	bTerrainAlign = pBInfo->wallFragments.empty() && pBInfo->solidFragments.empty();
	nMaxFloor = pBInfo->nMaxFloor;
	CVec3 ptPos = VNULL3;
	// при создании сетки ее размеры указываем по максимуму, т.к. мин\макс инфо значения могут меняться во время редактирования
	pBuildingGrid->Setup( pVar->pTemplate->nWidth + 2, pVar->pTemplate->nHeight + 2, pBInfo->nMinFloor - 10, pBInfo->nMaxFloor + 10, VNULL2, MakeTransform( ptPos ) );
	pBuildingGrid->ToggleStability();
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	pBuildingGrid->SetVisibleLayers( layers );
	pBuildInfo->Updated();

	sMapBuildingInfo.pVariant = pVar;
	sMapBuildingInfo.pGrid = pBuildingGrid;
	sMapBuildingInfo.pSWMap = pSWMap;
	sMapBuildingInfo.pos = MakeTransform( ptPos ) * posTerrain;
	AddBuilding( sMapBuildingInfo );
	AddEmptySolids( pBInfo );
	//
	const CVec3 ptCSize( 0.2f, 0.2f, 0.2f );
	pMemCube = new ::CMemObject;
	pMemCube->CreateCube( -ptCSize / 2, ptCSize );
	pMemFlag = new ::CMemObject;
	pMemFlag->CreateFlag( VNULL3, 1.5f, 0.04f, 0.6f );
	AddSpots();
	AddWaypoints();
	//
	CreateOrigin();
	//
	NAI::SObjectPosition aiPos;
	pPathNetwork->SetOnLayer( &aiPos, 0, VNULL3 );
	AddMapObjects();
	AddLadders();
	AddUnits();
	AddSubTemplates();
	//
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::CreateOrigin()
{
	vector<CVec3> vx(2), vy(2), vz(2);
	vx[0] = vy[0] = vz[0] = VNULL3;
	vx[1] = CVec3( 2 * FP_GRID_STEP, 0, 0 );
	vy[1] = CVec3( 0, 2 * FP_GRID_STEP, 0 );
	vz[1] = CVec3( 0, 0, 2 * FP_GRID_STEP );

	::CMemObject *pX = new ::CMemObject;
	::CMemObject *pY = new ::CMemObject;
	::CMemObject *pZ = new ::CMemObject;
	pX->CreatePolyline( vx );
	pY->CreatePolyline( vy );
	pZ->CreatePolyline( vz );
	memOrigin.push_back( pX );
	memOrigin.push_back( pY );
	memOrigin.push_back( pZ );

	origin.push_back( new CMemMesh( pShow, pX, MakeTransform( VNULL3 ), 0, CVec4( 1, 0, 0, 1 ) ) );
	origin.push_back( new CMemMesh( pShow, pY, MakeTransform( VNULL3 ), 0, CVec4( 0, 1, 0, 1 ) ) );
	origin.push_back( new CMemMesh( pShow, pZ, MakeTransform( VNULL3 ), 0, CVec4( 0, 0, 1, 1 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::Explode( const CVec3 &ptEpicentre, int nPower )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetSpotVectors( const NBuilding::SProjectedSpot &spot, CVec3 *pVecX, CVec3 *pVecY )
{
	float fRotation = ToRadian( (float)spot.nRotation );
	float fCos = cos( fRotation ), fSin = sin( fRotation );
	CVec3 vU( spot.ptSize.x * fCos, spot.ptSize.x * fSin, 0 );
	CVec3 vV( -spot.ptSize.y * fSin, spot.ptSize.y * fCos, 0 );
	CVec3 ptX = CVec3( -vU.y, 0, -vU.x );
	CVec3 ptY = CVec3( -vV.y, 0, -vV.x );

	SHMatrix m;
	MakeMatrix( &m, VNULL3, spot.ptNormal );
	m.RotateHVector( pVecX, ptX );
	m.RotateHVector( pVecY, ptY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound GetSpotBound( const NBuilding::SProjectedSpot &spot )
{
	CVec3 ptMin( 1e30f, 1e30f, 1e30f ), ptMax( -1e30f, -1e30f, -1e30f );
	CVec3 ptX, ptY;
	GetSpotVectors( spot, &ptX, &ptY );
	CVec3 ptFar = spot.ptOrigin + ptX + ptY;
	ptX += spot.ptOrigin;
	ptY += spot.ptOrigin;
	ptMin.x = Min( ptMin.x, Min( spot.ptOrigin.x, Min( ptFar.x, Min( ptX.x, ptY.x ) ) ) );
	ptMin.y = Min( ptMin.y, Min( spot.ptOrigin.y, Min( ptFar.y, Min( ptX.y, ptY.y ) ) ) );
	ptMin.z = Min( ptMin.z, Min( spot.ptOrigin.z, Min( ptFar.z, Min( ptX.y, ptY.z ) ) ) );

	ptMax.x = Max( ptMax.x, Max( spot.ptOrigin.x, Max( ptFar.x, Max( ptX.x, ptY.x ) ) ) );
	ptMax.y = Max( ptMax.y, Max( spot.ptOrigin.y, Max( ptFar.y, Max( ptX.y, ptY.y ) ) ) );
	ptMax.z = Max( ptMax.z, Max( spot.ptOrigin.z, Max( ptFar.z, Max( ptX.y, ptY.z ) ) ) );

	SBound b;
	b.BoxInit( ptMin, ptMax );
	return b;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddTexSpot( NWysiwyg::ESpotType type, CVec3 color, 
	const NBuilding::SProjectedSpot &spot, int nSpotIndex, bool bIgnoreTerr )
{
	if ( spot.nMaterialID == -1 )
		return;
	CVec3 ptX;
	CVec3 ptY;
	GetSpotVectors( spot, &ptX, &ptY );

	typedef pair<CVec3, NWysiwyg::CSpotSel::EHandle > CSpotPoint;
	vector<CSpotPoint> spoints;
	vector<CVec3> polyline;
	int nUserID;
	CVec3 ptO = spot.ptOrigin + 0.005f * spot.ptNormal;
	CQuat rot( ToRadian( (float)spot.nRotation ), spot.ptNormal );
	SFBTransform pos = MakeTransform( ptO );
	spoints.push_back( CSpotPoint( VNULL3, NWysiwyg::CSpotSel::H_DL ) );
	spoints.push_back( CSpotPoint( ptX, NWysiwyg::CSpotSel::H_DR ) );
	spoints.push_back( CSpotPoint( ptX + ptY, NWysiwyg::CSpotSel::H_TR ) );
	spoints.push_back( CSpotPoint( ptY, NWysiwyg::CSpotSel::H_TL ) );

	for ( int i = 0; i < spoints.size(); ++i )
	{
		//nUserID = NWysiwyg::MakeUserID( BT_TEXSPOT, NWysiwyg::MakeSpotID( type, nSpotIndex, spoints[i].second ) );
		//AddMemObject( pMemCube, pos * MakeTransform( spoints[i].first ), CVec4( color, 1 ), nUserID );
		polyline.push_back( spoints[i].first );
	}
	nUserID = NWysiwyg::MakeUserID( BT_TEXSPOT, NWysiwyg::MakeSpotID( type, nSpotIndex, NWysiwyg::CSpotSel::H_C ) );
	SFBTransform tr = pos * MakeTransform( 0.5f * (ptX + ptY) );
	if ( bIgnoreTerr )
	{
		bool bOldTerrAlign = bTerrainAlign;
		bTerrainAlign = true;
		tr = tr * GetTerrainTransform( tr );
		pos = pos * GetTerrainTransform( tr );
		bTerrainAlign = bOldTerrAlign;
	}
	AddMemObject( &spots, pMemCube, tr, CVec4( color, 1 ), nUserID, bIgnoreTerr );
	//
	color += CVec3( 0.1f, 0.1f, 0.1f );
	polyline.push_back( polyline.front() );
	AddLines( &polyline, pos, color, bIgnoreTerr );
	//
	polyline.clear();
	polyline.push_back( 0.5f * ptX );
	polyline.push_back( 0.5f * ptX + ptY );
	AddLines( &polyline, pos, color, bIgnoreTerr );
	//
	polyline.clear();
	polyline.push_back( 0.5f * ptY );
	polyline.push_back( 0.5f * ptY + ptX );
	AddLines( &polyline, pos, color, bIgnoreTerr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddLines( vector<CVec3> *pPoints, const SFBTransform &pos, const CVec3 &color, bool bIgnoreTerr )
{
	SFBTransform tr = bIgnoreTerr ? pos : pos * GetTerrainTransform( pos );
	CPtr<::CMemObject> pLines = new ::CMemObject;
	for ( int i = 0; i < pPoints->size(); ++i )
	{
		CVec3 &pt = (*pPoints)[i];
		tr.forward.RotateHVector( &pt, pt );
	}
	pLines->CreatePolyline( *pPoints );
	AddMemObject( &spots, pLines, MakeTransform( VNULL3 ), CVec4( color, 1 ), 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateWorld( STime tScene, IPlayer *pPlayer )
{
	if ( tScene > 0 )
	{
		STime tCurrent = pTime->GetValue();
		pTime->Set( tScene );
	}

	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pBInfo = pBuildInfo->GetValue();
	bool bNewTerrainAlign = pBInfo->wallFragments.empty() && pBInfo->solidFragments.empty();
	bool bUpdateAIMap = false;

	if ( bHolesUpdated )
	{
		CreateTerrain();
		bHolesUpdated = false;
	}
	else
	{
		bool bForceGrassUpdate = false;
		if ( bUpdateTerrain && (!rectsToUpdateGeometry.empty() || !rectsToUpdateTexture.empty() || !rectsToUpdateGrass.empty() ) )
			RebuildTerrInfo( false );
		if ( !rectsToUpdateGeometry.empty() && bUpdateTerrain )
		{
			bUpdateAIMap = true;
			bForceGrassUpdate = true;
			for ( int i = 0; i < rectsToUpdateGeometry.size(); ++i )
				UpdateTerrainGeometry( rectsToUpdateGeometry[i] );
			rectsToUpdateGeometry.clear();
		}
		if ( !rectsToUpdateTexture.empty() && bUpdateTerrain )
		{
			for ( int i = 0; i < rectsToUpdateTexture.size(); ++i )
				UpdateTerrainTexture( rectsToUpdateTexture[i] );
			rectsToUpdateTexture.clear();
		}
		if ( !rectsToUpdateGrass.empty() && bUpdateTerrain )
		{
			bForceGrassUpdate = true;
			for ( int i = 0; i < rectsToUpdateGrass.size(); ++i )
				UpdateGrass( rectsToUpdateGrass[i] );
			rectsToUpdateGrass.clear();
		}
	//	pTerrain->Update( pBuildingGrid->GetCutFloor(), !rectsToUpdateGrass.empty() );
	//	rectsToUpdateGrass.clear();
		pTerrain->Update( pBuildingGrid->GetCutFloor(), bForceGrassUpdate );
	}
	if ( bNewTerrainAlign != bTerrainAlign )
	{
		bTerrainAlign = bNewTerrainAlign;
		UpdateAll();
		bUpdateAIMap = false;
	}
	if ( bUpdateAIMap )
		UpdateAIMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateAIMap()
{
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::ShowMainBuilding( bool bShow )
{
	if ( !bShow )
	{
		buildings.clear();
		return;
	}
	if ( !buildings.empty() )
		return;
	AddBuilding( sMapBuildingInfo );
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddSpots()
{
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	pBuildingGrid->SetVisibleLayers( layers );
	//
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( pInfo )
	{
		for ( int i = 0; i < pInfo->spots.size(); ++i )
			AddTexSpot( NWysiwyg::ST_WALL, 	CVec3( 0.6f, 0.6f, 0.7f ), pInfo->spots[i], pInfo->spots[i].nID );
	}
	//
	NDb::CTemplVariant *pVar = sMapBuildingInfo.pVariant;
	SRand rand;
	if ( pVar )
	{
		for ( int i = 0; i < pVar->terrainSpots.size(); ++i )
		{
			NDb::CRndTerrainSpot *pRSpot = pVar->terrainSpots[i];
			if ( !IsValid( pRSpot ) )
				continue;
			if ( !pBuildingGrid->IsLayerVisible( NBuilding::MakeFragmentID( LID_SPOTS, pRSpot->nLayer ) ) )
				continue;
			CPtr<NDb::CTerrainSpot> pSpot = pRSpot->CreateTerrainSpot( &rand );
			if ( !IsValid( pSpot ) )
				continue;
			NBuilding::SProjectedSpot spot;
			spot.nRotation = pSpot->nRotation;
			spot.nMaterialID = pRSpot->GetRecordID();
			spot.ptNormal = CVec3( 0, 0, 1 );
			spot.ptOrigin = CVec3( pSpot->ptPos.x, pSpot->ptPos.y, 0 );
			spot.ptSize = pSpot->ptSize;
			AddTexSpot( NWysiwyg::ST_TERRAIN, CVec3( 0.0f, 0.1f, 0.05f ), spot, i, true );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddMapObjects()
{
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;
	frozen.clear();
	objects.clear();
	iobjects.clear();
	vHoldObjects.clear();
	AddFragments( pInfo->wallFragments );
	AddFragments( pInfo->solidFragments );

	int nCutFloor = sMapBuildingInfo.pGrid->GetCutFloor();
	for ( int i = 0; i < sMapBuildingInfo.pVariant->pFinalElements.size(); ++i )
	{
		NDb::CFinalElement *p = sMapBuildingInfo.pVariant->pFinalElements[i];
		if ( !IsValid( p ) )
			continue;
		nMaxFloor = Max( nMaxFloor, p->nFloor );
		iobjects.push_back( IEditorObject::Create( this, p, nCutFloor, pTime ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddFragments( const vector<NBuilding::SBuildFragment> &frags )
{
	SRand rand;
	for ( int i = 0; i < frags.size(); ++i )
	{
		const NBuilding::SBuildFragment &fr = frags[i];
		NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !pTCP )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );
		if ( !IsValid( pCP ) || !IsValid( pCP->pObject ) )
			continue;
		if ( pCP->nSizeY == 0 && fr.nSubBlockID != NBuilding::GetPartHashID( 1, 1, 1 ) )
			continue;
		//
		CVec3 ptLocal( FP_GRID_STEP * fr.ptPos.x, FP_GRID_STEP * fr.ptPos.y, fr.ptPos.z * WALL_HEIGHT );
		if ( IsValid( pCP->pObject->pModels[0] ) )
			AddFrozen( MakeTransform( ptLocal, RotationIDToAngle( fr.nRotationID ) ), pCP->pObject->pModels[0]->pModel );
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> void UpdateObjects( const T &objs )
{
	for ( T::const_iterator i = objs.begin(); i != objs.end(); ++i )
		(*i)->bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> void UpdateBuildings( const T &objs )
{
	for ( T::const_iterator i = objs.begin(); i != objs.end(); ++i )
		(*i)->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateBuilding()
{
	UpdateBuildings( buildings );
	UpdateFakeSolids();
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateAll()
{
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	for ( list<CObj<CBuilding> >::const_iterator i = buildings.begin(); i != buildings.end(); ++i )
		(*i)->GetInfo().pGrid->SetVisibleLayers( layers );
	UpdateBuildings( buildings );
	NWorld::UpdateObjects( spots );
	NWorld::UpdateObjects( frozen );
	UpdateSubTemplates();
	//UpdateObjects();
	UpdateFakeSolids();
//	UpdateTerrain();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateAllBuildingParts()
{
	for ( list<CObj<CBuilding> >::const_iterator i = buildings.begin(); i != buildings.end(); ++i )
		(*i)->UpdateAllParts();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::ResetBuilding()
{
	UpdateAllBuildingParts();
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateTerrain()
{
	//pTerrain->Update( pBuildingGrid->GetCutFloor() );
	CreateTerrain();
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateWallSpot( int nSpotID )
{
	spots.clear();
	AddSpots();
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::RebuildTerrInfo( bool bResetPins )
{
	bool bShowHoles = GetUserSettings().GetParam( ME_TERRAIN_SHOWHOLES );
	list<SMapHole> oldholes = sMapInfo.holesList;
	list<SMapWall> oldwalls = sMapInfo.wallsList;

	sMapInfo = SMapInfo();
	if ( IsValid( pVar ) )
	{
		BuildTerrain( pVar->GetRecordID(), &sMapInfo, GetUserSettings().GetSubTemplateDepth() + 1, bResetPins, bShowHoles );
		pTerrainInfo->GetWritableInfo() = sMapInfo.terrain;
		if ( bShowHoles && (oldholes != sMapInfo.holesList || oldwalls != sMapInfo.wallsList) )
			bHolesUpdated = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::InvalidateTerrainGeometryRect( const CTRect<float> &sRegion )
{
	rectsToUpdateGeometry.push_back( sRegion );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::InvalidateTerrainTextureRect( const CTRect<float> &sRegion )
{
	rectsToUpdateTexture.push_back( sRegion );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::InvalidateTerrainGrassRect( const CTRect<float> &sRect )
{
	rectsToUpdateGrass.push_back( sRect );
	rectsToUpdateTexture.push_back( sRect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateTerrainGeometry( const CTRect<float> &sReg )
{
	CTRect<int> reg( sReg.minx * FP_INV_GRID_STEP, sReg.miny * FP_INV_GRID_STEP, sReg.maxx * FP_INV_GRID_STEP, sReg.maxy * FP_INV_GRID_STEP );
	pTerrainInfo->UpdateRegionGeometry( reg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateTerrainTexture( const CTRect<float> &sReg )
{
	CTRect<int> reg( sReg.minx * FP_INV_GRID_STEP, sReg.miny * FP_INV_GRID_STEP, sReg.maxx * FP_INV_GRID_STEP, sReg.maxy * FP_INV_GRID_STEP );
	pTerrainInfo->UpdateRegionTexture( reg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateGrass( const CTRect<float> &sReg )
{
	CTRect<int> reg( sReg.minx * FP_INV_GRID_STEP, sReg.miny * FP_INV_GRID_STEP, sReg.maxx * FP_INV_GRID_STEP, sReg.maxy * FP_INV_GRID_STEP );
	pTerrainInfo->UpdateRegionGrass( reg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateTerrainSpots( const vector<NBuilding::SProjectedSpot> &rspots )
{
	spots.clear();
	AddSpots();
	//
	if ( spots.empty() )
		return;
	
	CTRect<float> reg( 1e6, 1e6, -1e6, -1e6 );

	for ( int i = 0; i < rspots.size(); ++i )
	{
		const NBuilding::SProjectedSpot &spot = rspots[i];
		CVec3 ptX, ptY;
		GetSpotVectors( spot, &ptX, &ptY );
		CVec3 ptFar = spot.ptOrigin + ptX + ptY;
		ptX += spot.ptOrigin;
		ptY += spot.ptOrigin;
		reg.minx = Min( (float)reg.minx, Min( spot.ptOrigin.x, Min( ptFar.x, Min( ptX.x, ptY.x ) ) ) );
		reg.miny = Min( (float)reg.miny, Min( spot.ptOrigin.y, Min( ptFar.y, Min( ptX.y, ptY.y ) ) ) );
		reg.maxx = Max( (float)reg.maxx, Max( spot.ptOrigin.x, Max( ptFar.x, Max( ptX.x, ptY.x ) ) ) );
		reg.maxy = Max( (float)reg.maxy, Max( spot.ptOrigin.y, Max( ptFar.y, Max( ptX.y, ptY.y ) ) ) );
	}
	rectsToUpdateTexture.push_back( reg );
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateSubTemplates()
{
	for ( list<CObj<IEditorSubTemplate> >::iterator i = subtemplates.begin(); i != subtemplates.end(); )
	{
		if ( !IsValid( (*i)->GetDBRect() ) )
		{
			i = subtemplates.erase( i );
			continue;
		}
		(*i)->Update( pBuildingGrid->GetCutFloor(), 0 );
		++i;
	}
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateSubTemplate( int nUserID )
{
	for ( list<CObj<IEditorSubTemplate> >::iterator i = subtemplates.begin(); i != subtemplates.end(); ++i )
	{
		if ( (*i)->GetUserID() == nUserID )
		{
			if ( !IsValid( (*i)->GetDBRect() ) )
			{
				CTRect<float> r;
				(*i)->GetOccupiedRect( &r );
				rectsToUpdateGeometry.push_back( r );
				rectsToUpdateTexture.push_back( r );
				rectsToUpdateGrass.push_back( r );
				subtemplates.erase( i );
			}
			else
			{
				vector<CTRect<float> > rects;
				(*i)->Update( pBuildingGrid->GetCutFloor(), &rects );
				rectsToUpdateGeometry.insert( rectsToUpdateGeometry.end(), rects.begin(), rects.end() );
				rectsToUpdateTexture.insert( rectsToUpdateTexture.end(), rects.begin(), rects.end() );
				rectsToUpdateGrass.insert( rectsToUpdateGrass.end(), rects.begin(), rects.end() );
			}
			pAIMap->Sync();
			return;
		}
	}
	// такого подтемплейта в списке неоказалось, может его только что добавили?
	EBrushType type;
	int nID;
	NWysiwyg::GetFragmentID( nUserID, &type, &nID );
	if ( type != BT_SUBTEMPLATE )
	{
		ASSERT(0);
		return;
	}
	for ( int i = 0; i < pVar->rects.size(); ++i )
	{
		NDb::CRectangle *pRect = pVar->rects[i];
		if ( !IsValid( pRect ) || pRect->GetRecordID() != nID )
			continue;
		int nCutFloor = pBuildingGrid->GetCutFloor();
		subtemplates.push_back( IEditorSubTemplate::Create( this, pRect, nCutFloor ) );
		CTRect<float> r;
		if ( subtemplates.back()->GetOccupiedRect( &r ) )
		{
			rectsToUpdateGeometry.push_back( r );
			rectsToUpdateTexture.push_back( r );
			rectsToUpdateGrass.push_back( r );
		}
		pAIMap->Sync();
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IBuilding* CEditorWorld::GetMainBuildingInterface()
{
	if ( buildings.empty() )
		return false;
	return buildings.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddEmptySolids( NBuilding::CBuildInfo *pBInfo )
{
	SRand rand;
	CVec4 color( 0.6f, 0.6f, 0.65f, 0.6f );
	for ( int i = 0; i < pBInfo->solidFragments.size(); ++i )
	{
		const NBuilding::SBuildFragment &fr = pBInfo->solidFragments[i];
		if ( !pBuildingGrid->IsLayerVisible( fr.nFragmentID ) )
			continue;
		NDb::CTConstructionPart *pTConstructionPart = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !IsValid( pTConstructionPart ) )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTConstructionPart->CreateConstructionPart( &rand );
		if ( !IsValid( pCP ) || IsValid( pCP->pGeometry ) )
			continue;
		CPtr<CMemObject> pCube = new ::CMemObject;
		const float fStep = 2 * FP_GRID_STEP;
		pCube->CreateCube( VNULL3, CVec3( pCP->nSizeX * fStep, pCP->nSizeY * fStep, pCP->nSizeZ * NBuilding::WALL_HEIGHT ), true );
		CVec3 ptShift = VNULL3;
		switch ( fr.nRotationID )
		{
			case SDiscretePos::TURN_90:
				ptShift = CVec3( 2 * pCP->nSizeY, 0, 0 );
				break;
			case SDiscretePos::TURN_180:
				ptShift = CVec3( 2 * pCP->nSizeX, 2 * pCP->nSizeY, 0 );
				break;
			case SDiscretePos::TURN_270:
				ptShift = CVec3( 0, 2 * pCP->nSizeX, 0 );
				break;
		}
		ptShift += fr.ptPos;
		CVec3 ptPos( ptShift.x * FP_GRID_STEP, ptShift.y * FP_GRID_STEP, ptShift.z * NBuilding::WALL_HEIGHT );
		int nUserID = NWysiwyg::MakeUserID( BT_GEOMETRY, i );
		AddMemObject( &solids, pCube, MakeTransform( ptPos, RotationIDToAngle( fr.nRotationID ) ), color, nUserID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddWaypoints()
{
	if ( !IsValid( sMapBuildingInfo.pVariant ) )
		return;
	const vector<CPtr<NDb::CWaypoint> > &wps = sMapBuildingInfo.pVariant->waypoints;
	for ( int i = 0; i < wps.size(); ++i )
	{
		if ( !IsValid( wps[i] ) )
			continue;
		CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( wps[i]->GetRecordID() );
		pWPLoader.Refresh();
		NAI::CWaypoint *pWP = pWPLoader->GetValue();
		if ( !pWP )
			continue;
		int nUserID = NWysiwyg::MakeUserID( BT_WAYPOINT, wps[i]->GetRecordID() );
		CVec3 pt = FP_GRID_STEP * pWP->ptPos;
		pt.z = pWP->ptPos.z + pWP->nFloor * NBuilding::WALL_HEIGHT;
		AddMemObject( &waypoints, pMemFlag, MakeTransform( pt ), CVec4( 1, 0.3f, 0.2f, 1 ), nUserID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateObjects()
{
	for ( list<CObj<IEditorObject> >::iterator i = iobjects.begin(); i != iobjects.end(); )
	{
		if ( !IsValid( (*i)->GetDBElement() ) )
		{
			i = iobjects.erase( i );
			continue;
		}
		(*i)->Update( pBuildingGrid->GetCutFloor() );
		++i;
	}
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddLadders()
{
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();

	for ( int i = 0; i < pInfo->ladders.size(); ++i )
	{
		NBuilding::SLadder &lad = pInfo->ladders[i];

		if ( lad.nID <= 0 )
			continue;
		int nUserID = NWysiwyg::MakeUserID( BT_LADDER, lad.nID );
		CVec3 pt( lad.pos.ptMove.x, lad.pos.ptMove.y, 0 );
		pt *= FP_GRID_STEP;
		pt.z = lad.pos.ptMove.z * NBuilding::WALL_HEIGHT;

		CPtr<CMemObject> pLad = new ::CMemObject;
		//pLad->CreateCube( VNULL3, CVec3( FP_GRID_STEP, 0.5f * FP_GRID_STEP, lad.nHeight * NAI::F_LADDER_STEP ), true );
		pLad->CreateIsoscelesColumn( VNULL3, lad.nHeight * NAI::F_LADDER_STEP, FP_GRID_STEP );

		AddMemObject( &ladders, pLad, MakeTransform( pt, RotationIDToAngle( lad.pos.nRotation ) ), CVec4( 0.2f, 0.51f, 0.8f, 1 ), nUserID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateUnits()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::AddSubTemplates()
{
	for ( int i = 0; i < pVar->rects.size(); ++i )
	{
		NDb::CRectangle *pRect = pVar->rects[i];
		if ( !IsValid( pRect ) )
			continue;
		int nCutFloor = pBuildingGrid->GetCutFloor();
		IEditorSubTemplate *pSub = IEditorSubTemplate::Create( this, pRect, nCutFloor );
		subtemplates.push_back( pSub );
		CTRect<float> r;
		if ( pSub->GetOccupiedRect( &r ) )
		{
			rectsToUpdateGrass.push_back( r );
			rectsToUpdateGeometry.push_back( r );
			rectsToUpdateTexture.push_back( r );
		}
		nMaxFloor = Max( nMaxFloor, pSub->GetMaxFloor() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateCutFloor()
{
	UpdateBuildings( buildings );
	NWorld::UpdateObjects( spots );
	NWorld::UpdateObjects( frozen );
	UpdateSubTemplates();
	const int nCutFloor = pBuildingGrid->GetCutFloor();
	for ( list<CObj<IEditorObject> >::iterator i = iobjects.begin(); i != iobjects.end(); ++i )
		(*i)->UpdateCutFloor( nCutFloor );
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateUnit( int nDBUnitID )
{
	for ( list<CObj<IEditorUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		NDb::CUnit *pU = (*i)->GetDBUnit();
		if ( IsValid( pU ) && pU->GetRecordID() == nDBUnitID )
		{
			if ( !IsValid( pU->pMonster ) )
				units.erase( i );
			else
				(*i)->Update();
			pAIMap->Sync();
			return;
		}
	}
	// такого юнита в списке неоказалось, может его только что добавили?
	for ( int i = 0; i < pVar->pUnits.size(); ++i )
	{
		NDb::CUnit *pU = pVar->pUnits[i];
		if ( !IsValid( pU ) || pU->GetRecordID() != nDBUnitID )
			continue;
		units.push_back( IEditorUnit::Create( this, pU, GetTerrainTransform( pU->ptPos.x, pU->ptPos.y ) ) );
		pAIMap->Sync();
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateWaypoints()
{
	waypoints.clear();
	AddWaypoints();
	UpdateAIMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline bool IsExplosion( NDb::CFinalElement *p )
{
	return IsValid( p ) && IsValid( p->pObject ) && IsValid( p->pObject->pRPGItem ) && 189 == p->pObject->pRPGItem->GetRecordID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateExplosions()
{
	pBuildingGrid->Reset();
	for ( list<CObj<IEditorObject> >::iterator i = iobjects.begin(); i != iobjects.end(); ++i )
	{
		NDb::CFinalElement *pF = (*i)->GetDBElement();
		if ( !IsExplosion( pF ) )
			continue;
		CVec3 pt( pF->ptPos, NBuilding::WALL_HEIGHT * pF->nFloor + pF->fDZ );
		pBuildingGrid->Explode( pt, pF->fPower, pF->fRadius );
	}
	ResetBuilding();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateObject( int nDBObjectID )
{
	int nCutFloor = pBuildingGrid->GetCutFloor();
	for ( list<CObj<IEditorObject> >::iterator i = iobjects.begin(); i != iobjects.end(); )
	{
		NDb::CFinalElement *pF = (*i)->GetDBElement();
		if ( !IsValid( pF ) )
		{
			i = iobjects.erase( i );
			continue;
		}
		if ( pF->GetRecordID() == nDBObjectID )
		{
			if ( !IsValid( pF->pVariant ) )
				iobjects.erase( i );
			else
			{
//				if ( IsExplosion( pF ) )
//					UpdateExplosions();
				(*i)->Update( nCutFloor );
			}
			pAIMap->Sync();
			return;
		}
		++i;
	}
	// такого юнита в списке неоказалось, может его только что добавили?
	for ( int i = 0; i < pVar->pFinalElements.size(); ++i )
	{
		NDb::CFinalElement *pF = pVar->pFinalElements[i];
		if ( !IsValid( pF ) || pF->GetRecordID() != nDBObjectID )
			continue;
		iobjects.push_back( IEditorObject::Create( this, pF, nCutFloor, pTime ) );
		pAIMap->Sync();
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::OpenObject( int nDBObjectID )
{
	for ( list<CObj<IEditorObject> >::iterator i = iobjects.begin(); i != iobjects.end(); ++i )
	{
		NDb::CFinalElement *pF = (*i)->GetDBElement();
		if ( IsValid( pF ) && pF->GetRecordID() == nDBObjectID )
		{
			if ( IsValid( pF->pVariant ) )
				(*i)->ToggleOpen();
			pAIMap->Sync();
			return;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateLadders()
{
	ladders.clear();
	AddLadders();
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STerrainInfo& CEditorWorld::GetTerrainInfo()
{
	CDGPtr<CFuncBase<STerrainInfo> > pInfo = pTerrainInfo;
	pInfo.Refresh();
	return pInfo->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::EnableTerrainUpdate( bool bEnable )
{
	bUpdateTerrain = bEnable;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::ShowInfo( bool bShow )
{
	if ( bShowInfo == bShow )
		return;
	bShowInfo = bShow;
	//
	for ( list<CObj<IEditorSubTemplate> >::iterator i = subtemplates.begin(); i != subtemplates.end(); ++i )
		if ( IsValid( *i ) )
			(*i)->ShowInfo( bShow );
	//
	if ( bShow )
	{
		AddSpots();
		AddLadders();
		AddWaypoints();
		pBuildInfo.Refresh();
		AddEmptySolids( pBuildInfo->GetValue() );
	}
	else
	{
		spots.clear();
		ladders.clear();
		waypoints.clear();
		solids.clear();
	}
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CEditorWorld::GetWorldID() const 
{ 
	if ( IsValid( pVar ) ) 
		return pVar->GetRecordID();
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorWorld::UpdateFakeSolids()
{
	solids.clear();
	pBuildInfo.Refresh();
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	pBuildingGrid->SetVisibleLayers( layers );
	AddEmptySolids( pBuildInfo->GetValue() );
	pAIMap->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
BASIC_REGISTER_CLASS( CEditorWorld );
