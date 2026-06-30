#pragma once
#include "wUnitServer.h"
#include "wUnitMove.h"
#include "wUnitCommands.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSimpleExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSimpleExecQueue : public CCommandExecute
{
	OBJECT_BASIC_METHODS(CSimpleExecQueue);
protected:
	ZDATA_(CCommandExecute)
	list<CObj<CCommandExecute> > execList;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&execList); return 0; }

	CSimpleExecQueue( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}

	void AddExecutor( CCommandExecute *pExec );
	void AddFrontExecutor( CCommandExecute *pExec );
	int GetStartAP() const;
	int GetActionAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void AnimationFinished();
	virtual void Cancel();
	virtual bool IsExecuting();
	virtual bool IsWaitingForPath( NAI::SUnitPosition *p );
	virtual NAI::CPath* GetCurrentPath() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecQueue : public CSimpleExecQueue, public IExecMove
{
	OBJECT_BASIC_METHODS(CExecQueue);
	ZDATA_(CSimpleExecQueue)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CSimpleExecQueue*)this); return 0; }

	CExecQueue( CUnitServer *_pUS = 0 ): CSimpleExecQueue(_pUS) {}

	void CheckOpenCloseOnce();
	// IExecMove
	virtual void GetSearchFromPosition( NAI::SPathPlace *pRes );
	virtual void GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams );
	virtual void GetPathPoints( list<SPathPoint> *pRes );
	virtual void FullCancel();
	void SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive = ITEM_NO_MATTER );
	void AddPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, IExecMove *pOldFront = 0, 
		bool bCheckCanRotate = true );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecOpenClose
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecOpenClose: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecOpenClose);
private:
	ZDATA_(CCommandExecute)
	CObj<CCmdOpenClose> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }

public:
	CExecOpenClose() {}
	CExecOpenClose( CUnitServer *_pUS, CCmdOpenClose *_pCmd );
	void GetParams( bool *pBOpen, IObject **ppObject ) { *pBOpen = pCmd->bOpen; *ppObject = pCmd->pObject; }

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
	virtual bool TimeLabelReached();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecBlowTrappedDoor
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindowDoor;
/*class CExecBlowTrappedDoor: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecBlowTrappedDoor);
private:
	ZDATA_(CCommandExecute)
	CPtr<CWindowDoor> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pTarget); return 0; }

public:
	CExecBlowTrappedDoor() {}
	CExecBlowTrappedDoor( CUnitServer *_pUS, CWindowDoor *_pTarget );

	virtual EUnitCommandResult CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget = false ) const;
	virtual int GetStartAP() const;
	virtual void Run();
};*/
}
