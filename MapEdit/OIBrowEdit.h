#ifndef __OIBROWEDIT_H__
#define __OIBROWEDIT_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIEdit.h"

class COIBrowseEdit;

////////////////////////////////////////////////////////////////////////////////////////////////////

class COIBrowseButton : public CButton
{
public:
  COIBrowseButton( COIBrowseEdit *pPrnt, CEdit* pEdtBrowse );
  
  ~COIBrowseButton();
  
  BOOL Create();
  
protected:
  //{{AFX_MSG(COIBrowseButton)
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    
protected:
  virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pLResult);
  
  // Button width enumeration.
  enum { 
    BTN_WIDTH = 20		// emem Button width.
  } ;
  
  // Pointer to parent control.
  CEdit* m_pEdtBrowse;
  COIBrowseEdit *m_pParentWnd;
  
  // control ID for this button.
  UINT m_uiID;  
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class COIBrowseEdit : public CWnd
{
public:
  
  COIBrowseEdit();  
  virtual ~COIBrowseEdit();
  
protected:
  enum { 
    BTN_WIDTH = 17
  };
  CFont	m_fntDef;
  COIEdit m_Edit;
  COIBrowseButton m_BrowseBtn;
  
  CString m_strCaption;
  CString m_szDirPrefix;
  
public:
  
  void SetBrowseCaption(LPCTSTR lpcszCaption)	
		{ m_strCaption = lpcszCaption; }
  
  LPCTSTR GetBrowseCaption() const
		{ return m_strCaption; }
  
  COIBrowseButton* GetBrowseButton()
		{ return &m_BrowseBtn; }
  virtual void SetWindowText( LPCTSTR lpszString );
  virtual void GetWindowText( CString &rString ) const;
  void SetDirPrefix( LPCTSTR lpszDirPref );
	void SetCheckSyntax( bool bCheck );
  
public:
  
  virtual void OnBrowse();
  
protected:

  virtual BOOL PreTranslateMessage( MSG* pMsg );
  
  //{{AFX_MSG(COIBrowseEdit)  
  afx_msg void OnEnable(BOOL bEnable);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSetFocus(CWnd* pNewWnd);
	//}}AFX_MSG

  DECLARE_MESSAGE_MAP()
    
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class COIExEdit : public COIBrowseEdit
{
public:
	virtual void OnBrowse() { m_Edit.ShowEditDlg(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __OIBROWEDIT_H__
