#ifndef __WINTERFACE_H_
#define __WINTERFACE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DG.h"
#include "Time.h"
#include "Sync.h"
#include "wTSFlags.h"
#include "aiPosition.h"
#include "../DBFormat/DataAck.h"
#include "../DBFormat/DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCTime;
struct SRandomSeed;
class CTerrainPart;
namespace NLSHead { class CHeadInfo; }   // NWorld::CUnit::GetHeadInfo (baked static head -> CHeadsController)
namespace NRPG
{
	struct SStoreItem;
	class CGlobalGame;
	class CGlobalPlayer;
	struct SUnitInfo;
	class IGame;
	class IInventoryItem;
	class IUnitMissionInfo;
	class CGlobalDiplomacy;
	class CUnit;
}
namespace NDb
{
	class CModel;
	class CAmbientLightReal;
	//class CDBAckInfo;
	class CDBAckSequence;
	class CComplexHead;
	class CPanzerklein;
	class CRPGGrenade;       // IExplosionMaster::AddExplosion (ordinary grenade)
	class CRPGEngGrenade;    // IExplosionMaster::AddExplosion (engineer grenade)
	enum EDiplomacyState;
}
namespace NWorld
{
	class CUnit;
	class CPlayer;
	class CGlobalAck;
	class CPocket;               // wPocket.h -- IWorld::GetPocket (retail vtbl+0xd8 @0x376f60)
	class IHeightLayers;         // wHeightLayers.h -- IWorld::GetHeightLayers (retail vtbl+0x90 @0x376f00)
	enum ETBSEvent;
	class CUnitServer;
	enum EUnitCommandResult;
	class CCmd;
	struct SItem;
	struct SPerkMineModifiers;   // wExplosionPerks.h (IExplosionMaster::AddExplosion)
}
namespace NAI
{
	class CPath;
	class IAIMap;
	class IPathNetwork;
	struct SPathPlace;
	struct SPosition;
	struct SUnitPosition;
	enum EPose;
}
namespace NScenario
{
	class CScenarioClue;
}
namespace NScript
{
	class CLUACallParam;
}
struct STerrainInfo;
struct SMapBuilding;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommand: public CObjectBase
{
public:
	virtual bool IsSkippable() const { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCheat: public CCommand
{
	OBJECT_BASIC_METHODS(CCmdCheat);
public:
	ZDATA
	int nCheatMask;
	bool bState;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nCheatMask); f.Add(3,&bState); return 0; }

public:
	CCmdCheat() {}
	CCmdCheat( int _nCheatMask, bool _bState ): nCheatMask( _nCheatMask ), bState( _bState ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdEndOfTurn: public CCommand
{
	OBJECT_BASIC_METHODS(CCmdEndOfTurn);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EInterfaceActionType
{
	IAT_CAMERA = 0,
	IAT_DIALOG,
	N_INTERFACE_ACTION_TYPE
};
//
enum EInterfaceEvent
{
	IE_ACTION_STARTED = 0,
	IE_ACTION_FINISHED,
	N_INTERFACE_EVENTS
};
//
class CCmdInterfaceEvent: public CCommand
{
	OBJECT_BASIC_METHODS( CCmdInterfaceEvent );
	ZDATA
public:
	int nID;		// release: id of the finished UI command. CWorld::ExecuteCommand routes it to
				// pOwnScript->RemoveUIActionID(nID), which unblocks the lua WaitForUI(id) that queued it.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); return 0; }
	//
	CCmdInterfaceEvent() {}
	CCmdInterfaceEvent( int _nID ): nID( _nID ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCallScriptFunction: public CCommand
{
	OBJECT_BASIC_METHODS( CCmdCallScriptFunction );
	ZDATA
public:
	string szFuncName;
	vector< CObj<NScript::CLUACallParam> > params;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&szFuncName); f.Add(3,&params); return 0; }
	//
	CCmdCallScriptFunction() {}
	CCmdCallScriptFunction( string _szFuncName, char *szParams, ...  );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail-new (iMission.obj; ctor @0x204b50, operator& @0x205440): wraps a game-over script call to be
// fired LATER. CMission::GameStep wraps OnPlayerLose in it (nMaxDelay = 4000ms) on a natural (non-
// scenario) single-player defeat; CWorld's command drain stashes the payload (pGameOverCall /
// tMaxGameOverCall = now + nMaxDelay) and fires it when the hero's corpse settles
// (CWorld::InformCorpseStop @0x362180) or when the cap expires (CWorld::Segment tail @0x36bce0).
// NOT saveload-registered in retail (serialization audit: retail_registered=false) -- only the
// serializer exists (tags 2/3); the stashed CObj<CCmdCallScriptFunction> is what reaches the save.
class CCmdDelayedCallGameOver: public CCommand
{
	OBJECT_BASIC_METHODS( CCmdDelayedCallGameOver );
	ZDATA
public:
	CObj<CCmdCallScriptFunction> pGameOverCommand;	// retail @0x0c
	int nMaxDelay;									// retail @0x10 (ms)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGameOverCommand); f.Add(3,&nMaxDelay); return 0; }
	//
	CCmdDelayedCallGameOver(): nMaxDelay( 0 ) {}
	// retail ctor @0x204b50: adopt the wrapped call (CObj assign AddRefs) + the delay cap
	CCmdDelayedCallGameOver( CCmdCallScriptFunction *_pGameOverCommand, int _nMaxDelay ):
		pGameOverCommand( _pGameOverCommand ), nMaxDelay( _nMaxDelay ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (dev CCmdQuitGame 0x02741156 REMOVED -- W5 serialization-convergence: never constructed, its only
// dispatch was an ASSERT(0) stub, and the id is ABSENT from retail. Retail quits through the UI
// bind layer -- CMainMenuInterface::ProcessEvent @0x1f7540 / CExitMenuInterface @0x1d1310 /
// CLoseMenuInterface @0x1f3880 -- never a world command.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommander: public CObjectBase
{
	OBJECT_BASIC_METHODS(CCommander);
	ZDATA
	list< CObj<CCommand> > cmds;
	bool bInterruptRequest, bStopAction;
public:
	ZEND 	list< CObj<CCommand> > events;
	int operator&( CStructureSaver &f ) { f.Add(2,&cmds); f.Add(3,&bInterruptRequest); f.Add(4,&bStopAction); f.Add(5,&events); return 0; }
	CCommander(): bInterruptRequest(false), bStopAction(false) {}
	bool IsRequestCancel() const { return bStopAction; }
	virtual bool IsRequestInterrupt() const { return bInterruptRequest; }
	void ClearRequests() { bInterruptRequest = false; bStopAction = false; }
	void Do( CCommand *pCmd ) { if ( !pCmd->IsSkippable() ) bInterruptRequest = true; cmds.push_back( pCmd ); }
	void ClearList() { cmds.clear(); }
	bool HasCommands() const { return !cmds.empty(); }
	void StopAction() { ClearList(); bStopAction = true; }
	CCommand* GetCommand()
	{
		if ( cmds.empty() )
			GenerateCommand();
		if ( cmds.empty() )
			return 0;
		CCommand *pRes = cmds.front().Extract();
		cmds.pop_front();
		return pRes;
	}
	virtual void GenerateCommand() {}
	virtual void OnTBSEvent( ETBSEvent event ) {}
	virtual void OnPassControl( CPlayer *pPlayer ) {}
	virtual void OnUnitDied( CUnitServer *pUnit ) {}
	virtual void Segment() {}
	virtual void OnUnitAdded( CUnitServer *pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IItem : virtual public CObjectBase
{
public:
	// retail IItem vftable slot 0 (CDFrozenItem vft 6BIItem +0x4c, a COMDAT-folded trivial getter):
	// the item's floor -- CMissionBase::FocusCameraOnItem @0x1a1740 cuts the scene to it.
	virtual int GetFloor() const = 0;
	virtual CVec3 GetPos() const = 0;
	virtual NRPG::IInventoryItem* GetInvItem() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IObject : virtual public CObjectBase
{
public:
	virtual bool IsTargetable() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPassageObject: virtual public CObjectBase 
{
public:
	virtual bool IsBroken() const = 0;
	virtual bool CanPass( CUnitServer *pUS ) = 0;
	virtual bool UsePassageObject( CUnitServer *pUS ) = 0;
	virtual void GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches ) = 0;
	virtual int GetAPRadius() const = 0;
	virtual int GetPassageZoneID() const = 0;
	virtual int GetPassageObjectID() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IWindowDoor
{
public:
	virtual bool IsBroken() const = 0;
	virtual bool IsOpen() const = 0;
	virtual void OpenClose( bool bOpen, bool bAbruptly, CUnitServer *pWho = 0 ) = 0;
	virtual CVec3 GetChangeStateDirection( bool bOpen ) const = 0;
	virtual void LockDoor( bool bLock, int nKeyID, int nLockHardness ) = 0;
	virtual bool IsLockedDoor() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMine : virtual public CObjectBase
{
public:
	virtual int GetMineDC() = 0;
	virtual CVec3 GetMinePos() = 0;
	virtual bool IsMineSet() = 0;
	virtual NDb::CRPGItem* DisarmMine() = 0;
	virtual bool IsHiddenObject() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail free NWorld::GoBoom @0x37e530 (wMine.obj): detonate an IMine (dynamic-cast to CMine, no
// attributed shooter). Fired by the stability trackers when the ground under a mine drops away.
void GoBoom( IMine *pMine );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit;
class ICannon
{
public:
	virtual bool IsBroken() const = 0;
	virtual bool IsOccupied() const = 0;
	virtual CUnit* GetCurrentUnit() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IBuilding
{
public:
	virtual const SMapBuilding& GetInfo() const = 0;
	virtual void UpdateAllParts() = 0;
	virtual void Update() = 0;
	virtual CObjectBase* GetSceneHandle() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IPathViewer
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPathPoint
{
	ZDATA
	int nAP;
	int nFloor;
	CVec3 vPoint;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nAP); f.Add(3,&nFloor); f.Add(4,&vPoint); return 0; }

	SPathPoint() {}
	SPathPoint( int _nAP, int _nFloor, CVec3 _vPoint ): nAP( _nAP ), nFloor( _nFloor ), vPoint( _vPoint ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPathViewer: public CObjectBase
{
public:
	virtual void SetPath( NAI::CPath *pPath ) = 0;

	virtual int GetResult() const = 0;
	virtual void GetPoints( vector<SPathPoint> *pRes ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPlayer;
struct IVisObj;
class CUnit: virtual public CObjectBase
{
public:
	enum EState
	{
		ST_NORMAL_DEFAULT,
		ST_NORMAL_PISTOL,
		ST_NORMAL_RIFLE,
		ST_NORMAL_SUB_MACHINE_GUN,
		ST_NORMAL_KNIFE,
		ST_NORMAL_HAND_MACHINE_GUN,
		ST_NORMAL_RLAUNCHER,
		ST_MACHINE_GUN,
		ST_CARRY_CORPSE,
		ST_NORMAL_MEDKIT,
		ST_NORMAL_MELEE,
		ST_NORMAL_GRENADE,
		ST_NORMAL_TOOL,
		ST_NORMAL_KEY,
		ST_HEALER,
		ST_SNIPE,
		ST_NORMAL_MINE
	};
	virtual bool IsMoving() const = 0;
	virtual bool IsDead() const = 0;
	virtual bool IsUnconscious() const = 0;
	// retail exposes CanFight on the world-unit interface (CUnitServer::CanFight body); the
	// inventory can't-use tint (CSlot::Draw @0x1c34d0) reddens EVERYTHING for a downed unit.
	virtual bool CanFight() const { return !IsDead() && !IsUnconscious(); }
	virtual bool IsHiding() const = 0;
	virtual bool IsEmptyPK() const = 0;
	virtual bool IsStrafing() const = 0;
	virtual bool IsCarryingCorpse() const = 0;
	virtual bool IsPerformingAction() const { return false; }
	virtual CUnit* GetCorpseCarrier() const = 0;
	// retail CUnit vtbl+0x94 = GetCorpseHLs @0x3c68e0: the 6 hit-location ray points of a downed body
	// (CDumbUnitServer::corpseHLpos, refreshed in Segment @0x350960). Consumed by the corpse-sighting
	// probe CGame::IsCorpseVisible @0x298da0. Default 0 for non-corpse-capable impls.
	virtual const vector<CVec3>* GetCorpseHLs() const { return 0; }
	virtual NDb::CModel* GetModel() const = 0;
	virtual void GetVisible( vector<CPtr<CUnit> > *pTarget ) const = 0;
	virtual void GetInfo( NRPG::SUnitInfo *pInfo ) const = 0;
	virtual IPlayer* GetPlayer() const = 0;
	virtual NAI::CPath* GetCurrentPath() = 0;
	virtual IPathViewer* CreatePathViewer() = 0;
	virtual NRPG::IUnitMissionInfo* GetRPG() const = 0;
	virtual const NAI::SUnitPosition& GetPosition() const = 0;
	// retail CUnit vtbl+0x48 @0x3c6a00 -> @0x34f430: pose-change anchor -- nextLock (the tile being
	// stepped into) when mid-step (bLocksTwoPlaces) in realtime, else the current position.
	virtual NAI::SUnitPosition GetSetPosePosition() const = 0;
	virtual void GetRealPosition( CVec3 *pRes ) = 0;
	virtual void AddVisitableChildren( vector<IVisObj*> *pRes ) = 0;
	virtual bool GetCurrentCommandName( string *pName ) const = 0;
	virtual CVec3 GetAttackOrigin() const = 0;
	virtual CVec3 GetAttackOrigin( const NAI::SUnitPosition &from ) const = 0;
	virtual float GetMinClearDistance() const = 0;
	virtual const CObjectBase* GetAttackIgnore() const = 0;
	virtual EUnitCommandResult CanDo( CCmd *p, int *pnStartAP = 0, int *pnFullAP = 0 ) = 0; // destroys CCmd if it has zero references
	virtual bool HasEnoughAP() = 0; // to start execution of current command, return false if no command is set
	virtual EState GetState() = 0;
	virtual NDb::CComplexHead* GetDBHead() = 0;
	virtual SRandomSeed GetHeadSeed() = 0;   // per-unit head-rnd seed (retail CreateLSHead source; impl in CUnitServer)
	// The unit's live head info (NULL = none): a committed advanced-FaceGen hero carries a baked static head
	// (CHeadInfo::pMesh) that CHeadsController::GetAnimator renders instead of the base mesh. Impl in CUnitServer.
	virtual NLSHead::CHeadInfo* GetHeadInfo() { return 0; }
	virtual bool IsCapPresent() = 0;
	virtual int GetCarefulShotExtraAP() = 0;
	virtual bool IsCheatEnabled( int nCheat ) = 0;
	virtual bool CanStrafe() = 0;
	virtual NDb::CPanzerklein *GetWearingDBPK() = 0;
	virtual CObjectBase* GetAIMapHull() = 0;
	// release CUnitServer::GetBleeding @0x7c68d0 (int @+0xb0): mission-scripted bleeding counter,
	// shown as a permanent fake critical icon (NUI::UpdateCriticalIcons). No in-exe writer exists
	// (trigger/reflection-set); the default 0 = no icon, which matches unscripted play.
	virtual int GetBleeding() const { return 0; }
	////
	NAI::EPose GetPose() const;
	virtual bool IsUnitVisible( const CUnit *pUnit ) const = 0;
	virtual bool IsUnitAudible( const CUnit *pUnit ) const = 0;
	virtual bool CanTalk() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckEvent
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEvent: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAckEvent);
public:
	ZDATA
	int nPriority; // 
	CPtr<NWorld::CUnit> pUnit; //  
	CDBPtr<NDb::CDBAckInfo> pAckInfo; // ack
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPriority); f.Add(3,&pUnit); f.Add(4,&pAckInfo); return 0; }

	CAckEvent() {}
	CAckEvent( int _nPriority, NWorld::CUnit *_pUnit, NDb::CDBAckInfo *_pAckInfo ): nPriority( _nPriority ), pUnit( _pUnit ), pAckInfo( _pAckInfo ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHitLocator: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHitLocator);
public:
	ZDATA
	int nHitValue;
	// retail PDB layout: +0x0c nHitValue [tag 2], +0x10 bool bPK [tag 3], +0x14 vPosition [tag 4],
	// +0x20 CPtr<CUnit> pUnit (NOT serialized). bPK = the damage receiver was a Panzerklein
	// (CReceivedDmg.type == RD_PK at the ProcessAttack site @0x350e20) -- the hit-number floater
	// colors PK hits differently (retail NUI::CHitText).
	bool bPK;
	CVec3 vPosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nHitValue); f.Add(3,&bPK); f.Add(4,&vPosition); return 0; }   // retail @0x353120
	CPtr<NWorld::CUnit> pUnit;   // the hit unit (retail back-ref, not in the save stream)

	CHitLocator(): nHitValue( 0 ), bPK( false ), vPosition( 0, 0, 0 ) {}
	// retail 4-arg ctor @0x3530c0
	CHitLocator( int _nHitValue, bool _bPK, const CVec3 &_vPosition, NWorld::CUnit *_pUnit = 0 ):
		nHitValue( _nHitValue ), bPK( _bPK ), vPosition( _vPosition ), pUnit( _pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdUnit: public CCommand
{
public:
	ZDATA
	CPtr<CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); return 0; }
	CCmdUnit() {}
	CCmdUnit( CUnit *_pUnit ): pUnit(_pUnit) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdCancel: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdCancel);
public:
	CCmdCancel() {}
	CCmdCancel( CUnit *_pUnit ): CCmdUnit(_pUnit) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CWorld::PlayAck @0x361e50 ack kinds (jump table 0x761ef4)
enum EInterfaceAcks
{
	IA_WEAPON_EMPTY				= 0,
	IA_CONFIRMATION				= 1,
	IA_IMPOSSIBLE_TO_PERFORM	= 2,
	IA_NO_PLACE_IN_INVENTORY	= 3,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CCmdPlayAck (ctor @0x1de1c0, saveload 0x23075380): the UI-side "say an interface
// ack" request. CRITICAL: it is dispatched through the ONE-ARG mission Command channel (world
// commander queue) and consumed by CWorld::ExecuteCommand -> CGlobalAck -- it must NEVER go through
// Command(unit, cmd)/CCmdSetCommand: that path lands in CUnitServer::Do, which CANCELS the unit's
// running executor to install the new command (the "orders die after one step" regression).
class CCmdPlayAck: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdPlayAck);
	ZDATA_(CCmdUnit)
	int eAck;
	// retail @0x1e1690: {2 = pUnit direct, 3 = condition} -- NO base chunk (retail owns pUnit itself; W5)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&eAck); return 0; }
public:
	CCmdPlayAck(): eAck( IA_CONFIRMATION ) {}
	CCmdPlayAck( CUnit *_pUnit, EInterfaceAcks _eAck ): CCmdUnit(_pUnit), eAck(_eAck) {}
	EInterfaceAcks GetAck() const { return (EInterfaceAcks)eAck; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdSetCommand : public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdSetCommand);
	ZDATA_(CCmdUnit)
	CObj<CCmd> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&pCmd); return 0; }
public:
	CCmdSetCommand() {}
	CCmdSetCommand( CUnit *_pUnit, CCmd *_p ): CCmdUnit(_pUnit), pCmd(_p) {}
	CCmd* GetCmd() const { return pCmd; }
	bool IsSkippable() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdAddUnit: public CCommand
{
	OBJECT_BASIC_METHODS(CCmdAddUnit);
public:
	ZDATA
	CPtr<IPlayer> pPlayer;
	NAI::SPosition sPos;
	CObj<NRPG::CUnit> pMerc;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPlayer); f.Add(3,&sPos); f.Add(4,&pMerc); return 0; }
	//
	CCmdAddUnit() {}
	CCmdAddUnit( IPlayer* _pPlayer, const NAI::SPosition &_sPos, NRPG::CUnit *_pMerc ): pPlayer( _pPlayer ), sPos( _sPos ), pMerc( _pMerc ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdRemoveUnit: public CCommand
{
	OBJECT_BASIC_METHODS(CCmdRemoveUnit);
public:
	ZDATA
	CPtr<CUnit> pUnit;
	CPtr<IPlayer> pPlayer;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pPlayer); return 0; }
	//
	CCmdRemoveUnit() {}
	CCmdRemoveUnit( IPlayer* _pPlayer, CUnit *_pUnit ): pPlayer( _pPlayer ), pUnit( _pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NWorld::SEnemyInfo (PDB 44 bytes, static_assert-verified) -- what a player currently KNOWS about a
// unit: perceived person/PK hit-points, whether that info is available + visible, the display name and
// the derived health-condition enums. Filled by CPlayer::GetEnemyUnitInfo (retail @0x386c20); read by
// MakeUnitStateToolTip to build the fog-of-war-limited hover tooltip.
struct SEnemyInfo
{
	enum ECondition
	{
		CND_HEALTHY_INTACT = 0,
		CND_LIGHTLY_WOUNDED_DAMAGED = 1,
		CND_WOUNDED_DAMAGED = 2,
		CND_HEAVILY_WOUNDED_DAMAGED = 3,
		CND_CRITICALY_WOUNDED_DAMAGED = 4,
		CND_UNCONSCIOUS_DESTROYED = 5,
	};
	int nPKHP;
	int nUnitHP;
	bool bPKInfo;
	bool bCanSeePKHP;
	bool bCanSeeUnitHP;
	wstring wsName;
	ECondition ePKCondition;
	ECondition eUnitCondition;
	bool bUnitInfo;
	int nMaxPKHP;
	int nMaxUnitHP;
	SEnemyInfo(): nPKHP(0), nUnitHP(0), bPKInfo(false), bCanSeePKHP(false), bCanSeeUnitHP(false),
		ePKCondition(CND_HEALTHY_INTACT), eUnitCondition(CND_HEALTHY_INTACT), bUnitInfo(false),
		nMaxPKHP(0), nMaxUnitHP(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPlayer: public CObjectBase
{
public:
	typedef vector< CPtr<CUnit> > CUnitSet;

	virtual CCommander *GetCommander() = 0;
	virtual const wstring& GetPlayerName() const = 0;
	virtual NRPG::CGlobalPlayer* GetGlobalPlayer() const = 0;
	virtual void GetDeploySpot( NAI::SPathPlace *pRes ) = 0;
	////
	// retail IPlayer vtable slot 12 (+0x30, PDB-confirmed; NUI::CSlot::GetDragItem @0x1beb80 and
	// CExecMoveInventoryItem::GetActionType @0x3a7990 both dispatch it there). The parameter is the
	// FULL NWorld::SItem: retail has no IPlayer::SItemInfo at all (zero UDT hits in Game.pdb), and
	// the in-hand item's eType/nSlot/sPosition ARE load-bearing -- they record the item's drag ORIGIN,
	// which GetActionType @0x3a7990 compares against the drop destination.
	virtual bool GetInHandItem( SItem *pInfo ) const = 0;
	virtual void GetStoreItems( list<CPtr<NRPG::IInventoryItem> > *pItems ) = 0;
	virtual bool TakeStoreItem( NRPG::IInventoryItem *pItem ) = 0;
	virtual void PlaceStoreItem( NRPG::IInventoryItem *pItem ) = 0;
	////
	virtual void GetUnits( CUnitSet *pRes ) const = 0;
	// retail CPlayer::GetEnemyUnitInfo @0x386c20: fill *out with what THIS player may learn about
	// pEnemy (fog-of-war HP visibility via perks/own-roster + name + health condition). Default no-op
	// so non-CPlayer IPlayer implementations need not override.
	virtual void GetEnemyUnitInfo( CObjectBase *pEnemy, SEnemyInfo *out ) const {}
	virtual void GetVisible( list< CPtr<CUnit> > *pRes ) const = 0;
	virtual void GetVisibleObjects( list< CPtr<CObjectBase> > *pRes ) const = 0;
	virtual void GetTrappedObjectsList( list< CPtr<CObjectBase> > *pRes ) const = 0;
	virtual void GetSounds( vector<IVisObj*> *pRes ) = 0;
	bool IsControlling( CUnit *pUnit )
	{
		CUnitSet u;
		GetUnits( &u );
		return find( u.begin(), u.end(), pUnit ) != u.end();
	}
	virtual int GetScenarioPlayerID() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthQuakeEvent (retail wInterface.obj, 0x1c bytes, saveload id 0xB3625120): a grenade blast's
// camera-shake request. Producers: both CWorld::AddGrenadeExplosion overloads (gated on the
// game_eq_on_grenades console var); drain: the mission's per-step ProcessCameraCommands loop ->
// active camera AddEarthQuake(vPosition, fPower). fPower = the grenade record's fDecalRadius.
class CEarthQuakeEvent: public CObjectBase
{
	OBJECT_BASIC_METHODS( CEarthQuakeEvent );
public:
	ZDATA
	float fPower;      // retail +0x0c (operator& @0x35ef90 chunk 2)
	CVec3 vPosition;   // retail +0x10 (chunk 3)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fPower); f.Add(3,&vPosition); return 0; }
	//
	CEarthQuakeEvent(): fPower( 0 ), vPosition( VNULL3 ) {}
	CEarthQuakeEvent( float _fPower, const CVec3 &_vPosition ): fPower( _fPower ), vPosition( _vPosition ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IExplosionMaster (retail wExplTracker.obj; W4 serialization-convergence) -- the engine-wide voxel
// explosion scheduler interface. CWorld owns one (retail CWorld+0x1b4, save tag 0x32) and drives it
// once per Segment; both CWorld::AddGrenadeExplosion overloads hand the blast to it in retail.
// Concrete class: NWorld::CExplosionMaster (wExplTracker.h, factory NWorld::CreateExplosionMaster
// @0x355b40 = new CExplosionMaster(pWorld)).
class IExplosionMaster: public CObjectBase
{
public:
	// retail IExplosionMaster vtbl+0x10 (CExplosionMaster::AddExplosion @0x357620): engineer-grenade blast
	virtual void AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGEngGrenade *pEngGrenade,
		CUnitServer *pThrower, const SPerkMineModifiers &modifiers, int nEngSkill ) = 0;
	// retail IExplosionMaster vtbl+0x14 (CExplosionMaster::AddExplosion @0x357520): ordinary-grenade blast
	virtual void AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGGrenade *pGrenade,
		CUnitServer *pThrower, const SPerkMineModifiers &modifiers ) = 0;
	// retail IExplosionMaster vtbl+0x18 (CExplosionMaster::Segment @0x3571d0): the per-segment driver
	virtual void Segment() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmd;
class CPostWorldCreateInfo;
class CDebrisController;
class IWorld: public CObjectBase
{
public:
	virtual CSyncSrc<IVisObj>* GetActive() const = 0;
	virtual CSyncSrc<IVisObj>* GetUnits() const = 0;
	virtual CUICmd* GetUICommand() { return 0; }
	virtual CHitLocator* GetHitEvent() = 0;
	// retail IWorld vtbl+0x10 @0x363ff0: pop-front (ownership transfer) of the earthquake queue
	virtual CEarthQuakeEvent* GetEarthQuakeEvent() { return 0; }
	//
	virtual CCTime* GetAimTime() const = 0;
	virtual NRPG::IGame* GetGame() = 0;
	virtual NAI::IAIMap* GetAIMap() = 0;
	virtual NAI::IPathNetwork* GetPathNetwork() = 0;
	virtual CFuncBase<STerrainInfo>* GetTerrainInfo() const = 0;
	// retail IWorld vtbl+0x90 = CWorld::GetHeightLayers @0x376f00 -- the per-floor height cache the
	// camera samples (CCamera::Update @0x4cdd4a). GetTerrainInfo is the raw GROUND heightmap; this is
	// the building-aware field, so the two are NOT interchangeable.
	virtual IHeightLayers* GetHeightLayers() = 0;
	virtual NDb::CAmbientLightReal* GetDefaultLight() = 0;
	// retail IWorld vtbl+0xd8 = CWorld::GetPocket @0x376f60 -- the strategic between-maps pocket
	// (units + objects). Every retail consumer reaches the pocket through this and calls CPocket
	// directly: luaUnitPlaceInPocket @0x2f9500, luaObjectPlaceInPocket @0x2e9000,
	// CUnitStateInPocket::OnStateStarted/OnStateFinished @0x3c9fb0/@0x3ca060.
	virtual CPocket* GetPocket() = 0;
	// retail NWorld::IWorld::EWeather (PDB) + IWorld vtbl+0x1c0 = CWorld::GetWeather @0x376e00,
	// returning the CWeatherTracker state (rolled by CWorld::RollNewWeather @0x3631c0). Consumed
	// by CRenderGame::SyncWeather @0x2cb9b0 (1500ms sun<->rain light cross-fade + precipitation).
	// The dev world predates the weather tracker, so the interface defaults to sunny until that
	// subsystem is ported.
	enum EWeather
	{
		WEATHER_SUNNY = 0,
		WEATHER_SNOW  = 1,
		WEATHER_RAIN  = 2,
	};
	virtual EWeather GetWeather() const { return WEATHER_SUNNY; }
	virtual void GetInterrupts( vector< CPtr<IPlayer> > *pInterrups ) const = 0;
	virtual CGlobalAck *GetGlobalAck() const = 0;
	//
	virtual const CTRect<float>& GetMapSafeZone() const = 0;
	//
	virtual void CreateRandom( int nTemplateID, const vector<string> &params, bool bBuildingStability, 
		const list< CPtr<NScenario::CScenarioClue> > &clues, int nMobsLevel,
		CObj<CPostWorldCreateInfo> *pPostInfo, SRandomSeed sSeed, bool bLeanAndMean = false ) = 0;
	virtual void RunPostInit( CPostWorldCreateInfo *pPostInfo ) = 0;
	virtual void CreateDefault() = 0;
	// retail @0x36e100 takes the LIVE session's CGlobalGame: the zone-reenter save carries only a
	// dangling weak ref to it, so the loaded world must be re-bound (and every building's parts
	// refreshed) before StartGame. ZONE-REENTER ONLY: StartGame restarts the turn.
	virtual void CreateRestored( NRPG::CGlobalGame *pGlobalGame ) = 0;
	// dev-only save-load reconnect (game.sav/restart.sav resume): CreateRestored MINUS StartGame --
	// retail's load path runs NO world restore hook (CICLoad::Exec @0x1f5fd0 post-deserialize call is
	// CMission::OnLoad @0x1fb8e0 = loading-bar counters only; CICLoadFile::Exec @0x1f6830 runs nothing),
	// so loading must NOT touch TBS state: a realtime save stores bTurnDone=0 for every live player and
	// StartGame's IsRealTimePossible (@0x364f10) gate would falsely restart a player turn.
	virtual void RestoreRuntimeCaches( NRPG::CGlobalGame *pGlobalGame ) {}
	virtual IPlayer* AddPlayer( const wstring &wsName, NRPG::CGlobalPlayer *pGlobalPlayer, 
		CCommander *pCommander, bool bAddOnManyDeploySpots = false ) = 0;
	virtual void RemovePlayer( IPlayer *pPlayer ) = 0;
	virtual IPlayer* GetCurrentPlayer() const = 0;
	virtual bool IsUnitActive( CUnit *pTest ) const = 0;
	virtual void GetActiveUnits( IPlayer *pPlayer, list<CUnit*> *pRes ) = 0;
	virtual bool IsFirstTurn() const = 0;
	virtual bool IsInterrupt() const = 0;
	virtual void ClickOfDeath( const CRay &ray, int nMaxFloor ) = 0;
	virtual CUnit* GetUnit( const NAI::SUnitPosition &pos ) = 0;
	virtual CUnit* GetUnitInTile( const NAI::SUnitPosition &pos ) = 0;
	virtual void FindCloseGroundItems( CUnit *pUnit, vector<SItem> *pRes ) = 0;
	virtual bool IsWinnerPlayer( IPlayer *pPlayer ) = 0;
	virtual int  GetEnemyWatchers( IPlayer *pPlayer ) const = 0;
	//
	virtual bool IsExecuting() const = 0;
	virtual bool CanSeeAction( IPlayer *pPlayer ) = 0;
	virtual void UpdateWorld( STime tScene, IPlayer *pPlayer ) = 0;
	virtual IPlayer* GetNextPlayerForScript( IPlayer *pPrev ) = 0;// pPrev = 0 returns first; pPrev = last player returns 0.
	virtual void MakeExplosion( const CRay &ray, int nMaxFloor ) = 0;
	virtual void GetAllUnits( vector< CPtr<NWorld::CUnit> > *pUnits ) = 0;

	virtual NDb::EDiplomacyState GetDiplomacyState( CUnit *pUnit, IPlayer *pPlayer ) const = 0;
	virtual NDb::EDiplomacyState GetDiplomacyState( IPlayer *pPlayer1, IPlayer *pPlayer2 ) const = 0;
	// retail vtbl slot (release +0x1f8): a corpse has settled. Added at the end of the dev IWorld (no
	// existing slot shifts; the dispatcher calls it by name, not raw index). Default no-op here; the
	// faithful CWorld impl @0x362180 (hero-corpse gate -> fire the stashed pGameOverCall) lives in wMain.cpp.
	virtual void InformCorpseStop( CUnitServer *pUS ) {}
	// retail CWorld::IsSequence @0x376ff0 (world vtbl+0x1a8): true while a scripted-interrupt sequence runs.
	// Default no-op here (same tail-append rationale as InformCorpseStop); CWorld delegates to its existing
	// CTBSWorld predicate. Consumed by CDumbUnitServer::Segment's track-sequence ORIGINAL-BUG branch.
	virtual bool IsSequence() const { return false; }
	// Zone re-entry predicates + prep (tail-appended; retail slots differ). CMission::Terminate
	// @0x1fbc30 saves the zone world for re-entry when IsBase() (retail vtbl+0x1dc @0x376e20:
	// the current zone IS the scenario "base") or IsLinkedZone() (vtbl+0x1e0 @0x361c80: the
	// current zone has MULTIPLE templates) or the script enabled the "reenter" feature; and it
	// first drops carried corpses (vtbl+0xc4 @0x365900) so they leave with their carrier.
	virtual bool IsBase() const { return false; }
	virtual bool IsLinkedZone() const { return false; }
	virtual void RemoveCarriedCorpses() {}
	// retail CWorld::IsAttackAllowed @0x376da0 (IWorld vtbl+0xc8): false when the zone template
	// variant carried NoAttack=1 (base zones, e.g. Gbase4 5376) -- CreateActionExecutor refuses every
	// attack executor (shoot/melee/grenade/mine -> UCR_GENERAL_FAILURE) while it is false. Default
	// true matches the retail CWorld ctor seed (@0x36a4b0 sets bAttackAllowed = true).
	// (Tail-appended like the predicates above; retail slot order differs, dispatch is by name.)
	virtual bool IsAttackAllowed() const { return true; }
	// retail CWorld::GetAllSoundStuff @0x3770c0 (IWorld vtbl+0xac): weak refs to the LIVE
	// heard-not-seen noise markers (CDMesh). CRenderGame::UpdateVisible @0x2cee50 subtracts this set
	// from GetUnits() in its no-viewer (show-all / cinematic) branch so silhouettes never draw when
	// every real unit is already shown. Default: leave the (caller-supplied, empty) vector untouched
	// -- IVisObj is incomplete here, so the body must not instantiate CPtr<IVisObj> destruction.
	virtual void GetAllSoundStuff( vector< CPtr<IVisObj> > *pRes ) {}
	// retail IWorld vtbl+0xe4 (body @0x376de0: the CDebrisController base at CWorld+0x28) -- the
	// debris manager the stability trackers hand unsupported frozen items to
	// (CStabilityTracker::OnChange @0xa59a0 debris branch). Tail-appended, dispatch by name.
	virtual CDebrisController* GetDebris() { return 0; }
	// retail IWorld slot 59 (CWorld::GetGlobalGame @0x376f40); CalcStructDmg @0x28f960 reaches
	// pDifficulty through it. Tail-appended, dispatch by name.
	virtual NRPG::CGlobalGame* GetGlobalGame() const { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorldSyncSrc: public CSyncSrc<IVisObj>
{
	OBJECT_BASIC_METHODS(CWorldSyncSrc);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IWorld* CreateWorld( NRPG::CGlobalGame *_pGlobalGame );
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif