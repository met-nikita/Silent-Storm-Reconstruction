#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"          // IAIState::GetCurrentAIEnemy
//
#include "aiActionBase.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - CAIAction base helpers. WIP - reconstructed; the release reads the
// enemy from GetAIUnitState()+0x88, reconciled here to the dev IAIState::GetCurrentAIEnemy().
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
IAIUnit* CAIAction::GetUnit() const { return pUnit; }
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
bool CAIAction::CanPerform() { return true; }   // slot4 base impl
////////////////////////////////////////////////////////////////////////////////////////////////////
}
