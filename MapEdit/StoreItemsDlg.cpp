// StoreItemsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "StoreItemsDlg.h"
#include "TreeView.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "StoreItemsDB.h"

static const int SUB_RATING = 0;
static const int SUB_QUANTITY = 1;
static const int SUB_ITEM = 2;
// CStoreItemsDlg dialog

const int AXIS_SIDE = 1;
const int ALLIES_SIDE = 2;

static CStoreItemsDB db;

IMPLEMENT_DYNAMIC(CStoreItemsDlg, CDialog)
CStoreItemsDlg::CStoreItemsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CStoreItemsDlg::IDD, pParent)
{
	nSide = ALLIES_SIDE;
	m_pTree = 0;
}

CStoreItemsDlg::~CStoreItemsDlg()
{
}

void CStoreItemsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_PLACE, m_treePlace);
	DDX_Control(pDX, IDC_STORE_LIST, m_list);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CStoreItemsDlg::OnInitDialog()
{
	const SResTree *pTree = theApp.GetResTree( IDC_RPG_ITEMS_TREE );
	if ( !pTree )
		return FALSE;
	rpgitems = *pTree;
	if ( !m_pTree )
		m_pTree = new CMETreeView( vector<SResTree>( 1, rpgitems ) );

	if ( !CDialog::OnInitDialog() )
		return FALSE;

	CRect r;
	m_treePlace.GetWindowRect( &r );
	ScreenToClient( &r );
	m_pTree->CreateEx( WS_EX_CLIENTEDGE, 0, "tree", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, 1 );

	m_list.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT );
	m_list.InsertColumn( SUB_RATING, "Rating", LVCFMT_CENTER, 55, SUB_RATING );
	m_list.InsertColumn( SUB_QUANTITY, "Quantity", LVCFMT_CENTER, 60, SUB_QUANTITY );
	m_list.InsertColumn( SUB_ITEM, "Item", LVCFMT_LEFT, 200, SUB_ITEM );

	SetSide( nSide );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CStoreItemsDlg, CDialog)
	ON_BN_CLICKED(IDC_ADD_BUTTON, OnBnClickedAddButton)
	ON_BN_CLICKED(IDC_REMOVE_BUTTON, OnBnClickedRemoveButton)
	ON_NOTIFY(NM_DBLCLK, IDC_STORE_LIST, OnNMDblclkStoreList)
	ON_BN_CLICKED(IDC_ALLIES, OnBnClickedAllies)
	ON_BN_CLICKED(IDC_AXIS, OnBnClickedAxis)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_STORE_LIST, OnLvnEndlabeleditStoreList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_STORE_LIST, OnLvnKeydownStoreList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_STORE_LIST, OnLvnItemchangedStoreList)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::ChangeItemQuantity( int nDelta, int iDefItem, bool bCanDelete )
{
	int iItem = iDefItem != -1 ? iDefItem : m_list.GetSelectionMark();
	if ( iItem != -1 )
	{
		int nRecordID = m_list.GetItemData( iItem );
		CString str = m_list.GetItemText( iItem, SUB_QUANTITY );
		int nQuantity = atoi( (LPCSTR)str );
		int nNewQ = nQuantity + nDelta;
		if ( !bCanDelete )
			nNewQ = Max( 1, nNewQ );
		if ( nNewQ <= 0 )
		{
			if ( db.DeleteItem( nRecordID ) )
				m_list.DeleteItem( iItem );
		}
		else
		{
			if ( db.SetItemQuantity( nRecordID, nNewQ ) )
				m_list.SetItemText( iItem, SUB_QUANTITY, IToA( nNewQ ).c_str() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStoreItemsDlg message handlers
void CStoreItemsDlg::OnBnClickedAddButton()
{
	DropTreeItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnBnClickedRemoveButton()
{
	ChangeItemQuantity( -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnNMDblclkStoreList(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	int iItem = m_list.GetSelectionMark();
	if ( iItem != -1 )
		m_list.EditLabel( iItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnBnClickedAllies()
{
	SetSide( ALLIES_SIDE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnBnClickedAxis()
{
	SetSide( AXIS_SIDE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::SetSide( int _nSide )
{
	nSide = _nSide;
	CButton *pB = 0;
	switch ( nSide )
	{
		case AXIS_SIDE:
			pB = (CButton*)GetDlgItem( IDC_AXIS );
			break;
		case ALLIES_SIDE:
			pB = (CButton*)GetDlgItem( IDC_ALLIES );
			break;
		default:
			ASSERT(0);
			break;
	}
	if ( pB )
		pB->SetCheck( 1 );
	//
	if ( !db.OpenSideItems( nSide ) )
		return;
	m_list.DeleteAllItems();
	while ( db.MoveNext() == S_OK )
		InsertItem( db.m_nID, db.m_nItemID, db.m_nRating, db.m_nQuantity );
	//
	Sort();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::InsertItem( int nRecordID, int nItemID, int nRating, int nQuantity )
{
	string szItem = rpgitems.pItemsTree->GetItemPath( nItemID );
	string szRating = IToA( nRating );
	int iItem = m_list.InsertItem( LVIF_TEXT | LVIF_PARAM, m_list.GetItemCount(), szRating.c_str(), 0, 0, 0, nRecordID );
	m_list.SetItemText( iItem, SUB_QUANTITY, IToA( nQuantity ).c_str() );
	m_list.SetItemText( iItem, SUB_ITEM, szItem.c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnLvnEndlabeleditStoreList(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	*pResult = 0;
	if ( /*pDispInfo->item.iSubItem == SUB_QUANTITY &&*/ pDispInfo->item.pszText )
	{
		int nRating = atoi( pDispInfo->item.pszText );
		int nRecordID = m_list.GetItemData( pDispInfo->item.iItem );
		if ( db.SetItemRating( nRecordID, nRating ) )
		{
			static char buf[64];
			sprintf( buf, "%d", nRating );
			pDispInfo->item.pszText = buf;
			*pResult = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::OnLvnKeydownStoreList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	*pResult = 0;

	switch ( pLVKeyDow->wVKey )
	{
		case VK_DELETE:
		{
			int iItem = m_list.GetSelectionMark();
			if ( iItem != -1 )
			{
				int nRecordID = m_list.GetItemData( iItem );
				if ( db.DeleteItem( nRecordID ) )
					m_list.DeleteItem( iItem );
			}
		}
		case VK_OEM_PLUS:
		case VK_ADD:
			ChangeItemQuantity( 1 );
			break;
		case VK_SUBTRACT:
		case VK_OEM_MINUS:
			ChangeItemQuantity( -1, -1, false );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreItemsDlg::DropTreeItem()
{
	int nTree, nRPGItem;
	m_pTree->GetSelectedItemID( &nTree, &nRPGItem );
	const SResTree *pTree = theApp.GetResTree( IDC_RPG_STOREITEMS_TREE );
	if ( !pTree )
		return;
	for ( int i = 0; i < m_list.GetItemCount(); ++i )
	{
		int nID = m_list.GetItemData( i );
		const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator it = pProps->find( "ItemID" );
		if ( it != pProps->end() && (int)it->second->GetValue() == nRPGItem )
		{
			pTree->pItemsTree->ReleasePropList( pProps );
			ChangeItemQuantity( 1, i );
			return;
		}
	}
	int nRecordID = db.InsertItem( nRPGItem, nSide, 1 );
	InsertItem( nRecordID, nRPGItem, 0, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CStoreItemsDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_ME_TREESEL:
		DropTreeItem();
		return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	int i1 = db.GetItemRating( lParam1 );
	int i2 = db.GetItemRating( lParam2 );
	return i1 - i2;
}
void CStoreItemsDlg::Sort()
{
	m_list.SortItems(MyCompareProc, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

void CStoreItemsDlg::OnLvnItemchangedStoreList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if ( pNMLV->iItem != -1 && (pNMLV->uChanged & LVIF_TEXT) )
		Sort();
	*pResult = 0;
}
