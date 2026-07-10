#ifndef __WMAIN_H_
#define __WMAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wInterface.h"
#include "wTurnBased.h"
#include "wDebris.h"
#include "TerrainInfo.h"
#include "aiPosition.h"
#include "..\Misc\EventsBase.h"
#include "wVision.h"
#include "eventPlayer.h"
#include "wUnitCommands.h"   // complete NWorld::SItem for CPlayer::sHandItem (release save-format, by-value)
#include "wPocket.h"         // CPocket::SSmthPtrHolder -- the release object pocket (luaObjectPlaceInPocket family)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapUnit;
struct SMapInfo;
struct SMapElement;
struct SClueSlot;
class CMapWaypoint;

namespace NAI
{
	class IAISignalManager;
	class IAIJobManager;
	class CAIRouteWaypoint;
	class CAICommander;
}
namespace NGScene
{
	class CBuildInfo;
}
namespace NRPG
{
	class CGlobalPlayer;
	class IAttackable;
	class IGame;
	class IObject;
	class CCoverInfo;
	class CAttackPortion;
	class IUnitMission;
	class IClipItem;
	enum EAction;
	class CUnit;
	class CGlobalDiplomacy;
}
namespace NDb
{
	class CObject;
	class CDebrisMaterial;
	class CRPGGrenade;
	class CRPGMeleeWeapon;
	class CAISound;
	class CDBAckSequence;
	class CScript;
	class CDBCamera;
	class CRPGArmor;
	enum EDiplomacyState;
}
namespace NScript
{
	class CScript;
}
namespace NScenario
{
	class CScenarioClue;
}
struct STerrainInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
class CPlayer;
class CWorld;
class CUnitServer;
class CDumbUnitServer;
class CObjectServerBase;
class CObjectServer;
class CCannon;
struct SObjectPlace;
class CBuilding;
class IDynamicObject;
class CTimedObject;
class CGlobalAck;
class CTerrain;
struct SInterfaceAck;
class IPassageObject;
class CCameraTracker;
enum EInterfaceEvent;
enum ESkipMode;
class CUnitGroup;
class IMine;
class CMineTracker;
struct SPerkMineModifiers;   // explosive-perk damage modifiers (wExplosionPerks.h); AddGrenadeExplosion takes them by ptr
enum EInterfaceEventType;
//
//class CEventOnNewPlayerFastTurnOrTime;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayer: public IPlayer, public CPlayerBase<CUnitServer,CCommander>, public CPlayerBaseVision<CUnitServer>
{
	OBJECT_BASIC_METHODS(CPlayer);
	typedef CPlayerBase<CUnitServer,CCommander> TPlayerBase;
	typedef CPlayerBaseVision<CUnitServer> TPlayerBaseVision;
	ZDATA_(TPlayerBase)
	ZPARENT(TPlayerBaseVision)
	wstring wsName;
	int nScenarioPlayerID;
	NAI::SPathPlace deploySpot;
	CPtr<NRPG::CGlobalGame> pGlobalGame;
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	CObj<NRPG::IInventoryItem> pInHandItem; //// item w/o owner (release dropped from serialize; kept as member)
	SItem sHandItem;   // release save-format tag 9 (replaces pInHandItem@8; by-value SItem)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TPlayerBase*)this); f.Add(2,(TPlayerBaseVision*)this); f.Add(3,&wsName); f.Add(4,&nScenarioPlayerID); f.Add(5,&deploySpot); f.Add(6,&pGlobalGame); f.Add(7,&pGlobalPlayer); f.Add(9,&sHandItem); return 0; }
public:
	CPlayer() {}
	CPlayer( const wstring &_wsName, NRPG::CGlobalGame *_pGlobalGame, NRPG::CGlobalPlayer *_pGlobalPlayer, int _nScenarioPlayerID );

	virtual CCommander* GetCommander() { return TPlayerBase::GetCommander(); }	
	virtual const wstring& GetPlayerName() const { return wsName; }
	virtual NRPG::CGlobalPlayer* GetGlobalPlayer() const { return pGlobalPlayer; }
	virtual void GetDeploySpot( NAI::SPathPlace *pRes ) { *pRes = deploySpot; }
	void SetDeploySpot( const NAI::SPathPlace &p ) { deploySpot = p; }
	////
	bool GetInHandItem( SItemInfo *pInfo ) const;
	void SetInHandItem( const SItemInfo &sInfo );
	////
	void GetStoreItems( list<CPtr<NRPG::IInventoryItem> > *pItems );
	bool TakeStoreItem( NRPG::IInventoryItem *pItem );
	void PlaceStoreItem( NRPG::IInventoryItem *pItem );
	////
	virtual void GetUnits( vector<CPtr<CUnitServer> > *pRes ) const;
	virtual void GetUnits( CUnitSet *pRes ) const;
	virtual void GetEnemyUnitInfo( CObjectBase *pEnemy, SEnemyInfo *out ) const;   // retail @0x386c20
	virtual void GetUnitsThatCanFight( list<CPtr<CUnitServer> > *pRes ) const;
	virtual void GetVisible( list<CPtr<CUnit> > *pRes ) const;
	virtual void GetVisibleObjects( list<CPtr<CObjectBase> > *pRes ) const;
	virtual void GetTrappedObjectsList( list<CPtr<CObjectBase> > *pRes ) const ;
	virtual void GetSounds( vector<IVisObj*> *pRes );
	void GetUnitsRPGs( vector< CPtr<NRPG::IUnitMission> > *pRes ) const;
	////
	virtual int GetScenarioPlayerID() const { return nScenarioPlayerID; }
	virtual void SetCheat( int nCheat, bool bOn );
	bool HasLostFromSightAliveUnits();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInterruptInfo
{
	struct SNotice
	{
		CUnitServer *pWho, *pWhom;
		bool bIsMutual;
		bool bWasShot;
		float fStrength;
		
		SNotice() {}
		SNotice( CUnitServer *_pWho, CUnitServer *_pWhom, bool _bWasShot ): pWho(_pWho), 
			pWhom(_pWhom), bIsMutual(false), bWasShot(_bWasShot) {}
	};
	list<SNotice> events;

	void AddEvent( CUnitServer *pWho, CUnitServer *pWhom, bool bWasShot = false )
	{ 
		events.push_back( SNotice( pWho, pWhom, bWasShot ) ); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindowDoor;
struct SDoorTrap
{
	CPtr<CWindowDoor> pDoor;
	CDBPtr<NDb::CRPGGrenade> pGrenade;
	int nDC;
	SDoorTrap( CWindowDoor *pD, NDb::CRPGGrenade *pGr, int _nDC ): pDoor(pD), pGrenade(pGr), nDC(_nDC) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPostWorldCreateInfo : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CPostWorldCreateInfo);
public:
	list<CDBPtr<NDb::CScript> > scripts;
	list<SDoorTrap> traps;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// World time-of-day, carried as a single sentinel value in CWorld::createFlags (or none = ANYTIME).
// The sentinel values (5/6) are the createFlags codes the map-load / door-sound-variant path uses.
enum ETimeOfDay
{
	TOD_ANYTIME = 0,
	TOD_NIGHT   = 5,
	TOD_DAY     = 6,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorld: public IWorld, public CTBSWorld<CUnitServer, CPlayer, CCommander>, public CDebrisController
{
public:
	struct SWorldDeploySpot
	{
		NAI::SPathPlace p;
		int nID, nPlayer;
		SWorldDeploySpot() {}
		SWorldDeploySpot( const NAI::SPathPlace &_p, int _nID, int _nPlayer ) : p(_p), nID(_nID), nPlayer(_nPlayer) {}
	};
	struct SUnitPtrHolder
	{
		ZDATA
		CObj<CUnitServer> pCObjHolder;
		CMObj<CUnitServer> pCMObjHolder;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCObjHolder); f.Add(3,&pCMObjHolder); return 0; }
		SUnitPtrHolder() {}
		SUnitPtrHolder( CUnitServer *_pUS ): pCObjHolder( _pUS ), pCMObjHolder( _pUS ) {}
	};
	typedef CTBSWorld<CUnitServer, CPlayer, CCommander> TTBSWorld;
	typedef unordered_map< CPtr<NScenario::CScenarioClue>, SClueSlot, SPtrHash > ClueToSlot;
	//
	NGlobal::CEventRegister< CWorld, NWorld::CEventOnNewPlayerFastTurnOrTime > registerOnNewPlayerFastTurnOrTime;
	//
	OBJECT_BASIC_METHODS(CWorld);
	ZDATA_(TTBSWorld)
	CObj<CWorldSyncSrc> pShow, pShowUnits;
	list< CPtr<CUICmd> > uiCmdsList;
	list< CPtr<CHitLocator> > eventHits;
	CObj< NWorld::CTerrain > pTerrain;
	CObj<CCTime> pTime, pAimTime;
	STime tPrev, tHiddenDelta;
	CObj<NAI::IAIMap> pAIMap;
	CObj<NAI::IPathNetwork> pPathNetwork;
	CObj<NRPG::IGame> pRPGGame;
	CPtr<NDb::CAmbientLightReal> pDefaultLight;
	list< CObj<CUnitServer> > units;
	list< CObj<CObjectServerBase> > objects;
	list< CPtr<CObjectServerBase> > segmentObjects;
	list< CObj<IDynamicObject> > miscObjects;
	list< CObj<CBuilding> > buildings;
	CObj< CGlobalAck > pGlobalAck;
	CObj<CTerrainInfoHolder> pTerrainInfo;
	
	vector<SWorldDeploySpot> deploySpots;
	
	bool bLeanAndMean;
	int nRootLayersGroup, nPartiesAdded;
	CTRect<float> sMapSafeZone;
	ZPARENT(CDebrisController)
	CObj<NAI::IAIJobManager> pAIJobManager;
	CObj<NScript::CScript> pOwnScript;
	CObj<NAI::IAISignalManager> pAISignalManager;
	int nAIUnitsCreated;
	CPtr<NRPG::CGlobalGame> pGlobalGame;
	CObj<CPlayer> pDeployedDeadUnitsPlayer;
	unordered_map< string, CObj<NAI::CAIRouteWaypoint> > waypoints;
	vector< CObj<CUnitGroup> > unitGroups;
	vector<int> createFlags;
	// (bForcedRealTime REMOVED -- retail has no such field; a sequence is the ownerless SInterrupt on
	// the TBS stack, StartSequence @0x375dd0 / EndOfTurn @0x3776f0. Save tag 37 retired with it: old
	// saves' chunk 37 is simply never requested by operator&.)
	// BUG 2 (realtime reaction delay): retail CWorld willWantTBS -- a per-AI-player deferred "want turn-based"
	// request armed on a one-sided real-time sighting. CWorld::WillWantTBS @0x3683e0 pushes {player,
	// nTimeLeft=0x32}; CWorld::Segment @0x36bce0 counts each down and fires WantTurnBased at 0, so the AI
	// reacts after a short delay instead of seizing turn-based the instant it spots you. Transient (like
	// bIsBase below): NOT in operator& -- a mid-countdown save just re-arms on the next spot.
	struct SWillWantTBS { CPtr<CPlayer> pPlayer; int nTimeLeft; };
	vector<SWillWantTBS> willWantTBS;
	bool bScriptWantTurnBased = false;  // retail @CWorld+0x1c0 -- script's turn-based wish (saved state; ScriptWantTurnBased)
	bool bIsBase = false;               // retail @CWorld+0x1b9 -- current zone IS the scenario "base"; computed in
	                                    // StartGame @0x36bb50, NOT serialized (retail omits it from operator&)
	// retail CWorld @+0x1ba/+0x1bb -- transient per-segment vision-refresh COALESCING state (NOT serialized,
	// reset every Segment). During a Segment, UpdateVisible(bForce=false) requests are DEFERRED into
	// bCallUpdateVisible and flushed once at the Segment tail (@0x36bce0), so the per-unit FilterSounds sweep
	// runs AFTER the segment's SetPosition/MakeAISound marker creation -- which is what erases the heard-not-seen
	// CDMesh silhouette markers during a c_BeginSequence cutscene (their makers carry CHEAT_SCRIPTSEQUENCE).
	bool bDelayUpdateVisibleCalc = false;   // +0x1ba
	bool bCallUpdateVisible = false;        // +0x1bb
	bool bFreezeStart = false;          // retail @CWorld+0x1b8 -- a script froze the game start (c_DelayGameStartEx);
	                                    // StartFirstSegments keeps segmenting while it's set (DelayGameStart sets it)
	bool bFirstSegment = true;          // retail @CWorld -- one-shot latch: fire global lua OnEnterZone() on the
	                                    // first Segment of a freshly (re)started scenario; re-armed in RunPostInit
	bool bAttackAllowed = true;         // retail @CWorld+421, save chunk 0x2e -- ctor seeds true (@0x36a4b0);
	                                    // CreateRandom @0x36d0b0 sets !mapInfo.bNoAttack (variant NoAttack=1 in
	                                    // base zones); read ONLY via IsAttackAllowed @0x376da0 (IWorld vtbl+0xc8)
	int nTurnID;
	STime prevTurnTime;
	CObj< NRPG::CGlobalDiplomacy > pDiplomacy;
	list<CPtr<IMine> > trappedObjects;
	CObj<CMineTracker> pMineTracker;
	STime prevFastTurnTime;
	vector<SUnitPtrHolder> pocket;
	// retail CPocket's OBJECT side (wPocket.h; consumed by luaObjectPlaceInPocket @0x2e9000 /
	// luaObjectRestoreFromPocket @0x2e9130): plain world objects the script pockets are held alive here
	// (non-master O-ref only) while removed from the world lists. Tag 49 (appended; old saves lack it).
	vector< CPocket::SSmthPtrHolder<CObjectServerBase> > objectPocket;
	unordered_map< string, CPtr<CObjectBase> > nameToObj;
	vector<CPtr<IVisObj> > allSoundStuff;   // @CWorld+0x1a8: weak refs to the live heard-not-seen markers (CDMesh), tag 48
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TTBSWorld*)this); f.Add(2,&pShow); f.Add(3,&pShowUnits); f.Add(4,&uiCmdsList); f.Add(5,&eventHits); f.Add(6,&pTerrain); f.Add(7,&pTime); f.Add(8,&pAimTime); f.Add(9,&tPrev); f.Add(10,&tHiddenDelta); f.Add(11,&pAIMap); f.Add(12,&pPathNetwork); f.Add(13,&pRPGGame); f.Add(14,&pDefaultLight); f.Add(15,&units); f.Add(16,&objects); f.Add(17,&segmentObjects); f.Add(18,&miscObjects); f.Add(19,&buildings); f.Add(20,&pGlobalAck); f.Add(21,&pTerrainInfo); f.Add(22,&deploySpots); f.Add(23,&bLeanAndMean); f.Add(24,&nRootLayersGroup); f.Add(25,&nPartiesAdded); f.Add(26,&sMapSafeZone); f.Add(27,(CDebrisController*)this); f.Add(28,&pAIJobManager); f.Add(29,&pOwnScript); f.Add(30,&pAISignalManager); f.Add(31,&nAIUnitsCreated); f.Add(32,&pGlobalGame); f.Add(33,&pDeployedDeadUnitsPlayer); f.Add(34,&waypoints); f.Add(35,&unitGroups); f.Add(36,&createFlags); f.Add(38,&nTurnID); f.Add(39,&prevTurnTime); f.Add(40,&pDiplomacy); f.Add(41,&trappedObjects); f.Add(42,&pMineTracker); f.Add(43,&prevFastTurnTime); f.Add(44,&pocket); f.Add(45,&nameToObj); f.Add(46,&bScriptWantTurnBased); f.Add(47,&bFreezeStart); f.Add(48,&allSoundStuff); f.Add(49,&objectPocket); f.Add(50,&bAttackAllowed); return 0; }
	
	CObjectServerBase* AddObject( const SObjectPlace &pos, 
		NRPG::IObject *pRPGObject, const SMapElement &mapElement, CPostWorldCreateInfo *pPostInfo = 0 );
	void AddBuilding( const SMapBuilding &info );
	void CreateFakeTerrainInfo( STerrainInfo *pTerrain );
	void CheckStability();
	CUnitServer* GetUnit( CUnit *pUnit ) const;
	void PlaceAllUnits();
	void StartGame();
	void StartFirstSegments();           // retail @0x36c4c0 -- the freeze-driven initial-segment warm-up loop (internal)

	void Segment();
	virtual void OnNewPlayerTurn( CPlayer *pPlayer );
	virtual void OnRealTimeStarted();
	// TBSWorld<>	
private:
	void CheckSpot( const vector< CPtr<CPlayer> > &players );
	virtual bool IsTBSRealTimeModePossible() const;
	virtual void OnAction( bool bStartAction );
	bool IsPersonSlotUsed( int nUnitID, const ClueToSlot &clueToSlot, int *pPersID, string *pClueName );
	void DistributeClues( const SMapInfo &mapInfo,
		const list< CPtr<NScenario::CScenarioClue> > &clues,
		ClueToSlot *personClueToSlot,	ClueToSlot *itemClueToSlot );
	void PlaceItemSlotsToMap( const ClueToSlot &clueToSlot );
	void PlaceItemSlotsToInventory( const ClueToSlot &clueToSlot );

	// CDebrisController
	virtual CActionCounter* CreateActionCounter() { return GetActiveCounter(); }
	virtual CSyncSrc<IVisObj>* GetShowList() { return pShow; }
	virtual CSyncSrc<IVisObj>* GetVisibleShowList() { return pShowUnits; }
	virtual void OnFrozenItemDestroyed( int nItemID );
	void GetPassageObjects( int nPassageZoneID, list< CPtr<IPassageObject> > *pPassageObjects );
	bool GetPassageDeployPlace( IPassageObject *pPassage, 
		CUnitServer *pUS, const vector<NAI::SPathPlace> &lockedPlaces, NAI::SPathPlace *pPathPlace );
	void InitPlayerCorpseCarrying( CPlayer *pPlayer );
	// retail @0x369b30: pOwner is who receives a freshly created body (enemy corpse -> the shared
	// dead-units player, ally body -> the carrier's player); the corpse state tail runs on both paths
	CUnitServer *GetDeployedDeadUnit( const NAI::SPathPlace &aiPos, NRPG::CUnit *pRPGUnit, CPlayer *pOwner );
	void LoadWaypoints( const list< CObj<CMapWaypoint> > &_waypoints );
	virtual STime GetWorldTime() { return GetTime()->GetValue(); }
	void RunAutoLoadScripts();
	CUnitServer* AddUnit( const NAI::SPathPlace &aiPos, NRPG::IUnitMission *_pRPG, CPlayer *pPlayer, const string &szName = "" );
	void AddAIPlayer( const wstring &wsName, int nScenarioPlayerID );
	void CreateAIUnits( const SMapInfo &mapInfo, const ClueToSlot &personClueToSlot, 
		int nMobsLevel, unordered_map< int, CPtr<CUnitServer> > *pIDToUnit, CVec3 ptDeltaPos = VNULL3 );
	virtual void OnNewTurn();
	void CreateUnitGroups( const SMapInfo &mapInfo, 
		unordered_map< int, CPtr<CUnitServer> > *pIDToUnit );
	void CreateObjects( const SMapInfo &mapInfo, CPostWorldCreateInfo *pPostInfo, CVec3 ptDeltaPos = VNULL3, bool bCreateBorder = true );
	void UpdateAICommander( NAI::CAICommander *pAICommander );
	void CheckForAcks();
	void RemoveUnitFromAI( CUnitServer *pUS );
	void CheckRealTimeTurn();

public:
	CWorld();
	CWorld( NRPG::CGlobalGame *_pGlobalGame );

	virtual void ExecuteCommand( CCommand *_pCmd );
	CUnitServer* AddUnitInGame( const NAI::SPathPlace &aiPos, NRPG::IUnitMission *_pRPG, CPlayer *pPlayer, const string &szName = "" );
	CUnitServer* AddUnit( CUnitServer *pUS ); // for pocket
	void RemoveUnit( CUnitServer *pUnit );

	CObjectServerBase* AddObject( const SObjectPlace &pos, NDb::CObject *pDBObject, string szName = "" );
	void AddWaypoint( CMapWaypoint *pWaypoint );
	bool PlaceTemplate( int nTemplateID, CVec3 ptPos );
	// events
	void OnNewPlayerFastTurnOrTime( const CEventOnNewPlayerFastTurnOrTime &event );
	//
	void MergeFriendlyPlayersVisibleSets();
	virtual void UpdateVisible( bool bForce = false );
	virtual CSyncSrc<IVisObj>* GetActive() const { return pShow; }
	virtual CSyncSrc<IVisObj>* GetUnits() const { return pShowUnits; }
	virtual CUICmd* GetUICommand();
	virtual CHitLocator* GetHitEvent();
	//
	CUnitServer *GetUnitServerByPersID( int nPersID ) const;
	virtual CCTime* GetAimTime() const { return pAimTime; }
	virtual NRPG::IGame* GetGame() { return pRPGGame; }
	virtual NAI::IAIMap* GetAIMap() { return pAIMap; }
	virtual NAI::IPathNetwork* GetPathNetwork() { return pPathNetwork; }
	virtual CFuncBase<STerrainInfo>* GetTerrainInfo() const { return pTerrainInfo; }
//	virtual NTerrain::CTerrain* GetTerrain() const { return pTerrain; }
	virtual NDb::CAmbientLightReal* GetDefaultLight() { return pDefaultLight; }
	virtual void GetInterrupts( vector< CPtr<IPlayer> > *pInterrups ) const 
	{ 
		vector<CPlayer*> ints;
		TTBSWorld::GetInterrupts( &ints ); 
		pInterrups->resize( ints.size() );
		for ( size_t k = 0; k < ints.size(); ++k )
			(*pInterrups)[k] = ints[k];
	}
	virtual void CheckInterrupt( SInterruptInfo *info );
	void WillWantTBS( CPlayer *pPlayer );   // BUG 2: arm the deferred realtime->TBS switch (retail @0x3683e0)
	virtual CGlobalAck *GetGlobalAck() const { return pGlobalAck; }	
	//
	virtual const CTRect<float>& CWorld::GetMapSafeZone() const;
	//
	virtual void CreateRandom( int nVariantID, const vector<string> &params, bool bBuildingStability, 
		const list< CPtr<NScenario::CScenarioClue> > &clues, int nMobsLevel,
		CObj<CPostWorldCreateInfo> *pPostInfo, SRandomSeed sSeed, bool bLeanAndMean = false );
	virtual void RunPostInit( CPostWorldCreateInfo *pPostInfo );
	virtual void CreateDefault();
	virtual void CreateRestored( NRPG::CGlobalGame *pGlobalGame );	// retail @0x36e100
	virtual bool IsBase() const { return bIsBase; }					// retail @0x376e20
	virtual bool IsLinkedZone() const;								// retail @0x361c80
	virtual void RemoveCarriedCorpses();							// retail @0x365900
	virtual bool IsAttackAllowed() const { return bAttackAllowed; }	// retail @0x376da0 (mov al,[this+0x1a5])
	// retail @0x3770c0: plain vector copy-out of the live heard-marker weak refs
	virtual void GetAllSoundStuff( vector< CPtr<IVisObj> > *pRes ) { *pRes = allSoundStuff; }
	virtual IPlayer* AddPlayer( const wstring &wsName, NRPG::CGlobalPlayer *pGlobalPlayer, 
		CCommander *pCommander, bool bAddOnManyDeploySpots = false );
	virtual void RemovePlayer( IPlayer *pPlayer );
	virtual IPlayer* GetCurrentPlayer() const { return GetTBSCurrentPlayer(); }
	// retail throws CEventOnPassControl on EVERY control hand-over (CTBSWorld::OnPassControl @0x372bf0
	// queues STBSEvent tag9 -> ProcessTBSEvents @0x3675d0 throws) -- base turn AND stacked interrupt AND
	// interrupt-pop resume. The event drives the per-unit begin-turn threat refresh (tracker OnNewTurn
	// @0xab180 -> CAIBeginTurnEvent -> PrepareEnemies @0xb17a0 == dev Populate). Defined in wMain.cpp.
	virtual void OnPassControlNotify();
	virtual bool IsUnitActive( CUnit *pTest ) const { return IsTBSUnitActive( GetUnit(pTest) ); }
	virtual void GetActiveUnits( IPlayer *pPlayer, list<CUnit*> *pRes );
	virtual bool IsFirstTurn() const { return TTBSWorld::IsFirstTurn(); }
	virtual bool IsInterrupt() const { return TTBSWorld::IsInterrupt(); }
	// retail CWorld::IsSequence @0x376ff0 -> CTBSWorld::IsSequence @0x375a30: "non-empty interrupt stack
	// with an OWNERLESS top". luac_BeginSequence @0x2f1890 pushes that ownerless entry (StartSequence
	// @0x375dd0), luaEndSequence @0x2f1a60 pops it (EndOfTurn) -- true for exactly the
	// c_BeginSequence..EndSequence span, EVEN when the sequence starts from clean real time.
	virtual bool IsSequence() const { return TTBSWorld::IsSequence(); }
	virtual void ClickOfDeath( const CRay &ray, int nMaxFloor );
	virtual CUnit* GetUnit( const NAI::SUnitPosition &pos );
	virtual CUnit* GetUnitInTile( const NAI::SUnitPosition &pos );
	virtual int  GetEnemyWatchers( IPlayer *pPlayer ) const;
	//
	virtual bool IsExecuting() const { return IsAction(); }
	virtual bool CanSeeAction( IPlayer *pPlayer );
	virtual void UpdateWorld( STime tScene, IPlayer *pPlayer );
	void GetAllUnits( list<CPtr<CUnitServer> > *pRes );
	virtual void GetAllUnits( vector< CPtr<NWorld::CUnit> > *pUnits );
	void GetUnitsNear( const CVec3 &pos, list<CPtr<CUnitServer> > *pRes, float fRadius );
	void GetCannons( vector<CCannon*> *pRes );   // every cannon on the map (used by the AI heavy-gun actions)

	void AddMine( IMine *pMine );
	void RemoveMine( IMine *pMine );
	void GetMinesNear( const CVec3 &pos, list<CPtr<IMine> > *pRes, float fRadius );
	CMineTracker* GetMineTracker() const { return pMineTracker; }
	
	CCTime* GetTime() const { return pTime; }
	void PerformRangedAttack( const NRPG::CAttackPortion &ap, const CRay &ray, const vector<NRPG::IAttackable*> &ignores, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed );
	virtual void Explode( const CVec3 &ptEpicentre, int nPower );
	virtual void CreateParticle( const CVec3 &ptPos, const CQuat &rot, NDb::CEffect *pEffect, int nFloor = -100 );
	void AttachMiscObject( CTimedObject *p );
	void AddUICommand( CUICmd* pCmd );
	void ScriptWantTurnBased( bool bWant );   // retail @0x361c00 -- the WantTurnBased("b") script binding
	void DelayGameStart( int bDelay );        // retail IWorld vtbl+0x1d8 -- script start control (c_StartGameEx(0)/c_DelayGameStartEx(1))
	void AddNextUIHint( bool bSilent );        // retail @0x368140 IWorld vtbl+0x118 -- advance the sequential-hint cursor (lua AddHints)
	ETimeOfDay GetTimeOfDay();                // retail @0x361bb0 -- read the TOD sentinel from createFlags
	void SetTimeOfDay( ETimeOfDay tod );      // retail @0x362b90 -- rewrite the TOD sentinel + re-light every object
	void AddHitLocator( CHitLocator* pLocator );
	void ThrowGrenade( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fTFly, 
		NDb::CModel *pModel, NDb::CRPGGrenade *pRPGGrenade, CUnitServer *pUnitServer );
	void ThrowKnife( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fDistance,
		NDb::CModel *pModel, NRPG::CAttackPortion &attack, NRPG::IInventoryItem *pIItem, CUnitServer *pUnitServer );
	void LaunchRocket( const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &attack, 
		NRPG::IClipItem *pRocket, CUnitServer *pIgnored, NDb::CEffect *_pEffect = 0 );
	virtual void AddGrenadeExplosion( const CVec3 &vStartPosition,
		NDb::CRPGGrenade *pRPGGrenade, CUnitServer *pUnitServer = 0, CObjectBase *pIgnitionObject = 0,
		const SPerkMineModifiers *pMods = 0 );
	void KillObject( CObjectServerBase *pOS );
	// retail object pocket (luaObjectPlaceInPocket @0x2e9000 / luaObjectRestoreFromPocket @0x2e9130):
	// pocket = hold alive + KillObject + unbind from the vis sync; restore = re-list + rebind + unpocket.
	bool IsObjectInPocket( CObjectServerBase *pObject ) const;
	void PlaceObjectInPocket( CObjectServerBase *pObject );
	void RestoreObjectFromPocket( CObjectServerBase *pObject );
	void FindCloseGroundItems( CUnit *pU, vector<SItem> *pRes );
	bool IsWinnerPlayer( IPlayer *pPlayer );
	void GenerateDebris( NDb::CDebrisMaterial *pDebrisMaterial, const CVec3 &ptCenter, const CVec3 &ptDir, int nDebris );
	void CreateSoundStuff( CUnitServer *pWho, vector<CObj<CTimedObject> > *stuff, CVec3 ptPos );   // @0x369110
	void MakeAISound( NDb::CAISound *pAISound, CDumbUnitServer *pWho, int nSoundType = 0, NDb::CSound *pSound = 0 );
	void MakeSound( const CVec3 &ptCenter, NDb::CSound *pSound );
	list< CObj<IDynamicObject> > *GetMiscObjects() { return &miscObjects; }
	NAI::IAIJobManager *GetAIJobManager() { return pAIJobManager; }
	NAI::IAISignalManager *GetAISignalManager() { return pAISignalManager; }
	CUnitServer *GetUnitServer( NRPG::IUnitMissionInfo *pUnitMission );
	CUnitServer *GetUnitServer( NRPG::CUnit *pRPGUnit );
	CUnitServer *GetUnitServer( string szName );
	void SetAudible( CUnitServer *pHearer, CUnitServer *pSource );
	// pPrev = 0 returns first; pPrev = last player returns 0. user only in script functions which must check all players
	virtual IPlayer* GetNextPlayerForScript( IPlayer *pPrev )
	{
		return GetNextPlayer( (CPlayer*)pPrev );
	}
	virtual void MakeExplosion( const CRay &ray, int nMaxFloor );
	NRPG::CGlobalGame *GetGlobalGame() const { return pGlobalGame; }
	bool UsePassageObject( CUnitServer *pUS, int nPassageZoneID );
	void GetScenarioPlayerUnits( int nScenarioPlayer, vector< CPtr<CUnitServer> > *pUnits );
	void GetScenarioPlayerUnits( int nScenarioPlayer, vector< CPtr<NRPG::IUnitMission> > *pUnits );
	CPlayer *GetPlayerByID( int nScenarioPlayerID, int nIndex = 0 );  // retail @0x365530: the nIndex-th player with that scenario id
	NAI::CAIRouteWaypoint *GetWaypoint( string szName );
	CUnitGroup* GetUnitGroup( int nGroupID );
	CUnitGroup* CreateUnitGroup( int nGroupID = -1 );
	void RemoveUnitGroup( CUnitGroup* pUnitGroup );
	NDb::CRPGArmor* GetArmor( const CVec3 &vPos );
	void CreateBloodyMess( const CVec3 &vCenter, const CVec3 &vDirection, CObjectBase *pIgnore, int nParts );
	virtual const vector<int>& GetCreateFlags() const { return createFlags; }
	CObjectServerBase* GetObjectByName( const string &szName );
	NScript::CScript* GetOwnScript() const { return pOwnScript; }
	void ExecuteOwnScript();   // set the global active-script context (pScript=pOwnScript) THEN tick its threads, exactly as Segment does -- a context-less ExecuteThreads() crashes (GetScript()==0)
	CDFrozenItem* GetItemByName( const string &szName );
	void RegisterObjectForSegment( CObjectServerBase *p ) { segmentObjects.push_back( p ); }
	int GetTurnID() { return nTurnID; }
	NRPG::CGlobalDiplomacy* GetDiplomacy() const;
	NDb::EDiplomacyState GetDiplomacyState( CUnit *pUnit, IPlayer *pPlayer ) const;
	NDb::EDiplomacyState GetDiplomacyState( IPlayer *pPlayer1, IPlayer *pPlayer2 ) const;
	// these functions are used from script
	bool GetObjectName( CObjectServerBase *pObject, string *pName ) const;
	bool GetUnitName( CUnitServer *pUnit, string *pName ) const;
	bool GetItemName( CDFrozenItem *pUnit, string *pName ) const;
	//
	void ProcessAISignals();
	void OnUnitAdded( CUnitServer *pUnit );
	void ChangeUnitPlayer( CUnitServer *pUnit, CPlayer *pPlayer );
	void PlaceUnitInPocket( CUnitServer *pUnit );
	void RemoveUnitFromPocket( CUnitServer *pUnit );
	bool IsUnitInPocket( CUnitServer *pUnit ) const;
};
extern CWorld *pCurrentWorld;
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif