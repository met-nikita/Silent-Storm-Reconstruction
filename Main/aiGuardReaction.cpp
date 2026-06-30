#include "StdAfx.h"
//
#include "aiUnit.h"            // NAI::IAIUnit
#include "aiUnitState.h"       // NAI::SAIUnitState (pEnemy / pPossibleEnemy / pAlly)
#include "aiCombatLogic.h"     // NAI::CreateAIGuardLogic / CreateAIAfterCombatLogic / CAIGuardLogic (IsGuardLogic)
#include "aiRouteLogic.h"      // NAI::CreateAICheckPositionLogic / CreateAIMoveToPositionLogic
#include "aiDefenceReaction.h" // NAI::CanUseDefenceReaction / CreateAIDefenceReaction
#include "aiMisc.h"            // NAI::GetPathAP
#include "wUnitServer.h"       // NWorld::CUnitServer (GetWorld + the public CUnitAnimator animator)
#include "wMain.h"             // NWorld::CWorld::GetPathNetwork
#include "wMainPath.h"         // NWorld::FindPath
#include "aiPath.h"            // NAI::CPath, NAI::PF_DEFAULT
//
#include "aiGuardReaction.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiGuardReaction -- the stand-and-watch reaction. Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_aiguardreaction.h; all 10 Update steps disasm-verified @0x510e0 -- the answer-key
// transcription was confirmed faithful step-by-step). See aiGuardReaction.h for the architecture + the
// transient-reaction note. Every reach resolves to a real in-tree call; the documented elisions (the early
// route-clear + the two AI-event raises) are noted inline.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
namespace {
// Same tile ignoring the direction/pose/moving bits (the route layer's mask 0x1feffff). The dev tree has no
// shared masked form -- inlined like aiDefenceReaction.cpp.
inline bool SamePlace( const SPathPlace &a, const SPathPlace &b )
{
	return ( ( a.GetData() ^ b.GetData() ) & 0x1feffff ) == 0;
}
// NAI::IsGuardLogic @0x44e370 -- the release RTTI predicate (a live CAIGuardLogic). aiCombatLogic.h declares
// IsAttackLogic/IsDefenceLogic but not IsGuardLogic, so it is reconstructed here over the present concrete
// CAIGuardLogic (the release form is __RTDynamicCast<CAIGuardLogic>).
inline bool IsGuardLogic( IAILogic *pLogic )
{
	return IsValid( dynamic_cast<CAIGuardLogic*>( pLogic ) );
}
}   // anonymous namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x50f40: pUnit + the (clamped) radius; the area is null, the post starts at the invalid place, no combat
// seen yet.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGuardReaction::CAIGuardReaction( IAIUnit *pUnit, NDb::CAnimation *pGuardAnimation, int nRadius )
	: CAIReaction( pUnit ), pGuardAnimation( pGuardAnimation ),
	  nAPRadius( nRadius < 0 ? 0 : nRadius ), bWasCombat( false )
{
	initialPos.pos.p = SPathPlace( (int)0xfdffffff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x510e0 -- the per-think stand-and-watch dance (the 10 steps map 1:1 to the oracle).
//
// ELIDED (build-validation scope):
//  * the early pU->SetRoute(NULL) (release IAIUnit vtbl+0x5c == NAI::CAIUnit::SetRoute @0x4adb90, which cancels
//    + clears the unit's current route logic so it re-decides). The dev unit has a single CObj<IAILogic> pLogic
//    that SetLogic replaces in place (aiUnit.cpp:162); the landed CAINormalReaction/CAIDefenceReaction omit any
//    such clear and IAIUnit declares no SetRoute slot, so it is dropped consistently.
//  * the two AI-event raises -- AddEvent(CreateAILostPossibleEnemyEvent(possible)) (chase branch) and
//    AddEvent(CreateAILostAllyEvent(ally)) (ally branch). The per-unit AI event system (IAIEvent /
//    CAIUnit::Notify(IAIEvent*), vtbl+0x48) is unported; SAIUnitState::Populate() re-polls the seen sets every
//    think (aiUnitState.h), so the events' RemovePossibleEnemy/RemoveAlly cleanup is redundant under the dev
//    polling model -- behaviour-safe to drop.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIGuardReaction::Update()
{
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// (a) lazily (re)build the guarded area centred on the post (the unit's current position), priced at WALK;
	// a failed Prepare drops the area and the reaction stays inert this think.
	if ( !IsValid( pArea ) )
	{
		initialPos = pU->GetUnitPosition();
		CUnitArea *pNew = new CUnitArea( pUS, initialPos.pos.p, nAPRadius, WALK );
		pArea = pNew;
		if ( pNew != 0 && !pNew->Prepare() )
			pArea = 0;
	}
	if ( !IsValid( pArea ) )
		return;
	// (b) [SetRoute(NULL) elided -- see banner]  the per-unit threat state (populated by the commander each think).
	SAIUnitState *pState = GetAIUnitState();
	if ( pState == 0 )
		return;
	IAIUnit *pPossible = pState->pPossibleEnemy.GetPtr();
	IAIUnit *pEnemy    = pState->pEnemy.GetPtr();
	IAIUnit *pAlly     = pState->pAlly.GetPtr();
	// (c) a live known enemy + a fresh cover/attack pair -> hand off to the take-cover reaction (Guard is the
	// fall-back it reverts to, held in CAIDefenceReaction::pPrevReaction so this SetReaction won't delete it).
	if ( CanUseDefenceReaction( pU ) )
	{
		pU->SetReaction( CreateAIDefenceReaction( pU, this ) );
		return;
	}
	// (d) saw combat -> remember it (drives the AfterCombat regroup once the enemy is gone).
	if ( IsValid( pEnemy ) )
		bWasCombat = true;
	bool bChasePossible = IsValid( pPossible );
	if ( bChasePossible && IsValid( pEnemy ) )
	{
		// both up: chase the SUSPECTED contact only when it is much closer than the known enemy (< 1/3 dist).
		CVec3 cpSelf     = pU->GetUnitServer()->GetPosition().GetCP();
		CVec3 cpPossible = pPossible->GetUnitServer()->GetPosition().GetCP();
		CVec3 cpEnemy    = pEnemy->GetUnitServer()->GetPosition().GetCP();
		bChasePossible = fabs( cpSelf - cpPossible ) < fabs( cpSelf - cpEnemy ) * 0.33333334f;   // const @0x8b2fd4
	}
	bool bHandled = false;
	// (e) chase the suspected enemy: inspect its place from inside the guarded area.
	if ( bChasePossible )
	{
		SPosition pos = pPossible->GetPosition();
		IAILogic *pLogic = CreateAICheckPositionLogic( pU, pArea, pos, CROUCH );
		if ( IsValid( pLogic ) )
		{
			SetLogic( pLogic );
			pState->RemovePossibleEnemy( pPossible );   // RE-ENABLED (retail raised CreateAILostPossibleEnemyEvent here):
			// consume the suspect once we commit to investigating its position. This bounds the now-preserved
			// possibleEnemies (aiUnitState.cpp Populate no longer wipes them) -- each suspect triggers ONE investigation
			// then is dropped, so a shot-from-concealment is checked once and not chased forever.
			bHandled = true;
		}
	}
	if ( !bHandled )
	{
		if ( IsValid( pEnemy ) )
		{
			// (f) a known enemy is up: hold + engage from within the area (keep an existing guard logic).
			if ( !IsGuardLogic( pU->GetLogic() ) )
				SetLogic( CreateAIGuardLogic( pU, pArea ) );
		}
		else if ( IsValid( pAlly ) )
		{
			// (g) a calling ally: glance toward its place from inside the area.
			SPosition pos = pAlly->GetPosition();
			if ( SetLogic( CreateAICheckPositionLogic( pU, pArea, pos, CROUCH ) ) )
			{
				// [ELIDED: pU->AddEvent(CreateAILostAllyEvent(pAlly)) -- see banner]
			}
		}
		else if ( bWasCombat )
		{
			// (h) combat is over: regroup.
			SetLogic( CreateAIAfterCombatLogic( pU, false ) );
			bWasCombat = false;
		}
		else if ( IsValid( pArea ) )
		{
			// (i) all quiet: if the unit has left its post, walk back to it (gated on a real path with AP > 0).
			// The release gates this on CPathNetwork::GetPassability(initialPos) != AIP_YES (path-net vtbl+0x3c,
			// the mover AddRef'd around the call); the dev IPathNetwork exposes no per-position EPassable
			// accessor, so the verified "1 == still on post" semantic is reproduced as "not already on the post
			// place" (masked SamePlace). The path + GetPathAP gates below still guard the actual move.
			IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
			if ( pNet != 0 && !SamePlace( pU->GetUnitPosition().pos.p, initialPos.pos.p ) )
			{
				NWorld::CUnit *pComp = static_cast<NWorld::CUnit*>( pUS );
				vector<SPathPlace> targets;
				targets.push_back( initialPos.pos.p );
				CPtr<NAI::CPath> path = NWorld::FindPath( pNet, pComp, pU->GetUnitPosition().pos.p, targets,
					pComp, false, NAI::PF_DEFAULT, false, true, true );
				if ( IsValid( path ) && GetPathAP( pUS, path.GetPtr() ) > 0 )
					SetLogic( CreateAIMoveToPositionLogic( pU, initialPos.pos.p, WALK, WALK, false ) );
			}
		}
	}
	// (j) idle-animation tail: a unit with a live logic clears its custom idle animation (re-fetching its server,
	// as the release does); an idle guard with a configured animation shows it.
	if ( IsValid( pU->GetLogic() ) )
		pU->GetUnitServer()->animator.SetCustomIdleAnimation( 0 );
	else
	{
		NDb::CAnimation *pAnim = pGuardAnimation.GetPtr();
		if ( IsValid( pAnim ) )
			pUS->animator.SetCustomIdleAnimation( pAnim );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x50ef0 -- a live unit -> a new guard reaction (the area is flooded on the first Update).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIGuardReaction( IAIUnit *pUnit, NDb::CAnimation *pGuardAnimation, int nRadius )
{
	if ( !IsValid( pUnit ) )
		return 0;
	return new CAIGuardReaction( pUnit, pGuardAnimation, nRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
BASIC_REGISTER_CLASS( CAIGuardReaction )
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// NDatabase::GetTable<NDb::CAnimation> @0x51a20 -- the typed-table accessor COMDAT the matched release emits
// into aiGuardReaction.obj (the compiland resolves the guard-reaction animation table by type id). The
// template body -- GetRecordTypes().GetTypeID(p) -> non-template GetTable(int) -- is already defined in
// ADOImport/BasicDB.h:143-150 and visible here via aiGuardReaction.h -> ../DBFormat/DataAnimation.h ->
// ../ADOImport/BasicDB.h; NDb::CAnimation is a complete CDBRecord subclass (DataAnimation.h), so this explicit
// instantiation forces the per-RVA coverage-parity COMDAT with NO behaviour change (the live ctor takes
// NDb::CAnimation* directly via the pGuardAnimation member -- this is coverage-only, not a runtime lookup).
// NOTE: CDBTable<T> is declared at GLOBAL scope in BasicDB.h (the NDatabase namespace block wraps only the
// GetTable accessors), so the return/param type is ::CDBTable<NDb::CAnimation>, not NDatabase::CDBTable.
// Oracle: decomp/src/s2_ndatabase_aiguardreaction.h.
////////////////////////////////////////////////////////////////////////////////////////////////////
template CDBTable<NDb::CAnimation>* NDatabase::GetTable<NDb::CAnimation>( CDBTable<NDb::CAnimation>** );
