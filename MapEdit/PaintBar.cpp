#include "StdAfx.h"
#include "PaintBar.h"
#include "resource.h"

BEGIN_MESSAGE_MAP(CPaintBar, SECCustomToolBar )
//{{AFX_MSG_MAP(CPaintBar)
	ON_WM_CREATE()
	ON_WM_CHILDACTIVATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CPaintBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECCustomToolBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	if ( !LoadToolBar(IDR_PAINTBAR) )
		return -1;
	return 0;
}


void CPaintBar::OnChildActivate() 
{
	SECCustomToolBar ::OnChildActivate();
}
/*
void CPaintBar::SetCurrentColor( COLORREF cr )
{
	HBITMAP hBmp;
	IDToBmpIndex( ID_PAINT_PALETTE, &hBmp );
	CRect r;
	GetItemRect( CommandToIndex( ID_PAINT_PALETTE ), &r );
	if ( !hBmp )
		return;
	CBitmap *pBmp = CBitmap::FromHandle( hBmp );
	if ( !pBmp )
		return;
	BITMAP bmpi;
	pBmp->GetBitmap( &bmpi );
	CDC *pDC = GetDC();
	BITMAPINFO bmi;
	memset( &bmi, 0, sizeof( bmi ) );
	const int nw = bmpi.bmWidth;
	const int nh = bmpi.bmHeight;
	bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bmi.bmiHeader.biWidth  = nw;
	bmi.bmiHeader.biHeight = nh;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	
	DWORD rgb = GetBValue( cr ) | GetGValue( cr ) << 8 | GetRValue( cr ) << 16;

	vector<DWORD> bits( bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight, rgb );
	memset( &bits[0], 0, nw * sizeof( DWORD ) );
	memset( &bits[nw * (nh - 1)], 0, nw * sizeof( DWORD ) );
	for ( int i = 0; i < nh; ++i )
	{
		bits[i * nw] = 0;
		bits[i * nw + nw - 1] = 0;
	}
	int nScans = SetDIBits( pDC->m_hDC, hBmp, 0, nh, &bits[0], &bmi, DIB_RGB_COLORS );
	
	ReleaseDC( pDC );
	Invalidate( FALSE );
}
*/