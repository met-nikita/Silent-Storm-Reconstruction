#ifndef __WALLSPOTDLG_H_
#define __WALLSPOTDLG_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	struct SProjectedSpot;
}
class CWallSpotDlg : public CDialog
{
// Construction
public:
	CWallSpotDlg( NBuilding::SProjectedSpot *pSpot, CWnd* pParent = NULL);   // standard constructor

	BOOL m_bMaterial0;
	BOOL m_bMaterial1;
	BOOL m_bMaterial2;
	BOOL m_bMaterial3;
// Dialog Data
	//{{AFX_DATA(CWallSpotDlg)
	enum { IDD = IDD_WALLSPOT };
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWallSpotDlg)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	NBuilding::SProjectedSpot *pSpot;
	// Generated message map functions
	//{{AFX_MSG(CWallSpotDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSelectFragments();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WALLSPOTDLG_H_