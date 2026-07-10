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
}
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
struct SActionInfo
{
	int nActionAP;

	bool bOk;
	bool bEnoughAP;
	bool bAvailable;
	NWorld::EUnitCommandResult eResult;

	SActionInfo(): nActionAP( -1 ), bOk( false ), bAvailable( false ), bEnoughAP( false ), eResult( NWorld::UCR_UNAVAILABLE ) {}
};
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
	virtual void SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly = false ) = 0;
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

	virtual const ICamera::SCameraPos& GetCamera() const = 0;
	virtual void SetCamera( const ICamera::SCameraPos &sPosition ) = 0;

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
	virtual CObjectBase* GetStateTarget() const = 0;
	virtual void SetStateTarget( CObjectBase* pObject ) = 0;

	virtual int GetUnitsState() = 0;
	virtual NWorld::CUnit::EState GetUnitsWorldState() = 0;
	virtual void GetActionInfo( EUnitAction eAction, SActionInfo *pInfo ) = 0;
	virtual void CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo ) = 0;
	virtual NWorld::EUnitCommandResult CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget = false, int *pnAP = 0 ) = 0;

	virtual ICamera* GetCamera() const = 0;
	virtual void GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits ) = 0;
	virtual void SetCameraParams( ECameraType eType, float fFOV, const ICamera::SCameraLimits &limits ) = 0;
	virtual void FreezeCamera( bool bState ) = 0;
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
	virtual bool IsSequence() const { return false; }   // retail mission vtbl+0x44: true while a scripted sequence runs (CMission override = nSequenceDepth>0)
	////
	virtual void SetWaitForPartFinished( bool bState ) = 0;
	virtual bool IsWaitForPartFinished() const = 0;
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

	virtual void Step() = 0;
	virtual bool ProcessEvent( const NInput::SEvent &sEvent ) = 0;
	virtual void RenderFrame( int nMode, const STime &sTime, ICamera *pCamera = 0, bool bShowUnits = true ) = 0;
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