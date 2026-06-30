// TemplDBCmd.H : Declaration of the CTemplDBCmd class

#ifndef __TEMPLDBCMD_H_
#define __TEMPLDBCMD_H_

#include "dbDefs.h"

class CTemplDBCmdAccessor
{
public:
	LONG m_TemplateId;
	TCHAR m_Name[50];
	LONG m_Type;
	LONG m_Width;
	LONG m_Height;

BEGIN_ACCESSOR_MAP( CTemplDBCmdAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
  	COLUMN_ENTRY(1, m_TemplateId)
	  COLUMN_ENTRY(2, m_Name)
	  COLUMN_ENTRY(3, m_Type)
  	COLUMN_ENTRY(4, m_Width)
	  COLUMN_ENTRY(5, m_Height)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
	  COLUMN_ENTRY(2, m_Name)
	  COLUMN_ENTRY(3, m_Type)
  	COLUMN_ENTRY(4, m_Width)
	  COLUMN_ENTRY(5, m_Height)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};

class CTemplDBCmd : public CBaseDBCmd<CAccessor<CTemplDBCmdAccessor> >
{
public:
};

int AddNewTemplate( int nHintWidth, int nHintHeight ); // возвр. id добавленного варианта
void SetActiveTemplateVariant( int nVarID );
#endif // __TEMPLDBCMD_H_
