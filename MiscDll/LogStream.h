#ifndef __A5_LOGSTREAM_H__
#define __A5_LOGSTREAM_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EStreamType
{
	STREAM_SYSTEM,
	STREAM_SCRIPT,
	STREAM_AI,
	STREAM_RPG,
	STREAM_GAME
};
enum EConsoleColor
{
	CC_WHITE,
	CC_RED,
	CC_GREEN,
	CC_BLUE,
	CC_PINK,
	CC_GREY,
	CC_CYAN,
	CC_YELLOW,
	CC_BROWN,
	CC_ORANGE
};

class CLogStream
{
protected:
	wstring wsStreamBuffer;
	EStreamType eType;
		
public:
	CLogStream( EStreamType _eType ): eType( _eType )	{}
		
	CLogStream& operator<< ( const int &n );
	CLogStream& operator<< ( const long &l );
	CLogStream& operator<< ( const double &d );
	CLogStream& operator<< ( const bool &bVal );
	CLogStream& operator<< ( const CHAR* szText );
	CLogStream& operator<< ( const WCHAR* szText );
	CLogStream& operator<< ( const wstring &szText );
	CLogStream& operator<< ( const string &szText ) { operator<<(szText.c_str()); return *this; }
	CLogStream& operator<< ( const EConsoleColor &eColor );
	
	CLogStream& operator<< ( CLogStream& (*Func)( CLogStream& csStream ) );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SConsoleLine
{
	int nID;
	EStreamType eType;

	bool bCommand;
	wstring szText;
	
	SConsoleLine( int _nID, EStreamType _eType, bool _bCommand, wstring _szText ): nID( _nID ), eType( _eType ), bCommand( _bCommand ), szText( _szText ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// for Console.cpp
externA5 bool bConsoleUpdated;
externA5 list<SConsoleLine> consoleLines;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CLogStream& endl( CLogStream& sStream )
{
	sStream << L"\n";
	return sStream;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 CLogStream csSystem, csScript, csAI, csRPG, csGame;
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif