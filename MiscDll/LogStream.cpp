#include "StdAfx.h"
#include "Commands.h"
#include "LogStream.h"
#include "..\FileIO\BasicChunk1.h"   // START_REGISTER / FINISH_REGISTER (ui_messages)
#include <fstream>
////////////////////////////////////////////////////////////////////////////////////////////////////
int nID = 0;
bool bConsoleUpdated = false;
list<SConsoleLine> consoleLines;
// retail log sink: the CLogPanel currently mirroring console output (installed each frame in
// CLogPanel::Draw via SetLogNotify). Weak CPtr -- the panel owns the notify, not the other way round.
CPtr<ILogNotify> pLogNotify;
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream csSystem( STREAM_SYSTEM );	// output for engine messages
CLogStream csScript( STREAM_SCRIPT );	// output for scripts
CLogStream csAI( STREAM_AI );		// output for AI
CLogStream csRPG( STREAM_RPG );	// output for rpg messages
CLogStream csGame( STREAM_GAME );	// In-game messages
// maximum number of lines kept in the console
const int CONSOLE_MAX_SIZE = 256;
bool g_bHarnessLog = false;   // [HARNESS] console-log tee toggle (see LogStream.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetLogNotify( ILogNotify *pNotify )
{
	pLogNotify = pNotify;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
////////////////////////////////////////////////////////////////////////////////////////////////////
// ui_messages (retail @0x984806, default 1, saved) -- the in-game message-line switch
static bool bShowMessages = true;
// console_writelog (retail @0x9c986d, default 0, saved; registered in the A5Script block)
bool bConsoleWriteLog = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail EraseTags @0x3d6df0: strip every <...> tag from a console line
static wstring EraseTags( const wstring &s )
{
	wstring res;
	int nPos = 0;
	for ( ;; )
	{
		int nOpen = s.find( L'<', nPos );
		if ( nOpen == wstring::npos )
		{
			res += s.substr( nPos );
			return res;
		}
		res += s.substr( nPos, nOpen - nPos );
		int nClose = s.find( L'>', nOpen );
		if ( nClose == wstring::npos )
			return res;		// retail: an unterminated tag drops the tail
		nPos = nClose + 1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddConsoleLine( const SConsoleLine &sLine )
{
	if ( bShowMessages )	// retail @0x3d71c0: ui_messages gates ONLY the console-list push
		consoleLines.push_front( sLine );
	// retail: every appended line is pushed to the active log panel's notify (null when no panel).
	if ( IsValid( pLogNotify ) )
		pLogNotify->OnAddConsoleLine( sLine );
	DebugTrace( "%S\n", sLine.szText.data() );

	// [HARNESS] tee every console line to _console.log (UTF-8) for unattended runs
	if ( g_bHarnessLog )
	{
		FILE *pF = fopen( "_console.log", "a" );
		if ( pF )
		{
			char szBuf[4096];
			int n = WideCharToMultiByte( CP_UTF8, 0, sLine.szText.data(), (int)sLine.szText.size(), szBuf, sizeof(szBuf) - 1, 0, 0 );
			if ( n >= 0 ) { szBuf[n] = 0; fprintf( pF, "%s\n", szBuf ); }
			fclose( pF );
		}
	}

	while( consoleLines.size() > CONSOLE_MAX_SIZE )
	{
		consoleLines.pop_back();
	}

	// retail @0x3d71c0: console_writelog tees every line (tags stripped) into Console.txt
	if ( bConsoleWriteLog )
	{
		static wofstream f;	// retail function-local static; first open truncates (openmode 0x12)
		if ( !f.is_open() )
			f.open( "Console.txt", ios::out | ios::trunc );
		if ( f.is_open() )
		{
			f << EraseTags( sLine.szText ) << L"\n";
			f.flush();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console stream
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const bool &bVal )
{
	bConsoleUpdated = true;
	wsStreamBuffer += bVal ? L"<green>true<white>" : L"<red>false<white>";
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const int &nVal )
{
	WCHAR wszBuffer[1024];

	bConsoleUpdated = true;
	swprintf( wszBuffer, L"%d", nVal );
	wsStreamBuffer = wsStreamBuffer + wszBuffer;
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const long &lVal )
{
	WCHAR wszBuffer[1024];
	
	bConsoleUpdated = true;
	swprintf( wszBuffer, L"%d", lVal );
	wsStreamBuffer = wsStreamBuffer + wszBuffer;
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const double &dVal )
{
	WCHAR wszBuffer[1024];
	
	bConsoleUpdated = true;
	swprintf( wszBuffer, L"%.3f", dVal );
	wsStreamBuffer = wsStreamBuffer + wszBuffer;
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const CHAR* szText )
{
	int nLen = 0;
	WCHAR wszText[1024];
	
	bConsoleUpdated = true;
	nLen = MultiByteToWideChar( CP_ACP, 0, szText, strlen( szText ), wszText, 1024 );

	if ( nLen > 0 )
	{
		wszText[nLen] = 0;
		*this << wstring( wszText );
	}

	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const WCHAR* szText ) 
{
	bConsoleUpdated = true;

	*this << wstring( szText );

	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const wstring &szText )
{
	bConsoleUpdated = true;
	for ( int nTemp = 0; nTemp < szText.length(); nTemp++ )
	{
		if ( szText[nTemp] == L'\n' )
		{
			nID++;
			AddConsoleLine( SConsoleLine( nID, eType, false, wsStreamBuffer ) );
			wsStreamBuffer.clear();
		}
	/*
		else if ( szText[nTemp] == L'<' )
			wsStreamBuffer.append( L"<lb>" );
		else if ( szText[nTemp] == L'>' )
			wsStreamBuffer.append( L"<rb>" );
		*/
		else if ( szText[nTemp] == L'\t' )
			wsStreamBuffer.append( 1, szText[nTemp] );
		else if ( iswprint( szText[nTemp] ) )
			wsStreamBuffer.append( 1, szText[nTemp] );
	}
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( const EConsoleColor &eColor )
{
	switch( eColor )
	{
		case CC_WHITE:
			wsStreamBuffer.append( L"<color=white>" );
			break;
		case CC_RED:
			wsStreamBuffer.append( L"<color=red>" );
			break;
		case CC_GREEN:
			wsStreamBuffer.append( L"<color=green>" );
			break;
		case CC_BLUE:
			wsStreamBuffer.append( L"<color=blue>" );
			break;
		case CC_PINK:
			wsStreamBuffer.append( L"<color=pink>" );
			break;
		case CC_GREY:
			wsStreamBuffer.append( L"<color=grey>" );
			break;
		case CC_CYAN:
			wsStreamBuffer.append( L"<color=cyan>" );
			break;
		case CC_YELLOW:
			wsStreamBuffer.append( L"<color=yellow>" );
			break;
		case CC_BROWN:
			wsStreamBuffer.append( L"<color=brown>" );
			break;
		case CC_ORANGE:
			wsStreamBuffer.append( L"<color=orange>" );
			break;
	}
	return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream& CLogStream::operator<< ( CLogStream& (*Func)( CLogStream& csStream ) )
{
	return Func( *this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail LogStream.obj registrar: single var (bShowMessages defined above AddConsoleLine)
START_REGISTER(LogStream)
	REGISTER_VAR_EX( "ui_messages", NGlobal::VarBoolHandler, &bShowMessages, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
