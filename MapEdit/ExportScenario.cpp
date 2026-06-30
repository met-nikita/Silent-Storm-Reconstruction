#include "StdAfx.h"
#include "MapEdit.h"
#include "ExportScenario.h"
#include "ItemsMgr.h"
#include "SGData.h"
#include "Export.h"
#include "ScenarioDB.h"

bool EraseScenarioData( int nScenarioID );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjectiveData
{
	int nDbID;
	CObjective objective;

	SObjectiveData( int nID, const CObjective &o ) : nDbID(nID), objective(o) {}
};
static int InsertString( CItemsMgr *pStrs, const string &szStr )
{
	int nRet = pStrs->AddItem( -1, 76, szStr );
	if ( nRet > 0 )
	{
		const CPropMap *pProps = pStrs->GetPropList( nRet );
		if ( pProps )
		{
			CPropMap::const_iterator i = pProps->find( "String" );
			i->second->SetValue( szStr );
			pStrs->ReleasePropList( pProps );
		}
	}
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool Export( int nScenarioID, CItemsMgr *pMgr )
{
	const CPropMap *props = pMgr->GetPropList( nScenarioID );
	if ( !props )
		return false;
	CPropMap::const_iterator isrc = props->find( "SrcName" );
	if ( isrc == props->end() )
		return false;
	string szSrc = GetExportSrcDir() + (string)isrc->second->GetValue();
	//
	CScenGraph graph;
	if ( !graph.LoadZones( szSrc ) )
		return false;
	if ( !EraseScenarioData( nScenarioID ) )
		return false;
	//
	vector<SObjectiveData> objectives;
	unordered_map<CClueP, int>  clue2ID;
	CScenarioClueDB clueDB;
	CScenarioObjectiveDB objDB;
	const SResTree *pStrings = theApp.GetResTree( IDC_STRINGS_TREE );
	if ( !pStrings )
		return false;

	for ( unordered_map<string,CClue>::const_iterator i = graph.clues.begin(); i != graph.clues.end(); ++i )
	{
		const CClue &clue = i->second;
		vector<int> objectivesIDs;
		//
		for ( int j = 0; j < clue.objectives.size(); ++j )
		{
			const CObjective &obj = clue.objectives[j];
			int nZone1 = obj.zone2open.size() > 0 ? GetScenarioZoneID( obj.zone2open[0], nScenarioID ) : 0;
			int nZone2 = obj.zone2open.size() > 1 ? GetScenarioZoneID( obj.zone2open[1], nScenarioID ) : 0;
			int nZone3 = obj.zone2open.size() > 2 ? GetScenarioZoneID( obj.zone2open[2], nScenarioID ) : 0;
			int nZoneBlock1 = obj.zone2destroy.size() > 0 ? GetScenarioZoneID( obj.zone2destroy[0], nScenarioID ) : 0;
			int nZoneBlock2 = obj.zone2destroy.size() > 1 ? GetScenarioZoneID( obj.zone2destroy[1], nScenarioID ) : 0;
			int nZoneBlock3 = obj.zone2destroy.size() > 2 ? GetScenarioZoneID( obj.zone2destroy[2], nScenarioID ) : 0;
			if ( !objDB.Open( -1 ) )
				return false;
			strcpy( objDB.m_szName, obj.action.c_str() );
			objDB.m_nDescr = InsertString( pStrings->pItemsTree, obj.description );
			if ( objDB.m_nDescr <= 0 )
			{
				ASSERT(0);
				return false;
			}
			objDB.m_nZone2Open1 = nZone1;
			objDB.m_nZone2Open2 = nZone2;
			objDB.m_nZone2Open3 = nZone3;
			objDB.m_nZone2Block1 = nZoneBlock1;
			objDB.m_nZone2Block2 = nZoneBlock2;
			objDB.m_nZone2Block3 = nZoneBlock3;
			objDB.m_nScenario = nScenarioID;
			HRESULT hr = objDB.Insert( 1 );
			if ( FAILED( hr ) )
			{
				DisplayOLEDBErrorRecords( hr );
				return false;
			}
			hr = objDB.MoveNext();
			if ( FAILED( hr ) )
			{
				DisplayOLEDBErrorRecords( hr );
				return false;
			}
			objectivesIDs.push_back( objDB.m_nID );
			objectives.push_back( SObjectiveData( objDB.m_nID, obj ) );
		}
		int nZone1 = clue.zone2place.size() > 0 ? GetScenarioZoneID( clue.zone2place[0], nScenarioID ) : 0;
		int nZone2 = clue.zone2place.size() > 1 ? GetScenarioZoneID( clue.zone2place[1], nScenarioID ) : 0;
		int nZone3 = clue.zone2place.size() > 2 ? GetScenarioZoneID( clue.zone2place[2], nScenarioID ) : 0;
		if ( !clueDB.Open( -1 ) )
			return false;
		strcpy( clueDB.m_szType, clue.type.c_str() );
		clueDB.m_bPermanent = clue.isPermanent;
		clueDB.m_nMinParent2Open = clue.nMinClueToOpen;
		clueDB.m_nZone2Place1 = nZone1;
		clueDB.m_nZone2Place2 = nZone2;
		clueDB.m_nZone2Place3 = nZone3;
		clueDB.m_nObjective1 = objectivesIDs.size() > 0 ? objectivesIDs[0] : 0;
		clueDB.m_nObjective2 = objectivesIDs.size() > 1 ? objectivesIDs[1] : 0;
		clueDB.m_nScenario = nScenarioID;
		clueDB.m_nDescription = InsertString( pStrings->pItemsTree, clue.description );
		strcpy( clueDB.m_szSmallDescription, clue.code.c_str() );
		clueDB.m_nItemID = clue.nItemID;
		clueDB.m_nPersID = clue.nPersID;
		HRESULT hr = clueDB.Insert( 1 );
		if ( FAILED( hr ) )
		{
			DisplayOLEDBErrorRecords( hr );
			return false;
		}
		hr = clueDB.MoveNext();
		if ( FAILED( hr ) )
		{
			DisplayOLEDBErrorRecords( hr );
			return false;
		}
		clue2ID[clue.code] = clueDB.m_nID;
	}
	//
	CScenarioObjective2ClueDB obj2clueDB;
	for ( int i = 0; i < objectives.size(); ++i )
	{
		const SObjectiveData &d = objectives[i];
		for ( int j = 0; j < d.objective.cluesToOpen.size(); ++j )
		{
			HRESULT hr = obj2clueDB.Open( "SELECT ObjectiveID, ClueID FROM ScenarioObjective2Clues WHERE ID = -1" );
			if ( FAILED( hr ) )
				return false;
			string szClue = d.objective.cluesToOpen[j];
			if ( clue2ID.find( szClue ) == clue2ID.end() )
			{
				string sz = "Objective \"";
				sz += d.objective.description + "\":\n";
				sz += string( "ClueToOpen \"" ) + szClue + "\" not found";
				MessageBox( 0, sz.c_str(), "Warning", MB_OK | MB_ICONWARNING );
				continue;
			}
			obj2clueDB.m_nObjectiveID = d.nDbID;
			obj2clueDB.m_nClueID = clue2ID[szClue];
			hr = obj2clueDB.Insert();
			if ( FAILED( hr ) )
			{
				DisplayOLEDBErrorRecords( hr );
				return false;
			}
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportScenario( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
		Export( nItemIDs[i], pItems );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool EraseScenarioData( int nScenarioID )
{
	CScenarioClueDB clues;
	CScenarioObjectiveDB objectives;

	HRESULT hr = clues.OpenScenario( nScenarioID );
	if ( FAILED( hr ) )
		return false;

	while ( clues.MoveNext() == S_OK ) 
	{
		vector<int> ids;
		if ( clues.m_nObjective1 > 0 ) ids.push_back( clues.m_nObjective1 );
		if ( clues.m_nObjective2 > 0 ) ids.push_back( clues.m_nObjective2 );
		hr = clues.Delete();
		for ( int i = 0; i < ids.size(); ++i )
		{
			if ( FAILED( objectives.Open( ids[i] ) ) )
				continue;
			if( S_OK == objectives.MoveNext() )
				hr = objectives.Delete();
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
