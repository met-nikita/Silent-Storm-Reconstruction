#include "StdAfx.h"
#include "FinDBCmd.h"
#include "DbInl.h"

extern HRESULT InitDB( CSession &session, CDBPropSet &propset );
extern string IToA( int n );
////////////////////////////////////////////////////////////////////////////////////////////////////
string CFinDBCmd::MakePropQuery( const string &szTable, const CPropMap *propMap, int nItem, bool bVarColumn )
{
  string str;
  if ( -1 == nItem )
    str = "SELECT ID, VariantID, ";
  else
  {
    str = "SELECT ";
    if ( bVarColumn )
    {
      if ( propMap )
        str += "VariantID, ";
      else
        str += "VariantID ";
    }
    else if ( !propMap )
      str += "VariantID ";
  }
  
  if ( propMap )
  {
    CPropMap::const_iterator it = propMap->begin();

		if ( dynamic_cast<CListProp*>( it->second.GetPtr() ) )
			++it;
    if ( it != propMap->end() )
    { // перед первой колнкой зап€тую не ставим
      str += it->first;
      ++it;
    }
    while ( it != propMap->end() )
    {
			if ( !dynamic_cast<CListProp*>( it->second.GetPtr() ) )
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
void CFinDBCmd::SetConnection( SDBConnection *pConnection )
{
	dbID.SetConnection( pConnection );
	CBaseDBCmd<CDynamicAccessor >::SetConnection( pConnection );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬ базе данных открываетс€ одна запись с ID = nItemID,
// и становитс€ активной ( автоматически вызываетс€ MoveNext )
// propMap может быть = 0
HRESULT CFinDBCmd::Open( const string &szTable, int nID, const CPropMap *propMap )
{
  string szQuery = MakePropQuery( szTable, propMap, nID );
  
  HRESULT hr = Open( szQuery );
  if ( FAILED( hr ) )
  {
    return hr;
  }
  hr = MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  return S_OK;  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ќткрываетс€ вс€ таблица
// propMap может быть = 0
HRESULT CFinDBCmd::Open( const string &szTable, const CPropMap *propMap )
{
  string szQuery = MakePropQuery( szTable, propMap );

  return Open( szQuery );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFinDBCmd::Open( const std::string &szQuery )
{
  Close();  // очищаем запрос
  HRESULT hr = CCommand<CDynamicAccessor>::Open(pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  
  DBORDINAL       nColumns;
  DBCOLUMNINFO*   pColumnInfo;
  // Get the column information from the opened rowset
  //GetColumnInfo(&nColumns, &pColumnInfo );
	OLECHAR *pStrings;
	//GetColumnInfo( &nColumns, &pColumnInfo );
	CDynamicAccessor::GetColumnInfo( GetInterface(), &nColumns, &pColumnInfo, &pStrings );
  
  for ( int i = 0; i < nColumns; ++i )
  {
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
bool CFinDBCmd::GetElement( int *pElemID, int *pVarID )
{
  bool ret = GetValue( (ULONG)1, pElemID );
  ret = ret && GetValue( (ULONG)2, pVarID );

  return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinDBCmd::DeleteElement()
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
bool CFinDBCmd::UpdateElement( const CPropMap *propMap )
{
  if ( !WritePropsToDB( this, propMap ) )
    return false;

  HRESULT hr = SetData();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }  
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinDBCmd::ReadProps( const CPropMap* props )
{
  return ReadPropsFromDB( this, props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFinDBCmd::InsertMax( const string &szTable, const string &szSomeColumn, int nSomeValue )
{
	Close();
	HRESULT hr = dbID.Open( "SELECT ID, " + szSomeColumn + " FROM " + szTable + " WHERE ID=0" );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	dbID.m_nSomeColumn = nSomeValue;
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
int CFinDBCmd::Insert( const string &szTable, int nVarID, const CPropMap *pDefValues )
{
  int nID = InsertMax( szTable, "Floor", 0 );

	if ( -1 == nID )
		return -1;
  HRESULT hr = Open( szTable, nID, pDefValues );
  if ( FAILED( hr ) )
    return -1;
  if ( !UpdateElement( pDefValues ) )
    return -1;
	string szQuery = string( "SELECT VariantID FROM " ) + szTable + " WHERE ID = " + IToA( nID );
	hr = Open( szQuery );
	if ( FAILED(hr) )
		return -1;
	if ( MoveNext() == S_OK )
	{
		SetValue( "VariantID", nVarID );
		SetStatus( "VariantID", DBSTATUS_S_OK );		
	  HRESULT hr = SetData();
		if ( FAILED( hr ) )
			return -1;
	}

  return nID;
  //return InsertCopyMax( szTable, nVarID, pDefValues );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬ставка в таблицу новой записи, котора€ €вл€етс€ копией(за исключением ID и VariantID )
// записи с максимальным ID
// ¬озвр ID новой записи или -1 при ошибке
int CFinDBCmd::InsertCopyMax( const string &szTable, int nVarID, const CPropMap *pDefValues )
{
  USES_CONVERSION;
  
  int  nMaxID = GetMaxID( szTable );
  char idBuf[MAX_STRING_LEN];
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
  DBCOLUMNINFO*   pColInfo;  
  // Get the column information from the opened rowset
	OLECHAR *pStrings;
	CDynamicAccessor::GetColumnInfo( GetInterface(), &nColumns, &pColInfo, &pStrings );
  
  int i;
  
  for ( i = 0; i < nColumns; ++i )
  {
    pColInfo[i].wType = DBTYPE_STR;
    AddBindEntry( pColInfo[i] );
  }
  Bind();
  if ( S_OK != MoveNext() )
	{
		// ¬озможно в базе еще нет ни одной записи
		// пробуем вставить первую запись 
		CoTaskMemFree(pColInfo);
		Close();
		szQuery = string( "INSERT INTO " ) + szTable + "(ID, VariantID) VALUES (1," + IToA( nVarID ) + ")";
		hr = CCommand<CDynamicAccessor>::Open( pConnection->session,  szQuery.c_str(), &pConnection->propset, 0, DBGUID_DEFAULT, false );
		if ( FAILED( hr ) )
			return -1;
		return 1;
	}
  
  LPWSTR pwszID = A2W( "ID" );  
  LPWSTR pwszVarID = A2W( "VariantID" );  
  ++nMaxID;
  szQuery = string( "INSERT INTO " ) + szTable + " VALUES (";
  for ( i = 0; i < nColumns; ++i )
  {
    char* tmp;

    if ( wcscmp( pColInfo[i].pwszName, pwszID ) == 0  )
      tmp = itoa( nMaxID, idBuf, 10 );
    else if ( wcscmp( pColInfo[i].pwszName, pwszVarID ) == 0 )
      tmp = itoa( nVarID, idBuf, 10 );
    else
      tmp = (char*)GetValue( i + 1 );

		// ѕодставл€ем дефолтное значение, если оно указано
		if ( pDefValues )
		{
			LPSTR pName = W2A( pColInfo[i].pwszName );
			CPropMap::const_iterator it = pDefValues->find( pName );
			
			if ( pDefValues->end() != it )
			{
				CVariant var = it->second->GetDefValue();
				if ( CVariant::VT_NULL != var.GetType() )
				{
					tmp = strncpy( idBuf, (const char*)var, MAX_STRING_LEN );
				}
			}
		}
		szQuery += string("'") + tmp + "', ";
  }
  szQuery.erase( szQuery.end() - 2, szQuery.end() );
  szQuery += ");";
  
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
int CFinDBCmd::GetMaxID( const string &szTable )
{
  string szQuery = "SELECT MAX(ID) FROM " + szTable;
 
  HRESULT hr = Open( szQuery );
  if ( FAILED( hr ) )
    return 0;
  if ( S_OK != MoveNext() )
    return 0;
  
  ULONG nID;
  if ( !GetValue( 1, &nID ) )
    return 0;
  return nID;
}
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::Open( int nID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT * FROM FinalElements WHERE ID=";
	szQuery += IToA( nID );
	if ( FAILED( CBaseDBCmd<CAccessor<CFinPosAccessor> >::Open( szQuery ) ) || MoveNext() != S_OK )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetPos( int nID, CVec2 &ptPos, float fDZ, int nFloor, float fRotation )
{
	if ( !Open( nID ) )
		return false;
	m_fPosX = ptPos.x;
	m_fPosY = ptPos.y;
	m_fDZ = fDZ;
	m_nFloor = nFloor;
	m_fRotation = fRotation;
  HRESULT hr = SetData( 1 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::Delete( int nID )
{
	if ( !Open( nID ) )
		return false;
	HRESULT hr = CBaseDBCmd<CAccessor<CFinPosAccessor> >::Delete();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetOpen( int nID, bool bOpen )
{
	if ( !Open( nID ) )
		return false;
	m_bOpen = bOpen;
	HRESULT hr = SetData( 4 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetLightmap( int nID, bool bLightmap )
{
	if ( !Open( nID ) )
		return false;
	m_bLightmap = bLightmap;
	HRESULT hr = SetData( 6 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFinPosDB::Insert( int nVariantID, int nModelID )
{
	if ( !HasConnection() )
		SetConnection( &dbConnection );
	string szQuery = "SELECT ID, ModelID, VariantID FROM FinalElements WHERE ID=-1";
	if ( FAILED( CBaseDBCmd<CAccessor<CFinPosAccessor> >::Open( szQuery ) ) )
		return -1;
	m_nVarID = nVariantID;
	m_nModelID = nModelID;
	HRESULT hr = CBaseDBCmd<CAccessor<CFinPosAccessor> >::Insert( 2 );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	hr = MoveNext();
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
	return m_nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetModel( int nID, int nModelID )
{
	if ( !Open( nID ) )
		return false;
	m_nModelID = nModelID;
	HRESULT hr = SetData( 3 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetScale( int nID, float fX, float fY, float fZ )
{
	if ( !Open( nID ) )
		return false;
	m_fScaleX = fX;
	m_fScaleY = fY;
	m_fScaleZ = fZ;
	HRESULT hr = SetData( 5 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFinPosDB::SetLight( int nID, int nColor, const CVec3 &ptPos, float fRadius, float fFlareRadius, int nFlareTexture, const CVec3 &ptFlarePos )
{
	if ( !Open( nID ) )
		return false;
	m_nLightCr = nColor;
	m_fLightX = ptPos.x;
	m_fLightY = ptPos.y;
	m_fLightZ = ptPos.z;
	m_fLightRadius = fRadius;
	m_fFlareRadius = fFlareRadius;
	m_nFlareTexture = nFlareTexture;
	m_fFlareX = ptFlarePos.x;
	m_fFlareY = ptFlarePos.y;
	m_fFlareZ = ptFlarePos.z;
	HRESULT hr = SetData( 7 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
