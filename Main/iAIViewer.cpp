#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "iMain.h"
#include "aiMap.h"
#include "MemObject.h"
#include "..\Input\Input.h"
#include "..\Input\Bind.h"
#include "Camera.h"
#include "Transform.h"
#include "iAIViewer.h"
#include "Interface.h"
#include "aiPosition.h"
#include "aiRender.h"
#include "aiVision.h"
#include "wInterface.h"
#include "wExplTracker.h"
#include "..\Misc\RandomGen.h"
#include "wOSBase.h"
#include "Grid.h"
#include "RPGGame.h"
#include "RPGVision.h"

#include "aiCollider.h"
#define PFINDER_DEBUG
#ifdef PFINDER_DEBUG
	#include "aiGrid.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOV = 60;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIViewer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIViewer: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CAIViewer);
	//
	CPtr<ICamera> pCamera;
	CPtr<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	NInput::CBind cTrace, cExit, cNextLayer, cPrevLayer, cShowGrid, cShowKnots, cClearTrace;
	NInput::CBind cGridTrace, cShowNetwork, cShadows, cPerspTrace, cVerifyVision;
	NInput::CBind cShowFragments, cShowLayVariants;
	NInput::CBind cPathfindColouring;
	NInput::CBind cRPGArmorMode;
	NInput::CBind cVoxelTracer, cVoxelExpl;
	NInput::CBind cTSGoOver, cTSVision, cTSVirtual, cTSPick, cTSPassBlocker, cTSCover, cTSItemBlocker;
	NInput::CBind cGetApproaches, cVoxelVision, cShowVisionCache;
	enum EColorState
	{
		NORMAL,
		ARMOR,
		VISION = NWorld::TS_VISION,
		VIRTUAL = NWorld::TS_VIRTUAL,
		PICK = NWorld::TS_PICK,
		PASS_BLOCKER = NWorld::TS_PASS_BLOCKER,
		COVER = NWorld::TS_COVER,
		ITEM_BLOCKER = NWorld::TS_ITEM_BLOCKER,
	};
	inline bool IsTSFlagState( EColorState s ) { return s != NORMAL && s != ARMOR; }
	ZDATA
	CPtr<NAI::IAIMap> pAIMap;
	CPtr<NGScene::IGameView> pScene;
	list<CObj<CObjectBase> > objects, tracePoints;
	list<CObj<CObjectBase> > knots;
	list<CObj<CObjectBase> > voxels;
	list<CObj<NGScene::CPolyline> > voxelsCubes;
	list<CObj<NGScene::CPolyline> > traceLines;
	int nCurrentLayer;
	CPtr<NAI::IPathNetwork> pNetwork;
	bool bShowFragments, bShowLayVariants;
	EColorState colorState;
	CPtr<NRPG::IVisionTracker> pVision;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIMap); f.Add(3,&pScene); f.Add(4,&objects); f.Add(5,&tracePoints); f.Add(6,&knots); f.Add(7,&voxels); f.Add(8,&voxelsCubes); f.Add(9,&traceLines); f.Add(10,&nCurrentLayer); f.Add(11,&pNetwork); f.Add(12,&bShowFragments); f.Add(13,&bShowLayVariants); f.Add(14,&colorState); f.Add(15,&pVision); return 0; }
	// 
	void Trace();
	void TraceRay( const CRay &r, bool bTakeAll );
	void GridTrace( NAI::CFastRenderer *pRender );
	void PerspTrace();
	void GridTrace();
	void ShowObjects();
	void ShowKnots();
	void NextLayer( int nDelta );
	void ToggleGrid();
	void ToggleKnots();
	void AddLayerKnots( int nLayer );
	void ShowNetwork( bool bOnlyOneColor );
	bool GetEntityUnderCursor( CVec3 *pWhere, const NAI::SSourceInfo **pEntity );
	bool GetPointUnderCursor( CVec3 *pRes ) 
	{
		const NAI::SSourceInfo *pInfo;
		return GetEntityUnderCursor( pRes, &pInfo ); 
	}
	void VerifyVision();
	void AddSphere( list<CObj<CObjectBase> > *holder, CVec3 ptCenter, float fRadius, CVec4 color );
	void AddCube( list<CObj<NGScene::CPolyline> > *holder, CVec3 ptFirst, CVec3 ptSecond, CVec3 color );
	void VerifyVoxelTracer();
	void VerifyVoxelExpl();
	void ShowVisionCache();
	void ShowObjects( EColorState newState );
	void ShowGetApproaches();
	NAI::CFloorsSet GetFloorSet();
	void VerifyVoxelVision();
public:
	CAIViewer( NAI::IPathNetwork *_pNet = 0, NRPG::IVisionTracker *_pVision = 0 );
	//
	void ImportAIMap( NAI::IAIMap *pMap );
	void SetCamera( const ICamera::SCameraPos &cameraPos );
	virtual bool ProcessEvent( const NInput::SEvent &eEvent );
	virtual void Step();
	void OnGetFocus() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIViewer::CAIViewer( NAI::IPathNetwork *_pNet, NRPG::IVisionTracker *_pVision )
	: cTrace("traceAIView"), cExit("cancel"), pNetwork(_pNet), pVision(_pVision),
	cNextLayer("aiNextLayer"), cPrevLayer("aiPrevLayer"), cShowGrid("aiShowGrid"), cShowKnots("aiShowKnots"),
	cClearTrace("aiClearTrace"), cGridTrace("aiGridTrace"), cShowNetwork("aiShowNetwork"), 
	cShadows("toggle_shadows"), cPerspTrace("aiPerspGridTrace"), cVerifyVision("aiVerifyVision"), 
	cShowFragments("aiShowFragments"), cPathfindColouring("aiPathfindColour"), cShowLayVariants("aiLayVariants"), 
	bShowFragments(false), bShowLayVariants(false), colorState(NORMAL),
	cRPGArmorMode("aiArmorMode"), cTSGoOver("aiTSGoOver"), cTSVision("aiTSVision"), cTSVirtual("aiTSVirtual"), cTSPick("aiTSPick"),
	cTSPassBlocker("aiTSPassBlocker"), cTSCover("aiTSCover"), cTSItemBlocker("aiTSItemBlocker"), 
	cVoxelTracer("aiVoxelTracer"), cVoxelExpl("aiVoxelExpl"), cGetApproaches("aiGetApproaches"),
	cVoxelVision("aiVoxelVision"), cShowVisionCache( "aiShowVisionCache" )
{
	pScene = NGScene::CreateNewView();
	pScene->SetFastMode();
	pScene->SetHSRMode( NGScene::HSR_NONE );
	pScene->AddDirectionalLight( CVec3(0.3f,0.3f,0.3f), CVec3( 0.1f,0.3f,-1), CVec3(-1,-1,0), CVec2( 150, 150 ), 20 );
	pCamera = CreateCamera();
	///
	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );
	///	pScene->CreateText( new wstring(L"AI representation viewing mode"), 200 );
	nCurrentLayer = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CFloorsSet CAIViewer::GetFloorSet()
{
	if ( nCurrentLayer != -1 )
		return NAI::CFloorsSet( pNetwork->GetFloor( nCurrentLayer ) );
	else
		return NAI::CFloorsSet();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::SetCamera( const ICamera::SCameraPos &cameraPos )
{
	pCamera->SetPlacement( cameraPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIViewer::ProcessEvent( const NInput::SEvent &eEvent )
{
	pCursor->ProcessEvent( eEvent );
	pCamera->ProcessEvent( eEvent );

	bool bRet = pInterface->ProcessEvent( eEvent );
	if ( bRet )
		return true;

	if ( cPerspTrace.ProcessEvent( eEvent ) )
		PerspTrace();
	else if ( cShadows.ProcessEvent( eEvent ) )
		pScene->SetNextShadowsMode();
	else if ( cTrace.ProcessEvent( eEvent ) )
		Trace();
	else if ( cExit.ProcessEvent( eEvent ) )
		NMainLoop::Command( new NMainLoop::CICExitModal );
	else if ( cNextLayer.ProcessEvent( eEvent ) )
		NextLayer( 1 );
	else if ( cPrevLayer.ProcessEvent( eEvent ) )
		NextLayer( -1 );
	else if ( cShowGrid.ProcessEvent( eEvent ) )
		ToggleGrid();
	else if ( cShowKnots.ProcessEvent( eEvent ) )
		ToggleKnots();
	else if ( cClearTrace.ProcessEvent( eEvent ) )
	{
		tracePoints.clear();
		traceLines.clear();
	}
	else if ( cGridTrace.ProcessEvent( eEvent ) )
		GridTrace();
	else if ( cShowNetwork.ProcessEvent( eEvent ) )
		ShowNetwork( false );
	else if ( cVerifyVision.ProcessEvent( eEvent ) )
		VerifyVision();
	else if ( cPathfindColouring.ProcessEvent( eEvent ) )
		ShowNetwork( true );
	else if ( cShowLayVariants.ProcessEvent( eEvent ) )
		bShowLayVariants = !bShowLayVariants;
	else if ( cShowFragments.ProcessEvent( eEvent ) )
	{
		bShowFragments = !bShowFragments;
		ShowObjects();
	}
	else if ( cRPGArmorMode.ProcessEvent( eEvent ) )
		ShowObjects( ARMOR );
	else if ( cTSVision.ProcessEvent( eEvent ) )
		ShowObjects( VISION );
	else if ( cTSVirtual.ProcessEvent( eEvent ) )
		ShowObjects( VIRTUAL );
	else if ( cTSPick.ProcessEvent( eEvent ) )
		ShowObjects( PICK );
	else if ( cTSPassBlocker.ProcessEvent( eEvent ) )
		ShowObjects( PASS_BLOCKER );
	else if ( cTSCover.ProcessEvent( eEvent ) )
		ShowObjects( COVER );
	else if ( cTSItemBlocker.ProcessEvent( eEvent ) )
		ShowObjects( ITEM_BLOCKER );
	else if ( cVoxelTracer.ProcessEvent( eEvent ) )
		VerifyVoxelTracer();
	else if ( cVoxelExpl.ProcessEvent( eEvent ) )
		VerifyVoxelExpl();
	else if ( cGetApproaches.ProcessEvent( eEvent ) )
		ShowGetApproaches();
	else if ( cVoxelVision.ProcessEvent( eEvent ) )
		VerifyVoxelVision();
	else if ( cShowVisionCache.ProcessEvent( eEvent ) )
		ShowVisionCache();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::VerifyVoxelTracer()
{
	voxels.clear();
	//
	float fWidth = 2;//0.5f;
	int nResolution = 10;
	CVec3 vCenter;// = CVec3( 1, 1, 2 );
	GetPointUnderCursor( &vCenter );
	vCenter.x += random.GetFloat( -0.1f, 0.1f );
	vCenter.y += random.GetFloat( -0.1f, 0.1f );
	vCenter.z += random.GetFloat( -0.1f, 0.1f );
	NAI::CExplVoxelRenderer renderer;
	NAI::CExplVoxelRenderer::CObjectsHash objects;
	int nObjectIndex = 0;
	renderer.Init( vCenter, fWidth, nResolution, &objects, &nObjectIndex );
	float fRes = fWidth / nResolution;
	pAIMap->TraceVoxelGrid( &renderer, NWorld::TS_FRAGMENTED );
	//
	AddSphere( &voxels, vCenter, 0.01f, CVec4( 1, 0.5f, 0.9f, 1.f ) );
	for ( int x = 0; x < renderer.voxels.GetXSize(); ++x )
	{
		for ( int y = 0; y < renderer.voxels.GetYSize(); ++y )
		{
			for ( int z = 0; z < renderer.voxels.GetZSize(); ++z )
			{
				if ( renderer.voxels[x][y][z].nObject > 0 )
				{
					CVec3 ptPoint = CVec3( 
						( x - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.x,
						( y - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.y,
						( z - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.z
						);
					int n = renderer.voxels[x][y][z].nObject;
					CVec4 vColor( (n & 1) ? 0.5f : 0, (n&2) ? 0.5f : 0, (n&4) ? 0.5f : 0, 1 );
					AddSphere( &voxels, ptPoint, 0.05f, vColor );//CVec4( 1, 0.3f, 0.3f, 1.0f ) );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::VerifyVoxelVision()
{
	voxels.clear();
	//
	float F_STEP = FP_GRID_STEP / 4;
	int nResolution = 10;
	float fWidth = F_STEP * nResolution;//0.5f;
	CVec3 vCenter;// = CVec3( 1, 1, 2 );
	GetPointUnderCursor( &vCenter );
	vCenter.x += random.GetFloat( -0.1f, 0.1f );
	vCenter.y += random.GetFloat( -0.1f, 0.1f );
	vCenter.z += random.GetFloat( -0.1f, 0.1f );
	NAI::CVisionVoxelRenderer renderer;
	renderer.Init( vCenter, fWidth, nResolution, NWorld::TS_VISION_SOLID );
	pAIMap->TraceVisionGrid( &renderer, NWorld::TS_VISION );
	//
	AddSphere( &voxels, vCenter, 0.01f, CVec4( 1, 0.5f, 0.9f, 1.f ) );
	int nMargin = 1; // 0;
	for ( int x = nMargin; x < renderer.voxels.GetXSize() - nMargin; ++x )
	{
		for ( int y = nMargin; y < renderer.voxels.GetYSize() - nMargin; ++y )
		{
			for ( int z = nMargin; z < renderer.voxels.GetZSize() - nMargin; ++z )
			{
				char cTest = renderer.voxels[x][y][z];
				if ( cTest == 0 )
					continue;
				CVec4 vColor;
				if ( cTest & 0x80 )
					vColor = CVec4( 1, 1, 1, 1.0f );
				if ( cTest & 0x40 )
					vColor = CVec4( 0.3f, 0.3f, 1, 1.0f );
				CVec3 ptPoint = CVec3( 
					( x - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.x,
					( y - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.y,
					( z - nResolution * 0.5f + 0.5f ) * fWidth / (nResolution) + vCenter.z
					);
				AddSphere( &voxels, ptPoint, 0.05f, vColor );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ShowVisionCache()
{
	voxels.clear();
	CVec3 vCenter;// = CVec3( 1, 1, 2 );
	GetPointUnderCursor( &vCenter );
	CTPoint3<int> center;
	pVision->GetCoord( vCenter, &center );
	int nMargin = 10;
	for ( int x = -nMargin; x < nMargin; ++x )
	{
		for ( int y = -nMargin; y < nMargin; ++y )
		{
			for ( int z = -nMargin; z < nMargin; ++z )
			{
				NRPG::EVoxelVisionState res;
				CTPoint3<int> p( center.x + x, center.y + y, center.z + z );
				res = pVision->GetVision( p.x, p.y, p.z );
				if ( res == NRPG::VVS_NONE )
					continue;
				CVec4 vColor;
				if ( res == NRPG::VVS_SOLID )
					vColor = CVec4( 1, 1, 1, 1.0f );
				if ( res == NRPG::VVS_TRANSPARENT )
					vColor = CVec4( 0.3f, 0.3f, 1, 1.0f );
				CVec3 vPos;
				pVision->GetCenter( p, &vPos );
				AddSphere( &voxels, vPos, 0.05f, vColor );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::AddSphere( list<CObj<CObjectBase> > *holder, 
	CVec3 ptCenter, float fRadius, CVec4 color )
{
	CPtr<CMemObject> pModel = new CMemObject;
	pModel->CreateSphere( ptCenter, fRadius, 0 );
	holder->push_back( pScene->CreateMesh( pModel, color, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::AddCube( list<CObj<NGScene::CPolyline> > *holder, 
	CVec3 ptFirst, CVec3 ptSecond, CVec3 color )
{
	vector<CVec3> points;
	//
	points.clear();
	points.push_back( ptFirst ); 
	points.push_back( CVec3( ptSecond.x, ptFirst.y, ptFirst.z ) );
	points.push_back( CVec3( ptSecond.x, ptFirst.y, ptSecond.z ) );
	points.push_back( CVec3( ptFirst.x, ptFirst.y, ptSecond.z ) );
	points.push_back( ptFirst );
	holder->push_back( pScene->CreatePolyline( points, color ) );
	//
	points.clear();
	points.push_back( ptSecond );
	points.push_back( CVec3( ptFirst.x, ptSecond.y, ptSecond.z ) );
	points.push_back( CVec3( ptFirst.x, ptSecond.y, ptFirst.z ) );
	points.push_back( CVec3( ptSecond.x, ptSecond.y, ptFirst.z ) );
	points.push_back( ptSecond );
	holder->push_back( pScene->CreatePolyline( points, color ) );
	//
	points.clear();
	points.push_back( ptFirst );
	points.push_back( CVec3( ptFirst.x, ptSecond.y, ptFirst.z ) );
	holder->push_back( pScene->CreatePolyline( points, color ) );
	//
	points.clear();
	points.push_back( CVec3( ptSecond.x, ptFirst.y, ptFirst.z ) );
	points.push_back( CVec3( ptSecond.x, ptSecond.y, ptFirst.z ) );
	holder->push_back( pScene->CreatePolyline( points, color ) );
	//
	points.clear();
	points.push_back( ptSecond );
	points.push_back( CVec3( ptSecond.x, ptFirst.y, ptSecond.z ) );
	holder->push_back( pScene->CreatePolyline( points, color ) );
	//
	points.clear();
	points.push_back( CVec3( ptFirst.x, ptFirst.y, ptSecond.z ) );
	points.push_back( CVec3( ptFirst.x, ptSecond.y, ptSecond.z ) );
	holder->push_back( pScene->CreatePolyline( points, color ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::VerifyVoxelExpl()
{
	voxels.clear();
	voxelsCubes.clear();
	//
	CVec3 ptCenter;
	float fHalfSize = NWorld::F_CUBE_SIZE / 2.f;
	CVec3 ptHalfSize = CVec3( fHalfSize, fHalfSize, fHalfSize );
	GetPointUnderCursor( &ptCenter );
	CObj<NWorld::CVoxelExpl> pExplosion = new NWorld::CVoxelExpl( ptCenter, 0, 0, 0, pAIMap, 0 );
	//
	while ( !pExplosion->IsFinished() )
		pExplosion->Segment();
	//
	for ( vector< CObj<NWorld::CExplCube> >::iterator cube = pExplosion->cubes.begin();
		cube != pExplosion->cubes.end(); ++cube )
	{
		ptCenter = (*cube)->ptCenter;
		AddCube( &voxelsCubes, ptCenter - ptHalfSize, ptCenter + ptHalfSize, CVec3( 0.8f, 0.8f, 1.0f ) );
		//
		/*
		for ( int nX = 1; nX < NWorld::N_REAL_CUBE_SIZE - 1; ++nX )
			for ( int nY = 1; nY < NWorld::N_REAL_CUBE_SIZE - 1; ++nY )
				for ( int nZ = 1; nZ < NWorld::N_REAL_CUBE_SIZE - 1; ++nZ )
				{
//					if ( (*cube)->renderer.voxels[nX][nY][nZ].nObject > 0 )
					if ( (*cube)->renderer.voxels[nX][nY][nZ].nIndex > 0 )
						AddSphere( &voxels, (*cube)->GetVoxelCenter( nX, nY, nZ ), 0.05f, CVec4( 1, 0.3f, 0.3f, 1.0f ) );
//					else
//						AddSphere( &voxels, (*cube)->GetVoxelCenter( nX, nY, nZ ), 0.05f, CVec4( 1, 0.9f, 0.9f, 1.0f ) );
				}
				*/
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::Step()
{
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pCamera->Update( GetTime() );
		pInterface->Step( GetTime() );

		CTransformStack ts;
		ts.MakeProjective( pScene->GetScreenRect(), N_FOV );
		ts.SetCamera( pCamera->GetPos() );
		NGScene::IGameView::SDrawInfo drawInfo;
		drawInfo.pTS = &ts;
		drawInfo.vClearColor = CVec3(0.5f, 0.5f, 0.5f);
		pScene->Draw( drawInfo );
		pInterface->Draw( GetTime() );
		NGScene::Flip();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::NextLayer( int nDelta )
{
	if ( nCurrentLayer < 0 )
	{
		ToggleGrid();
		return;
	}
	nCurrentLayer += nDelta + pNetwork->GetNumLayers();
	nCurrentLayer %= pNetwork->GetNumLayers();
#ifdef PFINDER_DEBUG
	csSystem << CC_RED << " Layer number: " << nCurrentLayer << "\n";
	CDynamicCast<NAI::CPathNetwork> pNet( pNetwork );
	pNet->PrintLayerInfo( nCurrentLayer );
#endif
	objects.clear();
	ShowObjects();
	if ( !knots.empty() )
		ShowKnots();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ToggleGrid()
{
	objects.clear();
	if ( nCurrentLayer >= 0 )
		nCurrentLayer = -1;
	else
		nCurrentLayer = 0;
	ShowObjects();
	if ( !knots.empty() )
		ShowKnots();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ToggleKnots()
{
	if ( !knots.empty() )
		knots.clear();
	else
		ShowKnots();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::AddLayerKnots( int nLayer )
{
	// floating point coords are not shown yet
	CArray2D<bool> pass;
	pNetwork->GetPassability( &pass, nLayer );
	for ( int y = 0; y < pass.GetYSize(); ++y )
	{
		for ( int x = 0; x < pass.GetXSize(); ++x )
		{
			CPtr<CMemObject> pModel = new CMemObject;
			NAI::SPosition p;
			p.p.SetOnLayer( nLayer, x, y );
			p.SetNetwork( pNetwork );
			pModel->CreateSphere( p.GetCP(), 0.06f, 0 );
			CVec4 color = pass[y][x] ? CVec4( 0.3f, 1, 0.3f, 1.0f ) : CVec4( 1, 0.3f, 0.3f, 1.0f );
			knots.push_back( pScene->CreateMesh( pModel, color, 0 ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ShowKnots()
{
	knots.clear();
	if ( nCurrentLayer >= 0 )
		AddLayerKnots( nCurrentLayer );
	else
	{
		for ( int i = 0; i < pNetwork->GetNumLayers(); ++i )
			AddLayerKnots( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static float differentColors[7][3] = {
	{ 0.3f, 1, 0.3f },
	{ 0.3f, 0.3f, 1 },
	{ 0.3f, 1, 1 },
	{ 1, 0.3f, 0.3f },
	{ 1, 1, 0.3f },
	{ 1, 0.3f, 1 },
	{ 1, 1, 1 },
};
static CVec4 GetArmorColor( int nArmorID )
{
	switch ( nArmorID )
	{
	case 1: //Человек
		return CVec4(0.9f,0.9f,0.9f,1);
	case 2: //Грунт
		return CVec4(0.5f,0.5f,0,1);
	case 3: //Камень
		return CVec4(0.4f,0.4f,0.5f,1);
	case 4: //Дерево
		return CVec4(0.6f,0.3f,0.1f,1);
	case 5: //Кирпич
		return CVec4(1,0.2f,0,1);
	case 7: //Листва
		return CVec4(0,1,0,0.8f);
	case 8: //Стекло
		return CVec4(1,1,1,0.5f);
	case 20: //Материя
		return CVec4(1,1,0,1);
	case 21: //Сталь (корпус автомашины)
		return CVec4(0,1,1,1);
	case 22: //Бронесталь гомогенная
		return CVec4(0,0.6f,1,1);
	case 23: //Бронесталь цементированная
		return CVec4(0,0,1,1);
	case 24: //Черепица
		return CVec4(0.9f,0.6f,0.4f,1);
	case 25: //Неразрушаемый
		return CVec4(1,0,1,1);
	case 26: //Бетон особокрепкий (crap)
		return CVec4(0.5f,0,0.5f,1);
	}
	return CVec4(0,0,0,1);
}
void CAIViewer::ShowObjects( EColorState newState )
{
	if ( colorState != newState )
		colorState = newState;
	else
		colorState = NORMAL;
	ShowObjects();
}
void CAIViewer::ShowObjects()
{
	objects.clear();
	list<NAI::SObjectInfo> objs;
	int flags;
	if ( bShowFragments )
		flags = NWorld::TS_FRAGMENTED;
	else
		flags = NWorld::TS_ALL;
	if ( IsTSFlagState( colorState ) )
		flags |= colorState;
	if ( nCurrentLayer == -1 )
		pAIMap->GetEntities( &objs, flags, NAI::CFloorsSet() );
	else
		pAIMap->GetEntities( &objs, flags, GetFloorSet() );
	SFBTransform id;
	Identity( &id.forward );
	Identity( &id.backward );
	for ( list<NAI::SObjectInfo>::iterator i = objs.begin(); i != objs.end(); ++i )
	{
		NAI::SObjectInfo &info = *i;
		if ( info.tris.size() > 0 )
		{
			CPtr<CMemObject> pModel = new CMemObject;
			pModel->Create( info.points, info.tris );
			//
			CVec4 color;
			if ( colorState == NORMAL )
			{
				float *pColor = differentColors[abs(info.nPieceID) % 7];
				color = CVec4( pColor[0], pColor[1], pColor[2], 1.0f );
			}
			else if ( colorState == ARMOR )
				color = GetArmorColor( info.nArmorID );
			else 
			{
				ASSERT(	IsTSFlagState( colorState ) );
				if ( info.nTSFlags & colorState )
					color = CVec4(1,1,1,1);
				else
					color = CVec4(0,0,1,1);
			}
			//
			CObjectBase *pR = pScene->CreateMesh( pModel, color, id );
			objects.push_back( pR );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DrawLine( NGScene::IGameView *pScene, const CVec3 &src, const CVec3 &dst, const CVec3 &color,
	list<CObj<NGScene::CPolyline> > *pLines )
{
	vector<CVec3> points;
	points.push_back( src );
	points.push_back( dst );
	pLines->push_back( pScene->CreatePolyline( points, color ) );
}
void CAIViewer::Trace()
{
	CRay r;
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, pCamera->GetPos(), pScene->GetScreenRect(), pCursor->GetPos(), N_FOV );
	TraceRay( r, false );
}
void CAIViewer::TraceRay( const CRay &r, bool bTakeAll )
{
	vector<NAI::SInterval> intervals;
	if ( bTakeAll )
		pAIMap->Trace( r, &intervals, NWorld::TS_ALL, NAI::CFloorsSet() );
	else
		pAIMap->Trace( r, &intervals, NWorld::TS_ALL, GetFloorSet() );
	//
	for ( int i = 0; i < intervals.size(); i++ )
	{
		NAI::SInterval &interv = intervals[i];
		const NAI::SInterval::SCrossPoint &enter = interv.enter, &exit = interv.exit;
		CVec3 enterPoint = r.Get( enter.fT ), exitPoint = r.Get( exit.fT );
		char buf[80];
		sprintf( buf, "Enter point x = %f, y = %f, z = %f \n", enterPoint.x, enterPoint.y, enterPoint.z );
		csSystem << CC_RED << buf;
		sprintf( buf, "Exit point x = %f, y = %f, z = %f \n", exitPoint.x, exitPoint.y, exitPoint.z );
		csSystem << CC_BLUE << buf;
		CDynamicCast<NWorld::CObjectServerBase> pObj( interv.pSrc->pUserData );
		if ( pObj )
		{
			sprintf( buf, "Object ID is %d\n", pObj->GetDBObjectID() );
			csSystem << CC_GREEN << buf;
		}

		//DrawLine( pScene, enterPoint, exitPoint, CVec3(1,0.3f,0.3f) );
		CPtr<CMemObject> pModel = new CMemObject;
		pModel->CreateSphere( enterPoint, 0.06f );
		CVec4 color( 1, 0.1f, 0.1f, 1.0f );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
		DrawLine( pScene, enterPoint, enterPoint + enter.ptNormal * 0.3f, CVec3(1,1,1), &traceLines );
		//
		pModel->CreateSphere( exitPoint, 0.06f );
		color = CVec4( 0.1f, 0.1f, 1, 1.0f );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
		DrawLine( pScene, exitPoint, exitPoint + exit.ptNormal * 0.3f, CVec3(1,1,1), &traceLines );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIViewer::GetEntityUnderCursor( CVec3 *pWhere, const NAI::SSourceInfo **pEntity )
{
	CRay r;
	vector<NAI::SInterval> intervals;
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, pCamera->GetPos(), pScene->GetScreenRect(), pCursor->GetPos(), N_FOV );
	pAIMap->Trace( r, &intervals, NWorld::TS_ALL, GetFloorSet() );
	if ( intervals.empty() )
	{
		pAIMap->Trace( r, &intervals, NWorld::TS_ALL, NAI::CFloorsSet() );
		if ( intervals.empty() )
			return false;
	}
	float fT = intervals[0].enter.fT;
	if ( fabs(fT) > 1e4f )
		fT = intervals[0].exit.fT;
	*pWhere = r.Get( fT );
	*pEntity = intervals[0].pSrc;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::GridTrace( NAI::CFastRenderer *pRender )
{
	NAI::CFastRenderer &render = *pRender;
	pAIMap->TraceGrid( &render, NWorld::TS_PASS_BLOCKER, 
		NAI::IAIMap::STH_SORT_INTERVALS, GetFloorSet(), NAI::IAIMap::STH_SPLIT_TERR_HG, true );
//	pAIMap->TraceGrid( &render, NWorld::TS_ALL, NAI::IAIMap::STH_NOSORT, GetFloorSet() );
	vector<CVec3> enters, exits;
	render.GetPoints( &enters, &exits );
	CPtr<CMemObject> pModel = new CMemObject;
	pModel->CreateSphere( CVec3(0,0,0), 0.06f, 1 );
	CVec4 color = CVec4( 1, 0.1f, 0.1f, 1.0f );
	for ( int i = 0; i < enters.size(); ++i )
	{
		SFBTransform pos;
		MakeMatrix( &pos, CVec3(1,1,1), enters[i] );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, pos ) );
	}
	color = CVec4( 0.1f, 0.1f, 1, 1.0f );
	for ( int i = 0; i < exits.size(); ++i )
	{
		SFBTransform pos;
		MakeMatrix( &pos, CVec3(1,1,1), exits[i] );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, pos ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::GridTrace()
{/*
	CVec3 ptCenter;
	if ( !GetPointUnderCursor( &ptCenter ) )
		return;
	NAI::CFastRenderer render;
	render.InitParallel( CVec2( ptCenter.x, ptCenter.y ), 0, 0.3f, CTRect<int>(-15,-10,15,10) );
	GridTrace( &render );*/

	CVec3 ptCenter;
	if ( !GetPointUnderCursor( &ptCenter ) )
		return;

	CPtr<NAI::CCollider> pCollider = new NAI::CCollider;
	NAI::CCollider &collider = *pCollider;
	SBound test;
	CVec3 ptMin( ptCenter.x - 0.8f, ptCenter.y - 0.8f, ptCenter.z - 0.8f ); 
	CVec3 ptMax( ptCenter.x + 0.85f, ptCenter.y + 0.85f, ptCenter.z + 0.85f ); 
	test.BoxInit( ptMin, ptMax );
	test.Extend( 1 );
	pAIMap->PrepareCollider( &collider, test, 0.2f, NWorld::TS_PASS_BLOCKER, false );
	CVec3 vec( ptCenter );
	CPtr<CMemObject> pModel = new CMemObject;
	CVec4 color = CVec4( 1, 0.1f, 0.1f, 1.0f );
	//for ( int i = 0; i < 140; ++i )
	{
		for ( vec.x = ptCenter.x - 0.8f; vec.x < ptCenter.x + 0.85f; vec.x += 0.1f )
		{
			for ( vec.y = ptCenter.y - 0.8f; vec.y < ptCenter.y + 0.85f; vec.y += 0.1f )
			{
				for ( vec.z = ptCenter.z - 0.8f; vec.z < ptCenter.z + 8; vec.z += 0.1f )
				{
					if ( collider.DoesIntersect( vec, 0.31f ) )
					{
						SFBTransform pos;
						float zStart = vec.z;
						vec.z += 0.1f;
						while ( collider.DoesIntersect( vec, 0.31f ) && vec.z < ptCenter.z + 8 )
							vec.z += 0.1f;
						pModel->CreateCube( CVec3(0,0,0), CVec3( 0.1f, 0.1f, 0.1f * ( vec.z - zStart ) ) );
						MakeMatrix( &pos, CVec3(1,1,1), vec + CVec3(-0.05f,-0.05f,-0.05f) );
						tracePoints.push_back( pScene->CreateMesh( pModel, color, pos ) );
						continue;
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::PerspTrace()
{
	CVec3 ptCenter;
	if ( !GetPointUnderCursor( &ptCenter ) )
		return;
	SHMatrix cameraPos;
	MakeMatrix( &cameraPos, pCamera->GetCP(), ptCenter - pCamera->GetCP() );
	NAI::CFastRenderer render;
	render.InitProjective( cameraPos, 150, 15, 10 );
	GridTrace( &render );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ShowNetwork( bool bOnlyOneColor )
{
	CVec3 ptCenter;
	if ( !GetPointUnderCursor( &ptCenter ) )
		return;
	if ( bOnlyOneColor )
	{
		tracePoints.clear();
		traceLines.clear();	
	}
	NAI::SPosition pos;
	int nLayer = nCurrentLayer;
	if ( nLayer < 0 )
		nLayer = pNetwork->GetNumLayers() - 1;
	pNetwork->SetOnLayer( &pos, nLayer, ptCenter );
	vector<NAI::SShowPoint> knots;
	vector<NAI::SShowLink> links;
	pNetwork->GetNetworkFragment( pos, bOnlyOneColor, &knots, &links );

#ifdef PFINDER_DEBUG
	// showing console info
	csSystem << CC_RED << " Layer number: " << nCurrentLayer << "\n";
	CDynamicCast<NAI::CPathNetwork> pNet( pNetwork );
	pNet->PrintConsoleInfo( pos.p );
	// showing ray
	CVec3 vPos = pNet->GetCP( pos.p );
	CRay r;
	r.ptOrigin = vPos;
	r.ptOrigin.z -= 20;
	r.ptDir = CVec3(0,0,1);
	TraceRay( r, false );
	NAI::CFastRenderer render;
	render.InitParallel( CVec2( vPos.x, vPos.y ), 0, 0.625f / 3, CTRect<int>(-15,-10,15,10) );
	GridTrace( &render );
#endif

	CPtr<CMemObject> pModel = new CMemObject;
	pModel->CreateSphere( CVec3(0,0,0), 0.06f, 1 );
	CVec3 vecX( 0.03f, 0.01f, 0.0f ), vecY( 0.01f, 0.03f, 0.0 );
	for ( int i = 0; i < knots.size(); ++i )
	{
		SFBTransform pos;
		MakeMatrix( &pos, CVec3(1,1,1), knots[i].pos );
		CVec4 color;
		bool bDrawBigSphere = true;
		if ( knots[i].nFlags & NAI::SShowPoint::COLOR_CENTER )
			color = CVec4( 1.0f, 0.0f, 0.0f, 1.0f );
		else
		{
			switch ( knots[i].nFlags )
			{
				case NAI::SShowPoint::EVERY_POSE: color = CVec4( 0.1f, 1, 0.1f, 1.0f ); break;
				case NAI::SShowPoint::BOW_LEGGED: color = CVec4( 0.1f, 1, 1, 1.0f ); break;
				case NAI::SShowPoint::EVERY_POSE|NAI::SShowPoint::LOCKED:     color = CVec4( 1, 1, 0.1f, 1 ); break;
				case NAI::SShowPoint::BOW_LEGGED|NAI::SShowPoint::LOCKED: color = CVec4( 1, 1, 1, 1 ); break;
				default: bDrawBigSphere = false; break;
			}
		}
		if ( knots[i].nFlags & NAI::SShowPoint::LOCKED )
		{
			color = CVec4( 1, 1, 0.1f, 1 ); 
			bDrawBigSphere = true;
		}
		if ( bDrawBigSphere )
			tracePoints.push_back( pScene->CreateMesh( pModel, color, pos ) );
		else
		{
			if ( knots[i].nFlags & NAI::SShowPoint::CAN_STAND )
			{
				CPtr<CMemObject> pSmallModel = new CMemObject;
				pSmallModel->CreateSphere( CVec3(0.03f,0,0), 0.03f, 1 );
				tracePoints.push_back( pScene->CreateMesh( pSmallModel, CVec4( 0.4f, 1, 0.4f, 1 ), pos ) );
			}
			if ( knots[i].nFlags & NAI::SShowPoint::CAN_LAY )
			{
				CPtr<CMemObject> pSmallModel = new CMemObject;
				pSmallModel->CreateSphere( CVec3(-0.03f,-0.03f,0), 0.03f, 1 );
				tracePoints.push_back( pScene->CreateMesh( pSmallModel, CVec4( 1, 0.4f, 0.4f, 1 ), pos ) );
			}
			if ( knots[i].nFlags & NAI::SShowPoint::CAN_CROUCH )
			{
				CPtr<CMemObject> pSmallModel = new CMemObject;
				pSmallModel->CreateSphere( CVec3(0,0.03f,0), 0.03f, 1 );
				tracePoints.push_back( pScene->CreateMesh( pSmallModel, CVec4( 0.4f, 0.4f, 0.4f, 1 ), pos ) );
			}
		}
	}
	for ( int i = 0; i < links.size(); ++i )
	{
		links[i].start.z += 0.1f;
		links[i].finish.z += 0.1f;
		if ( links[i].t == NAI::SShowLink::STAND_MOVE )
			DrawLine( pScene, links[i].start + vecX, links[i].finish + vecX, CVec3(0.4f, 1, 0.4f), &traceLines );
		if ( links[i].t == NAI::SShowLink::CROUCH_MOVE )
			DrawLine( pScene, links[i].start + vecY, links[i].finish + vecY, CVec3(0.4f, 0.4f, 0.4f), &traceLines );
		if ( links[i].t == NAI::SShowLink::CRAWL_MOVE )
			DrawLine( pScene, links[i].start - vecX - vecY, links[i].finish - vecX - vecY, CVec3(1, 0.4f, 0.4f), &traceLines );
		if ( links[i].t == NAI::SShowLink::LAY_POSE_SHOW && bShowLayVariants )
			DrawLine( pScene, links[i].start - vecX - vecY, links[i].finish - vecX - vecY, CVec3(1, 0.4f, 0.4f), &traceLines );
		switch ( links[i].t )
		{
			case NAI::SShowLink::ANY_MOVE:
				DrawLine( pScene, links[i].start, links[i].finish, CVec3(1,1,1), &traceLines );
				break;
			case NAI::SShowLink::DIRECT:
				DrawLine( pScene, links[i].start, links[i].finish, CVec3(0,0.1f,0.1f), &traceLines );
				break;
			case NAI::SShowLink::HEIGHT_CHANGE:
				DrawLine( pScene, links[i].start, links[i].finish, CVec3(0,1,0), &traceLines );
				break;
			case NAI::SShowLink::NEIGHBOUR_ZONE_ANY_MOVE:
				DrawLine( pScene, links[i].start, links[i].finish, CVec3(1,1,0), &traceLines );
				break;
			case NAI::SShowLink::NEIGHBOUR_ZONE_STAND_ONLY:
				DrawLine( pScene, links[i].start, links[i].finish, CVec3(1,0,1), &traceLines );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::VerifyVision()
{
	CVec3 ptCenter = pCamera->GetCP(), ptTarget;
	if ( !GetPointUnderCursor( &ptTarget ) )
		return;

	const int nHalfSize = 32;
	NAI::SCalcIndex index;
	NAI::CFastRenderer render;
	NAI::GetIndex( &index, ptCenter, ptTarget, nHalfSize );
	NAI::TraceSide( pAIMap, &render, ptCenter, index.nSide, 40, nHalfSize, NWorld::TS_VISION );

	vector<CVec3> enters, exits;
	render.GetPoints( &enters, &exits, index.nX, index.nY );

	CPtr<CMemObject> pModel = new CMemObject;
	DrawLine( pScene, ptCenter, ptTarget, CVec3(1,0,0), &traceLines );
	ASSERT( enters.size() == exits.size() );
	for ( int i = 0; i < Min( enters.size(), exits.size() ); i++ )
	{
		pModel->CreateSphere( enters[i], 0.06f );
		CVec4 color = CVec4( 0, 1, 1, 1 );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
		pModel->CreateSphere( exits[i], 0.06f );
		color = CVec4( 0, 0, 1, 1 );
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
	}
	//DrawLine( pScene, ptCenter, exits.back(), CVec3(0, 1, 0), &traceLines );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ShowGetApproaches()
{
	const NAI::SSourceInfo *pInfo = 0;
	CVec3 where;
	if ( !GetEntityUnderCursor( &where, &pInfo ) )
		return;
	if ( !pInfo )
		return;
	CDynamicCast<NWorld::IGetApproaches> pGetAppr( pInfo->pUserData );
	if ( !pGetAppr )
		return;
	
	CPtr<CMemObject> pModel = new CMemObject;
	vector<CVec3> pts;
	pGetAppr->GetApproachPts( &pts );
	for ( int i = 0; i < pts.size(); ++i )
	{
		pModel->CreateSphere( pts[i], 0.04f );
		CVec4 color = CVec4( 0.4f, 1, 0, 1 ); 
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
	}
	
	vector<NAI::SPathPlace> res;
	pGetAppr->GetApproaches( &res, pNetwork );
	for ( int i = 0; i < res.size(); ++i )
	{
		NAI::SPosition pos; 
		pos.p = res[i];
		pos.SetNetwork( pNetwork );
		CVec3 ptPos = pos.GetCP();
		pModel->CreateSphere( ptPos, 0.06f );
		CVec4 color = CVec4( 1, 0.4f, 0, 1 ); 
		tracePoints.push_back( pScene->CreateMesh( pModel, color, 0 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIViewer::ImportAIMap( NAI::IAIMap *pMap )
{
	pAIMap = pMap;
	ShowObjects();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICAIView
////////////////////////////////////////////////////////////////////////////////////////////////////
CICAIView::CICAIView( NAI::IAIMap *_pMap, NAI::IPathNetwork *_pNetwork, NRPG::IVisionTracker *_pVision, 
	const ICamera::SCameraPos &_cameraPos )
	: pMap(_pMap), pNet(_pNetwork), pVision(_pVision), cameraPos(_cameraPos)
{
	ASSERT( _pNetwork->GetNumLayers() != 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICAIView::Exec()
{
	CAIViewer *pView = new CAIViewer( pNet, pVision );
	pView->ImportAIMap( pMap );
	pView->SetCamera( cameraPos );
	PushInterface( pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0x02911174, CAIViewer );
////////////////////////////////////////////////////////////////////////////////////////////////////
