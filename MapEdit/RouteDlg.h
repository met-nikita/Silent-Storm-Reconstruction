#if !defined(AFX_ROUTEDLG_H__6DC90359_1310_485C_AF41_C3DA3DF330EA__INCLUDED_)
#define AFX_ROUTEDLG_H__6DC90359_1310_485C_AF41_C3DA3DF330EA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RouteDlg.h : header file
//

namespace NAI
{
	class CUnitAIInfo;
}
class CMETreeView;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRouteDlg dialog

class CRouteDlg : public CDialog
{
// Construction
public:
	CRouteDlg( int nUnitID, CWnd* pParent = NULL);   // standard constructor
	~CRouteDlg();

// Dialog Data
	//{{AFX_DATA(CRouteDlg)
	enum { IDD = IDD_UNIT_ROUTE };
	CListCtrl	m_list;
	CButton	m_treePlace;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRouteDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int nUnitID;
	CPtr<NAI::CUnitAIInfo> pUnit;
	CMETreeView *m_pTree;
	int nGroupID;

	void InsertWaypoints();
	// Generated message map functions
	//{{AFX_MSG(CRouteDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkRoute(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLvnKeydownRoute(NMHDR *pNMHDR, LRESULT *pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ROUTEDLG_H__6DC90359_1310_485C_AF41_C3DA3DF330EA__INCLUDED_)
