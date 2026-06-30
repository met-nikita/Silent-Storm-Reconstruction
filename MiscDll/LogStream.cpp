#include "StdAfx.h"
#include "Commands.h"
#include "LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
int nID = 0;
bool bConsoleUpdated = false;
list<SConsoleLine> consoleLines;
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogStream csSystem( STREAM_SYSTEM );	// Ответы на консольные комманды
CLogStream csScript( STREAM_SCRIPT );	// Ошибки в скрипте
CLogStream csAI( STREAM_AI );		// Аишный лог 
CLogStream csRPG( STREAM_RPG );	// Отчеты rpg мехпники
CLogStream csGame( STREAM_GAME );	// In-game сообщения
// максимальное кол-во строк в консоле
const int CONSOLE_MAX_SIZE = 256;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddConsoleLine( const SConsoleLine &sLine )
{
	consoleLines.push_front( sLine );
	DebugTrace( "%S\n", sLine.szText.data() );

	while( consoleLines.size() > CONSOLE_MAX_SIZE )
	{
		consoleLines.pop_back();
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
