#ifndef __SPLASHSCREENDIALOG_H__
#define __SPLASHSCREENDIALOG_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  SplashScreenDialog  --  NSplash::CSplashScreen, the tiny borderless Win32 splash
//  window the game pops while it loads.  Reconstructed from the release module
//  .\release\SplashScreenDialog.obj (absent from this predecessor tree).
//
//    NSplash::CSplashScreen          -- owns a per-instance registered window class
//                                       (szWndClassName), the splash image path
//                                       (szImageFileName), the cached GDI bitmap +
//                                       palette (SBitmap bitmap) and the HWND.
//    NSplash::CSplashScreen::SBitmap -- the cached DIB + palette blitted on WM_PAINT.
//    NSplash::SplashScreenWndProc    -- the window procedure for the splash class.
//
//  Release RVAs (VA = RVA + 0x400000):
//    SBitmap::SBitmap    @0x89a0   SBitmap::Load   @0x89c0   SBitmap::Draw  @0x87e0
//    SplashScreenWndProc @0x8880   CSplashScreen   @0x8be0   Destroy        @0x8b20
//    IsWindow @0x8960   ShowWindow @0x8920   UpdateWindow @0x8970   Create     @0x8cb0
//
//  Win32/GDI/CRT types (HWND, BITMAP, HBITMAP, HPALETTE, HDC, LRESULT/CALLBACK) and the
//  engine string / CTPoint come in through the StdAfx.h precompiled header, which every
//  translation unit in this project includes first (cf. GBinkPlayer.h's same pattern).
//
//  CSplashScreen is a plain transient window owner -- no ZDATA / operator& / vtable, so
//  it is NOT a save/load class and needs no class registrar.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSplash
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// The window procedure registered for the splash window class.  Must use the WNDPROC
// (CALLBACK / __stdcall) convention so it is assignable to WNDCLASSEXA::lpfnWndProc (the
// release decompiler mislabels it __cdecl).
LRESULT CALLBACK SplashScreenWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSplashScreen
{
public:
	//----------------------------------------------------------------------------------------------
	// SBitmap -- the cached GDI bitmap + palette the splash window blits on WM_PAINT.
	//----------------------------------------------------------------------------------------------
	class SBitmap
	{
	public:
		CTPoint<int> size;        // +0x00  (unused by the splash code; kept for layout)
		BITMAP       bitmapInfo;  // +0x08  GetObjectA target (tagBITMAP, 24 bytes)
		HBITMAP      hBitmap;     // +0x20
		HPALETTE     hPalette;    // +0x24

		SBitmap();                          // @0x4089a0
		bool Load( const string &szName );  // @0x4089c0
		bool Draw( HDC *pHDC );             // @0x4087e0  (pHDC == &destinationDC)
	};

	string  szWndClassName;   // +0x00  per-instance registered window class name
	string  szImageFileName;  // +0x0c  splash image file path
	SBitmap bitmap;           // +0x18  cached image
	HWND    hWnd;             // +0x40  the live splash window (NULL when not shown)

	explicit CSplashScreen( const string &szPrefix = string() );  // @0x408be0

	bool Create( const string &szImage, bool bTopmost );  // @0x408cb0
	void Destroy();                                       // @0x408b20
	bool IsWindow() const;                                // @0x408960
	bool ShowWindow( int nCmdShow );                      // @0x408920
	bool UpdateWindow();                                  // @0x408970
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NSplash
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SPLASHSCREENDIALOG_H__
