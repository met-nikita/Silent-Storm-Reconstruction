#ifndef __AICONTROL_H_
#define __AICONTROL_H_

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class CAICommander;
class CTask;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAIManager
{
	AIM_AI = 0,
	AIM_SCRIPT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAIControlType
{
	AI_CONTROL_UNINTERRUPTABLE = 0, // непрерываемый другими заданиями
	AI_CONTROL_INTERRUPTABLE, // прерываемый другими заданиями
	AI_CONTROL_ERASABLE, // удаляется при поступлении нового задания
	N_AI_CONTROL_TYPE_COUNT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIControl
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIControl: public CObjectBase
{
public:
	virtual void Activate() = 0;
	virtual void DeActivate() = 0;
	virtual bool IsActive() const = 0;
	virtual IAIUnit *GetAIUnit() = 0;
	virtual CAICommander *GetAICommander() = 0;
	virtual EAIControlType GetType() = 0;
	virtual void OnPerformerDied() = 0;
	virtual void DebugOutput() = 0;
	virtual EAIManager GetManager() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIControl* CreateAITacticalControl( CAICommander *pAICommander, IAIUnit *pAIUnit, EAIManager manager );
IAIControl* CreateAITaskControl( CAICommander *pAICommander, CTask *pAITask, EAIControlType ControlType, EAIManager manager );
CTask* GetTaskFromControl( IAIControl *pControl );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif