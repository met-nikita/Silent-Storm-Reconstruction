#include "StdAfx.h"
//
#include "..\DBFormat\DataRPG.h"        // NDb::WT_PISTOL / WT_SUB_MACHINE_GUN -- BEFORE aiInventory.h (its NDB:: fwd-decl typo)
#include "aiUnit.h"            // NAI::IAIUnit (GetUnitServer/GetUnitMission/GetAIInventory/GetUnitPosition/SetReaction)
#include "aiUnitState.h"       // NAI::SAIUnitState (pEnemy / pPossibleEnemy)
#include "aiReactions.h"       // NAI::CAINormalReaction (the give-up fall-back)
#include "aiRouteLogic.h"      // NAI::CreateAIHideLogic / CreateAIMoveToPositionLogic / CreateAICheckForEnemyLogic
#include "aiRouteMisc.h"       // NAI::GetNearestPlaces
#include "aiMisc.h"            // NAI::HasPath
#include "aiActionBase.h"      // NAI::SPlaceWithAP (complete) -- before aiMoveAction.h (C2036 guard)
#include "aiMoveAction.h"      // NAI::GetPos / GetUnitPos (place -> SPosition / SUnitPosition)
#include "aiInventory.h"       // NAI::CAIInventory::GetFirstFireArms
#include "aiWeapon.h"          // NAI::CAIFireArmsWeapon::GetAnimType
#include "wUnitServer.h"       // NWorld::CUnitServer (GetWorld/CanFight/CanDo/IsHiding/GetWearingDBPK)
#include "wMain.h"             // NWorld::CWorld::GetPathNetwork / GetGlobalGame
#include "wUnitCommands.h"     // NWorld::CCmdHide (the hide probe command)
#include "RPGGlobal.h"         // NRPG::CGlobalGame::pDifficulty
#include "RPGUnitMission.h"    // NRPG::IUnitMission::GetHearingProbability
#include "..\DBFormat\DataDifficulty.h" // NDb::CDBDifficulty::nAssassinProbability
#include "..\DBFormat\DataAI.h"         // NDb::SAISound / NDb::GetAISound
#include "..\Misc\RandomGen.h"          // random (CRandomGenerator)
//
#include "aiAssassinReaction.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiAssassinReaction -- the hide-and-creep sneak-attack reaction. Reconstructed from the matched-release
// decode (oracle: decomp/src/s2_aiassassinreaction.h; CanUse @0x1ab00 / GetAssassinPosition @0x1ae20 /
// Update @0x1b500 / factory @0x1ade0, all disasm-verified). See aiAssassinReaction.h for the architecture +
// the transient-reaction note. Every reach resolves to a real in-tree call; the documented elisions/
// answer-key corrections are noted inline.
//
// SEAM RESOLUTIONS (ground-truth, this session -- the answer-key pfn* labels were imprecise):
//  * pfnServerAttackObj (server vtbl+0x34) is the GetWearingDBPK mislabel (sess22): the veto is "wearing a
//    panzerklein" (a PK unit doesn't sneak-attack), reproduced as IsValid( pUS->GetWearingDBPK() ).
//  * the weapon gate is a SHORT arm -- WT_PISTOL(1) || WT_SUB_MACHINE_GUN(3) (the answer-key "long arm"
//    comment was WRONG); via GetAIInventory()->GetFirstFireArms()->GetAnimType().
//  * pfnPushHideCommand (+0x14c vtbl+0x5c) is NWorld::CUnitServer::CanDo -- a side-effect-free PROBE, NOT a
//    queue: pUS->CanDo( new CCmdHide() ) == UCR_OK (the exact CTaskCommandHide::Do idiom, aiRouteLogic.cpp:82).
//    The release "== 1" maps to dev NWorld::UCR_OK (enum order differs; port the semantic).
//  * pfnHearChance -- the enemy's RPG is reached via IAIUnit::GetUnitMission() (vtbl+4, NOT a GetUnitServer()
//    ->GetUnitRPG() chain): pEnemy->GetUnitMission()->GetHearingProbability( pUnit->GetUnitMission(), fDist,
//    sound, &bAudible ), sound = { GetAISound(3) /*RUN*/, 0, 1.0f }; >= 15 means the enemy would hear us.
//  * pfnAssassinChanceOption = GetWorld()->GetGlobalGame()->pDifficulty->nAssassinProbability (int +0x5c,
//    tag 22 -- the difficulty option next to bAICheckCorpses).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
namespace {
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetAssassinPosition @0x1ae20 -- wave 14 AP at RUN around the ENEMY; of the reachable places on the
// unit's SIDE of the enemy ((cpEnemy - cp).(cpEnemy - cpOwn) > 0), the first one the enemy could not hear the
// unit RUN to (its hearing chance for the RUN sound at the CP distance scaled by 1.6 is below 15) decides --
// the answer is whether the unit HasPath to it. *pRes is written for every side-passing candidate (the binary
// fills it before the hearing test), so it can be left clobbered on failure. File-static (no other in-tree
// caller; the release NAI::GetAssassinPosition is not declared in any dev header).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetAssassinPosition( IAIUnit *pUnit, IAIUnit *pEnemy, SUnitPosition *pRes )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	if ( !IsValid( pEnemy ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	SUnitPosition enemyPos = pEnemy->GetUnitPosition();
	CVec3 cpEnemy = enemyPos.GetCP();
	CVec3 cpOwn = pUnit->GetUnitPosition().GetCP();
	CVec3 d = cpEnemy - cpOwn;               // the unit->enemy axis
	vector<SPathPlace> places;
	GetNearestPlaces( pUS, enemyPos.pos.p, 14, RUN, &places );
	NDb::SAISound sound = { NDb::GetAISound( 3 ), 0, 1.0f };   // the RUN movement sound, no silencer
	NRPG::IUnitMission *pEnemyRPG = pEnemy->GetUnitMission();  // the listener
	NRPG::IUnitMission *pOwnRPG = pUnit->GetUnitMission();     // the source of the noise
	for ( int i = 0; i < (int)places.size(); ++i )
	{
		CVec3 cp = GetPos( places[i], pNet ).GetCP();
		CVec3 toEnemy = cpEnemy - cp;
		float fDot = toEnemy.u * d.u + toEnemy.v * d.v + toEnemy.q * d.q;
		if ( !( fDot > 0.0f ) )   // past the enemy (on its far side) -> not an approach spot
			continue;
		*pRes = GetUnitPos( places[i], pNet );
		CVec3 cpRes = pRes->GetCP();
		float fDist = fabs( cpEnemy - cpRes ) * 1.6f;   // CP distance scaled (const @0x8b1fcc)
		bool bAudible;   // out-param the hearing query fills; discarded here
		if ( pEnemyRPG->GetHearingProbability( pOwnRPG, fDist, sound, &bAudible ) >= 15 )
			continue;   // the enemy would hear us run there -> try the next place
		return HasPath( pUnit, pUnit->GetUnitPosition().pos.p, places[i], CRAWL, false );
	}
	return false;
}
}   // anonymous namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1b3a0: pUnit + no tracked enemy yet + just-started. pEnemy default-constructs to a null CPtr.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIAssassinReaction::CAIAssassinReaction( IAIUnit *pUnit )
	: CAIReaction( pUnit ), bJustStarted( true )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1b500 -- the per-think hide-and-creep dance. Re-validate the tracked victim (else re-pick known then
// suspected; no live target -> give up). A no-longer-just-started unit that has lost its hide gives up; a
// just-started unit that is not yet hidden installs the hide logic. Otherwise (hidden): close on a live enemy
// > 5 away via a quiet approach spot (GetAssassinPosition) -- a CheckForEnemy logic while still 10+ away, a
// MoveToPosition logic when closer; < 5 hand back to Normal to strike. Any failure gives up to Normal.
// bJustStarted clears at the end of every live pass.
//
// ELIDED (build-validation scope): the per-unit AI event raises the release brackets the plan with are
// unported (the dev tree omits the IAIEvent system; SAIUnitState::Populate re-polls each think -- behaviour-
// safe to drop, as in CAIGuardReaction). DEFENSIVE: a degenerate server-gone case hands back to Normal up
// front (the decode dereferences the server unguarded, trusting the live unit; equivalent give-up outcome).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIAssassinReaction::Update()
{
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	SAIUnitState *pState = GetAIUnitState();   // plain struct ptr -- not a CObjectBase, so no IsValid()
	bool bGiveUp = false;
	// re-validate the tracked victim: gone, dead, or no longer combat-capable -> re-pick (known enemy first,
	// then a merely suspected one). Still nothing -> give up.
	IAIUnit *pE = pEnemy.GetPtr();
	NWorld::CUnitServer *pES = IsValid( pE ) ? pE->GetUnitServer() : 0;
	if ( !IsValid( pES ) || !pES->CanFight() )
	{
		IAIUnit *pPick = ( pState != 0 ) ? pState->pEnemy.GetPtr() : 0;
		if ( !IsValid( pPick ) )
			pPick = ( pState != 0 ) ? pState->pPossibleEnemy.GetPtr() : 0;
		pEnemy = CPtr<IAIUnit>( pPick );
		if ( !IsValid( pPick ) )
			bGiveUp = true;
	}
	NWorld::CUnitServer *pUS = pU->GetUnitServer();
	if ( !IsValid( pUS ) )
	{
		pU->SetReaction( new CAINormalReaction( pU ) );
		bJustStarted = false;
		return;
	}
	bool bDone = false;
	if ( !bJustStarted && !pUS->IsHiding() )
	{
		bGiveUp = true;   // broke out of hiding -> the plan is off
		bDone = true;
	}
	else if ( !pUS->IsHiding() )
	{
		SetLogic( CreateAIHideLogic( pU ) );   // just started: take cover first
		bDone = true;
	}
	if ( !bDone )
	{
		pE = pEnemy.GetPtr();
		if ( !IsValid( pE ) )
			bGiveUp = true;
		else
		{
			CVec3 cpOwn = pU->GetUnitPosition().GetCP();
			CVec3 cpEnemy = pE->GetUnitPosition().GetCP();
			float fDist = fabs( cpEnemy - cpOwn );
			if ( fDist < 5.0f )   // crept close enough (const @0x8b1fc4) -> Normal strikes next think
			{
				bGiveUp = true;
			}
			else
			{
				SUnitPosition target;
				target.pos.p = SPathPlace( (int)0xfdffffff );
				target.bRun = false;
				if ( !GetAssassinPosition( pU, pE, &target ) )
				{
					bGiveUp = true;
				}
				else
				{
					// pLogic is the release CPtr<IAILogic> (not a raw IAILogic*): IsValid below then binds the
					// CPtrBase<T,TRef> overload (Basic2.h:154) and emits the release COMDAT
					// IsValid<NAI::IAILogic,CObjectBase::SRef> @0x1b830. Behaviour-neutral -- SetLogic stores into
					// CAIUnit's owning CObj<IAILogic> (aiUnit.cpp:162), so the logic lifetime is governed by that
					// owning count, not this transient weak (SRef) handle.
					CPtr<IAILogic> pLogic = ( fDist >= 10.0f )   // (const @0x8b1fc8)
						? CreateAICheckForEnemyLogic( pU, target, CRAWL, 0 )
						: CreateAIMoveToPositionLogic( pU, target.pos.p, CRAWL, CRAWL, false );
					if ( IsValid( pLogic ) )
						SetLogic( pLogic.GetPtr() );
					else
						bGiveUp = true;
				}
			}
		}
	}
	if ( bGiveUp )
		pU->SetReaction( new CAINormalReaction( pU ) );
	bJustStarted = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1ade0
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIAssassinReaction( IAIUnit *pUnit, bool bUnitAlive )
{
	if ( !IsValid( pUnit ) || !bUnitAlive )
		return 0;
	return new CAIAssassinReaction( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1ab00
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanUseAssassinReaction( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	if ( !pUS->CanFight() )
		return false;
	// pfnServerAttackObj is the GetWearingDBPK mislabel (sess22): a unit wearing a panzerklein does not sneak.
	if ( IsValid( pUS->GetWearingDBPK() ) )
		return false;
	// a SHORT arm in hand -- pistol or SMG (the release hold-type gate; rifle/MG/launcher excluded).
	CAIInventory *pInv = pUnit->GetAIInventory();
	CAIFireArmsWeapon *pWeapon = IsValid( pInv ) ? pInv->GetFirstFireArms() : 0;
	if ( !IsValid( pWeapon ) )
		return false;
	int nType = pWeapon->GetAnimType();
	if ( nType != NDb::WT_PISTOL && nType != NDb::WT_SUB_MACHINE_GUN )
		return false;
	SAIUnitState *pState = pUnit->GetAIUnitState();
	if ( pState == 0 )
		return false;
	IAIUnit *pTarget = pState->pEnemy.GetPtr();
	if ( !IsValid( pTarget ) )
		pTarget = pState->pPossibleEnemy.GetPtr();
	if ( !IsValid( pTarget ) )
		return false;
	CVec3 cpOwn = pUnit->GetUnitPosition().GetCP();
	CVec3 cpTarget = pTarget->GetUnitPosition().GetCP();
	float fDist = fabs( cpTarget - cpOwn );
	int nChance = pUS->GetWorld()->GetGlobalGame()->pDifficulty->nAssassinProbability;
	if ( !( fDist > 5.0f ) )   // (const @0x8b1fc4)
		return false;
	if ( random.Get( 0, 99 ) >= (unsigned)nChance )   // d100 [0,98] >= the option -> doesn't trigger
		return false;
	if ( pUS->IsHiding() )
		return true;
	// not yet hidden: only usable if the engine accepts a hide (CanDo is a probe -- it does NOT queue).
	return pUS->CanDo( new NWorld::CCmdHide() ) == NWorld::UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x23069400, CAIAssassinReaction )
