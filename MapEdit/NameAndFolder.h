#if !defined(AFX_NAMEANDFOLDER_H__74432E37_BD8D_4E89_A05C_2B2CD6694300__INCLUDED_)
#define AFX_NAMEANDFOLDER_H__74432E37_BD8D_4E89_A05C_2B2CD6694300__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// NameAndFolder.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNameAndFolder dialog
class CMETreeView;
class CNameAndFolder : public CDialog
{
// Construction
public:
	CNameAndFolder(const SResTree *pResTree, CWnd* pParent = NULL);   // standard constructor
	~CNameAndFolder();

// Dialog Data
	//{{AFX_DATA(CNameAndFolder)
	enum { IDD = IDD_SELECT_NAME_AND_FOLDER };
	CButton	m_treePlace;
	CButton	m_okButton;
	//}}AFX_DATA
	
	int nSelectedItem;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNameAndFolder)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CMETreeView *m_pTree;

	// Generated message map functions
	//{{AFX_MSG(CNameAndFolder)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NAMEANDFOLDER_H__74432E37_BD8D_4E89_A05C_2B2CD6694300__INCLUDED_)
