#ifndef __WININPUTCONV_H__
#define __WININPUTCONV_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// SWinToInputMessageConverter -- the Main.obj helper that drains the NWinFrame OS
// message queue and re-emits it as NInput control messages, so held keys inherit the
// OS keyboard auto-repeat (retail behaviour: holding PgUp scrolls the console rapidly).
//
//   SWinToInputMessageConverter::ParseChars @0xa940  [.\release\Main.obj]
//   SWinToInputMessageConverter::Do         @0xa9f0  [.\release\Main.obj]
//
// WM_CHAR runs are accumulated and flushed through the codepage->unicode codec so each
// produced wide character becomes a CT_WIN_CHAR input message; every coalesced WM_KEYDOWN
// repeat emits its nRep count of CT_WIN_KEY messages. The retail main loop constructs one
// of these on the stack and calls Do() once per frame, right after NInput::PumpMessages
// and before StepApp (Game::WinMain @0x9810). There is NO custom repeat timer -- the rate
// is the OS keyboard setting, replayed via the WM_KEYDOWN repeat count.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <string>
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWinToInputMessageConverter
{
	std::string szCharBuffer;   // accumulated WM_CHAR run (PDB UDT size 12, member @0)

	// @0xa940: widen the accumulated narrow run once (codepage->unicode) and emit each
	// produced wide character as a CT_WIN_CHAR input message, then reset the buffer.
	void ParseChars();

	// @0xa9f0: drain NWinFrame::GetMessage; CHAR -> accumulate nRep chars; KEY_DOWN ->
	// flush pending chars first, then emit nRep CT_WIN_KEY messages. Trailing flush.
	void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
