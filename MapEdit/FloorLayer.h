#ifndef __FLOORLAYER_H__
#define __FLOORLAYER_H__

class CPlacement;
enum EFloorType;
#include "Floor.h"

#define DEF_FLOOR_COLOR		RGB( 200, 200, 200 )
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования перекрытий
class CFloorsLayer : public CBaseLayer
{
	const EFloorType floorType;
	const int nFloorLayer;
	unordered_map<int, DWORD> colorMap;
	unordered_map<int, CSize> sizeMap;
	vector<CFloorPlan::SCell> activeCells;
	CRect rOldBrush;
	HCURSOR hLayerCursor;
	
  const CFloorPlan* GetFloor() const;
  CFloorPlan* GetFloor();
	DWORD GetColor( int nFloorModelID ) const;
	void  DrawCells( CDC *pDC, int nModelID, const vector<CFloorPlan::SCell> &cells, ITemplateView *pView ) const;
	void  ClearSelection();
	void  ClearColors();  
	bool  TrackFloor( ITemplateView *pView );
	void  DrawBrush( const CPoint &pt, ITemplateView *pView );

protected:
	CPlacement *pPlacement;	
	
public:
  CFloorsLayer( EFloorType type, int nLayerID, CString szName, int nBrushesTreeID, int nFloorLayerID );
  ~CFloorsLayer();
	
	virtual void Paint( ITemplateView *pView, float fBrightness, bool bGrayed );
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual bool OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView );
	virtual void Reset();
	
  void  SetPlacement( CPlacement *pPl );
	CSize GetSize( int nFloorModelID ) const;
	void  SetActiveCells( const CRect &rect, ITemplateView *pView );
	void  Delete();
protected:
	virtual afx_msg void OnVisible();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FLOORLAYER_H__