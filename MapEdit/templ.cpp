#include "StdAfx.h"
#include "TemplMgr.h"
#include "templ.h"
#include "VariantsDBCmd.h"
#include "Placement.h"
#include "FinalElem.h"
#include "ItemsMgr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Форматирование запроса к базе данных на извлечение записей 
// c ID индексами, заданными в "items"
// предполагается что в items есть хотя бы один элемент
void MakeQueryStr( string &szQuery, const string &szTable, vector<int> items )
{
  char buf[8];
  const int n = items.size();

  szQuery = " SELECT * FROM " + szTable + " WHERE ID = ";
  itoa( items[0], buf, 10 );
  szQuery += buf;

  for( int i = 1; i < n; ++i )
  {
    szQuery += " OR ID = ";
    itoa( items[i], buf, 10 );
    szQuery += buf;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//      CLASS CTemplate
////////////////////////////////////////////////////////////////////////////////////////////////////
CTemplate::CTemplate( CVariantsDBCmd *_pDBVars ) : bInitialized(false), iCurPlacement(-1), pDBVars( _pDBVars )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTemplate::~CTemplate()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplate::Init( int _id, int _type, int _width, int _height, const char* _name )
{
  id = _id;
  name = _name;
  type = _type;
  bInitialized = true;
  width  = _width;
  height = _height;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление нового варианта расстановки в темплейт
void CTemplate::AddPlacement( int varID, bool bSetModified )
{
  variants.push_back( varID );
  varID2Ind[varID] = variants.size() - 1;
  if ( bSetModified )
    theTemplMgr.SetModifiedTempl( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление нового варианта расстановки в темплейт
// возвращается ID новой расстановки  (-1 если произошла ошибка)
int CTemplate::AddPlacement()
{
  string szQuery;
  vector<int> items;

  // запрос должен вернуть пустой rowset
  items.push_back( -1 );
  MakeQueryStr( szQuery, VARIANTS_TBL, items );
  HRESULT hr = pDBVars->Open( szQuery );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  }
  pDBVars->m_TemplID = id;
  hr = pDBVars->Insert( 1 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  } 
  hr = pDBVars->MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  } 

  int varID = pDBVars->m_ID;

  variants.push_back( varID );
  varID2Ind[varID] = variants.size() - 1;

  theTemplMgr.SetModifiedTempl( id );
  return varID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Устанавливает новый тип для темплейта
bool CTemplate::SetType( int typeID )
{
  type = typeID;
  theTemplMgr.SetModifiedTempl( id );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplate::SetupPlacementMap()
{
  varID2Ind.clear();

  for ( int i=0; i < (int)variants.size(); ++i )
    varID2Ind[variants[i]] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
