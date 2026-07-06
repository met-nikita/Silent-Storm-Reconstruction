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
namespace NSound
{
	class ISoundScene;
}
class CTransformStack;
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
	// release IShowUnit vtbl+0x14 takes (lipsync seq, expression seq) -- retail CUnitView::SetSequence
	// @0x1bf030 forwards both; the expression is the head animator's MASK entry. Defaulted so the
	// one-sequence callers stay untouched.
	virtual void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 ) = 0;
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
	virtual void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 ) = 0;
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
	// retail IRenderGame vtbl+0x30 @0x2cb1c0: advance BOTH sound mixers (world pSound + fog-gated
	// pUnitSounds). Retail threads a bAdvanceTime bool; dev CRenderSound::Update advances
	// unconditionally, so the bool is folded (matches the old dev pRenderSound->Update call sites).
	virtual void UpdateSound( CTransformStack *pTS, STime currentTime ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CreateRenderGame @0x2d3aa0/ctor @0x2ceae0 takes the sound scene too: the render sounds are
// OWNED by CRenderGame (pSound over GetActive, pUnitSounds over GetUnits) so UpdateVisible can
// re-point pUnitSounds at the visibility-filtered source (voice fog-of-war).
IRenderGame* CreateRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene, NSound::ISoundScene *_pSoundScene );
IRenderGame* CreateDummyRenderGame( NGScene::IGameView *_pScene );
////
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NRPG::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame = 0 );
// release @0x2ce9c0: the NWorld overload threads THREE bools into the CFakeWorldUnit ctor (@0x2ce440 params
// 4-6: bItems, bPlayIdle, bShowCap). Defaults reproduce the old dev path (item pose flags on, static POSE,
// cap shown) so existing callers keep their behavior. (The NRPG overload @0x2ce730 threads the same bools
// into CFakeRPGUnit; no dev caller needs them yet, so its signature is left alone.)
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NWorld::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame = 0, bool bItems = true, bool bPlayIdle = false, bool bShowCap = true );
IShowUnitHead* CreateShowUnitHead( NGScene::IGameView *pView, NWorld::CUnit *pUnit, NLSHead::CHeadsController *pController, CFuncBase<SFBTransform> *pTransform );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif