#if !defined(AFX_CHAPTERFRAME_H__8EBDDAD7_2420_45FE_9B3F_D0A77A0EA140__INCLUDED_)
#define AFX_CHAPTERFRAME_H__8EBDDAD7_2420_45FE_9B3F_D0A77A0EA140__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ChapterFrame.h : header file
//
#include "ChapterView.h"
#include "GlobalMapView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterFrame frame

class CChapterFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CChapterFrame)
protected:
	CChapterFrame();           // protected constructor used by dynamic creation

// Attributes
public:
	CChapterView m_View;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChapterFrame)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CChapterFrame();

	// Generated message map functions
	//{{AFX_MSG(CChapterFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////

class CGlobalMapFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CGlobalMapFrame)
protected:
	CGlobalMapFrame();

public:
	CGlobalMapView m_View;
protected:
	virtual ~CGlobalMapFrame();

	//{{AFX_MSG(CChapterFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHAPTERFRAME_H__8EBDDAD7_2420_45FE_9B3F_D0A77A0EA140__INCLUDED_)
