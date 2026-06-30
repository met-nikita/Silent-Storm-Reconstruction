#ifndef __AITACTICALCOMMANDER_H_
#define __AITACTICALCOMMANDER_H_
//
#include "aiJob.h"
//
namespace NWorld
{
	class CCmd;
	class CWorld;
	class CCommand;
	class CUnitServer;
	class CPlayer;
}
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class IAIState;
class IAIControl;
class IAIIterator;
class IAILogContainer;
class CAICommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITacticalCommander
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAITacticalCommander: public CAIJob
{
	typedef list< CPtr<NWorld::CCommand> > CommandSet;
	//
	OBJECT_BASIC_METHODS(CAITacticalCommander);
	ZDATA
	ZPARENT(CAIJob);
	bool bEndOfTurn;
	CommandSet commands;
	CPtr<NWorld::CUnitServer> pLastCommandedUnit;
	CObj<IAIState> pAIState;
	CObj<IAILogContainer> pAILog; 
	CPtr<CAICommander> pAICommander;
	CObj<IAIIterator> pAIUnitIterator;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIJob*)this); f.Add(3,&bEndOfTurn); f.Add(4,&commands); f.Add(5,&pLastCommandedUnit); f.Add(6,&pAIState); f.Add(7,&pAILog); f.Add(8,&pAICommander); f.Add(9,&pAIUnitIterator); return 0; }
	//
	void ThinkForNextGoodUnit();
	void CheckForReleasableUnits();
	void ChooseLogic( IAIUnit *pUnit );
	//
public:
	CAITacticalCommander() {}
	CAITacticalCommander( CAICommander *_pAICommander );
	//
	NWorld::CCommand* GetCommand();
	void Think();
	bool IsEndOfTurn() const;
	bool IsPerformingAction() const;
	bool IsUnderControl( IAIUnit *_pAIUnit );
	void OnPassControl( NWorld::CPlayer *pPlayer );
	void OnUnitWasKilled( NWorld::CUnitServer *pUnit );
	void RemoveUnit( NWorld::CUnitServer *pUS );
	void OnTurnStarted();
	void OnTurnFinished();
	void OnStartRealTime();
	void OnCancelAction();
	void Segment();
	void AddAllyUnit( IAIUnit *_pAIUnit );
	void AddEnemyUnit( IAIUnit *_pAIUnit );
	void OnSeeUnit( NWorld::CUnitServer *pWatcher, NWorld::CUnitServer *pTarget );
	// AIJob
	virtual void DoJob();
	virtual bool IsIdleJob();
	virtual bool IsJobFinished();
	virtual void OnJobFinished();
	//
	NWorld::CWorld *GetWorld() const;
	IAIUnit* GetDangerousAttackableEnemy( IAIUnit *pAIUnit ) const;
	void DismissUnit( IAIUnit *_pAIUnit );
	void Synchronize();
	bool IsAITurn() const;
	void DismissAllUnits();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAITacticalCommander* CreateAITacticalCommander( CAICommander *pAICommander );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif