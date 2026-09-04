#ifndef __I_GAMESTATES_H__
#define __I_GAMESTATES_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Input\Bind.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI { class CTextFrame; }   // retail unit-hover tooltip window ("enemyToolTip"), owned by the tactical states
class CVec4;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowError( IMission *pMission, NWorld::EUnitCommandResult eResult );
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 selection palette dispatcher @0x5d6130 (game_selectionmode-aware).
// Index: 0 team/selected, 1 team-hilight, 2 enemy, 3 corpse, 4 object, 5 neutral.
const CVec4& GetSelectionColor( int nIndex );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Cursor ids -- retail game.db UICursors table ROW ids, resolved via NDb::GetUICursor exactly like
// retail (SCursorInfo now carries the CUICursor record itself; the record supplies the UITexture and
// the hotspot center). The former dev hedge (mapping each cursor to its UITexture id and resolving
// via GetUITexture) is REVERSED as part of the serialization convergence.
const int
	N_CURSOR_NORMAL					= 2,    // "normal"  -> Normal.cur   (UITexture 292)
	N_CURSOR_BUSY						= 3,    // "busy"    -> Busy.cur     (665)
	N_CURSOR_BLOCK					= 4,    // "block"   -> Blocked.cur  (208)
	N_CURSOR_MOVE						= 5,    // "move"    -> Normal.cur   (292, same texture as normal)
	N_CURSOR_ROTATE					= 25,   // "look" -> Look.cur (the EYE). retail CStateRotate::GetCursorInfo @0x1d68b0:
	                                 // disasm @0x5d68e2 `mov ecx,0x19` -> NDb::GetUICursor(25) = UICursors row 25 "look"
	                                 // (UITexture 291, Look.cur) -- the Look button's state shows the eye cursor
	N_CURSOR_HEAL						= 6,    // "heal"    -> Heal.cur     (205)
	N_CURSOR_ATTACK					= 7,    // "attack"  -> AttackFireArm.cur (218, generic attack fallback)
	N_CURSOR_ATTACK_MELEE		= 9,    // "attack_coldsteel" (203)
	N_CURSOR_ATTACK_RIFLE		= 10,   // "attack_firearm"   (218)
	N_CURSOR_ATTACK_PISTOL	= 10,   // (no pistol-specific cursor -> firearm)
	N_CURSOR_ATTACK_MACHINEGUN	= 10,   // (firearm)
	N_CURSOR_ATTACK_GRENADE	= 11,   // "attack_grenade"   (217)
	N_CURSOR_ATTACK_HEAD		= 12,   // "attack_head" -> HitLocationHead.cur (202)
	N_CURSOR_ATTACK_BODY		= 13,   // "attack_body"      (204)
	N_CURSOR_ATTACK_LARM		= 14,   // "attack_larm"      (492)
	N_CURSOR_ATTACK_RARM		= 15,   // "attack_rarm"      (293)
	N_CURSOR_ATTACK_LLEG		= 16,   // "attack_lleg"      (209)
	N_CURSOR_ATTACK_RLEG		= 17,   // "attack_rleg"      (294)
	N_CURSOR_OPEN_CLOSE			= 23,   // "use_openclose" -> UseOpen&Close.cur (690)
	N_CURSOR_USE						= 21,   // "use"          -> Use.cur          (207)
	N_CURSOR_USE_TOOL				= 22,   // "usetool"      -> UseTool.cur      (579)
	N_CURSOR_PICKITEM				= 18,   // "pickitem"     -> PickItem.cur     (296)
	N_CURSOR_TALK						= 20,   // "talk"         -> Talk.cur         (290)
	N_CURSOR_UNLOAD					= 24;   // "unload"  -> Unload.cur (210)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateBase: public IState
{
private:
	// TRANSIENT: retail v1.2 REMOVED CStateBase::bLButtonDown entirely (the +0xC byte -- every
	// CState* v1.2 ctor drops its init and all members shift -4; confirmed by the v1.2 save wire,
	// where the base chunk carries ONLY tag 3 (6 bytes) in all 9 audit slots). The dev keeps the
	// member for its v1.1-shaped ProcessMessage spurious-up filter, but it must NOT be serialized
	// (wire-audit: every CState* class MISSed dev-extra tag 1.2).
	bool bLButtonDown;
	ZDATA
	CPtr<IMission> pMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(3,&pMission); return 0; }	// retail v1.2 wire: tag 3 only

public:
	CStateBase( bool bNeedMouseInstantly = false );

	bool Initialize( IMission *pMission );
	void Terminate();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	bool ProcessMessage( const NUI::SEvent &sEvent );
	void Step();

	bool OnLButtonUp( int nX, int nY ) { return false; }
	bool OnLButtonDown( int nX, int nY ) { return false; }
	bool OnLButtonDblClk( int nX, int nY ) { return false; }
	NUI::SCursorInfo GetCursorInfo() const { return NUI::SCursorInfo(); }

	IMission* GetMission() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// UPDATED STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateWait
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateWait: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateWait)
private:
	ZDATA_(CStateBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); return 0; }

public:
	bool Initialize( IMission *pMission );

	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateTeam
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateTeam: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateTeam);
private:
	NInput::CBind bindModifier;
	ZDATA_(CStateBase)
	bool bModifier;
	CPtr<IUnitTracker> pUnitTracker;
	CObj<CObjectBase> pTraceSelection;
	// retail CStateTeam +0x28 pUnitToolTip (CObj<NUI::CTextFrame>): the "enemyToolTip" hover frame
	// built in Initialize (@0x1d97f0) and refilled by UpdateToolTipInfo (@0x1d8230). SERIALIZED --
	// retail CStateTeam::operator& @0x1e18b0 tag 5 (wire-audit: UNREAD 5, 4 bytes, all 9 slots).
	CObj<NUI::CTextFrame> pUnitToolTip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&bModifier); f.Add(3,&pUnitTracker); f.Add(4,&pTraceSelection); f.Add(5,&pUnitToolTip); return 0; }

protected:
	void UpdateToolTipInfo();	// retail @0x1d8230: refill the hover tooltip from the traced unit

public:
	CStateTeam();

	bool Initialize( IMission *pMission );
	void Terminate();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void Step();

	bool OnLButtonUp( int nX, int nY );

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateFriend
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateFriend: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateFriend);
private:
	ZDATA_(CStateBase)
	NUI::SCursorInfo sCursorInfo;
	CObj<CObjectBase> pTraceSelection;
	// retail CStateFriend +0x2c pUnitToolTip (CObj<NUI::CTextFrame>): the name+VP hover tooltip.
	// SERIALIZED -- retail CStateFriend::operator& @0x1e1be0 tag 4 (wire-audit: UNREAD 4, 4 bytes).
	CObj<NUI::CTextFrame> pUnitToolTip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); f.Add(4,&pUnitToolTip); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();
	void UpdateToolTipInfo();   // retail @0x1d8290: refill the tooltip from the traced unit

public:
	bool Initialize( IMission *pMission );
	void Terminate();
	void Step();

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateMove
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateMove: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateMove);
private:
	ZDATA_(CStateBase)
	bool bForced;
	bool bAnchorSet;
	CVec2 vAnchor;
	NUI::SCursorInfo sCursorInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&bForced); f.Add(3,&bAnchorSet); f.Add(4,&vAnchor); f.Add(5,&sCursorInfo); return 0; }

protected:
	void DoMove( bool bInstant );
	void UpdateCursor();

public:
	CStateMove() {}
	CStateMove( bool bForced );

	bool Initialize( IMission *pMission );
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void Step();

	bool OnLButtonUp( int nX, int nY );
	bool OnLButtonDown( int nX, int nY );
	bool OnLButtonDblClk( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateAttack
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateAttack: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateAttack);
private:
	NInput::CBind bindHitLocationHead, bindHitLocationBody, bindHitLocationLArm, bindHitLocationRArm, bindHitLocationLLeg, bindHitLocationRLeg;
	ZDATA_(CStateBase)
	bool bForced;
	// retail CStateAttack +0x48 sInfo (NGame::SActionInfo, 20 bytes): the cached feasibility of the
	// current target command, refreshed by UpdateInfo (@0x1da140 -- reset to ctor defaults, then
	// GetTargetCmd + CanDoCommand). Replaces the old dev trio nActionAP/bActionUnavailable/bEnoughAP
	// (bActionUnavailable == !sInfo.bOk -- in the retail eResult switch bOk=true implies
	// bAvailable=true, so !bAvailable||!bOk collapses to !bOk).
	SActionInfo sInfo;
	NUI::SCursorInfo sCursorInfo;
	NAI::EHitLocation eHitLocation;
	CObj<CObjectBase> pTraceSelection;
	// retail CStateAttack +0x74 pEnemyToolTip (PDB name; CObj<NUI::CTextFrame>): the "enemyToolTip"
	// frame over the aimed enemy, refilled by UpdateEnemyStateInfo (@0x1d8540). SERIALIZED (tag 7).
	CObj<NUI::CTextFrame> pEnemyToolTip;
	// retail CStateAttack::operator& @0x1e1b40 wire (v1.2-save-verified: 1:6 2:1 3:20 4:10|405 5:4 6:4 7:4):
	// 1 base, 2 bForced, 3 sInfo (RAW 20 bytes), 4 sCursorInfo, 5 eHitLocation, 6 pTraceSelection, 7 pEnemyToolTip
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&bForced); f.Add(3,&sInfo); f.Add(4,&sCursorInfo); f.Add(5,&eHitLocation); f.Add(6,&pTraceSelection); f.Add(7,&pEnemyToolTip); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();
	void UpdateCursor();
	void UpdateCursorInfo();
	void UpdateInfo();              // retail @0x1da140 (was dev UpdateBlockedState)
	void UpdateTraceSelection();
	void UpdateEnemyStateInfo();    // retail @0x1d8540 (was dev UpdateToolTipInfo)

public:
	CStateAttack();
	CStateAttack( bool bForced );

	bool Initialize( IMission *pMission );
	void Terminate();
	void Step();

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUse
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateUse: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateUse);
private:
	ZDATA_(CStateBase)
	NUI::SCursorInfo sCursorInfo;
	CObj<CObjectBase> pTraceSelection;
	// retail CStateUse +0x2c pUnitToolTip (CObj<NUI::CTextFrame>): the "enemyToolTip" frame built in
	// Initialize (@0x1d85a0) for an UNCONSCIOUS corpse unit under the cursor, released in Terminate
	// (@0x1d6fc0). SERIALIZED -- retail CStateUse::operator& is COMDAT-folded onto CStateFriend's
	// @0x1e1be0 (identical layout), tag 4 (wire-audit: UNREAD 4, 4 bytes, all 9 slots).
	CObj<NUI::CTextFrame> pUnitToolTip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); f.Add(4,&pUnitToolTip); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateUse() {}

	bool Initialize( IMission *pMission );
	void Terminate();

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatePickItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStatePickItem: public CStateBase
{
	OBJECT_BASIC_METHODS(CStatePickItem);
private:
	ZDATA_(CStateBase)
	// retail CStatePickItem +0x14 sCursorInfo: the CACHED pick-item cursor, filled in Initialize
	// (@0x1dc570: bOk -> GetUICursor(18 "pickitem") + MakeCursorString AP caption, else
	// GetUICursor(4 "block") + L"") and returned by GetCursorInfo (@0x1d6770). SERIALIZED --
	// retail CStatePickItem::operator& @0x1e1dc0 tag 2 (wire-audit: RAWSZ 2.2 save=10|191 dev=4).
	NUI::SCursorInfo sCursorInfo;
	CObj<CObjectBase> pTraceSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStatePickItem() {}

	bool Initialize( IMission *pMission );
	void Terminate();

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateDropItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateDragItem: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateDragItem);
private:
	ZDATA_(CStateBase)
	CObj<NUI::CModel> pModel;
	// retail CStateDragItem +0x18 sCursorInfo: the CACHED drag cursor, refreshed by UpdateCursor
	// (@0x1dccf0: bOk -> GetUICursor(2 "normal") + (nMaxAP>0 ? MakeCursorString caption : L""),
	// else GetUICursor(4 "block") + L"") and returned by GetCursorInfo (@0x1d6790). SERIALIZED --
	// retail CStateDragItem::operator& @0x1e1c40 tag 3 (wire-audit: UNREAD 3, 10 bytes, all slots).
	NUI::SCursorInfo sCursorInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&pModel); f.Add(3,&sCursorInfo); return 0; }

protected:
	// retail @0x1dc960: build the drop command for the cursor target. bAllowSlot gates the
	// NUI::CSlotInfo (inventory-slot) target leg -- UpdateCursor peeks with true (`push 1`
	// @0x5dcd11), OnLButtonDown commits with false (a click on a slot falls through to the slot
	// window's own handler).
	NWorld::CCmd* GetTargetCmd( bool bAllowSlot );
	void UpdateCursor();			// retail @0x1dccf0

public:
	bool Initialize( IMission *pMission );
	void Terminate();
	void Step();

	bool OnLButtonUp( int nX, int nY );
	bool OnLButtonDown( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;	// retail @0x1d6790: return the cached sCursorInfo

	EType GetType() const { return UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUntrap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateUntrap: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateUntrap);
private:
	ZDATA_(CStateBase)
	// retail CStateUntrap +0x14 bForced: set by the bool ctor (@0x1d72a0). The forced use-tool
	// state comes from the "usetool" key/icon (retail CMission::ProcessEvent @0x202600 constructs
	// CStateUntrap(true)); the hover pool instance is CStateUntrap(false) (CMission::Initialize
	// @0x200690). GetType (@0x1d5960) returns FORCED for a forced untrap. SERIALIZED -- retail
	// CStateUntrap::operator& @0x1e1d20 tag 2 (wire-audit: MISS 2.2/2.3 -- dev's sCursorInfo sat
	// on retail's bForced tag; sCursorInfo is retail tag 3, UNREAD 3, 10 bytes).
	bool bForced;
	NUI::SCursorInfo sCursorInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&bForced); f.Add(3,&sCursorInfo); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateUntrap(): bForced( false ) {}						// retail default ctor @0x1e00f0 leaves bForced at the zeroed head
	CStateUntrap( bool _bForced ): bForced( _bForced ) {}	// retail @0x1d72a0

	bool Initialize( IMission *pMission );

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	// retail CStateUntrap::GetType @0x1d5960: `return (uint)(bForced == false)` -- FORCED(0) when forced
	EType GetType() const { return bForced ? FORCED : UPDATED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSelection
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateSelection: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateSelection);
private:
	ZDATA_(CStateBase)
	CVec2 vAnchor;
	CObj<NUI::CImage> pSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&vAnchor); f.Add(3,&pSelection); f.Add(4,&pLockedCamera); return 0; }
	CObj<ICamera> pLockedCamera;

public:
	CStateSelection() {}
	CStateSelection( const CVec2 &vAnchor );

	bool Initialize( IMission *pMission );
	void Terminate();
	void Step();

	void Cancel();
	void Handle();

	EType GetType() const { return TEMPORARY; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUnloadItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateUnloadItem: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateUnloadItem)
private:
	NInput::CBind bindCancel;
	ZDATA_(CStateBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateUnloadItem();

	bool Initialize( IMission *pMission );
	bool ProcessEvent( const NInput::SEvent &sEvent );

	bool OnLButtonUp( int nX, int nY );
	bool OnLButtonDown( int nX, int nY );
	bool OnLButtonDblClk( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return TEMPORARY; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// FORCED STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateRotate
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateRotate: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateRotate);
private:
	ZDATA_(CStateBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); return 0; }

public:
	CStateRotate() {}

	bool Initialize( IMission *pMission );

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return FORCED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSetTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateSetTrap: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateSetTrap);
private:
	ZDATA_(CStateBase)
	NUI::SCursorInfo sCursorInfo;
	// retail CStateSetTrap +0x24 sLastPosition (NAI::SPosition): the last hovered position, kept by
	// UpdateCursor so the cursor is rebuilt only when the trace target moves (same idiom as
	// CStateSetMine, members in the opposite order). SERIALIZED -- retail CStateSetTrap::operator&
	// @0x205500 tag 3 (note: SetTrap = {2 sCursorInfo, 3 sLastPosition}; SetMine = {2 sLastPosition,
	// 3 sCursorInfo}).
	NAI::SPosition sLastPosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&sLastPosition); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();
	void UpdateCursor();	// retail @0x1d8cc0 (mirrors CStateSetMine::UpdateCursor @0x1d8cd0)

public:
	CStateSetTrap() {}

	bool Initialize( IMission *pMission );
	void Step();			// retail vftable slot @0x1d8cc0-adjacent: per-frame cursor refresh like SetMine

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return FORCED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSetMine
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateSetMine: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateSetMine);
private:
	ZDATA_(CStateBase)
	NAI::SPosition sLastPosition;
	NUI::SCursorInfo sCursorInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sLastPosition); f.Add(3,&sCursorInfo); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();
	void UpdateCursor();

public:
	CStateSetMine() {}

	bool Initialize( IMission *pMission );
	void Step();

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return FORCED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateFirstAid
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateFirstAid: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateFirstAid);
private:
	ZDATA_(CStateBase)
	NUI::SCursorInfo sCursorInfo;
	CObj<CObjectBase> pTraceSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateFirstAid() {}

	bool Initialize( IMission *pMission );

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return FORCED; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// INSTANT STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateEmpty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateEmpty: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateEmpty)
private:
	ZDATA_(CStateBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); return 0; }

public:
	bool Initialize( IMission *pMission );

	EType GetType() const { return INSTANT; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatePose
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStatePose: public CStateBase
{
	OBJECT_BASIC_METHODS(CStatePose);
private:
	ZDATA_(CStateBase)
	NAI::EPose ePose;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&ePose); return 0; }

public:
	CStatePose() {}
	CStatePose( NAI::EPose ePose );

	bool Initialize( IMission *pMission );

	EType GetType() const { return INSTANT; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateDropCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateDropCorpse: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateDropCorpse);
private:
	ZDATA_(CStateBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); return 0; }

public:
	CStateDropCorpse() {}

	bool Initialize( IMission *pMission );

	EType GetType() const { return INSTANT; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateMoveItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateMoveItem: public CStateBase
{
	OBJECT_BASIC_METHODS(CStateMoveItem);
private:
	ZDATA_(CStateBase)
	CPtr<NWorld::CUnit> pUnit;
	NWorld::SItem sSource;
	NWorld::SItem sTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&pUnit); f.Add(3,&sSource); f.Add(4,&sTarget); return 0; }

public:
	CStateMoveItem() {}
	CStateMoveItem( NWorld::CUnit *pUnit, const NWorld::SItem &sSource, const NWorld::SItem &sTarget );

	bool Initialize( IMission *pMission );

	EType GetType() const { return INSTANT; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
