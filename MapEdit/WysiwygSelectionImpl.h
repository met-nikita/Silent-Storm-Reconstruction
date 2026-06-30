#ifndef __WYSIWYGSELECTIONIMPL_H_
#define __WYSIWYGSELECTIONIMPL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "WysiwygSelection.h"
#include "WysiwygClipboard.h"
#include "..\Misc\HPTimer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class IEditorWorld;
	class CEditorWorld;
}
namespace NDb
{
	class CTemplVariant;
	class CModel;
}
namespace NBuilding
{
	class CBuildInfo;
}
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelection: public ISelection
{
	OBJECT_NOCOPY_METHODS(CSelection);

	enum EAction
	{
		A_MOVE,
		A_MOVE_Z,
		A_ROTATE,
		A_FILL,
		A_RECTSEL,
	};
	CPtr<NGScene::IGameView> pScene;
	CPtr<NWorld::CEditorWorld> pWorld;
	int nWorldID;
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuilding;
	CPtr<NBuilding::CBuildingGrid> pGrid;
	CPtr<NDb::CTemplVariant> pVar;
	CPtr<ICamera> pCamera;
	vector< CPtr<CSelectedObj> > selection;
	list< CObj<NGScene::CPolyline> > pLines;
	list< CObj<CObjectBase> > parts;
	list< CObj<NGScene::CPolyline> > rectSelLines;
	list< CObj<CObjectBase> > fillparts;
	CObj< NDb::CModel > pHoldModel;
	EAction eAction;
	bool bLBDown;
	CVec2 ptLBDTile, ptCurrentTile;
	CTRect<float> rectSelection;
	SForceSelection rectLastSel;
	SForceSelection collectedSel;
	float fLBDz;
	CVec2 ptLastPos;
	CVec3 ptLBDSelectionCenter;
	bool  bEmptyOnLBDown;
	NHPTimer::STime tSelectTime;
	CVec2 ptWorldSize; // in tiles
	//
	CVec3 ptCurrentKbdMove;
	float fCurrentKbdRotation;

	bool Move( const CVec2 &ptPos );
	void Test( const CVec2 &ptPos );
	bool Rotate( const CVec2 &ptPos );
	bool AddFragment( NBuilding::CBuildInfo *pInfo, CVec3 ptPos, int nGeomID, int nRotationID, int nLayerID, vector<int> *pUserIDs = 0 );
	void GetFilledFragments( vector<NBuilding::SBuildFragment> *pFragments, int nCPartID, 
		int nLayerID, float fFloor, int nRotationID, CVec2 ptStart, CVec2 ptEnd );
	EAction GetAction() const;
	void CollectSelection( SForceSelection *pSel );
	void SendSelectionInfo();

public:
	CSelection() {}
	CSelection( int nWorldID, NWorld::IEditorWorld *pWorld, NBuilding::CBuildingGrid *pGrid, NGScene::IGameView *pScene, ICamera *pCamera );

	bool IsEmpty() const { return selection.empty(); }
	void SetSelection( CObjectBase *pObj, int nUserID );
	void AddSelection( CObjectBase *pObj, int nUserID );
	void SelectAll();
	bool CheckSelection( CObjectBase *pObj, int nUserID );
	void AddObject( const CVec2 &ptPos );
	void DeleteSelected();
	void BuildingUpdated();
	virtual void ObjectUpdated( int nDBFinElemID );
	void TerrainSpotUpdated( const vector<NBuilding::SProjectedSpot> &spots );
	void Clear();

	bool Update( const CVec2 &ptCursorMove, CObjectBase *pObj, int nUserID );
	void Draw();
	void DrawRectangularSelection( const CVec3 &color );
	void DrawRectangularFill();
	CRay GetProjectiveRay( const CVec2 &ptPos );

	void OnLButtonDown( const CVec2 &ptPos );
	void OnLButtonUp( const CVec2 &ptPos );
	void OnLBDblClick( const CVec2 &ptPos );
	bool GetSelectionBound( SBound *pRes );
	bool GetSelectionMovingBound( SBound *pRes );
	virtual void Select( const SForceSelection &sel, bool bDiscardOldSel = true );
	void Select( CTRect<float> r );
	void OnCopy();
	void OnPaste( const SForceSelection &sel, const CVec2 &ptCursor, bool bAlignLeftTop, int nSelectionMinFloor );
	CVec2 GetTileUnderPos( const CRay &ray, float fFloor );
	NWorld::IEditorWorld* GetWorld();
	void ProcessEvent( const string &str );
	int GetSelectionMask();
	void AssignBuildingSpot( int nSpotID );
	virtual int GetSelectedSpotID() const;
	virtual SFBTransform GetTerrainTransform( float x, float y );
	virtual void SubTemplateUpdated( int nUserID );
	virtual void UnitUpdated( int nDBUnitID );
	virtual bool AddConstructionPart( vector<int> *pUserIDs, int nCPartID, CVec3 ptPos );
	virtual void WaypointUpdated( int nWaypointID );
	virtual void WallSpotUpdated( int nSpotID );
	virtual void LadderUpdated( int nLadderID );
	virtual void OpenObject( int nObjectID );
	virtual int  GetFirstSelectedID( EBrushType eType );

	const vector< CPtr<CSelectedObj> >& GetSelection() const { return selection; }
	//
	virtual void StartRotate();
	virtual void Rotate( float fAngle );
	virtual void EndRotate();
	virtual void StartMove();
	virtual void Move( const CVec3 &ptDelta );
	virtual void EndMove();
	virtual void MoveSelection( const CVec3 &ptCenter );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
