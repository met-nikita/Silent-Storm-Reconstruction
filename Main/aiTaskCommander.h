#ifndef __AITASKCOMMANDER_H_
#define __AITASKCOMMANDER_H_

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

struct SMapUnit;

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPose;
class IAIUnit;
class CAICommander;
class CTaskCommand;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITaskCommander
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTask;
class CAITaskCommander: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAITaskCommander);
	ZDATA
	list< CObj<CTask> > Tasks;
	CPtr<CAICommander> pAICommander;
	list< CObj<CTask> > TasksToAddOrRemove;
	list<bool> TasksAddFlag;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&Tasks); f.Add(3,&pAICommander); f.Add(4,&TasksToAddOrRemove); f.Add(5,&TasksAddFlag); return 0; }
	//
	bool CanGetCommand() const;
	NWorld::CWorld *GetWorld() const;
public:
	//
	CAITaskCommander() {}
	CAITaskCommander( CAICommander *_pAICommander ): pAICommander( _pAICommander ) {}
	//
	virtual void AddTask( CTask *pTask );
	virtual void RemoveTask( CTask *pTask );
	virtual NWorld::CCommand* GetCommand();
	virtual bool IsEndOfTurn() const;
	virtual bool IsUnderControl( IAIUnit *pAIUnit ) const;
	virtual bool IsUnderControl( NWorld::CUnitServer *pUnitServer ) const;
	virtual void CreateRoute( NWorld::CUnitServer *pUnitServer, SMapUnit sMapUnit );
	virtual void Segment();
	virtual void OnTurnStarted();
	virtual void OnTurnFinished() {}
	virtual void OnUnitWasKilled( NWorld::CUnitServer *pUS );
	void RemoveUnit( NWorld::CUnitServer *pUS );
	void Synchronize();
	bool HasActiveTasks() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandList
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTask: public CObjectBase
{
	OBJECT_BASIC_METHODS(CTask);
	ZDATA
	bool bCircled; // ���� true, �� �������� ����������� �� �����	
	CPtr<NWorld::CUnitServer> pUnitServer; // ��� ��������� ��������
	vector< CObj<CTaskCommand> > Commands; // ��������
	int nCurrentCommand; // �������� ���������� �� ���������� ���������
	STime tTime; // ����� � �������� �������� ���������� task-�
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
	CPtr<NWorld::CUnitServer> pUnitServer; // unit server ����������� ��������
	list< CPtr<NWorld::CCmd> > Commands; // ����� ��� �������
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
// �TaskCommandChangePose
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
// �TaskCommandWait
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandWait: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandWait);
	ZDATA_(CTaskCommand)
	STime tTime; // �� ������ ������� �����
	STime tLength; // ������� �����
	bool bIsWaiting; // ���� ��� ���
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
#endif 