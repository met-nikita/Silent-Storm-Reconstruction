#ifndef __WYSIWYGFRAGMENTSEL_H_
#define __WYSIWYGFRAGMENTSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygSelection.h"
#include "..\Main\BuildingGrid.h"
#include "..\Main\BuildingInfo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CTemplVariant;
	class CConstructionPart;
}
namespace NWorld
{
	class IEditorWorld;
}
namespace NBuilding
{
	class CBuildingGrid;
}
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
class CFragmentSel: public CSelectedObj
{
	OBJECT_BASIC_METHODS(CFragmentSel);

	CPtr<NBuilding::CBuildingGrid> pBuildingGrid;
	CPtr<NBuilding::CBuildInfo> pBInfo;
	CPtr<NDb::CTemplVariant> pVar;
	NBuilding::SBuildFragment *pFragment;
	NBuilding::SBuildFragment frRollback;
	bool bWall;
	int  nFragmentID;
	CPtr<NDb::CConstructionPart> pCP;
	SBound boundbox;
	SBound primarybox; // box for primary part of the solid
	CVec2 ptStartMove;
	CVec3 ptCurrentMove;
	CPtr<ISelection> pSelection;
	CObj<CMemObject> pGBox;
	CObj<CMemObject> pGPrimaryBox;
	vector<NBuilding::SPoint3> parts2update;
	list< CObj<NGScene::CPolyline> > pLines;
	list< CObj<CObjectBase> > parts;
	CPtr<NWorld::IEditorWorld> pWorld;
	bool bDirty;

	void ComputeBoundBox();
	void FindPoints2Update();
	void Update();

public:
	CFragmentSel(): pFragment(0) {}
	CFragmentSel( ISelection *pSel, NWorld::IBuilding *pObj, int nUserID, NWorld::IEditorWorld *pW );

	virtual bool IsInitialized() const;
	virtual bool IsEqual( CObjectBase *pObj, int nUserID ) const;
	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool EndMove( bool bCancel );
	virtual bool Rotate( float fDRotation );
	virtual SBound GetMovingBoundBox() const;
	virtual bool GetMovingPrimaryBoundBox( SBound *pBox ) const;
	virtual SBound GetBoundBox() const { return boundbox; }
	virtual bool GetPrimaryBoundBox( SBound *pBox ) const { *pBox = primarybox; return true; }
	virtual void GetGBoundBox( CMemObject **pBox, CMemObject **pPrimary ) const;
	virtual bool DelayedDelete();
	virtual bool GetInfo( SSelectedInfo *pInfo );
	virtual void OnCopy();
	virtual void OnLBDblClick();
	virtual bool RotateAround( float fRotation, const CVec2 &ptCenter );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual bool IsEqual( int nID ) const { return nFragmentID == nID; }
	virtual int  GetSelectionID() const { return nFragmentID; }
	virtual int  GetMovementDiscrete() const { return 1; }
	virtual int  GetRotationDiscrete() const { return 90; }
	virtual int  GetZDiscrete() const { return 4; }

	void AddSpot( int nSpotID );
	const vector<NBuilding::SPoint3>& GetUpdateParts() const { return parts2update; }
};
/////////////////////////////////////////////////////////////////////////////////////
NBuilding::SBuildFragment* GetFragment( NWorld::IBuilding *pObj, int nUserID );
inline bool IsWall( int nFragmentID ) {	return 0x40000000 & nFragmentID; }
inline bool IsSecondGeometry( int nFragmentID ) { return 0x20000000 & nFragmentID; }
bool ComputeBoundBox( SBound *pBound, int nConstructionID, int nRotationID, bool bSelection = true );
/////////////////////////////////////////////////////////////////////////////////////
} // namespace
/////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGFRAGMENTSEL_H_
