#ifndef __UNITOBJDRAW_H_
#define __UNITOBJDRAW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RectTrack.h"

const COLORREF UNIT_COLOR = RGB( 150, 0, 0 );
const COLORREF UNIT_SELCOLOR = RGB( 40, 120, 75 );
const COLORREF OBJ_COLOR = RGB( 100, 0, 150 );
const COLORREF OBJ_SELCOLOR = UNIT_SELCOLOR;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitTracker : public CMERectTracker
{
	struct SArrow
  {
    CPoint ptBase;
    CPoint ptDir;
    CPoint ptLeft;
    CPoint ptRight;
  };
  SArrow      dirArrow;
  SArrow      oldDir;     // используется при вращении

protected:
	virtual void AdjustRect( CPoint *pCenter );
	
public:
	CUnitTracker( ITemplateView *pView, CRectsLayer *pLayer, const CPoint &ptCenter, int nRotation, int nSpacing );

	virtual void Draw( CDC *pDC, int nThickness, bool bGrayed );
	virtual void Rotate( CDC *pDC, int nAng );
	virtual int HitTest( CPoint pt );
	virtual void GetBoundsRect( CRect *pRect );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjTracker : public CUnitTracker
{
	COLORREF crDraw;
	COLORREF crSelection;
protected:
	virtual void AdjustRect( CPoint *pCenter );
	
public:
	CObjTracker( ITemplateView *pView, CRectsLayer *pLayer, const CPoint &ptCenter, const CVec2 &ptSize, 
		int nRotation, int nSpacing, COLORREF crDraw = OBJ_COLOR );
	
	virtual void Draw( CDC *pDC, int nThickness, bool bGrayed );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UNITOBJDRAW_H_