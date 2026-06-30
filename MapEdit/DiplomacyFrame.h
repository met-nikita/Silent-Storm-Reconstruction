#ifndef  __DIPLOMACYFRAME_H_
#define __DIPLOMACYFRAME_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "DiplomacyView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyFrame frame
class CDiplomacyFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CDiplomacyFrame)
protected:
	CDiplomacyFrame();

	// Attributes
public:
	CDiplomacyView m_View;
	// Operations
public:

protected:
	virtual ~CDiplomacyFrame();

	// Generated message map functions
	//{{AFX_MSG(CDiplomacyFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DIPLOMACYFRAME_H_
