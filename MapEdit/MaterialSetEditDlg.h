#if !defined(AFX_MATERIALSETEDITDLG_H__7CFB3DC1_3B6A_4B39_B460_5369D05C7C7D__INCLUDED_)
#define AFX_MATERIALSETEDITDLG_H__7CFB3DC1_3B6A_4B39_B460_5369D05C7C7D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MaterialSetEditDlg.h : header file
//

#define REG_MATERIALS "Materials"
enum EMaterialSet;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditCtrl window
class CMaterialEditCtrl : public CEdit
{
// Construction
public:
	CMaterialEditCtrl();

// Attributes
public:
	int nMaterialID;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMaterialEditCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMaterialEditCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(CMaterialEditCtrl)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialSetEditDlg dialog
class CMaterialSetEditDlg : public CDialog
{
// Construction
public:
	CMaterialSetEditDlg( EMaterialSet set, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMaterialSetEditDlg)
	enum { IDD = IDD_MATERIALSET_EDIT };
	CMaterialEditCtrl	m_mat3;
	CMaterialEditCtrl	m_mat2;
	CMaterialEditCtrl	m_mat0;
	CMaterialEditCtrl	m_mat1;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMaterialSetEditDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	EMaterialSet eSet;
	// Generated message map functions
	//{{AFX_MSG(CMaterialSetEditDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MATERIALSETEDITDLG_H__7CFB3DC1_3B6A_4B39_B460_5369D05C7C7D__INCLUDED_)
