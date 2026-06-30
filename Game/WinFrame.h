#ifndef __WINFRAME_H__
#define __WINFRAME_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\misc\HPTimer.h"
//
namespace NWinFrame
{
	struct SWindowsMsg
	{
		enum EMsg
		{
			MOUSE_MOVE,
			RB_DOWN,
			LB_DOWN,
			RB_UP,
			LB_UP,
			KEY_DOWN,
			KEY_UP,
			TIME,
		};
		NHPTimer::STime time;
		EMsg msg;
		union
		{
			struct { int x,y; };         // mouse
			struct { int nKey, nRep; };  // key
		};
		DWORD dwFlags;
	};
	// WinFrame interface
	bool GetMessage( SWindowsMsg *pRes );
	bool IsAppActive();
	bool IsExit();
	void Exit();
	HWND GetWnd();
	void PumpMessages();
	bool InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
