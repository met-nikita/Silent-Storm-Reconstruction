#include "StdAfx.h"
#include "..\Main\BuildingInfo.h"
#include "WysiwygWaypointSel.h"
#include "..\Main\aiWaypoint.h"
#include "MEUserSettings.h"
#include "MESerialize.h"
#include "..\Misc\BasicShare.h"
#include "iWysiwyg.h"
#include "..\Main\Grid.h"
#include "..\MapEdit\dbDefs.h"
#include "..\MapEdit\ItemsMgr.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "..\DBFormat\DataMap.h"
#include "..\Input\Bind.h"

extern CBasicShare<int, NAI::CWaypointLoader> shareWaypoints;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CWaypoint::CWaypoint( int _nWaypointID, ISelection *_pSelection, NDb::CTemplVariant *_pVar )
{
	nWaypointID = _nWaypointID;
	pSelection = _pSelection;
	pVar = _pVar;
	ptCurrentMove = VNULL3;
	ComputeBoundBox();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypoint::ComputeBoundBox()
{
	CPtr<NAI::CWaypoint> pWP = GetWaypoint( nWaypointID );
	if ( IsValid( pWP ) )
	{
		CVec3 pt = FP_GRID_STEP * pWP->ptPos;
		pt.z = pWP->ptPos.z + pWP->nFloor * NBuilding::WALL_HEIGHT;

		boundbox.BoxExInit( pt, CVec3( 0.05f, 0.05f, 0.75f ) );
		boundbox.s.ptCenter.z += 0.75f;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypoint::CreateObjects()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CWaypoint* CWaypoint::GetWaypoint( int nID )
{
	CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( nID );
	if ( !IsValid( pWPLoader ) )
		return 0;
	pWPLoader.Refresh();
	NAI::CWaypoint *pWP = pWPLoader->GetValue();

	return pWP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::IsInitialized() const
{
	return nWaypointID != -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::IsEqual( CObjectBase *pObj, int nUserID ) const
{
	EBrushType bt;
	int nID;
	GetFragmentID( nUserID, &bt, &nID );
	return bt == BT_WAYPOINT && nID == nWaypointID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypoint::StartMove( const CVec2 &ptCursor )
{
	ptStartMove = CVec3( pSelection->GetTileUnderPos( pSelection->GetProjectiveRay( ptCursor ), GetUserSettings().GetActiveFloor() ), 0 );
	ptCurrentMove = VNULL3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	ptCurrentMove = ptMove;

	CPtr<NAI::CWaypoint> pWP = GetWaypoint( nWaypointID );
	if ( IsValid( pWP ) )
	{
		CVec3 pt = pWP->ptPos + FP_INV_GRID_STEP * ptMove;
		ptCurrentMove.x = FP_GRID_STEP * (Float2Int( pt.x ) - pWP->ptPos.x);
		ptCurrentMove.y = FP_GRID_STEP * (Float2Int( pt.y ) - pWP->ptPos.y);
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::EndMove( bool bCancel )
{
	bool bRet = false;
	if ( !bCancel && fabs2( ptCurrentMove ) > FP_EPSILON )
	{
		CPtr<NAI::CWaypoint> pWP = GetWaypoint( nWaypointID );
		if ( IsValid( pWP ) )
		{
			pWP->ptPos += CVec3( FP_INV_GRID_STEP * ptCurrentMove.x, FP_INV_GRID_STEP * ptCurrentMove.y, ptCurrentMove.z );
			SerializeWaypoint( pWP, nWaypointID );
			//NInput::PostEvent( "update" );
			pSelection->WaypointUpdated( nWaypointID );
			bRet = true;
		}
	}
	ptCurrentMove = VNULL3;
	ComputeBoundBox();
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::Rotate( float fDRotation )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::Draw( NGScene::IGameView *pScene, bool bShow )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound CWaypoint::GetBoundBox() const
{
	return boundbox;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SBound CWaypoint::GetMovingBoundBox() const
{
	SBound b = boundbox;
	b.s.ptCenter += ptCurrentMove;
	return b;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::DelayedDelete()
{
	CItemsMgr *pMgr = GetUserSettings().GetResourceManager( IDC_WAYPOINTS_TREE );
	if ( !pMgr )
		return false;
	pMgr->DeleteItem( -1, nWaypointID );
	DeleteWaypoint( nWaypointID );
	pSelection->WaypointUpdated( nWaypointID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::Delete() 
{
	return DelayedDelete();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypoint::OnLBDblClick()
{
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_WAYPOINT;
	msg.data = nWaypointID;

	GetUserSettingsSetup().SendMessage( msg );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CWaypoint::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsInitialized() )
		return false;
	CPtr<NAI::CWaypoint> pWP = GetWaypoint( nWaypointID );
	if ( !pWP )
		return false;
	pInfo->eBrushType = BT_WAYPOINT;
	pInfo->nBrushID = -1;
	pInfo->nRotation =  pWP->fRotation;
	pInfo->ptPos = pWP->ptPos;
	pInfo->nObjectID = nWaypointID;
	pInfo->nFloor = pWP->nFloor;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
