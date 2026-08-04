#ifndef __IMISSION_H__
#define __IMISSION_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// CONST
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOV = 35;
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
#include "iMain.h"
#include "Camera.h"
#include "Interface.h"
#include "wInterface.h"
#include "..\Misc\RandomGen.h"
#include "wUnitCommands.h"

namespace NDb
{
	enum EDiplomacyState;
	class CChapterMap;		// IMission::GetChapterMap (retail mission vtbl+0x14c) -- see below
}
class CChapterInfo;			// IMission::GetChapterInfo (retail mission vtbl+0x150) -- see below
namespace NGScene
{
	class ILight;
	class IGameView;
}
namespace NRPG
{
	class CGlobalGame;
}
namespace NRender
{
	class IRenderGame;
}
namespace NSound
{
	class ISoundScene;
}
namespace NScenario
{
	class CScenarioZone;
}
namespace NAI
{
	struct SPosition;
}
namespace NUI
{
	class CDesktopWindow;
}
namespace NWorld
{
	class CUnit;
	class IItem;
	class IPlayer;
	class IWorld;
	class CCommander;
	class CCmd;
	class CCommand;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NWorld::STraceResult -- the retail cursor-trace result record (serialization-convergence W4.2).
// Retail CMission caches the per-frame cursor trace in ONE embedded struct (CMission tag 21,
// operator& @0x1a0ff0: 2=bPointSet 3=vPoint 4=bTileSet 5=sTile 6=bObjectSet 7=pObject) instead of
// the dev's loose bTraceOk/sTraceTile/pTraceObject members. Unregistered in retail (embedded only).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
struct STraceResult
{
	bool bPointSet;                 // retail: the scene-point trace hit (dev computes the world
	CVec3 vPoint;                   //   point live in GetTracePosition(CVec3*) -- kept default here)
	bool bTileSet;                  // dev bTraceOk
	NAI::SPosition sTile;           // dev sTraceTile
	bool bObjectSet;                // retail: IsValid(pObject) latch
	CPtr<CObjectBase> pObject;      // dev pTraceObject

	STraceResult(): bPointSet( false ), vPoint( 0, 0, 0 ), bTileSet( false ), bObjectSet( false ) {}
	// retail NWorld::STraceResult::operator& @0x1a0ff0
	int operator&( CStructureSaver &f ) { f.Add(2,&bPointSet); f.Add(3,&vPoint); f.Add(4,&bTileSet); f.Add(5,&sTile); f.Add(6,&bObjectSet); f.Add(7,&pObject); return 0; }
};
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sounds
const int
	N_SOUND_ERROR					= 4507;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Render modes
const int
	N_RENDERMODE_2D				= 1,
	N_RENDERMODE_3D				= 2,
	N_RENDERMODE_SHOWALL	= 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
// State flags
const int
	N_UNITSTATE_DEFAULT					= 0x00000001,
	N_UNITSTATE_CANNON					= 0x00000002,
	N_UNITSTATE_SNIPE						= 0x00000003;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Actions
enum EUnitAction
{
	UA_DEFAULT,
	UA_STOP,
	UA_CONTINUE,
	//// moves
	UA_MOVE,
	UA_LOOK,
	//// actions
	UA_USE,
	UA_HEAL,
	UA_MINE,
	UA_ATTACK,
	// retail action 9 (oracle s2_cmainiconbarset.h:141 "usetool" button; dropcorpse=10,
	// weaponreload=0xb align after it): the use-tool action for a unit holding a TOOL
	// (disassemble/mount turrets, disarm) -- dev's enum skipped it, shifting everything after.
	UA_USETOOL,
	UA_DROPCORPSE,
	UA_WEAPONRELOAD,
	UA_EXITPK,
	UA_HIDE,
	//// poses
	UA_STRAFE,
	UA_POSERUN,
	UA_POSEWALK,
	UA_POSECROUCH,
	UA_POSECRAWL,
	//// snipe
	UA_SNIPE_ATTACK,
	UA_COLLECTAP_1AP,
	UA_COLLECTAP_10AP,
	UA_COLLECTAP_MAX,
	UA_COLLECTAP_ALL,
	////
	UA_MAXVALUE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail NGame::SActionInfo -- 20 bytes, PDB-exact layout. The struct is RAW-serialized in two
// places (CMission::operator& tag 16 DoDataVector over actionsInfoSet: 24 x 20 = 480 bytes, and
// CStateAttack::operator& tag 3 DataChunk(0x14)), so the layout must stay EXACTLY
// {bValid@0, nMinAP@4, nMaxAP@8, bOk@12, bEnoughAP@13, bEnoughAPToStart@14, bAvailable@15,
// eResult@16} = 20 bytes (wire-audit findings CMission 16.2 save=480 dev=288 and CStateAttack
// tag 3 save=20 dev=1 were caused by the old dev shape {nActionAP,bOk,bEnoughAP,bAvailable,
// eResult} = 12 bytes).
// Field semantics (retail CMission::CanDoCommand @0x1fb680 / @0x1fc870):
//   bValid           -- "this cached entry has been computed" (the actionsInfoSet lazy-cache
//                       flag; @0x1fb680 sets it true on every compute).
//   nMinAP / nMaxAP  -- the full-action AP folded MIN/MAX across the selected units
//                       (@0x1fc870; -1 when no unit reported an AP).
//   bEnoughAPToStart -- AND across selected units of (AP-to-start <= unit->GetRPG()->GetAP()),
//                       evaluated in turn-based only (@0x1fc870).
//   bOk/bEnoughAP/bAvailable/eResult -- as before (the @0x1fb680 eResult switch).
struct SActionInfo
{
	bool bValid;

	int nMinAP;
	int nMaxAP;

	bool bOk;
	bool bEnoughAP;
	bool bEnoughAPToStart;
	bool bAvailable;
	NWorld::EUnitCommandResult eResult;

	// retail SActionInfo::SActionInfo @0x1a0a20 (bEnoughAPToStart is left UNINITIALIZED by the
	// retail ctor; initialized false here -- an uninitialized read cannot be reproduced safely).
	// Retail seeds eResult = UCR_NULL (retail ordinal 0, the "no result yet" value); the dev enum
	// has no UCR_NULL (its ordinals deliberately diverge -- see wUnitCommands.h), so the dev keeps
	// its established UCR_UNAVAILABLE sentinel.
	SActionInfo(): bValid( false ), nMinAP( -1 ), nMaxAP( -1 ), bOk( false ), bEnoughAP( false ), bEnoughAPToStart( false ), bAvailable( false ), eResult( NWorld::UCR_UNAVAILABLE ) {}

	// retail SActionInfo::SetValid @0x1a18d0: the "instant OK, zero AP" filler used by
	// CMission::UpdateActionsInfo for the always-possible actions.
	void SetValid() { bValid = true; eResult = NWorld::UCR_OK; nMinAP = 0; nMaxAP = 0; bOk = true; bEnoughAP = true; bAvailable = true; }
};
static_assert( sizeof(SActionInfo) == 20, "NGame::SActionInfo is raw-serialized (CMission tag 16 / CStateAttack tag 3) -- must stay the retail 20-byte layout" );
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EScriptMode
{
	SM_NO_SCRIPT = 0,
	SM_SCRIPT_RUNNING,
	SM_SKIPPING_SCRIPT_PART,
	SM_SKIPPING_SCRIPT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPanel
{
	PANEL_PERKS				= 0x00000001,
	PANEL_STORE				= 0x00000002,
	PANEL_INVENTORY		= 0x00000004,
	PANEL_CHARACTER		= 0x00000008,
	// retail hosts FOUR character sub-panels (skills/perks/medals/biography) switched exclusively
	// (ActivateCharacterSubPanel @0x20f140); medals + biography were missing from the dev enum.
	PANEL_MEDALS			= 0x00000010,
	PANEL_BIOGRAPHY		= 0x00000020,
	////
	PANEL_ALL					= 0xFFFFFFFF
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EActionIconsSet
{
	AIS_FORCED,
	AIS_MAIN,
	AIS_POSES,
	AIS_WEAPONMODES,
	AIS_GRENADEMODES
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IUnitTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class IUnitTracker: public CObjectBase
{
public:
	// retail @0x32b670 returns EUnitCommandResult from a click-time CanDo (FindPath preview) --
	// the SOLE source of the user-visible "Path not found" on an unreachable move click; a
	// rejected command is never queued.
	virtual NWorld::EUnitCommandResult SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly = false ) = 0;
	virtual NAI::SPosition GetTargetPosition() const = 0;

	virtual bool IsPathComplete() const = 0;
	virtual void CancelPath() = 0;

	virtual bool IsActive() const = 0;
	virtual bool IsSelected() const = 0;

	virtual bool IsHilighted() const = 0;
	virtual void SetHilighted( bool bState ) = 0;

	virtual void Update() = 0;
	
	virtual NWorld::CUnit* GetUnit() const = 0;

	virtual NDb::EDiplomacyState GetUnitDiplomacy( NWorld::CUnit *pUnit ) const = 0;

	virtual void GetVisibleEnemiesList( list<CPtr<NWorld::CUnit> > *pEnemies ) const = 0;
	virtual NWorld::CUnit* GetNextVisibleEnemy() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IPlayerTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPlayerTracker: public CObjectBase
{
public:
	virtual void AddUnit( NRPG::CUnit *pMerc ) = 0;
	virtual void RemoveUnit( IUnitTracker *pUnit ) = 0;

	virtual bool IsPlayerLoser() = 0;
	virtual bool IsPlayerWinner() = 0;
	virtual bool IsUnitVisible( NWorld::CUnit *pUnit ) const = 0;

	virtual NDb::EDiplomacyState GetUnitDiplomacy( NWorld::CUnit *pUnit ) const = 0;

	virtual void GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const = 0;
	virtual void GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const = 0;

	virtual int CountSelected() = 0;
	virtual void Select( NWorld::CUnit *pUnit, bool bAdditive = false ) = 0;
	virtual void SelectNext() = 0;
	virtual void SelectPrev() = 0;

	virtual void Activate() = 0;
	virtual void Deactivate() = 0;
	
	virtual void Update( bool bActive ) = 0;

	// retail IPlayerTracker vtbl+0x18/+0x1c (CPlayerTracker::GetCamera @0x2877f0 / SetCamera
	// @0x287d40): each player tracker OWNS a full ICamera object (CObj, save tag 10) -- the camera
	// the player actually drives in normal play. CMissionBase::GetCamera (@0x1a1ee0) SELECTS between
	// this per-player camera and the mission's cinematic pCamera (sequences / enemy-turn follow).
	virtual ICamera* GetCamera() const = 0;
	virtual void SetCamera( ICamera *pCamera ) = 0;

	virtual NWorld::IPlayer* GetPlayer() const = 0;
	virtual NWorld::CCommander* GetCommander() const = 0;
	virtual NRPG::CGlobalPlayer* GetGlobalPlayer() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IState
////////////////////////////////////////////////////////////////////////////////////////////////////
class IState: public CObjectBase
{
public:
	enum EType
	{
		FORCED,
		UPDATED,
		INSTANT,
		TEMPORARY
	};

public:
	virtual bool Initialize( IMission *pGame ) = 0;
	virtual void Terminate() = 0;
	virtual bool ProcessEvent( const NInput::SEvent &sEvent ) = 0;
	virtual bool ProcessMessage( const NUI::SEvent &sEvent ) = 0;
	virtual void Step() = 0;

	virtual bool OnLButtonUp( int nX, int nY ) = 0;
	virtual bool OnLButtonDown( int nX, int nY ) = 0;
	virtual bool OnLButtonDblClk( int nX, int nY ) = 0;
	virtual NUI::SCursorInfo GetCursorInfo() const = 0;

	virtual EType GetType() const = 0;
	virtual IMission* GetMission() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMission
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMission: public NMainLoop::IInterfaceBase
{
public:
	virtual bool Initialize( int nTemplateID, int nVariantID, NScenario::CScenarioZone *pZone, const vector<string> &params, NRPG::CGlobalGame *pGlobalGame, NDb::CUITexture *pPWLImage = 0 ) = 0;
	virtual void Terminate() = 0;

	virtual void Command( NWorld::CCommand *pCmd ) = 0;
	virtual void Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly = true ) = 0;
	// retail mission vtbl+0x2c (CMissionBase::DoEvent @0x1a2fa0): the interface-EVENT channel --
	// ack barks / script callbacks / UI-action ids / delayed game-over post here, NOT via Command
	virtual void DoEvent( NWorld::CCommand *pCmd ) = 0;
	virtual void StopAction() = 0;
	virtual bool IsReady() const = 0;
	virtual bool IsActionExecuted() const = 0;
	virtual bool IsRealTime() const = 0;

	virtual void PauseGame( bool bState ) = 0;
	virtual bool IsGamePaused() const = 0;

	virtual IPlayerTracker* GetActivePlayer() const = 0;
	virtual void SetActivePlayer( IPlayerTracker *pPlayer ) = 0;
	virtual void GetPlayers( vector< CPtr<IPlayerTracker> > *pPlayersSet ) const = 0;

	virtual void GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const = 0;
	virtual void GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const = 0;

	virtual int CountSelected() = 0;
	virtual void Select( NWorld::CUnit *pUnit, bool bAdditive = false ) = 0;
	virtual void SelectNext() = 0;
	virtual void SelectPrev() = 0;

	virtual bool IsUpdated() const = 0;

	virtual IState* GetState() const = 0;
	virtual bool CommandState( IState *pState ) = 0;
	virtual void ResetState() = 0;
	virtual void SetUpdatedStates( const vector<CObj<IState> > &updatedStatesSet ) = 0;
	virtual CObjectBase* GetStateTarget( bool *pbFrom3DWorld = 0 ) const = 0;
	virtual void SetStateTarget( CObjectBase* pObject ) = 0;

	virtual int GetUnitsState() = 0;
	virtual NWorld::CUnit::EState GetUnitsWorldState() = 0;
	virtual void GetActionInfo( EUnitAction eAction, SActionInfo *pInfo ) = 0;
	// retail mission vtbl+0xe0 (CMission @0x1fcbd0): may the squad leave the zone now? Fills *pwsReason
	// with the localized status line (also filled with "you can leave" 20238 on the true path). Consumers:
	// CTopBar::Draw @0x24d010 (tooltip "state" var + button face) and CMission::ProcessEvent @0x202f96
	// (failed exit click -> csGame log).
	virtual bool CanLeaveZone( wstring *pwsReason ) = 0;
	// retail mission vtbl+0xa8 (CMission::CanDoCommand @0x1fbf10): the bool WHOLE-COMMAND probe --
	// only CCmdEndOfTurn is gated (2s cooldown after a current-player change); consumer: v1.2
	// CUnitPanel::Draw @0x653f40 (button enable + text state).
	virtual bool CanDoCommand( NWorld::CCommand *pCmd ) = 0;
	virtual void CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo ) = 0;
	// retail mission vtbl+0xac (@0x1fc870): per-selected-unit probe folding the full-action AP into
	// MIN/MAX and AND-ing the "enough AP to start" flag (turn-based only).
	virtual NWorld::EUnitCommandResult CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget = false, int *pnMinAP = 0, int *pnMaxAP = 0, bool *pbEnoughAPToStart = 0 ) = 0;

	// retail mission vtbl+0xb4 (CMissionBase::CreateCamera @0x1a2390): the camera FACTORY -- creates
	// a camera and stamps the module default hard/soft limits + the world's scroll-zone rect. Each
	// CPlayerTracker creates its own camera through it (retail ctor @0x287d70).
	virtual ICamera* CreateCamera( ECameraType eType ) = 0;
	virtual ICamera* GetCamera() const = 0;
	// retail @0x19df00 (mission vtbl+0xbc): the CINEMATIC camera, unfiltered by the GetCamera selector.
	// GetCamera() != GetEnemyTurnCamera() is true exactly when the user has taken the camera -- that
	// is the test the auto-show executors veto on.
	virtual ICamera* GetEnemyTurnCamera() const = 0;
	virtual void GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits ) = 0;
	virtual void SetCameraParams( ECameraType eType, float fFOV, const ICamera::SCameraLimits &limits ) = 0;
	virtual void FocusCameraOnUnit( NWorld::CUnit *pUnit ) = 0;
	// retail mission vtbl+0xc4 (CMissionBase::FocusCameraOnItem @0x1a1740, right after FocusCameraOnUnit
	// @vtbl+0xc0): anchor the camera on a world item -- the hint/clue in-world icons' RBUTTONUP action.
	virtual void FocusCameraOnItem( NWorld::IItem *pItem ) = 0;
	virtual int GetCutFloor() = 0;
	virtual void SetCutFloor( int nFloor ) = 0;
	virtual const CVec3& GetCameraCP() const = 0;
	virtual const SHMatrix& GetCameraPos() const = 0;
	virtual const CTransformStack& GetCameraTransform() const = 0;

	virtual CObjectBase* GetTraceObject() const = 0;
	virtual CRay GetTraceRay() const = 0;
	virtual bool GetTracePosition( CVec3 *pPos ) const = 0;
	virtual bool GetTracePosition( NAI::SPosition *pPos ) const = 0;

	virtual NUI::ICursor* GetCursor() const = 0;
	virtual NUI::CInterface* GetInterface() const = 0;
	virtual bool IsInterfaceHidden() const = 0;
	virtual bool IsSequence() const { return false; }   // retail mission vtbl+0x44: true while a scripted sequence runs (CMission override = nSequence>0)
	// retail mission vtbl+0x48 (CMissionBase::IsPlayerTurn @0x1a1910): false only in turn-based play
	// while the world's current player is NOT the active (human) tracker's player.
	virtual bool IsPlayerTurn() const = 0;
	// retail mission vtbl+0xcc (CMissionBase::GetFollowCameraState @0x1a1790): the "ui_followcamera"
	// game option (camera follows enemy actions) -- a gate of the GetCamera selector @0x1a1ee0.
	virtual bool GetFollowCameraState() const = 0;
	////
	virtual void SetWaitForPartFinished( bool bState ) = 0;
	virtual bool IsWaitForPartFinished() const = 0;
	// retail mission vtbl+0xf8 (CMission @0x19def0): true while the scripted "first mission"
	// (tutorial) HUD mode is latched -- gates the perks-panel Medals/Biography buttons
	// (CPerksPanelView::Draw @0x22c630).
	virtual bool IsSpecialFirstMissionMode() const = 0;
	////
	virtual void PopDesktop( NUI::CDesktopWindow *pDesktop ) = 0;
	virtual void PushDesktop( NUI::CDesktopWindow *pDesktop ) = 0;
	virtual NUI::CDesktopWindow* GetDesktop() const = 0;
	////
	virtual EActionIconsSet GetActionIconsSet() const = 0;
	virtual void SetActionIconsSet( EActionIconsSet eMode ) = 0;
	////
	virtual int GetPanelState( int nMask ) const = 0;
	virtual void SetPanelState( int nMask, bool bState ) = 0;
	////
	virtual void SetCheatVisibility( bool bState ) = 0;
	virtual bool GetCheatVisibility() const = 0;

	virtual NRPG::CGlobalGame *GetRPGGame() const = 0;
	virtual NWorld::IWorld* GetWorld() const = 0;
	virtual NGScene::IGameView* GetScene() const = 0;
	virtual NSound::ISoundScene* GetSoundScene() const = 0;
	virtual NRender::IRenderGame* GetRenderGame() const = 0;

	// retail mission vtbl+0x144: the mission's scenario zone (null when launched outside a campaign,
	// e.g. the tutorial). Gates the topbar Objectives button (CTopBar::ProcessMessage @0x24d670).
	virtual NScenario::CScenarioZone* GetScenarioZone() const = 0;

	// retail mission vtbl+0x14c / +0x150 (slots 83/84 of the 90-slot mission vtable). These are part
	// of the COMMON mission interface, NOT CChapterMap extensions -- PROVEN by a vftable slot-walk
	// (ICF-folding hides the symbols, so the ctor -> vftable VA -> slot read route was used):
	//   ??_7CMissionBase@NGame@@6B@ @0x4b960c, ??_7CMission@NGame@@6B@ @0x4b932c and
	//   ??_7CChapterMap@NGame@@6B@ @0x4b9adc are ALL exactly 90 slots long (last = slot 89
	//   CMissionBase::RenderFrame @0x1a19f0). CMissionBase's OWN vftable already carries slots 83/84,
	//   filled with the ICF-folded `return 0` stub @0x3538d0; CChapterMap's vftable overrides exactly
	//   those two slots with CChapterMap::GetChapterMap @0x1a6530 / GetChapterInfo @0x1a6540. Were
	//   they CChapterMap-specific they would be APPENDED, making CChapterMap's vtable two slots
	//   LONGER than CMissionBase's; instead all three are the same length and the base already
	//   reserves 83/84 -- i.e. the slots are declared on the shared interface, not added by the leaf.
	// That is why retail's chapter-map UI calls them straight through its CPtr<NGame::IMission> with
	// NO downcast -- e.g. CExitZoneSector::UpdateSector @0x1aa690 emits
	// `(**(code **)(*(int *)this->pChapter + 0x150))()` on a member the PDB types CPtr<NGame::IMission>.
	// (The CChapterMap ctor @0x1a6a60 vftable store `mov dword ptr [esi], 0x8b9adc` confirms the VA.)
	virtual NDb::CChapterMap* GetChapterMap() const = 0;
	virtual CPtrFuncBase<CChapterInfo>* GetChapterInfo() const = 0;

	virtual void Step() = 0;
	virtual bool ProcessEvent( const NInput::SEvent &sEvent ) = 0;
	// retail PDB: RenderFrame(int,bool,ICamera*,bool) -- param 2 is the ADVANCE flag (Step's world-advance
	// gate), NOT a time: the body reads the raw clock (GetTime) for sound and the UI counter for Draw.
	virtual void RenderFrame( int nMode, bool bAdvanceTime, ICamera *pCamera = 0, bool bShowUnits = true ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionBase -- the retail mission base class (serialization-convergence W4.2; release iBase.obj).
//
// Retail splits the old flat CMission into CMissionBase + CMission: the base owns the
// scene/sound/render/world/player-tracker/interface/camera/desktop/time bookkeeping and is
// serialized NON-POLYMORPHICALLY as the tag-1 base chunk of its children (retail operator&
// @0x19f3f0, 34 tags; UNREGISTERED -- exactly like retail, do NOT add a REGISTER for it).
// Children: CMission (op& @0x1a03a0), CRenderBaseInterface (op& @0x1cce20 = base chunk ONLY,
// + the 3D-backdrop menus derived from it), CMultiPlayerInterface (: CMission, op& @0x21d720).
//
// Behaviour placement: the trivial accessors + the player/desktop/light/camera-focus helpers land
// here with their retail-base bodies (all only touch base members). The retail base ALSO owns the
// mission pump (Step @0x1a3ad0 / InternalStep @0x1a29f0 / ProcessEvent @0x1a2010 / ExecWorldCommand
// @0x1a30c0) and the exit/save/load binds (bindExit/bindGameMenu/bindLoad/bindSave/bindSaveMenu/
// bindLoadMenu) -- those stay on the dev CMission for now (deferred behaviour leg); this leg is the
// SERIALIZATION split. Members that IMission children never re-implement get the retail COMDAT-stub
// defaults inline below (retail folds identical `return 0` stubs; confirmed @0x1a16c0 / @0x1a16d0).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdLocatorExec;	// iMissionExec.h -- the dedicated camera-locator executor slot
class CMissionBase: public IMission
{
protected:
	ZDATA
	CPtr<NRPG::CGlobalGame> pGlobalGame;
	//// graphics
	CObj<NGScene::IGameView> pScene;
	CObj<NSound::ISoundScene> pSoundScene;
	CObj<NRender::IRenderGame> pRender;	// owns the sound mixers (retail CRenderGame pSound/pUnitSounds)
	//// world
	bool bPause;
	bool bRenderWorld;					// retail base tag 8; the dev render path does not consult it yet (always true)
	STime nDeltaTime;
	CObj<NWorld::IWorld> pWorld;
	//// players
	CPtr<IPlayerTracker> pActivePlayer;
	vector< CObj<IPlayerTracker> > playersSet;
	//// interface
	bool bHideInterface;
	bool bSpecialHideInterface;
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	//// camera
	CObj<ICamera> pCamera;
	ICamera::SCameraLimits cameraLimits;
	// the per-BeginSequence camera-pose stack (retail base tag 20 "cameraPosStack"; every begin
	// pushes the current pose, EndSequence pops -- see CMission::ExecWorldCommands)
	vector<ICamera::SCameraPos> cameraPosStack;
	// retail CMissionBase pExecLocator: a dedicated camera-command executor slot, SEPARATE from
	// CMission::pCmdExec, so a CUICmdCameraLocator (CUICmdUnitCamera auto-focus) does NOT stall the
	// general UI-command drain and can be preempted by a higher-priority one
	// (MustReplaceCameraExecutor). Driven by ExecWorldCommand @0x1a30c0. CObj like retail (tag 21).
	CObj<CUICmdLocatorExec> pExecLocator;
	bool bWaitForPartFinished;
	list<CObj<NUI::CDesktopWindow> > desktopWindowsList;
	//// light
	int nLightMode;
	CObj<CObjectBase> pLightSource;
	bool bCheatVisibility;
	// LUA convergence (hint machinery): the script SetTutorialMode(b) command sets this; the
	// ShowHint dispatch gate reads it (retail CMissionBase::ExecWorldCommand @0x1a30c0) so hints
	// always show in a tutorial even when the "ui_showhints" game option is off. Retail base tag 31.
	bool bTutorialMode;
	//// time bookkeeping (retail base tags 28..37). The counters are ctor-wired exactly like the
	//// retail default ctor @0x1a2c70 (pTimeFunc/pUITimeFunc adopt the counters' CCTime nodes); the
	//// per-frame Advance calls live in the retail base Step @0x1a3ad0, which is still the dev
	//// CMission's monolithic Step -- advancing them is the deferred behaviour leg. Serialized 1:1.
	CTimeCounter sTimeCounter;
	CDGPtr<CCTime> pTimeFunc;
	bool bCanSave;						// retail default true (@0x1a2c70)
	bool bCanRestart;					// retail default false
	CTimeCounter sUITimeCounter;
	CDGPtr<CCTime> pUITimeFunc;
	STime sLastUpdateTime;
	STime sMinFrameTime;
	STime sFPSLimitLastTime;
	// retail base tag 38: the script PlaySound channels list (ExecWorldCommand @0x1a30c0 parks the
	// opened channel here). The dev Part-B port parks the channel on the CUICmdPlaySound command
	// itself (StopSound releases it there), so this list stays empty for now -- serialized 1:1.
	list<CObj<CObjectBase> > soundsList;
public:
	// retail NGame::CMissionBase::operator& @0x19f3f0 -- 34 tags, gaps at 6/17/27 preserved
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGlobalGame); f.Add(3,&pScene); f.Add(4,&pSoundScene); f.Add(5,&pRender); f.Add(7,&bPause); f.Add(8,&bRenderWorld); f.Add(9,&nDeltaTime); f.Add(10,&pWorld); f.Add(11,&pActivePlayer); f.Add(12,&playersSet); f.Add(13,&bHideInterface); f.Add(14,&bSpecialHideInterface); f.Add(15,&pCursor); f.Add(16,&pInterface); f.Add(18,&pCamera); f.Add(19,&cameraLimits); f.Add(20,&cameraPosStack); f.Add(21,&pExecLocator); f.Add(22,&bWaitForPartFinished); f.Add(23,&desktopWindowsList); f.Add(24,&nLightMode); f.Add(25,&pLightSource); f.Add(26,&bCheatVisibility); f.Add(28,&sTimeCounter); f.Add(29,&pTimeFunc); f.Add(30,&bCanSave); f.Add(31,&bTutorialMode); f.Add(32,&bCanRestart); f.Add(33,&sUITimeCounter); f.Add(34,&pUITimeFunc); f.Add(35,&sLastUpdateTime); f.Add(36,&sMinFrameTime); f.Add(37,&sFPSLimitLastTime); f.Add(38,&soundsList); return 0; }

	CMissionBase();

protected:
	// one retail SetLightMode @0x1a2640 on the base (the dev carried IDENTICAL copies on CMission
	// and CRenderBaseInterface; both consolidated here)
	void SetLightMode( int _nLightMode );

public:
	//// real retail-base bodies (iMissionBase.cpp) -- all only touch base members
	virtual void Command( NWorld::CCommand *pCmd );										// retail @0x1a18f0
	virtual void Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly = true );	// retail @0x1a15b0
	virtual void DoEvent( NWorld::CCommand *pCmd );										// retail @0x1a2fa0 (vtbl+0x2c, events channel)
	virtual void OnGetFocus();															// retail @0x1a19c0: ResetTiming + sound-scene unpause
	virtual void OnLostFocus();															// retail @0x1a19e0: sound-scene pause (menu over mission)
	virtual void StopAction();															// retail @0x1a29d0
	virtual bool IsReady() const;														// retail @0x1a1e60
	virtual bool IsActionExecuted() const;												// retail @0x1a3e30
	virtual bool IsRealTime() const;													// retail @0x1a1900
	// retail CMissionBase::GetUITime @0x19d490: the always-running UI clock (sUITimeCounter's node,
	// advanced every Step) -- feeds the @0x1fbf10 end-of-turn cooldown.
	STime GetUITime() const { return pUITimeFunc->GetValue(); }
	// retail CMissionBase::GetGameTime @0x1a4500: the GAME clock -- refreshed pTimeFunc (sTimeCounter)
	// + nDeltaTime. Drives UpdateDesktop and the camera-command executors; nDeltaTime growth is what
	// fast-forwards the movie fades/camera moves through the Step skip-part loop.
	STime GetGameTime() { pTimeFunc.Refresh(); return pTimeFunc->GetValue() + nDeltaTime; }
	virtual void PauseGame( bool bState );												// retail @0x1a16a0
	virtual bool IsGamePaused() const;													// retail @0x1a16b0
	virtual IPlayerTracker* GetActivePlayer() const;									// retail @0x1a1950
	virtual void SetActivePlayer( IPlayerTracker *pPlayer );							// retail @0x1a2480
	virtual void GetPlayers( vector< CPtr<IPlayerTracker> > *pPlayersSet ) const;		// retail @0x1a3f90
	virtual void GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;				// retail @0x1a1960
	virtual void GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const;		// retail @0x1a1970
	virtual int CountSelected();														// retail @0x1a1980
	virtual void Select( NWorld::CUnit *pUnit, bool bAdditive = false );				// retail @0x1a1990
	virtual void SelectNext();															// retail @0x1a19a0
	virtual void SelectPrev();															// retail @0x1a19b0
	virtual bool IsPlayerTurn() const;													// retail @0x1a1910
	virtual bool GetFollowCameraState() const;											// retail @0x1a1790
	virtual ICamera* CreateCamera( ECameraType eType );									// retail @0x1a2390 (body in iMission.cpp -- uses the module default-limit statics)
	virtual ICamera* GetCamera() const;													// retail @0x1a1ee0 (the SELECTOR: cinematic pCamera vs the active player's camera)
	virtual ICamera* GetEnemyTurnCamera() const { return pCamera; }						// retail @0x19df00
	virtual void FocusCameraOnUnit( NWorld::CUnit *pUnit );								// retail @0x1a16e0
	virtual void FocusCameraOnItem( NWorld::IItem *pItem );								// retail @0x1a1740
	virtual NUI::ICursor* GetCursor() const;
	virtual NUI::CInterface* GetInterface() const;
	virtual bool IsInterfaceHidden() const;												// retail @0x19de80
	virtual void SetWaitForPartFinished( bool bState );									// retail @0x19de90
	virtual bool IsWaitForPartFinished() const;											// retail @0x19dea0
	virtual bool IsSpecialFirstMissionMode() const { return false; }					// retail CMissionBase vtbl+0xf8 = ICF-folded false stub (@0x163980); CMission override @0x19def0
	virtual void PopDesktop( NUI::CDesktopWindow *pDesktop );							// retail @0x1a2fc0
	virtual void PushDesktop( NUI::CDesktopWindow *pDesktop );							// retail @0x1a3030
	virtual NUI::CDesktopWindow* GetDesktop() const;									// retail @0x1a2000
	virtual void SetCheatVisibility( bool bState );										// retail @0x19deb0
	virtual bool GetCheatVisibility() const;											// retail @0x19dec0
	virtual NRPG::CGlobalGame *GetRPGGame() const;										// retail @0x19df10
	virtual NWorld::IWorld* GetWorld() const;
	virtual NGScene::IGameView* GetScene() const;										// retail @0x103d20
	virtual NSound::ISoundScene* GetSoundScene() const;									// retail @0x19df20
	virtual NRender::IRenderGame* GetRenderGame() const;								// retail @0x19df30
	virtual void RenderFrame( int nMode, bool bAdvanceTime, ICamera *pCamera = 0, bool bShowUnits = true );	// retail @0x1a19f0 (PDB: (int,bool,ICamera*,bool))

	//// retail COMDAT-stub defaults (the release folds identical trivial stubs across the vtable;
	//// CMission overrides every one of these with its real body -- the menus never call them)
	virtual bool Initialize( int nTemplateID, int nVariantID, NScenario::CScenarioZone *pZone, const vector<string> &params, NRPG::CGlobalGame *pGlobalGame, NDb::CUITexture *pPWLImage = 0 ) { return false; }
	virtual void Terminate() {}
	virtual bool IsUpdated() const { return false; }
	virtual IState* GetState() const { return 0; }
	virtual bool CommandState( IState *pState ) { CObj<IState> pHold( pState ); return false; }	// retail CRenderBaseInterface @0x19f1a0
	virtual void ResetState() {}
	virtual void SetUpdatedStates( const vector<CObj<IState> > &updatedStatesSet ) {}
	virtual CObjectBase* GetStateTarget( bool *pbFrom3DWorld = 0 ) const { if ( pbFrom3DWorld ) *pbFrom3DWorld = false; return 0; }
	virtual void SetStateTarget( CObjectBase* pObject ) {}
	virtual int GetUnitsState() { return N_UNITSTATE_DEFAULT; }
	virtual NWorld::CUnit::EState GetUnitsWorldState() { return NWorld::CUnit::ST_NORMAL_DEFAULT; }	// retail @0x1a16c0 (CRBI override @0x22ecd0)
	virtual void GetActionInfo( EUnitAction eAction, SActionInfo *pInfo ) { *pInfo = SActionInfo(); }
	virtual bool CanLeaveZone( wstring *pwsReason ) { return false; }	// retail: folded `return 0` stub; only CMission overrides (@0x1fcbd0)
	virtual bool CanDoCommand( NWorld::CCommand *pCmd ) { CObj<NWorld::CCommand> pHold( pCmd ); return true; }	// base default = @0x1fbf10's non-end-of-turn fallthrough
	virtual void CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo ) { CObj<NWorld::CCmd> pHold( pCmd ); *pInfo = SActionInfo(); pInfo->eResult = NWorld::UCR_GENERAL_FAILURE; }
	virtual NWorld::EUnitCommandResult CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget = false, int *pnMinAP = 0, int *pnMaxAP = 0, bool *pbEnoughAPToStart = 0 ) { CObj<NWorld::CCmd> pHold( pCmd ); return NWorld::UCR_GENERAL_FAILURE; }	// retail @0x1a16d0
	virtual void GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits ) { *pType = CAMERA_PC; *pFOV = F_FOV; *pLimits = cameraLimits; }
	virtual void SetCameraParams( ECameraType eType, float fFOV, const ICamera::SCameraLimits &limits ) {}
	virtual int GetCutFloor() { return 0; }
	virtual void SetCutFloor( int nFloor ) {}
	virtual const CVec3& GetCameraCP() const { static CVec3 v( 0, 0, 0 ); return v; }
	virtual const SHMatrix& GetCameraPos() const { static SHMatrix m; return m; }
	virtual const CTransformStack& GetCameraTransform() const;	// out-of-line (CTransformStack is fwd-declared here)
	virtual CObjectBase* GetTraceObject() const { return 0; }
	virtual CRay GetTraceRay() const { return CRay(); }		// retail CRenderBaseInterface @0x19f190
	virtual bool GetTracePosition( CVec3 *pPos ) const { return false; }
	virtual bool GetTracePosition( NAI::SPosition *pPos ) const { return false; }
	virtual EActionIconsSet GetActionIconsSet() const { return AIS_FORCED; }
	virtual void SetActionIconsSet( EActionIconsSet eMode ) {}
	virtual int GetPanelState( int nMask ) const { return 0; }
	virtual void SetPanelState( int nMask, bool bState ) {}
	virtual NScenario::CScenarioZone* GetScenarioZone() const { return 0; }	// retail: folded null stub; only CMission overrides
	// retail CMissionBase vtbl+0x14c / +0x150 = the ICF-folded `return 0` stub @0x3538d0 (the release
	// folds every identical trivial pointer stub together; the symbol it happens to carry is
	// NWorld::CCommandExecute::GetCurrentPath -- decompiles to a bare `return (CPath *)0x0;`).
	// NGame::CChapterMap overrides both (@0x1a6530 / @0x1a6540); CMission does NOT (its slots 83/84
	// hold the same folded stub) -- a mission simply has no chapter.
	virtual NDb::CChapterMap* GetChapterMap() const { return 0; }
	virtual CPtrFuncBase<CChapterInfo>* GetChapterInfo() const { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface commands
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICBeginMission: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICBeginMission);
protected:
	int nVariantID;
	int nTemplateID;
	vector<string> params;
	CPtr<NRPG::CGlobalGame> pGlobalGame;
	CPtr<NScenario::CScenarioZone> pZone;
	// retail CICBeginMission +0x14 (zone ctor @0x20b7a0 param 7): a DIRECT mission->zone transition
	// (script BeginZone) must "emulate the chapter map" leave-zone bookkeeping in Exec @0x20ba00 --
	// notably UpdateScenarioOnLeaveZone, which clears deployData.pCorpse so a CARRIED BODY stays
	// behind. Chapter/global-map entries leave it false (CICContinueChapter already ran it), and
	// within-zone template walks leave it false (the carried body must cross sub-maps).
	bool bEmulateChapter = false;
	// retail CICBeginMission +0x30 (CDBPtr<NDb::CUITexture>): the chapter/caller-supplied PreWorldLoad
	// splash. Exec @0x20ba00 does a 3-way pick zone-image -> this->pPWLImage -> default 0x373, so a
	// chapter whose zone carries no PWL image still shows the chapter's own loading background.
	CDBPtr<NDb::CUITexture> pPWLImage;

public:
	CICBeginMission() {}
	CICBeginMission( int nTemplateID, int nVariantID, const vector<string> &params, NRPG::CGlobalGame *_pGlobalGame, NDb::CUITexture *_pPWLImage = 0 );
	// one scenario zone can consist of several templates
	CICBeginMission( NScenario::CScenarioZone *pZone,
		int _nTemplateID, const vector<string> &params, NRPG::CGlobalGame *_pGlobalGame, bool bEmulateChapter = false, NDb::CUITexture *_pPWLImage = 0 );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICEndMission: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICEndMission);
protected:
	CPtr<IMission> pMission;

public:
	CICEndMission() {}
	CICEndMission( IMission *pMission );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::CICRealEndMission @0x20a850 -- the SAFE, NMainLoop-level (between-frame) mission teardown:
// Terminate() (RemovePlayer + SaveWorld) then route to the chapter/global map. Posted by the
// CUICmdContinueChapter handler so the teardown never runs mid-Segment.
class CICRealEndMission: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICRealEndMission);
protected:
	CPtr<IMission> pMission;

public:
	CICRealEndMission() {}
	CICRealEndMission( IMission *_pMission ): pMission( _pMission ) {}
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICMapEditBeginMission: public CICBeginMission
{
	OBJECT_BASIC_METHODS(CICMapEditBeginMission);
public:
	CICMapEditBeginMission() {}
	CICMapEditBeginMission( int nVariantID, NRPG::CGlobalGame *pGlobalGame );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif