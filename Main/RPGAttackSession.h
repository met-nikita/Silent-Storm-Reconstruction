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
// de-duplicated linear stores; medals are tallied from them at end of session.
// NRPG::IUnitMissionForMedals is the release-new 4-byte vtable-only interface base
// (PDB size 4); CUnitMissionForMedals is PDB size 76 with the members below in
// exact offset order.
//
// Landed here: the three pure-bookkeeping methods (no cross-subsystem / vtable
// reach) -- AddAttackToAttackSession, AddPKHitToAttackSession, Segment.
//
// DEFERRED: AddMedalPoints (release VA 0x68fbf0). Its end-of-session tally needs
// the medal SINK NRPG::CMedalsGainer::AddMedalPoints(CGlobalGame*,EMedalPointCases,
// float) -- still ABSENT from RPGUnit.h pending the NDb::CSide::medals DB column --
// plus two virtuals on this interface (the enemy/self/ally relation classifier and
// the gainer's own panzerklein) whose concrete impls live in an out-of-compiland
// subclass. (EMedalPointCases itself is now present in RPGMedals.h.) The method is
// declared below for class-surface parity but intentionally left undefined until
// that subsystem lands; nothing calls it, so the build stays green.
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

	// DEFERRED (see file banner): end-of-session medal tally. Needs the absent
	// CMedalsGainer::AddMedalPoints sink + out-of-compiland relation/PK virtuals.
	// Declared for class-surface parity; intentionally left undefined (nothing
	// calls it, so this is link-safe).
	void AddMedalPoints();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RPGATTACKSESSION_H_
