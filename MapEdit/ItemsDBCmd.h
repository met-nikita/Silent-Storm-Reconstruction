#ifndef __ITEMSDBCMD_H__
#define __ITEMSDBCMD_H__

#include "dbDefs.h"
#include "ItemsMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIDAccessor
{
public:
	LONG  m_ID;
	TCHAR m_szSomeColumn[MAX_DBSTRING];
	
BEGIN_ACCESSOR_MAP( CIDAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_ID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
		COLUMN_ENTRY(2, m_szSomeColumn)
  END_ACCESSOR()
END_ACCESSOR_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIDDBCmd : public CBaseDBCmd<CAccessor<CIDAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemsDBCmd : public CBaseDBCmd<CDynamicAccessor >
{
	CIDDBCmd dbID;
  string MakeItemQuery( const string &szTable, const CPropMap *propMap, int nItem = -1 );
	string MakeVariantQuery( const string &szTable, const CPropMap *propMap, int nItem = -1 );
  
  int InsertCopyMax( const string &szTable, const CPropMap *pDefValues );
	int InsertMax( const string &szTable, const string &szSomeColumn, const string &szSomeValue );
  int GetMaxID( const string &szTable );  
  
public:
	void SetConnection( SDBConnection *pConnection );
	HRESULT OpenQuery( const std::string &szQuery, bool bReadOnly );
  HRESULT Open( const string &szTable, bool bReadOnly, const CPropMap *propMap );
  HRESULT OpenItem( const string &szTable, int nItemID, bool bReadOnly, const CPropMap *propMap );
	HRESULT OpenVariant( const string &szTable, int nVariantID, bool bReadOnly, const CPropMap *propMap );
	HRESULT FindOpen( const string &szTable, const string &szProp, CVariant val );
	HRESULT FindOpenVariant( const string &szTable, const string &szProp, CVariant val );

  bool  GetItem( int *pItemID, int *pFolderID, char *szName, DWORD *pdwColor );
	bool  GetVariant( int *pItemID, int *pTemplateID );
  bool  ReadProps( const CPropMap *props );
  int   Insert( const string &szTable, int nFolderID, const char *szName, const CPropMap *pDefValues );
	int   InsertVariant( const string &szTable, int nTemplateID, const CPropMap *pDefValues );
  bool  DeleteItem();
  bool  UpdateItem( int nFolderID, const char *szName, const CPropMap *propMap = 0, bool bDefValues = false );
	bool  UpdateVariant( int nTemplateID, const CPropMap *propMap, bool bDefValues = false );

	bool  SetEmpty( const string &szTable, int nItemID, const string &szColName );
	bool  SetItemColor( const string &szTable, int nItemID, DWORD dwColor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ITEMSDBCMD_H__