#include "StdAfx.h"
#include "UnitDB.h"

extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPosDB::Open( int nID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT * FROM Units WHERE ID=";
	szQuery += IToA( nID );
	if ( FAILED( CBaseDBCmd<CAccessor<CUnitAccessor> >::Open( szQuery ) ) || MoveNext() != S_OK )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPosDB::SetPos( int nID, int x, int y, int nFloor, float fRotation )
{
	if ( !Open( nID ) )
		return false;
	m_nPosX = x;
	m_nPosY = y;
	m_nFloor = nFloor;
	m_fRotation = fRotation;
	HRESULT hr = SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPosDB::Delete( int nID )
{
	if ( !Open( nID ) )
		return false;
	HRESULT hr = CBaseDBCmd<CAccessor<CUnitAccessor> >::Delete();
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitPosDB::Insert( int nVariantID, int nModelID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT * FROM Units WHERE ID=-1";
	if ( FAILED( CBaseDBCmd<CAccessor<CUnitAccessor> >::Open( szQuery ) ) )
		return -1;
	m_nVarID = nVariantID;
	m_nPersID = nModelID;
	HRESULT hr = CBaseDBCmd<CAccessor<CUnitAccessor> >::Insert( 2 );
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
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPosDB::SetPers( int nID, int nPersID )
{
	if ( !Open( nID ) )
		return false;
	m_nPersID = nPersID;
	HRESULT hr = SetData( 3 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
