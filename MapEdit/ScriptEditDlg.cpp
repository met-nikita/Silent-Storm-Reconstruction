// ScriptEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "ScriptEditDlg.h"
#include "ItemsMgr.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "TreeView.h"
#include "dbDefs.h"
#include "MEParams.h"
#include "MEUserSettings.h"

// CScriptEditDlg dialog

IMPLEMENT_DYNAMIC(CScriptEditDlg, CDialog)
CScriptEditDlg::CScriptEditDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScriptEditDlg::IDD, pParent), nItemID(-1), nPrevSelected(-1), m_pTree(0)
{
}

CScriptEditDlg::~CScriptEditDlg()
{
}

void CScriptEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SELTREE_PLACE, m_treePlace);
	DDX_Control(pDX, IDC_SCRIPT_PLACE, m_ScriptPlace);
}


BEGIN_MESSAGE_MAP(CScriptEditDlg, CDialog)
	ON_BN_CLICKED(IDC_REL_EMPTY, OnRelEmpty)
END_MESSAGE_MAP()


BOOL CScriptEditDlg::OnInitDialog() 
{
	m_pTree = new CMETreeView( vector<SResTree>( 1, *theApp.GetResTree( IDC_SCRIPTS_TREE ) ) );

	if ( !CDialog::OnInitDialog() )
		return false;
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

	m_ScriptPlace.GetWindowRect( r );
	ScreenToClient( &r );
	mEdit.Create( CTextEditor::IDD, this );
	mEdit.ModifyStyle( WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, WS_CHILD );
	mEdit.ModifyStyleEx( WS_EX_DLGMODALFRAME, WS_EX_CLIENTEDGE );
	mEdit.SetParent( this );
	mEdit.ShowWindow( SW_SHOW );
	mEdit.CheckSyntax( true );
	::SetWindowPos( mEdit.m_hWnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER|SWP_NOACTIVATE);
	mEdit.SetModal( false );
	CButton *pOk = (CButton*)mEdit.GetDlgItem( IDOK );
	CButton *pCancel = (CButton*)mEdit.GetDlgItem( IDCANCEL );
	if ( pOk && pCancel )
	{
		pOk->ShowWindow( SW_HIDE );
		pCancel->ShowWindow( SW_HIDE );
	}

	if ( -1 != nPrevSelected )
		m_pTree->SelectItem( IDC_SCRIPTS_TREE, nPrevSelected );
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// CScriptEditDlg message handlers

void CScriptEditDlg::OnRelEmpty()
{
	SetMapScriptID( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptEditDlg::OnOK() 
{
	int nSelectedTree;
	m_pTree->GetSelectedItemID( &nSelectedTree, &nItemID );
	nPrevSelected = nItemID;
	SetMapScriptID( nItemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptEditDlg::OnCancel() 
{
	NDatabase::Refresh<NDb::CScript>();
	NDatabase::Refresh<NDb::CTemplVariant>();
	CDialog::OnCancel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptEditDlg::SetMapScriptID( int nID )
{
	int nTree, nItem, nVar;
	theApp.GetActiveItem( &nTree, &nItem, &nVar );
	if ( nTree != IDC_TEMPLATE_TREE )
		return;
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItem, nVar );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "ScriptID" );
	if ( i != pProps->end() )
		i->second->SetValue( nID > 0 ? nID : CVariant() );
	pTree->pItemsTree->ReleasePropList( pProps );
	//
	NDatabase::Refresh<NDb::CScript>();
	NDatabase::Refresh<NDb::CTemplVariant>();
	//
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nVar );
	if ( pVar )
		pVar->pScript = NDb::GetDBScript( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptEditDlg::SetScriptID( int nID )
{
	nItemID = nID;
	nPrevSelected = nID;
	m_pTree->SelectItem( IDC_SCRIPTS_TREE, nItemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CScriptEditDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_ME_TREESEL:
			//PostMessage( WM_COMMAND, IDOK );
			return true;
		case WM_ME_TREESELCHANGED:
			ShowScriptText( pMsg->lParam );
			m_pTree->SetFocus();
			return true;
			break;
		case WM_ME_TEXTCHANGED:
			{
				const SResTree *pTree = theApp.GetResTree( IDC_SCRIPTS_TREE );
				if ( pTree )
				{
					const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItemID );
					if ( pProps )
					{
						CPropMap::const_iterator i = pProps->find( "CodeText" );
						if ( i != pProps->end() )
							i->second->SetValue( mEdit.GetText() );
						pTree->pItemsTree->ReleasePropList( pProps );
					}
				}
			}
			return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptEditDlg::ShowScriptText( int nID )
{
	mEdit.CheckSyntax( GetUserSettings().GetParam( ME_SCRIPT_SYNAXCOLORING ) );

	const SResTree *pTree = theApp.GetResTree( IDC_SCRIPTS_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "CodeText" );
	if ( i != pProps->end() )
	{
		nItemID = nID;
		mEdit.SetText( (string)i->second->GetValue() );
		mEdit.UpdateData( FALSE );
	}
	pTree->pItemsTree->ReleasePropList( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////