#if !defined(AFX_UIFRAME_H__6AF7F9DB_0B96_4509_81A4_FFE3A184C188__INCLUDED_)
#define AFX_UIFRAME_H__6AF7F9DB_0B96_4509_81A4_FFE3A184C188__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// UIFrame.h : header file
//
#include "UIView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIFrame frame

class CUIFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CUIFrame)
protected:
	CUIFrame();           // protected constructor used by dynamic creation

// Attributes
public:
	CUIView m_View;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUIFrame)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CUIFrame();

	// Generated message map functions
	//{{AFX_MSG(CUIFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UIFRAME_H__6AF7F9DB_0B96_4509_81A4_FFE3A184C188__INCLUDED_)
