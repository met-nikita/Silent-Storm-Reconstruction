// GameView.cpp : implementation of the CGameView class
//

#include "stdafx.h"

#include "MapEdit.h"
#include "GameView.h"
#include "TemplMgr.h"
#include "Placement.h"

#include "preferences.h"
#include "..\Misc\Geom.h"
#include "..\Misc\StrProc.h"
#include "..\ADOImport\BasicDB.h"
#include "..\Input\Bind.h"
#include "..\Input\Input.h"
#include "..\Main\Gfx.h"
#include "..\Main\iMain.h"
#include "..\Main\iMapEditor.h"
#include "iWysiwyg.h"
#include "..\Main\GResource.h"
#include "..\Main\Grid.h"
#include "..\Main\Sound.h"
#include "..\Main\wInterface.h"
#include "..\Main\iMission.h"
#include "..\Main\RPGGlobal.h"
#include "MEUserSettings.h"
#include "MEParams.h"
#include "..\Main\GInit.h"
#include "dbDefs.h"
#include "TreeSelItemDlg.h"
#include "UserSettingsSetup.h"
#include "WaypointDlg.h"
#include "NameAndFolder.h"
#include "ItemsMgr.h"
#include "MESerialize.h"
#include "..\Main\MELayers.h"
#include "..\DBFormat\DataMap.h"
#include "FinDBCmd.h"
#include "AnimItemsDlg.h"
#include "RPGItemsContainerDlg.h"
#include "..\MiscDll\Commands.h"
#include "WaypointDB.h"
#include "TextEditor.h"
#include "AnimFlagsDlg.h"
#include "ObjBrowserDescription.h"
#include "ObjBrowserContainer.h"
#include "ScriptEditDlg.h"

SLightPrefs gLightPrefs;
string gszBuildParams;
float gfMayaCameraSpeed;
externA5 bool bDXTModeOn;
externA5 bool bWYSIWYGActive;
bool gbAutoSelAnimModel = true;
bool gbAutoSelAnimItems = true;
ERPGItemCamera geRPGInventoryCamera;
vector<pair<string, int> > gvAnimItems;
int gnHeadID = -1;
bool gbShowPersItems = true;
externA5 bool gbLoad2DView;
externA5 bool gbLoadGameDB;
bool gbIterfacePreview;

static CScriptEditDlg scriptEditor;
void SyncActiveScript();

BEGIN_MESSAGE_MAP(CGameView,CWnd )
//{{AFX_MSG_MAP(CGameView)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_RBUTTONDOWN()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	ON_COMMAND(ID_NEW_TEXSPOT, OnNewTexspot)
	ON_COMMAND(ID_ADDTERRSPOT, OnNewTerrspot)
	ON_COMMAND(ID_ADDWAYPOINT, OnNewWaypoint)
	ON_COMMAND(ID_ADDOBJECT, OnNewObject)
	ON_COMMAND(ID_ADDLADDER, OnNewLadder)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WYSIWYG, OnUpdateViewWysiwyg)
	ON_COMMAND(ID_VIEW_WYSIWYG, OnViewWysiwyg)
	ON_COMMAND(ID_ASSIGN_SPOT_FRAGMENTS, OnSpotFragments)
	ON_COMMAND(ID_NEW_TEMPLATE_FROM_SEL, OnNewTemplateFromSel)
	ON_COMMAND(ID_SAVE_CAMERA, OnSaveCamera)
	ON_COMMAND(ID_RESTORE_CAMERA, OnRestoreCamera)
	ON_COMMAND(ID_VIEW_UNIT_ROUTE, OnViewRoute)
	ON_COMMAND(ID_SET_UNIT_ROUTE, OnSetRoute)
	ON_COMMAND(ID_ANIMATE_CAMERA, OnAnimateCamera)
	ON_COMMAND(ID_LOCKSELECTION, OnLockSelection)
	ON_COMMAND(ID_EDIT_MAPSCRIPT, OnEditScript)
	ON_COMMAND(ID_VIEW_PROPBROWSER, OnViewBrowser)
	ON_UPDATE_COMMAND_UI(ID_EDIT_MAPSCRIPT, OnUpdateEditScript)
	ON_UPDATE_COMMAND_UI(ID_LOCKSELECTION, OnUpdateLockSelection)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

#define DW_BITS( dw ) ( *reinterpret_cast<float*>( &(dw) ) )

CGameView::CGameView() : nLastPlacement(0), bWysiwyg(true)
{
	nMenuTimer = 0;
	nTerrSpotTimer = 0;
	gszBuildParams = theApp.GetProfileString( "", REG_MAPBUILD_ARG, "" );
	COLORREF crLight = theApp.GetProfileInt( LIGHT_SECTION, "LightCr", 0x80808080 );
	gLightPrefs.vLightColor.x = (float)GetRValue( crLight ) / 255.0f;
	gLightPrefs.vLightColor.y = (float)GetGValue( crLight ) / 255.0f;
	gLightPrefs.vLightColor.z = (float)GetBValue( crLight ) / 255.0f;

	COLORREF crAmbient = theApp.GetProfileInt( LIGHT_SECTION, "AmbientCr", 0x80808080 );
	gLightPrefs.vAmbientColor.x = (float)GetRValue( crAmbient ) / 255.0f;
	gLightPrefs.vAmbientColor.y = (float)GetGValue( crAmbient ) / 255.0f;
	gLightPrefs.vAmbientColor.z = (float)GetBValue( crAmbient ) / 255.0f;	
	
	COLORREF crFog = theApp.GetProfileInt( LIGHT_SECTION, "FogCr", 0xa5b2a5a5 );
	gLightPrefs.vFogColor.x = (float)GetRValue( crFog ) / 255.0f;
	gLightPrefs.vFogColor.y = (float)GetGValue( crFog ) / 255.0f;
	gLightPrefs.vFogColor.z = (float)GetBValue( crFog ) / 255.0f;

	COLORREF crGloss = theApp.GetProfileInt( LIGHT_SECTION, "GlossCr", 0xffffffff );
	gLightPrefs.vGlossColor.x = (float)GetRValue( crGloss ) / 255.0f;
	gLightPrefs.vGlossColor.y = (float)GetGValue( crGloss ) / 255.0f;
	gLightPrefs.vGlossColor.z = (float)GetBValue( crGloss ) / 255.0f;

	COLORREF crVapour = theApp.GetProfileInt( LIGHT_SECTION, "VapourCr", 0xffffffff );
	gLightPrefs.vVapourColor.x = (float)GetRValue( crVapour ) / 255.0f;
	gLightPrefs.vVapourColor.y = (float)GetGValue( crVapour ) / 255.0f;
	gLightPrefs.vVapourColor.z = (float)GetBValue( crVapour ) / 255.0f;

	COLORREF crShadow = theApp.GetProfileInt( LIGHT_SECTION, "ShadowCr", 0 );
	gLightPrefs.vShadowColor.x = (float)GetRValue( crShadow ) / 255.0f;
	gLightPrefs.vShadowColor.y = (float)GetGValue( crShadow ) / 255.0f;
	gLightPrefs.vShadowColor.z = (float)GetBValue( crShadow ) / 255.0f;

	COLORREF crBack = theApp.GetProfileInt( LIGHT_SECTION, "BackCr", 0 );
	gLightPrefs.vBackColor.x = (float)GetRValue( crBack ) / 255.0f;
	gLightPrefs.vBackColor.y = (float)GetGValue( crBack ) / 255.0f;
	gLightPrefs.vBackColor.z = (float)GetBValue( crBack ) / 255.0f;

	COLORREF crGroundAmbient = theApp.GetProfileInt( LIGHT_SECTION, "GroundAmbientColor", crAmbient );
	gLightPrefs.vGroundAmbientColor.x = (float)GetRValue( crGroundAmbient ) / 255.0f;
	gLightPrefs.vGroundAmbientColor.y = (float)GetGValue( crGroundAmbient ) / 255.0f;
	gLightPrefs.vGroundAmbientColor.z = (float)GetBValue( crGroundAmbient ) / 255.0f;

	float fox = 10;
	float foy = 10;
	float foz = 0;
	DWORD ox = theApp.GetProfileInt( LIGHT_SECTION, "OriginX", FP_BITS( fox ) );
	DWORD oy = theApp.GetProfileInt( LIGHT_SECTION, "OriginY", FP_BITS( foy ) );
	DWORD oz = theApp.GetProfileInt( LIGHT_SECTION, "OriginZ", FP_BITS( foz ) );
	gLightPrefs.ptOrigin.x = DW_BITS( ox );
	gLightPrefs.ptOrigin.y = DW_BITS( oy );
	gLightPrefs.ptOrigin.z = DW_BITS( oz );

	float fVNP = 0.8f;
	float fVS  = 0.05f;
	float fVST = 5;
	DWORD fS = theApp.GetProfileInt( LIGHT_SECTION, "FogStart", FP_BITS( foz ) );
	DWORD fD = theApp.GetProfileInt( LIGHT_SECTION, "FogDistance", FP_BITS( foz ) );
	DWORD dwVNP = theApp.GetProfileInt( LIGHT_SECTION, "VapourNoiseParam", FP_BITS( fVNP ) );
	DWORD dwVS  = theApp.GetProfileInt( LIGHT_SECTION, "VapourSpeed", FP_BITS( fVS ) );
	DWORD dwVST = theApp.GetProfileInt( LIGHT_SECTION, "VapourSwitchTime", FP_BITS( fVST ) );
	gLightPrefs.fFogStart = DW_BITS( fS );
	gLightPrefs.fFogDistance = DW_BITS( fD );
	gLightPrefs.fVapourNoiseParam = DW_BITS( dwVNP );
	gLightPrefs.fVapourSpeed = DW_BITS( dwVS );
	gLightPrefs.fVapourSwitchTime = DW_BITS( dwVST );

	float fdx = 0.1f;
	float fdy = 0.2f;
	float fdz = -1.0f;
	DWORD dx = theApp.GetProfileInt( LIGHT_SECTION, "DirectionX", FP_BITS( fdx ) );
	DWORD dy = theApp.GetProfileInt( LIGHT_SECTION, "DirectionY", FP_BITS( fdy ) );
	DWORD dz = theApp.GetProfileInt( LIGHT_SECTION, "DirectionZ", FP_BITS( fdz ) );
	gLightPrefs.ptLight.x = DW_BITS( dx );
	gLightPrefs.ptLight.y = DW_BITS( dy );
	gLightPrefs.ptLight.z = DW_BITS( dz );

	float fVH = 5, fVD = 1;
	gLightPrefs.fVapourHeight  = theApp.GetProfileInt( LIGHT_SECTION, "VapourHeight", FP_BITS(fVH) );
	gLightPrefs.fVapourDensity = theApp.GetProfileInt( LIGHT_SECTION, "VapourDensity", FP_BITS(fVD) );
	gLightPrefs.nSkyID = theApp.GetProfileInt( LIGHT_SECTION, "SkyID", 1 );

	gfMayaCameraSpeed = 1.0f;
	DWORD dw = theApp.GetProfileInt( CAMERA_SECTION, "Speed", FP_BITS( gfMayaCameraSpeed ) );
	gfMayaCameraSpeed = DW_BITS( dw );
	bDXTModeOn = theApp.GetProfileInt( "", REG_USEDXT, 1 );
	gbAutoSelAnimModel = theApp.GetProfileInt( "", REG_AUTOSELANIMMODEL, 1 );
	gbAutoSelAnimItems = theApp.GetProfileInt( "", REG_AUTOSELANIMITEMS, 1 );
	ReadEffectorPrefs();
	geRPGInventoryCamera = (ERPGItemCamera)theApp.GetProfileInt( "", "RPGInventoryCamera", CAM_INVENTORY );
	gnHeadID = theApp.GetProfileInt( "", "HeadID", -1 );
	gbShowPersItems = theApp.GetProfileInt( "", "RPGShowPersItems", true );
	gbLoad2DView = theApp.GetProfileInt( "", REG_SHOW2DVIEW, true );
	gbIterfacePreview = theApp.GetProfileInt( "", REG_INTERFACEPREVIEW, false );

	GetUserSettingsSetup().SetParam( ME_LOCK_SELECTION, false );

	CItemsListDlg::RegisterControlClass();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGameView::~CGameView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CGameView::PreCreateWindow(CREATESTRUCT& cs) 
{
  if (!CWnd::PreCreateWindow(cs))
    return FALSE;
//  cs.dwExStyle |= WS_EX_CLIENTEDGE;
  cs.style &= ~WS_BORDER;
  cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
    ::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
  
  return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGameView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CWnd ::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  if ( !SetGameWnd( this ) )
		return -1;
	wysiwygMenu.CreatePopupMenu();
	wysiwygMenu.AppendMenu( MF_STRING, ID_NEW_TEXSPOT, "New Building Spot..." );
	wysiwygMenu.AppendMenu( MF_STRING, ID_ASSIGN_SPOT_FRAGMENTS, "Set building spot fragments" );
	wysiwygMenu.AppendMenu( MF_STRING, ID_ADDTERRSPOT, "New Terrain Spot..." );
	wysiwygMenu.AppendMenu( MF_SEPARATOR );
//	wysiwygMenu.AppendMenu( MF_STRING, ID_ADDOBJECT, "New Object..." );
	wysiwygMenu.AppendMenu( MF_STRING, ID_ADDWAYPOINT, "New Waypoint..." );
	wysiwygMenu.AppendMenu( MF_STRING, ID_ADDLADDER, "New Ladder" );
	wysiwygMenu.AppendMenu( MF_SEPARATOR );
	wysiwygMenu.AppendMenu( MF_STRING, ID_NEW_TEMPLATE_FROM_SEL, "New template from selection" );
	wysiwygMenu.AppendMenu( MF_SEPARATOR );
	wysiwygMenu.AppendMenu( MF_STRING, ID_SAVE_CAMERA, "Save Camera position" );
	wysiwygMenu.AppendMenu( MF_STRING, ID_RESTORE_CAMERA, "Restore Camera position" );
	wysiwygMenu.AppendMenu( MF_STRING, ID_ANIMATE_CAMERA, "Animate Camera position..." );
	wysiwygMenu.AppendMenu( MF_SEPARATOR );
	wysiwygMenu.AppendMenu( MF_STRING, ID_VIEW_UNIT_ROUTE, "View unit route" );
	wysiwygMenu.AppendMenu( MF_STRING, ID_SET_UNIT_ROUTE, "Set route" );
	wysiwygMenu.AppendMenu( MF_SEPARATOR );
	wysiwygMenu.AppendMenu( MF_STRING, ID_VIEW_PROPBROWSER, "View property browser" );
	if ( GetUserSettings().GetParam( ME_SWITCH_SYSCOLORS ) )
		SetupSysColors();
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 GetBestCameraPos( int nPlacementID )
{
	CPlacement *pPl = theApp.GetActivePlacement();
	if ( !pPl || pPl->GetID() != nPlacementID )
		pPl = theTemplMgr.GetPlacement( nPlacementID );
  if ( !pPl )
    return CVec3( 0, 0, 4 );
  
  int l = FP_GRID_STEP * max( pPl->GetWidth(), pPl->GetHeight() );
  float z = 4.0f + float(l >> 1) / tan( ToRadian( 17.5f ) );
  z = Max( z, 5.0f );
  return CVec3( FP_GRID_STEP * pPl->GetWidth() / 2, FP_GRID_STEP * pPl->GetHeight() / 2, z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//#pragma comment(linker, "/include:_ForceGSceneGraph")
#pragma comment(linker, "/include:_ForceFontFormat")
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGameView::SetGameWnd( CWnd *pWnd )
{
	char buf[1024];

	GetCurrentDirectory( sizeof( buf ), buf );
	strcat( buf, "\\res" );
	NGScene::AddResourceDir( buf );
	NGScene::RunResourceLoadingThread();
	theApp.UpdateGameDB();
  //
	if ( !NGfx::Init3D( pWnd->m_hWnd ) )
  {
    ASSERT(0); // DX8 not found
		::MessageBox( AfxGetMainWnd()->m_hWnd, "Failed to initialize Direct3D8", "Error", MB_OK );
    return false;
  }
  if ( !NInput::InitInput( AfxGetMainWnd()->m_hWnd, true, 128 ) )//pParent->GetParent()->m_hWnd );
	{
		ASSERT(0); // DX8input not found
		::MessageBox( AfxGetMainWnd()->m_hWnd, "Failed to initialize DirectInput", "Error", MB_OK );
		return false;
	}
	if ( !NSound::InitSound( AfxGetMainWnd()->m_hWnd ) )
	{
		::MessageBox( AfxGetMainWnd()->m_hWnd, "Failed to initialize FMod", "Error", MB_OK );
	}
  //
  //NInput::LoadBindScript( "bind.cfg" );
	NGlobal::LoadConfig( ".\\cfg\\autoexec.cfg" );
	NInput::SetSection( "mapeditor" );
  //NGfx::SetMode( NGfx::SVideoMode( 640, 480, 32, NGfx::WINDOWED ) );
	if ( !NGScene::SetModeFromConfig() )
	{
		ASSERT(0);
		return false;
	}
	if ( !NSound::SetModeFromConfig() )
	{
		::MessageBox( AfxGetMainWnd()->m_hWnd, "Failed to set sound mode", "Error", MB_OK );
	}
	bDXTModeOn = theApp.GetProfileInt( "", REG_USEDXT, 1 );

	gLightPrefs.bShadows = theApp.HasShadows();
	//
	CICMapEditor::SMapParams p;
	p.nView        = CICMapEditor::VIEW_MODEL;
	p.nObjectID    = -1;
	p.lightPrefs   = gLightPrefs;
	p.bCameraReset = true;
	p.ptCameraPos  = GetBestCameraPos( -1 );
	if ( gbLoadGameDB )
		NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
  return true;
}
static CICMapEditor::EViewType eLastViewType = CICMapEditor::EViewType(-1);
////////////////////////////////////////////////////////////////////////////////////////////////////
CICMapEditor::SMapParams FillParams( CICMapEditor::EViewType type, int nObjectID, int nExtra = -1 )
{
	eLastViewType = type;
	gLightPrefs.bShadows = theApp.HasShadows();
	CICMapEditor::SMapParams p;
	p.nView        = type;
	p.nObjectID    = nObjectID;
	p.lightPrefs   = gLightPrefs;
	p.bCameraReset = theApp.IsCameraReset();
	p.ptCameraPos  = GetBestCameraPos( -1 );
	p.nExtra = nExtra;

	p.camera.fRod = 4;
	p.camera.fPitch = ToRadian( -89.0f );
	p.camera.fYaw   = ToRadian( -0.0f );
	p.camera.fYaw = 0;
	p.camera.fRoll = 0;
	p.camera.ptAnchor = VNULL3;

	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetRootPlacement( int nPlacementID )
{
	if ( bWysiwyg )
	{
		CVec3 ptPos  = GetBestCameraPos( nPlacementID );
		ICamera::SCameraPos pos;
		pos.fRod = ptPos.z;
		pos.fPitch = ToRadian( -89.0f );
		pos.fYaw   = ToRadian( -0.0f );
		pos.fYaw = 0;
		pos.fRoll = 0;
		pos.ptAnchor = CVec3( ptPos.x, ptPos.y, 0 );
		NMainLoop::Command( new CICWysiwyg( m_hWnd, nPlacementID, pos ) );
	}
	else
	{
		CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_PLACEMENT, nPlacementID, (int)&gszBuildParams );
		p.ptCameraPos  = GetBestCameraPos( nPlacementID );
		p.camera.ptAnchor = CVec3( p.ptCameraPos.x, p.ptCameraPos.y, 0 );
		p.camera.fRod = p.ptCameraPos.z;
		NMainLoop::Command( new CICMapEditor( p, m_hWnd, false ) );
	}
	SyncActiveScript();
	Invalidate(FALSE);
  nLastPlacement = nPlacementID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetModel( int nPlacementID, int nModelID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_MODEL, nModelID );
	p.ptCameraPos  = GetBestCameraPos( nPlacementID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );	
  nLastPlacement = nPlacementID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetCamera( ICamera::SCameraPos *pRes, float *pfFOV, const SResTree *pTree, int nItemID, const string &szPref )
{
	if ( pTree )
	{
		const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItemID );
		if ( pProps )
		{
			CPropMap::const_iterator iax = pProps->find( szPref + "CameraAnchorX" );
			CPropMap::const_iterator iay = pProps->find( szPref + "CameraAnchorY" );
			CPropMap::const_iterator iaz = pProps->find( szPref + "CameraAnchorZ" );
			CPropMap::const_iterator id = pProps->find( szPref + "CameraDistance" );
			CPropMap::const_iterator iy = pProps->find( szPref + "CameraYaw" );
			CPropMap::const_iterator ip = pProps->find( szPref + "CameraPitch" );
			CPropMap::const_iterator ir = pProps->find( szPref + "CameraRoll" );
			CPropMap::const_iterator ifov = pProps->find( szPref + "CameraFOV" );
			CPropMap::const_iterator e = pProps->end();
			if ( iax == e || iay == e || iaz == e || id == e || iy == e || ip == e || ir == e || ifov == e )
				return;
			pRes->ptAnchor.x = iax->second->GetValue();
			pRes->ptAnchor.y = iay->second->GetValue();
			pRes->ptAnchor.z = iaz->second->GetValue();
			if ( id->second->GetValue().GetType() == CVariant::VT_NULL )
				pRes->fRod = 3;
			else
				pRes->fRod = id->second->GetValue();
			pRes->fYaw = iy->second->GetValue();
			pRes->fPitch = ip->second->GetValue();
			pRes->fRoll = ir->second->GetValue();
			*pfFOV = ifov->second->GetValue();
			pTree->pItemsTree->ReleasePropList( pProps );
		}
	}
}
void CGameView::SetRPGItem( int nRPGItemID )
{
	gLightPrefs.bShadows = theApp.HasShadows();

	CICMapEditor::SMapParams p;
	p.nView       = CICMapEditor::VIEW_RPGITEM;
	p.camera.fRod = -1;

	string szPref;
	switch ( geRPGInventoryCamera )
	{
	case CAM_SLOT:
		szPref = "Slot";
		break;
	case CAM_AMMO:
		szPref = "Ammo";
		break;
	}
	const SResTree *pTree = theApp.GetResTree( IDC_RPG_ITEMS_TREE );
	SetCamera( &p.camera, &p.fFOV, pTree, nRPGItemID, szPref );

	p.nObjectID    = nRPGItemID;
	p.lightPrefs   = gLightPrefs;
	p.bCameraReset = theApp.IsCameraReset();
	p.ptCameraPos  = GetBestCameraPos( -1 );

  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
	
  nLastPlacement = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetAIModel( int nAIModelID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_AIMODEL, nAIModelID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetRndModel( int nModelID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_RNDMODEL, nModelID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetTexture( int nTextureID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_TEXTURE, nTextureID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetGeometry( int nGeometryID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_GEOMETRY, nGeometryID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetConstructionPart( int nPartVarID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_CONSTRUCTION_PART, nPartVarID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetMaterial( int nMaterialID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_MATERIAL, nMaterialID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetAnimation( int nGeometryID, int nAnimationID )
{
	if ( !gbAutoSelAnimItems )
	{
		CAnimItemsDlg dlg;

		dlg.DoModal();
	}
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_ANIMATION, nAnimationID, nGeometryID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
	//
	static CAnimFlagsDlg dlg( this );

	dlg.SetAnimationID( nAnimationID );
	if ( GetUserSettings().GetParam( ME_ANIM_SHOWFLAGS ) )
	{
		if ( !::IsWindow( dlg.m_hWnd ) )
		{
			dlg.Create( IDD_ANIM_FLAGS, this );
			dlg.ShowWindow( SW_SHOW );
		}
		else
		{
			if ( !dlg.IsWindowVisible() )
				dlg.ShowWindow( SW_SHOW );
			//
		}
	}
	else if ( ::IsWindow( dlg.m_hWnd ) )
		dlg.DestroyWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetParticle( int nParticleID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_PARTICLES, nParticleID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetContainer( int nContainerID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_CONTAINER, nContainerID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetSoundEffect( int nID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_SOUNDEFFECT, nID );
	NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetInterface( int nUIContainerID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_INTERFACE, nUIContainerID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetSound( int nSoundID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_SOUND, nSoundID );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetPers( int nPersID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_PERS, nPersID );

	const SResTree *pTree = theApp.GetResTree( IDC_RPG_PERS_TREE );
	p.camera.fRod = -1;
	string szPref = GetUserSettings().GetParam( ME_PERS_FACEGENCAMERA ) ? "FaceGen" : "";
	SetCamera( &p.camera, &p.fFOV, pTree, nPersID, szPref );

  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
	//CItemsListDlg dlg( nPersID, IDC_RPG_WEAPONS_TREE, "RPGWeapon4Pers", this );
	static CRPGItemsContainerDlg dlg( nPersID, this );

	dlg.SetRPGPersID( nPersID );
	if ( gbShowPersItems )
	{
		//dlg.DoModal();
		if ( !::IsWindow( dlg.m_hWnd ) )
			dlg.Create( this );
		else
		{
			if ( !dlg.IsWindowVisible() )
				dlg.ShowWindow( SW_SHOW );
			//
		}
	}
	else if ( ::IsWindow( dlg.m_hWnd ) )
		dlg.DestroyWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetHead( int nHeadID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_HEAD, nHeadID );
	p.camera.ptAnchor = VNULL3;
	p.camera.fRod = 1;
	p.ptCameraPos = CVec3( 0, 0, 1 );
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetObject( int nID, int nVarID )
{
	CICMapEditor::SMapParams p = FillParams( CICMapEditor::VIEW_OBJECT, nID );
	p.nExtra = nVarID;
  NMainLoop::Command( new CICMapEditor( p, m_hWnd ) );

	IObjectBrowser *p1 = GetBrowser( OB_OBJECTS_MODEL0 );
	IObjectBrowser *p2 = GetBrowser( OB_OBJECTS_MODEL1 );
	IObjectBrowser *p3 = GetBrowser( OB_OBJECTS_MODEL2 );
	IObjectBrowser *p4 = GetBrowser( OB_OBJECTS_MODEL3 );
	IObjectBrowser *p5 = GetBrowser( OB_OBJECTS_MODEL4 );
	if ( p1 && p2 && p3 && p4 && p5 )
	{
		vector< CPtr<IObjectBrowser> > v;
		v.push_back( p1 );
		v.push_back( p2 );
		v.push_back( p3 );
		v.push_back( p4 );
		v.push_back( p5 );
		static CObjBrowserContainer dlg( v );

		if ( GetUserSettings().GetParam( ME_SHOW_OBJ_BROWSER ) )
		{
			if ( !::IsWindow( dlg.m_hWnd ) )
			{
				static_cast<CDialog&>( dlg ).Create( IDD_OB_BROWSER_CONTAINER, this );
				dlg.ShowWindow( SW_SHOW );
			}
			else
			{
				if ( !dlg.IsWindowVisible() )
					dlg.ShowWindow( SW_SHOW );
			}
			dlg.SetObject( nID, nVarID );
		}
		else if ( ::IsWindow( dlg.m_hWnd ) )
			dlg.DestroyWindow();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CWnd ::OnLButtonDown(nFlags, point);
	theApp.bGameActive = true;
  theApp.bInputActive = true;
	SetFocus();
	//AfxGetMainWnd()->SetActiveWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const UINT MENU_TIMER = 1611;
const UINT TERRSPOT_TIMER = 1612;
const UINT WALLSPOT_TIMER = 1613;
void CGameView::OnRButtonDown(UINT nFlags, CPoint point) 
{
  CWnd ::OnRButtonDown(nFlags, point);
  theApp.bGameActive = true;
  theApp.bInputActive = true;
	SetFocus();
	ptRBDown = point;
	if ( bWysiwyg )
		nMenuTimer = SetTimer( MENU_TIMER, 100, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnDestroy() 
{
	NMainLoop::DoneInterface();		
  NInput::DoneInput();
  NGfx::Done3D();
	NSound::DoneSound();

  CWnd ::OnDestroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnPaint() 
{
	if ( NMainLoop::GetInterfaceStackDepth() > 0 )
		NMainLoop::StepApp( true, true );
	// Do not call CWnd ::OnPaint() for painting messages
	CPaintDC dc(this); // device context for painting
  //CWnd ::OnPaint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd ::OnSize(nType, cx, cy);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);
	theApp.bGameActive = true;
	theApp.bInputActive = true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnKillFocus(CWnd* pNewWnd) 
{
	CWnd ::OnKillFocus(pNewWnd);
	theApp.bGameActive = false;
	theApp.bInputActive = false;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnMouseMove(UINT nFlags, CPoint point) 
{
	KillTimer( nMenuTimer );
	CWnd ::OnMouseMove(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SelectMaterial( int nMaterialTree )
{
	static bool bLock = false;
	if ( bLock )
		return false;
	const SResTree *pTree = theApp.GetResTree( nMaterialTree );
	if ( !pTree )
		return false;
	bLock = true; //	
	bool bRet = false;
	if ( IDOK == pTree->pTreeDlg->DoModal() )
	{
		int nTree, nID;
		pTree->pTreeDlg->GetSelectedItemID( &nTree, &nID );
		if ( nID >= 0 )
		{
			GetUserSettingsSetup().SetSpotMaterial( nID );
			bRet = true;
		}
	}
	bLock = false;
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygRequest: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CWysiwygRequest)
public:
	int nSelectionMask;

	CWysiwygRequest() : nSelectionMask(0) {}
	virtual void Exec()
	{
		CDynamicCast<CIMapEditor> p(GetInterface());
		if (p)
		{
			nSelectionMask = p->GetSelectionMask();
			p->SaveHitTestObject();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnTimer(UINT nIDEvent) 
{
	if ( !(0x8000 & GetAsyncKeyState( VK_CONTROL )) && !(0x8000 & GetAsyncKeyState( VK_MENU )) )
	{
		switch ( nIDEvent )
		{
			case MENU_TIMER:
			{
				if ( nIDEvent != MENU_TIMER )
					break;
				CObj<CWysiwygRequest> pR = new CWysiwygRequest;
				NMainLoop::Command( pR );
				NMainLoop::StepApp(false, true);
				NInput::PumpMessages( false );
				CPoint pt;
				GetCursorPos( &pt );
				wysiwygMenu.EnableMenuItem( ID_NEW_TEXSPOT, pR->nSelectionMask & BT_GEOMETRY ? MF_ENABLED : MF_GRAYED );
				wysiwygMenu.EnableMenuItem( ID_ASSIGN_SPOT_FRAGMENTS, pR->nSelectionMask & BT_WALLSPOT ? MF_ENABLED : MF_GRAYED );
				wysiwygMenu.EnableMenuItem( ID_VIEW_UNIT_ROUTE, pR->nSelectionMask & BT_UNIT ? MF_ENABLED : MF_GRAYED );
				bool bSetRoute = pR->nSelectionMask & BT_UNIT &&  pR->nSelectionMask & BT_WAYPOINT;
				wysiwygMenu.EnableMenuItem( ID_SET_UNIT_ROUTE, bSetRoute ? MF_ENABLED : MF_GRAYED );
				bool bObject = pR->nSelectionMask & BT_OBJECT;
				wysiwygMenu.EnableMenuItem( ID_VIEW_PROPBROWSER, bObject ? MF_ENABLED : MF_GRAYED );
				bWYSIWYGActive = false;
				wysiwygMenu.TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );
				break;
			}
			case TERRSPOT_TIMER:
			{
				if ( SelectMaterial( IDC_SPOTS_TREE ) )
				{
					NInput::PostEvent( "new_terrspot" );
					NMainLoop::StepApp( false, true );
				}
				break;
			}
			case WALLSPOT_TIMER:
			{
				if ( SelectMaterial( IDC_MATERIALS_TREE ) )
				{
					NInput::PostEvent( "new_texprojection" );
					NMainLoop::StepApp( false, true );
				}
			}
		}
	}
	KillTimer( nIDEvent );
	CWnd ::OnTimer(nIDEvent);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewTexspot() 
{
	// если из этого обработчика вызывать диалог, возикают глюки
	nTerrSpotTimer = SetTimer( WALLSPOT_TIMER, 10, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewTerrspot() 
{
	// если из этого обработчика вызывать диалог, возикают глюки
	nTerrSpotTimer = SetTimer( TERRSPOT_TIMER, 10, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnUpdateViewWysiwyg(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( bWysiwyg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnViewWysiwyg() 
{
	bWysiwyg = !bWysiwyg;
	int nTree, nItem, nVariantID;

	theApp.GetActiveItem( &nTree, &nItem, &nVariantID );
	if ( nTree == IDC_TEMPLATE_TREE )
		SetRootPlacement( nVariantID );
	NInput::PostEvent( "update" );
	NMainLoop::StepApp( true, true );
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewWaypoint()
{
	int nTree, nItem, nVariant;
	theApp.GetActiveItem( &nTree, &nItem, &nVariant );
	const SResTree *pNames = theApp.GetResTree( IDC_WAYPOINTNAMES_TREE );
	if ( nTree != IDC_TEMPLATE_TREE || !pNames )
		return;
	CNameAndFolder dlg( pNames );

	if ( IDOK != dlg.DoModal() )
		return;
	int nID = AddWaypoint2DB( dlg.nSelectedItem, nVariant );
	if ( nID < 0 )
		return;
	GetUserSettingsSetup().SetSelectedBrushID( LID_WAYPOINTS, nID );
	Sleep(0);
//	NDatabase::Refresh<NDb::CWaypoint>();
//	NDatabase::Refresh<NDb::CTemplVariant>();
	//
	NInput::PostEvent( "new_waypoint" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::Activate()
{
	if ( CICMapEditor::VIEW_INTERFACE == eLastViewType )
	{
		NInput::PostEvent( "update" );
		NMainLoop::StepApp( true, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CGameView::PreTranslateMessage(	MSG *pMsg )
{
	return CWnd::PreTranslateMessage( pMsg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewObject()
{
	static bool bLock = false;
	int nTree, nItem, nVariant;
	theApp.GetActiveItem( &nTree, &nItem, &nVariant );
	const SResTree *pTree = theApp.GetResTree( IDC_PLACABLE_TREE );
	if ( nTree != IDC_TEMPLATE_TREE || !pTree )
		return;
	CPlacement *pPl = theApp.GetActivePlacement();
	if ( !pPl )
		return;
	vector<SResTree> trees;
	trees.push_back( *theApp.GetResTree( IDC_OBJECTS_TREE ) );
	trees.push_back( *theApp.GetResTree( IDC_RPG_ITEMS_TREE ) );
	static CTreeSelItemDlg seldlg( trees );

	bLock = true; //
	if ( IDOK != seldlg.DoModal() )
		return;
	seldlg.GetSelectedItemID( &nTree, &nItem );
	if ( nItem <= 0 )
		return;
	int nID = pPl->AddObj( theApp.GetActiveFloor(), VNULL2, nTree, nItem );
	GetUserSettingsSetup().SetSelectedBrushID( LID_OBJECTS, nID );
	if ( nID > 0 )
	{
		CFinPosDB posDb;
		posDb.SetLightmap( nID, true );
	}
	bLock = false;
	NDatabase::Refresh<NDb::CFinalElement>();
	//
	NInput::PostEvent( "new_object" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnSpotFragments()
{
	NInput::PostEvent( "assign_spot_fragments" );
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewLadder()
{
	NInput::PostEvent( "new_ladder" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::RunMap( int nMapID )
{
	//SetParent( GetDesktopWindow() );
	//ModifyStyle( WS_CHILDWINDOW, 0 );
	//SetWindowPos( 0, 0, 0, 1024, 768, SWP_NOZORDER | SWP_SHOWWINDOW );
	//SetWindowPos( &wndTop, 0, 0, 1024, 768, SWP_SHOWWINDOW );
	//BringWindowToTop();
	//SetFocus();
	//SetActiveWindow();
	//NMainLoop::StepApp( true );
//	NGfx::SetMode( NGfx::SVideoMode( 1024, 768, 32, NGfx::FULL_SCREEN ) );

//	string szCmd = "map ";
//	szCmd += IToA( nMapID );
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nMapID );
	if ( !IsValid( pVar ) || !IsValid( pVar->pTemplate ) )
		return;
	if ( pVar->pTemplate->nWidth < 9 || pVar->pTemplate->nHeight < 9 )
	{
		::MessageBox( AfxGetMainWnd()->m_hWnd, "Too small map", "Error", MB_OK );
		return;
	}
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );

	NMainLoop::ShowLogo();
	NMainLoop::Command( new NGame::CICMapEditBeginMission( nMapID, pGlobalGame ) );
	BeginWaitCursor();
	NMainLoop::StepApp( false, true );
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnNewTemplateFromSel()
{
	BeginWaitCursor();
	NInput::PostEvent( "selection_template" );
	NMainLoop::StepApp( true, true );
	NMainLoop::StepApp( true, true );
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnViewBrowser()
{
	NInput::PostEvent( "view_objbrowser" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int SelectCameraID()
{
	const SResTree *pTree = theApp.GetResTree( IDC_CAMERAS_TREE );
	if ( !pTree )
		return -1;
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return -1;
	int nTree, nID;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nID );
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnSaveCamera()
{
	int nID = SelectCameraID();
	if ( nID <= 0 )
		return;
	GetUserSettingsSetup().SetSelectedBrushID( LID_CAMERAS, nID );
	NInput::PostEvent( "save_camera" );
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnRestoreCamera()
{
	int nID = SelectCameraID();
	if ( nID <= 0 )
		return;
	GetUserSettingsSetup().SetSelectedBrushID( LID_CAMERAS, nID );
	NInput::PostEvent( "restore_camera" );
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnViewRoute()
{
	NInput::PostEvent( "view_unit_route" );
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnSetRoute()
{
	NInput::PostEvent( "set_unit_route" );
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnAnimateCamera()
{
	NInput::PostEvent( "animate_camera" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnLockSelection()
{
	bool bLock = GetUserSettings().GetParam( ME_LOCK_SELECTION );
	GetUserSettingsSetup().SetParam( ME_LOCK_SELECTION, !bLock );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnUpdateLockSelection(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck( GetUserSettings().GetParam( ME_LOCK_SELECTION ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnUpdateEditScript(CCmdUI* pCmdUI)
{
	int nTree, nID, nVar;
	theApp.GetActiveItem( &nTree, &nID, &nVar );

	pCmdUI->Enable( nTree == IDC_TEMPLATE_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SyncActiveScript()
{
	if ( !::IsWindow( scriptEditor.m_hWnd ) || !scriptEditor.IsWindowVisible() )
		return;
	//
	int nTree, nID, nVar;
	theApp.GetActiveItem( &nTree, &nID, &nVar );

	if ( nTree != IDC_TEMPLATE_TREE )
		return;
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	const SResTree *pScriptTree = theApp.GetResTree( IDC_SCRIPTS_TREE );
	if ( !pTree || !pScriptTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID, nVar );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "ScriptID" );
	int nScripID = i != pProps->end() ? i->second->GetValue() : -1;
	pTree->pItemsTree->ReleasePropList( pProps );
	//
	const CPropMap *pSProps = pScriptTree->pItemsTree->GetPropList( nScripID );
	if ( !pSProps )
	{
		scriptEditor.ShowWindow( SW_HIDE );
		return;
	}
	CPropMap::const_iterator icode = pSProps->find( "CodeText" );
	if ( icode == pSProps->end() )
		return;
	scriptEditor.SetScriptID( nScripID );
	pScriptTree->pItemsTree->ReleasePropList( pSProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::OnEditScript()
{
	if ( !::IsWindow( scriptEditor.m_hWnd ) )
	{
		if ( !scriptEditor.Create( CScriptEditDlg::IDD, this ) )
			return;
	}
	if ( !scriptEditor.IsWindowVisible() )
		scriptEditor.ShowWindow( SW_SHOW );
	SyncActiveScript();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline float GetLinear( float f )
{
	if ( f <= 0.1047f )//0.04045f )
		return f / 4;//12.92f;
	else
		return exp( log( ( f + 0.1466f ) / 1.1466f ) * 2.4f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int MAX_SYSCOLORS = 30;
static DWORD sysColors[MAX_SYSCOLORS];
static int sysElements[MAX_SYSCOLORS];
static bool bMapEditColors = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateSysColor( int nID, vector<int> *pElements, vector<COLORREF> *pColors )
{
	DWORD cr = GetSysColor( nID );
	sysColors[nID] = cr;
	sysElements[nID] = nID;
	int r = 255 * GetLinear( GetRValue(cr) / 255.0f );
	int g = 255 * GetLinear( GetGValue(cr) / 255.0f );
	int b = 255 * GetLinear( GetBValue(cr) / 255.0f );
	pElements->push_back( nID );
	pColors->push_back( RGB( r, g, b ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetupSysColors()
{
	if ( bMapEditColors )
		return;
	vector<int> elements;
	vector<COLORREF> colors;
	for ( int i = 0; i < MAX_SYSCOLORS; ++i )
		UpdateSysColor( i, &elements, &colors );

	bMapEditColors = true;
	SetSysColors( elements.size(), &elements[0], &colors[0] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RestoreSysColors()
{
	if ( !bMapEditColors )
		return;
	SetSysColors( MAX_SYSCOLORS, sysElements, sysColors );
	bMapEditColors = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
