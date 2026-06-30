// TEdit.cpp : Defines the class behaviors for the application.
//
#include "stdafx.h"
#include "MapEdit.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "IconView.h"
#include "GameFrm.h"
#include "TemplMgr.h"
#include "templ.h"
#include "FinalElem.h"
#include "Unit.h"
#include "placement.h"
#include "FinTypeDlg.h"
#include "dbDefs.h"
#include "Export.h"
#include "PropView.h"
#include "ParamsView.h"
#include "UIView.h"
#include "ChapterView.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Input\Input.h"
#include "..\Main\iMain.h"
#include "iWysiwyg.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\Main\GResource.h"
#include "..\Main\Gfx.h"
#include "MEUserSettings.h"
#include "MEParams.h"
#include "history.h"
#include "..\Input\Bind.h"
#include "ScenarioView.h"
#include "GlobalMapView.h"
#include "DiplomacyView.h"
#include "..\Scintilla\Platform.h"
#include "..\Scintilla\Scintilla.h"

#include "TreeSelItemDlg.h"
#include "ItemsMgr.h"
#include "ChooseDBDlg.h"
#include "SharedEditing.h"
#include "ObjBrowserDescription.h"

const char MAP_EDIT_VERSION[] = "0.050";

const char REG_SEC[] = "Layout";
const char REG_TIP[] = "ShowTip";
const char TIP_FILE[] = "MapEdit.tip";
const char REG_VERSION[] = "Version";
const char TEMPLATE_DEPTH_SEC[] = "TemplateDepth";
externA5 bool gbAutoSelAnimModel;
externA5 bool gbLoad2DView;
externA5 string gszDBServer;
externA5 bool gbIterfacePreview;
bool gbLoadGameDB = true;

SDBConnection dbConnection;
//externA5 bool LoadResources();  // загрузка ресурсных деревьев из базы данных
externA5 void ReleaseResources();

class CPopInterfaceCmd: public NMainLoop::CInterfaceCommand
{
	OBJECT_NOCOPY_METHODS(CPopInterfaceCmd);
public:
	virtual void Exec()	
	{	
		PopInterface();	
		NMainLoop::StepApp( true, true ); 
		if ( theApp.GetGameView() )
			theApp.GetGameView()->SetFocus();
	}
};

//void DumpMemoryStats() {} // CRAP

	#define _ATL_APARTMENT_THREADED
#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
class CTEditModule : public CComModule
{
public:
	LONG Unlock();
	LONG Lock();
	LPCTSTR FindOneOf(LPCTSTR p1, LPCTSTR p2);
	DWORD dwThreadID;
};

#ifdef _ATL_STATIC_REGISTRY
#include <statreg.h>
#endif
//#include <atlimpl.cpp>

externA5 CTEditModule _Module;

#undef for
#include <atlcom.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp

BEGIN_MESSAGE_MAP(CTEditApp, CWinApp)
	//{{AFX_MSG_MAP(CTEditApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_SAVEALL, OnFileSaveall)
	ON_COMMAND(ID_VIEW_UPDGAME, OnViewUpdgame)
	ON_COMMAND(ID_NEW_VARIANT, OnNewVariant)
	ON_UPDATE_COMMAND_UI(ID_NEW_VARIANT, OnUpdateNewVariant)
	ON_COMMAND(ID_DELVARIANT, OnDelvariant)
	ON_UPDATE_COMMAND_UI(ID_DELVARIANT, OnUpdateDelvariant)
	ON_COMMAND(ID_RELOAD_TREE, OnReloadTree)
	ON_COMMAND(ID_VIEW_SHADOWS, OnViewShadows)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHADOWS, OnUpdateViewShadows)
	ON_COMMAND(ID_HELP_TIPOFTHEDAY, OnHelpTipoftheday)
	ON_COMMAND(ID_GOBACK, OnGoback)
	ON_COMMAND(ID_GOFORWARD, OnGoforward)
	ON_UPDATE_COMMAND_UI(ID_GOBACK, OnUpdateGoback)
	ON_UPDATE_COMMAND_UI(ID_GOFORWARD, OnUpdateGoforward)
	ON_COMMAND(ID_VIEW_CAMERARESET, OnViewCamerareset)
	ON_COMMAND(ID_VIEW_REALTIMEPREVIEW, OnViewRealtimePreview)
	ON_UPDATE_COMMAND_UI(ID_VIEW_REALTIMEPREVIEW, OnUpdateViewRealtimePreview)
	ON_COMMAND(ID_MAKE_CURRENT, OnMakeCurrent)
	ON_COMMAND(ID_VIEW_ROLL, OnViewRoll)
	ON_COMMAND(ID_COPYVARIANT, OnCopyVariant)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp construction

CTEditApp::CTEditApp() : pMainFrame(0)
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	bInputActive  = false;
	bGameActive   = false;
  bShadows      = false;
	bCameraReset  = true;
	bRealtimePreview = false;
	nActiveFloor  = 0;
	nMaxTreeDepth = 1;
}

CTEditApp::~CTEditApp()
{
  resTreesHash.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The one and only CTEditApp object

CTEditApp theApp;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHistEvent
{
	int nTableID;
	int nItemID;
	SHistEvent() {};
	SHistEvent( int _nTableID, int _nItemID ) : nTableID( _nTableID ), nItemID( _nItemID ) {}
	boolean operator== ( const SHistEvent &op ) { return op.nTableID == nTableID && op.nItemID == nItemID; }
};
static CHistory<SHistEvent> history;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp initialization

void DeleteRegistryKeys()
{
	const string str = "Software\\Nival Interactive\\A5 MapEdit\\";
	SHDeleteKey( HKEY_CURRENT_USER, (str + "Layout").c_str() );
	//SHDeleteKey( HKEY_CURRENT_USER, (str + "ResView1").c_str() );
	//SHDeleteKey( HKEY_CURRENT_USER, (str + "ResView2").c_str() );
	//SHDeleteKey( HKEY_CURRENT_USER, (str + "ResView3").c_str() );
	//SHDeleteKey( HKEY_CURRENT_USER, (str + "ResView4").c_str() );
}
void CTEditApp::CheckMapEditVerison()
{
	CString szVersion = GetProfileString( "", REG_VERSION );

	if ( 0 != szVersion.Compare( MAP_EDIT_VERSION ) )
	{
		DeleteRegistryKeys();
		WriteProfileString( "", REG_VERSION, MAP_EDIT_VERSION );
	}
}

bool Step( bool bActive )
{
	bool bRet = NMainLoop::StepApp( bActive, CWnd::GetActiveWindow() == theApp.m_pMainWnd );
	if ( !bRet )
		NMainLoop::Command( new CPopInterfaceCmd );
	if ( theApp.GetGameView() )
		theApp.GetGameFrame()->DrawFrame();
	return bRet;
}

bool ErrExit( SECSplashWnd *pSplashWnd, const char *pszErr )
{
  if ( ::IsWindow( pSplashWnd->m_hWnd ) )
    pSplashWnd->Dismiss();
  AfxMessageBox( pszErr, MB_OK | MB_ICONSTOP );
  return false;
}

BOOL CTEditApp::InitInstance()
{
	puts( "InitInstance" );
  SetRegistryKey( _T( "Nival Interactive" ) );
	CheckMapEditVerison();
	if ( (string)m_lpCmdLine == "-clean" )
		DeleteRegistryKeys();
	//if ( (string)m_lpCmdLine == "-novs" )
	//	bNoVS = true;
	if ( (string)m_lpCmdLine == "-nogamedb" )
		gbLoadGameDB = false;
	SetTemplateMaxDepth( GetProfileInt( "", TEMPLATE_DEPTH_SEC, 0 ) );
	
	HDC hDC = ::GetDC( 0 );
	int nCBits = GetDeviceCaps( hDC, BITSPIXEL );
	if ( nCBits < 32 )
	{
		MessageBox( 0, "Need 32 bit color depth mode set on desktop", "Error", MB_OK );
		return FALSE;
	}
		
#ifdef _DEBUG
  int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	tmpFlag &= ~_CRTDBG_ALLOC_MEM_DF;
  tmpFlag |= _CRTDBG_LEAK_CHECK_DF;// | _CRTDBG_CHECK_ALWAYS_DF;
  _CrtSetDbgFlag( tmpFlag );
//  _CrtSetBreakAlloc( 730 );
#endif // _DEBUG

	OpenMESocket();
	RWSetDotNetStyle(TRUE);

	BOOL bAfxInit = AfxOleInit();
	AfxEnableControlContainer();
	bAfxInit = AfxInitRichEdit2();
	if (!InitATL())
		return FALSE;
	puts( "InitATL" );
	

	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if (cmdInfo.m_bRunEmbedded || cmdInfo.m_bRunAutomated)
	{
		return TRUE;
	}


  SECSplashWnd *pSplashWnd = new SECSplashWnd( IDB_SPLASH );
  pSplashWnd->Create();
	puts( "SECSplashWnd" );
	
	const char szLoadErr[] = "Cant load data\nЌе найдена игрова€ база данных";
	while ( S_OK != OpenDBConnection( &dbConnection ) )
	{
		ErrExit( pSplashWnd, szLoadErr );
		CChooseDBDlg dlg;
		dlg.m_szDBServer = gszDBServer.c_str();
		if ( IDCANCEL == dlg.DoModal() )
			return FALSE;
		WriteProfileString( "", "DB", dlg.m_szDBServer );
	}


	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.
/*
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
*/
	// To create the main window, this code creates a new frame window
	// object and then sets it as the application's main window object.

  pMainFrame = new CMainFrame;
	CMDIFrameWnd* pFrame = pMainFrame;
	m_pMainWnd = pFrame;
/*
	// create main MDI frame window
	if (!pFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	puts( "LoadFrame(IDR_MAINFRAME)" );
*/	
	// try to load shared MDI menus and accelerator table
	//TODO: add additional member variables and load calls for
	//	additional menu types your application may need. 

  
  HINSTANCE hInst = AfxGetResourceHandle();
	m_hMDIMenu  = ::LoadMenu(hInst, MAKEINTRESOURCE(IDR_TEDITTYPE));
	m_hMDIAccel = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_TEDITTYPE));

	Scintilla_RegisterClasses( hInst );

	// setting working directory
	string szWD = GetExportDstDir();
	if ( !SetCurrentDirectory( szWD.c_str() ) )
	{
		//ErrExit( pSplashWnd, "Cant set working directory" );
		//return false;
	}

  if ( !LoadResources() || !theTemplMgr.Create() )
	{
		ErrExit( pSplashWnd, szLoadErr );
		return FALSE;
	}
	CreateBrowsers();
	puts( "LoadResources()" );

	// create main MDI frame window
	if (!pFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	puts( "LoadFrame(IDR_MAINFRAME)" );

	// The main window has been initialized, so show and update it.
  pMainFrame->RestoreLayout();
	pMainFrame->SetView( vector<EView>() );
  if ( !pMainFrame->IsWindowVisible() )
    pMainFrame->ShowWindow( SW_SHOWNORMAL );
	pFrame->UpdateWindow();
	puts( "pMainFrame->ShowWindow" );

	if ( ::IsWindow( pSplashWnd->m_hWnd ) )
		pSplashWnd->Dismiss();
  //  delete pSplashWnd;
	puts( "pSplashWnd->Dismiss()" );	

	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = 17;
	strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
		
	m_TipFont.CreateFontIndirect( &lf );
  bool bShowTipAtStartup = (bool)GetProfileInt( REG_SEC, REG_TIP, 1 );
	if ( bShowTipAtStartup )
	{
		SECTipOfDay tipsDlg( TIP_FILE, 0 );
		tipsDlg.SetTipFont( &m_TipFont );
    tipsDlg.DoModal();
		WriteProfileInt( REG_SEC, REG_TIP, tipsDlg.GetShowAtStartup() );
	}
  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp message handlers

int CTEditApp::ExitInstance() 
{
	if ( activeItem.nTableID == IDC_TEMPLATE_TREE )
		ClosePlacement( activeItem.nVariantID );
	CloseMESocket();
  ReleaseResources();
//  OnFileSaveall();
  //TODO: handle additional resources you may have added
	if (m_hMDIMenu != NULL)
		FreeResource(m_hMDIMenu);
	if (m_hMDIAccel != NULL)
		FreeResource(m_hMDIAccel);

	if (m_bATLInited)
	{
		_Module.RevokeClassObjects();
		_Module.Term();
	
		CoUninitialize();
	}

	Scintilla_ReleaseResources();
	return CWinApp::ExitInstance();
}

inline bool IsUniTemplate( int nResTree )
{
	const SResTree *pTree = theApp.GetResTree( nResTree );

	if ( !pTree )
		return false;
	return pTree->pItemsTree->IsUniTemplate();
}

void CTEditApp::OnFileNew() 
{
	CMainFrame* pFrame = STATIC_DOWNCAST(CMainFrame, m_pMainWnd);

	// create a new MDI child window
	CMDIChildWnd* pChildWnd = pFrame->CreateNewChild(
		RUNTIME_CLASS(CChildFrame), IDR_TEDITTYPE, NULL, m_hMDIAccel);


}

CChildView* CTEditApp::GetTemplateView()
{
  if ( pMainFrame )
    return pMainFrame->GetTemplateView();
  return 0;
}

CGameView* CTEditApp::GetGameView()
{
  if ( pMainFrame )
    return pMainFrame->GetGameView();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CTEditApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTEditApp message handlers


#include <initguid.h>
//#include "MapEdit_i.c"
	
CTEditModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()

LONG CTEditModule::Unlock()
{
	AfxOleUnlockApp();
	return 0;
}

LONG CTEditModule::Lock()
{
	AfxOleLockApp();
	return 1;
}
LPCTSTR CTEditModule::FindOneOf(LPCTSTR p1, LPCTSTR p2)
{
	while (*p1 != NULL)
	{
		LPCTSTR p = p2;
		while (*p != NULL)
		{
			if (*p1 == *p)
				return CharNext(p1);
			p = CharNext(p);
		}
		p1++;
	}
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTEditApp::InitATL()
{
	m_bATLInited = TRUE;

	HRESULT hRes = CoInitialize(NULL);
	/*
#if _WIN32_WINNT >= 0x0400
	HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
	HRESULT hRes = CoInitialize(NULL);
#endif
*/
	if (FAILED(hRes))
	{
		m_bATLInited = FALSE;
		return FALSE;
	}

	_Module.Init(ObjectMap, AfxGetInstanceHandle());
	_Module.dwThreadID = GetCurrentThreadId();

	LPTSTR lpCmdLine = GetCommandLine(); //this line necessary for _ATL_MIN_CRT
	TCHAR szTokens[] = _T("-/");

	BOOL bRun = TRUE;
	LPCTSTR lpszToken = _Module.FindOneOf(lpCmdLine, szTokens);
	while (lpszToken != NULL)
	{
		if (lstrcmpi(lpszToken, _T("UnregServer"))==0)
		{
			_Module.UpdateRegistryFromResource(IDR_TEDIT, FALSE);
			_Module.UnregisterServer(TRUE); //TRUE means typelib is unreg'd
			bRun = FALSE;
			break;
		}
		if (lstrcmpi(lpszToken, _T("RegServer"))==0)
		{
			_Module.UpdateRegistryFromResource(IDR_TEDIT, TRUE);
			_Module.RegisterServer(TRUE);
			bRun = FALSE;
			break;
		}
		lpszToken = _Module.FindOneOf(lpszToken, szTokens);
	}

	if (!bRun)
	{
		m_bATLInited = FALSE;
		_Module.Term();
		CoUninitialize();
		return FALSE;
	}

	hRes = _Module.RegisterClassObjects(CLSCTX_LOCAL_SERVER, 
		REGCLS_MULTIPLEUSE);
	if (FAILED(hRes))
	{
		m_bATLInited = FALSE;
		CoUninitialize();
		return FALSE;
	}	

	return TRUE;

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnFileSaveall() 
{
  BeginWaitCursor();
	theTemplMgr.UpdateModified();
	/*
  // ѕри сохранении могут помен€тьс€ некоторые ID, 
  // поэтому необходимо обновить некоторые окна
  CChildView *pView = GetTemplateView();
  if ( pView )
  {
    CPlacement *pPl = GetActivePlacement();
    if ( pPl )
      pView->SetPlacement( pPl );
  }
	*/
  // «апись данных из ресурсных деревьев
  for ( unordered_map<int, SResTree>::const_iterator i = resTreesHash.begin(); i != resTreesHash.end(); ++i )
  {
    if ( i->second.pItemsTree )
			i->second.pItemsTree->UpdateModified();
  }
  EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTEditApp::IsCursorInGridView() const
{
  if ( pMainFrame )
    return pMainFrame->IsCursorInGridView();
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::DropTemplate( int id )
{
  if ( pMainFrame )
    pMainFrame->DropTemplate( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ѕоместить на зону какой либо объект
bool CTEditApp::DropItem( int nTableID, int nItemID )
{
	CPoint pt;
	bool bPropUpd;
	if ( GetCursorPos( &pt ) )
	{
		CRect r;
		pMainFrame->m_pPropView->GetWindowRect( &r );
		bPropUpd = PtInRect( &r, pt );
	}
	return DropItem( activeItem.nTableID, activeItem.pProps, bPropUpd ? &pMainFrame->m_pPropView->m_OIDlg : 0, nTableID, nItemID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// „тобы определить, можно ли бросить итем nItemID из таблицы nTableID 
// в текущий активный итем, используем св€зи между таблицами
bool CTEditApp::DropItem( int nDstTableID, const CPropMap *pDstProps, COIDlg *pView, int nTableID, int nItemID )
{
  const SResTree *pDropRes = GetResTree( nTableID );
  if ( !pDstProps || !pDropRes /*|| pDstProps->empty()*/ )
    return false;
	const int nDstItemID = pDstProps->begin()->second->GetOwnerID().nItemID;
	// ќбработка исключений в правилах drag&drop
	if ( CheckDropExeptions( nDstTableID, nTableID ) )
	{
		int nPropID = GetDropExeptionTarget( nTableID, nDstItemID, nTableID, nItemID );
		if ( -1 != nPropID && pView )
			pView->SetActiveProp( nPropID );
	}
  //  огда в темплейт кидаетс€ моделька, или темплейт, обрабатываем это по особому
  if ( IDC_TEMPLATE_TREE == nDstTableID && !pView )
	{
    switch ( nTableID )
    {
    case IDC_RPG_PERS_TREE:
			pMainFrame->DropUnit( nItemID );
      OnFileSaveall();
      break;
    case IDC_OBJECTS_TREE:
      pMainFrame->DropObject( nItemID );
      OnFileSaveall();
      break;
    case IDC_TEMPLATE_TREE:
      DropTemplate( nItemID );
      OnFileSaveall();
      break;
    case IDC_CONTAINERS_TREE:
      GetTemplateView()->DropContainer( nItemID );
      OnFileSaveall();
      break;
		}
		return true;
	}
	if ( !pView )
		return false; // это не изменение св-ва, а добавление объекта на карту
  // ѕросматриваем все св-ва активного объекта и ищем св€зь с таблицей nTableID
  for ( CPropMap::const_iterator it = pDstProps->begin(); it != pDstProps->end(); ++it )
    if ( it->second->IsRelatedTable( nTableID ) )
    {
      int nActiveProp = pView->GetActiveProp( it->second->GetGroup() );
      if ( nActiveProp != it->second->GetID() && -1 != nActiveProp )
        continue;
			if ( EMPTY_VALUE == nItemID )
				// устанавливаем тип пол€ в VT_NULL, в базе даннных будет пустое значение
				it->second->SetValue( CVariant() );
			else
				it->second->SetValue( nItemID );
			// обновл€ем внешний вид поселекченного итема, если он изменилс€ (есть исключени€)
			if ( IDC_TEMPLATE_TREE != nDstTableID && activeItem.nTableID == nDstTableID && activeItem.nItemID == nDstItemID )
			{
				const SResTree *pActiveRes = GetResTree( nDstTableID );
				ASSERT( pActiveRes );
				(*pActiveRes->pSelectCb)( activeItem.nItemID, pMainFrame->GetActiveVariant() );
			}
      // обновл€ем значени€ в окошке св-в
      pView->UpdateProperty( it->second->GetID() );
      OnFileSaveall();
      return true;
    }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬озвращает активную расстановку, если есть
// указатель на расстановку временный и не должен сохран€тьс€
CPlacement* CTEditApp::GetActivePlacement() const
{
	if ( IDC_TEMPLATE_TREE != activeItem.nTableID )
		return 0;
	return activeItem.pPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetTemplate( int id, int nVarID )
{
  CTemplate *pTempl = theTemplMgr.GetTempl( id );
  if ( pTempl )
  {
    //
    char buf[64];
    _snprintf( buf, sizeof(buf) - 1, "%s: %d x %d", pTempl->GetName(), pTempl->GetWidth(), pTempl->GetHeight() );
    buf[sizeof(buf) - 1] = 0;
    SetRectViewTitle( buf );
    //
		SetPlacement( nVarID );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetPlacement( int id )
{
  if ( !pMainFrame )
    return;
  vector<EView> views;
  const CPropMap *pProps = 0;
  const SResTree *pRes = GetResTree( activeItem.nTableID );
  
	views.push_back( PARAMS_VIEW );
	if ( gbLoad2DView )
		views.push_back( GRID_VIEW );
	views.push_back( GAME_VIEW );
//	GetGameFrame()->SetCurPlacement( id );
  if ( pRes )
    pProps = pRes->pItemsTree->GetPropList( activeItem.nItemID, activeItem.nVariantID );
	BeginWaitCursor();
	ClearPlacementCache( &placementCache );
	GetTemplateView()->SaveData();
  CPlacement *pPl = theTemplMgr.GetPlacement( id );
	if ( gbLoad2DView )
		CollectPlacementTree( pPl, &placementCache, 0 );
	CPlacement *pOldPlacement = activeItem.pPlacement;
	activeItem.pPlacement = pPl;
  if ( GetTemplateView() )
  {
    GetTemplateView()->SetPlacement( pPl, &placementCache );
  }
	GetGameFrame()->SetCurPlacement( id );
//	NMainLoop::StepApp( true );
	EndWaitCursor();
  if ( !pMainFrame->SetView( views ) && gbLoad2DView )
		pMainFrame->SetActive( GRID_VIEW );
  if ( pProps )
  {
    if ( pRes )
      pRes->pItemsTree->ReleasePropList( activeItem.pProps );
    SetPropMap( pProps );
  }
	if ( pOldPlacement )
		theTemplMgr.ReleasePlacement( pOldPlacement );
  OnFileSaveall();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPropMap* CTEditApp::SetPropMap( const CPropMap *pProps )
{
	pMainFrame->m_pPropView->SetPropMap( activeItem.nTableID, pProps );
	const CPropMap *pOld = activeItem.pProps;
	activeItem.pProps = pProps;
	return pOld;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetModel( int id, int nVarID )
{
  if ( GetGameView() )
    GetGameView()->SetModel( -1, nVarID );
  vector<EView> views;
	
	views.push_back( GAME_VIEW );
	views.push_back( PARAMS_VIEW );
  pMainFrame->SetView( views );
	pMainFrame->SetActive( GAME_VIEW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetAIModel( int id, int nVarID )
{
  if ( GetGameView() )
    GetGameView()->SetAIModel( id );
  vector<EView> views;
	
	views.push_back( GAME_VIEW );
  pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetGeometry( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
 
  if ( GetGameView() )
    GetGameView()->SetGeometry( id );
  pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetMaterial( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
  
  if ( GetGameView() )
    GetGameView()->SetMaterial( nVarID );
	views.push_back( PARAMS_VIEW );
  pMainFrame->SetView( views );
	pMainFrame->SetActive( GAME_VIEW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetTexture( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
  
  if ( GetGameView() )
  {
    GetGameView()->SetTexture( id );
    pMainFrame->SetView( views );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetParticle( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	views.push_back( PARAMS_VIEW );
  
  if ( GetGameView() )
  {
    GetGameView()->SetParticle( nVarID );
    pMainFrame->SetView( views );
		pMainFrame->SetActive( GAME_VIEW );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetAmbientLight( int id, int nVar )
{
	vector<EView> views( 1, PARAMS_VIEW );
	pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetRndSound( int id, int nVar )
{
	vector<EView> views( 1, PARAMS_VIEW );
	pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetSoundEffect( int id, int nVar )
{
	vector<EView> views( 1, GAME_VIEW );

	if ( GetGameView() )
	{
		GetGameView()->SetSoundEffect( id );
		pMainFrame->SetView( views );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetDiplomacy( int id, int nVar )
{
	vector<EView> views( 1, DIPLOMACY_VIEW );

	pMainFrame->GetDiplomacyView()->SetActiveDiplomacy( id );
	pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetContainer( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
  
  if ( GetGameView() )
  {
    GetGameView()->SetContainer( id );
    pMainFrame->SetView( views );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetObject( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	views.push_back( PARAMS_VIEW );
	const SResTree *pTree = GetResTree( IDC_OBJECTS_TREE );
  
  if ( GetGameView() && pTree )
  {
		GetGameView()->SetObject( id, nVarID );
		pMainFrame->SetView( views );
		pMainFrame->SetActive( GAME_VIEW );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetSound( int id, int nVarID )
{
	vector<EView> views;
  if ( GetGameView() )
  {
    GetGameView()->SetSound( id );
    pMainFrame->SetView( views );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetUI( int id, int nVarID )
{
  vector<EView> views( 1, UI_VIEW );
	if ( gbIterfacePreview )
		views.push_back( GAME_VIEW );
	
  pMainFrame->SetView( views );
	pMainFrame->SetActive( UI_VIEW );
	CUIView *pV = pMainFrame->GetUIView();
	if ( pV )
		pV->SetUIContainer( id, activeItem.pProps );
	if ( gbIterfacePreview )
		GetGameView()->SetInterface( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetHead( int id, int nVarID )
{
	vector<EView> views( 1, GAME_VIEW );

  GetGameView()->SetHead( id );
  pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetScenario( int id, int nVarID )
{
	vector<EView> views( 1, SCENARIO_VIEW );

	pMainFrame->GetScenarioView()->SetScenario( id );
	pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetConstructionPart( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	views.push_back( PARAMS_VIEW );
  
  if ( GetGameView() )
  {
    GetGameView()->SetConstructionPart( nVarID );
    pMainFrame->SetView( views );
		pMainFrame->SetActive( GAME_VIEW );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetModelID( CItemsMgr *pMgr, int nID )
{
	int nModel = -1;
	
	if ( !pMgr )
		return -1;
	const CPropMap *props = pMgr->GetPropList( nID );
	if ( props )
	{
		CPropMap::const_iterator it = props->find( "ModelID" );
		if ( it != props->end() )
			nModel = it->second->GetValue();
		pMgr->ReleasePropList( props );
		const SResTree *pTree = theApp.GetResTree( IDC_MODELS_TREE );
		vector<int> vars;
		if ( !pTree || !pTree->pItemsTree->GetItemVariants( nModel, &vars ) || vars.empty() )
			return -1;
		return vars[0];
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetWeapon( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	int nModel = -1;
	CVariant var;
  
  const SResTree *pRes = GetResTree( IDC_RPG_WEAPONS_TREE );
	const SResTree *pItemsRes = GetResTree( IDC_RPG_ITEMS_TREE );
	if ( !pRes || !pItemsRes )
		return;
	const CPropMap *pProps = pRes->pItemsTree->GetPropList( id, nVarID );
	if ( pProps )
	{
		CPropMap::const_iterator iIt = pProps->find( "ItemID" );
		if ( iIt != pProps->end() )
		{
			nModel = GetModelID( pItemsRes->pItemsTree, iIt->second->GetValue() );
			if ( GetGameView() )
			{
				GetGameView()->SetModel( -1, nModel );
				pMainFrame->SetView( views );
			}
		}
		pRes->pItemsTree->ReleasePropList( pProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetPers( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
  
  if ( GetGameView() )
  {
    GetGameView()->SetPers( id );
    pMainFrame->SetView( views );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetChapter( int id, int nVarID )
{
  vector<EView> views( 1, CHAPTER_VIEW );
//	views.push_back( GAME_VIEW );
	
  pMainFrame->SetView( views );
	CChapterView *pV = pMainFrame->GetChapterView();
	if ( pV )
		pV->SetChapter( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetGlobalMap( int id, int nVarID )
{
  vector<EView> views( 1, GLOBALMAP_VIEW );
	
  pMainFrame->SetView( views );
	CGlobalMapView *pV = pMainFrame->GetGlobalMapView();
	if ( pV )
		pV->SetGlobalMap( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetRPGItem( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	int nModel = -1;
	CVariant var;
  
  if ( GetGameView() )
  {
    GetGameView()->SetRPGItem( id );
    pMainFrame->SetView( views );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetAnimation( int id, int nVarID )
{
  vector<EView> views( 1, GAME_VIEW );
	int nModel = -1;
	CVariant var;
  
  const SResTree *pRes = GetResTree( IDC_ANIMATIONS_TREE );
	const SResTree *pModelRes = GetResTree( IDC_MODELS_TREE );
  if ( pRes && pRes->pItemsTree && pModelRes && pModelRes->pItemsTree )
  {
		const CPropMap *pAnimProps = pRes->pItemsTree->GetPropList( id );
		if ( pAnimProps )
		{
			CPropMap::const_iterator it = pAnimProps->find( "SkeletonID" );
			if ( it != pAnimProps->end() )
			{
				var = it->second->GetValue();
				if ( gbAutoSelAnimModel )
					nModel = pModelRes->pItemsTree->GetItemByProp( "SkeletonID", var );
				else
				{
					int n = GetUserSettings().GetParam( ME_DEF_ANIM_MODEL );
					pModelRes->pTreeDlg->SetSelectedItemID( IDC_MODELS_TREE, n );
					if ( IDOK == pModelRes->pTreeDlg->DoModal() )
					{
						int nTree, nSelID;
						pModelRes->pTreeDlg->GetSelectedItemID( &nTree, &nSelID );
						if ( nSelID != -1 )
						{
							const CPropMap *props = pModelRes->pItemsTree->GetPropList( nSelID );
							if ( props )
							{
								CPropMap::const_iterator it = props->find( "SkeletonID" );
								if ( it != props->end() && (int)it->second->GetValue() == (int)var )
									nModel = nSelID;
								else
								{
									var = CVariant();
									MessageBox( pMainFrame->m_hWnd, "Bad SkeletonID", "Error", MB_OK | MB_ICONWARNING );
								}
								pRes->pItemsTree->ReleasePropList( props );
							}
						}
					}
				}
			}
			pRes->pItemsTree->ReleasePropList( pAnimProps );
		}
	}
	if ( var.GetType() != CVariant::VT_NULL && -1 == nModel )
	{
		MessageBox( pMainFrame->m_hWnd, GetResString( IDS_ERR_ANIMATION ).c_str(), 0, MB_OK | MB_ICONWARNING );
	}
	if ( GetGameView() )
	{
		GetGameView()->SetAnimation( nModel, id );
		pMainFrame->SetView( views );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetActiveBrushModel( int nTreeID, int brushID, int nVarID )
{ 
  vector<EView> views;

  const SResTree *pRes = GetResTree( nTreeID );
  if ( pRes )
  {  
    const CPropMap *props = pRes->pItemsTree->GetPropList( brushID );
    if ( props )
    {
      CPropMap::const_iterator it = props->find( "ModelID" );
      if ( it != props->end() )
      {
				vector<EView> views( 1, GAME_VIEW );
				
				if ( GetGameView() )
					GetGameView()->SetRndModel( it->second->GetValue() );
				pMainFrame->SetView( views );
        return;
      }
    }
  }
  pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetEmptyView()
{
  vector<EView> views;  
  pMainFrame->SetView( views );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetRectViewTitle( const std::string& szTitle )
{
  if ( pMainFrame )
    pMainFrame->SetRectViewTitle( szTitle );
}

CGameFrame* CTEditApp::GetGameFrame()
{
  if ( pMainFrame )
    return pMainFrame->GetGameFrame();
  return 0;
}

BOOL CTEditApp::OnIdle(LONG lCount) 
{
  CWinApp::OnIdle(lCount);
	//
	if ( !GetGameView() || !gbLoadGameDB )
		return FALSE;
  const int RENDER_DELAY = 1;
  static int cDelay = 0;
	//
	CWnd *pWnd = pMainFrame->MDIGetActive();
	//bool bForeground = pMainFrame->GetForegroundWindow()->m_hWnd == pMainFrame->m_hWnd;
	//bool bFocus = CWnd::GetFocus() == pMainFrame->GetGameView();
	bool bSteep = IsRealtimePreview() ?  pMainFrame->GetGameFrame() == pWnd /*&& bForeground*/ : bInputActive;
	NInput::PumpMessages( bInputActive );
  if ( cDelay++ == RENDER_DELAY )
  {
    cDelay = 0;
//    if ( !NMainLoop::StepApp( bGameActive ) )
    if ( !Step( bSteep ) )
    {
      bInputActive = false;
    }
  }
	return TRUE;
}

void CTEditApp::UpdateGameDB()
{
  USES_CONVERSION;

  OnFileSaveall();
  const char *pSrc = W2A( GetDBProvider() );
  NDatabase::SetSource( pSrc );
	if ( !gbLoadGameDB )
		return;
  // read from database
	puts( "start: NDatabase::Import()" );
	NMainLoop::Command( 0 );
	ClearHoldQueue();
  NDatabase::Import();
  NDb::BuildMapLinks();
	puts( "end: NDatabase::Import()" );
/*
  const SResTree *pRes = GetResTree( activeItem.nTableID );
  if ( pRes )
  {
    pRes->pSelectCb( activeItem.nItemID, pMainFrame->GetActiveVariant() );
  }
*/
	Step( true );
}

void CTEditApp::OnViewUpdgame() 
{
  BeginWaitCursor();
	GetTemplateView()->SaveData();
  UpdateGameDB();

	if ( IDC_TEMPLATE_TREE == activeItem.nTableID )
	{
		CPlacement *pPl = activeItem.pPlacement;
		if ( pPl && GetGameView() )
		{
			vector<EView> views;
			if ( gbLoad2DView )
				views.push_back( GRID_VIEW );
			views.push_back( GAME_VIEW );
			views.push_back( PARAMS_VIEW );
			pMainFrame->SetView( views );
			pMainFrame->SetActive( GAME_VIEW );
			
			//GetGameFrame()->SetCurPlacement( pPl->GetID() );
			NInput::PostEvent( "update" );
			Step( true );
		}
	}
	else
		SetActiveItem( activeItem.nTableID, activeItem.nItemID );
//  CopyLastVersion();
	Step( false );
  EndWaitCursor();
}

const SResTree* CTEditApp::GetResTree( int nTreeID )
{
	unordered_map<int, SResTree>::const_iterator i = resTreesHash.find( nTreeID );
	if ( i == resTreesHash.end() )
		return 0;
	//
	ASSERT( i->second.pTreeDlg );
	if ( i->second.pItemsTree && !i->second.pItemsTree->IsLoaded() )
	{
		BeginWaitCursor();
		BOOL bRet = i->second.pItemsTree->Load();
		EndWaitCursor();
		ASSERT( bRet );
	}
  return &i->second;
}

void CTEditApp::SetActiveItem( int nResourceTree, int nItemID, int nVariantID, bool bUpdateHistory )
{
	// обработка forward/back листа
	if ( bUpdateHistory && nResourceTree != -1 && nItemID != -1 )
	{
		SHistEvent ev( nResourceTree, nItemID );
		SHistEvent lastEv;
		
		if ( activeItem.nTableID != nResourceTree || activeItem.nItemID != nItemID )
			history.Push( ev );
	}
	//
	OnFileSaveall();
  const SResTree *pRes = GetResTree( nResourceTree );
  if ( !pRes )
    return;
	
  const SResTree *pOldTree = GetResTree( activeItem.nTableID );
  if ( pOldTree )
    pOldTree->pItemsTree->ReleasePropList( activeItem.pProps );
	if ( activeItem.nTableID == IDC_TEMPLATE_TREE )
		ClosePlacement( activeItem.nVariantID );
	activeItem.nTableID = nResourceTree;
	activeItem.nItemID  = nItemID;
	activeItem.nVariantID = nVariantID;
	activeItem.pProps   = 0;
	pMainFrame->m_pPropView->SetPropMap( -1, 0 );
	
	if ( pRes->pItemsTree->IsUniTemplate() )
	{
		vector<int> vars;
		pRes->pItemsTree->GetItemVariants( nItemID, &vars );
		pMainFrame->SetVariants( vars, activeItem.nTableID == nResourceTree );
	}
	else
	{
		activeItem.pProps   = pRes->pItemsTree->GetPropList( nItemID, nVariantID );
		pMainFrame->m_pPropView->SetPropMap( activeItem.nTableID, activeItem.pProps );
		if ( pRes->pSelectCb )
		{
			// pSelectCb должна вызыватьс€ после установки activeItem.pProps
			(*pRes->pSelectCb)( nItemID, -1 );
		}
		else
			SetEmptyView();		
	}
	BeginWaitCursor();
	Step( true );
	while( NGScene::HasFileRequestsInFly() )
		Sleep( 2 );
	Step( true );
	EndWaitCursor();
	pMainFrame->GetParamsView()->SetActiveItem( nResourceTree, nItemID );
	bCameraReset = false;
	//
	if ( activeItem.nTableID == IDC_TEMPLATE_TREE )
		OpenPlacement( activeItem.nVariantID );
}

bool CTEditApp::IsEditingTemplate() const 
{ 
  return IsUniTemplate( activeItem.nTableID );
}

void CTEditApp::OnNewVariant() 
{
	const SResTree *pRes = GetResTree( activeItem.nTableID );
	if ( !pRes || !pRes->pItemsTree->IsUniTemplate() )
		return;
	int nVarID = pRes->pItemsTree->AddVariant( activeItem.nItemID );
	if ( -1 == nVarID )
		return;
	if ( activeItem.nTableID == IDC_TEMPLATE_TREE )
	{
		OnFileSaveall();
		Sleep(0);
		UpdateGameDB();
	}
	vector<int> vars;
	pRes->pItemsTree->GetItemVariants( activeItem.nItemID, &vars );
	pMainFrame->SetVariants( vars );
	SetActiveVariant( nVarID );
}

void CTEditApp::OnUpdateNewVariant(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable( IsUniTemplate( activeItem.nTableID ) );
}

void CTEditApp::OnDelvariant() 
{
	const SResTree *pTree = GetResTree( activeItem.nTableID );
	if ( !pTree || !pTree->pItemsTree->IsUniTemplate() )
		return;
	vector<int> vars;
	if ( !pTree->pItemsTree->GetItemVariants( activeItem.nItemID, &vars ) || vars.empty() )
		return;
	int ret = MessageBox( pMainFrame->m_hWnd, 
		GetResString( IDS_CONFIRM_DELVARIANT ).c_str(), 
		GetResString( AFX_IDS_APP_TITLE ).c_str(), 
		MB_YESNO );
	if ( IDYES != ret )
		return;
  BeginWaitCursor();
	pTree->pItemsTree->DeleteVariant( activeItem.nItemID, pMainFrame->GetActiveVariant() );
	if ( pTree->pItemsTree->GetItemVariants( activeItem.nItemID, &vars ) )
		pMainFrame->SetVariants( vars );

  EndWaitCursor();
}

void CTEditApp::OnUpdateDelvariant(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable( IsUniTemplate( activeItem.nTableID ) );  
}

string CTEditApp::GetResSrcDir() const
{
  return GetExportSrcDir();
}

void CTEditApp::OnReloadTree() 
{
	const SResTree *pTree = GetResTree( activeItem.nTableID );

  if ( pTree && pTree->pItemsTree )
  {
    DWORD t = GetTickCount();
    for ( int i = 0; i < 100; ++i )
    {
      pMainFrame->UpdateTreeView( pTree->nTreeID );
    }
    t = GetTickCount() - t;
    string time = string( "time = " ) + IToA( t ) + "\n";
    OutputDebugString( time.c_str() );
  }
}

void CTEditApp::OnViewShadows() 
{
	bShadows = !bShadows;
//  SetActiveItem( activeItem.nTableID, activeItem.nItemID );
}

void CTEditApp::OnUpdateViewShadows(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( HasShadows() );
}

void CTEditApp::OnHelpTipoftheday() 
{
	SECTipOfDay tipsDlg( TIP_FILE, 0 );
	tipsDlg.SetTipFont( &m_TipFont );
  tipsDlg.DoModal();
	WriteProfileInt( REG_SEC, REG_TIP, tipsDlg.GetShowAtStartup() );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ѕроверка на исключение в правилах drag&drop
bool CTEditApp::CheckDropExeptions( int nTblTarget, int nTblDrop )
{
	switch ( nTblTarget )
	{
		case IDC_MATERIALS_TREE:
			return IDC_TEXTURES_TREE == nTblDrop;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает ID св-ва в таблице nTblTarget куда можно бросить итем из таблицы nTblDrop
int CTEditApp::GetDropExeptionTarget( int nTblTarget, int nItemTarget, int nTblDrop, int nItemDrop )
{
	if ( IDC_MATERIALS_TREE == nTblTarget && IDC_TEXTURES_TREE == nTblDrop )
	{
		if ( EMPTY_VALUE == nItemDrop )
			return nItemTarget; // fake текстуру можно бросать в любое поле
		const SResTree *pDropTree = GetResTree( nTblDrop );
		const SResTree *pTargTree = GetResTree( nTblTarget );
		if ( !pDropTree || !pTargTree )
			return -1;
		const CPropMap *pDropProps = pDropTree->pItemsTree->GetPropList( nItemDrop );
		const CPropMap *pTargProps = pTargTree->pItemsTree->GetPropList( nItemTarget );
		if ( !pDropProps || !pTargProps )
			return -1;
		CPropMap::const_iterator it = pDropProps->find( "Format" );
		if ( pDropProps->end() == it )
			return -1;
		CPropMap::const_iterator itTarg = string(it->second->GetValue()) == "normals" ?
			pTargProps->find( "BumpID" ) : pTargProps->find( "TextureID" );
		if ( pTargProps->end() == itTarg )
			return -1;
		int nID = itTarg->second->GetID();
		pDropTree->pItemsTree->ReleasePropList( pDropProps );
		pDropTree->pItemsTree->ReleasePropList( pTargProps );
		return nID;
	}

	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SelChanged( int nTreeID, int nItemID )
{
	const SResTree *pRes = GetResTree( nTreeID );
	if ( !pRes )
		return;
	pMainFrame->m_pIconView->SetObject( pRes, nItemID );	
	/*
	if ( IDC_WALLS_TREE == nTreeID || IDC_SOLIDMODELS_TREE == nTreeID || IDC_FLOORS_TREE == nTreeID )
	{
		const CPropMap *pProps = pRes->pItemsTree->GetPropList( nItemID );
		if ( !pProps )
			return;
		CPropMap::const_iterator iL = pProps->find( "Length" );
		CPropMap::const_iterator iW = pProps->find( "Width" );
		CPropMap::const_iterator iC = pProps->find( "UserColor" );
		if ( pProps->end() == iL || pProps->end() == iW || pProps->end() == iC )
			return;
		float fLength = iL->second->GetValue();
		float fWidth  = iW->second->GetValue();
		int cr = iC->second->GetValue();
		pRes->pItemsTree->ReleasePropList( pProps );
		return;
	} 
	if ( IDC_CONSTRUCTIONPARTS_TREE == nTreeID )
	{
		const CPropMap *pProps = pRes->pItemsTree->GetPropList( nItemID );
		if ( !pProps )
			return;
		CPropMap::const_iterator iX = pProps->find( "SizeX" );
		CPropMap::const_iterator iY = pProps->find( "SizeY" );
		CPropMap::const_iterator iW = pProps->find( "Thickness" );
		CPropMap::const_iterator iC = pProps->find( "UserColor" );
		if ( pProps->end() == iL || pProps->end() == iW || pProps->end() == iC )
			return;
		float fSizeX = iL->second->GetValue();
		float fSizeY = iW->second->GetValue();
		int cr = iC->second->GetValue();
		pRes->pItemsTree->ReleasePropList( pProps );
		return;
	} 
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetActiveFloor( int nFloorID )
{
	nActiveFloor = nFloorID;
	if ( IDC_TEMPLATE_TREE == activeItem.nTableID )
	{
		NInput::PostEvent( "active_floor_changed" );
		//SetActiveItem( activeItem.nTableID, activeItem.nItemID );
		//GetTemplateView()->SetPlacement( activeItem.pPlacement, &placementCache );
		GetTemplateView()->ActiveFloorChanged();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnGoback() 
{
	if ( history.CanBackward() )
	{
		SHistEvent ev = history.Back();
		SetActiveItem( ev.nTableID, ev.nItemID, false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnGoforward() 
{
	if ( history.CanForward() )
	{
		SHistEvent ev = history.Forward();
		SetActiveItem( ev.nTableID, ev.nItemID, false );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnUpdateGoback(CCmdUI* pCmdUI) 
{
	if ( history.CanBackward() )
		pCmdUI->Enable();
	else
		pCmdUI->Enable( FALSE );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnUpdateGoforward(CCmdUI* pCmdUI) 
{
	if ( history.CanForward() )
		pCmdUI->Enable();
	else
		pCmdUI->Enable( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnViewCamerareset() 
{
	bCameraReset = true;
	NInput::PostEvent( "camera_reset" );
	NMainLoop::StepApp( true, true );
	//NMainLoop::StepApp( true );
	//OnViewRoll();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnViewRealtimePreview() 
{
	bRealtimePreview = !bRealtimePreview;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnUpdateViewRealtimePreview(CCmdUI* pCmdUI) 
{
	if ( IsRealtimePreview() )
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnMakeCurrent() 
{
	GetTemplateView()->SaveData();
	NGScene::CloseAllResources();
	CopyLastVersion();
	OnViewUpdgame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPlacement* GetFirstPlacement( int nTemplateID, CPlacementCache *pCache )
{
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	if ( !pTree )
		return 0;
	vector<int> vars;
	if ( !pTree->pItemsTree->GetItemVariants( nTemplateID, &vars ) )
		return false;
	
	if ( !vars.empty() )
	{
		int id = vars[0];
		if ( !(*pCache)[id] )
			return theTemplMgr.GetPlacement( id );
		else
			return (*pCache)[id];
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::CollectPlacementTree( CPlacement *pRoot, CPlacementCache *pCache, int nDepth )
{
	ASSERT( pCache );
	if ( nDepth == nMaxTreeDepth || !pRoot )
		return;
	pRoot->MoveFirst( GetActiveFloor() );
	while ( pRoot->MoveNext() )
	{
		CPlacement *pTmpPl = GetFirstPlacement( pRoot->GetRectTemplID(), pCache );
		if ( pTmpPl )
			(*pCache)[pTmpPl->GetID()] = pTmpPl;
		CollectPlacementTree( pTmpPl, pCache, nDepth + 1 );
	}		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::ClearPlacementCache( CPlacementCache *pCache )
{
	for ( CPlacementCache::iterator it = pCache->begin(); it != pCache->end(); ++it )
		if ( it->second )
			theTemplMgr.ReleasePlacement( it->second );
		pCache->clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetTemplateMaxDepth( int nDepth )
{ 
	nMaxTreeDepth = nDepth;
	WriteProfileInt( "", TEMPLATE_DEPTH_SEC, nDepth );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::SetActiveVariant( int nVariantID )
{
	const SResTree *pRes = GetResTree( activeItem.nTableID );
  if ( !pRes )
    return;
	
  const CPropMap *props = pRes->pItemsTree->GetPropList( activeItem.nItemID, nVariantID );
	activeItem.nVariantID = nVariantID;
	
  pRes->pItemsTree->ReleasePropList( activeItem.pProps );
  activeItem.pProps = props;
	
	pMainFrame->SetActiveVariant( nVariantID );
	pMainFrame->m_pPropView->SetPropMap( activeItem.nTableID, props );
	
	BeginWaitCursor();
  if ( pRes->pSelectCb )
    (*pRes->pSelectCb)( activeItem.nItemID, nVariantID );
	else
		SetEmptyView();
	//
	Step( true );
	EndWaitCursor();
	bCameraReset = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 void UpdateMapDB();
void CTEditApp::OnViewRoll() 
{
	if ( IDC_TEMPLATE_TREE == activeItem.nTableID && GetGameView() )
	{
		CPlacement *pPl = activeItem.pPlacement;
		if ( pPl )
		{
			BeginWaitCursor();
			GetTemplateView()->SaveData();
			UpdateMapDB();
			//GetGameFrame()->SetCurPlacement( pPl->GetID() );
			NInput::PostEvent( "update" );
			Step( true );
			Step( true );
			bCameraReset = false;
			EndWaitCursor();
		}
	}
	else
		SetActiveItem( activeItem.nTableID, activeItem.nItemID, false );	
	//
	pMainFrame->SetActive( GAME_VIEW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 void CopyVariantProperties( CItemsMgr *pItemsMgr, int nDstID, int nDstVarID, int nSrcID, int nSrcVarID );
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTEditApp::OnCopyVariant() 
{
	const SResTree *pRes = GetResTree( activeItem.nTableID );
	if ( !pRes || !pRes->pItemsTree->IsUniTemplate() )
		return;
	BeginWaitCursor();
	int nVarID = pRes->pItemsTree->AddVariant( activeItem.nItemID );
	if ( -1 == nVarID )
	{
		EndWaitCursor();
		return;
	}
	CopyVariantProperties( pRes->pItemsTree, activeItem.nItemID, nVarID, activeItem.nItemID, activeItem.nVariantID );
	if ( IDC_TEMPLATE_TREE == activeItem.nTableID )
	{
		CPlacement *pNew = theTemplMgr.GetPlacement( nVarID );
		if ( pNew && activeItem.pPlacement )
			pNew->CopyFrom( *activeItem.pPlacement );
		theTemplMgr.ReleasePlacement( pNew );
	}
	vector<int> vars;
	pRes->pItemsTree->GetItemVariants( activeItem.nItemID, &vars );
	pMainFrame->SetVariants( vars );
	Sleep(0);
	if ( IDC_TEMPLATE_TREE == activeItem.nTableID )
		UpdateMapDB();
	SetActiveVariant( nVarID );	
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTEditApp::PreTranslateMessage( MSG* pMsg )
{
	if ( WM_KEYDOWN == pMsg->message && VK_MENU == pMsg->lParam )
	{
		if ( pMainFrame->MDIGetActive() == pMainFrame->GetGameFrame() )
		{
			pMainFrame->GetGameView()->SetFocus();
			pMainFrame->GetGameFrame()->Invalidate(FALSE);
		}
	}
	return CWinApp::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const unordered_map<int, SResTree>& CTEditApp::GetResTrees() const
{
	return resTreesHash;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
