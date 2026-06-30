// ParamsView.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "ParamsView.h"
#include "Attributes.h"
#include "ItemsMgr.h"
#include "..\Misc\StrProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int IDC_PARAMLIST_CTRL = 100;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsView

CParamsView::CParamsView() : pAttrList( new CAttributesList )
{
}

CParamsView::~CParamsView()
{
	delete pAttrList;
}


BEGIN_MESSAGE_MAP(CParamsView, CWnd)
	//{{AFX_MSG_MAP(CParamsView)
	ON_NOTIFY(NM_CLICK, IDC_PARAMLIST_CTRL, OnClickParamlist)	
	ON_NOTIFY(NM_DBLCLK, IDC_PARAMLIST_CTRL, OnDblClickParamlist)	
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsView message handlers
const int COL_WIDTH = 30;
int CParamsView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rect;
	int nWidth = 70;
	
	GetClientRect( &rect );
	
	DWORD dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | 
		LVS_EDITLABELS | LVS_NOSORTHEADER;
	
	m_wndList.Create( dwStyle, rect, this, IDC_PARAMLIST_CTRL );
	m_wndList.SetExtendedStyle( LVS_EX_GRIDLINES );
	m_wndList.GetClientRect( &rect );
	// вставляем первую колонку с именами атрибутов
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.pszText = "Flags";
	column.cx = nWidth;
	column.iSubItem = 0;
	m_wndList.InsertColumn( 0, &column );
	// читаем атрибуты
	pAttrList->ReadListFromDB();
	UpdateAttributes();
		
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::UpdateAttributes()
{
	CAttrMap attrs;
	pAttrList->GetList( &attrs );

	m_wndList.DeleteAllItems();
	for ( CAttrMap::const_iterator i = attrs.begin(); i != attrs.end(); ++i )
		m_wndList.InsertItem( LVIF_TEXT | LVIF_PARAM, 0, i->first.c_str(), 0, 0, 0, i->second );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	
	m_wndList.MoveWindow( 0, 0, cx, cy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	ToggleFlag( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	ToggleFlag( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::ToggleFlag( NM_LISTVIEW *pNMListView )
{
	if ( pNMListView->iSubItem == 0 )
		return;
	
	CPoint ptAction = pNMListView->ptAction;
	ptAction.x = 4;
	int iItem = m_wndList.HitTest( ptAction );
	
	CString str = m_wndList.GetItemText( iItem, pNMListView->iSubItem );
	m_wndList.SetItemText( iItem, pNMListView->iSubItem, str == "+" ? "-" : "+" );

	CHeaderCtrl *pHdr = m_wndList.GetHeaderCtrl();
	HDITEM hdrItem;
	hdrItem.mask = HDI_LPARAM;
	if ( !pHdr->GetItem( pNMListView->iSubItem, &hdrItem ) )
		return;

	const SResTree *pTree = theApp.GetResTree( aItem.nTreeID );
	if ( !pTree )
		return;
	const int nCol = pHdr->GetItemCount();
	const int nAttrs = m_wndList.GetItemCount();
	const int nVarID = hdrItem.lParam;
	vector<unordered_map<int, bool> > flags;
	for ( int i = 0; i < nCol; ++i )
	{
		pHdr->GetItem( i, &hdrItem );
		if ( nVarID == hdrItem.lParam )
		{
			unordered_map<int, bool> flagmap;
			for ( int j = 0; j < nAttrs; ++j )
				if ( string( "+" ) == (LPCTSTR)m_wndList.GetItemText( j, i ) )
					flagmap[m_wndList.GetItemData( j )] = true;
			flags.push_back( flagmap );
		}
	}
	pTree->pItemsTree->SetVariantFlags( nVarID, flags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::SetActiveItem( int nTreeID, int nItemID )
{
	int nColumnCount = m_wndList.GetHeaderCtrl()->GetItemCount();
	aItem = SActiveItem();

	// Delete all of the columns.
	for (int i = 1; i < nColumnCount; ++i )
		m_wndList.DeleteColumn( 1 );
	//
	const SResTree *pTree = theApp.GetResTree( nTreeID );
	if ( !pTree || !pTree->pItemsTree || !pTree->pItemsTree->IsUniTemplate() )
		return;
	//
	pAttrList->ReadListFromDB();
	UpdateAttributes();
	
	vector<int> variants;
	if ( !pTree->pItemsTree->GetItemVariants( nItemID, &variants ) )
		return;
	for ( int i = 0; i < variants.size(); ++i )
	{
		vector<unordered_map<int, bool> > flags;
		if ( !pTree->pItemsTree->GetVariantFlags( variants[i], &flags ) )
			continue;
		for ( int j = 0; j < flags.size(); ++j )
			AddVariantFlags( variants[i], flags[j] );
		if ( flags.empty() )
		{
			unordered_map<int, bool> nullflags;
			AddVariantFlags( variants[i], nullflags );
		}
	}
	aItem.nTreeID = nTreeID;
	aItem.nItemID = nItemID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::AddVariantFlags( int nVarID, const unordered_map<int, bool> &flags )
{
	int nSub = m_wndList.GetHeaderCtrl()->GetItemCount();
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.cx = 50;
	column.iSubItem = nSub;
	column.cx = COL_WIDTH;
	char buf[MAX_DBSTRING];
	strcpy( buf, IToA( nVarID ).c_str() );
	column.pszText = buf;
	nSub = m_wndList.InsertColumn( nSub, &column );

	HDITEM hdrItem;
	hdrItem.mask = HDI_LPARAM;
	hdrItem.lParam = nVarID;
	m_wndList.GetHeaderCtrl()->SetItem( nSub, &hdrItem );
	
	const int nAttrs = m_wndList.GetItemCount();
	for ( int i = 0; i < nAttrs; ++i )
	{
		int nID = m_wndList.GetItemData( i );
		unordered_map<int, bool>::const_iterator it = flags.find( nID );
		if ( flags.end() == it || !it->second )
			m_wndList.SetItemText( i, nSub, "-" );
		else
			m_wndList.SetItemText( i, nSub, "+" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParamsView::SetFlags( const string &szFlags )
{
	int nSub = m_wndList.GetHeaderCtrl()->GetItemCount();
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.cx = 50;
	column.iSubItem = nSub;
	column.cx = COL_WIDTH;
	column.pszText = "";
	nSub = m_wndList.InsertColumn( nSub, &column );

	vector<string> vStrings;
	NStr::SplitString( szFlags, vStrings, ';' );
	const int nAttrs = m_wndList.GetItemCount();
	for ( int i = 0; i < nAttrs; ++i )
	{
		CString szAttr = m_wndList.GetItemText( i, 0 );
		vector<string>::const_iterator it = find( vStrings.begin(), vStrings.end(), (LPCSTR)szAttr );
		if ( vStrings.end() == it )
			m_wndList.SetItemText( i, nSub, "-" );
		else
			m_wndList.SetItemText( i, nSub, "+" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CParamsView::GetFlags()
{
	CHeaderCtrl *pHdr = m_wndList.GetHeaderCtrl();
	const int nCol = pHdr->GetItemCount();
	const int nAttrs = m_wndList.GetItemCount();
	string szRet;

	if ( nCol > 1 )
	{
		unordered_map<int, bool> flagmap;
		for ( int j = 0; j < nAttrs; ++j )
			if ( string( "+" ) == (LPCTSTR)m_wndList.GetItemText( j, 1 ) )
			{
				szRet += (LPCTSTR)m_wndList.GetItemText( j, 0 );
				szRet += ';';
			}
		if ( !szRet.empty() && szRet[szRet.size()-1] == ';' )
			szRet.pop_back();
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
