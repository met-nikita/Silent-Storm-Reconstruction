#ifndef __AIUNITSTATE_H_
#define __AIUNITSTATE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIUnitState - the per-unit tactical "threat state" (release NAI::SAIUnitState, by-value member of
// CAIUnit; CAIUnit::GetAIUnitState @0x004ad180 returns &state). It tracks the units this unit knows about
// (enemies / possible enemies / allies) and derives the current most-dangerous enemy, nearest ally and a
// "scared" morale flag - the inputs the reaction layer (CAINormalReaction::Update @0x0047f190 + the
// Defence/Fear/Retreat/Guard reactions) uses to choose the unit's logic.
//
// FIDELITY/SCOPE: the struct layout + the derive methods (FindMostDangerousEnemy/FindNearestAlly/
// CheckScared/Update) follow the release (reconstruction/exports/aiunitstate_*.txt). The release maintains
// the lists incrementally through the AI event system (Notify/OnAIEvent -> AddEnemy/AddAlly) and a
// visibility sweep (PrepareEnemies); that event layer is not ported, so Populate() here refreshes the
// lists each think from the AI players (the same enemy/ally sets the commander already uses). The
// SUnitsAndPositions per-unit position cache and the SModifiable lazy-recompute lock are kept structurally
// but driven eagerly. State is transient on CAIUnit (rebuilt each turn), so it is not serialized.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiPosition.h"     // SUnitPosition
namespace NAI
{
class IAIUnit;
struct SAIState;
////////////////////////////////////////////////////////////////////////////////////////////////////
// SModifiable<T> - a value with a dirty flag + a lock that defers clearing the flag during iteration.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct SModifiable
{
	int  nLock;
	bool bModified;
	T    data;
	SModifiable(): nLock( 0 ), bModified( false ) {}
	void SetModified() { bModified = true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitsAndPositions
{
	vector< CPtr<IAIUnit> > units;   // release also caches a unit->SUnitPosition map; deferred
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUnitState
{
	CPtr<IAIUnit>                   pUnit;
	SModifiable<bool>               selfModified;     // release "bModified": set when pEnemy/pAlly change
	SModifiable<SUnitsAndPositions> enemies;
	SModifiable<SUnitsAndPositions> possibleEnemies;
	SModifiable<SUnitsAndPositions> allies;
	bool                            bHelpCalled;
	CPtr<IAIUnit>                   pEnemy;           // most dangerous known enemy
	CPtr<IAIUnit>                   pPossibleEnemy;
	CPtr<IAIUnit>                   pAlly;            // nearest ally
	bool                            bScared;
	vector< CPtr<IAIUnit> >         knownCorpses;
	//
	SAIUnitState();
	void SetUnit( IAIUnit *_pUnit ) { pUnit = _pUnit; }
	//
	void AddEnemy( IAIUnit *p );           void RemoveEnemy( IAIUnit *p );
	void AddPossibleEnemy( IAIUnit *p );   void RemovePossibleEnemy( IAIUnit *p );
	void AddAlly( IAIUnit *p );            void RemoveAlly( IAIUnit *p );
	bool IsKnownCorpse( IAIUnit *p ) const;
	void AddKnownCorpse( IAIUnit *p );
	//
	void Populate();      // refresh enemies/allies from the AI players (release: events + PrepareEnemies)
	void Update();        // recompute pEnemy/pPossibleEnemy/pAlly when lists changed, then CheckScared
	void Reset();
	// AI-convergence Stage 2: the commander's reaction pump reads this dirty flag. IsModified @0xb0550
	// reports "the derived threat changed since last processed"; the commander's GetReactionForUpdate
	// consumes it (ClearModified) when it enqueues the unit's reaction. selfModified is SET by
	// FindMostDangerousEnemy/FindNearestAlly (a pEnemy/pAlly change) and OnSequenceFinished.
	bool IsModified() const { return selfModified.bModified; }   // @0xb0550 (dev: the dirty flag)
	void ClearModified() { selfModified.bModified = false; selfModified.data = false; }
	const vector< CPtr<IAIUnit> >& GetKnownEnemies() const { return enemies.data.units; }
private:
	void FindMostDangerousEnemy();         // @0x004b0b10  -> pEnemy
	void FindNearestPossibleEnemy();       // @0x004b08d0  -> pPossibleEnemy
	void FindNearestAlly();                // @0x004b0810  -> pAlly
	void CheckScared();                    // @0x004b0ea0  -> bScared
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AIUNITSTATE_H_
