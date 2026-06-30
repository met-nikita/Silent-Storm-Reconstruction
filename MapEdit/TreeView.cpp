#include "StdAfx.h"
#include "MapEdit.h"
#include "TreeView.h"
#include "TreeVDialogs.h"
#include "dbDefs.h"
#include "TemplMgr.h"
#include "Templ.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\GResource.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "placement.h"
#include "OIDlg.h"  // for WM_ME_CANCEL 
#include "FindDialog.h"
#include "PlacableDB.h"

#define IDC_TREE_CONTROL 233

const int WMU_SORT = WM_USER + 1;

struct SUneraseItems
{
	int nTreeID;
	int nItemID;
};

SUneraseItems unerasableItems[] = 
{
	{ IDC_RPG_ITEMS_TREE, 188 }, // Clue slot
	{ IDC_RPG_ITEMS_TREE, 189 }, // explosion
};
bool CanDelete( int nTreeID, int nItemID )
{
	for ( int i = 0; i < ARRAY_SIZE( unerasableItems ); ++i )
		if ( unerasableItems[i].nTreeID == nTreeID && unerasableItems[i].nItemID == nItemID )
			return false;
	return true;
}
IMPLEMENT_DYNCREATE(CMETreeView, CWnd)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMETreeView

CMETreeView::CMETreeView( const vector<SResTree> &vResTrees, bool _bOnlyFolders, UINT nMenuID ) 
  : m_bDragging(false), m_pDragImageList(0), m_treeCtrl( vResTrees, this )
{
	bOnlyFolders = _bOnlyFolders;

	for ( int i = 0; i < vResTrees.size(); ++i )
	{
		const SResTree &resTree = vResTrees[i];
		SRoot r;
		r.nTreeID   = resTree.nTreeID;
		r.resTree  = resTree;  
		r.pItemsMgr = resTree.pItemsTree;
		//
		r.szRootName = resTree.szRootName;
		r.nListenerID = resTree.pItemsTree->AddListener( this );
		roots.push_back( r );
	}
  m_menu.LoadMenu( nMenuID );
	m_bExpanding = false;
	oleDropTarget.pTree = this;
	bInit = false;
}

CMETreeView::~CMETreeView()
{
	for ( int i = 0; i < roots.size(); ++i )
		roots[i].pItemsMgr->RemoveListener( roots[i].nListenerID );
	m_menu.DestroyMenu();
}


BEGIN_MESSAGE_MAP(CMETreeView, CWnd)
	//{{AFX_MSG_MAP(CMETreeView)
	ON_WM_CREATE()
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_CONTROL, OnDblClick)
  ON_NOTIFY(NM_RCLICK, IDC_TREE_CONTROL, OnRClick)
  ON_NOTIFY(TVN_BEGINDRAG, IDC_TREE_CONTROL, OnBegindrag)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_CONTROL, OnSelect)
	ON_NOTIFY(TVN_ENDLABELEDIT, IDC_TREE_CONTROL, OnEndEdit)
	ON_NOTIFY(TVN_KEYDOWN, IDC_TREE_CONTROL, OnKeyDown)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_TREE_CONTROL, OnItemExpanding)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_SIZE()
	ON_COMMAND(ID_NEWFOLDER, OnNewFolder)
	ON_COMMAND(ID_DELETEFOLDER, OnDeleteFolder)
	ON_WM_CLOSE()
	ON_COMMAND(ID_NEWITEM, OnNewitem)
	ON_COMMAND(ID_DELETEITEM, OnDeleteitem)
	ON_COMMAND(ID_EXPORTITEM, OnExportItem)
	ON_COMMAND(ID_FORCED_EXPORTITEM, OnForcedExportitem)
	ON_COMMAND(ID_RELOAD_TREE, OnReloadTree)
	ON_WM_SHOWWINDOW()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_RENAMEITEM, OnRename)
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_FOLDER_COLOR, OnFolderColor)
	ON_COMMAND(ID_UPDATEDBVALUES, OnUpdatedbvalues)
	ON_MESSAGE( WMU_SORT, OnSort)
	ON_COMMAND(ID_FOLDER_FIND, OnFolderFind)
	ON_COMMAND(ID_COPYSELECTEDNAMES, OnCopyNames)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CMETreeView message handlers

int CMETreeView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if ( CWnd::OnCreate(lpCreateStruct) == -1 )
		return -1;
	oleDropTarget.Register( this );

	InitImageLists();

	// create a tree control
	DWORD dwStyle = TVS_SHOWSELALWAYS |
					TVS_HASBUTTONS |
					TVS_LINESATROOT |
					TVS_HASLINES |
					TVS_EDITLABELS |
					TVS_SHOWSELALWAYS |
					//TVS_DISABLEDRAGDROP |
					WS_CHILD | WS_VISIBLE | WS_TABSTOP;


	DWORD dwStyleEx = TVXS_FLYBYTOOLTIPS | LVXS_HILIGHTSUBITEMS | TVXS_MULTISEL;

  bool bCreated = m_treeCtrl.Create( dwStyle, dwStyleEx, CRect(0, 0, 0, 0), this, IDC_TREE_CONTROL);
  ASSERT(bCreated);
  
  m_treeCtrl.SetImageList(&m_imlNormal, TVSIL_NORMAL);
  m_treeCtrl.ShowWindow( SW_SHOW );  
  m_treeCtrl.SetNotifyWnd( this );
	m_hPlusCursor = LoadCursor( 0, IDC_UPARROW );
	m_hNoCursor = LoadCursor( 0, IDC_NO );
	m_hArrowCursor = LoadCursor( 0, IDC_ARROW );

	bInit = false;
  //ResetTree();
  
	// add a client edge
//	m_missonWnd.ModifyStyleEx( 0, WS_EX_CLIENTEDGE, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::InitImageLists()
{
	if ( m_imlNormal.m_hImageList )
		return;
	// TODO: create and load your image lists
	CBitmap bmp;
	// normal tree images
	m_imlNormal.Create(16,
				 16,
				 ILC_COLOR16 | ILC_MASK,
				 8,	// number of initial images
				 8);

	ASSERT(m_imlNormal.m_hImageList);

	bmp.LoadBitmap(IDB_PROJWSP);
	m_imlNormal.Add( &bmp, RGB(255,255,255));
	bmp.DeleteObject();

}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMETreeView::GetTreeID( HTREEITEM hti ) const
{
	HTREEITEM hroot = m_treeCtrl.GetRootItem( hti );
	if ( !hroot )
		return -1;
	return GetFoldID( m_treeCtrl.GetItemData( hroot ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnDblClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
  UINT flags;
  CPoint pt;
  GetCursorPos( &pt );
  m_treeCtrl.ScreenToClient( &pt );
  HTREEITEM hti = m_treeCtrl.HitTest( pt, &flags );
  
  if ( hti )
  {
    LPARAM lParam = m_treeCtrl.GetItemData( hti );
    // Если поселекчен итем, шлем родительскому окну сообщение
    if ( !IsFolder( lParam ) )
      GetParent()->PostMessage( WM_ME_TREESEL, GetTreeID( hti ), lParam );
  }
  *pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	HTREEITEM hti = m_treeCtrl.GetFirstSelectedItem();

	if ( hti )
	{
		LPARAM lParam = m_treeCtrl.GetItemData( hti );
		if ( !IsFolder( lParam ) )
		{
			theApp.SelChanged( GetTreeID( hti ), lParam );
			GetParent()->PostMessage( WM_ME_TREESELCHANGED, GetTreeID( hti ), lParam );
		}
		else
			GetParent()->PostMessage( WM_ME_TREESELCHANGED, GetTreeID( hti ), -1 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::GetInfo( SRoot *pRoot, HTREEITEM hti ) const
{
	const int nTree = GetTreeID( hti );
	for ( int i = 0; i < roots.size(); ++i )
		if ( roots[i].nTreeID == nTree )
		{
			*pRoot = roots[i];
			return true;
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::GetInfo( SRoot *pRoot, int nTreeID ) const
{
	for ( int i = 0; i < roots.size(); ++i )
		if ( roots[i].nTreeID == nTreeID )
		{
			*pRoot = roots[i];
			return true;
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnEndEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = false;
	LPNMTVDISPINFO ptvdi = (LPNMTVDISPINFO)pNMHDR;
	if ( 0 == ptvdi->item.hItem || 0 == ptvdi->item.pszText )
		return;
//	Sort( m_treeCtrl.GetParentItem( ptvdi->item.hItem ), false );
	PostMessage( WMU_SORT, (WPARAM)ptvdi->item.hItem );
	LPARAM lParam = m_treeCtrl.GetItemData( ptvdi->item.hItem );
	SRoot r;
	if ( !GetInfo( &r, ptvdi->item.hItem ) )
		return;
	if ( IsFolder( lParam ) )
	{
		if ( !r.pItemsMgr->SetFolderName( r.nListenerID, GetFoldID( lParam), ptvdi->item.pszText ) )
			return;
	}
	else if ( !r.pItemsMgr->SetItemName( r.nListenerID, lParam, ptvdi->item.pszText ) )
		return;
	*pResult = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	TRACE(_T("Begin Drag\n"));
	ASSERT(m_bDragging == FALSE);
	ASSERT(m_pDragImageList == NULL);
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;

	CPoint	ptAction;
	//UINT		nFlags;
	DWORD		dwPos = GetMessagePos();

	POINTS pts = MAKEPOINTS( dwPos );
	ptAction.x = pts.x;
	ptAction.y = pts.y;
  m_treeCtrl.ScreenToClient( &ptAction );

	HTREEITEM hTreeItem;// = m_treeCtrl.HitTest(ptAction, &nFlags);
	hTreeItem = pnmtv->itemNew.hItem;

  if ( !hTreeItem )
    return;
	m_bDragging = TRUE;
	m_hitemDrag = hTreeItem;

	COleDataSource OleDataSource;
	COleDropSource OleDropSource;

	LPARAM lDragParam = m_treeCtrl.GetItemData( m_hitemDrag );
	if ( !IsFolder( lDragParam ) )
	{
		char buf[64];
		sprintf( buf, "%d %d", GetTreeID( m_hitemDrag ), lDragParam );
		//Copy above CSting in global allocated shareable memory
		HGLOBAL hGlobal = ::GlobalAlloc( GMEM_SHARE|GMEM_FIXED, strlen( buf + 1 ) * 4 );
		LPTSTR lpszDbObjPtr = (LPTSTR)::GlobalLock( hGlobal );
		strcpy( lpszDbObjPtr, buf );
		//Put reference to global memory into clipboard
		OleDataSource.CacheGlobalData( CF_TEXT, hGlobal );
		sprintf( buf, "%x\n", hGlobal );
		OutputDebugString( buf );
		::GlobalUnlock( hGlobal );
		/*
		STGMEDIUM *pStg = new STGMEDIUM;
		pStg->hGlobal = hGlobal;
		pStg->tymed = TYMED_HGLOBAL;
		pStg->pUnkForRelease = 0;
		OleDataSource.CacheData( CF_TEXT, pStg );
		*/
		//OleDataSource.SetClipboard();
	}

	m_treeCtrl.StartAutoScroll( 100 );
	OleDataSource.DoDragDrop( DROPEFFECT_MOVE | DROPEFFECT_SCROLL | DROPEFFECT_COPY, 0, &OleDropSource );
	//OleDataSource.FlushClipboard();
	m_treeCtrl.StopAutoScroll();
	m_treeCtrl.SelectDropTarget( 0 );
	m_bDragging = false;

  *pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (m_bDragging)
	{
  	HTREEITEM		hitem;
	  UINT				flags;
    CRect       r;
    CPoint      pt, ptTree;

		ASSERT(m_pDragImageList != NULL);
  
    theApp.GetMainWnd()->GetWindowRect( &r );
  	GetCursorPos( &pt );
    pt -= r.TopLeft();
		m_pDragImageList->DragMove( pt );

    GetCursorPos(&ptTree);
    ScreenToClient( &ptTree );
		if ( (hitem = m_treeCtrl.HitTest(ptTree, &flags)) != 0 )
		{
			m_pDragImageList->DragLeave(theApp.GetMainWnd());
			m_treeCtrl.SelectDropTarget(hitem);
			m_hitemDrop = hitem;
			m_pDragImageList->DragEnter(theApp.GetMainWnd(), pt);
			int dataDrag = m_treeCtrl.GetItemData( m_hitemDrag );
			int dataDrop = m_treeCtrl.GetItemData( m_hitemDrop );
			if ( IsFolder( dataDrop ) && IsFolder( dataDrag ) && GetTreeID( m_hitemDrag ) == GetTreeID( m_hitemDrop ) )
			{
				SRoot r;
				if ( !GetInfo( &r, m_hitemDrag ) || r.pItemsMgr->IsSubfolder( GetFoldID( dataDrag), GetFoldID( dataDrop) ) )
					SetCursor( m_hNoCursor );
				else
					SetCursor( m_hArrowCursor );
			}
			else
				SetCursor( m_hNoCursor );
		}
	}
	
	CWnd::OnMouseMove(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnLButtonUp(UINT nFlags, CPoint point) 
{
  CWnd::OnLButtonUp(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int FixRootFolder( CMETreeCtrl *pTreeCtrl, HTREEITEM hti, int nFolderID )
{
  HTREEITEM hroot = pTreeCtrl->GetRootItem();
	while ( hroot )
	{
		if ( hti == hroot )
			return 0;
		hroot = pTreeCtrl->GetNextSiblingItem( hroot );
	}
	return nFolderID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::ChangeParent( HTREEITEM hti, HTREEITEM htiNewParent, HTREEITEM htiInsertAfter )
{
  TV_ITEM tvItem, tvFolder;

  if ( m_treeCtrl.GetParentItem( hti ) == htiNewParent || GetTreeID( hti ) != GetTreeID( htiNewParent ) )
    return;
	SRoot r;
	if ( !GetInfo( &r, hti ) )
		return;
  memset(&tvItem,0,sizeof(tvItem));
  tvItem.mask  = TVIF_PARAM | TVIF_TEXT;
  tvItem.hItem = hti;
  m_treeCtrl.GetItem( &tvItem, FALSE );
  memset(&tvFolder,0,sizeof(tvFolder));
  tvFolder.mask  = TVIF_PARAM | TVIF_TEXT;
  tvFolder.hItem = htiNewParent;
  m_treeCtrl.GetItem( &tvFolder, FALSE );

  int nFolderID = GetFoldID( tvFolder.lParam );
	nFolderID = FixRootFolder( &m_treeCtrl, htiNewParent, nFolderID );

  if ( IsFolder( tvItem.lParam ) )
  {
    // не пытаемся ли мы перетащить папку в свою подпапку ?
    if ( !r.pItemsMgr->IsSubfolder( GetFoldID( tvItem.lParam) , nFolderID ) )
      r.pItemsMgr->SetParentFolder( r.nListenerID, GetFoldID( tvItem.lParam), nFolderID );
  }
  else
  {
    r.pItemsMgr->SetFolderForItem( r.nListenerID, tvItem.lParam, nFolderID );
  }
	HTREEITEM hAfter = htiInsertAfter ? htiInsertAfter : TVI_FIRST;
  m_treeCtrl.SetNodeParent( hti, htiNewParent, TRUE, hAfter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
  CPoint pt;
  
  GetCursorPos( &pt );
  CMenu *pPopup = m_menu.GetSubMenu( 0 );
  if ( pPopup )
  {
    HTREEITEM hti = m_treeCtrl.GetSelectedItem();
    if ( !hti )
      return;

    MenuState( pPopup, hti );
    pPopup->TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Без этой функции триконтрола мы не увидим
void CMETreeView::OnSize(UINT nType, int cx, int cy) 
{
  CWnd::OnSize(nType, cx, cy);
  
  if ( ::IsWindow(m_treeCtrl) )
    m_treeCtrl.SetWindowPos( NULL, 0, 0, cx, cy,
    SWP_NOMOVE | 
    SWP_NOACTIVATE | 
    SWP_NOZORDER );    
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnNewFolder() 
{
  CNewFolderDlg dlg;
  
  if ( IDOK != dlg.DoModal() )
    return;

  // найдем папку, которая будет являться родительской для новой
  HTREEITEM hti = m_treeCtrl.GetSelectedItem();
  if ( !hti && !(hti = m_treeCtrl.GetRootItem()) )
    return;
	SRoot r;
	if ( !GetInfo( &r, hti ) )
		return;
  int id = m_treeCtrl.GetItemData( hti );
  if ( !IsFolder( id ) )
    hti = m_treeCtrl.GetParentItem( hti );
  if ( !hti )
    return;
  int nParent = GetFoldID( m_treeCtrl.GetItemData( hti ) );
	nParent = FixRootFolder( &m_treeCtrl, hti, nParent );

  // добавим новую папку в базу
  int newID = r.pItemsMgr->AddFolder( r.nListenerID, nParent, LPCTSTR( dlg.m_NewName ) );
  if ( -1 == newID )
    return;

  HTREEITEM htiFold = hti;
  hti = m_treeCtrl.InsertItem( dlg.m_NewName, 0, 0, htiFold );
  m_treeCtrl.SetItemData( hti, MakeFoldParam( newID ) );
	Sort( htiFold, false );	
  m_treeCtrl.Expand( htiFold, TVE_COLLAPSE );
  m_treeCtrl.Expand( htiFold, TVE_EXPAND );
	m_treeCtrl.DeselectAllItems();
  m_treeCtrl.SelectItem( hti );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::DeleteFolder( HTREEITEM hti )
{
  int id = m_treeCtrl.GetItemData( hti );
	SRoot r;
  if ( !IsFolder( id ) || !GetInfo( &r, hti ) )
    return;
	bool bHasItems = m_treeCtrl.GetChildItem( hti ) != 0;
	CDelFolderDlg dlg( m_treeCtrl.GetItemText( hti ), bHasItems, this );

	if ( IDOK != dlg.DoModal() )
		return;
	if ( bHasItems )
	{
		int i;
		vector<int> items;

		CollectItems( hti, &items );

		CString szItems;
		for ( i = 0; i < items.size(); ++i )
			szItems += CString( r.pItemsMgr->GetItemName( items[i] ) ) + "\r\n";
		CDelItemsDlg dlgDelitems( szItems, this );
		if ( IDOK != dlgDelitems.DoModal() )
			return;

		for ( i = 0; i < items.size(); ++i )
		{
			if ( CanDelete( r.nTreeID, items[i]) )
				r.pItemsMgr->DeleteItem( -1, items[i] );
		}
		items.clear();
		CollectFolders( hti, &items );
		for ( i = 0; i < items.size(); ++i )
			r.pItemsMgr->DeleteFolder( -1, items[i] );
	}
  if ( r.pItemsMgr->DeleteFolder( r.nListenerID, GetFoldID( id ) ) )
    m_treeCtrl.DeleteItem( hti );
	m_treeCtrl.DeselectAllItems();
	Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnDeleteFolder()
{
  HTREEITEM hti = m_treeCtrl.GetSelectedItem();

  if ( !hti )
    return;
	DeleteFolder( hti );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Создание структуры папок
// возвращается указатель на папку с id = nID или 0, если такой папки нет
HTREEITEM CMETreeView::MakeFolders( HTREEITEM *hRoot, int nTreeID, int nID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
	HTREEITEM reth = 0;

  // корневая папка
  // итемы, которые не попали ни в какую друную папку кладутся сюда
  //folders[0].hti = m_treeCtrl.InsertItem( szRootName.c_str(), 0, 0 );
  HTREEITEM hti = m_treeCtrl.InsertItem( r.szRootName.c_str(), 0, 0 );
  m_treeCtrl.SetItemData( hti, MakeFoldParam( nTreeID ) );
	*hRoot = hti;
  
  // создаем также управлющую структуру для InsertBatch
  r.pItemsMgr->MoveFirstFolder();
  while ( r.pItemsMgr->MoveNextFolder() )
  {
    int nFolderID = r.pItemsMgr->GetFolderID();
    HTREEITEM hti = (HTREEITEM)FolderAdded( nTreeID, nFolderID );
		if ( nID == nFolderID )
			reth = hti;
  }
  hti = m_treeCtrl.GetRootItem();
	return reth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращается указатель на итем с id = nID или 0, если такого итема нет
HTREEITEM CMETreeView::MakeItems( int nTreeID, int nID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
	HTREEITEM reth = 0;
  //
  r.pItemsMgr->MoveFirstItem();
  while ( r.pItemsMgr->MoveNextItem() )
  {
    HTREEITEM hti = AddItem( nTreeID, r.pItemsMgr->GetItemID() );
		if ( r.pItemsMgr->GetItemID() == nID )
			reth = hti;
  }
  HTREEITEM hti = m_treeCtrl.GetRootItem();
  if ( hti )
    m_treeCtrl.Expand( hti, TVE_EXPAND );
	return reth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HTREEITEM CMETreeView::AddItem( int nTreeID, int nItemID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
  HTREEITEM hfold = GetFolder( nTreeID, r.pItemsMgr->GetItemFolderID( nItemID ) );
  if ( !hfold )
    hfold = m_treeCtrl.GetRootItem();
  
	HTREEITEM hti;
  if ( hfold )
  {
    hti = m_treeCtrl.InsertItem( r.pItemsMgr->GetItemName( nItemID ), 6, 6, hfold, TVI_LAST );
    if ( !hti )
      return 0 ;
    m_treeCtrl.SetItemData( hti, nItemID );
	}
	return hti;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMETreeView::FolderAdded( int nTreeID, int nFolderID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
	HTREEITEM hTest = GetFolder( nTreeID, nFolderID );
	if ( hTest )
		return (int)hTest; // папка уже есть в дереве
	//
	int nParentID = r.pItemsMgr->GetParentID( nFolderID );
  HTREEITEM hfold = GetFolder( nTreeID, nParentID );
	if ( !hfold && -1 != nParentID )
	{
		// попытка добавить child папку, когда еще нет родительской
		// пытаемся добавить родительскую
		hfold = (HTREEITEM)FolderAdded( nTreeID, nParentID );
	}
  
  if ( !hfold )
    return (int)hfold;

	const char *pszFoldName = r.pItemsMgr->ID2FolderName( nFolderID );
	if ( !pszFoldName )
		pszFoldName = "";
  HTREEITEM hti = m_treeCtrl.InsertItem( pszFoldName, 0, 0, hfold );
  if ( hti )
    m_treeCtrl.SetItemData( hti, MakeFoldParam( nFolderID ) );
  if ( hfold )
    if ( m_treeCtrl.IsExpanded( hfold ) )
    {
      m_treeCtrl.Expand( hfold, TVE_COLLAPSE );
      m_treeCtrl.Expand( hfold, TVE_EXPAND );
    }
    else
      m_treeCtrl.InvalidateItem( hfold );
  return (int)hti;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::ParentChanged( int nTreeID, int nFolderID, int nOldParent )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return;
  HTREEITEM hold  = GetFolder( nTreeID, nOldParent );
  HTREEITEM hprnt = GetFolder( nTreeID, r.pItemsMgr->GetParentID( nFolderID ) );

  if ( !hold || !hprnt )
    return;
  HTREEITEM hfold = m_treeCtrl.GetChildItem( hold );
  while ( hfold )
  {
    if ( m_treeCtrl.GetItemData( hfold ) == MakeFoldParam( nFolderID ) )
      break;
    hfold = m_treeCtrl.GetNextSiblingItem( hfold );
  }
  if ( hfold )
   m_treeCtrl.SetNodeParent( hfold, hprnt );  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMETreeView::ItemAdded( int nTreeID, int nItemID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
  HTREEITEM hfold = GetFolder( nTreeID, r.pItemsMgr->GetItemFolderID( nItemID ) );
  if ( !hfold )
    hfold = m_treeCtrl.GetRootItem();

	switch ( nTreeID )
	{
		case IDC_TEXTURES_TREE:
			NDatabase::Refresh<NDb::CTexture>();
			break;
		case IDC_GEOMETRIES_TREE:
			NDatabase::Refresh<NDb::CGeometry>();
			break;
		case IDC_AIGEOMETRIES_TREE:
			NDatabase::Refresh<NDb::CAIGeometry>();
			break;
	}
  if ( hfold )
  {
    HTREEITEM hti = m_treeCtrl.InsertItem( r.pItemsMgr->GetItemName( nItemID ), 6, 6, hfold, TVI_LAST );
    if ( !hti )
      return 0;
    m_treeCtrl.SetItemData( hti, nItemID );
    if ( hfold )
      if ( m_treeCtrl.IsExpanded( hfold ) )
      {
        m_treeCtrl.Expand( hfold, TVE_COLLAPSE );
        m_treeCtrl.Expand( hfold, TVE_EXPAND );
      }
      else
        m_treeCtrl.InvalidateItem( hfold );
			return (int)hti;
  }
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::ItemDeleted( int nTreeID, int nItemID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return;
  HTREEITEM hfold = GetFolder( nTreeID, r.pItemsMgr->GetItemFolderID( nItemID ) );
  
  if ( hfold )
  {
    HTREEITEM hti = m_treeCtrl.GetChildItem( hfold );

    while ( hti )
    {
      if ( m_treeCtrl.GetItemData( hti ) == nItemID )
      {
        m_treeCtrl.DeleteItem( hti );
        return;
      }
      hti = m_treeCtrl.GetNextSiblingItem( hti );
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::ItemFolderChanged( int nTreeID, int nItemID, int nOldFolder )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return;
  HTREEITEM hold =  GetFolder( nTreeID, nOldFolder );
  HTREEITEM hfold = GetFolder( nTreeID, r.pItemsMgr->GetItemFolderID( nItemID ) );
  
  if ( !hold || !hfold )
    return;

  HTREEITEM hitem = m_treeCtrl.GetChildItem( hold );
  while ( hitem )
  {
    if ( m_treeCtrl.GetItemData( hitem ) == nItemID )
      break;
    hitem = m_treeCtrl.GetNextSiblingItem( hitem );
  }
  if ( !hitem )
    return;
  m_treeCtrl.SetNodeParent( hitem, hfold );  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::FolderDeleted( int nTreeID, int nFolderID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return;
  HTREEITEM hfold = GetFolder( nTreeID, nFolderID );

  if ( !hfold )
    return;
  HTREEITEM hti = m_treeCtrl.GetParentItem( hfold );
  m_treeCtrl.DeleteItem( hfold );
  if ( hti )
    m_treeCtrl.InvalidateItem( hti );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnClose() 
{
	CWnd::OnClose();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HTREEITEM CMETreeView::GetFolder( int nTreeID, int nFolderID )
{
	SRoot r; 
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
  int foldID = nFolderID;
  vector<int> path;
  
  while( foldID > 0 )
  {
    path.push_back( foldID );
    int id = r.pItemsMgr->GetParentID( foldID );
    // указывает сам на себя ?
    ASSERT( id != foldID );
		if ( path.size() > 100 )
		{
			ASSERT( 0 );
			return 0;
		}
    foldID = id;
  }
  HTREEITEM hti = m_treeCtrl.GetRootItem();
	while ( hti )
	{
		if ( m_treeCtrl.GetItemData( hti ) == MakeFoldParam( nTreeID ) )
			break;
		hti = m_treeCtrl.GetNextSiblingItem( hti );
	}
  HTREEITEM htiPrev = hti;
  if ( !hti )
    return 0;
  hti = m_treeCtrl.GetChildItem( hti );
  
  for ( int i = path.size() - 1; i >= 0; --i )
  {
    while ( hti )
    {
      if ( m_treeCtrl.GetItemData( hti ) == MakeFoldParam( path[i] ) )
        break;
      hti = m_treeCtrl.GetNextSiblingItem( hti );
    }
    if ( !hti )
      return 0;
    htiPrev = hti;
    hti = m_treeCtrl.GetChildItem( hti );
  }
  return htiPrev;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPrepareInsert
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnNewitem() 
{
  // найдем папку, которая будет являться родительской для новой
  HTREEITEM hti = m_treeCtrl.GetSelectedItem();
  if ( !hti && !(hti = m_treeCtrl.GetRootItem()) )
    return;
  int id = m_treeCtrl.GetItemData( hti );
  if ( !IsFolder( id ) )
    hti = m_treeCtrl.GetParentItem( hti );
  if ( !hti )
    return;
	SRoot r; 
	if ( !GetInfo( &r, hti ) )
		return;
  int nParent = GetFoldID( m_treeCtrl.GetItemData( hti ) );
	nParent = FixRootFolder( &m_treeCtrl, hti, nParent );

  string szNewName;
  int nWidth, nHeight;
  
	CString str;
	str.LoadString( IDS_NEWITEMFOLDER );
	string szFoldPath = string( str ) + string( " \"\\" ) + r.pItemsMgr->GetFolderPath( nParent ) + "\"";
	int nItems = 1;
  if ( IDC_TEMPLATE_TREE == r.nTreeID )
  {
    CAddTemplDlg dlg( szFoldPath );
    
    if ( IDOK != dlg.DoModal() )
      return;
    szNewName = dlg.m_name;
    nWidth  = dlg.m_width;
    nHeight = dlg.m_height;
  }
  else
  {
    CNewItemDlg dlg( szFoldPath );
    
    if ( IDOK != dlg.DoModal() )
      return;
    szNewName = dlg.m_NewName;
		nItems = dlg.m_nQuantity;
  }  
  BeginWaitCursor();
  // добавим новый итем в базу
  int newID;
  if ( szNewName.find( '\'' ) != string::npos )
  {
    MessageBox( "Label syntax is incorrect!", "Warning", MB_OK | MB_ICONWARNING );
    EndWaitCursor();
    return;
  }
	HTREEITEM newhti = 0;
	for ( int i = 0; i < nItems; ++i )
	{
		newID = r.pItemsMgr->AddItem( r.nListenerID, nParent, szNewName );
		if ( -1 == newID )
		{
			EndWaitCursor();
			return;
		}
		static CPlaceDB placeDB;
		switch ( r.nTreeID )
		{
			case IDC_TEMPLATE_TREE:
				if ( !theTemplMgr.AddTemplate( newID, nWidth, nHeight ) )
				{
					EndWaitCursor();
					return;
				}
				break;
			case IDC_OBJECTS_TREE:
				placeDB.InsertPlacableObject( "ObjectTemplates", szNewName, newID );
				break;
			case IDC_RPG_ITEMS_TREE:
				placeDB.InsertPlacableObject( "RPGItems", szNewName, newID );
				break;
		}
		newhti = (HTREEITEM)ItemAdded( r.nTreeID, newID );
	}
	// CRAP { слишком медленно
	if ( r.nTreeID == IDC_TEMPLATE_TREE )
		theApp.UpdateGameDB();
	// } CRAP END
  EndWaitCursor();
	if ( newhti )
	{
		m_treeCtrl.DeselectAllItems();
		m_treeCtrl.SelectItem( newhti );
		Sort( m_treeCtrl.GetParentItem( newhti ), false );
		GetParent()->PostMessage( WM_ME_TREE_NEWITEM, r.nTreeID, newID );
	}
	m_treeCtrl.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnDeleteitem() 
{
	vector<HTREEITEM> hitems;
	for ( HTREEITEM h = m_treeCtrl.GetFirstSelectedItem(); h != 0; h = m_treeCtrl.GetNextSelectedItem( h ) )
		hitems.push_back( h );

	for ( int i = 0; i < hitems.size(); ++i )
	{
		HTREEITEM hti = hitems[i];
		int id = m_treeCtrl.GetItemData( hti );
		if ( IsFolder( id ) )
		{
			DeleteFolder( hti );
			continue;
		}
		SRoot r; 
		if ( !GetInfo( &r, hti ) )
			continue;
		if ( !CanDelete( r.nTreeID, id  ) )
			continue;
		CString str;
		m_treeCtrl.GetItemString( hti, 0, str );
		CString szText = "Do you really want to delete the ";
		szText += str + " ?";
		if ( IDNO == MessageBox( szText, "Confirm", MB_YESNO | MB_ICONQUESTION ) )
			return;
		if ( !id )
			continue;
		m_treeCtrl.DeleteItem( hti );
		r.pItemsMgr->DeleteItem( r.nListenerID, id );
	}
	m_treeCtrl.SetFocus();
	m_treeCtrl.DeselectAllItems();
	GetParent()->PostMessage( WM_ME_TREE_DELITEM );
	Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::MenuState( CMenu *pPopup, HTREEITEM hti )
{
  ASSERT( pPopup );
  
	SRoot r; 
	if ( !GetInfo( &r, hti ) )
		return;

	int nSelectedID = m_treeCtrl.GetItemData( hti );
  UINT nEnable = r.resTree.pDoExport != 0 && roots.size() == 1 ? MF_ENABLED : MF_GRAYED;
  pPopup->EnableMenuItem( ID_EXPORTITEM, nEnable );
  pPopup->EnableMenuItem( ID_FORCED_EXPORTITEM, nEnable );
	
	pPopup->EnableMenuItem( ID_UPDATEDBVALUES, r.resTree.pDoUpdateDB != 0 && roots.size() == 1 ? MF_ENABLED : MF_GRAYED );
	
  nEnable = !IsFolder( nSelectedID ) ? MF_ENABLED : MF_GRAYED;
  pPopup->EnableMenuItem( ID_DELETEITEM, nEnable );
  
  nEnable = IsFolder( nSelectedID ) ? MF_ENABLED : MF_GRAYED;
  pPopup->EnableMenuItem( ID_DELETEFOLDER, nEnable );

	//pPopup->EnableMenuItem( ID_FOLDER_COLOR, nEnable );
	
	nEnable = !bOnlyFolders ? MF_ENABLED : MF_GRAYED;
	pPopup->EnableMenuItem( ID_DELETEITEM, nEnable );
	pPopup->EnableMenuItem( ID_NEWITEM, nEnable );
	pPopup->EnableMenuItem( ID_RENAMEITEM, nEnable );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::PrepareExport( vector<int> *pItems )
{
	vector<HTREEITEM> hitems;
	for ( HTREEITEM h = m_treeCtrl.GetFirstSelectedItem(); h != 0; h = m_treeCtrl.GetNextSelectedItem( h ) )
		hitems.push_back( h );

	for ( int i = 0; i < hitems.size(); ++i )
	{
		HTREEITEM hti = hitems[i];
		NGScene::CloseAllResources();
		int id = m_treeCtrl.GetItemData( hti );
		if ( !IsFolder( id ) )
			pItems->push_back( id );
		else
			CollectItems( hti, pItems );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnExportItem() 
{
	if ( roots.size() > 1 )
		return;
	SRoot r = roots.front();
	if ( !r.resTree.pDoExport )
		return;
	vector<int> items;
	PrepareExport( &items );
	BeginWaitCursor();
  r.resTree.pDoExport( r.resTree.pItemsTree, items, false );
	EndWaitCursor();
	ClearHoldQueue();
	int nTree, nID, nVariantID;
	theApp.GetActiveItem( &nTree, &nID, &nVariantID );
	theApp.SetActiveItem( nTree, nID, nVariantID, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnForcedExportitem() 
{
	if ( roots.size() > 1 )
		return;
	SRoot r = roots.front();
	if ( !r.resTree.pDoExport )
		return;
	vector<int> items;
	PrepareExport( &items );
	BeginWaitCursor();
  r.resTree.pDoExport( r.resTree.pItemsTree, items, true );
	EndWaitCursor();
	ClearHoldQueue();
	int nTree, nID, nV;
	theApp.GetActiveItem( &nTree, &nID, &nV );
	theApp.SetActiveItem( nTree, nID, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnUpdatedbvalues() 
{
	if ( roots.size() > 1 )
		return;
	SRoot r = roots.front();
	if ( !r.resTree.pDoUpdateDB )
		return;
	BeginWaitCursor();
	vector<int> items;
	PrepareExport( &items );
  r.resTree.pDoUpdateDB( r.resTree.pItemsTree, items );	
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Собрать ID'шники всех итемов, которые находятся ниже по дереву от данной папки
void CMETreeView::CollectItems( HTREEITEM htiFolder, vector<int> *pItems )
{
  if ( !htiFolder )
    return;
  if ( !IsFolder( m_treeCtrl.GetItemData( htiFolder ) ) )
  {
    pItems->push_back( m_treeCtrl.GetItemData( htiFolder ) );
    return;
  }
  //
  HTREEITEM hti = m_treeCtrl.GetChildItem( htiFolder );
  while ( hti )
  {
    int nID = m_treeCtrl.GetItemData( hti );
    if ( IsFolder( nID ) )
      CollectItems( hti, pItems );
    else
      pItems->push_back( nID );
    hti = m_treeCtrl.GetNextSiblingItem( hti );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::CollectFolders( HTREEITEM htiFolder, vector<int> *pFolders )
{
  if ( !htiFolder )
    return;
  //
  HTREEITEM hti = m_treeCtrl.GetChildItem( htiFolder );
  while ( hti )
  {
    int nID = m_treeCtrl.GetItemData( hti );
    if ( IsFolder( nID ) )
		{
			CollectFolders( hti, pFolders );
			pFolders->push_back( GetFoldID( nID ) );
		}
    hti = m_treeCtrl.GetNextSiblingItem( hti );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnReloadTree() 
{
	for ( int i = 0; i < roots.size(); ++i )
		roots[i].pItemsMgr->Reload();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CALLBACK 
MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	if ( !lParamSort )
		return 0;
	CItemsMgr *pItemsMgr = (CItemsMgr*)lParamSort;
/*
	CMETreeView *pTree = (CMETreeView*)lParamSort;
	CMETreeView::SRoot r1, r2;
	
	if ( !pTree->GetInfo( &r1, (HTREEITEM)lParam1 ) || !pTree->GetInfo( &r2, (HTREEITEM)lParam2 ) )
		return -1;
	ASSERT( r1.nTreeID == r2.nTreeID );
*/
	bool bFolder1 = IsFolder( lParam1 );
	bool bFolder2 = IsFolder( lParam2 );
	
	if ( (bFolder1 && bFolder2) || (!bFolder1 && !bFolder2) )
	{
		CString strItem1 = bFolder1 ? pItemsMgr->ID2FolderName( GetFoldID( lParam1 ) ) : pItemsMgr->GetItemName( lParam1 );
		CString strItem2 = bFolder2 ? pItemsMgr->ID2FolderName( GetFoldID( lParam2 ) ) : pItemsMgr->GetItemName( lParam2 );
		int nRet = strItem1.CompareNoCase( strItem2 );
		if ( 0 == nRet )
			return lParam1 > lParam2;
		return nRet;
	}
	return bFolder1 ? -1 : 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::Sort( HTREEITEM hti, bool bRecursive )
{
	SRoot r;
	if ( !GetInfo( &r, hti ) )
		return;
	TVSORTCB tvs;
	
	tvs.hParent = hti;
	tvs.lpfnCompare = MyCompareProc;
	tvs.lParam = (LPARAM)r.pItemsMgr;
	
	m_treeCtrl.SortChildrenCB( &tvs, bRecursive );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::ResetTree()
{
	if ( !::IsWindow( m_hWnd ) )
		return;
	HTREEITEM hti = m_treeCtrl.GetSelectedItem();
  int id = hti ? m_treeCtrl.GetItemData( hti ) : 0;
	int nTree = GetTreeID( hti );
	
  m_treeCtrl.DeleteAllItems();
  m_treeCtrl.Invalidate();
  m_treeCtrl.UpdateWindow();
	int nFold = IsFolder( id ) ? GetFoldID( id ) : -1;
	int nItem = IsFolder( id ) ? -1 : id;
  HTREEITEM hf = 0;
	HTREEITEM hi = 0;
	for ( int i = 0; i < roots.size(); ++i )
	{
		const SRoot &r = roots[i];
		if ( !r.pItemsMgr->IsLoaded() )
			r.pItemsMgr->Load();
		HTREEITEM hroot;
		hf = MakeFolders( &hroot, r.nTreeID, nFold );
		if ( !bOnlyFolders )
			hi = MakeItems( r.nTreeID, nItem );
		else
			m_treeCtrl.ExpandCompletely( m_treeCtrl.GetRootItem() );
		Sort( hroot );
	}

	m_treeCtrl.UpdateWindow();
	
	// по возможности сохраняем старый селекшен
	if ( hf )
		m_treeCtrl.SelectItem( hf );
	if ( hi )
		m_treeCtrl.SelectItem( hi );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CWnd::OnShowWindow(bShow, nStatus);
	
	if ( bShow && !bInit )
	{
		bInit = true;
		ResetTree();
	}
	// для правильной работы QuickView окна, 
	// которое показывает текущий поселекченный объект
	//if ( !bShow )
	//	m_treeCtrl.DeselectAllItems();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвр. ID поселекченного итема или -1, если такого нет
bool CMETreeView::GetSelectedItemID( int *pnTreeID, int *pnItemID )
{
	HTREEITEM hti = m_treeCtrl.GetSelectedItem();
	
	if ( !hti )
		return false;
	*pnTreeID = GetTreeID( hti );
  *pnItemID = m_treeCtrl.GetItemData( hti );
	if ( IsFolder( *pnItemID ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::GetSelectedFolderID( int *pnTreeID, int *pnFolderID )
{
	HTREEITEM hti = m_treeCtrl.GetSelectedItem();
	
	if ( !hti )
		return false;
  int id = m_treeCtrl.GetItemData( hti );
	if ( !IsFolder( id ) )
		return false;
	*pnTreeID = GetTreeID( hti );
	*pnFolderID = GetFoldID( id );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnRename() 
{
	HTREEITEM hti = m_treeCtrl.GetSelectedItem();
	
	if ( !hti )
		return;
	m_treeCtrl.EditLabel( hti );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LPNMTVKEYDOWN ptvkd = (LPNMTVKEYDOWN)pNMHDR;

	switch( ptvkd->wVKey )
	{
		case VK_DELETE:
			OnDeleteitem();
			break;
		case VK_INSERT:
			OnNewitem();
			break;
		case VK_RETURN:
			{
				HTREEITEM hti = m_treeCtrl.GetSelectedItem();
				if ( hti )
				{
					LPARAM lParam = m_treeCtrl.GetItemData( hti );
					// Если поселекчен итем, шлем родительскому окну сообщение
					if ( !IsFolder( lParam ) )
						GetParent()->PostMessage( WM_ME_TREESEL, GetTreeID( hti ), lParam );
				}
			}
			break;
		case VK_ESCAPE:
		{
			CWnd *pPrnt = GetParent();
			if ( pPrnt )
				pPrnt->PostMessage( WM_ME_CANCEL );
		}
		break;
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::Copy( HTREEITEM htiDestFold, HTREEITEM htiSrcFold, bool bNeddCopyStr )
{
  if ( !htiDestFold || !htiSrcFold )
    return false;
	int dstData = m_treeCtrl.GetItemData( htiDestFold );
	int srcData = m_treeCtrl.GetItemData( htiSrcFold );
  if ( !IsFolder( dstData ) || !IsFolder( srcData ) || GetTreeID( htiSrcFold ) != GetTreeID( htiDestFold ) )
    return false;
	SRoot r;
	if ( !GetInfo( &r, htiSrcFold ) )
		return false;
	// не пытаемся ли мы перетащить папку в свою подпапку ?
	if ( r.pItemsMgr->IsSubfolder( GetFoldID( srcData ), GetFoldID( dstData ) ) )
		return false;
	// добавим новую папку в базу
	CString szName = bNeddCopyStr ? "Copy of " : "";
	szName += m_treeCtrl.GetItemText( htiSrcFold );
  int newID = r.pItemsMgr->AddFolder( r.nListenerID, GetFoldID( dstData ), (LPCSTR)szName );
  if ( -1 == newID )
    return false;
	
  HTREEITEM htiNew = m_treeCtrl.InsertItem( szName, 0, 0, htiDestFold );
  m_treeCtrl.SetItemData( htiNew, MakeFoldParam( newID ) );
  //
  HTREEITEM hti = m_treeCtrl.GetChildItem( htiSrcFold );
  while ( hti )
  {
    int nID = m_treeCtrl.GetItemData( hti );
    if ( IsFolder( nID ) )
		{
      if ( !Copy( htiNew, hti ) )
				return false;
		}
    else
		{
      if ( !CopyItem( htiNew, hti ) )
				return false;
		}
    hti = m_treeCtrl.GetNextSiblingItem( hti );
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CopyVariantProperties( CItemsMgr *pItemsMgr, int nDstID, int nDstVarID, int nSrcID, int nSrcVarID )
{
	// копируем все св-ва объекта
	const CPropMap *pSrcProps = pItemsMgr->GetPropList( nSrcID, nSrcVarID );
	const CPropMap *pDstProps = pItemsMgr->GetPropList( nDstID, nDstVarID );
	if ( pSrcProps && pDstProps )
	{
		if ( pSrcProps->size() > 0 )
		{
			for ( CPropMap::const_iterator it = pSrcProps->begin(); it != pSrcProps->end(); ++it )
			{
				CPropMap::const_iterator itd = pDstProps->find( it->first );
				if ( itd == pDstProps->end() )
					continue;
				CDynamicCast<CListProp> plist(itd->second);
				if (plist)
					plist->Copy( dynamic_cast<CListProp*>( it->second.GetPtr() ) );
				else
					itd->second->SetValue( it->second->GetValue(), false ); // не записываем в базу каждое св-во в отдельности
			}
			// а теперь записываем всю PropMap в базу
			if ( pItemsMgr->IsUniTemplate() )
			{
				if ( !pItemsMgr->SetVariantProps( nDstID, nDstVarID, pDstProps ) )
					return;
			}
			else
			{
				if ( !pItemsMgr->SetItemProps( nDstID, pDstProps ) )
					return;
			}
		}
		//
		pItemsMgr->ReleasePropList( pSrcProps );
		pItemsMgr->ReleasePropList( pDstProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateMapDB()
{
	Sleep(0);
	NDatabase::Refresh<NDb::CTemplate>();
	NDatabase::Refresh<NDb::CTemplVariant>();
	NDatabase::Refresh<NDb::CRectangle>();
	NDatabase::Refresh<NDb::CUnit>();
	NDatabase::Refresh<NDb::CFinalElement>();
	NDatabase::Refresh<NDb::CRndTerrainSpot>();
	NDatabase::Refresh<NDb::CExplosion>();
	NDatabase::Refresh<NDb::CWaypoint>();
	NDatabase::Refresh<NDb::CRPGArmor>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::CopyItem( HTREEITEM htiDestFold, HTREEITEM htiItem, bool bNeddCopyStr )
{
  if ( !htiDestFold || !htiItem )
    return false;
	int dstData = m_treeCtrl.GetItemData( htiDestFold );
	int itemData = m_treeCtrl.GetItemData( htiItem );
  if ( !IsFolder( dstData ) || IsFolder( itemData ) || GetTreeID( htiDestFold ) != GetTreeID( htiItem ) )
    return false;
	SRoot r;
	if ( !GetInfo( &r, htiItem ) )
		return false;
	// добавим новый итем в базу
	CString szName = bNeddCopyStr ? "Copy of " : "";
	szName += m_treeCtrl.GetItemText( htiItem );
  int newID = r.pItemsMgr->AddItem( r.nListenerID, GetFoldID( dstData ), (LPCSTR)szName, IDC_TEMPLATE_TREE != r.nTreeID );
  if ( -1 == newID )
    return false;

	static CPlaceDB placeDB;
	switch ( r.nTreeID )
	{
		case IDC_OBJECTS_TREE:
			placeDB.InsertPlacableObject( "ObjectTemplates", (LPCSTR)szName, newID );
			break;
		case IDC_RPG_ITEMS_TREE:
			placeDB.InsertPlacableObject( "RPGItems", (LPCSTR)szName, newID );
			break;
	}

	// копируем объект
  if ( IDC_TEMPLATE_TREE == r.nTreeID )
	{
		CTemplate *pTempl = theTemplMgr.GetTempl( itemData );
		if ( !pTempl )
			return false;
    if ( !theTemplMgr.AddTemplate( newID, pTempl->GetWidth(), pTempl->GetHeight() ) )
      return false;
		Sleep(0);
		NDatabase::Refresh<NDb::CTemplate>();
		vector<int> oldVars, vars;
		int i;

		r.pItemsMgr->GetItemVariants( newID, &oldVars );
		for ( i = 0; i < oldVars.size(); ++i )
			r.pItemsMgr->DeleteVariant( newID, oldVars[i] );
		//NDatabase::Refresh<NDb::CTemplVariant>();
		r.pItemsMgr->GetItemVariants( itemData, &vars );
		//
		vector<int> newVarIDs;
		for ( i = 0; i < vars.size(); ++i )
		{
			int varID = r.pItemsMgr->AddVariant( newID );
			CopyVariantProperties( r.pItemsMgr, newID, varID, itemData, vars[i] );
			newVarIDs.push_back( varID );
			if ( -1 == varID )
				break;
		}
		Sleep(0);
		NDatabase::Refresh<NDb::CTemplate>();
		NDatabase::Refresh<NDb::CTemplVariant>();
		//NDatabase::Import();
		for ( i = 0; i < newVarIDs.size(); ++i )
		{
			int varID = newVarIDs[i];
			if ( -1 == varID )
				continue;
			CPlacement *pSrc = theTemplMgr.GetPlacement( vars[i] );
			CPlacement *pPl = theTemplMgr.GetPlacement( varID );
			pPl->CopyFrom( *pSrc );
			theTemplMgr.ReleasePlacement( pSrc );
			theTemplMgr.ReleasePlacement( pPl );
		}
		Sleep(0);
		UpdateMapDB();
	} 
	else if ( r.pItemsMgr->IsUniTemplate() )
	{
		int i;
		vector<int> vars;

		r.pItemsMgr->GetItemVariants( newID, &vars );
		for ( i = 0; i < vars.size(); ++i )
			r.pItemsMgr->DeleteVariant( newID, vars[i] );
		r.pItemsMgr->GetItemVariants( itemData, &vars );
		for ( i = 0; i < vars.size(); ++i )
		{
			int varID = r.pItemsMgr->AddVariant( newID );
			if ( -1 == varID )
				break;
			CopyVariant( r.pItemsMgr, newID, varID, itemData, vars[i] );
		}
	}
	else
		CopyVariant( r.pItemsMgr, newID, -1, itemData, -1 );

	HTREEITEM newhti = (HTREEITEM)ItemAdded( r.nTreeID, newID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::CopyVariant( CItemsMgr *pItemsMgr, int nDstID, int nDstVarID, int nSrcID, int nSrcVarID )
{
	CopyVariantProperties( pItemsMgr, nDstID, nDstVarID, nSrcID, nSrcVarID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMETreeView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CMETreeView::GetItemPath( int nTreeID, int nItemID )
{
	if ( nItemID < 0 )
		return "";

	SRoot r;
	if ( !GetInfo( &r, nTreeID ) )
		return "";
	vector<string> path;
	path.push_back( r.pItemsMgr->GetItemName( nItemID ) );

	int nFID = r.pItemsMgr->GetItemFolderID( nItemID );
	HTREEITEM htif = GetFolder( nTreeID, nFID );
	while ( htif )
	{
		path.push_back( LPCSTR( m_treeCtrl.GetItemText( htif ) + "\\" ) );
		htif = m_treeCtrl.GetParentItem( htif );
	}
	string szPath;
	for ( vector<string>::reverse_iterator rit = path.rbegin(); rit != path.rend(); ++rit )
		szPath += *rit;
	return szPath;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HTREEITEM CMETreeView::GetItem( int nTreeID, int nItemID )
{
	SRoot r;
	if ( !GetInfo( &r, nTreeID ) )
		return 0;
	int nFID = r.pItemsMgr->GetItemFolderID( nItemID );
	HTREEITEM htif = GetFolder( nTreeID, nFID );
	if ( !htif )
		return false;
  HTREEITEM hti = m_treeCtrl.GetChildItem( htif );
  while ( hti )
  {
    if ( m_treeCtrl.GetItemData( hti ) == nItemID )
			return hti;
    hti = m_treeCtrl.GetNextSiblingItem( hti );
  }
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::SelectItem( int nTreeID, int nItemID )
{
	HTREEITEM hti = GetItem( nTreeID, nItemID );
	if ( !hti )
		return false;
	m_treeCtrl.DeselectAllItems();
	m_treeCtrl.Select( hti, TVGN_CARET );
	m_treeCtrl.SetFocus();
	CWnd *pWnd = GetParent();
	if ( pWnd )
		pWnd->PostMessage( WM_ME_TREESELCHANGED, nTreeID, nItemID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult)
{
	if ( m_bExpanding || !(0x8000 & GetAsyncKeyState( VK_SHIFT )) )
		return;
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;

	HTREEITEM hti = pnmtv->itemNew.hItem;
	if ( !hti )
		return;
	int nAction = pnmtv->action;
	if ( TVE_TOGGLE == nAction )
		nAction = m_treeCtrl.IsExpanded( hti ) ? TVE_COLLAPSE : TVE_EXPAND;

	m_bExpanding = true; // защита от переполнения стека

	switch( nAction )
	{
		case TVE_COLLAPSE:
			m_treeCtrl.CollapseCompletely( hti );
			break;
		case TVE_EXPAND:
			m_treeCtrl.ExpandCompletely( hti );
			break;
	}
	m_bExpanding = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeCtrl::PickTextColors(LvPaintContext* pPC)
{
	ASSERT(pPC);
	SECTreeCtrl::PickTextColors(pPC);

	TvPaintContext* pTvPC = (TvPaintContext*)pPC;

	if ( !pTvPC || pTvPC->tvi.state & TVIS_SELECTED )
		return;
	
	//TV_ITEM tvi = *((TV_ITEM*)((char*)pTvPC + 120));
	TV_ITEM tvi = pTvPC->tvi;
	int id = tvi.lParam;

	HTREEITEM hti = GetRootItem( tvi.hItem );
	if ( hti )
	{
		CMETreeView::SRoot r; 
		if ( !pTree->GetInfo( &r, hti ) )
			return;

		if ( IsFolder( id ) )
		{
			int nTreeID = GetFoldID( GetItemData( hti ) );
			DWORD cr = r.pItemsMgr->GetFolderColor( GetFoldID( id ) );
			if ( cr != 0 )
			{
				pTvPC->rgbText = cr;
				return;
			}
		}
		else
		{
			DWORD cr = r.pItemsMgr->GetItemColor( id );
			if ( cr != 0 )
			{
				pTvPC->rgbText = cr;
				return;
			}
		}
	}
	pTvPC->rgbText = GetSysColor( COLOR_WINDOWTEXT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnFolderColor() 
{
	HTREEITEM hti = m_treeCtrl.GetSelectedItem();

	if ( !hti )
		return;
	SRoot r;
	if ( !GetInfo( &r, hti ) )
		return;
	int id = m_treeCtrl.GetItemData( hti );
	bool bFolder = IsFolder( id );
	DWORD cr = bFolder ? r.pItemsMgr->GetFolderColor( GetFoldID( id ) ) : r.pItemsMgr->GetItemColor( id );
	CColorDialog dlg( cr );

	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	//
	for ( HTREEITEM hti = m_treeCtrl.GetFirstSelectedItem(); hti != 0; hti = m_treeCtrl.GetNextSelectedItem( hti ) )
	{
		SRoot r;
		if ( !GetInfo( &r, hti ) )
			return;
		int id = m_treeCtrl.GetItemData( hti );
		if ( IsFolder( id ) )
			r.pItemsMgr->SetFolderColor( GetFoldID( id ), dlg.GetColor() );
		else
			r.pItemsMgr->SetItemColor( id, dlg.GetColor() );
		m_treeCtrl.InvalidateItem( hti );
	}
	Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CMETreeView::OnSort( WPARAM wParam, LPARAM lParam )
{
	Sort( (HTREEITEM)wParam, false );
	Invalidate();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMETreeView::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WMU_SORT:
			OnSort( pMsg->wParam, pMsg->lParam );
			break;
		case WM_KEYDOWN:
			switch ( pMsg->wParam )
			{
				case VK_F3:
				OnFolderFind();
				break;
			}
			break;
	}
	return CWnd::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CTreeDropTarget::OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point)
{
	ASSERT(pTree);
	if ( pWnd == pTree )
		return pTree->OnDragEnter( pDataObject, dwKeyState, point);
	return DROPEFFECT_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CTreeDropTarget::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	ASSERT(pTree);
	if ( pWnd == pTree )
		return pTree->OnDragOver( pDataObject, dwKeyState, point);
	return DROPEFFECT_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTreeDropTarget::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	ASSERT(pTree);
	if ( pWnd == pTree )
		return pTree->OnDrop( pDataObject, dropEffect, point);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CTreeDropTarget::OnDropEx(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point)
{
	ASSERT(pTree);
	return (DROPEFFECT)-1;  // not implemented
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTreeDropTarget::OnDragLeave(CWnd* pWnd)
{
	ASSERT(pTree);
	if ( pWnd == pTree )
		pTree->OnDragLeave();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CMETreeView::OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	return OnDragOver(pDataObject, dwKeyState, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CMETreeView::OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT de=DROPEFFECT_NONE;

	if ( !m_bDragging )
		return de;
	if( dwKeyState & MK_CONTROL )
		de=DROPEFFECT_COPY;
	else
		de=DROPEFFECT_MOVE;

	//hit-test stuff
	HTREEITEM hti;
	UINT      htflags;

	ClientToScreen(&point);
	CPoint ptX=point;

	//Stingray control
	m_treeCtrl.ScreenToClient(&ptX);
	hti = m_treeCtrl.HitTest( ptX, &htflags );
	if( hti )
		m_treeCtrl.SelectDropTarget( hti, FALSE );	

	return de;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnDragLeave()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMETreeView::OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	m_treeCtrl.StopAutoScroll();
  if( !m_bDragging )
		return false;

	SRoot r;
	if ( !GetInfo( &r, m_hitemDrag ) )
		return false;
  TRACE(_T("End Drag\n"));

  LPARAM lDragParam = m_treeCtrl.GetItemData( m_hitemDrag );
  
  CRect rect;
  GetClientRect( &rect );

  // если объект кинули мимо нашего окна, посылаем сообщение родительскому окну и выходим
  if ( !rect.PtInRect( point ) && !IsFolder( lDragParam ) )
  {
    m_treeCtrl.SelectDropTarget( 0 );
    GetParent()->PostMessage( WM_ME_DROPITEM, r.nTreeID, lDragParam );
    return false;
  }

  HTREEITEM hItemDrop = m_treeCtrl.GetDropHilightItem( );
  m_treeCtrl.SelectDropTarget( 0 );
  if ( hItemDrop == NULL || hItemDrop == m_hitemDrag || GetTreeID( hItemDrop ) != r.nTreeID )
    return false;

  // Иначе перестраиваем структуру дерева

  LPARAM lparam = m_treeCtrl.GetItemData( hItemDrop );

	vector<HTREEITEM> hitems;
	HTREEITEM m_hitemDrag = m_treeCtrl.GetFirstSelectedItem();
	for ( ; m_hitemDrag != 0; m_hitemDrag = m_treeCtrl.GetNextSelectedItem( m_hitemDrag ) )
		if ( GetTreeID( m_hitemDrag ) == r.nTreeID )
			hitems.push_back( m_hitemDrag );

	for ( int i = 0; i < hitems.size(); ++i )
	{
		m_hitemDrag = hitems[i];
		if ( DROPEFFECT_COPY == dropEffect )
		{
			BeginWaitCursor();
			if ( !IsFolder( lparam ) ) // можно кидать только в папку
				hItemDrop = m_treeCtrl.GetParentItem( hItemDrop );
			if ( IsFolder( lDragParam ) )
			{
				Copy( hItemDrop, m_hitemDrag, false );
				Sort( hItemDrop );
			}
			else
			{
				CopyItem( hItemDrop, m_hitemDrag, false );
				Sort( hItemDrop, false );
			}
			Invalidate();
			EndWaitCursor();
		}
		else
		{
			if ( IsFolder( lparam ) ) // можно кидать только в папку
				ChangeParent( m_hitemDrag, hItemDrop );
			else
			{
				HTREEITEM hNewParent = m_treeCtrl.GetParentItem( hItemDrop );
				if ( hNewParent && hNewParent != m_hitemDrag )
					ChangeParent( m_hitemDrag, hNewParent, hItemDrop );
			}
		}
	}
	m_bDragging = false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnFolderFind() 
{
	CItemsMgr *pItemsMgr = roots.front().pItemsMgr;
	if ( !pItemsMgr )
		return;
	CFindDialog dlg( pItemsMgr->IsUniTemplate(), this );

	if ( dlg.DoModal() != IDOK )
		return;

	HTREEITEM hti = 0;
	BeginWaitCursor();
	if ( dlg.bItem )
	{
		hti = GetItem( roots.front().nTreeID, dlg.m_nItemID );
	}
	else
	{
		int nID = pItemsMgr->FindVariant( dlg.m_nVariantID );
		if ( nID > 0 )
			hti = GetItem( roots.front().nTreeID, nID );
	}
	EndWaitCursor();
	//
	if ( !hti )
		MessageBox( "Item not found", "Find" );
	else
	{
		m_treeCtrl.DeselectAllItems();
		m_treeCtrl.SelectItem( hti );
		m_treeCtrl.SetFocus();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnSetFocus( CWnd* pOldWnd )
{
	m_treeCtrl.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMETreeView::OnCopyNames()
{
	string str;
	for ( HTREEITEM h = m_treeCtrl.GetFirstSelectedItem(); h != 0; h = m_treeCtrl.GetNextSelectedItem( h ) )
	{
		int nID = m_treeCtrl.GetItemData( h );
		if ( !IsFolder( nID ) )
		{
			SRoot r;
			if ( GetInfo( &r, h ) )
			{
				str += r.szRootName + '\\' + r.pItemsMgr->GetItemPath( nID );
				str += "\r\n";
			}
		}
	}
	//
	if ( !::OpenClipboard( theApp.GetMainWnd()->m_hWnd ) )
  {
    AfxMessageBox( "Cannot open the Clipboard" );
    return;
  }
  // Remove the current Clipboard contents
	if( !::EmptyClipboard() )
  {
    AfxMessageBox( "Cannot empty the Clipboard" );
    return;
  }
	//
	HGLOBAL hGlobal = ::GlobalAlloc( GMEM_SHARE|GMEM_FIXED, str.size() + 1 );
	LPTSTR lpszDbObjPtr = (LPTSTR)::GlobalLock( hGlobal );
	strcpy( lpszDbObjPtr, str.c_str() );
	//Put reference to global memory into clipboard
	::GlobalUnlock( hGlobal );

  // For the appropriate data formats...
	if ( ::SetClipboardData( CF_TEXT, hGlobal ) == NULL )
    AfxMessageBox( "Unable to set Clipboard data" );
	::CloseClipboard();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMETreeView::PrepareInsertItem( SPrepareInsert *pPrepare )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SInsert CMETreeView::InsertItem( const SPrepareInsert &params )
{
	SInsert ins;

	return ins;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
