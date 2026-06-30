// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <vcruntime_typeinfo.h>

/* // simplified windows.h :) 
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef const char *LPCSTR;
typedef int BOOL;
struct HWND__; typedef struct HWND__ *HWND;
typedef char CHAR;
typedef wchar_t WCHAR;
#define OutputDebugString OutputDebugStringA
externA5 "C" __declspec(dllimport) void __stdcall  OutputDebugStringA( const char * );
externA5 "C" __declspec(dllimport) DWORD __stdcall  GetTickCount();
*/
#include <windows.h>
//#include <objbase.h>
//#include <assert.h>
#ifdef _DEBUG
#  ifdef FAST_DEBUG
#    define ASSERT( a ) if ( !(a) ) __debugbreak();
#  else
#    define ASSERT( aParam ) if ( !(aParam) ) { char szBuf[1024]; sprintf( szBuf, "%s(%d) assertion %s failed", __FILE__, __LINE__, #aParam ); MessageBox( 0, szBuf, "Error", MB_OK ); __debugbreak(); }
#  endif
#else
#  define ASSERT( a ) ((void)0)
#endif
//_ASSERT( a )
//
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <crtdbg.h>
#include <unordered_map>
#include "..\Misc\basic2.h"
#include "..\Misc\tools.h"

using namespace std;
#include "Specific.h"
//
#define for if(false); else for
#define dbgnew new

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
