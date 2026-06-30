#ifndef __FLOORDB_H__
#define __FLOORDB_H__

#include "dbDefs.h"

class CFloorDBAccessor
{
public:
	LONG  m_VariantID;
	LONG	m_Floor;
	LONG	m_ModelID;
	CHAR  m_Coords[MAX_DBSTRING + 1];
	LONG  m_nLayer;
	
	BEGIN_COLUMN_MAP( CFloorDBAccessor )
		COLUMN_ENTRY(2, m_VariantID)
		COLUMN_ENTRY(3, m_Floor)
		COLUMN_ENTRY(4, m_ModelID)
		COLUMN_ENTRY(5, m_Coords)
		COLUMN_ENTRY(6, m_nLayer)
	END_COLUMN_MAP()
		
	// You may wish to call this function if you are inserting a record and wish to
	// initialize all the fields, if you are not going to explicitly set all of them.
	void ClearRecord()
	{
		memset(this, 0, sizeof(*this));
	}
};

class CFloorDB : public CBaseDBCmd<CAccessor<CFloorDBAccessor> >
{
public:
	HRESULT OpenTable( const char *szTableName )
	{
    string szQuery = " SELECT *	FROM ";
    szQuery += szTableName;
    
		return Open( szQuery );
	}
};

#endif // __FLOORDB_H__