#ifndef __WYSIWYGSUBTEMPLATESEL_H_
#define __WYSIWYGSUBTEMPLATESEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CRectangle;
}
namespace NGScene
{
	class CPolyline;
}
/////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygMovingSel.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
const float SUBTEMPL_DHEIGHT = 0.05f;
/////////////////////////////////////////////////////////////////////////////////////
class CSubTemplateSel: public CMovingSelection
{
	OBJECT_BASIC_METHODS(CSubTemplateSel);
	int nUserID;
	NDb::CRectangle *pRect;
	vector<CObj<NGScene::CPolyline> > lines;
	SBound bbox;
	float fStartRotation;
	float fCurrentRotation;
	CVec2 ptStartCenter;
	CVec2 ptRotatedShift; //CRAP
	CVec3 ptCurrentMove;
	CObj<NDb::CRectangle> pRollback;
protected:
	virtual CVec2 GetCenter() const;

public:
	CSubTemplateSel() {}
	CSubTemplateSel( ISelection *pSelection, const CTPoint<int> &ptSize, int nUserID );

	virtual void StartMove( const CVec2 &ptCursor );
	virtual bool Move( const CVec3 &ptMove );
	virtual bool IsInitialized() const;
	virtual bool IsEqual(CObjectBase *pObj, int nUserID ) const;
	virtual bool DelayedDelete();
	virtual SBound GetBoundBox() const;
	virtual SBound GetMovingBoundBox() const;
	virtual void OnLBDblClick();
	virtual void OnCopy();
	virtual bool GetInfo( SSelectedInfo *pInfo );
	virtual bool Rotate( float fDRotation );
	virtual bool Draw( NGScene::IGameView *pScene, bool bShow );
	virtual bool TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor );
	virtual bool IsEqual( int nID ) const;
	virtual int  GetSelectionID() const;
	virtual void Cancel();
	virtual int  GetMovementDiscrete() const { return 0; }
	virtual int  GetRotationDiscrete() const { return 0; }
	virtual int  GetZDiscrete() const { return 0; }

	static SBound ComputeBBox( NDb::CRectangle *pRect );
};
/////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGSUBTEMPLATESEL_H_
