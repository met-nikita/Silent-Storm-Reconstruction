// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "MapEdit.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "GameFrm.h"
#include "IconView.h"
#include "preferences.h"
#include "AnalyseData.h"
#include "PaintBar.h"
#include "PropView.h"
#include "LayerList.h"
#include "ParamsFrm.h"
#include "UIFrame.h"
#include "ChapterFrame.h"
#include "ScenarioFrame.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "MEUserSettings.h"
#include "MEParams.h"
#include "UserSettingsSetup.h"
#include "..\Main\DiscretePos.h"
#include "..\Main\iMain.h"
#include "..\Input\Bind.h"
#include "DiplomacyFrame.h"
#include "StoreItemsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int REFRESH_TIMER_ID = 1;
const int REFRESH_INTERVAL = 120000;
const char REG_LAYOUT[]   = "Layout";
const char REG_FLAYOUT[]   = "FullscreenLayout";
const char REG_BARSLAYOUT[] = "Layout\\Controls";
const char REG_FBARSLAYOUT[] = "FullscreenLayout\\Controls";
const char REG_APPWIN[] = "AppWinLayout";
const char GRID_SEC[] = "Grid";

const int MAX_TREE_VIEWS = 2;

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, SECWorkbook)

const int wmAppToolBarWndNotify = RegisterWindowMessage(_T("WM_SECTOOLBARWNDNOTIFY"));

BEGIN_MESSAGE_MAP(CMainFrame, SECWorkbook)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_NEWTREEVIEW, OnNewTreeView)
  ON_COMMAND(ID_TOOLS_CUSTOMIZE, OnToolsCustomize)
	ON_COMMAND(ID_VIEW_PROPERTIES, OnViewProperties)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PROPERTIES, OnUpdateViewProperties)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_COMMAND(ID_VIEW_QUICKVIEW, OnViewQuickview)
	ON_UPDATE_COMMAND_UI(ID_VIEW_QUICKVIEW, OnUpdateViewQuickview)
	ON_UPDATE_COMMAND_UI(ID_NEWTREEVIEW, OnUpdateNewtreeView)
	ON_COMMAND(ID_VIEW_TOOLBARS_DEFAULT, OnViewToolbarsDefault)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBARS_DEFAULT, OnUpdateViewToolbarsDefault)
	ON_COMMAND(ID_VIEW_TOOLBARS_TEMPLATETOOLS, OnViewToolbarsTemplatetools)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBARS_TEMPLATETOOLS, OnUpdateViewToolbarsTemplatetools)
	ON_COMMAND(ID_VIEW_TOOLBARS_VARIANTSELECTION, OnViewToolbarsVariantselection)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBARS_VARIANTSELECTION, OnUpdateViewToolbarsVariantselection)
	ON_COMMAND(ID_VIEW_PREFERENCES, OnViewPreferences)
	ON_COMMAND(ID_TOOLS_ANALYSE, OnToolsAnalyse)
	ON_REGISTERED_MESSAGE(wmAppToolBarWndNotify, OnToolBarWndNotify)
	ON_CBN_CLOSEUP(IDC_COMBO_SCALE, OnScaleChange)
	ON_CBN_CLOSEUP(IDC_GEOMETRY_ROTATION, OnRotationChange)
	ON_COMMAND(ID_VIEW_TOOLBARS_PAINT, OnViewToolbarsPaint)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBARS_PAINT, OnUpdateViewToolbarsPaint)
	ON_COMMAND(ID_VIEW_LAYERS, OnViewLayers)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LAYERS, OnUpdateViewLayers)
	ON_UPDATE_COMMAND_UI(ID_COPYVARIANT, OnUpdateCopyVariant)
	ON_COMMAND(ID_VIEW_BUILDBRUSH, OnViewBuildbrush)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BUILDBRUSH, OnUpdateViewBuildbrush)
	ON_COMMAND(ID_VIEW_FULLSCREEN, OnViewFullscreen)
	ON_COMMAND(ID_FLOOR_MINUS1, OnFloorMinus1)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_MINUS1, OnUpdateFloorMinus1)
	ON_COMMAND(ID_FLOOR_MINUS2, OnFloorMinus2)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_MINUS2, OnUpdateFloorMinus2)
	ON_COMMAND(ID_FLOOR_4, OnFloor4)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_4, OnUpdateFloor4)
	ON_COMMAND(ID_FLOOR_3, OnFloor3)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_3, OnUpdateFloor3)
	ON_COMMAND(ID_FLOOR_2, OnFloor2)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_2, OnUpdateFloor2)
	ON_COMMAND(ID_FLOOR_1, OnFloor1)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_1, OnUpdateFloor1)
	ON_UPDATE_COMMAND_UI(ID_FLOOR_0, OnUpdateFloor0)
	ON_COMMAND(ID_FLOOR_0, OnFloor0)
	ON_COMMAND(ID_BRUSH, OnBrush)
	ON_UPDATE_COMMAND_UI(ID_BRUSH, OnUpdateBrush)
	ON_COMMAND(ID_ROTATIONID, OnRotationid)
	ON_UPDATE_COMMAND_UI(ID_ROTATIONID, OnUpdateRotationid)
	ON_COMMAND(ID_SELECT, OnSelect)
	ON_UPDATE_COMMAND_UI(ID_SELECT, OnUpdateSelect)
	ON_COMMAND(ID_GEOMETRY, OnGeometry)
	ON_UPDATE_COMMAND_UI(ID_GEOMETRY, OnUpdateGeometry)
	ON_COMMAND(ID_PAINT_SELECTION, OnRectangularSelection)
	ON_UPDATE_COMMAND_UI(ID_PAINT_SELECTION, OnUpdateRectangularSelection)
	ON_COMMAND(ID_PAINT_ERASE, OnModeErase)
	ON_UPDATE_COMMAND_UI(ID_PAINT_ERASE, OnUpdateModeErase)
	ON_COMMAND(ID_PAINT_SELECTMATERIAL, OnModeSelectMaterial)
	ON_UPDATE_COMMAND_UI(ID_PAINT_SELECTMATERIAL, OnUpdateModeSelectMaterial)
	ON_COMMAND(ID_PAINT_FILL, OnModeFill)
	ON_UPDATE_COMMAND_UI(ID_PAINT_FILL, OnUpdateModeFill)
	ON_COMMAND(ID_TEMPLATE_MODE_ZOOM, OnModeZoom)
	ON_UPDATE_COMMAND_UI(ID_TEMPLATE_MODE_ZOOM, OnUpdateModeZoom)
	ON_COMMAND(ID_TEMPLATE_MODE_PAN, OnModePan)
	ON_UPDATE_COMMAND_UI(ID_TEMPLATE_MODE_PAN, OnUpdateModePan)
	ON_COMMAND(ID_TOOLS_CAMERA, OnToolsCamera)
	ON_COMMAND(ID_FILE_EXPORTACKS, OnExportAcks)
	ON_COMMAND(ID_XY, OnXY)
	ON_UPDATE_COMMAND_UI(ID_XY, OnUpdateXY)
	ON_COMMAND(ID_Z, OnZ)
	ON_UPDATE_COMMAND_UI(ID_Z, OnUpdateZ)
	ON_COMMAND(ID_PAINT_PEN1, OnPaintPen1)
	ON_UPDATE_COMMAND_UI(ID_PAINT_PEN1, OnUpdatePaintPen1)
	ON_COMMAND(ID_PAINT_PEN2, OnPaintPen2)
	ON_UPDATE_COMMAND_UI(ID_PAINT_PEN2, OnUpdatePaintPen2)
	ON_COMMAND(ID_PAINT_PEN3, OnPaintPen3)
	ON_UPDATE_COMMAND_UI(ID_PAINT_PEN3, OnUpdatePaintPen3)
	ON_COMMAND(ID_VIEW_GRID, OnViewGrid)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GRID, OnUpdateViewGrid)
	ON_COMMAND(ID_RUNGAME, OnRunGame)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SETGAMEASPECTRATIO, OnUpdateSetGameAspectRatio)
	ON_COMMAND(ID_TOOLS_SETGAMEASPECTRATIO, OnSetGameAspectRatio)
	ON_WM_ACTIVATEAPP()
	ON_WM_ACTIVATE()
	ON_COMMAND(ID_TOOLS_STOREITEMS, OnEditStoreItems)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

BEGIN_BUTTON_MAP(templateBtnMap)
	COMBO_BUTTON(ID_TEMPLATE_SCALE, IDC_COMBO_SCALE, 0, CBS_DROPDOWN, 50, 50, 250 )
	COMBO_BUTTON(ID_ROTATIONID, IDC_GEOMETRY_ROTATION, 0, CBS_DROPDOWNLIST, 45, 45, 150 )
	TWOPART_BUTTON(ID_BRUSH, ID_DOWNARROW, TBBS_BUTTON, 0)
END_BUTTON_MAP()
							 
							 
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame() 
	: m_pRectView(0), m_pGameView(0), m_pModelView(0), m_FSView(this), pScenarioView(0),
		m_pIconView( new CIconView ), m_pPropView( new CPropView ), m_pUIView(0), pChapterView(0), iActiveVariantID(-1),
		pGlobalMapView(0), pDiplomacyView(0)
{
	m_pControlBarManager = new SECToolBarManager(this);	// this is a base class member
	// create a menu bar
	m_pMenuBar = new SECMDIMenuBar;	// this is a base class member
	// enable bitmap menu support.
	EnableBmpMenus();
	// dynamic toolbar button group array
	m_pDefButtonGroup = NULL;
	m_nDefButtonCount = 0;
  m_VarButtonCount = 0;
  m_pVarButtonGroup = 0;
  m_ToolButtonCount = 0;
  m_pToolButtonGroup = 0;
  m_UIButtonCount = 0;
  m_pUIButtonGroup = 0;
	m_EditmodeButtonCount = 0;
	m_pEditmodeButtonGroup = 0;
	m_PaintButtonCount = 0;
	m_pPaintButtonGroup = 0;
	
  memset( viewInds, 0, sizeof( viewInds ) );
	// TODO: add member initialization code here

  nVarBarID				= AFX_IDW_TOOLBAR + 5;
	nTemplateBarID	= AFX_IDW_TOOLBAR + 6;
	nUIBarID = AFX_IDW_TOOLBAR + 7;
	nEditmodeBarID = AFX_IDW_TOOLBAR + 8;
	nPaintBarID = AFX_IDW_TOOLBAR + 9;
  m_pVarBmpMgr = 0;
}

CMainFrame::~CMainFrame()
{
	if ( m_pControlBarManager )	delete m_pControlBarManager;
  if ( m_pVarButtonGroup )		delete [] m_pVarButtonGroup;
  if (m_pDefButtonGroup)			delete [] m_pDefButtonGroup;
  if ( m_pToolButtonGroup )		delete [] m_pToolButtonGroup;
	if ( m_pUIButtonGroup )			delete [] m_pUIButtonGroup;
	if ( m_pEditmodeButtonGroup )			delete [] m_pEditmodeButtonGroup;
	if ( m_pPaintButtonGroup )			delete [] m_pPaintButtonGroup;
	if ( m_pMenuBar )		delete m_pMenuBar;
	if ( m_pIconView )	delete m_pIconView;
	if ( m_pPropView )	delete m_pPropView;
  if ( m_pVarBmpMgr )
		m_pVarBmpMgr->Release();
  m_pVarBmpMgr = 0;
  m_pControlBarManager = 0;
  m_pVarButtonGroup = 0;
  m_pDefButtonGroup = 0;
	m_pToolButtonGroup = 0;
	m_pUIButtonGroup = 0;
	m_pEditmodeButtonGroup = 0;
	m_pPaintButtonGroup = 0;
  m_pMenuBar = 0;
	const_cast<CIconView*>( m_pIconView ) = 0;

  for( int i = 0; i < m_treeViews.size(); ++i )
    if ( m_treeViews[i] )
      delete m_treeViews[i];
  m_treeViews.clear();  
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (SECWorkbook::OnCreate(lpCreateStruct) == -1)
		return -1;
	// Load the master bitmap for ALL toolbars administrated by the
	// toolbar manager (and the large bitmap counterpart). All toolbars
	// (now and future) will be indices into this bitmap.

	// Todo: create a large button resource and pass the ID as the
	// second parameter if you want large icon capability
	SECToolBarManager* pToolBarMgr=(SECToolBarManager *)m_pControlBarManager;	

  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_MAINFRAME),
								MAKEINTRESOURCE(IDR_MAINFRAME)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_VARIANT_SEL),
    MAKEINTRESOURCE(IDR_VARIANT_SEL)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_TEMPLATE_BAR),
    MAKEINTRESOURCE(IDR_TEMPLATE_BAR)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_UICONTROLS_BAR),
    MAKEINTRESOURCE(IDR_UICONTROLS_BAR)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_EDITMODE_BAR),
    MAKEINTRESOURCE(IDR_EDITMODE_BAR)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_PAINTBAR),
    MAKEINTRESOURCE(IDR_PAINTBAR)));
  VERIFY(pToolBarMgr->LoadToolBarResource(MAKEINTRESOURCE(IDR_BITMAPS),
    MAKEINTRESOURCE(IDR_BITMAPS)));
	//
	pToolBarMgr->SetButtonMap( templateBtnMap );
	// establish the default toolbar groupings.
	// Note: m_pDefButtonGroup is allocated by the toolbar manager, 
	// and must be deleted in your destructor.
	pToolBarMgr->DefineDefaultToolBar(AFX_IDW_TOOLBAR + 4, "Default",
			IDR_MAINFRAME,
			m_nDefButtonCount,
			m_pDefButtonGroup,
			CBRS_ALIGN_ANY,AFX_IDW_DOCKBAR_TOP);
  pToolBarMgr->DefineDefaultToolBar( nTemplateBarID, "Template tools",
		IDR_TEMPLATE_BAR,
		m_ToolButtonCount,
		m_pToolButtonGroup,
		CBRS_ALIGN_ANY,AFX_IDW_DOCKBAR_TOP, AFX_IDW_TOOLBAR + 4 );
  pToolBarMgr->DefineDefaultToolBar( nPaintBarID, "Editor tools",
			IDR_PAINTBAR,
			m_PaintButtonCount,
			m_pPaintButtonGroup,
			CBRS_ALIGN_ANY, AFX_IDW_DOCKBAR_TOP, nTemplateBarID );
//  pToolBarMgr->DefineDefaultToolBar( nEditmodeBarID, "Editor tools",
//			IDR_EDITMODE_BAR,
//			m_EditmodeButtonCount,
//			m_pEditmodeButtonGroup,
//			CBRS_ALIGN_ANY, AFX_IDW_DOCKBAR_TOP, nPaintBarID );
  pToolBarMgr->DefineDefaultToolBar( nVarBarID, "Variant Selection",
			IDR_VARIANT_SEL,
			m_VarButtonCount,
			m_pVarButtonGroup,
			CBRS_ALIGN_ANY,AFX_IDW_DOCKBAR_TOP, nPaintBarID );
  pToolBarMgr->DefineDefaultToolBar( nUIBarID, "Controls",
			IDR_UICONTROLS_BAR,
			m_UIButtonCount,
			m_pUIButtonGroup,
			CBRS_ALIGN_ANY, AFX_IDW_DOCKBAR_RIGHT );
	
	pToolBarMgr->EnableCoolLook(TRUE);
	pToolBarMgr->SetMenuInfo( 1, IDR_MAINFRAME );
	// this is required when not using document/view
	//	LoadAdditionalMenus(1, IDR_TEDITTYPE);

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	EnableDocking(CBRS_ALIGN_ANY);
	// Call this to position the default toolbars as configured by
	// the DefineDefaultToolBar	commands above. Don't do this
	// if you are going immediately use LoadBarState/LoadState,
	// as these functions will call it anyway on nonexistant state info.
	pToolBarMgr->SetDefaultDockState();
	// Comment out if you don't want the application to start in
	// workbook mode.
	SetWorkbookMode(TRUE);
	//
	
//  HINSTANCE hInst = AfxGetResourceHandle();
//  m_hMDIAccel = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_TEDITTYPE));

	//  SwapMenu( IDR_TEDITTYPE );
  m_pVarToolBar = pToolBarMgr->ToolBarFromID( nVarBarID );
	m_pUIToolBar = pToolBarMgr->ToolBarFromID( nUIBarID );
  ASSERT( m_pVarToolBar && m_pUIToolBar );
  RemoveAllVariants();

	// create a new MDI child window
	// Создаем закладку в которой редактируются расстановки
	CMDIChildWnd* pChildWnd = CreateNewChild(RUNTIME_CLASS(CChildFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
	CChildFrame *pRectFrame = (CChildFrame*)pChildWnd;
  m_pRectView = &pRectFrame->m_wndView;
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
  SECWorksheet* pwsh = GetWorksheet( 0 );
  if ( pwsh )
  {
    pwsh->SetWindowText( "Grid View" );
    pwsh->SetTitle( "Grid View" );
  }
  viewInds[GRID_VIEW] = 0;

	// Dockable windows
	// Ресурсное окно (должно быть хотя бы одно)
  if ( !m_treeViews.empty() )
    DockControlBarEx( m_treeViews[0], AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.80f, 200);
  DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_RIGHT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
  DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE | CBRS_EX_STDCONTEXTMENU ;
	// Окно где отображается скриншут объекта
	UINT nID = SECControlBar::GetUniqueBarID(this, 100);
  if ( !m_pIconView->Create( this, _T("Quick View"), WS_CHILD|CBRS_RIGHT|CBRS_TOOLTIPS, 
		CBRS_EX_COOLBORDERS | CBRS_EX_GRIPPER | CBRS_EX_GRIPPER_CLOSE | CBRS_EX_BORDERSPACE | CBRS_EX_STDCONTEXTMENU, nID ) )
  {
    TRACE(_T("Failed to create docking window\n"));
    return -1;
  }
  m_pIconView->EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);
  DockControlBarEx(	m_pIconView, AFX_IDW_DOCKBAR_LEFT, 0, 0, 0.20f, 100 );
	//OnViewQuickview();
	// Окно редактирования св-св
  nID = SECControlBar::GetUniqueBarID(this, 100);
  if (!m_pPropView->Create(this, _T("Properties Window"), dwStyle, dwStyleEx, nID))
  {
    TRACE(_T("Failed to create docking window\n"));
    return -1;
  }  
  m_pPropView->EnableDocking(CBRS_ALIGN_ANY);
  DockControlBarEx(m_pPropView, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 1.00, 250);
	// Управление wysiwyg редактированием
	nID = SECControlBar::GetUniqueBarID(this, 100);
  if (!wysiwygBar.Create(this, _T("Building brushes"), dwStyle, dwStyleEx, nID))
  {
    TRACE(_T("Failed to create docking window\n"));
    return -1;
  }  
  wysiwygBar.EnableDocking(CBRS_ALIGN_ANY);
  DockControlBarEx( &wysiwygBar, AFX_IDW_DOCKBAR_RIGHT, 0, 0, 1.00, 250);
	// Закладка с 3d окошком
  pChildWnd = CreateNewChild(RUNTIME_CLASS(CGameFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
  m_pGameFrame = (CGameFrame*)pChildWnd;
  m_pGameView = &m_pGameFrame->m_wndView;
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
  pwsh = GetWorksheet( 1 );
  if ( pwsh )
  {
    pwsh->SetWindowText( "Game View" );
    pwsh->SetTitle( "Game View" );
  }
  viewInds[GAME_VIEW] = 1;
	// Закладка с параметрами вариантов универсального темплейта
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CParamsFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
  CParamsFrame *m_pParamsFrame = (CParamsFrame*)pChildWnd;
	m_pParamsView = &m_pParamsFrame->m_View;
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
  pwsh = GetWorksheet( 2 );
  if ( pwsh )
  {
    pwsh->SetWindowText( "Parameters" );
    pwsh->SetTitle( "Parameters" );
  }
  viewInds[PARAMS_VIEW] = 2;
	// Закладка с UI
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CUIFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
  CUIFrame *m_pUIFrame = (CUIFrame*)pChildWnd;
	m_pUIView = &m_pUIFrame->m_View;
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
  pwsh = GetWorksheet( 3 );
  if ( pwsh )
  {
    pwsh->SetWindowText( "UI" );
    pwsh->SetTitle( "UI" );
  }
  viewInds[UI_VIEW] = 3;
	// Закладка с ChapterView
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CChapterFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
  CChapterFrame *pChapterFrame = (CChapterFrame*)pChildWnd;
	pChapterView = &pChapterFrame->m_View;
  pChildWnd->MDIMaximize();
  pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
  pwsh = GetWorksheet( 4 );
  if ( pwsh )
  {
    pwsh->SetWindowText( "Chapter" );
    pwsh->SetTitle( "Chapter" );
  }
  viewInds[CHAPTER_VIEW] = 4;
	// Закладка с ScenarioView
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CScenarioFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
	CScenarioFrame *pScnearioFrame = (CScenarioFrame*)pChildWnd;
	pScenarioView = &pScnearioFrame->m_View;
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	pwsh = GetWorksheet( 5 );
	if ( pwsh )
	{
		pwsh->SetWindowText( "Scenario" );
		pwsh->SetTitle( "Scenario" );
	}
	viewInds[SCENARIO_VIEW] = 5;
	// Закладка с GlobalMapView
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CGlobalMapFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
	CGlobalMapFrame *pGlobalMapFrame = (CGlobalMapFrame*)pChildWnd;
	pGlobalMapView = &pGlobalMapFrame->m_View;
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	pwsh = GetWorksheet( 6 );
	if ( pwsh )
	{
		pwsh->SetWindowText( "GlobalMap" );
		pwsh->SetTitle( "GlobalMap" );
	}
	viewInds[GLOBALMAP_VIEW] = 6;
	// Закладка с Diplomacy View
	pChildWnd = CreateNewChild(RUNTIME_CLASS(CDiplomacyFrame), IDR_MAINFRAME, NULL, m_hMDIAccel);
	CDiplomacyFrame *pDiplomacyFrame = (CDiplomacyFrame*)pChildWnd;
	pDiplomacyView = &pDiplomacyFrame->m_View;
	pChildWnd->MDIMaximize();
	pChildWnd->ModifyStyle( WS_SYSMENU, 0 );
	pwsh = GetWorksheet( 7 );
	if ( pwsh )
	{
		pwsh->SetWindowText( "Diplomacy" );
		pwsh->SetTitle( "Diplomacy" );
	}
	viewInds[DIPLOMACY_VIEW] = 7;
	//
	MDIActivate( pRectFrame );
	// Установка таймера по которому обнавляется список ресурсов из базы данных	
  nRefreshTimer = SetTimer( REFRESH_TIMER_ID, REFRESH_INTERVAL, 0 );
  if ( 0 == nRefreshTimer )
    MessageBox( GetResString( IDS_ERR_SET_REFRESH ).c_str(), 0, MB_OK | MB_ICONWARNING );
	//
	FloatControlBar( m_pUIToolBar, CPoint( 300, 200 ), CBRS_ALIGN_RIGHT );
	ShowUIBar( false );
	//
	GetUserSettingsSetup().SetGridVisible( theApp.GetProfileInt( "", GRID_SEC, 1 ) );
	GetUserSettingsSetup().SetParam( ME_SEQUENCE_MODE, false );	
	//
	return 0;
}

CChapterView* CMainFrame::GetChapterView()
{
	return pChapterView;
}

CGlobalMapView* CMainFrame::GetGlobalMapView()
{
	return pGlobalMapView;
}

CDiplomacyView* CMainFrame::GetDiplomacyView()
{
	return pDiplomacyView;
}

CScenarioView* CMainFrame::GetScenarioView()
{
	return pScenarioView;
}

void CMainFrame::SetRectViewTitle( const std::string& szTitle )
{
  std::string str = "MapEdit - [ " + szTitle + " ]";

  SetWindowText( str.c_str() );
  SetTitle( str.c_str() );
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !SECWorkbook::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

void CMainFrame::OnToolsCustomize() 
{
	// customizable toolbar propery sheet:
	// Note, these classes are derived from CPropertySheet and CPropertyPage,
	// so it is quite easy to add your own custom pages as well 
	// (i.e. keyboard shortcut mappings, etc.).
	SECToolBarSheet    toolbarSheet;
	SECToolBarsPage    toolbarPage;
	toolbarPage.SetManager((SECToolBarManager*)m_pControlBarManager);
	toolbarSheet.AddPage(&toolbarPage);
	SECToolBarCmdPage  cmdPage(SECToolBarCmdPage::IDD, IDS_COMMANDS);
	cmdPage.SetManager((SECToolBarManager*)m_pControlBarManager);
	cmdPage.DefineBtnGroup(_T("default"), m_nDefButtonCount, m_pDefButtonGroup);
	cmdPage.DefineMenuGroup(_T("Menu"));	// menubar support
	toolbarSheet.AddPage(&cmdPage);
	toolbarSheet.DoModal();	
}

bool CMainFrame::IsCursorInGridView()
{
  if ( m_pRectView && m_pRectView->IsWindowVisible() )
    return m_pRectView->IsCursorInWindow();
  return false;
}

void CMainFrame::DropTemplate( int id )
{
  if ( m_pRectView )
    m_pRectView->DropTemplate( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::DropUnit( int id )
{
  if ( m_pRectView )
    m_pRectView->DropUnit( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::DropObject( int id )
{
  if ( m_pRectView )
    m_pRectView->DropObject( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::SetActive( EView view )
{
  SECWorksheet *pwsh = GetWorksheet( view );
  if ( !pwsh || !pwsh->IsWindowVisible() )
		return;
  MDIActivate( pwsh );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает true, если сохранен предыдущий активный view
bool CMainFrame::SetView( const vector<EView> &views )
{
	bool bRet = false;
  if ( GetSheetCount() == 0 )
    return bRet;

	ShowUIBar( false );
	CMDIChildWnd *pActive = MDIGetActive();
	CMDIChildWnd *pVisible = 0;
  int i;
  for ( i = 0; i < views.size(); ++i )
  {
		if ( UI_VIEW == views[i] )
			ShowUIBar( true );
    SECWorksheet *pwsh = GetWorksheet( viewInds[views[i]] );
    if ( !pwsh )
      continue;
		pVisible = pwsh;
		if ( pwsh->IsWindowVisible() )
			continue;
    pwsh->Invalidate( false );
    pwsh->ShowWindow( SW_SHOW );
    MDIActivate( pwsh );
  }
  for ( i=0; i < MAX_VIEW; ++i )
  {
    if ( find( views.begin(), views.end(), i ) == views.end() )
    {
      SECWorksheet *pwsh = GetWorksheet( viewInds[i] );
      if ( !pwsh )
        continue;
			if ( MDIGetActive() == pwsh && pVisible )
				MDIActivate( pVisible );
      pwsh->ShowWindow( SW_HIDE );
    }
  }
	if ( pActive && !pActive->IsWindowVisible() && pVisible )
		MDIActivate( pVisible );
	if ( pActive && MDIGetActive() == pActive )
		bRet = true;
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	SECWorkbook::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	SECWorkbook::Dump(dc);
}

#endif //_DEBUG

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

void CMainFrame::OnNewTreeView() 
{
  for ( int i = 0; i < m_treeViews.size(); ++i )
    if ( !m_treeViews[i]->IsWindowVisible() )
    {
      ShowControlBar( m_treeViews[i], true, false );
      return;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateNewtreeView(CCmdUI* pCmdUI) 
{
  for ( int i = 0; i < m_treeViews.size(); ++i )
    if ( !m_treeViews[i]->IsWindowVisible() )
    {
			pCmdUI->Enable();
      return;
    }
	pCmdUI->Enable( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::CreateTrees()
{
  for ( int i = 0; i < MAX_TREE_VIEWS; ++i )
  {
    string szRegSection = string( "ResView" ) + IToA( m_treeViews.size() + 1 );
    CTmplTreeWnd *pTreeWnd = new CTmplTreeWnd( szRegSection );
    
    DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_LEFT|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
    DWORD dwStyleEx = CBRS_EX_COOL | CBRS_EX_BORDERSPACE;
    UINT nID = SECControlBar::GetUniqueBarID( this, 100 );
    if (!pTreeWnd->Create(this, _T("\"ResourceView\" docking Window"), dwStyle, dwStyleEx, nID))
    {
      TRACE(_T("Failed to create docking window\n"));
      return;
    }
    pTreeWnd->SetWindowText( "ResourceView" );
    
    // Dock it
    pTreeWnd->EnableDocking(CBRS_ALIGN_ANY);
    DockControlBarEx(pTreeWnd, AFX_IDW_DOCKBAR_RIGHT, 0, 0, (float)1.00, 200);
    pTreeWnd->LoadLayout();
    ShowControlBar( pTreeWnd, false, false );
    m_treeViews.push_back( pTreeWnd );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
  if ( CN_UPDATE_COMMAND_UI == nCode )
  {
    CCmdUI* pCmdUI = (CCmdUI*)pExtra;
		
    if ( pCmdUI && ( (pCmdUI->m_nID >= ID_VARIANT_START && pCmdUI->m_nID < ID_VARIANT_END) 
			|| (pCmdUI->m_nID >= ID_FLOOR_START && pCmdUI->m_nID < ID_FLOOR_END)
			|| pCmdUI->m_nID == ID_TEMPLATE_SCALE ) )
    {
			bool bEnable = false;
			int nTree, nItem, nVarID;
			theApp.GetActiveItem( &nTree, &nItem, &nVarID );
			const SResTree *pTree = theApp.GetResTree( nTree );
			if ( pTree )
				bEnable = pTree->pItemsTree->IsUniTemplate();
      pCmdUI->Enable( bEnable );
      return true;
    }
  }
	if ( nCode != CN_UPDATE_COMMAND_UI && (nID >= ID_VARIANT_START && nID < ID_VARIANT_END) )
	{
		unordered_map<int, int>::const_iterator it = m_CmdID2VarID.find( nID );
		if ( it != m_CmdID2VarID.end() )
		{
			theApp.SetActiveVariant( it->second );
			return true;
		}
	}		
  return SECWorkbook::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление кнопки в тулбар выбора текущей расстановки
// Возвр. COMMAND ID, генерируемый кнопкой при нажатии
int CMainFrame::AddVariant( EPlacement type )
{
  int n = m_pVarToolBar->GetBtnCount();
  UINT nID = ID_VARIANT_START + n;
  UINT nBmp = IDB_GRIDVARIANT;

  switch ( type )
  {
  case PT_GRID:
    nBmp = IDB_GRIDVARIANT;
    break;
  case PT_FIN:
    nBmp = IDB_FINVARIANT;
    break;
  case PT_UNIT:
    nBmp = IDB_UNITVARIANT;
    break;
  }
  m_pVarBmpMgr->AddBitmapResource( nBmp, nBmp, &nID, 1 );
  m_pVarToolBar->AddButton( n,  nID );

  return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Удаление всех кнопок из тулбара выбора вариантов
const int STATIC_BUTT_NUM = 4;
void CMainFrame::RemoveAllVariants()
{
  if ( m_pVarBmpMgr )
    m_pVarBmpMgr->Release();
  const int n = m_pVarToolBar->GetBtnCount();
  
  // первые несколько кнопок у нас статические
  for ( int i = STATIC_BUTT_NUM; i < n; ++i )
  {
    m_pVarToolBar->RemoveButton( STATIC_BUTT_NUM );
  }
  m_pVarBmpMgr = new SECBmpMgr;
  m_pVarToolBar->SetBmpMgr( m_pVarBmpMgr );
	
  UINT nID = ID_NEW_VARIANT;
  m_pVarBmpMgr->AddBitmapResource( IDB_NEWVARIANT, IDB_NEWVARIANT, &nID, 1 );
  nID = ID_COPYVARIANT;
  m_pVarBmpMgr->AddBitmapResource( IDB_COPYVARIANT, IDB_COPYVARIANT, &nID, 1 );
  nID = ID_DELVARIANT;
  m_pVarBmpMgr->AddBitmapResource( IDB_DELVARIANT, IDB_DELVARIANT, &nID, 1 );
  nID = ID_VARIANT_START + 1;
  m_pVarBmpMgr->AddBitmapResource( IDB_NEWVARIANT, IDB_NEWVARIANT, &nID, 1 );
  m_pVarToolBar->AddButton( STATIC_BUTT_NUM, nID, true ); // сепаратор
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Установить активный вариант в тулбаре выбора вариантов
void CMainFrame::SetActiveVariant( int nVariantID )
{
  const int n = m_pVarToolBar->GetBtnCount();
	unordered_map<int, int>::const_iterator it = m_VarID2CmdID.find( nVariantID );
	if ( it == m_VarID2CmdID.end() )
		return;
	iActiveVariantID = nVariantID;

  for ( int i = STATIC_BUTT_NUM + 1; i < n; ++i )
		if ( it->second == m_pVarToolBar->GetItemID( i ) )
			m_pVarToolBar->SetButtonStyle( i, TBBS_CHECKED );
		else
			m_pVarToolBar->SetButtonStyle( i, TBBS_BUTTON );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewProperties() 
{
  ShowControlBar( m_pPropView, !m_pPropView->IsVisible(), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewProperties(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( m_pPropView->IsVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::UpdateTreeView( int nTreeID )
{
  for ( int i = 0; i < m_treeViews.size(); ++i )
    if ( m_treeViews[i]->IsWindowVisible() )
    {
      m_treeViews[i]->UpdateTree( nTreeID );
    }    
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnTimer(UINT nIDEvent) 
{
	MSG msg;
	// выкидываем из очереди сообщения таймера, мы не успеваем их обрабатывать
	while( PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE ) )
		;
	//
  if ( !theApp.bInputActive )
  {
    switch ( nIDEvent )
    {
    case REFRESH_TIMER_ID:
      for ( int i = 0; i < m_treeViews.size(); ++i )
        if ( m_treeViews[i]->IsWindowVisible() )
        {
          m_treeViews[i]->UpdateTree();
        }       
        break;
    }
  }
	//
	while( PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE ) )
		;
	SECWorkbook::OnTimer(nIDEvent);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnClose() 
{
  WINDOWPLACEMENT pl;
  CRect r;
  
	KillTimer( REFRESH_TIMER_ID );
  if ( !GetWindowPlacement( &pl ) )
    return;
  theApp.WriteProfileBinary( REG_LAYOUT, REG_APPWIN, (LPBYTE)&pl, sizeof(pl) );

  for ( int i = 0; i < m_treeViews.size(); ++i )
  {
    m_treeViews[i]->SaveLayout();
  }
	if ( !m_FSView.GetFSMode() )
		SaveBarState( REG_BARSLAYOUT );

	RestoreSysColors();
	// CRAP из-за Stigray\MFC bug
	//m_listControlBars.RemoveAll();

  SECWorkbook::OnClose();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::RestoreLayout()
{
//	DockControlBarEx( m_pIconView, AFX_IDW_DOCKBAR_LEFT, 2, 1 );
  CreateTrees();
  LoadBarState( REG_BARSLAYOUT );

  int nVisible = 0;
  for ( int i = 0; i < m_treeViews.size(); ++i )
  {
    if ( m_treeViews[i]->IsVisible() )
      ++nVisible;
  }
  if ( 0 == nVisible )
  {// не видно ни одного ресурсного дерева
    DockControlBarEx( m_treeViews[0], AFX_IDW_DOCKBAR_LEFT, 0, 0, (float)1.00, 200);
    ShowControlBar( m_treeViews[0], true, false );
  }

  WINDOWPLACEMENT *ppl;
  UINT l;

  if ( !theApp.GetProfileBinary( REG_LAYOUT, REG_APPWIN, (LPBYTE*)&ppl, &l ) 
        || l != sizeof(WINDOWPLACEMENT) )
  {
    CRect def;    
    GetDesktopWindow()->GetWindowRect( &def );
    
    def.left   += 20;
    def.top    += 20;
    def.right  -= 20;
    def.bottom -= 20;
    MoveWindow( &def );
    ShowWindow( SW_SHOW );
    return;
  }
  SetWindowPlacement( ppl );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewQuickview() 
{
  ShowControlBar( m_pIconView, !m_pIconView->IsVisible(), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewQuickview(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( m_pIconView->IsVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbarsDefault() 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( AFX_IDW_TOOLBAR + 4 );
	if ( !pBar )
		return;
	ShowControlBar( pBar, !pBar->IsWindowVisible(), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbarsDefault(CCmdUI* pCmdUI) 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( AFX_IDW_TOOLBAR + 4 );
	if ( !pBar )
		return;
	pCmdUI->SetCheck( pBar->IsWindowVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbarsTemplatetools() 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( nTemplateBarID );
	if ( !pBar )
		return;
	ShowControlBar( pBar, !pBar->IsWindowVisible(), false );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbarsTemplatetools(CCmdUI* pCmdUI) 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( nTemplateBarID );
	if ( !pBar )
		return;
	pCmdUI->SetCheck( pBar->IsWindowVisible() );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbarsVariantselection() 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( nVarBarID );
	if ( !pBar )
		return;
	ShowControlBar( pBar, !pBar->IsWindowVisible(), false );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbarsVariantselection(CCmdUI* pCmdUI) 
{
	SECCustomToolBar *pBar = ((SECToolBarManager *)m_pControlBarManager)->ToolBarFromID( nVarBarID );
	if ( !pBar )
		return;
	pCmdUI->SetCheck( pBar->IsWindowVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewToolbarsPaint() 
{
	if ( !GetTemplateView() )
		return;
	CPaintBar *pWnd = GetTemplateView()->GetPaintBar();
	if ( !pWnd )
		return;
	ShowControlBar( pWnd, !pWnd->IsWindowVisible(), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewToolbarsPaint(CCmdUI* pCmdUI) 
{
	if ( !GetTemplateView() )
		return;	
	CPaintBar *pWnd = GetTemplateView()->GetPaintBar();
	if ( !pWnd || !::IsWindow( pWnd->m_hWnd ) )
		return;
	pCmdUI->SetCheck( pWnd->IsWindowVisible() );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewPreferences() 
{
	CPropertySheet  sheet( "Preferences", this );
	sheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	CExportPrefsDlg exportpr;
	CLightPrefsDlg light;
	CTemplateViewPrefsDlg tview;
	CMapbuildPrefsDlg mbuild;
	CUILayersDlg ui;
	CRPGItemPrefs rpg;
	
	sheet.AddPage( &light );
	sheet.AddPage( &exportpr );
	//sheet.AddPage( &tview );
	sheet.AddPage( &mbuild );
	sheet.AddPage( &ui );
	sheet.AddPage( &rpg );

	if ( IDOK != sheet.DoModal() )
		return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnToolsAnalyse() 
{
	if ( !::IsWindow( analyseDlg.m_hWnd ) )
	{
		analyseDlg.Create( IDD_ANALYSE_DATA, this );
	}
	analyseDlg.ShowWindow( SW_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
LONG CMainFrame::OnToolBarWndNotify(UINT wParam, LONG lParam)
{
	HWND hWnd		 = HWND(lParam);
	UINT nNotifyCode = HIWORD(wParam);
	UINT nIDCtl		 = LOWORD(wParam);
	ASSERT(::IsWindow(hWnd));
	
	CWnd* pWnd = CWnd::FromHandle(hWnd);
	CComboBox* pCombo;
	
	switch(nIDCtl)
	{
	case IDC_COMBO_SCALE:
		ASSERT_KINDOF(CComboBox, pWnd);
		pCombo = (CComboBox*) pWnd;
		
		switch(nNotifyCode)
		{
		case SECWndBtn::WndInit:
			{
				//pCombo->SendMessage(WM_SETFONT, (WPARAM) m_hComboFont);
				
				pCombo->AddString( "10%" );
				pCombo->AddString( "20%" );
				pCombo->AddString( "50%" );
				pCombo->AddString( "100%" );
				pCombo->AddString( "150%" );
				pCombo->AddString( "200%" );
				pCombo->AddString( "300%" );
				pCombo->AddString( "700%" );
				if ( m_pRectView )
				{
					string str = IToA( m_pRectView->GetZoom() * 100 ) + '%';
					if ( CB_ERR == pCombo->SelectString( -1, str.c_str() ) )
					{
						int ind = pCombo->AddString( str.c_str() );
						pCombo->SetCurSel( ind );
					}
				}
			}
			break;
		case SECComboBtn::Entered:
			if(pCombo->GetWindowTextLength() > 0)
			{
				CString text;
				pCombo->GetWindowText( text );
				if ( text.GetAt( text.GetLength() - 1 ) == '%' )
					text.Delete( text.GetLength() - 1 );
				int num = atoi( text );
				if ( num > 0 )
					m_pRectView->SetZoom( (float)num / 100 );
			}
			break;
		}
		break;
	case IDC_GEOMETRY_ROTATION:
		ASSERT_KINDOF(CComboBox, pWnd);
		pCombo = (CComboBox*) pWnd;
		
		switch(nNotifyCode)
		{
		case SECWndBtn::WndInit:
			pCombo->AddString( "0" );
			pCombo->AddString( "90" );
			pCombo->AddString( "180" );
			pCombo->AddString( "270" );
			pCombo->SetCurSel( 0 );
			break;
		}
		break;
	}	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnScaleChange()
{
	SECToolBarManager* pToolBarMgr=(SECToolBarManager *)m_pControlBarManager;	
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID( nTemplateBarID );
	if ( !pToolBar )
		return;
	CWnd *pWnd = pToolBar->GetDlgItem( IDC_COMBO_SCALE );
	ASSERT_KINDOF(CComboBox, pWnd);
	CComboBox* pCombo = (CComboBox*)pWnd;	
	int iSel;
	//
	if( (iSel = pCombo->GetCurSel()) != CB_ERR )
	{
		CString text;
		pCombo->GetLBText( iSel, text );
		if ( text.GetAt( text.GetLength() - 1 ) == '%' )
			text.Delete( text.GetLength() - 1 );
		int num = atoi( text );
		if ( num > 0 )
			m_pRectView->SetZoom( (float)num / 100 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRotationChange()
{
	SECToolBarManager* pToolBarMgr=(SECToolBarManager *)m_pControlBarManager;	
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID( nPaintBarID );
	if ( !pToolBar )
		return;
	CWnd *pWnd = pToolBar->GetDlgItem( IDC_GEOMETRY_ROTATION );
	ASSERT_KINDOF(CComboBox, pWnd);
	CComboBox* pCombo = (CComboBox*)pWnd;	
	int iSel;
	//
	if( (iSel = pCombo->GetCurSel()) != CB_ERR )
	{
		CString text;
		pCombo->GetLBText( iSel, text );
		GetUserSettingsSetup().SetActiveRotationID( AngleToRotationID( atoi( text ) ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewLayers() 
{
	if ( !GetTemplateView() )
		return;
	CLayerList *pWnd = GetTemplateView()->GetLayers();
	if ( !pWnd )
		return;
	ShowControlBar( pWnd, !pWnd->IsWindowVisible(), false );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewLayers(CCmdUI* pCmdUI) 
{
	if ( !GetTemplateView() )
		return;
	CLayerList *pWnd = GetTemplateView()->GetLayers();
	if ( !pWnd )
		return;
	pCmdUI->SetCheck( pWnd->IsWindowVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Обновление тулбара выбора текущей расстановки
// если bSaveVarSelection = true, 
// то после обновления пытаемся активизировать предыдущий активный вариант
void CMainFrame::SetVariants( const vector<int> &variants, bool bSaveVarSelection )
{
	RemoveAllVariants();
	m_CmdID2VarID.clear();
	m_VarID2CmdID.clear();

	int nOldVarID = iActiveVariantID;
	bool bOldVarExist = false;
	for ( int i = 0; i < variants.size(); ++i )
	{
		if ( variants[i] == nOldVarID )
			bOldVarExist = true;
		int nCmd = AddVariant( PT_GRID );
		m_CmdID2VarID[nCmd] = variants[i];
		m_VarID2CmdID[variants[i]] = nCmd;
	}
	if ( bSaveVarSelection && bOldVarExist )
		theApp.SetActiveVariant( nOldVarID );
	else if ( !variants.empty() )
	{
		int nT, nI, nV;
		theApp.GetActiveItem( &nT, &nI, &nV );
		if ( nV > 0 )
			theApp.SetActiveVariant( nV );
		else
			theApp.SetActiveVariant( variants[0] );
	}
	else
		theApp.SetActiveVariant( -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateCopyVariant(CCmdUI* pCmdUI) 
{
	int nTree, nID, nVarID;

	theApp.GetActiveItem( &nTree, &nID, &nVarID );
	const SResTree *pRes = theApp.GetResTree( nTree );
	if ( !pRes || !pRes->pItemsTree->IsUniTemplate() )
		return;

	pCmdUI->Enable( pRes && pRes->pItemsTree->IsUniTemplate() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::ShowUIBar( bool bShow )
{
	ShowControlBar( m_pUIToolBar, bShow, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewBuildbrush() 
{
  ShowControlBar( &wysiwygBar, !wysiwygBar.IsVisible(), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewBuildbrush(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( wysiwygBar.IsVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewFullscreen() 
{
	// Save normal layout
	SaveBarState( REG_BARSLAYOUT );
	m_FSView.SetFSMode( SEC_FS_TEXTTOOLBAR|SEC_FS_DROPDOWNMENU, _T("Full Screen") );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFullScrennView::PreFullScreenMode()
{
	// Restore fullscreen layout
	pMainWnd->LoadBarState( REG_FBARSLAYOUT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFullScrennView::CloseDefFSToolBar()
{
	// Save fullscreen layout
	pMainWnd->SaveBarState( REG_FBARSLAYOUT );
	SECFullScreenView::CloseDefFSToolBar();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFullScrennView::PostFullScreenMode()
{
	// Restore normal layout
	pMainWnd->LoadBarState( REG_BARSLAYOUT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Выбор активного этажа
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloorMinus1() 
{
	theApp.SetActiveFloor( -1 );
}
void CMainFrame::OnUpdateFloorMinus1(CCmdUI* pCmdUI) 
{
	if ( -1 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloorMinus2() 
{
	theApp.SetActiveFloor( -2 );
}
void CMainFrame::OnUpdateFloorMinus2(CCmdUI* pCmdUI) 
{
	if ( -2 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloor4() 
{
	theApp.SetActiveFloor( 4 );
}
void CMainFrame::OnUpdateFloor4(CCmdUI* pCmdUI) 
{
	if ( 4 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloor3() 
{
	theApp.SetActiveFloor( 3 );
}
void CMainFrame::OnUpdateFloor3(CCmdUI* pCmdUI) 
{
	if ( 3 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloor2() 
{
	theApp.SetActiveFloor( 2 );
}
void CMainFrame::OnUpdateFloor2(CCmdUI* pCmdUI) 
{
	if ( 2 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnFloor1() 
{
	theApp.SetActiveFloor( 1 );
}
void CMainFrame::OnUpdateFloor1(CCmdUI* pCmdUI) 
{
	if ( 1 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateFloor0(CCmdUI* pCmdUI) 
{
	if ( 0 == theApp.GetActiveFloor() ) 
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );			
}
void CMainFrame::OnFloor0() 
{
	theApp.SetActiveFloor( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnSelect() 
{
	GetUserSettingsSetup().SetMode( EM_SELECT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnGeometry() 
{
	GetUserSettingsSetup().SetMode( EM_GEOMETRY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnBrush() 
{
	GetUserSettingsSetup().SetMode( EM_MATERIAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateBrush(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_MATERIAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateGeometry(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_GEOMETRY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateSelect(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_SELECT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRotationid() 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateRotationid(CCmdUI* pCmdUI) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRectangularSelection() 
{
	GetUserSettingsSetup().SetMode( EM_RECTANGULAR_SELECTION );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateRectangularSelection(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_RECTANGULAR_SELECTION );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnModeErase() 
{
	GetUserSettingsSetup().SetMode( EM_ERASE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateModeErase(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_ERASE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnModeSelectMaterial() 
{
	GetUserSettingsSetup().PushMode( EM_SELECTMATERIAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateModeSelectMaterial(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_SELECTMATERIAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnModeFill() 
{
	GetUserSettingsSetup().SetMode( EM_FILL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateModeFill(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_FILL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnModeZoom() 
{
	GetUserSettingsSetup().SetMode( EM_ZOOM );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateModeZoom(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_ZOOM );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnModePan() 
{
	GetUserSettingsSetup().SetMode( EM_PAN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateModePan(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetMode() == EM_PAN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnPaintPen1() 
{
	GetUserSettingsSetup().SetBrushSize( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdatePaintPen1(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetBrushSize() == 0 );			
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnPaintPen2() 
{
	GetUserSettingsSetup().SetBrushSize( 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdatePaintPen2(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetBrushSize() == 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnPaintPen3() 
{
	GetUserSettingsSetup().SetBrushSize( 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdatePaintPen3(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().GetBrushSize() == 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if ( WM_SYSKEYDOWN == pMsg->message || WM_SYSKEYUP == pMsg->message 
		|| (WM_KEYDOWN == pMsg->message && VK_MENU == pMsg->lParam ) )
	{
		if ( MDIGetActive() == m_pGameFrame )
		{
			m_pGameView->SetFocus();
			m_pGameFrame->Invalidate(FALSE);
		}
		return true;
	}
	return SECWorkbook::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnToolsCamera() 
{
	NInput::PostEvent( "camera_info" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnXY()
{
	GetUserSettingsSetup().SetMoveMode( MM_XY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateXY(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( GetUserSettings().GetMoveMode() == MM_XY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnZ()
{
	GetUserSettingsSetup().SetMoveMode( MM_Z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateZ(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( GetUserSettings().GetMoveMode() == MM_Z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnViewGrid()
{
	bool bGrid = !GetUserSettings().IsGridVisible();
	GetUserSettingsSetup().SetGridVisible( bGrid );
	GetTemplateView()->SetGridVisible( bGrid );
	theApp.WriteProfileInt( "", GRID_SEC, bGrid );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateViewGrid(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( GetUserSettings().IsGridVisible() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnRunGame()
{
	int nTree, nItem, nVar;
	theApp.GetActiveItem( &nTree, &nItem, &nVar );
	if ( IDC_TEMPLATE_TREE == nTree )
		GetGameView()->RunMap( nVar );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	if ( GetUserSettings().GetParam( ME_SWITCH_SYSCOLORS ) )
	{
		if ( bActive )
			SetupSysColors();
		else
			RestoreSysColors();
	}
	SECWorkbook::OnActivateApp( bActive, dwThreadID );
	NMainLoop::StepApp( true, CWnd::GetActiveWindow() == this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized)
{
	SECWorkbook::OnActivate( nState, pWndOther, bMinimized);
	NMainLoop::StepApp( true, CWnd::GetActiveWindow() == this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnUpdateSetGameAspectRatio(CCmdUI* pCmdUI)
{
	CWnd *pWnd = MDIGetActive();
	pCmdUI->Enable( GetGameFrame() == pWnd );
	bool bCheck = GetUserSettings().GetParam( ME_SEQUENCE_MODE );
	pCmdUI->SetCheck( bCheck ? 1 : 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnSetGameAspectRatio()
{
	WINDOWPLACEMENT wp;

	GetWindowPlacement( &wp );
	if ( wp.showCmd == SW_MAXIMIZE )
	{
		wp.flags &= ~WPF_RESTORETOMAXIMIZED;
		wp.showCmd = SW_RESTORE;
		SetWindowPlacement( &wp );
	}
	CRect r;
	m_pGameView->GetClientRect( &r );
	int nW = r.Width();
	int nH = nW * 768/1024;
	int nDeltaH = nH - r.Height();
	if ( abs( nDeltaH ) > 0 )
	{
		GetWindowRect( &r );
		SetWindowPos( 0, 0, 0, r.Width(), r.Height() + nDeltaH, SWP_NOMOVE | SWP_NOZORDER  );
	}
	//
	GetUserSettingsSetup().SetParam( ME_SEQUENCE_MODE, !GetUserSettings().GetParam( ME_SEQUENCE_MODE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnEditStoreItems()
{
	CStoreItemsDlg dlg;

	dlg.DoModal();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
