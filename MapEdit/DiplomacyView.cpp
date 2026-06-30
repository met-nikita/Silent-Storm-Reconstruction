#include "stdafx.h"
#include "mapedit.h"
#include "DiplomacyView.h"
#include "ItemsMgr.h"
#include "..\Misc\StrProc.h"
#include "..\Main\RPGDiplomacy.h"
#include "..\DBFormat\DataMap.h"
#include "dbDefs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int IDC_PARAMLIST_CTRL = 100;
const int N_PLAYERS = 16;
const char* VSZ_STATE[NDb::N_DS_COUNT] = 
{
	"En",
	"Neut",
	"Ally"
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyView

CDiplomacyView::CDiplomacyView() : nActiveDiplomacyID(-1), pItems(0)
{
}

CDiplomacyView::~CDiplomacyView()
{
}


BEGIN_MESSAGE_MAP(CDiplomacyView, CWnd)
	//{{AFX_MSG_MAP(CDiplomacyView)
	ON_NOTIFY(NM_CLICK, IDC_PARAMLIST_CTRL, OnClickParamlist)	
	ON_NOTIFY(NM_DBLCLK, IDC_PARAMLIST_CTRL, OnDblClickParamlist)	
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyView message handlers
const int COL_WIDTH = 30;
int CDiplomacyView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	const SResTree *pTree = theApp.GetResTree( IDC_DIPLOMACY_TREE );
	if ( !pTree )
		return -1;
	pItems = pTree->pItemsTree;

	CRect rect;
	int nWidth = 60;

	GetClientRect( &rect );

	DWORD dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;

	m_wndList.Create( dwStyle, rect, this, IDC_PARAMLIST_CTRL );
	m_wndList.SetExtendedStyle( LVS_EX_GRIDLINES );
	m_wndList.GetClientRect( &rect );

	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.pszText = "Players";
	column.cx = nWidth;
	column.iSubItem = 0;
	m_wndList.InsertColumn( 0, &column );

	for ( int i = 0; i < N_PLAYERS; ++i )
	{
		char buf[64];

		column.pszText = itoa( i, buf, 10 );
		column.cx = 40;
		column.iSubItem = i + 1;
		m_wndList.InsertColumn( i + 1, &column );
		int n = m_wndList.InsertItem( i, buf );
	}

	m_wndList.EnableWindow( FALSE );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	m_wndList.MoveWindow( 0, 0, cx, cy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	ToggleFlag( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	ToggleFlag( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline NDb::EDiplomacyState GetState( const char *pszState )
{
	for ( int i = 0; i < NDb::N_DS_COUNT; ++i )
		if ( string( pszState ) == VSZ_STATE[i] )
			return NDb::EDiplomacyState(i);
	ASSERT(0);
	return NDb::DS_ENEMY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::ToggleFlag( NM_LISTVIEW *pNMListView )
{
	if ( pNMListView->iSubItem == 0 )
		return;

	if ( nActiveDiplomacyID <= 0 )
		return;

	CPoint ptAction = pNMListView->ptAction;
	ptAction.x = 4;
	int iItem = m_wndList.HitTest( ptAction );
	if ( iItem + 1 == pNMListView->iSubItem )
		return;

	CString str = m_wndList.GetItemText( iItem, pNMListView->iSubItem );
	NDb::EDiplomacyState eState = GetState( (LPCSTR) str );
	int nNewState = eState + 1;
	if ( nNewState == NDb::N_DS_COUNT )
		nNewState = 0;
	m_wndList.SetItemText( iItem, pNMListView->iSubItem, VSZ_STATE[nNewState] );
	//
	SavePlayerDiplomacy( iItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::SetActiveDiplomacy( int nItemID )
{
	nActiveDiplomacyID = nItemID;
	const CPropMap *pProps = pItems->GetPropList( nActiveDiplomacyID );
	if ( !pProps )
	{
		m_wndList.EnableWindow( FALSE );
		return;
	}
	m_wndList.EnableWindow( TRUE );
	for ( int i = 0; i < N_PLAYERS; ++i )
	{
		CPropMap::const_iterator it = pProps->find( string( "Diplomacy" ) + IToA( i + 1 ) );
		if ( it == pProps->end() )
			continue;
		SetPlayerDiplomacy( i, (int)it->second->GetValue() );
	}
	pItems->ReleasePropList( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::SetPlayerDiplomacy( int nPlayer, DWORD nDiplomacy )
{
	NRPG::SDiplomacy sDiplomacy( nDiplomacy );

	for ( int i = 0; i < N_PLAYERS; ++i )
	{
		if ( i == nPlayer )
			continue;
		NDb::EDiplomacyState eState = sDiplomacy.GetDiplomacyState( i );
		m_wndList.SetItemText( nPlayer, i + 1, VSZ_STATE[eState] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyView::SavePlayerDiplomacy( int nPlayer )
{
	const CPropMap *pProps = pItems->GetPropList( nActiveDiplomacyID );
	if ( !pProps )
		return;
	CPropMap::const_iterator it = pProps->find( string( "Diplomacy" ) + IToA( nPlayer + 1 ) );
	if ( it != pProps->end() )
	{
		NRPG::SDiplomacy sDiplomacy;

		for ( int i= 0; i < N_PLAYERS; ++i )
			if ( i != nPlayer )
				sDiplomacy.SetDiplomacyState( i, GetState( (LPCSTR) m_wndList.GetItemText( nPlayer, i + 1 ) ) );
		//
		it->second->SetValue( (int)sDiplomacy.GetDiplomacy() );
	}
	pItems->ReleasePropList( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
