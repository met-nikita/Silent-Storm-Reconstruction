#include "StdAfx.h"
#include "Commands.h"
#include "..\Misc\StrProc.h"
#include "LogStream.h"
#include "..\FileIO\BasicChunk1.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGlobal
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRecord
{
	void *pCmdContext; 
	CmdHandler pCmdHandler;

	bool bSave;
	void *pVarContext;
	CValue sValue;
	CValue sDefaultValue;    // release SCommandInfo[+0x20] -- the registered default, restored by ResetVar
	VarHandler pVarHandler;

	SRecord(): pCmdContext( 0 ), pCmdHandler( 0 ), bSave( false ), pVarContext( 0 ), pVarHandler( 0 ) {}
};
typedef unordered_map<string, SRecord> TRecordsMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRecordsMap : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CRecordsMap)
public:
	TRecordsMap recordsMap;
	CPtr<CRecordsMap> pHold;
};
static CRecordsMap *pRecordsMap;
static int bIsExiting;
struct SKillRecordsMap
{
	~SKillRecordsMap() { if ( pRecordsMap ) pRecordsMap->pHold = 0; pRecordsMap = 0; bIsExiting = true; }
} killRecordsMap;
static CRecordsMap* GetRecordsMap()
{
	if ( bIsExiting )
		return new CRecordsMap;
	if ( !pRecordsMap )
	{
		pRecordsMap = new CRecordsMap;
		pRecordsMap->pHold = pRecordsMap;
	}
	return pRecordsMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdDefaultHandler( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void SplitString( const wstring &szCmd, vector<wstring> *pRes );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Value
////////////////////////////////////////////////////////////////////////////////////////////////////
CValue::CValue(): fVal( 0.0f )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CValue::CValue( float _fVal ): fVal( _fVal )
{
	szVal = NStr::ToUnicode( NStr::Format( "%.2f", fVal ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CValue::CValue( const wstring &_szVal ): fVal( 0.0f ), szVal( _szVal )
{
	fVal = _wtof( szVal.c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CValue::GetFloat() const
{
	return fVal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 (new helper, v1.2 VA 0x7ce990): FLD fVal / FISTP -- round-to-nearest(-even) under the
// default FPU control word; Float2Int is the tree's exact FLD/FISTP idiom.
int CValue::GetInt() const
{
	return Float2Int( fVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CValue::GetString() const
{
	return szVal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// VARS
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterCmd( const string &szID, CmdHandler pHandler, void *pContext )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	SRecord &sRecord = recordsMap[szID];
	sRecord.pCmdContext = pContext;
	sRecord.pCmdHandler = pHandler;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterVar( const string &szID, VarHandler pHandler, void *pContext, const CValue &sValue, bool bSave )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	TRecordsMap::iterator iTemp = recordsMap.find( szID );
	if ( iTemp != recordsMap.end() )
	{
		SRecord &sRecord = iTemp->second;
		sRecord.bSave = bSave;
		sRecord.pVarContext = pContext;
		sRecord.pVarHandler = pHandler;
		return;
	}

	SRecord &sRecord = recordsMap[szID];
	sRecord.bSave = bSave;
	sRecord.sValue = sValue;
	sRecord.sDefaultValue = sValue;    // remember the registered default so ResetVar can restore it
	sRecord.pVarContext = pContext;
	sRecord.pVarHandler = pHandler;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnregisterCmd( const string &szID )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	TRecordsMap::iterator iTemp = recordsMap.find( szID );
	if ( iTemp == recordsMap.end() )
		return;

	SRecord &sRecord = iTemp->second;
	sRecord.pCmdHandler = 0;
	sRecord.pCmdContext = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnregisterVar( const string &szID )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	TRecordsMap::iterator iTemp = recordsMap.find( szID );
	if ( iTemp == recordsMap.end() )
		return;

	SRecord &sRecord = iTemp->second;
	sRecord.pVarHandler = 0;
	sRecord.pVarContext = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetIDList( vector<string> *pList )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	pList->resize( recordsMap.size() );

	int nCount = 0;
	for ( TRecordsMap::const_iterator iTemp = recordsMap.begin(); iTemp != recordsMap.end(); iTemp++ )
	{
		(*pList)[nCount] = iTemp->first;
		nCount++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CValue& GetVar( const string &szID, const CValue &sDefault )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	TRecordsMap::const_iterator iTemp = recordsMap.find( szID );
	if ( iTemp == recordsMap.end() )
		return sDefault;

	return iTemp->second.sValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetVar( const string &szVar, const CValue &sValue )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	SRecord &sRecord = recordsMap[szVar];
	if ( sRecord.pVarHandler != 0 )
		sRecord.pVarHandler( szVar, sValue, sRecord.pVarContext );

	recordsMap[szVar].sValue = sValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ResetVar @0x3d4c80 -- restore the registered default. The release stores the default in the
// command record (SCommandInfo[+0x20]) and SetVar's it back; the (default-inserted) record of an
// unregistered var holds CValue() (0), so a never-registered var resets to 0.
////////////////////////////////////////////////////////////////////////////////////////////////////
void ResetVar( const string &szVar )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	SetVar( szVar, recordsMap[szVar].sDefaultValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCommand( const wstring &wsCommandStr )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	vector<wstring> wordsSet;
	SplitString( wsCommandStr, &wordsSet );

	if ( wordsSet.empty() )
		return;

	string szCommandName = NStr::ToAscii( *wordsSet.begin() );
	wordsSet.erase( wordsSet.begin() );

	TRecordsMap::iterator iTemp = recordsMap.find( szCommandName );
	if ( iTemp == recordsMap.end() )
	{
		csSystem << CC_RED << "Unknow command name'" << wsCommandStr << "'" << endl;
		csSystem << CC_RED << "Type 'help' to list available commands." << endl;
		return;
	}

	SRecord &sRecord = iTemp->second;
	if ( sRecord.pCmdHandler == 0 )
	{
		CmdDefaultHandler( szCommandName, wordsSet, sRecord.pCmdContext );
		return;
	}

	sRecord.pCmdHandler( szCommandName, wordsSet, sRecord.pCmdContext );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadConfig( const string &szFileName )
{
	CMemoryStream sStream;
	try
	{
		CFileStream sFile;
		sFile.OpenRead( szFileName.c_str() );
		sStream.WriteFrom( sFile );
		sStream << '\0';
		csSystem << "Executing " << szFileName << endl;
	}
	catch(...)
	{
		csSystem << "Can't open " << szFileName << endl;
	}

	vector<string> cmdsSet;
	NStr::SplitString( (const char*)sStream.GetBuffer(), cmdsSet, '\n' );
	for ( vector<string>::const_iterator iTemp = cmdsSet.begin(); iTemp != cmdsSet.end(); ++iTemp )
	{
		if ( iTemp->empty() )
			continue;

		string szCmd( *iTemp );
		NStr::TrimBoth( szCmd, "\n\r" );
		if ( szCmd.substr( 0, 1 ).compare( ";" ) == 0 )
			continue;
		if ( szCmd.substr( 0, 2 ).compare( "//" ) == 0 )
			continue;

		ProcessCommand( NStr::ToUnicode( szCmd ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveConfig( const string &szFileName )
{
	CPtr<CRecordsMap> pHold( GetRecordsMap() );
	TRecordsMap &recordsMap = pHold->recordsMap;

	FILE *pFile = fopen( szFileName.c_str(), "w+" );
	if ( pFile == 0 )
	{
		csSystem << "Can't open " << szFileName << endl;
		return;
	}

	fprintf( pFile, "//============================================================================\n" );
	fprintf( pFile, "// generated by S^2, do not modify\n" );
	fprintf( pFile, "//============================================================================\n" );
	for ( TRecordsMap::iterator iTemp = recordsMap.begin(); iTemp != recordsMap.end(); iTemp++ )
	{
		if ( !iTemp->second.bSave )
			continue;

		fprintf( pFile, "setvar %s = %s\n", iTemp->first.c_str(), NStr::ToAscii( iTemp->second.sValue.GetString() ).c_str() );
	}

	fclose( pFile );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Dependent command
////////////////////////////////////////////////////////////////////////////////////////////////////
CCmd::CCmd( const string &_szID, CmdHandler _pHandler, void *_pContext ):
	szID( _szID ), pHandler( _pHandler ), pContext( _pContext )
{
	RegisterCmd( szID, pHandler, pContext );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCmd::~CCmd()
{
	UnregisterCmd( szID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCmd::Run( const vector<wstring> &paramsSet )
{
	pHandler( szID, paramsSet, pContext );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Dependent var
////////////////////////////////////////////////////////////////////////////////////////////////////
CVar::CVar( const string &_szID, VarHandler pHandler, void *pContext, const CValue &sValue, bool bSave ):
	szID( _szID )
{
	RegisterVar( szID, pHandler, pContext, sValue, bSave );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVar::~CVar()
{
	UnregisterVar( szID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CValue& CVar::Get()
{
	return GetVar( szID, CValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVar::Set( const CValue &sValue )
{
	SetVar( szID, sValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdDefaultHandler( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.empty() )
	{
		const CValue &sValue = GetVar( szID, CValue() );
		csSystem << szID << " string = '" << sValue.GetString() << "' float = '" << sValue.GetFloat() << "'" << endl;
		return;
	}

	SetVar( szID, CValue( paramsSet.front() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void VarBoolHandler( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bool *pFlag = (bool*)pContext;

	*pFlag = false;
	if ( sValue.GetFloat() == 1 )
		*pFlag = 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SplitString( const wstring &szCmd, vector<wstring> *pRes )
{
	vector<wstring> params;
	list<WCHAR> stackBrackets;
	int i, nLastPos = 0;
	//
	for ( i = 0; i < szCmd.size(); ++i )
	{
		WCHAR c = szCmd[i];
		if ( NStr::IsOpenBracket(c) )
			stackBrackets.push_back( NStr::GetCloseBracket( c ) );
		else if ( stackBrackets.empty() )
		{
			if ( ( c == L' ' ) || ( c == L'\t' ) || ( c == L'\r' ) )
			{
				wstring szRes = szCmd.substr( nLastPos, i - nLastPos );
				if ( !szRes.empty() )
					params.push_back( szRes );
				nLastPos = i + 1; 
			}
		}
		else if ( c == stackBrackets.back() )
			stackBrackets.pop_back();
	}
	// last substring
	if ( nLastPos + 1 <= int( i ) )
		params.push_back( szCmd.substr( nLastPos ) );

	*pRes = params;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CmdPrintHelp( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	vector<string> varsSet;
	NGlobal::GetIDList( &varsSet );
	sort( varsSet.begin(), varsSet.end() );

	csSystem << CC_BLUE << "Commands:" << endl;
	for ( int nTemp = 0; nTemp < varsSet.size(); nTemp++ )
		csSystem << varsSet[nTemp] << endl;

	csSystem << CC_BLUE << "Total:" << (int)varsSet.size() << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdLoadConfig( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	for ( int nTemp = 0; nTemp < paramsSet.size(); ++nTemp )
		NGlobal::LoadConfig( NStr::ToAscii( paramsSet[nTemp] ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdSetVar( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 3 )
	{
		csSystem << "usage: " << szID << " 'name' = 'value'" << endl;		
		return;
	}

	if ( paramsSet[1].compare( L"=" ) != 0 )
		return;

	NGlobal::SetVar( NStr::ToAscii( paramsSet[0] ), NGlobal::CValue( paramsSet[2] ) );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(Commands)
	REGISTER_CMD( "help", CmdPrintHelp )
	REGISTER_CMD( "exec", CmdLoadConfig )
	REGISTER_CMD( "setvar", CmdSetVar )
FINISH_REGISTER
