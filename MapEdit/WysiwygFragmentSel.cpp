#include "StdAfx.h"
#include "WysiwygFragmentSel.h"
#include "..\Main\wInterface.h"
#include "..\Main\MapBuildingInfo.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\BuildingInfo.h"
#include "iWysiwyg.h"
#include "MEUserSettings.h"
#include "..\Main\Grid.h"
#include "..\Main\MemObject.h"
#include "WysiwygClipboard.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "..\DBFormat\DataMap.h"
#include "weInterface.h"
#include "..\Main\gView.h"
#include "..\Main\aiMap.h"
#include "WysiwygUndo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
inline int  GetFragmentIndex( int nFragmentID ) { return ~0x60000000 & nFragmentID; } // отрезаем 2 старших бита
/////////////////////////////////////////////////////////////////////////////////////
inline NBuilding::SBuildFragment* GetFragment( vector<NBuilding::SBuildFragment> &fragments, int nFragmentID )
{
	int i = GetFragmentIndex( nFragmentID );
	if ( i >= 0 && i < fragments.size() )
		return &fragments[i];
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////
NBuilding::SBuildFragment* GetFragment( NWorld::IBuilding *pObj, int nUserID )
{
	NDb::CTemplVariant *pVar = pObj->GetInfo().pVariant;
	if ( !pVar )
		return 0;
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( pVar->GetRecordID() );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pBInfo = pLoader->GetValue();
	if ( !pBInfo )
		return 0;
	return IsWall( nUserID ) ? GetFragment( pBInfo->wallFragments, nUserID ) : GetFragment( pBInfo->solidFragments, nUserID );
}
/////////////////////////////////////////////////////////////////////////////////////
CFragmentSel::CFragmentSel( ISelection *pSel, NWorld::IBuilding *pObj, int nUserID, NWorld::IEditorWorld *pW )
: pFragment(0), pWorld(pW)
{
	ASSERT( pSel );
	pSelection = pSel;
	bWall = IsWall( nUserID );
	nFragmentID = nUserID;

	if ( !IsValid( pObj->GetInfo().pVariant ) )
		return;
	pBuildingGrid = pObj->GetInfo().pGrid;
	pVar = pObj->GetInfo().pVariant;
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( pVar->GetRecordID() );
	pLoader.Refresh();
	pBInfo = pLoader->GetValue();
	if ( !pBInfo )
		return;
	pFragment = bWall ? GetFragment( pBInfo->wallFragments, nFragmentID ) : GetFragment( pBInfo->solidFragments, nFragmentID );
	if ( !pFragment )
		return;
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( pFragment->nConstructionPartID );
	if ( !IsValid( pTCP ) )
		return;
	SRand rand;
	pCP = pTCP->CreateConstructionPart( &rand );
	// т.к здания добавляются по кускам, то нужно будет обновить все куски в которых находится фрагмент
	FindPoints2Update();
	//
	ComputeBoundBox();
	ptCurrentMove = VNULL3;
	bDirty = true;
}
/////////////////////////////////////////////////////////////////////////////////////
inline CVec3 GetShift( NDb::CConstructionPart *p, int nRotationID, bool bSwapXY = false )
{
	if ( p->nSizeY == 0 )
		return VNULL3;
	//
	int sx = 2 * p->nSizeX;
	int sy = 2 * p->nSizeY;
	if ( bSwapXY )
		swap( sx, sy );
	CVec3 ptShift = VNULL3;
	switch ( nRotationID )
	{
		case SDiscretePos::TURN_90:	 ptShift = CVec3( sy, 0, 0 );	break;
		case SDiscretePos::TURN_180: ptShift = CVec3( sx, sy, 0 );	break;
		case SDiscretePos::TURN_270: ptShift = CVec3( 0, sx, 0 );	break;
	}
	return ptShift;
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::Update()
{
	FindPoints2Update();
	for ( int i = 0; i < parts2update.size(); ++i )
		pBuildingGrid->UpdatePart( parts2update[i] );
}
/////////////////////////////////////////////////////////////////////////////////////
void FindPoints2Update( vector<NBuilding::SPoint3> *pParts, const NBuilding::SBuildFragment *pFragment )
{
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( pFragment->nConstructionPartID );
	if ( !IsValid( pTCP ) )
		return;
	SRand rand;
	CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );

	int nSizeY = pCP->nSizeY == 0 ? 1 : pCP->nSizeY;
	CVec3 ptShift = VNULL3;
	switch ( pFragment->nRotationID )
	{
		case SDiscretePos::TURN_90:	 ptShift = CVec3( 2 * pCP->nSizeY, 0, 0 );	break;
		case SDiscretePos::TURN_180: ptShift = CVec3( 2 * pCP->nSizeX, 2 * pCP->nSizeY, 0 );	break;
		case SDiscretePos::TURN_270: ptShift = CVec3( 0, 2 * pCP->nSizeX, 0 );	break;
	}
	for ( int x = 0; x < pCP->nSizeX; ++x )
		for ( int y = 0; y < nSizeY; ++y )
			for ( int z = 0; z < pCP->nSizeZ; ++z )
			{
				CVec3 fpos( pFragment->ptPos );
				fpos.z = int(fpos.z);
				SDiscretePos dpos( 0, fpos, pFragment->nRotationID );
				CVec3 pos( x, y, z );
				pos.x *= 2;
				pos.y *= 2;
				dpos.MoveAndRotate( &pos );
				pos += ptShift;
				SDiscretePos dpos2( 0, pos, pFragment->nRotationID );
				CVec3 dpt(1, 0, 0);
				dpos2.MoveAndRotate( &dpt );
				pParts->push_back( NBuilding::SPoint3( dpt.x, dpt.y, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x + 1, dpt.y, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x, dpt.y + 1, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x - 1, dpt.y, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x, dpt.y - 1, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x + 1, dpt.y + 1, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x - 1, dpt.y + 1, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x - 1, dpt.y - 1, dpt.z * 4 + 1 ) );
				pParts->push_back( NBuilding::SPoint3( dpt.x + 1, dpt.y - 1, dpt.z * 4 + 1 ) );

			}
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::FindPoints2Update()
{
	NWysiwyg::FindPoints2Update( &parts2update, pFragment );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::GetInfo( SSelectedInfo *pInfo )
{
	ASSERT(pInfo);
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_GEOMETRY;
	pInfo->nBrushID = pFragment->nConstructionPartID;
	pInfo->nRotation =  RotationIDToAngle( pFragment->nRotationID );
	pInfo->ptPos = pFragment->ptPos;
	pInfo->nObjectID = nFragmentID;
	pInfo->nFloor = pFragment->ptPos.z;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::IsInitialized() const
{
	return IsValid( pCP );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::IsEqual( CObjectBase *pObj, int nUserID ) const
{
	NWorld::IBuilding *pB = dynamic_cast<NWorld::IBuilding*>( pObj );
	if ( !pB )
		return false;
	return nUserID == nFragmentID;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::DelayedDelete()
{
	Update();
	NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_DELETE, pVar->GetRecordID(), pFragment, 0 ) );
	int i = GetFragmentIndex( nFragmentID );
	if ( bWall )
		pBInfo->wallFragments[i].nConstructionPartID = -1;
	else
		pBInfo->solidFragments[i].nConstructionPartID = -1;
	if ( IsValid( pCP ) && !IsValid( pCP->pGeometry ) )
		pWorld->UpdateFakeSolids();
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::StartMove( const CVec2 &ptCursor )
{
	ptStartMove = pSelection->GetTileUnderPos( pSelection->GetProjectiveRay( ptCursor ), GetUserSettings().GetActiveFloor() );
	ptCurrentMove = VNULL3;
	frRollback = *pFragment;
}
/////////////////////////////////////////////////////////////////////////////////////
// Возвращает проверенное значение ptMove
// Все размеры в тайлах
CVec2 CheckTilePos( const SBound &box, const CVec2 &ptMove, const CVec2 &ptSize )
{
	CVec2 ret = ptMove * FP_INV_GRID_STEP;
	ret.x = Float2Int( ret.x );
	ret.y = Float2Int( ret.y );
	CVec3 ptFar = box.s.ptCenter + box.ptHalfBox;
	CVec3 ptNear = box.s.ptCenter - box.ptHalfBox;
	ptFar *= FP_INV_GRID_STEP;
	ptNear *= FP_INV_GRID_STEP;
	CVec2 pt( ptSize.x - Float2Int(ptFar.x), ptSize.y - Float2Int(ptFar.y)	);
	ret.x = Min( pt.x, ret.x );
	ret.y = Min( pt.y, ret.y );
	pt = CVec2( -Float2Int(ptNear.x), -Float2Int(ptNear.y)	);
	ret.x = Max( pt.x, ret.x );
	ret.y = Max( pt.y, ret.y );
	ret *= FP_GRID_STEP;
	return ret;
}
/////////////////////////////////////////////////////////////////////////////////////
CVec2 CheckPos( const SBound &box, const CVec2 &ptMove, const CVec2 &ptSize )
{
	CVec2 ret = ptMove;
	CVec3 ptEpsilon(FP_EPSILON, FP_EPSILON, FP_EPSILON);
	CVec3 ptFar = box.s.ptCenter + box.ptHalfBox - ptEpsilon;
	CVec3 ptNear = box.s.ptCenter - box.ptHalfBox +  ptEpsilon;
	CVec2 ptMax( FP_GRID_STEP * ptSize.x - ptFar.x, FP_GRID_STEP * ptSize.y - ptFar.y );
	CVec2 ptMin( -ptNear.x, -ptNear.y );
	//
	ret.x = Clamp( ret.x, ptMin.x, ptMax.x );
	ret.y = Clamp( ret.y, ptMin.y, ptMax.y );
	return ret;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	//CVec2 pt = GetTileUnderPos( pSelection->GetProjectiveRay( ptCursor ) );
	ptCurrentMove = CVec3( CheckTilePos( boundbox, CVec2( ptMove.x, ptMove.y ), CVec2( pVar->pTemplate->nWidth, pVar->pTemplate->nHeight ) ), ptMove.z );
	bDirty = true;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::EndMove( bool bCancel )
{
	bool bRet = false;
	ptCurrentMove.x = int( ptCurrentMove.x * FP_INV_GRID_STEP );
	ptCurrentMove.y = int( ptCurrentMove.y * FP_INV_GRID_STEP );
	ptCurrentMove.z = Min( 4, int(ptCurrentMove.z / NBuilding::WALL_HEIGHT) );

	if ( fabs2( ptCurrentMove ) > 1e-3 )
	{
		pFragment->ptPos += ptCurrentMove;
		pBInfo->nMinFloor = Min( pBInfo->nMinFloor, (int)pFragment->ptPos.z );
		pBInfo->nMaxFloor = Max( pBInfo->nMaxFloor, (int)(pFragment->ptPos.z + 0.5f) );
		Update();
		if ( !bCancel )
		{
			bRet = true;
			NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_CHANGE_POS, pVar->GetRecordID(), &frRollback, pFragment ) );
			if ( IsValid( pCP ) && !IsValid( pCP->pGeometry ) )
				pWorld->UpdateFakeSolids();
		}
		else
			pFragment->ptPos -= ptCurrentMove;
	}
	ComputeBoundBox();
	ptCurrentMove = VNULL3;
	bDirty = true;
	return bRet;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::Rotate( float fDRotation )
{
	bDirty = true;
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound ComputeBoundBox( SBound *pPrimary, NDb::CConstructionPart *pCP, int nRotationID, bool bSelection = true )
{
	SDiscretePos dpos( 0, VNULL3, nRotationID );
	CVec3 ptSize, ptPrimarySize = VNULL3, ptPrimaryOffset = VNULL3;
	const int bWall = pCP->nSizeY == 0;
	const float fYSize = bWall ? pCP->fThickness : pCP->nSizeY * 2 * FP_GRID_STEP; // nSize - в строительных тайлах

	if ( bWall )
	{
		// wall
		const float fXSize = bSelection ? 2 * FP_GRID_STEP : 2 * FP_GRID_STEP * pCP->nSizeX;
		ptSize = CVec3( fXSize, fYSize, NBuilding::WALL_HEIGHT );
	}
	else
	{
		const float fH = pCP->nSizeZ * NBuilding::WALL_HEIGHT;
		ptSize = CVec3( pCP->nSizeX * 2 * FP_GRID_STEP, fYSize, fH );
		CVec2 ptMin( 100, 100 ), ptMax( 0, 0 );
		for ( int i = 0; i < pCP->nSizeX; ++i )
			for ( int j = 0; j < pCP->nSizeY; ++j )
			{
				if ( NDb::CConstructionPart::IsPrimaryPart( pCP->nSubPartsMask, i, j ) )
				{
					ptMin.x = Min( ptMin.x, (float)i );
					ptMin.y = Min( ptMin.y, (float)j );
					ptMax.x = Max( ptMax.x, (float)i );
					ptMax.y = Max( ptMax.y, (float)j );
				}
			}
		ptMax -= ptMin;
		ptMax += CVec2( 1, 1 );
		ptPrimaryOffset = CVec3( ptMin.x * 2 * FP_GRID_STEP, ptMin.y * 2 * FP_GRID_STEP, 0 );
		ptPrimarySize = CVec3( ptMax.x * 2 * FP_GRID_STEP, ptMax.y * 2 * FP_GRID_STEP, fH );
		ptPrimarySize *= 0.5f;
	}
	//
	CVec3 ptOrig = bWall ? CVec3( 0, -0.5f * fYSize, 0 ) : VNULL3;
	dpos.MoveAndRotate( &ptSize );
	dpos.MoveAndRotate( &ptPrimarySize );
	dpos.MoveAndRotate( &ptPrimaryOffset );
	dpos.MoveAndRotate( &ptOrig );
	ptSize *= 0.5f;
	//
	CVec3 ptShift = GetShift( pCP, nRotationID );
	ptShift *= FP_GRID_STEP;
	//
	SBound bbox;
	bbox.BoxExInit( ptSize + ptOrig + ptShift, ptSize );
	bbox.ptHalfBox.x = fabs( bbox.ptHalfBox.x );
	bbox.ptHalfBox.y = fabs( bbox.ptHalfBox.y );
	bbox.ptHalfBox.z = fabs( bbox.ptHalfBox.z );
	if ( pPrimary )
	{
		pPrimary->BoxExInit( ptPrimarySize + ptOrig + ptPrimaryOffset + ptShift, ptPrimarySize );
		pPrimary->ptHalfBox.x = fabs( pPrimary->ptHalfBox.x );
		pPrimary->ptHalfBox.y = fabs( pPrimary->ptHalfBox.y );
		pPrimary->ptHalfBox.z = fabs( pPrimary->ptHalfBox.z );
	}
	return bbox;
}
/////////////////////////////////////////////////////////////////////////////////////
bool ComputeBoundBox( SBound *pBound, int nConstructionID, int nRotationID, bool bSelection )
{
	NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( nConstructionID );
	if ( !pTCP )
		return false;
	SRand rand;
	CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );
	if ( !IsValid( pCP ) )
		return false;
	*pBound = ComputeBoundBox( 0, pCP, nRotationID, bSelection );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::ComputeBoundBox()
{
	if ( !IsInitialized() )
		return;
	boundbox = NWysiwyg::ComputeBoundBox( &primarybox, pCP, pFragment->nRotationID );
	CVec3 pos( pFragment->ptPos.x * FP_GRID_STEP, pFragment->ptPos.y * FP_GRID_STEP, pFragment->ptPos.z * NBuilding::WALL_HEIGHT );
	boundbox.s.ptCenter += pos;
//	boundbox.Extend( 0.01f );
	primarybox.s.ptCenter += pos;
//	primarybox.Extend( 0.01f );
	pGBox = new CMemObject;
	pGPrimaryBox = new CMemObject;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CFragmentSel::GetMovingBoundBox() const
{
	SBound box = boundbox;
	box.s.ptCenter += CVec3( ptCurrentMove.x, ptCurrentMove.y, int(ptCurrentMove.z / NBuilding::WALL_HEIGHT) * NBuilding::WALL_HEIGHT );
	return box;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::GetMovingPrimaryBoundBox( SBound *pBox ) const
{
	*pBox = primarybox;
	pBox->s.ptCenter += CVec3( ptCurrentMove.x, ptCurrentMove.y, 0 );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::GetGBoundBox( CMemObject **pBox, CMemObject **pPrimary ) const
{
	ASSERT( IsValid( pGBox ) && IsValid( pGPrimaryBox ) );
	SBound b = GetMovingBoundBox();
	b.Extend( 0.01f );
	pGBox->CreateCube( b.s.ptCenter - b.ptHalfBox, b.ptHalfBox * 2, true );
	GetMovingPrimaryBoundBox( &b );
	b.Extend( 0.01f );
	pGPrimaryBox->CreateCube( b.s.ptCenter - b.ptHalfBox, b.ptHalfBox * 2, true );
	*pBox = pGBox; 
	*pPrimary = pGPrimaryBox;
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::OnCopy()
{
	if ( pFragment )
		AddFragmentToClipboardBuffer( *pFragment );
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::OnLBDblClick()
{
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_GEOMETRY;
	msg.data = (DWORD)pFragment;

	GetUserSettingsSetup().SendMessage( msg );
}
/////////////////////////////////////////////////////////////////////////////////////
void CFragmentSel::AddSpot( int nSpotID )
{
	pFragment->spots.push_back( nSpotID );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::RotateAround( float fRotation, const CVec2 &ptC )
{
	int nRotation = fRotation;
	nRotation = nRotation - (nRotation % 90);
	if ( nRotation < 0 )
		nRotation = 360 + nRotation;
	NBuilding::SBuildFragment fr = *pFragment;

	CVec2 ptCenter( ptC );
	ptCenter *= FP_INV_GRID_STEP;
	ptCenter = CVec2( Float2Int( ptCenter.x ), Float2Int( ptCenter.y ) );
	ptCenter *=FP_GRID_STEP;

	CVec2 pt( frRollback.ptPos.x, frRollback.ptPos.y );
	pt *= FP_GRID_STEP;
	CVec2 newPt = pt - ptCenter;
	RotatePt( &newPt, nRotation );
	newPt += ptCenter;
	pt = FP_INV_GRID_STEP * (newPt - pt);
	pFragment->ptPos.x = frRollback.ptPos.x + Float2Int( pt.x );
	pFragment->ptPos.y = frRollback.ptPos.y + Float2Int( pt.y );
	bool bSwap = frRollback.nRotationID == SDiscretePos::TURN_90 || frRollback.nRotationID == SDiscretePos::TURN_270;
	pFragment->ptPos -= GetShift( pCP, AngleToRotationID( nRotation ), bSwap );

	int nAng = RotationIDToAngle( frRollback.nRotationID );
	nAng += nRotation;
	pFragment->nRotationID = AngleToRotationID( nAng );
	Update();
	ComputeBoundBox();
	bDirty = true;
	NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_CHANGE_POS, pVar->GetRecordID(), &frRollback, pFragment ) );
	if ( IsValid( pCP ) && !IsValid( pCP->pGeometry ) )
		pWorld->UpdateFakeSolids();
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CFragmentSel::Draw( NGScene::IGameView *pScene, bool bShow )
{
	if ( !bDirty && bShow )
		return true;
	bDirty = false;
	pLines.clear();
	parts.clear();
	if ( !bShow )
	{
		bDirty = true;
		return true;
	}

	SBound box = GetMovingBoundBox();
	box.Extend( 0.01f );
	SFBTransform posTerrain = pWorld->GetTerrainTransform( box.s.ptCenter.x, box.s.ptCenter.y );
	DrawBox( pScene, box, crLines, &pLines, posTerrain );
	DrawBox( pScene, primarybox, 0.7f * crLines, &pLines, posTerrain );
	CMemObject *pBox, *pPrimaryBox;
	GetGBoundBox( &pBox, &pPrimaryBox );
	parts.push_back( pScene->CreateMesh( pPrimaryBox, CVec4( 0.5f, 0.5f, 0.7f, 0.2f ), posTerrain ) );
	parts.push_back( pScene->CreateMesh( pBox, CVec4( 0.5f, 0.5f, 0.3f, 0.2f ), posTerrain ) );

	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
