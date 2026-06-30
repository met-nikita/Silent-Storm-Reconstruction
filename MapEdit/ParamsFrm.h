#if !defined(AFX_PARAMSFRM_H__90C3F01D_C951_46F9_ABE3_FA9CF9461A12__INCLUDED_)
#define AFX_PARAMSFRM_H__90C3F01D_C951_46F9_ABE3_FA9CF9461A12__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ParamsFrm.h : header file
//

#include "ParamsView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsFrame frame

class CParamsFrame : public SECWorksheet
{
	DECLARE_DYNCREATE(CParamsFrame)
protected:
	CParamsFrame();           // protected constructor used by dynamic creation

// Attributes
public:
	CParamsView m_View;
	
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CParamsFrame)
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CParamsFrame();

	// Generated message map functions
	//{{AFX_MSG(CParamsFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PARAMSFRM_H__90C3F01D_C951_46F9_ABE3_FA9CF9461A12__INCLUDED_)
