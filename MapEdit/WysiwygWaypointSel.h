#ifndef __WYSIWYGWAYPOINTSEL_H_
#define __WYSIWYGWAYPOINTSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\MemObject.h"
#include "WysiwygSelection.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class CWaypoint;
}
namespace NGScene
{
	class CPolyline;
}
namespace NDb
{
	class CTemplVariant;
}
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWaypoint: public CSelectedObj
{
	OBJECT_BASIC_METHODS(CWaypoint);
private:
	vector<CObj<CObjectBase> > rNodes;
	SBound boundbox;
	CVec3 ptStartMove;
	CVec3 ptCurrentMove;
	bool bInitialized;
	
	static CObj<CMemObject> pmoFlag;
	void ComputeBoundBox();

	//void Draw( NGScene::IGameView *pScene, const NBuilding::SProjectedSpot &spot, bool bSelected );
	static void CreateObjects();

protected:
	int nWaypointID;
	CPtr<ISelection> pSelection;
	CPtr<NDb::CTemplVariant> pVar;


public:
	CWaypoint(): bInitialized(false), nWaypointID(-1) {}
	CWaypoint( int nWaypointID, ISelection *pSelection, NDb::CTemplVariant *pVar );

	static NAI::CWaypoint* GetWaypoint( int nID );

	virtual bool IsInitialized() const;
	virtual bool IsEqual( CObjectBase *pObj, int nUserID ) const;
	virtual void StartMove( const CVec2 &ptCursor );
	// ptMove - вектор в горизонтальной плоскости от точки где была нажата левая кнопка до текущей позиции
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool EndMove( bool bCancel );
	virtual bool Rotate( float fDRotation );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual SBound GetBoundBox() const;
	virtual SBound GetMovingBoundBox() const;
	virtual bool Delete();
	virtual bool DelayedDelete();
	virtual void OnLBDblClick();
	virtual bool IsEqual( int nID ) const { return nWaypointID == nID; }
	virtual int  GetSelectionID() const { return nWaypointID; }
	virtual int  GetMovementDiscrete() const { return 0; }
	virtual int  GetRotationDiscrete() const { return 0; }
	virtual int  GetZDiscrete() const { return 0; }
	virtual bool GetInfo( SSelectedInfo *pInfo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__WYSIWYGWAYPOINTSEL_H_