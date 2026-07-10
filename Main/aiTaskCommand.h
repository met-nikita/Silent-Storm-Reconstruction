#ifndef __AITASKCOMMAND_H_
#define __AITASKCOMMAND_H_

#include "aiPosition.h"
#include "time.h"
#include "../DBFormat/DataAnimation.h"

namespace NWorld
{
	class CCmd;
	class CCommand;
	class CUnitServer;
	class CWorld;
}

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiTaskCommand -- the CTask command-list walker + the CTaskCommand step family. Split out of
// aiTaskCommander.h (Stage-0 of the CAITaskCommander removal): the CTaskCommand family is retail's
// CRouteCommand family (its saveload ids already match the release exactly) and CTask is threaded through
// ~15 files (IAIUnit::GetRoute() returns CTask*, aiRoute/aiControl/aiRouteLogic/aiRouteMisc/scriptUnit/
// wUnitServer/aiSignal), so it survives the CAITaskCommander deletion. Only the CAITaskCommander container
// stays in aiTaskCommander.h (deleted in Stage-1). REGISTER_SAVELOAD ids are kept byte-identical -- their
// bodies moved verbatim to aiTaskCommand.cpp.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPose;
class IAIUnit;
class CAICommander;
class CTaskCommand;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandList
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTask: public CObjectBase
{
	OBJECT_BASIC_METHODS(CTask);
	ZDATA
	bool bCircled; // if true, commands are executed in a loop
	CPtr<NWorld::CUnitServer> pUnitServer; // who executes the commands
	vector< CObj<CTaskCommand> > Commands; // commands
	int nCurrentCommand; // command last handed off for execution
	STime tTime; // time at which the task starts executing
	bool bActive;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bCircled); f.Add(3,&pUnitServer); f.Add(4,&Commands); f.Add(5,&nCurrentCommand); f.Add(6,&tTime); f.Add(7,&bActive); return 0; }
	//
public:
	CTask() {}
	CTask( NWorld::CUnitServer *_pUnitServer, bool _bCircled );
	//
	void AddCommand( CTaskCommand *pCmd );
	void AddLookAround( bool bWalk = true );
	void AddRoaming( const NAI::SPathPlace &_p, int _nAPRadius );
	virtual NWorld::CCmd *GetCommand();
	virtual CTaskCommand *GetCurrentTaskCommand() { return Commands[nCurrentCommand]; }
	void SetToBeginning() { nCurrentCommand = -1; }
	void ClearCommands() { Commands.clear(); }
	virtual void OnNewTurn();
	virtual NWorld::CUnitServer *GetUnitServer() { return pUnitServer; }
	virtual bool IsEndOfTurn() const;
	virtual bool IsEmpty()  const { return Commands.empty(); }
	virtual bool IsEndOfTask()  const { return !bCircled && nCurrentCommand >= (int)Commands.size(); }
	virtual void DelayExecution( int nTime );
	virtual void DebugOutput();
	void Activate() { bActive = true; }
	void DeActivate() { bActive = false; }
	bool IsActive() { return bActive; }
	virtual void OnPerformerDied();
	virtual void OnTaskStarted();
	// aiTaskCommand additions (CAITaskCommander-removal re-home helpers): expose the walked command list
	// + the loop flag so a CAIRouteLogic can be built from a CAIRoute-produced CTask (SetUnitRoute /
	// CreateUnitRoute) without re-implementing CAIRoute::GetTask's waypoint->command translation. The
	// CObj-owned commands are handed out as CPtr refs (refcounted) -- the temporary CTask releases its
	// own refs on destruction while the route logic keeps the commands alive.
	bool IsCircled() const { return bCircled; }
	void GetCommands( vector< CPtr<CTaskCommand> > *pOut ) const
	{
		for ( vector< CObj<CTaskCommand> >::const_iterator i = Commands.begin(); i != Commands.end(); ++i )
		{
			CTaskCommand *pCmd = *i;
			pOut->push_back( pCmd );
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskSyncObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskSyncObjectClient: virtual public CObjectBase
{
public:
	virtual void OnUnlock() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskSyncObject: public CObjectBase
{
	OBJECT_BASIC_METHODS( CTaskSyncObject );
	ZDATA
	vector< CPtr<CTaskSyncObjectClient> > clients;
	list< CPtr<CTaskSyncObjectClient> > unlockedClients;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&clients); f.Add(3,&unlockedClients); return 0; }
	//
public:
	CTaskSyncObject() {}
	void Register( CTaskSyncObjectClient *pClient );
	void UnRegister( CTaskSyncObjectClient *pClient );
	void Unlock( CTaskSyncObjectClient *pClient );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommand
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommand: virtual public CObjectBase
{
	ZDATA
public:
	CPtr<NWorld::CUnitServer> pUnitServer; // unit server executing the commands
	list< CPtr<NWorld::CCmd> > Commands; // buffer for commands
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnitServer); f.Add(3,&Commands); return 0; }

	CTaskCommand( NWorld::CUnitServer *_pUnitServer = 0 ): pUnitServer(_pUnitServer) {}

	virtual NWorld::CCmd *GetCommand();
	virtual void Do() {}

	virtual bool IsEndOfCommand() { return Commands.empty(); }
	virtual bool IsEndOfUnitTurn() { return false; }

	virtual void SetUnitServer( NWorld::CUnitServer *_pUnitServer ) { pUnitServer = _pUnitServer; }
	virtual void DoCommand( NWorld::CCmd *pCmd ) { Commands.push_back(pCmd); }
	virtual void OnNewTurn() {}
	virtual void OnPerformerDied() {}
	virtual void OnCommandFinished() {}
	virtual void OnTaskStarted() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandSync
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandSync: public CTaskCommand, public CTaskSyncObjectClient
{
	OBJECT_BASIC_METHODS( CTaskCommandSync );
	ZDATA
	ZPARENT( CTaskCommand )
	bool bLocked;
	CObj<CTaskSyncObject> pSync;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand *)this); f.Add(3,&bLocked); f.Add(4,&pSync); return 0; }
public:
	CTaskCommandSync() {}
	CTaskCommandSync( CTaskSyncObject *_pSync );
	//
	virtual void Do();
	virtual bool IsEndOfCommand();
	virtual bool IsEndOfUnitTurn();
	virtual void OnPerformerDied();
	virtual void OnTaskStarted();
	virtual void OnUnlock();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandGoto
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandGoto: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandGoto);
	ZDATA_(CTaskCommand)
	SPosition ptPosition;
	bool bStrafe;                    // retail CRouteCommandGoto::bStrafe -- strafe (keep facing) vs. face-and-walk
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTaskCommand*)this); f.Add(2,&ptPosition); f.Add(3,&bStrafe); return 0; }
public:
	CTaskCommandGoto(): CTaskCommand( 0 ), bStrafe( false ) {}
	CTaskCommandGoto( SPosition _ptPosition, bool _bStrafe = false ): CTaskCommand( 0 ), ptPosition(_ptPosition), bStrafe(_bStrafe) {}
	//
	virtual void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandRoaming
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandRoaming: public CTaskCommand
{
	OBJECT_BASIC_METHODS( CTaskCommandRoaming );
	ZDATA
	ZPARENT( CTaskCommand );
	NAI::SPathPlace p;
	int nAPRadius;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand *)this); f.Add(3,&p); f.Add(4,&nAPRadius); return 0; }
public:
	CTaskCommandRoaming(): CTaskCommand() {}
	CTaskCommandRoaming( const NAI::SPathPlace &_p, int _nAPRadius ):
		CTaskCommand(), p( _p ), nAPRadius( _nAPRadius ) {}
	//
	virtual void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangePose
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandChangePose: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandChangePose);
	ZDATA_(CTaskCommand)
	EPose nPose;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTaskCommand*)this); f.Add(2,&nPose); return 0; }
public:
	CTaskCommandChangePose() {}
	CTaskCommandChangePose( EPose _nPose ): CTaskCommand(0), nPose(_nPose) {}
	//
	virtual void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandWait
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandWait: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandWait);
	ZDATA_(CTaskCommand)
	STime tTime; // until what moment to wait
	STime tLength; // how long to wait
	bool bIsWaiting; // waiting or not
	bool bNewTurnStarted;
	int nStartTurnID;   // turn id captured when the wait began -- makes the wait self-determining inside a CAIRouteLogic
	                    // (which never propagates OnNewTurn). release CRouteCommandWait::nTurnID@24.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTaskCommand*)this); f.Add(2,&tTime); f.Add(3,&tLength); f.Add(4,&bIsWaiting); f.Add(5,&bNewTurnStarted); f.Add(6,&nStartTurnID); return 0; }
public:
	CTaskCommandWait(): nStartTurnID(-1) {}
	CTaskCommandWait( STime _tLength ): CTaskCommand(), tLength(_tLength), bIsWaiting(false), bNewTurnStarted(false), nStartTurnID(-1) {}
	//
	virtual void Do();
	virtual bool IsEndOfCommand();
	virtual bool IsEndOfUnitTurn();
	virtual void OnNewTurn();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeDirection
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandChangeDirection: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandChangeDirection);
	ZDATA_(CTaskCommand)
	int nDirection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTaskCommand*)this); f.Add(2,&nDirection); return 0; }
public:
	CTaskCommandChangeDirection() {}
	CTaskCommandChangeDirection( int _nDirection ) : CTaskCommand(), nDirection(_nDirection) {}
	//
	virtual void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandCustomIdleAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandCustomIdleAnimation: public CTaskCommand
{
	OBJECT_BASIC_METHODS( CTaskCommandCustomIdleAnimation );
	ZDATA
	ZPARENT( CTaskCommand )
	CDBPtr<NDb::CAnimation> pAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand *)this); f.Add(3,&pAnimation); return 0; }
public:
	CTaskCommandCustomIdleAnimation( NDb::CAnimation *_pAnimation = 0 ) : CTaskCommand(), pAnimation( _pAnimation ) {}
	//
	virtual void Do();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AITASKCOMMAND_H_
