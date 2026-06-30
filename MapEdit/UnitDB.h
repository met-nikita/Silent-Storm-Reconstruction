#ifndef __UNUTDB_H_
#define __UNUTDB_H_

#include "dbDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitAccessor
{
public:
	LONG  m_nID;
	LONG  m_nVarID;
	LONG  m_nPersID;
	LONG  m_nPosX;
	LONG  m_nPosY;
	LONG  m_nFloor;
	float m_fRotation;
	BOOL  m_bClueSlot;
	BOOL  m_bClueInventorySlot;

	BEGIN_ACCESSOR_MAP( CUnitAccessor, 5 )
		BEGIN_ACCESSOR( 0, true )
			COLUMN_ENTRY(1, m_nID)
		END_ACCESSOR()
		BEGIN_ACCESSOR( 1, false )
			COLUMN_ENTRY(3, m_fRotation)
			COLUMN_ENTRY(5, m_nPosX)
			COLUMN_ENTRY(6, m_nPosY)
			COLUMN_ENTRY(7, m_nFloor)
		END_ACCESSOR()
		BEGIN_ACCESSOR( 2, false )
			COLUMN_ENTRY(2, m_nVarID)
			COLUMN_ENTRY(4, m_nPersID)
		END_ACCESSOR()
		BEGIN_ACCESSOR( 3, false )
			COLUMN_ENTRY(4, m_nPersID)
		END_ACCESSOR()
		BEGIN_ACCESSOR( 4, false )
			COLUMN_ENTRY(8, m_bClueSlot)
			COLUMN_ENTRY(9, m_bClueInventorySlot)
		END_ACCESSOR()
	END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPosDB : public CBaseDBCmd<CAccessor<CUnitAccessor> >
{
	bool Open( int nID );
public:
	bool SetPos( int nID, int x, int y, int nFloor, float fRotation );
	bool SetPers( int nID, int nPersID );
	bool SetSlots( int nID, bool bClueSlot, bool bInventorySlot );
	int  Insert( int nVariantID, int nPersID );
	bool Delete( int nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UNUTDB_H_