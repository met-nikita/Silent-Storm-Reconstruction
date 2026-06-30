#include "StdAfx.h"
#include <stdlib.h>            // malloc / free (LOGPALETTE allocation, faithful to the release)
#include "SplashScreenDialog.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// SplashScreenDialog -- direct port of the release module .\release\SplashScreenDialog.obj.
// Every function is a faithful reconstruction of the Game.exe decompilation; the original's
// harmless quirks are reproduced and flagged (Draw leaks its memory DC, Destroy unregisters
// the class unconditionally, Create checks IsWindow twice, WM_PAINT returns 0 even with no
// update region, WM_ENTERIDLE is swallowed/returns TRUE).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSplash
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::SBitmap::SBitmap  @0x4089a0
////////////////////////////////////////////////////////////////////////////////////////////////////
CSplashScreen::SBitmap::SBitmap()
{
	// Quirk (disasm @0x4089a0): the ctor zeroes ONLY these four words.  size and the rest of
	// bitmapInfo (bmType/bmWidthBytes/bmPlanes/bmBitsPixel/bmBits) are left uninitialised; Load
	// fills them in.
	hBitmap             = NULL;  // +0x20
	hPalette            = NULL;  // +0x24
	bitmapInfo.bmHeight = 0;     // +0x10
	bitmapInfo.bmWidth  = 0;     // +0x0c
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::SBitmap::Load  @0x4089c0
//   bool Load( nstl::basic_string<char> const &name )
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::SBitmap::Load( const string &szName )
{
	// (1) release any previously held GDI objects
	if ( hBitmap != NULL ) { DeleteObject( hBitmap ); hBitmap = NULL; }
	if ( hPalette != NULL ) { DeleteObject( hPalette ); hPalette = NULL; }

	LPCSTR name = szName.c_str();

	// (2) reset cached dimensions; only observable on the LoadImage-failure path
	bitmapInfo.bmHeight = 0;
	bitmapInfo.bmWidth  = 0;

	// (3)+(4) LoadImageA( NULL, name, IMAGE_BITMAP, 0, 0, 0x2050 )
	hBitmap = (HBITMAP)LoadImageA( NULL, name, IMAGE_BITMAP, 0, 0,
	                               LR_LOADFROMFILE | LR_CREATEDIBSECTION | LR_DEFAULTSIZE ); // 0x2050
	if ( hBitmap == NULL )
		return false;

	// (5) GetObjectA( hBitmap, 24, &bitmapInfo )
	GetObjectA( hBitmap, sizeof( BITMAP ), &bitmapInfo );

	// (6) decision: bmBitsPixel * bmPlanes  (<= 8 => palettised)
	if ( (int)( (UINT)bitmapInfo.bmBitsPixel * (UINT)bitmapInfo.bmPlanes ) <= 8 )
	{
		// palettised: read the 256-entry DIB colour table and convert to a LOGPALETTE
		HDC     hMemDC = CreateCompatibleDC( NULL );
		HGDIOBJ hOld   = SelectObject( hMemDC, hBitmap );

		RGBQUAD aColors[256];
		GetDIBColorTable( hMemDC, 0, 256, aColors );

		// Original over-allocates 0x408 bytes (payload is 4 + 256*4 = 1028); reproduced.
		LOGPALETTE *pLogPal = (LOGPALETTE *)malloc( 0x408 );
		pLogPal->palVersion    = 0x300;
		pLogPal->palNumEntries = 0x100;
		for ( int i = 0; i < 256; ++i )
		{
			pLogPal->palPalEntry[i].peRed   = aColors[i].rgbRed;
			pLogPal->palPalEntry[i].peGreen = aColors[i].rgbGreen;
			pLogPal->palPalEntry[i].peBlue  = aColors[i].rgbBlue;
			pLogPal->palPalEntry[i].peFlags = 0;
		}
		hPalette = CreatePalette( pLogPal );
		free( pLogPal );

		SelectObject( hMemDC, hOld );
		DeleteDC( hMemDC );
		return true;
	}

	// true-colour: synthesise a halftone palette
	HDC hScreenDC = GetDC( NULL );
	hPalette = CreateHalftonePalette( hScreenDC );
	ReleaseDC( NULL, hScreenDC );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::SBitmap::Draw  @0x4087e0
//   bool Draw( HDC *pHDC )   // pHDC is the address of the destination HDC (&destDC)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::SBitmap::Draw( HDC *pHDC )
{
	if ( hBitmap != NULL && hPalette != NULL )
	{
		HDC      hMemDC  = CreateCompatibleDC( *pHDC );
		HGDIOBJ  hOld    = SelectObject( hMemDC, hBitmap );
		HPALETTE hOldPal = SelectPalette( *pHDC, hPalette, FALSE );
		RealizePalette( *pHDC );
		BitBlt( *pHDC, 0, 0, bitmapInfo.bmWidth, bitmapInfo.bmHeight, hMemDC, 0, 0, SRCCOPY );
		SelectObject( hMemDC, hOld );
		SelectPalette( *pHDC, hOldPal, FALSE );
		// ORIGINAL BUG (confirmed @0x4087e0): hMemDC is intentionally NOT DeleteDC'd here --
		// a one-DC GDI leak per paint, reproduced faithfully.
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::SplashScreenWndProc  @0x408880
////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK SplashScreenWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	// Unconditional user-data fetch (precedes the dispatch in the original).  The GWL_USERDATA
	// long is the CSplashScreen base pointer stored by Create.
	LONG userData = GetWindowLongA( hWnd, GWL_USERDATA );

	if ( uMsg == WM_PAINT )
	{
		if ( GetUpdateRect( hWnd, NULL, FALSE ) )
		{
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );
			if ( userData != 0 )
				( (CSplashScreen *)userData )->bitmap.Draw( &hDC );  // bitmap subobject == base + 0x18
			EndPaint( hWnd, &ps );
			ValidateRect( hWnd, NULL );
		}
		return 0;  // 0 even when there was no update region (faithful)
	}

	if ( uMsg != WM_ENTERIDLE )
		return DefWindowProcA( hWnd, uMsg, wParam, lParam );

	return 1;  // swallow WM_ENTERIDLE, report handled (TRUE)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::CSplashScreen  @0x408be0
//   CSplashScreen( nstl::basic_string<char> const &prefix )
////////////////////////////////////////////////////////////////////////////////////////////////////
CSplashScreen::CSplashScreen( const string &szPrefix )
{
	// szWndClassName / szImageFileName default-construct empty.  bitmap's own ctor already zeroes
	// the four cached words; the original (which inlined that ctor) re-zeroes them here -- kept
	// for fidelity, harmless.
	bitmap.hBitmap             = NULL;
	bitmap.hPalette            = NULL;
	bitmap.bitmapInfo.bmHeight = 0;
	bitmap.bitmapInfo.bmWidth  = 0;
	hWnd = NULL;

	// Per-instance unique window class name (literal @0x8b164c in the decomp).
	szWndClassName = string( "SS_CSplashScreen_WindowClass" ) + szPrefix;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::Destroy  @0x408b20
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSplashScreen::Destroy()
{
	if ( ::IsWindow( hWnd ) )
		DestroyWindow( hWnd );
	hWnd = NULL;
	// Quirk (confirmed @0x408b20): UnregisterClassA is called unconditionally, even if no class
	// was ever registered.
	UnregisterClassA( szWndClassName.c_str(), GetModuleHandleA( NULL ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::IsWindow  @0x408960
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::IsWindow() const
{
	return ::IsWindow( hWnd ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::ShowWindow  @0x408920
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::ShowWindow( int nCmdShow )
{
	if ( ::IsWindow( hWnd ) )
	{
		::ShowWindow( hWnd, nCmdShow );
		::UpdateWindow( hWnd );  // hard TRUE below; UpdateWindow's own result is discarded
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::UpdateWindow  @0x408970
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::UpdateWindow()
{
	if ( ::IsWindow( hWnd ) )
	{
		::UpdateWindow( hWnd );  // result discarded
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::CSplashScreen::Create  @0x408cb0
//   bool Create( nstl::basic_string<char> const &image, bool bTopmost )
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSplashScreen::Create( const string &szImage, bool bTopmost )
{
	// Cache the image path (self-assign guard, faithful to the decomp).
	if ( &szImage != &szImageFileName )
		szImageFileName = szImage;

	// Inlined Destroy of any currently live window (the original checks IsWindow twice).
	if ( ::IsWindow( hWnd ) )
	{
		if ( ::IsWindow( hWnd ) )
			DestroyWindow( hWnd );
		LPCSTR lpOldClass = szWndClassName.c_str();
		hWnd = NULL;
		UnregisterClassA( lpOldClass, GetModuleHandleA( NULL ) );
	}

	if ( !bitmap.Load( szImage ) )
		return false;

	WNDCLASSEXA wc;
	wc.cbSize        = sizeof( WNDCLASSEXA );          // 0x30
	wc.style         = CS_HREDRAW | CS_VREDRAW;        // 3
	wc.lpfnWndProc   = SplashScreenWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = GetModuleHandleA( NULL );
	wc.hIcon         = NULL;
	wc.hCursor       = LoadCursorA( NULL, (LPCSTR)IDC_ARROW );  // 0x7f00
	wc.hbrBackground = (HBRUSH)( COLOR_WINDOW + 1 );           // 6
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = szWndClassName.c_str();
	wc.hIconSm       = NULL;
	if ( RegisterClassExA( &wc ) == 0 )
		return false;

	// Ex-style: (bTopmost ? WS_EX_TOPMOST(8) : 0) | WS_EX_TOOLWINDOW(0x80), i.e. (-b & 8) | 0x80.
	DWORD dwExStyle = ( (DWORD)( -(int)bTopmost ) & 8u ) | 0x80u;
	hWnd = CreateWindowExA( dwExStyle, szWndClassName.c_str(), "", WS_POPUP,
	                        0, 0, 0, 0, NULL, NULL, GetModuleHandleA( NULL ), NULL );
	if ( ::IsWindow( hWnd ) )
	{
		int nWidth   = bitmap.bitmapInfo.bmWidth;
		int nHeight  = bitmap.bitmapInfo.bmHeight;
		int cxScreen = GetSystemMetrics( SM_CXSCREEN );
		int cyScreen = GetSystemMetrics( SM_CYSCREEN );
		MoveWindow( hWnd, ( cxScreen - nWidth ) / 2, ( cyScreen - nHeight ) / 2,
		            nWidth, nHeight, FALSE );
		SetWindowLongA( hWnd, GWL_USERDATA, (LONG)(LONG_PTR)this );
		::UpdateWindow( hWnd );
	}
	return ::IsWindow( hWnd ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NSplash
