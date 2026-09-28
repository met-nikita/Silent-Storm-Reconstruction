#ifndef __RPGATTACKSESSION_H_
#define __RPGATTACKSESSION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// RPGAttackSession -- per-unit medal/statistics attack-session bookkeeping
// (release-new rpgAttackSession.obj compiland).
//
// NRPG::CUnitMissionForMedals records, for one combat "session", which attacks a
// unit took part in (attackedInSession), which connected (hitInSession), which
// were critical (criticalHits) and which struck a panzerklein (hitsPK), plus the
// bullets it is still waiting to resolve (waitForBullets). The mission lists are
// de-duplicated linear stores. Retail's end-of-session wait loop does NOT tally medals.
// NRPG::IUnitMissionForMedals is the release-new 4-byte vtable-only interface base
// (PDB size 4); CUnitMissionForMedals is PDB size 76 with the members below in
// exact offset order.
//
// Landed here: the three pure-bookkeeping methods (no cross-subsystem / vtable
// reach) -- AddAttackToAttackSession, AddPKHitToAttackSession, Segment.
//
// Audited 2026-09-28: the medal sink and side-medal DB column ARE implemented.
// However, v1.1 AddMedalPoints (0x68fbf0) has no absolute references or direct
// call/jump references in the retail image. Both v1.1 Segment (0x68ff40) and
// v1.2 Segment (0x68fce0) only drain expired weak projectile references and reset
// bFinished; neither dispatches the tally. This isolated tracker remains unwired:
// it is not an AI action-completion gate, and wiring awards here would invent
// behavior absent from the inspected retail paths. See the scratch backlog audit
// and audit_attack_session_reachability.py for reproducible binary checks.
//
// The object is transient (no operator& in the PDB -> not serialised, and it is not
// a CObjectBase), so it gets neither REGISTER_SAVELOAD_CLASS nor a class-registrar
// tag.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RPGUnitMission.h"   // NRPG::IUnitMission (+ CObjectBase / CPtr / vector / IsValid)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
class CUnit;         // held only via CPtr<CUnit> (never dereferenced in this compiland)
class CGlobalGame;   // held only via CPtr<CGlobalGame>
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-new 4-byte vtable-only interface base (PDB: NRPG::IUnitMissionForMedals,
// size 4). The concrete relation/panzerklein virtuals are added by an
// out-of-compiland subclass; only the vptr is needed here so CUnitMissionForMedals
// gets the correct layout (members start at +4).
class IUnitMissionForMedals
{
public:
	virtual ~IUnitMissionForMedals() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Per-unit medal attack-session tracker (PDB: NRPG::CUnitMissionForMedals, size 76,
// base IUnitMissionForMedals @0). Data members in exact PDB offset order.
class CUnitMissionForMedals: public IUnitMissionForMedals
{
public:
	vector<CPtr<IUnitMission> > attackedInSession; // +4  every attack this unit made
	vector<CPtr<IUnitMission> > hitInSession;       // +16 attacks that connected
	vector<CPtr<IUnitMission> > criticalHits;       // +28 critical hits (may stack)
	vector<CPtr<IUnitMission> > hitsPK;             // +40 hits on a panzerklein
	vector<CPtr<CObjectBase> >  waitForBullets;     // +52 bullets still in flight
	bool                        bFinished;          // +64 session ended, draining bullets
	CPtr<CUnit>                 pMedalsGainer;      // +68 unit that earns the medals
	CPtr<CGlobalGame>           pGame;             // +72 owning game

	// De-duplicated record of one attack. bHit selects hitInSession (else
	// attackedInSession); a repeat is not re-stored. When bHit && bCritical the same
	// mission is ALSO appended to criticalHits WITHOUT de-dup, so repeated critical
	// hits by the same mission STACK (release-faithful).
	void AddAttackToAttackSession( IUnitMission *pAttack, bool bHit, bool bCritical );

	// De-duplicated record of one panzerklein hit (hitsPK); a repeat is ignored.
	void AddPKHitToAttackSession( IUnitMission *pAttack );

	// Once bFinished, drop waitForBullets and clear bFinished only after every
	// awaited bullet has resolved (null or destroyed); otherwise stay pending.
	void Segment();

	// Unreferenced retail tally (see file banner); deliberately not dispatched by Segment.
	void AddMedalPoints();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RPGATTACKSESSION_H_
