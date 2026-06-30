// TemplTypesDBCmd.H : Declaration of the CTemplTypesDBCmd class

#ifndef __TEMPLTYPESDBCMD_H_
#define __TEMPLTYPESDBCMD_H_

#include "dbDefs.h"

class CTreeDBCmdAccessor
{
public:
	TCHAR m_FolderName[50];
  LONG  m_FolderID;
	LONG  m_ParentID;
	DWORD m_UserColor;

BEGIN_ACCESSOR_MAP( CTreeDBCmdAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_FolderID)
  	COLUMN_ENTRY(2, m_FolderName)
    COLUMN_ENTRY(3, m_ParentID)
		COLUMN_ENTRY(4, m_UserColor)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
	  COLUMN_ENTRY(2, m_FolderName)
    COLUMN_ENTRY(3, m_ParentID)
		COLUMN_ENTRY(4, m_UserColor)
	END_ACCESSOR()
END_ACCESSOR_MAP()

	// You may wish to call this function if you are inserting a record and wish to
	// initialize all the fields, if you are not going to explicitly set all of them.
	void ClearRecord()
	{
		memset(this, 0, sizeof(*this));
	}
};

class CTreeDBCmd : public CBaseDBCmd<CAccessor<CTreeDBCmdAccessor> >
{
public:
	HRESULT OpenTable( const char *szTableName )
	{
    string szQuery = " SELECT *	FROM ";
    szQuery += szTableName;
    
		return Open( szQuery );
	}
};

#endif // __TEMPLTYPESDBCMD_H_
