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
// In the release this bridge is a CAIUnit base subobject and is fed by the global world event bus.
// Here it is an owned CObj because the dev CObjectBase hierarchy cannot accept the second base safely.
// Fresh units construct it immediately (so StartGame is observed), loaded units re-arm it on their
// first OnAISegment, and SAIUnitState::PrepareEnemies performs the retail begin-turn reconciliation.
// The world producers and corpse range/radius seams are now wired. Oracle:
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
	// Retail's interface is a CAIUnit base subobject: serialize the owner's identity,
	// not the free-standing adapter used by this source tree.
	CPtr<IAIUnit>            pInterface;
	int operator&( CStructureSaver &f );

	CAIEventTrackerImpl();
	CAIEventTrackerImpl( IAIUnit *_pInterface, NWorld::CUnitServer *_pUnit );

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

	void ThrowAIEvent( IAIEvent *pEvent );   // gate on a live, fight-capable, AI-driven owner
	void CheckForVisibleCorpses();
	void ProcessAISegment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIEventTracker (PDB 28): owns the impl via CObj. Retail's standalone Notify is
// empty; its CAIUnit-derived instance handles events. This adapter keeps the owner
// explicitly in the impl. ProcessAISegment forwards to the impl.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIEventTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAIEventTracker)
public:
	CObj<CAIEventTrackerImpl> pImpl;
	int operator&( CStructureSaver &f ) { f.Add( 2, &pImpl ); return 0; }   // retail @0xac110

	CAIEventTracker() {}
	CAIEventTracker( IAIUnit *pOwner, NWorld::CUnitServer *pUS );
	CAIEventTracker( const CAIEventTracker &src );

	virtual void Notify( IAIEvent *pEvent );   // retail standalone base: no-op
	void ProcessAISegment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AITHREATTRACKER_H_
