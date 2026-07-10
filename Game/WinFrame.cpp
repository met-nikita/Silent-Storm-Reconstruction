#include "StdAfx.h"
#include "WinFrame.h"
#include "..\Misc\StrProc.h"
#include "..\Misc\HPTimer.h"
#include "..\Misc\Win32Helper.h"
#include <strstream>
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWinFrame;
using namespace NWin32Helper;
////////////////////////////////////////////////////////////////////////////////////////////////////
static HWND hWnd = 0;                            // window handle
static HINSTANCE hInstance = 0;                  // instance handle
static ATOM atomWndClassName = 0;                // atom window class name identification (assigned during registration)
static volatile bool bExit = false;
static volatile bool bActive = true;
static CCriticalSection msgs;
static list< SWindowsMsg > msgList;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Report( const char *pszText, int nVal = -0x7fffffff )
{
	strstream s;
	s << pszText;
	if ( nVal != -0x7fffffff )
		s << nVal;
	s << "\n" << (char)0;
	OutputDebugString( s.str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NWinFrame::GetMessage( SWindowsMsg *pRes )
{
	CCriticalSectionLock lock( msgs );
	if ( !msgList.empty() )
	{
		*pRes = msgList.front();
		msgList.pop_front();
		return true;
	}
	pRes->msg = SWindowsMsg::TIME;
	NHPTimer::GetTime( &pRes->time );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NWinFrame::IsAppActive()
{
	return bActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetActive( bool _bActive )
{
 	bActive = _bActive;        // activation flag 
	if ( !bActive )
		ShowWindow( hWnd, SW_MINIMIZE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NWinFrame::IsExit()
{
	return bExit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NWinFrame::PumpMessages()
{
  // Now we are ready to recieve and process Windows messages.
  MSG msg;
	while ( PeekMessage( &msg, 0, 0, 0, PM_NOREMOVE ) )
	{
		if ( ::GetMessage( &msg, 0, 0, 0 ) )
		{
			if ( msg.message == WM_ACTIVATEAPP )
			{
				SetActive( msg.wParam != 0 );
				//Report( "MainMsgProcess::WM_activateapp ", msg.wParam );
			}
			TranslateMessage( &msg );
			DispatchMessage( &msg );
			//Report( "...finish msg process", msg.message );
		}
		else
			bExit = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HWND NWinFrame::GetWnd()
{
	return hWnd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NWinFrame::Exit()
{
	PostQuitMessage(0);
	//bClientExitReq = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags )
{
	NHPTimer::STime time;
	NHPTimer::GetTime( &time );
	CCriticalSectionLock lock( msgs );
	SWindowsMsg &m = *msgList.insert( msgList.end(), SWindowsMsg());
	m.time = time;
	m.msg = msg;
	m.x = x;
	m.y = y;
	m.dwFlags = dwFlags;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: prevent application from running more then one application window
static bool CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle )
{
  HWND hwndFind, hwndLast, hwndForeGround;
  DWORD dwFindID, dwForeGroundID;
  // Check if application is already running
  hwndFind = FindWindow( pszMainClass, pszMainTitle );
  if ( hwndFind )
  {
    // Bring previously running application's main
    // window to the user's attention
    hwndForeGround = GetForegroundWindow();
    dwForeGroundID = GetWindowThreadProcessId( hwndForeGround, 0 );
    dwFindID = GetWindowThreadProcessId( hwndFind, 0 );
    // Don't do anything if window is already in foreground
    // Unless it is iconized.
    if ( (dwFindID != dwForeGroundID) || IsIconic(hwndFind) )
    {
      hwndLast = GetLastActivePopup( hwndFind );
      if ( IsIconic(hwndLast) )
        ShowWindow( hwndLast, SW_RESTORE );
      BringWindowToTop( hwndLast );
      SetForegroundWindow( hwndLast );
    }
    // Prevent additional instance's of this application
    // from running
    return false;
  }

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static bool CreateWin( LPCSTR pszApp, LPCSTR pszWnd, unsigned dwWidth, unsigned dwHeight )
{
  // create and register class style
        // Register the windows class
  WNDCLASS wndClass = { 0, WndProc, 0, 0, hInstance,
                        0,//LoadIcon( hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON) ),
                        0,//LoadCursor( NULL, IDC_ARROW ), 
                        (HBRUSH)GetStockObject(NULL_BRUSH), // NULL_BRUSH // WHITE_BRUSH
                        NULL, pszWnd };
 atomWndClassName = RegisterClass( &wndClass );

  // Set the window's initial style
  DWORD dwWinStyle = WS_POPUP|WS_SYSMENU|WS_VISIBLE;//WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_VISIBLE;

  // Create the render window
  hWnd = CreateWindow( pszWnd, pszApp, dwWinStyle,
                         0, 0, dwWidth, dwHeight, 0L,
                         0,//LoadMenu( hInstance, MAKEINTRESOURCE(IDR_MENU) ), 
                         hInstance, 0L );
  if ( !hWnd )
	{
		//ThrowException( "Can't create main app window\n" );
		return false;
	}
  // show & update window
  ShowWindow( hWnd, SW_SHOW );
  UpdateWindow( hWnd );
  // eliminate cursor once for this widow
  SetCursor( 0 );

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// did not know how to return NCHitTest
static LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	//Report( "WndProc_", uMsg );
	//
	bool bCallDefWindowProc = false;
	switch ( uMsg )
	{
		case WM_PAINT:
			/*RECT rect;
			if ( GetUpdateRect(hWnd, &rect, FALSE) )
				ValidateRect( hWnd, &rect );*/
			break;
    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 100;
      ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 100;
      break;
    case WM_ENTERSIZEMOVE:
    // Halt frame movement while the app is sizing or moving
			ASSERT(0);
      break;
    case WM_EXITSIZEMOVE:
      break;
		case WM_SETCURSOR:
			SetCursor( 0 );
			break;
    case WM_NCHITTEST:
      // Prevent the user from selecting the menu in fullscreen mode
      //if( !m_bWindowed )
      return HTCLIENT;
      break;
    case WM_POWERBROADCAST:
      switch( wParam )
      {
        case PBT_APMQUERYSUSPEND:
          // At this point, the app should save any data for open
          // network connections, files, etc., and prepare to go into
          // a suspended mode.
          return TRUE;

        case PBT_APMRESUMESUSPEND:
          // At this point, the app should recover any data, network
          // connections, files, etc., and resume running from when
          // the app was suspended.
          return TRUE;
      }
      break;
    case WM_SYSCOMMAND:
      // Prevent moving/sizing and power loss in fullscreen mode
      switch( wParam )
      {
        case SC_MOVE:
        case SC_SIZE:
        case SC_MAXIMIZE:
        case SC_KEYMENU:
        case SC_MONITORPOWER:
				case SC_SCREENSAVE:
          //if( FALSE == m_bWindowed )
          return 1; // in both modes is prevented
          break;
/*				case SC_RESTORE:
					ShowWindow( hWnd, SW_RESTORE );
					break;*/
      }
      break;
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
		case WM_ACTIVATEAPP:
			//Report( "WndProc::WM_activateapp ", wParam );
			SetActive( wParam != 0 );
			break;
		case WM_ACTIVATE:
			//if ( !(HIWORD(wParam)) )          // if window is not minimized
			{
				switch ( LOWORD(wParam) )
				{
					case WA_CLICKACTIVE:					// activate window
					case WA_ACTIVE:
						//if ( (HIWORD(wParam)) ) restore window
						//bActive = true;
						break;
					case WA_INACTIVE:						// deactivate window
						SetActive( false );
						//Report( "WndProc::WM_activate, WA_INACTIVE ", wParam );
						break;
				}
			}
			break;

		case WM_MOUSEMOVE:
			AddMsg( SWindowsMsg::MOUSE_MOVE, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_RBUTTONDOWN:
			AddMsg( SWindowsMsg::RB_DOWN, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_RBUTTONUP:
			AddMsg( SWindowsMsg::RB_UP, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_LBUTTONDOWN:
			AddMsg( SWindowsMsg::LB_DOWN, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_LBUTTONUP:
			AddMsg( SWindowsMsg::LB_UP, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_KEYDOWN:
			AddMsg( SWindowsMsg::KEY_DOWN, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
		case WM_KEYUP:
			AddMsg( SWindowsMsg::KEY_UP, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
		case WM_CHAR:
			// OS-generated (TranslateMessage) translated character; OS auto-repeats the
			// underlying WM_KEYDOWN so the coalesced repeat-count rides in lParam&0xFFFF.
			// nKey = character code (wParam), nRep = repeat count. Mirrors WM_KEYDOWN.
			AddMsg( SWindowsMsg::CHAR, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NWinFrame::InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName )
{
	::hInstance = hInstance;
	if ( !CreateWin( pszAppName, pszWndName, 100, 100 ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/*int CWin32Frame::MessageBox( LPCSTR pszText, UINT uType )
{
  HCURSOR hOldCursor = GetCursor();     // remember old cursor before message box call
//	pGraphicsEngine->FlipToGDI();
  int nRetVal = ::MessageBox( hWnd, pszText, szAppTitleName.c_str(), uType );
  SetCursor( hOldCursor );              // restore old cursor after message box usage
  return nRetVal;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
