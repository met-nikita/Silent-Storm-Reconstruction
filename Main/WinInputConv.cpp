#include "StdAfx.h"
#include "WinInputConv.h"
#include "..\Game\WinFrame.h"
#include "..\Input\Input.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstruction of SWinToInputMessageConverter (retail Main.obj @0xa940 / @0xa9f0). See
// WinInputConv.h. Ported verbatim from the oracle src/s2_swintoinputmessageconverter.h,
// adapted to the dev NWinFrame::GetMessage(SWindowsMsg*) signature and NStr::ToUnicode.
////////////////////////////////////////////////////////////////////////////////////////////////////
// SWinToInputMessageConverter::ParseChars @0xa940
//   if (szCharBuffer.empty()) return;
//   wstring szRes; NStr::ToUnicode(&szRes, szCharBuffer);
//   for (k = 0; k < szRes.size(); ++k) NInput::AddWinMessage(CT_WIN_CHAR, szRes[k]);
//   szCharBuffer = "";
void SWinToInputMessageConverter::ParseChars()
{
	if ( szCharBuffer.empty() )
		return;

	wstring szRes;
	NStr::ToUnicode( &szRes, szCharBuffer );
	for ( int k = 0; k < (int)szRes.size(); ++k )
		NInput::AddWinMessage( NInput::CT_WIN_CHAR, (unsigned short)szRes[k] );   // zero-extended

	szCharBuffer = "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SWinToInputMessageConverter::Do @0xa9f0
//   while (NWinFrame::GetMessage(&wMsg)) {
//       if (wMsg.msg == CHAR)      for (k=0;k<wMsg.nRep;++k) szCharBuffer += (char)wMsg.nKey;
//       else if (wMsg.msg == KEY_DOWN) {
//           ParseChars();          // flush pending chars FIRST (ordering)
//           for (k=0;k<wMsg.nRep;++k) NInput::AddWinMessage(CT_WIN_KEY, wMsg.nKey);
//       }
//   }
//   ParseChars();                  // trailing flush
// All other message kinds (mouse / key-up / time / ...) fall through untouched.
void SWinToInputMessageConverter::Do()
{
	NWinFrame::SWindowsMsg wMsg;
	while ( NWinFrame::GetMessage( &wMsg ) )
	{
		if ( wMsg.msg == NWinFrame::SWindowsMsg::CHAR )
		{
			for ( int k = 0; k < wMsg.nRep; ++k )
				szCharBuffer += (char)wMsg.nKey;
		}
		else if ( wMsg.msg == NWinFrame::SWindowsMsg::KEY_DOWN )
		{
			ParseChars();
			for ( int k = 0; k < wMsg.nRep; ++k )
				NInput::AddWinMessage( NInput::CT_WIN_KEY, wMsg.nKey );
		}
	}

	ParseChars();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
