#ifndef __AICOMBATLOGIC_H_
#define __AICOMBATLOGIC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the combat-logic ENGINE + its concrete logics (structural port).
//
// CAICombatLogic is the heart of the tactical AI. It owns: a CAILog (pLog) the chosen action writes
// into, a set of place sources (placeSources) enumerating where the unit could act, and a
// CAIChoosePlaceJob (pChoosePlace) that picks the best place per action. Each turn the concrete logic's
// MakeDecision() builds a CDecision<CAIAction*> over the registered actions (scored via SActionInfo +
// CSign rules), picks the best with GetBestAction, then DoAction() logs the move to that place and runs
// the action; GenerateCommand drains pLog into the unit's world-command queue.
//
// MI (verified from the ctor @0x00433340): CAICombatLogic : CAILogic + CAIJob, both over a VIRTUAL
// CObjectBase. Ctor CAICombatLogic(IAIUnit*, IAIChoosePlaceJob*). Members: pLog(=CreateAILog()),
// placeSources, pChoosePlace, prepareState(=PS_FINISHED), nUnitLastAP, regOnNewTurn (a CEventOnPassControl
// handler that fires OnNewTurn). AddAction<T>/AddPlaceSource/GetInfo<T> bind actions to sources;
// concrete logics (Attack/Defence/Guard/Retreat/AfterCombat) differ only in their ctor repertoire +
// MakeDecision rules.
//
// WIP - NOT yet in Main.vcxproj; the engine method bodies + the per-logic MakeDecisions land in the
// phase-5 / build-settle work. Decompiles: reconstruction/exports/{defence.c, decisions.c}.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiLogic.h"          // IAILogic, CAILogic
#include "aiJob.h"            // CAIJob
#include "aiDecision.h"       // CDecision<CAIAction*>, CRule, CSign, ISign
#include "aiActionBase.h"     // CAIAction, SActionInfo, SPlaceWithAP
#include "aiCombatLog.h"      // CAILog
#include "aiActionPlaceSource.h"
#include "aiChoosePlace.h"
#include "..\Misc\EventsBase.h"   // NGlobal::CEventRegister (CAIAfterCombatLogic::regOnNewTurn)
namespace NWorld { class CEventOnNewPlayerTurn; }
namespace NAI
{
// the 19 concrete actions (full defs in aiActions.h; pointers only here)
class CAIShootAction; class CAIThrowGrenadeAction; class CAILaunchRocketAction; class CAIReloadAction;
class CAIThrowKnifeAction; class CAIMeleeAction; class CAILootAction; class CAIHealAction;
class CAIMoveToEnemyAction; class CAIBeginSnipeAction; class CAICollectSnipeAPAction; class CAISnipeShotAction;
class CAICancelSnipeAction; class CAIDockWithHGAction; class CAIUndockFromHGAction; class CAIShootFromHGAction;
class CAITerrorPKAction; class CAIWearPKAction; class CAILeavePKAction;
class CUnitArea;
////////////////////////////////////////////////////////////////////////////////////////////////////
// prepare state of the per-turn decision pipeline. RETAIL PDB enum (NAI::CAICombatLogic::EPrepareState):
// prepareState IS the DoJob per-tick stage counter (there is NO separate nJobStage). PS_FINISHED (=4) is the
// "not thinking" sentinel (IsThinking == prepareState != PS_FINISHED). Converging from the dev 4-value enum is
// a latent save-compat break on the tag-7 prepareState ordinal -- transient per-turn state, runtime-deferred.
enum EPrepareState { PS_INIT, PS_PLACESOURCE, PS_ACTION, PS_THINK, PS_FINISHED };
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICombatLogic - command-driven combat behaviour base (the engine).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAICombatLogic: public CAILogic, public CAIJob
{
	ZDATA
	CPtr<CAILog>                          pLog;          // action log the chosen action writes into
	vector< CObj<IAIActionPlaceSource> >  placeSources;  // candidate-place enumerators
	CObj<IAIChoosePlaceJob>               pChoosePlace;  // picks the best place per action
	EPrepareState                         prepareState;  // pipeline state
	int                                   nPSToPrepare = 0; // release tag 8: place-source index pending prepare (save-format)
	int                                   nUnitLastAP;   // anti-cycling: AP at last decision (release tag 9)
	// regOnNewTurn: fires CAICombatLogic::OnNewTurn on each new player turn. The a5dll's CEventOnNewPlayerTurn
	// is the live stand-in for retail's CEventOnPassControl (both fire at StartPlayerTurn; the a5dll never
	// throws CEventOnPassControl). NOT serialized (a runtime subscription re-established by the ctor -> excluded
	// from operator&, exactly like CAIAfterCombatLogic's own regOnNewTurn). Fires ALONGSIDE AfterCombat's on the
	// one event; they touch disjoint state (base: prepareState/nUnitLastAP; derived: bAPUpdated).
	NGlobal::CEventRegister< CAICombatLogic, NWorld::CEventOnNewPlayerTurn > regOnNewTurn;
	ZEND
public:
	int operator&( CStructureSaver &f );   // public: the concrete logics serialize (CAICombatLogic*)this
protected:
	// Register an action with the choose-place job (bound to a place source) and return it. @0x00423630
	template<class TAction> TAction* AddAction( TAction *pAction, IAIActionPlaceSource *pSrc )
	{
		if ( IsValid( pChoosePlace ) && IsValid( pAction ) && IsValid( pSrc ) )
			pChoosePlace->AddAction( pAction, pSrc );
		return pAction;
	}
	// Fetch the action's info at the place the chooser picked for it. @0x004166d0 (release uses the
	// SActionInfo memo cache; here we compute via GetInfoInner at the chosen place).
	// The SActionInfo cache memoizes exactly this GetInfoInner result, so the direct call is shape-faithful
	// (release routes through SActionInfo<TAction>::GetInfo, whose `info` member is private to the action).
	// aiAttackLogic CAICombatLogic::GetInfo<TAction> instantiations (VA = RVA + 0x400000):
	//   @0x004260c0 <CAIShootAction>       @0x004261a0 <CAIThrowGrenadeAction>  @0x00426280 <CAILaunchRocketAction>
	//   @0x00426360 <CAIThrowKnifeAction>  @0x00426440 <CAIMeleeAction>         @0x00423e20 <CAICollectSnipeAPAction>
	//   @0x00423ee0 <CAISnipeShotAction>   @0x00423fa0 <CAICancelSnipeAction>   @0x00424060 <CAILeavePKAction>
	//   @0x00425a90 <CAIDockWithHGAction>  @0x00425b50 <CAIUndockFromHGAction>  @0x004265e0 <CAIShootFromHGAction>
	//   @0x00426520 <CAIBeginSnipeAction>  @0x004266a0 <CAITerrorPKAction>      @0x00426760 <CAIWearPKAction>
	template<class TAction> void GetInfo( TAction *pAction, typename TAction::SInfo *pInfo )
	{
		SPlaceWithAP place;
		if ( IsValid( pChoosePlace ) && IsValid( pAction ) &&
			 pChoosePlace->GetPlaceForAction( pAction, &place ) )
			pAction->GetInfoInner( place, pInfo );
	}
	//
	void DoAction( CAIAction *pAction );        // log move-to-place (pos+AP) then pAction->Do(pLog)
	CAILog* GetLog() const { return pLog; }
	// --- Stage-0 parity scaffolding (BUILD-SAFE, currently UNCALLED) -- release helper predicates.
	// Non-virtual, no new member => vtable/layout/save-format unchanged. Foundation for the world
	// executor-probe + job-manager driving convergence; do NOT wire into IsNeedToThink/IsEndOfTurn/
	// IsFinished until the executor object + tactical-commander Add/Remove reconciliation land.
	bool IsLogicValid();          // @0x00432780  live unit+server+world + server CanFight
	bool IsExecutingCommand();    // @0x00432ea0  the unit's person component is mid-action
	bool HasCommandToExecute();   // @0x00432da0  live server cmd | queued logic cmd | non-empty log
	bool HasSameAP();             // @0x00432730  unit AP == the StopThinking snapshot (nUnitLastAP)
	//
public:
	// both ctors MUST init regOnNewTurn (CEventRegister has no default ctor -- it ASSERTs), exactly like
	// CAIAfterCombatLogic; the default ctor is the save/load registry create path.
	CAICombatLogic(): regOnNewTurn( this, &CAICombatLogic::OnNewTurn ) {}
	CAICombatLogic( IAIUnit *pUnit, IAIChoosePlaceJob *pChoosePlace );
	//
	IAIActionPlaceSource* AddPlaceSource( IAIActionPlaceSource *pSrc );  // dedup-append
	//
	// IAILogic / CAIJob overrides
	virtual void GenerateCommand();             // drain pLog's world commands into the queue
	virtual void DoJob();                       // run the find-places -> choose -> MakeDecision pipeline
	virtual bool IsIdleJob();                   // @0x004331c0 -- overrides CAIJob's return-false idle probe
	virtual bool IsNeedToThink();
	virtual void Think();
	virtual bool IsThinking();
	virtual void StopThinking();
	virtual bool IsFinished();
	virtual bool IsEndOfTurn();
	// CAICombatLogic does NOT override the no-arg virtual slot-12 OnNewTurn() (retail's slot-12 is empty) -- it
	// inherits CAILogic::OnNewTurn(){}. The per-turn reset is the EVENT handler below, fired via regOnNewTurn.
	void OnNewTurn( const NWorld::CEventOnNewPlayerTurn &event );   // @0x00432820 (retail CEventOnPassControl handler)
	//
	virtual void MakeDecision() = 0;            // per-logic action/rule set
	virtual bool CanSkip() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIAttackLogic - the full-repertoire combat logic. 3 place sources + one of every action; MakeDecision
// builds the 19-action CDecision. Member set authoritative from the ctor @0x00420f30.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIAttackLogic: public CAICombatLogic
{
	OBJECT_BASIC_METHODS( CAIAttackLogic );
	ZDATA
	CObj<IAIActionPlaceSource>      attackPlaceSource;   // CreateAttackPlaceSource
	CObj<IAIActionPlaceSource>      currentPlaceSource;  // CreateCurrentPlaceSource
	CObj<IAIActionPlaceSource>      enemyPlaceSource;    // CreateNearEnemyPlaceSource
	CObj<CAIShootAction>            pShoot;              // -> attackPlaceSource
	CObj<CAIThrowGrenadeAction>     pThrowGrenade;       // -> attackPlaceSource
	CObj<CAILaunchRocketAction>     pLaunchRocket;       // -> attackPlaceSource
	CObj<CAIReloadAction>           pReload;             // -> currentPlaceSource
	CObj<CAIThrowKnifeAction>       pThrowKnife;         // -> attackPlaceSource
	CObj<CAIMeleeAction>            pMelee;              // -> enemyPlaceSource
	CObj<CAILootAction>             pLoot;               // -> currentPlaceSource
	CObj<CAIHealAction>             pHeal;               // -> currentPlaceSource
	CObj<CAIMoveToEnemyAction>      pGetRidOfInactive;   // -> currentPlaceSource
	CObj<CAIBeginSnipeAction>       pBeginSnipe;         // -> currentPlaceSource
	CObj<CAICollectSnipeAPAction>   pCollectSnipeAP;     // -> currentPlaceSource
	CObj<CAISnipeShotAction>        pSnipeShot;          // -> currentPlaceSource
	CObj<CAICancelSnipeAction>      pCancelSnipe;        // -> currentPlaceSource
	CObj<CAIDockWithHGAction>       pDockWithHG;         // -> currentPlaceSource
	CObj<CAIUndockFromHGAction>     pUndockFromHG;       // -> currentPlaceSource
	CObj<CAIShootFromHGAction>      pShootFromHG;        // -> currentPlaceSource
	CObj<CAITerrorPKAction>         pTerrorPK;           // -> currentPlaceSource
	CObj<CAIWearPKAction>           pWearPK;             // -> currentPlaceSource
	CObj<CAILeavePKAction>          pLeavePK;            // -> currentPlaceSource
	ZEND int operator&( CStructureSaver &f );
public:
	CAIAttackLogic() {}
	CAIAttackLogic( IAIUnit *pUnit );           // @0x00420f30
	virtual void MakeDecision();                // @0x00421890 (19-action CDecision)
	virtual bool CanSkip() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIDefenceLogic - hold a commanded place: 1 OnePlace source + shoot/grenade/rocket/reload. @0x004394b0
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIDefenceLogic: public CAICombatLogic
{
	OBJECT_BASIC_METHODS( CAIDefenceLogic );
	ZDATA
	CObj<IAIActionPlaceSource>      currentPlaceSource;  // CreateOnePlacePlaceSource(attackPlace)
	CObj<CAIShootAction>            pShoot;
	CObj<CAIThrowGrenadeAction>     pThrowGrenade;
	CObj<CAILaunchRocketAction>     pLaunchRocket;
	CObj<CAIReloadAction>           pReload;
	SPathPlace                      attackPlace;         // the place being defended
	ZEND int operator&( CStructureSaver &f );
public:
	CAIDefenceLogic() {}
	CAIDefenceLogic( IAIUnit *pUnit, const SPathPlace &attackPlace, int nArg );   // @0x004394b0
	virtual void MakeDecision();                // @0x00439810
	virtual bool CanSkip() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGuardLogic / CAIRetreatLogic / CAIAfterCombatLogic - same engine, different repertoire+rules.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGuardLogic - hold + engage from within a CUnitArea: the FULL release repertoire (18 actions, no
// TerrorPK and no enemyPlaceSource - the attack logic's set minus TerrorPK, with Melee on the attack source)
// over an area-gated attack place source (CreateAttackPlaceSource(unit,pArea)), so the unit only fires from /
// advances to places inside the guarded area. ctor @0x0044e430, MakeDecision @0x0044ed00 (a DIFFERENT rule
// order from attack - snipe-above-HG, Shoot>Rocket>Grenade, heal IS ruled). Member layout from the release
// PDB (size 240, on CAICombatLogic @0..135): the 2 place sources, then the actions with pArea interleaved
// @192 (between pShootFromHG and the snipe block). operator& @0x50c70 serializes base(2) + the members in
// declaration order (tags 3..23, pArea at 17 as a WEAK CPtr) -- retail-exact since the serialization-
// convergence Wave 2; the CObj members are the same objects as in the base's pChoosePlace.actions, so the
// framework writes them by ref-id.
class CAIGuardLogic: public CAICombatLogic
{
	OBJECT_BASIC_METHODS( CAIGuardLogic );
	ZDATA
	CObj<IAIActionPlaceSource>      attackPlaceSource;   // @136
	CObj<IAIActionPlaceSource>      currentPlaceSource;  // @140
	CObj<CAIShootAction>            pShoot;              // @144 -> attackPlaceSource
	CObj<CAIThrowGrenadeAction>     pThrowGrenade;       // @148 -> attackPlaceSource
	CObj<CAILaunchRocketAction>     pLaunchRocket;       // @152 -> attackPlaceSource
	CObj<CAIReloadAction>           pReload;             // @156 -> currentPlaceSource
	CObj<CAIThrowKnifeAction>       pThrowKnife;         // @160 -> attackPlaceSource
	CObj<CAIMeleeAction>            pMelee;              // @164 -> attackPlaceSource (NO separate enemy source)
	CObj<CAILootAction>             pLoot;               // @168 -> currentPlaceSource
	CObj<CAIHealAction>             pHeal;               // @172 -> currentPlaceSource
	CObj<CAIMoveToEnemyAction>      pGetRidOfInactive;   // @176 -> currentPlaceSource
	CObj<CAIDockWithHGAction>       pDockWithHG;         // @180 -> currentPlaceSource
	CObj<CAIUndockFromHGAction>     pUndockFromHG;       // @184 -> currentPlaceSource
	CObj<CAIShootFromHGAction>      pShootFromHG;        // @188 -> currentPlaceSource
	CPtr<CUnitArea>                 pArea;               // @192  the guarded area (WEAK -- the guard REACTION owns it via CObj; retail operator& @0x50c70 tag 17 is CPtr)
	CObj<CAIBeginSnipeAction>       pBeginSnipe;         // @196 -> currentPlaceSource
	CObj<CAICollectSnipeAPAction>   pCollectSnipeAP;     // @200 -> currentPlaceSource
	CObj<CAISnipeShotAction>        pSnipeShot;          // @204 -> currentPlaceSource
	CObj<CAICancelSnipeAction>      pCancelSnipe;        // @208 -> currentPlaceSource
	CObj<CAIWearPKAction>           pWearPK;             // @212 -> currentPlaceSource
	CObj<CAILeavePKAction>          pLeavePK;            // @216 -> currentPlaceSource
	ZEND int operator&( CStructureSaver &f );
public:
	CAIGuardLogic() {}
	CAIGuardLogic( IAIUnit *pUnit, CUnitArea *pArea );
	virtual void MakeDecision();                // @0x0044ed00
	virtual bool CanSkip() const;
};
// CAIRetreatLogic - fall back toward `pos` (a fear position away from the enemy) while shooting
// opportunistically: a to-place source aimed at `pos` + the full attack repertoire + the attack decision.
// ctor @0x00494590, MakeDecision @0x00494a10.
class CAIRetreatLogic: public CAICombatLogic
{
	OBJECT_BASIC_METHODS( CAIRetreatLogic );
	ZDATA
	SPathPlace                      pos;          // retreat target
	CObj<IAIActionPlaceSource>      retreatPlaceSource;
	CObj<CAIShootAction>            pShoot;
	CObj<CAIThrowGrenadeAction>     pThrowGrenade;
	CObj<CAILaunchRocketAction>     pLaunchRocket;
	CObj<CAIReloadAction>           pReload;
	CObj<CAIThrowKnifeAction>       pThrowKnife;
	CObj<CAIMeleeAction>            pMelee;
	CObj<CAIMoveToEnemyAction>      pMove;
	ZEND int operator&( CStructureSaver &f );
public:
	CAIRetreatLogic() {}
	CAIRetreatLogic( IAIUnit *pUnit, const SPathPlace &pos );
	virtual void MakeDecision();                // @0x00494a10
	virtual bool CanSkip() const;
};
// CAIAfterCombatLogic - regroup once the enemy is gone: reload / loot / heal / advance, off the current
// place. ctor @0x00414150, MakeDecision @0x00414440.
class CAIAfterCombatLogic: public CAICombatLogic
{
	OBJECT_BASIC_METHODS( CAIAfterCombatLogic );
	ZDATA
	// regOnNewTurn @136: fires OnNewTurn(CEventOnNewPlayerTurn) each player turn (NGlobal global event; the
	// infra IS present in-tree -- cf. CUnitServer::registerOnNewPlayerTurnOrTime). NOT serialized (a runtime
	// subscription -- like CUnitServer's; not in operator&).
	NGlobal::CEventRegister< CAIAfterCombatLogic, NWorld::CEventOnNewPlayerTurn > regOnNewTurn;
	bool                            bAPUpdated;  // @160 set once the unit's player gets a fresh turn (or in real time) -> idle may finish the logic
	CObj<IAIActionPlaceSource>      currentPlaceSource;  // @164
	CObj<CAIReloadAction>           pReload;             // @168
	CObj<CAILootAction>             pLoot;               // @172
	CObj<CAIHealAction>             pHeal;               // @176
	CObj<CAIMoveToEnemyAction>      pGetRidOfInactive;   // @180
	bool                            bLoot;               // @184 whether the loot rule is considered
	ZEND int operator&( CStructureSaver &f );
public:
	// both ctors must init regOnNewTurn (CEventRegister has no default ctor -- it ASSERTs) -- the default
	// ctor is the save/load registry's create path (ConstructAfterCombatLogic @0x14050 also registers).
	CAIAfterCombatLogic(): regOnNewTurn( this, &CAIAfterCombatLogic::OnNewTurn ), bAPUpdated( false ), bLoot( false ) {}
	CAIAfterCombatLogic( IAIUnit *pUnit, bool bLoot );
	// the event handler @0x14000 (NOT the base virtual OnNewTurn() -- the release does not override that one;
	// declaring only this overload hides the inherited no-arg name so &CAIAfterCombatLogic::OnNewTurn is
	// unambiguous, while the inherited CAICombatLogic::OnNewTurn() still fills vtable slot 12).
	void OnNewTurn( const NWorld::CEventOnNewPlayerTurn &event );
	virtual void MakeDecision();                // @0x00414440
	virtual bool CanSkip() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Factories + predicates (verified addresses).
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIAttackLogic( IAIUnit *pUnit );                                         // @0x00423260
IAILogic* CreateAIDefenceLogic( IAIUnit *pUnit, const SPathPlace &attackPlace, int nArg ); // @0x00439780
IAILogic* CreateAIAfterCombatLogic( IAIUnit *pUnit, bool bLoot );                        // @0x00414a50
IAILogic* CreateAIRetreatLogic( IAIUnit *pUnit, const SPathPlace &pos );                 // @0x00494990
IAILogic* CreateAIGuardLogic( IAIUnit *pUnit, CUnitArea *pArea );                        // @0x004506f0
bool      IsAttackLogic( IAILogic *pLogic );                                             // @0x00420e70
bool      IsDefenceLogic( IAILogic *pLogic );                                            // @0x00439480
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AICOMBATLOGIC_H_
