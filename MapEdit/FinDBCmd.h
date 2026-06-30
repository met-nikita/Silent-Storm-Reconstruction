#ifndef __FINDBCMD_H_
#define __FINDBCMD_H_

#include "dbDefs.h"
#include "FinalElem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
class CFIDAccessor
{
public:
	LONG m_ID;
	LONG m_nSomeColumn;
	
BEGIN_ACCESSOR_MAP( CFIDAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_ID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
		COLUMN_ENTRY(2, m_nSomeColumn)
  END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFIDDBCmd : public CBaseDBCmd<CAccessor<CFIDAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinPosAccessor
{
public:
	LONG  m_nID;
	LONG  m_nVarID;
	LONG  m_nModelID;
	float m_fPosX;
	float m_fPosY;
	float m_fDZ;
	LONG  m_nFloor;
	float m_fRotation;
	BOOL  m_bOpen;
	float m_fScaleX;
	float m_fScaleY;
	float m_fScaleZ;
	BOOL  m_bLightmap;

	LONG  m_nLightCr;
	float m_fLightX;
	float m_fLightY;
	float m_fLightZ;
	float m_fLightRadius;
	float m_fFlareRadius;
	LONG  m_nFlareTexture;
	float m_fFlareX;
	float m_fFlareY;
	float m_fFlareZ;

BEGIN_ACCESSOR_MAP( CFinPosAccessor, 8 )
  BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(4, m_fRotation)
		COLUMN_ENTRY(5, m_fPosX)
		COLUMN_ENTRY(6, m_fPosY)
		COLUMN_ENTRY(7, m_nFloor)
		COLUMN_ENTRY(8, m_fDZ)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 2, false )
		COLUMN_ENTRY(2, m_nModelID)
    COLUMN_ENTRY(3, m_nVarID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 3, false )
		COLUMN_ENTRY(2, m_nModelID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 4, false )
		COLUMN_ENTRY(14, m_bOpen)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 5, false )
		COLUMN_ENTRY(9, m_fScaleX)
		COLUMN_ENTRY(10, m_fScaleY)
		COLUMN_ENTRY(11, m_fScaleZ)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 6, false )
		COLUMN_ENTRY(15, m_bLightmap)
  END_ACCESSOR()
	BEGIN_ACCESSOR( 7, false )
		COLUMN_ENTRY(22, m_nLightCr)
		COLUMN_ENTRY(23, m_fLightX)
		COLUMN_ENTRY(24, m_fLightY)
		COLUMN_ENTRY(25, m_fLightZ)
		COLUMN_ENTRY(26, m_fLightRadius)
		COLUMN_ENTRY(27, m_fFlareRadius)
		COLUMN_ENTRY(28, m_nFlareTexture)
		COLUMN_ENTRY(30, m_fFlareX)
		COLUMN_ENTRY(31, m_fFlareY)
		COLUMN_ENTRY(32, m_fFlareZ)
	END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinPosDB : public CBaseDBCmd<CAccessor<CFinPosAccessor> >
{
	bool Open( int nID );
public:
	bool SetPos( int nID, CVec2 &ptPos, float fDZ, int nFloor, float fRotation );
	bool SetOpen( int nID, bool bOpen );
	bool SetModel( int nID, int nModelID );
	bool SetLightmap( int nID, bool bLightmap );
	bool SetScale( int nID, float fX, float fY, float fZ );
	bool SetLight( int nID, int nColor, const CVec3 &ptPos, float fRadius, float fFlareRadius, int nFlareTexture, const CVec3 &ptFlarePos );
	int  Insert( int nVariantID, int nModelID );
	bool Delete( int nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinDBCmd : public CBaseDBCmd<CDynamicAccessor >
{
	CFIDDBCmd dbID;
  string MakePropQuery( const string &szTable, const CPropMap *propMap, int nElem = -1, bool bVarColumn = false );
  int InsertCopyMax( const string &szTable, int nVarID, const CPropMap *pDefValues );
  int GetMaxID( const string &szTable );
	int InsertMax( const string &szTable, const string &szSomeColumn, int nSomeValue );
  
public:
	void SetConnection( SDBConnection *pConnection );
	HRESULT Open( const string &szQuery );
  HRESULT Open( const string &szTable, const CPropMap *propMap );
  HRESULT Open( const string &szTable, int nID, const CPropMap *propMap = 0 );
  
  int   Insert( const string &szTable, int nVarID, const CPropMap *pDefValues );
  bool  GetElement( int *pElemID, int *pVarID );
  bool  ReadProps( const CPropMap *props );
  bool  DeleteElement();
  bool  UpdateElement( const CPropMap *propMap );
};

#endif // __FINDBCMD_H_