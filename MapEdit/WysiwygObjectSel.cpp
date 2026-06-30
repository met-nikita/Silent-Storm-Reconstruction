#include "StdAfx.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataObject.h"
#include "WysiwygObjectSel.h"
#include "weInterface.h"
#include "..\Main\Grid.h"
#include "..\Main\aiObjectLoader.h"
#include "..\MapEdit\FinDBCmd.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\Transform.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "iWysiwyg.h"
#include "WysiwygClipboard.h"
#include "..\Misc\BasicShare.h"
#include "WysiwygUndo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	extern CBasicShare<int, NAI::CLoadGeometryInfo> shareAIModel;
}
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
CFinPosDB db;
/////////////////////////////////////////////////////////////////////////////////////
	CObjectSel::CObjectSel( NDb::CFinalElement *_pFin, NDb::CModel *_pObj, ISelection *pSelection, const CTPoint<int> &ptSize )
	: CMovingSelection( pSelection, ptSize ), pFin(_pFin), pModel( _pObj )
{
	if ( IsValid( pModel ) && IsValid( pModel->pGeometry ) )
	{
		NDb::CGeometry *pGeom = pModel->pGeometry;
		if ( IsValid( pGeom->pAIGeometry ) )
		{
			CDGPtr<CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( pGeom->pAIGeometry->GetRecordID() );
			pSrc.Refresh();
			pGeomInfo = pSrc->GetValue();
			if ( IsValid( pGeomInfo ) )
				pGeomInfo->CalcBound();
		}
	}
	bbox = ComputeBoundBox();
	pRollback = new NDb::CFinalElement;
	if ( IsInitialized() )
		*pRollback = *pFin;
}
/////////////////////////////////////////////////////////////////////////////////////
static float GetDistance( NDb::CFinalElement *a, NDb::CFinalElement *b )
{
	float d = fabs( a->ptPos - b->ptPos );
	d += fabs( a->fRotation - b->fRotation );
	d += fabs( float(a->nFloor - b->nFloor) );
	d += fabs( a->fDZ - b->fDZ );

	return d;
}
bool CObjectSel::Move( const CVec3 &ptMove )
{
	if ( !IsInitialized() || GetDistance( pFin, pRollback ) < FP_EPSILON )
		return false;
	
	if ( !db.SetPos( pFin->GetRecordID(), pFin->ptPos, pFin->fDZ, pFin->nFloor, pFin->fRotation ) )
	{
		Cancel();
		return false;
	}
	else
	{
		NMapEditor::PushUndoCmd( CreateObjSelectionUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pFin ) );
	}
	bbox = ComputeBoundBox();
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::IsInitialized() const
{
	if ( IsValid( pFin ) )
		return true;
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::IsEqual(CObjectBase *pObj, int nUserID ) const
{
	CDynamicCast<NWorld::IEditorObject> pM(pObj);
	if (pM)
	{
		NDb::CFinalElement *p = pM->GetDBElement();
		if ( p && p->GetRecordID() == pFin->GetRecordID() )
			return true;
	}
	EBrushType type;
	int n;
	GetFragmentID( nUserID, &type, &n );
	if ( BT_OBJECT == type && pFin->GetRecordID() == n )
		return true;
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::DelayedDelete()
{
	CObj<CWysiwygUndo> pUndo = CreateObjSelectionUndo( CWysiwygUndo::UA_DELETE, pFin, 0 );
	bool bRet = db.Delete( pFin->GetRecordID() );
	if ( bRet && IsValid( pFin->pVariant ) )
	{
		vector< CPtr<NDb::CFinalElement> > &fins = pFin->pVariant->pFinalElements;
		for ( int i = 0; i < fins.size(); ++i )
			if ( IsValid( fins[i] ) && fins[i]->GetRecordID() == pFin->GetRecordID() )
			{
				fins[i] = 0;
				pFin->pVariant = 0;
				NMapEditor::PushUndoCmd( pUndo );
				break;
			}
		pSelection->ObjectUpdated( pFin->GetRecordID() );
	}
	return bRet;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CObjectSel::ComputeBoundBox() const
{
	if ( !IsInitialized() )
		return SBound();
	SBound b;
	if ( IsValid( pGeomInfo ) )
		b = pGeomInfo->bound;
	else
	{
		float h = 0.5f * FP_GRID_STEP;
		b.BoxInit( CVec3(-h,-h,-h), CVec3(h,h,h) );
	}
	SFBTransform tr;
	MakeMatrix( &tr, pFin->ptScale, VNULL3, ToRadian( pFin->fRotation ) );
	tr.forward.RotateHVector( &b.s.ptCenter, b.s.ptCenter );
	vector<CVec3> boxrotate( 4 );
	tr.forward.RotateHVector( &boxrotate[0], b.ptHalfBox );
	tr.forward.RotateHVector( &boxrotate[1], -b.ptHalfBox );
	tr.forward.RotateHVector( &boxrotate[2], CVec3( b.ptHalfBox.x, -b.ptHalfBox.y, b.ptHalfBox.z ) );
	tr.forward.RotateHVector( &boxrotate[3], CVec3( -b.ptHalfBox.x, b.ptHalfBox.y, b.ptHalfBox.z ) );
	CVec3 ptHalf = VNULL3;
	for ( int i = 0; i < 4; ++i )
	{
		ptHalf.x = Max( ptHalf.x, boxrotate[i].x );
		ptHalf.y = Max( ptHalf.y, boxrotate[i].y );
		ptHalf.z = Max( ptHalf.z, boxrotate[i].z );
	}
	//ptHalf.z = b.ptHalfBox.z;
	//b.s.ptCenter.z = ptHalf.z;
	b.s.ptCenter += CVec3( FP_GRID_STEP * pFin->ptPos, pFin->fDZ + pFin->nFloor * NBuilding::WALL_HEIGHT );
	b.ptHalfBox = ptHalf;
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
void CObjectSel::OnLBDblClick()
{
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_OBJECT;
	msg.data = (DWORD)pFin->GetRecordID();

	//GetUserSettingsSetup().SendMessage( msg );
	pSelection->OpenObject( pFin->GetRecordID() );
}
/////////////////////////////////////////////////////////////////////////////////////
void CObjectSel::OnCopy()
{
	if ( IsInitialized() )
		AddObjectToClipboardBuffer( pFin->GetRecordID() );
}
/////////////////////////////////////////////////////////////////////////////////////
const float MAX_Z_DELTA = 1.26f;
const float MIN_Z_DELTA = -1.24f;
bool CObjectSel::TrackMove( bool bTileAlign, const CVec3 &ptMv, const CVec2 &ptCursor )
{
	CVec3 ptMove = ptMv;
	if ( bTileAlign )
	{
		CVec3 pt = CVec3( pRollback->ptPos, 0 ) + FP_INV_GRID_STEP * ptMv;
		ptMove.x = FP_GRID_STEP * ((int)pt.x - pRollback->ptPos.x);
		ptMove.y = FP_GRID_STEP * ((int)pt.y - pRollback->ptPos.y);
	}

	CVec2 move2( ptMove.x, ptMove.y );
	pFin->ptPos = pRollback->ptPos + FP_INV_GRID_STEP * move2;
	pFin->fDZ = pRollback->fDZ + ptMove.z;
	pFin->nFloor = pRollback->nFloor;
	// 
	SBound b = GetMovingBoundBox();
	int nBoundFloor = floor( b.s.ptCenter.z / NBuilding::WALL_HEIGHT );
	int nDFloor = nBoundFloor - pFin->nFloor;
	//
	/*
	int nDFloor = 0;
	if ( pFin->fDZ >= MAX_Z_DELTA )
		nDFloor = ceil( pFin->fDZ / NBuilding::WALL_HEIGHT );
	else if ( pFin->fDZ < MIN_Z_DELTA )
		nDFloor = floor( pFin->fDZ / NBuilding::WALL_HEIGHT );
	*/
	pFin->fDZ -= nDFloor * NBuilding::WALL_HEIGHT;
	pFin->nFloor = Min( 4, pRollback->nFloor + nDFloor );
	//
	if ( IsValid( pFin->pVariant ) && IsValid( pFin->pVariant->pTemplate ) )
	{
		pFin->ptPos.x = Clamp( pFin->ptPos.x, 0.0f, (float)pFin->pVariant->pTemplate->nWidth );
		pFin->ptPos.y = Clamp( pFin->ptPos.y, 0.0f, (float)pFin->pVariant->pTemplate->nHeight );
	}
	pSelection->ObjectUpdated( pFin->GetRecordID() );

	return CMovingSelection::TrackMove( bTileAlign, ptMove, ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_OBJECT;
	pInfo->nBrushID = pFin->pObject->GetRecordID();
	pInfo->nRotation =  pFin->fRotation;
	pInfo->ptPos = CVec3( pFin->ptPos, pFin->fDZ );
	pInfo->nObjectID = pFin->GetRecordID();
	pInfo->nFloor = pFin->nFloor;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::Rotate( float fDRotation )
{
	fCurrentRotation = fDRotation;
	float fNextRotation = pRollback->fRotation + fDRotation;
	if ( IsDiscreteRotation() )
		fNextRotation = 45 * int( (fStartRotation + fCurrentRotation) / 45 );
	//
	pFin->fRotation = fNextRotation;
	//bbox = ComputeBoundBox();
	pSelection->ObjectUpdated( pFin->GetRecordID() );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::Draw( NGScene::IGameView *pScene, bool bShow )
{
	return CMovingSelection::InternalDraw( bShow ? pScene : 0 );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CObjectSel::IsEqual( int nID ) const 
{ 
	if ( IsValid( pFin ) ) 
		return pFin->GetRecordID() == nID; 
	return false; 
}
/////////////////////////////////////////////////////////////////////////////////////
int CObjectSel::GetSelectionID() const 
{ 
	if ( IsValid( pFin ) ) 
		return pFin->GetRecordID(); 
	return -1;
}
/////////////////////////////////////////////////////////////////////////////////////
void CObjectSel::StartMove( const CVec2 &ptCursor )
{
	fCurrentRotation = 0;
	fStartRotation = pFin->fRotation;
	*pRollback = *pFin;
	CMovingSelection::StartMove( ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
void CObjectSel::Cancel()
{
	*pFin = *pRollback;
	pSelection->ObjectUpdated( pFin->GetRecordID() );
}
/////////////////////////////////////////////////////////////////////////////////////
CVec2 CObjectSel::GetCenter() const
{	
	return FP_GRID_STEP * CVec2( pRollback->ptPos.x, pRollback->ptPos.y );
}
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
