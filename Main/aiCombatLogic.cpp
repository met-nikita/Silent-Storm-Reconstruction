#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiGrid.h"           // IsSamePlace
#include "AILog.h"            // dev records (CAILogPosition/CAILogSpendAP)
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"   // NWorld::CCmd, CCmdEmpty, CCommand
#include "eventPlayer.h"     // NWorld::CEventOnNewPlayerTurn (CAIAfterCombatLogic::OnNewTurn handler)
//
#include "aiCombatLogic.h"
#include "aiActions.h"
#include "aiMoveAction.h"     // NAI::GetUnitPos (retreat arrival gate); aiActionBase.h already in via aiCombatLogic.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - engine + concrete-logic bodies (structural port). Reconstructed from
// reconstruction/exports/{engine.c, defence.c, decisions.c}. WIP - NOT yet in Main.vcxproj.
//
// FAITHFUL: DoAction, GenerateCommand, AddPlaceSource, the ctors' place-source/action wiring (defence.c),
// the engine flow (DoJob pipeline: Prepare sources -> run chooser -> MakeDecision -> finish).
// DEEP CONTENT (deferred to build-settle, sign-lists in decisions.c): each MakeDecision's exact CRule/CSign
// thresholds. The MakeDecisions below build the real CDecision<CAIAction*> over the right actions and pick
// the best; the per-rule sign sets are the tactical tuning to transcribe from decisions.c.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// helper: the unit-area an attacker searches (release derives it from the unit's mission). Build-settle.
static CUnitArea* GetUnitArea( IAIUnit *pUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICombatLogic engine
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICombatLogic::CAICombatLogic( IAIUnit *pUnit, IAIChoosePlaceJob *_pChoosePlace ):
	CAILogic( pUnit ), CAIJob( 0 ), pChoosePlace( _pChoosePlace ), prepareState( PS_FINISHED ), nUnitLastAP( 0 )
{
	pLog = CreateAILog();
	// regOnNewTurn: register OnNewTurn on the world's pass-control event (build-settle: NGlobal event hook).
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAICombatLogic::operator&( CStructureSaver &f )
{ f.Add( 2, (CAILogic*)this ); f.Add( 3, (CAIJob*)this ); f.Add( 4, &pLog ); f.Add( 5, &placeSources ); f.Add( 6, &pChoosePlace ); f.Add( 7, &prepareState ); f.Add( 8, &nPSToPrepare ); f.Add( 9, &nUnitLastAP ); return 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Dedup-append a place source. @0x004335b0
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIActionPlaceSource* CAICombatLogic::AddPlaceSource( IAIActionPlaceSource *pSrc )
{
	if ( !IsValid( pSrc ) )
		return 0;
	for ( int i = 0; i < (int)placeSources.size(); ++i )
		if ( placeSources[i] == pSrc )
			return pSrc;
	placeSources.push_back( pSrc );
	return pSrc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Move the unit to the action's chosen place (logging AP + position), then run the action. @0x00432900
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICombatLogic::DoAction( CAIAction *pAction )
{
	IAIUnit *pUnit = GetUnit();
	if ( !IsValid( pUnit ) || !IsValid( pAction ) || !IsValid( pLog ) )
		return;
	SPlaceWithAP place;
	if ( pChoosePlace->GetPlaceForAction( pAction, &place ) )
	{
		int nMoveAP = pUnit->GetAP() - place.nUnitAP;
		*pLog << new CAILogSpendAP( pUnit, nMoveAP );
		if ( !( place.place.pos.p == pUnit->GetPosition().p ) )   // dev SPathPlace::operator== (no ISamePlace)
			*pLog << new CAILogPosition( pUnit, pUnit->GetPosition(), place.place.pos, place.place.GetPose() );
	}
	pAction->Do( pLog );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Drain the chosen action's logged records into world commands. @0x004334b0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICombatLogic::GenerateCommand()
{
	list< CPtr<NWorld::CCommand> > cmds;
	pLog->GetWorldCommands( &cmds );
	for ( list< CPtr<NWorld::CCommand> >::iterator i = cmds.begin(); i != cmds.end(); ++i )
		DoCommand( *i );   // CAILogic::commands is now list<CPtr<CCommand>> - the log's world commands flow straight through
	pLog->Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Per-segment pipeline (state machine over prepareState): prepare place sources -> run the choose-place
// job -> MakeDecision -> finish. @0x00432aa0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICombatLogic::DoJob()
{
	if ( IsJobFinished() )
		return;
	switch ( prepareState )
	{
	case PS_FINDING_PLACES:
		// (release: snapshot the unit's reachable moves) then advance
		prepareState = PS_CHOOSING_PLACE;
		break;
	case PS_CHOOSING_PLACE:
		// prepare each place source's candidate places
		for ( int i = 0; i < (int)placeSources.size(); ++i )
			placeSources[i]->Prepare();
		prepareState = PS_DECIDING;
		break;
	case PS_DECIDING:
		// run the choose-place job to completion (it picks the best place per action), then decide
		if ( IsValid( pChoosePlace ) )
		{
			pChoosePlace->Reset();
			while ( !pChoosePlace->IsJobFinished() )
				pChoosePlace->DoJob();
		}
		MakeDecision();
		prepareState = PS_FINISHED;
		CAIJob::Finish();
		break;
	default:
		CAIJob::Finish();
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICombatLogic::Think()           { prepareState = PS_FINDING_PLACES; bFinished = false; CAIJob::ReArm(); } // @0x00432c20 (release zeroes prepareState + the CAIJob finished-state so a reused logic re-runs)
bool CAICombatLogic::IsThinking()      { return prepareState != PS_FINISHED; }                     // @0x00432720
bool CAICombatLogic::IsNeedToThink()   { return IsValid( GetUnit() ) && GetUnit()->GetAP() > 0 && !bFinished; } // @0x00432f70
void CAICombatLogic::StopThinking()    { prepareState = PS_FINISHED; }
bool CAICombatLogic::IsFinished()      { return bFinished; }
bool CAICombatLogic::IsEndOfTurn()     { return bFinished || ( IsValid( GetUnit() ) && GetUnit()->GetAP() <= 0 ); } // @0x00433010
void CAICombatLogic::OnNewTurn()       { prepareState = PS_FINISHED; bFinished = false; }           // @0x00432820
////////////////////////////////////////////////////////////////////////////////////////////////////
static CUnitArea* GetUnitArea( IAIUnit * ) { return 0; /* build-settle: derive from unit mission */ }
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIAttackLogic - full repertoire (3 place sources + 19 actions). ctor @0x00420f30
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIAttackLogic::CAIAttackLogic( IAIUnit *pUnit ):
	CAICombatLogic( pUnit, CreateAIChoosePlaceForAttackJob( 0, 0 ) )
{
	IAIUnit *u = GetUnit();
	attackPlaceSource  = AddPlaceSource( CreateAttackPlaceSource( u, GetUnitArea( u ) ) );
	currentPlaceSource = AddPlaceSource( CreateCurrentPlaceSource( u ) );
	enemyPlaceSource   = AddPlaceSource( CreateNearEnemyPlaceSource( u ) );
	pShoot            = AddAction( new CAIShootAction( u ),        attackPlaceSource );
	pThrowGrenade     = AddAction( new CAIThrowGrenadeAction( u ), attackPlaceSource );
	pLaunchRocket     = AddAction( new CAILaunchRocketAction( u ), attackPlaceSource );
	pReload           = AddAction( new CAIReloadAction( u ),       currentPlaceSource );
	pThrowKnife       = AddAction( new CAIThrowKnifeAction( u ),   attackPlaceSource );
	pMelee            = AddAction( new CAIMeleeAction( u ),        enemyPlaceSource );
	pLoot             = AddAction( new CAILootAction( u ),         currentPlaceSource );
	pHeal             = AddAction( new CAIHealAction( u ),         currentPlaceSource );
	pGetRidOfInactive = AddAction( new CAIMoveToEnemyAction( u ),  currentPlaceSource );
	pBeginSnipe       = AddAction( new CAIBeginSnipeAction( u ),   currentPlaceSource );
	pCollectSnipeAP   = AddAction( new CAICollectSnipeAPAction( u ),currentPlaceSource );
	pSnipeShot        = AddAction( new CAISnipeShotAction( u ),    currentPlaceSource );
	pCancelSnipe      = AddAction( new CAICancelSnipeAction( u ),  currentPlaceSource );
	pDockWithHG       = AddAction( new CAIDockWithHGAction( u ),   currentPlaceSource );
	pUndockFromHG     = AddAction( new CAIUndockFromHGAction( u ), currentPlaceSource );
	pShootFromHG      = AddAction( new CAIShootFromHGAction( u ),  currentPlaceSource );
	pTerrorPK         = AddAction( new CAITerrorPKAction( u ),     currentPlaceSource );
	pWearPK           = AddAction( new CAIWearPKAction( u ),       currentPlaceSource );
	pLeavePK          = AddAction( new CAILeavePKAction( u ),      currentPlaceSource );
}
// @0x00420ea0 - RELEASE-RECONCILED: skip the turn only when out of useful AP. A fight-capable unit with
// >5 AP keeps acting; a unit that cannot fight (downed) never skips. Identical to the guard/retreat CanSkip
// (disasm-confirmed @0x20ea0: GetUnitServer valid+not-destroyed, CanFight (server vtbl+0x44), then GetUnit
// valid && AP>5 -> false; constant 5 cmp-confirmed). Was the predecessor `!IsValid(u) || u->GetAP() <= 0`.
bool CAIAttackLogic::CanSkip() const
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( !IsValid( pUS ) || !pUS->CanFight() ) return false;
	IAIUnit *u = GetUnit();
	if ( IsValid( u ) && u->GetAP() > 5 ) return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The attack decision. @0x00421890 - RELEASE-RECONCILED to the full 18-rule ladder.
//
// Every action's GetInfoInner is now live (the PK / heavy-gun / snipe / loot bodies all landed in earlier
// sessions), so the decision rules on ALL 18 do-able actions, not just the 6 with bodies at the time of the
// original structural port. The rule SET, ORDER and sign attachment were recovered from the raw x86 of
// @0x21890 by two independent methods that agreed on every one of the 18 positions:
//   (1) reversed CSign-construction order (Ghidra-faithful: the 18 required signs are built move-first, i.e.
//       in REVERSE priority, leavePK's sign last) -> reversed = the ladder below;
//   (2) the decomp decode (decode_makedecision.py / src/s2_aiattacklogic.h).
// Adversarial raw traces additionally confirmed: heal is GATHERED but NEVER ruled (an ORIGINAL QUIRK - the
// attack logic, unlike guard/after-combat, never chooses to heal); the ThrowGrenade rule carries BOTH
// regular votes (overkill-defer CSign(shoot.bKillTargetCertainly,F,F,F) + bad-group-health
// CSign(grenade.bBadGroupHealth,T,T,T)) while SHOOT is PLAIN (the same release pattern as the sess23/24
// CAIRetreatLogic/CAIGuardLogic reconciles - the predecessor split them onto shoot+grenade); and the tail is
// GetBestAction -> destroyed-bit gate -> DoAction, with NO Finish() and NO move fallback (move/GetRidOfInactive
// is the lowest-priority RULE, not an else branch).
//
// PRIORITY (first AddRule = highest; CDecision::GetBestAction commits to the first rule beating the 0.5^k
// acceptance bar): LeavePK > WearPK > TerrorPK > UndockFromHG > DockWithHG > ShootFromHG > CancelSnipe >
// CollectSnipeAP > SnipeShot > BeginSnipe > LaunchRocket > ThrowGrenade > Shoot > ThrowKnife > Reload >
// Loot > Melee > GetRidOfInactive.
//
// LIVE: this is the only logic the tactical commander builds (CreateAIAttackLogic), so every AI unit now uses
// the full repertoire in combat. BODY-ONLY reconcile - the ctor already wires all 19 actions, so the base's
// serialized pChoosePlace.actions vector is unchanged -> save format identical (build-validation-safe).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIAttackLogic::MakeDecision()
{
	// gather each action's info at its chosen place (heal IS gathered though never ruled -- ORIGINAL QUIRK @0x21890)
	CAIShootAction::SInfo shoot;                 GetInfo( pShoot.GetPtr(), &shoot );
	CAIThrowGrenadeAction::SInfo grenade;        GetInfo( pThrowGrenade.GetPtr(), &grenade );
	CAILaunchRocketAction::SInfo rocket;         GetInfo( pLaunchRocket.GetPtr(), &rocket );
	CAIReloadAction::SInfo reload;               GetInfo( pReload.GetPtr(), &reload );
	CAIThrowKnifeAction::SInfo knife;            GetInfo( pThrowKnife.GetPtr(), &knife );
	CAIMeleeAction::SInfo melee;                 GetInfo( pMelee.GetPtr(), &melee );
	CAILootAction::SInfo loot;                   GetInfo( pLoot.GetPtr(), &loot );
	CAIHealAction::SInfo heal;                   GetInfo( pHeal.GetPtr(), &heal );   // QUIRK: gathered, never ruled
	CAIMoveToEnemyAction::SInfo move;            GetInfo( pGetRidOfInactive.GetPtr(), &move );
	CAIBeginSnipeAction::SInfo beginSnipe;       GetInfo( pBeginSnipe.GetPtr(), &beginSnipe );
	CAICollectSnipeAPAction::SInfo collectSnipe; GetInfo( pCollectSnipeAP.GetPtr(), &collectSnipe );
	CAISnipeShotAction::SInfo snipeShot;         GetInfo( pSnipeShot.GetPtr(), &snipeShot );
	CAICancelSnipeAction::SInfo cancelSnipe;     GetInfo( pCancelSnipe.GetPtr(), &cancelSnipe );
	CAIDockWithHGAction::SInfo dockHG;           GetInfo( pDockWithHG.GetPtr(), &dockHG );
	CAIUndockFromHGAction::SInfo undockHG;       GetInfo( pUndockFromHG.GetPtr(), &undockHG );
	CAIShootFromHGAction::SInfo shootHG;         GetInfo( pShootFromHG.GetPtr(), &shootHG );
	CAITerrorPKAction::SInfo terrorPK;           GetInfo( pTerrorPK.GetPtr(), &terrorPK );
	CAIWearPKAction::SInfo wearPK;               GetInfo( pWearPK.GetPtr(), &wearPK );
	CAILeavePKAction::SInfo leavePK;             GetInfo( pLeavePK.GetPtr(), &leavePK );

	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	// 18 rules in priority order (highest first); each required sign = CSign(&xxx.bCanDo,T,T,T).
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &leavePK.bCanDo, true, true, true ) );      pDecision->AddRule( new CRule<CAIAction*>( pLeavePK.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &wearPK.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pWearPK.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &terrorPK.bCanDo, true, true, true ) );     pDecision->AddRule( new CRule<CAIAction*>( pTerrorPK.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &undockHG.bCanDo, true, true, true ) );     pDecision->AddRule( new CRule<CAIAction*>( pUndockFromHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &dockHG.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pDockWithHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shootHG.bCanDo, true, true, true ) );      pDecision->AddRule( new CRule<CAIAction*>( pShootFromHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &cancelSnipe.bCanDo, true, true, true ) );  pDecision->AddRule( new CRule<CAIAction*>( pCancelSnipe.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &collectSnipe.bCanDo, true, true, true ) ); pDecision->AddRule( new CRule<CAIAction*>( pCollectSnipeAP.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &snipeShot.bCanDo, true, true, true ) );    pDecision->AddRule( new CRule<CAIAction*>( pSnipeShot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &beginSnipe.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pBeginSnipe.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &rocket.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pLaunchRocket.GetPtr(), req, reg ) ); }
	// ThrowGrenade carries BOTH regular votes; the following Shoot rule is PLAIN (required-only).
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &grenade.bCanDo, true, true, true ) );
	  reg.push_back( new CSign<bool>( &shoot.bKillTargetCertainly, false, false, false ) );   // overkill-defer (moved off Shoot)
	  reg.push_back( new CSign<bool>( &grenade.bBadGroupHealth, true, true, true ) );          // save grenades for wounded groups
	  pDecision->AddRule( new CRule<CAIAction*>( pThrowGrenade.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shoot.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pShoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &knife.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pThrowKnife.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &reload.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pReload.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &loot.bCanDo, true, true, true ) );         pDecision->AddRule( new CRule<CAIAction*>( pLoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &melee.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pMelee.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &move.bCanDo, true, true, true ) );         pDecision->AddRule( new CRule<CAIAction*>( pGetRidOfInactive.GetPtr(), req, reg ) ); }
	CAIAction *pBest = pDecision->GetBestAction();
	if ( IsValid( pBest ) )
		DoAction( pBest );
	// no Finish(), no move fallback -- the attack logic never self-ends (base DoJob Finishes the job after MakeDecision)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIDefenceLogic - hold a place (1 OnePlace source + shoot/grenade/rocket/reload). ctor @0x004394b0
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIDefenceLogic::CAIDefenceLogic( IAIUnit *pUnit, const SPathPlace &_attackPlace, int nArg ):
	CAICombatLogic( pUnit, CreateAIChoosePlaceForAttackJob( 0, 0 ) ), attackPlace( _attackPlace )
{
	IAIUnit *u = GetUnit();
	currentPlaceSource = AddPlaceSource( CreateOnePlacePlaceSource( u, attackPlace, false ) );
	pShoot        = AddAction( new CAIShootAction( u ),        currentPlaceSource );
	pThrowGrenade = AddAction( new CAIThrowGrenadeAction( u ), currentPlaceSource );
	pLaunchRocket = AddAction( new CAILaunchRocketAction( u ), currentPlaceSource );
	pReload       = AddAction( new CAIReloadAction( u ),       currentPlaceSource );
}
bool CAIDefenceLogic::CanSkip() const { IAIUnit *u = GetUnit(); return !IsValid( u ) || u->GetAP() <= 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Defence decision (shoot/rocket/grenade/reload, no move fallback -> Finish). @0x00439810.
// FAITHFUL rule set, recovered by static stack-trace of the release MakeDecision (TraceDecision.py ->
// reconstruction/exports/decision_traced.txt). Rule order hi->lo = shoot, rocket, grenade, reload; each
// rule's required sign is CSign(bCanDo); shoot adds a regular CSign(bKillTargetCertainly==false) and
// grenade a regular CSign(bBadGroupHealth==true). Note this logic is not yet on the live path
// (aiTacticalCommander::ChooseLogic only builds CAIAttackLogic), so this is fidelity-only for now.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIDefenceLogic::MakeDecision()
{
	CAIShootAction::SInfo shoot; GetInfo( pShoot.GetPtr(), &shoot );
	CAIThrowGrenadeAction::SInfo grenade; GetInfo( pThrowGrenade.GetPtr(), &grenade );
	CAILaunchRocketAction::SInfo rocket; GetInfo( pLaunchRocket.GetPtr(), &rocket );
	CAIReloadAction::SInfo reload; GetInfo( pReload.GetPtr(), &reload );
	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	// shoot (highest): only fires when it would NOT massively overkill (bKillTargetCertainly==false); an
	// overkill target defers to rocket/grenade. required=[bCanDo], regular=[!bKillTargetCertainly].
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shoot.bCanDo, true, true, true ) );    reg.push_back( new CSign<bool>( &shoot.bKillTargetCertainly, false, false, false ) ); pDecision->AddRule( new CRule<CAIAction*>( pShoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &rocket.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pLaunchRocket.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &grenade.bCanDo, true, true, true ) );  reg.push_back( new CSign<bool>( &grenade.bBadGroupHealth, true, true, true ) ); pDecision->AddRule( new CRule<CAIAction*>( pThrowGrenade.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &reload.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pReload.GetPtr(), req, reg ) ); }
	CAIAction *pBest = pDecision->GetBestAction();
	if ( pBest )
		DoAction( pBest );
	else
		CAIJob::Finish();   // defence holds position
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// operator& for the two instantiated logics. Parent-only like Guard/Retreat/AfterCombat: the action +
// place-source repertoire is deterministically rebuilt by the ctor, so only the engine base state (and
// Defence's commanded place) need persisting. build-settle: if save/load of in-combat logics must
// preserve per-action SActionInfo caches, serialize the CObj members explicitly (tags 3..N).
int CAIAttackLogic::operator&( CStructureSaver &f )  { f.Add( 2, (CAICombatLogic*)this ); return 0; }
int CAIDefenceLogic::operator&( CStructureSaver &f ) { f.Add( 2, (CAICombatLogic*)this ); f.Add( 3, &attackPlace ); return 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Guard / Retreat / AfterCombat - ctors + MakeDecisions structural (same engine; decisions.c@0x0044ed00/
// 0x00494a10/0x00414440 for the rule sets). Build-settle.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGuardLogic - ctor @0x0044e430 - RELEASE-RECONCILED to the full 18-action repertoire (session 25; was the
// sess24 8-action predecessor). The attack place source is gated to pArea (CreateAttackPlaceSource(u,pArea) --
// the 2nd arg IS pArea, disasm-confirmed @0x4e57d: [this+0xc0]=pArea -> edx -> fastcall arg2; the Ghidra "0"/
// answer-key "0" were the MI-thunk arg-confusion) so the unit only acts from inside the guarded area. The
// AddAction ORDER below is the release ctor's order = the serialized pChoosePlace.actions vector order; 5
// actions bind to the attack source (Shoot/ThrowGrenade/LaunchRocket/ThrowKnife/Melee -- Melee is on the
// ATTACK source, NOT a separate enemy source), the other 13 to the current source. NO TerrorPK.
CAIGuardLogic::CAIGuardLogic( IAIUnit *pUnit, CUnitArea *_pArea ):
	CAICombatLogic( pUnit, CreateAIChoosePlaceForAttackJob( 0, 0 ) ), pArea( _pArea )
{
	IAIUnit *u = GetUnit();
	attackPlaceSource  = AddPlaceSource( CreateAttackPlaceSource( u, pArea ) );
	currentPlaceSource = AddPlaceSource( CreateCurrentPlaceSource( u ) );
	pShoot            = AddAction( new CAIShootAction( u ),          attackPlaceSource );
	pThrowGrenade     = AddAction( new CAIThrowGrenadeAction( u ),   attackPlaceSource );
	pLaunchRocket     = AddAction( new CAILaunchRocketAction( u ),   attackPlaceSource );
	pReload           = AddAction( new CAIReloadAction( u ),         currentPlaceSource );
	pThrowKnife       = AddAction( new CAIThrowKnifeAction( u ),     attackPlaceSource );
	pMelee            = AddAction( new CAIMeleeAction( u ),          attackPlaceSource );
	pLoot             = AddAction( new CAILootAction( u ),           currentPlaceSource );   // release ctor passes pose RUN (EVar26=RUN @0x4e7ed); the dev CAILootAction is the predecessor with no pose param -> RUN elided (as the attack/after-combat loot)
	pHeal             = AddAction( new CAIHealAction( u ),           currentPlaceSource );
	pGetRidOfInactive = AddAction( new CAIMoveToEnemyAction( u ),    currentPlaceSource );
	pDockWithHG       = AddAction( new CAIDockWithHGAction( u ),     currentPlaceSource );
	pUndockFromHG     = AddAction( new CAIUndockFromHGAction( u ),   currentPlaceSource );
	pShootFromHG      = AddAction( new CAIShootFromHGAction( u ),    currentPlaceSource );
	pBeginSnipe       = AddAction( new CAIBeginSnipeAction( u ),     currentPlaceSource );
	pCollectSnipeAP   = AddAction( new CAICollectSnipeAPAction( u ), currentPlaceSource );
	pSnipeShot        = AddAction( new CAISnipeShotAction( u ),      currentPlaceSource );
	pCancelSnipe      = AddAction( new CAICancelSnipeAction( u ),    currentPlaceSource );
	pWearPK           = AddAction( new CAIWearPKAction( u ),         currentPlaceSource );
	pLeavePK          = AddAction( new CAILeavePKAction( u ),        currentPlaceSource );
}
int  CAIGuardLogic::operator&( CStructureSaver &f ) { f.Add( 2, (CAICombatLogic*)this ); f.Add( 3, &pArea ); return 0; }
// @0x0044ed00 - RELEASE-RECONCILED to the full 18-rule ladder (session 25; was the sess24 8-action
// decision-only partial). With the repertoire now ctor-wired, the guard rules on all 18 actions. The order
// DIFFERS from the attack ladder: the snipe state machine sits ABOVE the heavy guns, Shoot outranks the big
// ordnance, and -- unlike attack -- HEAL is a real rule (15th). The ThrowGrenade rule carries BOTH regular
// votes (overkill-defer CSign(shoot.bKillTargetCertainly,F,F,F) + bad-group-health CSign(grenade.bBadGroupHealth,
// T,T,T)); SHOOT is plain. No Finish() / no move fallback (move is the lowest rule; the base DoJob Finishes the
// job, turn-end = AP-gated CanSkip + the guard reaction swapping the logic).
//
// VERIFIED from the raw x86 of @0x4ed00 (the session-25 methodology, two INDEPENDENT methods agreeing on all
// 18 positions): (1) reversed CSign-construction order (the 18 required signs are built move-first = reverse
// priority); (2) the decode (s2_aiguardlogic.h). grenade-carries-both proven STRUCTURALLY: Signs() called
// 36x = 18 required 1-sign + 17 EMPTY regular + EXACTLY ONE 2-sign regular vector (@0x44f370) -> single 2-sign
// vector refutes a split. Tail GetBestAction->destroyed-bit gate (test [pBest+7],0x80)->DoAction confirmed @0x4504f1.
//
// PRIORITY (first AddRule = highest; CDecision::GetBestAction commits to the first rule beating the 0.5^k bar):
// LeavePK > WearPK > CancelSnipe > CollectSnipeAP > SnipeShot > BeginSnipe > UndockFromHG > DockWithHG >
// ShootFromHG > Shoot > LaunchRocket > ThrowGrenade > ThrowKnife > Reload > Loot > Heal > Melee > GetRidOfInactive.
void CAIGuardLogic::MakeDecision()
{
	// gather each action's info at its chosen place (member order; all 18 are ruled -- guard DOES heal, unlike attack)
	CAIShootAction::SInfo shoot;                 GetInfo( pShoot.GetPtr(), &shoot );
	CAIThrowGrenadeAction::SInfo grenade;        GetInfo( pThrowGrenade.GetPtr(), &grenade );
	CAILaunchRocketAction::SInfo rocket;         GetInfo( pLaunchRocket.GetPtr(), &rocket );
	CAIReloadAction::SInfo reload;               GetInfo( pReload.GetPtr(), &reload );
	CAIThrowKnifeAction::SInfo knife;            GetInfo( pThrowKnife.GetPtr(), &knife );
	CAIMeleeAction::SInfo melee;                 GetInfo( pMelee.GetPtr(), &melee );
	CAILootAction::SInfo loot;                   GetInfo( pLoot.GetPtr(), &loot );
	CAIHealAction::SInfo heal;                   GetInfo( pHeal.GetPtr(), &heal );
	CAIMoveToEnemyAction::SInfo move;            GetInfo( pGetRidOfInactive.GetPtr(), &move );
	CAIDockWithHGAction::SInfo dockHG;           GetInfo( pDockWithHG.GetPtr(), &dockHG );
	CAIUndockFromHGAction::SInfo undockHG;       GetInfo( pUndockFromHG.GetPtr(), &undockHG );
	CAIShootFromHGAction::SInfo shootHG;         GetInfo( pShootFromHG.GetPtr(), &shootHG );
	CAIBeginSnipeAction::SInfo beginSnipe;       GetInfo( pBeginSnipe.GetPtr(), &beginSnipe );
	CAICollectSnipeAPAction::SInfo collectSnipe; GetInfo( pCollectSnipeAP.GetPtr(), &collectSnipe );
	CAISnipeShotAction::SInfo snipeShot;         GetInfo( pSnipeShot.GetPtr(), &snipeShot );
	CAICancelSnipeAction::SInfo cancelSnipe;     GetInfo( pCancelSnipe.GetPtr(), &cancelSnipe );
	CAIWearPKAction::SInfo wearPK;               GetInfo( pWearPK.GetPtr(), &wearPK );
	CAILeavePKAction::SInfo leavePK;             GetInfo( pLeavePK.GetPtr(), &leavePK );

	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	// 18 rules in priority order (highest first); each required sign = CSign(&xxx.bCanDo,T,T,T).
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &leavePK.bCanDo, true, true, true ) );      pDecision->AddRule( new CRule<CAIAction*>( pLeavePK.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &wearPK.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pWearPK.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &cancelSnipe.bCanDo, true, true, true ) );  pDecision->AddRule( new CRule<CAIAction*>( pCancelSnipe.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &collectSnipe.bCanDo, true, true, true ) ); pDecision->AddRule( new CRule<CAIAction*>( pCollectSnipeAP.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &snipeShot.bCanDo, true, true, true ) );    pDecision->AddRule( new CRule<CAIAction*>( pSnipeShot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &beginSnipe.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pBeginSnipe.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &undockHG.bCanDo, true, true, true ) );     pDecision->AddRule( new CRule<CAIAction*>( pUndockFromHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &dockHG.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pDockWithHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shootHG.bCanDo, true, true, true ) );      pDecision->AddRule( new CRule<CAIAction*>( pShootFromHG.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shoot.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pShoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &rocket.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pLaunchRocket.GetPtr(), req, reg ) ); }
	// ThrowGrenade carries BOTH regular votes; the preceding Shoot rule is PLAIN.
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &grenade.bCanDo, true, true, true ) );
	  reg.push_back( new CSign<bool>( &shoot.bKillTargetCertainly, false, false, false ) );   // overkill-defer
	  reg.push_back( new CSign<bool>( &grenade.bBadGroupHealth, true, true, true ) );          // save grenades for wounded groups
	  pDecision->AddRule( new CRule<CAIAction*>( pThrowGrenade.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &knife.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pThrowKnife.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &reload.bCanDo, true, true, true ) );       pDecision->AddRule( new CRule<CAIAction*>( pReload.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &loot.bCanDo, true, true, true ) );         pDecision->AddRule( new CRule<CAIAction*>( pLoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &heal.bCanDo, true, true, true ) );         pDecision->AddRule( new CRule<CAIAction*>( pHeal.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &melee.bCanDo, true, true, true ) );        pDecision->AddRule( new CRule<CAIAction*>( pMelee.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &move.bCanDo, true, true, true ) );         pDecision->AddRule( new CRule<CAIAction*>( pGetRidOfInactive.GetPtr(), req, reg ) ); }
	CAIAction *pBest = pDecision->GetBestAction();
	if ( IsValid( pBest ) )
		DoAction( pBest );
	// no Finish() on no-best -- the release guard never self-ends (base DoJob Finish + AP-gated CanSkip + reaction swap)
}
// @0x0044e3a0 - RELEASE-RECONCILED: skip the turn only when out of useful AP. A fight-capable unit with
// >5 AP keeps acting; one that can't fight (downed) never skips, keeping the guard alive. Same gate as
// CAIRetreatLogic::CanSkip (disasm-confirmed @0x4e3a0: GetUnitServer valid+not-destroyed, CanFight (server
// vtbl+0x44), then GetUnit valid && AP>5 -> false; constant 5 cmp-confirmed). Was a `return false` stub.
bool CAIGuardLogic::CanSkip() const
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( !IsValid( pUS ) || !pUS->CanFight() ) return false;
	IAIUnit *u = GetUnit();
	if ( IsValid( u ) && u->GetAP() > 5 ) return false;
	return true;
}
//
// CAIRetreatLogic - ctor @0x00494590: a CreateToPlacePlaceSource aimed at the retreat point `pos` (so the
// unit moves toward it, within 25 AP) + the full attack repertoire on that source.
CAIRetreatLogic::CAIRetreatLogic( IAIUnit *pUnit, const SPathPlace &_pos ):
	CAICombatLogic( pUnit, CreateAIChoosePlaceForRetreatJob( 0 ) ), pos( _pos )
{
	IAIUnit *u = GetUnit();
	retreatPlaceSource = AddPlaceSource( CreateToPlacePlaceSource( u, pos, 25 ) );
	pShoot        = AddAction( new CAIShootAction( u ),        retreatPlaceSource );
	pThrowGrenade = AddAction( new CAIThrowGrenadeAction( u ), retreatPlaceSource );
	pLaunchRocket = AddAction( new CAILaunchRocketAction( u ), retreatPlaceSource );
	pReload       = AddAction( new CAIReloadAction( u ),       retreatPlaceSource );
	pThrowKnife   = AddAction( new CAIThrowKnifeAction( u ),   retreatPlaceSource );
	pMelee        = AddAction( new CAIMeleeAction( u ),        retreatPlaceSource );
	pMove         = AddAction( new CAIMoveToEnemyAction( u ),  retreatPlaceSource );
}
int  CAIRetreatLogic::operator&( CStructureSaver &f ) { f.Add( 2, (CAICombatLogic*)this ); f.Add( 3, &pos ); return 0; }
// @0x00494a10 - ARRIVAL-GATED: finishes once the unit's control point is within 0.5 of the retreat
// place's (release ends ONLY by arriving -- no finish-on-no-best). Otherwise the attack rule set over
// the path-to-retreat places (shoot > rocket > grenade > knife > reload > melee > move), but the release
// puts BOTH regular signs on the GRENADE rule (shoot overkill-defer !bKillTargetCertainly + grenade
// !bBadGroupHealth) and leaves the shoot rule plain.
void CAIRetreatLogic::MakeDecision()
{
	// arrival gate: |selfCP - retreatCP| < 0.5 (float @0x008b19ec) -> done. Path net from the unit
	// server's world (== the AI state's world for an in-world unit), as Guard/Defence/Retreat-reaction.
	IAIUnit *u = GetUnit();
	IPathNetwork *pNet = GetUnitServer()->GetWorld()->GetPathNetwork();
	CVec3 cpRetreat = GetUnitPos( pos, pNet ).pos.GetCP();
	CVec3 cpCur     = u->GetPosition().GetCP();
	float du = cpCur.u - cpRetreat.u, dv = cpCur.v - cpRetreat.v, dq = cpCur.q - cpRetreat.q;
	if ( sqrtf( du * du + dv * dv + dq * dq ) < 0.5f ) { CAIJob::Finish(); return; }

	CAIShootAction::SInfo shoot; GetInfo( pShoot.GetPtr(), &shoot );
	CAIThrowGrenadeAction::SInfo grenade; GetInfo( pThrowGrenade.GetPtr(), &grenade );
	CAILaunchRocketAction::SInfo rocket; GetInfo( pLaunchRocket.GetPtr(), &rocket );
	CAIReloadAction::SInfo reload; GetInfo( pReload.GetPtr(), &reload );
	CAIThrowKnifeAction::SInfo knife; GetInfo( pThrowKnife.GetPtr(), &knife );
	CAIMeleeAction::SInfo melee; GetInfo( pMelee.GetPtr(), &melee );
	CAIMoveToEnemyAction::SInfo move; GetInfo( pMove.GetPtr(), &move );
	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &shoot.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pShoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &rocket.bCanDo, true, true, true ) );  pDecision->AddRule( new CRule<CAIAction*>( pLaunchRocket.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &grenade.bCanDo, true, true, true ) ); reg.push_back( new CSign<bool>( &shoot.bKillTargetCertainly, false, false, false ) ); reg.push_back( new CSign<bool>( &grenade.bBadGroupHealth, true, true, true ) ); pDecision->AddRule( new CRule<CAIAction*>( pThrowGrenade.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &knife.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pThrowKnife.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &reload.bCanDo, true, true, true ) );  pDecision->AddRule( new CRule<CAIAction*>( pReload.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &melee.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pMelee.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &move.bCanDo, true, true, true ) );    pDecision->AddRule( new CRule<CAIAction*>( pMove.GetPtr(), req, reg ) ); }
	CAIAction *pBest = pDecision->GetBestAction();
	if ( pBest )
		DoAction( pBest );
	// no Finish() on no-best -- the release retreat logic ends ONLY by arriving (gate at top)
}
// @0x00494500 - skip only when low on AP: a fight-capable unit with >5 AP keeps acting; one that
// can't fight (downed) never skips, keeping the retreat alive.
bool CAIRetreatLogic::CanSkip() const
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( !IsValid( pUS ) || !pUS->CanFight() ) return false;
	IAIUnit *u = GetUnit();
	if ( IsValid( u ) && u->GetAP() > 5 ) return false;
	return true;
}
//
// CAIAfterCombatLogic - regroup when the enemy is gone. ctor @0x00414150: register OnNewTurn, bAPUpdated=false,
// 1 current-place source + the reload/loot/heal/advance actions. bLoot decides whether looting is considered.
CAIAfterCombatLogic::CAIAfterCombatLogic( IAIUnit *pUnit, bool _bLoot ):
	CAICombatLogic( pUnit, CreateAIChoosePlaceForAttackJob( 0, 0 ) ),
	regOnNewTurn( this, &CAIAfterCombatLogic::OnNewTurn ), bAPUpdated( false ), bLoot( _bLoot )
{
	IAIUnit *u = GetUnit();
	currentPlaceSource = AddPlaceSource( CreateCurrentPlaceSource( u ) );
	pReload           = AddAction( new CAIReloadAction( u ),      currentPlaceSource );
	pLoot             = AddAction( new CAILootAction( u ),        currentPlaceSource );   // release ctor pose WALK; dev CAILootAction is the no-pose predecessor (elided, as guard/attack loot)
	pHeal             = AddAction( new CAIHealAction( u ),        currentPlaceSource );
	pGetRidOfInactive = AddAction( new CAIMoveToEnemyAction( u ), currentPlaceSource );
}
// @0x17c40 - the release serializes the FULL state (NOT parent-only like the predecessor): base(2) +
// bAPUpdated(3) + currentPlaceSource(4) + pReload(5) + pLoot(6) + pHeal(7) + pGetRidOfInactive(8) + bLoot(9).
// The CObj members are the SAME objects as in the base's pChoosePlace.actions / placeSources, so the framework
// writes them by ref-id; serializing them here just preserves the member pointers. regOnNewTurn is NOT
// serialized (a runtime event subscription, re-established by the ctor). A converging save-format change.
int  CAIAfterCombatLogic::operator&( CStructureSaver &f )
{
	f.Add( 2, (CAICombatLogic*)this );
	f.Add( 3, &bAPUpdated );
	f.Add( 4, &currentPlaceSource );
	f.Add( 5, &pReload );
	f.Add( 6, &pLoot );
	f.Add( 7, &pHeal );
	f.Add( 8, &pGetRidOfInactive );
	f.Add( 9, &bLoot );
	return 0;
}
// @0x00414440 - rule order reload > heal > loot > move, each required=[bCanDo]. Loot is GetInfo'd/ruled only
// when bLoot. RELEASE FINISH LOGIC (vs the predecessor's unconditional finish-on-no-best): a no-doable-action
// decision finishes the LOGIC (CAILogic::Finish -- end the whole logic, NOT just the per-think job) ONLY when
// bAPUpdated, i.e. after the unit's player has had a fresh turn (OnNewTurn); so in turn-based the logic persists
// one turn to re-act with refreshed AP. A real-time world (no turns to wait for, IsRealTime @ vtbl+0x1a0) sets
// bAPUpdated itself, so it ends at the first empty decision (the predecessor behaviour, recovered as a fall-out).
void CAIAfterCombatLogic::MakeDecision()
{
	CAIReloadAction::SInfo reload; GetInfo( pReload.GetPtr(), &reload );
	CAIHealAction::SInfo heal; GetInfo( pHeal.GetPtr(), &heal );
	CAILootAction::SInfo loot;
	if ( bLoot ) GetInfo( pLoot.GetPtr(), &loot );
	CAIMoveToEnemyAction::SInfo move; GetInfo( pGetRidOfInactive.GetPtr(), &move );
	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &reload.bCanDo, true, true, true ) ); pDecision->AddRule( new CRule<CAIAction*>( pReload.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &heal.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pHeal.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &loot.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pLoot.GetPtr(), req, reg ) ); }
	{ vector< CPtr<ISign> > req, reg; req.push_back( new CSign<bool>( &move.bCanDo, true, true, true ) );   pDecision->AddRule( new CRule<CAIAction*>( pGetRidOfInactive.GetPtr(), req, reg ) ); }
	CAIAction *pBest = pDecision->GetBestAction();
	if ( IsValid( pBest ) )
		DoAction( pBest );
	else if ( bAPUpdated )
		CAILogic::Finish();              // end the logic only after a fresh turn (or in real time, set just below)
	NWorld::IWorld *pWorld = GetWorld();   // IsRealTime is on the concrete CWorld (via CTBSWorld), not IWorld
	if ( IsValid( pWorld ) && static_cast<NWorld::CWorld*>( pWorld )->IsRealTime() )
		bAPUpdated = true;               // real time has no turns to wait for -> finish at the first empty decision
}
bool CAIAfterCombatLogic::CanSkip() const { return false; }
// @0x14000 - the CEventOnNewPlayerTurn handler (registered via regOnNewTurn). When control passes to the unit's
// OWN player, mark AP refreshed so the next empty MakeDecision finishes the logic. (GetPlayer = CUnit-base
// vtbl+0x34; the event carries the player whose turn it now is.)
void CAIAfterCombatLogic::OnNewTurn( const NWorld::CEventOnNewPlayerTurn &event )
{
	NWorld::CUnitServer *pUS = GetUnitServer();
	if ( IsValid( pUS ) && pUS->GetPlayer() == event.pPlayer.GetPtr() )
		bAPUpdated = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Factories + predicates
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogic* CreateAIAttackLogic( IAIUnit *pUnit )                                          // @0x00423260
{
	return IsValid( pUnit ) ? new CAIAttackLogic( pUnit ) : 0;
}
IAILogic* CreateAIDefenceLogic( IAIUnit *pUnit, const SPathPlace &attackPlace, int nArg ) // @0x00439780
{
	return IsValid( pUnit ) ? new CAIDefenceLogic( pUnit, attackPlace, nArg ) : 0;
}
IAILogic* CreateAIAfterCombatLogic( IAIUnit *pUnit, bool bLoot )                          // @0x00414a50
{
	return IsValid( pUnit ) ? new CAIAfterCombatLogic( pUnit, bLoot ) : 0;
}
IAILogic* CreateAIRetreatLogic( IAIUnit *pUnit, const SPathPlace &pos )                   // @0x00494990
{
	return IsValid( pUnit ) ? new CAIRetreatLogic( pUnit, pos ) : 0;
}
IAILogic* CreateAIGuardLogic( IAIUnit *pUnit, CUnitArea *pArea )                          // @0x004506f0
{
	return IsValid( pUnit ) ? new CAIGuardLogic( pUnit, pArea ) : 0;
}
bool IsAttackLogic( IAILogic *pLogic )   { return CDynamicCast<CAIAttackLogic>( pLogic ) != 0; }   // @0x00420e70
bool IsDefenceLogic( IAILogic *pLogic )  { return CDynamicCast<CAIDefenceLogic>( pLogic ) != 0; }  // @0x00439480
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
BASIC_REGISTER_CLASS( CAIAttackLogic )
BASIC_REGISTER_CLASS( CAIDefenceLogic )
BASIC_REGISTER_CLASS( CAIGuardLogic )
BASIC_REGISTER_CLASS( CAIRetreatLogic )
BASIC_REGISTER_CLASS( CAIAfterCombatLogic )
