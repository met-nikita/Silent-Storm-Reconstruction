#if !defined(AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_)
#define AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TreeSeltemDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTreeSeltemDlg dialog

class CTreeSeltemDlg : public CDialog
{
// Construction
public:
	CTreeSeltemDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTreeSeltemDlg)
	enum { IDD = IDD_TREE_SELECT_ITEM };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeSeltemDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTreeSeltemDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_)
