#include "StdAfx.h"
//
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitSounds.h"
#include "wOSBase.h"
#include "wTurnBased.h"
#include "wUnitCommands.h"
#include "wUICommands.h"     // NWorld::CUICmdAIUnitWillMove (the AI "unit will move" UI hint)
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiPlayer.h"
#include "aiUnitState.h"      // SAIUnitState (the per-unit threat tracker: pEnemy / IsModified)
#include "aiLogic.h"          // IAILogic (per-unit command-driven behaviour)
#include "aiReaction.h"       // CAIReaction (the update-tracker's payload)
#include "aiMisc.h"           // NAI::CanAIOperateThisUnit
#include "aiJob.h"            // NAI::IAIJobManager::HasPassCalcerJobs (GenerateCommand pass-calc barrier)
#include "scriptCallLUA.h"    // NScript::luaCallFunction (OnStartTurn)
//
#include "RPGUnitInfo.h"
#include "RPGItemSet.h"
#include "rpgDiplomacy.h"
#include "rpgCheatConstants.h"
//
#include "aiCommander.h"
//
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataConst.h"
#include "..\DBFormat\DataMap.h"
#include "aiWeapon.h"
#include "aiInventory.h"
//
bool bForbidAI = false;
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAICommandTracker (aiCommander.obj decodes) -- the AI's per-segment outgoing command queue.
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x341b0: pop the front command once per segment (the bCommandGiven latch).
NWorld::CCommand* SAICommandTracker::GetCommand()
{
	if ( bCommandGiven || commands.empty() )
		return 0;
	NWorld::CCommand *pRes = commands.front().Extract();   // transfer the list's ref to the caller
	commands.pop_front();
	bCommandGiven = true;
	return pRes;
}
// @0x34290: queue a command (bump the freeze tally EVEN for a null/dead one).
void SAICommandTracker::DoCommand( NWorld::CCommand *pCmd )
{
	if ( IsValid( pCmd ) )
		commands.push_back( pCmd );
	++nNonFreezeCounter;
}
// @0x34200: a dead/null unit wipes the whole queue; otherwise drop every queued unit-command aimed at it.
void SAICommandTracker::Clear( NWorld::CUnitServer *pUS )
{
	if ( !IsValid( pUS ) )
	{
		commands.clear();
		return;
	}
	for ( list< CObj<NWorld::CCommand> >::iterator i = commands.begin(); i != commands.end(); )
	{
		CDynamicCast<NWorld::CCmdUnit> pCU( i->GetPtr() );
		CDynamicCast<NWorld::CUnitServer> pCmdUS( IsValid( pCU ) ? pCU->pUnit.GetPtr() : (NWorld::CUnit*)0 );
		if ( IsValid( pCU ) && pCmdUS.GetPtr() == pUS )
			i = commands.erase( i );
		else
			++i;
	}
}
// @0x33940
bool SAICommandTracker::IsEmpty() const
{
	return commands.empty() && !bCommandGiven;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUpdateTracker -- the queue of unit reactions waiting to fire (one per segment).
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x34320: dedup-append.
void SAIUpdateTracker::Add( CAIReaction *pReaction )
{
	if ( !IsValid( pReaction ) )
		return;
	for ( list< CPtr<CAIReaction> >::iterator i = reactions.begin(); i != reactions.end(); ++i )
		if ( i->GetPtr() == pReaction )
			return;
	reactions.push_back( pReaction );
}
// @0x343c0: pop the FRONT reaction and run its Update() (which re-picks that unit's logic). A CPtr holds
// the reaction across the call -- Update() may SetReaction() the unit (releasing its ref) mid-call.
void SAIUpdateTracker::Update()
{
	if ( reactions.empty() )
		return;
	CPtr<CAIReaction> pR = reactions.front();
	reactions.pop_front();
	if ( IsValid( pR ) )
		pR->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitsTracker -- the round-robin over the commander's own units.
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33a10
bool SAIUnitsTracker::IsGoodUnit( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	if ( (NWorld::IPlayer*)pUS->GetPlayer() != pPlayer.GetPtr() )
		return false;
	return pUS->CanFight();
}
// @0x33b90: nearest live tracked unit to pCurrent by CP distance (strictly < 65535).
IAIUnit* SAIUnitsTracker::GetNearestUnit()
{
	if ( !IsValid( pCurrent ) || !IsValid( pCurrent->GetUnitServer() ) )
		return 0;
	CVec3 ptCur = pCurrent->GetPosition().GetCP();
	float fBest = 65535.0f;
	IAIUnit *pBest = 0;
	for ( int i = 0; i < (int)units.size(); ++i )
	{
		IAIUnit *p = units[i].GetPtr();
		if ( !IsValid( p ) || !IsValid( p->GetUnitServer() ) )
			continue;
		float d = fabs( p->GetPosition().GetCP() - ptCur );
		if ( d < fBest ) { fBest = d; pBest = p; }
	}
	return pBest;
}
// @0x350c0: reseed the working copy from the commander's roster, then pick a first current.
void SAIUnitsTracker::SetUnits( const vector< CObj<IAIUnit> > &src )
{
	units.clear();
	for ( vector< CObj<IAIUnit> >::const_iterator i = src.begin(); i != src.end(); ++i )
		units.push_back( i->GetPtr() );
	Next();
}
// @0x34450: shed bad units off the front, then the new current is the nearest good unit to the previous
// one (or the front) and it LEAVES the working set (so the rotation visits each unit once).
void SAIUnitsTracker::Next()
{
	while ( !units.empty() && !IsGoodUnit( units.front().GetPtr() ) )
		units.erase( units.begin() );
	if ( units.empty() )
	{
		pCurrent = 0;
		return;
	}
	IAIUnit *pOld = pCurrent.GetPtr();
	bool bOldOk = IsValid( pOld ) && IsValid( pOld->GetUnitServer() );
	pCurrent = bOldOk ? GetNearestUnit() : units.front().GetPtr();
	if ( IsValid( pCurrent ) )
	{
		for ( int i = 0; i < (int)units.size(); ++i )
			if ( units[i].GetPtr() == pCurrent.GetPtr() )
			{
				units.erase( units.begin() + i );
				break;
			}
	}
}
// @0x34620
void SAIUnitsTracker::Validate()
{
	if ( !IsGoodUnit( pCurrent.GetPtr() ) )
		Next();
}
// @0x33d20
void SAIUnitsTracker::SetPlayer( NWorld::IPlayer *p )
{
	if ( IsValid( p ) )
		pPlayer = p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICommander
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICommander::CAICommander( NWorld::CWorld *_pWorld, NWorld::CPlayer *_pPlayer ):
	pPlayer(_pPlayer), pWorld( _pWorld ), bAITurn( false ),
	state( _pWorld, this ), nAILag( 0 )
{
	// AI-convergence: the commander OWNS the embedded SAIState by value (tag7) -- constructed above with a
	// store-only ctor (world + this, no half-built-member deref). Seed the units-tracker's target player.
	unitsTracker.SetPlayer( pPlayer );
	// retail ctor @0x35a10 stores its player arg into state.pPlayer (state+4) at construction; SetPlayer
	// @0x33e60 never refreshes it later. Mirror that here (execution order: member pPlayer is already set).
	state.pPlayer = pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAICommander::SetPlayer @0x33e60: pPlayer + unitsTracker.pPlayer, nothing else.
void CAICommander::SetPlayer( NWorld::IPlayer *_pPlayer )
{
	if ( !IsValid( _pPlayer ) )
		return;
	CDynamicCast<NWorld::CPlayer> pP( _pPlayer );
	pPlayer = pP;
	unitsTracker.SetPlayer( _pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAICommander::AddUnit @0x351c0: only a live unit that HAS a player; skip when this commander
// already maps it; REUSE the globally-unique wrapper (NAI::GetAIUnit @0x742c0 -- resolved through the
// unit's OWN player's commander) so every commander aliases ONE CAIUnit per server; create only when
// none exists anywhere. The shared identity is load-bearing: the AI perception events (OnSeeEnemy/
// OnBullet/OnHearEnemy/OnGrenade payloads) carry NAI::GetAIUnit(server) wrappers, and SAIUnitState's
// pointer-keyed lists (enemies/possibleEnemies) must see the SAME object the SAIState rosters hold.
void CAICommander::OnUnitAdded( NWorld::CUnitServer *pUnitServer )
{
	ASSERT( IsValid( pUnitServer ) );
	if ( !IsValid( pUnitServer ) || pUnitServer->GetPlayer() == 0 )
		return;
	if ( IsValid( GetAIUnit( pUnitServer ) ) )
		return;                                     // already tracked by this commander
	CObj<IAIUnit> pAIUnit = NAI::GetAIUnit( pUnitServer );
	if ( !IsValid( pAIUnit.GetPtr() ) )
		pAIUnit = CreateAIUnit( pUnitServer, pUnitServer->GetPlayer() == pPlayer );
	units.push_back( pAIUnit );
	// retail AddUnit @0x351c0: cache the world-unit -> AI-unit mapping right after the units push_back.
	worldToAIUnit[ pUnitServer ] = pAIUnit.GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SyncAIState -- retail SAIState::Synchronize @0xa9ff0 analog: rebuild the state's ally/enemy rosters +
// enemy groups from the commander's unit list (and thread the state into the own units). Called every AI
// segment + on pass-control + turn-start.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::SyncAIState()
{
	state.Synchronize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GenerateCommand @0x353d0 -- one decision per segment; the round-robin pumps route + combat logics.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::GenerateCommand()
{
	// retail @0x353d0 opens with the AI-job-manager barrier: while a highest-priority (pass-calculator /
	// pathfinding) job is in flight, generate NO command -- do not pull the command tracker, retire finished
	// logics, or run the round-robin. Without it the commander can retire / re-decide a unit's per-unit logic
	// while that unit's strafe/move path is still being computed on the job manager; combined with the
	// per-poll reaction pump, that retires a just-installed strafe logic before it can execute, so a
	// take-cover unit "spins" (re-plans the strafe every think, AP frozen) and never completes its pop-out
	// shot. World vtbl 0xf0 = GetAIJobManager (@0x376f30); job-mgr vtbl 0x28 = HasPassCalcerJobs (@0x58b20).
	if ( IsValid( pWorld ) && pWorld->GetAIJobManager() != 0 && pWorld->GetAIJobManager()->HasPassCalcerJobs() )
		return;
	bool bTurnBased = !pWorld->IsRealTime();
	// (1) turn-based waits out animations; real-time clears the freeze counters.
	if ( bTurnBased )
	{
		if ( pWorld->IsAction() )
			return;
	}
	else
		ClearNonFreezeCounters();
	// (2) a pending tracker command goes to the world queue (once per segment) when CheckCanDo lets it.
	CObj<NWorld::CCommand> pCmd = commandTracker.GetCommand();
	if ( pCmd.GetPtr() != 0 )
	{
		if ( CheckCanDo( pCmd.GetPtr() ) )
			Do( pCmd.GetPtr() );
		return;
	}
	// (3) AI forbidden -> a turn-based side just finishes the turn.
	if ( bForbidAI )
	{
		if ( bTurnBased )
			FinishTurn();
		return;
	}
	// (4) one decision per segment, no AI lag: retire finished logics, then let the reaction pump settle
	//     before the round-robin, then (turn-based) maybe end the turn.
	if ( !commandTracker.commands.empty() || commandTracker.bCommandGiven || nAILag > 0 )
		return;
	CheckForFinishedLogics();
	if ( !IsSequence() )
	{
		if ( !updateTracker.reactions.empty() )
			return;                               // wait for the reaction pump to drain
		if ( IsSomebodyNeedUpdate() )
			return;
	}
	if ( bTurnBased && IsEndOfTurn() )
	{
		FinishTurn();
		return;
	}
	// units-tracker round-robin (this is what pumps every per-unit CAIRouteLogic AND combat logic).
	unitsTracker.Validate();
	if ( !IsValid( unitsTracker.pCurrent ) )
		unitsTracker.SetUnits( units );
	while ( IsValid( unitsTracker.pCurrent ) && !HasUnitWork( unitsTracker.pCurrent ) )
		unitsTracker.Next();
	IAIUnit *pUnit = unitsTracker.pCurrent;
	if ( !IsValid( pUnit ) )
		return;
	// @0x353d0: real-time drops the "last reported" latch; turn-based, when the unit about to be driven
	// CHANGES, tells the UI which AI unit is about to act (a camera/turn hint) via a CUICmdAIUnitWillMove
	// on the world UI-cmd queue (retail world[vtbl 0x110] == CWorld::AddUICommand).
	if ( bTurnBased )
	{
		if ( pLastReportedUnit.GetPtr() != pUnit )
		{
			pLastReportedUnit = pUnit;
			NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
			pWorld->AddUICommand( new NWorld::CUICmdAIUnitWillMove( pUS ? (NWorld::CUnit*)pUS : 0 ) );
		}
	}
	else
		pLastReportedUnit = 0;
	// point the shared AI state at the unit being driven -- its combat logic + actions read the state's
	// current unit/enemy (this is the old CAITacticalCommander iterator's ModifyState, relocated here).
	state.SetCurrentAIUnit( pUnit );
	SAIUnitState *us = pUnit->GetAIUnitState();
	state.SetCurrentAIEnemy( us ? us->pEnemy.GetPtr() : 0 );
	bool bSomebodyThinking = IsSomebodyThinking();
	IAILogic *pLogic = pUnit->GetLogic();
	if ( !IsValid( pLogic ) )
		return;
	if ( pLogic->IsThinking() )
		return;                                   // wait for this unit's think job
	if ( pLogic->IsNeedToThink() )
	{
		if ( !bSomebodyThinking )
			pLogic->Think();                      // the logic SELF-Adds its CAIJob (S3)
	}
	else
	{
		CObj<NWorld::CCommand> pNew = pLogic->GetCommand();
		if ( pNew.GetPtr() != 0 )
		{
			CDynamicCast<NWorld::CCmdUnit> pCmdUnit( pNew.GetPtr() );
			if ( pCmdUnit )
			{
				CDynamicCast<NWorld::CUnitServer> pUS( pCmdUnit->pUnit.GetPtr() );
				if ( IsValid( pUS ) && pWorld->IsUnitActive( pUS ) && pUS->CanFight() )
				{
					commandTracker.DoCommand( pCmdUnit );
					// retail @0x353d0: a CCmdEmpty keepalive converts to exactly ONE SetCommand{unit,
					// Continue}; only REAL commands get the usual Continue pair. The dev keepalive
					// (CAILogic::GetCommand @0x462c70 port) arrives pre-converted as SetCommand{unit,
					// Continue} -- detect the bare-Continue inner and skip the second Continue.
					CDynamicCast<NWorld::CCmdSetCommand> pSet( pCmdUnit.GetPtr() );
					bool bKeepalive = pSet &&
						CDynamicCast<NWorld::CCmdContinue>( pSet->GetCmd() ).GetPtr() != 0;
					if ( !bKeepalive )
						commandTracker.DoCommand( new NWorld::CCmdSetCommand( pCmdUnit->pUnit, new NWorld::CCmdContinue() ) );
				}
			}
		}
	}
	if ( !bTurnBased )
		unitsTracker.Next();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CheckForFinishedLogics @0x34040: retire each fightable unit's finished per-unit logic.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::CheckForFinishedLogics()
{
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsOwnUnit( *i ) )
			continue;
		if ( !IsValid( *i ) || !IsValid( (*i)->GetUnitServer() ) || !(*i)->GetUnitServer()->CanFight() )
			continue;
		IAILogic *pLogic = (*i)->GetLogic();
		if ( IsValid( pLogic ) && pLogic->IsFinished() )
			(*i)->OnLogicFinished( pLogic );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CheckForUpdates @0x34640: enqueue each AI-operable unit whose threat state changed (GetReactionForUpdate
// returns the reaction only then), clearing its pending commands + in-flight action first.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::CheckForUpdates()
{
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsOwnUnit( *i ) || !CanAIOperateThisUnit( *i ) )
			continue;
		CAIReaction *pReaction = (*i)->GetReactionForUpdate();
		if ( IsValid( pReaction ) )
		{
			commandTracker.Clear( (*i)->GetUnitServer() );
			(*i)->CancelCommand();
			updateTracker.Add( pReaction );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnAISegment @0x346f0: per-segment tick of every operable unit (builds/updates its threat state), then
// the reaction pump (enqueue changed units' reactions + fire ONE reaction -> it installs the logic).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnAISegment()
{
	if ( bForbidAI )
		return;
	if ( pWorld->IsSequence() )
		return;
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		if ( IsOwnUnit( *i ) && CanAIOperateThisUnit( *i ) )
			(*i)->OnAISegment();
	if ( !pWorld->IsRealTime() && !IsThisPlayerTurn() )
		return;
	CheckForUpdates();
	if ( !pWorld->IsRealTime() && pWorld->IsAction() )
		return;
	updateTracker.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ownership domain (see aiCommander.h): the unit's CURRENT player is this commander's player.
bool CAICommander::IsOwnUnit( IAIUnit *pUnit ) const
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	return pUS->GetPlayer() == (NWorld::IPlayer*)pPlayer.GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::HasUnitWork( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	IAILogic *pLogic = pUnit->GetLogic();
	if ( !IsValid( pLogic ) )
		return false;
	if ( !pLogic->IsActive() )                    // retail vtbl 0x24 (dev IsActive == not paused)
		return false;
	if ( !pWorld->IsRealTime() && IsEndOfLogicTurn( pLogic ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsSomebodyNeedUpdate()
{
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( IsOwnUnit( *i ) && CanAIOperateThisUnit( *i ) )
		{
			SAIUnitState *us = (*i)->GetAIUnitState();
			if ( us && us->IsModified() )
				return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsSomebodyThinking()
{
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !IsOwnUnit( *i ) )
			continue;
		IAILogic *pLogic = (*i)->GetLogic();
		if ( IsValid( pLogic ) && pLogic->IsThinking() )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33a90: zero the commander tally AND every unit's own per-unit counter (IAIUnit vtbl 0x94).
void CAICommander::ClearNonFreezeCounters()
{
	commandTracker.nNonFreezeCounter = 0;
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		if ( IsValid( *i ) && IsOwnUnit( *i ) )
			(*i)->ClearNonFreezeCounter();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33ad0: the tally alone trips the 5000 cap, else each unit's own counter (weighted x200) plus the
// tally is tested -- a single churning unit trips the freeze guard well before the aggregate does.
bool CAICommander::IsPossibleFreeze()
{
	if ( commandTracker.nNonFreezeCounter > 5000 )
		return true;
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !IsOwnUnit( *i ) )
			continue;
		if ( commandTracker.nNonFreezeCounter + (*i)->GetNonFreezeCounter() * 200 > 5000 )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsSequence()
{
	return pWorld->IsSequence();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsThisPlayerTurn()
{
	NWorld::IPlayer *pCur = pWorld->GetCurrentPlayer();
	if ( pCur == 0 || !IsValid( pCur ) )
		return true;
	return (NWorld::IPlayer*)pPlayer.GetPtr() == pCur;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x34810: already-marked logics are end-of-turn; else query the logic once and memoize on a positive.
bool CAICommander::IsEndOfLogicTurn( IAILogic *pLogic )
{
	if ( !IsValid( pLogic ) )
		return true;
	if ( eotLogics.find( pLogic ) != eotLogics.end() )
		return true;
	if ( !pLogic->IsEndOfTurn() )
		return false;
	eotLogics[ pLogic ] = 1;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x34c90: probe the wrapped command's feasibility (CUnit::CanDo, vtbl 0x5c). A result worse than
// UCR_NO_TARGET (>4: UCR_NOT_ENOUGH_AP / path-not-found / condition failures) means the unit can't execute
// it -> reject the command AND mark that unit's logic end-of-turn so the round-robin stops re-offering it.
// A CCmdContinue (resume the running executor) is never probed. CUnit vtbl 0x5c == a5dll IUnit::CanDo (the
// int command-feasibility probe with the exact (CCmd*,int*,int*) signature retail calls as (inner,0,0)).
bool CAICommander::CheckCanDo( NWorld::CCommand *pCmd )
{
	CDynamicCast<NWorld::CCmdSetCommand> pSet( pCmd );
	if ( !pSet )
		return true;
	NWorld::CUnit *pUnit = pSet->pUnit.GetPtr();
	if ( !IsValid( pUnit ) )
		return true;
	NWorld::CCmd *pInner = pSet->GetCmd();
	CDynamicCast<NWorld::CCmdContinue> pCont( pInner );
	if ( pCont )
		return true;
	NWorld::EUnitCommandResult r = pUnit->CanDo( pInner, 0, 0 );
	if ( (int)r > NWorld::UCR_NO_TARGET )   // retail literal: result > 4
	{
		CDynamicCast<NWorld::CUnitServer> pUS( pUnit );
		IAIUnit *p = GetAIUnit( pUS );
		if ( IsValid( p ) )
		{
			IAILogic *pLogic = p->GetLogic();
			if ( IsValid( pLogic ) )
				eotLogics[ pLogic ] = 1;
		}
		DebugTrace( "AI error : unit can't do command! [ result = %d ]\n", (int)r );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::FinishTurn()
{
	commandTracker.commands.clear();
	commandTracker.DoCommand( new NWorld::CCmdEndOfTurn() );
	ClearNonFreezeCounters();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsEndOfTurn()
{
	if ( pWorld->IsRealTime() )
		return false;
	if ( pWorld->IsAction() )
		return false;
	if ( IsPossibleFreeze() )
		return true;
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !IsOwnUnit( *i ) )
			continue;
		NWorld::CUnitServer *pUS = (*i)->GetUnitServer();
		if ( !IsValid( pUS ) || !pUS->CanFight() )
			continue;
		if ( !pWorld->IsUnitActive( pUS ) )
			continue;
		// @0x34e20 per-unit skip: a CHEAT_NOAI (0x20) unit is script/player-driven, not the AI's to wait on --
		// its logic must never keep the AI turn alive. Retail probes CUnit vtbl 0x6c(0x20), which == the a5dll
		// IUnit::IsCheatEnabled(NRPG::CHEAT_NOAI) (CHEAT_NOAI == 0x20). A pure ADD: NOAI units are the only ones
		// newly skipped, so no AI-driven unit's turn is cut short.
		if ( pUS->IsCheatEnabled( NRPG::CHEAT_NOAI ) )
			continue;
		IAILogic *pLogic = (*i)->GetLogic();
		if ( IsValid( pLogic ) && pLogic->IsActive() && !IsEndOfLogicTurn( pLogic ) )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTurnStarted()
{
	UnLockAllObjects();
	eotLogics.clear();
	bAITurn = true;
	SyncAIState();
	state.OnTurnStarted();   // HP snapshots + enemy groups (rosters were just filled by SyncAIState)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTurnFinished()
{
	bAITurn = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnCancelAction()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::Synchronize()
{
	bool bPruned = false;
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( !IsValid( (*i)->GetUnitServer() ) )
		{
			i = units.erase( i );
			bPruned = true;
		}
		else
		{
			// per-unit sync only for OWN units (shared wrappers: a foreign unit is synced by its owner)
			if ( (*i)->GetUnitServer()->CanFight() && IsOwnUnit( *i ) )
				(*i)->Synchronize();
			++i;
		}
	}
	// keep worldToAIUnit in lockstep with `units`: a pruned unit's server pointer is already dead, so it can't
	// key a targeted erase -- rebuild the cache from the surviving roster whenever a prune actually happened.
	if ( bPruned )
		RebuildAIUnitCache();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// RebuildAIUnitCache -- reseed worldToAIUnit from the current `units` roster (used after Synchronize prunes
// dead-server units, where the removed key is no longer recoverable). Behaviour-neutral: GetAIUnit keeps
// returning exactly the unit the linear scan over `units` would.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::RebuildAIUnitCache()
{
	worldToAIUnit.clear();
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( IsValid( *i ) && IsValid( (*i)->GetUnitServer() ) )
			worldToAIUnit[ (*i)->GetUnitServer() ] = i->GetPtr();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnPassControl( NWorld::CPlayer *_pPlayer )
{
	commandTracker.commands.clear();
	if ( _pPlayer == pPlayer )
		bAITurn = true;
	else
		bAITurn = false;
	//
	if ( bAITurn )
	{
		Synchronize();
		SyncAIState();
		eotLogics.clear();
		// retail CAICommander::OnPassControl (aiCommander.c:1612): fire the per-turn lua hook OnStartTurn(
		// scenarioPlayerID ) for the player now taking the turn.
		if ( IsValid( pPlayer ) )
			NScript::luaCallFunction( "OnStartTurn", "i", pPlayer->GetScenarioPlayerID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::Segment()
{
	// re-arm the one-decision-per-segment latch every world tick (SAICommandTracker::OnSegment @0x338b0).
	commandTracker.OnSegment();
	//
	// (The old dev-only bWantTurnBased latch is GONE: it re-armed on EVERY vision recompute, so the
	// game re-entered turn-based whenever an enemy stayed visible. Retail's ONLY realtime->TBS arm is
	// the transition-gated notice path -- SInterruptInfo::AddEvent on a NEW sighting (UpdateVisible
	// @0x3c4450) -> CWorld::CheckInterrupt @0x3684c0 -> WillWantTBS @0x3683e0 -- which this fork
	// already ports world-side; retail CAICommander has no TBS latch at all.)
	//
	// Segment @0x347e0: the AI thinks (OnAISegment) every 3rd world segment (the nAILag throttle -- this
	// is also GenerateCommand's `nAILag < 1` gate, so the round-robin only fires on the reset tick).
	++nAILag;
	if ( nAILag > 2 )
	{
		nAILag = 0;
		SyncAIState();
		OnAISegment();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnUnitDied( NWorld::CUnitServer *pUnit )
{
	// owner-only: the wrapper is SHARED across commanders now (retail identity model) -- without the
	// filter every commander's broadcast would OnDied() the same CAIUnit once per commander.
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetUnitServer() == pUnit && IsOwnUnit( *i ) )
			(*i)->OnDied();
	}
	// AI-convergence Stage 2: no tactical commander to notify -- the dead unit is filtered out by CanFight
	// everywhere (round-robin / CanAIOperateThisUnit / IsEndOfTurn) and pruned by Synchronize.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::RemoveUnit( NWorld::CUnitServer *pUS )
{
	for ( vector< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( (*i)->GetUnitServer() == pUS )
			i = units.erase( i );
		else
			++i;
	}
	// retail RemoveUnit @0x34910: drop the world-unit -> AI-unit cache entry alongside the units erase.
	worldToAIUnit.erase( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTBSEvent( NWorld::ETBSEvent event )
{
	if ( bForbidAI )
		return;
	//
	switch ( event )
	{
		case NWorld::TBS_START_NEW_TURN:
			OnTurnStarted();
			break;
		case NWorld::TBS_FINISH_OWN_TURN:
			OnTurnFinished();
			break;
		case NWorld::TBS_CANCEL_ACTION:
			OnCancelAction();
			break;
		case NWorld::TBS_START_REAL_TIME:
			OnStartRealTime();
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CAICommander::OnSeeUnit is GONE -- retail has NO OnSeeUnit anywhere (no PDB symbol, no commander
// vcall in UpdateVisible @0x3c4450). Its dev-only bWantTurnBased latch re-armed on every vision
// recompute; retail's realtime->TBS arm is the transition-gated SNotice path (AddEvent ->
// CheckInterrupt @0x3684c0 -> WillWantTBS @0x3683e0), fully ported world-side. Combat entry stays
// event-driven: CEventOnSeeNewEnemy -> tracker -> AddEnemy -> FindMostDangerousEnemy; see the
// GFirst 2026-07-07 root-fix note in git history for why no state-modified mark belongs here.)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::LockObject( NWorld::CObjectServerBase *pObject )
{
	if ( !IsObjectLocked( pObject ) )
		LockedObjects.push_back( pObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::UnLockObject( NWorld::CObjectServerBase *pObject )
{
	list< CPtr<NWorld::CObjectServerBase> >::iterator i =
		find( LockedObjects.begin(), LockedObjects.end(), pObject );
	if ( i != LockedObjects.end() )
		LockedObjects.erase( i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsObjectLocked( NWorld::CObjectServerBase *pObject )
{
	return find( LockedObjects.begin(), LockedObjects.end(), pObject ) != LockedObjects.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::UnLockAllObjects()
{
	LockedObjects.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (W5: the dev NAI signal layer was REMOVED outright -- retail has ZERO NAI::*Signal* symbols; retail routes
// every modeled stimulus through the ported CAIEventTracker events: OnHear*/OnBullet/OnAttack/CheckForVisibleCorpses.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33eb0: retail's O(1) lookup through the worldToAIUnit cache (was a dev O(n) linear scan over `units`).
// The cache is kept in lockstep with `units` at every commander-units mutation (OnUnitAdded / RemoveUnit /
// Synchronize), so this returns exactly what the linear scan would.
IAIUnit *CAICommander::GetAIUnit( NWorld::CUnitServer *pUnit )
{
	unordered_map< CPtr<NWorld::CUnitServer>, CPtr<IAIUnit>, SPtrHash >::iterator i = worldToAIUnit.find( pUnit );
	if ( i != worldToAIUnit.end() )
		return i->second;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::HasVisibleEnemies()
{
	list< CPtr<NWorld::CUnit> > visibleUnits;
	pPlayer->GetVisible( &visibleUnits );
	for ( list< CPtr<NWorld::CUnit> >::const_iterator i = visibleUnits.begin(); i != visibleUnits.end(); ++i )
	{
		NWorld::IPlayer *pEnemyPlayer = (*i)->GetPlayer();
		bool bDiplomacyEnemy = GetWorld()->GetDiplomacyState( pPlayer, pEnemyPlayer ) == NDb::DS_ENEMY;
		if ( pEnemyPlayer != pPlayer && bDiplomacyEnemy )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnStartRealTime()
{
	// AI-convergence Stage 2: the tactical commander's lost-enemy re-home (CheckForReleasableUnits) is gone.
	// The per-segment reaction pump now handles it: a unit whose SAIUnitState loses its enemy re-evaluates
	// (CAINormalReaction::Update -> AfterCombat/idle logic) on its next update.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::WantTurnBased()
{
	GetWorld()->WantTurnBased( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander @0x35b60 -- see aiCommander.h. Release-NEW; parity-only. The ctor chains
// CAICommander( world, 0 ): a sequence commander owns no AI player.
////////////////////////////////////////////////////////////////////////////////////////////////////
CSequenceCommander::CSequenceCommander( NWorld::CWorld *_pWorld ): CAICommander( _pWorld, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander::GenerateCommand @0x358d0 -- auto-drive the base commander ONLY while the world runs
// a cinematic sequence.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSequenceCommander::GenerateCommand()
{
	if ( GetWorld()->IsSequence() )
		CAICommander::GenerateCommand();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(aiCommander)
	REGISTER_VAR_EX( "game_noai", NGlobal::VarBoolHandler, &bForbidAI, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x02731170, CAICommander )
REGISTER_SAVELOAD_CLASS( 0x51823190, CSequenceCommander )
