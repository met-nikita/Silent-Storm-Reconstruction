// DiplomacyEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "DiplomacyEditDlg.h"
#include "TreeView.h"
#include "dbDefs.h"
#include "OIDlg.h"

// CDiplomacyEditDlg dialog

IMPLEMENT_DYNAMIC(CDiplomacyEditDlg, CDialog)

CDiplomacyEditDlg::CDiplomacyEditDlg( int _nPrevSelected, CWnd *pParent )
: CDialog(CDiplomacyEditDlg::IDD, pParent), nItemID(-1), nPrevSelected(_nPrevSelected)
{
	m_pTree = new CMETreeView( vector<SResTree>( 1, *theApp.GetResTree( IDC_DIPLOMACY_TREE ) ) );
}

CDiplomacyEditDlg::~CDiplomacyEditDlg()
{
	if ( m_pTree )
		delete m_pTree;
	m_pTree = 0;
}

void CDiplomacyEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DIPLOMACY_PLACE, m_ctrlPlace);
	DDX_Control(pDX, IDC_SELTREE_PLACE, m_treeplace);
}


BEGIN_MESSAGE_MAP(CDiplomacyEditDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDC_REL_EMPTY, OnRelEmpty )
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyEditDlg message handlers

BOOL CDiplomacyEditDlg::OnInitDialog() 
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
	m_treeplace.GetWindowRect( &r );
	ScreenToClient( &r );
	if ( !::IsWindow( m_pTree->m_hWnd ) )
		m_pTree->CreateEx( WS_EX_CLIENTEDGE, 0, "tree", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, 1 );

	m_ctrlPlace.GetWindowRect( r );
	ScreenToClient( &r );
	if (!m_view.Create(NULL, "DiplomacyView", AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, NULL))
	m_view.ModifyStyleEx( 0, WS_EX_CLIENTEDGE );
	::SetWindowPos( m_view.m_hWnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER|SWP_NOACTIVATE);

	if ( -1 != nPrevSelected )
		m_pTree->SelectItem( IDC_DIPLOMACY_TREE, nPrevSelected );
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyEditDlg::OnOK() 
{
	int nSelectedTree;
	m_pTree->GetSelectedItemID( &nSelectedTree, &nItemID );
	nPrevSelected = nItemID;
	MSG msg;
	while( PeekMessage( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
		;
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CDiplomacyEditDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_ME_TREESEL:
		//PostMessage( WM_COMMAND, IDOK );
		return true;
	case WM_ME_TREESELCHANGED:
		m_view.SetActiveDiplomacy( pMsg->lParam );
		m_pTree->SetFocus();
		return true;
	case WM_ME_CANCEL:
		CDialog::OnCancel();
		break;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyEditDlg::OnRelEmpty() 
{
	nItemID = EMPTY_VALUE;
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////