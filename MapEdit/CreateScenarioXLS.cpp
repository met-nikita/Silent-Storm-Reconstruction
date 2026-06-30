#include "StdAfx.h"
#include "MapEdit.h"
#include "ScenarioXLS.h"
#include "ItemsMgr.h"
#include "Export.h"
#include "ScenarioDB.h"

extern HRESULT InitDB( const CString &szConnection, CSession &session, CDBPropSet &propset );
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CreateScenarioXLS( SDBConnection *pConnection, const string &szSrc )
{
	CString szConnect = "Provider=MSDASQL.1;Persist Security Info=False;Extended Properties=\"DSN=Excel Files;CREATE_DB=";
	szConnect += szSrc.c_str();
	szConnect += ";DBQ=";
	szConnect += szSrc.c_str();
	szConnect += ";DriverId=790;MaxBufferSize=2048;FIRSTROWHASNAMES=1;PageTimeout=5;\"";

	HRESULT hr = InitDB( szConnect, pConnection->session, pConnection->propset );
	if ( FAILED( hr ) )
		return false;
	CExcelScenarioDB db;
	db.SetConnection( pConnection );
	string szQuery = "CREATE TABLE ScenarioClues ([ClueCode] TEXT,[Description] LONGTEXT,[Objective] TEXT, [ObjectiveDescription] LONGTEXT)";
	hr = ((CCommand<CAccessor<CExcelCluesAccessor> >)db).Open( pConnection->session, szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteScenarioClues( int nSceanrioID, const CPropMap *pScenarioProps )
{
	CPropMap::const_iterator isrc = pScenarioProps->find( "SrcXLSName" );
	if ( isrc == pScenarioProps->end() )
		return;
	SDBConnection connection;
	string szSrc = GetExportSrcDir() + (string)isrc->second->GetValue();
	if ( !CreateScenarioXLS( &connection, szSrc ) )
		return;

	CExcelScenarioDB db;
	db.SetConnection( &connection );
	//
	CScenarioClueDB clueDB;
	CScenarioObjectiveDB objectiveDB;

	if ( !clueDB.OpenScenario( nSceanrioID ) )
		return;
	while ( clueDB.MoveNext() == S_OK )
	{
		db.Insert( clueDB.m_szSmallDescription, GetString( clueDB.m_nDescription ), "", "" );
		if ( objectiveDB.Open( clueDB.m_nObjective1 ) && objectiveDB.MoveNext() == S_OK )
			db.Insert( "", "", objectiveDB.m_szName, GetString( objectiveDB.m_nDescr ) );
		if ( objectiveDB.Open( clueDB.m_nObjective2 ) && objectiveDB.MoveNext() == S_OK )
			db.Insert( "", "", objectiveDB.m_szName, GetString( objectiveDB.m_nDescr ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateScenarioXLS( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const int nID = nItemIDs[i];
		const CPropMap *pProps = pItems->GetPropList( nID );
		if ( pProps )
		{
			WriteScenarioClues( nID, pProps );
			pItems->ReleasePropList( pProps );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
