#include "StdAfx.h"
//
#include "aiUnitState.h"
#include "aiUnit.h"        // IAIUnit
#include "aiEvent.h"
#include "aiMisc.h"        // GetAIUnit
#include "aiState.h"       // SAIState
#include "aiPlayer.h"      // IAIPlayer::GetUnits / IsContain
#include "wUnitServer.h"   // CanFight
#include "wMain.h"
#include "RPGUnit.h"
#include "..\DBFormat\DataRPG.h"  // NDb::EShootMode -- BEFORE aiInventory.h (its NDB:: fwd-decl typo)
#include "..\DBFormat\DataMap.h"  // NDb::DS_ENEMY
#include "aiInventory.h"   // CAIInventory::GetBestFireArms (the FindMostDangerousEnemy to-hit metric)
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitState - per-unit threat tracker. See aiUnitState.h for the fidelity/scope notes (event-driven
// maintenance and the position cache follow retail's membership protocol).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsFightable( IAIUnit *p ) { return IsValid( p ) && IsValid( p->GetUnitServer() ) && p->GetUnitServer()->CanFight(); }
static void RemoveFrom( vector< CPtr<IAIUnit> > *pv, IAIUnit *p )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = pv->begin(); i != pv->end(); ++i )
		if ( (*i).GetPtr() == p ) { pv->erase( i ); return; }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x4b0a00: shared nearest-contact scan, using live world positions.
// The bit-0x80 test is CObjectBase validity, NOT the unit's health/death state.
// Contact-removal events own membership; do not silently filter valid corpses here.
IAIUnit *FindNearestUnit( IAIUnit *pSelf, vector< CPtr<IAIUnit> > &units )
{
	if ( !IsValid( pSelf ) )
		return 0;
	IAIUnit *pBest = 0;
	float fBest = 65535.0f;
	CVec3 me = pSelf->GetUnitServer()->GetPosition().GetCP();
	for ( vector< CPtr<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) )
			continue;
		float d = fabs( (*i)->GetUnitServer()->GetPosition().GetCP() - me );
		if ( d < fBest ) { fBest = d; pBest = (*i).GetPtr(); }
	}
	return pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SAIUnitState::SAIUnitState(): bHelpCalled( false ), bScared( false )
{
	selfModified.data = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SUnitsAndPositions::IsContain( IAIUnit *pUnit, bool bCheckPosition ) const
{
	for ( vector< CPtr<IAIUnit> >::const_iterator i = units.begin(); i != units.end(); ++i )
		if ( (*i).GetPtr() == pUnit )
		{
			if ( !bCheckPosition )
				return true;
			unordered_map< CPtr<IAIUnit>, SUnitPosition, SPtrHash >::const_iterator pos = positions.find( pUnit );
			return pos != positions.end() && pos->second == pUnit->GetUnitPosition();
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SUnitsAndPositions::Remove( IAIUnit *pUnit )
{
	units.erase( remove( units.begin(), units.end(), CPtr<IAIUnit>( pUnit ) ), units.end() );
	positions.erase( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SUnitsAndPositions::Add( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return;
	Remove( pUnit );
	units.push_back( pUnit );
	positions[pUnit] = pUnit->GetUnitPosition();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Synchronize()
{
	// Retail v1.2 0x4b09a0. Do not refresh merely suspected contacts or allies.
	for ( vector< CPtr<IAIUnit> >::iterator i = enemies.data.units.begin(); i != enemies.data.units.end(); ++i )
		if ( IsValid( *i ) )
			(*i)->Synchronize( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::OnAIEvent( IAIEvent *pEvent )
{
	// Retail v1.2 0x4b0dd0: the alarm's immediate-delivery path has no locks.
	if ( !IsValid( pEvent ) )
		return;
	CPtr<IAIEvent> pHold = pEvent;
	if ( IsValid( pUnit ) && IsValid( pUnit->GetUnitServer() ) && pUnit->GetUnitServer()->CanFight() )
		pEvent->Modify( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Notify( IAIEvent *pEvent )
{
	// Retail v1.2 0x4b0e80: keep the event alive and defer collection-dirty
	// consumption while its Modify callback (including nested delivery) runs.
	if ( !IsValid( pEvent ) )
		return;
	CPtr<IAIEvent> pHold = pEvent;
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) || !pUnit->GetUnitServer()->CanFight() )
		return;
	++enemies.nLock;
	++possibleEnemies.nLock;
	++allies.nLock;
	pEvent->Modify( this );
	--enemies.nLock;
	--possibleEnemies.nLock;
	--allies.nLock;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::AddEnemy( IAIUnit *p )
{
	if ( IsValid( pUnit ) && IsFightable( p ) && pUnit->GetDiplomacyState( p ) == NDb::DS_ENEMY
		&& !enemies.data.IsContain( p, true ) )
	{
		enemies.SetModified();
		enemies.data.Add( p );
	}
}
// Retail v1.2 0x4b1630/0x4b1690/0x4b16f0 also clear the matching selected contact
// immediately, without marking selfModified. Deferring the clear until Update()
// would report a new threat change and cancel the investigation that consumed it.
void SAIUnitState::RemoveEnemy( IAIUnit *p )
{
	if ( enemies.data.IsContain( p, false ) )
	{
		enemies.SetModified();
		enemies.data.Remove( p );
	}
	if ( pEnemy.GetPtr() == p )
		pEnemy = 0;
}
void SAIUnitState::AddPossibleEnemy( IAIUnit *p )
{
	if ( IsValid( pUnit ) && IsFightable( p ) && pUnit->GetDiplomacyState( p ) == NDb::DS_ENEMY
		&& !enemies.data.IsContain( p, false ) && !possibleEnemies.data.IsContain( p, true ) )
	{
		possibleEnemies.SetModified();
		possibleEnemies.data.Add( p );
	}
}
void SAIUnitState::RemovePossibleEnemy( IAIUnit *p )
{
	if ( possibleEnemies.data.IsContain( p, false ) )
	{
		possibleEnemies.SetModified();
		possibleEnemies.data.Remove( p );
	}
	if ( pPossibleEnemy.GetPtr() == p )
		pPossibleEnemy = 0;
}
void SAIUnitState::AddAlly( IAIUnit *p )
{
	if ( p != pUnit.GetPtr() && IsValid( pUnit ) && IsFightable( p ) && pUnit->GetDiplomacyState( p ) != NDb::DS_ENEMY
		&& !allies.data.IsContain( p, true ) )
	{
		allies.SetModified();
		allies.data.Add( p );
	}
}
void SAIUnitState::RemoveAlly( IAIUnit *p )
{
	if ( allies.data.IsContain( p, false ) )
	{
		allies.SetModified();
		allies.data.Remove( p );
	}
	if ( pAlly.GetPtr() == p )
		pAlly = 0;
}
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
	// Retail v1.2 0x4b09f0: consume the value, not the wrapper's dirty flag.
	selfModified.data = false;
	if ( selfModified.nLock < 1 )
		selfModified.bModified = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::Modified()
{
	// Retail v1.2 0x4b09d0: a lock suppresses dirty tracking, not the value write.
	if ( selfModified.nLock < 1 )
		selfModified.bModified = true;
	selfModified.data = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 SAIUnitState::PrepareEnemies @0x004b1bb0. At the start of a turn, promote every
// live hostile in this unit's own CTBSUnitVision list to a confirmed enemy.  Confirmed enemies
// that vanished from that list become possible enemies.  This deliberately does not depend on
// SAIState's commander rosters: StartGame raises the begin-turn event before those transient
// back-pointers have necessarily been synchronized.
// Retail's AI-wrapper tests are object validity, not IAIUnit::IsDead(): planning HP can be zero
// while the live server can still fight. Filtering that cache strands visible targets as suspects.
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::PrepareEnemies()
{
	if ( !IsValid( pUnit ) )
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
		if ( !IsValid( pAI ) || pUnit->GetDiplomacyState( pAI ) != NDb::DS_ENEMY )
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
		Modified();
	pEnemy = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::FindNearestPossibleEnemy()   // @0x004b08d0
{
	CPtr<IAIUnit> pBest = FindNearestUnit( pUnit, possibleEnemies.data.units );
	// retail @0x4b08d0: a CHANGED winner marks the state modified, exactly as FindMostDangerousEnemy /
	// FindNearestAlly do -- this is what re-fires the reaction (the HUNT rung reads pPossibleEnemy) when a
	// suspect appears, resolves, or is superseded. The dev predecessor silently updated the pointer.
	if ( pPossibleEnemy.GetPtr() != pBest.GetPtr() )
		Modified();
	pPossibleEnemy = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitState::FindNearestAlly()   // @0x004b0810
{
	CPtr<IAIUnit> pBest = FindNearestUnit( pUnit, allies.data.units );
	if ( pAlly.GetPtr() != pBest.GetPtr() )
		Modified();
	pAlly = pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x4b12e0: wounded below half VP OR facing more than two enemies,
// without a fight-capable non-enemy within nine units. Panzerkleins do not scare.
void SAIUnitState::CheckScared()
{
	if ( bScared || bHelpCalled )
		return;
	if ( !IsValid( pEnemy ) || !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return;
	if ( IsValid( pUnit->GetUnitServer()->GetWearingDBPK() ) )
		return;
	NRPG::CUnit *pRPG = pUnit->GetRPGUnit();
	if ( !IsValid( pRPG ) )
		return;
	const NRPG::CDynamicSkill &vp = pRPG->Skills( NDb::ST_VP );
	const bool bWounded = int( vp ) < 0.5 * vp.GetMaxValue();
	const float F_ALLY_NEAR = 9.0f;
	// Retail compares the vector's BYTE span to 8, i.e. more than TWO pointers.
	if ( !bWounded && enemies.data.units.size() <= 2 )
		return;
	CVec3 me = pUnit->GetPosition().GetCP();
	// GetUnitsAtRange(..., true, self) enumerates fight-capable WORLD units,
	// including non-enemy players, not just the owning commander's team roster.
	SAIState *pSt = pUnit->GetAIState();
	NWorld::CWorld *pWorld = pSt ? pSt->GetWorld() : 0;
	if ( IsValid( pWorld ) )
	{
		list< CPtr<NWorld::CUnitServer> > units;
		pWorld->GetAllUnits( &units );
		for ( list< CPtr<NWorld::CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
		{
			if ( !IsValid( *i ) || !(*i)->CanFight()
				|| pWorld->GetDiplomacyState( pSt->pPlayer, (*i)->GetPlayer() ) == NDb::DS_ENEMY )
				continue;
			IAIUnit *pOther = GetAIUnit( *i );
			if ( IsValid( pOther ) && pOther != pUnit.GetPtr()
				&& fabs( pOther->GetPosition().GetCP() - me ) < F_ALLY_NEAR )
				return;
		}
	}
	bScared = true;
	Modified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
vector< CPtr<IAIUnit> > SAIUnitState::GetKnownEnemies() const
{
	// Retail v1.2 0x4b1df0 inserts possible enemies at the beginning, then known enemies.
	vector< CPtr<IAIUnit> > result = possibleEnemies.data.units;
	result.insert( result.end(), enemies.data.units.begin(), enemies.data.units.end() );
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
