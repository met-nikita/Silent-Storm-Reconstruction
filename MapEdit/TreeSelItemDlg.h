#if !defined(AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_)
#define AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TreeSeltemDlg.h : header file
//
#include "OIDlg.h"

class CMETreeView;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTreeSelItemDlg dialog

class CTreeSelItemDlg : public CDialog
{
// Construction
public:
	CTreeSelItemDlg( const vector<SResTree> &vResTrees, int nPrevSelectedTree = -1, int nPrevSelectedID = -1, CWnd *pParent = 0 );
	~CTreeSelItemDlg();

	virtual void SetSelectedItemID( int nTreeID, int id );
	virtual void GetSelectedItemID( int *pnTree, int *pnID );
	void SetReadOnly( bool bReadOnly );
	virtual string GetItemPath( int nTreeID, int nItemID ) { return ""; }
// Dialog Data
	//{{AFX_DATA(CTreeSelItemDlg)
	enum { IDD = IDD_TREE_SELECT_ITEM };
	CButton	m_treeplace;
	CString	m_szPrevSelection;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeSelItemDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CMETreeView *m_pTree;
	COIDlg m_OIDlg;
	int nSelectedTree;
	int nSelectedID;
	int nPrevSelectedTree;
	int nPrevSelectedID;
	vector<SResTree> vResTrees;
	const CPropMap *pPropMap;
	BOOL bReadOnly;

	void SetPropMap( int nTreeID, int nItemID );
	// Generated message map functions
	//{{AFX_MSG(CTreeSelItemDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnRelEmpty();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREESELTEMDLG_H__F8EB6E9A_BEAC_4C5B_B50D_DFE82977B2E4__INCLUDED_)
