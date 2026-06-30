#ifndef __TERRSPOTDB_H_
#define __TERRSPOTDB_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrSPotDBAccessor
{
public:
	LONG  m_nID;
	LONG  m_nVariantID;
	LONG  m_nMaterialID;
	CVec2 m_ptPos;
	CVec2 m_ptSize;
	LONG  m_nRotation;
	LONG  m_nPriority;
	LONG  m_nLayer;
	
BEGIN_ACCESSOR_MAP( CTerrSPotDBAccessor, 2 )
	BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_nVariantID)
		COLUMN_ENTRY(3, m_nMaterialID)
		COLUMN_ENTRY(4, m_ptSize.x)
		COLUMN_ENTRY(5, m_ptSize.y)
		COLUMN_ENTRY(6, m_ptPos.x)
		COLUMN_ENTRY(7, m_ptPos.y)
		COLUMN_ENTRY(8, m_nRotation)
		COLUMN_ENTRY(9, m_nPriority)
		COLUMN_ENTRY(10, m_nLayer)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_nVariantID)
		COLUMN_ENTRY(3, m_nMaterialID)
		COLUMN_ENTRY(4, m_ptSize.x)
		COLUMN_ENTRY(5, m_ptSize.y)
		COLUMN_ENTRY(6, m_ptPos.x)
		COLUMN_ENTRY(7, m_ptPos.y)
		COLUMN_ENTRY(8, m_nRotation)
		COLUMN_ENTRY(9, m_nPriority)
		COLUMN_ENTRY(10, m_nLayer)
	END_ACCESSOR()
END_ACCESSOR_MAP()
	
	// You may wish to call this function if you are inserting a record and wish to
	// initialize all the fields, if you are not going to explicitly set all of them.
	void ClearRecord()
	{
		memset(this, 0, sizeof(*this));
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrSpotDB : public CBaseDBCmd<CAccessor<CTerrSPotDBAccessor> >
{
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __TERRSPOTDB_H_