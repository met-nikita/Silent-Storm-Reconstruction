#include "StdAfx.h"
#include "Attributes.h"

extern SDBConnection dbConnection; // находится в TemplMgr.h
static CAttrDB dbAttr;

////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttributesList::ReadListFromDB()
{
	dbAttr.SetConnection( &dbConnection );
	attrs.clear();
	
	if ( FAILED( dbAttr.Open( string( "SELECT * FROM " ) + ATTRIBUTES_TBL ) ) )
		return false;
	
	while ( S_OK == dbAttr.MoveNext() )
		attrs[dbAttr.m_attrName] = dbAttr.m_attrID;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAttributesList::GetList( CAttrMap *pList )
{
	*pList = attrs;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttributesList::AddAttribute( const string &szAttr )
{
	if ( !ReadListFromDB() )
		return false;
	//
	CAttrMap::iterator it = attrs.find( szAttr );		
	if ( attrs.end() != it ) // такой уже есть
		return false;
	if ( FAILED(dbAttr.Open( string( "SELECT * FROM " ) + ATTRIBUTES_TBL + " WHERE ID=-1" )) )
		return false;
	HRESULT hr = dbAttr.Insert( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	ReadListFromDB();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttributesList::DelAttribute( const string &szAttr )
{
	if ( !ReadListFromDB() )
		return false;
	//
	CAttrMap::iterator it = attrs.find( szAttr );
	if ( attrs.end() == it )
		return false;
	if ( FAILED(dbAttr.Open( string( "SELECT * FROM " ) + ATTRIBUTES_TBL + " WHERE ID=" + IToA( it->second ) )) )
		return false;
	if ( S_OK != dbAttr.MoveNext() )
		return false;
	dbAttr.Delete();
	attrs.erase( it );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAttributesList::operator== ( const CAttributesList &op ) const
{
	if ( attrs.size() != op.attrs.size() ) 
		return false;
	for ( CAttrMap::const_iterator i = attrs.begin(); i != attrs.end(); ++i )
	{
		CAttrMap::const_iterator io = op.attrs.find( i->first );
		if ( io == op.attrs.end() || io->second != i->second )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
