#ifndef __I_GAMESTATES_H__
#define __I_GAMESTATES_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Input\Bind.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI { class CTextFrame; }   // retail unit-hover tooltip window ("enemyToolTip"), owned by the tactical states
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowError( IMission *pMission, NWorld::EUnitCommandResult eResult );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Cursor UITexture ids. The whole block was mis-mapped (dev used arbitrary ids, so e.g. N_CURSOR_MOVE
// was 202 = "HitLocation - Head" -> the body-part targeting cursor showed over open ground). These now
// match the retail cursor->UITexture mapping in the game.db UICursors table exactly (retail resolves
// them via GetUICursor(name); this dev tree resolves the mapped UITexture id via GetUITexture).
const int
	N_CURSOR_NORMAL					= 292,   // "normal"  -> Normal.cur
	N_CURSOR_BUSY						= 665,   // "busy"    -> Busy.cur
	N_CURSOR_BLOCK					= 208,   // "block"   -> Blocked.cur
	N_CURSOR_MOVE						= 292,   // "move"    -> Normal.cur (same texture as normal)
	N_CURSOR_ROTATE					= 291,   // "look" -> Look.cur (the EYE). retail CStateRotate::GetCursorInfo @0x1d68b0:
	                                 // disasm @0x5d68e2 `mov ecx,0x19` -> NDb::GetUICursor(25) = UICursors row 25 "look"
	                                 // (UITexture 291, Look.cur) -- the Look button's state shows the eye cursor
	N_CURSOR_HEAL						= 205,   // "heal"    -> Heal.cur
	N_CURSOR_ATTACK					= 218,   // "attack"  -> AttackFireArm.cur (generic attack fallback)
	N_CURSOR_ATTACK_MELEE		= 203,   // "attack_coldsteel"
	N_CURSOR_ATTACK_RIFLE		= 218,   // "attack_firearm"
	N_CURSOR_ATTACK_PISTOL	= 218,   // (no pistol-specific cursor -> firearm)
	N_CURSOR_ATTACK_MACHINEGUN	= 218,   // (firearm)
	N_CURSOR_ATTACK_GRENADE	= 217,   // "attack_grenade"
	N_CURSOR_ATTACK_HEAD		= 202,   // "attack_head" -> HitLocationHead.cur
	N_CURSOR_ATTACK_BODY		= 204,   // "attack_body"
	N_CURSOR_ATTACK_LARM		= 492,   // "attack_larm"
	N_CURSOR_ATTACK_RARM		= 293,   // "attack_rarm"
	N_CURSOR_ATTACK_LLEG		= 209,   // "attack_lleg"
	N_CURSOR_ATTACK_RLEG		= 294,   // "attack_rleg"
	N_CURSOR_OPEN_CLOSE			= 690,   // "use_openclose" -> UseOpen&Close.cur (UICursors row 23)
	N_CURSOR_USE						= 207,   // "use"          -> Use.cur          (UICursors row 21)
	N_CURSOR_USE_TOOL				= 579,   // "usetool"      -> UseTool.cur      (UICursors row 22)
	N_CURSOR_PICKITEM				= 296,   // "pickitem"     -> PickItem.cur     (UICursors row 18)
	N_CURSOR_TALK						= 290,   // "talk"         -> Talk.cur         (UICursors row 20)
	N_CURSOR_UNLOAD					= 210;   // "unload"  -> Unload.cur
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStateBase: public IState
{
private:
	ZDATA
	bool bLButtonDown;
	CPtr<IMission> pMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bLButtonDown); f.Add(3,&pMission); return 0; }

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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&bModifier); f.Add(3,&pUnitTracker); f.Add(4,&pTraceSelection); return 0; }

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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); return 0; }
	// retail CStateFriend +0x2c pUnitToolTip (CObj<NUI::CTextFrame>, here the dev CToolTip window):
	// the name+VP hover tooltip. Runtime-only (the state is re-Initialized on load), so not serialized.
	CObj<NUI::CTextFrame> pUnitToolTip;

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
	int nActionAP;
	bool bForced;
	bool bActionUnavailable;
	NUI::SCursorInfo sCursorInfo;
	NAI::EHitLocation eHitLocation;
	CObj<CObjectBase> pTraceSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&nActionAP); f.Add(3,&bForced); f.Add(4,&bActionUnavailable); f.Add(5,&sCursorInfo); f.Add(6,&eHitLocation); f.Add(7,&pTraceSelection); return 0; }
	// retail CStateAttack tooltip frame (@0x1d8540 UpdateToolTipInfo) -- name+VP over the aimed enemy;
	// runtime-only (re-Initialized on load).
	CObj<NUI::CTextFrame> pUnitToolTip;

protected:
	NWorld::CCmd* GetTargetCmd();
	void UpdateCursor();
	void UpdateCursorInfo();
	void UpdateBlockedState();
	void UpdateTraceSelection();
	void UpdateToolTipInfo();   // retail @0x1d8540

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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); f.Add(3,&pTraceSelection); return 0; }

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
	CObj<CObjectBase> pTraceSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&pTraceSelection); return 0; }

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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&pModel); return 0; }

public:
	bool Initialize( IMission *pMission );
	void Terminate();
	void Step();

	bool OnLButtonUp( int nX, int nY );
	bool OnLButtonDown( int nX, int nY );

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
	NUI::SCursorInfo sCursorInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateUntrap() {}

	bool Initialize( IMission *pMission );

	bool OnLButtonUp( int nX, int nY );
	NUI::SCursorInfo GetCursorInfo() const;

	EType GetType() const { return UPDATED; }
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CStateBase*)this); f.Add(2,&sCursorInfo); return 0; }

protected:
	NWorld::CCmd* GetTargetCmd();

public:
	CStateSetTrap() {}

	bool Initialize( IMission *pMission );

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
