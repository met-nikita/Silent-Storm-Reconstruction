#include "StdAfx.h"
//
#include "aiUnitState.h"
#include "aiUnit.h"        // IAIUnit
#include "aiState.h"       // IAIState
#include "aiPlayer.h"      // IAIPlayer::GetUnits / IsContain
#include "wUnitServer.h"   // CanFight
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitState - per-unit threat tracker. See aiUnitState.h for the fidelity/scope notes (event-driven
// maintenance + the position cache + the morale-skill half of CheckScared are simplified; the derive
// methods + Populate keep enemies/allies/pEnemy/pAlly/bScared correct each think).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsAlive( IAIUnit *p ) { return IsValid( p ) && !p->IsDead(); }
static bool IsFightable( IAIUnit *p ) { return IsValid( p ) && IsValid( p->GetUnitServer() ) && p->GetUnitServer()->CanFight(); }
// fog of war: is enemy `e` in the seer's currently-visible set?
static bool CanSee( const vector< CPtr<NWorld::CUnit> > &visible, IAIUnit *e )
{
	if ( !IsValid( e ) || !IsValid( e->GetUnitServer() ) )
		return false;
	NWorld::CUnit *pServer = e->GetUnitServer();   // CUnitServer is-a CUnit
	for ( vector< CPtr<NWorld::CUnit> >::const_iterator i = visible.begin(); i != visible.end(); ++i )
		if ( (*i).GetPtr() == pServer )
			return true;
	return false;
}
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
void SAIUnitState::AddAlly( IAIUnit *p )          { if ( IsValid( p ) ) { AddUnique( &allies.data.units, p ); allies.SetModified(); } }
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
// Refresh the threat lists from the AI players, FOG-OF-WAR limited: the unit only knows the enemies it can
// currently SEE (CUnitServer's per-unit visibility, kept up to date by UpdateVisible for interrupts). This
// is the dev equivalent of the release's event-maintained seen-set (CAIEventTracker::OnSeeEnemy/OnLostEnemy
// -> SAIUnitState AddEnemy/RemoveEnemy); the release's transient-stimulus events (bullet/grenade/heard) have
// no dev emitter and are not modelled. Enemies the unit no longer sees but saw last think are remembered as
// possible enemies. Allies are full (own team shares positions).
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Populate()
{
	bScared = false;   // re-evaluate scared from the current situation each think (CheckScared latches it)
	vector< CPtr<IAIUnit> > seenLastThink = enemies.data.units;   // for the lost-from-sight -> possible memory
	enemies.data.units.clear();         enemies.SetModified();
	// possibleEnemies are now EVENT-managed (the reconstructed OnBullet/OnHear/OnGrenade/OnLostEnemy handlers
	// AddPossibleEnemy a NON-visible threat; LostPossibleEnemy/EnemyDied/Enemy remove it). Do NOT clear them here:
	// the dev's predecessor-era poll wiped every event suspect each think, which made the whole reactive layer
	// inert (a shot-from-concealment was forgotten before the unit could act). Just drop suspects that resolved on
	// their own -- ones whose unit is no longer valid/alive. (Retail SAIUnitState has no Populate at all; this hybrid
	// keeps the dev's cheap poll for the visible enemies/allies and lets events own the non-visible suspects.)
	possibleEnemies.SetModified();
	for ( int k = (int)possibleEnemies.data.units.size() - 1; k >= 0; --k )
		if ( !IsValid( possibleEnemies.data.units[ k ] ) )
			possibleEnemies.data.units.erase( possibleEnemies.data.units.begin() + k );
	allies.data.units.clear();          allies.SetModified();
	IAIState *pState = IsValid( pUnit ) ? pUnit->GetAIState() : 0;
	if ( !IsValid( pState ) )
		return;
	IAIPlayer *pAllyPlayer = pState->GetAllyAIPlayer();
	IAIPlayer *pEnemyPlayer = pState->GetEnemyAIPlayer();
	const bool bUnitInAlly = IsValid( pAllyPlayer ) && pAllyPlayer->IsContain( pUnit );
	IAIPlayer *pMine = bUnitInAlly ? pAllyPlayer : pEnemyPlayer;
	IAIPlayer *pFoes = bUnitInAlly ? pEnemyPlayer : pAllyPlayer;
	// the unit's currently-visible set (fog of war)
	vector< CPtr<NWorld::CUnit> > visible;
	if ( IsValid( pUnit ) && IsValid( pUnit->GetUnitServer() ) )
		pUnit->GetUnitServer()->GetVisible( &visible );
	if ( IsValid( pFoes ) )
	{
		vector< CPtr<IAIUnit> > *pUnits = pFoes->GetUnits();
		for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
		{
			if ( !IsFightable( *i ) )
				continue;
			if ( CanSee( visible, *i ) )
				AddEnemy( *i );                          // seen -> a known enemy
			else
			{
				for ( vector< CPtr<IAIUnit> >::iterator j = seenLastThink.begin(); j != seenLastThink.end(); ++j )
					if ( (*j).GetPtr() == (*i).GetPtr() ) { AddPossibleEnemy( *i ); break; }   // lost from sight -> remembered
			}
		}
	}
	if ( IsValid( pMine ) )
	{
		vector< CPtr<IAIUnit> > *pUnits = pMine->GetUnits();
		for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
			if ( (*i).GetPtr() != pUnit.GetPtr() && IsFightable( *i ) )
				AddAlly( *i );
	}
	// promote: a suspect that is now a visible/known enemy this think is no longer a mere "possible enemy"
	// (mirrors retail's Enemy event RemovePossibleEnemy). Bounds the preserved set together with the invalid-prune
	// above and the LostPossibleEnemy events the reactions raise once a suspect is investigated and found empty.
	for ( int k = (int)possibleEnemies.data.units.size() - 1; k >= 0; --k )
	{
		bool bNowSeen = false;
		for ( vector< CPtr<IAIUnit> >::iterator e = enemies.data.units.begin(); e != enemies.data.units.end(); ++e )
			if ( (*e).GetPtr() == possibleEnemies.data.units[ k ].GetPtr() ) { bNowSeen = true; break; }
		if ( bNowSeen )
			possibleEnemies.data.units.erase( possibleEnemies.data.units.begin() + k );
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
// @0x004b0b10: the release scores each enemy by the best to-hit of the unit's weapons at the nearest
// cluster; reconstructed here as the nearest live enemy (the cluster + to-hit metric is deferred).
void SAIUnitState::FindMostDangerousEnemy()
{
	CPtr<IAIUnit> pBest = 0;
	float fBest = float( 0xFFF );
	if ( IsValid( pUnit ) )
	{
		CVec3 me = pUnit->GetPosition().GetCP();
		for ( vector< CPtr<IAIUnit> >::iterator i = enemies.data.units.begin(); i != enemies.data.units.end(); ++i )
			if ( IsAlive( *i ) )
			{
				float d = fabs( (*i)->GetPosition().GetCP() - me );
				if ( d < fBest ) { fBest = d; pBest = *i; }
			}
	}
	if ( pEnemy.GetPtr() != pBest.GetPtr() )
		selfModified.SetModified();
	pEnemy = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::FindNearestPossibleEnemy()
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
	for ( vector< CPtr<IAIUnit> >::iterator i = allies.data.units.begin(); i != allies.data.units.end(); ++i )
		if ( IsAlive( *i ) && fabs( (*i)->GetPosition().GetCP() - me ) < F_ALLY_NEAR )
			return;   // an ally is close -> hold
	bScared = true;
	selfModified.SetModified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
