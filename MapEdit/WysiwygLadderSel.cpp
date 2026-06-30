#include "StdAfx.h"
#include "WysiwygLadderSel.h"
#include "..\Main\Grid.h"
#include "..\MapEdit\UserSettingsSetup.h"
#include "iWysiwyg.h"
#include "WysiwygClipboard.h"
#include "..\Main\aiGrid.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
CLadderSel::CLadderSel( ISelection *pSelection, const CTPoint<int> &ptSize, int _nUserID, NBuilding::CBuildInfo *pInfo )
	: CMovingSelection( pSelection, ptSize ), nUserID( _nUserID ), pBInfo( pInfo ), pLadder(0)
{
	EBrushType type;
	int nID;

	GetFragmentID( nUserID, &type, &nID );
	if ( type = BT_LADDER )
	{
		for ( vector<NBuilding::SLadder>::iterator it = pBInfo->ladders.begin(); it != pBInfo->ladders.end(); ++it )
			if ( it->nID == nID )
			{
				pLadder = &(*it);
				break;
			}
	}
	EnableSnap( true );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::Move( const CVec3 &ptMove )
{
	if ( !IsInitialized() )
		return false;
	CVec3 move( ptMove.x, ptMove.y, 0 );
	pLadder->pos.ptMove += FP_INV_GRID_STEP * move;
	pLadder->pos.ptMove.x = Float2Int( pLadder->pos.ptMove.x );
	pLadder->pos.ptMove.y = Float2Int( pLadder->pos.ptMove.y );
	pLadder->fBeginHeight += ptMove.z;
	pLadder->fEndHeight += ptMove.z;
	if ( fabs( pLadder->fBeginHeight ) > NBuilding::WALL_HEIGHT )
	{
		int nDFloor = pLadder->fBeginHeight / NBuilding::WALL_HEIGHT;
		pLadder->fBeginHeight -= nDFloor * NBuilding::WALL_HEIGHT;
		pLadder->fEndHeight -= nDFloor * NBuilding::WALL_HEIGHT;
		pLadder->pos.ptMove.z += nDFloor;
	}
	//
	pLadder->pos.ptMove.x = Clamp( pLadder->pos.ptMove.x, 0.0f, (float)ptSize.x );
	pLadder->pos.ptMove.y = Clamp( pLadder->pos.ptMove.y, 0.0f, (float)ptSize.y );
	pSelection->BuildingUpdated();
	pSelection->LadderUpdated( pLadder->nID );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::IsInitialized() const
{
	return pLadder != 0 && pBInfo != 0;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::IsEqual(CObjectBase *pObj, int _nUserID ) const
{
	return nUserID == _nUserID;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::DelayedDelete()
{
	if ( !IsInitialized() )
		return false;
	for ( vector<NBuilding::SLadder>::iterator it = pBInfo->ladders.begin(); it != pBInfo->ladders.end(); ++it )
		if ( it->nID == pLadder->nID )
		{
			pBInfo->ladders.erase( it );
			pSelection->BuildingUpdated();
			pSelection->LadderUpdated( pLadder->nID );
			return true;
		}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CLadderSel::GetBoundBox() const
{
	if ( !IsInitialized() )
		return SBound();
	SBound b;
	float h = pLadder->nHeight * NAI::F_LADDER_STEP;
	CVec3 pos( pLadder->pos.ptMove.x, pLadder->pos.ptMove.y - 0.5f, 0 );
	pos *= FP_GRID_STEP;
	pos.z = pLadder->pos.ptMove.z * NBuilding::WALL_HEIGHT;

	SDiscretePos dpos( 0, FP_GRID_STEP * CVec3( pLadder->pos.ptMove.x, pLadder->pos.ptMove.y, 0 ), pLadder->pos.nRotation );

	CVec3 ptMin( 0, 0.5f * FP_GRID_STEP, pos.z );
	CVec3 ptMax( 0.5f * FP_GRID_STEP, -0.5f * FP_GRID_STEP, pos.z + h );
	dpos.MoveAndRotate( &ptMin );
	dpos.MoveAndRotate( &ptMax );

	b.BoxInit( ptMin, ptMax );
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
void CLadderSel::OnLBDblClick()
{
	SMessage msg;

	msg.msg = MSG_EDIT;
	msg.brush = BT_LADDER;
	msg.data = (DWORD)pLadder;

	GetUserSettingsSetup().SendMessage( msg );
}
/////////////////////////////////////////////////////////////////////////////////////
void CLadderSel::OnCopy()
{
	if ( IsInitialized() )
		AddLadderToClipboardBuffer( *pLadder );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	CVec3 ptLocal = ptMove;
	if ( true )
	{
		CVec3 pt = CVec3( pLadder->pos.ptMove.x, pLadder->pos.ptMove.y, 0 ) + FP_INV_GRID_STEP * ptMove;
		ptLocal.x = FP_GRID_STEP * (Float2Int( pt.x ) - pLadder->pos.ptMove.x);
		ptLocal.y = FP_GRID_STEP * (Float2Int( pt.y ) - pLadder->pos.ptMove.y);
	}
	return CMovingSelection::TrackMove( bTileAlign, ptLocal, ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsInitialized() )
		return false;
	pInfo->eBrushType = BT_LADDER;
	pInfo->nBrushID = -1;
	pInfo->nRotation =  pLadder->pos.nRotation;
	pInfo->ptPos = CVec3( pLadder->pos.ptMove.x, pLadder->pos.ptMove.y, pLadder->fBeginHeight );
	pInfo->nObjectID = pLadder->nID;
	pInfo->nFloor = pLadder->pos.ptMove.z;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::Rotate( float fDRotation )
{
	int nRotation = Float2Int( fDRotation ) - Float2Int( fDRotation ) % 90;
	if ( nRotation < 0 )
		nRotation = 360 + nRotation;
	int nAng = RotationIDToAngle( rollback.pos.nRotation );
	nAng += nRotation;
	pLadder->pos.nRotation = AngleToRotationID( nAng );
	pSelection->BuildingUpdated();
	pSelection->LadderUpdated( pLadder->nID );
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::Draw( NGScene::IGameView *pScene, bool bShow )
{
	return CMovingSelection::InternalDraw( bShow ? pScene : 0 );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CLadderSel::IsEqual( int nID ) const 
{ 
	if ( pLadder ) 
		return pLadder->nID == nID; 
	return false; 
}
/////////////////////////////////////////////////////////////////////////////////////
int CLadderSel::GetSelectionID() const 
{ 
	if ( pLadder ) 
		return pLadder->nID;
	return -1;
}
/////////////////////////////////////////////////////////////////////////////////////
void CLadderSel::StartMove( const CVec2 &ptCursor )
{
	rollback = *pLadder;
	CMovingSelection::StartMove( ptCursor );
}
/////////////////////////////////////////////////////////////////////////////////////
CVec2 CLadderSel::GetCenter() const
{
	SBound b = GetBoundBox();
	return CVec2( b.s.ptCenter.x, b.s.ptCenter.y );
}
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
