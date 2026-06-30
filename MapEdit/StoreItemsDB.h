#ifndef __STOREITEMSDB_H_
#define __STOREITEMSDB_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStoreAccessor
{
public:
	LONG  m_nID;
	LONG  m_nRating;
	LONG  m_nQuantity;
	LONG	m_nItemID;
	LONG	m_nSideID;

	BEGIN_ACCESSOR_MAP( CStoreAccessor, 4 )
		BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(4, m_nRating)
		COLUMN_ENTRY(5, m_nQuantity)
		COLUMN_ENTRY(6, m_nItemID)
		COLUMN_ENTRY(7, m_nSideID)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(4, m_nRating)
		COLUMN_ENTRY(5, m_nQuantity)
		COLUMN_ENTRY(6, m_nItemID)
		COLUMN_ENTRY(7, m_nSideID)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 2, false )
		COLUMN_ENTRY(4, m_nRating)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 3, false )
		COLUMN_ENTRY(5, m_nQuantity)
	END_ACCESSOR()
	END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStoreItemsDB : public CBaseDBCmd<CAccessor<CStoreAccessor> >
{
	bool OpenItem( int nID );
public:
	bool OpenSideItems( int nSideID );
	int InsertItem( int nItemID, int nSideID, int nQuantity ); // возвр. RecordID
	bool SetItemQuantity( int nRecordID, int nQuantity );
	bool SetItemRating( int nRecordID, int nRating );
	bool DeleteItem( int nRecordID );
	int GetItemRating( int nRecordID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STOREITEMSDB_H_
