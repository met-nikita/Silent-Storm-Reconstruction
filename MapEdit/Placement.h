#ifndef __PLACEMENT_H__
#define __PLACEMENT_H__

#include "PropMap.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Main\TerrainInfo.h"
#include "..\Main\METerrain.h"

namespace NBuilding
{
	struct SLayerGroup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMapObjType
{
	MO_TEMPLATE,
	MO_UNIT,
	MO_OBJECT,
	MO_EXPLOSION,
	MO_LIGHT,
	MO_EMPTY
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFloorType
{
	FT_FLOOR = 0, // не менять порядок объявлений
	FT_FLOOR_INTERMEDIATE,
	FT_SOLID_, // в .Net объявился свой FT_SOLID
	FT_SOLID_INTERMEDIATE,
	FT_ROOM,
	FT_MAX,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinProp : public CProp
{
OBJECT_BASIC_METHODS(CFinProp);
private:
	CVariant     value;
	CVariant     defValue;
  
public:
	CFinProp() {ASSERT(0);}
  CFinProp( const string &szName, int nID, int nType, int nViewType, CVariant defValue = CVariant(), bool bReadOnly = false );
  
  const CVariant& GetValue() const { return value; }
	const CVariant GetDefValue() const { return defValue; }
  void SetValue( const CVariant &value, bool bModified = true ) const;
	CProp* Clone() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SObjRect
{
  int       rectID;
  int       templID;
  int       rotation;
	CVec3			ptCenter;
	float			fWidth;
	float     fHeight;
  CVec2			rect[4];
  bool      bNewRect;
	
  SObjRect() : templID(-1), bNewRect(false) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnit
{
	int nUnitID;			// ID юнита
	CPropMap props;   // св-ва юнита
	SUnit();
	~SUnit();
	
private:
	SUnit( const SUnit &op ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFinElement : public CObjectBase
{
	int nObjID;			// ID обекта
	CPropMap props;   // св-ва объекта
	const CVec2 ptSize;
	const CVec2 ptCenter;

	SFinElement();
	
private:
	OBJECT_BASIC_METHODS( SFinElement )
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SExplosion : public CObjectBase
{
	int nObjID;			  // ID обекта
	CPropMap props;   // св-ва объекта
	SExplosion();
	
private:
	OBJECT_BASIC_METHODS( SExplosion )
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRoom
{
	DWORD dwColor;
	BYTE  nRoomID; // nRoomID уникален в пределах одного этажа расстановки
	SRoom() : nRoomID( -1 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWallsPlan;
class CFloorPlan;
class CItemsMgr;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacement
{
private:
	CDGPtr< CPtrFuncBase<CMETerrainInfo> > pTerrLoader;
	int				id;                 // ID варианта - индекс в таблице вариантов расстановок.
  int				prntTemplID;        // ID темплейта, которому принадлежит расстановка
  int				nWidth;
	int				nHeight;
	int				iCurFloor;          // текущий этаж, на котором перебираются прямоугольники
  int				iCurRect;           // текущий прямоугольник
	int				iCurFloorUnit;      // текущий этаж, на котором перебираются юниты
  int				iCurUnit;           // текущий юнит
	int				iCurFloorObj;       // текущий этаж, на котором перебираются конечные элементы
  int				iCurObj;            // текущий конечный элемент
	int				iCurFloorExplosion; // текущий этаж, на котором перебираются взрывы
	int				iCurExplosion;      // текущий взрыв
	
	struct SFloorRects
	{
		vector<SObjRect> placement;
		unordered_map<int, int> objID2objIndex;				// отображение ID объекта в индекс в векторе placement
	};
	struct SFloorUnits
	{
		vector<SUnit*> units;
		unordered_map<int, int> unitID2unitIndex;				// отображение ID юнита в индекс в векторе units
	};
	struct SFloorObjs
	{
		vector<CPtr<SFinElement> > objs;
		unordered_map<int, int> objID2objIndex;
	};
	struct SFloorExplosions
	{
		vector<CPtr<SExplosion> > objs;
		unordered_map<int, int> objID2objIndex;
	};
  vector<int>        delObjs;           // список удаленных об., используется в UpdateRects

	// информация об объектах в расстановке, сгруппированная по этажам
	vector<SFloorRects>  rects;
	vector<SFloorUnits>  units;
	vector<SFloorObjs>   finobjs;
	vector<SFloorExplosions>   explosions;
  vector<CPtr<CWallsPlan> >  walls;
	vector<vector< CPtr<CFloorPlan> > > floors;
	vector<vector< CPtr<CFloorPlan> > > floorsInterm;
	vector<vector< CPtr<CFloorPlan> > > solids;
	vector<vector< CPtr<CFloorPlan> > > solidsInterm;
	vector<vector< CPtr<CFloorPlan> > > rooms;
	vector<vector<int> > roomIDs;
	unordered_map<int, SRoom> roomParams;
	
	string szTerrName;
	//CArray2D<BYTE> textureMap;
	//CArray2D<WORD> heightMap;
	//CArray2D<unsigned short> alphaMap;
	//float fMinH, fMaxH;
	//vector<STerrainHole> *const pHoles;
    
  bool  bInitialized;
	bool  bHasAIGrid;
	float fRndWeight;
	int   nDefLightID;
	int   nScriptID;
	bool  bLoaded;

	static int iMaxRectID;
	static int GetTempRectID();

  CPlacement( const CPlacement &pl ) {}

  // Получение индекса в placement по ID объекта
  // "-1", если ID не найден
  int GetObjIndex( int nFloor, int id ) const 
  {
		const SFloorRects* pRects = GetRects( nFloor );
		if ( !pRects )
			return -1;
		
    unordered_map<int, int>::const_iterator it = pRects->objID2objIndex.find( id );
    if ( pRects->objID2objIndex.end() == it )
      return -1;
    return it->second;
  }
  // Получение индекса в units по ID юнита
  // "-1", если ID не найден
  int GetUnitIndex( int nFloor, int id ) const 
  {
		const SFloorUnits *pUnits = GetUnits( nFloor );
		if ( !pUnits )
			return -1;
		
    unordered_map<int, int>::const_iterator it = pUnits->unitID2unitIndex.find( id );
    if ( pUnits->unitID2unitIndex.end() == it )
      return -1;
    return it->second;
  }
  int GetFinObjIndex( int nFloor, int id ) const 
  {
		const SFloorObjs *pObjs = GetFinObjs( nFloor );
		if ( !pObjs )
			return -1;
		
    unordered_map<int, int>::const_iterator it = pObjs->objID2objIndex.find( id );
    if ( pObjs->objID2objIndex.end() == it )
      return -1;
    return it->second;
  }
  int GetExplosionIndex( int nFloor, int id ) const 
  {
		const SFloorExplosions *pObjs = GetExplosions( nFloor );
		if ( !pObjs )
			return -1;
		
    unordered_map<int, int>::const_iterator it = pObjs->objID2objIndex.find( id );
    if ( pObjs->objID2objIndex.end() == it )
      return -1;
    return it->second;
  }
	//
  void SetupRectID2IndMap( int nFloor );
	void SetupUnitID2IndMap( int nFloor );
	void SetupObjID2IndMap( int nFloor );
	void SetupExplosionID2IndMap( int nFloor );
  int  AddRectToDB( int nFloor, int ind );
  bool RemoveRectFromDB( int id );
	void MakeRect( CVec2 rect[4], const CVec2 &ptCenter, float fWidth, float fHeight, int nRotation );
  bool GetNearestPos( POINT *pPtNearest, const POINT &ptDesired, int width, int height )  const;
  bool CheckRect( const CVec2 rect[4] )  const;
	void GetModelSize( CVec2 *pptSize, CVec2 *pptCenter, int nObjectID, CItemsMgr *pOItems, CItemsMgr *pModelItems, CItemsMgr *pGeomItems, CItemsMgr *pCItems );
	//
	SFloorRects* GetRects( int nFloor );
	SFloorUnits* GetUnits( int nFloor );
	SFloorObjs*  GetFinObjs( int nFloor );
	SFloorExplosions* GetExplosions( int nFloor );
	const SExplosion* GetExplosion( int nFloor, int nID ) const;
	SExplosion* GetExplosion( int nFloor, int nID );
	const SFloorRects* GetRects( int nFloor ) const;
	const SFloorUnits* GetUnits( int nFloor ) const;
	const SFloorObjs*  GetFinObjs( int nFloor ) const;
	const SFloorExplosions*  GetExplosions( int nFloor ) const;
	//
	bool UpdateItem( const string &szTable, int nID, const CPropMap *pProps );
	bool LoadFloors();
	bool LoadRects();
	bool LoadUnits();
	bool LoadFinObjs();
	bool LoadExplosions();
	bool LoadRooms();
	//
	int   GetTemplRectID( int nFloor, float x, float y ) const;
  int   GetUnitID( int nFloor, float x, float y, float fRadius ) const;
	int   GetObjID( int nFloor, float x, float y, float fRadius ) const;
	int   GetExplosionID( int nFloor, float x, float y, float fRadius ) const;
	int   GetLightID( int nFloor, float x, float y, float fRadius ) const;
  bool  GetUnitPos( int nFloor, int unitID, CTPoint<int> *pPos, int *pRotation ) const;
  bool  GetObjPos( int nFloor, int objID, CVec3 *pPos, int *pRotation ) const;
  bool  GetExplosionPos( int nFloor, int objID, CVec3 *pPos, int *pRotation ) const;
	bool  GetLightPos( int nFloor, int objID, CVec2 *pPos, int *pRotation ) const;
  bool  MoveRect( int nFloor, int objID, const CVec3 &ptCenter );
  bool  MoveUnit( int nFloor, int unitID, const CTPoint<int> &pt );
  bool  MoveObj( int nFloor, int finID, const CVec3 &pt );
	bool  MoveExplosion( int nFloor, int explosionID, const CVec3 &pt );
  bool  RotateRect ( int nFloor, int objID, int angle );
  bool  RotateUnit( int nFloor, int unitID, int nAngle );
  bool  RotateObj( int nFloor, int unitID, int nAngle );
  bool  DeleteRect( int nFloor, int objID );
  bool  DeleteUnit( int nFloor, int unitID );
  bool  DeleteObj( int nFloor, int objID );
	bool  DeleteExplosion( int nFloor, int explosionID );
	//
	bool  HasFloor( int nFloor, int nLayer, const vector< vector< CPtr<CFloorPlan> > > &floors ) const;
	CFloorPlan* GetFloor( int nFloor, int nLayer, vector< vector< CPtr<CFloorPlan> > > *pFloors, EFloorType );
	const CFloorPlan* GetFloor( int nFloor, int nLayer, const vector< vector< CPtr<CFloorPlan> > > &floors ) const;
	//	
  friend class CTemplateMgr;
	void  PushRect( int nFloor, int rectID, const CVec3 &ptCenter, float fWidth, float fHeight, int templID, int nRotation );
	void  GetBoundRect( const CVec2 rect[4], CVec2 *pPtCenter, float *pWidth, float *pHeight );

//	bool  ReadTerrain();
//	bool  WriteTerrain();
//	void  SerializeTerrain( CStructureSaver *pFile );
  
public:
  CPlacement();
  ~CPlacement();

  void  Init( int id, int templID, bool bGrid, float fRndWeight, int nDefLightID, int nScriptID );
	void  UpdateLayers();
  bool  Empty( int nFloor );
	bool  Empty();
	bool  HasGrid() const { return bHasAIGrid; }
	bool  SetGrid( CVariant bGrid );
	float GetRndWeight() const { return fRndWeight; }
	bool  SetRndWeight( CVariant weight );
	int   GetDefLight() const { return nDefLightID; }
	bool  SetDefLight( CVariant light );
	int   GetScriptID() const { return nScriptID; }
	bool  SetScriptID( CVariant light );
	bool  CopyFrom( const CPlacement &pl );
	bool  Save() const;

  int   GetTemplateID()  const;
  int   GetWidth() const { return nWidth; }
  int   GetHeight() const { return nHeight; }
	bool  GetNearestPos( CVec2 *pPtNearest, const CVec2 &ptDesired, float width, float height )  const;
	
	// подтемплейты
  void  MoveFirst( int nFloor );
  bool  MoveNext();
  int   GetID() const { return id; }
  bool  GetRect( CVec3 *pPtCenter, float *pWidth, float *pHeight, int *pRotation ) const;
  bool  GetRect( int nFloor, int objID, CVec3 *pPtCenter, float *pWidth, float *pHeight, int *pRotation ) const;
  int   GetRectTemplID()  const;
  int   GetRectTemplID( int nFloor, int objID )  const;
  int   GetRectID() const;
  EMapObjType GetObjID( int *pID, int nFloor, float x, float y, float fRadius ) const;

  bool  IsInitialized() const { return bInitialized; }

  bool  MoveObj( int nFloor, EMapObjType objType, int objID, const CVec3 &ptCenter );
  bool  RotateObj( int nFloor, EMapObjType objType, int objID, int angle );
	bool  GetObjPos( int nFloor, EMapObjType objType, int objID, CVec3 *pPos, int *pAngle ) const;
	bool  DeleteObj( int nFloor, EMapObjType objType, int objID );
	bool  GetObjectSize( int nFloor, EMapObjType objType, int objID, CVec2 *pptSize, CVec2 *pptCenter );
	const CPropMap* GetObjectProps( int nFloor, EMapObjType objType, int objID );

	int   AddRect( int nFloor, const CVec2 &ptCenter, float fWidth, float fHeight, int templID );
  void  DeleteRect();

  bool  UpdateRects();
	CMETerrainInfo* GetTerrainInfo();
	bool  CreateTerrain();
	bool  SerializeTerrain();
	void  ForceLoad();
	//const CArray2D<BYTE>* GetTerrTexMap();
	//const CArray2D<WORD>* GetTerrHMap( float *pMinH, float *pMaxH );
	//const CArray2D<WORD>* GetTerrAMap();
	//const vector<STerrainHole>* GetTerrHoles();
	//const vector<SGrassLayer>*  GetTerrGrass();
	//const CArray2D<DWORD>* GetTerrColormap();
//	bool  SetTerrMap( const CArray2D<BYTE> *pTmap, 
//										float fMinH, float fMaxH, const CArray2D<WORD> *pHmap, const vector<STerrainHole> *pHoles, 
//										const CArray2D<WORD> *pAmap, const vector<SGrassLayer> *pGrass, const CArray2D<DWORD> *pColormap );
	bool GetCellar( CArray2D<bool> *pCellar ) const;
	bool SetCellar( const CArray2D<bool> &cellar );
	void GetLayerGroups( vector<NBuilding::SLayerGroup> *pGroups );
	bool SetLayerGroups( const vector<NBuilding::SLayerGroup> &groups );
	
  // Фун-ции для работы со стенками

  bool  HasWalls( int nFloor ) const;
  CWallsPlan* GetFloorWalls( int nFloor );
  const CWallsPlan* GetFloorWalls( int nFloor ) const;

  // Фун-ции для работы со полами и с solid objects

  bool  HasFloor( EFloorType type, int nFloor, int nLayer ) const;
	int   GetFloorLayersNum( EFloorType type ) const;
  CFloorPlan* GetFloor( EFloorType type, int nFloor, int nLayer );
  const CFloorPlan* GetFloor( EFloorType type, int nFloor, int nLayer ) const;
	
	bool GetRoomParams( int nFloor, vector<SRoom> *pRooms ) const;
	bool SetRoomColor( int nFloor, int nRoomID, DWORD dwColor );
	int  AddRoom( int nFloor, DWORD dwColor );
	bool GetRoom( SRoom *pRoom, int nFloor, int nRoomID );

	// Units
	void  MoveFirstUnit( int nFloor );
  bool  MoveNextUnit();
  int   GetUnitID() const;
  int   GetUnitMonsterID( int nFloor, int unitID )  const;
	
	int   AddUnit( int nFloor, const CTPoint<int> &pt, int nMonsterID );

	// FinElements
	void  MoveFirstObj( int nFloor );
  bool  MoveNextObj();
  int   GetObjID() const;
  int   GetObjModelID( int nFloor, int objID )  const;
	BYTE  GetObjRoomID( int nFloor, int nObjID ) const;
	CVec3 GetObjScale( int nFloor, int nObjID ) const;
	bool  SetObjRoomID( int nFloor, int nObjID, BYTE nRoomID );
	bool  SetObjScaleX( int nFloor, int nObjID, float fScale );
	bool  SetObjScaleY( int nFloor, int nObjID, float fScale );
	bool  SetObjScaleZ( int nFloor, int nObjID, float fScale );

	int   AddObj( int nFloor, const CVec2 &pt, int nTreeID, int nModelID );
	int   AddPlaceObj( int nFloor, const CVec2 &pt, int nPlaceID, CPtr<SFinElement> *pObj = 0 );
		
	// Explosions
	void  MoveFirstExplosion( int nFloor );
  bool  MoveNextExplosion();
  int   GetExplosionID() const;
	float GetExplosionPower( int nFloor, int nObjID ) const;
	bool  SetExplosionPower( int nFloor, int nObjID, float fPower );

	int   AddExplosion( int nFloor, const CVec2 &pt, float fPower );
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Inline functions 

#endif // __PLACEMENT_H__