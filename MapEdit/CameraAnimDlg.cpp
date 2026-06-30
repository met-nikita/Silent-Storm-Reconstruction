// CameraAnimDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "CameraAnimDlg.h"
#include "dbDefs.h"
#include "TreeView.h"
#include "WysiwygCamera.h"
#include "TextEditor.h"

// CCameraAnimDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CCameraAnimDlg, CDialog)
CCameraAnimDlg::CCameraAnimDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCameraAnimDlg::IDD, pParent)
{
	const SResTree *pTree = theApp.GetResTree( IDC_CAMERAS_TREE );

	m_pTree = new CMETreeView( vector<SResTree>( 1, *pTree ) );
	nLastSelected = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCameraAnimDlg::~CCameraAnimDlg()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_PLACE, m_treePlace);
	DDX_Control(pDX, IDC_KEYFRAMES, m_list);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CCameraAnimDlg, CDialog)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_KEYFRAMES, OnLvnEndlabeleditKeyframes)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_NOTIFY(NM_DBLCLK, IDC_KEYFRAMES, OnNMDblclkKeyframes)
	ON_NOTIFY(LVN_KEYDOWN, IDC_KEYFRAMES, OnLvnKeydownKeyframes)
	ON_BN_CLICKED(IDC_GENERATE_SCRIPT, OnBnClickedGenerateScript)
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CCameraAnimDlg::OnInitDialog()
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
	m_pTree->SelectItem( IDC_CAMERAS_TREE, nLastSelected );
	//
	m_list.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT );
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.pszText = "Transition time";
	column.cx = 100;
	column.iSubItem = 0;
	m_list.InsertColumn( 0, &column );
	column.pszText = "Position";
	column.iSubItem = 1;
	m_list.InsertColumn( 1, &column );
	//
	const SResTree *pTree = theApp.GetResTree( IDC_CAMERAS_TREE );
	for ( int i = 0; i < transitions.size(); ++i )
	{
		int n = m_list.InsertItem( i, IToA( transitions[i].sTransitionTime ).c_str() );
		m_list.SetItemData( i, transitions[i].nCameraID );
		m_list.SetItemText( i, 1, pTree->pItemsTree->GetItemPath( transitions[i].nCameraID ).c_str() );
	}
	//
	return TRUE; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CCameraAnimDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_ME_TREESEL:
		{
			int nTree, nID;
			if ( m_pTree->GetSelectedItemID( &nTree, &nID ) )
			{
				nLastSelected = nID;
				const SResTree *pTree = theApp.GetResTree( IDC_CAMERAS_TREE );
				CString szTime = m_list.GetItemCount() == 0 ? "0" : "1000";
				int n = m_list.InsertItem( m_list.GetItemCount(), szTime );
				m_list.SetItemData( n, nID );
				m_list.SetItemText( n, 1, pTree->pItemsTree->GetItemPath( nID ).c_str() );
			}
		}
		return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::OnLvnEndlabeleditKeyframes(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	if ( !pDispInfo->item.pszText )
		return;
	int nTime = atoi( pDispInfo->item.pszText );
	m_list.SetItemText( pDispInfo->item.iItem, 0, IToA( nTime ).c_str() );
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::OnBnClickedOk()
{
	transitions.clear();
	int nCount = m_list.GetItemCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CString szTime = m_list.GetItemText( i, 0 );
		if ( szTime == "" )
			continue;
		int nData = m_list.GetItemData( i );
		transitions.push_back( NWysiwyg::SDBCamera( nData, atoi( szTime ) ) );
	}
	OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::OnNMDblclkKeyframes(NMHDR *pNMHDR, LRESULT *pResult)
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos == NULL)
		TRACE0("No items were selected!\n");
	else
	{
		int nItem = m_list.GetNextSelectedItem(pos);
		m_list.EditLabel( nItem );
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::OnLvnKeydownKeyframes(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	if ( pLVKeyDow->wVKey == VK_DELETE )
	{
		POSITION pos = m_list.GetFirstSelectedItemPosition();
		if (pos == NULL)
			TRACE0("No items were selected!\n");
		else
		{
			int nItem = m_list.GetNextSelectedItem(pos);
			m_list.DeleteItem( nItem );
		}
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCameraAnimDlg::OnBnClickedGenerateScript()
{
	CString szScript;
	int nCount = m_list.GetItemCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CString szTime = m_list.GetItemText( i, 0 );
		if ( szTime == "" )
			continue;
		int nData = m_list.GetItemData( i );
		nData, atoi( szTime );
		szScript += CString( "CameraMove( GetCamera( " ) + IToA( nData ).c_str() + " ), " + szTime + " )\n";
	}
	//
	CTextEditor dlg( true );

	dlg.SetText( (LPCSTR)szScript );
	dlg.DoModal();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
