#include "StdAfx.h"
//
#include "RPGGlobal.h"
//
#include "..\Misc\StrProc.h"
//
#include "scScenarioTracker.h"
#include "scCommands.h"
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetParam( const vector<wstring> &paramsSet, int n )
{
	if ( n >= paramsSet.size() )
		return "";
	//
	string szParam = NStr::ToAscii( paramsSet[n] );
	NStr::ToUpper( szParam );
	return szParam;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandScenario( const vector<wstring> &paramsSet, CScenarioTracker *pScenarioTracker )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	if ( !IsValid( pScenarioTracker ) )
		return;
	//
	if ( GetParam( paramsSet, 0 ) == "LIST" )
		pScenarioTracker->PrintScenarioList();
	else if ( GetParam( paramsSet, 0 ) == "DRAW" )
		pScenarioTracker->DrawScenario();
	else if ( GetParam( paramsSet, 0 ) == "TAKE" )
		pScenarioTracker->CheatTakeClue( pScenarioTracker->GetClueByName( GetParam( paramsSet, 1 ) ) );
	else if ( GetParam( paramsSet, 0 ) == "DESTROY" )
		pScenarioTracker->CheatDestroyClue( pScenarioTracker->GetClueByName( GetParam( paramsSet, 1 ) ) );
//	else if ( GetParam( paramsSet, 0 ) == "OPEN" )
//		pScenarioTracker->CheatOpenZone( GetParam( paramsSet, 1 ) );
	else
		pScenarioTracker->CreateScenario( GetParam( paramsSet, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandScenario( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	CDynamicCast<CScenarioTracker> pScenarioTracker( (CObjectBase*)pContext );
	if ( IsValid( pScenarioTracker ) )
		CommandScenario( paramsSet, pScenarioTracker );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandZone( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CDynamicCast<CScenarioTracker> pScenarioTracker( (CObjectBase*)pContext );
	if ( !IsValid( pScenarioTracker ) )
		return;
	//
	CPtr<CScenarioZone> pZone = pScenarioTracker->GetZoneByName( GetParam( paramsSet, 0 ) );
	if ( IsValid( pZone ) )
		pScenarioTracker->OpenZone( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}