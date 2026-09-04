#include "StdAfx.h"
#include "aiEvent.h"
#include "aiUnit.h"          // IAIUnit
#include "aiMisc.h"          // NAI::GetAIUnit( CUnitServer* )
#include "wUnitServer.h"     // NWorld::CUnitServer
#include "wDumbUnit.h"       // CDumbUnitServer::GetWorld
#include "wMain.h"           // NWorld::CWorld::IsRealTime (via TTBSWorld)

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- Modify bodies (vtbl+0x10): each re-targets the release SAIUnitStateR mutators onto the real
// dev SAIUnitState methods.

// @0x3b920: the begin-turn refresh.
void CAIBeginTurnEvent::Modify( SAIUnitState *pState )
{
	pState->PrepareEnemies();
}

// @0x3b940: mark the state changed so the next Update() recomputes the derived enemy/ally.
void CAIUpdateEvent::Modify( SAIUnitState *pState )
{
	pState->selfModified.SetModified();
}

// @0x3b930: the first help call (or any while scared) sets bHelpCalled, clears bScared, marks dirty.
void CAIHelpCalledEvent::Modify( SAIUnitState *pState )
{
	if ( pState->bScared || !pState->bHelpCalled )
	{
		pState->bHelpCalled = true;
		pState->bScared = false;
		pState->selfModified.SetModified();
	}
}

// @0x3bf70: a confirmed enemy stops being a suspect.
void CAIEnemyEvent::Modify( SAIUnitState *pState )
{
	pState->RemovePossibleEnemy( pEnemy.GetPtr() );
	pState->AddEnemy( pEnemy.GetPtr() );
}

// @0x3c020: in real-time mode a lost enemy downgrades to a suspect; in turn-based the begin-turn
// refresh handles it (so skip).
void CAILostEnemyEvent::Modify( SAIUnitState *pState )
{
	IAIUnit *p = pEnemy.GetPtr();
	if ( !IsValid( p ) )
		return;
	NWorld::CUnitServer *pUS = p->GetUnitServer();
	if ( pUS == 0 || !pUS->GetWorld()->IsRealTime() )
		return;
	pState->RemoveEnemy( p );
	pState->AddPossibleEnemy( p );
}

// @0x3c0f0: record a new suspect and mark dirty.
void CAIPossibleEnemyEvent::Modify( SAIUnitState *pState )
{
	pState->AddPossibleEnemy( pEnemy.GetPtr() );
	pState->selfModified.SetModified();
}

// @0x3c190.
void CAILostPossibleEnemyEvent::Modify( SAIUnitState *pState )
{
	pState->RemovePossibleEnemy( pEnemy.GetPtr() );
}

// @0x3c220: a dead enemy leaves both the enemy and suspect sets.
void CAIEnemyDiedEvent::Modify( SAIUnitState *pState )
{
	pState->RemoveEnemy( pEnemy.GetPtr() );
	pState->RemovePossibleEnemy( pEnemy.GetPtr() );
}

// @0x3c2d0.
void CAIAllyNeedHelpEvent::Modify( SAIUnitState *pState )
{
	pState->AddAlly( pAlly.GetPtr() );
}

// @0x3c360.
void CAILostAllyEvent::Modify( SAIUnitState *pState )
{
	pState->RemoveAlly( pAlly.GetPtr() );
}

// @0x3c470: a NEW corpse goes on record; then retail reads the corpse server's pKiller (+0x1f8) and
// promotes THE KILLER to possibleEnemy. ⚠ pKiller is never written anywhere in retail (byte-scan
// proven: only ctor-null + the serializer), so the promotion arm is retail-inert -- a noticed corpse
// only lands in knownCorpses. The dev predecessor promoted the CORPSE itself (more aggressive than
// retail) -- removed.
void CAICorpseEvent::Modify( SAIUnitState *pState )
{
	IAIUnit *p = pCorpse.GetPtr();
	if ( pState->IsKnownCorpse( p ) )
		return;
	pState->AddKnownCorpse( p );
	NWorld::CUnitServer *pUS = p->GetUnitServer();
	IAIUnit *pAI = GetAIUnit( pUS != 0 ? pUS->pKiller.GetPtr() : 0 );
	if ( IsValid( pAI ) )
		pState->AddPossibleEnemy( pAI );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- Factories. A null/invalid payload yields a null event.
#define AIEVENT_FACTORY( Name, Member )                        \
	IAIEvent* Create##Name( IAIUnit *pUnit )                   \
	{                                                          \
		if ( !IsValid( pUnit ) )                               \
			return 0;                                          \
		C##Name *p = new C##Name();                            \
		p->Member = pUnit;                                     \
		return p;                                              \
	}
AIEVENT_FACTORY( AIEnemyEvent, pEnemy )
AIEVENT_FACTORY( AILostEnemyEvent, pEnemy )
AIEVENT_FACTORY( AIPossibleEnemyEvent, pEnemy )
AIEVENT_FACTORY( AILostPossibleEnemyEvent, pEnemy )
AIEVENT_FACTORY( AIEnemyDiedEvent, pEnemy )
AIEVENT_FACTORY( AIAllyNeedHelpEvent, pAlly )
AIEVENT_FACTORY( AILostAllyEvent, pAlly )
AIEVENT_FACTORY( AICorpseEvent, pCorpse )
#undef AIEVENT_FACTORY

IAIEvent* CreateAIBeginTurnEvent()  { return new CAIBeginTurnEvent(); }
IAIEvent* CreateAIHelpCalledEvent() { return new CAIHelpCalledEvent(); }
IAIEvent* CreateAIUpdateEvent()     { return new CAIUpdateEvent(); }

bool IsBeginTurnEvent( IAIEvent *pEvent )
{
	return dynamic_cast<CAIBeginTurnEvent*>( pEvent ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NAI
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x23062AC0, CAIEnemyEvent )
REGISTER_SAVELOAD_CLASS( 0x23062AC1, CAILostEnemyEvent )
REGISTER_SAVELOAD_CLASS( 0x23062AC2, CAIPossibleEnemyEvent )
REGISTER_SAVELOAD_CLASS( 0x23062AC3, CAIEnemyDiedEvent )
REGISTER_SAVELOAD_CLASS( 0x23062B00, CAIAllyNeedHelpEvent )
REGISTER_SAVELOAD_CLASS( 0x23062B01, CAILostAllyEvent )
REGISTER_SAVELOAD_CLASS( 0x23062B02, CAIBeginTurnEvent )
REGISTER_SAVELOAD_CLASS( 0x23062B40, CAIHelpCalledEvent )
REGISTER_SAVELOAD_CLASS( 0x23062B80, CAILostPossibleEnemyEvent )
REGISTER_SAVELOAD_CLASS( 0x23068C00, CAICorpseEvent )
REGISTER_SAVELOAD_CLASS( 0x23069B00, CAIUpdateEvent )
