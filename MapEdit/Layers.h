#ifndef __LAYERS_H_
#define __LAYERS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "LayerCtrl.h"
#include "2DEditor.h"
/*! \file Layers.h
\brief Определения слоев используемых в TemplateView.

Все слои наследуются от CLayerCtrl, переопределяется функция CLayerCtrl::BrowseBrush() 
вызываемая при выборе новой кисти для слоя.
\sa CLayerCtrl
*/

struct SResTree;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Базовый класс слоя, кисть в котором выбирается в CItemsMgr, и представляется струкутрой SBrush
class CBaseLayer : public CLayerCtrl
{
	const SResTree *pBrushes;		//!< указатель на менеджер кистей для данного слоя
protected:
	SBrush activeBrush;					//!< выбранная активная кисть
public:
	CBaseLayer( int nLayerType, int nLayerInd, CString szName, int nBrushesTreeID );

	virtual int  GetBrushID() const { return activeBrush.nItemID; }
	virtual void BrowseBrush();
	virtual void Reset();
	virtual bool CanDraw() const;
	SBrush GetBrush() const { return activeBrush; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой закраски тайлов ландшафта определенными типами
class CTilesLayer : public CBaseLayer
{
protected:
	bool bImageShift;
	C2DEditor editor;

	void Track( UINT nFlags, CPoint point, ITemplateView *pView );
	virtual afx_msg void OnVisible();
public:
	CTilesLayer();
	virtual void Paint( ITemplateView *pView, float fBrightness, bool bGrayed );
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual bool Export( const string &szExportDir, const string &szPrefix );

	void CreateImage( int nWidth, int nHeight );
	void SetImage( const CArray2D<BYTE> *pColors );
	void GetTiles( CArray2D<BYTE> *pTiles );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой закраски тайлов ландшафта определенными типами
class CSpotsLayer : public CBaseLayer
{
protected:
	virtual afx_msg void OnVisible();

public:
	CSpotsLayer( int nLayerInd );

	virtual void BrowseBrush();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __LAYERS_H_
