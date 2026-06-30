#include "StdAfx.h"
#include "SplashScreenDialog.h"   // NSplash::CSplashScreen (the delegated-to lifecycle class)
#include "SplashScreen.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// SplashScreen -- direct port of the release module .\release\SplashScreen.obj.  The three NSplash
// facade functions are trivial __thiscall dispatchers on the single global defaultSplashScreen that
// forward to the CSplashScreen lifecycle methods in SplashScreenDialog.cpp.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSplash
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// The single global splash window (release .data @0x987260).  Its dynamic initializer ($E27 @0x895db0)
// constructs it with the prefix "_S2_SPLASH" (so the per-instance registered window class becomes
// "SS_CSplashScreen_WindowClass_S2_SPLASH"); $E28 @0x8af660 runs the dtor at exit.  TU-local: only
// the three facade functions below reference it.
////////////////////////////////////////////////////////////////////////////////////////////////////
static CSplashScreen defaultSplashScreen( string( "_S2_SPLASH" ) );
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::ShowSplashScreen  @0x408740
//   Destroy(); Create(szImage, bTopmost); ShowWindow(SW_SHOW); return IsWindow();
// The Destroy/Create/ShowWindow return values are discarded (faithful to the decomp): even if
// Create fails, ShowWindow still runs (it no-ops on a dead window) and IsWindow() then reports false.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ShowSplashScreen( const string &szImage, bool bTopmost )
{
	defaultSplashScreen.Destroy();
	defaultSplashScreen.Create( szImage, bTopmost );
	defaultSplashScreen.ShowWindow( SW_SHOW );  // nCmdShow == 5 (the 0x5 the disasm pushes)
	return defaultSplashScreen.IsWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::UpdateSplashScreen  @0x408780   (tail-jmp to CSplashScreen::UpdateWindow)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateSplashScreen()
{
	return defaultSplashScreen.UpdateWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NSplash::HideSplashScreen  @0x408790   (tail-jmp to CSplashScreen::Destroy)
////////////////////////////////////////////////////////////////////////////////////////////////////
void HideSplashScreen()
{
	defaultSplashScreen.Destroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NSplash
