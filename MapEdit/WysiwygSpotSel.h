#ifndef __WYSIWYGSPOTSEL_H_
#define __WYSIWYGSPOTSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\MemObject.h"
#include "WysiwygSelection.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	struct SProjectedSpot;
}
namespace NGScene
{
	class CPolyline;
}
namespace NWorld
{
	class CEditorWorld;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSpot: public CSelectedObj
{
public:
	enum EHandle
	{
		H_TL,
		H_TM,
		H_TR,
		H_LM,
		H_RM,
		H_DL,
		H_DM,
		H_DR,
		H_C,
	};

private:
	vector<CObj<CObjectBase> > rNodes;
	vector<CObj<NGScene::CPolyline> > pPLines;
	int nInitUserID;
	SBound boundbox;
	EHandle handle;
	bool bInitialized;
	NBuilding::SProjectedSpot *pSpot;
	float fRotation;
	bool bMove;
	bool bDraw;
	CVec3 ptStartMove;
	NBuilding::SProjectedSpot startSpot;
	CVec3 ptRotatedShift;
	
	static CObj<CMemObject> pmoCube;
	void ComputeBoundBox();
	void MakeForwMatrix( SHMatrix *pRes, const NBuilding::SProjectedSpot *pSpot ) const;
	CVec3 ComputeStartMove( const SHMatrix &mforw, const NBuilding::SProjectedSpot *pSpot );

	void Draw( NGScene::IGameView *pScene, const NBuilding::SProjectedSpot &spot, bool bSelected );
	static void CreateObjects();

	struct SOnDelete
	{
		vector<CObj<CObjectBase> > pObjects;
		CPtr<NWorld::CEditorWorld> pWorld;

		SOnDelete(){}
		~SOnDelete();
	} onDelete;

protected:
	int nSpotID;
	CPtr<ISelection> pSelection;
	CPtr<NWorld::CEditorWorld> pWorld;
	bool bTerrAlign;
	bool bUseStartMove;

	virtual NBuilding::SProjectedSpot* GetSpot( int nUserID ) const {return 0;}
	NBuilding::SProjectedSpot* GetSpot() const {return pSpot;}
	SBound GetBoundBox( const NBuilding::SProjectedSpot *pSpot ) const;

public:
	CSpot(): pSpot(0), nSpotID(-1) {}
	CSpot( ISelection *pSel, int nUserID );

	virtual bool IsInitialized() const;
	virtual bool IsEqual( CObjectBase *pObj, int nUserID ) const;
	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool EndMove( bool bCancel );
	virtual bool Rotate( float fDRotation );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual SBound GetBoundBox() const;
	virtual SBound GetMovingBoundBox() const;
	virtual bool DelayedDelete() {return false;}
	void SetActiveHandle( int nUserID );
	virtual void Move( const CVec3 &ptMove ); // OnPaste;
	virtual int  GetMovementDiscrete() const { return 0; }
	virtual int  GetRotationDiscrete() const { return 0; }
	virtual int  GetZDiscrete() const { return 0; }
	virtual void ProcessEvent( const string &str );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSpotSel:public CSpot
{
	OBJECT_BASIC_METHODS(CSpotSel);
	CPtr<NBuilding::CBuildInfo> pInfo;
	NBuilding::CBuildingGrid *pGrid;
protected:
	NBuilding::SProjectedSpot* GetSpot( int nUserID ) const;

public:
	CSpotSel() {}
	CSpotSel( ISelection *pSel, CPtrFuncBase<NBuilding::CBuildInfo> *pInfo, NBuilding::CBuildingGrid *pGrid, int nUserID );

	virtual bool Delete();
	virtual bool DelayedDelete();
	virtual void OnLBDblClick();
	virtual bool EndMove( bool bCancel );
	int GetSpotID() const;
	virtual bool IsEqual( int nID ) const { return GetSpotID() == nID; }
	virtual int  GetSelectionID() const { return GetSpotID(); }
	virtual bool RotateAround( float fRotation, const CVec2 &ptCenter );
	virtual bool GetInfo( SSelectedInfo *pInfo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESpotType
{
	ST_WALL,
	ST_TERRAIN,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int  MakeSpotID( ESpotType type, int nIndex, CSpotSel::EHandle hadnle );
void GetSpotID( int nSpotID, ESpotType *pType, int *pnIndex, CSpotSel::EHandle *pHandle );
CVec3 NearestTile( const CVec3 &pt );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__WYSIWYGSPOTSEL_H_