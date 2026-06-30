#include "StdAfx.h"
#include "MapEdit.h"
#include "ScenarioXLS.h"
#include "ItemsMgr.h"
#include "Export.h"
#include "ScenarioDB.h"

extern HRESULT InitDB( const CString &szConnection, CSession &session, CDBPropSet &propset );
extern string ReplaceApo( const string &str );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetString( int nID, const string &szStr )
{
	static const SResTree tree = *theApp.GetResTree( IDC_STRINGS_TREE );
	const CPropMap *pProps = tree.pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "String" );
	i->second->SetValue( szStr );
	tree.pItemsTree->ReleasePropList( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ReadScenarioClues( int nScenarioID, const CPropMap *pScenarioProps )
{
	CPropMap::const_iterator isrc = pScenarioProps->find( "SrcXLSName" );
	if ( isrc == pScenarioProps->end() )
		return;
	string szSrc = GetExportSrcDir() + (string)isrc->second->GetValue();

	CString szConnect = "Provider=MSDASQL.1;Persist Security Info=False;Extended Properties=\"DSN=Excel Files;DBQ=";
	szConnect += szSrc.c_str();
	szConnect += ";DriverId=790;MaxBufferSize=2048;PageTimeout=5;\"";

	SDBConnection connection;
	HRESULT hr = InitDB( szConnect, connection.session, connection.propset );
	if ( FAILED( hr ) )
		return;

	CExcelScenarioDB db;
	CScenarioClueDB clueDB;
	CScenarioObjectiveDB objectiveDB;

	db.SetConnection( &connection );

	hr = db.Open( "SELECT * FROM `ScenarioClues$`" );
	if ( FAILED( hr ) )
		return;
	int nClueID = -1;
	int nObjective = 0;
	while ( db.MoveNext() == S_OK )
	{
		if ( string( db.m_szClueCode ) != "" ) // строка является клюесом
		{
			nClueID = -1;
			if ( !clueDB.OpenCode( nScenarioID, db.m_szClueCode ) || clueDB.MoveNext() != S_OK )
				continue;
			nClueID = clueDB.m_nID;
			string szDescr = GetString( clueDB.m_nDescription );
			szDescr = ReplaceApo( szDescr );
			if ( szDescr != db.m_szClueDescription )
				SetString( clueDB.m_nDescription, db.m_szClueDescription );
			nObjective = 0;
		}
		// видимо строка является обжективом
		if ( string( db.m_szObjectiveAction ) == "" )
			continue;
		if ( nClueID < 0 || nObjective >= 2 )
		{
			ASSERT(0);
			continue;
		}
		if ( !objectiveDB.Open( nObjective++ == 0 ? clueDB.m_nObjective1 : clueDB.m_nObjective2 ) || objectiveDB.MoveNext() != S_OK )
			continue;
		ASSERT( string( objectiveDB.m_szName ) == db.m_szObjectiveAction );
		string szDescr = GetString( objectiveDB.m_nDescr );
		szDescr = ReplaceApo( szDescr );
		if ( szDescr != db.m_szObjectiveDescription )
			SetString( objectiveDB.m_nDescr, db.m_szObjectiveDescription );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ReadScenarioXLS( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const int nID = nItemIDs[i];
		const CPropMap *pProps = pItems->GetPropList( nID );
		if ( pProps )
		{
			ReadScenarioClues( nID, pProps );
			pItems->ReleasePropList( pProps );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
