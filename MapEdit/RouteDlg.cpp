// RouteDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "RouteDlg.h"
#include "MapEdit.h"
#include "dbDefs.h"
#include "TreeView.h"
#include "..\Main\aiWaypoint.h"
#include "iWysiwyg.h"
#include "..\Misc\BasicShare.h"
#include "MESerialize.h"
#include "ObjectMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CBasicShare<int, NAI::CUnitAIInfoLoader> shareUnits;
extern CBasicShare<int, NAI::CUnitGroupAIInfoLoader> shareUnitGroups;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRouteDlg dialog


CRouteDlg::CRouteDlg(int _nUnitID, CWnd* pParent /*=NULL*/)
	: CDialog(CRouteDlg::IDD, pParent), nGroupID(-1)
{
	nUnitID = _nUnitID;
	const SResTree *pTree = theApp.GetResTree( IDC_WAYPOINTNAMES_TREE );
	
	m_pTree = new CMETreeView( vector<SResTree>( 1, *pTree ) );
	//{{AFX_DATA_INIT(CRouteDlg)
	//}}AFX_DATA_INIT

	CObjectMgr *pUnits = GetObjectMgr( BT_UNIT );
	if ( !pUnits )
	{
		ASSERT(0);
		return;
	}
	CPropMap props;
	pUnits->MergeWith( &props, nUnitID );
	CProp *pGP = props["Group"];
	if ( !IsValid( pGP ) )
	{
		ASSERT(0);
		return;
	}
	nGroupID = pGP->GetValue();
}

CRouteDlg::~CRouteDlg()
{
	if ( m_pTree )
		delete m_pTree;
	m_pTree = 0;
}
void CRouteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRouteDlg)
	DDX_Control(pDX, IDC_ROUTE, m_list);
	DDX_Control(pDX, IDC_TREE_PLACE, m_treePlace);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRouteDlg, CDialog)
	//{{AFX_MSG_MAP(CRouteDlg)
	ON_NOTIFY(NM_DBLCLK, IDC_ROUTE, OnDblclkRoute)
	ON_NOTIFY(LVN_KEYDOWN, IDC_ROUTE, OnLvnKeydownRoute)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRouteDlg message handlers
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CRouteDlg::OnInitDialog() 
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
	//
	CDGPtr< CPtrFuncBase<NAI::CUnitAIInfo> > pUnitLoader;
	if ( nGroupID > 0 ) 
		pUnitLoader = shareUnitGroups.Get( nGroupID );
	else
		pUnitLoader = shareUnits.Get( nUnitID );
	if ( !IsValid(pUnitLoader) )
		return FALSE;
	pUnitLoader.Refresh();
	pUnit = pUnitLoader->GetValue();
	if ( !IsValid( pUnit ) )
	{
		pUnit = new NAI::CUnitAIInfo;
	}
	if ( pUnit->routes.empty() )
		pUnit->routes.push_back( NAI::SRoute() );
	//
	InsertWaypoints();

	CString szCaption = "Route ";
	if ( nGroupID > 0 )
	{
		const SResTree *pTree = theApp.GetResTree( IDC_UNITGROUPS_TREE );
		if ( pTree )
			szCaption += CString("for group \"") + pTree->pItemsTree->GetItemPath( nGroupID ).c_str() + '\"';
	}
	else
		szCaption += "for unit";

	SetWindowText( szCaption );
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRouteDlg::InsertWaypoints()
{
	if ( !IsValid( pUnit ) || pUnit->routes.empty() )
	{
		ASSERT(0);
		return;
	}
	//
	const SResTree *pTree = theApp.GetResTree( IDC_WAYPOINTNAMES_TREE );
	if ( !pTree )
		return;
	CItemsMgr *pItems = pTree->pItemsTree;
	//
	m_list.DeleteAllItems();
	vector<int> &waypoints = pUnit->routes.front().waypoints;
	for ( int i = 0; i < waypoints.size(); ++i )
	{
		if ( pItems->IsExist( waypoints[i] ) )
			m_list.InsertItem( LVIF_TEXT | LVIF_PARAM, i, pItems->GetItemPath( waypoints[i] ).c_str(), 0, 0, 0, i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRouteDlg::OnOK() 
{
	if ( !IsValid( pUnit ) )
	{
		ASSERT(0);
		return;
	}
	if ( nGroupID > 0 )
		SerializeUnitGroupAIInfo( pUnit, nGroupID );
	else
		SerializeUnitAIInfo( pUnit, nUnitID );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRouteDlg::OnDblclkRoute(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( !IsValid( pUnit ) || pUnit->routes.empty() )
	{
		ASSERT(0);
		return;
	}
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos == NULL)
		 TRACE0("No items were selected!\n");
	else
	{
		int nItem = m_list.GetNextSelectedItem(pos);
		int nInd = m_list.GetItemData( nItem );
		pUnit->routes.front().waypoints.erase( pUnit->routes.front().waypoints.begin() + nInd );
		InsertWaypoints();
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CRouteDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_ME_TREESEL:
			{
				if ( !IsValid( pUnit ) || pUnit->routes.empty() )
				{
					ASSERT(0);
					return true;
				}
				int nTree, nID;
				if ( m_pTree->GetSelectedItemID( &nTree, &nID ) )
				{
					pUnit->routes.front().waypoints.push_back( nID );
					InsertWaypoints();
				}
			}
			return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRouteDlg::OnLvnKeydownRoute(NMHDR *pNMHDR, LRESULT *pResult)
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
			int nInd = m_list.GetItemData( nItem );
			pUnit->routes.front().waypoints.erase( pUnit->routes.front().waypoints.begin() + nInd );
			InsertWaypoints();
		}
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
