#ifndef __PLACABLEDB_H_
#define __PLACABLEDB_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacableIDAccessor
{
public:
	LONG  m_nID;
	LONG  m_nPlacableID;
BEGIN_ACCESSOR_MAP( CPlacableIDAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_nPlacableID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_nPlacableID)
  END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlaceDB : public CBaseDBCmd<CAccessor<CPlacableIDAccessor> >
{
public:
	// создает запись в PlacableObjects и присваевает ее Id'шник полю PlacabaleID в таблице szRelTable в записи nItemID
	void InsertPlacableObject( const string &szRelTable, const string &szItemName, int nItemID );
	int  GetPlaceID( const string &szObjectTable, int nItemID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __PLACABLEDB_H_