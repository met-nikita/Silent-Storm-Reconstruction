#ifndef __WALLSLAYER_H__
#define __WALLSLAYER_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CPlacement;
class CWallsPlan;
struct SWall;

#define DEF_WALL_COLOR	RGB( 128, 64, 0 )
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования стен
class CWallsLayer : public CBaseLayer
{
  vector<CPoint> chain;
  CPlacement *pPlacement;
  CPoint ptLast;
  vector<const SWall*> activeWalls;
	unordered_map<int, DWORD> colorMap;
	int nWallWidth;
	CMenu  wallMenu;
	HCURSOR hLayerCursor;

  const CWallsPlan* GetFloor() const;
  CWallsPlan* GetFloor();
  bool  CrossNearestBorder( CPoint *pPt, int nLength );
  void  GetInternalWall( int *pX, int *pY, const CPoint &pt1, const CPoint &pt2, int nWidth ) const;
	COLORREF GetColor( int nWallModelID, bool bGrayed ) const;

  bool WallMove( const CPoint &pt, ITemplateView *pView );
  void WallEnd( const CPoint &pt, ITemplateView *pView );
  void DrawChain( CDC *pDC, ITemplateView *pView ) const;
  bool Track( ITemplateView *pView );
	void ClearColors();
	const SWall* SelectWall( const CPoint &pt, ITemplateView *pView );
  void SetActiveWall( const CPoint &pt, ITemplateView *pView );
  bool Flip();
  bool Delete();
	bool IsSelectionEmpty() { return activeWalls.empty(); }
	
public:
  CWallsLayer();
  ~CWallsLayer();

  virtual void Paint( ITemplateView *pView, float fBrightness, bool bGrayed );
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual bool OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView );
	virtual void Reset();
	virtual void Selection( const CRect &r, ITemplateView *pView );
	virtual void ClearSelection();
	virtual void BrowseBrush();

  void SetPlacement( CPlacement *pPl );
	void SetWallWidth( int nWidth ) { nWallWidth = nWidth; }

protected:
	//{{AFX_MSG(CWallsLayer)
	afx_msg void OnWallFlip();
	afx_msg void OnDeleteWall();
	//}}AFX_MSG
	virtual afx_msg void OnVisible();
	DECLARE_MESSAGE_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WALLSLAYER_H__
