#ifndef __MODELSDBCMD_H_
#define __MODELSDBCMD_H_

#include "dbDefs.h"

class CModelsDBCmdAccessor
{
public:
  LONG  m_ID;
  TCHAR m_Name[200];

BEGIN_ACCESSOR_MAP( CModelsDBCmdAccessor, 1 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_ID)
    COLUMN_ENTRY(5, m_Name)
  END_ACCESSOR()
  /*
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
	  COLUMN_ENTRY(2, m_SrcName)
  END_ACCESSOR()
  */
END_ACCESSOR_MAP()

DEFINE_COMMAND(CModelsDBCmdAccessor, _T(" SELECT * FROM Models"))

	// You may wish to call this function if you are inserting a record and wish to
	// initialize all the fields, if you are not going to explicitly set all of them.
	void ClearRecord()
	{
		memset(this, 0, sizeof(*this));
	}
};

class CModelsDBCmd : public CCommand<CAccessor<CModelsDBCmdAccessor> >
{
public:
  HRESULT Open( const std::string szQuery );

	CSession	m_session;
};

#endif // __MODELSDBCMD_H_