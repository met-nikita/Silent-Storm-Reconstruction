#if !defined(AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_)
#define AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OIDlg.h : header file
//

#include "CtrlObjectInspector.h"

const UINT WM_ME_CANCEL = WM_USER + 111;
////////////////////////////////////////////////////////////////////////////////////////////////////
// COIDlg dialog

class COIDlg : public CDialog
{
// Construction
public:
	COIDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
  CCtrlObjectInspector m_wndOI;
  
  void SetPropMap( int nTableID, const CPropMap *_pPropMap );
  int  GetActiveProp( int nGroupID );
	void SetActiveProp( int nPropID );
  void UpdatePropList();
  void UpdateProperty( int nPropID );
  
  // Implementation
  BOOL PreTranslateMessage( MSG* pMsg );

	//{{AFX_DATA(COIDlg)
	enum { IDD = IDD_OBJINSPECTOR };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COIDlg)
	protected:
  virtual void OnCancel();
  virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  const CPropMap    *pPropMap;
	int nPropsTable;
  
	// Generated message map functions
	//{{AFX_MSG(COIDlg)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OIDLG_H__FE9526CD_F8BB_4265_81AE_7B8836040C87__INCLUDED_)
