// LayerCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "LayerCtrl.h"
#include "LayerList.h"
#include "iWysiwyg.h"
#include "Placement.h"
#include "..\Input\Bind.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
// CCBrushButton
BEGIN_MESSAGE_MAP(CBrushButton, CButton)
//{{AFX_MSG_MAP(CBrushButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CBrushButton::CBrushButton()
{
	crBtn = GetSysColor( COLOR_BTNFACE );
	szBtnName = "...";
	if ( !m_font.m_hObject )
	{
		// Create font
		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
		lf.lfHeight = -10;							// request a ?-pixel-height font
		strcpy( lf.lfFaceName, "Microsoft Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
		m_font.CreateFontIndirect(&lf);			// create the fonts
	}
}

CBrushButton::~CBrushButton()
{
	m_font.DeleteObject();
}

void CBrushButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	CDC *pDC = CDC::FromHandle( lpDrawItemStruct->hDC );
	CBrush brush( crBtn );
	CBrush *pOldBrush = pDC->SelectObject( &brush );
	CRect r = lpDrawItemStruct->rcItem;
	pDC->Rectangle( &r );

	pDC->SetTextAlign(TA_LEFT);
	pDC->SetBkMode(TRANSPARENT);
	CSize size = pDC->GetTextExtent( szBtnName );
	pDC->SetTextColor( RGB( 10, 20, 10 ) );
	int x = Max( 0, int(r.Width() - size.cx) / 2 );
	pDC->ExtTextOut( x, (r.Height() - size.cy) / 2, ETO_CLIPPED, &r, szBtnName,	0 );
	pDC->SelectObject( pOldBrush );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLinkButton
BEGIN_MESSAGE_MAP(CLinkButton, CButton)
//{{AFX_MSG_MAP(CLinkButton)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CLinkButton::CLinkButton()
{
	bmplink.LoadBitmap( IDB_LINK );
}

CLinkButton::~CLinkButton()
{
	bmplink.DeleteObject();
}

void CLinkButton::OnPaint() 
{
	//CPaintDC dc(this); // device context for painting

	SetCheck( 2 );
	CButton::OnPaint();

	if ( true || GetCheck() )
	{
		CDC *pDC = GetDC();
		CDC bmp;
		bmp.CreateCompatibleDC( pDC );
		CBitmap *pBmpOld = bmp.SelectObject( &bmplink );
		//pDC->BitBlt( 2, 3, 11, 11, &bmp, 0, 0, SRCPAINT );
		pDC->BitBlt( 2, 3, 11, 11, &bmp, 0, 0, SRCPAINT );
		bmp.SelectObject( pBmpOld );
		ReleaseDC( pDC );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerCtrl dialog
CLayerCtrl::CLayerCtrl( int nLayerType, int nLayerInd, CString szName )
	: CDialog(CLayerCtrl::IDD, 0 )
{
	//{{AFX_DATA_INIT(CLayerCtrl)
	m_szName = _T("");
	m_bVisible = FALSE;
	m_bLink = FALSE;
	//}}AFX_DATA_INIT
	SetLayerName( (LPCTSTR)szName );
	bActive = false;
	SetLayerID( nLayerType, nLayerInd );
}

void CLayerCtrl::SetLayerID( int nTypeID, int nLayerInd ) 
{ 
	nLayerID = NBuilding::MakeFragmentID( (ELayer)nTypeID, nLayerInd );
}

int CLayerCtrl::GetLayerType() const 
{ 
	ELayer type;
	int nLayer;

	NBuilding::GetLayerID( nLayerID, &type, &nLayer );
	return type;
}

CLayerCtrl::~CLayerCtrl()
{
	m_font.DeleteObject();
	m_boldFont.DeleteObject();
}

void CLayerCtrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLayerCtrl)
	DDX_Control(pDX, IDC_LINK, m_linkCtrl);
	DDX_Control(pDX, IDC_LAYER_NAME, m_Name);
	DDX_Control(pDX, IDC_BRUSH_SELECT, m_btnBrush);
	DDX_Text(pDX, IDC_LAYER_NAME, m_szName);
	DDX_Check(pDX, IDC_VISIBLE, m_bVisible);
	DDX_Check(pDX, IDC_LINK, m_bLink);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CLayerCtrl, CDialog)
	//{{AFX_MSG_MAP(CLayerCtrl)
	ON_BN_CLICKED(IDC_BRUSH_SELECT, OnBrushSelect)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_BN_CLICKED(IDC_VISIBLE, OnVisible)
	ON_BN_CLICKED(IDC_LINK, OnLink)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::Activate( bool bActivate )
{
	if ( bActivate != bActive )
		Invalidate();
	bActive = bActivate;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::SendNotify( WPARAM wParam )
{
	if ( !::IsWindow( m_hWnd ) )
		return;
	CWnd *pW = GetParent();
	if ( pW )
		pW->PostMessage( WM_USER_NOTIFY, wParam );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerCtrl message handlers
void CLayerCtrl::OnBrushSelect() 
{
	GetParent()->PostMessage( WM_USER_SELECT, nLayerID );
	BrowseBrush();
	if ( GetParent() )
		GetParent()->Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

	CRect r;
	CFont *pOldFont = 0;
	if ( bActive )
	{
		GetClientRect( &r );
		dc.FillSolidRect( &r, GetSysColor( COLOR_HIGHLIGHT ) );
		pOldFont = dc.SelectObject( &m_boldFont );
	}
	else
	{
		pOldFont = dc.SelectObject( &m_font );
	}
	m_Name.GetWindowRect( &r );
	ScreenToClient( &r );
	if ( nLayerID == 11 )
		int v = 0;
	dc.TextOut( r.left, r.top, szLayerName );
	dc.SelectObject( pOldFont );
	CPen pen( PS_SOLID, 1, RGB(255,255,255) );
	CPen *pOldPen = dc.SelectObject( &pen );
	CGdiObject *pOldBrush = dc.SelectStockObject( HOLLOW_BRUSH );
	GetClientRect( &r );
	r.InflateRect( 1, 0, 1, 1 );
	dc.Rectangle( &r );
	dc.SelectObject( pOldPen );
	dc.SelectObject( pOldBrush );
	CPen black( PS_SOLID, 1, RGB(0,0,0) );
	pOldPen = dc.SelectObject( &black );
	GetClientRect( &r );
	if ( bFirst )
	{
		dc.MoveTo( r.left, r.top );
		dc.LineTo( r.right, r.top );
	}
	if ( bLast )
	{
		dc.MoveTo( r.left, r.bottom - 1 );
		dc.LineTo( r.right, r.bottom - 1 );
	}
	dc.SelectObject( pOldPen );
	// Do not call CDialog::OnPaint() for painting messages
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	GetParent()->PostMessage( WM_USER_SELECT, nLayerID );
	CDialog::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CLayerCtrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_btnBrush.SetFont( &m_btnBrush.m_font );
	
	CFont *pTempF = m_btnBrush.GetFont();
	if ( pTempF )
	{
		LOGFONT lf;
		pTempF->GetLogFont( &lf );
		m_font.CreateFontIndirect( &lf );
		lf.lfWeight = FW_DEMIBOLD;
		m_boldFont.CreateFontIndirect( &lf );
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::SetBrush( COLORREF cr, CString szName )
{
	m_btnBrush.szBtnName = szName;
	m_btnBrush.crBtn = cr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::OnCancel() 
{
}
void CLayerCtrl::OnOK() 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerCtrl::IsVisible() const
{ 
	if ( IsWindow( m_hWnd ) )
		const_cast<CLayerCtrl*>(this)->UpdateData();
	return m_bVisible; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::SetVisible( bool bVisible )
{
	if ( bVisible != (bool)m_bVisible )
	{
		NInput::PostEvent( "update" );
		Repaint();
	}
	m_bVisible = bVisible;
	UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::OnVisible() 
{
//	NInput::PostEvent( "update" );
//	NMainLoop::StepApp( true );
	UpdateData();
	theApp.WriteProfileInt( "Layers", IToA( GetLayerID() ).c_str(), m_bVisible );
	Repaint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::OnLink() 
{
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_LINK, GetLayerID() );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerCtrl::GetLink()
{
	UpdateData();
	return m_bLink;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerCtrl::SetLink( bool bLink )
{
	m_bLink = bLink;
	UpdateData(	FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
HCURSOR CreateCursor( LPTSTR lpSysCursorName, UINT nExtraCursorID, float fAlpha )
{
	ICONINFO infoSys, infoExtra;
	
	HCURSOR hSys = LoadCursor( 0, lpSysCursorName );
	HCURSOR hExtra = theApp.LoadCursor( nExtraCursorID );
	if ( !GetIconInfo( (HICON)hSys, &infoSys ) || infoSys.fIcon )
		return 0;
	if ( !GetIconInfo( (HICON)hExtra, &infoExtra ) || infoExtra.fIcon )
		return 0;
	int nWidth  = GetSystemMetrics( SM_CXCURSOR );
	int nHeight = GetSystemMetrics( SM_CYCURSOR );
	HDC hdcSrc = CreateCompatibleDC( 0 );
	HDC hdcDst = CreateCompatibleDC( 0 );

	if ( !hdcSrc || !hdcDst )
		return 0;
	
	HGDIOBJ pOldSrc = SelectObject( hdcSrc, infoExtra.hbmMask );
	HGDIOBJ pOldDst = SelectObject( hdcDst, infoSys.hbmMask );
	BitBlt( hdcDst, 0, 0, nWidth, nHeight, hdcSrc, 0, 0, SRCAND );
	SelectObject( hdcDst, infoSys.hbmColor );
	SelectObject( hdcSrc, infoExtra.hbmColor );
	BitBlt( hdcDst, 0, 0, nWidth, nHeight, hdcSrc, 0, 0, SRCPAINT );
	SelectObject( hdcSrc, pOldSrc );
	SelectObject( hdcDst, pOldDst );
/*
	BITMAPINFO bmi;
	Zero( bmi );
	bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
	if ( GetDIBits( hdcDst, infoSys.hbmColor, 0, nHeight, 0, &bmi, DIB_RGB_COLORS ) 
		&& bmi.bmiHeader.biSizeImage )
	{
		vector<COLORREF> bits( bmi.bmiHeader.biSizeImage );
		if ( GetDIBits( hdcSrc, infoSys.hbmColor, 0, nHeight, &bits[0], &bmi, DIB_RGB_COLORS ) )
		{
			int nAlpha = fAlpha * 255;
			nAlpha = Min( nAlpha, 255 );
			nAlpha = Max( 0, nAlpha );
			nAlpha <<= 24;
			
			int i = 0;
			for ( int y = bmi.bmiHeader.biHeight - 1; y >= 0; --y )
				for ( int x = 0; x < bmi.bmiHeader.biWidth; ++x )
				{
					//if ( bits[i] )
					if ( y < 16 )
						bits[i] |= nAlpha;
					++i;
				}
				SetDIBits( hdcSrc, infoSys.hbmColor, 0, nHeight, &bits[0], &bmi, DIB_RGB_COLORS );
		}
	}
*/
	DeleteDC( hdcSrc );
	DeleteDC( hdcDst );

	return CreateIconIndirect( &infoSys );
}
