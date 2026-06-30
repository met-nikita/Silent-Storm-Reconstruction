// NameAndFolder.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "NameAndFolder.h"
#include "TreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNameAndFolder dialog


CNameAndFolder::CNameAndFolder(const SResTree *pResTree, CWnd* pParent /*=NULL*/)
	: CDialog(CNameAndFolder::IDD, pParent)
{
	m_pTree = new CMETreeView( vector<SResTree>( 1, *pResTree ) );
	//{{AFX_DATA_INIT(CNameAndFolder)
	//}}AFX_DATA_INIT
	nSelectedItem = -1;
}

CNameAndFolder::~CNameAndFolder()
{
	if ( m_pTree )
		delete m_pTree;
	m_pTree = 0;
}

void CNameAndFolder::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNameAndFolder)
	DDX_Control(pDX, IDC_SELTREE_PLACE, m_treePlace);
	DDX_Control(pDX, IDOK, m_okButton);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNameAndFolder, CDialog)
	//{{AFX_MSG_MAP(CNameAndFolder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNameAndFolder message handlers

BOOL CNameAndFolder::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	static CPoint pt( 0, 0 );
	CRect r;
	
	//
	pt += CPoint( 10, 10 );
	pt.x %= 50;
	pt.y %= 50;
	::GetClientRect( theApp.GetMainWnd()->m_hWnd, &r );
	CPoint ptLT( r.Width() / 2, r.Height() / 2 );
	GetWindowRect( &r );
	ptLT -= CPoint( r.Width() / 2, r.Height() / 2 );
	r -= r.TopLeft();
	MoveWindow( r + pt + ptLT, FALSE );
	//
	m_treePlace.GetWindowRect( &r );
	ScreenToClient( &r );
	if ( !::IsWindow( m_pTree->m_hWnd ) )
		m_pTree->CreateEx( WS_EX_CLIENTEDGE, 0, "tree", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, 1 );

//	m_okButton.EnableWindow( false );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CNameAndFolder::OnOK() 
{
	int nTree;
	m_pTree->GetSelectedItemID( &nTree, &nSelectedItem );
	CDialog::OnOK();
}
