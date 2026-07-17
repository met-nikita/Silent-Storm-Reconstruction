#include "StdAfx.h"
//
#include "aiReactions.h"
#include "aiUnit.h"          // IAIUnit
#include "aiUnitState.h"     // SAIUnitState (the per-unit threat tracker)
#include "aiState.h"         // SAIState
#include "aiLogic.h"         // IAILogic
#include "aiCombatLogic.h"   // CreateAIAttackLogic / CreateAIAfterCombatLogic / CreateAIRetreatLogic / CreateAIGuardLogic / IsAttackLogic
#include "aiDefenceReaction.h"   // CanUseDefenceReaction / CreateAIDefenceReaction (the flag-gated Defence branch)
#include "aiGuardReaction.h"     // CreateAIGuardReaction (the cornered-while-scared stand-and-watch reaction)
#include "aiAssassinReaction.h"  // CanUseAssassinReaction / CreateAIAssassinReaction (the hide-and-creep sneak-attack reaction)
#include "aiActionPlaceSource.h" // CUnitArea (the guard area)
#include "aiPosition.h"      // IPathNetwork, SUnitPosition, SPathPlace
#include "aiActionBase.h"    // NAI::SPlaceWithAP (complete) -- before aiMoveAction.h (C2036 guard)
#include "aiMoveAction.h"    // NAI::GetUnitPos (the reconciled CAIRetreatReaction::Update)
#include "aiRouteLogic.h"    // NAI::CreateAILookToPositionLogic / CreateAIMoveToPositionLogic / CreateAIAlarmLogic / CAIRouteLogic
#include "aiPlayer.h"        // NAI::IAIPlayer::GetUnits (squad-alarm ally count)
#include "aiEvent.h"         // NAI::CreateAIEnemyDiedEvent / IAIEvent (squad-alarm: drop own enemy)
#include "wUnitServer.h"     // NWorld::CUnitServer
#include "wMain.h"           // NWorld::CWorld::GetPathNetwork
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Concrete reaction bodies. CAINormalReaction::Update is the core of the release @0x0047f190: enemy ->
// Attack, combat-over -> AfterCombat, and (now) scared -> hand off to a Retreat reaction. The release's
// other escalations there (Defence/Assassin/Guard, the event hooks) still need the AI event system and are
// omitted; bScared comes from the reconstructed SAIUnitState threat tracker.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// A fall-back place away from the enemy. The release GetFearPosition @0x004a0b20 picks a random place in
// the direction away from the enemies' average position (GetAvgPos + GetPlacesAtDirection, not ported);
// reconstructed here as the passable place farthest from the enemy among those ~8 units back along the
// unit->enemy axis.
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool GetFearPosition( IAIUnit *pUnit, IAIUnit *pEnemy, SPathPlace *pOut )
{
	if ( !IsValid( pUnit ) || !IsValid( pEnemy ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) || !IsValid( pUS->GetWorld() ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	CVec3 me = pUnit->GetPosition().GetCP();
	CVec3 foe = pEnemy->GetPosition().GetCP();
	CVec3 away = me - foe;
	float len = fabs( away );
	if ( len < 0.1f )
		return false;
	CVec3 target = me + away * ( 8.0f / len );   // ~8 units back from the enemy
	SSphere s;
	s.ptCenter = target;
	s.fRadius = 5.0f;
	vector<SPathPlace> places;
	pNet->GetNearPlaces( s, &places );
	SUnitPosition probe( pUnit->GetUnitPosition() );
	float fBest = -1.0f;
	bool bFound = false;
	for ( int k = 0; k < (int)places.size(); ++k )
	{
		if ( !pNet->IsNativePassable( places[k] ) )
			continue;
		probe.pos.p = places[k];
		float d = fabs( probe.GetCP() - foe );   // farthest-from-enemy passable place
		if ( d > fBest )
		{
			fBest = d;
			*pOut = places[k];
			bFound = true;
		}
	}
	return bFound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (Wireable-surface note: CAIDefenceReaction is the ONLY landed route reaction/logic whose install site
// exists in the dev tree. The other route logics -- CheckPosition/LookRound/Roaming/Hide -- are installed
// only by the still-absent CAIGuardReaction/CAIFearReaction/CAIAssassinReaction; LookTo/MoveTo would go via
// the present CAIRetreatReaction but that is a separate behaviour reconciliation. See
// docs/CONVERGENCE_PROGRESS.md session 19.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CountActiveAllies (release GetUnits(allies, active).size()): the alarming unit's garrison size -- how many fightable
// side-units it could rally. The ally AI-player roster substitutes the absent SAIState::GetUnits.
static int CountActiveAllies( IAIUnit *u )
{
	if ( !IsValid( u ) )
		return 0;
	SAIState *pSt = u->GetAIState();
	if ( pSt == 0 )
		return 0;
	IAIPlayer *p = pSt->GetAllyAIPlayer();
	if ( p == 0 )
		return 0;
	vector< CPtr<IAIUnit> > &units = *p->GetUnits();
	int n = 0;
	for ( int i = 0; i < (int)units.size(); ++i )
	{
		IAIUnit *a = units[i].GetPtr();
		if ( IsValid( a ) && !a->IsDead() && IsValid( a->GetUnitServer() ) )
			++n;
	}
	return n;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAINormalReaction::Update()
{
	IAIUnit *u = GetUnit();
	if ( !IsValid( u ) )
		return;
	// (A dev-only "let a mid-run route finish" top guard lived here; REMOVED for retail parity @0x47f190.
	// Its rationale -- the poll-Populate re-deriving the enemy EVERY think and re-rolling the squad-alarm
	// route each time -- died with the poll (item-7 event-driven state: modified now fires only on real
	// changes). Retail has no such guard, and keeping it would shield the new HUNT route below from
	// re-decisions: a unit that RE-SPOTS its enemy mid-hunt must switch to the attack logic immediately,
	// not walk out the stale investigation first.)
	// the per-unit threat tracker (SAIUnitState, populated by the commander before Update) supplies the
	// current enemy + the bScared flag. A live pEnemy means combat.
	SAIUnitState *us = GetAIUnitState();   // plain struct ptr - not a CObjectBase, so no IsValid()
	IAIUnit *pEnemy = ( us != 0 ) ? us->pEnemy.GetPtr() : 0;
	// scared (outnumbered + no support) with a threat present -> fall back. Hand the unit to a Retreat
	// reaction (release scared branch: CreateAIRetreatReaction). Run it now so the retreat logic is set
	// this think.
	if ( us != 0 && us->bScared && IsValid( pEnemy ) )
	{
		// SQUAD ALARM (release @0x47f241): an outnumbered-but-supported scared unit may break off and RUN to a nearby
		// ally to raise the garrison (2+ active side-units && 40% roll) instead of merely retreating. It drops its own
		// enemy (CreateAIEnemyDiedEvent -> RemoveEnemy) so it disengages and runs the alarm route; the route's
		// CTaskCommandAlarm then alerts every ally within 5 m on arrival. The Update-top guard lets the route finish.
		if ( CountActiveAllies( u ) > 1 && random.Get( 100 ) < 40 )   // release @0x47f254: raw Get()%100 < 0x28 (40%)
		{
			if ( IAILogic *pAlarm = CreateAIAlarmLogic( u, pEnemy ) )
			{
				SetLogic( pAlarm );
				CObj<IAIEvent> e = CreateAIEnemyDiedEvent( pEnemy );
				if ( IsValid( e ) )
					e->Modify( us );   // drop own enemy -> disengage and run the alarm route
				return;
			}
		}
		SPathPlace fearPos;
		if ( GetFearPosition( u, pEnemy, &fearPos ) )
		{
			// Hold the new reaction in a CPtr across the inline Update: the reconciled
			// CAIRetreatReaction::Update may SetReaction(Guard) on arrival, which releases the unit's pReaction
			// ref -- the CPtr keeps `this` alive through the rest of that Update, exactly as the tactical
			// commander's CPtr<CAIReaction> does each think (aiTacticalCommander.cpp:120/126). Without the hold
			// the raw pointer would dangle mid-Update.
			CPtr<CAIReaction> pRetreat = CreateAIRetreatReaction( u, fearPos );
			u->SetReaction( pRetreat );
			if ( IsValid( pRetreat ) )
				pRetreat->Update();
			return;
		}
		// Cornered: nowhere to fall back. Dig in via the guard REACTION (release @0x47f2f2:
		// CreateAIGuardReaction(u, /*anim*/0, /*nRadius*/12) -> u->SetReaction) -- stand-and-watch the local
		// area + escalate to Defence, rather than the bare guard LOGIC. The reaction floods its own CUnitArea
		// lazily (radius 12) and tracks bWasCombat itself; SetReaction + return, no inline-Update (the commander
		// Updates the new reaction next think). LIVE (build-validation scope: build-verified, NOT runtime-
		// validated -- the game is not runtime-testable; this changes in-mission AI for cornered scared units,
		// matching the release order @0x47f190). One-line revert: restore the bare SetLogic( CreateAIGuardLogic(
		// u, new CUnitArea( u->GetUnitServer(), u->GetPosition().p, 30, 0 ) ) ) install + bWasCombat = true.
		if ( IsValid( u->GetUnitServer() ) )
		{
			u->SetReaction( CreateAIGuardReaction( u, 0, 12 ) );
			return;
		}
		// no unit server (shouldn't happen): hold and fight.
	}
	// Release @0x47f190 possible-enemy preference (computed before the Defence rung, consumed after
	// Assassin): a merely SUSPECTED enemy (pPossibleEnemy -- lost from sight / heard / shot from
	// concealment) is worth hunting when there is no live enemy at all, or when the suspect is closer
	// than a THIRD of the live enemy's distance (@0x47f3xx: dist(me,enemy) * 0.33333 <= dist(me,suspect)
	// -> prefer the enemy). A unit whose own place is the invalid sentinel (top two place bits set,
	// the same 0xC0000000 test as aiDefenceReaction's IsValidPlace) never hunts.
	IAIUnit *pPossible = ( us != 0 ) ? us->pPossibleEnemy.GetPtr() : 0;
	bool bCheckPossible = IsValid( pPossible );
	if ( bCheckPossible && IsValid( pEnemy ) )
	{
		CVec3 me = u->GetPosition().GetCP();
		float fEnemyDist = fabs( pEnemy->GetPosition().GetCP() - me );
		float fSuspectDist = fabs( pPossible->GetPosition().GetCP() - me );
		if ( fEnemyDist * ( 1.0f / 3.0f ) <= fSuspectDist )
			bCheckPossible = false;
	}
	const bool bOwnPlaceInvalid =
		( (unsigned)u->GetUnitPosition().pos.p.GetData() & 0xc0000000u ) == 0xc0000000u;   // @0x47f47a, [esp+0x13]
	if ( bCheckPossible && bOwnPlaceInvalid )
		bCheckPossible = false;
	// Release escalation rung (after the scared early-returns, before the attack block -- matches @0x47f190):
	// an armed unit near usable cover takes cover and pops out rather than charging. Pass `this` (the current
	// Normal reaction) as the fall-back the Defence reaction reverts to -- it is held in the Defence reaction's
	// CObj<CAIReaction> pPrevReaction, so the SetReaction below won't delete it (lifetime-safe). Match the
	// release: SetReaction + return, do NOT inline-Update (the ctor plans; the commander Updates the new
	// reaction next think). LIVE (build-validation scope: build-verified, NOT runtime-validated -- the game is
	// not runtime-testable; this changes in-mission AI for armed units near cover).
	if ( CanUseDefenceReaction( u ) )
	{
		u->SetReaction( CreateAIDefenceReaction( u, this ) );
		return;
	}
	// Release escalation rung @0x47f4d7 (immediately after Defence, before the attack block): a fight-capable,
	// not-yet-engaged unit with a pistol/SMG and a live enemy more than 5 units away that passes the difficulty
	// assassin-chance roll switches to the hide-and-creep sneak attack. Unlike Defence, the Assassin reaction
	// keeps NO fall-back prev-reaction (it gives up via a fresh CAINormalReaction), so the factory takes only
	// (u) -- bUnitAlive defaults true. SetReaction + return, no inline-Update (the ctor leaves bJustStarted set;
	// the commander Updates the new reaction next think). LIVE (build-validation scope: build-verified, NOT
	// runtime-validated -- the game is not runtime-testable; this changes in-mission AI for armed units with a
	// short arm + a distant live enemy, matching the release order @0x47f190). One-line revert: delete this rung.
	if ( CanUseAssassinReaction( u ) )
	{
		u->SetReaction( CreateAIAssassinReaction( u ) );
		return;
	}
	// Release @0x47f190 HUNT rung (immediately after Assassin, before the attack block) -- THE rung that
	// keeps retail AI aggressive across line-of-sight breaks: go CHECK the suspect's last-known position
	// at a RUN (CreateAICheckForEnemyLogic @0x9a710 -- the RouteAddRoundUp flanking script + 6s wait; the
	// pose constant 3 == RUN, the 4th arg is the suspect for the IsAudible silent-approach query). On a
	// successful install the update is DONE (retail skips the attack/after-combat tail); a factory miss
	// (no reachable round-up) falls through to the normal ladder. Without this rung a unit whose enemy
	// ducked out of sight (demoted to pPossibleEnemy by the lost-from-sight event / begin-turn
	// PrepareEnemies sweep) dropped straight to AfterCombat -> SetLogic(0) and stood around doing nothing.
	if ( bCheckPossible )
	{
		CObj<IAILogic> pHunt = CreateAICheckForEnemyLogic( u, pPossible->GetUnitPosition(), RUN, pPossible );
		if ( IsValid( pHunt ) )
		{
			SetLogic( pHunt.GetPtr() );
			return;
		}
	}
	if ( IsValid( pEnemy ) )
	{
		// Keep the existing attack logic across turns (release: if IsAttackLogic, don't rebuild). The
		// commander re-Think()s + re-Add()s the logic-job each think; CAICombatLogic::Think now re-arms the
		// CAIJob finished-state, so the reused job runs again instead of being skipped as already-finished.
		bool bNew = !IsAttackLogic( u->GetLogic() );
		if ( bNew )
			SetLogic( CreateAIAttackLogic( u ) );
		bWasCombat = true;
		return;   // release @0x47f635->0x47f6f3: enemy-valid skips the ally rung + tail
	}
	// Release ALLY-ASSIST rung @0x47f5d3: no live enemy, but an ally called for help (us->pAlly is
	// populated ONLY by CAIAllyNeedHelpEvent now -- event-only allies semantics, see aiUnitState.cpp
	// Populate) -> run to the ally's position with the same CheckForEnemy round-up logic (pose 3 == RUN,
	// NULL suspect @0x47f683 -> CreateAICheckForEnemyLogic @0x9a710) and consume the help call
	// (retail @0x47f6bf AddEvent(CreateAILostAllyEvent) -> Modify == RemoveAlly, clears pAlly). On a
	// handled assist the after-combat/idle tail is skipped and bWasCombat is PRESERVED (@0x47f6ca).
	IAIUnit *pAlly = ( us != 0 ) ? us->pAlly.GetPtr() : 0;
	if ( IsValid( pAlly ) && !bOwnPlaceInvalid )
	{
		CObj<IAILogic> pAssist = CreateAICheckForEnemyLogic( u, pAlly->GetUnitPosition(), RUN, 0 );
		if ( IsValid( pAssist ) )
		{
			SetLogic( pAssist.GetPtr() );
			CObj<IAIEvent> e = CreateAILostAllyEvent( pAlly );   // consume: RemoveAlly + clear pAlly
			if ( IsValid( e ) )
				e->Modify( us );
			return;
		}
	}
	if ( bWasCombat )
	{
		SetLogic( CreateAIAfterCombatLogic( u, false ) );   // regroup once combat is over (loot off: stub body)
		bWasCombat = false;
	}
	else
		SetLogic( 0 );   // a unit that has never fought and sees no enemy stays idle
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRetreatReaction::Update @0x00495ad0 -- the reaction of a unit falling back to `pos` (a fear position).
// Reconciled to the matched-release decode (oracle: decomp/src/s2_airetreatreaction.h, disasm-verified
// @0x495ad0). This SUPERSEDES the dev predecessor (which merely kept retreating while scared and reverted to a
// Normal reaction once safe): the release reads NO bScared flag and has NO Normal-revert -- on reaching the
// fall-back it becomes a Guard reaction; otherwise it keeps retreating from a live enemy, glances toward a
// merely suspected one, and (whenever it holds no current logic) keeps moving to the fall-back at a run.
//
// ELIDED (build-validation scope, as the other reactions):
//  * the opening pU->SetRoute(NULL) route-clear (release IAIUnit vtbl+0x5c == NAI::CAIUnit::SetRoute; the decode
//    hook mislabels it "pfnUnitNotify" -- it is NOT the +0x48 Notify event slot). IAIUnit has no SetRoute slot;
//    SetLogic replaces the unit's logic in place, so the landed reactions drop it consistently.
//  * the suspected-enemy AddEvent(CreateAILostPossibleEnemyEvent) (release vtbl+0x48 == CAIUnit::Notify, the
//    per-unit AI event system -- unported; SAIUnitState::Populate() re-polls each think, so behaviour-safe).
//  * the path network is taken from the unit SERVER's world (as Guard/Defence do) rather than the AI state's
//    pWorld -- the same NWorld::IWorld for an in-world unit; the release's GetAIState()!=NULL gate is folded
//    into the GetAIUnitState()!=NULL gate (both non-null for a live in-world unit).
//
// LIFETIME: the dist<0.5 arm SetReaction()s a Guard and then the tail still runs (matching the release, which
// reads `this` after the SetReaction). That is safe because every Update() caller holds the reaction in a
// CPtr<CAIReaction> across the call -- the tactical commander (aiTacticalCommander.cpp:120/126) and the
// hardened inline path in CAINormalReaction::Update above -- so SetReaction releasing the unit's ref does not
// free `this` mid-Update. LIVE (build-validation scope: build-verified, NOT runtime-validated; this changes
// in-mission AI for scared units that reach their fall-back or lose their enemy).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIRetreatReaction::Update()
{
	IAIUnit *u = GetUnit();
	if ( !IsValid( u ) )
		return;
	// retail SetRoute(NULL) RESTORED (release IAIUnit vtbl+0x5c @0xadb90 -- possible now that the route
	// slot exists): the retreating unit abandons its route for good.
	u->SetRouteLogic( 0 );
	SAIUnitState *us = GetAIUnitState();   // plain struct ptr -- not a CObjectBase, so no IsValid()
	if ( us == 0 )
		return;
	NWorld::CUnitServer *pUS = u->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return;
	// distance from the unit to the fall-back place
	CVec3 cpTarget = GetUnitPos( pos, pNet ).GetCP();
	CVec3 cpUnit = u->GetUnitPosition().GetCP();
	float fDist = fabs( cpTarget - cpUnit );
	if ( fDist < 0.5f )   // reached the fall-back (const @0x8b19ec) -> stand and watch the local area
	{
		u->SetReaction( CreateAIGuardReaction( u, 0, 8 ) );
	}
	else
	{
		IAIUnit *pEnemy = us->pEnemy.GetPtr();
		IAIUnit *pPossible = us->pPossibleEnemy.GetPtr();
		if ( IsValid( pEnemy ) )
		{
			// a live enemy: keep falling back toward the place.
			SetLogic( CreateAIRetreatLogic( u, pos ) );
		}
		else if ( IsValid( pPossible ) )
		{
			// only a suspected contact: glance toward its place from where we are.
			if ( IsValid( pPossible->GetUnitServer() ) )
			{
				SPathPlace p = pPossible->GetUnitPosition().pos.p;
				if ( SetLogic( CreateAILookToPositionLogic( u, p ) ) )
				{
					us->RemovePossibleEnemy( pPossible );   // RE-ENABLED (retail raised CreateAILostPossibleEnemyEvent here):
						// consume the suspect once we glance at its place, so a retreating unit resumes its fall-back instead of
						// freezing to stare at a hidden shooter forever (the now-preserved possibleEnemies must be consumed by
						// every suspect-acting reaction -- Guard/Fear/Retreat -- to stay bounded).
				}
			}
		}
	}
	// tail: a unit left with no current logic keeps moving to the fall-back at a run, ending crouched.
	if ( IsValid( u ) && !IsValid( u->GetLogic() ) )
		SetLogic( CreateAIMoveToPositionLogic( u, pos, RUN, CROUCH, true ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIRetreatReaction( IAIUnit *pUnit, const SPathPlace &pos )
{
	return IsValid( pUnit ) ? new CAIRetreatReaction( pUnit, pos ) : 0;
}
// @0x0043b780 / @0x0007efe0 -- the null reaction and the default reaction factories (the release
// 2nd `bUnitAlive` arg is folded into IsValid here, like CreateAIRetreatReaction).
CAIReaction* CreateAIEmptyReaction( IAIUnit *pUnit )
{
	return IsValid( pUnit ) ? new CAIEmptyReaction( pUnit ) : 0;
}
CAIReaction* CreateAINormalReaction( IAIUnit *pUnit )
{
	return IsValid( pUnit ) ? new CAINormalReaction( pUnit ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51253120, CAIEmptyReaction )
REGISTER_SAVELOAD_CLASS( 0x51943160, CAINormalReaction )
REGISTER_SAVELOAD_CLASS( 0x52443150, CAIRetreatReaction )
