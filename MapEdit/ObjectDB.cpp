#include "StdAfx.h"
#include "ObjectDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef CBaseDBCmd<CDynamicAccessor > CParentDB;
string CObjectDB::MakeQuery( const string &szTable, const string &szColumn, int nObjectID )
{
	string szQuery = "SELECT \"";
	szQuery += szColumn;
	szQuery += "\" FROM " + szTable;
	szQuery += " WHERE ID=" + IToA( nObjectID );
	return szQuery;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVariant CObjectDB::ReadValue( int type, const string &szColumn )
{
	USES_CONVERSION;
	const char *pCol = szColumn.c_str();
	DBSTATUS status;
	if ( !GetStatus( pCol, &status ) || DBSTATUS_S_ISNULL == status )
		return CVariant();

	switch ( type )
	{
	case CVariant::VT_STR:
		{
			LPWSTR p = (LPWSTR)CParentDB::GetValue( pCol );
			return W2A( p );
		}
	case CVariant::VT_INT:
		{
			int nVal;
			CParentDB::GetValue( pCol, &nVal );
			return nVal;
		}
	case CVariant::VT_FLOAT:
		{
			float fVal;
			CParentDB::GetValue( pCol, &fVal );
			return fVal;
		}
	case CVariant::VT_BOOL:
		{
			USHORT val;
			CParentDB::GetValue( pCol, &val );
			return val;
		}
	}
	return CVariant();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVariant CObjectDB::GetValue( int type, const string &szTable, const string &szColumn, int nObjectID )
{
	SetConnection( &dbConnection );
	if ( FAILED( Open( MakeQuery( szTable, szColumn, nObjectID ) ) ) )
		return CVariant();
	HRESULT hr = MoveNext();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return CVariant();
	}
	//
	return ReadValue( type, szColumn );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectDB::SetValue( int type, const string &szTable, const string &szColumn, int nObjectID, CVariant var )
{
	USES_CONVERSION;
	SetConnection( &dbConnection );
	if ( FAILED( Open( MakeQuery( szTable, szColumn, nObjectID ) ) ) )
		return CVariant();
	HRESULT hr = MoveNext();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return CVariant();
	}
	//
	const char *pCol = szColumn.c_str();
	bool bRet = false;
	switch ( type )
	{
		case CVariant::VT_STR:
		{
			LPWSTR p = A2W( (const char*)var );
			LPWSTR pStr = (LPWSTR)CParentDB::GetValue( pCol );
			wcscpy( pStr, p );
			SetLength( pCol, 2 * wcslen( pStr ) );
			bRet = true;
			break;
		}
		case CVariant::VT_INT:
		{
			int nVal = var;
			bRet = CParentDB::SetValue( pCol, nVal );
			break;;
		}
		case CVariant::VT_FLOAT:
		{
			float fVal = var;
			bRet = CParentDB::SetValue( pCol, fVal );
			break;
		}
		case CVariant::VT_BOOL:
		{
			USHORT val = (int)var;
			bRet = CParentDB::SetValue( pCol, val );
			break;
		}
	}
	SetStatus( pCol, DBSTATUS_S_OK );
	if ( bRet )
	{
		hr = SetData();
		if ( FAILED( hr ) )
		{
			DisplayOLEDBErrorRecords( hr );
			return false;
		}
		Close();
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectDB::GetValueBatch( vector<SValue> *pValues, const string &szTable, int nObjectID )
{
	if ( !pValues )
	{
		ASSERT(0);
		return false;
	}
	//
	SetConnection( &dbConnection );
	string szQuery = "SELECT ";
	for ( int i = 0; i < pValues->size(); ++i )
		szQuery += '\"' + (*pValues)[i].szColumn + '\"' + ", ";
	szQuery.pop_back();
	szQuery.pop_back();
	szQuery += " FROM " + szTable;
	szQuery += " WHERE ID=" + IToA( nObjectID );

	if ( FAILED( Open( szQuery ) ) )
		return false;
	HRESULT hr = MoveNext();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	for ( int i = 0; i < pValues->size(); ++i )
	{
		SValue &v = (*pValues)[i];
		v.value = ReadValue( v.nType, v.szColumn );
	}
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
