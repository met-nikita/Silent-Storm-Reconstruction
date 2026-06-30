#ifndef __WYSIWYGSELECTION_H_
#define __WYSIWYGSELECTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class IGameView;
	class CPolyline;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class IEditorWorld;
	class IBuilding;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{ 
	struct SBuildFragment;
	class CBuildInfo;
	class CBuildingGrid;
	struct SProjectedSpot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera;
class CMemObject;
enum EBrushType;
struct SForceSelection;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
const CVec3 crLines( 0.6f, 1, 0.7f );
const int N_DEF_SELMINF = -100;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSelectedInfo
{
	EBrushType eBrushType;
	int nBrushID;
	int nObjectID;
	CVec3 ptPos;
	int nRotation;
	int nFloor;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelectedObj: public CObjectBase
{
//	OBJECT_BASIC_METHODS(CSelectedObj);
public:
	virtual bool IsInitialized() const = 0;
	virtual bool IsEqual( CObjectBase *pObj, int nUserID ) const = 0;
	virtual bool IsEqual( int nID ) const = 0; // тот же самый ID, что и SForceSelection
	virtual int  GetSelectionID() const = 0; // тот же самый ID, что и SForceSelection
	virtual bool DelayedDelete() = 0;
	virtual SBound GetBoundBox() const = 0;
	virtual SBound GetMovingBoundBox() const = 0;
	virtual int  GetMovementDiscrete() const = 0; // дискретность сдвига данного типа объектов в тайлах
	virtual int  GetRotationDiscrete() const = 0; // дискретность поворота данного типа объектов
	virtual int  GetZDiscrete() const = 0; // дискретность таскани€ по Z в четвертинках этажа

	virtual void OnCopy() {}
	virtual void StartMove( const CVec2 &ptCursor ) {}
	// ptMove - вектор в горизонтальной плоскости от точки где была нажата лева€ кнопка до текущей позиции
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor ) { return false; }
	virtual bool EndMove( bool bCancel ) { return false; }
	virtual bool Rotate( float fDRotation ) {return false;}
	virtual bool RotateAround( float fRotation, const CVec2 &ptCenter ) {return false;}
	virtual void OnLBDblClick() {};
	virtual bool GetInfo( SSelectedInfo *pInfo ) { return false; }

	// если возвр. значение Draw() = false, то рисуетс€ bound box возвращаемый GetMovingBoundBox()
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow ) { return false; }

	// выделение дл€ составных объектов (солиды)
	virtual bool GetMovingPrimaryBoundBox( SBound *pBox ) const { return false; }
	virtual bool GetPrimaryBoundBox( SBound *pBox ) const { return false; }
	virtual void GetGBoundBox( CMemObject **pBox, CMemObject **pPrimary ) const { *pBox = 0; *pPrimary = 0; }

	virtual void ProcessEvent( const string &str ) {};
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISelection: public CObjectBase
{
public:
	virtual bool IsEmpty() const = 0;
	virtual void SetSelection( CObjectBase *pObj, int nUserID ) = 0;
	virtual void AddSelection( CObjectBase *pObj, int nUserID ) = 0;
	virtual void SelectAll() = 0;
	virtual bool CheckSelection( CObjectBase *pObj, int nUserID ) = 0;
	virtual void AddObject( const CVec2 &ptPos ) = 0;
	virtual void DeleteSelected() = 0;
	virtual void BuildingUpdated() = 0;
	virtual void ObjectUpdated( int nDBFinElemID ) = 0;
	virtual void TerrainSpotUpdated( const vector<NBuilding::SProjectedSpot> &spots ) = 0;
	virtual void WallSpotUpdated( int nSpotID ) = 0;
	virtual void SubTemplateUpdated( int nUserID ) = 0;
	virtual void UnitUpdated( int nDBUnitID ) = 0;
	virtual void WaypointUpdated( int nWaypointID ) = 0;
	virtual void LadderUpdated( int nLadderID ) = 0;
	virtual void Clear() = 0;

	virtual bool Update( const CVec2 &ptCursorMove, CObjectBase *pObj, int nUserID ) = 0;
	virtual void Draw() = 0;
	virtual CRay GetProjectiveRay( const CVec2 &ptPos ) = 0;

	virtual void OnLButtonDown( const CVec2 &ptPos ) = 0;
	virtual void OnLButtonUp( const CVec2 &ptPos ) = 0;
	virtual void OnLBDblClick( const CVec2 &ptPos ) = 0;
	virtual bool GetSelectionBound( SBound *pRes ) = 0;
	virtual void Select( const SForceSelection &sel, bool bDiscardOldSel = true ) = 0;
	virtual void OnCopy() = 0;
	virtual void OnPaste( const SForceSelection &sel, const CVec2 &ptCursor, bool bAlignLeftTop = true, int nSelectionMinFloor = N_DEF_SELMINF ) = 0;
	virtual CVec2 GetTileUnderPos( const CRay &ray, float fFloor ) = 0;
	virtual NWorld::IEditorWorld* GetWorld() = 0;
	virtual void ProcessEvent( const string &str ) = 0;
	virtual int  GetSelectionMask() = 0;
	virtual void AssignBuildingSpot( int nSpotID ) = 0;
	virtual int  GetSelectedSpotID() const = 0;
	virtual SFBTransform GetTerrainTransform( float x, float y ) = 0;
	virtual bool AddConstructionPart( vector<int> *pUserIDs, int nCPartID, CVec3 ptPos ) = 0; // возвр. nUserID добавленных фрагментов
	virtual void OpenObject( int nObjectID ) = 0;
	virtual int  GetFirstSelectedID( EBrushType eType ) = 0;
	//
	virtual void StartRotate() = 0;
	virtual void Rotate( float fAngle ) = 0;
	virtual void EndRotate() = 0;
	virtual void StartMove() = 0;
	virtual void Move( const CVec3 &ptDelta ) = 0;
	virtual void EndMove() = 0;
	virtual void MoveSelection( const CVec3 &ptCenter ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ISelection* CreateSelection( int nWorldID, NWorld::IEditorWorld *pWorld, NBuilding::CBuildingGrid *pGrid, NGScene::IGameView *pScene, ICamera *pCamera );
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateBuildInfo( int nBuildingID );
bool UpdateBuildInfo( NWorld::IBuilding *pObj );
// функции провер€ющие выход bbox'a за размеры карты, перва€ выбирает ближайщий узел тайловой сетки
// все в тайловых единицах
CVec2 CheckTilePos( const SBound &box, const CVec2 &pt, const CVec2 &ptSize );
CVec2 CheckPos( const SBound &box, const CVec2 &pt, const CVec2 &ptSize );
//
void  DrawBox( NGScene::IGameView *pScene, const SBound &bbox, const CVec3 &color, list<CObj<NGScene::CPolyline> > *pL, const SFBTransform &pos );
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void RotatePt( CVec3 *pVec, int nAngle )
{
  const float fAng = -ToRadian( (float) nAngle );
  const float fc = cos( fAng ), fs = sin( fAng );
	
  float x = fc * pVec->x + fs * pVec->y;
  pVec->y = -fs * pVec->x + fc * pVec->y;
  pVec->x = x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void RotatePt( CVec2 *pVec, float fAngle )
{
  const float fAng = -ToRadian( fAngle );
  const float fc = cos( fAng ), fs = sin( fAng );
	
  float x = fc * pVec->x + fs * pVec->y;
  pVec->y = -fs * pVec->x + fc * pVec->y;
  pVec->x = x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsPressed( int nKey )
{
	return 0x8000 & GetAsyncKeyState( nKey );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsRotation();
bool IsDiscreteRotation();
bool TileAlign();
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__WYSIWYGSELECTION_H_