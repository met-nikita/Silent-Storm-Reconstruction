#ifndef __WYSIWYGLADDERSEL_H_
#define __WYSIWYGLADDERSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CTemplVariant;
}
namespace NBuilding
{
	class CBuildInfo;
	struct SLadder;
}
/////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygMovingSel.h"
#include "..\Main\BuildingInfo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
class CLadderSel: public CMovingSelection
{
	OBJECT_BASIC_METHODS(CLadderSel);
	int nUserID;
	NBuilding::CBuildInfo *pBInfo;
	NBuilding::SLadder *pLadder;
	NBuilding::SLadder rollback;
protected:
	virtual CVec2 GetCenter() const;

public:
	CLadderSel() {}
	CLadderSel( ISelection *pSelection, const CTPoint<int> &ptSize, int nUserID, NBuilding::CBuildInfo *pInfo );

	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool Move( const CVec3 &ptMove );
	virtual bool IsInitialized() const;
	virtual bool IsEqual(CObjectBase *pObj, int nUserID ) const;
	virtual bool DelayedDelete();
	virtual SBound GetBoundBox() const;
	virtual void OnLBDblClick();
	virtual void OnCopy();
	virtual bool GetInfo( SSelectedInfo *pInfo );
	virtual bool Rotate( float fDRotation );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual bool IsEqual( int nID ) const;
	virtual int  GetSelectionID() const;
	virtual void Cancel() {}
	virtual int  GetMovementDiscrete() const { return 1; }
	virtual int  GetRotationDiscrete() const { return 90; }
	virtual int  GetZDiscrete() const { return 0; }
};
/////////////////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGLADDERSEL_H_
