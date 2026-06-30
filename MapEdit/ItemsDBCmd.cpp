#include "StdAfx.h"
#include "ItemsDBCmd.h"
#include "ItemsMgr.h"
#include "DbInl.h"

extern HRESULT InitDB( CSession &session, CDBPropSet &propset );
extern string IToA( int n );

////////////////////////////////////////////////////////////////////////////////////////////////////
inline string MakePropQuery( string *pRes, const string &szTable, const CPropMap *propMap, int nItem )
{
  string &str = *pRes;
	
  if ( propMap )
  {
    CPropMap::const_iterator it = propMap->begin();
    
    while ( it != propMap->end() )
    {
			CListProp *p = dynamic_cast<CListProp*>( it->second.GetPtr() );
			if ( !p )
				str += ", " + it->first;
      ++it;
    }
  }
  str += " FROM " + szTable;
	
  if ( -1 != nItem )
  {
    char buf[8];
    itoa( nItem, buf, 10 );
		
    str += string( " WHERE ID = " ) + buf;
  }
  return str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemsDBCmd::SetConnection( SDBConnection *pConnection )
{
	dbID.SetConnection( pConnection );
	CBaseDBCmd<CDynamicAccessor >::SetConnection( pConnection );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CItemsDBCmd::MakeItemQuery( const string &szTable, const CPropMap *propMap, int nItem )
{
  string str;
  if ( -1 == nItem )
    str = "SELECT ID, FolderID, UserName, MEUserColor";
  else
    str = "SELECT FolderID, UserName, MEUserColor";
	//
	MakePropQuery( &str, szTable, propMap, nItem );
  return str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CItemsDBCmd::MakeVariantQuery( const string &szTable, const CPropMap *propMap, int nItem )
{
  string str;
  if ( -1 == nItem )
    str = "SELECT ID, TemplateID";
  else
    str = "SELECT TemplateID";
	//
	MakePropQuery( &str, szTable, propMap, nItem );
  return str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ќткрываетс€ вс€ таблица
// propMap может быть = 0
HRESULT CItemsDBCmd::Open( const string &szTable, bool bReadOnly, const CPropMap *propMap )
{
//  GetComboOptions();
//  return S_OK;
  
  string szQuery = MakeItemQuery( szTable, propMap );

  return OpenQuery( szQuery, bReadOnly );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬ базе данных открываетс€ одна запись с ID = nItemID,
// и становитс€ активной ( автоматически вызываетс€ MoveNext )
// propMap может быть = 0
HRESULT CItemsDBCmd::OpenItem( const string &szTable, int nItemID, bool bReadOnly, const CPropMap *propMap )
{
  string szQuery = MakeItemQuery( szTable, propMap, nItemID );
  
	HRESULT hr = OpenQuery( szQuery, bReadOnly );
  if ( FAILED( hr ) )
    return hr;
  hr = MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CItemsDBCmd::FindOpen( const string &szTable, const string &szProp, CVariant val )
{
	string szQuery = MakeItemQuery( szTable, 0 );

	szQuery += string( " WHERE " );
	szQuery += szProp;
	szQuery += string( "='" );
	szQuery += (string)val;
	szQuery += "'";
	
	return OpenQuery( szQuery, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CItemsDBCmd::FindOpenVariant( const string &szTable, const string &szProp, CVariant val )
{
	string szQuery = MakeVariantQuery( szTable, 0 );
	
	szQuery += string( " WHERE " );
	szQuery += szProp;
	szQuery += string( "=" );
	szQuery += (string)val;
	
	return OpenQuery( szQuery, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CItemsDBCmd::OpenQuery( const std::string &szQuery, bool bReadOnly )
{
  Close(); // очищаем запрос
	//CCommand<CDynamicAccessor>::SetBlobHandling( DBBLOBHANDLING_NOSTREAMS );
  HRESULT hr = CCommand<CDynamicAccessor>::Open(pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }

  DBORDINAL nColumns;
  DBCOLUMNINFO*   pColumnInfo;
	OLECHAR *pStrings;
  // Get the column information from the opened rowset
  //GetColumnInfo(&nColumns, &pColumnInfo);
	CDynamicAccessor::GetColumnInfo( GetInterface(), &nColumns, &pColumnInfo, &pStrings );

  for ( int i = 0; i < nColumns; ++i )
  {
		bool bIsBLOB = pColumnInfo[i].dwFlags & DBCOLUMNFLAGS_ISLONG;
		if ( bIsBLOB && !bReadOnly )
			continue;
    if ( pColumnInfo[i].wType == DBTYPE_WSTR )
    {
      pColumnInfo[i].wType = DBTYPE_STR;
    }
    AddBindEntry( pColumnInfo[i] );
  }
  // We've finished specifying the bindings. Go ahead and bind
  Bind();
  CoTaskMemFree(pColumnInfo);  
  
  return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// »з текущей активной записи считываютс€ об€зательные пол€
bool CItemsDBCmd::GetItem( int *pItemID, int *pFolderID, char *szName, DWORD *pdwColor )
{
	ULONG len1, len2, len4;
	GetLength( 1, &len1 );
	GetLength( 2, &len2 );
	GetLength( 4, &len4 );
	if ( sizeof( int ) != len1 )
		return false; // неправильный формат полей
  bool ret = GetValue( (ULONG)1, pItemID );
	if ( sizeof( int ) != len2 )
	{
		DBSTATUS status;
		GetStatus( 2, &status );
		if ( DBSTATUS_S_ISNULL != status )
			return false; // неправильный формат полей
		pFolderID = 0;
	}
	else
		ret = GetValue( (ULONG)2, pFolderID ) && ret;
  const char *szStr = (char*)GetValue( 3 );
  if ( !szStr )
    return false;
  strcpy( szName, szStr );
	//
	if ( sizeof( DWORD ) != len4 )
	{
		DBSTATUS status;
		GetStatus( 4, &status );
		if ( DBSTATUS_S_ISNULL != status )
			return false; // неправильный формат полей
		*pdwColor = 0;
	}
	else
		ret = GetValue( (ULONG)4, pdwColor ) && ret;
  return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ”даление текущей записи из базы данных
bool CItemsDBCmd::DeleteItem()
{
  HRESULT hr = Delete();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }  
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ќбновление текущей записи в базе данных
// bDefValues определ€ет откуда берутс€ данные и как обрабатываеютс€ NULL значени€ в propMap
// true  - CProp::GetDefValue(); NULL значени€ не обновл€ютс€
// false - CProp::GetValue(); NULL значени€ записываютс€ в базу
bool CItemsDBCmd::UpdateItem( int nFolderID, const char *szName, const CPropMap *propMap, bool bDefValues )
{
  if ( !SetValue( 1, nFolderID ) )
    return false;
	SetStatus( 1, DBSTATUS_S_OK );

  char *pszValue = (char*)GetValue( 2 );
  strncpy( pszValue, szName, MAX_STRING_LEN );
	pszValue[MAX_STRING_LEN] = 0;
  int len = strlen( pszValue );
  SetLength( 2, len );
  SetStatus( 2, DBSTATUS_S_OK );
	//
  HRESULT hr = SetData();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }  
  //
  if ( !WritePropsToDB( this, propMap, bDefValues ) )
    return false;
	//
  hr = SetData();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsDBCmd::SetEmpty( const string &szTable, int nItemID, const string &szColName )
{
	Close();
	string szQuery = "SELECT " + szColName + " FROM " + szTable + " WHERE ID=" + IToA( nItemID);
	HRESULT hr = CCommand<CDynamicAccessor>::Open(pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT );
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	hr = MoveNext();
	if ( FAILED(hr) )
		return false;
	if ( !SetStatus( 1, DBSTATUS_S_ISNULL ) )
		return false;
	if ( FAILED( SetData() ) )
		return false;
	
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// »з текущей активной записи считываютс€ специфические св-ва
bool CItemsDBCmd::ReadProps( const CPropMap* props )
{
  return ReadPropsFromDB( this, props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsDBCmd::InsertMax( const string &szTable, const string &szSomeColumn, const string &szSomeValue )
{
	Close();
	HRESULT hr = dbID.Open( "SELECT ID, " + szSomeColumn + " FROM " + szTable + " WHERE ID=0" );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	StrCpy( dbID.m_szSomeColumn, szSomeValue.c_str() );
	hr = dbID.Insert( 1 );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	hr = dbID.MoveNext();
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }

	return dbID.m_ID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬ставка новой записи в таблицу
// ¬озвр ID новой записи или -1 при ошибке
int CItemsDBCmd::Insert( const string &szTable, int nFolderID, const char *szName, const CPropMap *pDefValues )
{
  int nID = InsertMax( szTable, "UserName", "New Item" );

	if ( -1 == nID )
		return -1;
  HRESULT hr = OpenItem( szTable, nID, false, pDefValues );
  if ( FAILED( hr ) )
    return -1;
  if ( !UpdateItem( nFolderID, szName, pDefValues, true ) )
    return -1;
	SetItemColor( szTable, nID, 0 );

  return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬ставка в таблицу новой записи, котора€ €вл€етс€ копией(за исключением ID)
// записи с максимальным ID
// ¬озвр ID новой записи или -1 при ошибке
int CItemsDBCmd::InsertCopyMax( const string &szTable, const CPropMap *pDefValues )
{
  USES_CONVERSION;

  int  nMaxID = GetMaxID( szTable );
	char idBuf[MAX_STRING_LEN + 1];
  itoa( nMaxID, idBuf, 10 );

  string szQuery = string( "SELECT * FROM " ) + szTable + " WHERE ID = " + idBuf;

  Close();
  HRESULT hr = CCommand<CDynamicAccessor>::Open( pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }

  DBORDINAL nColumns;
  DBCOLUMNINFO *pColInfo;  
	OLECHAR *pStrings;
  // Get the column information from the opened rowset
  //hr = GetColumnInfo(&nColumns, &pColInfo);
	hr = CDynamicAccessor::GetColumnInfo( GetInterface(), &nColumns, &pColInfo, &pStrings );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }  

  int i;
  
  for ( i = 0; i < nColumns; ++i )
  {
    if ( !pColInfo[i].pwszName )
      continue;
		if ( DBTYPE_BOOL != pColInfo[i].wType )
			pColInfo[i].wType = DBTYPE_STR;
    AddBindEntry( pColInfo[i] );
  }
  Bind();
	++nMaxID;
	//
  if ( S_OK != MoveNext() )
	{
		nMaxID = 1;
		szQuery = string( "INSERT INTO " ) + szTable + " (ID) VALUES (1)";
	}
	else
	{
		string szColNames = " (";
		string szValues = " (";
		for ( i = 0; i < nColumns; ++i )
		{
			if ( !pColInfo[i].pwszName )
				continue;
			LPSTR pName = W2A( pColInfo[i].pwszName );
			char  *tmp = strcmp( pName, "ID" ) == 0 ? itoa( nMaxID, idBuf, 10 ) : (char*)GetValue( i + 1 );
			DBSTATUS status;
			GetStatus( i + 1, &status );
			if ( DBSTATUS_S_ISNULL == status )
				continue;
			szColNames += string(pName) + ", ";
			if ( DBTYPE_BOOL == pColInfo[i].wType )
			{
				if ( tmp[0] )
					tmp = strcpy( idBuf, "-1" );
				else
					tmp = strcpy( idBuf, "0" );
			}
			if ( pDefValues )
			{
				CPropMap::const_iterator it = pDefValues->find( pName );
				// ѕодставл€ем дефолтное значение, если оно указано
				if ( pDefValues->end() != it )
				{
					CVariant var = it->second->GetDefValue();
					if ( CVariant::VT_NULL != var.GetType() )
					{
						if ( it->second->GetType() == CVariant::VT_FLOAT )
							tmp = "0";
						else
							tmp = strncpy( idBuf, (const char*)var, MAX_STRING_LEN );
					}
				}
			}
			//
			if ( !tmp )
			{
#ifdef _DEBUG
				LONG l;
				GetValue( i + 1, &l );
				string msg = "Empty val in column ";
				msg += W2A( pColInfo[i].pwszName );
				MessageBox( 0, msg.c_str(), 0, MB_OK );
#endif
				szValues += string("'") + "', ";
			}
			else
			{
				szValues += string("'") + tmp + "', ";
			}
		}
		szValues.erase( szValues.end() - 2, szValues.end() );
		szColNames.erase( szColNames.end() - 2, szColNames.end() );
		szQuery = string( "INSERT INTO " ) + szTable + szColNames + ") VALUES" + szValues + ");";
	}  
  CoTaskMemFree(pColInfo);
  Close();
  hr = CCommand<CDynamicAccessor>::Open( pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
  return nMaxID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ѕолучить максимальный ID записи в табице
int CItemsDBCmd::GetMaxID( const string &szTable )
{
  string szQuery = "SELECT MAX(ID) FROM " + szTable;
  
  HRESULT hr = OpenQuery( szQuery, true );
  if ( FAILED( hr ) )
    return 0;
  if ( S_OK != MoveNext() )
    return 0;
  
  ULONG nID;
  if ( !GetValue( 1, &nID ) )
    return 0;
  return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CItemsDBCmd::OpenVariant( const string &szTable, int nVariantID, bool bReadOnly, const CPropMap *propMap )
{
  string szQuery = MakeVariantQuery( szTable, propMap, nVariantID );
  
	HRESULT hr = OpenQuery( szQuery, bReadOnly );
  if ( FAILED( hr ) )
    return hr;
  hr = MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  return S_OK;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CItemsDBCmd::InsertVariant( const string &szTable, int nTemplateID, const CPropMap *pDefValues )
{
  int nID = InsertMax( szTable, "Flags", "" );
  if ( -1 == nID)
    return nID;
	
  HRESULT hr = OpenVariant( szTable, nID, false, pDefValues );
  if ( FAILED( hr ) )
    return -1;
  if ( !UpdateVariant( nTemplateID, pDefValues, true ) )
    return -1;
	
  return nID;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// запись данных варианта в базу данных
// если nTemplateID < 0, то ссылка на темплейт не обновл€етс€
bool CItemsDBCmd::UpdateVariant( int nTemplateID, const CPropMap *propMap, bool bDefValues )
{
	if ( nTemplateID > 0 )
	{
		if ( !SetValue( 1, nTemplateID ) )
			return false;
		SetStatus( 1, DBSTATUS_S_OK );
	}
	
  if ( !WritePropsToDB( this, propMap, bDefValues ) )
    return false;
	//
  HRESULT hr = SetData();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	Close();
  return true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsDBCmd::GetVariant( int *pItemID, int *pTemplateID )
{
	ULONG len1, len2;
	GetLength( 1, &len1 );
	GetLength( 2, &len2 );
	if ( sizeof( int ) != len1 || sizeof( int ) != len2 )
		return false; // неправильный формат полей
  bool ret = GetValue( (ULONG)1, pItemID );
  ret = ret && GetValue( (ULONG)2, pTemplateID );
	
  return ret;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemsDBCmd::SetItemColor( const string &szTable, int nItemID, DWORD dwColor )
{
	if ( FAILED( OpenItem( szTable, nItemID, false, 0 ) ) )
		return false;
	if ( !SetValue( "MEUserColor", dwColor ) )
		return false;
	SetStatus( "MEUserColor", DBSTATUS_S_OK );
	HRESULT hr = SetData();
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	} 
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
