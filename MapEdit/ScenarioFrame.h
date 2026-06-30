#if !defined(__SCENARIOFRAME_H_)
#define __SCENARIOFRAME_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ScenarioView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterFrame frame

class CScenarioFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CScenarioFrame)
protected:
	CScenarioFrame();           // protected constructor used by dynamic creation

	// Attributes
public:
	CScenarioView m_View;
	// Operations

	// Implementation
protected:
	virtual ~CScenarioFrame();

	// Generated message map functions
	//{{AFX_MSG(CScenarioFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(__SCENARIOFRAME_H_)
