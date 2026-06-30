#if !defined(AFX_UIVIEW_H__CFC51403_D1F3_44BD_91D7_9D936FAC15C4__INCLUDED_)
#define AFX_UIVIEW_H__CFC51403_D1F3_44BD_91D7_9D936FAC15C4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// UIView.h : header file
//

class CUIContainer;
class CUIControl;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CTracker: public CRectTracker
{
	CString szText;
	CPoint  ptSpacing;
	CScrollView *pView;
	BOOL TrackHandle(int nHandle, CWnd* pWnd, CPoint point, CWnd* pWndClipTo);
	
public:
	CTracker();
	CTracker(LPCRECT lpSrcRect, UINT nStyle, CString szText, const CPoint &ptSpacing, CScrollView *pView);
	void Draw(CDC* pDC, bool bBk, int nDepth = 0 ) const;
	BOOL SetCursor( CPoint pt, UINT nHitTest) const;
	BOOL Track(CWnd* pWnd, CPoint point, BOOL bAllowInvert = FALSE,	CWnd* pWndClipTo = NULL);

	virtual void AdjustRect(int nHandle, CRect *lpRect);
};

const int UI_SELECT = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIView window
class CUIView : public CScrollView
{
// Construction
public:
	CUIView();

	void SetUIContainer( int nUIContainerID, const CPropMap *pProps );
	CPoint GetGridSpacing() const { return ptGridSpacing; }
	void SetGridSpacing( const CPoint &ptSpacing  ) { ptGridSpacing = ptSpacing; }

// Attributes
public:

// Operations
public:
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUIView)
	protected:
	virtual void PostNcDestroy();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CUIView();

	struct SRect
	{
		int nID;
		CPtr<CUIControl> pCtrl;
		CTracker track;

		SRect( int id, CUIControl *pControl, CTracker tr ) : nID(id), pCtrl( pControl ), track( tr ) {}
	};

	// Generated message map functions
protected:
	CFont m_defFont;
	CPoint ptGridSpacing;
	bool  bGrid;

	// информация о редактируемом контейнере
	CPtr<CUIContainer> pContainer;	// текущий активный контейнер
	const CPropMap *pUICProps;
	CSize sizeContainer;
	CMenu m_popup;
	CPoint ptRightClick;
	//
	vector<SRect> rects;
	vector<int> activeRects;
	int nUIControlMode;
	vector<SRect>::iterator GetRect( int nID );
	
	void OnDraw(CDC* pDC);
	void DrawGrid( CDC *pDC );
	void SetActiveControl( int rectID );
	bool AddActiveControl( int rectID );
	void ClearActiveControl( int nRectInd );
	void ClearActiveControls();
	void AddNewControl( const CPoint &pt );
	void DeleteActiveControls();
	int  GetHitRect( CPoint pt );

	//{{AFX_MSG(CUIView)
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnUiButton();
	afx_msg void OnUiPushButton();
	afx_msg void OnUiCheckbutton();
	afx_msg void OnUiEdit();
	afx_msg void OnUiImage();
	afx_msg void OnUiImagelist();
	afx_msg void OnUiMessagebox();
	afx_msg void OnUiModel();
	afx_msg void OnUiRadiobutton();
	afx_msg void OnUiSelect();
	afx_msg void OnUiSlider();
	afx_msg void OnUiText();
	afx_msg void OnUiScroll();
	afx_msg void OnUiCombobox();
	afx_msg void OnUpdateUiButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiPushButton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiCheckbutton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiEdit(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiImage(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiImagelist(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiMessagebox(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiModel(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiRadiobutton(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiSelect(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiSlider(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiText(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiScroll(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUiCombobox(CCmdUI* pCmdUI);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnUiGroup();
	afx_msg void OnUpdateUiGroup(CCmdUI* pCmdUI);
	afx_msg void OnUiWindow();
	afx_msg void OnUpdateUiWindow(CCmdUI* pCmdUI);
	afx_msg void OnUiProgressBar();
	afx_msg void OnUpdateUiProgressBar(CCmdUI* pCmdUI);
	afx_msg void OnContextMenu( CWnd*, CPoint );
	afx_msg void OnImageSize();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetNearestPos( int pos, int spacing )
{
  return spacing * int(0.5 + (double)pos / spacing);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UIVIEW_H__CFC51403_D1F3_44BD_91D7_9D936FAC15C4__INCLUDED_)
