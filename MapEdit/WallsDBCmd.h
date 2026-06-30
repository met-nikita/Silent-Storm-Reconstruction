#ifndef __WALLSDBCMD_H__
#define __WALLSDBCMD_H__

#include "dbDefs.h"

class CWallsDBCmdAccessor
{
public:
  LONG  m_ID;
  LONG  m_VariantID;
  LONG  m_ModelID;
  LONG  m_StartX;
  LONG  m_StartY;
  LONG  m_EndX;
  LONG  m_EndY;
  LONG  m_Floor;
  
  BEGIN_ACCESSOR_MAP( CWallsDBCmdAccessor, 2 )
    BEGIN_ACCESSOR( 0, true )
      COLUMN_ENTRY(1, m_ID)
      COLUMN_ENTRY(2, m_VariantID)
      COLUMN_ENTRY(3, m_ModelID)
      COLUMN_ENTRY(4, m_StartX)
      COLUMN_ENTRY(5, m_StartY)
      COLUMN_ENTRY(6, m_EndX)
      COLUMN_ENTRY(7, m_EndY)
      COLUMN_ENTRY(8, m_Floor)
    END_ACCESSOR()
    BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
      COLUMN_ENTRY(2, m_VariantID)
      COLUMN_ENTRY(3, m_ModelID)
      COLUMN_ENTRY(4, m_StartX)
      COLUMN_ENTRY(5, m_StartY)
      COLUMN_ENTRY(6, m_EndX)
      COLUMN_ENTRY(7, m_EndY)
      COLUMN_ENTRY(8, m_Floor)
    END_ACCESSOR()
  END_ACCESSOR_MAP()
};

class CWallsDBCmd : public CBaseDBCmd<CAccessor<CWallsDBCmdAccessor> >
{
public:
};

#endif // __WALLSDBCMD_H__