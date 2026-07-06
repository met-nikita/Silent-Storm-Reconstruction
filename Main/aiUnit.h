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
}

namespace NWorld
{
	class CUnitServer;
	class CCannon;
}

namespace NAI
{
class IAIState;
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

	// --- release CAICombatLogic substrate additions (phase-6 port) ---
	// GetAIState (release IAIUnit vtbl 0x78) returns the unit's tactical AI state; SetAIState threads it
	// in when the unit joins the state (tactical commander). Non-pure defaults so any other IAIUnit impl
	// still builds; CAIUnit overrides both with a real weak back-pointer.
	virtual IAIState* GetAIState() { return 0; }
	virtual void SetAIState( IAIState * ) {}
	// the unit's tactical threat state (release IAIUnit vtbl 0x74). CAIUnit stores it by value.
	virtual SAIUnitState* GetAIUnitState() { return 0; }
	// the unit's reaction (release IAIUnit vtbl 0x60/0x64). The tactical commander installs a
	// CAINormalReaction and Update()s it each think; the reaction picks the unit's logic. CAIUnit owns it.
	virtual CAIReaction* GetReaction() const { return 0; }
	virtual void SetReaction( CAIReaction * ) {}
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
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *CreateAIUnit( NWorld::CUnitServer *pUnitServer, bool bUnderAIControl );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif
