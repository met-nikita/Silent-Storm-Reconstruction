#ifndef __SCENARIODB_H_
#define __SCENARIODB_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szDescr[255];

	BEGIN_COLUMN_MAP(CStateAccessor)
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szDescr)
	END_COLUMN_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CClueAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szType[255];
	LONG  m_nItemID;
	LONG  m_nPersID;
	LONG  m_nState;
	BOOL  m_bPermanent;
	LONG  m_nMinParent2Open;
	LONG  m_nZone2Place1;
	LONG  m_nZone2Place2;
	LONG  m_nZone2Place3;
	LONG  m_nObjective1;
	LONG  m_nObjective2;
	LONG  m_nScenario;
	LONG  m_nDescription;
	TCHAR m_szSmallDescription[255];
	BEGIN_ACCESSOR_MAP( CClueAccessor, 2 )
	BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szType)
		COLUMN_ENTRY(3,	m_nItemID)
		COLUMN_ENTRY(4,	m_nPersID)
		COLUMN_ENTRY(5,	m_nState)
		COLUMN_ENTRY(6,	m_bPermanent)
		COLUMN_ENTRY(7,	m_nMinParent2Open)
		COLUMN_ENTRY(8,	m_nZone2Place1)
		COLUMN_ENTRY(9,	m_nZone2Place2)
		COLUMN_ENTRY(10, m_nZone2Place3)
		COLUMN_ENTRY(11, m_nObjective1)
		COLUMN_ENTRY(12, m_nObjective2)
		COLUMN_ENTRY(13, m_nScenario)
		COLUMN_ENTRY(14, m_nDescription)
		COLUMN_ENTRY(15, m_szSmallDescription)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szType)
		COLUMN_ENTRY(3,	m_nItemID)
		COLUMN_ENTRY(4,	m_nPersID)
		COLUMN_ENTRY(5,	m_nState)
		COLUMN_ENTRY(6,	m_bPermanent)
		COLUMN_ENTRY(7,	m_nMinParent2Open)
		COLUMN_ENTRY(8,	m_nZone2Place1)
		COLUMN_ENTRY(9,	m_nZone2Place2)
		COLUMN_ENTRY(10, m_nZone2Place3)
		COLUMN_ENTRY(11, m_nObjective1)
		COLUMN_ENTRY(12, m_nObjective2)
		COLUMN_ENTRY(13, m_nScenario)
		COLUMN_ENTRY(14, m_nDescription)
		COLUMN_ENTRY(15, m_szSmallDescription)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectiveAccessor
{
public:
	LONG	m_nID;
	TCHAR	m_szName[255];
	LONG	m_nZone2Open1;
	LONG	m_nZone2Open2;
	LONG	m_nZone2Open3;
	LONG  m_nDescr;
	LONG	m_nScenario;
	LONG	m_nZone2Block1;
	LONG	m_nZone2Block2;
	LONG	m_nZone2Block3;

	BEGIN_ACCESSOR_MAP( CObjectiveAccessor, 2 )
	BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szName)
		COLUMN_ENTRY(3,	m_nZone2Open1)
		COLUMN_ENTRY(4,	m_nZone2Open2)
		COLUMN_ENTRY(5,	m_nZone2Open3)
		COLUMN_ENTRY(6,	m_nDescr)
		COLUMN_ENTRY(7,	m_nScenario)
		COLUMN_ENTRY(8,	m_nZone2Block1)
		COLUMN_ENTRY(9,	m_nZone2Block2)
		COLUMN_ENTRY(10,m_nZone2Block3)
	END_ACCESSOR()
	BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szName)
		COLUMN_ENTRY(3,	m_nZone2Open1)
		COLUMN_ENTRY(4,	m_nZone2Open2)
		COLUMN_ENTRY(5,	m_nZone2Open3)
		COLUMN_ENTRY(6,	m_nDescr)
		COLUMN_ENTRY(7,	m_nScenario)
		COLUMN_ENTRY(8,	m_nZone2Block1)
		COLUMN_ENTRY(9,	m_nZone2Block2)
		COLUMN_ENTRY(10,m_nZone2Block3)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjective2ClueAccessor
{
public:
	LONG	m_nObjectiveID;
	LONG	m_nClueID;

	BEGIN_ACCESSOR_MAP( CObjective2ClueAccessor, 1 )
		BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_nObjectiveID)
		COLUMN_ENTRY(2, m_nClueID)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioStateDB : public CBaseDBCmd<CAccessor<CStateAccessor> >
{
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioClueDB : public CBaseDBCmd<CAccessor<CClueAccessor> >
{
public:
	bool Open( int nID );
	bool OpenCode( int nScenarioID, const string &szCode );
	bool OpenScenario( int nScenarioID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioObjectiveDB : public CBaseDBCmd<CAccessor<CObjectiveAccessor> >
{
public:
	bool Open( int nID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioObjective2ClueDB : public CBaseDBCmd<CAccessor<CObjective2ClueAccessor> >
{
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetScenarioZoneID( const string &szZone, int nScenarioID );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCENARIODB_H_
