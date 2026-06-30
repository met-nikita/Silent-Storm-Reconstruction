#ifndef __WYSIWYGOBJECTSEL_H_
#define __WYSIWYGOBJECTSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CFinalElement;
	class CObject;
}
namespace NAI
{
	class CGeometryInfo;
}
/////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygMovingSel.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
class CObjectSel: public CMovingSelection
{
	OBJECT_BASIC_METHODS(CObjectSel);
	CDBPtr<NDb::CFinalElement> pFin;
	CObj<NDb::CFinalElement> pRollback;
	CPtr<NDb::CModel> pModel;
	CPtr<NAI::CGeometryInfo> pGeomInfo;
	float fStartRotation;
	float fCurrentRotation;
	SBound bbox;

	SBound ComputeBoundBox() const;
	virtual CVec2 GetCenter() const;

public:
	CObjectSel() {}
	CObjectSel( NDb::CFinalElement *pFin, NDb::CModel *pModel, ISelection *pSelection, const CTPoint<int> &ptSize );

	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool Move( const CVec3 &ptMove );
	virtual bool IsInitialized() const;
	virtual bool IsEqual(CObjectBase *pObj, int nUserID ) const;
	virtual bool DelayedDelete();
	virtual SBound GetBoundBox() const { return bbox; }
	virtual void OnLBDblClick();
	virtual void OnCopy();
	virtual bool GetInfo( SSelectedInfo *pInfo );
	virtual bool Rotate( float fDRotation );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual bool IsEqual( int nID ) const;
	virtual int  GetSelectionID() const;
	virtual void Cancel();
	virtual int  GetMovementDiscrete() const { return 0; }
	virtual int  GetRotationDiscrete() const { return 0; }
	virtual int  GetZDiscrete() const { return 0; }
};
/////////////////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGOBJECTSEL_H_
