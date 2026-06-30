#include "StdAfx.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataRPG.h"
#include "rpgGlobal.h"
#include "scScenarioTracker.h"
#include "wMain.h"
#include "wUICommands.h"
#include "wInterface.h"
#include "..\Script\lua.h"
#include "A5Script.h"
#include "scriptUI.h"		// NScript::RegisterScriptUITagMethods (window.x gettable/settable tag methods)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScript
{
externA5 Script::SRegFunction pRegList[];
Script::SRegFunction pLuaPtrTagFuncList[] = { (0,0) };
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SharedInit( Script *scr )
{
	scr->Register( pRegList );
	int nTag = 0;
	nTag = scr->RegisterNewTag( pLuaPtrTagFuncList );
	ASSERT( nTag == tagLuaCPtr );
	nTag = scr->RegisterNewTag( pLuaPtrTagFuncList );
	ASSERT( nTag == tagLuaCObj );
	// script-UI bridge: a third user tag for NUI::CWindow userdata. The built-in type tags are 0..5
	// (LUA_TUSERDATA..LUA_TFUNCTION), tagLuaCPtr/tagLuaCObj are 6/7, so this third RegisterNewTag yields
	// 8 == tagLuaWindow -- byte-identical to retail's window tag. Like the CPtr/CObj tags it carries no
	// tagmethods (the empty pLuaPtrTagFuncList): windows are driven through the registered global
	// functions (CreateWindow/GetWindow/windowGet/SetProperty/...), exactly as the CPtr/CObj objects are.
	nTag = scr->RegisterNewTag( pLuaPtrTagFuncList );
	ASSERT( nTag == tagLuaWindow );
	// make the window property accessors reachable via lua `window.x` / `window.x = v` (the retail wiring):
	// windowGet/SetProperty become the tagLuaWindow gettable/settable tag methods.
	RegisterScriptUITagMethods( scr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandShowScriptError( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( luaLastError.szError.empty() )
	{
		csSystem << "there are no script errors" << endl;
		return;
	}
	//
	csSystem << endl << "Script last error:" << endl;
	csSystem << CC_RED << "Script error: " << CC_GREY << luaLastError.szError << endl;
	vector< SLUAError::SLUAStackTrace >::iterator i;
	for ( i = luaLastError.stack.begin(); i != luaLastError.stack.end(); ++i )
	{
		csSystem << CC_RED << "\t" << (*i).nDepth;
		csSystem << CC_GREY << "\tfile: \"" << (*i).szSource << "\"";
		csSystem << ",   function \"" << (*i).szFunctionName << "\"";
		csSystem << ",   defined at line  " << (*i).nDefinedAtLine;
		csSystem << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScript::CScript():
	cmdShowError( "scripterror", CommandShowScriptError, this )
{
	SharedInit(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface-action id-queue (release): tracks the ids of UI commands queued by this script. The lua
// WaitForUI(id) helper (Scripts\Common.l) polls IsUIActionIDPresent(id) until the command finishes; the
// command's executor / the dialog UI posts CCmdInterfaceEvent(id), which CWorld::ExecuteCommand turns into
// pOwnScript->RemoveUIActionID(id). (Replaces the dev's per-type counter OnInterfaceActionStarted/Finished.)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScript::IsUIActionIDPresent( int nID ) const
{
	for ( list<int>::const_iterator i = interfaceActionIDs.begin(); i != interfaceActionIDs.end(); ++i )
		if ( *i == nID )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::AddUIActionID( int nID )
{
	interfaceActionIDs.push_back( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::RemoveUIActionID( int nID )
{
	interfaceActionIDs.remove( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScript::AddUICommandWithID( NWorld::CUICmd *pCmd )
{
	if ( !IsValid( pCmd ) )
		return 0;
	AddUIActionID( pCmd->nID );
	PushNumber( pCmd->nID );		// return the assigned id to the script (lua WaitForUI(id) will poll it)
	AddUICommand( pCmd );
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScript::RunScriptByID( int nID )
{
	CDBPtr<NDb::CScript> pDBScript = NDb::GetDBScript( nID );
	if ( IsValid( pDBScript ) )
		return DoBuffer( (const char *)pDBScript->strCode.c_str(), pDBScript->strCode.length() );
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScript::RunScriptFile( const string &szFileName )
{
	try
	{
		CFileStream f;
		CMemoryStream m;
		f.OpenRead( szFileName.c_str() );
		m.WriteFrom( f );
		return DoBuffer( (const char *)m.GetBuffer(), m.GetSize(), szFileName.c_str() );
	}
	catch ( ... )
	{
		DebugTrace( "Can't find script file %s", szFileName.c_str() );
		ASSERT( 0 );
		return -1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NScenario::CScenarioTracker* CScript::GetScenarioTracker()
{
	return pWorld->GetGlobalGame()->pScenarioTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalGame* CScript::GetGlobalGame()
{
	return pWorld->GetGlobalGame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::AddUICommand( NWorld::CUICmd *pCmd )
{
	pWorld->AddUICommand( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::AddMiscObject( CObjectBase *pObj )
{
	miscObjectsHolder.push_back( pObj );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ScriptWarning( const string &message )
{
	csSystem << CC_RED << "Script warning: " << CC_GREY << message << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ScriptError( const string &message )
{
	csSystem << CC_RED << "Script error: " << CC_GREY << message << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
CPtr<NScript::CScript> pScript;
////////////////////////////////////////////////////////////////////////////////////////////////////
NScript::CScript *GetScript()
{
	if ( !IsValid( pScript ) )
		return 0;
	return pScript;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Command proccessing
////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessCommand( const wstring &szCmd )
{
	if ( szCmd.empty() || ( szCmd.length() >= 2 && szCmd[0] == '-' && szCmd[1] == '-') ) // Skip comments
		return;
	if ( szCmd[0] == '@' )
	{
		NScript::CScript *pScr = GetScript();
		if ( !pScr )
		{
			csSystem << "Error executing script: no script is allowed in that moment";
			return;
		}
		string expr = NStr::ToAscii( szCmd );
		expr = expr.substr( 1, expr.length() );
		int nErr = pScr->DoString( expr.c_str() );
		if ( nErr != 0 )
			csSystem << CC_RED << "script error : " << ErrorToString(nErr) << endl;
	}
	else
		NGlobal::ProcessCommand( szCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void RunScriptFile( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() == 0 )
	{
		csSystem << "usage: cmd < filename | ID >" << endl;
		return;
	}
	//
	NScript::CScript *pScr = GetScript();
	if ( !pScr )
	{
		csSystem << "Error querying script: no script is allowed in that moment";
		return;
	}
	//
	const char *pszRez = 0;
	string szParam = NStr::ToAscii( szParams.front() );
	int nID = atoi( szParam.c_str() );
	if ( nID > 0 )
	{
		csSystem << "Executing script, ID = " << nID << " ..." << endl;
		pszRez = ErrorToString( pScr->RunScriptByID( nID ) );
	}
	else
	{
		szParam = "scripts\\" + szParam;
		csSystem << "Executing script, filename = '" << szParam << "' ..." << endl;
		pszRez = ErrorToString( pScr->RunScriptFile( szParam ) );
	}
	//
	csSystem << pszRez << "." << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void PrintScriptState( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() == 0 )
	{
		csSystem << "usage: cmd all|ObjectName";
		return;
	}

	string strParam = NStr::ToAscii( szParams.front() );
	NScript::CScript *pScr = GetScript();
	if ( !pScr )
	{
		csSystem << "Error querying script: no script is allowed in that moment";
		return;
	}
	if ( strParam == "all" )
		csSystem << pScr->GetStateAsText();
	else
		csSystem << pScr->GetObjectAsText( strParam.c_str() );
}
#include "..\Misc\RandomGen.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static void TestRnd( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	if ( szParams.size() == 0 )
	{
		csSystem << "usage: rnd number";
		return;
	}
	int nMaxC = NStr::ToInt( NStr::ToAscii( szParams.front() ) );
	int nCount = 0;
	int nLast = 200;
	for( int i = 0; i < nMaxC; ++i )
	{
		int nN = random.Get(100);
		if ( abs(nN-nLast) < 10 )
			nCount++;
		nLast = nN;
	}
	csSystem << "Rnd test:" << (100*nCount)/nMaxC << "%" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*static void ExecuteScriptThreads( const vector<wstring> &szParams, void *pContext )
{
	NScript::CScript *pScr = GetScript();
	if ( !pScr )
	{
		csSystem << "Error executing script: no script is allowed in that moment";
		return;
	}
  pScr->ExecuteThreads();
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(A5Script)
	REGISTER_CMD( "script_run", RunScriptFile )
	REGISTER_CMD( "script_show", PrintScriptState )
	REGISTER_CMD( "script_rnd", TestRnd )
//	REGISTER_CMD( "continue", ExecuteScriptThreads )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NScript;
REGISTER_SAVELOAD_CLASS( 0x70652130, CScript );