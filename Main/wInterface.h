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
	enum EDiplomacyState;
}
namespace NWorld
{
	class CUnit;
	class CPlayer;
	class CGlobalAck;
	enum ETBSEvent;
	class CUnitServer;
	enum EUnitCommandResult;
	class CCmd;
	struct SItem;
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
// not implemented yet
class CCmdQuitGame: public CCommand
{
	OBJECT_BASIC_METHODS(CCmdQuitGame);
};
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
	virtual void OnSeeUnit( CUnitServer *pWatcher, CUnitServer *pTarget ) {}
	virtual void Segment() {}
	virtual void ProcessAISignals() {}
	virtual void OnUnitAdded( CUnitServer *pUnit ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IItem : virtual public CObjectBase
{
public:
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
	virtual bool IsHiding() const = 0;
	virtual bool IsEmptyPK() const = 0;
	virtual bool IsStrafing() const = 0;
	virtual bool IsCarryingCorpse() const = 0;
	virtual bool IsPerformingAction() const { return false; }
	virtual CUnit* GetCorpseCarrier() const = 0;
	virtual NDb::CModel* GetModel() const = 0;
	virtual void GetVisible( vector<CPtr<CUnit> > *pTarget ) const = 0;
	virtual void GetInfo( NRPG::SUnitInfo *pInfo ) const = 0;
	virtual IPlayer* GetPlayer() const = 0;
	virtual NAI::CPath* GetCurrentPath() = 0;
	virtual IPathViewer* CreatePathViewer() = 0;
	virtual NRPG::IUnitMissionInfo* GetRPG() const = 0;
	virtual const NAI::SUnitPosition& GetPosition() const = 0;
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
	CVec3 vPosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nHitValue); f.Add(3,&vPosition); return 0; }

	CHitLocator(): nHitValue( 0 ), vPosition( 0, 0, 0 ) {}
	CHitLocator( int _nHitValue, const CVec3 &_vPosition ): nHitValue( _nHitValue ), vPosition( _vPosition ) {}
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
class IPlayer: public CObjectBase
{
public:
	typedef vector< CPtr<CUnit> > CUnitSet;
	struct SItemInfo
	{
		ZDATA
		CPtr<CUnit> pUnit;
		CPtr<NRPG::IInventoryItem> pItem;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pItem); return 0; }
	};

	virtual CCommander *GetCommander() = 0;	
	virtual const wstring& GetPlayerName() const = 0;
	virtual NRPG::CGlobalPlayer* GetGlobalPlayer() const = 0;
	virtual void GetDeploySpot( NAI::SPathPlace *pRes ) = 0;
	////
	virtual bool GetInHandItem( SItemInfo *pInfo ) const = 0;
	virtual void GetStoreItems( list<CPtr<NRPG::IInventoryItem> > *pItems ) = 0;
	virtual bool TakeStoreItem( NRPG::IInventoryItem *pItem ) = 0;
	virtual void PlaceStoreItem( NRPG::IInventoryItem *pItem ) = 0;
	////
	virtual void GetUnits( CUnitSet *pRes ) const = 0;
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
class CUICmd;
class CPostWorldCreateInfo;
class IWorld: public CObjectBase
{
public:
	virtual CSyncSrc<IVisObj>* GetActive() const = 0;
	virtual CSyncSrc<IVisObj>* GetUnits() const = 0;
	virtual CUICmd* GetUICommand() { return 0; }
	virtual CHitLocator* GetHitEvent() = 0;
	//
	virtual CCTime* GetAimTime() const = 0;
	virtual NRPG::IGame* GetGame() = 0;
	virtual NAI::IAIMap* GetAIMap() = 0;
	virtual NAI::IPathNetwork* GetPathNetwork() = 0;
	virtual CFuncBase<STerrainInfo>* GetTerrainInfo() const = 0;
	virtual NDb::CAmbientLightReal* GetDefaultLight() = 0;
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
	virtual void CreateRestored() = 0;
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
	// existing slot shifts; the dispatcher calls it by name, not raw index). Default no-op here -- the
	// faithful CWorld impl (corpse-cap game-over arming via pGameOverCall) is a separate wMain leg,
	// deferred with the wCheckTooMuchCorpses arming path.
	virtual void InformCorpseStop( CUnitServer *pUS ) {}
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