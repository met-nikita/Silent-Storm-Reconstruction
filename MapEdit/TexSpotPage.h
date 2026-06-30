#if !defined(AFX_TEXSPOTPAGE_H__B4B2F713_00DD_45A6_9ACD_399B4D327773__INCLUDED_)
#define AFX_TEXSPOTPAGE_H__B4B2F713_00DD_45A6_9ACD_399B4D327773__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TexSpotPage.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTexSpotPage dialog

class CTexSpotPage : public CDialog
{
// Construction
public:
	CTexSpotPage(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTexSpotPage)
	enum { IDD = IDD_TEXTURESPOT_EDIT };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTexSpotPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTexSpotPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEXSPOTPAGE_H__B4B2F713_00DD_45A6_9ACD_399B4D327773__INCLUDED_)
