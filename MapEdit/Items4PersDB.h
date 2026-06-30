#ifndef __ITEMS4PERS_H_
#define __ITEMS4PERS_H_

#include "dbDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
class CItems4PersAccessor
{
public:
	LONG m_ID;
	LONG m_nPersID;
	LONG m_nItemID;
	LONG m_nQuantity;
	
BEGIN_ACCESSOR_MAP( CItems4PersAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_ID)
		COLUMN_ENTRY(2, m_nPersID)
		COLUMN_ENTRY(3, m_nItemID)
		COLUMN_ENTRY(4, m_nQuantity)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
		COLUMN_ENTRY(2, m_nPersID)
		COLUMN_ENTRY(3, m_nItemID)
		COLUMN_ENTRY(4, m_nQuantity)
  END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SItem
{
	int nItemID;
	int nQuantity;
	SItem( int _nItemID, int _nQuantity ): nItemID(_nItemID), nQuantity(_nQuantity) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItems4PersDB : public CBaseDBCmd<CAccessor<CItems4PersAccessor> >
{
	bool OpenQuery( const string &szQuery );
public:
	bool GetItems( vector<SItem> *pItems, const string &szTable, int nPersID );
	int  GetItemQuantity( const string &szTable, int nPersID, int nItemID ); // возвр -1 если ошибка
	bool SetItemQuantity( const string &szTable, int nPersID, int nItemID, int nQuantity );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ITEMS4PERS_H_
