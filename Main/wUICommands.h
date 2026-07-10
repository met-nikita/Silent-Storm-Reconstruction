#ifndef __A5_UICOMMANDS_H_
#define __A5_UICOMMANDS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "camera.h"
#include "..\DBFormat\DataSound.h"		// NDb::CSound complete (CUICmdPlaySound's CDBPtr saveload uses typeid)
#include "..\DBFormat\DataFormat.h"		// NDb::CTEffect complete (CUICmdPlayEffect's CDBPtr saveload uses typeid)
#include "..\DBFormat\DataLight.h"		// NDb::CTAmbientLight complete (CUICmdSetAmbient's CDBPtr saveload uses typeid)
#include "..\DBFormat\DataMisc.h"		// NDb::CUIHint complete (CUICmdShowHint's CDBPtr saveload uses typeid)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScenario
{
	class CScenarioZone;
	class CScenarioClue;
}
//
namespace NWorld
{
class CUnit;
class CAckEvent;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmd
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUICmdPriority
{
	UICP_HIGHEST			= 0,
	UICP_TURN					= 1,
	UICP_CAMERAMOVE		= 2,
	UICP_UNIT					= 3,
	UICP_INTERRUPT		= 4
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmd: public CObjectBase
{
	OBJECT_BASIC_METHODS(CUICmd);
public:
	ZDATA
	int nID;		// release: every UI command carries a unique id (assigned in the ctor from a global
				// counter). The CScript id-queue (AddUICommandWithID/IsUIActionIDPresent/RemoveUIActionID)
				// tracks it; the lua WaitForUI(id) helper polls IsUIActionIDPresent(id).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); return 0; }

	CUICmd();
	// The release dropped per-command priority for non-camera commands (priority now lives on
	// CUICmdCameraLocator). Legacy subclasses still pass UICP_* here; this ctor ignores the value.
	CUICmd( int _nPriority );

	int GetID() const { return nID; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdCameraLocator -- release-new camera-command base; carries the move priority that CUICmd lost.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdCameraLocator: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdCameraLocator );
	ZDATA_(CUICmd)
public:
	int nPriority;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); f.Add(2,&nPriority); return 0; }

	CUICmdCameraLocator(): nPriority( 0x7FFF ) {}
	CUICmdCameraLocator( int _nPriority ): nPriority( _nPriority ) {}

	int GetPriority() const { return nPriority; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Script control
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdPartFinished: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPartFinished );
	ZDATA_(CUICmd)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }
	//
	CUICmdPartFinished():
		CUICmd( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdPause -- release-new: the script Pause() binding queues this no-param command; the mission
// toggles its pause state when it processes the command (CMission::ExecWorldCommands).
class CUICmdPause: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPause );
	ZDATA_(CUICmd)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }
	//
	CUICmdPause():
		CUICmd( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdLockCamera -- release-new: the script CameraLock(bLock) binding queues this; when the mission
// processes it (CMissionBase::ExecWorldCommand -> controller lock), it freezes/unfreezes the camera.
class CUICmdLockCamera: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdLockCamera );
	ZDATA
public:
	ZPARENT( CUICmd );
	bool bLock;		// +0x10: lock (true) / unlock (false) the camera
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&bLock); return 0; }
	//
	CUICmdLockCamera(): bLock( false ) {}
	CUICmdLockCamera( bool _bLock ): CUICmd( UICP_HIGHEST ), bLock( _bLock ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdBeginZone -- release-new: the script BeginZone(zoneName) binding queues this; when the mission
// processes it (CMissionBase::ExecWorldCommand) it resolves the named scenario zone and posts a
// CICBeginMission to begin that zone.
class CUICmdBeginZone: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdBeginZone );
	ZDATA
public:
	ZPARENT( CUICmd );
	string szZone;	// the destination zone name
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&szZone); return 0; }
	//
	CUICmdBeginZone() {}
	CUICmdBeginZone( const string &_szZone ): CUICmd( UICP_HIGHEST ), szZone( _szZone ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdPlaySound -- release-new: the script PlaySound/Play3DSound bindings queue this; when the mission
// processes it (CMissionBase::ExecWorldCommand) it opens a channel on the mission's sound scene and parks
// the live handle in pChannel so the StopSound binding can release (stop) it. Kept alive while playing by
// the script's AddMiscObject holder. pChannel is runtime-only (not serialized).
class CUICmdPlaySound: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPlaySound );
	ZDATA
public:
	ZPARENT( CUICmd );
	CObj<CObjectBase> pChannel;		// the live playing channel (null when stopped / not yet played)
	CDBPtr<NDb::CSound> pSound;		// the DB sound to play
	bool b3DSound;					// 3D (positional) vs 2D
	CVec3 vPos;						// the 3D position (when b3DSound)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pSound); f.Add(4,&b3DSound); f.Add(5,&vPos); return 0; }
	//
	CUICmdPlaySound(): b3DSound( false ), vPos( 0, 0, 0 ) {}
	CUICmdPlaySound( NDb::CSound *_pSound, bool _b3D ): CUICmd( UICP_HIGHEST ), pSound( _pSound ), b3DSound( _b3D ), vPos( 0, 0, 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdPlayEffect -- release-new: the script PlayEffect binding queues this; when the mission processes it
// (CMissionBase::ExecWorldCommand) it spawns the particle effect at vPos and parks the live handle here so
// the StopEffect binding can release (remove) it. Kept alive by the script's AddMiscObject holder.
class CUICmdPlayEffect: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPlayEffect );
	ZDATA
public:
	ZPARENT( CUICmd );
	CObj<CObjectBase> pHandle;		// the live particle handle (null when stopped / not yet played)
	CDBPtr<NDb::CTEffect> pEffect;	// the DB effect to play
	CVec3 vPos;						// the world position to anchor the effect at
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pEffect); f.Add(4,&vPos); return 0; }
	//
	CUICmdPlayEffect(): vPos( 0, 0, 0 ) {}
	CUICmdPlayEffect( NDb::CTEffect *_pEffect, const CVec3 &_vPos ): CUICmd( UICP_HIGHEST ), pEffect( _pEffect ), vPos( _vPos ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdSetAmbient -- release-new: the script SetupAmbientLight/SetTimeOfDay bindings queue this; when the
// mission processes it (CMissionBase::ExecWorldCommand) it sets the scene's ambient light from the record.
class CUICmdSetAmbient: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdSetAmbient );
	ZDATA
public:
	ZPARENT( CUICmd );
	CDBPtr<NDb::CTAmbientLight> pLight;	// the ambient-light DB record
	bool bImmediate;					// release's second ctor arg (smooth vs immediate)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pLight); f.Add(4,&bImmediate); return 0; }
	//
	CUICmdSetAmbient(): bImmediate( false ) {}
	CUICmdSetAmbient( NDb::CTAmbientLight *_pLight, bool _bImmediate ): CUICmd( UICP_HIGHEST ), pLight( _pLight ), bImmediate( _bImmediate ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdSetAmbientEffect -- release-new: the script SetAmbientEffect(id) binding queues this; when the mission
// processes it (retail CMissionBase::ExecWorldCommand @0x1a30c0 -> CGameView::SetAmbientEffect @0x1895f0) it
// sets a single, replaceable scene-wide ambient effect from the record (a null/invalid record clears it -- the
// campaign calls SetAmbientEffect(-1) to clear, SetAmbientEffect(941) to set).
class CUICmdSetAmbientEffect: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdSetAmbientEffect );
	ZDATA
public:
	ZPARENT( CUICmd );
	CDBPtr<NDb::CTEffect> pEffect;	// the ambient effect DB record (null -> clear the current ambient effect)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pEffect); return 0; }
	//
	CUICmdSetAmbientEffect() {}
	CUICmdSetAmbientEffect( NDb::CTEffect *_pEffect ): CUICmd( UICP_HIGHEST ), pEffect( _pEffect ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdBeginFade / CUICmdEndFade -- release-new: the script FadeOut/FadeIn bindings queue these; the mission
// spawns a CMissionFadeUI to fade the screen to vColor over sFadeTime (BeginFade) and tears it down (EndFade).
class CUICmdBeginFade: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdBeginFade );
	ZDATA
public:
	ZPARENT( CUICmd );
	CVec3 vColor;		// fade target colour (rgb)
	STime sFadeTime;	// fade duration (ms)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&vColor); f.Add(4,&sFadeTime); return 0; }
	//
	CUICmdBeginFade(): vColor( 0, 0, 0 ), sFadeTime( 0 ) {}
	CUICmdBeginFade( const CVec3 &_vColor, STime _sFadeTime ): CUICmd( UICP_HIGHEST ), vColor( _vColor ), sFadeTime( _sFadeTime ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdEndFade: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdEndFade );
	ZDATA_(CUICmd)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }
	//
	CUICmdEndFade(): CUICmd( UICP_HIGHEST ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdLoseDialog -- release-new: the script ShowLoseDialog binding queues this; the mission posts a
// CICLoseMenu (the "mission lost" menu) when it processes it.
class CUICmdLoseDialog: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdLoseDialog );
	ZDATA
public:
	ZPARENT( CUICmd );
	int nRecordID;		// the dialog record id (the dev CICLoseMenu does not use it)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&nRecordID); return 0; }
	//
	CUICmdLoseDialog(): nRecordID( -1 ) {}
	CUICmdLoseDialog( int _nRecordID ): CUICmd( UICP_HIGHEST ), nRecordID( _nRecordID ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdLeaveZoneDlg -- release-new: the script ShowLeaveZoneDialog binding queues this; the mission posts a
// CICLeaveZoneMenu (the "leave zone" modal) when it processes it.
class CUICmdLeaveZoneDlg: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdLeaveZoneDlg );
	ZDATA
public:
	ZPARENT( CUICmd );
	int nRecordID;		// the dialog record id (carried; the dev menu does not use it)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&nRecordID); return 0; }
	//
	CUICmdLeaveZoneDlg(): nRecordID( -1 ) {}
	CUICmdLeaveZoneDlg( int _nRecordID ): CUICmd( UICP_HIGHEST ), nRecordID( _nRecordID ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdShowHint -- release-new: the script ShowHint(id) binding queues this; when the mission processes it
// (retail CMissionBase::ExecWorldCommand @0x1a30c0) it shows the hint modal (NGame::CICShowHint) -- gated on
// the "ui_showhints" game option OR the mission's tutorial mode (hints always show in a tutorial). pHint is
// the hint DB record (its +0x10 nSequenceID slot doubles as the XP the screen awards on show).
class CUICmdShowHint: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdShowHint );
	ZDATA
public:
	ZPARENT( CUICmd );
	CDBPtr<NDb::CUIHint> pHint;		// +0x10: the hint DB record to show
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pHint); return 0; }
	//
	CUICmdShowHint() {}
	CUICmdShowHint( NDb::CUIHint *_pHint ): CUICmd( UICP_HIGHEST ), pHint( _pHint ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdTutorialMode -- release-new: the script SetTutorialMode(b) binding queues this; when the mission
// processes it (retail CMissionBase::ExecWorldCommand @0x1a30c0) it sets CMission::bTutorialMode. The sole
// consumer is the ShowHint dispatch gate (in tutorial mode hints always show, even with the option off).
class CUICmdTutorialMode: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdTutorialMode );
	ZDATA
public:
	ZPARENT( CUICmd );
	bool bTutorial;		// +0x10: enable (true) / disable (false) tutorial mode
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&bTutorial); return 0; }
	//
	CUICmdTutorialMode(): bTutorial( false ) {}
	CUICmdTutorialMode( bool _bTutorial ): CUICmd( UICP_HIGHEST ), bTutorial( _bTutorial ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdFirstMissionMode -- SetFirstMissionMode(b) (retail @0x2ee200): the dispatch strips the 0x14 HUD
// panels now + latches CMission::bSpecialFirstMissionMode (which makes future SetPanelState keep those
// panels off). Fresh save id.
class CUICmdFirstMissionMode: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdFirstMissionMode );
	ZDATA
public:
	ZPARENT( CUICmd );
	bool bMode;		// +0x10: enter (true) / exit (false) first-mission mode
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&bMode); return 0; }
	//
	CUICmdFirstMissionMode(): bMode( false ) {}
	CUICmdFirstMissionMode( bool _bMode ): CUICmd( UICP_HIGHEST ), bMode( _bMode ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdEnableFeature -- EnableFeature("s") (retail @0x2ee310): only "reenter" is defined -> nFeature=0 ->
// the dispatch sets CMission::bEnableFeatureReenter = true (allow re-entering this zone's templates). Fresh
// save id.
class CUICmdEnableFeature: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdEnableFeature );
	ZDATA
public:
	ZPARENT( CUICmd );
	int nFeature;	// +0x10: feature id (0 = "reenter")
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&nFeature); return 0; }
	//
	CUICmdEnableFeature(): nFeature( 0 ) {}
	CUICmdEnableFeature( int _nFeature ): CUICmd( UICP_HIGHEST ), nFeature( _nFeature ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetLeaveZoneMode (retail @0x2f14e0 "bn[-1]"): bMode = may-leave; the dispatch sets
// CMission::bLeaveBlockedByScript = !bMode and stores nReason (a "can't leave" message DB id). Retail
// id 0xb3621120.
class CUICmdLeaveZoneMode: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdLeaveZoneMode );
	ZDATA
public:
	ZPARENT( CUICmd );
	bool bMode;			// +0x10: true = may leave the zone, false = leaving is blocked
	int nReason;		// +0x14: DB string id of the "can't leave" message (-1 = none)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&bMode); f.Add(4,&nReason); return 0; }
	//
	CUICmdLeaveZoneMode(): bMode( true ), nReason( -1 ) {}
	CUICmdLeaveZoneMode( bool _bMode, int _nReason ): CUICmd( UICP_HIGHEST ), bMode( _bMode ), nReason( _nReason ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// PlayVideo (retail @0x2f0e00 "s"): play the named .seq cut-scene; the dispatch hands szFileName to
// NGame::CICPlaySequence (the existing iIntroScreen sequence player). Retail id 0xb3327130.
class CUICmdPlayVideo: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPlayVideo );
	ZDATA
public:
	ZPARENT( CUICmd );
	string szFileName;	// +0x10: the .seq sequence file to play
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&szFileName); return 0; }
	//
	CUICmdPlayVideo() {}
	CUICmdPlayVideo( const string &_szFileName ): CUICmd( UICP_HIGHEST ), szFileName( _szFileName ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Camera control
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdMoveCamera: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdMoveCamera );
	ZDATA
public:
	ZPARENT( CUICmd );
	ICamera::SCameraPos pos;
	STime transitionTime;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pos); f.Add(4,&transitionTime); return 0; }
	//
	CUICmdMoveCamera() {}
	CUICmdMoveCamera(	const ICamera::SCameraPos &_pos, STime _transitionTime ):
		CUICmd( UICP_CAMERAMOVE  ), pos( _pos ), transitionTime( _transitionTime ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdScriptMoveCamera -- release-new: plays the camera through a list of waypoints over transitionTime.
// Built by the CameraSequence / CameraMove script bindings; queued via CScript::AddUICommandWithID so the
// lua WaitForUI(id) helper can poll the queued id. (The menu's CMainMenuInterface executes this directly.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdScriptMoveCamera: public CUICmdCameraLocator
{
	OBJECT_BASIC_METHODS( CUICmdScriptMoveCamera );
	ZDATA_(CUICmdCameraLocator)
public:
	STime transitionTime;
	vector<ICamera::SCameraPos> positions;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdCameraLocator*)this); f.Add(2,&transitionTime); f.Add(3,&positions); return 0; }
	//
	CUICmdScriptMoveCamera() {}
	CUICmdScriptMoveCamera( const vector<ICamera::SCameraPos> &_positions, STime _transitionTime ):
		CUICmdCameraLocator( 6 ), transitionTime( _transitionTime ), positions( _positions ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// EPriority -- the CUICmdCameraLocator::nPriority scale (NWorld::EPriority, inventory.json). Higher wins
// (MustReplaceCameraExecutor): a death-beauty shot (5) preempts a shot focus (1).
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPriority
{
	PR_UNIT_SOUND       = 0,
	PR_UNIT_ACTION      = 1,
	PR_EXPLOSION        = 2,
	PR_OUR_UNIT_IS_HIT  = 3,
	PR_UNIT_IS_DEAD     = 4,
	PR_UNIT_DIED_BEAUTY = 5,
	PR_SCRIPT           = 6,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdUnitCamera (wDumbUnit.obj: ctor @0x3531f0, operator& @0x353820) -- the retail arbitrated auto-focus
// camera command. Frame the shooter (+ optional target) at a move priority (EPriority), optionally with
// slo-mo. Replaces the dev CUICmdUnit focus hint at the event producers (shoot / grenade / death /
// unconscious). Arbitrated via CMission::pExecLocator + MustReplaceCameraExecutor.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdUnitCamera: public CUICmdCameraLocator
{
	OBJECT_BASIC_METHODS( CUICmdUnitCamera );
	ZDATA_(CUICmdCameraLocator)
public:
	CPtr<CUnit> pUnit;                  // +0x14  the framed unit (shooter / dying unit)
	CPtr<CUnit> pUnitTarget;            // +0x18  optional second framing point (shot target); null -> single point
	bool        bUseSloMo;              // +0x1c  carried for parity; the a5dll camera has no slo-mo -> inert
	float       fSloMoIncrProbability;  // +0x20
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdCameraLocator*)this); f.Add(2,&pUnit); f.Add(3,&pUnitTarget); f.Add(4,&bUseSloMo); f.Add(5,&fSloMoIncrProbability); return 0; }
	//
	CUICmdUnitCamera(): bUseSloMo( false ), fSloMoIncrProbability( 0 ) {}
	CUICmdUnitCamera( CUnit *_pUnit, int _nPriority, bool _bUseSloMo, float _fProb, CUnit *_pTarget ):
		CUICmdCameraLocator( _nPriority ), pUnit( _pUnit ), pUnitTarget( _pTarget ),
		bUseSloMo( _bUseSloMo ), fSloMoIncrProbability( _fProb ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdSetCameraClipDistance -- release-new: sets the camera near/far clip planes (CameraSetClipping binding).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdSetCameraClipDistance: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdSetCameraClipDistance );
	ZDATA
public:
	ZPARENT( CUICmd );
	float fMinDistance;
	float fMaxDistance;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd*)this); f.Add(3,&fMinDistance); f.Add(4,&fMaxDistance); return 0; }
	//
	CUICmdSetCameraClipDistance() {}
	CUICmdSetCameraClipDistance( float _fMinDistance, float _fMaxDistance ):
		fMinDistance( _fMinDistance ), fMaxDistance( _fMaxDistance ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdTurn: public CUICmd
{
	OBJECT_BASIC_METHODS(CUICmdTurn);
public:
	ZDATA_(CUICmd)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }

	CUICmdTurn(): CUICmd( UICP_TURN ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdUnit: public CUICmd
{
	OBJECT_BASIC_METHODS(CUICmdUnit);
public:
	ZDATA_(CUICmd)
	CPtr<CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); f.Add(2,&pUnit); return 0; }

	CUICmdUnit() {}
	CUICmdUnit( CUnit *_pUnit ): CUICmd( UICP_UNIT ), pUnit( _pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdAIUnitWillMove (aiCommander.obj: ctor @0x36020, operator& @0x36d80, MakeCopy @0x361c0,
// DestroyContents @0x36250; registered @0x392b90, save id 0x02973150). The AI posts one each time the
// unit it is about to drive changes (CAICommander::GenerateCommand, turn-based only): a "this AI unit is
// about to act" camera/UI hint carrying the unit. Same shape as CUICmdUnit but the ctor chains the DEFAULT
// CUICmd() (no UICP_UNIT priority -- retail @0x36020 calls CUICmd::CUICmd()).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdAIUnitWillMove: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdAIUnitWillMove );
public:
	ZDATA_(CUICmd)
	CPtr<CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); f.Add(2,&pUnit); return 0; }
	//
	CUICmdAIUnitWillMove() {}
	CUICmdAIUnitWillMove( CUnit *_pUnit ): pUnit( _pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Events
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NScript::luac_BeginSequence @0x2f1890 ("b[false]"): the command carries the script's bool at
// +0x10 -- the movie letterbox's bSkipFadeOut (Common.l DelayGameStart passes BeginSequence(true) so the
// bars appear instantly behind the loading screen). Serialize tags: 1 = base (pre-existing), 2 =
// bSkipFadeOut (dev-appended -- old saves without it default to false).
class CUICmdBeginSequence: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdBeginSequence );
private:
	ZDATA_(CUICmd)
public:
	bool bSkipFadeOut;	// +0x10: skip the letterbox fade-in/out (immediate bars)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); f.Add(2,&bSkipFadeOut); return 0; }
	//
	CUICmdBeginSequence():
		CUICmd( 0 ), bSkipFadeOut( false ) {}
	CUICmdBeginSequence( bool _bSkipFadeOut ):
		CUICmd( 0 ), bSkipFadeOut( _bSkipFadeOut ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NScript::luaEndSequence @0x2f1a60 ("b[false]b[false]"): +0x10 = bRestoreCamera (pop the
// BeginSequence camera pose back onto the camera; false = commit the cutscene-end pose as the gameplay
// camera), +0x11 = bSkipFade (tear the letterbox down immediately, movieUI SetSkipFade @0x20e130).
// Serialize tags: 1 = base (pre-existing), 2/3 = bRestoreCamera/bSkipFade (dev-appended).
class CUICmdEndSequence: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdEndSequence );
private:
	ZDATA_(CUICmd)
public:
	bool bRestoreCamera;	// +0x10: restore the pose pushed by the matching BeginSequence
	bool bSkipFade;			// +0x11: skip the letterbox fade-out (immediate teardown)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); f.Add(2,&bRestoreCamera); f.Add(3,&bSkipFade); return 0; }
	//
	CUICmdEndSequence():
		CUICmd( 0 ), bRestoreCamera( false ), bSkipFade( false ) {}
	CUICmdEndSequence( bool _bRestoreCamera, bool _bSkipFade ):
		CUICmd( 0 ), bRestoreCamera( _bRestoreCamera ), bSkipFade( _bSkipFade ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdPlayDialog: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPlayDialog );
	ZDATA
public:
	ZPARENT( CUICmd );
	string szDialogCode;
	vector< CObj<NWorld::CUnit> > units;
	vector< CPtr<NWorld::CAckEvent> > phrases;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&szDialogCode); f.Add(4,&units); f.Add(5,&phrases); return 0; }
	//
	CUICmdPlayDialog() {}
	CUICmdPlayDialog( const string &_szDialogCode, const vector< CObj<NWorld::CUnit> > &_units, const vector<CPtr<CAckEvent> > &_phrases ): 
		CUICmd( UICP_HIGHEST ), units( _units ), phrases( _phrases ), szDialogCode( _szDialogCode )	{}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdPlayAck: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdPlayAck );
	ZDATA
public:
	ZPARENT( CUICmd );
	vector< CPtr<NWorld::CAckEvent> > phrases;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&phrases); return 0; }
	//
	CUICmdPlayAck() {}
	CUICmdPlayAck( const vector<CPtr<CAckEvent> > &_phrases ): CUICmd( UICP_HIGHEST ), phrases( _phrases ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdSetFloor: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdSetFloor );
	ZDATA
public:
	ZPARENT( CUICmd );
	int nFloor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&nFloor); return 0; }
	//
	CUICmdSetFloor() {}
	CUICmdSetFloor( int _nFloor ): CUICmd( UICP_HIGHEST ), nFloor( _nFloor ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdContinueChapter: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdContinueChapter );
	ZDATA
public:
	ZPARENT( CUICmd );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); return 0; }
	//
	CUICmdContinueChapter(): CUICmd( UICP_HIGHEST ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdLoadTemplate: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdLoadTemplate );
	ZDATA
public:
	ZPARENT( CUICmd );
	int nTemplateID;
	CPtr<NScenario::CScenarioZone> pZone;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&nTemplateID); f.Add(4,&pZone); return 0; }
	//
	CUICmdLoadTemplate() {}
	CUICmdLoadTemplate( NScenario::CScenarioZone *_pZone, int _nTemplateID = -1 ): 
		pZone( _pZone ), nTemplateID( _nTemplateID ), CUICmd( UICP_HIGHEST ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdShowStore: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdShowStore );
	ZDATA_(CUICmd)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }
	//
	CUICmdShowStore(): CUICmd( UICP_HIGHEST ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdShowTeamMng: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdShowTeamMng );
	ZDATA_(CUICmd)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmd*)this); return 0; }
	//
	CUICmdShowTeamMng(): CUICmd( UICP_HIGHEST ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdShowClue: public CUICmd
{
	OBJECT_BASIC_METHODS( CUICmdShowClue );
	ZDATA
	ZPARENT( CUICmd )
public:
	CPtr<NScenario::CScenarioClue> pClue;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmd *)this); f.Add(3,&pClue); return 0; }
	//
	CUICmdShowClue( NScenario::CScenarioClue *_pClue = 0 ): CUICmd( UICP_HIGHEST ), pClue( _pClue ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
