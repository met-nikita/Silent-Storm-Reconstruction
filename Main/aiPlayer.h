#ifndef __AIPLAYER_H_
#define __AIPLAYER_H_

namespace NWorld
{
	class CUnitServer;
}

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class IAIState;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIPlayer: public CObjectBase
{
public:
	virtual vector< CPtr<IAIUnit> > *GetUnits() = 0;
	virtual void AddUnit( IAIUnit *_pAIUnit ) = 0;
	virtual IAIUnit *GetNearestUnit( IAIUnit *pUnit, float *fDistance ) = 0; 
	virtual IAIState *GetAIState() = 0;
	virtual void Synchronize() = 0;
	virtual bool IsContain( IAIUnit *pAIUnit ) = 0;
	virtual bool IsContain( NWorld::CUnitServer *pUnit ) = 0;
	// pAIUnit - для кого проверяется доступность положения
	virtual bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit ) = 0;
	virtual bool IsPerformingAction() = 0;
	virtual bool IsSomebodyKilled() = 0;
	virtual void RemoveUnit( IAIUnit *_pAIUnit ) = 0;
	virtual void OnTurnStarted() = 0;
	virtual void DebugOutput() = 0;
	virtual void CancelActions() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIPlayer *CreateAIPlayer( IAIState *pAIState );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif