#if !defined(AFX_WAYPOINTDLG_H__48A18475_DEC7_41A6_B988_3422B1A86485__INCLUDED_)
#define AFX_WAYPOINTDLG_H__48A18475_DEC7_41A6_B988_3422B1A86485__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// WaypointDlg.h : header file
//

namespace NAI
{
	class CWaypoint;
	struct SCommand;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EParam;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParam
{
public:
	EParam column;
	int nItem;
	
	virtual string GetText() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCombo : public CComboBox, public CParam
{
public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCombo)
	//}}AFX_VIRTUAL

	string GetText();
	// Generated message map functions
protected:
	//{{AFX_MSG(CCombo)
	afx_msg void OnSelchange();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEd: public CEdit, public CParam
{
public:
	string GetText();

	virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
	//{{AFX_MSG(CEd)
	afx_msg void OnChange();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointDlg dialog

class CWaypointDlg : public CDialog
{
// Construction
public:
	CWaypointDlg( CWnd* pParent = NULL);   // standard constructor

	void SetWaypoint( int nWaypointID );
// Dialog Data
	//{{AFX_DATA(CWaypointDlg)
	enum { IDD = IDD_WAYPOINT };
	CListCtrl	m_commands;
	CString	m_szName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWaypointDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
	void EditParam( NM_LISTVIEW *pNMListView );
	void GetParam( CParam *pCtrl );
	void HideEditBoxes();
protected:
	int nWaypointID;
	string szWaypoint;
	CPtr<NAI::CWaypoint> pWaypoint;
	CCombo cmd_combo;
	CCombo pose_combo;
	CCombo dir_combo;
	CEd edit;
	CFont m_font;
	CMenu m_menu;

	void SetWaypoint();
	void InsertCommand( int nID, const NAI::SCommand *pCmd );
	void ShowCombo( CCombo *pCombo, EParam column, int nItem );
	void ShowEdit( CEd *pCombo, EParam column, int nItem );
	// Generated message map functions
	//{{AFX_MSG(CWaypointDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnClickCommands(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkCommands(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddCommand();
	afx_msg void OnDelCommand();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WAYPOINTDLG_H__48A18475_DEC7_41A6_B988_3422B1A86485__INCLUDED_)
