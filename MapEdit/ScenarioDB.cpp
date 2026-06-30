#include "StdAfx.h"
#include "ScenarioDB.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CZoneAccessor
{
public:
	LONG  m_nID;
	LONG  m_nScenario;
	TCHAR m_szDescr[255];

	BEGIN_COLUMN_MAP(CZoneAccessor)
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_nScenario)
		COLUMN_ENTRY(3, m_szDescr)
	END_COLUMN_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioZoneDB : public CBaseDBCmd<CAccessor<CZoneAccessor> >
{
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetScenarioZoneID( const string &_szZone, int nScenarioID )
{
	static CScenarioZoneDB db;

	string szZone = _szZone;
	NStr::TrimBoth( szZone );
	string szQuery = "SELECT ID, Scenario, SmallDescription FROM ScenarioZones WHERE Scenario=";
	szQuery += IToA( nScenarioID ) + " AND SmallDescription=\'" + szZone + '\'';
	if ( FAILED( db.Open( szQuery ) ) )
		return 0;
	if ( db.MoveNext() == S_OK )
		return db.m_nID;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioClueDB::Open( int nID )
{
	string szQ = "SELECT ID, Type, ItemID, PersID, State, [Permanent], MinParentToOpen, ZoneToPlace1, ZoneToPlace2, ZoneToPlace3, Objective1, Objective2, Scenario, Description, SmallDescription FROM ScenarioClues WHERE ID=";
	HRESULT hr = CBaseDBCmd<CAccessor<CClueAccessor> >::Open( szQ + IToA(nID) );
	return !FAILED(hr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioClueDB::OpenCode( int nScenarioID, const string &szCode )
{
	string szQ = "SELECT ID, Type, ItemID, PersID, State, [Permanent], MinParentToOpen, ZoneToPlace1, ZoneToPlace2, ZoneToPlace3, Objective1, Objective2, Scenario, Description, SmallDescription FROM ScenarioClues WHERE SmallDescription=\'";
	szQ += szCode + "\' AND Scenario=" + IToA( nScenarioID );
	HRESULT hr = CBaseDBCmd<CAccessor<CClueAccessor> >::Open( szQ );
	return !FAILED(hr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioClueDB::OpenScenario( int nID )
{
	string szQ = "SELECT ID, Type, ItemID, PersID, State, [Permanent], MinParentToOpen, ZoneToPlace1, ZoneToPlace2, ZoneToPlace3, Objective1, Objective2, Scenario, Description, SmallDescription FROM ScenarioClues WHERE Scenario=";
	HRESULT hr = CBaseDBCmd<CAccessor<CClueAccessor> >::Open( szQ + IToA(nID) );
	return !FAILED(hr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioObjectiveDB::Open( int nID )
{
	string szQ = "SELECT ID, Type, ZoneToOpen1, ZoneToOpen2, ZoneToOpen3, Description, Scenario, ZoneToBlock1, ZoneToBlock2, ZoneToBlock3 FROM ScenarioObjectives WHERE ID=";
	HRESULT hr = CBaseDBCmd<CAccessor<CObjectiveAccessor> >::Open( szQ + IToA(nID) );
	return !FAILED(hr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
