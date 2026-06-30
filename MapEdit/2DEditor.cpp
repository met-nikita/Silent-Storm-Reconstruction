#include "StdAfx.h"
#include "2DEditor.h"
#include "PaintBar.h"
#include "MapEdit.h"
#include "MainFrm.h"
#include "dbDefs.h"
#include "ItemsMgr.h"

const COLORREF DEF_TERR_COLOR = RGB( 0, 0, 0 );
BEGIN_MESSAGE_MAP(C2DEditor,CWnd )
	//{{AFX_MSG_MAP(C2DEditor)
	ON_WM_LBUTTONDOWN()
	ON_WM_SHOWWINDOW()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
CMaskBrush::CMaskBrush()
{
	Create( BS_COS, 1, 1.0f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMaskBrush::CMaskBrush( EStyle nStyle, int fRadius, float fHardness )
{
	Create( nStyle, fRadius, fHardness );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaskBrush::Create( EStyle style, int radius, float hardness )
{
	nStyle = style;
	nRadius = radius;
	fHardness = hardness;
	mask.SetSizes( 2 * nRadius, 2 * nRadius );
	mask.FillZero();

	switch ( nStyle )
	{
		case BS_COS:
			CosineProfile();
			break;
		case BS_BELL:
			BellProfile();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaskBrush::CosineProfile()
{
	int nWidth = mask.GetXSize();
	float fStep = 1.0f / (nRadius - 1);
	int i, j;
	float fx, fy;

	for ( j = 0, fy = 0; j < nRadius; fy += fStep, ++j )
		for ( i = 0, fx = 0; i < nRadius; fx += fStep, ++i )
		{
			float fVal = fabs( fx, fy );
			if ( fVal > 1 )
				continue;
			fVal = cos( FP_PI2 * fVal );
			mask[nRadius - j - 1][nRadius - i - 1] = fVal;
			mask[nRadius - j - 1][nRadius + i] = fVal;
			mask[nRadius + j][nRadius - i - 1] = fVal;
			mask[nRadius + j][nRadius + i] = fVal;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaskBrush::BellProfile()
{
	int nWidth = mask.GetXSize();
	float fStep = 1.0f / (nRadius - 1);
	int i, j;
	float fx, fy;
	float fCoeff = -1.0f / sqrt( nRadius );
	
	for ( j = 0, fy = 0; j < nRadius; fy += fStep, ++j )
		for ( i = 0, fx = 0; i < nRadius; fx += fStep, ++i )
		{
			float fVal = fabs( fx, fy );
			if ( fVal > 1 )
				continue;
			//fVal = exp( -16.0f * sqr( sqr( sqr( fVal ) ) ) );
			fVal = exp( -2.0f * sqr( fVal )  );
			mask[nRadius - j - 1][nRadius - i - 1] = fVal;
			mask[nRadius - j - 1][nRadius + i] = fVal;
			mask[nRadius + j][nRadius - i - 1] = fVal;
			mask[nRadius + j][nRadius + i] = fVal;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline COLORREF Blend( float fAlpha, int srcR, int srcG, int srcB, COLORREF dst )
{
	float f1Alpha = 1.0f - fAlpha;
	return RGB( fAlpha * srcR + f1Alpha * GetRValue( dst ),
							fAlpha * srcG + f1Alpha * GetGValue( dst ),
							fAlpha * srcB + f1Alpha * GetBValue( dst ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaskBrush::Paint( CDC *pDC, COLORREF cr, int x, int y ) const
{
	int i, j;
	int ix, iy;
	int srcR = GetRValue( cr );
	int srcG = GetGValue( cr );
	int srcB = GetBValue( cr );

	for ( j = 0, iy = y - nRadius; iy < y + nRadius; ++iy, ++j )
		for ( i = 0, ix = x - nRadius; ix < x + nRadius; ++ix, ++i )
			pDC->SetPixelV( ix, iy, Blend( mask[j][i], srcR, srcG, srcB, pDC->GetPixel( ix, iy ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
C2DEditor::C2DEditor()
{
	nWidth  = 0;
	nHeight = 0;
	fScale = 1.0f;
	currentColor = RGB( 0, 0, 0 );
	dc.CreateCompatibleDC( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
C2DEditor::~C2DEditor()
{
	bitmap.DeleteObject();
	dc.DeleteDC();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::SetImage( int _nWidth, int _nHeight, const CArray2D<BYTE> *pColors, COLORREF cr )
{
	Clear();
	nWidth  = _nWidth;
	nHeight = _nHeight;
	bitmap.DeleteObject();
	bitmap.CreateBitmap( nWidth, nHeight, 1, 32, 0 );
	dc.SelectObject( &bitmap );
	int i, j;
	if ( pColors )
	{
		for ( j = 0; j < nHeight; ++j )
			for ( i = 0; i < nWidth; ++i )
				dc.SetPixelV( i, j, GetColor4Tile( (*pColors)[j][i] ) );
	}
	else
	{
		COLORREF color = cr == -2 ? DEF_TERR_COLOR : cr;
		for ( j = 0; j < nHeight; ++j )
			for ( i = 0; i < nWidth; ++i )
				dc.SetPixelV( i, j, color );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBitmap* C2DEditor::GetBitmap( int nZoom )
{
	ASSERT( nZoom >= 0 );

	return &bitmap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDC* C2DEditor::GetDC()
{
	return &dc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// true, если значение цвета в точке x, y изменилось
bool C2DEditor::SetPixel( int x, int y, DWORD cr )
{
	y = nHeight - y - 1;
	DWORD old = dc.GetPixel( x, y );
	return old != dc.SetPixel( x, y, cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD C2DEditor::GetPixel( CPoint pt ) const
{
	pt.y = nHeight - pt.y - 1;
	return dc.GetPixel( pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CWnd ::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CWnd ::OnShowWindow(bShow, nStatus);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int C2DEditor::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd ::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::SetCurrentColor( COLORREF cr )
{
	currentColor = cr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
COLORREF C2DEditor::GetColor4Tile( int nTileID )
{
	unordered_map<int, COLORREF>::const_iterator it = terrTileID2Color.find( nTileID );
	if ( it != terrTileID2Color.end() )
		return it->second;
	const SResTree *pRes = theApp.GetResTree( IDC_TERRAINTILES_TREE );
	if ( !pRes || !pRes->pItemsTree )
		return DEF_TERR_COLOR;
	const CPropMap *pProps = pRes->pItemsTree->GetPropList( nTileID );
	if ( !pProps )
		return DEF_TERR_COLOR;
	CPropMap::const_iterator cit = pProps->find( "UserColor" );
	if ( cit == pProps->end() )
		return DEF_TERR_COLOR;
	int cr = cit->second->GetValue();
	COLORREF crold = dc.GetPixel( 0, 0 );
	cr = dc.SetPixel( 0, 0, cr );
	dc.SetPixel( 0, 0, crold );
	terrTileID2Color[nTileID] = cr;
	color2TerrTileID[cr] = nTileID;
	return cr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int C2DEditor::GetTile4Color( COLORREF cr ) const
{
	unordered_map<COLORREF, int>::const_iterator it = color2TerrTileID.find( cr );
	if ( it != color2TerrTileID.end() )
		return it->second;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::Clear()
{
	terrTileID2Color.clear();
	color2TerrTileID.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool C2DEditor::SetTile( int x, int y, int nTileID )
{
	return SetPixel( x, y, GetColor4Tile( nTileID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::FillTile( int x, int y, int nTileID )
{
	y = nHeight - y - 1;
	CBrush br( GetColor4Tile( nTileID ) );
	CBrush *pOldBr = dc.SelectObject( &br );
	COLORREF crSeed = dc.GetPixel( x, y );
	dc.ExtFloodFill( x, y, crSeed, FLOODFILLSURFACE );
	dc.SelectObject( pOldBr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::Fill( int x, int y, DWORD cr )
{
	y = nHeight - y - 1;
	CBrush br( cr );
	CBrush *pOldBr = dc.SelectObject( &br );
	COLORREF crSeed = dc.GetPixel( x, y );
	dc.ExtFloodFill( x, y, crSeed, FLOODFILLSURFACE );
	dc.SelectObject( pOldBr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DEditor::PaintBrush( const CMaskBrush &brush, COLORREF cr, int x, int y )
{
	y = nHeight - y - 1;
	brush.Paint( &dc, cr, x, y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
