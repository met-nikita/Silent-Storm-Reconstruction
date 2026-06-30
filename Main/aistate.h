#ifndef __AISTATE_H_
#define __AISTATE_H_

namespace NWorld
{
	class CWorld;
	class CPlayer;
	class CUnitServer;
}

namespace NAI
{
struct SPathPlace;
class IAIPlayer;
class IAICriterion;
class IAILogContainer;
class IAIMap;
class IAIUnit;
class CAITacticalCommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUnitGroup
{
	ZDATA
	CVec3 ptCenter;
	vector< CPtr<IAIUnit> > enemies;
	vector< CPtr<IAIUnit> > allies;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptCenter); f.Add(3,&enemies); f.Add(4,&allies); return 0; }
	SAIUnitGroup(): ptCenter( VNULL3 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIState
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIState: public CObjectBase
{
public:
	virtual IAIPlayer *GetAllyAIPlayer() = 0;
	virtual IAIPlayer *GetEnemyAIPlayer() = 0;
	virtual NWorld::CWorld *GetWorld() = 0;
	virtual IAIUnit *GetCurrentAIUnit() = 0;
	virtual void SetCurrentAIUnit( IAIUnit *pAIUnit ) = 0;
	virtual IAIUnit *GetCurrentAIEnemy() = 0;
	virtual void SetCurrentAIEnemy( IAIUnit *pAIUnit ) = 0;
	virtual bool IsPerformingAction() = 0;
	virtual bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit ) = 0;
	virtual bool IsSomebodyKilled() = 0;
	virtual bool IsContain( IAIUnit *_pAIUnit ) = 0;
	virtual void RemoveUnit( IAIUnit *_pAIUnit ) = 0;
	virtual int GetEnemyHP() = 0;
	virtual int GetAllyHP() = 0;
	virtual int GetTurnStartEnemyHP() = 0;
	virtual int GetTurnStartAllyHP() = 0;
	virtual int GetCurrentAction() = 0;
	virtual void SetCurrentAction( int nAction ) = 0;
	virtual CAITacticalCommander *GetAITacticalCommander() = 0;
	virtual void OnTurnStarted() = 0;
	virtual bool IsAITurn() = 0;
	virtual const vector<SAIUnitGroup>& GetEnemyGroups() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIState *CreateAIState( NWorld::CWorld *pWorld, CAITacticalCommander *pAITacticalCommander );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif