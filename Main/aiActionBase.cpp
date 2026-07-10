#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"          // SAIState::GetCurrentAIEnemy
//
#include "aiActionBase.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - CAIAction base helpers. WIP - reconstructed; the release reads the
// enemy from GetAIUnitState()+0x88, reconciled here to the dev SAIState::GetCurrentAIEnemy().
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
IAIUnit* CAIAction::GetUnit() const { return pUnit; }
//
// @0x13ac0 - the acting unit's AI state, guarded on a live unit (pUnit != 0 && alive).
SAIState* CAIAction::GetAIState() const
{
	if ( IsValid( pUnit ) )
		return pUnit->GetAIState();
	return 0;
}
//
IAIUnit* CAIAction::GetEnemy() const
{
	if ( IsValid( pUnit ) && IsValid( pUnit->GetAIState() ) )
		return pUnit->GetAIState()->GetCurrentAIEnemy();
	return 0;
}
//
SPlaceWithAP CAIAction::GetCurrentPlace() const
{
	SPlaceWithAP p;
	if ( IsValid( pUnit ) )
	{
		p.place   = pUnit->GetUnitPosition();
		p.nUnitAP = pUnit->GetAP();
	}
	return p;
}
//
// @0x13a80 - gate on a LIVE unit. The release rejects a null/dead pUnit
// (pUnit != 0 && (alive-flag & 0x80) == 0, expressed in-tree as IsValid, the
// same guard GetEnemy/GetCurrentPlace use). It ALSO defers to a per-action
// "can-perform-while-busy" predicate (this vtbl+0x10) when the unit is busy
// (IAIUnit::IsUnitBusy, unit vtbl+0x20); but as compiled NO concrete action
// overrides that predicate and the base permits, so the busy branch never
// blocks and the function reduces to "the acting unit is alive". The previous
// `return true` skipped the unit-alive gate -> dead/invalid units were wrongly
// judged able to act.
bool CAIAction::CanPerform() { return IsValid( pUnit ); }   // slot4 base impl
////////////////////////////////////////////////////////////////////////////////////////////////////
}
