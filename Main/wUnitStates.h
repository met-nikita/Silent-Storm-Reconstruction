#ifndef __wUnitStates_H_
#define __wUnitStates_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "RPGUnitMission.h"
namespace NRPG
{
	//enum ECriticalAction;
	enum ECritical;
}		
namespace NAI
{
	struct SUnitPosition;
}
namespace NWorld
{
class CCmd;
class CCommandExecute;
class CUnitServer;
class CCannon;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCriticalsBan
{
	unordered_map< int, unordered_map< int, list<NDb::ECritical> > > commandsBans;
	//
	int GetParam( CUnitServer *pUS, CCmd *pCmd );
	int GetObjectID( CObjectBase *pObject );
	void AddCommandBans( int nCommandID, int nParam, ... );
	//
public:
	CCriticalsBan();
	const list<NDb::ECritical> &GetCommandBans( CUnitServer *pUS, CCmd *pCmd );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitState: public CObjectBase
{
protected:
	ZDATA
	CPtr<CUnitServer> pUS;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUS); return 0; }
	CUnitState( CUnitServer *_pUS = 0 ): pUS(_pUS) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) = 0;
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual bool IsCriticalsFailCommand( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void OnActionFinish() {}
	virtual void OnStartNewTurn() {}
	virtual void OnFinishOwnTurn() {}
	virtual void OnFinishTimeOrTurn( bool bRealTime ) {} // differs from OnFinishOwnTurn in that it is called every N seconds in realtime
	virtual void OnStartRealTime() {}
	virtual void OnDeath() {}
	virtual void FilterCriticals();
	virtual void Segment() {}
	virtual void OnStateStarted() {}
	virtual void OnStateFinished() {}
	virtual void OnUnitDied( CUnitServer *pUS ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateNormal: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateNormal);
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
	bool IsInactive() const;
	const NDb::CPanzerklein *GetPK() const; // returns 0 if no PK present
public:
	CUnitStateNormal( CUnitServer *_pUS = 0 ): CUnitState(_pUS) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void FilterCriticals();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateSniping: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateSniping);
	ZDATA_(CUnitState)
	int nBaseAP; // AP before snipe
	int nSnipeAP; // AP accumulated during snipe
	CPtr<CUnitServer> pTarget;
	NAI::SUnitPosition InitialTargetPosition;
	NAI::SUnitPosition TargetPosition;
	// release-added (tag 7, @0x3cbe50): the chosen called-shot hit location of the snipe.
	// The release ctor takes it as a 4th arg; this predecessor snipe has no called-shot
	// machinery, so it stays HL_BODY (un-called) -- save-format member only, behavior deferred.
	// (The release also retyped Initial/TargetPosition SUnitPosition->CVec3, but both are 12
	// bytes raw-serialized, so tags 5/6 are byte-identical and the dev type is kept for GetCenter().)
	NAI::EHitLocation hitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitState*)this); f.Add(2,&nBaseAP); f.Add(3,&nSnipeAP); f.Add(4,&pTarget); f.Add(5,&InitialTargetPosition); f.Add(6,&TargetPosition); f.Add(7,&hitLocation); return 0; }

	bool CheckTarget();

public:
	CUnitStateSniping() : hitLocation(NAI::HL_BODY) {}
	CUnitStateSniping( CUnitServer *_pUS, CUnitServer *_pTarget, int _nBaseAP );

	void CancelSnipe();

	virtual void Segment();
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );

	virtual CUnitServer *GetTarget() { return pTarget; }
	virtual void CollectSnipeAP( int _nAP );
	virtual int GetCollectedSnipeAP() { return nSnipeAP; }
	int GetBaseSnipeAP() const { return nBaseAP; } // the snipe-AP budget set at snipe start (the pool to fill)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateUsingCannon: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateUsingCannon);
	ZDATA_(CUnitState)
	CPtr<CCannon> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitState*)this); f.Add(2,&pCannon); return 0; }
public:
	CUnitStateUsingCannon() {}
	CUnitStateUsingCannon( CUnitServer *_pUS, CCannon *_pCannon ): CUnitState(_pUS), pCannon(_pCannon) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void OnActionFinish();
	virtual void OnDeath();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateCorpseCarrier: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateCorpseCarrier);
	ZDATA_(CUnitState)
	CPtr<CUnitServer> pDeadUnit;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitState*)this); f.Add(2,&pDeadUnit); return 0; }
	CUnitStateCorpseCarrier() {}
	CUnitStateCorpseCarrier( CUnitServer *_pUS, CUnitServer *_pDead );
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void OnStateStarted();
	virtual void OnStateFinished();
	CUnitServer *GetCorpse() { return pDeadUnit; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateHealer: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateHealer);
	// Retail tag 7 is a raw 40-byte SUnitInfo snapshot. The live UI struct in
	// this fork has extra fields, so keep the on-disk record separate.
	struct STreatmentInfo
	{
		bool bPKInfo;
		int nHP, nHealedHP, nMaxHP, nPKHP, nMaxPKHP, nAP, nMaxAP, nSightDistance;
		bool bUnitInfo;
		STreatmentInfo(): bPKInfo(false), nHP(0), nHealedHP(0), nMaxHP(0),
			nPKHP(0), nMaxPKHP(0), nAP(0), nMaxAP(0), nSightDistance(0), bUnitInfo(false) {}
	};
	ZDATA_(CUnitState)
	CPtr<CUnitServer> pTarget;
	bool bNewSegment;
	float fKitCapacity;
	int nCriticalHealAPRequired;
	NRPG::SHealCriticalInfo healCriticalInfo;
	STreatmentInfo sTargetInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitState*)this); f.Add(2,&pTarget); f.Add(3,&bNewSegment); f.Add(4,&fKitCapacity); f.Add(5,&nCriticalHealAPRequired); f.Add(6,&healCriticalInfo); f.Add(7,&sTargetInfo); return 0; }
	//
	void SayAck( bool bRepair );
	void DoHealing( int nUnitAP );
	void SetCriticalHealAPRequired();
	//
public:
	CUnitStateHealer();
	CUnitStateHealer( CUnitServer *_pUS, CUnitServer *_pTarget );
	//
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void OnFinishTimeOrTurn( bool bRealTime );
	virtual void OnStateStarted();
	virtual void OnStateFinished();
	virtual void OnUnitDied( CUnitServer *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateEngineering: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateEngineering);
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
public:
	CUnitStateEngineering( CUnitServer *_pUS = 0 ): CUnitState(_pUS) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateStun: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateStun);
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
public:
	CUnitStateStun( CUnitServer *_pUS = 0 ): CUnitState(_pUS) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void Segment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateDeath: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateDeath);
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
public:
	CUnitStateDeath( CUnitServer *_pUS = 0 ): CUnitState(_pUS) {}
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void FilterCriticals();
	virtual void OnStateStarted();
	virtual void OnStateFinished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateUnconscious: public CUnitState
{
	OBJECT_BASIC_METHODS(CUnitStateUnconscious);
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
public:
	//
	CUnitStateUnconscious( CUnitServer *_pUS = 0 );
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void FilterCriticals();
	virtual void OnStateStarted();
	virtual void OnStateFinished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateInPocket
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitStateInPocket: public CUnitState
{
	OBJECT_BASIC_METHODS( CUnitStateInPocket );
	ZDATA
	ZPARENT( CUnitState );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitState *)this); return 0; }
public:
	//
	CUnitStateInPocket( CUnitServer *_pUS = 0 );
	//
	virtual CCommandExecute* CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) { return 0; }
	virtual void ProcessCritical( NDb::ECritical eCA ) {}
	virtual void FilterCriticals() {}
	virtual void OnStateStarted();
	virtual void OnStateFinished();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
