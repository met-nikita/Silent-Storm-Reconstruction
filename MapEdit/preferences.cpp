// preferences.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "preferences.h"
#include "Export.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\iMain.h"
#include "..\Main\gResource.h"
#include "..\Main\iMapEditor.h"
#include "MEUserSettings.h"
#include "MEParams.h"
#include "GameView.h"
#include "dbDefs.h"
#include "TreeSelItemdlg.h"
#include "ItemsMgr.h"
#include "UserSettingsSetup.h"
#include "..\Input\Bind.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern SLightPrefs gLightPrefs;
extern string gszBuildParams;
extern float gfMayaCameraSpeed;
extern ERPGItemCamera geRPGInventoryCamera;
extern bool gbShowPersItems;
extern bool gbLoad2DView;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportPrefsDlg property page

IMPLEMENT_DYNCREATE(CExportPrefsDlg, CPropertyPage)

CExportPrefsDlg::CExportPrefsDlg() : CPropertyPage(CExportPrefsDlg::IDD)
, m_bGUIMode(FALSE)
, m_szDBServer(_T(""))
, m_bObjectsShowGround(FALSE)
, m_bAnimShowFlags(FALSE)
, m_bShowObjBrowser(FALSE)
{
	//{{AFX_DATA_INIT(CExportPrefsDlg)
	m_src = _T("");
	m_dst = _T("");
	m_fCameraSpeed = 0.0f;
	//}}AFX_DATA_INIT
}

CExportPrefsDlg::~CExportPrefsDlg()
{
}

void CExportPrefsDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExportPrefsDlg)
	DDX_Text(pDX, IDC_EXPORT_PREFS_SRC, m_src);
	DDX_Text(pDX, IDC_EXPORT_PREFS_DST, m_dst);
	DDX_Text(pDX, IDC_CAMERA_SPEED, m_fCameraSpeed);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_GUIMODE, m_bGUIMode);
	DDX_Check(pDX, ID_USEDXT, m_bUseDXT);
	DDX_Check(pDX, ID_AUTOSELECT_ANIM_MODEL, m_bAutoSelAnimModel);
	DDX_Check(pDX, ID_AUTOSELECT_ANIM_ITEMS, m_bAutoSelAnimItems);
	DDX_Text(pDX, IDC_EXPORT_PREFS_DB, m_szDBServer);
	DDX_Check(pDX, IDC_OBJECTS_SHOWGROUND, m_bObjectsShowGround);
	DDX_Check(pDX, ID_ANIM_SHOWFLAGS, m_bAnimShowFlags);
	DDX_Check(pDX, IDC_SHOW_OBJ_BROWSER, m_bShowObjBrowser);
}


BEGIN_MESSAGE_MAP(CExportPrefsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CExportPrefsDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportPrefsDlg message handlers
extern bool bDXTModeOn;
extern bool gbAutoSelAnimModel;
extern bool gbAutoSelAnimItems;
extern string gszDBServer;

BOOL CExportPrefsDlg::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	m_srcCtrl.Initialize( IDC_EXPORT_PREFS_SRC, this );
	m_dstCtrl.Initialize( IDC_EXPORT_PREFS_DST, this );
//	m_srcCtrl.SetWindowText( GetExportSrcDir().c_str() );
//	m_dstCtrl.SetWindowText( GetExportDstDir().c_str() );
	m_src = ( GetExportSrcDir().c_str() );
	m_dst = ( GetExportDstDir().c_str() );
	m_fCameraSpeed = gfMayaCameraSpeed;
	m_bGUIMode = theApp.GetProfileInt( "", REG_EXPORT_GUI, 0 );
	m_bUseDXT = bDXTModeOn;
	m_bAutoSelAnimModel = gbAutoSelAnimModel;
	m_bAutoSelAnimItems = gbAutoSelAnimItems;
	m_szDBServer = gszDBServer.c_str();
	m_bObjectsShowGround = GetUserSettings().GetParam( ME_OBJECTS_SHOWGROUND );
	m_bShowObjBrowser = GetUserSettings().GetParam( ME_SHOW_OBJ_BROWSER );
	m_bAnimShowFlags = GetUserSettings().GetParam( ME_ANIM_SHOWFLAGS );
	UpdateData( FALSE );
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CExportPrefsDlg::OnOK() 
{
	UpdateData();
  theApp.WriteProfileString( "", REG_EXPORT_SRC, m_src );
  theApp.WriteProfileString( "", REG_EXPORT_DST, m_dst );
	theApp.WriteProfileInt( CAMERA_SECTION, "Speed", FP_BITS( m_fCameraSpeed ) );
	theApp.WriteProfileInt( "", REG_EXPORT_GUI, m_bGUIMode );
	theApp.WriteProfileInt( "", REG_USEDXT, m_bUseDXT );
	theApp.WriteProfileInt( "", REG_AUTOSELANIMITEMS, m_bAutoSelAnimItems );
	if ( bDXTModeOn != (bool)m_bUseDXT )
	{
		int nTree, nItem, nVar;
		theApp.GetActiveItem( &nTree, &nItem, &nVar );
		theApp.SetActiveItem( nTree, -1, -1, false );
		NGScene::CloseAllResources();
		ClearHoldQueue();
		bDXTModeOn = m_bUseDXT;
		theApp.SetActiveItem( nTree, nItem, nVar, false );
	}
	gfMayaCameraSpeed = m_fCameraSpeed;
	gbAutoSelAnimModel = m_bAutoSelAnimModel;
	gbAutoSelAnimItems = m_bAutoSelAnimItems;
	GetUserSettingsSetup().SetParam( ME_OBJECTS_SHOWGROUND, m_bObjectsShowGround );
	GetUserSettingsSetup().SetParam( ME_SHOW_OBJ_BROWSER, m_bShowObjBrowser );
	GetUserSettingsSetup().SetParam( ME_ANIM_SHOWFLAGS, m_bAnimShowFlags );
	if ( m_szDBServer != CString( gszDBServer.c_str() ) )
	{
		gszDBServer = (LPCSTR)m_szDBServer;
		theApp.WriteProfileString( "", "DB", m_szDBServer );
		MessageBox( "Map editor will reconnect to database the next time you start MapEdit.exe" );
	}
	CPropertyPage::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightPrefsDlg property page

IMPLEMENT_DYNCREATE(CLightPrefsDlg, CPropertyPage)

CLightPrefsDlg::CLightPrefsDlg() : CPropertyPage(CLightPrefsDlg::IDD)
, m_bSwitchSysColors(FALSE)
{
	//{{AFX_DATA_INIT(CLightPrefsDlg)
	m_dirX = 0.0f;
	m_dirY = 0.0f;
	m_dirZ = 0.0f;
	m_orgX = 0.0f;
	m_orgY = 0.0f;
	m_orgZ = 0.0f;
	m_fFogDistance = 0.0f;
	m_fVapourDensity = 0.0f;
	m_fVapourHeight = 0.0f;
	m_fFogStart = 0.0f;
	//}}AFX_DATA_INIT
}

CLightPrefsDlg::~CLightPrefsDlg()
{
}

void CLightPrefsDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLightPrefsDlg)
	DDX_Control(pDX, IDC_VAPOUR_COLOR, m_vapourColor);
	DDX_Control(pDX, IDC_GLOSS_COLOR, m_glossColor);
	DDX_Control(pDX, IDC_FOG_COLOR, m_fogColor);
	DDX_Control(pDX, IDC_AMBIENT_COLOR, m_ambientColor);
	DDX_Control(pDX, IDC_LIGHT_COLOR, m_lightColor);
	DDX_Text(pDX, IDC_LIGHT_DIR_X, m_dirX);
	DDX_Text(pDX, IDC_LIGHT_DIR_Y, m_dirY);
	DDX_Text(pDX, IDC_LIGHT_DIR_Z, m_dirZ);
	DDX_Text(pDX, IDC_LIGHT_ORIGIN_X, m_orgX);
	DDX_Text(pDX, IDC_LIGHT_ORIGIN_Y, m_orgY);
	DDX_Text(pDX, IDC_LIGHT_ORIGIN_Z, m_orgZ);
	DDX_Text(pDX, IDC_FOG_DISTANCE, m_fFogDistance);
	DDV_MinMaxFloat(pDX, m_fFogDistance, 0.f, 10000.f);
	DDX_Text(pDX, IDC_VAPOUR_DENSITY, m_fVapourDensity);
	DDX_Text(pDX, IDC_VAPOUR_HEIGHT, m_fVapourHeight);
	DDX_Text(pDX, IDC_FOG_STARTDISTANCE, m_fFogStart);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_SWITCHSYSCOLORS, m_bSwitchSysColors);
}


BEGIN_MESSAGE_MAP(CLightPrefsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CLightPrefsDlg)
	ON_BN_CLICKED(IDC_AMBIENT_COLOR, OnAmbientColor)
	ON_BN_CLICKED(IDC_LIGHT_COLOR, OnLightColor)
	ON_BN_CLICKED(IDC_FOG_COLOR, OnFogColor)
	ON_BN_CLICKED(IDC_LIGHT_LOADPRESET, OnLightLoadpreset)
	ON_BN_CLICKED(IDC_LIGHT_SAVE, OnLightSave)
	ON_BN_CLICKED(IDC_VAPOUR_COLOR, OnVapourColor)
	ON_BN_CLICKED(IDC_GLOSS_COLOR, OnGlossColor)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightPrefsDlg message handlers

void CLightPrefsDlg::OnOK() 
{
	UpdateData();
	theApp.WriteProfileInt( LIGHT_SECTION, "AmbientCr", m_crAmbient );
	theApp.WriteProfileInt( LIGHT_SECTION, "LightCr", m_crLight );
	theApp.WriteProfileInt( LIGHT_SECTION, "FogCr",  m_crFog );
	theApp.WriteProfileInt( LIGHT_SECTION, "GlossCr",  m_crGloss );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourCr",  m_crVapour );

	theApp.WriteProfileInt( LIGHT_SECTION, "OriginX", FP_BITS( m_orgX ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "OriginY", FP_BITS( m_orgX ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "OriginZ", FP_BITS( m_orgX ) );

	theApp.WriteProfileInt( LIGHT_SECTION, "DirectionX", FP_BITS( m_dirX ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "DirectionY", FP_BITS( m_dirY ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "DirectionZ", FP_BITS( m_dirZ ) );

	theApp.WriteProfileInt( LIGHT_SECTION, "FogStart", FP_BITS( m_fFogStart ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "FogDistance", FP_BITS( m_fFogDistance ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "SkyID", m_nSkyID );
	theApp.WriteProfileInt( LIGHT_SECTION, "ShadowCr", m_crShadow );
	theApp.WriteProfileInt( LIGHT_SECTION, "BackCr", m_crBack );
	theApp.WriteProfileInt( LIGHT_SECTION, "GroundAmbientColor", m_crGroundAmbient );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourHeight", FP_BITS( m_fVapourHeight ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourDensity", FP_BITS( m_fVapourDensity ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourNoiseParam", FP_BITS( m_fVNP ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourSpeed", FP_BITS( m_fVS ) );
	theApp.WriteProfileInt( LIGHT_SECTION, "VapourSwitchTime", FP_BITS( m_fVST ) );


	gLightPrefs.vAmbientColor.x = float( GetRValue( m_crAmbient ) ) / 255.0f;
	gLightPrefs.vAmbientColor.y = float( GetGValue( m_crAmbient ) ) / 255.0f;
	gLightPrefs.vAmbientColor.z = float( GetBValue( m_crAmbient ) ) / 255.0f;

	gLightPrefs.vLightColor.x = float( GetRValue( m_crLight ) ) / 255.0f;
	gLightPrefs.vLightColor.y = float( GetGValue( m_crLight ) ) / 255.0f;
	gLightPrefs.vLightColor.z = float( GetBValue( m_crLight ) ) / 255.0f;

	gLightPrefs.vFogColor.x = float( GetRValue( m_crFog ) ) / 255.0f;
	gLightPrefs.vFogColor.y = float( GetGValue( m_crFog ) ) / 255.0f;
	gLightPrefs.vFogColor.z = float( GetBValue( m_crFog ) ) / 255.0f;

	gLightPrefs.vGlossColor.x = float( GetRValue( m_crGloss ) ) / 255.0f;
	gLightPrefs.vGlossColor.y = float( GetGValue( m_crGloss ) ) / 255.0f;
	gLightPrefs.vGlossColor.z = float( GetBValue( m_crGloss ) ) / 255.0f;

	gLightPrefs.vVapourColor.x = float( GetRValue( m_crVapour ) ) / 255.0f;
	gLightPrefs.vVapourColor.y = float( GetGValue( m_crVapour ) ) / 255.0f;
	gLightPrefs.vVapourColor.z = float( GetBValue( m_crVapour ) ) / 255.0f;

	gLightPrefs.vShadowColor.x = float( GetRValue( m_crShadow ) ) / 255.0f;
	gLightPrefs.vShadowColor.y = float( GetGValue( m_crShadow ) ) / 255.0f;
	gLightPrefs.vShadowColor.z = float( GetBValue( m_crShadow ) ) / 255.0f;

	gLightPrefs.vBackColor.x = float( GetRValue( m_crBack ) ) / 255.0f;
	gLightPrefs.vBackColor.y = float( GetGValue( m_crBack ) ) / 255.0f;
	gLightPrefs.vBackColor.z = float( GetBValue( m_crBack ) ) / 255.0f;

	gLightPrefs.vGroundAmbientColor.x = float( GetRValue( m_crGroundAmbient ) ) / 255.0f;
	gLightPrefs.vGroundAmbientColor.y = float( GetGValue( m_crGroundAmbient ) ) / 255.0f;
	gLightPrefs.vGroundAmbientColor.z = float( GetBValue( m_crGroundAmbient ) ) / 255.0f;

	gLightPrefs.ptOrigin.x = m_orgX;
	gLightPrefs.ptOrigin.y = m_orgY;
	gLightPrefs.ptOrigin.z = m_orgZ;

	gLightPrefs.ptLight.x = m_dirX;
	gLightPrefs.ptLight.y = m_dirY;
	gLightPrefs.ptLight.z = m_dirZ;

	gLightPrefs.fFogStart = m_fFogStart;
	gLightPrefs.fFogDistance = m_fFogDistance;
	gLightPrefs.fVapourDensity = m_fVapourDensity;
	gLightPrefs.fVapourHeight = m_fVapourHeight;
	gLightPrefs.nSkyID = m_nSkyID;
	gLightPrefs.fVapourNoiseParam = m_fVNP;
	gLightPrefs.fVapourSpeed = m_fVS;
	gLightPrefs.fVapourSwitchTime = m_fVST;
	
	GetUserSettingsSetup().SetParam( ME_SWITCH_SYSCOLORS, m_bSwitchSysColors );
	if ( m_bSwitchSysColors )
		SetupSysColors();
	else
		RestoreSysColors();
	CPropertyPage::OnOK();
}

BOOL CLightPrefsDlg::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	CVec3 cra = 255.0f * gLightPrefs.vAmbientColor;
	CVec3 crl = 255.0f * gLightPrefs.vLightColor;
	CVec3 crf = 255.0f * gLightPrefs.vFogColor;
	CVec3 crg = 255.0f * gLightPrefs.vGlossColor;
	CVec3 crv = 255.0f * gLightPrefs.vVapourColor;
	CVec3 crsh = 255.0f * gLightPrefs.vShadowColor;
	CVec3 crbk = 255.0f * gLightPrefs.vBackColor;
	CVec3 crgr = 255.0f * gLightPrefs.vGroundAmbientColor;
	m_crAmbient = RGB( cra.x, cra.y, cra.z );	
	m_crLight   = RGB( crl.x, crl.y, crl.z );
	m_crFog     = RGB( crf.x, crf.y, crf.z );
	m_crGloss   = RGB( crg.x, crg.y, crg.z );
	m_crVapour  = RGB( crv.x, crv.y, crv.z );
	m_crShadow  = RGB( crsh.x, crsh.y, crsh.z );
	m_crBack    = RGB( crbk.x, crbk.y, crbk.z );
	m_crGroundAmbient = RGB( crgr.x, crgr.y, crgr.z );
	
	m_orgX = gLightPrefs.ptOrigin.x;
	m_orgY = gLightPrefs.ptOrigin.y;
	m_orgZ = gLightPrefs.ptOrigin.z;

	m_dirX = gLightPrefs.ptLight.x;
	m_dirY = gLightPrefs.ptLight.y;
	m_dirZ = gLightPrefs.ptLight.z;
	
	m_fFogStart = gLightPrefs.fFogStart;
	m_fFogDistance = gLightPrefs.fFogDistance;
	m_fVapourHeight = gLightPrefs.fVapourHeight;
	m_fVapourDensity = gLightPrefs.fVapourDensity;
	
	m_ambientColor.cr = m_crAmbient;
	m_lightColor.cr = m_crLight;
	m_fogColor.cr = m_crFog;
	m_glossColor.cr = m_crGloss;
	m_vapourColor.cr = m_crVapour;
	m_nSkyID = gLightPrefs.nSkyID;

	m_bSwitchSysColors = GetUserSettings().GetParam( ME_SWITCH_SYSCOLORS );

	UpdateData( FALSE );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CLightPrefsDlg::OnAmbientColor() 
{
	CColorDialog dlg( m_crAmbient );

	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	m_crAmbient = dlg.GetColor();
	BYTE ambR = GetRValue( m_crAmbient );
	BYTE ambG = GetGValue( m_crAmbient );
	BYTE ambB = GetBValue( m_crAmbient );
	m_crLight = RGB( Max( GetRValue(m_crLight), ambR ), Max( GetGValue(m_crLight), ambG ), Max( GetBValue(m_crLight), ambB ) );
	
	m_ambientColor.cr = m_crAmbient;
	m_lightColor.cr = m_crLight;
	UpdateData( FALSE );
	Invalidate();
}

void CLightPrefsDlg::OnLightColor() 
{
	CColorDialog dlg( m_crLight );
	
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	m_crLight = dlg.GetColor();
	BYTE lightR = GetRValue( m_crLight );
	BYTE lightG = GetGValue( m_crLight );
	BYTE lightB = GetBValue( m_crLight );
	BYTE ar = Min( lightR, GetRValue(m_crAmbient) );
	BYTE ag = Min( lightG, GetGValue(m_crAmbient) );
	BYTE ab = Min( lightB, GetBValue(m_crAmbient) );

	m_crAmbient = RGB( ar, ag, ab );
	
	m_lightColor.cr = m_crLight;
	m_ambientColor.cr = m_crAmbient;
	UpdateData( FALSE );
	Invalidate();
}

void CLightPrefsDlg::OnFogColor() 
{	
	CColorDialog dlg( m_crFog );
	
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	m_crFog = dlg.GetColor();	
	m_fogColor.cr = m_crFog;
	UpdateData( FALSE );
	Invalidate();
}

void CLightPrefsDlg::OnVapourColor() 
{
	CColorDialog dlg( m_crVapour );
	
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	m_crVapour = dlg.GetColor();
	m_vapourColor.cr = m_crVapour;
	UpdateData( FALSE );
	Invalidate();	
}

void CLightPrefsDlg::OnGlossColor() 
{
	CColorDialog dlg( m_crGloss );
	
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	m_crGloss = dlg.GetColor();
	m_glossColor.cr = m_crGloss;
	UpdateData( FALSE );
	Invalidate();
}

void CLightPrefsDlg::OnLightLoadpreset() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_AMBIENTLIGHTS_TREE );
	if ( !pTree )
		return;
	
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree, nItemID;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItemID );
	if ( -1 == nItemID )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItemID );
	if ( !pProps )
		return;
	CPropMap::const_iterator il =pProps->find( "LightColor" );
	CPropMap::const_iterator ia =pProps->find( "AmbientColor" );
	CPropMap::const_iterator ifog =pProps->find( "FogColor" );
	CPropMap::const_iterator igloss =pProps->find( "GlossColor" );
	CPropMap::const_iterator ivcr =pProps->find( "VapourColor" );
	CPropMap::const_iterator ip =pProps->find( "Pitch" );
	CPropMap::const_iterator iy =pProps->find( "Yaw" );
	CPropMap::const_iterator ifstart =pProps->find( "FogStartDistance" );
	CPropMap::const_iterator ifdist =pProps->find( "FogDistance" );
	CPropMap::const_iterator ivheight =pProps->find( "VapourHeight" );
	CPropMap::const_iterator ivdensity =pProps->find( "VapourDensity" );
	CPropMap::const_iterator isky =pProps->find( "SkyID" );
	CPropMap::const_iterator ishcr =pProps->find( "ShadowColor" );
	CPropMap::const_iterator ivnp = pProps->find( "VapourNoiseParam" );
	CPropMap::const_iterator ivs = pProps->find( "VapourSpeed" );
	CPropMap::const_iterator ivst = pProps->find( "VapourSwitchTime" );
	CPropMap::const_iterator ibkcr =pProps->find( "BackLightColor" );
	CPropMap::const_iterator igrcr =pProps->find( "GroundAmbientColor" );
	CPropMap::const_iterator e =pProps->end();
	if ( e == il || ia == e || ip == e || iy == e || ifog == e || igloss == e 
			|| ivcr == e || ifdist == e || ivheight == e || ivdensity == e || ishcr == e 
			|| ivnp == e || ivs == e || ivst == e || ibkcr == e || igrcr == e )
		return;
	m_crLight = (int)il->second->GetValue();
	m_crAmbient = (int)ia->second->GetValue();
	m_crFog = (int)ifog->second->GetValue();
	m_crGloss = (int)igloss->second->GetValue();
	m_crVapour = (int)ivcr->second->GetValue();
	CVariant var = isky->second->GetValue();
	m_nSkyID = var.GetType() == CVariant::VT_NULL ? -1 : var;
	m_lightColor.cr = m_crLight;
	m_ambientColor.cr = m_crAmbient;
	m_fogColor.cr = m_crFog;
	m_glossColor.cr = m_crGloss;
	m_vapourColor.cr = m_crVapour;
	m_crShadow = (int)ishcr->second->GetValue();
	m_crBack = (int)ibkcr->second->GetValue();
	m_crGroundAmbient = (int)igrcr->second->GetValue();

	m_fFogStart = ifstart->second->GetValue();
	m_fFogDistance = ifdist->second->GetValue();
	m_fVapourHeight = ivheight->second->GetValue();
	m_fVapourDensity = ivdensity->second->GetValue();
	m_fVNP = ivnp->second->GetValue();
	m_fVS = ivs->second->GetValue();
	m_fVST = ivst->second->GetValue();

	float fYaw   = iy->second->GetValue();
	float fPitch = ip->second->GetValue();

	float fHor = sin( ToRadian( fPitch ) );
	m_dirZ = -cos( ToRadian( fPitch ) );
	m_dirX = fHor * cos( ToRadian( fYaw ) );
	m_dirY = fHor * sin( ToRadian( fYaw ) );
	UpdateData( FALSE );
	Invalidate();
	pTree->pItemsTree->ReleasePropList( pProps );
}

const float fToDegrees = 180.0 / PI;

void CLightPrefsDlg::OnLightSave() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_AMBIENTLIGHTS_TREE );
	if ( !pTree )
		return;
	
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree, nItemID;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItemID );
	if ( -1 == nItemID )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItemID );
	if ( !pProps )
		return;
	UpdateData();
	CPropMap::const_iterator il =pProps->find( "LightColor" );
	CPropMap::const_iterator ia =pProps->find( "AmbientColor" );
	CPropMap::const_iterator ip =pProps->find( "Pitch" );
	CPropMap::const_iterator iy =pProps->find( "Yaw" );
	CPropMap::const_iterator e =pProps->end();

	CPropMap::const_iterator ifog =pProps->find( "FogColor" );
	CPropMap::const_iterator igloss =pProps->find( "GlossColor" );
	CPropMap::const_iterator ivcr =pProps->find( "VapourColor" );
	CPropMap::const_iterator ifstart =pProps->find( "FogStartDistance" );
	CPropMap::const_iterator ifdist =pProps->find( "FogDistance" );
	CPropMap::const_iterator ivheight =pProps->find( "VapourHeight" );
	CPropMap::const_iterator ivdensity =pProps->find( "VapourDensity" );
	CPropMap::const_iterator isky =pProps->find( "SkyID" );
	CPropMap::const_iterator ishcr =pProps->find( "ShadowColor" );
	CPropMap::const_iterator ibkcr =pProps->find( "BackLightColor" );
	if ( e == il || ia == e || ip == e || iy == e || ifog == e || igloss == e 
			|| ivcr == e || ifdist == e || ivheight == e || ivdensity == e 
			|| ifstart == e || ishcr == e || ibkcr == e )
		return;

	il->second->SetValue( (int)m_lightColor.cr );
	ia->second->SetValue( (int)m_ambientColor.cr );
	ifog->second->SetValue( (int)m_fogColor.cr );
	igloss->second->SetValue( (int)m_glossColor.cr );
	ivcr->second->SetValue( (int)m_vapourColor.cr );
	isky->second->SetValue( m_nSkyID );
	CVec3 ptDir( m_dirX, m_dirY, m_dirZ );
	Normalize( &ptDir );
	float fPitch = fToDegrees * acos( -ptDir.z );
	float fYaw = fabs( ptDir.x ) < 1e-3 ? ( fabs( ptDir.y ) < 1e-3 ? 0 : 90 ) : fToDegrees * atan( ptDir.y / ptDir.x );
	ip->second->SetValue( fPitch );
	iy->second->SetValue( fYaw );

	ifdist->second->SetValue( m_fFogDistance );
	ivheight->second->SetValue( m_fVapourHeight );
	ivdensity->second->SetValue( m_fVapourDensity );
	ifstart->second->SetValue( m_fFogStart );
}

BEGIN_MESSAGE_MAP(CColorBtn, CButton)
//{{AFX_MSG_MAP(CColorBtn)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CColorBtn::DrawItem( LPDRAWITEMSTRUCT pdraw )
{
	CDC *pDC = CDC::FromHandle( pdraw->hDC );
	CRect r = pdraw->rcItem;
	r.DeflateRect( 2, 2, 2, 2 );
	pDC->FillSolidRect( &r, cr );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTemplateViewPrefsDlg property page

IMPLEMENT_DYNCREATE(CTemplateViewPrefsDlg, CPropertyPage)

CTemplateViewPrefsDlg::CTemplateViewPrefsDlg() : CPropertyPage(CTemplateViewPrefsDlg::IDD)
{
	//{{AFX_DATA_INIT(CTemplateViewPrefsDlg)
	m_nDepth = 0;
	//}}AFX_DATA_INIT
}

CTemplateViewPrefsDlg::~CTemplateViewPrefsDlg()
{
}

void CTemplateViewPrefsDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTemplateViewPrefsDlg)
	DDX_Text(pDX, IDC_TEMPLUNWINDING_DEPTH, m_nDepth);
	DDV_MinMaxInt(pDX, m_nDepth, 0, 100);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTemplateViewPrefsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CTemplateViewPrefsDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTemplateViewPrefsDlg message handlers

BOOL CTemplateViewPrefsDlg::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	m_nDepth = theApp.GetTemplateMaxDepth();
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTemplateViewPrefsDlg::OnOK() 
{
	theApp.SetTemplateMaxDepth( m_nDepth );
	CPropertyPage::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMapbuildPrefsDlg dialog

CMapbuildPrefsDlg::CMapbuildPrefsDlg() : CPropertyPage(CMapbuildPrefsDlg::IDD)
, m_bShow2DView(FALSE)
, m_fSubTemplateAlpha(0)
, m_bShowAISpheres(FALSE)
, m_bInstantTerrain(FALSE)
, m_nGridInterval(0)
, m_bGridSnap(FALSE)
, m_fKbdMovingSpeed(0)
, m_bShowHoles(FALSE)
{
	//{{AFX_DATA_INIT(CMapbuildPrefsDlg)
	m_szArguments = _T( gszBuildParams.c_str() );
	m_nDepth = 0;
	//}}AFX_DATA_INIT
}


void CMapbuildPrefsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMapbuildPrefsDlg)
	DDX_Text(pDX, IDC_MAPBUILD_PREFS_ARGUMENTS, m_szArguments);
	DDX_Text(pDX, IDC_TEMPLUNWINDING_DEPTH, m_nDepth);
	DDV_MinMaxInt(pDX, m_nDepth, 0, 100);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_SHOW2DVIEW, m_bShow2DView);
	DDX_Text(pDX, IDC_STALPHA, m_fSubTemplateAlpha);
	DDX_Check(pDX, IDC_SHOWAI_SPHERES, m_bShowAISpheres);
	DDX_Check(pDX, IDC_INSTANTTERRAIN, m_bInstantTerrain);
	//DDX_CBIndex(pDX, IDC_GRID_INTERVAL, m_nGridInterval);
	DDX_Control(pDX, IDC_GRID_INTERVAL, m_ctrlGridInterval);
	DDX_Check(pDX, IDC_GRID_SNAP, m_bGridSnap);
	DDX_Text(pDX, IDC_MOVING_SPEED, m_fKbdMovingSpeed);
	DDV_MinMaxFloat(pDX, m_fKbdMovingSpeed, 0.01f, 50);
	DDX_Check(pDX, IDC_TERRAIN_SHOWHOLES, m_bShowHoles);
}


BEGIN_MESSAGE_MAP(CMapbuildPrefsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CMapbuildPrefsDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CMapbuildPrefsDlg::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	m_szArguments = gszBuildParams.c_str();
	m_nDepth = theApp.GetTemplateMaxDepth();
	m_bShow2DView = gbLoad2DView;
	m_fSubTemplateAlpha = GetUserSettings().GetParam( ME_SUBTEMPLATE_ALPHA );
	m_bShowAISpheres = GetUserSettings().GetParam( ME_SHOW_AISPHERES );
	m_bInstantTerrain = GetUserSettings().GetParam( ME_INSTANT_TERRAIN );
	m_nGridInterval = GetUserSettings().GetParam( ME_GRID_INTERVAL );
	m_ctrlGridInterval.SelectString( -1, IToA( m_nGridInterval ).c_str() );
	m_bGridSnap = GetUserSettings().GetParam( ME_GRID_SNAP_2FLOOR );
	m_fKbdMovingSpeed = GetUserSettings().GetParam( ME_KBDMOVE_SPEED );
	m_bShowHoles = GetUserSettings().GetParam( ME_TERRAIN_SHOWHOLES );
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CMapbuildPrefsDlg::OnOK() 
{
	gszBuildParams = (LPCTSTR)m_szArguments;
	gbLoad2DView = m_bShow2DView;
	theApp.WriteProfileString( "", REG_MAPBUILD_ARG, m_szArguments );
	theApp.WriteProfileInt( "", REG_SHOW2DVIEW, m_bShow2DView );
	theApp.SetTemplateMaxDepth( m_nDepth );
	GetUserSettingsSetup().SetParam( ME_SUBTEMPLATE_ALPHA, m_fSubTemplateAlpha );
	GetUserSettingsSetup().SetParam( ME_SHOW_AISPHERES, m_bShowAISpheres );
	GetUserSettingsSetup().SetParam( ME_INSTANT_TERRAIN, m_bInstantTerrain );
	CString szSel;
	m_ctrlGridInterval.GetLBText( m_ctrlGridInterval.GetCurSel(), szSel );
	m_nGridInterval = atoi( szSel );
	if ( m_nGridInterval != GetUserSettings().GetParam( ME_GRID_INTERVAL ) )
		NInput::PostEvent( "update_grid" );
	GetUserSettingsSetup().SetParam( ME_GRID_INTERVAL, m_nGridInterval );
	GetUserSettingsSetup().SetParam( ME_GRID_SNAP_2FLOOR, m_bGridSnap );
	GetUserSettingsSetup().SetParam( ME_KBDMOVE_SPEED, m_fKbdMovingSpeed );
	GetUserSettingsSetup().SetParam( ME_TERRAIN_SHOWHOLES, m_bShowHoles );
	CPropertyPage::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMapbuildPrefsDlg message handlers
// D:\Home\A5\MapEdit\preferences.cpp : implementation file
//

// CRPGItemPrefs dialog

IMPLEMENT_DYNAMIC(CRPGItemPrefs, CPropertyPage)
CRPGItemPrefs::CRPGItemPrefs(CWnd* pParent /*=NULL*/)
	: CPropertyPage(CRPGItemPrefs::IDD)
	, m_bShowPersItems(FALSE)
	, m_bSynaxColouring(FALSE)
	, m_bCustomFrame(FALSE)
	, m_nFrameWidth(0)
	, m_nFrameHeight(0)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRPGItemPrefs::~CRPGItemPrefs()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemPrefs::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_SHOW_PERS_ITEMS, m_bShowPersItems);
	DDX_Check(pDX, IDC_SYNTAX_COLOURING, m_bSynaxColouring);
	DDX_Check(pDX, IDC_CUSTOMFRAME, m_bCustomFrame);
	DDX_Text(pDX, IDC_WIDTH, m_nFrameWidth);
	DDV_MinMaxInt(pDX, m_nFrameWidth, 0, 2000);
	DDX_Text(pDX, IDC_HEIGHT, m_nFrameHeight);
	DDV_MinMaxInt(pDX, m_nFrameHeight, 0, 2000);
	DDX_Control(pDX, IDC_WIDTH, m_ctrlWidth);
	DDX_Control(pDX, IDC_HEIGHT, m_ctrlHeight);
	DDX_Control(pDX, IDC_STATIC_W, m_staticW);
	DDX_Control(pDX, IDC_STATIC_H, m_staticH);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemPrefs::SetCheck( int nControlID )
{
	CButton *pB = (CButton*)GetDlgItem( nControlID );
	if ( pB )
		pB->SetCheck( 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CRPGItemPrefs::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	int nItemCtrlID;
	int nPersCtrlID;
	switch (geRPGInventoryCamera )
	{
		case CAM_INVENTORY:
			nItemCtrlID = IDC_INVENTORY_CAMERA;
			break;
		case CAM_SLOT:
			nItemCtrlID = IDC_SLOT_CAMERA;
			break;
		case CAM_AMMO:
			nItemCtrlID = IDC_AMMO_CAMERA;
			break;
	}
	if ( GetUserSettings().GetParam( ME_PERS_FACEGENCAMERA ) )
		nPersCtrlID = IDC_PERS_FACEGENCAMERA;
	else
		nPersCtrlID = IDC_PERS_GENERALCAMERA;
	SetCheck( nItemCtrlID );
	SetCheck( nPersCtrlID );
	m_bShowPersItems = gbShowPersItems;
	m_bSynaxColouring = GetUserSettings().GetParam( ME_SCRIPT_SYNAXCOLORING );
	m_bCustomFrame = GetUserSettings().GetParam( ME_SHOW_CUSTOMFRAME );
	m_nFrameWidth = GetUserSettings().GetParam( ME_CUSTOMFRAME_WIDTH );
	m_nFrameHeight = GetUserSettings().GetParam( ME_CUSTOMFRAME_HEIGHT );
	UpdateData( FALSE );
	UpdateCustomFrame();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemPrefs::OnOK()
{
	CButton* pInv = (CButton*)GetDlgItem( IDC_INVENTORY_CAMERA );
	CButton* pSlot = (CButton*)GetDlgItem( IDC_SLOT_CAMERA );
	CButton* pFaceGen = (CButton*)GetDlgItem( IDC_PERS_FACEGENCAMERA );
	//
	if ( pInv && pInv->GetCheck() == 1 )
		geRPGInventoryCamera = CAM_INVENTORY;
	else if ( pSlot && pSlot->GetCheck() == 1 )
		geRPGInventoryCamera = CAM_SLOT;
	else
		geRPGInventoryCamera = CAM_AMMO;
	//
	GetUserSettingsSetup().SetParam( ME_PERS_FACEGENCAMERA, pFaceGen && pFaceGen->GetCheck() == 1 );
	theApp.WriteProfileInt( "", "RPGInventoryCamera", geRPGInventoryCamera );
	int nTree, nID, nVar;
	theApp.GetActiveItem( &nTree, &nID, &nVar );
	if ( IDC_RPG_ITEMS_TREE == nTree )
		theApp.SetActiveItem( nTree, nID, nVar, false );
	//
	gbShowPersItems = m_bShowPersItems;
	theApp.WriteProfileInt( "", "RPGShowPersItems", gbShowPersItems );
	GetUserSettingsSetup().SetParam( ME_SCRIPT_SYNAXCOLORING, m_bSynaxColouring );
	GetUserSettingsSetup().SetParam( ME_SHOW_CUSTOMFRAME, m_bCustomFrame );
	GetUserSettingsSetup().SetParam( ME_CUSTOMFRAME_WIDTH, m_nFrameWidth );
	GetUserSettingsSetup().SetParam( ME_CUSTOMFRAME_HEIGHT, m_nFrameHeight );
	CPropertyPage::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CRPGItemPrefs, CDialog)
	ON_BN_CLICKED(IDC_CUSTOMFRAME, OnBnClickedCustomframe)
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemPrefs::OnBnClickedCustomframe()
{
	UpdateCustomFrame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRPGItemPrefs::UpdateCustomFrame()
{
	UpdateData();
	if ( m_bCustomFrame )
	{
		m_ctrlWidth.EnableWindow( TRUE );
		m_ctrlHeight.EnableWindow( TRUE );
		m_staticW.EnableWindow( TRUE );
		m_staticH.EnableWindow( TRUE );
	}
	else
	{
		m_ctrlWidth.EnableWindow( FALSE );
		m_ctrlHeight.EnableWindow( FALSE );
		m_staticW.EnableWindow( FALSE );
		m_staticH.EnableWindow( FALSE );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
