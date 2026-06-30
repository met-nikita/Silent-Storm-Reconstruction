#include "StdAfx.h"
#include "..\Main\Camera.h"
#include "IWysiwyg.h"
#include "..\Main\wInterface.h"
#include "WysiwygSPotSel.h"
#include "..\Main\GView.h"
#include "..\Main\Transform.h"
#include "..\Main\Grid.h"
#include "wEditor.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "..\Main\BuildingGrid.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CObj<CMemObject> CSpot::pmoCube = new CMemObject;
////////////////////////////////////////////////////////////////////////////////////////////////////
int MakeSpotID( ESpotType type, int nIndex, CSpot::EHandle hadnle )
{
	return (int)hadnle << 16 | (int)type << 14 | nIndex;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetSpotID( int nSpotID, ESpotType *pType, int *pnIndex, CSpot::EHandle *pHandle )
{
	*pType = (ESpotType)((nSpotID >> 14) & 0x3);
	*pHandle = (CSpot::EHandle) (nSpotID >> 16);
	*pnIndex = nSpotID & 0x3fff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 NearestTile( const CVec3 &pt )
{
	CVec3 r = (1.0f / FP_GRID_STEP) * CVec3( pt.x, pt.y, 0 );
	r = CVec3( Float2Int( r.x ), Float2Int( r.y ), 0 );
	r *= FP_GRID_STEP;
	return CVec3( r.x, r.y, pt.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 NearestTile( const CVec3 &pt,	const CVec3 &ptNormal )
{
	CVec3 r = (1.0f / FP_GRID_STEP) * CVec3( pt.x, pt.y, 0 );
	r = CVec3( Float2Int( r.x ), Float2Int( r.y ), 0 );
	r *= FP_GRID_STEP;
	return CVec3( fabs( ptNormal.x ) > FP_EPSILON ? pt.x : r.x, fabs( ptNormal.y ) > FP_EPSILON ? pt.y : r.y, pt.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSpot::CSpot( ISelection *pSel, int nUserID )
	: pSpot(0)
{
	ASSERT( pSel );
	pSelection = pSel;
	nInitUserID = nUserID;
	bInitialized = false;
	SetActiveHandle( nUserID );
	pWorld = dynamic_cast<NWorld::CEditorWorld*>( pSelection->GetWorld() );
	onDelete.pWorld = pWorld;
	bMove = false;
	bDraw = false;
	bTerrAlign = false;
	bUseStartMove = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::IsInitialized() const
{
	if ( !bInitialized )
	{
		const_cast<NBuilding::SProjectedSpot*>( pSpot ) = GetSpot( nInitUserID );
		if ( !pSpot )
			return false;
		const_cast<NBuilding::SProjectedSpot&>( startSpot ) = *pSpot;
		const_cast<float&>( fRotation ) = pSpot->nRotation;
		const_cast<bool&>( bInitialized ) = true;
	}
	return pSpot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::CreateObjects()
{
	const CVec3 ptCSize( 0.25f, 0.25f, 0.25f );
	pmoCube->CreateCube( -ptCSize / 2, ptCSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NGScene::CPolyline* CreateLines( NGScene::IGameView *pScene, vector<CVec3> *pPoints, const SFBTransform &pos, const CVec3 &color )
{
	CPtr<::CMemObject> pLines = new ::CMemObject;
	for ( int i = 0; i < pPoints->size(); ++i )
	{
		CVec3 &pt = (*pPoints)[i];
		pos.forward.RotateHVector( &pt, pt );
	}
	return pScene->CreatePolyline( *pPoints, color );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::ProcessEvent( const string &str )
{
	if ( str == "update_spot" )
		bDraw = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::Draw( NGScene::IGameView *pScene, const NBuilding::SProjectedSpot &spot, bool bSelected )
{
	if ( bDraw && pScene )
		return;
	rNodes.clear();
	onDelete.pObjects.clear();
	pPLines.clear();
	if ( !pScene )
	{
		bDraw = false;
		return;
	}

	bDraw = true;
	const CVec3 ptO = spot.ptOrigin + 0.006f * spot.ptNormal;
	SFBTransform pos = MakeTransform( ptO );

	float fRotation = ToRadian( (float)spot.nRotation );
	float fCos = cos( fRotation ), fSin = sin( fRotation );
	CVec3 vU( spot.ptSize.x * fCos, spot.ptSize.x * fSin, 0 );
	CVec3 vV( -spot.ptSize.y * fSin, spot.ptSize.y * fCos, 0 );
	CVec3 ptX = CVec3( -vU.y, 0, -vU.x );
	CVec3 ptY = CVec3( -vV.y, 0, -vV.x );

	SHMatrix m;
	MakeMatrix( &m, VNULL3, spot.ptNormal );
	m.RotateHVector( &ptX, ptX );
	m.RotateHVector( &ptY, ptY );

	vector<CVec3> points;
	vector<EHandle> handles;
	points.push_back( VNULL3 );
	handles.push_back( NWysiwyg::CSpotSel::H_DL );
	points.push_back( ptX );
	handles.push_back( NWysiwyg::CSpotSel::H_DR );
	points.push_back( ptX + ptY );
	handles.push_back( NWysiwyg::CSpotSel::H_TR );
	points.push_back( ptY );
	handles.push_back( NWysiwyg::CSpotSel::H_TL );
	points.push_back( VNULL3 );

	CreateObjects();
	CVec4 color( 0.5f, 0.5f, 1.0f, 1 );

	ESpotType type;
	EBrushType nBrush;
	EHandle h;
	int nFragmentID, nSpot;
	GetFragmentID( nInitUserID, &nBrush, &nFragmentID );
	GetSpotID( nFragmentID, &type, &nSpot, &h );

	SFBTransform trCenter = pos * MakeTransform( 0.5f * (ptX + ptY) );
	SFBTransform posTerrain;
	if ( bTerrAlign )
	{
		bool bOldAlign = pWorld->SetTerrAlign( true );
		posTerrain = pWorld->GetTerrainTransform( trCenter );
		pos = pos * posTerrain;
		pWorld->SetTerrAlign( bOldAlign );
	}
	else
		posTerrain = pWorld->GetTerrainTransform( trCenter );

	for ( int i = 0; i < points.size() - 1; ++i )
	{
		int nUserID = NWysiwyg::MakeUserID( BT_TEXSPOT, NWysiwyg::MakeSpotID( type, nSpot, handles[i] ) );
		onDelete.pObjects.push_back( pWorld->CreateMemObject( pmoCube, pos * MakeTransform( points[i] ), color, nUserID, bTerrAlign ) );
		//rNodes.push_back( pScene->CreateMesh( pmoCube, color, pos * MakeTransform( points[i] ) ) );
	}
	if ( !bTerrAlign )
		pos = pos * pWorld->GetTerrainTransform( trCenter );
	color = CVec4( 1.0f, 1.0f, 0.3f, 1 );
	rNodes.push_back( pScene->CreateMesh( pmoCube, color, pos * MakeTransform( 0.5f * (ptX + ptY) ) ) );
	CVec3 ptColor = CVec3(0.3f, 1.0f, 0.7f);
	//
	pPLines.push_back( CreateLines( pScene, &points, pos, ptColor ) );

	points.clear();
	points.push_back( 0.5f * ptX );
	points.push_back( 0.5f * ptX + ptY );
	pPLines.push_back( CreateLines( pScene, &points, pos, ptColor ) );
	//
	points.clear();
	points.push_back( 0.5f * ptY );
	points.push_back( 0.5f * ptY + ptX );
	pPLines.push_back( CreateLines( pScene, &points, pos, ptColor ) );
	pWorld->UpdateAIMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSpot::SOnDelete::~SOnDelete()
{
	pObjects.clear();
	if ( IsValid( pWorld ) )
		pWorld->UpdateAIMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::Draw( NGScene::IGameView *pScene, bool bShow )
{
	if ( !IsInitialized() )
		return false;
	//Draw( bShow ? pScene : 0, *pSpot, true );
	Draw( pScene, *pSpot, true );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::IsEqual( CObjectBase *pObj, int nUserID ) const
{
	if ( dynamic_cast<NWorld::IVisObj*>( pObj ) )
		return GetSpot( nUserID ) == pSpot;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CSpot::ComputeStartMove( const SHMatrix &mforw, const NBuilding::SProjectedSpot *pSpot )
{
	CVec3 ptSide;

	switch ( handle )
	{
	case H_DL:
		ptSide = VNULL3;
		break;
	case H_DR:
		ptSide = CVec3( pSpot->ptSize.x, 0, 0 );
		break;
	case H_TR:
		ptSide = CVec3( pSpot->ptSize.x, pSpot->ptSize.y, 0 );
		break;
	case H_TL:
		ptSide = CVec3( 0, pSpot->ptSize.y, 0 );
		break;
	case H_C:
		ptSide = 0.5f * CVec3( pSpot->ptSize.x, pSpot->ptSize.y, 0 );
		break;
	}
	//SFBTransform tr = MakeTransform( VNULL3, pSpot->nRotation );
	//tr.forward.RotateHVector( &ptSide, ptSide );
	ptSide = CVec3( -ptSide.y, ptSide.z, -ptSide.x );
	mforw.RotateHVector( &ptSide, ptSide );
	return pSpot->ptOrigin + ptSide;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::StartMove( const CVec2 &ptCursor )
{
	bMove = true;

	SHMatrix m;
	MakeForwMatrix( &m, pSpot );
	ptStartMove = ComputeStartMove( m, pSpot );
	startSpot = *pSpot;
	ptRotatedShift = VNULL3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::MakeForwMatrix( SHMatrix *pRes, const NBuilding::SProjectedSpot *pSpot ) const
{
	SHMatrix mforw, mrot;
	CQuat rot( ToRadian( (float)pSpot->nRotation ), pSpot->ptNormal );
	Identity( &mrot );
	rot.DecompEulerMatrix( &mrot );
	MakeMatrix( &mforw, VNULL3, pSpot->ptNormal );
	*pRes = mrot * mforw;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	if ( !bMove )
		return false;
	bDraw = false;
	CRay r = pSelection->GetProjectiveRay( ptCursor );

	float fDen = pSpot->ptNormal * r.ptDir;
	if ( fabs( fDen ) < 1e-3 )
		return false;
	CVec3 ptTerr = VNULL3;
	SFBTransform pos = pWorld->GetTerrainTransform( pSpot->ptOrigin.x, pSpot->ptOrigin.y );
	pos.forward.RotateHVector( &ptTerr, ptTerr );
	//pos.forward.RotateHVector( &pSpot->ptOrigin, pSpot->ptOrigin );
	float fT = pSpot->ptNormal * ( (pSpot->ptOrigin + ptTerr) - r.ptOrigin ) / fDen;
	CVec3 ptCross = r.ptOrigin + fT * r.ptDir;
	ptCross -= ptTerr;
	if ( bUseStartMove )
	{
		ptCross = ptStartMove + ptMove;// + ptRotatedShift;
//		DebugTrace( " \tMove: %.2f %0.2f\n", ptMove.x, ptMove.y );
	}
	if ( bTileAlign )
		ptCross = NearestTile( ptCross, pSpot->ptNormal );
	//
	SHMatrix mforw, m;
	MakeForwMatrix( &mforw, pSpot );
	InvertMatrix( &m, mforw );

	CVec3 ptSize;
	m.RotateHVector( &ptSize, ptCross );
	float x = ptSize.x;
	ptSize.x = -ptSize.z;
	ptSize.y = -x;
	ptSize.z = 0;

	CVec3 dpt = ptCross - pSpot->ptOrigin;
	m.RotateHVector( &ptSize, dpt );

	switch ( handle )
	{
		case H_DL:
			{
				pSpot->ptOrigin = ptCross;
				pSpot->ptSize += CVec2( ptSize.z, ptSize.x );
			}
			break;
		case H_DR:
			{
				pSpot->ptSize = CVec2( -ptSize.z, pSpot->ptSize.y + ptSize.x );
				//pSpot->ptOrigin. += ptSize.z;
				CVec3 pt;
				mforw.RotateHVector( &pt, CVec3( ptSize.x, 0, 0 ) );
				pSpot->ptOrigin += pt;
			}
			break;
		case H_TR:
			{
				pSpot->ptSize = CVec2( -ptSize.z, -ptSize.x );
			}
			break;
		case H_TL:
			{
				pSpot->ptSize = CVec2( ptSize.z + pSpot->ptSize.x, -ptSize.x );
				CVec3 pt;
				mforw.RotateHVector( &pt, CVec3( 0, 0, ptSize.z ) );
				pSpot->ptOrigin += pt;
			}
			break;
		case H_C:
			{
				CVec3 ptCenter;
				mforw.RotateHVector( &ptCenter, CVec3( -0.5f * pSpot->ptSize.y, 0, -0.5f * pSpot->ptSize.x ) );
				//bool bSmooth = !((0x8000 & GetAsyncKeyState( VK_SHIFT )) && (0x8000 & GetAsyncKeyState( VK_CONTROL )));
				//bool bSmooth = !bTileAlign;
				//if ( !bSmooth )
				//	ptCross = NearestTile( ptCross );
				pSpot->ptOrigin = ptCross - ptCenter;
			}
			break;
	}
	//pos.backward.RotateHVector( &pSpot->ptOrigin, pSpot->ptOrigin );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::EndMove( bool bCancel )
{
	bMove = false;
	startSpot = *pSpot;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::Move( const CVec3 &ptMove )
{
	pSpot->ptOrigin += ptMove;
	bDraw = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpot::Rotate( float fDRotation )
{
	*pSpot = startSpot;
	fRotation = startSpot.nRotation + fDRotation;
//	if ( pSpot->nRotation == (int)fRotation )
//		return true;
	//
	CVec3 ptCenter;
	CVec3 ptHalfSize( 0.5f * pSpot->ptSize.x, 0.5f * pSpot->ptSize.y, 0 );
	CQuat rot( ToRadian( (float)pSpot->nRotation ), pSpot->ptNormal );
	SHMatrix pos;

	MakeMatrix( &pos, pSpot->ptOrigin, rot );
	pos.RotateHVector( &ptCenter, ptHalfSize );
	pSpot->nRotation = fRotation;

	pSpot->nRotation = fRotation;
	rot = CQuat( ToRadian( (float)pSpot->nRotation ), pSpot->ptNormal );
	MakeMatrix( &pos, ptCenter, rot );
	pos.RotateHVector( &pSpot->ptOrigin, -ptHalfSize );
	//ptRotatedShift = pSpot->ptOrigin - startSpot.ptOrigin;
	SHMatrix m;
	MakeForwMatrix( &m, pSpot );
	ptStartMove = ComputeStartMove( m, pSpot );
//	DebugTrace( "Center: %.2f %0.2f", ptStartMove.x, ptStartMove.y );

	bDraw = false;
	pSelection->BuildingUpdated();
	pSelection->WallSpotUpdated( nSpotID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound CSpot::GetBoundBox( const NBuilding::SProjectedSpot *pSpot ) const
{
	SBound b;
	if ( !IsInitialized() )
		return b;
	SHMatrix m;
	MakeForwMatrix( &m, pSpot );
	CVec3 ptCenter( 0.5f * pSpot->ptSize, 0 );
	ptCenter = CVec3( -ptCenter.y, ptCenter.z, -ptCenter.x );
	m.RotateHVector( &ptCenter, ptCenter );
	b.BoxExInit( ( pSpot->ptOrigin + ptCenter ), CVec3( 0.1f, 0.1f, 0.1f ) );
	return b;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound CSpot::GetBoundBox() const
{
	return GetBoundBox( &startSpot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound CSpot::GetMovingBoundBox() const
{
	return GetBoundBox( pSpot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::SetActiveHandle( int nUserID )
{
	ESpotType type;
	EBrushType nBrush;
	int nFragmentID;
	GetFragmentID( nUserID, &nBrush, &nFragmentID );
	GetSpotID( nFragmentID, &type, &nSpotID, &handle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CSpotSel
////////////////////////////////////////////////////////////////////////////////////////////////////
CSpotSel::CSpotSel( ISelection *pSel, CPtrFuncBase<NBuilding::CBuildInfo> *pBInfo, NBuilding::CBuildingGrid *_pGrid, int nUserID )
	: CSpot( pSel, nUserID ), pGrid(_pGrid)
{
	CDGPtr<CPtrFuncBase<NBuilding::CBuildInfo> > pBuilding = pBInfo;
	pBuilding.Refresh();
	pInfo = pBuilding->GetValue();
	SetActiveHandle( nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::SProjectedSpot* CSpotSel::GetSpot( int nUserID ) const
{
	if ( !pInfo )
		return 0;
	EBrushType nBrush;
	int nFragmentID;
	GetFragmentID( nUserID, &nBrush, &nFragmentID );
	ESpotType type;
	EHandle h;
	int nSpotID;
	NWysiwyg::GetSpotID( nFragmentID, &type, &nSpotID, &h );
	if ( type != ST_WALL )
		return 0;
	for ( int i = 0; i < pInfo->spots.size(); ++i )
		if ( pInfo->spots[i].nID == nSpotID )
			return &pInfo->spots[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpotSel::Delete()
{
	DelayedDelete();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpotSel::DelayedDelete()
{
	if ( !IsInitialized() )
		return false;
	for ( int i = 0; i < pInfo->spots.size(); ++i )
		if ( pInfo->spots[i].nID == nSpotID )
			pInfo->spots[i].nMaterialID = -1;
	pSelection->BuildingUpdated();
	pSelection->WallSpotUpdated( nSpotID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpotSel::OnLBDblClick()
{
	if ( !CSpot::GetSpot() )
		return;
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_WALLSPOT;
	msg.data = (DWORD)CSpot::GetSpot();

	GetUserSettingsSetup().SendMessage( msg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSpotSel::GetSpotID() const
{
	return CSpot::GetSpot()->nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern void FindPoints2Update( vector<NBuilding::SPoint3> *pParts, const NBuilding::SBuildFragment *pFr );
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateGrid( NBuilding::CBuildingGrid *pGrid, const vector<NBuilding::SBuildFragment> &frags, const vector<int> &inds )
{
	for ( int i = 0; i < inds.size(); ++i )
	{
		vector<NBuilding::SPoint3> parts;
		FindPoints2Update( &parts, &frags[inds[i]] );
		for ( int j = 0; j < parts.size(); ++j )
			pGrid->UpdatePart( parts[j] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateWallSpot( int nSpotID, NBuilding::CBuildInfo *pInfo, NBuilding::CBuildingGrid *pGrid )
{
	if ( pGrid )	
	{
		vector<int> solids, walls;

		pInfo->GetSpotFragments( nSpotID, &solids, &walls );
		UpdateGrid( pGrid, pInfo->solidFragments, solids );
		UpdateGrid( pGrid, pInfo->wallFragments, walls );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpotSel::EndMove( bool bCancel )
{
	UpdateWallSpot( GetSpotID(), pInfo, pGrid );
	pSelection->WallSpotUpdated( nSpotID );
	pSelection->BuildingUpdated();
	return CSpot::EndMove( bCancel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpotSel::RotateAround( float fRotation, const CVec2 &ptCenter )
{
	return Rotate( fRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpotSel::GetInfo( SSelectedInfo *pInfo )
{
	ASSERT(pInfo);
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_WALLSPOT;
	pInfo->nBrushID = CSpot::GetSpot()->nMaterialID;
	pInfo->nRotation =  CSpot::GetSpot()->nRotation;
	pInfo->ptPos = CSpot::GetSpot()->ptOrigin;
	pInfo->nObjectID = CSpot::GetSpot()->nID;
	pInfo->nFloor = 0;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
