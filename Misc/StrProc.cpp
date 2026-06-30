#include "StdAfx.h"

#include "StrProc.h"

#include <unordered_map>
#include <stack>
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NStr
{
	static std::unordered_map<char, char> brackets;   // map with open bracket <=> close bracket respection
	static char cBracketTypes[8] = "({[\" ";     // all available brackets (open)
	static const int NUM_BRACKET_TYPES = 4;      // number of available brackets
	static int nCodePage = CP_ACP;
	//
	void InitStringProcessor();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// проинициализировать внутренние структуры string processor'а
void NStr::InitStringProcessor()
{
	brackets['('] = ')';
	brackets['['] = ']';
	brackets['{'] = '}';
	brackets['\"'] = '\"';
}
// это вспомогательная структура для автоматической инициализации string processor'а
struct SStrProcInit
{
	SStrProcInit() { NStr::InitStringProcessor(); }
};
static SStrProcInit spInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NStr::IsOpenBracket( const char cSymbol )
{
	return brackets.find( cSymbol ) != brackets.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// добавить новую пару скобок
void NStr::AddBrackets( const char cOpenBracket, const char cCloseBracket )
{
	brackets[cOpenBracket] = cCloseBracket;
}
// удалить пару скобок
void NStr::RemoveBrackets( const char cOpenBracket, const char cCloseBracket )
{
	brackets.erase( cOpenBracket );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// получить закрывающую скобку по открывающей
const char NStr::GetCloseBracket( const char cOpenBracket )
{
	return brackets[cOpenBracket];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// разделить строку на массив строк по заданному разделителю
void NStr::SplitString( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	int nPos = 0, nLastPos = 0;
	//
	do
	{
		nPos = szString.find( cSeparator, nLastPos );
		// add string
		szVector.push_back( szString.substr( nLastPos, nPos - nLastPos ) );
		nLastPos = nPos + 1;//szString.find_first_not_of( cSeparator, nPos );
		//
	} while( nPos != std::string::npos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// разделить строку на массив строк по заданному разделителю с учётом скобок одной вложенности
void NStr::SplitStringWithBrackets( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	int nPos = 0, nLastPos = 0;
	//
	cBracketTypes[NUM_BRACKET_TYPES] = cSeparator;
	//
	do
	{
		nPos = szString.find_first_of( cBracketTypes, nLastPos );
		if ( nPos != std::string::npos )
		{
			if ( szString[nPos] != cSeparator )      // this is a bracket
			{
				nPos = szString.find( brackets[szString[nPos]], nPos + 1 );
				continue;
			}
		}
		// add string
		szVector.push_back( szString.substr( nLastPos, nPos - nLastPos ) );
		nLastPos = nPos + 1;//szString.find_first_not_of( cSeparator, nPos );
		//
	} while( nPos != std::string::npos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// разделить строку на массив строк по заданному разделителю с учётом скобок любой вложенности
void NStr::SplitStringWithMultipleBrackets( const std::string &szString, std::vector<std::string> &szVector, const char cSeparator )
{
	std::stack<char> stackBrackets;
	int nLastPos = 0;
  int i;
	//
	for ( i=0; i<szString.size(); ++i )
	{
		char c = szString[i];
		if ( IsOpenBracket(c) )
			stackBrackets.push( brackets[c] );
		else if ( stackBrackets.empty() )
		{
			if ( c == cSeparator )
			{
				szVector.push_back( szString.substr( nLastPos, i - nLastPos ) );
				nLastPos = i + 1; // szString.find_first_not_of( cSeparator, i );
			}
		}
		else if ( c == stackBrackets.top() )
			stackBrackets.pop();
	}
	// last substring
	if ( nLastPos + 1 <= int( i ) )
		szVector.push_back( szString.substr( nLastPos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// найти закрывающую скобку без учёта внутренних скобок
int NStr::FindCloseBracket( const std::string &szString, int nPos, const char cOpenBracket )
{
	return szString.find( brackets[cOpenBracket], nPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// найти закрывающую скобку с учётом внутренних скобок
int NStr::FindMultipleCloseBracket( const std::string &szString, int nPos, const char cOpenBracket )
{
	std::stack<char> stackBrackets;
	//
	stackBrackets.push( brackets[cOpenBracket] );
	for ( int i=nPos; i<szString.size(); ++i )
	{
		char c = szString[i];
		if ( IsOpenBracket(c) )
			stackBrackets.push( brackets[c] );
		else if ( c == stackBrackets.top() )
		{
			stackBrackets.pop();
			// check bracket stack for empty in the case of the bracket pop
			if ( stackBrackets.empty() )
				return i;
		}
	}
	return std::string::npos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// отрезать все символы 'cTrim' справа
void NStr::TrimRight( std::string &szString, const char cTrim )
{
	int nPos = szString.find_last_not_of( cTrim );
	if ( nPos == std::string::npos )
	{
		if ( szString.find_first_of( cTrim ) == 0 )
			szString.clear();
	}
	else
		szString.erase( nPos + 1, std::string::npos );
}
void NStr::TrimRight( std::string &szString, const char *pszTrim )
{
	int nPos = szString.find_last_not_of( pszTrim );
	if ( nPos == std::string::npos )
	{
		if ( szString.find_first_of( pszTrim ) == 0 )
			szString.clear();
	}
	else
		szString.erase( nPos + 1, std::string::npos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// вырезать все символы 'cTrim' из строки
class CSymbolCheckFunctional
{
private:
  const char *pszSymbols;
public:
  explicit CSymbolCheckFunctional( const char *pszNewSymbols ) : pszSymbols( pszNewSymbols ) {  }
  bool operator()( const char cSymbol )
  {
    for ( const char *p = pszSymbols; *p != 0; ++p )
    {
      if ( *p == cSymbol )
        return true;
    }
    return false;
  }
};
void NStr::TrimInside( std::string &szString, const char *pszTrim )
{
  szString.erase( std::remove_if(szString.begin(), szString.end(), CSymbolCheckFunctional(pszTrim)), szString.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// преобразовать целое в строку, разделяя каждые три знака (три порядка) специальным разделителем (.)
void NStr::ToDotString( std::wstring *pDst, int nVal, const wchar_t cSeparator )
{
	wchar_t buff[32], buff2[32];
	buff[0] = buff2[0] = 0;
  int nOrder = static_cast<int>( log10( nVal ) );
  int nOrderVal = static_cast<int>( pow( 10, nOrder - (nOrder % 3) ) );
  while ( nOrderVal > 1 )
  {
    int nVal1 = nVal / nOrderVal;
		swprintf( buff2, L"%d%c", nVal1, cSeparator );
		wcscat( buff, buff2 );
    nVal -= nVal1 * nOrderVal;
    nOrderVal /= 1000;
  }
	wcscat( buff, _itow(nVal, buff2, 10) );
  *pDst = buff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// форматирование строки
const char* __cdecl NStr::Format( const char *pszFormat, ... )
{
  static char buff[2048];
  va_list va;
	// 
  va_start( va, pszFormat );
  vsprintf( buff, pszFormat, va );
  va_end( va );
	//
	return buff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wchar_t* __cdecl NStr::Format( const wchar_t *pszFormat, ... )
{
  static wchar_t buff[2048];
  va_list va;
	// 
  va_start( va, pszFormat );
  vswprintf( buff, pszFormat, va );
  va_end( va );
	//
	return buff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* NStr::BinToString( const void *pData, int nSize, char *pszBuffer )
{
	for ( int i=0; i<nSize; ++i )
		sprintf( pszBuffer + i*2, "%.2X", int( ((unsigned char*)pData)[i] ) );
	return pszBuffer;
}
void* NStr::StringToBin( const char *pszData, void *pBuffer, int *pnSize )
{
	int nStrSize = strlen( pszData );
	char buffer[8];
	buffer[0] = '0';
	buffer[1] = 'x';
	buffer[4] = '\0';
	int nData;
	for ( int i=0; i<nStrSize/2; ++i )
	{
		buffer[2] = pszData[i*2 + 0];
		buffer[3] = pszData[i*2 + 1];
		sscanf( buffer, "%i", &nData );
		((unsigned char*)pBuffer)[i] = (unsigned char)nData;
	}
	if ( pnSize )
		*pnSize = nStrSize / 2;
	return pBuffer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NStr::CBracketCharSeparator::operator()( const char cSymbol )
{
	if ( IsOpenBracket(cSymbol) )
		stackBrackets.push( brackets[cSymbol] );
	else if ( stackBrackets.empty() )
		return cSymbol == cSeparator;
	else if ( cSymbol == stackBrackets.top() )
		stackBrackets.pop();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// <[+/-]>[dec digit]*
bool NStr::IsDecNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits == 0 )
		return false;												// this is not a number at all => zero length digits
	if ( (nNumDigits > 1) && (szString[nFirstDigit] == '0') )
		return false;												// hex number
	for ( i=nFirstDigit; (i < szString.size()) && IsDecDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
// <[+/-]>[0][oct digit]*
bool NStr::IsOctNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits == 0 )
		return false;
	if ( szString[nFirstDigit] != '0' )
		return false;
	if ( nNumDigits < 2 )
		return false;

	for ( i=nFirstDigit; (i < szString.size()) && IsOctDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
// <[+/-]>[0x][hex digit]*
bool NStr::IsHexNumber( const std::string &szString )
{
	if ( szString.empty() )
		return false;
	int i, nFirstDigit = IsSign( szString[0] ) ? 1 : 0;
	int nNumDigits = szString.size() - nFirstDigit;
	if ( nNumDigits < 3 )
		return false;
	if ( (szString[nFirstDigit] != '0') || (szString[nFirstDigit + 1] != 'x') )
		return false;
	for ( i=nFirstDigit + 2; (i < szString.size()) && IsHexDigit(szString[i]); ++i ) { ; }
	return ( (i > nFirstDigit) && (i == szString.size()) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int NStr::ToInt( const char *pszString )
{
	int nNumber = 0;
	sscanf( pszString, "%i", &nNumber );
	return nNumber;
}
float NStr::ToFloat( const char *pszString )
{
	float fNumber = 0;
	sscanf( pszString, "%f", &fNumber );
	return fNumber;
}
double NStr::ToDouble( const char *pszString )
{
	double fNumber = 0;
	sscanf( pszString, "%lf", &fNumber );
	return fNumber;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NStr::SetCodePage( int _nCodePage )
{
	nCodePage = _nCodePage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NStr::ToAscii( std::string *pRes, const std::wstring &szSrc )
{
	const int N_STACK_BUFF_SIZE = 1024;
	char static_buff[N_STACK_BUFF_SIZE];
	int nBufLeng = szSrc.length() * 2 + 10;
	char *pszBuf;
	if ( nBufLeng < N_STACK_BUFF_SIZE )
		pszBuf = static_buff;
	else
		pszBuf = new char[ nBufLeng ];
	int nRes = WideCharToMultiByte( nCodePage, 0, szSrc.c_str(), szSrc.length(), pszBuf, nBufLeng, 0, 0 );
	pszBuf[nRes] = 0;
	*pRes = pszBuf;
	if ( nBufLeng >= N_STACK_BUFF_SIZE )
		delete[] pszBuf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NStr::ToUnicode( std::wstring *pRes, const std::string &szSrc )
{
	const int N_STACK_BUFF_SIZE = 1024;
	WCHAR static_buff[N_STACK_BUFF_SIZE];
	int nBufLeng = szSrc.length() + 3;
	WCHAR *pszBuf;
	if ( nBufLeng < N_STACK_BUFF_SIZE )
		pszBuf = static_buff;
	else
		pszBuf = new WCHAR[ nBufLeng ];
	int nRes = MultiByteToWideChar( nCodePage, 0, szSrc.c_str(), szSrc.length(), pszBuf, nBufLeng );
	pszBuf[nRes] = 0;
	*pRes = pszBuf;
	if ( nBufLeng >= N_STACK_BUFF_SIZE )
		delete[] pszBuf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
