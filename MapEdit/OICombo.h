#if !defined(AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_)
#define AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OICombo.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// COICombo window

class COICombo : public CComboBox
{
// Construction
public:
	COICombo();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COICombo)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~COICombo();

	// Generated message map functions
protected:
  CFont	m_fntDef;

	//{{AFX_MSG(COICombo)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSelchange();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OICOMBO_H__B4AC7D2E_CAF0_4314_BFCB_D0C13D6C244C__INCLUDED_)
