// UIView.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "UIView.h"
#include "UIContainer.h"
#include "UIControls.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern bool gbIterfacePreview;
const int TRACKER_STYLE = CRectTracker::solidLine | CRectTracker::resizeInside;
const int ATRACKER_STYLE = CRectTracker::dottedLine | CRectTracker::resizeInside;
COLORREF CR_GRID = RGB( 200, 230, 210 );
const int NX_STEPS = 32;
const int NY_STEPS = 32;
static vector<char> bLayers;
static CWnd *pUIWnd = 0;
static int nMaxID = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIView

CUIView::CUIView(): pUICProps(0)
{
	nUIControlMode = UI_SELECT;
	sizeContainer = CSize( 0, 0 );
	SetScrollSizes( MM_TEXT, sizeContainer );
	ptGridSpacing = CPoint( 32, 32 );
	bGrid = true;
	bLayers.clear();
	bLayers.resize( 8, true );
	pUIWnd = this;
	m_popup.CreatePopupMenu();
  m_popup.AppendMenu( MF_STRING, ID_IMAGESIZE, "Sync with image size" );
}

CUIView::~CUIView()
{
	m_defFont.DeleteObject();
	m_popup.DestroyMenu();
}


BEGIN_MESSAGE_MAP(CUIView, CScrollView)
	//{{AFX_MSG_MAP(CUIView)
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETCURSOR()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_UI_BUTTON, OnUiButton)
	ON_COMMAND(ID_UI_PUSHBUTTON, OnUiPushButton)
	ON_COMMAND(ID_UI_CHECKBUTTON, OnUiCheckbutton)
	ON_COMMAND(ID_UI_EDIT, OnUiEdit)
	ON_COMMAND(ID_UI_IMAGE, OnUiImage)
	ON_COMMAND(ID_UI_IMAGELIST, OnUiImagelist)
	ON_COMMAND(ID_UI_MESSAGEBOX, OnUiMessagebox)
	ON_COMMAND(ID_UI_MODEL, OnUiModel)
	ON_COMMAND(ID_UI_RADIOBUTTON, OnUiRadiobutton)
	ON_COMMAND(ID_UI_SELECT, OnUiSelect)
	ON_COMMAND(ID_UI_SLIDER, OnUiSlider)
	ON_COMMAND(ID_UI_TEXT, OnUiText)
	ON_COMMAND(ID_UI_SCROLL, OnUiScroll)
	ON_COMMAND(ID_UI_COMBOBOX, OnUiCombobox)
	ON_UPDATE_COMMAND_UI(ID_UI_BUTTON, OnUpdateUiButton)
	ON_UPDATE_COMMAND_UI(ID_UI_PUSHBUTTON, OnUpdateUiPushButton)
	ON_UPDATE_COMMAND_UI(ID_UI_CHECKBUTTON, OnUpdateUiCheckbutton)
	ON_UPDATE_COMMAND_UI(ID_UI_EDIT, OnUpdateUiEdit)
	ON_UPDATE_COMMAND_UI(ID_UI_IMAGE, OnUpdateUiImage)
	ON_UPDATE_COMMAND_UI(ID_UI_IMAGELIST, OnUpdateUiImagelist)
	ON_UPDATE_COMMAND_UI(ID_UI_MESSAGEBOX, OnUpdateUiMessagebox)
	ON_UPDATE_COMMAND_UI(ID_UI_MODEL, OnUpdateUiModel)
	ON_UPDATE_COMMAND_UI(ID_UI_RADIOBUTTON, OnUpdateUiRadiobutton)
	ON_UPDATE_COMMAND_UI(ID_UI_SELECT, OnUpdateUiSelect)
	ON_UPDATE_COMMAND_UI(ID_UI_SLIDER, OnUpdateUiSlider)
	ON_UPDATE_COMMAND_UI(ID_UI_TEXT, OnUpdateUiText)
	ON_WM_CREATE()
	ON_COMMAND(ID_UI_GROUP, OnUiGroup)
	ON_UPDATE_COMMAND_UI(ID_UI_GROUP, OnUpdateUiGroup)
	ON_COMMAND(ID_UI_WINDOW, OnUiWindow)
	ON_UPDATE_COMMAND_UI(ID_UI_WINDOW, OnUpdateUiWindow)
	ON_COMMAND(ID_UI_PROGRESSBAR, OnUiProgressBar)
	ON_UPDATE_COMMAND_UI(ID_UI_PROGRESSBAR, OnUpdateUiProgressBar)
	ON_COMMAND(ID_IMAGESIZE, OnImageSize)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUIView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CScrollView::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS|CS_BYTEALIGNCLIENT, 
		::LoadCursor(NULL, IDC_ARROW), /*HBRUSH(COLOR_WINDOW+1)*/0, NULL);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIView message handlers
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// Create font
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
	lf.lfHeight = 15;							// request a ?-pixel-height font
	strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
	m_defFont.CreateFontIndirect(&lf);			// create the fonts

	SetFont( &m_defFont );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::DrawGrid( CDC *pDC )
{
	vector<CPoint> pts;
	vector<DWORD> count;

	CPen pen( PS_SOLID, 0, CR_GRID );
//	DWORD style[] = { 1, 1 };
//	LOGBRUSH lb = { BS_SOLID, CR_GRID, 0xffffffff };
//	CPen pen( PS_USERSTYLE, 0, &lb, 2, style );
	CPen *pPenOld = pDC->SelectObject( &pen );

	if ( ptGridSpacing.x > 0 )
	{
		const int cntx = 1 + sizeContainer.cx / ptGridSpacing.x;
		pts.reserve( 2 * cntx );
		for ( int i = 0; i < cntx; ++i )
		{
			pts.push_back( CPoint( i * ptGridSpacing.x, 0 ) );
			pts.push_back( CPoint( i * ptGridSpacing.x, sizeContainer.cy ) );
		}
		count.resize( pts.size() / 2, 2 );
		pDC->PolyPolyline( &pts[0], &count[0], count.size() );
	}
	//
	if ( ptGridSpacing.y > 0 )
	{
		int cnty = 1 + sizeContainer.cy / ptGridSpacing.y;
		pts.clear();
		pts.reserve( 2 * cnty );
		for ( int i = 0; i < cnty; ++i )
		{
			pts.push_back( CPoint( 0, i * ptGridSpacing.y ) );
			pts.push_back( CPoint( sizeContainer.cx, i * ptGridSpacing.y ) );
		}
		count.resize( pts.size() / 2, 2 );
		pDC->PolyPolyline( &pts[0], &count[0], count.size() );
	}
	pDC->SelectObject( pPenOld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch ( nChar )
	{
		case VK_DELETE:
			DeleteActiveControls();
			break;
	}
	Invalidate( FALSE );
	CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::PostNcDestroy() 
{
	
//	CScrollView::PostNcDestroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline vector<CUIView::SRect>::iterator CUIView::GetRect( int nID )
{
	vector<SRect>::iterator i;
	for ( i = rects.begin(); i != rects.end(); ++i )
		if ( i->nID == nID )
			break;
	return i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::OnDraw( CDC* pDC )
{
	CRect r, rclient;
  CDC dcBuf;
  CBitmap bmp;
	CPoint ptScroll = GetScrollPosition();

  GetClientRect( &rclient );
	CRect rBuf( 0, 0, Max( ptScroll.x + rclient.right, sizeContainer.cx ), Max( ptScroll.y + rclient.bottom, sizeContainer.cy ) );
  bmp.CreateCompatibleBitmap( pDC, rBuf.right, rBuf.bottom );
  dcBuf.CreateCompatibleDC( pDC );
	dcBuf.SetMapMode( pDC->GetMapMode() );
	dcBuf.SetViewportOrg( pDC->GetViewportOrg() );
	dcBuf.SetBkMode( TRANSPARENT );
	dcBuf.SetTextAlign( TA_CENTER );
	dcBuf.SetTextColor( RGB( 70, 60, 70 ) );
	CBitmap *pOldBmp = dcBuf.SelectObject( &bmp );
	CFont *pOldFont = dcBuf.SelectObject( &m_defFont );
	r = rclient + ptScroll;
  dcBuf.FillSolidRect( &r, GetSysColor( COLOR_APPWORKSPACE ) );

	if ( sizeContainer != CSize(0,0) )
	{
		r = CRect( CPoint( 0, 0 ), sizeContainer );
		dcBuf.FillSolidRect( &r, GetSysColor( COLOR_WINDOW ) );
		for ( int i = rects.size() - 1; i >= 0; --i )
		{
			int nDepth = rects[i].pCtrl->m_nDepth;
			if ( nDepth >= 0 && nDepth < 8 )
			{
				if ( !bLayers[nDepth] )
					continue;
			}
			rects[i].pCtrl->CheckRect();
			rects[i].track.m_rect = rects[i].pCtrl->GetRect();
			rects[i].track.Draw( &dcBuf, true, nDepth );
		}
		if ( bGrid )
			DrawGrid( &dcBuf );
		for ( int i = rects.size() - 1; i >= 0; --i )
		{
			int nDepth = rects[i].pCtrl->m_nDepth;
			if ( nDepth >= 0 && nDepth < 8 )
			{
				if ( !bLayers[nDepth] )
					continue;
			}
			rects[i].pCtrl->CheckRect();
			rects[i].track.m_rect = rects[i].pCtrl->GetRect();
			rects[i].track.Draw( &dcBuf, false );
		}
		// активные объекты прорисовывыем сверху
		for ( int i = 0; i < activeRects.size(); ++i )
		{
			vector<SRect>::iterator it = GetRect( activeRects[i] );
			if ( it != rects.end() )
				it->track.Draw( &dcBuf, false );
		}
	}
  pDC->BitBlt( 0, 0, rBuf.right, rBuf.bottom, &dcBuf, 0, 0, SRCCOPY );
  dcBuf.SelectObject( pOldBmp );
	dcBuf.SelectObject( pOldFont );
  bmp.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CString GetStrType( CUIControl *pCtrl )
{
	const CPropMap *pProps = pCtrl->GetPropMap();
	CPropMap::const_iterator it = pProps->find( "Type" );
	return it != pProps->end() ? (const char*)it ->second->GetValue() : "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ControlCmp( const CUIView::SRect &r1, const CUIView::SRect &r2 )
{
	if ( !IsValid( r1.pCtrl ) || !IsValid( r2.pCtrl ) )
		return false;
	return r1.pCtrl->m_nDepth > r2.pCtrl->m_nDepth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::SetUIContainer( int nUIContainerID, const CPropMap *pProps )
{
	pUICProps = pProps;
	ClearActiveControls();
	rects.clear();
	pContainer = 0;
	sizeContainer = CSize( 0, 0 );
//	bGrid = false;
	SetScrollSizes( MM_TEXT, sizeContainer );
	Invalidate( FALSE );
	//
	if ( nUIContainerID <= 0 )
		return;
	pContainer = new CUIContainer;
	if ( !pContainer->Init( nUIContainerID ) )
	{
		pContainer = 0;
		return;
	}
	if ( pProps )
	{
		CPropMap::const_iterator iw = pProps->find( "Width" );
		CPropMap::const_iterator ih = pProps->find( "Height" );
		if ( iw != pProps->end() && ih != pProps->end() )
			sizeContainer = CSize( iw->second->GetValue(), ih->second->GetValue() );
		SetScrollSizes( MM_TEXT, sizeContainer );
		/*
		if ( sizeContainer.cx < NX_STEPS && sizeContainer.cy < NY_STEPS )
			bGrid = false;
		else
			bGrid = true;
		ptGridSpacing = CPoint( sizeContainer.cx / NX_STEPS, sizeContainer.cy / NY_STEPS );
		*/
	}
	//
	vector< CPtr<CUIControl> > &ctrls = pContainer->GetControls();
	for ( int i = 0; i < ctrls.size(); ++i )
	{
		CUIControl *pC = ctrls[i];
		if ( !pC )
			continue;
		int nID = ++nMaxID;
		CString str = GetStrType( pC ) + ": " + pC->m_szID;
		rects.push_back( SRect( nID, pC, CTracker( pC->GetRect(), TRACKER_STYLE, str, ptGridSpacing, this ) ) );
	}
	sort( rects.begin(), rects.end(), ControlCmp );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CPoint ptScroll = GetScrollPosition();
	if ( UI_SELECT != nUIControlMode )
	{
		AddNewControl( point + ptScroll );
		nUIControlMode = UI_SELECT;
		return;
	}
	//
	bool bHit = false;
	for ( int i = 0; i < activeRects.size(); ++i )
	{
		vector<SRect>::iterator it = GetRect( activeRects[i] );
		if ( it == rects.end() )
			continue;
		CTracker &tr = it->track;
		int nDepth = it->pCtrl->m_nDepth;
		if ( nDepth >= 0 && nDepth < 8 )
		{
			if ( !bLayers[nDepth] )
				continue;
		}
		if ( tr.HitTest( point + ptScroll ) != -1 )
		{
			if ( IsValid( it->pCtrl ) && !it->pCtrl->IsFreeze() && tr.Track( this, point + ptScroll ) )
			{
				it->pCtrl->SetRect( tr.m_rect );
				tr.m_rect = it->pCtrl->GetRect();
				Invalidate( FALSE );
			}
			bHit = true;
			break;
		}
	}
	if ( !bHit )
	{
		sort( rects.begin(), rects.end(), ControlCmp );
		for ( int i = 0; i < rects.size(); ++i )
		{
			int nDepth = rects[i].pCtrl->m_nDepth;
			if ( nDepth >= 0 && nDepth < 8 )
			{
				if ( !bLayers[nDepth] )
					continue;
			}
			CTracker &tr = rects[i].track;
			if ( tr.HitTest( point + ptScroll ) != -1 )
			{
				SetActiveControl( rects[i].nID );
				if ( IsValid( rects[i].pCtrl ) && !rects[i].pCtrl->IsFreeze() && tr.Track( this, point + ptScroll ) )
				{
					//tr.m_rect += ptScroll;
					rects[i].pCtrl->SetRect( tr.m_rect );
					tr.m_rect = rects[i].pCtrl->GetRect();
					Invalidate( FALSE );
				}
				bHit = true;
				break;
			}
		}
	}
	if ( !bHit )
	{
		ClearActiveControls();
	}
	
	CScrollView::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUIView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	point += GetScrollPosition();

	for ( int i = 0; i < rects.size(); ++i )
		if ( rects[i].track.SetCursor( point, nHitTest ) )
			return true;
	
	return CScrollView::OnSetCursor(pWnd, nHitTest, message);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::ClearActiveControl( int nRectInd )
{
	for ( vector<int>::iterator i = activeRects.begin(); i != activeRects.end(); ++i )
		if ( *i == nRectInd )
		{
			activeRects.erase( i );
			vector<SRect>::iterator it = GetRect( *i );
			if ( it != rects.end() )
			{
				it->track.m_nStyle = TRACKER_STYLE;
				Invalidate( FALSE );
			}
			return;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::ClearActiveControls()
{
	for ( int i = 0; i < activeRects.size(); ++i )
		ClearActiveControl( activeRects[i] );
	theApp.SetPropMap( pUICProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::SetActiveControl( int nRectID )
{
	ClearActiveControls();
	if ( AddActiveControl( nRectID ) )
		theApp.SetPropMap( GetRect( nRectID )->pCtrl->GetPropMap() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIView::AddActiveControl( int nRectID )
{
	vector<SRect>::iterator i = GetRect( nRectID );
	if ( i == rects.end() )
		return false;

	i->track.m_nStyle = ATRACKER_STYLE;
	activeRects.push_back( nRectID );
	Invalidate( FALSE );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::AddNewControl( const CPoint &pt )
{
	if ( UI_SELECT == nUIControlMode || !IsValid( pContainer ) )
		return;
	CPoint ptGrid( GetNearestPos( pt.x, ptGridSpacing.x ), GetNearestPos( pt.y, ptGridSpacing.y ) );
	CSize  size( 3 * ptGridSpacing.x, 3 * ptGridSpacing.y );
	CUIControl *pCtrl = pContainer->AddControl( (NDb::EUIControl)nUIControlMode, CRect( ptGrid, size ) );
	if ( pCtrl )
	{
		int nID = ++nMaxID;
		rects.push_back( SRect( nID, pCtrl, CTracker( pCtrl->GetRect(), TRACKER_STYLE, GetStrType( pCtrl ), ptGridSpacing, this ) ) );
		SetActiveControl( nID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::DeleteActiveControls()
{
	vector<int> todelete = activeRects;
	ClearActiveControls();
	//
	if ( !IsValid( pContainer ) )
		return;
	for ( int i = 0; i < todelete.size(); ++i )
	{
		vector<SRect>::iterator it = GetRect( todelete[i] );
		if ( it != rects.end() && pContainer->RemoveControl( it->pCtrl ) )
			it->pCtrl = 0;
	}
	//
	for ( vector<SRect>::iterator it = rects.begin(); it != rects.end(); )
		if ( !IsValid( it->pCtrl ) )
			it = rects.erase( it );
		else
			++it;
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::OnContextMenu( CWnd *pWnd, CPoint point )
{
	ptRightClick = point;
	ScreenToClient( &ptRightClick );
	ptRightClick += GetScrollPosition();
	//ClientToScreen( &point );
	m_popup.TrackPopupMenu( TPM_LEFTBUTTON, point.x, point.y, this );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIView::GetHitRect( CPoint point )
{
	for ( int i = 0; i < rects.size(); ++i )
	{
		int nDepth = rects[i].pCtrl->m_nDepth;
		if ( nDepth >= 0 && nDepth < 8 )
		{
			if ( !bLayers[nDepth] )
				continue;
		}
		if ( rects[i].track.HitTest( point ) != -1 )
			return i;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIView::OnImageSize()
{
	int nInd = GetHitRect( ptRightClick );
	if ( nInd < 0 )
		return;
	if ( rects[nInd].pCtrl->SyncWithImageSize() )
	{
		Invalidate();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUILayersDlg dialog
CUILayersDlg::CUILayersDlg(): CPropertyPage(CUILayersDlg::IDD)
, m_bShowPreview(FALSE)
{
	//{{AFX_DATA_INIT(CUILayersDlg)
	m_0 = TRUE;
	m_1 = TRUE;
	m_2 = TRUE;
	m_3 = TRUE;
	m_4 = TRUE;
	m_5 = TRUE;
	m_6 = TRUE;
	m_7 = TRUE;
	//}}AFX_DATA_INIT
}


void CUILayersDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUILayersDlg)
	DDX_Check(pDX, IDC_UILAYER_0, m_0);
	DDX_Check(pDX, IDC_UILAYER_1, m_1);
	DDX_Check(pDX, IDC_UILAYER_2, m_2);
	DDX_Check(pDX, IDC_UILAYER_3, m_3);
	DDX_Check(pDX, IDC_UILAYER_4, m_4);
	DDX_Check(pDX, IDC_UILAYER_5, m_5);
	DDX_Check(pDX, IDC_UILAYER_6, m_6);
	DDX_Check(pDX, IDC_UILAYER_7, m_7);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_UISHOWPREVIEW, m_bShowPreview);
}


BEGIN_MESSAGE_MAP(CUILayersDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CUILayersDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CUILayersDlg message handlers
void CUILayersDlg::OnOK() 
{
	UpdateData();
	bLayers[0] = m_0;
	bLayers[1] = m_1;
	bLayers[2] = m_2;
	bLayers[3] = m_3;
	bLayers[4] = m_4;
	bLayers[5] = m_5;
	bLayers[6] = m_6;
	bLayers[7] = m_7;
	gbIterfacePreview = m_bShowPreview;
	theApp.WriteProfileInt( "", REG_INTERFACEPREVIEW, m_bShowPreview );
	CDialog::OnOK();
	pUIWnd->Invalidate(TRUE);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUILayersDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	ASSERT( bLayers.size() == 8 );
	m_0 = bLayers[0];
	m_1 = bLayers[1];
	m_2 = bLayers[2];
	m_3 = bLayers[3];
	m_4 = bLayers[4];
	m_5 = bLayers[5];
	m_6 = bLayers[6];
	m_7 = bLayers[7];
	m_bShowPreview = gbIterfacePreview;
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
