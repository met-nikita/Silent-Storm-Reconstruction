// RectsDBCmd.H : Declaration of the CRectsDBCmd class

#ifndef __RECTSDBCMD_H_
#define __RECTSDBCMD_H_

#include <string>
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectsDBCmdAccessor
{
public:
	LONG m_RectID;
	LONG m_VariantID;
	LONG m_TemplateLink;
  float m_Rotation;
  LONG m_Floor;
	float m_CenterX;
	float m_CenterY;
	float m_Width;
	float m_Height;
	float m_fDZ;

BEGIN_ACCESSOR_MAP( CRectsDBCmdAccessor, 3 )
  BEGIN_ACCESSOR( 0, true )
  	COLUMN_ENTRY(1, m_RectID)
	  COLUMN_ENTRY(2, m_VariantID)
	  COLUMN_ENTRY(3, m_TemplateLink)
    COLUMN_ENTRY(4, m_Rotation)
    COLUMN_ENTRY(5, m_Floor)
		COLUMN_ENTRY(6, m_CenterX)
		COLUMN_ENTRY(7, m_CenterY)
		COLUMN_ENTRY(8, m_Width)
		COLUMN_ENTRY(9, m_Height)
		COLUMN_ENTRY(10, m_fDZ)
	END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
	  COLUMN_ENTRY(2, m_VariantID)
	  COLUMN_ENTRY(3, m_TemplateLink)
    COLUMN_ENTRY(4, m_Rotation)
    COLUMN_ENTRY(5, m_Floor)
		COLUMN_ENTRY(6, m_CenterX)
		COLUMN_ENTRY(7, m_CenterY)
		COLUMN_ENTRY(8, m_Width)
		COLUMN_ENTRY(9, m_Height)
		COLUMN_ENTRY(10, m_fDZ)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 2, false )
    COLUMN_ENTRY(4, m_Rotation)
    COLUMN_ENTRY(5, m_Floor)
		COLUMN_ENTRY(6, m_CenterX)
		COLUMN_ENTRY(7, m_CenterY)
		COLUMN_ENTRY(10, m_fDZ)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectsDBCmd : public CBaseDBCmd<CAccessor<CRectsDBCmdAccessor> >
{
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectPosDB : public CBaseDBCmd<CAccessor<CRectsDBCmdAccessor> >
{
public:
	bool SetPos( int nID, const CVec2 &ptPos, float fDZ, int nFloor, float fRotation );
	int  Insert( int nVariantID, int nTemplateID );
	bool Delete( int nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RECTSDBCMD_H_
