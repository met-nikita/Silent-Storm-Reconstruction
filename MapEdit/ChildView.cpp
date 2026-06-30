// ChildView.cpp : implementation of the CChildView class
//
#include "stdafx.h"
#include "MapEdit.h"
#include "ChildView.h"
#include "placement.h"
#include "dbDefs.h"
#include "PaintBar.h"
#include "LayerList.h"
#include "Layers.h"
#include "WallsLayer.h"
#include "FloorLayer.h"
#include "RectsLayer.h"
#include "TerrainLayer.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\TerrainInfo.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\iMain.h"
#include "..\Image\Image.h"
#include "..\Image\ImageTga.h"
#include "iWysiwyg.h"
#include "ItemsMgr.h"
#include "CtrlObjectInspector.h"
#include "RoomsLayer.h"
#include "CellarLayer.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataMap.h"

const float fMaxZoom = 7.0f;
const COLORREF GRID_BACKGROUND = RGB( 255, 255, 255 );

const int SELREFRESH_TIMER_ID = 2;
const int SELREFRESH_INTERVAL = 50;

static CLayerList *pLayerList = 0;
const IUserSettings& GetUserSettings() { return *pLayerList; }
IUserSettingsSetup& GetUserSettingsSetup() { return *pLayerList; }

int GetLayerGroup( int nLayerID, const vector<NBuilding::SLayerGroup> &groups )
{
	for ( int i = 0; i < groups.size(); ++i )
	{
		if ( find( groups[i].layers.begin(), groups[i].layers.end(), nLayerID ) != groups[i].layers.end() )
			return i;
	}
	return -1;
}

inline void CChildView::TemplateToScreen( CRect &r )
{
	int nDelta = fZoom * nSpacing;
  r.top    = nHeight - r.top * nDelta;
  r.bottom = nHeight - r.bottom * nDelta;
  r.left  *= nDelta;
  r.right *= nDelta;
}

inline void CChildView::ScreenToTemplate( CRect &r )
{
	int nDelta = fZoom * nSpacing;
  r.top    = (nHeight - r.top) / nDelta;
  r.bottom = (nHeight - r.bottom) / nDelta;
  r.left  /= nDelta;
  r.right /= nDelta;
}

float CChildView::ScreenToTemplate( int size )
{
	int nDelta = fZoom * nSpacing;
	return float( size ) / nDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int MODEL_PROP_ID			= 10;
const int POWER_PROP_ID			= 11;
const int ROOM_PROP_ID			= 12;
const int DZ_PROP_ID				= 13;
const int ROTATION_PROP_ID	= 14;
const int SCALEX_PROP_ID		= 15;
const int SCALEY_PROP_ID		= 16;
const int SCALEZ_PROP_ID		= 17;
const string OUTDOOR_ROOM = "Outdoor";
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectProp : public CFinProp
{
	OBJECT_BASIC_METHODS(CObjectProp);
private:
	CChildView *pView;
	int nTreeID;
	int nObjID;
  
public:
	CObjectProp() { ASSERT( 0 ); }
  CObjectProp( CChildView *_pView, const string &szName, int nPropID, int nTreeId, int nObjId, int nType, int nViewType = DT_DEC )
		: CFinProp( szName, nPropID, nType, nViewType ), 
			pView( _pView ), nTreeID( nTreeId ), nObjID( nObjId ) {}

	void SetValue( const CVariant &value, bool bModified = true ) const
	{
		if ( bModified )
		{
			switch ( GetID() )
			{
			case ROOM_PROP_ID:
				{
					int nRoom = OUTDOOR_ROOM == (string)value ? 0 : value;
					if ( !pView->SetContainerRoom( nObjID, nRoom ) )
						return;
					break;
				}
			case MODEL_PROP_ID:
				if ( !pView->ChangeObject( nTreeID, nObjID, value ) )
					return;
				break;
			case DZ_PROP_ID:
				if ( !pView->SetObjectDZ( nTreeID, nObjID, value ) )
					return;
				break;
			case ROTATION_PROP_ID:
				if ( !pView->SetObjectRotation( nTreeID, nObjID, value ) )
					return;
				break;
			case POWER_PROP_ID:
				if ( !pView->SetExplosionPower( nObjID, value ) )
					return;
				break;
			case SCALEX_PROP_ID:
				if ( !pView->SetScaleX( nObjID, value ) )
					return;
				break;
			case SCALEY_PROP_ID:
				if ( !pView->SetScaleY( nObjID, value ) )
					return;
				break;
			case SCALEZ_PROP_ID:
				if ( !pView->SetScaleZ( nObjID, value ) )
					return;
				break;
			}
		}
		CFinProp::SetValue( value, bModified );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef bool (CPlacement::*PLACEMENT_SET_VALUE)(CVariant);
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacementProp : public CFinProp
{
	OBJECT_BASIC_METHODS(CPlacementProp);
private:
	CPlacement *pPlacement;
	PLACEMENT_SET_VALUE pCallback;
  
public:
	CPlacementProp() { ASSERT( 0 ); }
  CPlacementProp( CPlacement *pPl, PLACEMENT_SET_VALUE pCB, const string &szName, int nPropID, int nType, int nViewType )
		: CFinProp( szName, nPropID, nType, nViewType ), pPlacement( pPl ), pCallback( pCB ) {}
	
	void SetValue( const CVariant &value, bool bModified = true ) const
	{
		if ( bModified && !(pPlacement->*pCallback)( value ) )
		{
			CFinProp::SetValue( GetValue(), bModified ); // старое значение
			return;
		}
		CFinProp::SetValue( value, bModified );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddLayerDlg dialog
CAddLayerDlg::CAddLayerDlg( CWnd* pParent /*=NULL*/)
: CDialog(CAddLayerDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAddLayerDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
void CAddLayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAddLayerDlg)
	DDX_Control(pDX, IDC_LAYER_LIST, m_list);
	//}}AFX_DATA_MAP
}
BEGIN_MESSAGE_MAP(CAddLayerDlg, CDialog)
//{{AFX_MSG_MAP(CAddLayerDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CAddLayerDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	for ( int i = 0; i < items.size(); ++i )
	{
		int ind = m_list.AddString( items[i].first.c_str() );
		m_list.SetItemData( ind, i);
	}
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAddLayerDlg::OnOK() 
{
	UpdateData();
	for ( int i = 0; i < m_list.GetCount(); ++i )
		if ( 1 != m_list.GetCheck( i ) )
			items[m_list.GetItemData( i )].second = 0;
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChildView

CChildView::CChildView() 
	: pCurPlacement( 0 ), 
		pPntbar( new CPaintBar ),
		pSubTemplL( new CRectsLayer( LID_SUBTEMPLATES, "Sub templates" ) ),
		pUnitsL( new CRectsLayer( LID_UNITS, "Units" ) ),
		pObjsL( new CRectsLayer( LID_OBJECTS, "Objects" ) ),
		pWallsL( new CWallsLayer ),
		pTilesL( new CTilesLayer ),
		pHeightsL( new CHeightsLayer( LID_HEIGHTS, "Height map" ) ),
		pAlphaL( new CAlphaLayer ),
		pCellarL( new CCellarLayer ),
		pHolesL( new CHeightsLayer( LID_HOLES, "Terrain holes" ) )
{
	if ( !pLayerList )
		pLayerList = new CLayerList;
	pLayers = pLayerList;
	pDCBuf = 0;
	nFontH = 14;
	nGridSize = 5;
	crGrid = RGB( 180, 210, 190 );
	crGridThick = RGB( 0.6*GetRValue( crGrid ), 0.6*GetGValue( crGrid ), 0.6*GetBValue( crGrid ) );
	//penWidth = PM_PEN_W1;
	bTerrChanged = false;
	m_rSelection.SetRectEmpty();
	bZooming = false;
	dropTarget.pView = this;
}

CChildView::~CChildView()
{
	if ( pPntbar ) delete pPntbar;
	if ( pLayerList ) delete pLayerList;
	if ( pSubTemplL ) delete pSubTemplL;
	if ( pUnitsL ) delete pUnitsL;
	if ( pObjsL ) delete pObjsL;
	if ( pWallsL ) delete pWallsL;
	if ( pTilesL ) delete pTilesL;
	if ( pHeightsL ) delete pHeightsL;
	if ( pAlphaL ) delete pAlphaL;
	if ( pCellarL ) delete pCellarL;
	if ( pHolesL ) delete pHolesL;
		
	pLayerList = 0;
	const_cast<CPaintBar*>( pPntbar ) = 0;  
	DeleteLayers();
}


BEGIN_MESSAGE_MAP(CChildView,CWnd )
	//{{AFX_MSG_MAP(CChildView)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETCURSOR()
	ON_WM_CREATE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_COMMAND(ID_FILE_EXPORTTEMPLATE, OnFileExportTemplate)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CChildView message handlers
int CChildView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd ::OnCreate(lpCreateStruct) == -1)
		return -1;
	dropTarget.Register( this );
	//
	SECMDIFrameWnd *pMainFrame = dynamic_cast<SECMDIFrameWnd*>( theApp.GetMainWnd() );
	if ( !pMainFrame )
		return -1;
	DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_SIZE_DYNAMIC;
//	if (!pPntbar->CreateEx( CBRS_EX_COOLBORDERS|CBRS_EX_GRIPPER, pMainFrame, dwStyle, 0xfffe, "PaintBar" ) )
//	{
//		TRACE0("Failed to create paint toolbar\n");
//		return -1;      // fail to create
//	}
//	pPntbar->EnableDocking( CBRS_ALIGN_ANY );
//	pMainFrame->DockControlBarEx( pPntbar, AFX_IDW_DOCKBAR_LEFT, 1, 1 );
	//
  DWORD dwStyleEx = CBRS_EX_COOL|CBRS_EX_BORDERSPACE|CBRS_EX_STDCONTEXTMENU;	
	if (!pLayers->Create( pMainFrame, "Layers", dwStyle, dwStyleEx, 0xffff ) )
	{
		TRACE0("Failed to create layer control\n");
		return -1;      // fail to create
	}
	pLayers->EnableDocking( CBRS_ALIGN_ANY );
	pMainFrame->DockControlBarEx( pLayers, AFX_IDW_DOCKBAR_RIGHT );
	pLayers->AddLayer( pSubTemplL );
	pLayers->AddLayer( pUnitsL );
	pLayers->AddLayer( pObjsL );
	pLayers->AddLayer( pWallsL );
	pLayers->AddLayer( pTilesL );
	pLayers->AddLayer( pHeightsL );
	pLayers->AddLayer( pHolesL );
	pLayers->AddLayer( pAlphaL );
	pLayers->AddLayer( pCellarL );
	pLayers->AddNotifyWnd( this );
	pSubTemplL->SetVisible();
	pUnitsL->SetVisible();
	pObjsL->SetVisible();
	pWallsL->SetVisible();
	pLayers->Activate( pObjsL->GetLayerID() );
	layers.push_back( SLayer( "Sub templates", LID_SUBTEMPLATES, true, true ));
	layers.push_back( SLayer( "Units", LID_UNITS, true, true ));
	layers.push_back( SLayer( "Objects", LID_OBJECTS, true, true ));
	layers.push_back( SLayer( "Walls", LID_WALLS, true, true ));
	layers.push_back( SLayer( "Cellar", LID_CELLAR, true ));
	layers.push_back( SLayer( "Floors", LID_FLOORS, true ));
	layers.push_back( SLayer( "Intermediate floors", LID_FLOORS_INTERM, true ));
	layers.push_back( SLayer( "Solids", LID_SOLIDS, false ));
	layers.push_back( SLayer( "Intermediate solids", LID_SOLIDS_INTERM, false ));
	layers.push_back( SLayer( "Texture", LID_TILES, true ));
	layers.push_back( SLayer( "Height map", LID_HEIGHTS, true ));
	layers.push_back( SLayer( "Terrain holes", LID_HOLES, true ));
	layers.push_back( SLayer( "Alpha", LID_ALPHA, true ));
	layers.push_back( SLayer( "Rooms", LID_ROOMS, true ) );
	layers.push_back( SLayer( "Grass", LID_GRASS, false ) );
	layers.push_back( SLayer( "Terrain color map", LID_TERRCOLOR, true ) );
	layers.push_back( SLayer( "Terrain spots", LID_SPOTS, false ) );
	//

	m_hArrow = theApp.LoadStandardCursor( IDC_ARROW );
	m_hHand  = theApp.LoadCursor( IDC_HANDTOOL );
	m_hZoom  = theApp.LoadCursor( IDC_ZOOM );
	m_hSelection = theApp.LoadCursor( IDC_SELECTION );
	m_hErase = theApp.LoadCursor( IDC_ERASE );
  CPaintDC dc(this);
	
  SetSpacing( 24 );
	SetZoom( 1 );

	// Установка таймера по которому обнавляется внешний вид поселекченных итемов
  SetTimer( SELREFRESH_TIMER_ID, SELREFRESH_INTERVAL, 0 );
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS|CS_BYTEALIGNCLIENT, 
		::LoadCursor(NULL, IDC_ARROW), /*HBRUSH(COLOR_WINDOW+1)*/0, NULL);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
  CDC dcBuf;
  CBitmap bmp;
  CRect r;

  GetClientRect( &r );
  bmp.CreateCompatibleBitmap( &dc, r.Width(), r.Height() );
  dcBuf.CreateCompatibleDC( &dc );
	CBitmap *pOld = dcBuf.SelectObject( &bmp );
  dcBuf.FillSolidRect( 0, 0, r.Width(), r.Height(), RGB( 180, 180, 180 )/*GetSysColor( COLOR_WINDOW )*/ );

	dcBuf.SetBkMode(TRANSPARENT);

	{
		CRect r;
		CPoint pt;
		GetGridPlacement( &r, &pt, CSize( GetSpacing(), GetSpacing() ) );	
		dcBuf.FillSolidRect( &r, GRID_BACKGROUND );	
	}
	//
	pDCBuf = &dcBuf;
	pLayers->Paint( this );
	DrawLineGrid( &dcBuf );
	pDCBuf = 0;
	//
	// рисуем текущий селекшен
	if ( !m_rSelection.IsRectEmpty() )
	{
		CPen pen( PS_DOT, 1, RGB( 0, 0, 0 ) );
		CPen *pOldPen = dcBuf.SelectObject( &pen );
		CBrush brush;
		brush.CreateHatchBrush( HS_FDIAGONAL, RGB( 200, 200, 200 ) );
		CBrush *pOldBrush = dcBuf.SelectObject( &brush );
		int oldMode = dcBuf.SetBkMode(TRANSPARENT);
		
		dcBuf.Rectangle( &m_rSelection );
		dcBuf.SelectObject( pOldPen );
		dcBuf.SelectObject( pOldBrush );
		dcBuf.SetBkMode( oldMode );
		brush.DeleteObject();
	}
	//
  dc.BitBlt( 0, 0, r.Width(), r.Height(), &dcBuf, 0, 0, SRCCOPY );
  dcBuf.SelectObject( pOld );
  bmp.DeleteObject();
	rOldBrush.SetRectEmpty();
	// Do not call CWnd::OnPaint() for painting messages
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CChildView::GetGridPlacement( CRect *pRect, CPoint *pptOrig, const CSize &cellSize )
{
	pRect->SetRectEmpty();
	if ( !pCurPlacement )
		return;
  GetClientRect( pRect );
	CPoint pt( 0, pCurPlacement->GetHeight() );
	TemplateToScreen( &pt, pt );
	pRect->left = Max( 0l, pt.x );
	pRect->top  = Max( 0l, pt.y );
	pRect->right = Min( pRect->left + nWidth, pRect->right );
	pRect->bottom = Min( pRect->top + nHeight, pRect->bottom );
	pRect->NormalizeRect();
		
	pptOrig->x = leftTop.x % cellSize.cx;
	pptOrig->y = leftTop.y % cellSize.cy;
	pptOrig->x = Max( 0l, pptOrig->x );
	pptOrig->y = Max( 0l, pptOrig->y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetLayersPlacement( EFloorType type, int nLayerID )
{
	if ( !pCurPlacement )
		return;
	const int nLayers = pCurPlacement->GetFloorLayersNum( type );
	for ( int i = 0; i < nLayers; ++i )
	{
		CLayerCtrl *pCtrl = AddLayer( nLayerID, i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetPlacement( CPlacement *pPl, CPlacementCache *pCache )
{
	BeginWaitCursor();

	SaveData();
	const CPropMap *pProps = theApp.SetPropMap( 0 );
	ClearSelection();
	theApp.SetPropMap( pProps );
  pCurPlacement = pPl;
	pPlacementCache = pCache;

  leftTop = CPoint( 0, 0 );

  int oldW = nWidth;
  int oldH = nHeight;
	int nDeltaOld = nSpacing * fZoom;
	
  nWidth  = pCurPlacement ? nDeltaOld * pCurPlacement->GetWidth() : 0;
  nHeight = pCurPlacement ? nDeltaOld * pCurPlacement->GetHeight() : 0;
	Centre( &leftTop );
  SetupScrolls();
	
	SetupLayerList();
	//ResetPropList();
	EndWaitCursor();
  Invalidate();
	NInput::PostEvent( "update" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetupLayerList()
{
	if ( pCurPlacement )
		pCurPlacement->UpdateLayers();
	pLayers->Setup();
	pWallsL->SetPlacement( pCurPlacement );
	pCellarL->SetPlacement( pCurPlacement );
	pHeightsL->SetPlacement( pCurPlacement );
	pHolesL->SetPlacement( pCurPlacement );
	pAlphaL->SetPlacement( pCurPlacement );
	pSubTemplL->SetPlacement( pCurPlacement, pPlacementCache, this );
	pUnitsL->SetPlacement( pCurPlacement, pPlacementCache, this );
	pObjsL->SetPlacement( pCurPlacement, pPlacementCache, this );

	float fmin, fmax;
	const CArray2D<BYTE> *pColors = 0;
	const CArray2D<WORD> *pHeights = 0;
	const CArray2D<WORD> *pAlphas = 0;
	const CArray2D<DWORD> *pColormap = 0;
	const vector<STerrainHole> *pHoles = 0;
	vector<SGrassLayer>  *pGrass = 0;

	if ( pCurPlacement )
	{
		CMETerrainInfo *pTerr = pCurPlacement->GetTerrainInfo();
		if ( pTerr )
		{
			int nX = pCurPlacement->GetWidth() + 1;
			int nY = pCurPlacement->GetHeight() + 1;
			if ( pTerr->info.typeMap.GetXSize() == nX && pTerr->info.typeMap.GetYSize() == nY )
				pColors = &pTerr->info.typeMap;
			if ( pTerr->info.heightMap.GetXSize() == nX && pTerr->info.heightMap.GetYSize() == nY )
			{
				pHeights = &pTerr->info.heightMap;
				fmin = pTerr->fMinH;
				fmax = pTerr->fMaxH;
			}
			if ( pTerr->alphaMap.GetXSize() == nX && pTerr->alphaMap.GetYSize() == nY )
				pAlphas = &pTerr->alphaMap;
			if ( pTerr->info.color.GetXSize() == nX && pTerr->info.color.GetYSize() == nY )
				pColormap = &pTerr->info.color;
			pHoles = &pTerr->info.holes;
			pGrass = &pTerr->info.grass;
			if ( pTerr->info.nMaxGrassLayerID <= 0 )
			{
				// старый формат слоев травы
				pTerr->info.nMaxGrassLayerID = 1;
				for ( int i = 0; i < pGrass->size(); ++i )
					(*pGrass)[i].nID = pTerr->info.nMaxGrassLayerID++;
				bTerrChanged = true;
			}
		}
	}
	pTilesL->SetImage(  pColors );
	pHeightsL->SetImage( pHeights, fmin, fmax );
	pAlphaL->SetImage( pAlphas );
	pHolesL->SetHoles( pHoles );

	// запоминаем старое состояние слоев
	vector< pair<int, bool> > lrOrder;
	pLayers->GetLayerOrder( &lrOrder );
	CLayerCtrl *pActiveLr = pLayers->GetActiveLayer();

	for ( int i = 0; i < layers.size(); ++i )
	{
		int ret = pLayers->IsLayerVisible( layers[i].nLayerID );
		if ( ret != -1 )
			layers[i].bLastVisible = ret;
	}
	pLayers->DeleteAllLayers();
	DeleteLayers(); // динамически созданные слои

	pLayers->AddLayer( pSubTemplL );
	pLayers->AddLayer( pUnitsL );
	pLayers->AddLayer( pObjsL );
	pLayers->AddLayer( pWallsL );
	SetLayersPlacement( FT_FLOOR, LID_FLOORS );
	SetLayersPlacement( FT_FLOOR_INTERMEDIATE, LID_FLOORS_INTERM );
	SetLayersPlacement( FT_SOLID_, LID_SOLIDS );
	SetLayersPlacement( FT_SOLID_INTERMEDIATE, LID_SOLIDS_INTERM );
	SetLayersPlacement( FT_ROOM, LID_ROOMS );
	pLayers->AddLayer( pCellarL );
	pLayers->Activate( pObjsL->GetLayerID() );

	if ( pColors )
		pLayers->AddLayer( pTilesL );
	if ( pHeights )
		pLayers->AddLayer( pHeightsL );
	if ( pHoles && !pHoles->empty() )
		pLayers->AddLayer( pHolesL );
	if ( pAlphas )
		pLayers->AddLayer( pAlphaL );
	if ( pGrass )
	{
		for ( int i = 0; i < pGrass->size(); ++i )
		{
			CLayerCtrl *pSecond;
			const SGrassLayer &grass = (*pGrass)[i];
			CGrassLayer *p = dynamic_cast<CGrassLayer*>( AddLayer( LID_GRASS, grass.nID, &pSecond ) );
			p->SetImage( &grass.grass, grass.fMaxDensity );
			p->SetGrassID( grass.nGrassID );
			CColorLayer *pcr = dynamic_cast<CColorLayer*>( pSecond );
			pcr->SetImage( &grass.grassColor );
		}
	}
	if ( pColormap )
	{
		CColorLayer* pCMLayer = dynamic_cast<CColorLayer*>( AddLayer( LID_TERRCOLOR ) );
		pCMLayer->SetImage( pColormap );
	}

	AddTerrainSpotLayers();

	//pLayers->SetLayerOrder( lrOrder );
	if ( pActiveLr )
		pLayers->Activate( pActiveLr->GetLayerID() );

	bTerrChanged = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	if ( !m_rSelection.IsRectEmpty() )
		ClearSelection();
	//
  if ( !pCurPlacement )
  {
    CWnd ::OnLButtonDown(nFlags, point);
    return;
  }
	//
	switch ( GetUserSettings().GetMode() )
	{
		case EM_SELECT:
		case EM_FILL:
		case EM_ERASE:
			{
				if ( nFlags & MK_CONTROL )
					break;
				CLayerCtrl *pLr = pLayers->GetActiveLayer();
				if ( pLr && pLr->IsVisible() )
					pLr->OnLButtonDown( nFlags, point, this );
			}
			break;		
		case EM_RECTANGULAR_SELECTION:
			break;
		case EM_ZOOM:
			TrackZoom();
			break;
		case EM_PAN:
			TrackPan();
			break;
	}

	CWnd ::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CChildView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if ( HTCLIENT != nHitTest || pWnd != this)
		return CWnd ::OnSetCursor(pWnd, nHitTest, message);

	// когда нажат CTRL, то mode = MODE_ZOOM
	short ctrl  = GetAsyncKeyState( VK_CONTROL );
	short shift = GetAsyncKeyState( VK_SHIFT );
	if ( 0x8000 & ctrl && !(0x8000 & shift) )
	{
		SetCursor( m_hZoom );
		return TRUE;
	}
	//
	switch ( GetUserSettings().GetMode() )
	{
		case EM_PAN:
			SetCursor( m_hHand );
			return TRUE;
		case EM_ZOOM:
			SetCursor( m_hZoom );
			return TRUE;
		case EM_RECTANGULAR_SELECTION:
			SetCursor( m_hSelection );
			return TRUE;
		case EM_ERASE:
			SetCursor( m_hErase );
			return TRUE;
	}
	//
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr )
		if ( pLr->OnSetCursor( pWnd, nHitTest, message, this ) )
			return TRUE;
	SetCursor( m_hArrow );
	
	return CWnd ::OnSetCursor(pWnd, nHitTest, message);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr )
		pLr->OnLButtonUp( nFlags, point, this );
	SaveData();
	CWnd ::OnLButtonUp(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnMouseMove(UINT nFlags, CPoint point) 
{
	// если двигаем мышь с нажатой левой кнопкой и нажатым CTRL - то zoom mode
	if ( (nFlags & MK_LBUTTON) && (nFlags & MK_CONTROL) && !(MK_SHIFT &nFlags) )
	{
		TrackZoom();
	}
	// если двигаем мышь с нажатой левой кнопкой - то это выделение
	else if ( EM_RECTANGULAR_SELECTION == GetUserSettings().GetMode() && (nFlags & MK_LBUTTON) && ! ((MK_CONTROL | MK_SHIFT) & nFlags) )
	{
		if ( !m_rSelection.IsRectEmpty() )
		{
			Invalidate( false );
			UpdateWindow();
		}
		TrackSelection();
	}
	else
	{
		CLayerCtrl *pLr = pLayers->GetActiveLayer();
		if ( pLr && pLr->IsVisible() )
			pLr->OnMouseMove( nFlags, point, this );
	}
	CWnd ::OnMouseMove(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::IsCursorInWindow()
{
  CPoint pt;
  CRect  r;

  GetCursorPos( &pt );
  GetWindowRect( &r );

  if ( pt.x > r.left && pt.x < r.right && 
       pt.y > r.top && pt.y < r.bottom )
    return true;
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DropTemplate( int templID )
{
	SetFocus();
	pSubTemplL->ClearSelection();
	SelectedItem( IDC_TEMPLATE_TREE, templID, pSubTemplL->DropObject( MO_TEMPLATE, templID, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DropUnit( int monsterID )
{
	SetFocus();
	pUnitsL->ClearSelection();
	SelectedItem( IDC_RPG_PERS_TREE, monsterID, pUnitsL->DropObject( MO_UNIT, monsterID, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DropObject( int modelID )
{
	SetFocus();
	pObjsL->ClearSelection();
	SelectedItem( IDC_OBJECTS_TREE, modelID, pObjsL->DropObject( MO_OBJECT, modelID, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DropContainer( int modelID )
{
//	pRectsL->ClearSelection();
//	SelectedItem( IDC_CONTAINERS_TREE, modelID, pRectsL->DropObject( MO_CONTAINER, modelID, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnRButtonDown(UINT nFlags, CPoint point) 
{
  if ( !pCurPlacement )
    return;
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr && pLr->IsVisible() )
		pLr->OnRButtonDown( nFlags, point, this );

	CWnd ::OnRButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DrawLineGrid( CDC *pDC )
{
	if ( !bGrid )
		return;
  int x, y;
  vector<CPoint> pts;
  vector<DWORD> count;
  CPen pen( PS_SOLID, 1, crGrid );
	int spacing = GetSpacing();
	CRect r;
	CPoint ptOrig;
  CPen *pOldPen = pDC->SelectObject( &pen );

	if ( spacing > 5 )
	{
		GetGridPlacement( &r, &ptOrig, CSize( spacing, spacing ) ); 
		if ( r.Width() < spacing || r.Height() < spacing )
			return;
		ptOrig.x = ptOrig.x == 0 ? spacing : ptOrig.x;
		ptOrig.y = ptOrig.y == 0 ? spacing : ptOrig.y;
		
		int cntx = 1 + r.Width() / spacing;
		int cnty = 1 + r.Height() / spacing;
		
		pts.reserve( 2 * cntx );
		for ( x = r.left + spacing - ptOrig.x; x <= r.right; x += spacing )
		{
			pts.push_back( CPoint( x, r.top ) );
			pts.push_back( CPoint( x, r.bottom ) );
		}
		count.resize( pts.size() / 2, 2 );
		pDC->PolyPolyline( &pts[0], &count[0], count.size() );
		//
		pts.clear();
		pts.reserve( 2 * cnty );
		for ( y = r.top + spacing - ptOrig.y; y <= r.bottom; y += spacing )
		{
			pts.push_back( CPoint( r.left, y ) );
			pts.push_back( CPoint( r.right, y ) );
		}
		count.resize( pts.size() / 2, 2 );
		pDC->PolyPolyline( &pts[0], &count[0], count.size() );
	}
	/////// толстые линии
	spacing *= 5;
	GetGridPlacement( &r, &ptOrig, CSize( spacing, spacing ) ); 
	if ( r.Width() < spacing || r.Height() < spacing )
		return;
	ptOrig.x = ptOrig.x == 0 ? spacing : ptOrig.x;
	ptOrig.y = ptOrig.y == 0 ? spacing : ptOrig.y;
	
	CPen penThick( PS_SOLID, 1, crGridThick );
	pDC->SelectObject( &penThick );

	pts.clear();
	for ( x = r.left + spacing - ptOrig.x; x <= r.right; x += spacing )
	{
		pts.push_back( CPoint( x, r.top ) );
		pts.push_back( CPoint( x, r.bottom ) );
	}
	count.resize( pts.size() / 2, 2 );
	pDC->PolyPolyline( &pts[0], &count[0], count.size() );
	//
	pts.clear();
	for ( y = r.top + spacing - ptOrig.y; y <= r.bottom; y += spacing )
	{
		pts.push_back( CPoint( r.left, y ) );
		pts.push_back( CPoint( r.right, y ) );
	}
	count.resize( pts.size() / 2, 2 );
	pDC->PolyPolyline( &pts[0], &count[0], count.size() );
	//
  pDC->SelectObject( pOldPen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetSpacing( int n )
{
  nSpacing = n;

  if ( GetActivePlacement() )
    SetPlacement( GetActivePlacement(), pPlacementCache );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
  if ( !pCurPlacement )
  {
    CWnd ::OnLButtonDblClk(nFlags, point);
    return;
  }
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr && pLr->IsVisible() )
		pLr->OnLButtonDblClk( nFlags, point, this );
	CWnd ::OnLButtonDblClk(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetupScrolls()
{
  CRect r;
  GetClientRect( &r );
  if ( nWidth > r.Width() )
  {
    SCROLLINFO info;

    EnableScrollBarCtrl( SB_HORZ );
	  info.fMask = SIF_PAGE|SIF_RANGE;
	  info.nMin = 0;
    info.nPage = r.Width();
		info.nMax = nWidth + 1;
		if ( !SetScrollInfo( SB_HORZ, &info ) )
			SetScrollRange( SB_HORZ, 0, nWidth + 1 );
    if ( GetScrollPos( SB_HORZ ) > nWidth + 1 )
      SetScrollPos( SB_HORZ, 0 );
  }
  else
	{
    EnableScrollBarCtrl( SB_HORZ, false );
		SetScrollPos( SB_HORZ, 0, false );
	}
  if ( nHeight > r.Height() )
  {
    SCROLLINFO info;

    EnableScrollBarCtrl( SB_VERT );
	  info.fMask = SIF_PAGE|SIF_RANGE;
	  info.nMin = 0;
    info.nPage = r.Height();
		info.nMax = nHeight + 1;
		if ( !SetScrollInfo( SB_VERT, &info ) )
			SetScrollRange( SB_VERT, 0, nHeight + 1 );
    if ( GetScrollPos( SB_VERT ) > nWidth + 1 )
      SetScrollPos( SB_VERT, 0 );
  }
  else
	{
    EnableScrollBarCtrl( SB_VERT, false );
		SetScrollPos( SB_VERT, 0, false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd ::OnSize(nType, cx, cy);
  SetupScrolls();

  CPoint old = leftTop;
  leftTop.x = GetScrollPos( SB_HORZ );
  leftTop.y = GetScrollPos( SB_VERT );
	Centre( &leftTop );
	Invalidate( false );	
  CPoint dl = old - leftTop;
  if ( 0 == dl.x && 0 == dl.y )
    return;
  pSubTemplL->MoveRects( dl );
	pUnitsL->MoveRects( dl );
	pObjsL->MoveRects( dl );
	CRect r( 0, 0, cx, cy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CChildView::UpdateScroll( int nBar, UINT nSBCode, UINT nPos )
{
  SCROLLINFO info;
  int line = int(nSpacing * fZoom) / 2;

  if ( !GetScrollInfo( nBar, &info ) )
    return 0;

  switch( nSBCode )
  {
  case SB_LINEDOWN:
    SetScrollPos( nBar, info.nPos + line );
    return line;
  case SB_LINEUP:
    SetScrollPos( nBar, info.nPos - line );
    return -line;
  case SB_PAGEDOWN:
    SetScrollPos( nBar, info.nPos + info.nPage );
    return info.nPage;
  case SB_PAGEUP:
    SetScrollPos( nBar, info.nPos - info.nPage );
    return -(int)info.nPage;
  case SB_BOTTOM:
   // SetScrollPos( nBar, nHeight );
    return 0;
  case SB_TOP:
    SetScrollPos( nBar, 0 );
    return -info.nPos;
  case SB_THUMBPOSITION:
    SetScrollPos( nBar, nPos );
    return nPos - info.nPos;
  case SB_THUMBTRACK:
    SetScrollPos( nBar, nPos );
    return nPos - info.nPos;
  }
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CRect r;
	GetClientRect( &r );
	if ( r.Width() > nWidth )
		return;
	//
	UpdateScroll( SB_HORZ, nSBCode, nPos );

  int oldx = leftTop.x;
  leftTop.x = GetScrollPos( SB_HORZ );
  int dl = oldx - leftTop.x;
  if ( !dl )
    return;
  pSubTemplL->MoveRects( CPoint( dl, 0 ) );
	pUnitsL->MoveRects( CPoint( dl, 0 ) );
	pObjsL->MoveRects( CPoint( dl, 0 ) );
  Invalidate( false );

	CWnd ::OnHScroll(nSBCode, nPos, pScrollBar);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CRect r;
	GetClientRect( &r );
	if ( r.Height() > nHeight )
		return;
	//
  UpdateScroll( SB_VERT, nSBCode, nPos );
	//
  int oldy = leftTop.y;
  leftTop.y = GetScrollPos( SB_VERT );
  int dl = oldy - leftTop.y;
  if ( !dl )
    return;
  pSubTemplL->MoveRects( CPoint( 0, dl ) );
	pUnitsL->MoveRects( CPoint( 0, dl ) );
	pObjsL->MoveRects( CPoint( 0, dl ) );
  Invalidate( false );

	CWnd ::OnVScroll(nSBCode, nPos, pScrollBar);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Новый коэффицент масштаба
void CChildView::SetZoom( float fNewZoom )
{
	if ( !IsWindow( m_hWnd ) )
		return;
	fNewZoom = Min( fNewZoom, fMaxZoom );
	int nDeltaOld = nSpacing * fZoom;
	int nDelta = nSpacing * fNewZoom;
	if ( nDeltaOld == nDelta )
		return;
	if ( nDelta < 1 )
	{
		nDelta = 1;
		fNewZoom = 1.0f / nSpacing;
	}
	fZoom = fNewZoom;
	pWallsL->SetWallWidth( fZoom * 3 );
	// Меняем размеры рабочего поля
	if ( pCurPlacement )
	{
		nWidth  = nDelta * pCurPlacement->GetWidth();
		nHeight = nDelta * pCurPlacement->GetHeight();
		Centre( &leftTop );
		SetupScrolls();
		pSubTemplL->SetPlacement( pCurPlacement, pPlacementCache, this );
		pUnitsL->SetPlacement( pCurPlacement, pPlacementCache, this );
		pObjsL->SetPlacement( pCurPlacement, pPlacementCache, this );
	}
	Invalidate( false );
	UpdateWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Плавное увеличение\уменьшение масштаба
float CChildView::TrackZoom()
{
  const float ZOOM_SPEED = 0.003f;
  // don't handle if capture already set
  if (::GetCapture() != NULL)
    return 0;
  
  // set capture to the window which received this message
  SetCapture();
	
  float fOldZoom = fZoom;
	float fTemp = fZoom;
  CPoint pt, ptOld;
  GetCursorPos( &ptOld );
  ScreenToClient( &ptOld );
	
	bZooming = true;

  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != this || WM_LBUTTONUP == msg.message )
      break;
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
      pt.y = (int)(short)HIWORD(msg.lParam);
      fTemp += ZOOM_SPEED * (ptOld.y - pt.y);
			fTemp = Clamp( fTemp, 0.01f, fMaxZoom );
      SetZoom( fTemp );
      ptOld.y = pt.y;
      break;
      // handle cancel messages
    case WM_KEYDOWN:
      if (msg.wParam != VK_ESCAPE)
        break;
    case WM_RBUTTONDOWN:
			ReleaseCapture();
			fZoom = fOldZoom;
			SetZoom( fZoom );
			bZooming = false;
      return fZoom;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
  }
	bZooming = false;
  ReleaseCapture();
	SetZoom( fTemp );
	pSubTemplL->SetPlacement( pCurPlacement, pPlacementCache, this );
	pUnitsL->SetPlacement( pCurPlacement, pPlacementCache, this );
	pObjsL->SetPlacement( pCurPlacement, pPlacementCache, this );
	Invalidate( FALSE );
  return fTemp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::TrackPan()
{
  const float PAN_SPEED = 1.0f;
  // don't handle if capture already set
  if (::GetCapture() != NULL)
    return;
  // set capture to the window which received this message
  SetCapture();
	
  CPoint pt, ptOld, ptStart( GetScrollPos( SB_HORZ ), GetScrollPos( SB_VERT ) );
  GetCursorPos( &ptOld );
	CRect rClip( 0, 0, nWidth, nHeight );
	ClientToScreen( &rClip );
	CRect rWin;
	GetWindowRect( &rWin );
	if ( rWin.Height() > nHeight )
	{
		rClip.top = rClip.bottom = ptOld.y;
	}
	else
	{
		rClip.top = ptOld.y - (nHeight - rWin.Height() - leftTop.y );
		rClip.bottom = ptOld.y + leftTop.y + 1;
	}
	if ( rWin.Width() > nWidth )
	{
		rClip.left = rClip.right = ptOld.x;
	}
	else
	{
		rClip.left = ptOld.x - (nWidth - rWin.Width() - leftTop.x );
		rClip.right  = ptOld.x + leftTop.x + 1;
	}
	BOOL ret = ClipCursor( &rClip );
  ScreenToClient( &ptOld );
	
  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != this || WM_LBUTTONUP == msg.message )
      break;
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
			{
				pt.x = (int)(short)LOWORD(msg.lParam);
				pt.y = (int)(short)HIWORD(msg.lParam);
				ptStart += ptOld - pt;
				SendMessage( WM_VSCROLL, MAKEWPARAM( SB_THUMBTRACK, ptStart.y ), 0 );
				SendMessage( WM_HSCROLL, MAKEWPARAM( SB_THUMBTRACK, ptStart.x ), 0 );
				//      leftTop += (ptOld - pt);// * PAN_SPEED;
				Invalidate( false );
				UpdateWindow();
				ptOld = pt;
			}
      break;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
  }
	ClipCursor( 0 );
  ReleaseCapture();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::TrackSelection()
{
  // don't handle if capture already set
  if (::GetCapture() != NULL)
	{
		SetSelection( CRect( 0, 0, 0, 0 ) );
    return;
	}
  
  // set capture to the window which received this message
  SetCapture();
	
  CPoint pt, ptOld, ptLT;
  GetCursorPos( &ptOld );
  ScreenToClient( &ptOld );
	ptLT = ptOld;
	CDC *pDC = GetDC();
	CPen pen( PS_DOT, 1, RGB( 0, 0, 0 ) );
	CPen *pOldPen = pDC->SelectObject( &pen );
	int oldMode = pDC->SetROP2( R2_XORPEN );
	
  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != this || WM_LBUTTONUP == msg.message )
		{
      break;
		}
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
      pt.x = (int)(short)LOWORD(msg.lParam);
      pt.y = (int)(short)HIWORD(msg.lParam);
			SetSelection( CRect( ptLT, pt ) );
			ptOld = pt;
      break;
      // handle cancel messages
    case WM_KEYDOWN:
      if (msg.wParam != VK_ESCAPE)
        break;
    case WM_RBUTTONDOWN:
			ReleaseCapture();
			pDC->SelectObject( pOldPen );
			pDC->SetROP2( oldMode );
			ReleaseDC( pDC );
			SetSelection( CRect( 0, 0, 0, 0 ) );
			return;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
  }
  ReleaseCapture();
	pDC->SelectObject( pOldPen );
	pDC->SetROP2( oldMode );
	ReleaseDC( pDC );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::Centre( CPoint *pPtLeftTop )
{
	CRect r;

	GetClientRect( &r );
	if ( r.Width() > nWidth )
	{
		pPtLeftTop->x = -0.5f * (r.Width() - nWidth);
	}
	if ( r.Height() > nHeight )
	{
		pPtLeftTop->y = -0.5f * (r.Height() - nHeight);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetSelection( const CRect &r )
{
	m_rSelection = r;
	m_rSelection.NormalizeRect();
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr )
		pLr->Selection( r, this );
	Invalidate( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::ClearSelection()
{
	m_rSelection.SetRectEmpty();
	pLayers->ClearSelection();
	ResetPropList();
	Invalidate( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnTimer(UINT nIDEvent) 
{
	CWnd::OnTimer(nIDEvent);
	if ( SELREFRESH_TIMER_ID != nIDEvent )
		return;
	//
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr )
		pLr->OnTimer( this );

	// выкидываем из очереди сообщения таймера, мы не успеваем их обрабатывать
	MSG msg;
	while( PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE ) )
		;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnClose() 
{
	SaveData();
	KillTimer( SELREFRESH_TIMER_ID );
	CWnd ::OnClose();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::WriteTerrMap()
{
	if ( !pCurPlacement )
		return false;
	CMETerrainInfo *pTerr = pCurPlacement->GetTerrainInfo();
	if ( !pTerr )
	{
		pCurPlacement->CreateTerrain();
		pTerr = pCurPlacement->GetTerrainInfo();
	}
	if ( !pTerr )
	{
		ASSERT(0);
		return false;
	}
	vector<SGrassLayer> &grass = pTerr->info.grass;

	if ( pLayers->HasLayer( pTilesL ) )
		pTilesL->GetTiles( &pTerr->info.typeMap );
	if ( pLayers->HasLayer( pHeightsL ) )
		pHeightsL->GetTerrain( &pTerr->fMinH, &pTerr->fMaxH, &pTerr->info.heightMap );
	if ( pLayers->HasLayer( pHolesL ) )
		pHolesL->GetHoles( &pTerr->info.holes );
	if ( pLayers->HasLayer( pAlphaL ) )
		pAlphaL->GetAlpha( &pTerr->alphaMap );
	CColorLayer *pColorMap = dynamic_cast<CColorLayer*>( pLayers->GetLayer( NBuilding::MakeFragmentID(LID_TERRCOLOR, 0) ) );
	if ( pColorMap )
		pColorMap->GetColormap( &pTerr->info.color );
	for ( int i = 0; i < grassLs.size(); ++i )
	{
		CGrassLayer *p = dynamic_cast<CGrassLayer*>( grassLs[i] );
		if ( !p )
			continue;
		ELayer eType;
		int nLayer;
		NBuilding::GetLayerID( p->GetLayerID(), &eType, &nLayer );
		int nID = NBuilding::MakeFragmentID( LID_GRASSCOLOR, nLayer );
		CColorLayer *pCr = dynamic_cast<CColorLayer*>( pLayers->GetLayer( nID ) );

		int nInd = p->GetGrassLayerInd();
		SGrassLayer *pGrass = 0;
		if ( nInd < 0  )
		{
			pGrass = &(*grass.insert( grass.end(), SGrassLayer() ));
			pGrass->nID = pTerr->info.nMaxGrassLayerID;
			++pTerr->info.nMaxGrassLayerID;
		}
		else
			for ( int i = 0; i < grass.size(); ++i )
			{
				if ( nInd == grass[i].nID )
				{
					pGrass = &grass[i];
					break;
				}
			}
		if ( pGrass )
		{
			p->GetGrass( &pGrass->grass );
			if ( pCr )
				pCr->GetColormap( &pGrass->grassColor );
			pGrass->nGrassID = p->GetGrassID();
			pGrass->fMaxDensity = p->GetMaxDensity();
		}
	}
	CArray2D<bool> cellar;
	pCellarL->GetCellar( &cellar );
	pCurPlacement->SetCellar( cellar );
	bTerrChanged = false;
	return pCurPlacement->SerializeTerrain();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CLayerCtrl *pLr = pLayers->GetActiveLayer();
	if ( pLr )
		pLr->OnKeyDown( nChar, nRepCnt, nFlags, this );
	switch ( nChar )
	{
		case VK_CONTROL:
			SetCursor( m_hZoom );
			break;
	}
	CWnd ::OnKeyDown(nChar, nRepCnt, nFlags);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch ( nChar )
	{
		case VK_CONTROL:
			SetCursor( m_hArrow );
			break;
	}
	CWnd ::OnKeyUp(nChar, nRepCnt, nFlags);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::HasPaintBar()
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SetGridVisible( bool bVisible )
{
	bGrid = bVisible;
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanDeleteLayer( int nLayerID );
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CChildView::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_USER_NOTIFY:
			switch ( pMsg->wParam )
			{
				case LLN_ADD:
					if ( pCurPlacement )
					{
						int i;
						CAddLayerDlg dlg( this );

						for ( i = 0; i < layers.size(); ++i )
						{
							// альфа добавляется вместе с картой высот
							if ( (layers[i].bSingle && pLayers->HasLayer( layers[i].nLayerID )) || LID_ALPHA == layers[i].nLayerID )
								continue;
							dlg.items.push_back( pair<string, DWORD>( layers[i].szName, layers[i].nLayerID + 1 ) );
						}

						if ( IDOK != dlg.DoModal() )
							break;
						for ( i = 0; i < dlg.items.size(); ++i )
							if ( dlg.items[i].second )
							{
								int nLayer= dlg.items[i].second  - 1;
								AddLayer( nLayer );
								if ( LID_TILES == nLayer || LID_HEIGHTS == nLayer || LID_GRASS == nLayer )
								{
									bTerrChanged = true;
									SaveData();
									NInput::PostEvent( "update_terrain" );
									NMainLoop::StepApp( true, true );
								}
							}
						break;
					}
					break;
				case LLN_DEL:
					{
						CLayerCtrl *pLr = pLayers->GetActiveLayer();
						if ( !pLr || !CanDeleteLayer( pLr->GetLayerType() ) )
							break;
						CString szWorn;
						szWorn.LoadString( IDS_CONFIRM_DELETELAYER );
						if ( IDYES != MessageBox( szWorn, 0, MB_YESNO ) )
							break;
						DeleteLayer( pLr );
					}
					break;
				case LLN_TERR_CHANGED:
					bTerrChanged = true;
					Invalidate( FALSE );
					break;
				case LLN_TERR_SERIALIZE:
					WriteTerrMap();
					break;
				case LLN_SETUP:
					{
						CLayerCtrl *pLr = pLayers->GetActiveLayer();
						if ( !pLr )
							break;
						CDynamicCast<CRectsLayer> p(pLr);
						if (p)
							p->SetPlacement( pCurPlacement, pPlacementCache, this );
					}
					break;
				case LLN_PROPSRESET:
					ResetPropList();
					break;
				default:
					Invalidate( FALSE );
			}
			return true;
		case WM_USER_EXPORTTERR:
			{
				CLayerCtrl *pl = pLayers->GetLayer( pMsg->wParam );
				if ( pl )
				{
					BeginWaitCursor();
					string szDir = pl->GetExportDir();
					string szPrefix = IToA( pCurPlacement->GetID() ) + "_";
					if ( pLayers->HasLayer( pHeightsL ) )
						pHeightsL->Export( szDir, szPrefix );
					if ( pLayers->HasLayer( pTilesL ) )
						pTilesL->Export( szDir, szPrefix );
					if ( pLayers->HasLayer( pAlphaL ) )
						pAlphaL->Export( szDir, szPrefix );
					CColorLayer *pCM = dynamic_cast<CColorLayer*>( pLayers->GetLayer( NBuilding::MakeFragmentID( LID_TERRCOLOR, 0 ) ) );
					if ( pCM )
						pCM->Export( szDir, szPrefix );
					EndWaitCursor();
				}
			}
			return true;
		case WM_USER_LINK:
			{
				CLayerCtrl *pl = pLayers->GetLayer( pMsg->wParam );
				if ( pl )
				{
					OnLinkLayer( pl );
				}
			}
			return true;
		case WM_USER_FLOORLINK:
			OnLinkFloor();
			return true;
		case WM_USER_SELECT:
			{
				CLayerCtrl *pl = pLayers->GetLayer( pMsg->wParam );
				if ( pl )
				{
					OnActivateLayer( pl );
				}
			}
			return true;
	}
	return CWnd ::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EEditMode CChildView::GetPaintMode( int *pExtra ) const
{
	if ( pExtra )
	{
		switch ( pLayerList->GetBrushSize() )
		{
			case 0:
				*pExtra = PM_PEN_W1;
				break;
			case 1:
				*pExtra = PM_PEN_W2;
				break;
			case 2:
				*pExtra = PM_PEN_W3;
				break;
		}
	}
	return GetUserSettings().GetMode(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLayerCtrl* CChildView::AddLayer( int nLayerID, int nLayerInd, CLayerCtrl **pSecond )
{
	if ( !pCurPlacement )
		return 0;
	const int nFloor = theApp.GetActiveFloor();
	CLayerCtrl *pCtrl = 0;
	CLayerCtrl *pSecondCtrl = 0;
	int nInd = -1;
	string szIndex = " ";

	switch ( nLayerID )
	{
		case LID_HEIGHTS:
			pHeightsL->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1, 0 );
			pAlphaL->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1, 0xffffffff );
			if ( !pLayers->HasLayer( pHeightsL ) )
				pLayers->AddLayer( pHeightsL );
			if ( !pLayers->HasLayer( pAlphaL ) )
				pLayers->AddLayer( pAlphaL );
			bTerrChanged = true;
			return pHeightsL;
		case LID_HOLES:
			pHolesL->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1, 0xffffffff );
			if ( !pLayers->HasLayer( pHolesL ) )
				pLayers->AddLayer( pHolesL );
			bTerrChanged = true;
			return pHolesL;
		case LID_TILES:
			pTilesL->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1 );
			if ( !pLayers->HasLayer( pTilesL ) )
				pLayers->AddLayer( pTilesL );
			return pTilesL;
		case LID_FLOORS:
			nInd = nLayerInd == -1 ? floorsLs.size() : nLayerInd;
			if ( nInd ) szIndex += IToA( nInd );
			pCtrl = new CFloorsLayer( FT_FLOOR, LID_FLOORS, ("Floors" + szIndex).c_str(), IDC_CONSTRUCTIONPARTS_TREE, nInd );
			floorsLs.push_back( pCtrl );
			break;
		case LID_FLOORS_INTERM:
			nInd = nLayerInd == -1 ? floorsIntermLs.size() : nLayerInd;
			if ( nInd ) szIndex += IToA( nInd );
			pCtrl = new CFloorsLayer( FT_FLOOR_INTERMEDIATE, LID_FLOORS_INTERM, ("Intermediate floors" + szIndex).c_str(), IDC_CONSTRUCTIONPARTS_TREE, nInd );
			floorsIntermLs.push_back( pCtrl );
			break;
		case LID_SOLIDS:
			nInd = nLayerInd == -1 ? solidsLs.size() : nLayerInd;
			if ( nInd ) szIndex += IToA( nInd );
			pCtrl = new CFloorsLayer( FT_SOLID_, LID_SOLIDS, ("Solids" + szIndex).c_str(), IDC_CONSTRUCTIONPARTS_TREE, nInd );
			solidsLs.push_back( pCtrl );
			break;
		case LID_SOLIDS_INTERM:
			nInd = nLayerInd == -1 ? solidsIntermLs.size() : nLayerInd;
			if ( nInd ) szIndex += IToA( nInd );
			pCtrl = new CFloorsLayer( FT_SOLID_INTERMEDIATE, LID_SOLIDS_INTERM, ("Intermediate Solids" + szIndex).c_str(), IDC_CONSTRUCTIONPARTS_TREE, nInd );
			solidsIntermLs.push_back( pCtrl );
			break;
		case LID_ROOMS:
			pCtrl = new CRoomsLayer;
			roomsLs.push_back( pCtrl );
			break;
		case LID_GRASS:
			{
				if ( nLayerInd < 0 )
				{
					if ( !pCurPlacement )
						break;
					CMETerrainInfo *pTerr = pCurPlacement->GetTerrainInfo();
					if ( !pTerr )
					{
						pCurPlacement->CreateTerrain();
						pTerr = pCurPlacement->GetTerrainInfo();
					}
					if ( !pTerr )
						break;
					SGrassLayer *pGrass = &(*pTerr->info.grass.insert( pTerr->info.grass.end(), SGrassLayer() ));
					pGrass->nID = pTerr->info.nMaxGrassLayerID;
					nLayerInd = pGrass->nID;
					++pTerr->info.nMaxGrassLayerID;
				}
				CGrassLayer *p = new CGrassLayer( nLayerInd );
				pCtrl = p;
				p->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1, 0 );
				p->SetLayerInd( nLayerInd );
				grassLs.push_back( p );
				CColorLayer *pGCr = new CColorLayer( RGB(255, 255, 255), 2, LID_GRASSCOLOR, nLayerInd );
				pSecondCtrl = pGCr;
				pGCr->CreateImage( pCurPlacement->GetWidth() / 2 + 1, pCurPlacement->GetHeight() / 2 + 1, 0xffffff );
				grassLs.push_back( pSecondCtrl );
				bTerrChanged = true;
			}
			break;
		case LID_TERRCOLOR:
			{
				CColorLayer *p = new CColorLayer( RGB(255, 255, 255), 1, LID_TERRCOLOR, 0, "Terrain color map" );
				p->CreateImage( pCurPlacement->GetWidth() + 1, pCurPlacement->GetHeight() + 1, 0xffffffff );
				pCtrl = p;
				terrColorLs.push_back( p );
				bTerrChanged = true;
			}
			break;
		case LID_SPOTS:
			{
				if ( nLayerInd < 0 )
				{
					for ( int i = 0; i < spotsLs.size(); ++i )
					{
						CSpotsLayer *p = dynamic_cast<CSpotsLayer*>( spotsLs[i] );
						if ( p )
						{
							ELayer type;
							int nLayer;

							NBuilding::GetLayerID( p->GetLayerID(), &type, &nLayer );
							nLayerInd = Max( nLayerInd, nLayer );
						}
					}
					++nLayerInd;
				}
				CSpotsLayer *p = new CSpotsLayer( nLayerInd );
				pCtrl = p;
				spotsLs.push_back( p );
				bTerrChanged = true;
			}
			break;
	}
	//
	if ( pCtrl )
	{
		bool bVis = false;
		for ( int i = 0; i < layers.size(); ++i )
			if ( layers[i].nLayerID == nLayerID )
				bVis = layers[i].bLastVisible;
		pLayers->AddLayer( pCtrl );
		//pCtrl->SetVisible( bVis );
		if ( pCurPlacement )
			pCtrl->SetPlacement( pCurPlacement );
	}
	if ( pSecondCtrl )
	{
		pLayers->AddLayer( pSecondCtrl );
		if ( pCurPlacement )
			pSecondCtrl->SetPlacement( pCurPlacement );
		if ( pSecond )
			*pSecond = pSecondCtrl;
	}
	return pCtrl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SaveData()
{
	if ( bTerrChanged )
		WriteTerrMap();		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::ExportBmp()
{
  char szWD[512];
  GetCurrentDirectory( sizeof(szWD) - 1, szWD );
	CFileDialog dlg( false, "tga", 0, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "(*.tga)|*.tga||" );

	if ( IDOK != dlg.DoModal() )
		return true;
	SetCurrentDirectory( szWD );
	CString path = dlg.GetPathName();
	CDC *pDC = GetDC();
  CDC dcBuf;
  CBitmap bmp;
	CPoint pt;

	BeginWaitCursor();
	// один тайл == 1 пиксел
	float fOldZoom = GetZoom();
	bool  bOldGrid = bGrid;
	bGrid = false;
	SetZoom( 1.0f / nSpacing );

  TemplateToScreen( &pt, 0, 0 );
  bmp.CreateCompatibleBitmap( pDC, pt.x + nWidth, pt.y );
  dcBuf.CreateCompatibleDC( pDC );
	CBitmap *pOld = dcBuf.SelectObject( &bmp );
  dcBuf.FillSolidRect( 0, 0, pt.x + nWidth, pt.y, RGB( 180, 180, 180 ) );
	dcBuf.SetBkMode(TRANSPARENT);

	{
		CRect r;
		CPoint pt;
		GetGridPlacement( &r, &pt, CSize( GetSpacing(), GetSpacing() ) );	
		dcBuf.FillSolidRect( &r, GRID_BACKGROUND );	
	}
	//
	pDCBuf = &dcBuf;
	pLayers->Paint( this );
	DrawLineGrid( &dcBuf );
	pDCBuf = 0;

	int x, y;
	int i, j, pos = 0;
	vector<DWORD> image( nWidth * nHeight );

	for ( j = 0, y = pt.y - nHeight; j < nHeight; ++j, ++y )
		for ( i = 0, x = pt.x; i < nWidth; ++i, ++x )
		{
			DWORD cr = dcBuf.GetPixel( x, y );
			image[pos++] = RGB( GetBValue( cr ), GetGValue( cr ), GetRValue( cr ) );
		}
#ifdef _DEBUG
	pos = 0;
	for ( j = 0, y = pt.y - nHeight; j < nHeight; ++j, ++y )
		for ( i = 0, x = pt.x; i < nWidth; ++i, ++x )
		{
			DWORD cr = image[pos++];
			dcBuf.SetPixel( x, y, RGB( GetBValue( cr ), GetGValue( cr ), GetRValue( cr ) ) );
		}
	pDC->BitBlt( leftTop.x, leftTop.y, pt.x + nWidth, pt.y, &dcBuf, 0, 0, SRCCOPY );
#endif
		
	// Сохраняем TGA на диске
	NImage::CImage cimage;
	NImage::Convert( image, nWidth, nHeight, &cimage );
	bool bRet = false;
	CFileStream file;
	file.OpenWrite( path );
	bRet = NImage::SaveImageAsTGA( &file, cimage );
	file.CloseFile();
	//
	dcBuf.SelectObject( pOld );
	bmp.DeleteObject();
	ReleaseDC( pDC );

	bGrid = bOldGrid;
	SetZoom( fOldZoom );
	EndWaitCursor();
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnFileExportTemplate() 
{
	if ( !pCurPlacement )
		return;
	ExportBmp();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::SelectedItem( int nResTreeID, int nModelID, int nObjID )
{
	ResetPropList();
	const SResTree *pTree = theApp.GetResTree( nResTreeID );
	const int nFloor = theApp.GetActiveFloor();
	/*
	if ( IDC_OBJECTS_TREE == nResTreeID )
	{
	}
	*/
	if ( pTree )
	{
		const char *pName = pTree->pItemsTree->GetItemName( nModelID );
		if ( pName )
		{
			CProp *prop = new CObjectProp( this, "SelectedItem", MODEL_PROP_ID, nResTreeID, nObjID, CVariant::VT_INT );
			prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
			prop->SetValue( nModelID, false );
			prop->SetGroup(  nResTreeID );
			prop->SetRelation( nResTreeID );
			props["SelectedItem"] =  prop;
			//
			CVec3 ptPos;
			int nRot;
			if ( pCurPlacement->GetObjPos( theApp.GetActiveFloor(), TreeID2MapObjType( nResTreeID ), nObjID, &ptPos, &nRot ) )
			{
				prop = new CObjectProp( this, "DeltaZ", DZ_PROP_ID, nResTreeID, nObjID, CVariant::VT_FLOAT );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				prop->SetValue( ptPos.z, false );
				prop->SetGroup(  nResTreeID );
				props["DeltaZ"] =  prop;
				//
				prop = new CObjectProp( this, "Rotation", ROTATION_PROP_ID, nResTreeID, nObjID, CVariant::VT_INT );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				prop->SetValue( nRot, false );
				prop->SetGroup(  nResTreeID );
				props["Rotation"] =  prop;
			}
			//
			if ( IDC_OBJECTS_TREE == pTree->nTreeID )
			{
				int nRoom = (char)pCurPlacement->GetObjRoomID( theApp.GetActiveFloor(), nObjID );
				vector<SRoom> rooms;
				pCurPlacement->GetRoomParams( theApp.GetActiveFloor(), &rooms );
				prop = new CObjectProp( this, "RoomID", ROOM_PROP_ID, nResTreeID, nObjID, CVariant::VT_INT, DT_COMBO );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				nRoom == 0 ? prop->SetValue( OUTDOOR_ROOM, false ) : prop->SetValue( nRoom, false );
				prop->SetGroup(  nResTreeID );
				prop->AddString( OUTDOOR_ROOM );
				prop->AddString( "-1" );
				for ( int i = 0; i < rooms.size(); ++i )
					prop->AddString( IToA( rooms[i].nRoomID ) );
				props["RoomID"] =  prop;
				//
				CVec3 ptScale = pCurPlacement->GetObjScale( theApp.GetActiveFloor(), nObjID );
				prop = new CObjectProp( this, "ScaleX", SCALEX_PROP_ID, nResTreeID, nObjID, CVariant::VT_FLOAT );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				prop->SetValue( ptScale.x, false );
				prop->SetGroup(  nResTreeID );
				props["ScaleX"] =  prop;
				//
				prop = new CObjectProp( this, "ScaleY", SCALEY_PROP_ID, nResTreeID, nObjID, CVariant::VT_FLOAT );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				prop->SetValue( ptScale.y, false );
				prop->SetGroup(  nResTreeID );
				props["ScaleY"] =  prop;
				//
				prop = new CObjectProp( this, "ScaleZ", SCALEZ_PROP_ID, nResTreeID, nObjID, CVariant::VT_FLOAT );
				prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
				prop->SetValue( ptScale.z, false );
				prop->SetGroup(  nResTreeID );
				props["ScaleZ"] =  prop;
			}
		}
	}
	if ( IDC_EXPLOSIONS_TREE == nResTreeID )
	{
		CVec3 ptPos;
		int nRot;
		if ( pCurPlacement->GetObjPos( nFloor, MO_EXPLOSION, nObjID, &ptPos, &nRot ) )
		{
			float fPower = pCurPlacement->GetExplosionPower( nFloor, nObjID );
			CProp *prop = new CObjectProp( this, "Explosion", POWER_PROP_ID, IDC_EXPLOSIONS_TREE, nObjID, CVariant::VT_FLOAT );
			prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
			prop->SetValue( fPower, false );
			prop->SetGroup( IDC_EXPLOSIONS_TREE );
			props["Explosion"] =  prop;
			//
			prop = new CObjectProp( this, "DeltaZ", DZ_PROP_ID, IDC_EXPLOSIONS_TREE, nObjID, CVariant::VT_FLOAT );
			prop->SetOwner( SOwner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() ) );
			prop->SetValue( ptPos.z, false );
			prop->SetGroup( IDC_EXPLOSIONS_TREE );
			props["DeltaZ"] =  prop;
		}
	}
	theApp.SetPropMap( &props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::ChangeObject( int nTreeID, int nObjectID, int nNewRelation )
{
	const EMapObjType type = TreeID2MapObjType( nTreeID);
	switch ( type )
	{
	case MO_TEMPLATE:
		return pSubTemplL->ChangeObject( nTreeID, nObjectID, nNewRelation, this );
	case MO_UNIT:
		return pUnitsL->ChangeObject( nTreeID, nObjectID, nNewRelation, this );
	case MO_OBJECT:
	case MO_EXPLOSION:
		return pObjsL->ChangeObject( nTreeID, nObjectID, nNewRelation, this );
	}
	ASSERT(0);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::ResetPropList()
{
	props.clear();
	if ( pCurPlacement )
	{
		CProp *prop = new CFinProp( "PlacementID", 1, CVariant::VT_INT, DT_DEC, true );
		SOwner owner( pCurPlacement->GetTemplateID(), pCurPlacement->GetID() );
		prop->SetOwner( owner );
		prop->SetValue( pCurPlacement->GetID(), false );
		props["PlacementID"] =  prop;
		prop = new CPlacementProp( pCurPlacement, &CPlacement::SetGrid, "Grid", 2, CVariant::VT_INT, DT_BOOL );
		prop->SetOwner( owner );
		prop->SetValue( pCurPlacement->HasGrid(), false );
		props["Grid"] =  prop;
		prop = new CPlacementProp( pCurPlacement, &CPlacement::SetRndWeight, "RndWeight", 3, CVariant::VT_FLOAT, DT_DEC );
		prop->SetOwner( owner );
		prop->SetValue( pCurPlacement->GetRndWeight(), false );
		props["RndWeight"] =  prop;
		prop = new CPlacementProp( pCurPlacement, &CPlacement::SetDefLight, "DefaultLight", 4, CVariant::VT_INT, DT_DEC );
		prop->SetOwner( owner );
		prop->SetValue( pCurPlacement->GetDefLight(), false );
		prop->SetGroup(  IDC_AMBIENTLIGHTS_TREE );
		prop->SetRelation( IDC_AMBIENTLIGHTS_TREE );
		props["DefaultLight"] =  prop;
		prop = new CPlacementProp( pCurPlacement, &CPlacement::SetScriptID, "Script", 5, CVariant::VT_INT, DT_DEC );
		prop->SetOwner( owner );
		prop->SetValue( pCurPlacement->GetScriptID(), false );
		prop->SetGroup(  IDC_SCRIPTS_TREE );
		prop->SetRelation( IDC_SCRIPTS_TREE );
		props["Script"] =  prop;
	}
	theApp.SetPropMap( &props );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetContainerRoom( int nObjID, int nRoom )
{
	if ( !pCurPlacement )
		return false;
	return pCurPlacement->SetObjRoomID( theApp.GetActiveFloor(), nObjID, nRoom );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetObjectDZ( int nTreeID, int nObjID, float fDZ )
{
	if ( !pCurPlacement )
		return false;
	const EMapObjType type = TreeID2MapObjType( nTreeID);
	const int nFloor = theApp.GetActiveFloor();
	int   nRot;
	CVec3 pt;

	if ( !pCurPlacement->GetObjPos( nFloor, type, nObjID, &pt, &nRot ) )
		return false;
	pt.z = fDZ;
	if ( pCurPlacement->MoveObj( nFloor, type, nObjID, pt ) )
	{
		switch ( type )
		{
			case MO_TEMPLATE:
				pSubTemplL->SetupRects( this, true );
				break;
			case MO_UNIT:
				pUnitsL->SetupRects( this, true );
				break;
			case MO_OBJECT:
			case MO_EXPLOSION:
				pObjsL->SetupRects( this, true );
				break;
		}
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetObjectRotation( int nTreeID, int nObjID, int nRotation )
{
	if ( !pCurPlacement )
		return false;
	
	return pCurPlacement->RotateObj( theApp.GetActiveFloor(), TreeID2MapObjType( nTreeID), nObjID, nRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetExplosionPower( int nObjID, float fPower )
{
	if ( !pCurPlacement )
		return false;
	
	return pCurPlacement->SetExplosionPower( theApp.GetActiveFloor(), nObjID, fPower );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetScaleX( int nObjID, float fScale )
{
	if ( !pCurPlacement )
		return false;
	return pCurPlacement->SetObjScaleX( theApp.GetActiveFloor(), nObjID, fScale );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetScaleY( int nObjID, float fScale )
{
	if ( !pCurPlacement )
		return false;
	return pCurPlacement->SetObjScaleY( theApp.GetActiveFloor(), nObjID, fScale );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChildView::SetScaleZ( int nObjID, float fScale )
{
	if ( !pCurPlacement )
		return false;
	return pCurPlacement->SetObjScaleZ( theApp.GetActiveFloor(), nObjID, fScale );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void DelLayer( CLayerCtrl *pLayer )
{
	if ( pLayer )
	{
		pLayer->DestroyWindow();
		delete pLayer;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::DeleteLayers()
{
	for_each( floorsLs.begin(), floorsLs.end(), DelLayer );
	for_each( floorsIntermLs.begin(), floorsIntermLs.end(), DelLayer );
	for_each( solidsLs.begin(), solidsLs.end(), DelLayer );
	for_each( solidsIntermLs.begin(), solidsIntermLs.end(), DelLayer );
	for_each( roomsLs.begin(), roomsLs.end(), DelLayer );
	for_each( grassLs.begin(), grassLs.end(), DelLayer );
	for_each( terrColorLs.begin(), terrColorLs.end(), DelLayer );
	for_each( spotsLs.begin(), spotsLs.end(), DelLayer );
	floorsLs.clear();
	floorsIntermLs.clear();
	solidsLs.clear();
	solidsIntermLs.clear();
	roomsLs.clear();
	grassLs.clear();
	terrColorLs.clear();
	spotsLs.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnDestroy()
{
	CWnd ::OnDestroy();
	
	SaveData();
	KillTimer( SELREFRESH_TIMER_ID );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanDeleteLayer( int nLayerID )
{
	switch( nLayerID )
	{
		case LID_GRASS:
			return true;
		default:
			return false;
	}
}
void CChildView::DeleteLayer( CLayerCtrl *pLr )
{
	CGrassLayer *pGrass = dynamic_cast<CGrassLayer*>( pLr );
	if ( pGrass )
	{
		vector<CLayerCtrl*>::iterator it = find( grassLs.begin(), grassLs.end(), pGrass );
		if ( it != grassLs.end() )
		{
			ELayer eType;
			int nInd;

			NBuilding::GetLayerID( pGrass->GetLayerID(), &eType, &nInd );
			int nID = NBuilding::MakeFragmentID( LID_GRASSCOLOR, nInd );
			CColorLayer *pCr = dynamic_cast<CColorLayer*>( pLayers->GetLayer( nID ) );
			if ( pCr )
			{
				vector<CLayerCtrl*>::iterator itc = find( grassLs.begin(), grassLs.end(), pCr );
				grassLs.erase( itc );
				pLayers->DeleteLayer( pCr->GetLayerID() );
				pCr->DestroyWindow();
				delete pCr;
			}
			grassLs.erase( it );
			pLayers->DeleteLayer( pLr->GetLayerID() );
			pLr->DestroyWindow();
			if ( pCurPlacement )
			{
				CMETerrainInfo *pTerr = pCurPlacement->GetTerrainInfo();
				if ( pTerr )
				{
					int nInd = pGrass->GetGrassLayerInd();
					for ( vector<SGrassLayer>::iterator it = pTerr->info.grass.begin(); it != pTerr->info.grass.end(); ++it )
						if ( it->nID == nInd )
						{
							pTerr->info.grass.erase( it );
							break;
						}
				}
				else
				{
					ASSERT(0);
				}
			}
			delete pLr;
			bTerrChanged = true;
			SaveData();
			NInput::PostEvent( "update_terrain" );
			NMainLoop::StepApp( true, true );
		}
	}
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnActivateLayer( CLayerCtrl *p )
{
	ASSERT( p );
	if ( !pCurPlacement )
		return;
	for ( CLayers::iterator i = pLayers->begin(); i != pLayers->end(); ++i )
	{
		if ( *i )
			(*i)->SetLink( false );
	}
	vector<NBuilding::SLayerGroup> groups;
	pCurPlacement->GetLayerGroups( &groups );
	int nGroup = GetLayerGroup( p->GetLayerID(), groups );
	if ( nGroup != -1 )
	{
		const NBuilding::SLayerGroup &g = groups[nGroup];
		for ( int i = 0; i < g.layers.size(); ++i )
		{
			CLayerCtrl *pl = pLayers->GetLayer( g.layers[i] );
			if ( pl )
				pl->SetLink( true );
		}
	}
	p->SetLink( true );
	OnActivateFloor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnLinkLayer( CLayerCtrl *p )
{
	ASSERT( p );
	if ( !pCurPlacement )
		return;

	CLayerCtrl *pActive = pLayers->GetActiveLayer();
	if ( pActive == p )
	{
		p->SetLink( true );
		return;
	}
	//
	vector<NBuilding::SLayerGroup> groups;
	pCurPlacement->GetLayerGroups( &groups );
	int nGroup = GetLayerGroup( p->GetLayerID(), groups );
	bool bLink = p->GetLink();
	if ( bLink )
	{
		if ( nGroup != -1 )
		{
			// удаляем из старой группы
			NBuilding::SLayerGroup &g = groups[nGroup];
			vector<int>::iterator i = find( g.layers.begin(), g.layers.end(), p->GetLayerID() );
			ASSERT( i != g.layers.end() );
			g.layers.erase( i );
		}
		if ( pActive )
		{
			// связываем с активным слоем, если группы для активного слоя нет, то создаем ее
			int nActiveGroup = GetLayerGroup( pActive->GetLayerID(), groups );
			if ( nActiveGroup != -1 )
			{
				groups[nActiveGroup].layers.push_back( p->GetLayerID() );
			}
			else
			{
				NBuilding::SLayerGroup g;
				g.layers.push_back( p->GetLayerID() );
				g.layers.push_back( pActive->GetLayerID() );
				groups.push_back( g );
			}
		}
	}
	else
	{
		if ( nGroup == -1 )
		{
			ASSERT( 0 );
			return;
		}
		NBuilding::SLayerGroup &g = groups[nGroup];
		vector<int>::iterator i = find( g.layers.begin(), g.layers.end(), p->GetLayerID() );
		ASSERT( i != g.layers.end() );
		g.layers.erase( i );
	}
	//
	BeginWaitCursor();
	pCurPlacement->SetLayerGroups( groups );
	NInput::PostEvent( "update" );
	NMainLoop::StepApp( true, true );
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::ActiveFloorChanged()
{
	OnActivateFloor();
	Invalidate(FALSE);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnActivateFloor()
{
	CLayerCtrl *p = pLayers->GetActiveLayer();
	if ( !pCurPlacement || !p )
		return;
	//
	pLayers->ResetLinkedFloors();
	vector<NBuilding::SLayerGroup> groups;
	pCurPlacement->GetLayerGroups( &groups );
	int nGroup = GetLayerGroup( p->GetLayerID(), groups );

	if ( nGroup != -1 )
	{
		const NBuilding::SLayerGroup &g = groups[nGroup];
		pLayers->SetLinkedFloors( g.floor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::OnLinkFloor()
{
	CLayerCtrl *p = pLayers->GetActiveLayer();
	if ( !pCurPlacement || !p )
		return;
	//
	vector<NBuilding::SLayerGroup> groups;
	pCurPlacement->GetLayerGroups( &groups );
	int nGroup = GetLayerGroup( p->GetLayerID(), groups );

	if ( nGroup != -1 )
	{
		NBuilding::SLayerGroup &g = groups[nGroup];
		pLayers->GetLinkedFloors( &g.floor );
	}
	else
	{
		NBuilding::SLayerGroup g;
		g.layers.push_back( p->GetLayerID() );
		pLayers->GetLinkedFloors( &g.floor );
		groups.push_back( g );
	}
	BeginWaitCursor();
	pCurPlacement->SetLayerGroups( groups );
	NInput::PostEvent( "update" );
	NMainLoop::StepApp( true, true );
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChildView::AddTerrainSpotLayers()
{
	if ( !pCurPlacement )
		return;
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( pCurPlacement->GetID() );
	if ( !IsValid( pVar ) )
		return;
	unordered_map<int, bool> spotlayers;
	for ( int i = 0; i < pVar->terrainSpots.size(); ++i )
		if ( IsValid( pVar->terrainSpots[i] ) )
			spotlayers[pVar->terrainSpots[i]->nLayer] = true;
	//
	for ( unordered_map<int, bool>::const_iterator i = spotlayers.begin(); i != spotlayers.end(); ++i )
		AddLayer( LID_SPOTS, i->first );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetDropInfo( COleDataObject* pDataObject, int *pnTree, int *pnItem )
{
	if( !pDataObject->IsDataAvailable( CF_TEXT ) )
		false;
	STGMEDIUM stg;
	pDataObject->GetData( CF_TEXT, &stg );
	if ( !stg.hGlobal )
		return false;
	LPTSTR lpszDbObjPtr = (LPTSTR)GlobalLock( stg.hGlobal );
	bool bRet = true;
	if ( lpszDbObjPtr )
	{
		if ( 2 != sscanf( lpszDbObjPtr, "%d%d", pnTree, pnItem ) )
			bRet = false;
	}
	GlobalUnlock( stg.hGlobal );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CDropTarget::OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject,	DWORD dwKeyState, CPoint point)
{
	ASSERT(pView);
	int nTree, n;
	if ( GetDropInfo( pDataObject, &nTree, &n ) && (IDC_OBJECTS_TREE == nTree || IDC_RPG_PERS_TREE == nTree || IDC_TEMPLATE_TREE == nTree ) )
		return DROPEFFECT_MOVE;
	return DROPEFFECT_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DROPEFFECT CDropTarget::OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	ASSERT(pView);
	int nTree, n;
	if ( GetDropInfo( pDataObject, &nTree, &n ) && (IDC_OBJECTS_TREE == nTree || IDC_RPG_PERS_TREE == nTree || IDC_TEMPLATE_TREE == nTree ) )
		return DROPEFFECT_MOVE;
	return DROPEFFECT_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CDropTarget::OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	ASSERT(pView);
	int nTree, nID;
	if ( !GetDropInfo( pDataObject, &nTree, &nID ) )
		return false;
	return theApp.DropItem( nTree, nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDropTarget::OnDragLeave(CWnd* pWnd)
{
	ASSERT(pView);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
