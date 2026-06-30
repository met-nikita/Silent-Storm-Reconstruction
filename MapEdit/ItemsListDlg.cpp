// ItemsListDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "ItemsListDlg.h"
#include "Items4PersDB.h"
#include "TreeView.h"


static CItems4PersDB db;
static const int SUB_QUANTITY = 0;
static const int SUB_ITEM = 1;
// CItemsListDlg dialog

IMPLEMENT_DYNAMIC(CItemsListDlg, CDialog)
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemsListDlg::CItemsListDlg( int nPersID, int nItemsTree, const string &_szItems4PersTbl, CWnd* pParent /*=NULL*/)
	: CDialog(CItemsListDlg::IDD, pParent), nRPGPersID(nPersID), szItems4PersTbl(_szItems4PersTbl), m_pTree(0)
{
	pItems = theApp.GetResTree( nItemsTree );
	if ( pItems )
		m_pTree = new CMETreeView( vector<SResTree>( 1, *pItems ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemsListDlg::~CItemsListDlg()
{
	if ( m_pTree )
		delete m_pTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ITEMS_TREE_PLACE, m_ctrlItemTreePlace);
	DDX_Control(pDX, IDC_ITEMS_LIST, m_ctrlItems);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CItemsListDlg, CDialog)
	ON_BN_CLICKED(IDC_ADD_ITEM, OnBnClickedAddItem)
	ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_ITEMS_LIST, OnLvnBeginlabeleditItemsList)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_ITEMS_LIST, OnLvnEndlabeleditItemsList)
	ON_NOTIFY(LVN_DELETEITEM, IDC_ITEMS_LIST, OnLvnDeleteitemItemsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_ITEMS_LIST, OnLvnKeydownItemsList)
END_MESSAGE_MAP()

LRESULT CALLBACK EXPORT
CItemsListDlg::WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// create new item and attach it
	CItemsListDlg* pDlg = new CItemsListDlg();
	pDlg->Attach(hWnd);

	BOOL bRet = pDlg->OnInitDialog();
	// set up wndproc to AFX one, and call it
	//pDlg->m_pfnSuper = CParsedEditExported::lpfnSuperEdit;
	::SetWindowLongPtr(hWnd, GWL_WNDPROC, (LONG_PTR)AfxWndProc);

	// then call it for this first message
	return ::CallWindowProc(AfxWndProc, hWnd, msg, wParam, lParam);
}
void CItemsListDlg::PostNcDestroy()
{
//	delete this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CItemsListDlg::RegisterControlClass()
{
	WNDCLASS wcls;

	// check to see if class already registered
	static const TCHAR szClass[] = _T("rpgitemlist");
	if (::GetClassInfo(AfxGetInstanceHandle(), szClass, &wcls))
	{
		//return true;
		// name already registered - ok if it was us
		return (wcls.lpfnWndProc == (WNDPROC)CItemsListDlg::WndProcHook);
	}

	// Use standard "edit" control as a template.
	VERIFY(::GetClassInfo(NULL, _T("edit"), &wcls));
	//CItemsListDlg::lpfnSuperEdit = wcls.lpfnWndProc;

	// set new values
	wcls.lpfnWndProc = CItemsListDlg::WndProcHook;
	wcls.hInstance = AfxGetInstanceHandle();
	wcls.lpszClassName = szClass;
	return (RegisterClass(&wcls) != 0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::SetRPGPersID( int nPersID )
{
	nRPGPersID = nPersID;

	if ( !::IsWindow( m_hWnd ) )
		return;
	//
	m_ctrlItems.DeleteAllItems();
	vector<SItem> items;
	if ( db.GetItems( &items, szItems4PersTbl, nRPGPersID ) )
	{
		for ( int i = 0; i < items.size(); ++i )
		{
			string szItem = pItems->pItemsTree->GetItemPath( items[i].nItemID );
			int iItem = m_ctrlItems.InsertItem( LVIF_TEXT | LVIF_PARAM, i, IToA( items[i].nQuantity ).c_str(), 0, 0, 0, items[i].nItemID );
			m_ctrlItems.SetItemText( iItem, SUB_ITEM, szItem.c_str() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CItemsListDlg::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() || !pItems )
		return FALSE;

	CRect r;
	m_ctrlItemTreePlace.GetWindowRect( &r );
	ScreenToClient( &r );
	m_pTree->CreateEx( WS_EX_CLIENTEDGE, 0, "tree", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, 1 );

	m_ctrlItems.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT );
	m_ctrlItems.InsertColumn( 0, "Quantity", LVCFMT_CENTER, 55 );
	m_ctrlItems.InsertColumn( 1, "Item", LVCFMT_LEFT, 200, SUB_ITEM );

	SetRPGPersID( nRPGPersID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemsListDlg message handlers
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnBnClickedAddItem()
{
	if ( !m_pTree )
		return;
	int nTree, nItem;
	if ( !m_pTree->GetSelectedItemID( &nTree, &nItem ) )
		return;
	LVFINDINFO info;

	info.flags = LVFI_PARAM;
	info.lParam = nItem;
	int iItem = m_ctrlItems.FindItem( &info );
	int nQuantity = 0;
	if ( iItem != -1 )
	{
		nQuantity = db.GetItemQuantity( szItems4PersTbl, nRPGPersID, nItem );
		if ( -1 == nQuantity )
			nQuantity = 0;
	}
	else
	{
		iItem = m_ctrlItems.InsertItem( LVIF_TEXT | LVIF_PARAM, m_ctrlItems.GetItemCount(), "0", 0, 0, 0, nItem );
		m_ctrlItems.SetItemText( iItem, SUB_ITEM, pItems->pItemsTree->GetItemPath( nItem ).c_str() );
	}
	++nQuantity;
	if ( db.SetItemQuantity( szItems4PersTbl, nRPGPersID, nItem, nQuantity ) )
		m_ctrlItems.SetItemText( iItem, 0, IToA( nQuantity ).c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnLvnBeginlabeleditItemsList(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	
	//if ( pDispInfo->item.iSubItem != SUB_QUANTITY )
//		m_ctrlItems.CancelEditLabel();

	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnLvnEndlabeleditItemsList(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	*pResult = 0;
	if ( /*pDispInfo->item.iSubItem == SUB_QUANTITY &&*/ pDispInfo->item.pszText )
	{
		int nQuantity = atoi( pDispInfo->item.pszText );
		int nItemID = m_ctrlItems.GetItemData( pDispInfo->item.iItem );
		if ( db.SetItemQuantity( szItems4PersTbl, nRPGPersID, nItemID, nQuantity ) )
		{
			static char buf[64];
			sprintf( buf, "%d", nQuantity );
			pDispInfo->item.pszText = buf;
			*pResult = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnLvnDeleteitemItemsList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnLvnKeydownItemsList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	if ( pLVKeyDow->wVKey == VK_DELETE )
	{
		int iItem = m_ctrlItems.GetSelectionMark();
		if ( iItem != -1 )
		{
			int nItemID = m_ctrlItems.GetItemData( iItem );
			if ( db.SetItemQuantity( szItems4PersTbl, nRPGPersID, nItemID, 0 ) )
				m_ctrlItems.DeleteItem( iItem );
		}
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsListDlg::OnCancel()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CItemsListDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_ME_TREESEL:
			OnBnClickedAddItem();
			return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsListPropPage dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CRPGItemsListPropPage, CPropertyPage)
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGItemsListPropPage::CRPGItemsListPropPage( UINT nIDCaption, int nRPGPersID, int nItemsTree, const string &szItems4PersTbl )
	: CPropertyPage(CRPGItemsListPropPage::IDD, nIDCaption ),
	m_list( nRPGPersID, nItemsTree, szItems4PersTbl, this )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGItemsListPropPage::~CRPGItemsListPropPage()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemsListPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PLACE, m_place);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CRPGItemsListPropPage, CPropertyPage)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsListPropPage message handlers
BOOL CRPGItemsListPropPage::OnInitDialog()
{
	if ( !CPropertyPage::OnInitDialog() )
		return FALSE;

	if ( !m_list.Create( IDD_RPGPERS_ITEMS, this ) )
		return FALSE;
	CRect r;
	m_place.GetWindowRect( &r );
	ScreenToClient( &r );
	m_list.MoveWindow( &r );
	m_list.ShowWindow( SW_SHOW );
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
