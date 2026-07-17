#ifndef __AITASKCOMMAND_H_
#define __AITASKCOMMAND_H_

#include "aiPosition.h"
#include "time.h"
#include "../DBFormat/DataAnimation.h"
#include "..\Misc\EventsBase.h"   // NGlobal::CEventRegister (CTaskCommandSync::regOnDie, retail CRouteCommandSync::regEvent @+0x18)
#include "eventUnit.h"            // NWorld::CEventOnUnitDiedOrLoseConsciousness (the sync barrier's died/unconscious drop)

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
// CRouteCommand family (aiRouteLogic.obj) and CTask is threaded through ~15 files (IAIUnit::GetRoute()
// returns CTask*, aiRoute/aiControl/aiRouteLogic/aiRouteMisc/scriptUnit/wUnitServer/aiSignal), so it
// survives the CAITaskCommander deletion. Only the CAITaskCommander container stays in aiTaskCommander.h
// (deleted in Stage-1).
//
// SERIALIZATION CONVERGENCE (retail v1.2 save parity, decomp + save byte-walk verified):
// every operator& tag table below matches the retail CRouteCommand family 1:1 so retail saves with
// in-progress AI routes load correctly. Retail wire shapes (gen/_save_tags.py, slots 1/4):
//   base CRouteCommand @0x9f190      {2 pUS CPtr, 3 commands list<CPtr<CCmd>>}   (tag2 len 8 nested)
//   Goto @0x9fc40                    {2 base, 3 SPosition, 4 bStrafe 1B}
//   Wait @0x9f730                    {2 base, 3 tLength, 4 tTime, 5 nTurnID}
//   ChangePose/ChangeWishPose/Look   {2 base, 3 int4}  (COMDAT-folded onto @0x9fba0)
//   Sync @0x9fd00                    {2 base, 3 pSync CObj, 4 bLocked 1B}
//   Roaming @0x9fca0                 {2 base, 3 place 4B, 4 nAPRadius 4B}
// NOTE the retail-id mapping: CTaskCommandChangeDirection is registered on 0x51222142 = retail
// CRouteCommandLook; CTaskCommandChangeWishPose (0x2305EC00) is the retail wish-pose-only step.
// CTask itself (0x51812130) is a DEV-ONLY wire id -- retail v1.2 registers nothing on it.
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
	// retail CRouteCommand::operator& @0x9f190: {2 pUS CPtr (CallObjectSerialize), 3 commands list<CPtr<CCmd>>}.
	// The tag-3 callee (@0x184f0, PDB-labelled Add<CPtr<CCmd>>) is by its decoded body the LIST serializer
	// (CountChunks + per-node CallObjectSerialize<CPtr<CCmd>>) -- the retail base persists the PENDING-COMMANDS
	// LIST, exactly this table. Byte-walk: nested base chunk len 8 (pUS 4B + empty list header).
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
	// retail CRouteCommand vtbl+0x10 -- fired when the step BECOMES CURRENT (CAIRouteLogic::GenerateCommand
	// @0x9a170 + PauseInner @0x98d10; the legacy CTask walker mirrors the call at its advance). Distinct from
	// OnTaskStarted (dev CTask task-level start hook, kept for the legacy walker).
	virtual void OnCommandStarted() {}
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
	// retail CRouteCommandSync::operator& @0x9fd00: {2 base, 3 pSync CObj, 4 bLocked 1B} (byte-walk slot 4:
	// 2:8 3:4 4:1). regOnDie is NOT serialized (retail skips its regEvent @+0x18 too; resubscribes from the ctor).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand *)this); f.Add(3,&pSync); f.Add(4,&bLocked); return 0; }
	// retail CRouteCommandSync::regEvent @+0x18 -- CEventRegister<CRouteCommandSync,CEventOnUnitDiedOrLoseConsciousness>:
	// a dead OR UNCONSCIOUS performer is dropped from the sync barrier so the group doesn't deadlock on him.
	NGlobal::CEventRegister< CTaskCommandSync, NWorld::CEventOnUnitDiedOrLoseConsciousness > regOnDie;
public:
	CTaskCommandSync(): regOnDie( this, &CTaskCommandSync::OnUnitDiedOrLoseConsciousness ) {}
	CTaskCommandSync( CTaskSyncObject *_pSync );
	//
	virtual void Do();                // retail @0x99d30
	virtual bool IsEndOfCommand();    // retail @0x98cf0
	virtual bool IsEndOfUnitTurn();   // retail @0x988e0
	virtual void OnCommandFinished(); // retail @0x9d950: re-arm bLocked for the next circled pass
	virtual void OnUnlock();
	// retail @0x99cf0 -- the CEventOnUnitDiedOrLoseConsciousness handler (replaces the dev OnPerformerDied
	// hook: the event also fires on unconsciousness, wUnitServer.cpp @0x3c2010/@0x3c2190)
	void OnUnitDiedOrLoseConsciousness( const NWorld::CEventOnUnitDiedOrLoseConsciousness &event );
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
	// retail CRouteCommandGoto::operator& @0x9fc40: {2 base, 3 pos SPosition, 4 bStrafe 1B}
	// (byte-walk slot 4: 2:8 3:12 4:1)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand*)this); f.Add(3,&ptPosition); f.Add(4,&bStrafe); return 0; }
public:
	CTaskCommandGoto(): CTaskCommand( 0 ), bStrafe( false ) {}
	CTaskCommandGoto( SPosition _ptPosition, bool _bStrafe = false ): CTaskCommand( 0 ), ptPosition(_ptPosition), bStrafe(_bStrafe) {}
	//
	virtual void Do();   // retail @0x99280
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
// CTaskCommandChangePose (retail CRouteCommandChangePose, id 0x51222141) -- set the wish pose AND re-pose
// in place (a PF_USE_POSEDIR path at the current spot).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandChangePose: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandChangePose);
	ZDATA_(CTaskCommand)
	EPose nPose;
	// retail operator& (COMDAT-folded family table @0x9fba0): {2 base, 3 pose int4} (byte-walk slot 4: 2:8 3:4)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand*)this); f.Add(3,&nPose); return 0; }
public:
	CTaskCommandChangePose() {}
	CTaskCommandChangePose( EPose _nPose ): CTaskCommand(0), nPose(_nPose) {}
	//
	virtual void Do();   // retail @0x994c0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeWishPose (retail CRouteCommandChangeWishPose, id 0x2305EC00) -- set the wish pose ONLY;
// the re-pose path is queued solely for a panzerklein wearer (a PK ignores the bare wish pose). This is the
// step the release route factories use for the MOVEMENT pose (CreateAIMoveToPositionLogic @0x9aad0,
// CreateAIStrafeToPositionLogic @0x9b8d0, RouteAddGoAndCheck @0xa24e0, RouteAddRoundUp @0xa2720); ChangePose
// (above) is the SETTLE step. Retail saves carry it (slot 4: one instance) -- without this class every such
// save misreads the route.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandChangeWishPose: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandChangeWishPose);
	ZDATA_(CTaskCommand)
	EPose nPose;
	// retail operator& (COMDAT-folded family table @0x9fba0): {2 base, 3 pose int4} (byte-walk slot 4: 2:8 3:4)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand*)this); f.Add(3,&nPose); return 0; }
public:
	CTaskCommandChangeWishPose() {}
	CTaskCommandChangeWishPose( EPose _nPose ): CTaskCommand(0), nPose(_nPose) {}
	//
	virtual void Do();   // retail @0x99600
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandWait
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandWait: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandWait);
	ZDATA_(CTaskCommand)
	STime tLength; // how long to wait, seconds (retail CRouteCommandWait +0x10)
	STime tTime;   // real-time deadline, captured when the step starts (retail +0x14)
	int nTurnID;   // TBS: turn id captured when the step starts (retail +0x18)
	// retail CRouteCommandWait::operator& @0x9f730: {2 base, 3 tLength, 4 tTime, 5 nTurnID} (byte-walk slot 4:
	// 2:8 3:4 4:4 5:4). NOTE the retail wait state is fully COMPUTED (IsWaiting @0x98e50) -- the dev flags
	// bIsWaiting/bNewTurnStarted are gone (they were never in the retail wire and their job is done by the
	// captured tTime/nTurnID pair).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand*)this); f.Add(3,&tLength); f.Add(4,&tTime); f.Add(5,&nTurnID); return 0; }
public:
	// retail value ctor @0x9cc40: tTime = 0, nTurnID = 0, tLength = arg (the default ctor @0x9cb70 leaves the
	// trio uninitialized -- loader-only; zero-init here is the value-ctor's own resting state)
	CTaskCommandWait(): tLength(0), tTime(0), nTurnID(0) {}
	CTaskCommandWait( STime _tLength ): CTaskCommand(), tLength(_tLength), tTime(0), nTurnID(0) {}
	//
	bool IsWaiting();                  // retail @0x98e50
	virtual void OnCommandStarted();   // retail @0x98db0: capture tTime/nTurnID (retail Wait has NO Do)
	virtual bool IsEndOfCommand();     // retail @0x98f60: !IsWaiting()
	virtual bool IsEndOfUnitTurn();    // retail @0x98f70: IsWaiting()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeDirection (retail CRouteCommandLook, id 0x51222142) -- turn in place to a fixed
// direction. Retail queues a CCmdLook at the current place with the direction folded in (@0x99740), NOT a
// PF_USE_DIR path.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandChangeDirection: public CTaskCommand
{
	OBJECT_BASIC_METHODS(CTaskCommandChangeDirection);
	ZDATA_(CTaskCommand)
	int nDirection;   // retail CRouteCommandLook::dir (EDirection, int4 on the wire)
	// retail operator& (COMDAT-folded family table @0x9fba0): {2 base, 3 dir int4} (byte-walk slot 4: 2:8 3:4)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CTaskCommand*)this); f.Add(3,&nDirection); return 0; }
public:
	CTaskCommandChangeDirection() {}
	CTaskCommandChangeDirection( int _nDirection ) : CTaskCommand(), nDirection(_nDirection) {}
	//
	virtual void Do();   // retail @0x99740
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
