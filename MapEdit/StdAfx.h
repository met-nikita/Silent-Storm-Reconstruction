// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__2916862C_C371_48A0_B719_2F0DAF12155C__INCLUDED_)
#define AFX_STDAFX_H__2916862C_C371_48A0_B719_2F0DAF12155C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define _WIN32_WINNT 0x0501

//#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

//#define __STL_NO_SGI_IOSTREAMS
#include <vcruntime_typeinfo.h>

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#define dbgnew new
#include <objbase.h>

//#define _SECDLL
// alternative to above: if you want to include ALL OT headers, uncomment this line
// to use the classic OT inclusion method (will increase build time)
#include "toolkit/config/ot_Default.h"
#include "toolkit/secall.h"

#pragma warning(disable: 4786) // identifier was truncated in the debug information

#include <vector>
#include <unordered_map>
#include <list>
#include <string>
#include <algorithm>
using namespace std;

#include "..\misc\basic2.h"
#include "..\misc\Tools.h"
#include "..\FileIO\basicChunk1.h"
#include "..\ADOImport\BasicDB.h"
#include "..\misc\Geom.h"
#include "..\misc\2DArray.h"

#define for if(false); else for
//#define ASSERT( a ) if ( !(a) ) __debugbreak();

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__2916862C_C371_48A0_B719_2F0DAF12155C__INCLUDED_)
