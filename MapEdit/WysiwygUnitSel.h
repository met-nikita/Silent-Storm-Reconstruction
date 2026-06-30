#ifndef __WYSIWYGUNITSEL_H_
#define __WYSIWYGUNITSEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CUnit;
	class CModel;
}
/////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygMovingSel.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
/////////////////////////////////////////////////////////////////////////////////////
class CUnitSel: public CMovingSelection
{
	OBJECT_BASIC_METHODS(CUnitSel);
	CDBPtr<NDb::CUnit> pUnit;
	float fStartRotation;
	float fCurrentRotation;
	CObj<NDb::CUnit> pRollback;

	CVec3 GetPos() const;
protected:
	virtual CVec2 GetCenter() const;

public:
	CUnitSel() {}
	CUnitSel( NDb::CUnit *pUnit, ISelection *pSelection, const CTPoint<int> &ptSize );

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
	virtual bool IsEqual( int nID ) const { if ( IsValid( pUnit ) ) return pUnit->GetRecordID() == nID; return false; }
	virtual int  GetSelectionID() const { if ( IsValid( pUnit ) ) return pUnit->GetRecordID(); return -1; }
	virtual void Cancel();
	virtual int  GetMovementDiscrete() const { return 1; }
	virtual int  GetRotationDiscrete() const { return 0; }
	virtual int  GetZDiscrete() const { return 0; }
};
/////////////////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGUNITSEL_H_
