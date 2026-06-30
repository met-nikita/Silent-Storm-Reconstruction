#ifndef __WYSIWYGMOVINGSEL_H_
#define __WYSIWYGMOVINGSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\MemObject.h"
#include "WysiwygSelection.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
class CMovingSelection: public CSelectedObj
{
	CVec3 ptStartMove;
	CVec3 ptCurrentMove;
	bool  bSnap;
	CVec2 ptStartBBoxPos;
protected:
	CPtr<ISelection> pSelection;
	CTPoint<int> ptSize;
	list<CObj<NGScene::CPolyline> > lines;

	bool bDirty;
	bool InternalDraw( NGScene::IGameView *pScene );
	CVec3 GetCurrentMove() const { return ptCurrentMove; }
	void EnableSnap( bool bSnap );
	virtual CVec2 GetCenter() const = 0;

public:
	CMovingSelection() {}
	CMovingSelection( ISelection *pSelection, const CTPoint<int> &ptSize );

	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool EndMove( bool bCancel );
	virtual SBound GetMovingBoundBox() const;

	virtual bool Move( const CVec3 &ptMove ) = 0;
	virtual void Cancel() = 0;
	virtual bool RotateAround( float fRotation, const CVec2 &ptCenter );
};
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
#endif //__WYSIWYGMOVINGSEL_H_