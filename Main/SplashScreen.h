#ifndef __SPLASHSCREEN_H__
#define __SPLASHSCREEN_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  SplashScreen  --  the NSplash free-function facade over the game's single global splash
//  window `defaultSplashScreen` (a NSplash::CSplashScreen).  Reconstructed from the release
//  module .\release\SplashScreen.obj (absent from this predecessor tree).
//
//  These three thin functions are the public API the engine startup/shutdown code calls to
//  pop, refresh and tear down the loading splash; all the real work is delegated to the
//  CSplashScreen lifecycle / window-state forwarders in SplashScreenDialog.h.
//
//  Release RVAs (VA = RVA + 0x400000):
//    ShowSplashScreen @0x408740   UpdateSplashScreen @0x408780   HideSplashScreen @0x408790
//
//  The engine string type (nstl::basic_string<char>) comes in through StdAfx.h, which every
//  translation unit in this project includes first.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSplash
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Pop the splash window: tear down any existing window, (re)create it for szImage, show it, and
// report whether the window ended up live (the intermediate results are discarded -- faithful to
// the release, which returns only the final IsWindow()).  @0x408740   bool(string const&, bool)
bool ShowSplashScreen( const string &szImage, bool bTopmost );

// Repaint/refresh the live splash window; returns FALSE when no window is up.
//   @0x408780   bool(void)   (tail-jmp to CSplashScreen::UpdateWindow in the disasm)
bool UpdateSplashScreen();

// Tear the splash window down.
//   @0x408790   void(void)   (tail-jmp to CSplashScreen::Destroy in the disasm)
void HideSplashScreen();
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NSplash
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SPLASHSCREEN_H__
