#ifndef _AICOMMANDER_H_
#define _AICOMMANDER_H_

#include "wInterface.h"

namespace NWorld
{
	enum ETBSEvent;
	class CWorld;
	class CPlayer;
	class CObjectServerBase;
}

namespace NAI
{
class IAIUnit;
class IAISignal;
class CAITaskCommander;
class CAITacticalCommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAICommander: public NWorld::CCommander
{
	OBJECT_BASIC_METHODS(CAICommander);
	ZDATA
	ZPARENT( NWorld::CCommander );
	CPtr<NWorld::CPlayer> pPlayer;
	CObj<CAITaskCommander> pTaskCommander;
	CObj<CAITacticalCommander> pTacticalCommander;
	list< CPtr<NWorld::CObjectServerBase> > LockedObjects;
	list< CObj<IAIUnit> > units;
	CPtr<NWorld::CWorld> pWorld;
	bool bAITurn;
	bool bWantTurnBased;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(NWorld::CCommander*)this); f.Add(3,&pPlayer); f.Add(4,&pTaskCommander); f.Add(5,&pTacticalCommander); f.Add(6,&LockedObjects); f.Add(7,&units); f.Add(8,&pWorld); f.Add(9,&bAITurn); f.Add(10,&bWantTurnBased); return 0; }
	//
private:
	// TBSEvents
	void OnTurnStarted();
	void OnTurnFinished();
	void OnCancelAction();
	void OnStartRealTime();
	//
public:
	CAICommander() {}
	CAICommander( NWorld::CWorld *_pWorld, NWorld::CPlayer *_pPlayer ); 
	void GenerateCommand();
	virtual void OnPassControl( NWorld::CPlayer *_pPlayer );
	virtual void OnUnitDied( NWorld::CUnitServer *pUnit );
	void RemoveUnit( NWorld::CUnitServer *pUS );
	virtual void OnSeeUnit( NWorld::CUnitServer *pWatcher, NWorld::CUnitServer *pTarget );
	virtual bool IsEndOfTurn();
	virtual bool IsRequestInterrupt() const { return false; }
	virtual void Segment(); 
	virtual CAITaskCommander *GetAITaskCommander() { return pTaskCommander; }
	virtual CAITacticalCommander *GetAITacticalCommander() { return pTacticalCommander; }
	virtual void OnUnitAdded( NWorld::CUnitServer *pUnit );
	virtual void OnTBSEvent( NWorld::ETBSEvent event );
	// Object lock
	bool IsObjectLocked( NWorld::CObjectServerBase *pObject );
	void LockObject( NWorld::CObjectServerBase *pObject );
	void UnLockObject( NWorld::CObjectServerBase *pObject );
	void UnLockAllObjects();
	//
	virtual void ProcessAISignals();
	NWorld::CWorld *GetWorld() { return pWorld; }
	IAIUnit *GetAIUnit( NWorld::CUnitServer *pUnit );
	bool IsAITurn() { return bAITurn; }
	bool HasVisibleEnemies();
	void Synchronize();
	void WantTurnBased();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander @0x35b60 -- the human/scripted player's flavour of CAICommander (release-NEW; oracle
// decomp/src/s2_aicommander.h, registered 0x51823190 alongside CAICommander 0x02731170). It carries
// NO new data members (release PDB: identical 160B layout); it differs from CAICommander only by its own
// vtable + a GenerateCommand override that, in the release, auto-drives the commander ONLY while the world
// runs a cinematic sequence.
//
// Role in this tree: it is the dyncast TARGET that NAI::IsAIPlayer (aiMisc) uses to tell an AI side from a
// scripted/human one. Purely additive -- nothing in this (predecessor) snapshot instantiates it yet; the
// ctor + override are reconstructed for parity, not wired into any existing call path.
//
// DIVERGENCE (documented): the release override gates on CWorld::IsSequence, which has no equivalent in
// this dev snapshot (wTurnBased.h:427 notes there is no world-level sequence predicate; the TBS layer uses
// IsForcedRealTime, a CTBSWorld member not reachable from CAICommander::GetWorld()). The override is
// therefore reconstructed as a conservative no-op (a human commander never auto-drives) -- the
// release-direction-preserving choice -- rather than inventing a predicate. The distinct vtable slot is
// preserved (faithful); only the body is dev-limited.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSequenceCommander: public CAICommander
{
	OBJECT_BASIC_METHODS(CSequenceCommander);
public:
	CSequenceCommander() {}
	CSequenceCommander( NWorld::CWorld *_pWorld );
	void GenerateCommand();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif