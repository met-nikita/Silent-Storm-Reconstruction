#include "StdAfx.h"

#include "TemplMgr.h"
#include "TemplDBCmd.h"
#include "RectsDBCmd.h"
#include "VariantsDBCmd.h"
#include "FinDBCmd.h"
#include "WallsDBCmd.h"
#include "ItemsMgr.h"

#include "templ.h"
#include "placement.h"
#include "FinalElem.h"
#include "Unit.h"
#include "MapEdit.h"

static const char LOCAL_FILE[] = __FILE__;
//#include "BugSlayer.h"
//#include "DebugAlloc.h"

extern SDBConnection dbConnection;
CTemplateMgr theTemplMgr;

namespace
{
	CVariantsDBCmd dbVars;
	CTemplDBCmd dbTempl;
	CFinDBCmd   dbFin;
	bool bDBInitialized = false;
}

inline void PushUnique( vector<int> &vec, int item )
{
  std::vector<int>::const_iterator it = std::find( vec.begin(), vec.end(), item );

  if ( vec.end() == it )
    vec.push_back( item );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//            CLASS CTemplateMgr
////////////////////////////////////////////////////////////////////////////////////////////////////
CTemplateMgr::CTemplateMgr() : iCurTempl(-1)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTemplateMgr::~CTemplateMgr()
{
#ifdef _DEBUG
	char buf[512];
	sprintf( buf, "Placement list size =  %d\n", placementList.size() );
	OutputDebugString( buf );
#endif

  int i;
	
  for ( list<CPlacement* >::iterator  it = placementList.begin(); it != placementList.end(); ++it )
    delete *it;
  for ( i=0; i < (int)templates.size(); ++i )
    delete templates[i];
  templates.clear();
  for ( i=0; i < (int)finElements.size(); ++i )
    delete finElements[i];

  placementList.clear();
  templates.clear();
  finElements.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Загрузка данных из базы
// функция должна быть вызвана приложением до создания окон
bool CTemplateMgr::Create()
{
	if ( !bDBInitialized )
	{
		dbVars.SetConnection( &dbConnection );
		dbTempl.SetConnection( &dbConnection );
		dbFin.SetConnection( &dbConnection );
		bDBInitialized = true;
	}

  if ( !LoadTemplateTree() )
    return false;

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Загружает список темплейтов из базы данных
// функция должна быть вызвана приложением до создания окон
// и после LoadFinElements()
bool CTemplateMgr::LoadTemplateTree()
{
  // Инициализируем интерфейс к базе данных
	string szTemplQuery = string("SELECT * FROM " ) + TEMPL_TBL;
	string szVarQuery = string("SELECT * FROM " ) + VARIANTS_TBL;
  if ( S_OK != dbTempl.Open( szTemplQuery ) )
    return false;
  
  // загружаем все темплейты
  while ( dbTempl.MoveNext() == S_OK )
  {
    CTemplate *pTempl = dbgnew CTemplate( &dbVars );

    pTempl->Init( dbTempl.m_TemplateId, 
                dbTempl.m_Type, 
                dbTempl.m_Width, 
                dbTempl.m_Height, 
                dbTempl.m_Name );
    templates.push_back( pTempl );
    templID2Ind[dbTempl.m_TemplateId] = templates.size() - 1;
  }
  // загрузка расстановок для прямоугольников
  // предпологается, что расст. для конечных элементов уже загружены
	if ( S_OK != dbVars.Open( szVarQuery ) )
		return false;
  while ( dbVars.MoveNext() == S_OK )
  {
    CTemplate *pTempl = GetTempl( dbVars.m_TemplID );
    if ( !pTempl )
      continue;
    pTempl->AddPlacement( dbVars.m_ID, false );
  }
  dbTempl.Close();
  dbVars.Close();

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на расстановку
// расстановка создается и считывается из базы данных
// после того, как она становится ненужна, необходимо вызвать ReleasePlacement()
CPlacement* CTemplateMgr::GetPlacement( int id )
{
	if ( id <= 0 )
		return 0;
	CPlacement *pPl = new CPlacement();

	if ( !LoadPlacement( id, pPl ) )
	{
		delete pPl;
		return 0;
	}
	placementList.push_back( pPl );
  return pPl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateMgr::ReleasePlacement( CPlacement *pPl )
{
	for ( list<CPlacement*>::iterator it = placementList.begin(); it != placementList.end(); ++it )
    if ( (*it) == pPl )
    {
      delete (*it);
      placementList.erase( it );
      return;
    }		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// сброс указателя на текущий темплейт
// для установки указателя на 1ый темплейт вызвать MoveNextTempl()
void CTemplateMgr::MoveFirstTempl()
{
  iCurTempl = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущий темплей в следующую позицию
// возвращает "false", если достигнут конец списка
bool CTemplateMgr::MoveNextTempl()
{
  if ( iCurTempl >= (int)templates.size() - 1 )
    return false;
  ++iCurTempl;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить ID текущего темплейта
// Возвр. -1 если тек. темплейт не установлен
int CTemplateMgr::GetCurTemplID()
{
  if ( iCurTempl != -1 )
    return templates[iCurTempl]->GetID();
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить текущий темплейт
// Возвр. 0 если тек. темплейт не установлен
CTemplate* CTemplateMgr::GetCurTempl()
{
  if ( iCurTempl != -1 )
    return templates[iCurTempl];
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTemplate* CTemplateMgr::GetTempl( int id )
{
  int ind = GetTemplIndex( id );

  if ( ind != -1 )
    return templates[ind];
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить временный ID для новой стенки
// реальный ID становится известен после добавления в базу данных
int CTemplateMgr::GetTempWallID()
{
  return ++iMaxWallID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateMgr::UpdateModifiedTempls()
{
  if ( modifiedTemplates.empty() )
    return true;

  // Set the parameter for the query
  string szQuery;
  MakeQueryStr( szQuery, TEMPL_TBL, modifiedTemplates );
  HRESULT hr = dbTempl.Open( szQuery );

  if ( FAILED( hr ) )
    return false;

  // обновление в базе всех полученных по запросу темплейтов
  while( dbTempl.MoveNext() == S_OK )
  {
    CTemplate *pTempl = GetTempl( dbTempl.m_TemplateId );
    if ( !pTempl )
      continue;
    const int maxl = sizeof(dbTempl.m_Name) - 1;
    strncpy( dbTempl.m_Name, pTempl->GetName(), maxl );
    dbTempl.m_Name[maxl] = 0;
    dbTempl.m_Type   = pTempl->GetTypeID();
    dbTempl.m_Width  = pTempl->GetWidth();
    dbTempl.m_Height = pTempl->GetHeight();
    
    // Запись обновления в базу
    // используется аксессор, который не содержит связанных полей
    hr = dbTempl.SetData( 1 );

    if ( FAILED(hr) )
    {
      DisplayOLEDBErrorRecords( hr );
      return false;
    }
  }

  modifiedTemplates.clear();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateMgr::UpdateModifiedFinElems()
{
  int i;

  for ( i = 0; i < modifiedFinElems.size(); ++i )
  {
    CFinElement *pFin = GetFinElement( modifiedFinElems[i] );
    if ( !pFin )
      continue;
    if ( FAILED( dbFin.Open( FINELEMS_TBL, pFin->GetElementID(), pFin->GetPropList() ) ) )
      return false;
    if ( !dbFin.UpdateElement( pFin->GetPropList() ) )
      return false;
    dbFin.Close();
  }
  modifiedFinElems.clear();
  for ( i = 0; i < modifiedUnits.size(); ++i )
  {
    CFinElement *pFin = GetFinElement( modifiedUnits[i] );
    if ( !pFin )
      continue;
    if ( FAILED( dbFin.Open( UNITS_TBL, pFin->GetElementID(), pFin->GetPropList() ) ) )
      return false;
    if ( !dbFin.UpdateElement( pFin->GetPropList() ) )
      return false;
    dbFin.Close();
  }
  modifiedUnits.clear();

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Функция для использования в классе CTemplate, 
// нет необходимости ее вызывать в других местах
// Установка "Modified" флага для темлейта с templID
// данный темплейт будет обновлен в базе данных
// при вызове UpdateModified()
void CTemplateMgr::SetModifiedTempl( int templID )
{
  PushUnique( modifiedTemplates, templID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Обновление модифицированных темплейтов в базе данных
bool CTemplateMgr::UpdateModified()
{
  bool ret = UpdateModifiedTempls();
  ret = ret && UpdateModifiedFinElems();
  return ret;// && pTemplateTypes->UpdateModified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateMgr::AddTemplate( int nTemplID, int width, int height )
{
  string      szQuery;
  vector<int> items( 1, nTemplID );

  MakeQueryStr( szQuery, TEMPL_TBL, items );
  HRESULT hr = dbTempl.Open( szQuery );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
  hr = dbTempl.MoveNext();
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
  dbTempl.m_Height = height;
  dbTempl.m_Width  = width;

  hr = dbTempl.SetData( 1 );
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }

  CTemplate *pTempl = dbgnew CTemplate( &dbVars );
  pTempl->Init( dbTempl.m_TemplateId, dbTempl.m_Type, width, height, dbTempl.m_Name );
  templates.push_back( pTempl );
  templID2Ind[dbTempl.m_TemplateId] = templates.size() - 1;
	//
	//pTempl->AddPlacement();
	dbTempl.Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateMgr::SetupFinElemMap()
{
  plcmnID2FinInd.clear();
  finID2FinInd.clear();

  for ( int i=0; i < (int)finElements.size(); ++i )
  {
    plcmnID2FinInd[finElements[i]->GetID()] = i;
    finID2FinInd[finElements[i]->GetElementID()] = i;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateMgr::SetupTemplateMap()
{
  templID2Ind.clear();

  for ( int i=0; i < (int)templates.size(); ++i )
    templID2Ind[templates[i]->GetID()] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Загружает список конечных элементов из базы данных
// функция должна быть вызвана до LoadTemplateTree()
bool CTemplateMgr::LoadFinElements()
{
  // конечные элементы
  {
    CFinElement tmp( &dbFin, 0, 0 );
    HRESULT hr = dbFin.Open( FINELEMS_TBL, tmp.GetPropList() );

    if ( FAILED( hr ) )
    {
      return false;
    } 
    while ( dbFin.MoveNext() == S_OK )
    {
      int nVarID, nElemID;
      dbFin.GetElement( &nElemID, &nVarID );
      CFinElement *pEl = dbgnew CFinElement( &dbFin, nVarID, nElemID );
      finElements.push_back( pEl );
      plcmnID2FinInd[pEl->GetID()] = finElements.size() - 1;
      finID2FinInd[pEl->GetElementID()] = finElements.size() - 1;
    }
  }
  // Юниты
  {
    CUnitElement tmp( &dbFin, 0, 0 );
    HRESULT hr = dbFin.Open( UNITS_TBL, tmp.GetPropList() );

    if ( FAILED( hr ) )
    {
      return false;
    }
    while ( dbFin.MoveNext() == S_OK )
    {
      int nVarID, nElemID;
      dbFin.GetElement( &nElemID, &nVarID );
      CUnitElement *pEl = dbgnew CUnitElement( &dbFin, nVarID, nElemID );
      finElements.push_back( pEl );
      plcmnID2FinInd[pEl->GetID()] = finElements.size() - 1;
      finID2FinInd[pEl->GetElementID()] = finElements.size() - 1;
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFinElement* CTemplateMgr::GetFinElement( int id )
{
  int ind = GetFinIndex( id );

  if ( ind != -1 )
    return finElements[ind];
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateMgr::LoadPlacement( int nVarID, CPlacement *pPl )
{
	if ( FAILED( dbVars.Open( string( "SELECT * FROM " ) + VARIANTS_TBL + " WHERE ID=" + IToA( nVarID ) ) ) )
		return false;
	// загрузка расстановоки
	if ( S_OK != dbVars.MoveNext() )
		return false;

  pPl->Init( dbVars.m_ID, dbVars.m_TemplID, dbVars.m_bGrid, dbVars.m_fRndWeight, dbVars.m_nDefLight, dbVars.m_nScriptID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
