#include "StdAfx.h"
#include "..\Main\Camera.h"
#include "IWysiwyg.h"
#include "WysiwygSelectionImpl.h"
#include "..\Main\GView.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\BuildingGrid.h"
#include "..\Main\BuildingClip.h"
#include "wEditor.h"
#include "..\Main\Grid.h"
#include "..\Main\Transform.h"
#include "WysiwygSpotSel.h"
#include "WysiwygTerrSpot.h"
#include "WysiwygWaypointSel.h"
#include "MEUserSettings.h"
#include "..\Main\MELayers.h"
#include "MESerialize.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "WysiwygFragmentSel.h"
#include "WysiwygObjectSel.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "WysiwygLadderSel.h"
#include "WysiwygSubTemplateSel.h"
#include "WysiwygUnitSel.h"
#include "weInterface.h"
#include "..\Main\aiMap.h"
#include "WysiwygUndo.h"
#include "MEParams.h"
#include "WysiwygTerrain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
double START_MOVE_TIME = 0.2;
const CVec3 crInsert( 1.0f, 1.0f, 0.7f );
extern NDb::CModel* CreateModel( NDb::CGeometry*, const CPtr<NDb::CMaterial> pMaterials[NDb::N_MODEL_MATERIALS] );
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
ISelection* CreateSelection( int nWorldID, NWorld::IEditorWorld *pWorld, NBuilding::CBuildingGrid *pGrid, NGScene::IGameView *pScene, ICamera *pCamera )
{
	return new CSelection( nWorldID, pWorld, pGrid, pScene, pCamera );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool TileAlign()
{
	return 0x1 & GetKeyState( VK_CAPITAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsDiscreteRotation()
{
	return IsPressed( VK_CONTROL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsRotation()
{
	return IsPressed( VK_SHIFT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMaxMovementDiscrete( vector< CPtr<CSelectedObj> > &selection )
{
	int nMax = 0;
	for ( int i = 0; i < selection.size(); ++i )
		nMax = Max( nMax, selection[i]->GetMovementDiscrete() );
	return nMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMaxRotationDiscrete( const vector< CPtr<CSelectedObj> > &selection )
{
	int nMax = 0;
	for ( int i = 0; i < selection.size(); ++i )
		nMax = Max( nMax, selection[i]->GetRotationDiscrete() );
	return nMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMaxZDiscrete( vector< CPtr<CSelectedObj> > &selection )
{
	int nMax = 0;
	for ( int i = 0; i < selection.size(); ++i )
		nMax = Max( nMax, selection[i]->GetZDiscrete() );
	return nMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateBuildInfo( NWorld::IBuilding *pObj )
{
	NDb::CTemplVariant *pVar = pObj->GetInfo().pVariant;
	if ( !pVar )
		return false;
	bool bRet = UpdateBuildInfo( pVar->GetRecordID() );
	pObj->GetInfo().pGrid->Updated();
	pObj->Update();
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateBuildInfo( int nBuildingID )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pBInfo = pLoader->GetValue();
	if ( !pBInfo )
		return false;
	if ( !SerializeBuilding( pBInfo, nBuildingID ) )
		return false;
	//
	pLoader->Updated();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawBox( NGScene::IGameView *pScene, const SBound &bbox, const CVec3 &color, list<CObj<NGScene::CPolyline> > *pL, const SFBTransform &pos )
{
	const CVec3 ptOrig( bbox.s.ptCenter - bbox.ptHalfBox );
	const CVec3 ptSize( 2 * bbox.ptHalfBox );
	vector<CVec3> points( 10, ptOrig );

	points[1].x += ptSize.x;
	points[2]   += CVec3( ptSize.x, ptSize.y, 0 );
	points[3].y += ptSize.y;
	points[5].z += ptSize.z;
	points[6]   += CVec3( ptSize.x, 0, ptSize.z );
	points[7]   += ptSize;
	points[8]   += CVec3( 0, ptSize.y, ptSize.z );
	points[9].z += ptSize.z;
	for ( int i = 0; i < points.size(); ++i )
		pos.forward.RotateHVector( &points[i], points[i] );
	pL->push_back( pScene->CreatePolyline( points, color ) );

	points.clear();
	points.resize( 2, ptOrig );
	points[0].x += ptSize.x;
	points[1]   += CVec3( ptSize.x, 0, ptSize.z );
	for ( int i = 0; i < points.size(); ++i )
		pos.forward.RotateHVector( &points[i], points[i] );
	pL->push_back( pScene->CreatePolyline( points, color ) );

	points[0] = ptOrig + CVec3( ptSize.x, ptSize.y, 0 );
	points[1] = ptOrig + CVec3( ptSize.x, ptSize.y, ptSize.z );
	for ( int i = 0; i < points.size(); ++i )
		pos.forward.RotateHVector( &points[i], points[i] );
	pL->push_back( pScene->CreatePolyline( points, color ) );

	points[0] = ptOrig + CVec3( 0, ptSize.y, 0 );
	points[1] = ptOrig + CVec3( 0, ptSize.y, ptSize.z );
	for ( int i = 0; i < points.size(); ++i )
		pos.forward.RotateHVector( &points[i], points[i] );
	pL->push_back( pScene->CreatePolyline( points, color ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//		CSelection
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelection::CSelection( int _nWorldID, NWorld::IEditorWorld *_pWorld, 
	NBuilding::CBuildingGrid *_pGrid, NGScene::IGameView *_pScene, ICamera *_pCamera )
	: pGrid(_pGrid), pScene(_pScene), pCamera(_pCamera), bLBDown(false) 
{ 
	pWorld = dynamic_cast<NWorld::CEditorWorld*>( _pWorld );
	ASSERT( pWorld );
	ASSERT( pScene );
	ASSERT( pCamera );
	nWorldID = _nWorldID;
	pBuilding = NGScene::shareBuildings.Get( nWorldID );
	pVar = NDb::GetTemplVariant( nWorldID );
	if ( !IsValid( pVar ) || !IsValid( pVar->pTemplate ) )
	{
		ASSERT(0);
		return;
	}
	ptWorldSize = CVec2( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight );
	rectSelection = CTRect<float>(0,0,0,0);
	ptCurrentKbdMove = VNULL3;
	fCurrentKbdRotation = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::IEditorWorld* CSelection::GetWorld() 
{ 
	return pWorld; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SFBTransform CSelection::GetTerrainTransform( float x, float y ) 
{ 
	return pWorld->GetTerrainTransform( x, y ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Clear()
{
	bool bEmpty = IsEmpty();
	if ( bLBDown )
	{
		for ( int i = 0; i < selection.size(); ++i )
			if ( IsValid( selection[i]) )
				selection[i]->EndMove( true );
	}
	if ( GetUserSettings().GetParam( ME_LOCK_SELECTION ) )
		return;
	selection.clear();
	bLBDown = false;
	if ( !bEmpty )
	{
		SMessage msg;
		msg.msg = MSG_SELECT;
		msg.data = 0;
		GetUserSettingsSetup().SendMessage( msg );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::CheckSelection( CObjectBase *pObj, int nUserID )
{
	for ( int i = 0; i < selection.size(); ++i )
		if ( selection[i]->IsEqual( pObj, nUserID ) )
		{
			CDynamicCast<CSpot> pS(selection[i]);
			if (pS)
				pS->SetActiveHandle( nUserID );
			return true;
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::SetSelection( CObjectBase *pObj, int nUserID )
{
	Clear();
	AddSelection( pObj, nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::SendSelectionInfo()
{
	static vector<SSelectedInfo> info;

	info.clear();
	for ( int i = 0; i < selection.size(); ++i )
	{
		SSelectedInfo si;
		if ( selection[i]->GetInfo( &si ) )
			info.push_back( si );
	}
	SMessage msg;
	msg.msg = MSG_SELECT;
	if ( !info.empty() )
		msg.data = (DWORD)&info;
	else
		msg.data = 0;
	//
	if ( !IsEmpty() )
	{
		SBound b;
		GetSelectionBound( &b );
		GetUserSettingsSetup().SetSelectionCenter( b.s.ptCenter );
	}
	//
	GetUserSettingsSetup().SendMessage( msg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::AddSelection( CObjectBase *pObj, int nUserID )
{
//	if ( GetUserSettings().GetParam( ME_LOCK_SELECTION ) )
//		return;
	//
	for ( vector< CPtr<CSelectedObj> >::iterator i = selection.begin(); i != selection.end(); ++i )
	{
		if ( (*i)->IsEqual( pObj, nUserID ) )
		{
			selection.erase( i );
			SendSelectionInfo();
			return;
		}
	}
	//
	CPtr<CSelectedObj> pSel;
	CDynamicCast<NWorld::CEditorWorld> pW( pWorld );
	if ( !IsValid( pW ) )
		return;
	EBrushType type;
	int n;
	GetFragmentID( nUserID, &type, &n );
	//
	NHPTimer::GetTime( &tSelectTime );
	//
	CDynamicCast<NWorld::IBuilding> pWB(pObj);
	if (pWB)
	{
		pSel = new CFragmentSel( this, pW->GetMainBuildingInterface(), nUserID, pW );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	else {
		CDynamicCast<NWorld::IEditorObject> pEO(pObj);
		if (pEO)
		{
			NDb::CFinalElement* pF = pEO->GetDBElement();
			NDb::CModel* pO = pEO->GetModel();
			pSel = new CObjectSel(pF, pO, this, CTPoint<int>(pVar->pTemplate->nWidth, pVar->pTemplate->nHeight));
			if (pSel->IsInitialized())
				selection.push_back(pSel);
		}
		else if (BT_LADDER == type)
		{
			pBuilding.Refresh();
			pSel = new CLadderSel(this, CTPoint<int>(pVar->pTemplate->nWidth, pVar->pTemplate->nHeight), nUserID, pBuilding->GetValue());
			if (pSel->IsInitialized())
				selection.push_back(pSel);
		}
		else if (BT_SUBTEMPLATE == type)
		{
			pSel = new CSubTemplateSel(this, CTPoint<int>(pVar->pTemplate->nWidth, pVar->pTemplate->nHeight), nUserID);
			if (pSel->IsInitialized())
				selection.push_back(pSel);
		}
		else {
			CDynamicCast<NWorld::IEditorUnit> pU(pObj);
			if (pU)
			{
				pSel = new CUnitSel(pU->GetDBUnit(), this, CTPoint<int>(pVar->pTemplate->nWidth, pVar->pTemplate->nHeight));
				if (pSel->IsInitialized())
					selection.push_back(pSel);
			}
			else {
				CDynamicCast<NWorld::IVisObj> pWMO(pObj);
				if (pWMO)
				{
					ESpotType type;
					int nFragmentID, nIndex;
					CSpotSel::EHandle h;
					EBrushType nBrush;
					GetFragmentID(nUserID, &nBrush, &nFragmentID);
					GetSpotID(nFragmentID, &type, &nIndex, &h);
					switch (nBrush)
					{
					case BT_GEOMETRY:
					{
						pSel = new CFragmentSel(this, pW->GetMainBuildingInterface(), nFragmentID, pW);
						if (pSel->IsInitialized())
							selection.push_back(pSel);
						break;
					}
					case BT_TEXSPOT:
						switch (type)
						{
						case ST_WALL:
						{
							pSel = new CSpotSel(this, pBuilding, pGrid, nUserID);
							if (pSel->IsInitialized())
								selection.push_back(pSel);
							break;
						}
						case ST_TERRAIN:
						{
							if (!IsValid(pVar) || nIndex < 0 || nIndex >= pVar->terrainSpots.size())
								break;
							CPtr<NDb::CRndTerrainSpot> pTSpot = pVar->terrainSpots[nIndex];
							if (!IsValid(pTSpot))
								break;
							pSel = new CTerrSpot(pTSpot, this, nUserID);
							if (pSel->IsInitialized())
								selection.push_back(pSel);
							break;
						}
						}
						break;
					case BT_WAYPOINT:
						pSel = new CWaypoint(nFragmentID, this, pVar);
						if (pSel->IsInitialized())
							selection.push_back(pSel);
						break;
					case BT_OBJECT:
					{
						NDb::CFinalElement* pF = NDb::GetFinalElement(n);
						CPtr<NDb::CModel> pModel;
						static SRand rnd;

						if (IsValid(pF) && IsValid(pF->pObject) && IsValid(pF->pObject->pObject))
						{
							CPtr<NDb::CObject> pO = pF->pObject->pObject->CreateObject(&rnd);
							if (IsValid(pO->pModels[0]))
								pModel = pO->pModels[0]->pModel;
						}
						else if (IsValid(pF) && IsValid(pF->pObject) && IsValid(pF->pObject->pRPGItem) && IsValid(pF->pObject->pRPGItem->pModel))
							pModel = pF->pObject->pRPGItem->pModel->CreateModel(&rnd);
						//
						pSel = new CObjectSel(pF, pModel, this, CTPoint<int>(pVar->pTemplate->nWidth, pVar->pTemplate->nHeight));
						if (pSel->IsInitialized())
							selection.push_back(pSel);
						break;
					}
					}
				}
			}
		}
	}
	//
	SendSelectionInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::Update( const CVec2 &ptCursor, CObjectBase *pObj, int nUserID )
{
	bool ret = false;
	if ( !IsPressed( VK_LBUTTON ) )
		OnLButtonUp( ptCursor );
	if ( bLBDown )
	{
		if ( GetAction() != eAction )
		{
			EAction eOldAction = eAction;
			OnLButtonUp( ptCursor );
			if ( eOldAction == A_MOVE || eOldAction == A_MOVE_Z )
			{
				OnLButtonDown( ptCursor );
				tSelectTime -= START_MOVE_TIME * NHPTimer::GetClockRate();
			}
		}
		else
		{
			// после того, как объекты поселекчены их разрешается двигать только после некоторого времени
			NHPTimer::STime tCurrent = tSelectTime;
			double dTime = NHPTimer::GetTimePassed( &tCurrent );
			if ( dTime >= START_MOVE_TIME )
			{
				if ( eAction == A_ROTATE )
					ret = Rotate( ptCursor );
				else
					ret = Move( ptCursor );
			}
			//ptLastPos = ptCursor;
		}
	}
	if ( IsEmpty() && IsValid( pVar ) )
	{
		const IUserSettings &settings = GetUserSettings();
		int nLayerID = settings.GetActiveLayerID();
		if ( !IsBuildingLayer( NBuilding::GetLayerType( nLayerID ) ) )
			return ret;
		const int nConstructionPartID = settings.GetSelectedBrushID( nLayerID );
		if ( EM_SELECT == settings.GetMode() )
		{
			SBound bbox;
			if ( nConstructionPartID > 0 && ComputeBoundBox( &bbox, nConstructionPartID, settings.GetActiveRotationID(), false ) )
			{
				CVec2 pt = GetTileUnderPos( GetProjectiveRay( ptCursor ), settings.GetActiveFloor() );
				pt = CheckTilePos( bbox, pt, ptWorldSize );
				bbox.s.ptCenter += CVec3( pt.x, pt.y, settings.GetActiveFloor() * NBuilding::WALL_HEIGHT );
				DrawBox( pScene, bbox, crInsert, &pLines, pWorld->GetTerrainTransform( bbox.s.ptCenter.x, bbox.s.ptCenter.y ) );
			}
		}
	}
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Draw()
{
	pLines.clear();
	parts.clear();

	if ( eAction != A_ROTATE )
	{
		for ( int i = 0; i < selection.size(); ++i )
			if ( !selection[i]->Draw( pScene, true ) )
			{
				SBound box = selection[i]->GetMovingBoundBox();
				box.Extend( 0.01f );
				SFBTransform posTerrain = pWorld->GetTerrainTransform( box.s.ptCenter.x, box.s.ptCenter.y );
				DrawBox( pScene, box, crLines, &pLines, posTerrain );
				SBound primary;
				if ( selection[i]->GetMovingPrimaryBoundBox( &primary ) )
					DrawBox( pScene, primary, 0.7f * crLines, &pLines, posTerrain );
				CMemObject *pBox, *pPrimaryBox;
				selection[i]->GetGBoundBox( &pBox, &pPrimaryBox );
				if ( pPrimaryBox )
					parts.push_back( pScene->CreateMesh( pPrimaryBox, CVec4( 0.5f, 0.5f, 0.7f, 0.2f ), posTerrain ) );
				if ( pBox )
					parts.push_back( pScene->CreateMesh( pBox, CVec4( 0.5f, 0.5f, 0.3f, 0.2f ), posTerrain ) );
			}
	}
	else
	{
		for ( int i = 0; i < selection.size(); ++i )
			selection[i]->Draw( pScene, false );
		//
		SBound box;
		if ( !GetSelectionBound( &box ) )
			return;
		float fMax = Max( box.ptHalfBox.x, box.ptHalfBox.y );
		box.ptHalfBox = CVec3( fMax, fMax, box.ptHalfBox.z );
		SFBTransform posTerrain = pWorld->GetTerrainTransform( box.s.ptCenter.x, box.s.ptCenter.y );
		DrawBox( pScene, box, crLines, &pLines, posTerrain );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::DrawRectangularSelection( const CVec3 &cr )
{
	rectSelLines.clear();

	CVec3 ptTerr( VNULL3 );
	SFBTransform tr = GetTerrainTransform( rectSelection.minx, rectSelection.miny );
	tr.forward.RotateHVector( &ptTerr, ptTerr );
	float fZ = GetUserSettings().GetActiveFloor() * NBuilding::WALL_HEIGHT + ptTerr.z + 0.1f;
	const float fStep = 0.2f;
	vector<CVec3> points(2);
	for ( float x = rectSelection.minx; x < rectSelection.maxx; x += 2 * fStep )
	{
		float fmax = Min( rectSelection.maxx, x + fStep );
		points[0] = CVec3( x, rectSelection.miny, fZ );
		points[1] = CVec3( fmax, rectSelection.miny, fZ );
		rectSelLines.push_back( pScene->CreatePolyline( points, cr ) );
		points[0] = CVec3( x, rectSelection.maxy, fZ );
		points[1] = CVec3( fmax, rectSelection.maxy, fZ );
		rectSelLines.push_back( pScene->CreatePolyline( points, cr ) );
	}
	for ( float y = rectSelection.miny; y < rectSelection.maxy; y += 2 * fStep )
	{
		float fmax = Min( rectSelection.maxy, y + fStep );
		points[0] = CVec3( rectSelection.minx, y, fZ );
		points[1] = CVec3( rectSelection.minx, fmax, fZ );
		rectSelLines.push_back( pScene->CreatePolyline( points, cr ) );
		points[0] = CVec3( rectSelection.maxx, y, fZ );
		points[1] = CVec3( rectSelection.maxx, fmax, fZ );
		rectSelLines.push_back( pScene->CreatePolyline( points, cr ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetHeight( const CRay &r, const CVec2 &ptO )
{
	CVec2 a( r.ptDir.x, r.ptDir.y );
	CVec2 b( ptO.x - r.ptOrigin.x, ptO.y - r.ptOrigin.y );

	float t = a * b / (a * a);
	return r.ptOrigin.z + r.ptDir.z * t;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelection::EAction CSelection::GetAction() const
{
	EEditMode eMode = GetUserSettings().GetMode();
	EAction a = A_MOVE;
	ELayer eType;
	int nLayer;
	NBuilding::GetLayerID( GetUserSettings().GetActiveLayerID(), &eType, &nLayer );
	if ( eMode == EM_SELECT && IsTerrainLayer( eType ) )
		return a;

	if ( IsRotation() )
		a = A_ROTATE;
	else if ( eMode == EM_RECTANGULAR_SELECTION )
		a = A_RECTSEL;
	else if ( bEmptyOnLBDown && (eMode == EM_SELECT || eMode == EM_RECTANGULAR_SELECTION) && !IsPressed( VK_CONTROL ) )
		a = A_RECTSEL;
	else if ( bEmptyOnLBDown && eMode == EM_SELECT && IsPressed( VK_CONTROL ) )
		a = A_FILL;
	else if ( IsPressed( 'Z' ) )
		a = A_MOVE_Z;

	return a;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OnLButtonDown( const CVec2 &ptPos )
{
	bLBDown = true;
	EEditMode eMode = GetUserSettings().GetMode();
	bEmptyOnLBDown = IsEmpty();
	eAction = GetAction();
	ptLastPos = ptPos;
	SBound box;
	float fZ = GetSelectionBound( &box ) ? box.s.ptCenter.z / NBuilding::WALL_HEIGHT : GetUserSettings().GetActiveFloor();
	ptLBDTile = GetTileUnderPos( GetProjectiveRay( ptPos ), fZ );
	ptCurrentTile = ptLBDTile;
	fLBDz = GetHeight( GetProjectiveRay( ptPos ), ptLBDTile );
	rectSelection = CTRect<float>( ptLBDTile.x, ptLBDTile.y, ptLBDTile.x, ptLBDTile.y );
	ptLBDSelectionCenter = box.s.ptCenter;
	ptLastPos = ptPos;

	if ( !GetUserSettings().GetParam( ME_INSTANT_TERRAIN ) )
		pWorld->EnableTerrainUpdate( false );

	NHPTimer::GetTime( &tSelectTime );

	if ( !IsEmpty() && eAction != A_RECTSEL )
	{
		for ( int i = 0; i < selection.size(); ++i )
			selection[i]->StartMove( ptPos );
	}
	else
	{
		collectedSel = SForceSelection();
		collectedSel.nWorldID = nWorldID;
		CollectSelection( &collectedSel );
		rectLastSel = collectedSel;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OnLButtonUp( const CVec2 &ptPos )
{
	rectSelLines.clear();
	fillparts.clear();
	eAction = GetAction();
	if ( false == bLBDown )
		return;
	pWorld->EnableTerrainUpdate( true );
	bLBDown = false;
	bool bRet = false;
	// после того, как объекты поселекчены их разрешается двигать только после некоторого времени
	NHPTimer::STime tCurrent = tSelectTime;
	double dTime = NHPTimer::GetTimePassed( &tCurrent );
	int nSelMask = GetSelectionMask();
	if ( nSelMask & BT_SUBTEMPLATE )
		pWorld->RebuildTerrInfo( false );
	if ( dTime > START_MOVE_TIME )
	{
		bool bBuildingSelected = false;
		NMapEditor::BeginUndoList();
		for ( int i = 0; i < selection.size(); ++i )
		{
			bRet = selection[i]->EndMove( false ) || bRet;
			if ( dynamic_cast<CFragmentSel*>( selection[i].GetPtr() ) || dynamic_cast<CSpotSel*>( selection[i].GetPtr() ) )
				bBuildingSelected = true;
		}
		NMapEditor::EndUndoList();
		if ( bBuildingSelected && UpdateBuildInfo( pWorld->GetMainBuildingInterface() ) )
				pWorld->GetAIMap()->Sync();
	}
	if ( !selection.empty() )
	{
		pWorld->UpdateWorld( 0, 0 );
		SendSelectionInfo();
	}
	else if ( eAction == A_FILL )
	{
		const IUserSettings &settings = GetUserSettings();
		int nLayerID = settings.GetActiveLayerID();
		ELayer eActiveType;
		int nLayer;
		NBuilding::GetLayerID( settings.GetActiveLayerID(), &eActiveType, &nLayer );
		if ( IsBuildingLayer( eActiveType ) )
		{
			const int nCPartID = settings.GetSelectedBrushID( nLayerID );
			vector<NBuilding::SBuildFragment> frags;

			GetFilledFragments( &frags, nCPartID, nLayerID, settings.GetActiveFloor(), settings.GetActiveRotationID(), ptLBDTile, ptCurrentTile );
			NMapEditor::BeginUndoList();
			if ( frags.empty() && !( fabs( rectSelection.x1 - rectSelection.x2 ) > 0.05 || fabs( rectSelection.y1 - rectSelection.y2 ) > 0.05 ) )
				AddObject( ptPos );
			else
			{
				pBuilding.Refresh();
				NBuilding::CBuildInfo *pInfo = pBuilding->GetValue();
				const int nFirstHash = NBuilding::GetPartHashID( 1, 1, 1 );
				SForceSelection sel;
				for ( int i = 0; i < frags.size(); ++i )
				{
					const NBuilding::SBuildFragment &fr = frags[i];
					if ( fr.nSubBlockID == nFirstHash )
					{
						CVec3 ptPos = fr.ptPos;
						ptPos.x *= FP_GRID_STEP;
						ptPos.y *= FP_GRID_STEP;
						AddFragment( pInfo, ptPos, fr.nConstructionPartID, fr.nRotationID, fr.nFragmentID, &sel.fragsUserIDs );
					}
				}
				if ( SerializeBuilding( pInfo, nWorldID ) )
				{
					pBuilding->Updated();
					pGrid->Updated();
					pWorld->UpdateBuilding();
				}
				sel.nWorldID = nWorldID;
				Select( sel );
			}
			NMapEditor::EndUndoList();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
float fabs2( const CTRect<T> &a, const CTRect<T> &b )
{
	return sqr( a.x1 - b.x1 ) + sqr( a.x2 - b.x2 ) + sqr( a.y1 - b.y1 ) + sqr( a.y2 - b.y2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::Move( const CVec2 &ptPos )
{
	if ( eAction == A_RECTSEL || eAction == A_FILL )
	{
		CTRect<float> r;
		ptCurrentTile = GetTileUnderPos( GetProjectiveRay( ptPos ), GetUserSettings().GetActiveFloor() );
		r.minx = Min( ptCurrentTile.x, ptLBDTile.x );
		r.maxx = Max( ptCurrentTile.x, ptLBDTile.x );
		r.miny = Min( ptCurrentTile.y, ptLBDTile.y );
		r.maxy = Max( ptCurrentTile.y, ptLBDTile.y );
		if ( fabs2( r, rectSelection ) > 0.01f )
		{
			rectSelection = r;
			if ( eAction == A_RECTSEL )
			{
				Select( rectSelection );
				DrawRectangularSelection( CVec3(0.85f,0.85f,0.9f) );
			}
			else
			{
				DrawRectangularSelection( crInsert );
				DrawRectangularFill();
			}
		}
		return false;
	}
	rectSelLines.clear();
	fillparts.clear();
	if ( IsEmpty() )
		return false;
	EMoveMode eMoveMode = GetUserSettings().GetMoveMode();
	CVec2 move = VNULL2;
	SBound box;
	float fZ = GetSelectionBound( &box ) ? box.s.ptCenter.z / NBuilding::WALL_HEIGHT : GetUserSettings().GetActiveFloor();
	float dZ = 0;

	ASSERT( eAction == A_MOVE_Z || eAction == A_MOVE );

	if ( eAction == A_MOVE_Z )
		eMoveMode = MM_Z;

	switch ( eMoveMode )
	{
		case MM_XY:
			{
				CVec2 pt = GetTileUnderPos( GetProjectiveRay( ptPos ), fZ );
				CVec2 ptOrig(box.s.ptCenter.x, box.s.ptCenter.y);
				pt += ptOrig - ptLBDTile;
				move = pt - ptOrig;
				break;
			}
		case MM_Z:
			dZ = GetHeight( GetProjectiveRay( ptPos ), ptLBDTile ) - fLBDz;
			break;
	}
	int nDiscrete = GetMaxMovementDiscrete( selection );
	CVec2 dmove = move;
	if ( nDiscrete > 0 )
	{
		dmove *= FP_INV_GRID_STEP;
		dmove.x = Float2Int( dmove.x );
		dmove.y = Float2Int( dmove.y );
		dmove *= FP_GRID_STEP;
	}
	int nZDiscrete = GetMaxZDiscrete( selection );
	if ( TileAlign() && nZDiscrete == 0 )
		nZDiscrete = 1;
	if ( nZDiscrete > 0 )
	{
		float fZDiscrete = nZDiscrete * NBuilding::WALL_HEIGHT / 4;
		dZ = fZDiscrete * Float2Int( dZ / fZDiscrete  );
	}
	move = CheckPos( box, dmove, ptWorldSize );
	for ( int i = 0; i < selection.size(); ++i )
		selection[i]->TrackMove( eMoveMode == MM_Z ? false : TileAlign(), CVec3( move, dZ ), ptPos );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Test( const CVec2 &ptPos )
{
	if ( selection.empty() )
		return;
	static SRand rand;
	for ( int i = 0; i < selection.size(); ++i )
	{
		CVec3 pos = selection[i]->GetBoundBox().s.ptCenter;
		selection[i]->StartMove( CVec2( pos.x, pos.y ) );
		//selection[i]->TrackMove( CVec2( pos.x, pos.y ) + CVec2( rand.GetFloat( -1.5, 1 ), rand.GetFloat( -1.5, 1 ) ) );
		selection[i]->EndMove( false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::Rotate( const CVec2 &ptPos )
{
	CVec2 pt = ptPos - ptLastPos;
	
	float fRotation = -0.7f * pt.y;
	bool ret = false;
	int nDiscrete = GetMaxRotationDiscrete( selection );
	if ( nDiscrete > 0 )
	{
		fRotation *= 1.0f / nDiscrete;
		fRotation = Float2Int( fRotation ) * nDiscrete;
	}
	for ( int i = 0; i < selection.size(); ++i )
		ret = selection[i]->RotateAround( fRotation, CVec2( ptLBDSelectionCenter.x, ptLBDSelectionCenter.y ) ) | ret;
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRay CSelection::GetProjectiveRay( const CVec2 &ptPos )
{
	CRay r;
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, pCamera->GetPos(), pScene->GetScreenRect(), ptPos, N_FOV );
	return r;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CSelection::GetTileUnderPos( const CRay &r, float fFloor )
{
	SFBTransform tr = pWorld->GetTerrainTransform( FP_GRID_STEP * ptWorldSize.x / 2, FP_GRID_STEP * ptWorldSize.y / 2 );
	CVec3 ptTerr = VNULL3;
	tr.forward.RotateHVector( &ptTerr, ptTerr );
	//
	if ( fabs( r.ptDir.z ) < FP_EPSILON )
		return CVec2( -1, -1 );
	float t = -(r.ptOrigin.z - (fFloor * NBuilding::WALL_HEIGHT + ptTerr.z)) / r.ptDir.z;
	return CVec2( r.ptOrigin.x + t * r.ptDir.x, r.ptOrigin.y + t * r.ptDir.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void GetBound( SBound *pRes, const vector<CVec3> &bound )
{
	CVec3 ptMin = bound.front();
	CVec3 ptMax = bound.front();
	for ( int i = 1; i < bound.size(); ++i )
	{
		const CVec3 &pt = bound[i];
		ptMin = CVec3( Min( ptMin.x, pt.x ), Min( ptMin.y, pt.y ), Min( ptMin.z, pt.z ) );
		ptMax = CVec3( Max( ptMax.x, pt.x ), Max( ptMax.y, pt.y ), Max( ptMax.z, pt.z ) );
	}
	pRes->BoxInit( ptMin, ptMax );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::GetSelectionBound( SBound *pRes )
{
	if ( selection.empty() )
		return false;

	vector<CVec3> bound;
	for ( int i = 0; i < selection.size(); ++i )
	{
		SBound b = selection[i]->GetBoundBox();
		bound.push_back( b.s.ptCenter - b.ptHalfBox );
		bound.push_back( b.s.ptCenter + b.ptHalfBox );
	}
	GetBound( pRes, bound );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::GetSelectionMovingBound( SBound *pRes )
{
	if ( selection.empty() )
		return false;

	vector<CVec3> bound;
	for ( int i = 0; i < selection.size(); ++i )
	{
		SBound b = selection[i]->GetMovingBoundBox();
		bound.push_back( b.s.ptCenter - b.ptHalfBox );
		bound.push_back( b.s.ptCenter + b.ptHalfBox );
	}
	GetBound( pRes, bound );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::AddObject( const CVec2 &ptPosCursor )
{
	const IUserSettings &settings = GetUserSettings();

	CVec2 pt = GetTileUnderPos( GetProjectiveRay( ptPosCursor ), settings.GetActiveFloor() );
	CVec3 ptPos( pt.x, pt.y, settings.GetActiveFloor() );

	switch ( settings.GetMode() )
	{
		case EM_SELECT:
			{
				pBuilding.Refresh();
				NBuilding::CBuildInfo *pInfo = pBuilding->GetValue();
				if ( !pInfo )
					break;
				const int nConstructionPartID = settings.GetSelectedBrushID( settings.GetActiveLayerID() );
				if ( nConstructionPartID > 0 )
				{
					const int nLayerID = settings.GetActiveLayerID();
					//const int nFragmentID = NBuilding::MakeFragmentID( GetLayerType( nLayerID ), GetLayerInd( nLayerID ) );
					const int nFragmentID = nLayerID;
					if ( AddFragment( pInfo, ptPos, nConstructionPartID, settings.GetActiveRotationID(), nFragmentID ) )
					{
						if ( SerializeBuilding( pInfo, nWorldID ) )
						{
							pBuilding->Updated();
							pGrid->Updated();
							pWorld->UpdateBuilding();
						}
					}
				}
			}
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::DeleteSelected()
{
	bool bRet = false;
	bool bBuildingSelected = false;
	NMapEditor::BeginUndoList();
	for ( int i = 0; i < selection.size(); ++i )
	{
		bBuildingSelected = bBuildingSelected || dynamic_cast<CFragmentSel*>( selection[i].GetPtr() );
		bool b = selection[i]->DelayedDelete();
		ASSERT( b );
		bRet =  b || bRet;
	}
	NMapEditor::EndUndoList();
	//
	Sleep(10);
	NDatabase::Refresh<NDb::CWaypoint>();
	if ( bBuildingSelected )
		UpdateBuildInfo( pVar->GetRecordID() );
	//
	if ( bRet )
	{
		CDynamicCast<NWorld::CEditorWorld> pW(pWorld);
		if (pW)
		{
			//pW->UpdateAll();
			if ( bBuildingSelected )
			{
				//pBuilding->Updated();
				//pW->GetMainBuildingInterface()->UpdateAllParts();
				pWorld->ShowMainBuilding( false );
				pWorld->ShowMainBuilding( true );
			}
		}
	}
	Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::AddFragment( NBuilding::CBuildInfo *pInfo, CVec3 ptPos, int nGeomID, int nRotationID, int nLayerID, vector<int> *pUserIDs )
{
	SBound bbox;
	if ( !ComputeBoundBox( &bbox, nGeomID, nRotationID, false ) )
		return false;
	CVec2 pt = CheckTilePos( bbox, CVec2( ptPos.x, ptPos.y ), ptWorldSize );
	ptPos.x = pt.x / FP_GRID_STEP;
	ptPos.y = pt.y / FP_GRID_STEP;
//	ptPos.x = int( ptPos.x + 0.5 );
//	ptPos.y = int( ptPos.y + 0.5 );

	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( nGeomID );
	if ( !IsValid( pTCP ) )
		return false;
	SRand rand;
	CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );
	if ( !IsValid( pCP ) )
		return false;
	NBuilding::SBuildFragment fr;
	fr.nConstructionPartID = nGeomID;
	fr.nSubBlockID = 0;
	fr.ptPos       = ptPos;
	fr.nRotationID = nRotationID;
	fr.nFragmentID = nLayerID;

	SDiscretePos dpos( 0, VNULL3, nRotationID );
	CVec3 ptDC = CVec3( 1, 1, 0 );
	dpos.MoveAndRotate( &ptDC );

	if ( pCP->nSizeY != 0 ) // стена или не стена ?
	{
		fr.nID = pInfo->CreateNextFragmentID();
		pInfo->solidFragments.push_back( fr );
		NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_INSERT, nWorldID, &fr, 0 ) );
		//
		CVec3 pt = fr.ptPos + ptDC;
		pGrid->UpdatePart( NBuilding::SPoint3( pt.x, pt.y, pt.z * 4 + 1 ) );
		//
		const int nf = (fr.ptPos.z + 0.5f) + pCP->nSizeZ;
		pInfo->nMaxFloor = Max( pInfo->nMaxFloor, nf );
		pInfo->nMinFloor = Min( pInfo->nMinFloor, nf );
		if ( pUserIDs )
			pUserIDs->push_back( pInfo->solidFragments.size() - 1 );
		CheckLayerExistence( fr.nFragmentID );
		return true;
	}
	const int nLength = pCP->nSizeX;
	CVec3 ptDir(2, 0, 0);
	dpos.MoveAndRotate( &ptDir );
	NMapEditor::BeginUndoList();
	for ( int k = 0; k < nLength; ++k )
	{
		if ( pUserIDs )
			pUserIDs->push_back( 0x40000000 | pInfo->wallFragments.size() );
		fr.ptPos = ptPos + k * ptDir;
		fr.nSubBlockID = NBuilding::GetPartHashID( 1 + k, 1, 1 );
		fr.nID = pInfo->CreateNextFragmentID();
		pInfo->wallFragments.push_back( fr );
		NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_INSERT, nWorldID, &fr, 0 ) );
		//
		CVec3 pt = fr.ptPos + 0.5f * ptDir;
		pGrid->UpdatePart( NBuilding::SPoint3( pt.x, pt.y, pt.z * 4 + 1 ) );
		//
		const int nf = fr.ptPos.z + 0.5f + pCP->nSizeZ;
		pInfo->nMaxFloor = Max( pInfo->nMaxFloor, nf );
		pInfo->nMinFloor = Min( pInfo->nMinFloor, nf );
	}
	NMapEditor::EndUndoList();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::BuildingUpdated()
{
	pBuilding.Refresh();
	SerializeBuilding( pBuilding->GetValue(), nWorldID );
	pBuilding->Updated();
	pGrid->Updated();
//	if ( CDynamicCast<NWorld::CEditorWorld> pW(pWorld) )
//		pW->UpdateAll();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OnLBDblClick( const CVec2 &ptPos )
{
	if ( selection.empty() )
		return;
	CSelectedObj *pObj = selection.front();
	if ( !IsValid( pObj ) )
		return;
	pObj->OnLBDblClick();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OnCopy()
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		if ( IsValid( selection[i] ) )
			selection[i]->OnCopy();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
bool PushSelected( const vector< CPtr<CSelectedObj> > &selection, vector< CPtr<CSelectedObj> > *pNewSelection, int nID, T *p = 0 )
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		T *pObj = dynamic_cast<T*>( selection[i].GetPtr() );
		if ( pObj && pObj->IsEqual( nID ) )
		{
			pNewSelection->push_back( pObj );
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Select( const SForceSelection &sel, bool bDiscardOldSel )
{
	if ( nWorldID != sel.nWorldID )
	{
		ASSERT(0);
		return;
	}
	CDynamicCast<NWorld::CEditorWorld> pW( pWorld );
	if ( !IsValid( pW ) )
		return;
	if ( bDiscardOldSel )
		Clear();
	vector< CPtr<CSelectedObj> > oldsel = selection;
	selection.clear();
	//
	for ( int i = 0; i < sel.fragsUserIDs.size(); ++i )
	{
		int nID = sel.fragsUserIDs[i];
		if ( PushSelected<CFragmentSel>( oldsel, &selection, nID ) )
			continue;
		CPtr<CSelectedObj> pSel = new CFragmentSel( this, pW->GetMainBuildingInterface(), nID, pW );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	SRand rand;
	for ( int i = 0; i < sel.objectIDs.size(); ++i )
	{
		int nID = sel.objectIDs[i];
		if ( PushSelected<CObjectSel>( oldsel, &selection, nID ) )
			continue;
		NDb::CFinalElement *pF = NDb::GetFinalElement( nID );
		if ( !pF || !IsValid( pF->pVariant ) || pF->pVariant->GetRecordID() != nWorldID || !IsValid( pF->pObject ) )
			continue;
		CPtr<NDb::CModel> pModel;
		if ( IsValid( pF->pObject->pObject ) )
		{
			CPtr<NDb::CObject> pO = pF->pObject->pObject->CreateObject( &rand );
			if ( IsValid( pO->pModels[0] ) )
			pModel = pO->pModels[0]->pModel;
		}
		if ( IsValid( pF->pObject->pRPGItem ) && IsValid( pF->pObject->pRPGItem->pModel ) )
		{
			SRand rand;
			pModel = pF->pObject->pRPGItem->pModel->CreateModel( &rand );
		}
		CPtr<CSelectedObj> pSel = new CObjectSel( pF, pModel, this, CTPoint<int>( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight ) );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	for ( int i = 0; i < sel.terrSpotsIDs.size(); ++i )
	{
		int nID = sel.terrSpotsIDs[i];
		if ( PushSelected<CTerrSpot>( oldsel, &selection, nID ) )
			continue;
		NDb::CRndTerrainSpot *pS = NDb::GetRndTerrainSpot( nID );
		if ( !pS )
			continue;
		for ( int j = 0; j < pVar->terrainSpots.size(); ++j )
		{
			if ( IsValid( pVar->terrainSpots[j] ) && pVar->terrainSpots[j]->GetRecordID() == nID )
			{
				int nUserID = MakeUserID( BT_TEXSPOT, MakeSpotID( ST_TERRAIN, j, CSpotSel::H_C ) );
				CPtr<CSelectedObj> pSel = new CTerrSpot( pS, this, nUserID );
				if ( pSel->IsInitialized() )
					selection.push_back( pSel );
			}
		}
	}
	//
	for ( int i = 0; i < sel.wallSpotsIDs.size(); ++i )
	{
		int nID = sel.wallSpotsIDs[i];
		if ( PushSelected<CSpotSel>( oldsel, &selection, nID ) )
			continue;

		int nUserID = MakeUserID( BT_TEXSPOT, MakeSpotID( ST_WALL, nID, CSpotSel::H_C ) );
		CPtr<CSelectedObj> pSel = new CSpotSel( this, pBuilding, pGrid, nUserID );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	for ( int i = 0; i < sel.subtemplateIDs.size(); ++i )
	{
		int nID = sel.subtemplateIDs[i];
		if ( PushSelected<CSubTemplateSel>( oldsel, &selection, nID ) )
			continue;

		int nUserID = MakeUserID( BT_SUBTEMPLATE, nID );
		CPtr<CSelectedObj> pSel = new CSubTemplateSel( this, CTPoint<int>( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight ), nUserID );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	for ( int i = 0; i < sel.unitIDs.size(); ++i )
	{
		int nID = sel.unitIDs[i];
		if ( PushSelected<CUnitSel>( oldsel, &selection, nID ) )
			continue;

		CPtr<CSelectedObj> pSel = new CUnitSel( NDb::GetUnit( nID ), this, CTPoint<int>( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight ) );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	for ( int i = 0; i < sel.waypointIDs.size(); ++i )
	{
		int nID = sel.waypointIDs[i];
		if ( PushSelected<CWaypoint>( oldsel, &selection, nID ) )
			continue;

		CPtr<CSelectedObj> pSel = new CWaypoint( nID, this, pVar );
		if ( pSel->IsInitialized() )
			selection.push_back( pSel );
	}
	//
	SendSelectionInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OnPaste( const SForceSelection &sel, const CVec2 &ptCursor, bool bAlignLeftTop, int nSelectionMinFloor )
{
	Select( sel );
	SBound box;
	if ( !GetSelectionBound( &box ) )
		return;
	int nFloor = nSelectionMinFloor == N_DEF_SELMINF ? GetUserSettings().GetActiveFloor() : nSelectionMinFloor;
	CVec2 pt = GetTileUnderPos( GetProjectiveRay( ptCursor ), nFloor );
	bool bNewTemplatePaste = ptCursor == CVec2(-1,-1);
	if ( bNewTemplatePaste ) // координаты кусора невалидны
	{
		float fBX = 2 * box.ptHalfBox.x * FP_INV_GRID_STEP;
		float fBY = 2 * box.ptHalfBox.y * FP_INV_GRID_STEP;
		pt = CVec2( 0.5f * (ptWorldSize.x - fBX), ptWorldSize.y - 0.5f * (ptWorldSize.y - fBY) );
		pt *= FP_GRID_STEP;
	}
	CVec2 ptLT = CVec2( box.s.ptCenter.x, box.s.ptCenter.y ) - CVec2( box.ptHalfBox.x, -box.ptHalfBox.y );
	CVec2 ptMove = bAlignLeftTop ? pt - ptLT : VNULL2;

	int nDiscrete = GetMaxMovementDiscrete( selection );

	if ( bNewTemplatePaste || nDiscrete > 0 )
	{
		ptMove *= FP_INV_GRID_STEP;
		ptMove = FP_GRID_STEP * CVec2( int( ptMove.x), int(ptMove.y - 0.5f) );
	}
	ptMove = CheckPos( box, ptMove, ptWorldSize );
	bool bBuildingSelected = false;
	//
	NMapEditor::BeginUndoList();
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *p = selection[i];
		if ( !p )
			continue;
		CDynamicCast<CSpot> pS(p);
		if (pS)
		{
			pS->Move( CVec3( ptMove, 0 ) );
			pS->EndMove( false );
		}
		else
		{
			p->StartMove( VNULL2 );
			p->TrackMove( TileAlign(), CVec3( ptMove, 0 ), ptCursor );
			p->EndMove( false );
			bBuildingSelected = bBuildingSelected || dynamic_cast<CFragmentSel*>( p );
		}
	}
	NMapEditor::EndUndoList();
	if ( bBuildingSelected && UpdateBuildInfo( pWorld->GetMainBuildingInterface() ) )
		pWorld->GetAIMap()->Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::ObjectUpdated( int nDBFinElemID )
{
	pWorld->UpdateObject( nDBFinElemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::TerrainSpotUpdated( const vector<NBuilding::SProjectedSpot> &spots )
{
	pWorld->UpdateTerrainSpots( spots );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsLayerVisible( ELayer type, int nLayer )
{
	return ::IsLayerVisible( NBuilding::MakeFragmentID( type, nLayer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::SelectAll()
{
	SForceSelection sel;

	pBuilding.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuilding->GetValue();
	sel.nWorldID = nWorldID;
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );

	for ( int i = 0; i < pInfo->wallFragments.size(); ++i )
	{
		NBuilding::SBuildFragment &fr = pInfo->wallFragments[i];
		ELayer type;
		int nLayer;
		NBuilding::GetLayerID( fr.nFragmentID, &type, &nLayer );
		if ( fr.nFragmentID == -1 || nLayer > 100 || find( layers.begin(), layers.end(), fr.nFragmentID ) != layers.end() )
			sel.fragsUserIDs.push_back( i | 0x40000000 );
	}
	for ( int i = 0; i < pInfo->solidFragments.size(); ++i )
	{
		NBuilding::SBuildFragment &fr = pInfo->solidFragments[i];
		ELayer type;
		int nLayer;
		NBuilding::GetLayerID( fr.nFragmentID, &type, &nLayer );
		if ( fr.nFragmentID == -1 || nLayer > 100 || find( layers.begin(), layers.end(), fr.nFragmentID ) != layers.end() )
			sel.fragsUserIDs.push_back( i );
	}
	for ( int i = 0; i < pInfo->ladders.size(); ++i )
	{
		NBuilding::SLadder &lad = pInfo->ladders[i];
		sel.ladderIDs.push_back( lad.nID );
	}
	//
	if ( find( layers.begin(), layers.end(), NBuilding::MakeFragmentID( LID_OBJECTS, 0 ) ) != layers.end() )
	{
		for ( int i = 0; i < pVar->pFinalElements.size(); ++i )
			if ( IsValid( pVar->pFinalElements[i] ) )
				sel.objectIDs.push_back( pVar->pFinalElements[i]->GetRecordID() );
	}
	for ( int i = 0; i < pInfo->spots.size(); ++i )
	{
		NBuilding::SProjectedSpot &s = pInfo->spots[i];
		//int nUserID = NWysiwyg::MakeUserID( BT_TEXSPOT, NWysiwyg::MakeSpotID( ST_WALL, s.nID, NWysiwyg::CSpotSel::H_C ) );
		//sel.wallSpotsIDs.push_back( s.nID );
	}
	for ( int i = 0; i < pVar->terrainSpots.size(); ++i )
	{
		if ( IsValid( pVar->terrainSpots[i] ) && IsLayerVisible( LID_SPOTS, pVar->terrainSpots[i]->nLayer ) )
			sel.terrSpotsIDs.push_back( pVar->terrainSpots[i]->GetRecordID() );
	}
	if ( find( layers.begin(), layers.end(), NBuilding::MakeFragmentID( LID_SUBTEMPLATES, 0 ) ) != layers.end() )
	{
		for ( int i = 0; i < pVar->rects.size(); ++i )
			if ( IsValid( pVar->rects[i] ) )
				sel.subtemplateIDs.push_back( pVar->rects[i]->GetRecordID() );
	}
	if ( find( layers.begin(), layers.end(), NBuilding::MakeFragmentID( LID_UNITS, 0 ) ) != layers.end() )
	{
		for ( int i = 0; i < pVar->pUnits.size(); ++i )
			if ( IsValid( pVar->pUnits[i] ) )
				sel.unitIDs.push_back( pVar->pUnits[i]->GetRecordID() );
	}
	for ( int i = 0; i < pVar->waypoints.size(); ++i )
		if ( IsValid( pVar->waypoints[i] ) )
			sel.waypointIDs.push_back( pVar->waypoints[i]->GetRecordID() );
	Select( sel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::ProcessEvent( const string &str )
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *p = selection[i];
		if ( p )
			p->ProcessEvent( str );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelection::GetSelectionMask()
{
	int nMask = 0;
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *pO = selection[i];
		CDynamicCast<CFragmentSel> pFrag(pO);
		if (pFrag)
			nMask |= BT_GEOMETRY;
		else {
			CDynamicCast<CObjectSel> pObjSel(pO);
			if (pObjSel)
				nMask |= BT_OBJECT;
			else {
				CDynamicCast<CSpotSel> pSpotSel(pO);
				if (pSpotSel)
					nMask |= BT_WALLSPOT;
				else {
					CDynamicCast<CSpot> pSpot(pO);
					if (pSpot)
						nMask |= BT_TEXSPOT;
					else {
						CDynamicCast<CWaypoint> pWayp(pO);
						if (pWayp)
							nMask |= BT_WAYPOINT;
						else {
							CDynamicCast<CUnitSel> pUnitSel(pO);
							if (pUnitSel)
								nMask |= BT_UNIT;
							else {
								CDynamicCast<CSubTemplateSel> pSubTemp(pO);
								if (pSubTemp)
									nMask |= BT_SUBTEMPLATE;
							}
						}
					}
				}
			}
		}
	}
	return nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::AssignBuildingSpot( int nSpotID )
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *pO = selection[i];
		CDynamicCast<CFragmentSel> p(pO);
		if (p)
		{
			p->AddSpot( nSpotID );
			const vector<NBuilding::SPoint3>& parts = p->GetUpdateParts();
			for ( int j = 0; j < parts.size(); ++j )
				pGrid->UpdatePart( parts[j] );
		}
	}
	BuildingUpdated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelection::GetSelectedSpotID() const
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *pO = selection[i];
		CDynamicCast<CSpotSel> p(pO);
		if ( p )
			return p->GetSpotID();
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::SubTemplateUpdated( int nUserID )
{
	pWorld->UpdateSubTemplate( nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::UnitUpdated( int nDBUnitID )
{
	pWorld->UpdateUnit( nDBUnitID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::AddConstructionPart( vector<int> *pUserIDs, int nCPartID, CVec3 ptPos )
{
	pBuilding.Refresh();
	const IUserSettings &settings = GetUserSettings();
	int nLayerID = settings.GetActiveLayerID();
	ELayer nType = NBuilding::GetLayerType( nLayerID );
	if ( !IsBuildingLayer( nType ) )
		nLayerID = NBuilding::MakeFragmentID( LID_FLOORS, 0 );
	ptPos.z = settings.GetActiveFloor();
	return AddFragment( pBuilding->GetValue(), ptPos, nCPartID, settings.GetActiveRotationID(), nLayerID, pUserIDs );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::WaypointUpdated( int nWaypointID )
{
	pWorld->UpdateWaypoints();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline bool IsInRect( const CTRect<float> &rect, const T &pt )
{
	return pt.x > rect.minx && pt.x < rect.maxx && pt.y > rect.miny && pt.y < rect.maxy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Select( CTRect<float> r )
{
	int fFloor = floor( GetUserSettings().GetActiveFloor() );
	SForceSelection sel( collectedSel );
	sel.nWorldID = nWorldID;
	r.x1 *= FP_INV_GRID_STEP;
	r.x2 *= FP_INV_GRID_STEP;
	r.y1 *= FP_INV_GRID_STEP;
	r.y2 *= FP_INV_GRID_STEP;
	//
	if ( IsLayerVisible( LID_OBJECTS, 0 ) )
	{
		for ( int i = 0; i < pVar->pFinalElements.size(); ++i )
		{
			NDb::CFinalElement *pF = pVar->pFinalElements[i];
			if ( IsValid( pF ) && pF->nFloor == fFloor && IsInRect( r, pF->ptPos ) )
				sel.objectIDs.push_back( pF->GetRecordID() );
		}
	}
	//
	//if ( IsLayerVisible( LID_TERRCOLOR ) || IsLayerVisible( LID_HEIGHTS ) || IsLayerVisible( LID_TILES ) )
	{
		for ( int i = 0; i < pVar->terrainSpots.size(); ++i )
		{
			NDb::CRndTerrainSpot *pS = pVar->terrainSpots[i];
			if ( IsValid( pS ) && IsInRect( r, FP_INV_GRID_STEP * pS->ptPos ) && IsLayerVisible( LID_SPOTS, pS->nLayer ) )
				sel.terrSpotsIDs.push_back( pS->GetRecordID() );
		}
	}
	//
	if ( IsLayerVisible( LID_SUBTEMPLATES, 0 ) )
	{
		for ( int i = 0; i < pVar->rects.size(); ++i )
		{
			NDb::CRectangle *pR = pVar->rects[i];
			if ( IsValid( pR ) && pR->nFloor == fFloor && IsInRect( r, FP_INV_GRID_STEP * pR->ptCenter ) )
				sel.subtemplateIDs.push_back( pR->GetRecordID() );
		}
	}
	//
	if ( IsLayerVisible( LID_UNITS, 0 ) )
	{
		for ( int i = 0; i < pVar->pUnits.size(); ++i )
		{
			NDb::CUnit *pU = pVar->pUnits[i];
			if ( IsValid( pU ) && pU->nFloor == fFloor && IsInRect( r, pU->ptPos ) )
				sel.unitIDs.push_back( pU->GetRecordID() );
		}
	}
	//
	pBuilding.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuilding->GetValue();
	if ( IsLayerVisible( LID_WALLS, 0 ) )
	{
		for ( int i = 0; i < pInfo->wallFragments.size(); ++i )
		{
			NBuilding::SBuildFragment &fr = pInfo->wallFragments[i];
			if ( floor( fr.ptPos.z ) == int( fFloor ) && IsInRect( r, fr.ptPos ) )
				sel.fragsUserIDs.push_back( i | 0x40000000 );
		}
		for ( int i = 0; i < pInfo->spots.size(); ++i )
		{
			NBuilding::SProjectedSpot &s = pInfo->spots[i];
			if ( IsInRect( r, s.ptOrigin ) )
				sel.wallSpotsIDs.push_back( s.nID );
		}
	}
	int nFloor = floor( fFloor );
	for ( int i = 0; i < pInfo->solidFragments.size(); ++i )
	{
		NBuilding::SBuildFragment &fr = pInfo->solidFragments[i];
		if ( floor( fr.ptPos.z ) == nFloor && IsInRect( r, fr.ptPos ) && ::IsLayerVisible( fr.nFragmentID ) )
			sel.fragsUserIDs.push_back( i );
	}
	//
	for ( int i = 0; i < pVar->waypoints.size(); ++i )
	{
	}
	if ( !(sel == rectLastSel) )
	{
		Select( sel, false );
		rectLastSel = sel;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::WallSpotUpdated( int nSpotID )
{
	pWorld->UpdateWallSpot( nSpotID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void FillWallLine( vector<NBuilding::SBuildFragment> *pFragments, const CVec3 &ptStart, 
	const CVec3 &ptDir, int nParts, int nPartLength, const NBuilding::SBuildFragment &pattern )
{
	const CVec3 ptPart = nPartLength * ptDir;
	for ( int i = 0; i < nParts; ++i )
		for ( int j = 0; j < nPartLength; ++j )
		{
			NBuilding::SBuildFragment fr = pattern;
			fr.ptPos = ptStart + i * ptPart + j * ptDir;
			fr.nSubBlockID = NBuilding::GetPartHashID( 1 + j, 1, 1 );
			pFragments->push_back( fr );
		}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
// rect - в метрах
void CSelection::GetFilledFragments( vector<NBuilding::SBuildFragment> *pFragments, 
	int nCPartID, int nLayerID, float fFloor, int nRotationID, CVec2 ptStart, CVec2 ptEnd )
{
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( nCPartID );
	if ( !IsValid( pTCP ) )
		return;
	static SRand rand;
	CPtr<NDb::CConstructionPart> pPart = pTCP->CreateConstructionPart( &rand );
	if ( !IsValid( pPart ) )
		return;
	//
	ptStart *= FP_INV_GRID_STEP;
	ptEnd *= FP_INV_GRID_STEP;
	NBuilding::SBuildFragment fr;
	fr.nConstructionPartID = nCPartID;
	fr.nFragmentID = nLayerID;
	const int nStepX = pPart->nSizeX * 2;
	const int nStepY = pPart->nSizeY * 2;
	const bool bWall = pPart->nSizeY == 0;
	const int nPartsX = fabs( ptEnd.x - ptStart.x ) / nStepX;
	const int nPartsY = fabs( ptEnd.y - ptStart.y ) / nStepX;
	//
	CVec2 ptCorner( ptStart );
	ptCorner.x += Sign( ptEnd.x - ptStart.x ) * nPartsX * nStepX;
	ptCorner.y += Sign( ptEnd.y - ptStart.y ) * nPartsY * nStepX;
	CTRect<int> r;
	r.minx = Max( 0, Float2Int( Min( ptStart.x, ptCorner.x ) ) );
	r.miny = Max( 0, Float2Int( Min( ptStart.y, ptCorner.y ) ) );
	r.maxx = Float2Int( Min( ptWorldSize.x, Max( ptStart.x, ptCorner.x ) ) );
	r.maxy = Float2Int( Min( ptWorldSize.y, Max( ptStart.y, ptCorner.y ) ) );
	//
	if ( bWall )
	{
		int nFloor = fFloor;
		fr.nRotationID = SDiscretePos::TURN_0;
		FillWallLine( pFragments, CVec3( r.minx, r.miny, nFloor ), CVec3(2, 0, 0), nPartsX, pPart->nSizeX, fr );
		if ( nPartsY > 0 )
		{
			fr.nRotationID = SDiscretePos::TURN_180;
			FillWallLine( pFragments, CVec3( r.maxx, r.maxy, nFloor ), CVec3(-2, 0, 0), nPartsX, pPart->nSizeX, fr );
		}
		//
		fr.nRotationID = SDiscretePos::TURN_90;
		FillWallLine( pFragments, CVec3( r.maxx, r.miny, nFloor ), CVec3(0, 2, 0), nPartsY, pPart->nSizeX, fr );
		if ( nPartsX > 0 )
		{
			fr.nRotationID = SDiscretePos::TURN_270;
			FillWallLine( pFragments, CVec3( r.minx, r.maxy, nFloor ), CVec3(0, -2, 0), nPartsY, pPart->nSizeX, fr );
		}
	}
	else
	{
		int nHash = NBuilding::GetPartHashID( 1, 1, 1 );
		fr.nRotationID = nRotationID;
		for ( int x = r.minx; x < r.maxx; x += nStepX )
			for ( int y = r.miny; y < r.maxy; y += nStepY )
			{
				fr.ptPos = CVec3( x, y, fFloor );
				fr.nSubBlockID = nHash;
				pFragments->push_back( fr );
			}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::DrawRectangularFill()
{
	fillparts.clear();
	const IUserSettings &settings = GetUserSettings();
	int nLayerID = settings.GetActiveLayerID();
	ELayer eActiveType;
	int nLayer;
	NBuilding::GetLayerID( GetUserSettings().GetActiveLayerID(), &eActiveType, &nLayer );

	if ( !IsBuildingLayer( eActiveType ) )
		return;
	const int nCPartID = settings.GetSelectedBrushID( nLayerID );
	const int nRotationID = GetUserSettings().GetActiveRotationID();
	vector<NBuilding::SBuildFragment> frags;
	GetFilledFragments( &frags, nCPartID, nLayerID, settings.GetActiveFloor(), nRotationID, ptLBDTile, ptCurrentTile );
	//
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( nCPartID );
	if ( !IsValid( pTCP ) )
		return;
	static SRand rand;
	CPtr<NDb::CConstructionPart> pPart = pTCP->CreateConstructionPart( &rand );
	if ( !IsValid( pPart ) || !IsValid( pPart->pGeometry ) )
		return;
	pHoldModel = CreateModel( pPart->pGeometry, pPart->pDefMaterials );
	//
	CVec3 ptShift(VNULL3);
	if ( pPart->nSizeY > 0 )
	{
		switch ( nRotationID )
		{
		case SDiscretePos::TURN_90:
			ptShift = CVec3( 2 * pPart->nSizeY, 0, 0 );
			break;
		case SDiscretePos::TURN_180:
			ptShift = CVec3( 2 * pPart->nSizeX, 2 * pPart->nSizeY, 0 );
			break;
		case SDiscretePos::TURN_270:
			ptShift = CVec3( 0, 2 * pPart->nSizeX, 0 );
			break;
		}
	}
	ptShift *= FP_GRID_STEP;
	const int nFirstHash = NBuilding::GetPartHashID( 1, 1, 1 );
	for ( int i = 0; i < frags.size(); ++i )
	{
		const NBuilding::SBuildFragment &fr = frags[i];
		if ( fr.nSubBlockID != nFirstHash )
			continue;
		CVec3 ptPos( fr.ptPos.x * FP_GRID_STEP, fr.ptPos.y * FP_GRID_STEP, fr.ptPos.z * NBuilding::WALL_HEIGHT );
		const SFBTransform tr = GetTerrainTransform( 0.5f * ptWorldSize.x, 0.5f * ptWorldSize.y ) * MakeTransform( ptPos + ptShift, RotationIDToAngle( fr.nRotationID ) );
		fillparts.push_back( pScene->CreateMesh( pHoldModel, tr ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::LadderUpdated( int nLadderID )
{
	pWorld->UpdateLadders();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::CollectSelection( SForceSelection *pSel )
{
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *p = selection[i];
		CDynamicCast<CFragmentSel> pFragSel(p);
		if (pFragSel)
			pSel->fragsUserIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CObjectSel> pObjectSel(p);
		if (pObjectSel)
			pSel->objectIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CTerrSpot> pTerrSpot(p);
		if (pTerrSpot)
			pSel->terrSpotsIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CSpotSel> pSpotSel(p);
		if (pSpotSel)
			pSel->wallSpotsIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CLadderSel> pLadderSel(p);
		if (pLadderSel)
			pSel->ladderIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CSubTemplateSel> pSubTemplate(p);
		if (pSubTemplate)
			pSel->subtemplateIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CUnitSel> pUnitSel(p);
		if (pUnitSel)
			pSel->unitIDs.push_back( p->GetSelectionID() );
		CDynamicCast<CWaypoint> pWayp(p);
		if (pWayp)
			pSel->waypointIDs.push_back( p->GetSelectionID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::StartRotate()
{
	if ( !GetUserSettings().GetParam( ME_INSTANT_TERRAIN ) )
		pWorld->EnableTerrainUpdate( false );
	//
	fCurrentKbdRotation = 0;
	NMapEditor::BeginUndoList();
	for ( int i = 0; i < selection.size(); ++i )
		selection[i]->StartMove( VNULL2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Rotate( float fAngle )
{
	SBound b;
	if ( !GetSelectionBound( &b ) )
		return;
	CVec2 ptCenter( b.s.ptCenter.x, b.s.ptCenter.y );
	fCurrentKbdRotation += fAngle;
	for ( int i = 0; i < selection.size(); ++i )
		selection[i]->RotateAround( fCurrentKbdRotation, ptCenter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::EndRotate()
{
	bool bBuildingSelected = false;

	for ( int i = 0; i < selection.size(); ++i )
	{
		selection[i]->EndMove( false );
		bBuildingSelected = bBuildingSelected || dynamic_cast<CFragmentSel*>( selection[i].GetPtr() );
	}
	if ( bBuildingSelected && UpdateBuildInfo( pWorld->GetMainBuildingInterface() ) )
		pWorld->GetAIMap()->Sync();
	pWorld->EnableTerrainUpdate( true );
	NMapEditor::EndUndoList();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::StartMove()
{
	if ( !GetUserSettings().GetParam( ME_INSTANT_TERRAIN ) )
		pWorld->EnableTerrainUpdate( false );
	//
	ptCurrentKbdMove = VNULL3;
	NMapEditor::BeginUndoList();
	for ( int i = 0; i < selection.size(); ++i )
		selection[i]->StartMove( VNULL2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Move( const CVec3 &ptMove )
{
	ptCurrentKbdMove += ptMove;
	for ( int i = 0; i < selection.size(); ++i )
		selection[i]->TrackMove( TileAlign(), ptCurrentKbdMove, VNULL2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::EndMove()
{
	bool bBuildingSelected = false;

	for ( int i = 0; i < selection.size(); ++i )
	{
		selection[i]->EndMove( false );
		bBuildingSelected = bBuildingSelected || dynamic_cast<CFragmentSel*>( selection[i].GetPtr() );
	}
	pWorld->EnableTerrainUpdate( true );
	if ( bBuildingSelected && UpdateBuildInfo( pWorld->GetMainBuildingInterface() ) )
		pWorld->GetAIMap()->Sync();
	NMapEditor::EndUndoList();
	SendSelectionInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::OpenObject( int nObjectID )
{
	pWorld->OpenObject( nObjectID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::MoveSelection( const CVec3 &ptCenter )
{
	if ( IsEmpty() )
		return;
	SBound b;
	GetSelectionBound( &b );
	CVec3 ptMove = ptCenter - b.s.ptCenter;
	StartMove();
	Move( ptMove );
	EndMove();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelection::GetFirstSelectedID( EBrushType eType )
{
	int nMask = 0;
	for ( int i = 0; i < selection.size(); ++i )
	{
		CSelectedObj *pO = selection[i];
		switch ( eType )
		{
			case BT_OBJECT:
				CDynamicCast<CObjectSel> p(pO);
				if (p)
					return pO->GetSelectionID();
				break;
		}
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
