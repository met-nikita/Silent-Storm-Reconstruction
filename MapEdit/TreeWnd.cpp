// Stingray Objective Toolkit
// AppWizard generated file
// dockwnd.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "TreeWnd.h"
#include "resource.h"
#include "ChildView.h"
#include "TemplMgr.h"
#include "templ.h"
#include "TreeView.h"
#include "TreeVDialogs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char REG_LAYOUT[] = "Layout";

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTmplTreeWnd

CTmplTreeWnd::CTmplTreeWnd( const string &_szRegSection )
{
  szRegSection = _szRegSection;
}

CTmplTreeWnd::~CTmplTreeWnd()
{
  for( int i = 0; i < tabList.size(); ++i )
  {
    if ( tabList[i].pTree )
      delete tabList[i].pTree;
  }
  tabList.clear();
}


BEGIN_MESSAGE_MAP(CTmplTreeWnd, SECControlBar)
	//{{AFX_MSG_MAP(CTmplTreeWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
  ON_MESSAGE( TCM_TABSEL, OnTabSel )  
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_RESOURCETABS, OnResourceTabs)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CTmplTreeWnd message handlers

int CTmplTreeWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

  m_tabWnd.Create( this, WS_CHILD | WS_VISIBLE | TWS_TABS_ON_LEFT );
	const unordered_map<int, SResTree> &resTrees = theApp.GetResTrees();

	bool bActivateTab = false;
	for ( unordered_map<int, SResTree>::const_iterator i = resTrees.begin(); i != resTrees.end(); ++i )
  {
    STreeTab tab;
    const SResTree &tree = i->second;
		if ( tree.bHideTree )
			continue;
    
		if ( !tree.pItemsTree )
		{
			ASSERT(0);
			continue;
		}
    tab.pTree = new CMETreeView( vector<SResTree>( 1, tree ) );
    tab.pTree->Create( 0, "TreeView", WS_CHILD, CRect(0, 0, 0, 0), &m_tabWnd, tree.nTreeID );
    tab.nTreeID = tree.nTreeID;
    tab.szTabName = tree.szTabName;
		tab.bInitiallyVisible = tree.bVisible;
    tabList.push_back( tab );
		if ( tree.bVisible )
		{
			m_tabWnd.AddTab( tab.pTree, tree.szTabName.c_str() );
			//tab.pTree->ShowWindow( SW_SHOW );
		}
		bActivateTab = true;
//    m_tabWnd.SetTabIcon( i, IDI_LIST_ICON );
  }
  
  if ( bActivateTab )
  {
    m_tabWnd.ActivateTab( 0 );
    m_tabWnd.ScrollToTab(0);
  }
	return 0;
}

void CTmplTreeWnd::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

  CRect rectInside;
	GetInsideRect(rectInside);
	::SetWindowPos(m_tabWnd.m_hWnd, NULL, rectInside.left, rectInside.top,
		rectInside.Width(), rectInside.Height(),
		SWP_NOZORDER|SWP_NOACTIVATE);
}

BOOL CTmplTreeWnd::PreTranslateMessage(MSG* pMsg) 
{
  switch ( pMsg->message )
  {
  case WM_ME_TREESEL:
	case WM_ME_TREE_NEWITEM:
    {
      theApp.SetActiveItem( pMsg->wParam, pMsg->lParam );
			CWnd *pWnd = 0;
			 m_tabWnd.GetActiveTab( pWnd );
			if ( pWnd )
				pWnd->SetFocus();
    }
    return true;
	case WM_ME_TREE_DELITEM:
    {
			int nTree, nItem, n;
			theApp.GetActiveItem( &nTree, &nItem, &n );
      theApp.SetActiveItem( nTree, -1 );
			CWnd *pWnd = 0;
			 m_tabWnd.GetActiveTab( pWnd );
			if ( pWnd )
				pWnd->SetFocus();
    }
    return true;
  case WM_ME_DROPITEM:
    theApp.DropItem( pMsg->wParam, pMsg->lParam );
    return true;
  }
	return SECControlBar::PreTranslateMessage(pMsg);
}

void CTmplTreeWnd::UpdateTree( int nTreeID )
{
  if ( -1 == nTreeID )
  {
    OnTabSel( 0, 0 );
    return;
  }
  for ( int i = 0; i < tabList.size(); ++i )
  {
    if ( tabList[i].nTreeID == nTreeID )
    {
      tabList[i].pTree->SendMessage( WM_COMMAND, ID_RELOAD_TREE );
      return;
    }
  }
}

LRESULT CTmplTreeWnd::OnTabSel(WPARAM wParam, LPARAM lParam)
{
  CWnd *pWnd = 0;

  m_tabWnd.GetActiveTab( pWnd );
  if ( pWnd )
  {
		// CRAP слишком медленно 
    // pWnd->SendMessage( WM_COMMAND, ID_RELOAD_TREE );
  }
  return true;
}

void CTmplTreeWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
  CMenu menu;
  
  if ( !menu.LoadMenu( IDR_TREEWND_MENU ) )
    return;
  CMenu *pPopup = menu.GetSubMenu( 0 );
  if ( !pPopup )
    return;
  ClientToScreen( &point );
  pPopup->TrackPopupMenu( TPM_LEFTBUTTON, point.x, point.y, this );
	menu.DestroyMenu();
  SECControlBar::OnRButtonDown(nFlags, point);
}

void CTmplTreeWnd::OnResourceTabs() 
{
  int i, ind, n = tabList.size();
  vector<pair<int, string> > ress(n);
  vector<int> activeTabs( n, 0 );
  
  for ( i = 0; i < n; ++i )
  {
    ress[i].first  = tabList[i].nTreeID;
    ress[i].second = tabList[i].szTabName;
    if ( m_tabWnd.FindTab( tabList[i].pTree, ind ) )
      activeTabs[i] = 1;
  }
  CTreeLayoutDlg dlg( ress, &activeTabs );
  
  if ( IDOK != dlg.DoModal() )
    return;
  unordered_map<string, bool> layout;

  for ( i = 0; i < n; ++i )
  {
    layout[tabList[i].szTabName] = activeTabs[i];
  }
  SetVisibleTabs( layout );
  SaveLayout();
}

void CTmplTreeWnd::OnPaint() 
{
  if ( 0 != m_tabWnd.GetTabCount() )
  {
    SECControlBar::OnPaint();
    return;
  }
	CPaintDC dc(this); // device context for painting
  CRect r;
  GetInsideRect( r );
  dc.FillSolidRect( r, GetSysColor( COLOR_MENU ) );
}

void CTmplTreeWnd::SetVisibleTabs( const unordered_map<string, bool> &layout )
{
  const int n = tabList.size();
  int nActive;
  m_tabWnd.GetActiveTab( nActive );

  for ( int i = 0; i < n; ++i )
  {
    int ind;
		ASSERT( tabList[i].pTree );
		if ( !tabList[i].pTree )
			continue;
    unordered_map<string, bool>::const_iterator it = layout.find( tabList[i].szTabName );
    if ( it == layout.end() || it->second  )
    {
      if ( !m_tabWnd.FindTab( tabList[i].pTree, ind ) )
      {
        m_tabWnd.AddTab( tabList[i].pTree, tabList[i].szTabName.c_str() );
        tabList[i].pTree->ShowWindow( SW_SHOWNOACTIVATE );
				if ( m_tabWnd.FindTab( tabList[i].pTree, ind ) )
					m_tabWnd.ActivateTab( ind );
				else
				{
					ASSERT( false );
				}
//        m_tabWnd.SetTabIcon( ind, IDI_LIST_ICON );
      }
    }
    else
    {
      if ( m_tabWnd.FindTab( tabList[i].pTree, ind ) )
      {
        tabList[i].pTree->ShowWindow( SW_HIDE );
        m_tabWnd.RemoveTab( ind );
      }
    }
  }
	if ( m_tabWnd.GetTabCount() > 0 )
	{
		if ( m_tabWnd.GetTabCount() <= nActive )
			m_tabWnd.ActivateTab( m_tabWnd.GetTabCount() - 1 );
		else
			m_tabWnd.ActivateTab( nActive );
	}
  Invalidate();
}

void CTmplTreeWnd::SaveLayout()
{
  const int n = tabList.size();
  
  for ( int i = 0; i < n; ++i )
  {
    int ind;
    int val = m_tabWnd.FindTab( tabList[i].pTree, ind ) ? 0x1 : 0x0;
    theApp.WriteProfileInt( szRegSection.c_str(), tabList[i].szTabName.c_str(), val );
  }
}

void CTmplTreeWnd::LoadLayout()
{
  const int n = tabList.size();  
  unordered_map<string, bool> layout;
  
  for ( int i = 0; i < n; ++i )
  {
    bool bVal = theApp.GetProfileInt( szRegSection.c_str(), 
				tabList[i].szTabName.c_str(), 
				tabList[i].bInitiallyVisible );
    layout[tabList[i].szTabName] = bVal;
  }
  SetVisibleTabs( layout );
}

void CTmplTreeWnd::OnExtendContextMenu(CMenu* pMenu)
{
  CString strMenu;
  VERIFY(strMenu.LoadString(ID_RESOURCETABS));
  pMenu->AppendMenu(MF_SEPARATOR);
  pMenu->AppendMenu(MF_STRING, ID_RESOURCETABS, strMenu);
}
