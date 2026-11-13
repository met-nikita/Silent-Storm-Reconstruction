#include "StdAfx.h"
//
#include "aiUnit.h"
#include "wUnitServer.h"
#include "aiState.h"
#include "aiUnitState.h"      // SAIUnitState::pEnemy
//
#include "aiActionBase.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - CAIAction base helpers.
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
	// Retail v1.2 0x414260: use the acting unit's own selected contact.
	// The shared commander context can change between place-chooser job ticks.
	if ( IsValid( pUnit ) )
		return pUnit->GetAIUnitState()->pEnemy;
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
// Retail 0x414180: PK pilots may only consider actions supported by the suit.
bool CAIAction::CanPerform()
{
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return false;
	return !pUnit->GetUnitServer()->IsWearingPK() || CanPerformInPK();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
}
