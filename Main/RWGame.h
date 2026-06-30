#ifndef __RWGAME_H_
#define __RWGAME_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CModel;
	class CSequence;
	class CAnimation;
}
namespace NWorld
{
	class IWorld;
	struct IVisObj;
	class IPlayer;
}
namespace NGScene
{
	class IGameView;
}
namespace NRPG
{
	class CUnit;
	class IUnitMissionInfo;
}
namespace NLSHead
{
	class CHeadsController;
	class CHeadInfo;
}
namespace NRender
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IShowUnit: public CObjectBase
{
public:
	virtual void Update( float fAngle ) = 0;
	virtual void SetSequence( NDb::CSequence *pSequence ) = 0;
	// Play a body animation (idle/talk gesture) on the shown unit -- used by the unit-panel face.
	// Default no-op (only CShowWorldUnit drives a skeletal animator); pass pAnim=0 to release.
	virtual void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop ) {}
	// Live head-morph (advanced FaceGen editor): push a named tension param onto the shown head,
	// read one back, and bake the current head into a CHeadInfo. Default no-ops (only CShowRPGUnit
	// drives a morphable preview unit).
	virtual void SetLSHeadParam( const char *szName, float fValue ) {}
	virtual float GetLSHeadParam( const char *szName ) { return 0; }
	virtual NLSHead::CHeadInfo* CreateLSHeadInfo() { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IShowUnitHead: public CObjectBase
{
public:
	virtual void SetSequence( NDb::CSequence *pSequence ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IRenderGame: public CObjectBase
{
public:
	virtual CObjectBase* Select( CObjectBase *pSelect, const CVec4 &vColor = CVec4( 0, 1, 1, 1 ) ) = 0;

	virtual CCTime* GetTime() = 0;
	virtual NLSHead::CHeadsController* GetHeadController() const = 0;

	virtual void UpdateViewWorld( bool bAdvanceTime, STime currentTime, NWorld::IPlayer *pViewFrom, bool bShowAllUnits = false ) = 0;
	virtual void FastUpdate( STime currentTime ) = 0;
	virtual void ResetTiming() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderGame* CreateRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene );
IRenderGame* CreateDummyRenderGame( NGScene::IGameView *_pScene );
////
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NRPG::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame = 0 );
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NWorld::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame = 0 );
IShowUnitHead* CreateShowUnitHead( NGScene::IGameView *pView, NWorld::CUnit *pUnit, NLSHead::CHeadsController *pController, CFuncBase<SFBTransform> *pTransform );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif