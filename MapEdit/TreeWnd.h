#if !defined(AFX_LSTDBAR_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)
#define AFX_LSTDBAR_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_

// Stingray Objective Toolkit
// AppWizard generated file
// treewnd.h : header file
//

class CMETreeView;

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTmplTreeWnd window

class CTmplTreeWnd : public SECControlBar
{
// Construction
public:
	CTmplTreeWnd( const string &szRegSection );

  virtual void OnExtendContextMenu(CMenu* pMenu);
  
// Attributes
public:

// Operations
public:
  SECTreeCtrl* GetActiveTree();
  void UpdateTree( int nTreeID = -1 );
  void SetVisibleTabs( const unordered_map<string, bool> &layout );
  void SaveLayout();
  void LoadLayout();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTmplTreeWnd)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTmplTreeWnd();


	// Generated message map functions
protected:
  SEC3DTabWnd m_tabWnd;
  string szRegSection;
  //
  struct STreeTab
  {
    CMETreeView *pTree;
    int nTreeID;
    string szTabName;
		bool bInitiallyVisible;
  };
  vector<STreeTab> tabList;

	//{{AFX_MSG(CTmplTreeWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg LRESULT OnTabSel(WPARAM wParam, LPARAM lParam);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnResourceTabs();
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline SECTreeCtrl* CTmplTreeWnd::GetActiveTree()
{
  CWnd *pWnd;
  m_tabWnd.GetActiveTab( pWnd );
  return dynamic_cast<SECTreeCtrl*>( pWnd );
}

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LSTDBAR_H__B6F638DA_2DBC_11D1_A86B_0060977B4135__INCLUDED_)
