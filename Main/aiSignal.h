#ifndef __AISIGNAL_H_
#define __AISIGNAL_H_

class CRay;

namespace NWorld
{
	class CWorld;
	class CUnitServer;
}

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class CAICommander;
class IAISignalManager;
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAISignal: public CObjectBase
{
public:
	virtual bool IsFinished() = 0;
	virtual bool IsActive() = 0;
	virtual bool CanDetect( IAIUnit *pAIUnit ) = 0;
	virtual IAIUnit *GetSetter() = 0;
	virtual void SetSetter( IAIUnit *_pSetter ) = 0;
	virtual CVec3 GetPosition() = 0;
	virtual void SetPosition( CVec3 _ptPos ) = 0;
	virtual void Process( IAIUnit *pAIUnit ) = 0;
	virtual void Segment() = 0;
	virtual int GetPriority() = 0;
	virtual IAISignalManager *GetAISignalManager() = 0;
	virtual void SetAISignalManager( IAISignalManager *_pAISignalManager ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAISignalManager: public CObjectBase
{
public:
	virtual void Add( IAISignal *pAISignal ) = 0;
	virtual IAISignal *Get( IAIUnit *pAIUnit ) = 0;
	virtual void Segment() = 0;
	virtual NWorld::CWorld *GetWorld() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignalManager *CreateAISignalManager( NWorld::CWorld *_pWorld );
IAISignal *CreateAIUnitSoundSignal( NWorld::CUnitServer *_pSource, 
	NWorld::CUnitServer *_pTarget, float _fRadius );
IAISignal *CreateAIGrenadeSoundSignal( CVec3 _ptPos );
IAISignal *CreateAIRevealEnemySignal( CVec3 _ptPos );
IAISignal *CreateAIHitSignal( NRPG::IUnitMissionInfo *pAttacker, NRPG::IUnitMissionInfo *pTarget );
IAISignal *CreateAISoundSignal( CVec3 ptPos, NWorld::CUnitServer *pReason, float fRadius );
IAISignal *CreateAICorpseSignal( NWorld::CUnitServer *_pUnitServer );
IAISignal *CreateAIShootSignal( NWorld::CUnitServer *pShooter, const CRay &ray );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif