#ifndef __AIUnit_H_
#define __AIUnit_H_

#include "aiPosition.h"

namespace NRPG
{
	class CWeaponItem;
	class CUnit;
	class IUnitMission;
}

namespace NDb
{
	class CRPGWeapon;
	class IUnitMission;
	class CUnit;
	enum EShootMode;   // MS-extension fwd decl (as NAI::EHitLocation below) -- DataRPG.h defines it
}

namespace NWorld
{
	class CUnitServer;
	class CCannon;
}

namespace NAI
{
struct SAIState;
class CAIInventory;
class IAILogRecord;
class	IAICriterionData;
class IAIControl;
class CTask;
class IAILogic;   // phase-7 supersede: the unit holds the command-driven IAILogic (was the dev CAILogic:CAIJob)
class CAIReaction; // the unit's reflex layer (release): chooses the logic each think (release vtbl 0x64 SetReaction)
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EHitLocation;
struct SPathPlace;
struct SPosition;
struct SUnitPosition;
struct SAIUnitState;   // per-unit threat state (release IAIUnit vtbl 0x74 GetAIUnitState)
enum EAIManager;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit: public CObjectBase
{
public:	
	virtual NWorld::CUnitServer* GetUnitServer() const = 0;
	virtual NRPG::IUnitMission* GetUnitMission() const = 0;
	virtual NRPG::CUnit* GetRPGUnit() const = 0;
	virtual void Synchronize() = 0;
	virtual SPosition GetPosition() = 0;
	virtual SUnitPosition GetUnitPosition() = 0;
	virtual void SetPosition( SPosition _pPosition ) = 0;
	virtual void SetPosition( SPathPlace _pPosition ) = 0;
	virtual SPosition GetPrevPosition() = 0;
	virtual void SavePrevPosition() = 0;
	virtual int GetTurnStartHP() = 0;
	virtual void GetHP( int *_nHP, int *_nMaxHP ) = 0;
	virtual int GetHP() = 0;
	virtual void SetHP( int _nHP, int _nMaxHP ) = 0;
	virtual void GetAP( int *_nAP, int *_nMaxAP ) = 0;
	virtual int GetAP() = 0;
	virtual void SetAP( int _nAP, int _nMaxAP ) = 0;
	virtual int GetMaxAP() = 0;
	virtual int GetMaxHP() = 0;
	virtual void SpendHP( int _nHP ) = 0;
	virtual void SpendAP( int _nAP ) = 0;
	virtual void SetPose( int pose ) = 0;
	virtual bool IsUsingCannon() = 0;
	virtual NWorld::CCannon *GetCannon() = 0;
	virtual void SetCannon( NWorld::CCannon *_pCannon ) = 0;
	virtual bool IsMovedThisTurn() = 0;
	virtual bool IsDead() = 0;
	virtual int GetRemainAP() = 0;
	virtual int GetToHit( IAIUnit *pTarget, const NAI::SUnitPosition &pos, NAI::EHitLocation hl = NAI::HL_ANY ) = 0;
	virtual bool IsPerformingAction() = 0;
	virtual void OnTurnStarted() = 0;
	virtual void GetLastSeenEnemy( SPosition *Position, IAIUnit **ppAIUnit ) = 0;
	virtual void SetLastSeenEnemy( IAIUnit *pAIUnit ) = 0;
	virtual CAIInventory* GetAIInventory() = 0;
	virtual int GetHurtHP() = 0;
	virtual void SetHurtHP( int _nHurtHP ) = 0;
	virtual int GetCoverForFixedUnit( const NAI::SUnitPosition &pos,
		NWorld::CUnitServer *pTarget, NRPG::CWeaponItem *pWeaponItem, NAI::EHitLocation HitLocation ) = 0;
	virtual bool HasInactivePose() = 0;
	virtual void AssignControl( IAIControl *pAIControl ) = 0;
	virtual CTask* GetRoute() const = 0;
	virtual void ActivateCurrentControl() = 0;
	virtual void DeactivateCurrentControl() = 0;
	virtual void OnControlFinished() = 0;
	virtual bool IsUnderAIControl() = 0;
	virtual int GetAdditionalExpediency() = 0;
	virtual void SetAdditionalExpediency( int nExpediency ) = 0;
	virtual void SetMaxToHit( int _nMaxToHit ) = 0;
	virtual int GetMaxToHit() = 0;
	virtual bool HasVisibleEnemies() = 0;
	virtual void DebugOutput() = 0;
	virtual void OnDied() = 0;

	virtual IAILogic* GetLogic() const = 0;             // release vtbl 0x50
	virtual void SetLogic( IAILogic *_pLogic ) = 0;     // release vtbl 0x58 (was CAILogic* in the dev predecessor)
	// --- the retail ROUTE slots (release GetRoute vtbl 0x54 @0xad3a0 / SetRoute vtbl 0x5c @0xadb90) ---
	// Retail CAIUnit keeps THREE behaviour slots: pCurrentLogic (reaction-installed combat logic) +
	// routes[0] (the NORMAL route: deploy/script patrols) + routes[1] (the SEQUENCE route: cutscene
	// actions; selected while the world runs a sequence). GetLogic @0xad340 resolves the ACTIVE one
	// (sequence -> routes[1]; else pCurrentLogic if set, else routes[0]); SetLogic @0xaddc0 PAUSES the
	// route under a new combat logic and NO-OPS during a sequence; SetLogic(0)/CancelCurrentLogic
	// @0xadcd0 RESUMES the paused route. Named *RouteLogic here because the legacy dev GetRoute() (the
	// CTask stub above) still occupies the GetRoute name for the lua UnitGetRoute binding. Non-pure
	// defaults (vtable-order append) so any other IAIUnit impl builds.
	virtual IAILogic* GetRouteLogic() const { return 0; }
	virtual void SetRouteLogic( IAILogic * ) {}

	// --- release CAICombatLogic substrate additions (phase-6 port) ---
	// GetAIState (release IAIUnit vtbl 0x78) returns the unit's tactical AI state; SetAIState threads it
	// in when the unit joins the state (tactical commander). Non-pure defaults so any other IAIUnit impl
	// still builds; CAIUnit overrides both with a real weak back-pointer.
	virtual SAIState* GetAIState() { return 0; }
	virtual void SetAIState( SAIState * ) {}
	// the unit's tactical threat state (release IAIUnit vtbl 0x74). CAIUnit stores it by value.
	virtual SAIUnitState* GetAIUnitState() { return 0; }
	// the unit's reaction (release IAIUnit vtbl 0x60/0x64). The tactical commander installs a
	// CAINormalReaction and Update()s it each think; the reaction picks the unit's logic. CAIUnit owns it.
	virtual CAIReaction* GetReaction() const { return 0; }
	virtual void SetReaction( CAIReaction * ) {}
	// AI-convergence Stage 2 reaction pump (release CAIUnit vtbl surface). GetReactionForUpdate @0xad260:
	// return the unit's reaction ONLY when its threat state changed (state IsModified), CLEARING the flag --
	// this is what lets the commander's updateTracker drain (else it never empties and the round-robin never
	// runs). OnLogicFinished @0xad1f0 (vtbl 0x60): drop a finished logic + mark the state modified so the
	// reaction re-picks. CancelCommand @0xad820 (vtbl 0x28): cancel the unit's in-flight world command when
	// its reaction is about to re-decide. All non-pure (vtable-order append) so other IAIUnit impls build.
	virtual CAIReaction* GetReactionForUpdate() { return 0; }
	virtual void OnLogicFinished( IAILogic * ) {}
	virtual void CancelCommand() {}
	// the AI hide-roll chance (release IAIUnit vtbl +0x88/+0x8c == CAIUnit::Get/SetHideProbability, member
	// +0x100). Seeded from the current difficulty's HideProbability on construction; read by the route-AI
	// CreateAICheckPositionLogic / CreateAICheckForEnemyLogic hide roll. Appended at the END of the vtable and
	// non-pure (like GetAIState/GetReaction above) so the dev<->release vtable order is irrelevant and any
	// other IAIUnit implementor still builds.
	virtual int GetHideProbability() { return 0; }
	virtual void SetHideProbability( int ) {}
	// release CAIUnit::OnAISegment @0xad2c0: per-segment tick that builds + ticks the unit's CAIEventTracker. Non-pure
	// (default no-op) so non-CAIUnit IAIUnit impls are unaffected and the vtable-order append is harmless.
	virtual void OnAISegment() {}
	// release CAIUnit::OnSequenceStarted @0xadec0 / OnSequenceFinished @0xad3f0 -- the per-unit
	// BeginSequence/EndSequence notify (retail luac_BeginSequence @0x2f1890 / EndSequence @0x2f1a60 run
	// them via GetAIUnits + CallAIFunc). Started: SetLogic(0), suspend the NORMAL route, drop any stale
	// sequence route. Finished: SetLogic(0), END the sequence route, RESUME the normal route. Non-pure
	// defaults (vtable-order append, same pattern as OnAISegment).
	virtual void OnSequenceStarted() {}
	virtual void OnSequenceFinished() {}
	// release CAIUnit::ContinueRoute @0xadf20 (v1.2 @0x4ae1b0): move the saved route into
	// the slot selected by the CURRENT sequence mode, resume it, clear the other slot, and
	// drop any combat logic. UnitKeepMoving is the script-facing caller.
	virtual void ContinueRoute() {}
	// release CAIUnit per-unit runaway-AI guard (IAIUnit vtbl 0x90/0x94): GetNonFreezeCounter @0xaef20 /
	// ClearNonFreezeCounter @0xaef30. The commander's IsPossibleFreeze @0x33ad0 weights each unit's counter
	// (x200) into its 5000-cap freeze test; ClearNonFreezeCounters @0x33a90 zeroes them each real-time
	// segment. The counter bumps on every per-unit logic/reaction churn (SetReaction / SetLogic). Non-pure
	// defaults (vtable-order append) so other IAIUnit impls are unaffected; CAIUnit overrides both.
	virtual int GetNonFreezeCounter() { return 0; }
	virtual void ClearNonFreezeCounter() {}
	// release CAIUnit shoot-mode disable machinery (IAIUnit vtbl 0x7c/0x80/0x84 == DisableShootMode
	// @0xad740 / EnableShootMode @0xad770 / IsShootModeDisabled @0xad590; hash_map<int,int> @CAIUnit+0xec,
	// key = bare NDb::EShootMode, value 1). Written ONLY by the Defence-dance brackets
	// (CAIDefenceReaction::Update @0x3b160: Disable(SM_Careful)+Disable(SM_LongBurst) while active,
	// Enable both on revert-to-prev); read ONLY by the CAIInventory::GetBestFireArms @0x560e0 per-mode
	// gate. Non-pure defaults (vtable-order append) so other IAIUnit impls build.
	virtual bool IsShootModeDisabled( NDb::EShootMode ) const { return false; }
	virtual void DisableShootMode( NDb::EShootMode ) {}
	virtual void EnableShootMode( NDb::EShootMode ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *CreateAIUnit( NWorld::CUnitServer *pUnitServer, bool bUnderAIControl );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif
