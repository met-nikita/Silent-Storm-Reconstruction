#include "StdAfx.h"
#include "Items4PersDB.h"

extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItems4PersDB::OpenQuery( const string &szQuery )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	if ( FAILED( CBaseDBCmd<CAccessor<CItems4PersAccessor> >::Open( szQuery ) ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItems4PersDB::GetItems( vector<SItem> *pItems, const string &szTable, int nPersID )
{
	string szQuery = "SELECT ID, RPGPersID, RPGItemID, Quantity FROM " + szTable + " WHERE RPGPersID=" + IToA( nPersID );
	if ( !OpenQuery( szQuery ) )
		return false;
	pItems->clear();
	while ( MoveNext() == S_OK )
	{
		pItems->push_back( SItem( m_nItemID, m_nQuantity ) );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItems4PersDB::SetItemQuantity( const string &szTable, int nPersID, int nItemID, int nQuantity )
{
	string szQuery = "SELECT ID, RPGPersID, RPGItemID, Quantity FROM " + szTable + " WHERE RPGPersID=" + IToA( nPersID );
	szQuery += " AND RPGItemID=" + IToA( nItemID );
	if ( !OpenQuery( szQuery ) )
		return false;
	HRESULT hr = S_OK;
	if ( MoveNext() == S_OK )
	{
		if ( nQuantity <= 0 )
			hr = Delete();
		else
		{
			m_nQuantity = nQuantity;
			hr = SetData( 1 );
		}
	}
	else if ( nQuantity > 0 )
	{
		m_nPersID = nPersID;
		m_nItemID = nItemID;
		m_nQuantity = nQuantity;
		hr = Insert( 1 );
	}
	if ( FAILED( hr ) )
	{
	  DisplayOLEDBErrorRecords( hr );
	  return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItems4PersDB::GetItemQuantity( const string &szTable, int nPersID, int nItemID )
{
	string szQuery = "SELECT ID, RPGPersID, RPGItemID, Quantity FROM " + szTable + " WHERE RPGPersID=" + IToA( nPersID );
	szQuery += " AND RPGItemID=" + IToA( nItemID );
	if ( !OpenQuery( szQuery ) )
		return -1;
	if ( MoveNext() == S_OK )
		return m_nQuantity;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
