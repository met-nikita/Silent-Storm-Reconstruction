#include "StdAfx.h"
#include "StoreItemsDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreItemsDB::OpenSideItems( int nSideID )
{
	string szQuery = "SELECT * FROM RPGStoreItems WHERE SideID = ";
	szQuery += IToA( nSideID );

	HRESULT hr = CBaseDBCmd<CAccessor<CStoreAccessor> >::Open( szQuery );
	return SUCCEEDED( hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStoreItemsDB::InsertItem( int nItemID, int nSideID, int nQuantity )
{
	if ( FAILED( CBaseDBCmd<CAccessor<CStoreAccessor> >::Open( "SELECT * FROM RPGStoreItems WHERE ID = -1" ) ) )
		return -1;
	m_nItemID = nItemID;
	m_nSideID = nSideID;
	m_nRating = 0;
	m_nQuantity = nQuantity;
	HRESULT hr = Insert( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return -1;
	}
	if ( MoveNext() == S_OK )
		return m_nID;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreItemsDB::OpenItem( int nID )
{
	string szQuery = "SELECT * FROM RPGStoreItems WHERE ID = ";
	szQuery += IToA( nID );

	HRESULT hr = CBaseDBCmd<CAccessor<CStoreAccessor> >::Open( szQuery );
	if ( FAILED( hr ) )
		return false;
	return MoveNext() == S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreItemsDB::SetItemQuantity( int nRecordID, int nQuantity )
{
	if ( !OpenItem( nRecordID ) )
		return false;
	m_nQuantity = nQuantity;
	HRESULT hr = SetData( 3 );
	if ( SUCCEEDED( hr ) )
		return true;
	DisplayOLEDBErrorRecords( hr );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreItemsDB::SetItemRating( int nRecordID, int nRating )
{
	if ( !OpenItem( nRecordID ) )
		return false;
	m_nRating = nRating;
	HRESULT hr = SetData( 2 );
	if ( SUCCEEDED( hr ) )
		return true;
	DisplayOLEDBErrorRecords( hr );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreItemsDB::DeleteItem( int nRecordID )
{
	if ( !OpenItem( nRecordID ) )
		return false;
	HRESULT hr = Delete();
	return SUCCEEDED( hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStoreItemsDB::GetItemRating( int nRecordID )
{
	if ( !OpenItem( nRecordID ) )
		return false;
	return m_nRating;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
