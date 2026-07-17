#ifndef __GAutoDetect_H_
#define __GAutoDetect_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Graphics auto-detect / quality "mode" plumbing (module GAutoDetect.obj).
//
// A console-var driven LOW/MED/HIGH/VHIGH preset system.  Each quality axis
// (speed/HSR, lighting, texture, FSAA) owns a small static SCfgValue table -- one
// NUL-terminated SCfgValue[] per EConfigValue (CV_LOW..CV_VHIGH).  An SCfgValue is
// a (console-var name, value) pair: the value that preset assigns the var.
//
//   FindCfgMode( modes, count )  -- which preset is currently active?  A preset
//     matches iff every one of its (name,value) pairs equals the var's live value
//     (NGlobal::GetVar(name, value).GetFloat()).  Returns that index, or CV_CUSTOM
//     if none match.  The live value is fetched with the preset's own value as the
//     GetVar default, so an unregistered var trivially matches.
//   ApplyCfgValues( modes, mode, count )  -- push a preset's values into the vars
//     via NGlobal::SetVar.  mode==CV_CUSTOM does nothing.
//
// The Get*/Set* wrappers bind those to the four module preset tables; SetSpeedMode
// additionally derives gfx_hsr (hidden-surface-removal level) from RAM + CPU clock.
//
// Functional reconstruction of Game.exe decomp (NOT byte-exact).  The three
// remaining GAutoDetect.obj functions (AutoDetectVideoConfig / CommandGfxAutodetect /
// GAutoDetectInit) are deferred: they reach absent cross-compiland NGfx internals
// (NGfx::GetVideoCard + videoCardsArray, and NGfx::GetSystemInfo whose real D3D
// video-memory probe is an unreconstructed empty stub).
// !! KNOWN RETAIL BUG -- DO NOT PORT AS-IS: retail's video-memory probe stores the size
// in a SIGNED int; on modern GPUs (>= 2GB VRAM) it wraps negative and auto-detect forces
// features off.  Any future GetSystemInfo/AutoDetectVideoConfig port must keep it unsigned.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Quality preset index, shared by every axis.  The four real presets are
// CV_LOW..CV_VHIGH; CV_CUSTOM means "no preset currently matches".  CV_CUSTOM (== 4)
// also doubles as the table element count passed to FindCfgMode/ApplyCfgValues.
enum EConfigValue
{
	CV_LOW,
	CV_MED,
	CV_HIGH,
	CV_VHIGH,
	CV_CUSTOM
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// One (console-var name, value) pair a preset assigns.  A preset is a
// NUL-terminated SCfgValue[] (pszName == 0 terminates).
struct SCfgValue
{
	const char *pszName;
	float fValue;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsLowRAM();                                                        // @0xf68e0 -- <= 256 MB physical RAM
EConfigValue FindCfgMode( SCfgValue **modes, int count );              // @0xf69b0 -- which preset is live?
void ApplyCfgValues( SCfgValue **modes, EConfigValue mode, int count ); // @0xf6900 -- push a preset into the vars
////
EConfigValue GetSpeedMode();                          // @0xf6ad0
void SetSpeedMode( EConfigValue mode );               // @0xf6ae0
EConfigValue GetLightingQualityMode();                // @0xf6aa0
void SetLightingQualityMode( EConfigValue mode );     // @0xf6ab0
EConfigValue GetTextureMode();                        // @0xf6bb0
void SetTextureMode( EConfigValue mode );             // @0xf6bc0
EConfigValue GetFSAAMode();                            // @0xf6be0
void SetFSAAMode( EConfigValue mode );                // @0xf6bf0
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
