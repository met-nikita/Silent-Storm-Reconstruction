#include "StdAfx.h"

#include "aiUnit.h"
#include "aiCommander.h"

#include "aiControl.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIControl
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIControl: public IAIControl
{
	ZDATA
	EAIControlType type;
	CPtr<IAIUnit> pAIUnit;
	CPtr<CAICommander> pAICommander;
	EAIManager manager;
protected:
	bool bActive;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&type); f.Add(3,&pAIUnit); f.Add(4,&pAICommander); f.Add(5,&manager); f.Add(6,&bActive); return 0; }
	//
	CAIControl() {}
	CAIControl( CAICommander *_pAICommander, IAIUnit *_pAIUnit, EAIControlType _type, EAIManager _manager ):
		pAICommander( _pAICommander ), pAIUnit( _pAIUnit ), type( _type ), manager( _manager ), bActive( false )
	{
		ASSERT( IsValid( pAICommander ) );
		ASSERT( IsValid( pAIUnit ) );
	}
	//
	virtual EAIManager GetManager() const { return manager; }
	virtual void Activate() = 0;
	virtual void DeActivate() = 0;
	virtual bool IsActive() const { return bActive; }
	virtual IAIUnit *GetAIUnit() { return pAIUnit; }
	virtual CAICommander *GetAICommander() { return pAICommander; }
	virtual EAIControlType GetType() { return type; }
	virtual void OnPerformerDied() {}
	virtual void DebugOutput() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITacticalControl REMOVED (AI-convergence Stage 2, with the tactical commander): it activated a unit
// onto the tactical commander (AddAllyUnit + Think). Combat entry now flows through the per-unit reaction
// installed at map-deploy (NAI::CreateUnitReaction) + the commander's per-segment reaction pump, so no
// IAIControl wrapper is needed. CAITaskControl was already removed in Stage 1. The IAIControl interface +
// the CAIControl base remain (the CAIUnit control stack is untouched); nothing constructs an IAIControl now.
////////////////////////////////////////////////////////////////////////////////////////////////////
}