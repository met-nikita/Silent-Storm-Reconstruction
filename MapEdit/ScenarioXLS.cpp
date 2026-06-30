#include "StdAfx.h"
#include "ScenarioXLS.h"
#include "MapEdit.h"
#include "ItemsMgr.h"

extern string ReplaceApo( const string &str );
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExcelScenarioDB::Insert( const string &szCode, const string &_szDescr, const string &szAction, const string &_szObjDescription )
{
	string szQuery = "INSERT INTO ScenarioClues (";
	string szVal = "(";
	//
	string szDescr = ReplaceApo( _szDescr );
	string szObjDescription = ReplaceApo( _szObjDescription );
	//
	if ( szCode != "" ) {	szQuery += "ClueCode,"; szVal += string("\'") + szCode + "\',"; }
	if ( szDescr != "" ) { szQuery += " Description,"; szVal += string("\'") + szDescr + "\',"; }
	if ( szAction != "" ) {	szQuery += " Objective,"; szVal += string("\'") + szAction + "\',"; }
	if ( szObjDescription != "" ) {	szQuery += " ObjectiveDescription"; szVal += string("\'") + szObjDescription + "\'"; }
	
	if ( szQuery[szQuery.size()-1] == ',' )
		szQuery.pop_back();
	if ( szVal[szVal.size()-1] == ',' )
		szVal.pop_back();
	szQuery += string( ") VALUES " ) + szVal + ")";

	Close();
	HRESULT hr = CCommand<CAccessor<CExcelCluesAccessor> >::Open( pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
	if ( FAILED(hr) )
		DisplayOLEDBErrorRecords( hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetString( int nID  )
{
	static const SResTree tree = *theApp.GetResTree( IDC_STRINGS_TREE );
	const CPropMap *pProps = tree.pItemsTree->GetPropList( nID );
	if ( !pProps )
		return "";
	CPropMap::const_iterator i = pProps->find( "String" );
	return i->second->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
