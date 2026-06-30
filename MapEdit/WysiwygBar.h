#ifndef __WYSIWYGBAR_H_
#define __WYSIWYGBAR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMaterialEditPage;
class CGeometryPage;
class CTexSpotPage;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBar
class CWysiwygBarWnd : public CPropertySheet
{
	DECLARE_DYNAMIC(CWysiwygBarWnd)

// Construction
public:
	CWysiwygBarWnd(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CWysiwygBarWnd(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWysiwygBarWnd)
	public:
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWysiwygBarWnd();

	// Generated message map functions
protected:
	CMaterialEditPage *const m_pMaterial;
	CGeometryPage *const m_pGeometry;
	//{{AFX_MSG(CWysiwygBarWnd)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBar window
class CWysiwygBar : public SECControlBar
{
// Construction
public:
	CWysiwygBar();

// Attributes
public:

// Operations
public:
	CMaterialEditPage *const m_pMaterial;
	CGeometryPage *const m_pGeometry;
	CTexSpotPage *const m_pTexSpot;
	SECTabWnd m_tab;
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWysiwygBar)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWysiwygBar();

	// Generated message map functions
protected:
	//{{AFX_MSG(CWysiwygBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnTabSel(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGBAR_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
