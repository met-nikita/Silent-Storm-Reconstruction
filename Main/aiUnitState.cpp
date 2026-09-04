#include "StdAfx.h"
//
#include "aiUnitState.h"
#include "aiUnit.h"        // IAIUnit
#include "aiMisc.h"        // GetAIUnit
#include "aiState.h"       // SAIState
#include "aiPlayer.h"      // IAIPlayer::GetUnits / IsContain
#include "wUnitServer.h"   // CanFight
#include "..\DBFormat\DataRPG.h"  // NDb::EShootMode -- BEFORE aiInventory.h (its NDB:: fwd-decl typo)
#include "..\DBFormat\DataMap.h"  // NDb::DS_ENEMY
#include "aiInventory.h"   // CAIInventory::GetBestFireArms (the FindMostDangerousEnemy to-hit metric)
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitState - per-unit threat tracker. See aiUnitState.h for the fidelity/scope notes (event-driven
// maintenance + the position cache + the morale-skill half of CheckScared are simplified).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsAlive( IAIUnit *p ) { return IsValid( p ) && !p->IsDead(); }
static bool IsFightable( IAIUnit *p ) { return IsValid( p ) && IsValid( p->GetUnitServer() ) && p->GetUnitServer()->CanFight(); }
static void AddUnique( vector< CPtr<IAIUnit> > *pv, IAIUnit *p )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = pv->begin(); i != pv->end(); ++i )
		if ( (*i).GetPtr() == p ) return;
	pv->push_back( p );
}
static void RemoveFrom( vector< CPtr<IAIUnit> > *pv, IAIUnit *p )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = pv->begin(); i != pv->end(); ++i )
		if ( (*i).GetPtr() == p ) { pv->erase( i ); return; }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x004b05c0: the shared "nearest live unit by CP distance" scan. The release factors FindNearestAlly
// (@0x004b0810) and FindNearestPossibleEnemy (@0x004b08d0) through this free fn (each calls it with its
// list, then keeps its own pAlly/pPossibleEnemy + selfModified bookkeeping); the dev keeps those two
// callers inlined, so this is added as the behaviour-neutral parity surface. Sentinel 65535 (no unit on a
// tactical map is that far). Skips null/dead candidates (the release's IsDead bit-0x80 test == !IsAlive()).
IAIUnit *FindNearestUnit( IAIUnit *pSelf, vector< CPtr<IAIUnit> > &units )
{
	if ( !IsValid( pSelf ) )
		return 0;
	IAIUnit *pBest = 0;
	float fBest = 65535.0f;
	CVec3 me = pSelf->GetPosition().GetCP();
	for ( vector< CPtr<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsAlive( *i ) )
			continue;
		float d = fabs( (*i)->GetPosition().GetCP() - me );
		if ( d < fBest ) { fBest = d; pBest = (*i).GetPtr(); }
	}
	return pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SAIUnitState::SAIUnitState(): bHelpCalled( false ), bScared( false ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::AddEnemy( IAIUnit *p )         { if ( IsValid( p ) ) { AddUnique( &enemies.data.units, p ); enemies.SetModified(); } }
void SAIUnitState::RemoveEnemy( IAIUnit *p )      { RemoveFrom( &enemies.data.units, p ); enemies.SetModified(); }
void SAIUnitState::AddPossibleEnemy( IAIUnit *p ) { if ( IsValid( p ) ) { AddUnique( &possibleEnemies.data.units, p ); possibleEnemies.SetModified(); } }
void SAIUnitState::RemovePossibleEnemy( IAIUnit *p ) { RemoveFrom( &possibleEnemies.data.units, p ); possibleEnemies.SetModified(); }
void SAIUnitState::AddAlly( IAIUnit *p )          { if ( IsValid( p ) && p != pUnit.GetPtr() ) { AddUnique( &allies.data.units, p ); allies.SetModified(); } }   // retail AddAlly @0xb1630: never yourself
void SAIUnitState::RemoveAlly( IAIUnit *p )       { RemoveFrom( &allies.data.units, p ); allies.SetModified(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAIUnitState::IsKnownCorpse( IAIUnit *p ) const
{
	for ( vector< CPtr<IAIUnit> >::const_iterator i = knownCorpses.begin(); i != knownCorpses.end(); ++i )
		if ( (*i).GetPtr() == p ) return true;
	return false;
}
void SAIUnitState::AddKnownCorpse( IAIUnit *p ) { if ( IsValid( p ) && !IsKnownCorpse( p ) ) knownCorpses.push_back( p ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Reset()
{
	enemies.data.units.clear();         enemies.SetModified();
	possibleEnemies.data.units.clear(); possibleEnemies.SetModified();
	allies.data.units.clear();          allies.SetModified();
	pEnemy = 0; pPossibleEnemy = 0; pAlly = 0;
	bScared = false; bHelpCalled = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail SAIUnitState::PrepareEnemies @0x004b17a0.  At the start of a turn, promote every
// live hostile in this unit's own CTBSUnitVision list to a confirmed enemy.  Confirmed enemies
// that vanished from that list become possible enemies.  This deliberately does not depend on
// SAIState's commander rosters: StartGame raises the begin-turn event before those transient
// back-pointers have necessarily been synchronized.
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::PrepareEnemies()
{
	if ( !IsAlive( pUnit ) )
		return;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;

	vector< CPtr<IAIUnit> > vanished = enemies.data.units;
	const list< CPtr<NWorld::CUnitServer> > &visible = pUS->GetTBSVisible();
	for ( list< CPtr<NWorld::CUnitServer> >::const_iterator i = visible.begin(); i != visible.end(); ++i )
	{
		NWorld::CUnitServer *pSeen = (*i).GetPtr();
		if ( !IsValid( pSeen ) || !pSeen->CanFight() )
			continue;
		IAIUnit *pAI = GetAIUnit( pSeen );
		if ( !IsAlive( pAI ) || pUS->GetDiplomacyState( pSeen ) != NDb::DS_ENEMY )
			continue;
		RemoveFrom( &vanished, pAI );
		RemovePossibleEnemy( pAI );
		AddEnemy( pAI );
	}
	for ( vector< CPtr<IAIUnit> >::iterator i = vanished.begin(); i != vanished.end(); ++i )
	{
		RemoveEnemy( *i );
		AddPossibleEnemy( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Update()   // @0x004b1030
{
	if ( enemies.bModified )         { FindMostDangerousEnemy();   if ( enemies.nLock < 1 )         enemies.bModified = false; }
	if ( possibleEnemies.bModified ) { FindNearestPossibleEnemy(); if ( possibleEnemies.nLock < 1 ) possibleEnemies.bModified = false; }
	if ( allies.bModified )          { FindNearestAlly();          if ( allies.nLock < 1 )          allies.bModified = false; }
	CheckScared();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x004b0b10 -- CONVERGED to the retail two-phase pick (was plain nearest-live-enemy, which FLIPPED
// pEnemy between advancing player units on every step -> selfModified -> CheckForUpdates cancelled the
// in-flight attack plan mid-approach -> the AI ran its shoot approach over and over and never fired):
//  phase 1: the NEAREST CLUSTER -- every valid, fightable enemy whose distance is within +-1.5 units
//           (@0xb0b10 const 0x3fc00000) of the closest one; a decisively closer enemy resets the cluster.
//  phase 2: within the cluster, the enemy the unit's own weapons hit BEST (CAIInventory::GetBestFireArms
//           max to-hit; the first candidate always beats the empty pick). Stable across an approach: the
//           winner only changes when the cluster membership or the to-hit ordering genuinely changes.
void SAIUnitState::FindMostDangerousEnemy()
{
	vector< CPtr<IAIUnit> > cluster;
	float fBest = 4095.0f;
	if ( IsValid( pUnit ) )
	{
		CVec3 me = pUnit->GetPosition().GetCP();
		for ( vector< CPtr<IAIUnit> >::iterator i = enemies.data.units.begin(); i != enemies.data.units.end(); ++i )
		{
			if ( !IsFightable( *i ) )
				continue;
			float d = fabs( (*i)->GetPosition().GetCP() - me );
			if ( fabs( d - fBest ) >= 1.5f )
			{
				if ( d < fBest )
				{
					cluster.clear();
					cluster.push_back( *i );
					fBest = d;
				}
			}
			else
				cluster.push_back( *i );
		}
	}
	CPtr<IAIUnit> pBest = 0;
	int nBestHit = 0;
	for ( int k = 0; k < (int)cluster.size(); ++k )
	{
		IAIUnit *e = cluster[k].GetPtr();
		int nToHit = 0, nHitCover = 0, nQuality = 0;
		NDb::EShootMode eMode = (NDb::EShootMode)0;
		CAIInventory *pInv = IsValid( pUnit ) ? pUnit->GetAIInventory() : 0;
		if ( IsValid( pInv ) && IsValid( e->GetUnitServer() ) )
			pInv->GetBestFireArms( pUnit->GetUnitPosition(), e, pUnit->GetAP(), &nHitCover, &nQuality, &eMode, &nToHit );
		if ( nToHit > nBestHit || !IsValid( pBest ) )
		{
			nBestHit = nToHit;
			pBest = e;
		}
	}
	if ( pEnemy.GetPtr() != pBest.GetPtr() )
		selfModified.SetModified();
	pEnemy = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::FindNearestPossibleEnemy()   // @0x004b08d0
{
	CPtr<IAIUnit> pBest = 0;
	float fBest = float( 0xFFF );
	if ( IsValid( pUnit ) )
	{
		CVec3 me = pUnit->GetPosition().GetCP();
		for ( vector< CPtr<IAIUnit> >::iterator i = possibleEnemies.data.units.begin(); i != possibleEnemies.data.units.end(); ++i )
			if ( IsAlive( *i ) )
			{
				float d = fabs( (*i)->GetPosition().GetCP() - me );
				if ( d < fBest ) { fBest = d; pBest = *i; }
			}
	}
	// retail @0x4b08d0: a CHANGED winner marks the state modified, exactly as FindMostDangerousEnemy /
	// FindNearestAlly do -- this is what re-fires the reaction (the HUNT rung reads pPossibleEnemy) when a
	// suspect appears, resolves, or is superseded. The dev predecessor silently updated the pointer.
	if ( pPossibleEnemy.GetPtr() != pBest.GetPtr() )
		selfModified.SetModified();
	pPossibleEnemy = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::FindNearestAlly()   // @0x004b0810
{
	CPtr<IAIUnit> pBest = 0;
	float fBest = float( 0xFFF );
	if ( IsValid( pUnit ) )
	{
		CVec3 me = pUnit->GetPosition().GetCP();
		for ( vector< CPtr<IAIUnit> >::iterator i = allies.data.units.begin(); i != allies.data.units.end(); ++i )
			if ( IsAlive( *i ) )
			{
				float d = fabs( (*i)->GetPosition().GetCP() - me );
				if ( d < fBest ) { fBest = d; pBest = *i; }
			}
	}
	if ( pAlly.GetPtr() != pBest.GetPtr() )
		selfModified.SetModified();
	pAlly = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x004b0ea0: scared when there is an enemy but no support. The release also factors a morale/skill
// check (CDynamicSkill < 0.5*max); here it is the "outnumbered + no ally within 9 units" half.
void SAIUnitState::CheckScared()
{
	if ( bScared || bHelpCalled )
		return;
	if ( !IsAlive( pEnemy ) || !IsValid( pUnit ) )
		return;
	const int   N_SCARE_ENEMIES = 8;
	const float F_ALLY_NEAR = 9.0f;
	if ( (int)enemies.data.units.size() <= N_SCARE_ENEMIES )
		return;
	CVec3 me = pUnit->GetPosition().GetCP();
	// Retail @0xb0ea0 queries the WORLD roster for teammates in range (SAIState::GetUnitsAtRange(cp, 9.0,
	// exclude self)) -- NOT s.allies (which is the event-only help-caller list). Walk the unit's own team.
	SAIState *pSt = pUnit->GetAIState();
	IAIPlayer *pAllyPlayer = IsValid( pSt ) ? pSt->GetAllyAIPlayer() : 0;
	IAIPlayer *pEnemyPlayer = IsValid( pSt ) ? pSt->GetEnemyAIPlayer() : 0;
	const bool bUnitInAlly = IsValid( pAllyPlayer ) && pAllyPlayer->IsContain( pUnit );
	IAIPlayer *pMine = bUnitInAlly ? pAllyPlayer : pEnemyPlayer;
	if ( IsValid( pMine ) )
	{
		vector< CPtr<IAIUnit> > *pTeam = pMine->GetUnits();
		for ( vector< CPtr<IAIUnit> >::iterator i = pTeam->begin(); i != pTeam->end(); ++i )
			if ( (*i).GetPtr() != pUnit.GetPtr() && IsAlive( *i )
				&& fabs( (*i)->GetPosition().GetCP() - me ) < F_ALLY_NEAR )
				return;   // an ally is close -> hold
	}
	bScared = true;
	selfModified.SetModified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
