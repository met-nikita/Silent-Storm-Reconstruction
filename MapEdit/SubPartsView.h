#ifndef __SUBPARTSVIEW_H_
#define __SUBPARTSVIEW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\DBFormat\DataMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSabPartsDlg dialog
class CSabPartsDlg : public CDialog
{
// Construction
public:
	CSabPartsDlg( int nID, CWnd* pParent = NULL);   // standard constructor

	int  GetMask();

// Dialog Data
	//{{AFX_DATA(CSabPartsDlg)
	enum { IDD = IDD_SABPARTS_DIALOG };
	//}}AFX_DATA
	BOOL	m_parts[NDb::CConstructionPart::N_SUBPARTS][NDb::CConstructionPart::N_SUBPARTS];


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSabPartsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int nConstructionPartID;
	int nMask;
	void SetConstructionPart( int nID );

	// Generated message map functions
	//{{AFX_MSG(CSabPartsDlg)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSubPartsView window
class CSubPartsView : public CWnd
{
// Construction
public:
	CSubPartsView();

// Attributes
public:

// Operations
public:
	void SetConstructionPartID( int nID );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSubPartsView)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CSubPartsView();

	// Generated message map functions
protected:
	CSabPartsDlg m_dlg;
	
	//{{AFX_MSG(CSubPartsView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SUBPARTSVIEW_H_
