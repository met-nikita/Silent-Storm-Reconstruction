#include "StdAfx.h"

#include "aiUnit.h"
#include "aiCommander.h"
#include "aiTacticalCommander.h"
#include "aiTaskCommander.h"

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
// CAITacticalControl
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAITacticalControl: public CAIControl
{
	OBJECT_BASIC_METHODS( CAITacticalControl );
	ZDATA
	ZPARENT( CAIControl );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIControl *)this); return 0; }
public:
	//
	CAITacticalControl() {}
	CAITacticalControl( CAICommander *_pAICommander, IAIUnit *_pAIUnit, EAIManager _manager ):
		CAIControl( _pAICommander, _pAIUnit, AI_CONTROL_UNINTERRUPTABLE, _manager )
	{
	}
	//
	virtual void Activate();
	virtual void DeActivate();
	virtual void DebugOutput() { OutputDebugString( "[AI TACTICAL CONTROL]\n" ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalControl::Activate()
{
	ASSERT( !bActive );
	if ( !bActive )
	{
		GetAICommander()->GetAITacticalCommander()->AddAllyUnit( GetAIUnit() );
		if ( GetAICommander()->IsAITurn() )
			GetAICommander()->GetAITacticalCommander()->Think();
		bActive = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalControl::DeActivate()
{
	ASSERT( bActive );
	if ( bActive )
	{
		GetAICommander()->GetAITacticalCommander()->DismissUnit( GetAIUnit() );
		bActive = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITaskControl
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAITaskControl: public CAIControl
{
	OBJECT_BASIC_METHODS( CAITaskControl );
	ZDATA
	ZPARENT( CAIControl );
	CPtr<CTask> pAITask;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIControl *)this); f.Add(3,&pAITask); return 0; }
public:
	//
	CAITaskControl() {}
	CAITaskControl( CAICommander *_pAICommander, CTask *_pAITask, EAIControlType ControlType, EAIManager _manager ):
		CAIControl( _pAICommander, _pAICommander->GetAIUnit( _pAITask->GetUnitServer() ), ControlType, _manager ), 
		pAITask( _pAITask )
	{
		ASSERT( IsValid( pAITask ) );
		//
		if ( !IsValid( pAITask ) )
			return;
		//
		pAITask->DeActivate();
		GetAICommander()->GetAITaskCommander()->AddTask( pAITask );
	}
	//
	virtual void Activate();
	virtual void DeActivate();
	virtual void OnPerformerDied() { if ( IsValid( pAITask ) ) pAITask->OnPerformerDied(); }
	virtual void DebugOutput() { OutputDebugString( "[AI TASK CONTROL]\n" ); }
	CTask* GetTask() const { return pAITask; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITaskControl::Activate()
{
	ASSERT( !bActive );
	if ( !bActive )
	{
		pAITask->Activate();
		bActive = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITaskControl::DeActivate()
{
	ASSERT( bActive );
	if ( bActive )
	{
		pAITask->DeActivate();
		bActive = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIControl* CreateAITacticalControl( CAICommander *pAICommander, IAIUnit *pAIUnit, EAIManager manager )
{
	return new CAITacticalControl( pAICommander, pAIUnit, manager );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIControl* CreateAITaskControl( CAICommander *pAICommander, 
	CTask *pAITask, EAIControlType ControlType, EAIManager manager )
{
	return new CAITaskControl( pAICommander, pAITask, ControlType, manager );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTask* GetTaskFromControl( IAIControl *pControl )
{
	CDynamicCast<CAITaskControl> pTaskControl(pControl);
	if (pTaskControl)
		return pTaskControl->GetTask();
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52662100, CAITacticalControl );
REGISTER_SAVELOAD_CLASS( 0x52662101, CAITaskControl );