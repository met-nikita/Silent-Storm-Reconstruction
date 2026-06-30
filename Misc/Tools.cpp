#include "StdAfx.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
void __cdecl DebugTrace( const char *pszFormat, ... )
{
	static char buff[2048];
	va_list va;
	// 
	va_start( va, pszFormat );
	vsprintf( buff, pszFormat, va );
	va_end( va );
	//
	OutputDebugString( buff );
}
