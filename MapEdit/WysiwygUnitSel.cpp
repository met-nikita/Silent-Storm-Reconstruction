#include "StdAfx.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
#include "WysiwygUnitSel.h"
#include "..\Main\Grid.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\Transform.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "..\MapEdit\UnitDB.h"
#include "iWysiwyg.h"
#include "WysiwygClipboard.h"
#include "weInterface.h"
#include "WysiwygUndo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
CUnitPosDB db;
/////////////////////////////////////////////////////////////////////////////////////
CUnitSel::CUnitSel( NDb::CUnit *_pUnit, ISelection *pSelection, const CTPoint<int> &ptSize )
	: CMovingSelection( pSelection, ptSize ), pUnit(_pUnit)
{
	pRollback = new NDb::CUnit;
	if ( IsInitialized() )
		*pRollback = *pUnit;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::Move( const CVec3 &ptMove )
{
	if ( !IsInitialized() || !(fabs( ptMove ) > FP_EPSILON || fabs( fStartRotation - pUnit->fRotation ) > FP_EPSILON ) )
		return false;
	CVec2 move2( ptMove.x, ptMove.y );
	move2 *= FP_INV_GRID_STEP;
	CTPoint<int> pt = pRollback->ptPos + CTPoint<int>( Float2Int( move2.x ), Float2Int( move2.y ) );
	int nFloor = Min( 4.0f, pRollback->nFloor + ptMove.z / NBuilding::WALL_HEIGHT );
	//
	pt.x = Clamp( pt.x, 0, ptSize.x );
	pt.y = Clamp( pt.y, 0, ptSize.y );
	if ( !db.SetPos( pUnit->GetRecordID(), pt.x, pt.y, nFloor, pUnit->fRotation  ) )
		return false;
	pUnit->ptPos = pt;
	pUnit->nFloor = nFloor;
	pSelection->UnitUpdated( pUnit->GetRecordID() );
	NMapEditor::PushUndoCmd( CreateUnitUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pUnit ) );
	*pRollback = *pUnit;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::IsInitialized() const
{
	if ( IsValid( pUnit ) )
		return true;
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::IsEqual(CObjectBase *pObj, int nUserID ) const
{
	CDynamicCast<NWorld::IEditorUnit> pU(pObj);
	if (pU)
	{
		NDb::CUnit *p = pU->GetDBUnit();
		if ( p && p->GetRecordID() == pUnit->GetRecordID() )
			return true;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::DelayedDelete()
{
	CObj<CWysiwygUndo> pUndo = CreateUnitUndo( CWysiwygUndo::UA_DELETE, pUnit, 0 );
	if ( db.Delete( pUnit->GetRecordID() ) )
	{
		NDb::CTemplVariant *pVar = pUnit->pVariant;
		if ( IsValid( pVar ) )
			for ( int i = 0; i < pVar->pUnits.size(); ++i )
			{
				if ( IsValid( pVar->pUnits[i] ) && pVar->pUnits[i]->GetRecordID() == pUnit->GetRecordID() )
					pVar->pUnits[i] = 0;
			}
		pUnit->pVariant = 0;
		pSelection->UnitUpdated( pUnit->GetRecordID() );
		NMapEditor::PushUndoCmd( pUndo );
		return true;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
CVec3 CUnitSel::GetPos() const
{
	CVec3 pt( pRollback->ptPos.x, pRollback->ptPos.y, 0 );
	pt *= FP_GRID_STEP;
	pt.z = pRollback->nFloor * NBuilding::WALL_HEIGHT;
	return pt;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CUnitSel::GetBoundBox() const
{
	if ( !IsInitialized() )
		return SBound();
	SBound b;	
	b.BoxExInit( GetPos() + CVec3( 0, 0, 0.9f ), CVec3( 0.4f * FP_GRID_STEP, 0.4f * FP_GRID_STEP, 0.9f ) );
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
void CUnitSel::OnLBDblClick()
{
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_UNIT;
	msg.data = (DWORD)pUnit->GetRecordID();

	GetUserSettingsSetup().SendMessage( msg );
}
/////////////////////////////////////////////////////////////////////////////////////
void CUnitSel::OnCopy()
{
	if ( IsInitialized() )
		AddUnitToClipboardBuffer( pUnit->GetRecordID() );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	CVec3 ptLocal = ptMove;
	CVec3 pt = CVec3( pRollback->ptPos.x, pRollback->ptPos.y, 0 ) + FP_INV_GRID_STEP * ptMove;
	ptLocal.x = FP_GRID_STEP * (Float2Int( pt.x ) - pRollback->ptPos.x);
	ptLocal.y = FP_GRID_STEP * (Float2Int( pt.y ) - pRollback->ptPos.y);

	{
		CVec3 move2 = ptLocal * FP_INV_GRID_STEP;
		CTPoint<int> pt = pRollback->ptPos + CTPoint<int>( Float2Int( move2.x ), Float2Int( move2.y ) );
		int nFloor = Min( 4.0f, pRollback->nFloor + ptMove.z / NBuilding::WALL_HEIGHT );
		//
		pt.x = Clamp( pt.x, 0, ptSize.x );
		pt.y = Clamp( pt.y, 0, ptSize.y );
		pUnit->ptPos = pt;
		pUnit->nFloor = nFloor;
		pSelection->UnitUpdated( pUnit->GetRecordID() );
	}

	return CMovingSelection::TrackMove( bTileAlign, ptLocal, ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_UNIT;
	pInfo->nBrushID = pUnit->pMonster->GetRecordID();
	pInfo->nRotation =  pUnit->fRotation;
	pInfo->ptPos = CVec3( pUnit->ptPos.x, pUnit->ptPos.y, 0 );
	pInfo->nObjectID = pUnit->GetRecordID();
	pInfo->nFloor = pUnit->nFloor;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::Rotate( float fDRotation )
{
	fCurrentRotation = fDRotation;
	float fNextRotation = pRollback->fRotation + fDRotation;
	if ( IsDiscreteRotation() )
		fNextRotation = 45 * int( (fStartRotation + fCurrentRotation) / 45 );

//	if ( !db.SetPos( pUnit->GetRecordID(), pUnit->ptPos.x, pUnit->ptPos.y, pUnit->nFloor, fNextRotation ) )
//		return false;
	pUnit->fRotation = fNextRotation;
	pSelection->UnitUpdated( pUnit->GetRecordID() );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CUnitSel::Draw( NGScene::IGameView *pScene, bool bShow )
{
	return CMovingSelection::InternalDraw( bShow ? pScene : 0 );
}
/////////////////////////////////////////////////////////////////////////////////////
void CUnitSel::StartMove( const CVec2 &ptCursor )
{
	fCurrentRotation = 0;
	fStartRotation = pUnit->fRotation;
	*pRollback = *pUnit;
	CMovingSelection::StartMove( ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
void CUnitSel::Cancel()
{
	if ( fabs( fCurrentRotation ) > FP_EPSILON )
		if ( db.SetPos( pUnit->GetRecordID(), pUnit->ptPos.x, pUnit->ptPos.x, pUnit->nFloor, fStartRotation ) )
		{
			pUnit->fRotation = fStartRotation;
			pSelection->UnitUpdated( pUnit->GetRecordID() );
		}
}
/////////////////////////////////////////////////////////////////////////////////////
CVec2 CUnitSel::GetCenter() const
{
	SBound b = GetBoundBox();
	return CVec2( b.s.ptCenter.x, b.s.ptCenter.y );
}
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
