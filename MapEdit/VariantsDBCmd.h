// TemplTypesDBCmd.H : Declaration of the CTemplTypesDBCmd class

#ifndef __VARIANTSDBCMD_H_
#define __VARIANTSDBCMD_H_

#include "dbDefs.h"

class CVariantsDBCmdAccessor
{
public:
  LONG  m_ID;
	LONG  m_TemplID;
	LONG  m_bGrid;
	FLOAT m_fRndWeight;
	LONG  m_nDefLight;
	LONG  m_nScriptID;

BEGIN_ACCESSOR_MAP( CVariantsDBCmdAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_ID)
  	COLUMN_ENTRY(2, m_TemplID)
		COLUMN_ENTRY(3, m_bGrid)
		COLUMN_ENTRY(4, m_fRndWeight)
		COLUMN_ENTRY(6, m_nDefLight)
		COLUMN_ENTRY(8, m_nScriptID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
	  COLUMN_ENTRY(2, m_TemplID)
		COLUMN_ENTRY(3, m_bGrid)
		COLUMN_ENTRY(4, m_fRndWeight)
		COLUMN_ENTRY(6, m_nDefLight)
		COLUMN_ENTRY(8, m_nScriptID)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};

class CVariantsDBCmd : public CBaseDBCmd<CAccessor<CVariantsDBCmdAccessor> >
{
public:
};

#endif // __VARIANTSDBCMD_H_