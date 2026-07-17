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
#include "wPocket.h"         // NWorld::CPocket complete -- CWorld::pPocket (save tag 42, retail @0x376f60 GetPocket)
#include "wHeightLayers.h"   // NWorld::IHeightLayers complete -- CWorld::pHeightLayers (save tag 44, @0x376f00)
#include "..\DBFormat\DataLight.h"   // NDb::CTAmbientLight complete (pDefaultLight's CDBPtr saveload uses typeid;
                                     // GetDefaultLight @0x3620a0 resolves the template through CTAmbientLight::GetLight)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapUnit;
struct SMapInfo;
struct SMapElement;
struct SClueSlot;
class CMapWaypoint;

namespace NAI
{
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
	class CRPGEngGrenade;
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
class C3DSound;
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
	// retail CPlayer is PDB size 0x88 with EXACTLY {wsName@0x48, nScenarioPlayerID@0x54, deploySpot@0x58,
	// pGlobalGame@0x5c, pGlobalPlayer@0x60, sHandItem@0x64} -- there is NO pInHandItem member. The
	// Jan03-era CObj<IInventoryItem> pInHandItem was removed when Get/SetInHandItem were ported to the
	// retail SItem bodies (@0x386ae0 / @0x386e70), which read and write sHandItem exclusively.
	SItem sHandItem;   // retail +0x64, save-format tag 9: the ownerless (no-unit) in-hand item
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
	bool GetInHandItem( SItem *pInfo ) const;			// retail @0x386ae0 (IPlayer vtable slot 12)
	void SetInHandItem( const SItem &sInfo );			// retail @0x386e70 (non-virtual, as in the PDB)
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
enum ETimeOfDay : int   // fixed underlying type so wOSBase.h / MapBuild.h can forward-declare it (retail NDb::ETimeOfDay)
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
	// (the dev-only SUnitPtrHolder is gone: retail factored the unit pocket out of CWorld into the
	// standalone CPocket saveload object, whose entry type is CPocket::SSmthPtrHolder<CUnitServer>
	// -- wPocket.h. See pPocket below.)
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
	// retail CWorld+0x1bc: the grenade-blast camera-shake queue (retail save tag 0x37; dev tag 51 --
	// numbering is independent). Producers: AddGrenadeExplosion (gated on game_eq_on_grenades);
	// drain: the mission's ProcessCameraCommands loop via GetEarthQuakeEvent.
	list< CPtr<CEarthQuakeEvent> > eventEarthQuakes;
	CObj< NWorld::CTerrain > pTerrain;
	CObj<CCTime> pTime, pAimTime;
	STime tPrev, tHiddenDelta;
	CObj<NAI::IAIMap> pAIMap;
	CObj<NAI::IPathNetwork> pPathNetwork;
	CObj<NRPG::IGame> pRPGGame;
	// retail CWorld+0x100 (save tag 18) is a CDBPtr<NDb::CTAmbientLight> -- the ambient-light TEMPLATE
	// *record*, not a resolved light. This fork used to hold the resolved CPtr<CAmbientLightReal>, so the
	// tag-18 chunk (retail's CDBPtr codec: a nested {1: int nID} short chunk = 2 hdr + 4 = 6 bytes) was
	// raw-read as a 4-byte pointer -> wire-audit RAWSZ 18.18 save=6 dev=4 on every slot (silent memcpy
	// corruption of a live pointer). Holding the template restores retail's exact wire; GetDefaultLight
	// (@0x3620a0) resolves it per call, so the IWorld contract is unchanged.
	CDBPtr<NDb::CTAmbientLight> pDefaultLight;
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
	// reacts after a short delay instead of seizing turn-based the instant it spots you. Serialized:
	// retail operator& @0x378730 DOES save the vector (tag 0x3b chunk, element serializer @0x37a750
	// tags 2/3) -- the earlier "not in operator&" note here was decomp-disproven (W3.3).
	struct SWillWantTBS
	{
		ZDATA
		CPtr<CPlayer> pPlayer;
		int nTimeLeft;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPlayer); f.Add(3,&nTimeLeft); return 0; }
	};
	vector<SWillWantTBS> willWantTBS;
	bool bScriptWantTurnBased = false;  // retail @CWorld+0x1c0 -- script's turn-based wish (saved state; ScriptWantTurnBased)
	// retail @CWorld+0x1c4/+0x1c8 (save tags 0x39/0x3a): the delayed game-over leg. The command drain
	// (ExecuteCommand, retail Segment @0x36bce0 DCK_GAME_OVER) stashes a CCmdDelayedCallGameOver's payload
	// here; it fires when the hero's corpse settles (InformCorpseStop @0x362180) or once tMaxGameOverCall
	// expires (Segment tail watchdog -- then EVERY segment: retail never clears the slot, the lua side
	// [OnPlayerLose -> ShowLoseDialog] is expected to end the game).
	CObj<CCmdCallScriptFunction> pGameOverCall;
	STime tMaxGameOverCall = 0;
	bool bIsBase = false;               // retail @CWorld+0x1b9 -- current zone IS the scenario "base"; computed in
	                                    // StartGame @0x36bb50; retail operator& @0x378730 saves it (tag 0x34)
	// retail CWorld @+0x1ba/+0x1bb -- per-segment vision-refresh COALESCING state (reset every Segment;
	// retail nevertheless serializes both, tags 0x35/0x36 -- behaviorally recomputed, format-relevant).
	// During a Segment, UpdateVisible(bForce=false) requests are DEFERRED into
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
	// retail CWorld+0x188 (save tag 42): THE strategic pocket -- the units (and, release-new, the plain
	// world objects) carried between maps. Retail factored this fork's flat unit-only `pocket` vector out
	// into the standalone CPocket saveload object (id 0xA0523090, wPocket.h/.cpp) and reaches it ONLY
	// through GetPocket() (@0x376f60, IWorld vtbl+0xd8) -- every consumer (luaUnitPlaceInPocket @0x2f9500,
	// luaObjectPlaceInPocket @0x2e9000, CUnitStateInPocket::OnStateStarted/Finished @0x3c9fb0/@0x3ca060)
	// goes through it. Created by the CGlobalGame ctor (@0x36a4b0 inlines `new CPocket`); the default
	// ctor (@0x36a120) leaves it null for the saveload path to fill from tag 42.
	CPtr<CPocket> pPocket;
	unordered_map< string, CPtr<CObjectBase> > nameToObj;
	// retail CWorld+0x1a0 (save tag 44): the per-floor height cache. Built by CreateRandom via
	// CreateHeightLayers @0x35de90; a retail save restores it populated (chunk 3 = the floor layers).
	CObj<NWorld::IHeightLayers> pHeightLayers;
	vector<CPtr<IVisObj> > allSoundStuff;   // @CWorld+0x1a8: weak refs to the live heard-not-seen markers (CDMesh), tag 48
	// retail CWorld+0x1b4 (save tag 0x32 = 50): the voxel-explosion scheduler (wExplTracker
	// CExplosionMaster). Created by CreateRandom/CreateDefault right after the nodes network (retail
	// @0x36d0b0/@0x36dc30 -> CreateExplosionMaster @0x355b40), Segment()ed between CheckForAcks and
	// the vision flush (retail Segment @0x36bce0). W5 serialization-convergence: LIVE -- both
	// AddGrenadeExplosion overloads enqueue into it (vtbl+0x14 STD / +0x10 ENG, retail
	// @0x364610/@0x3648c0); the dev miscObjects tracker pipeline is gone.
	CObj<IExplosionMaster> pExplosionMaster;
	// ---- retail v1.2 save convergence: CWorld::operator& rewritten to retail's EXACT tag scheme -----
	// (@0x378730). Retail organizes world state into inline HOLDER sub-objects at the low tags; this
	// fork keeps the state flat, so each holder's exact wire is emitted through a pointer-holder adapter
	// (the SBaseChunk pattern) mapped onto our flat members.
	//
	// The retail tag->member map was re-derived in wave 3. Ghidra renders @0x378730 with a broken `this`
	// (every operand is a bogus `&this[-1].<name>`), so the FIELD NAMES in that decomp are worthless --
	// what is trustworthy is each call's TEMPLATE INSTANTIATION and DataChunk size, which pin the member
	// type exactly. Cross-checked against the PDB layout those are unambiguous, because the serializer
	// walks members in ascending-offset order: e.g. tag 42 = CallObjectSerialize<CPtr<NWorld::CPocket>>
	// and pPocket (+0x188) is the only CPtr<CPocket> member; tag 18 = CDBPtr<NDb::CTAmbientLight> and
	// pDefaultLight (+0x100) is the only one; tags 45/46/47 are three consecutive 1-byte DataChunks and
	// the layout has exactly three consecutive bools there -- bFirstSegment (+0x1a4), bAttackAllowed
	// (+0x1a5), bUINeedUpdate (+0x1a6) -- between pHeightLayers (+0x1a0) and allSoundStuff (+0x1a8).
	//
	// Retail tags this fork still does not model are graceful-SKIPPED (the chunk format is tag-addressed
	// + length-prefixed, so an un-requested tag is simply left unread):
	//   47 = bool bUINeedUpdate (retail +0x1a6) -- retail's UI-refresh latch; no dev counterpart.
	//   49 = the CWeatherTracker base (retail CWorld+0xa0, 24 bytes on the wire) -- this fork predates
	//        the weather subsystem (IWorld::GetWeather is hardcoded WEATHER_SUNNY).
	// Old dev saves no longer load -- retail-save load is the acceptance test.
	struct SWaypointsChunk		// retail CWaypointsHolder @0x374e60 (tag 2): the named-waypoint hash
	{
		unordered_map< string, CObj<NAI::CAIRouteWaypoint> > *pWaypoints;
		int operator&( CStructureSaver &f ) { f.Add(2,pWaypoints); return 0; }
	};
	struct SMinesChunk		// retail CMinesWorld @0x37a270 (tag 3): {2=trappedObjects, 3=pMineTracker}
	{
		list<CPtr<IMine> > *pTrapped;
		CObj<CMineTracker> *pTracker;
		int operator&( CStructureSaver &f ) { f.Add(2,pTrapped); f.Add(3,pTracker); return 0; }
	};
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(TTBSWorld*)this);						// tag 1: CTBSWorld base
		SWaypointsChunk wpc = { &waypoints };					f.Add(2,&wpc);	// tag 2: CWaypointsHolder
		SMinesChunk     mnc = { &trappedObjects, &pMineTracker };		f.Add(3,&mnc);	// tag 3: CMinesWorld
		TTBSWorld::SActionTrackerChunk atc = ActionTrackerChunk();		f.Add(4,&atc);	// tag 4: CActionTracker
		f.Add(5,(CDebrisController*)this);					// tag 5: CDebrisController base
		f.Add(6,&pShow); f.Add(7,&pShowUnits);
		f.Add(8,&uiCmdsList); f.Add(9,&eventHits);
		f.Add(10,&pTerrain); f.Add(11,&pTime); f.Add(12,&pAimTime);
		f.Add(13,&tPrev); f.Add(14,&tHiddenDelta);
		f.Add(15,&pAIMap); f.Add(16,&pPathNetwork); f.Add(17,&pRPGGame);
		f.Add(18,&pDefaultLight);						// retail CDBPtr<CTAmbientLight> -- the CDBPtr codec emits {1:int nID} (6B)
		f.Add(19,&units); f.Add(20,&objects); f.Add(21,&segmentObjects);
		f.Add(22,&miscObjects); f.Add(23,&buildings);
		f.Add(24,&pGlobalAck); f.Add(25,&pTerrainInfo);
		f.Add(26,&deploySpots);
		f.Add(27,&bLeanAndMean); f.Add(28,&nRootLayersGroup); f.Add(29,&nPartiesAdded);
		f.Add(30,&sMapSafeZone);
		f.Add(31,&pAIJobManager); f.Add(32,&pOwnScript);
		f.Add(33,&nAIUnitsCreated);
		f.Add(34,&pGlobalGame); f.Add(35,&pDeployedDeadUnitsPlayer);
		f.Add(36,&unitGroups); f.Add(37,&createFlags);
		f.Add(38,&nTurnID); f.Add(39,&prevTurnTime);
		f.Add(40,&pDiplomacy); f.Add(41,&prevFastTurnTime);
		f.Add(42,&pPocket);							// retail +0x188 CPtr<CPocket> (@0x376f60 GetPocket)
		f.Add(43,&nameToObj);
		f.Add(44,&pHeightLayers);						// retail +0x1a0 CObj<IHeightLayers> (@0x376f00 GetHeightLayers)
		f.Add(45,&bFirstSegment);						// retail +0x1a4
		f.Add(46,&bAttackAllowed);						// retail +0x1a5 (IsAttackAllowed @0x376da0)
		f.Add(48,&allSoundStuff);						// tag 47 skipped (unmodeled -- see above)
		f.Add(50,&pExplosionMaster);						// tag 49 CWeatherTracker base skipped
		f.Add(51,&bFreezeStart);						// retail 0x33 (+0x1b8)
		f.Add(52,&bIsBase);							// retail 0x34 (+0x1b9)
		f.Add(53,&bDelayUpdateVisibleCalc);					// retail 0x35 (+0x1ba)
		f.Add(54,&bCallUpdateVisible);						// retail 0x36 (+0x1bb)
		f.Add(55,&eventEarthQuakes);						// retail 0x37 (+0x1bc)
		f.Add(56,&bScriptWantTurnBased);					// retail 0x38 (+0x1c0)
		f.Add(57,&pGameOverCall); f.Add(58,&tMaxGameOverCall);			// retail 0x39/0x3a
		f.Add(59,&willWantTBS);							// retail 0x3b
		return 0;
	}
	
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
	// retail @0x362180: hero-corpse-settled gate -> fire the stashed game-over call (see pGameOverCall)
	virtual void InformCorpseStop( CUnitServer *pUS );
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
	virtual bool TryUpdateVisible();   // retail @0x361610: action-finish vision-recalc delayer probe
	virtual CSyncSrc<IVisObj>* GetActive() const { return pShow; }
	virtual CSyncSrc<IVisObj>* GetUnits() const { return pShowUnits; }
	virtual CUICmd* GetUICommand();
	virtual CHitLocator* GetHitEvent();
	virtual CEarthQuakeEvent* GetEarthQuakeEvent();   // retail IWorld vtbl+0x10 @0x363ff0
	//
	CUnitServer *GetUnitServerByPersID( int nPersID ) const;
	virtual CCTime* GetAimTime() const { return pAimTime; }
	virtual NRPG::IGame* GetGame() { return pRPGGame; }
	virtual NAI::IAIMap* GetAIMap() { return pAIMap; }
	// retail IWorld vtbl+0xe4 @0x376de0: the CDebrisController base (CWorld+0x28)
	virtual CDebrisController* GetDebris() { return this; }
	virtual NAI::IPathNetwork* GetPathNetwork() { return pPathNetwork; }
	virtual CFuncBase<STerrainInfo>* GetTerrainInfo() const { return pTerrainInfo; }
	virtual NWorld::IHeightLayers* GetHeightLayers() { return pHeightLayers; }   // retail @0x376f00 (IWorld vtbl+0x90)
	// retail IWorld vtbl+0x30: the live terrain object (blast scorch CTerrain::DrawExplosion @0x38ab50)
	virtual NWorld::CTerrain* GetTerrain() const { return pTerrain; }
	// retail @0x3620a0 -- resolves the stored TEMPLATE to a fresh light on every call (out-of-line: the
	// body needs SRand + the CTAmbientLight roulette; see wMain.cpp).
	virtual NDb::CAmbientLightReal* GetDefaultLight();
	// retail @0x376f60 (IWorld vtbl+0xd8) -- the ONLY way retail reaches the pocket.
	virtual CPocket* GetPocket() { return pPocket; }
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
	virtual void CreateRestored( NRPG::CGlobalGame *pGlobalGame );	// retail @0x36e100 (zone-reenter: restarts the turn)
	virtual void RestoreRuntimeCaches( NRPG::CGlobalGame *pGlobalGame );	// save-load resume: no StartGame (retail load path @0x1f5fd0 runs no world hook)
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
	// fMaxRange: the per-attack range cap (retail SAttackRayInfo.fMaxRange). Default 30 = the old
	// N_WEAPONTRAIL_MAXDISTANCE == retail's value on the normal/accidental shot paths; grenade
	// splinters pass fFragmentRange*FP_GRID_STEP (retail ExplodeFragments @0x356a80).
	void PerformRangedAttack( const NRPG::CAttackPortion &ap, const CRay &ray, const vector<NRPG::IAttackable*> &ignores, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed, float fMaxRange = 30.0f );
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
	// retail @0x764140 takes BOTH grenade records: an engineer grenade arrives with a null
	// pRPGGrenade and a live pRPGEngGrenade (contact-fused eng server, thrower's ENG skill).
	void ThrowGrenade( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fTFly,
		NDb::CModel *pModel, NDb::CRPGGrenade *pRPGGrenade, CUnitServer *pUnitServer,
		NDb::CRPGEngGrenade *pRPGEngGrenade = 0 );
	void ThrowKnife( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fDistance,
		NDb::CModel *pModel, NRPG::CAttackPortion &attack, NRPG::IInventoryItem *pIItem, CUnitServer *pUnitServer );
	void LaunchRocket( const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &attack, 
		NRPG::IClipItem *pRocket, CUnitServer *pIgnored, NDb::CEffect *_pEffect = 0 );
	virtual void AddGrenadeExplosion( const CVec3 &vStartPosition,
		NDb::CRPGGrenade *pRPGGrenade, CUnitServer *pUnitServer = 0, CObjectBase *pIgnitionObject = 0,
		const SPerkMineModifiers *pMods = 0 );
	// retail @0x7648c0 (world vtbl+0x120): the ENGINEER-grenade blast, scaled by the
	// placer/thrower's ENGINEERING skill. The damage tracker's eng mode is still unported --
	// see docs/NEXT_SESSION_ITEMS_FOLLOWUPS.md.
	virtual void AddGrenadeExplosion( const CVec3 &vStartPosition,
		NDb::CRPGEngGrenade *pRPGEngGrenade, int nEngSkill, CUnitServer *pUnitServer = 0,
		CObjectBase *pIgnitionObject = 0, const SPerkMineModifiers *pMods = 0 );
	void KillObject( CObjectServerBase *pOS );
	// Retail has NO CWorld pocket methods -- it inlines these two bodies into luaObjectPlaceInPocket
	// (@0x2e9000) / luaObjectRestoreFromPocket (@0x2e9130), reaching the world through GetPocket()
	// (vtbl+0xd8) plus two virtuals this fork does not expose (vtbl+0x198 = the KillObject drop,
	// vtbl+0x1d0 = the AddObjectServerBase @0x3635b0 re-add). They stay factored here because both
	// need the private `objects` list; the pocket half now runs through pPocket, exactly as retail.
	//   pocket  = CPocket::PlaceObjectInPocket (non-master hold) + KillObject + BeAddedToVisitiors(false)
	//   restore = re-list + rebind + CPocket::RemoveObjectFromPocket
	void PlaceObjectInPocket( CObjectServerBase *pObject );
	void RestoreObjectFromPocket( CObjectServerBase *pObject );
	void FindCloseGroundItems( CUnit *pU, vector<SItem> *pRes );
	bool IsWinnerPlayer( IPlayer *pPlayer );
	void GenerateDebris( NDb::CDebrisMaterial *pDebrisMaterial, const CVec3 &ptCenter, const CVec3 &ptDir, int nDebris );
	void CreateSoundStuff( CUnitServer *pWho, vector<CObj<CTimedObject> > *stuff, CVec3 ptPos );   // @0x369110
	// retail returns the created C3DSound* (or 0) -- CDumbUnitServer::CreateFlash hands it up to
	// CExecShoot's long-burst retention slot (SLongBurstSnd, save tag 3, EndSound lifecycle)
	C3DSound* MakeAISound( NDb::CAISound *pAISound, CDumbUnitServer *pWho, int nSoundType = 0, NDb::CSound *pSound = 0 );
	void MakeSound( const CVec3 &ptCenter, NDb::CSound *pSound );
	list< CObj<IDynamicObject> > *GetMiscObjects() { return &miscObjects; }
	NAI::IAIJobManager *GetAIJobManager() { return pAIJobManager; }
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
	// retail @0x376f40 fills IWorld slot 59.
	virtual NRPG::CGlobalGame *GetGlobalGame() const { return pGlobalGame; }
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
	void OnUnitAdded( CUnitServer *pUnit );
	void ChangeUnitPlayer( CUnitServer *pUnit, CPlayer *pPlayer );
	// (the unit-pocket trio is gone: retail keeps no CWorld-level pocket methods -- every caller goes
	// GetPocket()->Is/Place/RemoveUnitInPocket, as luaUnitPlaceInPocket @0x2f9500 and
	// CUnitStateInPocket::OnStateStarted/OnStateFinished @0x3c9fb0/@0x3ca060 do.)
};
extern CWorld *pCurrentWorld;
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif