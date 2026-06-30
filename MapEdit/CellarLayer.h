#ifndef __CELLARLAYER_H_
#define __CELLARLAYER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Layers.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой закраски тайлов ландшафта определенными типами
class CCellarLayer : public CTilesLayer
{
	void Track( UINT nFlags, CPoint point, ITemplateView *pView );
public:
	CCellarLayer();

	virtual void BrowseBrush();
	virtual void Reset();
	virtual void SetPlacement( CPlacement *pPlacement );

	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView );

	void GetCellar( CArray2D<bool> *pCellar );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __CELLARLAYER_H_
