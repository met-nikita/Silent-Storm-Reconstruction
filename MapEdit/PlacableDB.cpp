#include "StdAfx.h"
#include "PlacableDB.h"

extern HRESULT InitDB( CSession &session, CDBPropSet &propset );
extern string IToA( int n );
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacableAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szUserName[255];
BEGIN_ACCESSOR_MAP( CPlacableAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szUserName)
  END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacableTableDB : public CBaseDBCmd<CAccessor<CPlacableAccessor> >
{
public:
	int Insert( const string &szUserName )
	{
		if ( !HasConnection() )
			SetConnection( &dbConnection );
		string szQuery = "SELECT ID, UserName FROM PlacableObjects WHERE ID=-1";
		if ( FAILED( CBaseDBCmd<CAccessor<CPlacableAccessor> >::Open( szQuery ) ) )
			return -1;
		_tcscpy( m_szUserName, szUserName.c_str() );
		HRESULT hr = CBaseDBCmd<CAccessor<CPlacableAccessor> >::Insert( 1 );
		if ( FAILED(hr) )
		{
			DisplayOLEDBErrorRecords( hr );
			return -1;
		}
		hr = MoveNext();
		if ( FAILED(hr) )
		{
			DisplayOLEDBErrorRecords( hr );
			return -1;
		}
		return m_nID;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlaceDB::InsertPlacableObject( const string &szRelTable, const string &szItemName, int nItemID )
{
	CPlacableTableDB db;

	int nID = db.Insert( szItemName );
	if ( nID <= 0 )
		return;

	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = string( "SELECT ID, PlacableID FROM " ) + szRelTable + " WHERE ID=";
	szQuery += IToA( nItemID );
	if ( FAILED( CBaseDBCmd<CAccessor<CPlacableIDAccessor> >::Open( szQuery ) ) || MoveNext() != S_OK )
		return;
	m_nPlacableID = nID;
	HRESULT hr = SetData( 1 );

  if ( FAILED( hr ) )
    DisplayOLEDBErrorRecords( hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlaceDB::GetPlaceID( const string &szObjectTable, int nItemID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = string( "SELECT ID, PlacableID FROM " ) + szObjectTable + " WHERE ID=";
	szQuery += IToA( nItemID );
	if ( FAILED( CBaseDBCmd<CAccessor<CPlacableIDAccessor> >::Open( szQuery ) ) || MoveNext() != S_OK )
	{
		return -1;
	}
	return m_nPlacableID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
