#include "StdAfx.h"
#include "..\Main\wInterface.h"
#include "MEDragDrop.h"
#include "iWysiwyg.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\MakeBuilding.h"
#include "..\Main\gView.h"
#include "MEUserSettings.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\Main\Transform.h"
#include "..\Main\iMain.h"
#include "WysiwygSelection.h"
#include "..\MapEdit\FinDBCmd.h"
#include "..\MapEdit\dbDefs.h"
#include "WysiwygClipboard.h"
#include "..\Main\Grid.h"
#include "..\Main\MemObject.h"
#include "..\MapEdit\RectsDBCmd.h"
#include "..\MapEdit\UnitDB.h"
#include "..\MapEdit\WaypointDB.h"
#include "..\Main\DiscretePos.h"
#include "WysiwygUndo.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\aiObjectLoader.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int CLUE_SLOT_ID = 188;
const int EXPLOSION_ID = 189;
extern bool GetDropInfo( COleDataObject* pDataObject, int *pnTree, int *pnItem );
namespace NWysiwyg
{
	int AddTerrSpotDB( int nVarID, const NBuilding::SProjectedSpot &spot );
}
namespace NGfx
{
	HWND GetHWND();
}
namespace NGScene
{
	extern CResourceTracker objChecker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	extern CBasicShare<int, NAI::CLoadGeometryInfo> shareAIModel;
}
static SRand rnd;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDragDrop: public IDragDrop
{
	OBJECT_NOCOPY_METHODS(CDragDrop)
	ZDATA
	CPtr<NGScene::IGameView> pScene;
	CPtr<ICamera> pCamera;
	CPtr<NWysiwyg::ISelection> pSelection;
	int nWorldID;
	CObj<NBuilding::CBuildingInfoHold> pBInfo;
	ZEND

	class COleDropTargetInternal: public COleDropTarget
	{
		CPtr<CDragDrop> pParent;
	public:
		COleDropTargetInternal( CDragDrop *_pParent = 0 ): pParent(_pParent) { if ( pParent ) Register( CWnd::FromHandle( NGfx::GetHWND() ) ); }
		~COleDropTargetInternal() { Revoke(); }

		virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point);
		virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
		virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
		virtual void OnDragLeave(CWnd* pWnd);
	} dropTarget;
	friend class COleDropTargetInternal;

	bool bUpdated;
	int  nType;
	int nObjectID;
	CVec2 ptPoint;
	CObj<NDb::CObject> pObject;
	CObj<NDb::CModel> pModel, pModel2;
	CObj<NDb::CConstructionPart> pCPart;
	CObj<CMemObject> pSphere;
	list<CObj<CObjectBase> > objects;

	CVec3 GetPos( const CVec2 &ptPoint );
	DROPEFFECT OnDragMove( COleDataObject* pDataObject, CPoint point );
	void InsertObject( int nPlacableID );
	void InsertSubTemplate( NDb::CTemplate *pTemplate );
	void InsertPers( NDb::CRPGPers *pPers );
	void InsertSpot( NDb::CSpot *pSpot );
	void InsertConstructionPart( int nCPartID );
	void InsertWaypoint( int nWaypointNameID );

public:
	CDragDrop(){}
	CDragDrop( int nWorldID, NGScene::IGameView *pScene, ICamera *pCamera, NWysiwyg::ISelection *pSelection, NBuilding::CBuildingGrid *pGrid, NBuilding::CSolidAndWallMap *pSWMap );

	virtual void Update();
	float GetObjectInitialDZ( int nPlacableID, const CVec2 &ptPos, int nFloor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDragDrop::CDragDrop( int _nWorldID, NGScene::IGameView *_pScene, ICamera *_pCamera, NWysiwyg::ISelection *_pSelection, NBuilding::CBuildingGrid *pGrid, NBuilding::CSolidAndWallMap *pSWMap )
	: pScene(_pScene), pCamera(_pCamera), nWorldID(_nWorldID), pSelection(_pSelection), dropTarget(this)
{
	bUpdated = false;
	pSphere = new ::CMemObject;
	pSphere->CreateSphere( VNULL3, 0.5f * FP_GRID_STEP, 4 );
	pBInfo = new NBuilding::CBuildingInfoHold( pGrid, pSWMap, nWorldID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CDragDrop::GetPos( const CVec2 &ptPoint )
{
	float fFloor = GetUserSettings().GetActiveFloor();
	CVec2 ptTile = pSelection->GetTileUnderPos( pSelection->GetProjectiveRay( ptPoint ), fFloor );
	return CVec3( ptTile, fFloor * NBuilding::WALL_HEIGHT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline CVec2 GetSpotSizeTiles( NDb::CSpot *pSpot )
{
	CVec2 ptSize( 2, 2 );
	CPtr<NDb::CMaterial> pM;
	if ( IsValid( pSpot->pMaterial ) )
	{
		pM = pSpot->pMaterial->GetMaterial( &rnd );
		if ( IsValid( pM ) && IsValid( pM->pTexture ) )
		{
			ptSize = CVec2( pM->pTexture->nWidth, pM->pTexture->nHeight );
			ptSize *= 8.0f / 256.0f;
		}
	}
	return ptSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CModel* CreateModel( NDb::CGeometry *pGeometry, const CPtr<NDb::CMaterial> pMaterials[NDb::N_MODEL_MATERIALS] )
{
	static NDb::CMaterial *pDefM = NDb::GetMaterial( NDb::N_DEF_MATERIAL_ID );
	NDb::CModel *pModel = new NDb::CModel();
	const int nID = pGeometry->GetRecordID();

	pModel->pGeometry = pGeometry;
	for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
	{
		NGScene::SPartKey k( nID, i );
		if ( !NGScene::objChecker.DoesExist( k ) )
			continue;
		pModel->pMaterials[i] = IsValid( pMaterials[i] ) ? pMaterials[i] : pDefM;
	}

	return pModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::Update()
{
	if ( !bUpdated )
		return;
	bUpdated = false;
	objects.clear();
	//
	CVec3 ptPos = GetPos(ptPoint );
	SFBTransform pos = pSelection->GetTerrainTransform( ptPos.x, ptPos.y ) * MakeTransform( ptPos );
	int nFloor = GetUserSettings().GetActiveFloor();
	switch ( nType )
	{
		case IDC_OBJECTS_TREE:
		{
			NDb::CTRndObject *pTObj = NDb::GetTRndObject( nObjectID );
			if ( IsValid( pTObj ) )
				pObject = pTObj->CreateObject( &rnd );
			if ( IsValid( pObject ) && IsValid( pTObj->pPlacable ) )
			{
				float fDZ = GetObjectInitialDZ( pTObj->pPlacable->GetRecordID(), FP_INV_GRID_STEP * CVec2( ptPos.x, ptPos.y ), nFloor );
				if ( fDZ > 0 )
				{
					pos = MakeTransform( CVec3( 0, 0, fDZ ) ) * pos;
				}
				if ( IsValid( pObject->pModels[0] ) && IsValid( pObject->pModels[0]->pModel ) )
					objects.push_back( pScene->CreateMesh( pObject->pModels[0]->pModel, pos ) );
				else
					objects.push_back( pScene->CreateMesh( pSphere, CVec4( 0.7f, 0.7f, 0.9f, 1.0f ), pos ) );
			}
			break;
		}
		case IDC_RPG_ITEMS_TREE:
		{
			NDb::CRPGItem *pRPG = NDb::GetRPGItem( nObjectID );
			if ( IsValid( pRPG ) )
			{
				if ( IsValid( pRPG->pModel ) )
					pModel = pRPG->pModel->CreateModel( &rnd );
				if ( IsValid( pModel ) )
					objects.push_back( pScene->CreateMesh( pModel, pos ) );
				else
				{
					CVec4 cr = pRPG->GetRecordID() == CLUE_SLOT_ID ? CVec4( 1.0f, 0.5f, 0.6f, 1.0f ) : CVec4( 0.7f, 0.7f, 0.9f, 1.0f );
					if ( pRPG->GetRecordID() == EXPLOSION_ID )
						cr = CVec4( 0.77f, 0.55f, 0.1f, 1.0f );
					objects.push_back( pScene->CreateMesh( pSphere, cr, pos ) );
				}
			}
			break;
		}
		case IDC_TEMPLATE_TREE:
		{
			NDb::CTemplate *pT = NDb::GetTemplate( nObjectID );
			if ( !IsValid( pT ) )
				break;
			CObj<CMemObject> pBox = new ::CMemObject;
			CVec3 ptBox = CVec3( FP_GRID_STEP * pT->nWidth, FP_GRID_STEP * pT->nHeight, 1.1f * NBuilding::WALL_HEIGHT );
			pBox->CreateCube( VNULL3, ptBox, true );
			objects.push_back( pScene->CreateMesh( pBox, CVec4( 0.68f, 0.87f, 1.0f, 0.2f ), pos ) );
			break;
		}
		case IDC_RPG_PERS_TREE:
		{
			NDb::CRPGPers *pPers = NDb::GetPers( nObjectID );
			if ( IsValid( pPers ) && IsValid( pPers->pModel ) )
				pModel = pPers->pModel->CreateModel( &rnd );
			if ( IsValid( pModel ) )
				objects.push_back( pScene->CreateMesh( pModel, pos * MakeTransform( VNULL3, 90 ) ) );
			break;
		}
		case IDC_SPOTS_TREE:
		{
			NDb::CSpot *pSpot = NDb::GetSpot( nObjectID );
			if ( !IsValid( pSpot ) )
				break;
			CVec2 ptSize = GetSpotSizeTiles( pSpot );
			pModel = new NDb::CModel();
			pModel->pGeometry = NDb::GetGeometry( NDb::N_CUBE_GEOMETRY_ID );
			if (  !IsValid( pModel->pGeometry ) )
				return;
			CPtr<NDb::CMaterial> pM = IsValid( pSpot->pMaterial ) ? pSpot->pMaterial->GetMaterial( &rnd ) : 0;
			if ( IsValid( pM ) )
				for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
					pModel->pMaterials[i]  = pM;
			objects.push_back( pScene->CreateMesh( pModel, pos * MakeTransform( VNULL3, CVec3( ptSize * FP_GRID_STEP, 1 ) ) ) );
			break;
		}
		case IDC_CONSTRUCTIONPARTS_TREE:
		{
			pos = pos * MakeTransform( VNULL3, RotationIDToAngle( GetUserSettings().GetActiveRotationID() ) );
			NDb::CTConstructionPart *pTCPart = NDb::GetTConstructionPart( nObjectID );
			if ( IsValid( pTCPart ) )
				pCPart = pTCPart->CreateConstructionPart( &rnd );
			if ( !IsValid( pCPart ) )
				break;
			if ( IsValid( pCPart->pGeometry ) )
			{
				pModel = CreateModel( pCPart->pGeometry, pCPart->pDefMaterials );
				objects.push_back( pScene->CreateMesh( pModel, pos ) );
			}
			else
			{
				CObj<CMemObject> pBox = new ::CMemObject;
				float fY = pCPart->nSizeY == 0 ? pCPart->fThickness : 2 * pCPart->nSizeY * FP_GRID_STEP;
				CVec3 ptBox = CVec3( 2 * FP_GRID_STEP * pCPart->nSizeX, fY, pCPart->nSizeZ * NBuilding::WALL_HEIGHT );
				pBox->CreateCube( VNULL3, ptBox, true );
				objects.push_back( pScene->CreateMesh( pBox, CVec4( 0.68f, 0.87f, 1.0f, 0.2f ), pos ) );
			}
			if ( IsValid( pCPart->p2ndGeometry ) )
			{
				pModel2 = CreateModel( pCPart->pGeometry, pCPart->pDefMaterials + NDb::N_MODEL_MATERIALS );
				objects.push_back( pScene->CreateMesh( pModel2, pos ) );
			}
			if ( IsValid( pCPart->pObject ) && IsValid( pCPart->pObject->pModels[0] ) && IsValid( pCPart->pObject->pModels[0]->pModel ) )
					objects.push_back( pScene->CreateMesh( pCPart->pObject->pModels[0]->pModel, pos ) );
			break;
		}
		case  IDC_WAYPOINTNAMES_TREE:
		{
			CObj<CMemObject> pBox = new ::CMemObject;
			pBox->CreateFlag( VNULL3, 1.5f, 0.04f, 0.6f );
			objects.push_back( pScene->CreateMesh( pBox, CVec4( 0.9f, 0.2f, 0.2f, 1.0f ), pos ) );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CDragDrop::OnDragMove( COleDataObject* pDataObject, CPoint point )
{
	int nTree;
	int nID;

	nType = -1;
	bUpdated = true;
	if ( !pDataObject || ! GetDropInfo( pDataObject, &nTree, &nID ) 
		|| (nTree != IDC_TEMPLATE_TREE && nTree != IDC_OBJECTS_TREE && nTree != IDC_RPG_ITEMS_TREE && nTree != IDC_RPG_PERS_TREE 
		&&  nTree != IDC_SPOTS_TREE && nTree != IDC_CONSTRUCTIONPARTS_TREE && nTree != IDC_WAYPOINTNAMES_TREE ) )
	{
		NMainLoop::StepApp( true, true );
		return DROPEFFECT_NONE;
	}
	nType = nTree;
	nObjectID = nID;
	ptPoint = CVec2( point.x, point.y );
	NMainLoop::StepApp( true, true );
	return DROPEFFECT_MOVE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CDragDrop::COleDropTargetInternal::OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point)
{
	return pParent->OnDragMove( pDataObject, point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CDragDrop::COleDropTargetInternal::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	return pParent->OnDragMove( pDataObject, point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CDragDrop::COleDropTargetInternal::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	int type;
	int nID;

	if ( !GetDropInfo( pDataObject, &type, &nID ) )
		return false;
	switch ( type )
	{
		case IDC_OBJECTS_TREE:
		{
			NDb::CTRndObject *pO = NDb::GetTRndObject( pParent->nObjectID );
			if ( IsValid( pO ) && IsValid( pO->pPlacable ) )
				pParent->InsertObject( pO->pPlacable->GetRecordID() );
			break;
		}
		case IDC_RPG_ITEMS_TREE:
		{
			NDb::CRPGItem *pI = NDb::GetRPGItem( pParent->nObjectID );
			if ( IsValid( pI ) && IsValid( pI->pPlaceObj ) )
				pParent->InsertObject( pI->pPlaceObj->GetRecordID() );
			break;
		}
		case IDC_TEMPLATE_TREE:
		{
			NDb::CTemplate *pTemplate = NDb::GetTemplate( pParent->nObjectID );
			if ( IsValid( pTemplate ) )
				pParent->InsertSubTemplate( pTemplate );
			break;
		}
		case IDC_RPG_PERS_TREE:
		{
			NDb::CRPGPers *pPers = NDb::GetPers( pParent->nObjectID );
			if ( IsValid( pPers ) )
				pParent->InsertPers( pPers );
			break;
		}
		case IDC_SPOTS_TREE:
		{
			NDb::CSpot *pSpot = NDb::GetSpot( pParent->nObjectID );
			if ( IsValid( pSpot ) )
				pParent->InsertSpot( pSpot );
			break;
		}
		case IDC_CONSTRUCTIONPARTS_TREE:
		{
			pParent->InsertConstructionPart( pParent->nObjectID );
			break;
		}
		case IDC_WAYPOINTNAMES_TREE:
		{
			pParent->InsertWaypoint( pParent->nObjectID );
			break;
		}
	}
	pParent->OnDragMove( 0, point );
	NMainLoop::StepApp( true, true );
	::SetFocus( NGfx::GetHWND() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::COleDropTargetInternal::OnDragLeave(CWnd* pWnd)
{
	pParent->OnDragMove( 0, CPoint() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertObject( int nPlacableID )
{
	static CFinPosDB db;

	int nID = db.Insert( nWorldID, nPlacableID );
	if ( nID > 0 )
	{
		CVec3 ptPos = FP_INV_GRID_STEP * GetPos( ptPoint );
		int nFloor = GetUserSettings().GetActiveFloor();
		float fDZ = GetObjectInitialDZ( nPlacableID, CVec2( ptPos.x, ptPos.y ), nFloor );
		db.SetPos( nID, CVec2( ptPos.x, ptPos.y ), fDZ, nFloor, 0 );
		db.Close();
		//Refresh<NDb::CFinalElement>( nID );
		//NDb::CFinalElement *p = NDb::GetFinalElement( nID );
		NDb::CPlacableObject *pObject = NDb::GetPlacableObject( nPlacableID );
		if ( IsValid( pObject ) && IsValid( pObject->pObject ) )
		{
			CPtr<NDb::CObject> pObj = pObject->pObject->CreateObject( &rnd );
			if ( IsValid( pObj->pModels[0] ) && fabs( pObj->pModels[0]->ptPLightCr ) > 0 )
			{
				NDb::CContainerModel *pM = pObj->pModels[0];
				int nFlare = IsValid( pM->pPFlareTexture ) ? pM->pPFlareTexture->GetRecordID() : 0;
				const CVec3 ptColor = 255 * pM->ptPLightCr;
				int nColor = RGB( ptColor.x, ptColor.y, ptColor.z );
				db.SetLight( nID, nColor, pM->ptPLightPos, pM->fPLightRadius, pM->fPFlareRadius, nFlare, pM->ptPLightFlarePos );
			}
		}
		NDb::CFinalElement *p = NDb::GetFinalElement( nID );
		if ( p )
			NMapEditor::PushUndoCmd( CreateObjSelectionUndo( CWysiwygUndo::UA_INSERT, p, 0 ) );
		else
		{
			ASSERT(0);
		}
		SForceSelection sel;
		sel.nWorldID = nWorldID;
		sel.objectIDs.push_back( nID );
		pSelection->OnPaste( sel, ptPoint, false );
		pSelection->ObjectUpdated( nID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertSubTemplate( NDb::CTemplate *pTemplate )
{
	static CRectPosDB db;

	if ( !IsValid( pTemplate ) )
		return;
	int nID = db.Insert( nWorldID, pTemplate->GetRecordID() );
	if ( nID <= 0 )
		return;
	CVec3 ptPos = GetPos( ptPoint );
	db.SetPos( nID, CVec2( ptPos.x, ptPos.y ), 0, GetUserSettings().GetActiveFloor(), 0 );
	//
	//Refresh<NDb::CRectangle>( nID );
	NDb::CRectangle *pRect = NDb::GetRectangle( nID );
	if ( !pRect )
	{
		ASSERT(0);
	}
	NMapEditor::PushUndoCmd( CreateSubTemplateUndo( CWysiwygUndo::UA_INSERT, NDb::GetRectangle( nID ), 0 ) );
	SForceSelection sel;
	sel.nWorldID = nWorldID;
	sel.subtemplateIDs.push_back( nID );
	pSelection->OnPaste( sel, ptPoint, false );
	pSelection->SubTemplateUpdated( NWysiwyg::MakeUserID( BT_SUBTEMPLATE, nID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertPers( NDb::CRPGPers *pPers )
{
	static CUnitPosDB db;

	int nID = db.Insert( nWorldID, pPers->GetRecordID() );
	if ( nID <= 0 )
		return;
	CVec3 ptPos = FP_INV_GRID_STEP * GetPos( ptPoint );
	db.SetPos( nID, ptPos.x, ptPos.y, GetUserSettings().GetActiveFloor(), 0 );
	//
	Refresh<NDb::CUnit>( nID );
	NDb::CUnit *p = NDb::GetUnit( nID );
	if ( IsValid( p ) )
		NMapEditor::PushUndoCmd( CreateUnitUndo( CWysiwygUndo::UA_INSERT, p, 0 ) );
	else
	{
		ASSERT(0);
	}
	SForceSelection sel;
	sel.nWorldID = nWorldID;
	sel.unitIDs.push_back( nID );
	pSelection->OnPaste( sel, ptPoint, false );
	pSelection->UnitUpdated( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertSpot( NDb::CSpot *pSpot )
{
	NBuilding::SProjectedSpot s;
	s.ptOrigin = GetPos( ptPoint );
	s.ptNormal = CVec3( 0, 0, 1 );
	s.ptSize = FP_GRID_STEP * GetSpotSizeTiles( pSpot );
	s.nRotation = 0;
	s.nMaterialID = pSpot->GetRecordID();
	int nID = NWysiwyg::AddTerrSpotDB( nWorldID, s );
	if ( nID < 0 )
		return;
	//
	Refresh<NDb::CRndTerrainSpot>( nID );
	NMapEditor::PushUndoCmd( CreateTerrSpotUndo( CWysiwygUndo::UA_INSERT, NDb::GetRndTerrainSpot( nID ), 0 ) );
	SForceSelection sel;
	sel.nWorldID = nWorldID;
	sel.terrSpotsIDs.push_back( nID );
	pSelection->OnPaste( sel, ptPoint, false );
	pSelection->TerrainSpotUpdated( vector<NBuilding::SProjectedSpot>( 1, s ) );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertConstructionPart( int nCPartID )
{
	vector<int> ids;
	pSelection->AddConstructionPart( &ids, nCPartID, GetPos( ptPoint ) );
	SForceSelection sel;
	sel.nWorldID = nWorldID;
	sel.fragsUserIDs.insert( sel.fragsUserIDs.end(), ids.begin(), ids.end() );
	pSelection->OnPaste( sel, ptPoint, false );
	pSelection->BuildingUpdated();	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDragDrop::InsertWaypoint( int nWaypointNameID )
{
	int nID = AddWaypoint2DB( nWaypointNameID, nWorldID );
	if ( nID < 0 )
		return;
	CVec3 pt = GetPos( ptPoint );
	SetWaypointPos( nID, CVec2( pt.x, pt.y ), GetUserSettings().GetActiveFloor() );
	Refresh<NDb::CWaypoint>( nID );
	SForceSelection sel;
	sel.nWorldID = nWorldID;
	sel.waypointIDs.push_back( nID );
	pSelection->OnPaste( sel, ptPoint, false );
	pSelection->WaypointUpdated( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CDragDrop::GetObjectInitialDZ( int nPlacableID, const CVec2 &ptPos, int nFloor )
{
	NDb::CPlacableObject *pObj = NDb::GetPlacableObject( nPlacableID );
	if ( !pObj )
		return 0;
	static SRand rand;
	NDb::CGeometry *pG = 0;
	if ( IsValid( pObj->pObject ) )
	{
		CObj<NDb::CObject> pO = pObj->pObject->CreateObject( &rand );
		if ( IsValid( pO ) && IsValid( pO->pModels[0] ) && IsValid( pO->pModels[0]->pModel ) )
			pG = pO->pModels[0]->pModel->pGeometry;
	}
	else if ( IsValid( pObj->pRPGItem ) && IsValid( pObj->pRPGItem->pModel ) )
	{
		CObj<NDb::CModel> pM = pObj->pRPGItem->pModel->CreateModel( &rand );
		if ( IsValid( pM ) )
			pG = pModel->pGeometry;
	}
	if ( !pG || !IsValid( pG->pAIGeometry ) )
		return 0;
	CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( pG->pAIGeometry->GetRecordID() );
	pSrc.Refresh();
	NAI::CGeometryInfo *pGI = pSrc->GetValue();
	if ( !pGI )
		return 0;
	if ( pGI->bound.s.ptCenter.z - pGI->bound.ptHalfBox.z > 0.05f )
		return 0;

	pBInfo->UpdateInfo();
	const NBuilding::SBuildingInfo& info = pBInfo->GetInfo();

	int x = ptPos.x / 2;
	int y = ptPos.y / 2;
	NBuilding::SPart part( nFloor, x, y );
	const NBuilding::SStoreyInfo &storey = const_cast<NBuilding::SBuildingInfo&>( info ).GetPart( part );

	if ( !storey.fragments.empty() )
		return 0.1f;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IDragDrop* CreateDragAndDrop( int nWorldID, NGScene::IGameView *pScene, ICamera *pCamera, NWysiwyg::ISelection *pSelection, NBuilding::CBuildingGrid *pGrid, NBuilding::CSolidAndWallMap *pSWMap )
{
	return new CDragDrop( nWorldID, pScene, pCamera, pSelection, pGrid, pSWMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
