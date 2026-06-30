#if !defined(AFX_FINDDIALOG_H__76E9AFE4_58FB_4B94_B440_1AF380FC8938__INCLUDED_)
#define AFX_FINDDIALOG_H__76E9AFE4_58FB_4B94_B440_1AF380FC8938__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FindDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFindDialog dialog

class CFindDialog : public CDialog
{
// Construction
public:
	CFindDialog( bool bHasVariants, CWnd* pParent = NULL);   // standard constructor

	bool bItem; // item or variant ?
// Dialog Data
	//{{AFX_DATA(CFindDialog)
	enum { IDD = IDD_FIND };
	CEdit	m_VariantCtrl;
	CEdit	m_ItemCtrl;
	int		m_nItemID;
	int		m_nVariantID;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFindDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	bool bHasVariants;

	void SetCheck();

	// Generated message map functions
	//{{AFX_MSG(CFindDialog)
	afx_msg void OnRadioItemID();
	afx_msg void OnRadioVariantID();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FINDDIALOG_H__76E9AFE4_58FB_4B94_B440_1AF380FC8938__INCLUDED_)
