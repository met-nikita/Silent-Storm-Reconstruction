#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "Placement.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\TerrainInfo.h"
#include "..\Main\BuildingInfo.h"
#include "TerrainLayer.h"
#include "..\Main\Grid.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "TreeSelItemdlg.h"
#include "SectorCtrl.h"
#include "TerrainExportDlg.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"
#include "MEParams.h"
#include "iWysiwyg.h"
#include "WysiwygSelection.h"
#include "UserSettingsSetup.h"
#include "NewHoleDlg.h"

const float SEGMENT_LEN = 3.0f;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainHole: public CSectorCtrl
{
	OBJECT_BASIC_METHODS(CTerrainHole);
public:
	int nHeight;
	bool bVisible;

	CTerrainHole( const STerrainHole &hole, CPlacement *pPl );
	CTerrainHole( CPlacement *pPl = 0 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainHole::CTerrainHole( const STerrainHole &hole, CPlacement *pPl )
	: CSectorCtrl( pPl->GetWidth(), pPl->GetHeight(), SEGMENT_LEN, hole.vPolygon )
{
	nHeight  = hole.nHeight;
	bVisible = hole.bVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainHole::CTerrainHole( CPlacement *pPl ): CSectorCtrl( pPl->GetWidth(), pPl->GetHeight(), SEGMENT_LEN )
{
	ASSERT( pPl );
	nHeight  = 0;
	bVisible = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CHeightsLayer, CLayerCtrl)
//{{AFX_MSG_MAP(CHeightsLayer)
	ON_COMMAND(ID_EXPORTITEM, OnExport)
	ON_COMMAND(ID_IMPORTITEM, OnImport)
	ON_COMMAND(ID_NEWCONTOUR, OnNewContour)
	ON_COMMAND(ID_DELCONTOUR, OnDelContour)
	ON_COMMAND(ID_HOLEPROPS, OnHoleProps)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeightsLayer::CHeightsLayer( int nLayerType, const string &szName ) : CLayerCtrl( nLayerType, 0, szName.c_str() )
{
	pPlacement = 0;
	fMinH = 0;
	fMaxH = 1;
	fCurrentH = 0;
	bEmboss = false;
	bDrag = false;
	activeBrush.Create( CMaskBrush::BS_COS, 10, 1 );
  m_popup.CreatePopupMenu();
	if ( nLayerType == LID_HOLES )
	{
		m_popup.AppendMenu( MF_STRING, ID_NEWCONTOUR, "New hole" );
		m_popup.AppendMenu( MF_STRING, ID_DELCONTOUR, "Delete hole" );
		m_popup.AppendMenu( MF_STRING, ID_HOLEPROPS, "Hole height..." );
	}
	else
	{
		m_popup.AppendMenu( MF_STRING, ID_IMPORTITEM, "Import..." );
		m_popup.AppendMenu( MF_STRING, ID_EXPORTITEM, "Export..." );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeightsLayer::~CHeightsLayer()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnVisible()
{
	CLayerCtrl::OnVisible();
	NInput::PostEvent( "update_terrain" );
	NMainLoop::StepApp(true, true);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::Draw( const CPoint &pt, ITemplateView *pView )
{
	float x, y;
	pView->ScreenToTemplate( pt, &x, &y );
	editor.PaintBrush( activeBrush, Height2Color( fCurrentH ), x, y );
	pView->Repaint();
	pView->SetModifiedFlag();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	if ( GetLayerType() == LID_HOLES && RegionHitTest( pView, pt, &nRegInd, &nPtInd ) )
	{
		if ( ::GetCapture() != NULL )
			return;
		pView->GetWnd()->SetCapture();
		static vector<NWysiwyg::SSelectedInfo> info(1);
		info[0].eBrushType = BT_TERRHOLE;
		info[0].nObjectID = nRegInd;
		SMessage msg;
		msg.msg = MSG_SELECT;
		msg.brush = BT_TERRHOLE;
		msg.data = (DWORD)&info;
		GetUserSettingsSetup().SendMessage( msg );
		bDrag = true;
		return;
	}
	Draw( pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnLButtonUp( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	if ( bDrag )
	{
		ReleaseCapture();
		bDrag = false;
		ASSERT( nRegInd >= 0 && nRegInd < holes.size() );
		if ( IsValid( holes[nRegInd] ) )
			holes[nRegInd]->Rebuild();
		Repaint();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	pView->ScreenToTemplate( pt, &ptTempl.x, &ptTempl.y );
	pView->GetWnd()->ClientToScreen( &pt );
	m_popup.TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView )
{
	if ( bDrag )
	{
		ASSERT( nRegInd >= 0 && nRegInd < holes.size() && IsValid( holes[nRegInd] ) );
		float x, y;		
		pView->ScreenToTemplate( pt, &x, &y );
		if ( CWnd::GetCapture() != pView->GetWnd() || !holes[nRegInd]->MovePoint( nPtInd, CVec2( x, y ) ) )
		{
			bDrag = false;
			holes[nRegInd]->Rebuild();
		}
		Repaint();
		SendNotify( LLN_TERR_CHANGED );
		return;
	}
	if ( MK_LBUTTON & nFlags )
	{
		if ( !bEmboss )
			Draw( pt, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::SetCurrentH( float fH )
{
	fH = Min( fH, fMaxH );
	fCurrentH = Max( fH, fMinH );
	char pszH[32];
	sprintf( pszH, "%.2f", fCurrentH );
	SetBrush( Height2Color( fCurrentH ), pszH );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::BrowseBrush( CTerrainBrushDlg *pDlg )
{
	if ( LID_HOLES == GetLayerType() )
		return;
	pDlg->m_fMin = fMinH;
	pDlg->m_fMax = fMaxH;
	pDlg->m_fCurrent = fCurrentH;
	pDlg->m_nBrushRadius = activeBrush.GetRadius();
	pDlg->bEmboss = bEmboss;
	
	if ( IDOK != pDlg->DoModal() )
		return;
	
	if ( fMinH != pDlg->m_fMin || fMaxH != pDlg->m_fMax )
	{
		CWnd *pPrnt = GetParent();
		if ( pPrnt )
			pPrnt->PostMessage( WM_USER_NOTIFY, LLN_TERR_SERIALIZE );		
	}
	fMinH = pDlg->m_fMin;
	fMaxH = pDlg->m_fMax;
	if ( fMinH > fMaxH )
		swap( fMinH, fMaxH );
	SetCurrentH( pDlg->m_fCurrent );
	activeBrush.Create( activeBrush.GetStyle(), pDlg->m_nBrushRadius, activeBrush.GetHardness() );	
	bEmboss = pDlg->bEmboss;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::BrowseBrush()
{
	CTerrainBrushDlg dlg( this );

	BrowseBrush( &dlg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::Paint( ITemplateView *pView, float fBrightness, bool bGrayed )
{
	ASSERT( pView );
	CDC *pDC = pView->GetPaintDC();
	if ( !pDC )
		return;
	CPoint ptLT( 0, editor.GetHeight() * editor.GetScale() );
	int nSpacing = pView->GetSpacing();
	
	pView->TemplateToScreen( &ptLT, ptLT );
	int nWidth  = editor.GetWidth() * editor.GetScale() * nSpacing;
	int nHeight = editor.GetHeight() * editor.GetScale() * nSpacing;
	//pDC->StretchBlt( ptLT.x - nSpacing/2 , ptLT.y + nSpacing/2, nWidth, nHeight, editor.GetDC(), 0, 0, editor.GetWidth(), editor.GetHeight(), SRCCOPY );
	BLENDFUNCTION blend;
	blend.BlendOp = AC_SRC_OVER;
	blend.BlendFlags = 0;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = 0;
	AlphaBlend( pDC->m_hDC, ptLT.x - nSpacing/2 , ptLT.y + nSpacing/2, nWidth, nHeight, editor.GetDC()->m_hDC, 0, 0, editor.GetWidth(), editor.GetHeight(), blend );
	if ( LID_HOLES == GetLayerType() )
		for ( int i = 0; i < holes.size(); ++i )
		{
			PaintRegion( pDC, pView, holes[i] );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::SetImage( const CArray2D<WORD> *pHeights, float _fMinH, float _fMaxH )
{
	if ( !pHeights )
		return;
	fMinH = _fMinH;
	fMaxH = _fMaxH;
	int nWidth  = pHeights->GetXSize();
	int nHeight = pHeights->GetYSize();
	editor.SetImage( nWidth, nHeight );
	const float fScale = FP_TERRAIN_H_SCALE * 255.0f / (fMaxH - fMinH);
	const	float fShift = fMinH / FP_TERRAIN_H_SCALE;
	for ( int j = 0; j < nWidth; ++j )
		for ( int i = 0; i < nHeight; ++i )
		{
			int cr = Float2Int( fScale * ((*pHeights)[i][j] - fShift) );
			editor.SetPixelV( j, i, RGB( cr, cr, cr ) );
		}
	SetCurrentH( fMinH + 0.5f * (fMaxH - fMinH) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::SetHoles( const vector<STerrainHole> *pNewHoles )
{
	holes.clear();
	if ( !pNewHoles )
		return;
	for ( int i = 0; i < pNewHoles->size(); ++i )
		holes.push_back( new CTerrainHole( (*pNewHoles)[i], pPlacement ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::CreateImage( int nWidth, int nHeight, COLORREF cr )
{
	editor.SetImage( nWidth, nHeight, 0, cr );
	holes.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::GetTerrain( float *pMinH, float *pMaxH, CArray2D<WORD> *pHeights )
{
	int nw = editor.GetWidth();
	int nh = editor.GetHeight();
	const float fScale = (fMaxH - fMinH) / ( FP_TERRAIN_H_SCALE * 255.0f );
	const	float fShift = fMinH / FP_TERRAIN_H_SCALE;
	int i, j;

	pHeights->SetSizes( nw, nh );
	for ( j = 0; j < nh; ++j )
		for ( i = 0; i < nw; ++i )
			(*pHeights)[j][i] = fShift + Float2Int( fScale * GetRValue( editor.GetPixel( i, j ) ) );
	*pMinH = fMinH;
	*pMaxH = fMaxH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::GetHoles( vector<STerrainHole> *_Holes  )
{
	if ( _Holes )
	{
		_Holes->clear();
		for ( int i = 0; i < holes.size(); ++i )
		{
			STerrainHole h;
			h.bVisible = holes[i]->bVisible;
			h.nHeight  = holes[i]->nHeight;
			h.vPolygon = holes[i]->GetPoints();
			_Holes->push_back( h );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline COLORREF CHeightsLayer::Height2Color( float height )
{
	height -= fMinH;
	height *= 255.0f / (fMaxH - fMinH);
	int cr = Min( 255, (int)height );
	cr = Max( 0, cr );
	return RGB( cr, cr, cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float CHeightsLayer::Color2Height( COLORREF cr )
{
	return fMinH + ((fMaxH - fMinH) / 255.0f) * GetRValue( cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::PutRegion( const CVec2 &pt, int nRadius )
{
	CNewHoleDlg dlg;

	if ( dlg.DoModal() != IDOK )
		return;
	holes.push_back( new CTerrainHole( pPlacement ) );
	CTerrainHole *pHole = holes.back();
	pHole->CreateSector( pt, dlg.m_fRadius, dlg.m_nSegments, true );
	pHole->nHeight = dlg.m_fHeight / FP_TERRAIN_H_SCALE;
	Repaint();
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_NOTIFY, LLN_TERR_SERIALIZE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::PaintRegion( CDC *pDC, ITemplateView *pView, const CTerrainHole *pReg )
{
	if ( !pReg )
		return;
	const vector<CVec2> &vPolygon = pReg->GetPoints();
	if ( vPolygon.empty() )
		return;
	int oldMode = pDC->SetROP2( R2_COPYPEN );
	CPen pen( PS_SOLID, 2, RGB( 0, 200, 0 ) );
	CPen *pOldPen = pDC->SelectObject( &pen );
	CPoint ptStart, pt;
	pView->TemplateToScreen( &ptStart, vPolygon[0].x, vPolygon[0].y );
	pt = ptStart;
	pDC->MoveTo( pt );
	int i;
	for ( i = 1; i < vPolygon.size(); ++i )
	{
		pView->TemplateToScreen( &pt, vPolygon[i].x, vPolygon[i].y );
		pDC->LineTo( pt );
	}
	pDC->LineTo( ptStart );

	const int nSpacing = 0.1f * pView->GetSpacing();
	CPen pen1( PS_SOLID, 2, RGB( 100, 100, 255 ) );
	pDC->SelectObject( &pen1 );
	PaintCtrlPoint( pDC, ptStart, nSpacing );
	for ( i = 1; i < vPolygon.size(); ++i )
	{
		pView->TemplateToScreen( &pt, vPolygon[i].x, vPolygon[i].y );
		PaintCtrlPoint( pDC, pt, nSpacing );
	}

	pDC->SelectObject( pOldPen );
	pDC->SetROP2( oldMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeightsLayer::Export( const string &szExportDir, const string &szPrefix )
{
	string szFile = szExportDir + "\\" + szPrefix + GetLayerName() + ".bmp";

	CFileStream f;
	try
	{
		f.OpenWrite( szFile.c_str() );
		BITMAPFILEHEADER head;
		BITMAPINFOHEADER info;
		Zero( head );
		Zero( info );
		head.bfType = 0x4d42;
		head.bfSize = sizeof( head ) + sizeof( info ) + editor.GetWidth() * editor.GetWidth() * 4;
		head.bfOffBits = sizeof( head ) + sizeof( info );
		info.biSize = sizeof( info );
		info.biWidth = editor.GetWidth();
		info.biHeight = editor.GetHeight();
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biCompression = BI_RGB;
		info.biSizeImage = 0;
		info.biXPelsPerMeter = 1;
		info.biYPelsPerMeter = 1;
		info.biClrUsed = 0;
		info.biClrImportant = 0;

		f.Write( &head, sizeof( head ) );
		f.Write( &info, sizeof( info ) );
		for ( int y = 0; y < editor.GetHeight(); ++y )
			for ( int x = 0; x < editor.GetWidth(); ++x )
			{
				DWORD cr = editor.GetPixel( x, y );
				cr = RGB( GetBValue(cr), GetBValue(cr), GetRValue(cr) );
				f.Write( &cr, sizeof(cr) );
			}
	}
	catch (...)
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnExport() 
{
  char szWD[512];
  GetCurrentDirectory( sizeof(szWD) - 1, szWD );
	CTerrainExportDlg dlg;
	dlg.m_szDirectory = szExportDir.c_str();

	if ( IDCANCEL == dlg.DoModal() )
		return;
	szExportDir = dlg.m_szDirectory;
	SetCurrentDirectory( szWD );
	MSG msg;
	while( PeekMessage( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
		;
	if ( dlg.m_bExportAll )
	{
		if ( !::IsWindow( m_hWnd ) )
			return;
		CWnd *pW = GetParent();
		if ( pW )
			pW->PostMessage( WM_USER_EXPORTTERR, GetLayerID() );
	}
	else
		Export( szExportDir, IToA( pPlacement->GetID() ) + "_" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnImport()
{
  char szWD[512];
  GetCurrentDirectory( sizeof(szWD) - 1, szWD );
	CFileDialog dlg( true, 0, 0, OFN_HIDEREADONLY, "(*.bmp)|*.bmp||" );

	if ( IDOK != dlg.DoModal() )
		return;
  SetCurrentDirectory( szWD );
	//
	MSG msg;
	while( PeekMessage( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
		;		
	//	
	CString szName = dlg.GetPathName();
	HBITMAP hbm = (HBITMAP)LoadImage( 0, szName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE );
	if ( !hbm )
	{
		char pszBuf[1024];
		FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, pszBuf, sizeof( pszBuf ), 0 );
		MessageBox( pszBuf, "Error", MB_OK | MB_ICONINFORMATION );
		return;
	}
	//
	HDC hdc = CreateCompatibleDC( 0 );
	HGDIOBJ hOldObj = SelectObject( hdc, hbm );
	CDC *pDC = editor.GetDC();
	CDC *pSrcDC = CDC::FromHandle( hdc );

	ASSERT( pDC );
	pDC->BitBlt( 0, 0, editor.GetWidth(), editor.GetHeight(), pSrcDC, 0, 0, SRCCOPY );
	SelectObject( hdc, hOldObj );
	
	DeleteDC( hdc );
	DeleteObject( hbm );
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_NOTIFY, LLN_TERR_SERIALIZE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnNewContour()
{
	PutRegion( ptTempl, activeBrush.GetRadius() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnDelContour()
{
	int iReg, iPt;
	if ( RegionHitTest( ptTempl, &iReg, &iPt ) )
	{
		holes.erase( holes.begin() + iReg );
		Repaint();
		SendNotify( LLN_TERR_SERIALIZE );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::OnHoleProps()
{
	int iReg, iPt;
	if ( !RegionHitTest( ptTempl, &iReg, &iPt ) )
		return;
	CHolePropsDlg dlg;

	CTerrainHole *pHole = holes[iReg];
	dlg.m_fHeight = FP_TERRAIN_H_SCALE * pHole->nHeight;
	if ( IDOK != dlg.DoModal() )
		return;
	pHole->nHeight = dlg.m_fHeight / FP_TERRAIN_H_SCALE;
	//
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_NOTIFY, LLN_TERR_SERIALIZE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeightsLayer::RegionHitTest( const CVec2 &pt, int *pRegInd, int *pPtInd )
{
	for ( int i = 0; i < holes.size(); ++i )
	{
		*pPtInd = holes[i]->RegionHitTest( pt );
		if ( *pPtInd >= 0 )
		{
			*pRegInd = i;
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeightsLayer::RegionHitTest( ITemplateView *pView, const CPoint &pt, int *pRegInd, int *pPtInd )
{
	float x, y;
	
	pView->ScreenToTemplate( pt, &x, &y );
	CVec2 vec( x, y );

	return RegionHitTest( vec, pRegInd, pPtInd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightsLayer::SetPlacement( CPlacement *_pPlacement )
{
	pPlacement = _pPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainBrushDlg dialog
CTerrainBrushDlg::CTerrainBrushDlg(CWnd* pParent /*=NULL*/, int _nLayerID, float _fMaxHeight )
	: CDialog(CTerrainBrushDlg::IDD, pParent)
{
	fMaxHeight = _fMaxHeight;
	//{{AFX_DATA_INIT(CTerrainBrushDlg)
	m_fMax = 0.0f;
	m_fMin = 0.0f;
	m_fCurrent = 0.0f;
	m_nBrushRadius = 0;
	m_szCurrent = _T("");
	//}}AFX_DATA_INIT
	nLayerID = _nLayerID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainBrushDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainBrushDlg)
	DDX_Control(pDX, IDC_MIN_HEIGHT, m_minCtrl);
	DDX_Control(pDX, IDC_MAX_HEIGHT, m_maxCtrl);
	DDX_Control(pDX, IDC_HEIGHT_GROUP, m_heightGroup);
	DDX_Control(pDX, IDC_HEIGHT_SLIDER, m_slider);
	DDX_Text(pDX, IDC_MAX_HEIGHT, m_fMax);
	if ( fMaxHeight != -1 )
		DDV_MinMaxFloat( pDX, m_fMax, 0, fMaxHeight );
	DDX_Text(pDX, IDC_MIN_HEIGHT, m_fMin);
	DDV_MinMaxFloat(pDX, m_fMin, -100.f, 100.f);
	DDX_Text(pDX, IDC_CURRENT_COLOR, m_fCurrent);
	DDX_Text(pDX, IDC_BRUSH_RADIUS, m_nBrushRadius);
	DDV_MinMaxInt(pDX, m_nBrushRadius, 1, 128);
	DDX_Text(pDX, IDC_CURHEIGHT_TEXT, m_szCurrent);
	//}}AFX_DATA_MAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTerrainBrushDlg, CDialog)
	//{{AFX_MSG_MAP(CTerrainBrushDlg)
	ON_WM_VSCROLL()
	ON_EN_CHANGE(IDC_CURRENT_COLOR, OnChangeCurrent)
	ON_BN_CLICKED(IDC_GRASS_TYPE, OnGrassType)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainBrushDlg message handlers
const int SLIDER_RANGE = 100;
BOOL CTerrainBrushDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	switch ( nLayerID )
	{
	case LID_ALPHA:
		SetWindowText( "Alpha layer brush" );
		m_szCurrent = "Current Alpha:";
		m_heightGroup.SetWindowText( "Alpha" );
		m_minCtrl.SetReadOnly();
		m_maxCtrl.SetReadOnly();
		break;
	case LID_GRASS:
		{
			CWnd *p = GetDlgItem( IDC_GRASS_TYPE );
			if ( p )
				p->ShowWindow( SW_SHOW );
			SetWindowText( "Grass layer brush" );
			m_szCurrent = "Current Grass density:";
			m_heightGroup.SetWindowText( "Grass" );
			m_minCtrl.SetReadOnly();
			//m_maxCtrl.SetReadOnly();
		}
		break;
	default:
		m_szCurrent = "Current Height:";
		break;
	}
	m_slider.SetParent( this );
	m_slider.SetRange( 0, SLIDER_RANGE );
	m_slider.SetLineSize( 1 );
	m_slider.SetPageSize( 10 );
	m_slider.SetPos( SLIDER_RANGE * (1.0f - (m_fCurrent - m_fMin) / (m_fMax - m_fMin)) );

	UpdateData( FALSE );
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainBrushDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	UpdateData();
	if ( (void*)pScrollBar == (void*)&m_slider )
	{
		int nPos = m_slider.GetPos();
		m_fCurrent = m_fMin + (1.0f - float( nPos ) / float( SLIDER_RANGE )) * (m_fMax - m_fMin) ;
		UpdateData( false );
	}
	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainBrushDlg::OnChangeCurrent() 
{
	UpdateData();
	m_slider.SetPos( SLIDER_RANGE * (1.0f - (m_fCurrent - m_fMin) / (m_fMax - m_fMin)) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainBrushDlg::OnGrassType() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_GRASS_TREE );
	if ( !pTree )
		return;
	
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nGrassID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CAlphaLayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CAlphaLayer::CAlphaLayer(): CHeightsLayer(LID_ALPHA,"")
{
	fMinH = 0;
	fMaxH = 1;
	SetLayerID( LID_ALPHA, 0 );
	SetLayerName( "Alpha Map" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAlphaLayer::BrowseBrush()
{
	CTerrainBrushDlg dlg( this, LID_ALPHA );

	CHeightsLayer::BrowseBrush( &dlg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAlphaLayer::SetImage( const CArray2D<WORD> *pAlphas )
{
	if ( !pAlphas )
		return;
	int nWidth  = pAlphas->GetXSize();
	int nHeight = pAlphas->GetYSize();
	editor.SetImage( nWidth, nHeight );
	const float fScale = 255.0f / 0xffff;		
	for ( int j = 0; j < nWidth; ++j )
		for ( int i = 0; i < nHeight; ++i )
		{
			int cr = fScale * (*pAlphas)[i][j];
			editor.SetPixelV( j, i, RGB( cr, cr, cr ) );
		}
	SetCurrentH( 0.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAlphaLayer::GetAlpha( CArray2D<WORD> *pAlpha )
{
	int nw = editor.GetWidth();
	int nh = editor.GetHeight();
	const float fScale = float( 0xffff ) / 255;
	int i, j;
	
	pAlpha->SetSizes( nw, nh );
	for ( j = 0; j < nh; ++j )
		for ( i = 0; i < nw; ++i )
			(*pAlpha)[j][i] = fScale * GetRValue( editor.GetPixel( i, j ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CGrassLayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrassLayer::CGrassLayer( int nLayerCnt ): CHeightsLayer(LID_GRASS,""), nGrassID(-1)
{
	fMinH = 0;
	fMaxH = 1;
	fMaxDensity = 1;
	SetLayerID( LID_GRASS, nLayerCnt );
	SetGrassID( nGrassID );
	nGrassLayerInd = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::SetLayerInd( int nLayerInd )
{
	nGrassLayerInd = nLayerInd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CGrassLayer::GetGrassName()
{
	const SResTree *pTree = theApp.GetResTree( IDC_GRASS_TREE );
	if ( !pTree )
		return "";
	const char *p = pTree->pItemsTree->GetItemName( nGrassID );
	return p ? p : "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::SetGrassID( int nGrassId ) 
{ 
	nGrassID = nGrassId; 
	SetLayerName( "Grass \"" + GetGrassName() + '\"' );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float MAX_GRASS_DENSITY = 32;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::BrowseBrush()
{
	CTerrainBrushDlg dlg( this, LID_GRASS, MAX_GRASS_DENSITY );

	dlg.nGrassID = nGrassID;
	dlg.m_fMax = fMaxDensity;
	fMaxH = fMaxDensity;
	CHeightsLayer::BrowseBrush( &dlg );
	if ( dlg.nGrassID != nGrassID || fabs( dlg.m_fMax - fMaxDensity ) > FP_EPSILON )
	{
		SendNotify( LLN_TERR_SERIALIZE );
		NInput::PostEvent( "update_terrain" );
		NMainLoop::StepApp( true, true );
	}
	SetGrassID( dlg.nGrassID );
	fMaxDensity = dlg.m_fMax;
	fMaxH = fMaxDensity;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::SetCurrentH( float fH )
{
	CHeightsLayer::SetCurrentH( fH );
	const float fScale = fMaxDensity > 0 ? 255.0f / fMaxDensity : 0;
	GetUserSettingsSetup().SetParam( ME_GRASS_DENSITY, fScale * GetCurrentH() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::SetImage( const CArray2D<BYTE> *pGrass, float _fMaxDensity )
{
	if ( !pGrass )
		return;
	int nWidth  = pGrass->GetXSize();
	int nHeight = pGrass->GetYSize();
	fMaxDensity = Min( MAX_GRASS_DENSITY, Max( 0.01f, _fMaxDensity ) );
	fMaxH = fMaxDensity;
	const float fScale = 1.0f / fMaxDensity;
	editor.SetImage( nWidth, nHeight );
	for ( int j = 0; j < nWidth; ++j )
		for ( int i = 0; i < nHeight; ++i )
		{
			int cr = (*pGrass)[i][j];
			editor.SetPixelV( j, i, RGB( cr, cr, cr ) );
		}
	SetCurrentH( 0.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassLayer::GetGrass( CArray2D<BYTE> *pGrass )
{
	int nw = editor.GetWidth();
	int nh = editor.GetHeight();
	int i, j;
	
	pGrass->SetSizes( nw, nh );
	for ( j = 0; j < nh; ++j )
		for ( i = 0; i < nw; ++i )
			(*pGrass)[j][i] = GetRValue( editor.GetPixel( i, j ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CColorLayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CColorLayer::CColorLayer( COLORREF cr, float fScale, int nLayerID, int nLayerCnt, const CString &name ): CHeightsLayer(ELayer(nLayerID),""), color(cr)
{
	SetLayerID( ELayer(nLayerID), nLayerCnt );
	SetBrush( color );
	SetLayerName( (LPCTSTR)name );
	editor.SetScale( fScale );
	brushID2Width[ITemplateView::PM_PEN_W1] = 1;
	brushID2Width[ITemplateView::PM_PEN_W2] = 3;
	brushID2Width[ITemplateView::PM_PEN_W3] = 5;
	brushWidth2ID[1] = ITemplateView::PM_PEN_W1;
	brushWidth2ID[3] = ITemplateView::PM_PEN_W2;
	brushWidth2ID[5] = ITemplateView::PM_PEN_W3;
	crBrush.Create( GetBrush().GetStyle(), 1, GetBrush().GetHardness() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorLayer::BrowseBrush()
{
	CColorDialog dlg( color );

	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( dlg.m_cc.lpCustColors )
	{
		dlg.m_cc.lpCustColors[0] = color;
	}
	if ( IDOK != dlg.DoModal() )
		return;
	color = dlg.GetColor();
	SetBrush( color );
	//activeBrush.Create( activeBrush.GetStyle(), pDlg->m_nBrushRadius, activeBrush.GetHardness() );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorLayer::SetImage( const CArray2D<DWORD> *pColormap )
{
	if ( !pColormap )
		return;
	int nWidth  = pColormap->GetXSize();
	int nHeight = pColormap->GetYSize();
	//editor.SetImage( nWidth, nHeight );
	int w = Min( nWidth, editor.GetWidth() );
	int h = Min( nHeight, editor.GetHeight() );
	for ( int j = 0; j < w; ++j )
		for ( int i = 0; i < h; ++i )
		{
			COLORREF cr = (*pColormap)[i][j];
			editor.SetPixelV( j, i, RGB( GetBValue( cr ), GetGValue( cr ), GetRValue( cr ) ) );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorLayer::GetColormap( CArray2D<DWORD> *pColormap )
{
	int nw = editor.GetWidth();
	int nh = editor.GetHeight();
	int i, j;
	
	pColormap->SetSizes( nw, nh );
	for ( j = 0; j < nh; ++j )
		for ( i = 0; i < nw; ++i )
		{
			COLORREF cr = editor.GetPixel( i, j );
			(*pColormap)[j][i] = RGB( GetBValue( cr ), GetGValue( cr ), GetRValue( cr ) );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorLayer::Draw( const CPoint &pt, ITemplateView *pView )
{
	ITemplateView::EPaintMode nBID;
	pView->GetPaintMode( (int*)&nBID );
	ITemplateView::EPaintMode nCurID = (ITemplateView::EPaintMode)brushWidth2ID[crBrush.GetRadius()];
	if ( nCurID != nBID )
	{
		crBrush.Create( GetBrush().GetStyle(), brushID2Width[nBID], GetBrush().GetHardness() );
	}
	float x, y;
	pView->ScreenToTemplate( pt, &x, &y );
	float fScale = 1.0f / editor.GetScale();
	editor.PaintBrush( crBrush, color, x * fScale + 0.5f , y * fScale - 0.5f );
	pView->Repaint();
	pView->SetModifiedFlag();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorLayer::OnVisible()
{
	CLayerCtrl::OnVisible();
	NInput::PostEvent( "update_terrain" );
	NMainLoop::StepApp(true, true);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHolePropsDlg dialog


CHolePropsDlg::CHolePropsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CHolePropsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CHolePropsDlg)
	m_fHeight = 0.0f;
	//}}AFX_DATA_INIT
}


void CHolePropsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CHolePropsDlg)
	DDX_Text(pDX, IDC_TERRHOLE_HEIGHT, m_fHeight);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CHolePropsDlg, CDialog)
	//{{AFX_MSG_MAP(CHolePropsDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CHolePropsDlg message handlers
