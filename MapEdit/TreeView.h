#ifndef __TREEVIEW_H__
#define __TREEVIEW_H__

#include "ItemsMgr.h"
#include "resource.h"

// сообщения, посылаемые родительскому окну
// wParam - tree view ID
// lParam - ID итема
const UINT WM_ME_TREESEL  = WM_USER + 1;       // поселекчен другой элемент (двойным кликом)
const UINT WM_ME_DROPITEM = WM_USER + 2;       // 
const UINT WM_ME_TREESELCHANGED = WM_USER + 3;       // поселекчен другой элемент (кликом)
const UINT WM_ME_TREE_NEWITEM = WM_USER + 4;
const UINT WM_ME_TREE_DELITEM = WM_USER + 5;

// Чтобы отличить среди элементов дерева папки от итемов 
// по хранимому в элементе полю lParam
// используем модификатор для создания ID папок
const LPARAM FOLD_MODIFIER = 0x80000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsFolder( LPARAM lParam )
{
  return lParam & FOLD_MODIFIER;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline LPARAM MakeFoldParam( int nFolderID )
{
  return FOLD_MODIFIER | LPARAM( nFolderID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetFoldID( LPARAM lParam )
{
  return ~FOLD_MODIFIER & lParam;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMETreeView;
class CMETreeCtrl : public SECTreeCtrl
{
	//CItemsMgr *pItemsMgr;
	vector<SResTree> vTrees;
	CMETreeView *pTree;
protected:
	void PickTextColors(LvPaintContext* pPC);
public:
	CMETreeCtrl( vector<SResTree> _vTrees, CMETreeView *_pTree ) : vTrees(_vTrees), pTree(_pTree) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTreeDropTarget: public COleDropTarget
{
public:
	CMETreeView *pTree;
public:
	CTreeDropTarget(): pTree(0) {}

	//{{AFX_VIRTUAL(CTreeDropTarget)
public:
	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
	virtual DROPEFFECT OnDropEx(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point);
	virtual void OnDragLeave(CWnd* pWnd);
	//}}AFX_VIRTUAL
};
struct SPrepareInsert;
struct SInsert
{
	int nNewID;
	HTREEITEM hti;

	SInsert() : nNewID(-1), hti(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMETreeView : public CWnd, ITreeView
{
	DECLARE_DYNCREATE(CMETreeView)
  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMETreeView)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual DROPEFFECT OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual DROPEFFECT OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
	virtual void OnDragLeave();
	virtual BOOL OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
	//}}AFX_VIRTUAL
  
  // Implementation
public:
	CMETreeView(): m_treeCtrl(vector<SResTree>(), this) {}
  CMETreeView( const vector<SResTree> &vResTrees, bool bOnlyFolders = false, UINT nMenuID = IDR_FOLDERS );
  virtual ~CMETreeView();
  
  virtual int  FolderAdded( int nTreeID, int nFolderID );
  virtual void ParentChanged( int nTreeID, int nFolderID, int nOldParent );
  virtual int  ItemAdded( int nTreeID, int nItemID );
  virtual void ItemDeleted( int nTreeID, int nItemID );
  virtual void ItemFolderChanged( int nTreeID, int nItemID, int nOldFolder );
  virtual void FolderDeleted( int nTreeID, int nFolderID );
  virtual void ResetTree();

	virtual bool GetSelectedItemID( int *pnTreeID, int *pnItemID );
	virtual string GetItemPath( int nTreeID, int nItemID );
	virtual bool SelectItem( int nTreeID, int nItemID );
	virtual bool GetSelectedFolderID( int *pnTreeID, int *pnFolderID );
  
  // Generated message map functions
protected:
  CMETreeCtrl m_treeCtrl;
  CMenu       m_menu;

	struct SRoot
	{
		string      szRootName;
		//
		SResTree   resTree;
		CItemsMgr  *pItemsMgr;
		int nListenerID;
		int nTreeID;

		SRoot(): pItemsMgr(0), nListenerID(-1), nTreeID(-1) {}
	};
	vector<SRoot> roots;

	HCURSOR m_hPlusCursor;
	HCURSOR m_hArrowCursor;
	HCURSOR m_hNoCursor;
  
  bool m_bDragging;
  CImageList* m_pDragImageList;
  HTREEITEM m_hitemDrop;
  HTREEITEM m_hitemDrag;
	bool m_bExpanding;
	CTreeDropTarget oleDropTarget;
	bool bOnlyFolders;
	bool bInit;

  CMenu     m_popup;
  
  HTREEITEM GetFolder( int nTreeID, int nFolderID );
//  HTREEITEM GetFolderFast( int nFolderID );
	HTREEITEM GetItem( int nTreeID, int nItemID );
  void InitImageLists();
  HTREEITEM MakeFolders( HTREEITEM *hRoot, int nTreeID = -1, int nID = -1 );
  HTREEITEM MakeItems( int nTreeID = -1, int nID = -1 );
  void ChangeParent( HTREEITEM hti, HTREEITEM htiNewParent, HTREEITEM htiInsertAfter = 0 );
  void MenuState( CMenu *pMenu, HTREEITEM hti );
  void CollectItems( HTREEITEM htiFolder, vector<int> *pItems );
	void CollectFolders( HTREEITEM htiFolder, vector<int> *pFolders );
	void PrepareExport( vector<int> *pItems );
  HTREEITEM AddItem( int nTreeID, int nItemID );
	bool Copy( HTREEITEM htiDestFold, HTREEITEM htiSrcFold, bool bNeddCopyStr = false );
	bool CopyItem( HTREEITEM htiDestFold, HTREEITEM htiItem, bool bNeddCopyStr = false  );
	void ExpandAll( HTREEITEM hti, int nAction );
	void CopyVariant( CItemsMgr *pItems, int nDstID, int nDstVarID, int nSrcID, int nSrcVarID );
	void Sort( HTREEITEM hti, bool bRecursive = true );
	void DeleteFolder( HTREEITEM hti );

	int GetTreeID( HTREEITEM hti ) const;
	bool GetInfo( SRoot *pr, HTREEITEM hti ) const;
	bool GetInfo( SRoot *pr, int nTreeID ) const;

	bool PrepareInsertItem( SPrepareInsert *pPrepare );
	SInsert InsertItem( const SPrepareInsert &params );
	
	friend int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	friend class CMETreeCtrl;

  //{{AFX_MSG(CMETreeView)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDblClick(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelect(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndEdit(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeyDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNewFolder();
	afx_msg void OnDeleteFolder();
	afx_msg void OnClose();
	afx_msg void OnNewitem();
	afx_msg void OnDeleteitem();
	afx_msg void OnExportItem();
	afx_msg void OnForcedExportitem();
	afx_msg void OnReloadTree();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnRename();
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnFolderColor();
	afx_msg void OnUpdatedbvalues();
	afx_msg LRESULT OnSort( WPARAM wParam, LPARAM lParam );
	afx_msg void OnFolderFind();
	afx_msg void OnCopyNames();
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    
  protected:
    CImageList m_imlNormal;
};
#endif // __TREEVIEW_H__
