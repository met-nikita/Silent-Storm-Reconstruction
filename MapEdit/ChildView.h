// ChildView.h : interface of the CChildView class
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHILDVIEW_H__3F4811C1_7694_4FD2_A156_A6EBED8143E5__INCLUDED_)
#define AFX_CHILDVIEW_H__3F4811C1_7694_4FD2_A156_A6EBED8143E5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TemplateView.h"

class CPlacement;
class CPaintBar;
class CLayerList;
class CLayerCtrl;
class CRectsLayer;
class CWallsLayer;
class CTilesLayer;
class CHeightsLayer;
class CAlphaLayer;
class CCellarLayer;
enum EFloorType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChildView;
class CDropTarget: public COleDropTarget
{
public:
	CChildView *pView;
public:
	CDropTarget(): pView(0) {}

	//{{AFX_VIRTUAL(CDropTarget)
public:
	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
	virtual void OnDragLeave(CWnd* pWnd);
	//}}AFX_VIRTUAL
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChildView window

class CChildView : public CWnd, public ITemplateView
{
// Construction
public:
	CChildView();

// Attributes
public:

// Operations
public:
  void SetPlacement( CPlacement *pPl, CPlacementCache *pCache );
  CPlacement* GetActivePlacement() { return pCurPlacement; }
  bool IsCursorInWindow();
  void DropTemplate( int id );
	void DropUnit( int id );
	void DropObject( int id );
	void DropContainer( int id );
  void SetSpacing( int n );
	void SetZoom( float fZoom );
	float GetZoom() const { return fZoom; }
	bool ChangeObject( int nTreeID, int nObjectID, int nNewRelation );
	bool SetContainerRoom( int nObjID, int nRoom );
	bool SetObjectDZ( int nTreeID, int nObjID, float fDZ );
	bool SetObjectRotation( int nTreeID, int nObjID, int nRotation );
	bool SetExplosionPower( int nObjID, float fPower );
	bool SetScaleX( int nObjID, float fScale );
	bool SetScaleY( int nObjID, float fScale );
	bool SetScaleZ( int nObjID, float fScale );

	// интерфейс ITemplateView
	virtual CDC* GetPaintDC() { return pDCBuf; }

	virtual int  GetSpacing() const { return nSpacing * fZoom; }
	virtual CWnd* GetWnd() { return this; }
	virtual EEditMode GetPaintMode( int *pExtra ) const;
  virtual void ScreenToTemplate( CPoint *pPtTempl, const CPoint &pt ) const;
  virtual void ScreenToTemplate( const CPoint &pt, float *pX, float *pY ) const;
  virtual void TemplateToScreen( CPoint *pPtScreen, const CPoint &pt ) const;
	virtual void TemplateToScreen( CPoint *pPtScreen, float x, float y ) const;
	virtual void Repaint() { Invalidate( FALSE ); }
	virtual void SetModifiedFlag() { bTerrChanged = true; }	
	virtual bool IsZooming() const { return bZooming; }
	virtual void SelectedItem( int nResTreeID, int nItemID, int nObjID );
	
	float ScreenToTemplate( int size );
	CPaintBar* GetPaintBar() const { return pPntbar; }
	CLayerList* GetLayers() const { return pLayers; }
	void SaveData();
	bool ExportBmp();
	void ActiveFloorChanged();
	void SetGridVisible( bool bVisible );
	void SetupLayerList();
	
  // Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildView)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CChildView();

	// Generated message map functions
protected:
  CPlacement *pCurPlacement;
	CPlacementCache *pPlacementCache;
  int   nSpacing;
  LONG  nWidth;
  LONG  nHeight;
  CPoint    leftTop;
	COLORREF  crGrid;
	COLORREF  crGridThick;
	int				nFontH;
	int				nGridSize; // интервал между жирными линиями в сетке
	float			fZoom;
	HCURSOR		m_hArrow;
	HCURSOR		m_hHand;
	HCURSOR		m_hZoom;
	HCURSOR		m_hSelection;
	HCURSOR		m_hErase;
	CRect			m_rSelection;
	bool			bTerrChanged;
	bool			bGrid;
	bool			bZooming;
	CDC *pDCBuf;
	CPaintBar *const pPntbar;
	CPropMap  props;
	// { слои
	struct SLayer
	{
		string szName;
		int nLayerID;
		bool bSingle;
		bool bLastVisible;
		SLayer( string name, int _nLayerID, bool _bSingle, bool bVis = false ) : szName( name ), nLayerID( _nLayerID ), bSingle( _bSingle ), bLastVisible(bVis) {}
	};
	
	CLayerList *pLayers;
	CRectsLayer *const pSubTemplL;
	CRectsLayer *const pUnitsL;
	CRectsLayer *const pObjsL;
	CWallsLayer *const pWallsL;
	vector<CLayerCtrl*> floorsLs;
	vector<CLayerCtrl*> floorsIntermLs;
	vector<CLayerCtrl*> solidsLs;
	vector<CLayerCtrl*> solidsIntermLs;
	vector<CLayerCtrl*> roomsLs;
	vector<CLayerCtrl*> grassLs;
	vector<CLayerCtrl*> terrColorLs;
	vector<CLayerCtrl*> spotsLs;
	CTilesLayer *const pTilesL;
	CHeightsLayer *const pHeightsL;
	CHeightsLayer *const pHolesL;
	CAlphaLayer *const pAlphaL;
	CCellarLayer *const pCellarL;
	CDropTarget dropTarget;

	vector<SLayer> layers;
	// }
	CLayerCtrl* AddLayer( int nLayerID, int nLayerInd = -1, CLayerCtrl **pSecond = 0 );
	void SetLayersPlacement( EFloorType type, int nLayerID );
	
	//EPaintMode penWidth;

	float TrackZoom();
	void  TrackPan();
	void  TrackSelection();
	bool WriteTerrMap();
  void SetupScrolls();
  int  UpdateScroll( int nBar, UINT nSBCode, UINT nPos );
  void TemplateToScreen( CRect &r );
  void ScreenToTemplate( CRect &r );
	void Centre( CPoint *pPtLeftTop );
	void SetSelection( const CRect &r );
	void ClearSelection();
	bool HasPaintBar();
	void ResetPropList();
	void DeleteLayers();
	void DeleteLayer( CLayerCtrl *pLr );
	void OnActivateLayer( CLayerCtrl *p );
	void OnLinkLayer( CLayerCtrl *p );
	void OnActivateFloor();
	void OnLinkFloor();
	void AddTerrainSpotLayers();

	CRect rOldBrush;

	void GetGridPlacement( CRect *pRect, CPoint *pptOrig, const CSize &cellSize );
  void DrawLineGrid( CDC *pDC );
	
	//{{AFX_MSG(CChildView)
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnFileExportTemplate();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CChildView::ScreenToTemplate( const CPoint &pt, float *pX, float *pY ) const
{
	int nDelta = fZoom * nSpacing;
  *pX = float(leftTop.x + pt.x) / nDelta;
  *pY = float(nHeight - (leftTop.y + pt.y)) / nDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CChildView::ScreenToTemplate( CPoint *pPtTempl, const CPoint &pt ) const
{
	int nDelta = fZoom * nSpacing;
  pPtTempl->x = 0.5f + float(leftTop.x + pt.x) / nDelta;
  pPtTempl->y = 0.5f + float(nHeight - (leftTop.y + pt.y)) / nDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CChildView::TemplateToScreen( CPoint *pPtScreen, const CPoint &pt ) const
{
	int nDelta = fZoom * nSpacing;
  pPtScreen->x = pt.x * nDelta - leftTop.x;
  pPtScreen->y = nHeight - pt.y * nDelta - leftTop.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CChildView::TemplateToScreen( CPoint *pPtScreen, float x, float y ) const
{
	int nDelta = fZoom * nSpacing;
  pPtScreen->x = x * nDelta - leftTop.x;
  pPtScreen->y = nHeight - y * nDelta - leftTop.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void ScreenToTemplate( CPoint *pPtTempl, const CPoint &pt )
{
  theApp.GetTemplateView()->ScreenToTemplate( pPtTempl, pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void ScreenToTemplate( float *pX, float *pY, const CPoint &pt )
{
  theApp.GetTemplateView()->ScreenToTemplate( pt, pX, pY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void TemplateToScreen( CPoint *pPtScreen, const CPoint &pt )
{
  theApp.GetTemplateView()->TemplateToScreen( pPtScreen, pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddLayerDlg dialog
class CAddLayerDlg : public CDialog
{
// Construction
public:
	vector<pair<string, DWORD> > items;

	CAddLayerDlg( CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAddLayerDlg)
	enum { IDD = IDD_ADD_LAYER };
	CCheckListBox	m_list;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAddLayerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAddLayerDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDVIEW_H__3F4811C1_7694_4FD2_A156_A6EBED8143E5__INCLUDED_)
