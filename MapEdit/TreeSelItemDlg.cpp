// TreeSeltemDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "TreeSelItemDlg.h"
#include "TreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTreeSelItemDlg dialog


CTreeSelItemDlg::CTreeSelItemDlg( const vector<SResTree> &_vResTrees, int _nPrevSelectedTree, int _nPrevSelectedID, CWnd *pParent )
	: CDialog(CTreeSelItemDlg::IDD, pParent), nPrevSelectedTree(_nPrevSelectedTree), nPrevSelectedID( _nPrevSelectedID )
{
	vResTrees = _vResTrees;
	m_pTree = new CMETreeView( vResTrees );
	ASSERT( !vResTrees.empty() );
	//{{AFX_DATA_INIT(CTreeSelItemDlg)
	m_szPrevSelection = _T("");
	//}}AFX_DATA_INIT
	nSelectedID = -1;
	pPropMap = 0;
	bReadOnly = false;
}

CTreeSelItemDlg::~CTreeSelItemDlg()
{
	if ( m_pTree )
		delete m_pTree;
	m_pTree = 0;
}

void CTreeSelItemDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTreeSelItemDlg)
	DDX_Control(pDX, IDC_SELTREE_PLACE, m_treeplace);
	DDX_Text(pDX, IDC_TREE_PREV_SELECTION, m_szPrevSelection);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTreeSelItemDlg, CDialog)
	//{{AFX_MSG_MAP(CTreeSelItemDlg)
	ON_BN_CLICKED(IDC_REL_EMPTY, OnRelEmpty)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTreeSelItemDlg message handlers

BOOL CTreeSelItemDlg::OnInitDialog() 
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

  // Init object inspector
	if ( !::IsWindow( m_OIDlg.m_hWnd ) )
	{
		CWnd *pPlace = GetDlgItem( IDC_SELPROPMAP_PLACE );
		if ( pPlace )
		{
	    pPlace->GetWindowRect( r );
			ScreenToClient( &r );
			m_OIDlg.Create( IDD_OBJINSPECTOR, this );
			m_OIDlg.ModifyStyleEx( 0, WS_EX_CLIENTEDGE );
			//m_OIDlg.CreateEx( WS_EX_CLIENTEDGE, 0, "props", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, IDD_OBJINSPECTOR );
			::SetWindowPos( m_OIDlg.m_hWnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER|SWP_NOACTIVATE);
		}
	}

	if ( -1 != nPrevSelectedID )
	{
		if ( m_pTree->SelectItem( nPrevSelectedTree, nPrevSelectedID ) )
		{
			m_szPrevSelection = (m_pTree->GetItemPath( nPrevSelectedTree, nPrevSelectedID )).c_str();
			UpdateData( FALSE );
		}
	}
	CButton *pOk = (CButton*)GetDlgItem( IDOK );
	CButton *pEmpty = (CButton*)GetDlgItem( IDC_REL_EMPTY );
	if ( pOk && pEmpty )
	{
		pOk->EnableWindow( !bReadOnly );
		pEmpty->EnableWindow( !bReadOnly );
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::OnOK() 
{
	m_pTree->GetSelectedItemID( &nSelectedTree, &nSelectedID );
	nPrevSelectedID = nSelectedID;
	MSG msg;
	while( PeekMessage( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
		;
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::GetSelectedItemID( int *pnTree, int *pnID )
{
	*pnTree = nSelectedTree;
	*pnID = nSelectedID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTreeSelItemDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_ME_TREESEL:
			//PostMessage( WM_COMMAND, IDOK );
			return true;
		case WM_ME_TREESELCHANGED:
			SetPropMap( pMsg->wParam, pMsg->lParam );
			m_pTree->SetFocus();
			return true;
		case WM_ME_CANCEL:
			CDialog::OnCancel();
			break;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::OnRelEmpty() 
{
	nSelectedID = EMPTY_VALUE;
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::SetSelectedItemID( int nTreeID, int id ) 
{ 
	if ( (-1 == id || 0 == id ) && nPrevSelectedID > 0 )
		return;
	nPrevSelectedTree = nTreeID;
	nPrevSelectedID	= id; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::SetPropMap( int nTreeID, int nItemID )
{
	for ( int i = 0; i < vResTrees.size(); ++i )
	{
		if ( vResTrees[i].nTreeID != nTreeID )
			continue;
		const CPropMap *pNewProps = vResTrees[i].pItemsTree->GetPropList( nItemID );
		m_OIDlg.SetPropMap( nTreeID, pNewProps );
		if ( pPropMap )
			vResTrees[i].pItemsTree->ReleasePropList( pPropMap );
		pPropMap = pNewProps;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeSelItemDlg::SetReadOnly( bool _bReadOnly )
{
	bReadOnly = _bReadOnly;
}
////////////////////////////////////////////////////////////////////////////////////////////////////