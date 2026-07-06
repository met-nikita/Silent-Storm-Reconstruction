#ifndef __IMISSION_INTERNAL_H__
#define __IMISSION_INTERNAL_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "..\Input\Bind.h"
#include "wInterfaceVisitors.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class CUICmdExec;
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
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMission: public IMission
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

	ZDATA
	int nTemplateID, nVariantID;
	// retail CMission::Initialize @0x200690: the template variant's cut-floor range clamps the
	// floor-switch keys (camera SetCutFloorRange(min, max-1); retail default [-3,4] when the
	// variant leaves MinCutFloor/MaxCutFloor unset, i.e. min >= max). Dev keeps floors on the
	// scene, so the clamp is applied at the bindAddFloor/bindSubFloor handler instead.
	int nMinCutFloor = -3;
	int nMaxCutFloor = 4;
	CPtr<NRPG::CGlobalGame> pGlobalGame;
	////
	CPtr<NScenario::CScenarioZone> pZone;
	//// graphics
	CObj<NGScene::IGameView> pScene;
	CObj<NSound::ISoundScene> pSoundScene;
	// (the render-sound mixers moved INTO the render game -- retail CRenderGame owns
	// pSound/pUnitSounds so UpdateVisible can fog-gate the unit-sound source)
	CObj<NRender::IRenderGame> pRender;
	//// world
	bool bPause;
	CObj<NWorld::IWorld> pWorld;
	//// players
	CPtr<IPlayerTracker> pActivePlayer;
	vector< CObj<IPlayerTracker> > playersSet;
	//// interface
	bool bHideInterface;
	bool bSpecialHideInterface;
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	//// State
	bool bUpdated;
	bool bForceUpdateNextFrame;
	CObj<IState> pState;
	CPtr<CObjectBase> pStateTarget;
	vector<CObj<IState> > updatedStatesSet;
	//// information needed to track changes
	bool bRealTime;
	bool bHasCommands;
	bool bActionExecuted;
	CPtr<CObjectBase> pTrackTarget;
	CPtr<CObjectBase> pPlayerInHand;
	CPtr<NWorld::IPlayer> pTrackPlayer;
	vector< CPtr<NGame::IUnitTracker> > selectedUnits;
	//// Actions info
	vector<SActionInfo> actionsInfoSet;
	//// camera
	float fFOV;
	ECameraType eCameraType;
	ICamera::SCameraLimits cameraLimits;
	CObj<ICamera> pCamera;
	CPtr<IPlayerTracker> pCameraOwner;
	// freeze pose
	bool bFreezeCamera;
	ICamera::SCameraPos sFreezePose;
	// Camera result
	CVec3 vCameraCP;
	SHMatrix sCameraPos;
	CTransformStack sTransform;
	//// trace
	bool bTraceOk;
	CRay rTraceRay;
	NAI::SPosition sTraceTile;
	CPtr<CObjectBase> pTraceObject;
	//// Active interface command
	CPtr<CUICmdExec> pCmdExec;
	//// interface
	int nPanelsState;
	bool bWaitForPartFinished;
	EActionIconsSet eActionIconsSet;
	CObj<NUI::CMissionUI> pMissionUI;
	list<CObj<NUI::CDesktopWindow> > desktopWindowsList;
	//// crap
	int nLightMode;
	bool bCheatVisibility;
	CObj<CObjectBase> pLightSource;
	CObj<CVisibleTracker> pVisibleTracker;
	vector<CObj<CObjectBase> > buildingSchemas;
	CObj<CObjectBase> pIntersectHolder;
	CObj<NGScene::CPolyline> pIntersectLineHolder;
	CVec3 vPrevCameraPosition;
	int nFramesSameCameraPosition;
	STime nDeltaTime;
	CDBPtr<NDb::CMusic> pCombatMelody;
	CObj<CObjectBase> pTestWeatherEffect;
	// LUA convergence: the single, replaceable scene-wide ambient effect (SetAmbientEffect). The retail keeps it
	// in a scene slot (CGameView::SetAmbientEffect @0x1895f0); the dev parks the live render handle here instead.
	// Runtime-only (a render handle, like CUICmdPlayEffect::pHandle) -- NOT serialized.
	CObj<CObjectBase> pAmbientEffect;
	// LUA convergence (hint machinery): the script SetTutorialMode(b) command sets this; the ShowHint dispatch
	// gate reads it (retail CMissionBase::ExecWorldCommand @0x1a30c0 tests CMissionBase+bTutorialMode) so hints
	// always show in a tutorial even when the "ui_showhints" game option is off.
	bool bTutorialMode;
	// SetLeaveZoneMode: bLeaveBlockedByScript blocks the player from leaving the zone; nLeaveBlockReason is
	// the DB string id of the "can't leave" message. Retail CMission::CanLeaveZone @0x1fcbd0 gates on
	// `if (bLeaveBlockedByScript == false)`. DOCUMENTED GAP: the dev has no in-mission exit detector that
	// calls a CanLeaveZone gate (the dev leaves a zone via the chapter map, CExitZoneSector) -- so the flag
	// is recorded + save-correct + queryable (CanLeaveZone()), but its in-mission consumer is the still-
	// absent leave-zone subsystem. Set faithfully by the CUICmdLeaveZoneMode dispatch.
	bool bLeaveBlockedByScript = false;
	int nLeaveBlockReason = -1;
	// OnPlayerLose one-shot latch (retail @CMission bLoseSignalSended): fire the lua lose hook once per mission.
	bool bLoseSignalSended = false;
	// BeginSequence/EndSequence nesting depth. Scripts nest sequences (e.g. Common.l DelayGameStart() opens
	// one via BeginSequence(true) and StartGame() closes one via EndSequence(), around the script's own
	// BeginSequence/EndSequence). Retail (@0x1fd8c0 "nSequence") stacks ONE movieUI PER begin and
	// EndSequence hides the TOP one; the depth gates only the camera-limits save (1) / restore (1->0).
	int nSequenceDepth = 0;
	// retail CMission::ExecWorldCommand @0x1fd8c0: the per-BeginSequence camera-pose stack. Every begin
	// pushes the current camera pose; EndSequence pops -- bRestoreCamera ? SetPlacement(popped) : commit
	// the CURRENT (cutscene-end) pose as the active player's gameplay camera (SetCamera) so the camera
	// doesn't jump back after a scripted move. Serialized (dev-appended tag 69) so a save mid-sequence
	// keeps the stack.
	vector<ICamera::SCameraPos> sequenceCameraPoses;
	// SetFirstMissionMode(b) (retail CMission @0x1fd8c0 / SetPanelState @0x1fb7e0): while set, SetPanelState
	// masks off the 0x14 panel bits -- the "first mission" HUD keeps those panels hidden. Save/loaded
	// (retail iAutoPlay.c:3716).
	bool bSpecialFirstMissionMode = false;
	// EnableFeature("reenter") (retail CMission::ExecWorldCommand @0x1fd8c0): allow re-entering this zone's
	// templates. Save/loaded (retail iAutoPlay.c:3722). Consumed by CMission::Terminate (@0x1fbc30): the zone
	// world is saved per zone+template ("%d_%d.sav") when IsBase() || IsLinkedZone() || this flag; Initialize
	// (@0x200690) loads that file on re-entry and restores via CWorld::CreateRestored (@0x36e100: per-building
	// Update() refresh + live-CGlobalGame rebind -- the save's weak global-game ref loads dangling).
	bool bEnableFeatureReenter = false;
public:
	bool CanLeaveZone() const { return !bLeaveBlockedByScript; }	// honors SetLeaveZoneMode (retail @0x1fcbd0 gate)
	int GetLeaveBlockReason() const { return nLeaveBlockReason; }
	bool IsSpecialFirstMissionMode() const { return bSpecialFirstMissionMode; }	// retail @0x19def0
	void OnSnapshotRestored();	// restart.sav in-place resume: rebuild building shells + camera height source
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nTemplateID); f.Add(3,&nVariantID); f.Add(4,&pGlobalGame); f.Add(5,&pZone); f.Add(6,&pScene); f.Add(7,&pSoundScene); f.Add(8,&pRender); f.Add(10,&bPause); f.Add(11,&pWorld); f.Add(12,&pActivePlayer); f.Add(13,&playersSet); f.Add(14,&bHideInterface); f.Add(15,&bSpecialHideInterface); f.Add(16,&pCursor); f.Add(17,&pInterface); f.Add(18,&bUpdated); f.Add(19,&bForceUpdateNextFrame); f.Add(20,&pState); f.Add(21,&pStateTarget); f.Add(22,&updatedStatesSet); f.Add(23,&bRealTime); f.Add(24,&bHasCommands); f.Add(25,&bActionExecuted); f.Add(26,&pTrackTarget); f.Add(27,&pPlayerInHand); f.Add(28,&pTrackPlayer); f.Add(29,&selectedUnits); f.Add(30,&actionsInfoSet); f.Add(31,&fFOV); f.Add(32,&eCameraType); f.Add(33,&cameraLimits); f.Add(34,&pCamera); f.Add(35,&pCameraOwner); f.Add(36,&bFreezeCamera); f.Add(37,&sFreezePose); f.Add(38,&vCameraCP); f.Add(39,&sCameraPos); f.Add(40,&sTransform); f.Add(41,&bTraceOk); f.Add(42,&rTraceRay); f.Add(43,&sTraceTile); f.Add(44,&pTraceObject); f.Add(45,&pCmdExec); f.Add(46,&nPanelsState); f.Add(47,&bWaitForPartFinished); f.Add(48,&eActionIconsSet); f.Add(49,&pMissionUI); f.Add(50,&desktopWindowsList); f.Add(51,&nLightMode); f.Add(52,&bCheatVisibility); f.Add(53,&pLightSource); f.Add(54,&pVisibleTracker); f.Add(55,&buildingSchemas); f.Add(56,&pIntersectHolder); f.Add(57,&pIntersectLineHolder); f.Add(58,&vPrevCameraPosition); f.Add(59,&nFramesSameCameraPosition); f.Add(60,&nDeltaTime); f.Add(61,&pCombatMelody); f.Add(62,&pTestWeatherEffect); f.Add(63,&bTutorialMode); f.Add(64,&bLeaveBlockedByScript); f.Add(65,&nLeaveBlockReason); f.Add(66,&nSequenceDepth); f.Add(67,&bSpecialFirstMissionMode); f.Add(68,&bEnableFeatureReenter); f.Add(69,&sequenceCameraPoses); return 0; }

private:
	void InternalStep();

protected:
	virtual const STime GetTime() { return IInterfaceObject::GetTime() + nDeltaTime; }
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
	void SetLightMode( int _nLightMode );
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

	bool Initialize( int nTemplateID, int nVariantID, NScenario::CScenarioZone *pZone, const vector<string> &params, NRPG::CGlobalGame *pGlobalGame );
	void Terminate();

	void Command( NWorld::CCommand *pCmd );
	void Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly = true );
	void StopAction();
	bool IsReady() const;
	bool IsActionExecuted() const;
	bool IsRealTime() const;

	void PauseGame( bool bState );
	bool IsGamePaused() const;

	IPlayerTracker* GetActivePlayer() const;
	void SetActivePlayer( IPlayerTracker *pPlayer );
	void GetPlayers( vector< CPtr<IPlayerTracker> > *pPlayersSet ) const;

	void GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;
	void GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;

	int CountSelected();
	void Select( NWorld::CUnit *pUnit, bool bAdditive );
	void SelectNext();
	void SelectPrev();

	bool IsUpdated() const;

	IState* GetState() const;
	bool CommandState( IState *pState );
	void ResetState();
	void SetUpdatedStates( const vector<CObj<IState> > &updatedStatesSet );
	CObjectBase* GetStateTarget() const;
	void SetStateTarget( CObjectBase* pObject );

	int GetUnitsState();
	NWorld::CUnit::EState GetUnitsWorldState();
	void GetActionInfo( EUnitAction eAction, SActionInfo *pInfo );
	void CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo );
	NWorld::EUnitCommandResult CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget = false, int *pnAP = 0 );

	ICamera* GetCamera() const;
	float GetCameraFOV() const;
	void GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits );
	void SetCameraParams( ECameraType eType, float fFOV, const ICamera::SCameraLimits &limits );
	void FreezeCamera( bool bState );
	void FocusCameraOnUnit( NWorld::CUnit *pUnit );
	int GetCutFloor();
	void SetCutFloor( int nFloor );
	const CVec3& GetCameraCP() const;
	const SHMatrix& GetCameraPos() const;
	const CTransformStack& GetCameraTransform() const;

	CObjectBase* GetTraceObject() const;
	CRay GetTraceRay() const;
	bool GetTracePosition( CVec3 *pPos ) const;
	bool GetTracePosition( NAI::SPosition *pPos ) const;

	NUI::ICursor* GetCursor() const;
	NUI::CInterface* GetInterface() const;
	bool IsInterfaceHidden() const;
	bool IsSequence() const;
	////
	void SetWaitForPartFinished( bool bState );
	bool IsWaitForPartFinished() const;
	////
	void PopDesktop( NUI::CDesktopWindow *pDesktop );
	void PushDesktop( NUI::CDesktopWindow *pDesktop );
	NUI::CDesktopWindow* GetDesktop() const;
	////
	EActionIconsSet GetActionIconsSet() const;
	void SetActionIconsSet( EActionIconsSet eMode );
	////
	int GetPanelState( int nMask ) const;
	void SetPanelState( int nMask, bool bState );
	////
	void SetCheatVisibility( bool bState );
	bool GetCheatVisibility() const;

	NRPG::CGlobalGame *GetRPGGame() const;
	NWorld::IWorld* GetWorld() const;
	NGScene::IGameView* GetScene() const;
	NSound::ISoundScene* GetSoundScene() const;
	NRender::IRenderGame* GetRenderGame() const;

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame( int nMode, const STime &sTime, ICamera *pCamera = 0, bool bShowUnits = true );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
