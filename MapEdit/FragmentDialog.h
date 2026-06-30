#ifndef __FRAGMENTDIALOG_H_
#define __FRAGMENTDIALOG_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CFinalElement;
}
namespace NBuilding
{
	struct SBuildFragment;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseObjDlg : public CDialog
{
// Construction
public:
	CBaseObjDlg( CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFragmentDlg)
	enum { IDD = IDD_FRAGMENT_DIALOG };
	//}}AFX_DATA

	BOOL m_bOpen;
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFragmentDlg)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CFragmentDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFragmentDlg : public CBaseObjDlg
{
// Construction
public:
	CFragmentDlg( int nWorldID, int nData, CWnd* pParent = NULL);   // standard constructor

protected:
	int nWorldID;
	NBuilding::SBuildFragment *pFr;

	virtual void OnOK();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectDlg : public CBaseObjDlg
{
// Construction
public:
	CObjectDlg( NDb::CFinalElement *pFin, CWnd* pParent = NULL);   // standard constructor

protected:
	CPtr<NDb::CFinalElement> pFin;
	void SetLightCheck();
	void GetLightCheck();

	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FRAGMENTDIALOG_H_