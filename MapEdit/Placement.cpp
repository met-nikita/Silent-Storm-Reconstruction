#include "StdAfx.h"
#include "TemplMgr.h"
#include "Placement.h"
#include "PlacementDefs.h"
#include "templ.h"
#include "Walls.h"
#include "WallsDBCmd.h"
#include "FloorDB.h"
#include "RoomsDB.h"
#include "Floor.h"
#include "MapEdit.h"
#include "ItemsMgr.h"
#include <limits>
#include "Export.h"
#include "VariantsDBCmd.h"
#include "PlacableDB.h"

#include "RectsDBCmd.h"
#include "FinDBCmd.h"
#include "CtrlObjectInspector.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\TerrainInfo.h"
#include "..\Main\Grid.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataTerrain.h"
#include "MESerialize.h"
#include "iWysiwyg.h"
#include "..\Main\aiWaypoint.h"
#include "ObjectMgr.h"
#include "WaypointDB.h"

bool gbLoad2DView = true;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, CMETerrainLoader> shareTerrains;
extern CBasicShare<int, NAI::CWaypointLoader> shareWaypoints;
namespace NWysiwyg
{
extern int  AddTerrSpotDB( int nVarID, const NBuilding::SProjectedSpot &spot );
}
int CPlacement::iMaxRectID = -1;
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int Floor2Ind( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	return nFloor - MIN_FLOOR;
}
static inline int Ind2Floor( int nInd )
{
	return nInd + MIN_FLOOR;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern SDBConnection dbConnection; // находится в TemplMgr.h
namespace 
{
  CRectsDBCmd rectsDB;
  CWallsDBCmd wallsDB;
	CFloorDB    floorDB;
	CRoomDB     roomsDB;
	CFinDBCmd   finDB;
	CVariantsDBCmd dbVars;
  bool bDBInitialized = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SUnit::SUnit()
{
	CFinProp *ppropFloor = new CFinProp( "Floor", 0, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropMonsterID = new CFinProp( "MonsterID", 1, CVariant::VT_INT, DT_DEC );
	CFinProp *ppropRotation = new CFinProp( "Rotation", 2, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropPosX = new CFinProp( "PosX", 3, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropPosY = new CFinProp( "PosY", 4, CVariant::VT_INT, DT_DEC, 0 );

	ppropMonsterID->SetGroup( IDC_RPG_PERS_TREE );
	ppropMonsterID->SetRelation( IDC_RPG_PERS_TREE );
		
	props.insert( CPropMap::value_type( ppropFloor->GetName(), ppropFloor ) );
	props.insert( CPropMap::value_type( ppropMonsterID->GetName(), ppropMonsterID ) );
	props.insert( CPropMap::value_type( ppropRotation->GetName(), ppropRotation ) );
	props.insert( CPropMap::value_type( ppropPosX->GetName(), ppropPosX ) );
	props.insert( CPropMap::value_type( ppropPosY->GetName(), ppropPosY ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SUnit::~SUnit()
{
	props.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SFinElement::SFinElement() : ptSize(VNULL2)
{
	CFinProp *ppropFloor = new CFinProp( "Floor", 0, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropMonsterID = new CFinProp( "ModelID", 1, CVariant::VT_INT, DT_DEC );
	CFinProp *ppropRotation = new CFinProp( "Rotation", 2, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropPosX = new CFinProp( "PosX", 3, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropPosY = new CFinProp( "PosY", 4, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropDZ = new CFinProp( "DeltaZ", 5, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropScaleX = new CFinProp( "ScaleX", 6, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropScaleY = new CFinProp( "ScaleY", 7, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropScaleZ = new CFinProp( "ScaleZ", 8, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropPRoomID = new CFinProp( "RoomParamID", 4, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropRoomID = new CFinProp( "RoomID", 6, CVariant::VT_INT, DT_DEC, 0 );
	
	ppropMonsterID->SetGroup( IDC_OBJECTS_TREE );
	ppropMonsterID->SetRelation( IDC_OBJECTS_TREE );
		
	props.insert( CPropMap::value_type( ppropFloor->GetName(), ppropFloor ) );
	props.insert( CPropMap::value_type( ppropMonsterID->GetName(), ppropMonsterID ) );
	props.insert( CPropMap::value_type( ppropRotation->GetName(), ppropRotation ) );
	props.insert( CPropMap::value_type( ppropPosX->GetName(), ppropPosX ) );
	props.insert( CPropMap::value_type( ppropPosY->GetName(), ppropPosY ) );
	props.insert( CPropMap::value_type( ppropDZ->GetName(), ppropDZ ) );
	props.insert( CPropMap::value_type( ppropScaleX->GetName(), ppropScaleX ) );
	props.insert( CPropMap::value_type( ppropScaleY->GetName(), ppropScaleY ) );
	props.insert( CPropMap::value_type( ppropScaleZ->GetName(), ppropScaleZ ) );
	props.insert( CPropMap::value_type( ppropPRoomID->GetName(), ppropPRoomID ) );
	props.insert( CPropMap::value_type( ppropRoomID->GetName(), ppropRoomID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SExplosion::SExplosion()
{
	CFinProp *ppropFloor = new CFinProp( "Floor", 0, CVariant::VT_INT, DT_DEC, 0 );
	CFinProp *ppropPowerID = new CFinProp( "Power", 1, CVariant::VT_FLOAT, DT_DEC );
	CFinProp *ppropPosX = new CFinProp( "PosX", 3, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropPosY = new CFinProp( "PosY", 4, CVariant::VT_FLOAT, DT_DEC, 0 );
	CFinProp *ppropDZ = new CFinProp( "DeltaZ", 5, CVariant::VT_FLOAT, DT_DEC, 0 );
	
	props.insert( CPropMap::value_type( ppropFloor->GetName(), ppropFloor ) );
	props.insert( CPropMap::value_type( ppropPowerID->GetName(), ppropPowerID ) );
	props.insert( CPropMap::value_type( ppropPosX->GetName(), ppropPosX ) );
	props.insert( CPropMap::value_type( ppropPosY->GetName(), ppropPosY ) );
	props.insert( CPropMap::value_type( ppropDZ->GetName(), ppropDZ ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить временный ID для нового прямоугольника
// реальный ID становится известен после добавления в базу данных
int CPlacement::GetTempRectID()
{
	return ++iMaxRectID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlacement::CPlacement() 
 : bInitialized(false), iCurRect(-1), iCurUnit( -1 ), iCurObj( -1 ), bHasAIGrid( false ), fRndWeight(1),
	 nDefLightID(-1), nScriptID(-1)
{
  if ( !bDBInitialized )
  {
    wallsDB.SetConnection( &dbConnection );
    rectsDB.SetConnection( &dbConnection );
		floorDB.SetConnection( &dbConnection );
		roomsDB.SetConnection( &dbConnection );
	  finDB.SetConnection( &dbConnection );
		dbVars.SetConnection( &dbConnection );
    bDBInitialized = true;
  }
	const int NFLOORS = 1 + MAX_FLOOR - MIN_FLOOR;
	rects.resize( NFLOORS );
	units.resize( NFLOORS );
	finobjs.resize( NFLOORS );
	solids.resize( NFLOORS );
	rooms.resize( NFLOORS );
	explosions.resize( NFLOORS );
	roomIDs.resize( NFLOORS );
	bLoaded = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlacement::~CPlacement()
{
	int i, j;
	//
	for( i = 0; i < units.size(); ++i )
		for( j = 0; j < units[i].units.size(); ++j )
			if ( units[i].units[j] )
				delete units[i].units[j];
	//
	walls.clear();
	floors.clear();
	floorsInterm.clear();
	solids.clear();
	solidsInterm.clear();
	rooms.clear();
	units.clear();
	finobjs.clear();
	explosions.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID текущего объекта-прямоугольника
// "-1" если указатель на текущий объект не установлен
int CPlacement::GetRectID() const
{
	int ind = Floor2Ind( iCurFloor );
  if ( iCurRect >= 0 && iCurRect < (int)rects[ind].placement.size() )
    return rects[ind].placement[iCurRect].rectID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сброс указателя на текущий объект-прямоугольник
// для установки на первый объект нужно вызвать MoveNext()
void CPlacement::MoveFirst( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	iCurFloor = nFloor;
  iCurRect = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущий объект-прямоугольник в следующую позицию
// возвращает "false", если достигнут конец списка
bool CPlacement::MoveNext()
{
  if ( iCurRect >= (int)rects[Floor2Ind( iCurFloor )].placement.size() - 1 )
    return false;
  ++iCurRect;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвр. true, если список прямоугольников для этажа nFloor пуст
bool CPlacement::Empty( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  return rects[Floor2Ind( nFloor )].placement.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвр. true, если список прямоугольников пуст
bool CPlacement::Empty()
{
	for ( int i = 0; i < rects.size(); ++i )
		if ( !rects[i].placement.empty() )
			return false;
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::Init( int _id, int templID, bool bGrid, float fRndW, int nDefLight, int _nScriptID )
{
  id = _id;
	bHasAIGrid	= bGrid;
  prntTemplID = templID;
	fRndWeight	= fRndW;
	nDefLightID = nDefLight;
	nScriptID = _nScriptID;
  bInitialized = true;
	szTerrName = GetExportDstDir() + TERRAIN_DIR + IToA( GetID() );
	CTemplate *pTempl = theTemplMgr.GetTempl( prntTemplID );
	if ( !pTempl )
		return;
	nWidth  = pTempl->GetWidth();
	nHeight = pTempl->GetHeight();
#ifdef _DEBUG
	char buf[512];
	sprintf( buf, "Loading placement %d\n", id );
	OutputDebugString( buf );
#endif
	if ( gbLoad2DView )
	{
		LoadRects();
		LoadUnits();
		LoadFinObjs();
		LoadExplosions();
		bLoaded = true;
	}
	LoadFloors();
	//ReadTerrain();
	pTerrLoader = shareTerrains.Get( id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::UpdateLayers()
{
	floors.clear();
	floorsInterm.clear();
	solids.clear();
	solidsInterm.clear();
	LoadFloors();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::ForceLoad()
{
	if ( !bLoaded )
	{
		LoadRects();
		LoadUnits();
		LoadFinObjs();
		LoadExplosions();
		bLoaded = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Используется только при чтении из базы данных
void CPlacement::PushRect( int nFloor,
												int rectID, 
												const CVec3 &ptCenter, 
												float fWidth, 
												float fHeight, 
												int templID, 
												int nRotation )
{
  if ( GetTemplateID() == templID )
    return;
  SObjRect or;
	MakeRect( or.rect, CVec2( ptCenter.x, ptCenter.y ), fWidth, fHeight, nRotation );
//  if ( !CheckRect( or.rect ) )
//    return;

  or.rectID   = rectID;
  or.templID  = templID;
	or.ptCenter = ptCenter;
	or.fWidth   = fWidth;
	or.fHeight  = fHeight;
  or.rotation = nRotation;

	SFloorRects* pRects = GetRects( nFloor );
	if ( !pRects )
		return;
  pRects->placement.push_back( or );
  pRects->objID2objIndex[rectID] = pRects->placement.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление нового прямоугольника в темплейт
// возвращается временный ID нового прямоугольника,
// -1 если произошла ошибка
// постоянный ID будет известен после добавления в базу данных
int CPlacement::AddRect( int nFloor, const CVec2 &ptCenter, float fWidth, float fHeight, int templID )
{
  if ( GetTemplateID() == templID )
    return -1;
  SObjRect or;
	CVec2 ptPos;
	if ( !GetNearestPos( &ptPos, ptCenter, fWidth, fHeight ) )
		return -1;
	MakeRect( or.rect, ptPos, fWidth, fHeight, 0 );
	
  or.rectID   = GetTempRectID();
  or.templID  = templID;
  or.bNewRect = true;
  or.rotation = 0;
	or.ptCenter = CVec3( ptPos.x, ptPos.y, 0 );
	or.fWidth   = fWidth;
	or.fHeight  = fHeight;
	
	SFloorRects* pRects = GetRects( nFloor );
	if ( !pRects )
		return -1;
  pRects->placement.push_back( or );
  pRects->objID2objIndex[or.rectID] = pRects->placement.size() - 1;

	if ( !UpdateRects() )
		return -1;
  return pRects->placement.back().rectID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавить прямоугольник в базу данных
// возвращается реальный ID прямоугольника, -1 при ошибке
int CPlacement::AddRectToDB( int nFloor, int ind )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  string szQuery;
  vector<int> items( 1, -1 );
  SObjRect *pObj = &rects[Floor2Ind( nFloor )].placement[ind];

  // запрос должен вернуть пустой rowset
  MakeQueryStr( szQuery, RECTS_TBL, items );
  HRESULT hr = rectsDB.Open( szQuery );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return -1;
  } 
  rectsDB.m_VariantID   = id;
  rectsDB.m_TemplateLink = pObj->templID;
  rectsDB.m_Rotation     = pObj->rotation;
	rectsDB.m_CenterX = pObj->ptCenter.x;
	rectsDB.m_CenterY = pObj->ptCenter.y;
	rectsDB.m_fDZ = 0;
	rectsDB.m_Width   = pObj->fWidth;
	rectsDB.m_Height  = pObj->fHeight;
	rectsDB.m_Floor   = nFloor;
  hr = rectsDB.Insert( 1 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    rectsDB.Close();
    return -1;
  } 
  hr = rectsDB.MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    rectsDB.Close();
    return -1;
  } 

  pObj->rectID   = rectsDB.m_RectID;
  pObj->bNewRect = false;

  rects[Floor2Ind( nFloor )].objID2objIndex[pObj->rectID] = ind;

  rectsDB.Close();
  return pObj->rectID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::RemoveRectFromDB( int id )
{
  string szQuery;
  vector<int> items( 1, id );

  MakeQueryStr( szQuery, RECTS_TBL, items );
  HRESULT hr = rectsDB.Open( szQuery );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
  rectsDB.MoveNext();
  hr = rectsDB.Delete();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    rectsDB.Close();
    return false;
  }
  rectsDB.Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID темплейта, которому принадлежит данная расстановка
int CPlacement::GetTemplateID() const
{
  return prntTemplID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Обновление прямоугольников
bool CPlacement::UpdateRects()
{
  // создание запроса:
  vector<int> rectsWrite;
  int i, j;
	
	for ( j = 0; j < rects.size(); ++j )
	{
		for ( i=0; i < (int)rects[j].placement.size(); ++i )
		{
			if ( rects[j].placement[i].bNewRect )
			{
				if ( -1 == AddRectToDB( Ind2Floor( j ), i ) )
					return false;
			}
			else
				rectsWrite.push_back( rects[j].placement[i].rectID );
		}
	}
	//
  for( i=0; i < (int)delObjs.size(); ++i )
    RemoveRectFromDB( delObjs[i] );
  delObjs.clear();

  if ( rectsWrite.empty() )
    return true;
  string szQuery;
  MakeQueryStr( szQuery, RECTS_TBL, rectsWrite );
  
  HRESULT hr = rectsDB.Open( szQuery );
  
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
  while( rectsDB.MoveNext() == S_OK )
  {
		int ind = GetObjIndex( rectsDB.m_Floor, rectsDB.m_RectID );
		const SObjRect &or = rects[Floor2Ind( rectsDB.m_Floor )].placement[ind];
		if ( -1 == ind )
			continue;
		rectsDB.m_CenterX = or.ptCenter.x;
		rectsDB.m_CenterY = or.ptCenter.y;
		rectsDB.m_Width   = or.fWidth;
		rectsDB.m_Height  = or.fHeight;
    rectsDB.m_TemplateLink = or.templID;
    rectsDB.m_Rotation = or.rotation;
		rectsDB.m_fDZ = or.ptCenter.z;
    hr = rectsDB.SetData( 1 );
    if ( FAILED(hr) )
    {
      DisplayOLEDBErrorRecords( hr );
      rectsDB.Close();
      return false;
    }
  }
  rectsDB.Close();
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID темплейта, который содержится 
// в данном ("objID") прямоугольнике
// если темплейт не найден возвращает "-1"
int CPlacement::GetRectTemplID( int nFloor, int objID ) const
{
  int ind = GetObjIndex( nFloor, objID );
	const SFloorRects* pRects = GetRects( nFloor );
  if ( ind != -1 || !pRects )
    return pRects->placement[ind].templID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID темплейта, который содержится 
// в текущем прямоугольнике (указатель на текущий объект 
// устанавливается ф-иями MoveFirst, MoveNext)
//
// если темплейт не найден или текущий объект не 
// установлен возвращает "-1"
int CPlacement::GetRectTemplID() const
{
	int ind = Floor2Ind( iCurFloor );
  if ( iCurRect >= 0 && iCurRect < (int)rects[ind].placement.size() )
    return rects[ind].placement[iCurRect].templID;
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// В "prect" возвращает местоположение объекта с ID "objID"
// если объект с данным ID не найден, возвращается "false"
bool CPlacement::GetRect( int nFloor, int objID, CVec3 *pPtCenter, float *pWidth, float *pHeight, int *pRotation ) const
{ 
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );

  int ind = GetObjIndex( nFloor, objID );

  if ( -1 == ind )
    return false;
    
	const SObjRect &r = rects[Floor2Ind( nFloor)].placement[ind];
	*pPtCenter = r.ptCenter;
  *pRotation = r.rotation;
	*pWidth  = r.fWidth;
	*pHeight = r.fHeight;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Новое местоположение объекта с ID "objID"
// если объект с данным ID не найден, возвращается "false"
bool CPlacement::MoveRect( int nFloor, int objID, const CVec3 &ptCenter )
{ 
  int ind = GetObjIndex( nFloor, objID );

  if ( -1 == ind )
    return false;
	//
	SObjRect &r = rects[Floor2Ind( nFloor)].placement[ind];

	CVec3 pt3 = ptCenter - r.ptCenter;
	CVec2 ptOffset( pt3.x, pt3.y );
	CVec2 rect[4];
	int   i;
	for ( i = 0; i < 4; ++i )
		rect[i] = r.rect[i] + ptOffset;
	//
	CVec2 ptDesired, ptNearest;
	float width, height;
	GetBoundRect( rect, &ptDesired, &width, &height );
	if ( !GetNearestPos( &ptNearest, ptDesired, width, height ) )
    return false;
	ptOffset = ptNearest - ptDesired;
	for ( i = 0; i < 4; ++i )
		r.rect[i] = rect[i] + ptOffset;
	r.ptCenter = ptCenter;
	r.ptCenter.z = ptCenter.z;
	//
  return UpdateRects();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// В "prect" возвращает местоположение текущего объекта
// (указатель на текущий объект устанавливается ф-иями MoveFirst, MoveNext)
// если текущий объект не установлен возвращает "-1"
bool CPlacement::GetRect( CVec3 *pPtCenter, float *pWidth, float *pHeight, int *pRotation ) const
{
	int ind = Floor2Ind( iCurFloor );

  if ( iCurRect >= 0 && iCurRect < (int)rects[ind].placement.size() )
  {
		const SObjRect &r = rects[ind].placement[iCurRect];
		*pPtCenter = r.ptCenter;
    *pRotation = r.rotation;
		*pWidth  = r.fWidth;
		*pHeight = r.fHeight;		
    return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::DeleteRect()
{
  if ( iCurRect != -1 )
    DeleteRect( iCurFloor, rects[Floor2Ind( iCurFloor)].placement[iCurRect].rectID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::DeleteRect( int nFloor, int objID )
{
  int ind = GetObjIndex( nFloor, objID );

  if ( -1 == ind )
    return false;
	SFloorRects &fr = rects[Floor2Ind(nFloor)];
  if ( !fr.placement[ind].bNewRect )
    delObjs.push_back( objID );
  fr.placement.erase( fr.placement.begin() + ind );
  SetupRectID2IndMap( nFloor );
  MoveFirst( nFloor );
	return UpdateRects();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::SetupRectID2IndMap( int nFloor )
{
	SFloorRects &fr = rects[Floor2Ind(nFloor)];

  fr.objID2objIndex.clear();
  for ( int i=0; i < (int)fr.placement.size(); ++i )
    fr.objID2objIndex[fr.placement[i].rectID] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::SetupUnitID2IndMap( int nFloor )
{
	SFloorUnits &fu = units[Floor2Ind(nFloor)];
	
  fu.unitID2unitIndex.clear();
  for ( int i=0; i < (int)fu.units.size(); ++i )
    fu.unitID2unitIndex[fu.units[i]->nUnitID] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::SetupObjID2IndMap( int nFloor )
{
	SFloorObjs &objs = finobjs[Floor2Ind(nFloor)];
	
  objs.objID2objIndex.clear();
  for ( int i=0; i < (int)objs.objs.size(); ++i )
    objs.objID2objIndex[objs.objs[i]->nObjID] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::SetupExplosionID2IndMap( int nFloor )
{
	SFloorExplosions &objs = explosions[Floor2Ind(nFloor)];
	
  objs.objID2objIndex.clear();
  for ( int i=0; i < (int)objs.objs.size(); ++i )
    objs.objID2objIndex[objs.objs[i]->nObjID] = i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Хочется разместить прямоугольник WIDTH x HEIGHT, с левым нижним
// уголом по координате ptDesired
// Если прямоугольник помещается в расстановку, возвращается true
// и ближайщая к ptDesired координата угла записывается в pPtNearest
// Иначе возвр. false
// pPtNearest и ptDesired могут быть одной и той же переменной
bool CPlacement::GetNearestPos( POINT *pPtNearest, const POINT &ptDesired, int width, int height ) const
{
  int maxw = GetWidth();
  int maxh = GetHeight();
  if ( width > maxw || height > maxh )
    return false;

  if ( ptDesired.x < 0 )
    pPtNearest->x = 0;
  else if ( ptDesired.x + width > maxw )
    pPtNearest->x = maxw - width;
  else
    pPtNearest->x = ptDesired.x;

  if ( ptDesired.y < 0 )
    pPtNearest->y = 0;
  else if ( ptDesired.y + height > maxh )
    pPtNearest->y = maxh - height;
  else
    pPtNearest->y = ptDesired.y;

  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Хочется разместить прямоугольник WIDTH x HEIGHT, с центром по координате ptDesired
// Если прямоугольник помещается в расстановку, возвращается true
// и ближайщая к ptDesired координата угла записывается в pPtNearest
// Иначе возвр. false
// pPtNearest и ptDesired могут быть одной и той же переменной
bool CPlacement::GetNearestPos( CVec2 *pPtNearest, const CVec2 &ptDesired, float width, float height )  const
{
  int maxw = GetWidth();
  int maxh = GetHeight();
  if ( width > maxw || height > maxh )
	{
		pPtNearest->x = 0.5 * maxw;
		pPtNearest->y = 0.5 * maxh;
    return false;
	}
	
	CVec2 half( 0.5f * width, 0.5f * height );
  if ( ptDesired.x < half.x )
    pPtNearest->x = half.x;
  else if ( ptDesired.x + half.x > maxw )
    pPtNearest->x = maxw - half.x;
  else
    pPtNearest->x = ptDesired.x;
	
  if ( ptDesired.y < half.y )
    pPtNearest->y = half.y;
  else if ( ptDesired.y + half.y > maxh )
    pPtNearest->y = maxh - half.y;
  else
    pPtNearest->y = ptDesired.y;
	
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// true, если "rect" не выходит за пределы расстановки
bool CPlacement::CheckRect( const CVec2 rect[4] ) const
{
	for ( int i = 0; i < 4; ++i )
	{
		if ( rect[i].x < 0 || rect[i].x > GetWidth() || 
			   rect[i].y < 0 || rect[i].y > GetHeight() )
			return false;
	}
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Повернуть объект objID на угол angle
// Возвр. false, если повернуть нельзя
bool CPlacement::RotateRect( int nFloor, int objID, int angle )
{
  int ind = GetObjIndex( nFloor, objID );

	CVec2 rect[4];
	SObjRect &or = rects[Floor2Ind( nFloor ) ].placement[ind];
	MakeRect( rect, CVec2( or.ptCenter.x, or.ptCenter.y ), or.fWidth, or.fHeight, angle );

	CVec2 ptDesired, ptNearest;
	float width, height;
	GetBoundRect( rect, &ptDesired, &width, &height );
	if ( !GetNearestPos( &ptNearest, ptDesired, width, height ) )
		return false;
	CVec2 ptOffset = ptNearest - ptDesired;
	for ( int i = 0; i < 4; ++i )
		or.rect[i] = rect[i] + ptOffset;
	or.ptCenter += CVec3( ptOffset.x, ptOffset.y, 0 );
	or.rotation = angle;
	
  return UpdateRects();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к стенам для этажа nFloorID
// Если этажа для nFloorID не существует, то он создается
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
CWallsPlan* CPlacement::GetFloorWalls( int nFloor )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind < walls.size() )
  {
    if ( walls[ind] )
      return walls[ind];
    else
    {
      walls[ind] = dbgnew CWallsPlan( this, GetID(), nFloor );
      return walls[ind];
    }
  }
  walls.resize( ind + 1 );
  walls[ind] = dbgnew CWallsPlan( this, GetID(), nFloor );
  return walls[ind];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::HasWalls( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind < walls.size() && walls[ind] )
    return true;
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к стенам для этажа nFloorID
// Возвр. 0 если этажа для nFloorID не существует
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
const CWallsPlan* CPlacement::GetFloorWalls( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind >= walls.size() )
    return 0;
  return walls[ind];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsPtInRect( double x, double y, const RECT& rect )
{
  return x >= rect.left && x <= rect.right &&
		y >= rect.bottom && y <= rect.top;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsPtInRect( const POINT &pt, const RECT& rect )
{
  return IsPtInRect( pt.x, pt.y, rect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает индекс объекта, содержащего точку (x,y)
// если (x,y) не находится внутри какого либо объекта, возвр. "-1"
int CPlacement::GetTemplRectID( int nFloor, float x, float y ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	//
	const int n = rects[Floor2Ind( nFloor )].placement.size();
	vector<int> inside;
	int i;

  for ( i=0; i < n; ++i )
	{
		const SObjRect &pl = rects[Floor2Ind( nFloor )].placement[i];
		CVec2 pt1 = pl.rect[1] - pl.rect[0];
		CVec2 pt2 = pl.rect[2] - pl.rect[1];
		CVec2 pt3 = pl.rect[3] - pl.rect[2];
		CVec2 pt4 = pl.rect[0] - pl.rect[3];
		
		if ( pt1.x * (y - pl.rect[0].y) - pt1.y * (x - pl.rect[0].x) > 0
			&& pt2.x * (y - pl.rect[1].y) - pt2.y * (x - pl.rect[1].x) > 0
			&& pt3.x * (y - pl.rect[2].y) - pt3.y * (x - pl.rect[2].x) > 0
			&& pt4.x * (y - pl.rect[3].y) - pt4.y * (x - pl.rect[3].x) > 0
			)
			inside.push_back( i ); // pl.rectID
	}
	if ( inside.empty() )
		return -1;
	// если есть несколько прямоугольников, внутри которых точка,
	// то находим с самыми ближайшими углами
	int idMin = -1;
	float fMin = 1e10;
	CVec2 pt( x, y );
	for ( i = 0; i < inside.size(); ++i )
	{
		const SObjRect &pl = rects[Floor2Ind( nFloor )].placement[inside[i]];
		float fl = fabs2( pl.rect[0] - pt )
			+ fabs2( pl.rect[1] - pt )
			+ fabs2( pl.rect[2] - pt )
			+ fabs2( pl.rect[3] - pt );
		if ( fl < fMin )
		{
			idMin = pl.rectID;
			fMin = fl;
		}
	}
	return idMin;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Вычисление координат углов прямоугольника
void CPlacement::MakeRect( CVec2 rect[4], const CVec2 &ptCenter, float fWidth, float fHeight, int nRotation )
{
	CVec2 ptCorners[] = 
	{
		CVec2( -0.5f * fWidth, -0.5f * fHeight ),
		CVec2(  0.5f * fWidth, -0.5f * fHeight ),
		CVec2(  0.5f * fWidth,  0.5f * fHeight ),
		CVec2( -0.5f * fWidth,  0.5f * fHeight ),
	};
	const float fAng = -ToRadian( (float)nRotation );
	const float fc = cos( fAng );
	const float fs = sin( fAng );

	for ( int i = 0; i < 4; ++i )
	{		
		rect[i].x = ptCenter.x + fc * ptCorners[i].x + fs * ptCorners[i].y;
		rect[i].y = ptCenter.y - fs * ptCorners[i].x + fc * ptCorners[i].y;
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает bound rect для развернутого произвольным образом прямоугольника rect
void CPlacement::GetBoundRect( const CVec2 rect[4], CVec2 *pPtCenter, float *pWidth, float *pHeight )
{
	float minx = rect[0].x;
	float maxx = rect[0].x;
	float miny = rect[0].y;
	float maxy = rect[0].y;
	//
	for ( int i = 1; i < 4; ++i )
	{
		minx = Min( minx, rect[i].x );
		maxx = Max( maxx, rect[i].x );
		miny = Min( miny, rect[i].y );
		maxy = Max( maxy, rect[i].y );
	}
	pPtCenter->x = 0.5f * (minx + maxx);
	pPtCenter->y = 0.5f * (miny + maxy);
	*pWidth  = maxx - minx;
	*pHeight = maxy - miny;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к полам для этажа nFloorID
// Если этажа для nFloorID не существует, то он создается
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
CFloorPlan* CPlacement::GetFloor( int nFloor, int nLayer, vector< vector< CPtr<CFloorPlan> > > *pFloors, EFloorType type )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR && nLayer >= 0 );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind >= pFloors->size() )
		pFloors->resize( ind +1 );
  if ( nLayer < (*pFloors)[ind].size() )
	{
    return (*pFloors)[ind][nLayer];
	}
  else
  {
		vector< CPtr<CFloorPlan> > &layers = (*pFloors)[ind];
		int nOldSize = layers.size();
		layers.resize( nLayer + 1 );
		for ( int i = nOldSize; i < layers.size(); ++i )
			layers[i] = new CFloorPlan( this, nFloor, type, nLayer );
    return layers[nLayer];
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::HasFloor( int nFloor, int nLayer, const vector<vector<CPtr<CFloorPlan> > > &floors ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind < floors.size() && nLayer < floors[ind].size() )
    return true;
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к полам для этажа nFloorID
// Возвр. 0 если этажа для nFloorID не существует
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
const CFloorPlan* CPlacement::GetFloor( int nFloor, int nLayer, const vector<vector<CPtr<CFloorPlan> > > &floors ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  const int ind = Floor2Ind( nFloor );
  //
  if ( ind >= floors.size() || nLayer >= floors[ind].size() )
    return 0;
  return floors[ind][nLayer];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadFloors()
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo = NGScene::shareBuildings.Get( GetID() );
	pBuildInfo.Refresh();
	const NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return true;
	for( int i = 0; i < pInfo->solidFragments.size(); ++i )
	{
		EFloorType type;
		int nLayer;
		NBuilding::GetLayerID( pInfo->solidFragments[i].nFragmentID, (ELayer*)&type, &nLayer );
		// создаем все присутсвующие в инфе слои
		volatile CFloorPlan *pPlan = GetFloor( type, pInfo->solidFragments[i].ptPos.z, nLayer );
		for ( int j = MIN_FLOOR; j < MAX_FLOOR; ++j )
			pPlan = GetFloor( type, j, nLayer );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadRects()
{
	string szQuery = string( "SELECT * FROM " ) + RECTS_TBL + " WHERE VariantID = " + IToA( GetID() );
	
	HRESULT hr = rectsDB.Open( szQuery );
	if ( FAILED(hr) )
		return false;
	//
	while ( S_OK == rectsDB.MoveNext() ) 
	{
		PushRect( rectsDB.m_Floor, rectsDB.m_RectID,
			CVec3( rectsDB.m_CenterX, rectsDB.m_CenterY, rectsDB.m_fDZ ),
			rectsDB.m_Width,
			rectsDB.m_Height,
			rectsDB.m_TemplateLink, 
			rectsDB.m_Rotation );
    iMaxRectID = Max( iMaxRectID, (int)rectsDB.m_RectID );		
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadUnits()
{
	SUnit tmpUnit;
	string szQuery = string( UNITS_TBL ) + " WHERE VariantID=" + IToA( GetID() );
	
  if ( FAILED( finDB.Open( szQuery, &tmpUnit.props ) ) )
    return false;
	//
  while ( finDB.MoveNext() == S_OK )
  {
		SUnit *pUnit = new SUnit;
    int nVarID, nUnitID;
    finDB.GetElement( &nUnitID, &nVarID );
		ASSERT( nVarID == GetID() );
		finDB.ReadProps( &pUnit->props );
		//
		pUnit->nUnitID = nUnitID;
		SFloorUnits* pUnits = GetUnits( pUnit->props["Floor"]->GetValue() );
		if ( !pUnits )
			return false;
		pUnits->unitID2unitIndex[pUnit->nUnitID] = pUnits->units.size();
		pUnits->units.push_back( pUnit );
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::GetModelSize( CVec2 *pptSize, CVec2 *pptCenter, int nObjectID, 
															CItemsMgr *pOItems, CItemsMgr *pMItems, CItemsMgr *pGItems, CItemsMgr *pCItems )
{
	const CPropMap *pOProps = pOItems->GetPropList( nObjectID );
	if ( !pOProps )
				return;
	CPropMap::const_iterator cit = pOProps->find( "Model0" );
	if ( pOProps->end() == cit )
				return;
	const int nConID = cit->second->GetValue();
	const CPropMap *pCProps = pCItems->GetPropList( nConID );
	if ( !pCProps )
				return;
	CPropMap::const_iterator mit = pCProps->find( "ModelID" );
	if ( pCProps->end() == mit )
				return;
	const int nModelID = mit->second->GetValue();
	const CPropMap *pMProps = pMItems->GetPropList( nModelID );
	if ( !pMProps )
				return;
	CPropMap::const_iterator git = pMProps->find( "GeometryID" );
	if ( pMProps->end() == git )
				return;
	const CPropMap *pGProps = pGItems->GetPropList( git->second->GetValue() );
	if ( !pGProps )
				return;
	CPropMap::const_iterator xit = pGProps->find( "SizeX" );
	CPropMap::const_iterator yit = pGProps->find( "SizeY" );
	CPropMap::const_iterator cxit = pGProps->find( "CenterX" );
	CPropMap::const_iterator cyit = pGProps->find( "CenterY" );
	CPropMap::const_iterator e = pGProps->end();
	if ( e == xit || e == yit || e == cxit || e == cyit )
		return;
	pptSize->x = float( xit->second->GetValue() ) / FP_GRID_STEP;
	pptSize->y = float( yit->second->GetValue() ) / FP_GRID_STEP;
	pptCenter->x = float( cxit->second->GetValue() ) / FP_GRID_STEP;
	pptCenter->y = float( cyit->second->GetValue() ) / FP_GRID_STEP;
	pMItems->ReleasePropList( pMProps );
	pGItems->ReleasePropList( pGProps );
	pOItems->ReleasePropList( pOProps );
	pCItems->ReleasePropList( pCProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadFinObjs()
{
	CPtr<SFinElement> pTmpObj = new SFinElement;
	string szQuery = string( FINELEMS_TBL ) + " WHERE VariantID=" + IToA( GetID() );
	
  if ( FAILED( finDB.Open( szQuery, &pTmpObj->props ) ) )
    return false;
	const SResTree *pObjsTree = theApp.GetResTree( IDC_OBJECTS_TREE );
	const SResTree *pConTree = theApp.GetResTree( IDC_CONTAINERS_TREE );
	const SResTree *pModelTree = theApp.GetResTree( IDC_MODELS_TREE );
	const SResTree *pGeomTree = theApp.GetResTree( IDC_GEOMETRIES_TREE );
	if ( !pObjsTree || !pModelTree || !pGeomTree || !pConTree )
		return false;
	CItemsMgr *pOItems = pObjsTree->pItemsTree;
	CItemsMgr *pMItems = pModelTree->pItemsTree;
	CItemsMgr *pGItems = pGeomTree->pItemsTree;
	//
  while ( finDB.MoveNext() == S_OK )
  {
		CPtr<SFinElement> pObj = new SFinElement;
    int nVarID, nObjID;
    finDB.GetElement( &nObjID, &nVarID );
		ASSERT( nVarID == GetID() );
		finDB.ReadProps( &pObj->props );
		// item bound box
		GetModelSize( const_cast<CVec2*>( &pObj->ptSize ), const_cast<CVec2*>( &pObj->ptCenter ), 
			pObj->props["ModelID"]->GetValue(), pOItems, pMItems, pGItems, pConTree->pItemsTree );
		//
		pObj->nObjID = nObjID;
		SFloorObjs* pObjs = GetFinObjs( pObj->props["Floor"]->GetValue() );
		if ( !pObjs )
			return false;
		pObjs->objID2objIndex[pObj->nObjID] = pObjs->objs.size();
		pObjs->objs.push_back( pObj );
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadExplosions()
{
	CPtr<SExplosion> pTmpObj = new SExplosion;
	string szQuery = string( EXPLOSIONS_TBL ) + " WHERE VariantID=" + IToA( GetID() );
	
  if ( FAILED( finDB.Open( szQuery, &pTmpObj->props ) ) )
    return false;
	//
  while ( finDB.MoveNext() == S_OK )
  {
		SExplosion *pObj = new SExplosion;
    int nVarID, nObjID;
    finDB.GetElement( &nObjID, &nVarID );
		ASSERT( nVarID == GetID() );
		finDB.ReadProps( &pObj->props );
		//
		pObj->nObjID = nObjID;
		SFloorExplosions* pObjs = GetExplosions( pObj->props["Floor"]->GetValue() );
		if ( !pObjs )
			return false;
		pObjs->objID2objIndex[pObj->nObjID] = pObjs->objs.size();
		pObjs->objs.push_back( pObj );
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::LoadRooms()
{
	string szQuery = string( "SELECT * FROM " ) + ROOMPARAMS_TBL + " WHERE VariantID=" + IToA( GetID() );
	
  if ( FAILED( roomsDB.Open( szQuery ) ) )
    return false;
	//
  while ( roomsDB.MoveNext() == S_OK )
  {
		SRoom room;

		room.nRoomID = roomsDB.m_nRoomID;
		room.dwColor = roomsDB.m_nUserColor;

		roomIDs[Floor2Ind( roomsDB.m_nFloor )].push_back( roomsDB.m_nID );
		roomParams[roomsDB.m_nID] = room;
  }
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo = NGScene::shareBuildings.Get( GetID() );
	pBuildInfo.Refresh();
	const NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return true;
	for( int i = 0; i < pInfo->roomMap.size(); ++i )
	{
		bool bExist = false;
		const CArray2D<BYTE> r = pInfo->roomMap[i];
		for ( int j = 0; j < r.GetYSize() && !bExist; ++j )
			for ( int k = 0; k < r.GetXSize(); ++k )
				if ( r[j][k] > 0 )
				{
					bExist = true;
					break;
				}
		if ( bExist )
		{
			volatile CFloorPlan *pPlan = GetFloor( FT_ROOM, pInfo->nMinFloor + i, 0 );
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к прямоугольникам для этажа nFloorID
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
inline CPlacement::SFloorRects* CPlacement::GetRects( int nFloor )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &rects[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CPlacement::SFloorRects* CPlacement::GetRects( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &rects[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFinProp::CFinProp( const string &szName, int nID, int nType, int nViewType, CVariant _defValue, bool bReadOnly )
: CProp( szName, nID, nType, nViewType, bReadOnly ), defValue( _defValue )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFinProp::SetValue( const CVariant &newValue, bool bModified ) const
{
  *(const_cast<CVariant*>(&value)) = newValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CProp* CFinProp::Clone() const
{
	CFinProp *pClone = new CFinProp( GetName(), GetID(), GetType(), GetViewType(), defValue );
	pClone->value = value;
	return pClone;		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EMapObjType CPlacement::GetObjID( int *pID, int nFloor, float x, float y, float fRadius ) const
{
	int id = GetUnitID( nFloor, x, y, fRadius );
	if ( -1 != id )
	{
		*pID = id;
		return MO_UNIT;
	}
	id = GetObjID( nFloor, x, y, fRadius );
	if ( -1 != id )
	{
		*pID = id;
		return MO_OBJECT;
	}
	id = GetExplosionID( nFloor, x, y, fRadius );
	if ( -1 != id )
	{
		*pID = id;
		return MO_EXPLOSION;
	}
	id = GetTemplRectID( nFloor, x, y );
	if ( -1 != id )
	{
		*pID = id;
		return MO_TEMPLATE;
	}
	return MO_EMPTY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//					UNITS
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к юнитам для этажа nFloorID
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
inline CPlacement::SFloorUnits* CPlacement::GetUnits( int nFloor )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &units[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CPlacement::SFloorUnits* CPlacement::GetUnits( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &units[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сброс указателя на текущий юнит
// для установки на первый юнит нужно вызвать MoveNextUnit()
void CPlacement::MoveFirstUnit( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	iCurFloorUnit = nFloor;
	iCurUnit = -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущий юнит в следующую позицию
// возвращает "false", если достигнут конец списка
bool CPlacement::MoveNextUnit()
{
  if ( iCurUnit >= (int)units[Floor2Ind( iCurFloorUnit )].units.size() - 1 )
    return false;
  ++iCurUnit;
  return true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID текущего юнита
// "-1" если указатель на юнит не установлен
int CPlacement::GetUnitID() const
{
	int ind = Floor2Ind( iCurFloorUnit );
  if ( iCurUnit >= 0 && iCurUnit < (int)units[ind].units.size() )
    return units[ind].units[iCurUnit]->nUnitID;
  return -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает местоположение юнита на этаже nFloor c ID unitID
// если юнит с данным ID не найден, возвращается "false"
bool CPlacement::GetUnitPos( int nFloor, int unitID, CTPoint<int> *pPos, int *pRotation ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	int ind = GetUnitIndex( nFloor, unitID );
	
  if ( -1 == ind )
    return false;
	SUnit *pUnit = units[Floor2Ind( nFloor )].units[ind];
	pPos->x = pUnit->props["PosX"]->GetValue();
	pPos->y = pUnit->props["PosY"]->GetValue();
	*pRotation = pUnit->props["Rotation"]->GetValue();;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ссылку на RPG pers юнита
// если юнит с данным ID не найден, возвращается -1
int CPlacement::GetUnitMonsterID( int nFloor, int unitID )  const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetUnitIndex( nFloor, unitID );
	
  if ( -1 == ind )
    return -1;
	return units[Floor2Ind( nFloor )].units[ind]->props["MonsterID"]->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвр. ID юнита, который попадает в круг с центром x,y и радиусом fRadius
// если такого юнита нет, возвр -1
int CPlacement::GetUnitID( int nFloor, float x, float y, float fRadius ) const
{
	const SFloorUnits *pUnits = GetUnits( nFloor );
	if ( !pUnits )
		return -1;
	//
	const int n = pUnits->units.size();
	float r2 = sqr( fRadius );
	for ( int i = 0; i < n; ++i )
	{
		int nx = pUnits->units[i]->props["PosX"]->GetValue();
		int ny = pUnits->units[i]->props["PosY"]->GetValue();

		if ( sqr( x - nx ) + sqr( y - ny ) < r2 )
			return pUnits->units[i]->nUnitID;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::MoveUnit( int nFloor, int unitID, const CTPoint<int> &pt )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetUnitIndex( nFloor, unitID );
	
	if ( -1 == ind )
		return false;
	SUnit *pUnit = units[Floor2Ind( nFloor )].units[ind];
	pUnit->props["PosX"]->SetValue( pt.x, false );
	pUnit->props["PosY"]->SetValue( pt.y, false );
	return UpdateItem( UNITS_TBL, pUnit->nUnitID, &pUnit->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::RotateUnit( int nFloor, int unitID, int nAngle )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	//
	int ind = GetUnitIndex( nFloor, unitID );
	
	if ( -1 == ind )
		return false;
	SUnit *pUnit = units[Floor2Ind( nFloor )].units[ind];
	pUnit->props["Rotation"]->SetValue( nAngle, false );
	return UpdateItem( UNITS_TBL, pUnit->nUnitID, &pUnit->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление нового юнита в базу данных
// возвращается ID добавленного юнита или -1 если ошибка
int CPlacement::AddUnit( int nFloor, const CTPoint<int> &pt, int nMonsterID )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	//
	SUnit defUnit;
	int nUnitID = finDB.Insert( UNITS_TBL, GetID(), &defUnit.props );
	if ( -1 == nUnitID )
		return -1;
	//
	SUnit *pUnit = new SUnit;
	pUnit->nUnitID = nUnitID;
	pUnit->props["Floor"]->SetValue( nFloor, false );
	pUnit->props["MonsterID"]->SetValue( nMonsterID, false );
	pUnit->props["Rotation"]->SetValue( 0, false );
	pUnit->props["PosX"]->SetValue( pt.x, false );
	pUnit->props["PosY"]->SetValue( pt.y, false );
	if ( !UpdateItem( UNITS_TBL, nUnitID, &pUnit->props ) )
	{
		delete pUnit;
		return -1;
	}
	//
	SFloorUnits *pUnits = GetUnits( nFloor );
	pUnits->unitID2unitIndex[pUnit->nUnitID] = pUnits->units.size();
	pUnits->units.push_back( pUnit );
	return pUnit->nUnitID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::DeleteUnit( int nFloor, int unitID )
{
	if ( FAILED( finDB.Open( UNITS_TBL, unitID ) ) )
    return false;	

  int ind = GetUnitIndex( nFloor, unitID );
	
  if ( -1 == ind )
    return false;
	SFloorUnits &fu = units[Floor2Ind(nFloor)];
	delete fu.units[ind];
  fu.units.erase( fu.units.begin() + ind );
  SetupUnitID2IndMap( nFloor );
  MoveFirstUnit( nFloor );
	
	return finDB.DeleteElement();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Обновление информации в базе данных
inline bool CPlacement::UpdateItem( const string &szTable, int nID, const CPropMap *pProps )
{
	if ( FAILED( finDB.Open( szTable, nID, pProps ) ) )
		return false;
	return finDB.UpdateElement( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//					FINAL ELEMENTS
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить указатель на интерфейс к юнитам для этажа nFloorID
// nFloorID должен лежать в диапазоне MIN_FLOOR - MAX_FLOOR
inline CPlacement::SFloorObjs* CPlacement::GetFinObjs( int nFloor )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &finobjs[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CPlacement::SFloorObjs* CPlacement::GetFinObjs( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &finobjs[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сброс указателя на текущий fin obj
// для установки на первый obj нужно вызвать MoveNextObj()
void CPlacement::MoveFirstObj( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	iCurFloorObj = nFloor;
	iCurObj = -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущий fin obj в следующую позицию
// возвращает "false", если достигнут конец списка
bool CPlacement::MoveNextObj()
{
  if ( iCurObj >= (int)finobjs[Floor2Ind( iCurFloorObj )].objs.size() - 1 )
    return false;
  ++iCurObj;
  return true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID текущего fin obj
// "-1" если указатель на fin obj не установлен
int CPlacement::GetObjID() const
{
	int ind = Floor2Ind( iCurFloorObj );
  if ( iCurObj >= 0 && iCurObj < (int)finobjs[ind].objs.size() )
    return finobjs[ind].objs[iCurObj]->nObjID;
  return -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает местоположение fin obj на этаже nFloor c ID objID
// если obj с данным ID не найден, возвращается "false"
bool CPlacement::GetObjPos( int nFloor, int objID, CVec3 *pPos, int *pRotation ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	int ind = GetFinObjIndex( nFloor, objID );
	
  if ( -1 == ind )
    return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pPos->x = pObj->props["PosX"]->GetValue();
	pPos->y = pObj->props["PosY"]->GetValue();
	pPos->z = pObj->props["DeltaZ"]->GetValue();
	*pRotation = pObj->props["Rotation"]->GetValue();
	/*
	//учет bound box'a
	CVec2 pt = pObj->ptCenter;
	RotatePt( &pt, *pRotation );
	pPos->x += pt.x;
	pPos->y += pt.y;
	*/
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ссылку на Model объекта
// если obj с данным ID не найден, возвращается -1
int CPlacement::GetObjModelID( int nFloor, int objID )  const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetFinObjIndex( nFloor, objID );
	
  if ( -1 == ind )
    return -1;
	return finobjs[Floor2Ind( nFloor )].objs[ind]->props["ModelID"]->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвр. ID fin obj, который попадает в круг с центром x,y и радиусом fRadius
// если такого obj нет, возвр -1
int CPlacement::GetObjID( int nFloor, float x, float y, float fRadius ) const
{
	const SFloorObjs *pObjs = GetFinObjs( nFloor );
	if ( !pObjs )
		return -1;
	//
	const int n = pObjs->objs.size();
	float r2 = sqr( fRadius );
	for ( int i = 0; i < n; ++i )
	{
		float fx = pObjs->objs[i]->props["PosX"]->GetValue();
		float fy = pObjs->objs[i]->props["PosY"]->GetValue();

		if ( sqr( x - fx ) + sqr( y - fy ) < r2 )
			return pObjs->objs[i]->nObjID;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::MoveObj( int nFloor, int objID, const CVec3 &pt )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetFinObjIndex( nFloor, objID );
	
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pObj->props["PosX"]->SetValue( pt.x, false );
	pObj->props["PosY"]->SetValue( pt.y, false );
	pObj->props["DeltaZ"]->SetValue( pt.z, false );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::RotateObj( int nFloor, int objID, int nAngle )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	//
	int ind = GetFinObjIndex( nFloor, objID );
	
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pObj->props["Rotation"]->SetValue( nAngle, false );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Добавление нового fin obj в базу данных
// возвращается ID добавленного fin obj или -1 если ошибка
int CPlacement::AddObj( int nFloor, const CVec2 &pt, int nTreeID, int nModelID )
{
	static CPlaceDB placeDB;
	int nPlaceID = -1;
	switch ( nTreeID )
	{
		case IDC_OBJECTS_TREE:
			nPlaceID = placeDB.GetPlaceID( "ObjectTemplates", nModelID );
			break;
		case IDC_RPG_ITEMS_TREE:
			nPlaceID = placeDB.GetPlaceID( "RPGItems", nModelID );
			break;
	}
	CPtr<SFinElement> pObj;
	int nID = AddPlaceObj( nFloor, pt, nPlaceID, &pObj );
	//
	if ( IDC_OBJECTS_TREE == nTreeID )
	{
		const SResTree *pOTree = theApp.GetResTree( IDC_OBJECTS_TREE );
		const SResTree *pMTree = theApp.GetResTree( IDC_MODELS_TREE );
		const SResTree *pGTree = theApp.GetResTree( IDC_GEOMETRIES_TREE );
		const SResTree *pCTree = theApp.GetResTree( IDC_CONTAINERS_TREE );
		if ( pMTree && pGTree )
			GetModelSize( const_cast<CVec2*>( &pObj->ptSize ), const_cast<CVec2*>( &pObj->ptCenter ), 
					nModelID, pOTree->pItemsTree, pMTree->pItemsTree, pGTree->pItemsTree, pCTree->pItemsTree );
	}
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::AddPlaceObj( int nFloor, const CVec2 &pt, int nPlaceID, CPtr<SFinElement> *pRetObj )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	//
	CPtr<SFinElement> pDefObj = new SFinElement;
	int nObjID = finDB.Insert( FINELEMS_TBL, GetID(), &pDefObj->props );
	if ( -1 == nObjID )
		return -1;
	//
	CPtr<SFinElement> pObj = new SFinElement;
	pObj->nObjID = nObjID;
	pObj->props["Floor"]->SetValue( nFloor, false );
	pObj->props["ModelID"]->SetValue( nPlaceID, false );
	pObj->props["Rotation"]->SetValue( 0, false );
	pObj->props["PosX"]->SetValue( pt.x, false );
	pObj->props["PosY"]->SetValue( pt.y, false );
	pObj->props["DeltaZ"]->SetValue( 0, false );
	pObj->props["ScaleX"]->SetValue( 1, false );
	pObj->props["ScaleY"]->SetValue( 1, false );
	pObj->props["ScaleZ"]->SetValue( 1, false );
	if ( pRetObj )
		*pRetObj = pObj;
	if ( !UpdateItem( FINELEMS_TBL, nObjID, &pObj->props ) )
		return -1;
	//
	SFloorObjs *pObjs = GetFinObjs( nFloor );
	pObjs->objID2objIndex[pObj->nObjID] = pObjs->objs.size();
	pObjs->objs.push_back( pObj );
	return pObj->nObjID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::DeleteObj( int nFloor, int objID )
{
	if ( FAILED( finDB.Open( FINELEMS_TBL, objID ) ) )
    return false;	

  int ind = GetFinObjIndex( nFloor, objID );
	
  if ( -1 == ind )
    return false;
	SFloorObjs &fo = finobjs[Floor2Ind(nFloor)];
  fo.objs.erase( fo.objs.begin() + ind );
  SetupObjID2IndMap( nFloor );
  MoveFirstObj( nFloor );
	
	return finDB.DeleteElement();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::MoveObj( int nFloor, EMapObjType objType, int objID, const CVec3 &ptCenter )
{
	switch ( objType )
	{
		case MO_TEMPLATE:
			return MoveRect( nFloor, objID, ptCenter );
		case MO_UNIT:
			return MoveUnit( nFloor, objID, CTPoint<int>( ptCenter.x + 0.5f, ptCenter.y + 0.5f ) );
		case MO_OBJECT:
			return MoveObj( nFloor, objID, ptCenter );
		case MO_EXPLOSION:
			return MoveExplosion( nFloor, objID, ptCenter );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::RotateObj( int nFloor, EMapObjType objType, int objID, int angle )
{
	switch ( objType )
	{
		case MO_TEMPLATE:
			return RotateRect( nFloor, objID, angle );
		case  MO_UNIT:
			return RotateUnit( nFloor, objID, angle );
		case  MO_OBJECT:
			return RotateObj( nFloor, objID, angle );
		case  MO_EXPLOSION:
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::GetObjPos( int nFloor, EMapObjType objType, int objID, CVec3 *pPos, int *pAngle ) const
{
	switch ( objType )
	{
	case MO_TEMPLATE:
		{
			float w, h;
			if ( !GetRect( nFloor, objID, pPos, &w, &h, pAngle ) )
				return false;
			return true;
		}
	case  MO_UNIT:
		{
			CTPoint<int> pt;
			GetUnitPos( nFloor, objID, &pt, pAngle );
			pPos->x = pt.x;
			pPos->y = pt.y;
			pPos->z = 0;
			return true;
		}
	case  MO_OBJECT:
		return GetObjPos( nFloor, objID, pPos, pAngle );
	case  MO_EXPLOSION:
		return GetExplosionPos( nFloor, objID, pPos, pAngle );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::DeleteObj( int nFloor, EMapObjType objType, int objID )
{
	switch ( objType )
	{
		case MO_TEMPLATE:
			return DeleteRect( nFloor, objID );
		case  MO_UNIT:
			return DeleteUnit( nFloor, objID );
		case  MO_OBJECT:
			return DeleteObj( nFloor, objID );
		case  MO_EXPLOSION:
			return DeleteExplosion( nFloor, objID );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает размер и центр BBOX'а для объекта
bool CPlacement::GetObjectSize( int nFloor, EMapObjType objType, int objID, CVec2 *pptSize, CVec2 *pptCenter )
{
	static CVec2 ptFake;
	int nRot;
	if ( !pptSize )	pptSize = &ptFake;
	if ( !pptCenter )	pptCenter = &ptFake;
	*pptSize = VNULL2;
	*pptCenter = VNULL2;
	//
	switch ( objType )
	{
		case MO_TEMPLATE:
			{
				CVec3 ptCenter;
				if ( !GetRect( nFloor, objID, &ptCenter, &pptSize->x, &pptSize->y, &nRot ) )
					return false;
				pptCenter->x = 0;
				pptCenter->y = 0;
				return true;
			}
		case MO_OBJECT:
			{
				int nInd = GetFinObjIndex( nFloor, objID );
				if ( -1 == nInd )
					return false;
				const SFinElement *pE = GetFinObjs( nFloor )->objs[nInd];
				if ( !pE )
					break;
				*pptSize = pE->ptSize;
				*pptCenter = pE->ptCenter;
			}
			return true;
		case MO_EXPLOSION:
		{
			CVec3 pos;
			if ( !GetExplosionPos( nFloor, objID, &pos, &nRot ) )
				return false;
			pptCenter->x = 0;
			pptCenter->y = 0;
		}
		case MO_UNIT:
		{
			CTPoint<int> pos;
			if ( !GetUnitPos( nFloor, objID, &pos, &nRot ) )
				return false;
			pptCenter->x = 0;
			pptCenter->y = 0;
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CPropMap* CPlacement::GetObjectProps( int nFloor, EMapObjType objType, int objID )
{
	switch ( objType )
	{
		case MO_OBJECT:
			{
				int nInd = GetFinObjIndex( nFloor, objID );
				if ( -1 == nInd )
					return false;
				const SFinElement *pE = GetFinObjs( nFloor )->objs[nInd];
				if ( !pE )
					break;
				return &pE->props;
			}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::HasFloor( EFloorType type, int nFloor, int nLayer ) const
{
	switch ( type )
	{
		case FT_FLOOR:
			return HasFloor( nFloor, nLayer, floors );
		case FT_FLOOR_INTERMEDIATE:
			return HasFloor( nFloor, nLayer, floorsInterm );
		case FT_SOLID_:
			return HasFloor( nFloor, nLayer, solids );
		case FT_SOLID_INTERMEDIATE:
			return HasFloor( nFloor, nLayer, solidsInterm );
		case FT_ROOM:
			return HasFloor( nFloor, nLayer, rooms );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFloorPlan* CPlacement::GetFloor( EFloorType type, int nFloor, int nLayer )
{
	switch ( type )
	{
		case FT_FLOOR:
			return GetFloor( nFloor, nLayer, &floors, type );
		case FT_FLOOR_INTERMEDIATE:
			return GetFloor( nFloor, nLayer, &floorsInterm, type );
		case FT_SOLID_:
			return GetFloor( nFloor, nLayer, &solids, type );
		case FT_SOLID_INTERMEDIATE:
			return GetFloor( nFloor, nLayer, &solidsInterm, type );
		case FT_ROOM:
			return GetFloor( nFloor, nLayer, &rooms, type );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CFloorPlan* CPlacement::GetFloor( EFloorType type, int nFloor, int nLayer ) const
{
	switch ( type )
	{
		case FT_FLOOR:
			return GetFloor( nFloor, nLayer, floors );
		case FT_FLOOR_INTERMEDIATE:
			return GetFloor( nFloor, nLayer, floorsInterm );
		case FT_SOLID_:
			return GetFloor( nFloor, nLayer, solids );
		case FT_SOLID_INTERMEDIATE:
			return GetFloor( nFloor, nLayer, solidsInterm );
		case FT_ROOM:
			return GetFloor( nFloor, nLayer, rooms );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int GetFloorLayersNum( const vector<vector< CPtr<CFloorPlan> > > &floors )
{
	int nLayers = 0;
	for ( int i = 0; i < floors.size(); ++i )
		nLayers = Max( int( floors[i].size() ), nLayers );
	return nLayers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::GetFloorLayersNum( EFloorType type ) const
{
	switch ( type )
	{
		case FT_FLOOR:
			return ::GetFloorLayersNum( floors );
		case FT_FLOOR_INTERMEDIATE:
			return ::GetFloorLayersNum( floorsInterm );
		case FT_SOLID_:
			return ::GetFloorLayersNum( solids );
		case FT_SOLID_INTERMEDIATE:
			return ::GetFloorLayersNum( solidsInterm );
		case FT_ROOM:
			return ::GetFloorLayersNum( rooms );
	}
	return 0;
}
/*
////////////////////////////////////////////////////////////////////////////////////////////////////
// Считывание и запись карты текстур
void CPlacement::SerializeTerrain( CStructureSaver *pFile )
{
	pFile->Add( 21, &info );
	pFile->Add( 6, &alphaMap );
	pFile->Add( 10, &fMinH );
	pFile->Add( 11, &fMaxH );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::ReadTerrain()
{
	try
	{
		CFileStream fp;
		fp.OpenRead( szTerrName.c_str() );
		{
			CStructureSaver file( fp, CStructureSaver::READ );
			SerializeTerrain( &file );
		}
	}
	catch ( ... )
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::WriteTerrain()
{
	try
	{
		CFileStream fp;
		fp.OpenWrite( szTerrName.c_str() );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			SerializeTerrain( &file );
		}
	}
	catch ( ... )
	{
		return false;
	}
	return true;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CMETerrainInfo* CPlacement::GetTerrainInfo()
{
	pTerrLoader.Refresh();
	return pTerrLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
const CArray2D<BYTE>* CPlacement::GetTerrTexMap()
{	
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( pTerr )
		return &pTerr->info.typeMap;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CArray2D<WORD>* CPlacement::GetTerrHMap( float *pMinH, float *pMaxH )
{	
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( !pTerr )
		return 0;
	*pMaxH = pTerr->fMaxH;
	*pMinH = pTerr->fMinH;
	return &pTerr->info.heightMap;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::Save() const
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	pLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	if ( !pbinfo )
		pbinfo = new NBuilding::CBuildInfo;
	// ROOMS
	CTPoint<int> rsize( pbinfo->nMaxX + 2, pbinfo->nMaxY + 2 );  // добавляем по краям полоску в 1 тайл
	for ( int i = pbinfo->nMinFloor, j = 0; i < pbinfo->nMaxFloor + 1; ++i, ++j )
	{
		if ( j >= pbinfo->roomMap.size() )
			break;
		CArray2D<BYTE> &rooms = pbinfo->roomMap[j];
		if ( rooms.GetXSize() < rsize.x || rooms.GetYSize() < rsize.y )
			rooms.SetSizes( rsize.x, rsize.y );
		rooms.FillZero();
		const CFloorPlan *pRooms = GetFloor( FT_ROOM, i, 0 );
		if ( !pRooms )
			continue;
		const CFloorPlan::CCellInfo* pcells = pRooms->GetFloor();
		for ( CFloorPlan::CCellInfo::const_iterator it = pcells->begin(); it != pcells->end(); ++it )
		{
			for ( int k = 0; k < it->second.size(); ++k )
			{
				const CFloorPlan::SCell &cell = it->second[k];
				if ( cell.x >= 0)
					rooms[cell.y + 1][cell.x + 1] = it->first;
			}
		}
	}

//	MakeBuildingInfo( this, pbinfo );
	pLoader->Updated();
	return SerializeBuilding( pbinfo, GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
bool CPlacement::SetTerrMap( const CArray2D<BYTE> *pTmap, 
								float _fMinH, float _fMaxH, const CArray2D<WORD> *pHmap, const vector<STerrainHole> *pHoles, 
								const CArray2D<WORD> *pAmap, const vector<SGrassLayer> *pGrass, const CArray2D<DWORD> *pColormap )
{
	info.nWidth = nWidth;
	info.nHeight = nHeight;
	int nw = nWidth + 1;
	int nh = nHeight + 1;
	if ( pTmap && nw == pTmap->GetXSize() && nh == pTmap->GetYSize() )
		info.typeMap = *pTmap;
	if ( pHmap && nw == pHmap->GetXSize() && nh == pHmap->GetYSize() )
	{
		fMinH = _fMinH;
		fMaxH = _fMaxH;
		info.heightMap = *pHmap;
	}
	if ( pAmap && nw == pAmap->GetXSize() && nh == pAmap->GetYSize() )
	{
		alphaMap = *pAmap;
	}
	if ( pColormap && nw == pColormap->GetXSize() && nh == pColormap->GetYSize() )
	{
		info.color = *pColormap;
	}

	if ( pHoles )
		info.holes = *pHoles;
	if ( pGrass )
		info.grass = *pGrass;

	return WriteTerrain();
}
*/
bool CPlacement::SerializeTerrain()
{
	pTerrLoader->Updated();
	return ::SerializeTerrain( GetTerrainInfo(), GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
const CArray2D<WORD>* CPlacement::GetTerrAMap()
{
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( pTerr )
		return &pTerr->alphaMap;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const vector<STerrainHole>* CPlacement::GetTerrHoles()
{	
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( pTerr )
		return &pTerr->info.holes;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const vector<SGrassLayer>* CPlacement::GetTerrGrass()
{
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( pTerr )
		return &pTerr->info.grass;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CArray2D<DWORD>* CPlacement::GetTerrColormap()
{	
	CMETerrainInfo *pTerr = GetTerrainInfo();
	if ( pTerr )
		return &pTerr->info.color;
	return 0;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::GetCellar( CArray2D<bool> *pCellar ) const
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	pLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	if ( !pbinfo )
		return false;
	*pCellar = pbinfo->cellar;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetCellar( const CArray2D<bool> &cellar )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	pLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	if ( !pbinfo )
		pbinfo = new NBuilding::CBuildInfo;
	pbinfo->cellar = cellar;
	pLoader->Updated();
	return Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//					Explosions
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPlacement::SFloorExplosions* CPlacement::GetExplosions( int nFloor )
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &explosions[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CPlacement::SFloorExplosions* CPlacement::GetExplosions( int nFloor ) const
{
  ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
  //
  return &explosions[Floor2Ind( nFloor )];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::MoveFirstExplosion( int nFloor )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	iCurFloorExplosion = nFloor;
	iCurExplosion = -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::MoveNextExplosion()
{
  if ( iCurExplosion >= (int)explosions[Floor2Ind( iCurFloorExplosion )].objs.size() - 1 )
    return false;
  ++iCurExplosion;
  return true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::GetExplosionID() const
{
	int ind = Floor2Ind( iCurFloorExplosion );
  if ( iCurExplosion >= 0 && iCurExplosion < (int)explosions[ind].objs.size() )
    return explosions[ind].objs[iCurExplosion]->nObjID;
  return -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::GetExplosionPos( int nFloor, int objID, CVec3 *pPos, int *pRotation ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	int ind = GetExplosionIndex( nFloor, objID );
	
  if ( -1 == ind )
    return false;
	SExplosion *pObj = explosions[Floor2Ind( nFloor )].objs[ind];
	pPos->x = pObj->props["PosX"]->GetValue();
	pPos->y = pObj->props["PosY"]->GetValue();
	pPos->z = pObj->props["DeltaZ"]->GetValue();
	*pRotation = 0;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CPlacement::GetExplosionPower( int nFloor, int nObjID ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	int ind = GetExplosionIndex( nFloor, nObjID );
	
  if ( -1 == ind )
    return false;
	SExplosion *pObj = explosions[Floor2Ind( nFloor )].objs[ind];
	return pObj->props["Power"]->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetExplosionPower( int nFloor, int nObjID, float fPower )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	int ind = GetExplosionIndex( nFloor, nObjID );
	
  if ( -1 == ind )
    return false;
	SExplosion *pObj = explosions[Floor2Ind( nFloor )].objs[ind];
	pObj->props["Power"]->SetValue( fPower );
	return UpdateItem( EXPLOSIONS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SExplosion* CPlacement::GetExplosion( int nFloor, int nID )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetExplosionIndex( nFloor, nID );
  if ( -1 == ind )
    return 0;
	return explosions[Floor2Ind( nFloor )].objs[ind];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SExplosion* CPlacement::GetExplosion( int nFloor, int nID ) const
{
	return const_cast<CPlacement*>(this)->GetExplosion( nFloor, nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CPlacement::GetObjRoomID( int nFloor, int objID )  const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetFinObjIndex( nFloor, objID );
	
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	CPropMap::const_iterator it = pObj->props.find( "RoomParamID" );
	if ( it == pObj->props.end() )
		return 0;
		
	CVariant val = it->second->GetValue();
	int nRoomID = val.GetType() == CVariant::VT_NULL ? 0 : val;
	
	unordered_map<int, SRoom>::const_iterator itR = roomParams.find( nRoomID );
	if ( roomParams.end() == itR )
	{
		if ( -1 == (char)nRoomID)
			return -1;
		return 0;
	}
	return itR->second.nRoomID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoomPred
{
	int nID;
	unordered_map<int, SRoom> &roomParams;
public:
	CRoomPred( int id, unordered_map<int, SRoom> &_roomParams ) : nID( id ), roomParams( _roomParams ) {}
	boolean operator() ( int r ) { return roomParams[r].nRoomID == nID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetObjRoomID( int nFloor, int objID, BYTE nRoomID )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetFinObjIndex( nFloor, objID );
	
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];

	const vector<int> &rv = roomIDs[Floor2Ind(nFloor)];	
	int rid;
	vector<int>::const_iterator it = find_if( rv.begin(), rv.end(), CRoomPred( nRoomID, roomParams ) );
	if ( it == rv.end() )
	{
		if ( 0 == nRoomID || -1 == (char)nRoomID )
			rid = nRoomID;
		else
			return false;
	}
	else
		rid = *it;

	pObj->props["RoomParamID"]->SetValue( rid );
	pObj->props["RoomID"]->SetValue( nRoomID );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CPlacement::GetObjScale( int nFloor, int nObjID ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	

	CVec3 res(1,1,1);
	int ind = GetFinObjIndex( nFloor, nObjID );
	
	if ( -1 == ind )
		return res;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	CPropMap::const_iterator ix = pObj->props.find( "ScaleX" );
	CPropMap::const_iterator iy = pObj->props.find( "ScaleY" );
	CPropMap::const_iterator iz = pObj->props.find( "ScaleZ" );
	CPropMap::const_iterator ie = pObj->props.end();
	if ( ix == ie || iy == ie || iz == ie )
		return res;

	res.x = ix->second->GetValue();
	res.y = iy->second->GetValue();
	res.z = iz->second->GetValue();
	
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetObjScaleX( int nFloor, int objID, float fScale )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	int ind = GetFinObjIndex( nFloor, objID );
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pObj->props["ScaleX"]->SetValue( fScale );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetObjScaleY( int nFloor, int objID, float fScale )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	int ind = GetFinObjIndex( nFloor, objID );
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pObj->props["ScaleY"]->SetValue( fScale );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetObjScaleZ( int nFloor, int objID, float fScale )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	int ind = GetFinObjIndex( nFloor, objID );
	if ( -1 == ind )
		return false;
	SFinElement *pObj = finobjs[Floor2Ind( nFloor )].objs[ind];
	pObj->props["ScaleZ"]->SetValue( fScale );
	return UpdateItem( FINELEMS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::GetExplosionID( int nFloor, float x, float y, float fRadius ) const
{
	const SFloorExplosions *pObjs = GetExplosions( nFloor );
	if ( !pObjs )
		return -1;
	//
	const int n = pObjs->objs.size();
	float r2 = sqr( fRadius );
	for ( int i = 0; i < n; ++i )
	{
		float fx = pObjs->objs[i]->props["PosX"]->GetValue();
		float fy = pObjs->objs[i]->props["PosY"]->GetValue();

		if ( sqr( x - fx ) + sqr( y - fy ) < r2 )
			return pObjs->objs[i]->nObjID;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::MoveExplosion( int nFloor, int objID, const CVec3 &pt )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	
	int ind = GetExplosionIndex( nFloor, objID );
	
	if ( -1 == ind )
		return false;
	SExplosion *pObj = explosions[Floor2Ind( nFloor )].objs[ind];
	pObj->props["PosX"]->SetValue( pt.x, false );
	pObj->props["PosY"]->SetValue( pt.y, false );
	pObj->props["DeltaZ"]->SetValue( pt.z, false );
	return UpdateItem( EXPLOSIONS_TBL, pObj->nObjID, &pObj->props );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::AddExplosion( int nFloor, const CVec2 &pt, float fPower )
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );	
	//
	CPtr<SExplosion> pDefObj = new SExplosion;
	int nObjID = finDB.Insert( EXPLOSIONS_TBL, GetID(), &pDefObj->props );
	if ( -1 == nObjID )
		return -1;
	//
	CPtr<SExplosion> pObj = new SExplosion;
	pObj->nObjID = nObjID;
	pObj->props["Floor"]->SetValue( nFloor, false );
	pObj->props["Power"]->SetValue( fPower, false );
	pObj->props["PosX"]->SetValue( pt.x, false );
	pObj->props["PosY"]->SetValue( pt.y, false );
	pObj->props["DeltaZ"]->SetValue( 0, false );
	if ( !UpdateItem( EXPLOSIONS_TBL, nObjID, &pObj->props ) )
		return -1;
	//
	SFloorExplosions *pObjs = GetExplosions( nFloor );
	pObjs->objID2objIndex[pObj->nObjID] = pObjs->objs.size();
	pObjs->objs.push_back( pObj );
	return pObj->nObjID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::DeleteExplosion( int nFloor, int objID )
{
	if ( FAILED( finDB.Open( EXPLOSIONS_TBL, objID ) ) )
    return false;	

  int ind = GetExplosionIndex( nFloor, objID );
	
  if ( -1 == ind )
    return false;
	SFloorExplosions &fo = explosions[Floor2Ind(nFloor)];
  fo.objs.erase( fo.objs.begin() + ind );
  SetupExplosionID2IndMap( nFloor );
  MoveFirstExplosion( nFloor );
	
	return finDB.DeleteElement();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::GetRoomParams( int nFloor, vector<SRoom> *pRooms ) const
{
	ASSERT( nFloor >= MIN_FLOOR && nFloor <= MAX_FLOOR );
	if ( nFloor < MIN_FLOOR || nFloor > MAX_FLOOR )
		return false;
	const vector<int> &rooms = roomIDs[Floor2Ind(nFloor)];
	for ( int i = 0; i < rooms.size(); ++i )
	{
		unordered_map<int, SRoom>::const_iterator it = roomParams.find( rooms[i] );
		if ( it != roomParams.end() )
			pRooms->push_back( it->second );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetRoomColor( int nFloor, int nRoomID, DWORD dwColor )
{
	vector<int> &rv = roomIDs[Floor2Ind(nFloor)];

	vector<int>::iterator it = find_if( rv.begin(), rv.end(), CRoomPred( nRoomID, roomParams ) );
	if ( it != rv.end() )
	{
		SRoom &room = roomParams[*it];
		room.dwColor = dwColor;
		string szQuery = "SELECT * FROM " + string( ROOMPARAMS_TBL ) + " WHERE ID=" + IToA( *it );
		if ( FAILED( roomsDB.Open( szQuery ) ) )
			return false;
		if ( roomsDB.MoveNext() != S_OK )
			return false;
		roomsDB.m_nUserColor = dwColor;
		if ( S_OK != roomsDB.SetData( 1 ) )
			return false;
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlacement::AddRoom( int nFloor, DWORD dwColor )
{
	if ( nFloor < MIN_FLOOR || nFloor > MAX_FLOOR )
		return -1;
	string szSubQuery = string( " FROM " ) + ROOMPARAMS_TBL + " WHERE VariantID=" + IToA( GetID() ) + " AND Floor=" + IToA( nFloor );
	string szQuery = string( "SELECT *" ) + szSubQuery + " AND RoomID=(" + "SELECT MAX(RoomID)" + szSubQuery + ")";
	if ( FAILED( roomsDB.Open( szQuery ) ) )
		return -1;
	HRESULT hr = roomsDB.MoveNext();
	if ( S_OK == hr )
		++roomsDB.m_nRoomID;
	else
	{
		roomsDB.m_nRoomID = 1;
#ifdef _DEBUG
		DisplayOLEDBErrorRecords( hr );
#endif
	}
	roomsDB.m_nUserColor = dwColor;
	roomsDB.m_nFloor = nFloor;
	roomsDB.m_nVariantID = GetID();

	if ( FAILED( roomsDB.Open( string( "SELECT * FROM " ) + ROOMPARAMS_TBL + " WHERE ID=-1" ) ) )
		return false;
	hr = roomsDB.Insert( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return -1;
	}
	hr = roomsDB.MoveNext();
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return -1;
	}
	SRoom r;
	r.dwColor = dwColor;
	r.nRoomID = roomsDB.m_nRoomID;

	roomParams[roomsDB.m_nID] = r;
	roomIDs[Floor2Ind(nFloor)].push_back( roomsDB.m_nID );
	
	return r.nRoomID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::GetRoom( SRoom *pRoom, int nFloor, int nRoomID )
{
	const vector<int> &rv = roomIDs[Floor2Ind(nFloor)];

	vector<int>::const_iterator it = find_if( rv.begin(), rv.end(), CRoomPred( nRoomID, roomParams ) );
	if ( it == rv.end() )
		return false;
	*pRoom = roomParams[*it];
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetGrid( CVariant varBGrid )
{
	bool bGrid = varBGrid;
	if ( bGrid == bHasAIGrid )
		return true;
	string szQuery = string( "SELECT * FROM " ) + VARIANTS_TBL + " WHERE ID=" + IToA( GetID() );
	if ( FAILED( dbVars.Open( szQuery ) ) || FAILED( dbVars.MoveNext() ) )
		return false;

	dbVars.m_bGrid = bGrid;
	HRESULT hr = dbVars.SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	bHasAIGrid = bGrid;		
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetRndWeight( CVariant weight )
{
	string szQuery = string( "SELECT * FROM " ) + VARIANTS_TBL + " WHERE ID=" + IToA( GetID() );
	if ( FAILED( dbVars.Open( szQuery ) ) || FAILED( dbVars.MoveNext() ) )
		return false;

	dbVars.m_fRndWeight = weight;
	HRESULT hr = dbVars.SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	fRndWeight = weight;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetDefLight( CVariant light )
{
	string szQuery = string( "SELECT * FROM " ) + VARIANTS_TBL + " WHERE ID=" + IToA( GetID() );
	if ( FAILED( dbVars.Open( szQuery ) ) || FAILED( dbVars.MoveNext() ) )
		return false;

	dbVars.m_nDefLight = (int)light;
	HRESULT hr = dbVars.SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	nDefLightID = light;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetScriptID( CVariant scriptID )
{
	string szQuery = string( "SELECT * FROM " ) + VARIANTS_TBL + " WHERE ID=" + IToA( GetID() );
	if ( FAILED( dbVars.Open( szQuery ) ) || FAILED( dbVars.MoveNext() ) )
		return false;

	dbVars.m_nScriptID= (int)scriptID;
	HRESULT hr = dbVars.SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	nScriptID = scriptID;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// скопировать всю ботву из pPl в текущую расстановку
bool CPlacement::CopyFrom( const CPlacement &pl )
{
	if ( GetWidth() != pl.GetWidth() || GetHeight() != pl.GetHeight() )
		return false;

	CPlacement *pNonConstPl = const_cast<CPlacement*>( &pl );
	pNonConstPl->ForceLoad();
	for ( int nf = MIN_FLOOR; nf < MAX_FLOOR; ++nf )
	{
		CVec3 ptPos;
		int   nRotation;

		pNonConstPl->MoveFirst( nf );
		while ( pNonConstPl->MoveNext() )
		{
			CVec3 ptCenter;
			float w,h;
			int   nRot;
			pl.GetRect( &ptCenter, &w, &h, &nRot );
			int id = AddRect( nf, CVec2( ptCenter.x, ptCenter.y ), w, h, pl.GetRectTemplID() );
			if ( -1 == id )
				return false;
			RotateObj( nf, MO_TEMPLATE, id, nRot );
			MoveObj( nf, MO_TEMPLATE, id, ptCenter );
		}
		pNonConstPl->MoveFirstUnit( nf );
		while ( pNonConstPl->MoveNextUnit() )
		{
			pl.GetObjPos( nf, MO_UNIT, pl.GetUnitID(), &ptPos, &nRotation );
			CTPoint<int> pt( ptPos.x, ptPos.y );
			int id = AddUnit( nf, pt, pl.GetUnitMonsterID( nf, pl.GetUnitID() ) );
			if ( -1 == id )
				return false;
			RotateObj( nf, MO_UNIT, id, nRotation );
		}
		//
		CObjectMgr *pMgr = GetObjectMgr( BT_OBJECT );
		CObjectMgr *pSMgr = GetObjectMgr( (EBrushType)BT_SCALABLEOBJECT );
		CObjectMgr *pEMgr = GetObjectMgr( (EBrushType)BT_EXPLOSION );
		CObjectMgr *pPMgr = GetObjectMgr( (EBrushType)BT_PASSAGEOBJECT );
		pNonConstPl->MoveFirstObj( nf );
		while ( pNonConstPl->MoveNextObj() )
		{
			const int nSrcID = pl.GetObjID();
			pl.GetObjPos( nf, MO_OBJECT, nSrcID, &ptPos, &nRotation );
			CVec2 pt( ptPos.x, ptPos.y );
			const int id = AddPlaceObj( nf, pt, pl.GetObjModelID( nf, nSrcID ) );
			if ( -1 == id )
				return false;
			//MoveObj( nf, MO_OBJECT, id, ptPos );
			//RotateObj( nf, MO_OBJECT, id, nRotation );

			CPropMap props;
			if ( pMgr )	 { pMgr->MergeWith( &props, nSrcID ); pMgr->SetObjectProps( id, &props ); }
			if ( pSMgr ) { pSMgr->MergeWith( &props, nSrcID ); pSMgr->SetObjectProps( id, &props ); }
			if ( pEMgr ) { pEMgr->MergeWith( &props, nSrcID ); pEMgr->SetObjectProps( id, &props ); }
			if ( pPMgr ) { pPMgr->MergeWith( &props, nSrcID ); pPMgr->SetObjectProps( id, &props ); }
		}
		pNonConstPl->MoveFirstExplosion( nf );
		while ( pNonConstPl->MoveNextExplosion() )
		{
			pl.GetObjPos( nf, MO_EXPLOSION, pl.GetExplosionID(), &ptPos, &nRotation );
			CVec2 pt( ptPos.x, ptPos.y );
			int id = AddExplosion( nf, pt, (pl.GetExplosionPower( nf, pl.GetExplosionID()) ) );
			pl.GetObjPos( nf, MO_EXPLOSION, pl.GetExplosionID(), &ptPos, &nRotation );
			MoveObj( nf, MO_EXPLOSION, id, ptPos );
			if ( -1 == id )
				return false;
		}
	}
	// Terrain
	const CMETerrainInfo *pSrcTerr = const_cast<CPlacement*>( &pl )->GetTerrainInfo();
	if ( pSrcTerr )
	{
		CMETerrainInfo *pTerr = GetTerrainInfo();
		if ( !pTerr )
		{
			CreateTerrain();
			pTerr = GetTerrainInfo();
		}
		if ( pTerr )
		{
			pTerr->info  = pSrcTerr->info;
			pTerr->fMinH = pSrcTerr->fMinH;
			pTerr->fMaxH = pSrcTerr->fMaxH;
			pTerr->alphaMap = pSrcTerr->alphaMap;
			SerializeTerrain();
		}
	}
	//
	//if ( !rects.empty() || !units.empty() || !finobjs.empty() || !explosions.empty() )
	//{
//		NDatabase::Import();
//	}
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pSrcLoader = NGScene::shareBuildings.Get( pl.GetID() );
	pLoader.Refresh();
	pSrcLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	CPtr<NBuilding::CBuildInfo> psrcinfo = pSrcLoader->GetValue();
	if ( !pbinfo )
		pbinfo = new NBuilding::CBuildInfo;
	//MakeBuildingInfo( this, pbinfo );
	if ( pbinfo && psrcinfo )
	{
		pbinfo->wallFragments = psrcinfo->wallFragments;
		pbinfo->solidFragments = psrcinfo->solidFragments;
		pbinfo->roomMap = psrcinfo->roomMap;
		pbinfo->spots = psrcinfo->spots;
		pbinfo->nMaxY = psrcinfo->nMaxY;
		pbinfo->nMaxX = psrcinfo->nMaxX;
		pbinfo->nMinFloor = psrcinfo->nMinFloor;
		pbinfo->nMaxFloor = psrcinfo->nMaxFloor;
		pbinfo->ladders = psrcinfo->ladders;
		pbinfo->cellar = psrcinfo->cellar;
		pbinfo->lgroups = psrcinfo->lgroups;
	}
	pLoader->Updated();
	// Terr spots
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( pl.GetID() );
	if ( IsValid( pVar ) )
	{
		SRand rand;
		for ( int i = 0; i < pVar->terrainSpots.size(); ++i )
		{
			NDb::CRndTerrainSpot *pSpot = pVar->terrainSpots[i];
			if ( !IsValid( pSpot ) )
				continue;
			NBuilding::SProjectedSpot s;
			s.ptOrigin = CVec3( pSpot->ptPos, 0 );
			s.ptNormal = CVec3(0, 0, 1);
			s.ptSize = pSpot->ptSize;
			s.nRotation = pSpot->nRotation;
			s.nMaterialID = pSpot->pSpot->GetRecordID();
			NWysiwyg::AddTerrSpotDB( GetID(), s );
		}
		//
		for ( int i = 0; i < pVar->waypoints.size(); ++i )
		{
			NDb::CWaypoint *pW = pVar->waypoints[i];
			if ( !IsValid( pW ) || !IsValid( pW->pName ) )
				continue;
			int nID = AddWaypoint2DB( pW->pName->GetRecordID(), GetID() );
			if ( nID <= 0 )
				continue;
			CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( pW->GetRecordID() );
			if ( !IsValid( pWPLoader ) )
				return false;
			pWPLoader.Refresh();
			SerializeWaypoint( pWPLoader->GetValue(), nID );
		}
	}
	return Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacement::GetLayerGroups( vector<NBuilding::SLayerGroup> *pGroups )
{
	ASSERT( pGroups );
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	pLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	if ( !pbinfo )
		return;
	*pGroups = pbinfo->lgroups;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::SetLayerGroups( const vector<NBuilding::SLayerGroup> &groups )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( GetID() );
	pLoader.Refresh();
	CPtr<NBuilding::CBuildInfo> pbinfo = pLoader->GetValue();
	if ( !pbinfo )
		pbinfo = new NBuilding::CBuildInfo;
	pbinfo->lgroups = groups;
	pLoader->Updated();
	return SerializeBuilding( pbinfo, GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacement::CreateTerrain()
{
	CPtr<CMETerrainInfo> pTerr = new CMETerrainInfo;

	pTerr->fMinH = 0;
	pTerr->fMaxH = 1;
	pTerr->info.nWidth = nWidth;
	pTerr->info.nHeight = nHeight;
	bool bRet = ::SerializeTerrain( pTerr, GetID() );
	pTerrLoader->Updated();
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
