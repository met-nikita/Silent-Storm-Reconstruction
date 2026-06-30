#ifndef __ROOMSDB_H_
#define __ROOMSDB_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoomDBAccessor
{
public:
	LONG  m_nID;
	LONG  m_nVariantID;
	LONG  m_nFloor;
	LONG  m_nRoomID;
	LONG  m_nUserColor;
	
BEGIN_ACCESSOR_MAP( CRoomDBAccessor, 2 )
	BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_nVariantID)
		COLUMN_ENTRY(3, m_nFloor)
		COLUMN_ENTRY(4, m_nRoomID)
		COLUMN_ENTRY(5, m_nUserColor)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_nVariantID)
		COLUMN_ENTRY(3, m_nFloor)
		COLUMN_ENTRY(4, m_nRoomID)
		COLUMN_ENTRY(5, m_nUserColor)
	END_ACCESSOR()
END_ACCESSOR_MAP()
	
	// You may wish to call this function if you are inserting a record and wish to
	// initialize all the fields, if you are not going to explicitly set all of them.
	void ClearRecord()
	{
		memset(this, 0, sizeof(*this));
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoomDB : public CBaseDBCmd<CAccessor<CRoomDBAccessor> >
{
public:
	HRESULT OpenTable( const char *szTableName )
	{
    string szQuery = " SELECT *	FROM ";
    szQuery += szTableName;
    
		return Open( szQuery );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ROOMSDB_H_