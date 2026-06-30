#if !defined(AFX_TREEVDIALOGS_H__DBE2661C_66CB_4BA4_970F_F3CEF628E44D__INCLUDED_)
#define AFX_TREEVDIALOGS_H__DBE2661C_66CB_4BA4_970F_F3CEF628E44D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TreeVDialogs.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewFolderDlg dialog

class CNewFolderDlg : public CDialog
{
// Construction
public:
	CNewFolderDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewFolderDlg)
	enum { IDD = IDD_NEWFOLDER };
	CButton	m_buttonOK;
	CString	m_NewName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewFolderDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewFolderDlg)
	afx_msg void OnChangeNewFolderName();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddTemplDlg dialog

class CAddTemplDlg : public CDialog
{
  // Construction
public:
  CAddTemplDlg( const string &szFolder, CWnd* pParent = NULL );   // standard constructor
  
  // Dialog Data
  //{{AFX_DATA(CAddTemplDlg)
	enum { IDD = IDD_ADD_TEMPL };
	CButton	m_buttonOK;
  int		m_height;
  int		m_width;
  CString	m_name;
	CString	m_szFolder;
	//}}AFX_DATA
  
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CAddTemplDlg)
protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  
  // Generated message map functions
  //{{AFX_MSG(CAddTemplDlg)
	afx_msg void OnChangeTemplName();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewItemDlg dialog

class CNewItemDlg : public CDialog
{
// Construction
public:
	CNewItemDlg( const string &szFolder, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewItemDlg)
	enum { IDD = IDD_NEWITEM };
  CButton	m_buttonOK;
  CString	m_NewName;
	CString	m_szFolder;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewItemDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewItemDlg)
  afx_msg void OnChangeNewItemName();
  virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	int m_nQuantity;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTreeLayoutDlg dialog

class CTreeLayoutDlg : public CDialog
{
// Construction
public:
	CTreeLayoutDlg( const vector<pair<int, string> > &allResources, vector<int> *pActiveTabs, CWnd* pParent = NULL);   // standard constructor
  ~CTreeLayoutDlg();

// Dialog Data
	//{{AFX_DATA(CTreeLayoutDlg)
	enum { IDD = IDD_RES_LAYOUT };
	CCheckListBox m_list;
	CButton	m_OK;
	CButton	m_Cancel;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTreeLayoutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
  
// Implementation
protected:
  vector<int>      *pActiveTabs;  
  vector<pair<int, string> >  resources;

	// Generated message map functions
	//{{AFX_MSG(CTreeLayoutDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelectAll();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelFolderDlg dialog

class CDelFolderDlg : public CDialog
{
// Construction
public:
	CDelFolderDlg( CString szName, bool bNoDefault, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDelFolderDlg)
	enum { IDD = IDD_DELETE_FOLDER };
	CString	m_szText;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDelFolderDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	bool bNoDefault;
	// Generated message map functions
	//{{AFX_MSG(CDelFolderDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnNo();
	afx_msg void OnYes();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelItemsDlg dialog

class CDelItemsDlg : public CDialog
{
// Construction
public:
	CDelItemsDlg( const CString &szItems, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDelItemsDlg)
	enum { IDD = IDD_DELETE_ITEMS };
	CString	m_szItems;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDelItemsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDelItemsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TREEVDIALOGS_H__DBE2661C_66CB_4BA4_970F_F3CEF628E44D__INCLUDED_)
