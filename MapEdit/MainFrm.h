// MainFrm.h : interface of the CMainFrame class
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__09784638_643A_4FE0_9154_78EB417C5347__INCLUDED_)
#define AFX_MAINFRM_H__09784638_643A_4FE0_9154_78EB417C5347__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TreeWnd.h"
#include "AnalyseData.h"
#include "WysiwygBar.h"
#include <string>

class CChildView;
class CGameView;
class CGameFrame;
class CIconView;
class CPropView;
class CParamsView;
class CUIView;
class CChapterView;
class CScenarioView;
class CGlobalMapView;
class CDiplomacyView;

enum EView
{
  GRID_VIEW,
  GAME_VIEW,
	PARAMS_VIEW,
	UI_VIEW,
	CHAPTER_VIEW,
	SCENARIO_VIEW,
	GLOBALMAP_VIEW,
	DIPLOMACY_VIEW,
  MAX_VIEW,
};

enum EPlacement
{
  PT_GRID,
  PT_FIN,
  PT_UNIT
};

class CMainFrame;

class CFullScrennView: public SECFullScreenView
{
protected:
	CMainFrame *pMainWnd;
	virtual void CloseDefFSToolBar();

	virtual void PreFullScreenMode();
	virtual void PostFullScreenMode();
public:
	CFullScrennView( CMainFrame *pWnd ):pMainWnd(pWnd) { ASSERT( pWnd ); }
};

class CMainFrame : public SECWorkbook
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

  CChildView* GetTemplateView();
  CGameView*  GetGameView();
	CParamsView* GetParamsView();
	CUIView* GetUIView();
  CGameFrame*   GetGameFrame();
	CChapterView* GetChapterView();
	CGlobalMapView* GetGlobalMapView();
	CScenarioView* GetScenarioView();
	CDiplomacyView* GetDiplomacyView();
	CWysiwygBar* GetWysiwygBar() { return &wysiwygBar; }
  int  AddVariant( EPlacement type );
  void RemoveAllVariants();
  void SetActiveVariant( int nVariantID );
	int  GetActiveVariant() const { return iActiveVariantID; }
  void UpdateTreeView( int nTreeID );

  bool SetView( const vector<EView>& views );
	void SetActive( EView view );

// Attributes
public:
  CPropView    *const m_pPropView;
	CIconView    *const m_pIconView;
  SECCustomToolBar *m_pVarToolBar;
	SECCustomToolBar *m_pUIToolBar;

// Operations
public:
  bool IsCursorInGridView();
  void DropTemplate( int id );
	void DropUnit( int id );
	void DropObject( int id );
  void SetRectViewTitle( const std::string& szTitle );
  void RestoreLayout();
	void SetVariants( const vector<int> &variants, bool bSaveVarSelection = false );
	void ShowUIBar( bool bShow );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	UINT*	m_pDefButtonGroup;	// toolbar default button group
	UINT	m_nDefButtonCount;	// the number of elements in m_pDefaultButtons
  UINT  m_VarButtonCount;
  UINT*	m_pVarButtonGroup;
  UINT  m_ToolButtonCount;
  UINT*	m_pToolButtonGroup;
  UINT  m_UIButtonCount;
  UINT*	m_pUIButtonGroup;
  UINT  m_EditmodeButtonCount;
  UINT*	m_pEditmodeButtonGroup;
  UINT  m_PaintButtonCount;
  UINT*	m_pPaintButtonGroup;
  UINT  nVarBarID;
	UINT	nTemplateBarID;
	UINT	nUIBarID;
	UINT	nEditmodeBarID;
	UINT	nPaintBarID;

	SECStatusBar  m_wndStatusBar;
  vector<CTmplTreeWnd*> m_treeViews;
  CChildView*  m_pRectView;
  CModelView*  m_pModelView;
  CGameView*   m_pGameView;
  CGameFrame*  m_pGameFrame;
	CParamsView* m_pParamsView;
	CUIView* m_pUIView;
	CChapterView* pChapterView;
	CGlobalMapView *pGlobalMapView;
	CScenarioView *pScenarioView;
	CDiplomacyView *pDiplomacyView;
  HACCEL m_hMDIAccel;
  SECBmpMgr *m_pVarBmpMgr;
  UINT  nRefreshTimer;
	CAnalyseDataDlg analyseDlg;
	CWysiwygBar wysiwygBar;
	int iActiveVariantID;
	unordered_map<int, int> m_CmdID2VarID;
  unordered_map<int, int> m_VarID2CmdID;	
	CFullScrennView m_FSView;

  int viewInds[MAX_VIEW];

  void CreateTrees();
// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnNewTreeView();
	afx_msg void OnToolsCustomize();
	afx_msg void OnViewProperties();
	afx_msg void OnUpdateViewProperties(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnViewQuickview();
	afx_msg void OnUpdateViewQuickview(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNewtreeView(CCmdUI* pCmdUI);
	afx_msg void OnViewToolbarsDefault();
	afx_msg void OnUpdateViewToolbarsDefault(CCmdUI* pCmdUI);
	afx_msg void OnViewToolbarsTemplatetools();
	afx_msg void OnUpdateViewToolbarsTemplatetools(CCmdUI* pCmdUI);
	afx_msg void OnViewToolbarsVariantselection();
	afx_msg void OnUpdateViewToolbarsVariantselection(CCmdUI* pCmdUI);
	afx_msg void OnViewPreferences();
	afx_msg void OnToolsAnalyse();
	afx_msg LONG OnToolBarWndNotify(UINT wParam, LONG lParam);
	afx_msg void OnScaleChange();
	afx_msg void OnRotationChange();
	afx_msg void OnViewToolbarsPaint();
	afx_msg void OnUpdateViewToolbarsPaint(CCmdUI* pCmdUI);
	afx_msg void OnViewLayers();
	afx_msg void OnUpdateViewLayers(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCopyVariant(CCmdUI* pCmdUI);
	afx_msg void OnViewBuildbrush();
	afx_msg void OnUpdateViewBuildbrush(CCmdUI* pCmdUI);
	afx_msg void OnViewFullscreen();
	afx_msg void OnFloorMinus1();
	afx_msg void OnUpdateFloorMinus1(CCmdUI* pCmdUI);
	afx_msg void OnFloorMinus2();
	afx_msg void OnUpdateFloorMinus2(CCmdUI* pCmdUI);
	afx_msg void OnFloor4();
	afx_msg void OnUpdateFloor4(CCmdUI* pCmdUI);
	afx_msg void OnFloor3();
	afx_msg void OnUpdateFloor3(CCmdUI* pCmdUI);
	afx_msg void OnFloor2();
	afx_msg void OnUpdateFloor2(CCmdUI* pCmdUI);
	afx_msg void OnFloor1();
	afx_msg void OnUpdateFloor1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFloor0(CCmdUI* pCmdUI);
	afx_msg void OnFloor0();
	afx_msg void OnBrush();
	afx_msg void OnUpdateBrush(CCmdUI* pCmdUI);
	afx_msg void OnRotationid();
	afx_msg void OnUpdateRotationid(CCmdUI* pCmdUI);
	afx_msg void OnSelect();
	afx_msg void OnUpdateSelect(CCmdUI* pCmdUI);
	afx_msg void OnGeometry();
	afx_msg void OnUpdateGeometry(CCmdUI* pCmdUI);
	afx_msg void OnRectangularSelection();
	afx_msg void OnUpdateRectangularSelection(CCmdUI* pCmdUI);
	afx_msg void OnModeErase();
	afx_msg void OnUpdateModeErase(CCmdUI* pCmdUI);
	afx_msg void OnModeSelectMaterial();
	afx_msg void OnUpdateModeSelectMaterial(CCmdUI* pCmdUI);
	afx_msg void OnModeFill();
	afx_msg void OnUpdateModeFill(CCmdUI* pCmdUI);
	afx_msg void OnModeZoom();
	afx_msg void OnUpdateModeZoom(CCmdUI* pCmdUI);
	afx_msg void OnModePan();
	afx_msg void OnUpdateModePan(CCmdUI* pCmdUI);
	afx_msg void OnToolsCamera();
	afx_msg void OnExportAcks();
	afx_msg void OnXY();
	afx_msg void OnUpdateXY(CCmdUI* pCmdUI);
	afx_msg void OnZ();
	afx_msg void OnUpdateZ(CCmdUI* pCmdUI);
	afx_msg void OnPaintPen1();
	afx_msg void OnUpdatePaintPen1(CCmdUI* pCmdUI);
	afx_msg void OnPaintPen2();
	afx_msg void OnUpdatePaintPen2(CCmdUI* pCmdUI);
	afx_msg void OnPaintPen3();
	afx_msg void OnUpdatePaintPen3(CCmdUI* pCmdUI);
	afx_msg void OnViewGrid();
	afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);
	afx_msg void OnRunGame();
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
	afx_msg void OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized);
	afx_msg void OnUpdateSetGameAspectRatio(CCmdUI* pCmdUI);
	afx_msg void OnSetGameAspectRatio();
	afx_msg void OnEditStoreItems();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline CChildView* CMainFrame::GetTemplateView()
{
  return m_pRectView;
}

inline CGameView* CMainFrame::GetGameView()
{
  return m_pGameView;
}

inline CParamsView* CMainFrame::GetParamsView()
{
  return m_pParamsView;
}

inline CUIView* CMainFrame::GetUIView()
{
  return m_pUIView;
}

inline CGameFrame* CMainFrame::GetGameFrame()
{
  return m_pGameFrame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__09784638_643A_4FE0_9154_78EB417C5347__INCLUDED_)
