#ifndef __SCENARIOXLS_H_
#define __SCENARIOXLS_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelCluesAccessor
{
public:
	TCHAR m_szClueCode[255];
	TCHAR m_szClueDescription[10240];
	TCHAR m_szObjectiveAction[255];
	TCHAR m_szObjectiveDescription[10240];

	BEGIN_ACCESSOR_MAP( CExcelCluesAccessor, 1 )
	BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_szClueCode)
		COLUMN_ENTRY(2, m_szClueDescription)
		COLUMN_ENTRY(3, m_szObjectiveAction)
		COLUMN_ENTRY(4, m_szObjectiveDescription)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelScenarioDB: public CBaseDBCmd<CAccessor<CExcelCluesAccessor> >
{
public:
	void Insert( const string &szCode, const string &szDescr, const string &szAction, const string &szObjDescription );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetString( int nID  );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCENARIOXLS_H_