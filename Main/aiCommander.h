#ifndef _AICOMMANDER_H_
#define _AICOMMANDER_H_

#include "wInterface.h"
#include "aiState.h"     // SAIState is embedded BY VALUE (operator& tag7) -- needs the full definition

namespace NWorld
{
	enum ETBSEvent;
	class CWorld;
	class CPlayer;
	class CObjectServerBase;
	class CCommand;
	class CUnitServer;
}

namespace NAI
{
class IAIUnit;
class IAILogic;
class CAIReaction;
////////////////////////////////////////////////////////////////////////////////////////////////////
// AI-convergence Stage 2: CAITacticalCommander removed. Retail inlines its per-unit-logic install + pump
// loop into CAICommander::GenerateCommand @0x353d0 via a family of tracker structs (SAICommandTracker /
// SAIUpdateTracker / SAIUnitsTracker). These are reconstructed here from the aiCommander.obj decodes.
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAICommandTracker (operator& @0x37f10): the AI's per-segment outgoing command queue. Folds the old
// CAICommander::bCommandGiven latch (at most ONE decision per world segment).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAICommandTracker
{
	ZDATA
	list< CObj<NWorld::CCommand> > commands;   // pending world commands (front popped once per segment). MUST be
	                                           // CObj (OWNING), like CCommander::cmds: a weak CPtr let the harvested
	                                           // command -- owned only by GenerateCommand's local CObj pNew -- die at
	                                           // end-of-segment (last owner released -> DestroyContents reset it to an
	                                           // empty CCmdSetCommand + marked it ref-invalid), so next segment GetCommand
	                                           // popped a null-pUnit/null-inner husk -> CCommander::Do->IsSkippable crash.
	bool bCommandGiven;                        // one-decision-per-segment latch (cleared each OnSegment)
	int  nNonFreezeCounter;                    // runaway-AI guard tally (DoCommand bumps it)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&commands); f.Add(3,&bCommandGiven); f.Add(4,&nNonFreezeCounter); return 0; }
	//
	SAICommandTracker(): bCommandGiven( false ), nNonFreezeCounter( 0 ) {}
	//
	NWorld::CCommand* GetCommand();                        // @0x341b0: pop the front once per segment
	void DoCommand( NWorld::CCommand *pCmd );              // @0x34290: queue a command; bump the counter
	void Clear( NWorld::CUnitServer *pUS );                // @0x34200: drop every queued CCmdSetCommand for pUS
	bool IsEmpty() const;                                  // @0x33940: empty AND nothing given
	void OnSegment() { bCommandGiven = false; }            // @0x338b0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUpdateTracker (serialize wrapper @0x386d0): the queue of unit reactions waiting to fire. Update()
// pops the FRONT reaction and runs its Update() (which re-picks that unit's logic) -- ONE per segment.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUpdateTracker
{
	ZDATA
	list< CPtr<CAIReaction> > reactions;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&reactions); return 0; }
	//
	void Add( CAIReaction *pReaction );                   // @0x34320: dedup-append
	void Update();                                         // @0x343c0: pop front reaction, fire its Update()
	bool CanGetCommand() const { return reactions.empty(); }  // @0x33960
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitsTracker (operator& @0x38890): the round-robin over the commander's own units. Holds its OWN
// working copy; Next() consumes units as it rotates; SetUnits() re-seeds from the commander's roster.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUnitsTracker
{
	ZDATA
	vector< CPtr<IAIUnit> > units;    // working copy (drained as the rotation advances)
	CPtr<IAIUnit>           pCurrent;
	CPtr<NWorld::IPlayer>   pPlayer;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&units); f.Add(3,&pCurrent); f.Add(4,&pPlayer); return 0; }
	//
	void SetUnits( const vector< CObj<IAIUnit> > &src );    // @0x350c0: reseed the working copy, then Next()
	void Next();                                          // @0x34450: advance to the next good unit
	void Validate();                                      // @0x34620: if pCurrent not good -> Next()
	void SetPlayer( NWorld::IPlayer *p );                 // @0x33d20
	bool IsGoodUnit( IAIUnit *pUnit );                    // @0x33a10: alive, my player, can fight
	IAIUnit* GetCurrentUnit() const { return pCurrent; }  // @0x33970
	IAIUnit* GetNearestUnit();                            // @0x33b90: nearest live unit to pCurrent
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAICommander: public NWorld::CCommander
{
	OBJECT_BASIC_METHODS(CAICommander);
	ZDATA
	ZPARENT( NWorld::CCommander );
	CPtr<NWorld::CPlayer> pPlayer;
	vector< CObj<IAIUnit> > units;   // retail tag4 vector (was a dev list; list<->vector serialize to the
	                                 // identical tag-1 sub-chunk layout, so this is save-COMPATIBLE).
	// AI-convergence Stage 3 item 6 (member-layout parity) -- worldToAIUnit LANDED: retail's O(1) GetAIUnit
	// cache at tag5 (operator& @0x374d0 DoHashMap<CPtr<CUnitServer>,CPtr<IAIUnit>,SPtrHash>). GetAIUnit @0x33eb0
	// is `worldToAIUnit.find(pUS)`; AddUnit @0x351c0 does `worldToAIUnit[pUS]=aiUnit` after `units.push_back`;
	// RemoveUnit @0x34910 erases the entry alongside the units erase. The cache is kept in LOCKSTEP with `units`
	// at every commander-`units` mutation: OnUnitAdded inserts, RemoveUnit erases, Synchronize rebuilds after a
	// prune (a pruned unit's now-dead server pointer can't key an erase). Serialized at tag5 exactly as retail.
	// LockedObjects (dev-only; retail has no such member) moves off tag5 to a retail-unused tag (16).
	// AI-convergence FINAL structural item -- SAIState is now EMBEDDED BY VALUE at tag7 (retail
	// CallObjectSerialize<SAIState> @0x374d0), NOT a heap CObj pointer. This became possible once the AI-state
	// layer was collapsed from the dev IAIState:CObjectBase interface (+CAIState impl) to a plain value struct
	// (aiState.h): the serialized CPtr<IAIState> back-refs that used to block it (in since-deleted dev-only
	// layers) were all DEAD and were removed, and the one live holder -- CAIUnit::pAIState -- is a raw, transient
	// (unserialized) weak pointer re-established every AI segment by SAIState::Synchronize. The state's own
	// pAICommander back-ref points at THIS (a registered object), so serializing the struct inline produces no
	// orphan. See aiState.h.
	unordered_map< CPtr<NWorld::CUnitServer>, CPtr<IAIUnit>, SPtrHash > worldToAIUnit;   // tag5 (retail GetAIUnit cache)
	list< CPtr<NWorld::CObjectServerBase> > LockedObjects;                              // dev-only -> tag16
	CPtr<NWorld::CWorld> pWorld;
	bool bAITurn;
	bool bWantTurnBased;
	// BUG 2 (realtime reaction delay): countdown for the delayed real-time -> turn-based switch. -1 = idle;
	// armed to 50 (retail willWantTBS nTimeLeft = 0x32) when bWantTurnBased first appears in real time, then
	// decremented each CAICommander::Segment (per world tick) until it fires WantTurnBased. Transient runtime
	// state -- deliberately NOT in operator& (a save mid-countdown just re-arms from the saved bWantTurnBased).
	int nWantTBSDelay = -1;
	// AI-convergence Stage 2 members (see the tracker structs above). The tactical commander is GONE; its
	// per-unit-logic install (now the map-deploy CreateUnitReaction) + pump loop lives here.
	SAIState             state;            // tag7 -- embedded by value (retail CallObjectSerialize<SAIState>)
	int                  nAILag;           // @+96: the AI thinks every 3rd segment (Segment throttle)
	SAICommandTracker    commandTracker;   // @+100 (folds the old bCommandGiven latch)
	SAIUpdateTracker     updateTracker;    // @+132: pending reaction updates
	SAIUnitsTracker      unitsTracker;     // @+136: the round-robin over own units
	CPtr<IAIUnit>        pLastReportedUnit; // @+156: last unit a CUICmdAIUnitWillMove was reported for
	// eotLogics @0x34810 (retail operator& tag10, DoHashMap): memoizes which per-unit logics reported
	// end-of-turn this turn -- CheckCanDo/IsEndOfLogicTurn mark `eotLogics[logic]=1`, IsEndOfLogicTurn/
	// the round-robin gate on `find(logic)`. Now the retail hash_map (was a transient linear-scan list) and
	// SERIALIZED at tag10 to match retail's save schema. Cleared on OnPassControl/OnTurnStarted.
	unordered_map< CPtr<IAILogic>, int, SPtrHash > eotLogics;
	// Save-format (AI-convergence): tags now match retail (operator& @0x374d0) exactly where the dev shares the
	// member -- units 4, worldToAIUnit 5, pWorld 6, state 7 (embedded SAIState, CallObjectSerialize<SAIState>),
	// nAILag 8, commandTracker 9, eotLogics 10, updateTracker 11, unitsTracker 12, pLastReportedUnit 13.
	// Retail ENDS at tag 13 -- the dev-only members (bAITurn/bWantTurnBased/LockedObjects) are TRANSIENT:
	// serializing them at 14/15/16 made dev-written saves diverge from retail's writer (wire-audit MISS
	// 2.14-2.16); the deserialize ctor re-arms them (bAITurn/bWantTurnBased=false, LockedObjects empty).
public:	// operator& must be reachable from CSequenceCommander's base-as-chunk Add (retail @0x393f0)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(NWorld::CCommander*)this); f.Add(3,&pPlayer); f.Add(4,&units); f.Add(5,&worldToAIUnit); f.Add(6,&pWorld); f.Add(7,&state); f.Add(8,&nAILag); f.Add(9,&commandTracker); f.Add(10,&eotLogics); f.Add(11,&updateTracker); f.Add(12,&unitsTracker); f.Add(13,&pLastReportedUnit); return 0; }
	//
private:
	// TBSEvents
	void OnTurnStarted();
	void OnTurnFinished();
	void OnCancelAction();
	void OnStartRealTime();
	// AI-convergence Stage 2 helpers (aiCommander.obj decodes)
	void RebuildAIUnitCache();                            // resync worldToAIUnit from `units` after a bulk prune
	void SyncAIState();                                   // rebuild the CAIState ally/enemy rosters + groups
	void CheckForFinishedLogics();                        // @0x34040: retire finished per-unit logics
	void CheckForUpdates();                               // @0x34640: enqueue each unit's pending reaction
	void OnAISegment();                                   // @0x346f0: per-segment unit tick + reaction pump
	bool HasUnitWork( IAIUnit *pUnit );                   // @0x34b60
	// retail ownership domain: a commander operates ONLY its own player's units (retail CWorld::AddUnit
	// @0x369580 -> CPlayerBase::AddUnit @0x374a10 routes each unit to its OWNING commander alone, so the
	// retail `units` list never holds foreign units). The dev tree keeps the every-commander broadcast
	// (SAIState::Synchronize builds the ally/enemy rosters from this list), so every operational loop
	// must filter to the owner. This replaces the old isolation-by-frozen-bUnderAIControl (each commander
	// used to hold its own duplicate wrappers, foreign ones flagged false; with the retail SHARED wrapper
	// the flag is true on every commander's copy and can no longer isolate).
	bool IsOwnUnit( IAIUnit *pUnit ) const;
	bool IsSomebodyNeedUpdate();                          // @0x33990
	bool IsSomebodyThinking();                            // @0x33f50
	bool IsSequence();                                    // @0x339f0
	bool IsThisPlayerTurn();                              // @0x33f10
	bool IsEndOfLogicTurn( IAILogic *pLogic );            // @0x34810
	bool IsPossibleFreeze();                              // @0x33ad0
	bool CheckCanDo( NWorld::CCommand *pCmd );            // @0x34c90
	void ClearNonFreezeCounters();                        // @0x33a90
	void FinishTurn();                                    // @0x34790
	//
public:
	// Wire-convergence note: bAITurn/bWantTurnBased are dev-only members (retail's 160-byte CAICommander has
	// no such fields and its operator& @0x374d0 ends at tag 13), so a RETAIL save never carries tags 14/15 --
	// the deserialize ctor (this one) must leave them in the retail-equivalent idle state, not uninitialized.
	CAICommander() : nAILag( 0 ), bAITurn( false ), bWantTurnBased( false ) {}
	CAICommander( NWorld::CWorld *_pWorld, NWorld::CPlayer *_pPlayer );
	// retail CAICommander::SetPlayer @0x33e60: bind the commander to its player AFTER construction --
	// sets pPlayer and re-seeds unitsTracker.pPlayer (nothing else). Used by the human player's
	// CSequenceCommander install (CPlayerTracker ctor @0x287d70 news the commander with player=0,
	// creates the player via AddPlayer, then SetPlayer()s it).
	void SetPlayer( NWorld::IPlayer *_pPlayer );
	void GenerateCommand();
	virtual void OnPassControl( NWorld::CPlayer *_pPlayer );
	virtual void OnUnitDied( NWorld::CUnitServer *pUnit );
	void RemoveUnit( NWorld::CUnitServer *pUS );
	virtual void OnSeeUnit( NWorld::CUnitServer *pWatcher, NWorld::CUnitServer *pTarget );
	virtual bool IsEndOfTurn();
	virtual bool IsRequestInterrupt() const { return false; }
	virtual void Segment();
	virtual void OnUnitAdded( NWorld::CUnitServer *pUnit );
	virtual void OnTBSEvent( NWorld::ETBSEvent event );
	// Object lock
	bool IsObjectLocked( NWorld::CObjectServerBase *pObject );
	void LockObject( NWorld::CObjectServerBase *pObject );
	void UnLockObject( NWorld::CObjectServerBase *pObject );
	void UnLockAllObjects();
	//
	NWorld::CWorld *GetWorld() { return pWorld; }
	// [post-load reconnect] a retail save stores pWorld as an IWorld* ref whose identity this fork's
	// CPtr<NWorld::CWorld> deserialize resolves to null (multiple-inheritance base-adjust); re-establish it
	// (and the embedded SAIState back-refs) from CWorld::CreateRestored before the first Segment runs, so
	// GenerateCommand()'s GetWorld()->IsSequence() (aiCommander.cpp:889) is safe.
	void ReconnectWorld( NWorld::CWorld *pW ) { pWorld = pW; state.SetBackRefs( pW, this ); }
	NWorld::CPlayer *GetPlayer() { return pPlayer; }
	SAIState *GetAIState() { return &state; }
	const vector< CObj<IAIUnit> > &GetUnitsList() const { return units; }
	IAIUnit *GetAIUnit( NWorld::CUnitServer *pUnit );
	bool IsAITurn() { return bAITurn; }
	bool HasVisibleEnemies();
	void Synchronize();
	void WantTurnBased();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander @0x35b60 -- the human/scripted player's flavour of CAICommander (release-NEW; oracle
// decomp/src/s2_aicommander.h, registered 0x51823190 alongside CAICommander 0x02731170). It carries
// NO new data members; it differs from CAICommander only by its own vtable + a GenerateCommand override
// that auto-drives the commander ONLY while the world runs a cinematic sequence (CWorld::IsSequence
// @0x376ff0). It is the HUMAN player's commander: CPlayerTracker's ctor (PlayerTracker.cpp, retail
// @0x287d70) news one per mission, so every retail save carries exactly one instance.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSequenceCommander: public CAICommander
{
	OBJECT_BASIC_METHODS(CSequenceCommander);
	ZDATA
	ZPARENT( CAICommander );
	// Wire-convergence (retail operator& @0x393f0): retail serializes the ENTIRE CAICommander base as ONE
	// nested sub-chunk at tag 2 (CallObjectSerialize<NAI::CAICommander>) and nothing else -- the class adds
	// NO wire fields of its own. Without this override the class inherited CAICommander::operator& and read/
	// wrote the base's FLAT tag 2..16 table at TOP level, so every retail save (top-level = the single tag-2
	// container) mis-parsed: retail 2.3 pPlayer(4B) was read as CCommander bInterruptRequest(1B), 2.4 units
	// as bStopAction, and everything from worldToAIUnit onward was silently dropped -- per-slot corruption
	// of the human player's commander on all 9 audit slots.
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, (CAICommander*)this ); return 0; }
public:
	CSequenceCommander() {}
	CSequenceCommander( NWorld::CWorld *_pWorld );
	void GenerateCommand();
	// retail CAICommander does NOT override CCommander::IsRequestInterrupt (no such method in the
	// binary) -- the `return false` override above is a dev-side hack for the AI issuance model. The
	// HUMAN's commander must keep the retail base semantics: a non-skippable command queued via Do()
	// (bInterruptRequest) seizes the turn (CTBSWorld request scan, retail @0x377830).
	virtual bool IsRequestInterrupt() const { return NWorld::CCommander::IsRequestInterrupt(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
