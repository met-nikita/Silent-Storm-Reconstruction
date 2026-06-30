#ifndef __AITHREATTRACKER_H_
#define __AITHREATTRACKER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiThreatTracker (retail-new aiThreatTracker.obj) -- CAIEventTracker / CAIEventTrackerImpl: the
// per-unit bridge that turns NWorld world events (a bullet flies past, an enemy is seen/heard/lost, a
// grenade lands, a corpse appears, our turn starts, ...) into the NAI AI events of aiEvent.h and hands
// them to the unit's threat state (SAIUnitState).
//
// ADDITIVE PARITY SHELL (behaviour-neutral). In the release this bridge is owned by the per-unit
// CAIUnit (which derives from CAIEventTracker) and is fed by the global world event bus. The dev tree
// deliberately omits that install path -- SAIUnitState is poll-based (Populate() each think), so the
// nine NWorld trigger payloads below are release-new world-subsystem events that NOTHING in the dev
// tree throws. This module reconstructs the classes + handlers faithfully so the compiland SOURCE
// converges. UPDATE (PART G): the layer is now ACTIVE -- CAIUnit owns a CObj<CAIEventTracker> built + ticked in
// CAIUnit::OnAISegment (commit 3ea95eb), and the CEventOnBullet producer (wBullet.cpp) is wired, so a unit shot from
// concealment now reacts (the suspect survives the no-clear SAIUnitState::Populate). Two release world seams have no dev twin (the
// CWorld range query at vtbl+0x10c and the RPG corpse-scan radius) -- they are documented absent and
// stubbed empty in the .cpp, so the (deferred) corpse scan finds no candidates. CAIEventTracker is a
// free-standing CObjectBase class here (NOT grafted onto the dev CAIUnit). Oracle:
// decomp/src/s2_threattracker.h (every handler disasm-verified there).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\Geom.h"          // CRay / CVec3 (by-value event-payload members)
#include "..\Misc\EventsBase.h"    // NGlobal::CEventRegister (the 11 auto-subscribing handler slots)
#include "eventUnit.h"             // NWorld::CEventOnUnitUnhide (reused; NOT redefined)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
class CUnitServer;
class CDumbUnitServer;
class CPlayer;
////////////////////////////////////////////////////////////////////////////////////////////////////
// The world AI-trigger event payloads (CEventOnSeeNewEnemy / OnLostEnemyFromSight / OnHearEnemy / OnHearAlly /
// OnGrenadeExplosion / OnBullet / OnUnitDiedOrLoseConsciousness / OnPassControl / OnStartGame / OnAttackAtUnit)
// were HOISTED to eventUnit.h (included above) so the world producers can throw them without dragging this NAI
// tracker header into world TUs. The handlers below still see them via that include.
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IAIUnit;
class IAIEvent;
class CAIEventTracker;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIEventTrackerImpl (PDB 152): CObjectBase + 11 auto-subscribing CEventRegister handler slots (the
// ctor wires each to its OnXxx handler and the global event bus) + the unit / interface back-pointers.
// Faithful handler bodies live in the .cpp; they are unreachable in the dev tree (no producer throws
// the trigger events), so subscribing is behaviour-neutral.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIEventTrackerImpl: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAIEventTrackerImpl)
public:
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnSeeNewEnemy >                 regOnSeeEnemy;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnLostEnemyFromSight >          regOnLostEnemy;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnHearEnemy >                   regOnHearEnemy;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnHearAlly >                    regOnHearAlly;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnGrenadeExplosion >            regOnGrenade;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnBullet >                      regOnBullet;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnUnitDiedOrLoseConsciousness > regOnDie;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnPassControl >                 regOnNewTurn;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnStartGame >                   regOnStartGame;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnAttackAtUnit >                regOnAttack;
	NGlobal::CEventRegister< CAIEventTrackerImpl, NWorld::CEventOnUnitUnhide >                  regOnUnhide;
	CPtr<NWorld::CUnitServer> pUnit;
	CPtr<CAIEventTracker>     pInterface;

	CAIEventTrackerImpl();
	CAIEventTrackerImpl( CAIEventTracker *_pInterface, NWorld::CUnitServer *_pUnit );

	NWorld::CUnitServer* GetUnit() const { return pUnit; }

	// world-event handlers (vtbl-less; reached only through the CEventRegister thunks)
	void OnSeeEnemy ( const NWorld::CEventOnSeeNewEnemy &e );
	void OnLostEnemy( const NWorld::CEventOnLostEnemyFromSight &e );
	void OnHearEnemy( const NWorld::CEventOnHearEnemy &e );
	void OnHearAlly ( const NWorld::CEventOnHearAlly &e );
	void OnGrenade  ( const NWorld::CEventOnGrenadeExplosion &e );
	void OnBullet   ( const NWorld::CEventOnBullet &e );
	void OnDie      ( const NWorld::CEventOnUnitDiedOrLoseConsciousness &e );
	void OnNewTurn  ( const NWorld::CEventOnPassControl &e );
	void OnStartGame( const NWorld::CEventOnStartGame &e );
	void OnAttack   ( const NWorld::CEventOnAttackAtUnit &e );
	void OnUnhide   ( const NWorld::CEventOnUnitUnhide &e );

	void ThrowAIEvent( IAIEvent *pEvent );   // gate on a live, fight-capable, AI-driven unit -> Notify
	void CheckForVisibleCorpses();
	void ProcessAISegment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIEventTracker (PDB 28): owns the impl via CObj. Notify (release vtbl slot 0) routes the AI event to
// the unit's threat state; ProcessAISegment forwards to the impl. Free-standing CObjectBase here.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIEventTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAIEventTracker)
public:
	CObj<CAIEventTrackerImpl> pImpl;

	CAIEventTracker() {}
	CAIEventTracker( NWorld::CUnitServer *pUS );
	CAIEventTracker( const CAIEventTracker &src );

	virtual void Notify( IAIEvent *pEvent );   // release vtbl slot 0: hand the event to the subscribers
	void ProcessAISegment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AITHREATTRACKER_H_
