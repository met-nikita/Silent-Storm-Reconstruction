// MapEdit.h : main header file for the TEDIT application
//

#if !defined(AFX_TEDIT_H__AAD7C19E_DAE6_42E5_A759_066B70E941F4__INCLUDED_)
#define AFX_TEDIT_H__AAD7C19E_DAE6_42E5_A759_066B70E941F4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "PropMapTypedef.h"
//#include "PropMap.h"

class CChildView;
class CModelView;
class CGameView;
class CMainFrame;
class CGameFrame;
class CTmplTreeWnd;
class CItemsMgr;
class CPlacement;
class CTreeSelItemDlg;
class COIDlg;

string GetResString( UINT nID );
string MakeDoubleSlash( string str );
string MakeOneSlash( string str );
void   CopyLastVersion();
string IToA( int n );

typedef unordered_map<int, CPlacement*> CPlacementCache;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct SResTree
{
  int         nTreeID;
  string      szTabName;
  string      szRootName;
  CItemsMgr   *pItemsTree;
	CTreeSelItemDlg *pTreeDlg;
	bool				bVisible;
	bool				bHideTree;
  // Оповещение о том, что в Tree View выбран элемент nItemID
  void (*pSelectCb)( int nItemID, int nVariantID );
  void (*pDoExport)( CItemsMgr *pItemsTree, vector<int> nItemIDs, bool bForceExport ); // nItemIDs может меняться внутри функции
	void (*pDoUpdateDB)( CItemsMgr *pItemsTree, const vector<int> &nItemIDs );
  //
  SResTree() : bVisible( false ), pItemsTree( 0 ), pSelectCb( 0 ), pDoExport( 0 ), pDoUpdateDB( 0 ), pTreeDlg( 0 ), bHideTree(false) { }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBrush
{
	int		nTreeID;
	int		nItemID;
	float nSizeX;
	float nSizeY;
	float fThickness;
	COLORREF color;

	SBrush() : nTreeID( -1 ), nItemID( -1 ), color( 0 ) {}
	SBrush( int nTreeId, int nItemId, int nX = 1, int nY = 1, float fThick = 0.1f, COLORREF cr = 0 ) 
		: nTreeID( nTreeId ), nItemID( nItemId ), color( cr ), nSizeX( nX ), nSizeY( nY ), fThickness( fThick ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp:
// See TEdit.cpp for the implementation of this class
//

class CTEditApp : public CWinApp
{
public:
	bool bInputActive;
	bool bGameActive;
	//
	CTEditApp();
  ~CTEditApp();

  CChildView* GetTemplateView();
  CGameView*  GetGameView();
  CGameFrame* GetGameFrame();
  //
  const  SResTree* GetResTree( int nTreeID );
  string GetResSrcDir() const;
  void SetActiveItem( int nResourceTree, int nItemID, int nVariantID = -1, bool bUpdateHistory = true );
	void SetActiveVariant( int nVariantID );
	void GetActiveItem( int *pnResTree, int *pnItemID, int *pnVariantID );
	void SelChanged( int nTreeID, int nItemID );
	void SetActiveFloor( int nFloorID );
  //
  int  GetActiveFloor() const { return nActiveFloor; }
  bool IsCursorInGridView() const;
  void DropTemplate( int id );
  bool DropItem( int nTableID, int nItemID );
	bool DropItem( int nDstTableID, const CPropMap *pDstProps, COIDlg *pView, int nTableID, int nItemID );
	bool CheckDropExeptions( int nTblTarget, int nTblDrop );
	int  GetDropExeptionTarget( int nTblTarget, int nItemTarget, int nTblDrop, int nItemDrop );
  //
  void SetTemplate( int id, int nVarID );
  void SetPlacement( int id );
  void SetModel( int id, int nVarID );
  void SetGeometry( int id, int nVarID );
  void SetMaterial( int id, int nVarID );
  void SetTexture( int id, int nVarID );
	void SetAnimation( int id, int nVarID );
	void SetParticle( int id, int nVarID );
	void SetContainer( int id, int nVarID );
	void SetObject( int id, int nVarID );
	void SetAIModel( int id, int nVarID );
	void SetSound( int id, int nVarID );
	void SetUI( int id, int nVarID );
	void SetWeapon( int id, int nVarID );
	void SetPers( int id, int nVarID );
	void SetChapter( int id, int nVarID );
	void SetGlobalMap( int id, int nVarID );
	void SetRPGItem( int id, int nVarID );
	void SetConstructionPart( int id, int nVarID );
	void SetHead( int id, int nVarID );
	void SetActiveBrushModel( int nTreeID, int brushID, int nVarID );
	void SetScenario( int id, int nVarID );
	void SetAmbientLight( int id, int nVar );
	void SetRndSound( int id, int nVar );
	void SetSoundEffect( int id, int nVar );
	void SetDiplomacy( int id, int nVar );
  void SetEmptyView();
	CPlacement* GetActivePlacement() const;
	const CPropMap* SetPropMap( const CPropMap *pProps );
  //
  void SetRectViewTitle( const std::string& szTitle );
  void UpdateGameDB();
  bool IsEditingTemplate() const;
  bool HasShadows() const { return bShadows; }
	bool IsCameraReset() const { return bCameraReset; }
	bool IsRealtimePreview() { return bRealtimePreview; }
	int  GetTemplateMaxDepth() const { return nMaxTreeDepth; } // глубина, на которую разворачиваются темплейты в TemplateView
	void SetTemplateMaxDepth( int nDepth );

	friend bool LoadResources();
	const unordered_map<int, SResTree>& GetResTrees() const;
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTEditApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	//}}AFX_VIRTUAL

// Implementation

protected:
	unordered_map<int, SResTree> resTreesHash;
  CMainFrame *pMainFrame;
	HMENU  m_hMDIMenu;
	HACCEL m_hMDIAccel;
  bool   bShadows;
	CFont	 m_TipFont;
	int    nActiveFloor;
  
  struct SActiveItem
  {
    int nTableID;
    int nItemID;
		int nVariantID;
    const CPropMap *pProps;
		CPlacement *pPlacement;
    
    SActiveItem() : nTableID( -1 ), nItemID( -1 ), pProps( 0 ), pPlacement( 0 ), nVariantID( -1 ) {}
  };
	CPlacementCache placementCache;
	int nMaxTreeDepth;
  SActiveItem activeItem;

	void CollectPlacementTree( CPlacement *pRoot, CPlacementCache *pCache, int nDepth );
	void ClearPlacementCache( CPlacementCache *pCache );

public:
	//{{AFX_MSG(CTEditApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileNew();
	afx_msg void OnFileSaveall();
	afx_msg void OnViewUpdgame();
	afx_msg void OnNewVariant();
	afx_msg void OnUpdateNewVariant(CCmdUI* pCmdUI);
	afx_msg void OnDelvariant();
	afx_msg void OnUpdateDelvariant(CCmdUI* pCmdUI);
	afx_msg void OnReloadTree();
	afx_msg void OnViewShadows();
	afx_msg void OnUpdateViewShadows(CCmdUI* pCmdUI);
	afx_msg void OnHelpTipoftheday();
	afx_msg void OnGoback();
	afx_msg void OnGoforward();
	afx_msg void OnUpdateGoback(CCmdUI* pCmdUI);
	afx_msg void OnUpdateGoforward(CCmdUI* pCmdUI);
	afx_msg void OnViewCamerareset();
	afx_msg void OnViewRealtimePreview();
	afx_msg void OnUpdateViewRealtimePreview(CCmdUI* pCmdUI);
	afx_msg void OnMakeCurrent();
	afx_msg void OnViewRoll();
	afx_msg void OnCopyVariant();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


private:
	BOOL m_bATLInited;
	bool bCameraReset;
	bool bRealtimePreview;
private:
	BOOL InitATL();
	void CheckMapEditVerison();
};

inline void CTEditApp::GetActiveItem( int *pnResTree, int *pnItemID, int *pnVariantID )
{ 
	*pnResTree   = activeItem.nTableID;
	*pnItemID    = activeItem.nItemID;
	*pnVariantID = activeItem.nVariantID;
}

extern CTEditApp theApp;

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEDIT_H__AAD7C19E_DAE6_42E5_A759_066B70E941F4__INCLUDED_)
