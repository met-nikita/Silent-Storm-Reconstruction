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
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIPlayer: public CObjectBase
{
public:
	virtual vector< CPtr<IAIUnit> > *GetUnits() = 0;
	virtual void AddUnit( IAIUnit *_pAIUnit ) = 0;
	virtual IAIUnit *GetNearestUnit( IAIUnit *pUnit, float *fDistance ) = 0;
	// (the dev IAIPlayer::GetAIState back-ref was dead -- zero callers -- and was removed with the
	//  IAIState->value-struct SAIState collapse; the state now queries units directly, as in retail.)
	virtual void Synchronize() = 0;
	virtual bool IsContain( IAIUnit *pAIUnit ) = 0;
	virtual bool IsContain( NWorld::CUnitServer *pUnit ) = 0;
	// pAIUnit - ��� ���� ����������� ����������� ���������
	virtual bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit ) = 0;
	virtual bool IsPerformingAction() = 0;
	virtual bool IsSomebodyKilled() = 0;
	virtual void RemoveUnit( IAIUnit *_pAIUnit ) = 0;
	virtual void OnTurnStarted() = 0;
	virtual void DebugOutput() = 0;
	virtual void CancelActions() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIPlayer *CreateAIPlayer();
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif