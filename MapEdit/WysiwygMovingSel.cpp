#include "StdAfx.h"
#include "WysiwygMovingSel.h"
#include "MEUserSettings.h"
#include "..\Main\Grid.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
CMovingSelection::CMovingSelection( ISelection *_pSelection, const CTPoint<int> &_ptSize )
{
	pSelection = _pSelection;
	ptSize = _ptSize;
	ptCurrentMove = VNULL3;
	ASSERT(pSelection);
	bDirty = true;
	bSnap  = false;
}
/////////////////////////////////////////////////////////////////////////////////////
void CMovingSelection::EnableSnap( bool _bSnap )
{
	bSnap = _bSnap;
}
/////////////////////////////////////////////////////////////////////////////////////
void CMovingSelection::StartMove( const CVec2 &ptCursor )
{
	SBound b = GetBoundBox();
	ptStartBBoxPos = CVec2( b.s.ptCenter.x, b.s.ptCenter.y );
	ptStartMove = CVec3( pSelection->GetTileUnderPos( pSelection->GetProjectiveRay( ptCursor ), GetUserSettings().GetActiveFloor() ), 0 );
	ptCurrentMove = VNULL3;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CMovingSelection::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	ptCurrentMove = ptMove;
	bDirty = true;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CMovingSelection::EndMove( bool bCancel )
{
	bool bRet = false;
	if ( !bCancel )
		bRet = Move( ptCurrentMove );
	if ( bCancel )
		Cancel();
	ptCurrentMove = VNULL3;
	bDirty = true;

	return bRet;
}
/////////////////////////////////////////////////////////////////////////////////////
SBound CMovingSelection::GetMovingBoundBox() const
{
	SBound b = GetBoundBox();
	b.s.ptCenter += CVec3( ptCurrentMove );
	return b;
}
/////////////////////////////////////////////////////////////////////////////////////
bool CMovingSelection::RotateAround( float fRotation, const CVec2 &ptC )
{
	CVec2 ptCenter( ptC );
	if ( bSnap )
	{
		ptCenter *= FP_INV_GRID_STEP;
		ptCenter = CVec2( Float2Int( ptCenter.x ), Float2Int( ptCenter.y ) );
		ptCenter *=FP_GRID_STEP;
	}
	SBound b = GetBoundBox();
	CVec2 pt( b.s.ptCenter.x, b.s.ptCenter.y );
	//pt = GetCenter();
	CVec2 newPt = pt - ptCenter;
	RotatePt( &newPt, fRotation );
	newPt += ptCenter;
	pt = newPt - ptStartBBoxPos;

	bDirty = true;
	return Rotate( fRotation ) && TrackMove( false, CVec3( pt.x, pt.y, 0 ), CVec2(0,0) );
}
/////////////////////////////////////////////////////////////////////////////////////
bool CMovingSelection::InternalDraw( NGScene::IGameView *pScene )
{
	if ( !bDirty && pScene )
		return true;
	lines.clear();
	if ( !pScene )
	{
		bDirty = true;
		return true;
	}

	SBound box = GetMovingBoundBox();
	box.Extend( 0.01f );
	SFBTransform posTerrain = pSelection->GetTerrainTransform( box.s.ptCenter.x, box.s.ptCenter.y );
	DrawBox( pScene, box, crLines, &lines, posTerrain );
	bDirty = false;
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////
}// namespace
/////////////////////////////////////////////////////////////////////////////////////
