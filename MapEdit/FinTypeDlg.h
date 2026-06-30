#if !defined(AFX_FINTYPEDLG_H__4D9EDBF3_C9D2_4AC2_ACDF_D4F2F7DD09F5__INCLUDED_)
#define AFX_FINTYPEDLG_H__4D9EDBF3_C9D2_4AC2_ACDF_D4F2F7DD09F5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FinTypeDlg.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CFinTypeDlg dialog

class CFinTypeDlg : public CDialog
{
// Construction
public:
	CFinTypeDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFinTypeDlg)
	enum { IDD = IDD_FINELEM_SELECT };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

  bool isUnit;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFinTypeDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFinTypeDlg)
	afx_msg void OnRadioUnit();
	afx_msg void OnRadioFinelem();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FINTYPEDLG_H__4D9EDBF3_C9D2_4AC2_ACDF_D4F2F7DD09F5__INCLUDED_)
