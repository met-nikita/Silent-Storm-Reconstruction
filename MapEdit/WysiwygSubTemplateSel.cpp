#include "StdAfx.h"
#include "WysiwygSubTemplateSel.h"
#include "iWysiwyg.h"
#include "..\DBFormat\DataMap.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\Transform.h"
#include "..\Main\Grid.h"
#include "..\Main\GView.h"
#include "WysiwygClipboard.h"
#include "..\MapEdit\RectsDBCmd.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "WysiwygUndo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	void ImportRectCoords( CVec2 *pptCenter, float fWidth, float fHeight, float fRotation );
}
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
static CRectPosDB db;
/////////////////////////////////////////////////////////////////////////////////////
CSubTemplateSel::CSubTemplateSel( ISelection *pSelection, const CTPoint<int> &ptSize, int _nUserID )
: CMovingSelection( pSelection, ptSize ), nUserID(_nUserID)
{
	EBrushType type;
	int nID;

	GetFragmentID( nUserID, &type, &nID );
	pRollback = new NDb::CRectangle;
	if ( BT_SUBTEMPLATE == type )
	{
		pRect = NDb::GetRectangle( nID );
		if ( pRect )
		{
			bbox = ComputeBBox( pRect );
			*pRollback = *pRect;
		}
	}
	else
	{
		ASSERT(0);
	}
	bDirty = true;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CSubTemplateSel::GetMovingBoundBox() const
{
	SBound b = GetBoundBox();
	b.s.ptCenter += ptCurrentMove;
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
inline float Dist( NDb::CRectangle *a, NDb::CRectangle *b )
{
	return fabs( a->ptCenter - b->ptCenter ) + fabs( a->fRotation - b->fRotation ) + fabs( a->fDZ - b->fDZ ) + fabs( (float)a->nFloor - b->nFloor );
}
/////////////////////////////////////////////////////////////////////////////////////
const float MAX_Z_DELTA = 1.26f;
const float MIN_Z_DELTA = -1.24f;
bool CSubTemplateSel::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	bDirty = true;
	CVec3 ptLocal = ptMove + CVec3( ptRotatedShift, 0 );
	if ( bTileAlign )
	{
		CVec3 ptC( FP_INV_GRID_STEP * pRollback->ptCenter, 0 );
		CVec3 pt = ptC + FP_INV_GRID_STEP * ptLocal;
		ptLocal.x = FP_GRID_STEP * ((int)pt.x - ptC.x);
		ptLocal.y = FP_GRID_STEP * ((int)pt.y - ptC.y);
	}
	ptCurrentMove = ptLocal;
	CVec2 move2( ptLocal.x, ptLocal.y );
	float fDZ = pRollback->fDZ + ptLocal.z;
	//
	int nDFloor = 0;
	if ( fDZ >= MAX_Z_DELTA )
		nDFloor = ceil( fDZ / NBuilding::WALL_HEIGHT );
	else if ( fDZ < MIN_Z_DELTA )
		nDFloor = floor( fDZ / NBuilding::WALL_HEIGHT );
	fDZ -= nDFloor * NBuilding::WALL_HEIGHT;
	//
	pRect->ptCenter = pRollback->ptCenter + move2;
	pRect->fDZ = fDZ;
	pRect->nFloor = Min( 4, pRollback->nFloor + nDFloor );
	if ( Dist( pRect, pRollback ) > FP_EPSILON )
		pSelection->SubTemplateUpdated( nUserID );

	return CMovingSelection::TrackMove( bTileAlign, ptLocal, ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::Move( const CVec3 &ptMove )
{
	bDirty = true;
	if ( !IsInitialized() || Dist( pRollback, pRect ) < FP_EPSILON )
		return false;

	if ( db.SetPos( pRect->GetRecordID(), pRect->ptCenter, pRect->fDZ, pRect->nFloor, pRect->fRotation ) )
	{
		pSelection->SubTemplateUpdated( nUserID );
		bbox = ComputeBBox( pRect );
		NMapEditor::PushUndoCmd( CreateSubTemplateUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pRect ) );
		return true;
	}
	else
	{
		*pRect = *pRollback;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::IsInitialized() const
{
	return IsValid( pRect );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::IsEqual(CObjectBase *pObj, int _nUserID ) const
{
	return _nUserID == nUserID;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::DelayedDelete()
{
	CObj<CWysiwygUndo> pUndo = CreateSubTemplateUndo( CWysiwygUndo::UA_DELETE, pRect, 0 );
	if ( db.Delete( pRect->GetRecordID() ) )
	{
		NDb::CTemplVariant *pVar = pRect->pVariant;
		if ( IsValid( pVar ) )
			for ( int i = 0; i < pVar->rects.size(); ++i )
			{
				if ( IsValid( pVar->rects[i] ) && pVar->rects[i]->GetRecordID() == pRect->GetRecordID() )
				{
					pVar->rects[i] = 0;
					NMapEditor::PushUndoCmd( pUndo );
					break;
				}
			}
		pSelection->SubTemplateUpdated( nUserID );
		return true;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CSubTemplateSel::ComputeBBox( NDb::CRectangle *pRect )
{
	SBound b;

	CVec3 pt( pRect->ptCenter.x, pRect->ptCenter.y, 0 );
	pt.z = pRect->nFloor * NBuilding::WALL_HEIGHT + pRect->fDZ + NWysiwyg::SUBTEMPL_DHEIGHT;
	SFBTransform tr = MakeTransform( pt, pRect->fRotation );

	CVec3 ptSize( FP_GRID_STEP * pRect->fWidth, FP_GRID_STEP * pRect->fHeight, 0 );
	CVec3 ptO;
	tr.forward.RotateHVector( &ptO, VNULL3 );
	CVec3 ptMin = ptO, ptMax = ptO ;
	ptMax.z = ptMin.z + NBuilding::WALL_HEIGHT;

	vector<CVec3> points;
	points.push_back( CVec3( ptSize.x, 0, 0 ) );
	points.push_back( ptSize );
	points.push_back( CVec3( 0, ptSize.y, 0 ) );
	for ( int i = 0; i < points.size(); ++i )
	{
		tr.forward.RotateHVector( &ptO, points[i] );
		ptMin.x = Min( ptMin.x, ptO.x );
		ptMin.y = Min( ptMin.y, ptO.y );
		ptMax.x = Max( ptMax.x, ptO.x );
		ptMax.y = Max( ptMax.y, ptO.y );
	}

	b.BoxInit( ptMin, ptMax );
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CSubTemplateSel::GetBoundBox() const
{
	return bbox;
}
/////////////////////////////////////////////////////////////////////////////////////
void CSubTemplateSel::OnLBDblClick()
{
	if ( !IsValid( pRect->pTemplate ) )
		return;

	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_SUBTEMPLATE;
	msg.data = (DWORD)pRect->pTemplate->GetRecordID();

	GetUserSettingsSetup().SendMessage( msg );
}
/////////////////////////////////////////////////////////////////////////////////////
void CSubTemplateSel::OnCopy()
{
	if ( IsInitialized() )
		AddSubTemplateToClipboardBuffer( pRect->GetRecordID() );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_SUBTEMPLATE;
	pInfo->nObjectID = pRect->GetRecordID();
	pInfo->nRotation =  pRect->fRotation;
	pInfo->ptPos = CVec3( pRect->ptCenter, pRect->fDZ );
	if ( IsValid( pRect->pTemplate ) )
		pInfo->nBrushID = pRect->pTemplate->GetRecordID();
	else
		pInfo->nBrushID = -1;
	pInfo->nFloor = pRect->nFloor;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::Rotate( float fDRotation )
{
	bDirty = true;

	//fCurrentRotation += fDRotation;
	fCurrentRotation = fDRotation;
	float fNextRotation = pRollback->fRotation + fDRotation;
	if ( IsDiscreteRotation() )
		fNextRotation = 45 * int( (fStartRotation + fCurrentRotation) / 45 );

	CVec2 pt1 = pRollback->ptCenter, pt2 = pRollback->ptCenter;
	NDb::ImportRectCoords( &pt1, pRect->fWidth, pRect->fHeight, pRollback->fRotation );
	NDb::ImportRectCoords( &pt2, pRect->fWidth, pRect->fHeight, fNextRotation );
	ptRotatedShift = -(pt1 - pt2);
	//CVec2 ptCenter = pRollback->ptCenter - FP_INV_GRID_STEP * (pt1 - pt2);

//	if ( db.SetPos( pRect->GetRecordID(), ptCenter, pRect->fDZ, pRect->nFloor, fNextRotation ) )
//	{
		pRect->fRotation = fNextRotation;
//		pRect->ptCenter = ptCenter;
		pSelection->SubTemplateUpdated( nUserID );
		//bbox = ComputeBBox( pRect );
		NMapEditor::PushUndoCmd( CreateSubTemplateUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pRect ) );
		return true;
	//}
	//return false;
}
/////////////////////////////////////////////////////////////////////////////////////
inline void TransformVec( vector<CVec3> *pVec, const SFBTransform &tr )
{
	for ( int i = 0; i < pVec->size(); ++i )
		tr.forward.RotateHVector( &(*pVec)[i], (*pVec)[i] );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::Draw( NGScene::IGameView *pScene, bool bShow )
{
	//return false;
	if ( !bDirty && bShow )
		return true;
	lines.clear();
	if ( !bShow )
	{
		bDirty = true;
		return true;
	}
	bDirty = false;
	//
	CVec3 pt( pRect->ptCenter.x, pRect->ptCenter.y, 0 );
	pt.z = pRect->nFloor * NBuilding::WALL_HEIGHT + pRect->fDZ + NWysiwyg::SUBTEMPL_DHEIGHT;
	//pt += GetCurrentMove();
	SFBTransform tr = MakeTransform( pt, pRect->fRotation );
	CVec3 ptC( pRect->fWidth, pRect->fHeight, 0 );
	tr.forward.RotateHVector( &ptC, 0.5f * FP_GRID_STEP * ptC );
	tr = tr * pSelection->GetTerrainTransform( ptC.x, ptC.y );

	CVec3 ptSize( FP_GRID_STEP * pRect->fWidth, FP_GRID_STEP * pRect->fHeight, 0 );
	float fH = 1.1f * NBuilding::WALL_HEIGHT;

	vector<CVec3> points;

	points.push_back( VNULL3 );
	points.push_back( CVec3( 0, 0, fH ) );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	points.clear();
	points.push_back( CVec3( ptSize.x, 0, 0 ) );
	points.push_back( CVec3( ptSize.x, 0, fH ) );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	points.clear();
	points.push_back( ptSize );
	points.push_back( ptSize + CVec3( 0, 0, fH ) );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	points.clear();
	points.push_back( CVec3( 0, ptSize.y, 0 ) );
	points.push_back( CVec3( 0, ptSize.y, fH ) );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	points.clear();
	points.push_back( VNULL3 );
	points.push_back( CVec3( ptSize.x, 0, 0 ) );
	points.push_back( ptSize );
	points.push_back( CVec3( 0, ptSize.y, 0 ) );
	points.push_back( VNULL3 );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	tr = MakeTransform( CVec3( 0, 0, fH ) );
	TransformVec( &points, tr );
	lines.push_back( pScene->CreatePolyline( points, crLines ) );

	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateSel::IsEqual( int nID ) const 
{ 
	if ( IsValid( pRect ) ) 
		return pRect->GetRecordID() == nID; 
	return false; 
}
/////////////////////////////////////////////////////////////////////////////////////
int CSubTemplateSel::GetSelectionID() const 
{ 
	if ( IsValid( pRect ) ) 
		return pRect->GetRecordID();
	return -1;
}
/////////////////////////////////////////////////////////////////////////////////////
void CSubTemplateSel::StartMove( const CVec2 &ptCursor )
{
	fCurrentRotation = 0;
	fStartRotation = pRect->fRotation;
	ptStartCenter = pRect->ptCenter;
	*pRollback = *pRect;
	ptRotatedShift = VNULL2;
	CMovingSelection::StartMove( ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
void CSubTemplateSel::Cancel()
{
	if ( Dist( pRollback, pRect ) > FP_EPSILON )
		if ( db.SetPos( pRect->GetRecordID(), pRollback->ptCenter, pRollback->fDZ, pRollback->nFloor, pRollback->fRotation ) )
		{
			*pRect = *pRollback;
			pSelection->SubTemplateUpdated( nUserID );
		}
}
/////////////////////////////////////////////////////////////////////////////////////
CVec2 CSubTemplateSel::GetCenter() const
{
	SBound b = GetBoundBox();
	return CVec2( b.s.ptCenter.x, b.s.ptCenter.y );
}
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////