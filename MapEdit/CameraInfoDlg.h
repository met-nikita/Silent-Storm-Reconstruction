#if !defined(AFX_CAMERAINFODLG_H__9AA3EA37_C9AE_4317_A217_6C36EB973E90__INCLUDED_)
#define AFX_CAMERAINFODLG_H__9AA3EA37_C9AE_4317_A217_6C36EB973E90__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CameraInfoDlg.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CCameraInfoDlg dialog

class CCameraInfoDlg : public CDialog
{
// Construction
public:
	CCameraInfoDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CCameraInfoDlg)
	enum { IDD = IDD_CAMERA_INFO };
	CString	m_szInfo;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCameraInfoDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CCameraInfoDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CAMERAINFODLG_H__9AA3EA37_C9AE_4317_A217_6C36EB973E90__INCLUDED_)
