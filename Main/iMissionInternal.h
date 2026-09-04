#ifndef __IMISSION_INTERNAL_H__
#define __IMISSION_INTERNAL_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "..\Input\Bind.h"
#include "wInterfaceVisitors.h"
#include "Transform.h"   // CTransformStack (CMission::sTransform by-value member; included by TUs beyond iMission.cpp now)
////////////////////////////////////////////////////////////////////////////////////////////////////
// forward decls for CMission's member types -- this header is now also included by TUs that do not
// pull the defining headers (iMultiPlayerMenu.cpp since the W4.2 CMultiPlayerInterface reparenting);
// all of these classes are saveload-registered, so smart-pointer members link on the incomplete-type
// cast path too.
namespace NUI { class CMissionUI; }
namespace NGScene { class CPolyline; }
namespace NDb { class CString; class CUITexture; }
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class CUICmdExec;
class CUICmdLocatorExec;
//////////////////////////////////////////////////////////////////////////////////////
// CVisibleTracker
//////////////////////////////////////////////////////////////////////////////////////
class CVisibleTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS( CVisibleTracker );
public:
	enum EType
	{
		VISIBLE,
		VISIBLE_FROM
	};
private:
	ZDATA
	bool bUpdated;
	vector< CPtr<IUnitTracker> > selectedUnits;
	vector< CObj<CObjectBase> > nodesSet;
	EType type;
	int nMask;
	CVec3 vFrom;
	float fRadius;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bUpdated); f.Add(3,&selectedUnits); f.Add(4,&nodesSet); f.Add(5,&type); f.Add(6,&nMask); f.Add(7,&vFrom); f.Add(8,&fRadius); return 0; }

public:
	CVisibleTracker() {}
	CVisibleTracker( EType _type ) : type(_type), nMask(0), vFrom(0,0,0), fRadius(0) {}
	CVisibleTracker( EType _type, int _nMask, const CVec3 &_vFrom, float _fRadius ) : type(_type), nMask(_nMask), vFrom(_vFrom), fRadius(_fRadius) {}

	void Update( IMission* pMission );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewBuildingScheme
////////////////////////////////////////////////////////////////////////////////////////////////////
class CViewBuildingScheme: public CSyncDst<NWorld::IVisObj>, public NWorld::IRenderVisitor
{
	NGScene::IGameView *pScene;
	vector<CObj<CObjectBase> > *pRes;

	virtual void VisitObject( int nID, NWorld::IVisObj *p ) { if ( p ) p->Visit( this ); }
public:
	CViewBuildingScheme( CSyncSrc<NWorld::IVisObj> *pSrc, NGScene::IGameView *_pScene, vector<CObj<CObjectBase> > *_pRes );
	virtual void AddBuilding( const SMapBuilding &info );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUpdateBuildingStability
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateBuildingStability: public CSyncDst<NWorld::IVisObj>, public NWorld::IRenderVisitor
{
	virtual void VisitObject( int nID, NWorld::IVisObj *p ) { if ( p ) p->Visit( this ); }
public:
	CUpdateBuildingStability( CSyncSrc<NWorld::IVisObj> *pSrc );
	virtual void AddBuilding( const SMapBuilding &info );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMission
//
// Serialization-convergence W4.2: split onto the retail NGame::CMissionBase (iMission.h) -- the
// scene/sound/render/world/players/interface/camera/desktops/light/time members moved to the base
// (retail base operator& @0x19f3f0, serialized as this table's tag-1 chunk); CMission keeps the
// retail-derived member set with the retail tag table @0x1a03a0 (tags 2..39, gaps at 27/35).
// Old dev saves (flat tags 2..71) are incompatible -- accepted for the campaign.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMission: public CMissionBase
{
	OBJECT_BASIC_METHODS(CMission)
private:
	NInput::CBind bindStartOfTurn, bindEndOfTurn, bindNextEnemy; 
	NInput::CBind bindSaveMenu, bindLoadMenu; 
	NInput::CBind bindFocusUnit, bindAddFloor, bindSubFloor;

	NInput::CBind bindCancel, bindCancelAction, bindContinue, bindPause, bindMainMenu, bindCluesMenu, bindObjectivesMenu, bindGameMenu, bindEndMission;
	NInput::CBind bindHero1, bindHero2, bindHero3, bindHero4, bindHero5, bindHero6, bindSelPrev, bindSelNext;
	NInput::CBind bindMove, bindAttack, bindSetMine, bindSetTrap, bindFirstAid, bindDropCorpse, bindExitPK, bindRotate;
	NInput::CBind bindUseTool;	// retail "usetool" icon-bar command (use the held tool on a target)
	NInput::CBind bindNormalPose, bindCrawlPose, bindCrouchPose, bindRunPose, bindStrafe, bindHide;
	NInput::CBind bindSnipeAttack, bindCollect1AP, bindCollect10AP, bindCollectMaxAP, bindCollectAllAP;
	NInput::CBind bindGrenadeModeThrow, bindGrenadeModeSetTrap;
	NInput::CBind bindSnapShot, bindAimedShot, bindCarefulShot, bindShortBurst, bindLongBurst, bindSnipeShot, bindWeaponPrevMode, bindWeaponNextMode;
	NInput::CBind bindWeaponReload, bindPrevSlot, bindNextSlot, bindItemUnload, bindItemArrange;

	NInput::CBind bindSwitchLighting, bindHideInterface, bindSpecialHideInterface, bindTrailerHideInterface;
	NInput::CBind bindShadows, bindFog, bindHSR, bindShowParticles;
	NInput::CBind bindShowAI, bindExplode, bindCheatTeleport, bindRPGStats, bindShowSchema, bindCheckBuildings, bindShowVision, bindShowRAD;
	NInput::CBind bindTestIsect, bindClickOfDeath, bindExplosion, bindErasePart, bindShowRain, bindShowSnow;
	NInput::CBind bindToggleTransp, bindShowLightmap;
	NInput::CBind bindShowVisibility;

	NGlobal::CCmd cmdTypeCamera, cmdSurrender;
	NGlobal::CCmd cmdSetXPLevel, cmdSummonUnit, cmdUnsummonUnit, cmdGetItem;
	NGlobal::CVar varCheatGodMode, varCheatSeeAll, varCheatTeleport, varCheatAP;

	ZDATA_(CMissionBase)
	int nTemplateID, nVariantID;
	// retail CMission::Initialize @0x200690: the template variant's cut-floor range clamps the
	// floor-switch keys (camera SetCutFloorRange(min, max-1); retail default [-3,4] when the
	// variant leaves MinCutFloor/MaxCutFloor unset, i.e. min >= max). Dev keeps floors on the
	// scene, so the clamp is applied at the bindAddFloor/bindSubFloor handler instead. TRANSIENT.
	int nMinCutFloor = -3;
	int nMaxCutFloor = 4;
	////
	CPtr<NScenario::CScenarioZone> pZone;
	//// State
	bool bUpdated;
	bool bForceUpdateNextFrame;
	CObj<IState> pState;
	CPtr<CObjectBase> pStateTarget;
	vector<CObj<IState> > updatedStatesSet;
	//// information needed to track changes
	// retail track pair (tags 10/11, TrackChanges @0x1fcf60): bTrackIsReady caches IsReady(),
	// bTrackActionExecuted caches IsActionExecuted() (was the dev's bActionExecuted).
	// bRealTime/bHasCommands are dev-extra trackers with no retail member -- TRANSIENT (no tags).
	bool bRealTime;
	bool bHasCommands;
	bool bTrackIsReady;
	bool bTrackActionExecuted;
	CPtr<CObjectBase> pTrackTarget;
	CPtr<CObjectBase> pTrackPlayerInHand;	// retail name (was dev pPlayerInHand), tag 13
	CPtr<NWorld::IPlayer> pTrackPlayer;
	vector< CPtr<NGame::IUnitTracker> > selectedUnits;
	//// Actions info
	vector<SActionInfo> actionsInfoSet;
	//// camera -- dev-extra camera bookkeeping; retail keeps this state on the camera object
	//// (serialized through the base pCamera) or recomputes it per frame. ALL TRANSIENT (no tags).
	float fFOV;
	ECameraType eCameraType;
	// (the old dev-only mission FreezeCamera pose pin -- bFreezeCamera/sFreezePose -- is gone:
	// retail CStateSelection @0x1d73b0/@0x1d75c0 locks the SELECTED camera via ICamera vtbl+0x70
	// Lock instead, and the retail script CameraLock is the camera-side FreezeCamera @0xcffc0)
	// Camera result (recomputed every InternalStep)
	CVec3 vCameraCP;
	SHMatrix sCameraPos;
	CTransformStack sTransform;
	//// trace (retail tags 20/21: the loose dev bTraceOk/sTraceTile/pTraceObject collapsed into
	//// the retail NWorld::STraceResult record -- bTileSet/sTile/pObject; bPointSet/vPoint stay
	//// default: the dev computes the world point live in GetTracePosition(CVec3*))
	CRay rTraceRay;
	NWorld::STraceResult sTraceResult;
	//// Active interface command -- TRANSIENT: retail serializes NO general executor on CMission
	//// (only the base's dedicated pExecLocator slot); it is rebuilt from the world UI-command queue.
	CPtr<CUICmdExec> pCmdExec;
	//// interface
	int nPanelsState;
	EActionIconsSet eActionIconsSet;
	// TRANSIENT: retail CMission has NO pMissionUI member -- the mission desktop lives at the
	// BOTTOM of the base desktopWindowsList (Initialize pushes it; base tag 23 round-trips it
	// through saves, so GetDesktop() works after a load). Live-session convenience cache only.
	CObj<NUI::CMissionUI> pMissionUI;
	//// crap
	// retail CMission's OWN pLightSource (tag 24) -- retail carries a second light slot on the
	// derived class, distinct from CMissionBase::pLightSource (base tag 25, the SetLightMode slot).
	// No dev writer yet; serialized for format parity. NOTE: intentionally shadows the base member
	// (retail has both); SetLightMode (base) writes the BASE slot.
	CObj<CObjectBase> pLightSource;
	CObj<CVisibleTracker> pVisibleTracker;
	vector<CObj<CObjectBase> > buildingSchemas;
	CObj<CObjectBase> pIntersectHolder;
	CObj<NGScene::CPolyline> pIntersectLineHolder;
	CVec3 vPrevCameraPosition;
	int nFramesSameCameraPosition;
	// retail tag 26: the test-weather-effect VECTOR (retail ShowWeatherEffect @0x201c60 clears the
	// vector, then pushes the one new effect handle).
	vector<CObj<CObjectBase> > testWeatherEffect;
	// LUA convergence: the single, replaceable scene-wide ambient effect (SetAmbientEffect). The retail keeps it
	// in a scene slot (CGameView::SetAmbientEffect @0x1895f0); the dev parks the live render handle here instead.
	// Runtime-only (a render handle, like CUICmdPlayEffect::pHandle) -- NOT serialized.
	CObj<CObjectBase> pAmbientEffect;
	// SetLeaveZoneMode: bLeaveBlockedByScript blocks the player from leaving the zone; pBlockReason is
	// the DB string of the "can't leave" message (retail tag 33; the dev previously kept the raw int id).
	// Consumed by CanLeaveZone (retail @0x1fcbd0); set by the CUICmdLeaveZoneMode dispatch.
	bool bLeaveBlockedByScript = false;
	CDBPtr<NDb::CString> pBlockReason;
	// OnPlayerLose one-shot latch (retail @CMission bLoseSignalSended, tag 31): queue the delayed lose
	// hook once per mission (GameStep wraps it in CCmdDelayedCallGameOver(4000) -- see the W3.3 retail
	// plumbing in CWorld; the old tGameOverTime/bLoseMenuShown 4s-timer approximation was removed with it).
	bool bLoseSignalSended = false;
	// BeginSequence/EndSequence nesting depth (retail name nSequence, tag 17). Scripts nest sequences
	// (e.g. Common.l DelayGameStart() opens one via BeginSequence(true) and StartGame() closes one via
	// EndSequence(), around the script's own BeginSequence/EndSequence). Retail (@0x1fd8c0) stacks ONE
	// movieUI PER begin and EndSequence hides the TOP one; the depth gates only the camera-limits save
	// (1) / restore (1->0). The per-begin camera-pose stack is the base's cameraPosStack (base tag 20).
	int nSequence = 0;
	// SetFirstMissionMode(b) (retail CMission @0x1fd8c0 / SetPanelState @0x1fb7e0): while set, SetPanelState
	// masks off the medals/biography bits (retail literal 0x14 in RETAIL bit values) -- the "first mission"
	// HUD keeps those panels hidden. Save/loaded (retail tag 34, iAutoPlay.c:3716).
	bool bSpecialFirstMissionMode = false;
	// retail tag 36: the scripted scenario-failure latch. Retail GameStep fires OnPlayerLose WITHOUT the
	// CCmdDelayedCallGameOver wrapper when this is set (scenario failure = immediate game over). Producer:
	// CanLeaveZone @0x1fcbd0 (via CScenarioTracker::CanLeaveZone's unwinnable test).
	bool bScenarioGameOver = false;
	// EnableFeature("reenter") (retail CMission::ExecWorldCommand @0x1fd8c0): allow re-entering this zone's
	// templates. Save/loaded (retail tag 39, iAutoPlay.c:3722). Consumed by CMission::Terminate (@0x1fbc30): the
	// zone world is saved per zone+template ("%d_%d.sav") when IsBase() || IsLinkedZone() || this flag; Initialize
	// (@0x200690) loads that file on re-entry and restores via CWorld::CreateRestored (@0x36e100: per-building
	// Update() refresh + live-CGlobalGame rebind -- the save's weak global-game ref loads dangling).
	bool bEnableFeatureReenter = false;
	// retail CMission pPWLImage (CDBPtr<NDb::CUITexture>, tag 37): the PreWorldLoad splash threaded from
	// CICBeginMission::Exec -> Initialize @0x200690 (this->pPWLImage = param_6). Carries the chapter's
	// loading background into the mission so a zone-reenter can re-show it.
	CDBPtr<NDb::CUITexture> pPWLImage;
	// retail tag 38 (+ TRANSIENT tLastCurrentPlayerChange @+0x58c): the previous world current-player;
	// on a change, GameStep @0x2017a0 stamps tLastCurrentPlayerChange with the UI clock (writer at the
	// top of InternalStep here). Feeds the CanDoCommand(CCommand*) end-of-turn 2s cooldown @0x1fbf10.
	CPtr<NWorld::IPlayer> pPrevCurrentPlayer;
	STime tLastCurrentPlayerChange = 0;
public:
	// retail @0x1fcbd0 (mission vtbl+0xe0): full exit-possibility query -- scenario-tracker clue gate,
	// script block (pBlockReason), map-safe-zone border test, loser/winner/realtime gates; fills
	// *pwsReason with the localized status line and refreshes bScenarioGameOver.
	virtual bool CanLeaveZone( wstring *pwsReason );
	NDb::CString* GetLeaveBlockReason() const { return pBlockReason; }
	virtual bool IsSpecialFirstMissionMode() const { return bSpecialFirstMissionMode; }	// retail @0x19def0 (mission vtbl+0xf8)
	void OnSnapshotRestored();	// restart.sav in-place resume: rebuild building shells + camera height source
	// retail NGame::CMission::operator& @0x1a03a0 -- tag 1 = the CMissionBase chunk (non-virtual
	// base chain), own tags 2..39 with the retail gaps at 27/35 preserved
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CMissionBase*)this); f.Add(2,&nTemplateID); f.Add(3,&nVariantID); f.Add(4,&pZone); f.Add(5,&bUpdated); f.Add(6,&bForceUpdateNextFrame); f.Add(7,&pState); f.Add(8,&pStateTarget); f.Add(9,&updatedStatesSet); f.Add(10,&bTrackIsReady); f.Add(11,&bTrackActionExecuted); f.Add(12,&pTrackTarget); f.Add(13,&pTrackPlayerInHand); f.Add(14,&pTrackPlayer); f.Add(15,&selectedUnits); f.Add(16,&actionsInfoSet); f.Add(17,&nSequence); f.Add(18,&nFramesSameCameraPosition); f.Add(19,&vPrevCameraPosition); f.Add(20,&rTraceRay); f.Add(21,&sTraceResult); f.Add(22,&nPanelsState); f.Add(23,&eActionIconsSet); f.Add(24,&pLightSource); f.Add(25,&pIntersectHolder); f.Add(26,&testWeatherEffect); f.Add(28,&pVisibleTracker); f.Add(29,&pIntersectLineHolder); f.Add(30,&buildingSchemas); f.Add(31,&bLoseSignalSended); f.Add(32,&bLeaveBlockedByScript); f.Add(33,&pBlockReason); f.Add(34,&bSpecialFirstMissionMode); f.Add(36,&bScenarioGameOver); f.Add(37,&pPWLImage); f.Add(38,&pPrevCurrentPlayer); f.Add(39,&bEnableFeatureReenter); return 0; }

private:
	void InternalStep();

protected:
	// NO GetTime() override: retail's mission GetTime() IS the raw main-loop clock (@0x1f4e00).
	// nDeltaTime (the script skip-part fast-forward) is applied ONLY at Step's UpdateViewWorld call
	// site (retail @0x5a3c24), so it never leaks into the sound/UI clocks.
	void SetDeltaTime( STime _nDeltaTime ) { nDeltaTime = _nDeltaTime; }
	STime GetDeltaTime() const { return nDeltaTime; }
	bool TrackChanges();
	void UpdateState();
	void UpdateSound();
	void WeaponReload();
	void SelectSlot( int nInc );
	void NewWeaponMode( NDb::EShootMode eMode );
	void NewGrenadeMode( NRPG::EGrenadeMode eMode );
	void SelectWeaponMode( int nInc );
	void UpdateActionsInfo();
	void TraceCursor();
	void UnitCollectAP( NWorld::ECollectSnipeAP eAP );
	void ExecWorldCommands();

	void SaveWorld( const string &szFile );
	void LoadWorld( const string &szFile );

	void TestIntersection();
	void ShowWeatherEffect( int nID );
	void ClickOfDeath();
	void Explosion();
	void ErasePartUnderCursor();
	void ShowVisibleFrom();

public:
	CMission();

	// The trivial accessors + player/desktop/light/camera-focus family moved to CMissionBase
	// (retail placement -- see iMission.h); CMission keeps only its real overrides below.
	bool Initialize( int nTemplateID, int nVariantID, NScenario::CScenarioZone *pZone, const vector<string> &params, NRPG::CGlobalGame *pGlobalGame, NDb::CUITexture *pPWLImage = 0 );
	void Terminate();

	// dev-extra bForceUpdateNextFrame latch on top of the retail base Command/DoEvent bodies
	void Command( NWorld::CCommand *pCmd );
	void Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly = true );
	void DoEvent( NWorld::CCommand *pCmd );

	bool IsUpdated() const;

	IState* GetState() const;
	bool CommandState( IState *pState );
	void ResetState();
	void SetUpdatedStates( const vector<CObj<IState> > &updatedStatesSet );
	CObjectBase* GetStateTarget( bool *pbFrom3DWorld = 0 ) const;
	void SetStateTarget( CObjectBase* pObject );

	int GetUnitsState();
	NWorld::CUnit::EState GetUnitsWorldState();
	void GetActionInfo( EUnitAction eAction, SActionInfo *pInfo );
	bool CanDoCommand( NWorld::CCommand *pCmd );	// retail @0x1fbf10 (mission vtbl+0xa8)
	void CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo );
	NWorld::EUnitCommandResult CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget = false, int *pnMinAP = 0, int *pnMaxAP = 0, bool *pbEnoughAPToStart = 0 );

	float GetCameraFOV() const;
	void GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits );
	void SetCameraParams( ECameraType eType, float fFOV, const ICamera::SCameraLimits &limits );
	int GetCutFloor();
	void SetCutFloor( int nFloor );
	const CVec3& GetCameraCP() const;
	const SHMatrix& GetCameraPos() const;
	const CTransformStack& GetCameraTransform() const;

	CObjectBase* GetTraceObject() const;
	CRay GetTraceRay() const;
	bool GetTracePosition( CVec3 *pPos ) const;
	bool GetTracePosition( NAI::SPosition *pPos ) const;

	bool IsSequence() const;
	// dev fallback: the live-session pMissionUI cache when the desktop list is empty (pre-Initialize)
	NUI::CDesktopWindow* GetDesktop() const;
	////
	EActionIconsSet GetActionIconsSet() const;
	void SetActionIconsSet( EActionIconsSet eMode );
	////
	int GetPanelState( int nMask ) const;
	void SetPanelState( int nMask, bool bState );
	// retail mission vtbl+0x144: the zone this mission was started with (null for the tutorial)
	NScenario::CScenarioZone* GetScenarioZone() const { return pZone; }

	void Step();
	// (OnGetFocus/OnLostFocus live on CMissionBase -- retail @0x1a19c0/@0x1a19e0, no CMission override)
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
