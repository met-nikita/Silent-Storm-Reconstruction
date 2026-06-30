// TreeVDialogs.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "TreeVDialogs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewFolderDlg dialog


CNewFolderDlg::CNewFolderDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNewFolderDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewFolderDlg)
	m_NewName = _T("");
	//}}AFX_DATA_INIT
}


void CNewFolderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewFolderDlg)
	DDX_Control(pDX, IDOK, m_buttonOK);
	DDX_Text(pDX, IDC_NEWFOLDERNAME, m_NewName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewFolderDlg, CDialog)
	//{{AFX_MSG_MAP(CNewFolderDlg)
	ON_EN_CHANGE(IDC_NEWFOLDERNAME, OnChangeNewFolderName)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewFolderDlg message handlers

BOOL CNewFolderDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_buttonOK.EnableWindow( false );
  
  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}

void CNewFolderDlg::OnChangeNewFolderName() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
  UpdateData();

  if ( m_NewName.GetLength() == 0 )
    m_buttonOK.EnableWindow( false );
  else
    m_buttonOK.EnableWindow();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddTemplDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////

CAddTemplDlg::CAddTemplDlg( const string &szFolder, CWnd* pParent /*=NULL*/)
: CDialog(CAddTemplDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CAddTemplDlg)
  m_height = 1;
  m_width = 1;
  m_name = _T("");
	m_szFolder = _T( szFolder.c_str() );
	//}}AFX_DATA_INIT
}

void CAddTemplDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CAddTemplDlg)
	DDX_Control(pDX, IDOK, m_buttonOK);
  DDX_Text(pDX, IDC_TEMPL_HEIGHT, m_height);
  DDX_Text(pDX, IDC_TEMPL_WIDTH, m_width);
  DDV_MinMaxInt(pDX, m_width, 0, 1000000);
  DDX_Text(pDX, IDC_TEMPL_NAME, m_name);
  DDV_MaxChars(pDX, m_name, 31);
	DDX_Text(pDX, IDC_FOLDERNAME, m_szFolder);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAddTemplDlg, CDialog)
  //{{AFX_MSG_MAP(CAddTemplDlg)
	ON_EN_CHANGE(IDC_TEMPL_NAME, OnChangeTemplName)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddTemplDlg message handlers

void CAddTemplDlg::OnChangeTemplName() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
  UpdateData();
  
  if ( m_name.GetLength() == 0 )
    m_buttonOK.EnableWindow( false );
  else
    m_buttonOK.EnableWindow();
}

BOOL CAddTemplDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  m_buttonOK.EnableWindow( false );
  
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewItemDlg dialog


CNewItemDlg::CNewItemDlg(const string &szFolder, CWnd* pParent /*=NULL*/)
	: CDialog(CNewItemDlg::IDD, pParent)
	, m_nQuantity(1)
{
	//{{AFX_DATA_INIT(CNewItemDlg)
  m_NewName = _T("");  
	m_szFolder = _T( szFolder.c_str() );
	//}}AFX_DATA_INIT
}


void CNewItemDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewItemDlg)
	DDX_Control(pDX, IDOK, m_buttonOK);
	DDX_Text(pDX, IDC_NEWITEMNAME, m_NewName);
	DDX_Text(pDX, IDC_FOLDERNAME, m_szFolder);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_NUMBEROFITEMS, m_nQuantity);
	DDV_MinMaxInt(pDX, m_nQuantity, 0, 100);
}


BEGIN_MESSAGE_MAP(CNewItemDlg, CDialog)
	//{{AFX_MSG_MAP(CNewItemDlg)
  ON_EN_CHANGE(IDC_NEWITEMNAME, OnChangeNewItemName)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewItemDlg message handlers

BOOL CNewItemDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  m_buttonOK.EnableWindow( false );
  
  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}

void CNewItemDlg::OnChangeNewItemName() 
{
  // TODO: If this is a RICHEDIT control, the control will not
  // send this notification unless you override the CDialog::OnInitDialog()
  // function and call CRichEditCtrl().SetEventMask()
  // with the ENM_CHANGE flag ORed into the mask.
  UpdateData();
  
  if ( m_NewName.GetLength() == 0 )
    m_buttonOK.EnableWindow( false );
  else
    m_buttonOK.EnableWindow();
}/////////////////////////////////////////////////////////////////////////////
// CTreeLayoutDlg dialog


CTreeLayoutDlg::CTreeLayoutDlg( const vector<pair<int, string> > &allResources, 
                                vector<int> *_pActiveTabs, 
                                CWnd* pParent /*=NULL*/)
	: CDialog(CTreeLayoutDlg::IDD, pParent), resources( allResources )
{
  ASSERT( pActiveTabs );
  pActiveTabs = _pActiveTabs;
  //{{AFX_DATA_INIT(CTreeLayoutDlg)
	//}}AFX_DATA_INIT
}

CTreeLayoutDlg::~CTreeLayoutDlg()
{
}

void CTreeLayoutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTreeLayoutDlg)
	DDX_Control(pDX, IDC_RESLAYOUT_LIST, m_list);
	DDX_Control(pDX, IDOK, m_OK);
	DDX_Control(pDX, IDCANCEL, m_Cancel);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTreeLayoutDlg, CDialog)
	//{{AFX_MSG_MAP(CTreeLayoutDlg)
	ON_BN_CLICKED(IDC_SELECT_ALL, OnSelectAll)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTreeLayoutDlg message handlers

BOOL CTreeLayoutDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();

  int height = m_list.GetItemHeight( 0 );
  m_list.SetItemHeight( 0, height - 2 );
  for ( int i = 0; i < resources.size(); ++i )
  {
    int ind = m_list.AddString( resources[i].second.c_str() );
    m_list.SetItemData( ind, i );
    if ( (*pActiveTabs)[i] )
      m_list.SetCheck( ind, 1 );
  }
  
  UpdateData( FALSE );
  
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTreeLayoutDlg::OnSelectAll() 
{
  m_list.SelItemRange( TRUE, 0, m_list.GetCount() );  
}

void CTreeLayoutDlg::OnOK() 
{
  UpdateData();
	if ( pActiveTabs )
	{
		const int nSize = pActiveTabs->size();
		for ( int i = 0; i < m_list.GetCount(); ++i )
		{
			int ind = m_list.GetItemData( i );
			if ( ind < 0 || ind >= nSize )
				continue;
			(*pActiveTabs)[ind] = m_list.GetCheck( i );
		}
	}
  
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelFolderDlg dialog


CDelFolderDlg::CDelFolderDlg( CString szName, bool _bNoDefault, CWnd* pParent /*=NULL*/)
	: CDialog(CDelFolderDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDelFolderDlg)
	m_szText = _T("");
	//}}AFX_DATA_INIT
	m_szText.LoadString( IDS_CONFIRM_DELFOLDER );
	m_szText += szName;
	m_szText += " \?\?!";
	bNoDefault = _bNoDefault;
}


void CDelFolderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDelFolderDlg)
	DDX_Text(pDX, IDC_DELFOLDER_TEXT, m_szText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDelFolderDlg, CDialog)
	//{{AFX_MSG_MAP(CDelFolderDlg)
	ON_BN_CLICKED(IDNO, OnNo)
	ON_BN_CLICKED(IDYES, OnYes)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelFolderDlg message handlers

BOOL CDelFolderDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CString szTitle;
	szTitle.LoadString( AFX_IDS_APP_TITLE );
	SetWindowText( szTitle );
	if ( bNoDefault )
	{
		SetDefID( IDNO );
	}
		
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDelFolderDlg::OnNo() 
{
	CDialog::OnCancel();
}

void CDelFolderDlg::OnYes() 
{
	CDialog::OnOK();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelItemsDlg dialog


CDelItemsDlg::CDelItemsDlg( const CString &szItems, CWnd* pParent /*=NULL*/)
	: CDialog(CDelItemsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDelItemsDlg)
	m_szItems = _T("");
	//}}AFX_DATA_INIT
	m_szItems = szItems;
}


void CDelItemsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDelItemsDlg)
	DDX_Text(pDX, IDC_DEL_ITEMS, m_szItems);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDelItemsDlg, CDialog)
	//{{AFX_MSG_MAP(CDelItemsDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDelItemsDlg message handlers

BOOL CDelItemsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CString szTitle;
	szTitle.LoadString( AFX_IDS_APP_TITLE );
	SetWindowText( szTitle );
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
