#include "StdAfx.h"
#include "wUnitMove.h"
#include "wUICommands.h"
#include "wUnitServer.h"
#include "Grid.h"
#include "wMain.h"
#include "wMainMoves.h"
#include "aiPath.h"
#include "wMainPath.h"   // unit-param FindPath overload (TryToSetNewPath reroute), as the CUnitServer::Segment pump used
#include "RPGItem.h"
#include "RPGUnitMission.h"
#include "aiMoves.h"
#include "..\DBFormat\DataRPG.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdTravel: public CCmdUnit
{
public:
	ZDATA_(CCmdUnit)
	NAI::SUnitPosition pos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&pos); return 0; }
	CCmdTravel() {}
	CCmdTravel( CUnit *_pUnit, const NAI::SUnitPosition &_pos ): CCmdUnit(_pUnit), pos(_pos) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdChangePose: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdChangePose);
public:
	CCmdChangePose() {}
	CCmdChangePose( CUnit *_pUnit, const NAI::SUnitPosition &_pos ): CCmdTravel(_pUnit,_pos) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdRotate: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdRotate);
public:
	enum EPhase
	{
		START,
		MIDDLE,
	};
	ZDATA_(CCmdTravel)
	EPhase phase;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&phase); return 0; }
	
	CCmdRotate() {}
	CCmdRotate( CUnit *_pUnit, NAI::SUnitPosition &_pos, EPhase _phase ): CCmdTravel(_pUnit,_pos), phase(_phase) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdMove: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdMove);
public:
	ZDATA_(CCmdTravel)
	bool bInterGrid;
	bool bStrafe;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bInterGrid); f.Add(3,&bStrafe); return 0; }
	//
	CCmdMove() {}
	CCmdMove( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool _bInterGrid, bool _bStrafe ):
		CCmdTravel(_pUnit,_pos), bInterGrid(_bInterGrid), bStrafe(_bStrafe) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdClimb: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdClimb);
public:
	ZDATA_(CCmdTravel)
	bool bRealClimb;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bRealClimb); return 0; }
	//
	CCmdClimb() {}
	CCmdClimb( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool b ): CCmdTravel(_pUnit,_pos), bRealClimb(b) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdJump: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdJump);
public:
	ZDATA_(CCmdTravel)
	bool bRealJump;
	// @0x12681161 tag 3 -- retail's jump-BACK flag (a downward jump facing away from the move dir). Carried
	// + serialized for parity; the CUnitAnimator::Jump that renders the backward-jump animation (retail
	// @0x73e050, 4-arg) is a separate animation-subsystem convergence, so the flag is not yet consumed at
	// dispatch. (Retail also adds a scratch processPos + ProcessMe -- deferred with the animator.)
	bool bJumpBack;
	// @0x3bb000/@0x3b87fc tag 4 -- unit position snapshotted at jump dispatch (retail cmd+0x20); read by the
	// DEFERRED FallFromHigh next pass. Default SUnitPosition/SPathPlace ctor == retail's 0xfdffffff sentinel + null pNet.
	NAI::SUnitPosition processPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bRealJump); f.Add(3,&bJumpBack); f.Add(4,&processPos); return 0; }
	//
	CCmdJump(): bJumpBack(false) {}   // default the NEW tag-3 field so a pre-tag3 save loads it as false, not garbage
	CCmdJump( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool b, bool bBack = false ): CCmdTravel(_pUnit,_pos), bRealJump(b), bJumpBack(bBack) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdEndMove: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdEndMove);
public:
	ZDATA_(CCmdUnit)
	// @0x12541120 tag 2 -- retail replaced the (write-only) Jan03 bool bFreeze with the CCmdMove this
	// ender terminates, so DoCommand can EndMove( pWhatToEnd->pos ) even after a path-conflict reroute
	// nulled pCurCmd. A null pWhatToEnd means an end-ROTATE (dev still reuses CCmdEndMove for both;
	// retail's separate CCmdEndRotate is a later structural step).
	CObj<CCmdMove> pWhatToEnd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&pWhatToEnd); return 0; }

	CCmdEndMove() {}
	CCmdEndMove( CUnit *_pUnit, CCmdMove *_pWhatToEnd = 0 ): CCmdUnit(_pUnit), pWhatToEnd(_pWhatToEnd) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdActivateItem: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdActivateItem);
public:
	ZDATA_(CCmdUnit)
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&nSlot); return 0; }
	//
	CCmdActivateItem() {}
	CCmdActivateItem( CUnit *_pUnit, int _nSlot ): CCmdUnit(_pUnit), nSlot(_nSlot) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdDeactivateItem: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdDeactivateItem);
public:
	ZDATA_(CCmdUnit)
	int nSlot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&nSlot); return 0; }
	//
	CCmdDeactivateItem() {}
	CCmdDeactivateItem( CUnit *_pUnit, int _nSlot ): CCmdUnit(_pUnit), nSlot(_nSlot) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdMoveLadder: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdMoveLadder);
public:
	CCmdMoveLadder() {}
	CCmdMoveLadder( CUnit *_pUnit, const NAI::SUnitPosition &_pos ): CCmdTravel(_pUnit,_pos) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdEnterLadder: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdEnterLadder);
public:
	ZDATA_(CCmdTravel)
	bool bUp;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bUp); return 0; }
	//
	CCmdEnterLadder() {}
	CCmdEnterLadder( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool b ): CCmdTravel(_pUnit,_pos), bUp(b) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdLeaveLadder: public CCmdTravel
{
	OBJECT_BASIC_METHODS(CCmdLeaveLadder);
public:
	ZDATA_(CCmdTravel)
	bool bUp;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bUp); return 0; }
	//
	CCmdLeaveLadder() {}
	CCmdLeaveLadder( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool b ): CCmdTravel(_pUnit,_pos), bUp(b) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPathConflictsRemover (release module wWaitForOthers.obj): the release factored the move wait-state
// out of CExecMove into this CCommandExecute-derived base and widened nTimeToWait char->int
// (nTimeToWait). On the binary this class also carries the CheckCanDoMove gate + the locker-chain
// recovery helpers (GetWhoLocks/TryToSetNewPath/UseAnotherPCR/CheckLockerState/Segment) -- those are
// BEHAVIOR (not serialized) and are DEFERRED here; CExecMove's existing CanDoMove/IsWaitingForPath (the
// predecessor of CheckCanDoMove) stay the live driver, now operating on these inherited fields. This
// leg lands the SAVE-FORMAT change: the wait-state moves to the base + the timer widens to int, with
// the operator& tags byte-faithful to the release (CPathConflictsRemover @0x3bc810 / CExecMove @0x3bcb70).
class CPathConflictsRemover: public CCommandExecute
{
	ZDATA_(CCommandExecute)
protected:
	bool bWaiting;
	bool bAfterWaiting;
	int  nTimeToWait;   // release: was char nTimeToWait on CExecMove
	NAI::SUnitPosition posToWait;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bWaiting); f.Add(3,&bAfterWaiting); f.Add(4,&nTimeToWait); f.Add(5,&posToWait); return 0; }

	// @0x3bab70 -- retail ctor zeroes the FULL wait-state. Adding bAfterWaiting/nTimeToWait
	// to the init list fixes an uninitialized read: CExecMove::CanDoMove parks the unit
	// (bWaiting=true) without setting the timer, then CExecMove::IsWaitingForPath reads
	// nTimeToWait. posToWait keeps its SUnitPosition/SPathPlace default ctor (0xFDFFFFFF == retail).
	CPathConflictsRemover( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS), bWaiting(false), bAfterWaiting(false), nTimeToWait(0) {}

	// @0x3babd0 -- retail's trivial wait-state accessor (supersedes the Jan03 heavy
	// CExecMove::IsWaitingForPath, retired below): report bWaiting + the parked pose.
	virtual bool IsWaitingForPath( NAI::SUnitPosition *p = 0 ) { if ( p ) *p = posToWait; return bWaiting; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecMove: public CPathConflictsRemover, public IExecMove
{
	OBJECT_BASIC_METHODS(CExecMove);
	ZDATA_(CPathConflictsRemover)
	list<CObj<CCommand> > commandsQueue;
	CObj<CCommand> pCurCmd;
	bool bLastCommand;
	EFinishType result;
	NAI::SPathPlace desired;
	NAI::EFindPathParams eParams;

	vector<NAI::CPath::SPathAction> pathActions;
	bool bCheckCanRotate;
	ENeedActiveItem eNeedActive;   // release-new (tag 14): the path's active-item constraint, kept saved
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CPathConflictsRemover*)this); f.Add(2,&commandsQueue); f.Add(3,&pCurCmd); f.Add(4,&bLastCommand); f.Add(5,&result); f.Add(10,&desired); f.Add(11,&eParams); f.Add(12,&pathActions); f.Add(13,&bCheckCanRotate); f.Add(14,&eNeedActive); return 0; }

	void DoGameMove( const NAI::SUnitPosition &dst );
	void CheckDoors( const NAI::SUnitPosition &dst );
	NRPG::EAction GetAction( const NAI::SUnitPosition &dst ) const
	{
		CUnitAnimator &animator = pUS->animator;
		return GetMoveActionType( pUS->GetWorld()->GetPathNetwork(), pUS->GetPosition(), dst, animator.IsCarryingCorpse() );
	}
	bool HaveEnoughAP( const NAI::SUnitPosition &dst ) const
	{
		return pUS->CanSpendAP( pUS->GetActionAP( GetAction( dst ) ) );
	}
	int GetStartAP() const;
	
	// Movement tests
	// @0x3cc570 gate: route the move test through the granular CheckCanDoMove so a blocked cell recovers
	// per the ECanMoveRes verdict (CMR_LOCKED -> locker chain, CMR_DOOR -> reroute) instead of the Jan03
	// blanket park. CheckCanDoMove parks the unit + drives recovery and reports "may proceed now".
	bool CanDoMove( CCmdTravel *pCmd ) { return CheckCanDoMove( &pCmd->pos ); }

	// CPathConflictsRemover wait-state recovery (release wWaitForOthers.obj), reconstructed on CExecMove.
	bool CheckCanDoMove( NAI::SUnitPosition *reqPos );                        // @0x3cc570
	CUnitServer* GetWhoLocks();                                              // @0x3cc0d0
	void TryToSetNewPath();                                                  // @0x3cc130
	bool UseAnotherPCR( CUnitServer *server, CPathConflictsRemover *other ); // @0x3cc280
	void CheckLockerState();                                                // @0x3cc2d0
	
	bool TestSingleGameMove( CCmdTravel *pCmd )
	{
		if ( !CanDoMove(pCmd) )
			return false;
		if ( !HaveEnoughAP( pCmd->pos ) )
		{
			StopAction();
			//commandsQueue.push_front( pCmd );
			return false;
		}
		return true;
	}

	bool TestNextGameMove( CCmdTravel *pCmd )
	{
		if ( !CanDoMove(pCmd) )
			return false;
		if ( !HaveEnoughAP( pCmd->pos ) )
		{
			bLastCommand = true; // stop after AnimationEnd
			commandsQueue.push_front( pCmd );
			return false;
		}
		return true;
	}

	void CreateRotate( NAI::EDirection dir, const NAI::SUnitPosition &curPos );
	void DoCommand();
	void ConvertPath( NAI::CPath *pPath, ENeedActiveItem eActive );
public:
	CExecMove() {}
	CExecMove( CUnitServer *_pUS, NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, bool _bCheckCanRotate );
	virtual void Segment();                                                  // @0x3cc670 per-tick wait service + resume
	virtual CPathConflictsRemover* GetPathConflictsRemover() { return this; }// @0x3bc3f0 (CExecMove IS-A CPathConflictsRemover)
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void AnimationFinished();
	virtual void Cancel();
	virtual NAI::CPath* GetCurrentPath() const;
	virtual int GetActionAP() const;
	// IExecMove
	void GetSearchFromPosition( NAI::SPathPlace *pRes );
	void SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive );
	void GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams, ENeedActiveItem *pActive );
	void GetPathPoints( list<SPathPoint> *pRes );
	virtual void FullCancel();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecMove
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecMove::CExecMove( CUnitServer *_pUS, NAI::CPath *pPath, NAI::EFindPathParams _eParams, 
	ENeedActiveItem eActive, bool _bCheckCanRotate )
: CPathConflictsRemover(_pUS), result(FINISHED), eParams(_eParams), bCheckCanRotate(_bCheckCanRotate), eNeedActive(eActive)
{
	pUS->DynamicallyLockWay( pPath );
	ConvertPath( pPath, eActive );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::CheckDoors( const NAI::SUnitPosition &dst )
{
	CVec3 test( dst.GetCP() );
	for ( int i = 0; i < pathActions.size(); ++i )
	{
		NAI::CPath::SPathAction action = pathActions[i];
		NAI::SPosition test2;
		test2.p = action.where;
		test2.SetNetwork( dst.pos.GetNetwork() );
		if ( !( test == test2.GetCP() ) )
			continue;
		CDynamicCast<IWindowDoor> pDoor( action.pObject );
		pDoor->OpenClose( action.action == NAI::PA_OPEN, false, pUS );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::DoGameMove( const NAI::SUnitPosition &dst )
{
	CheckDoors( dst );
	NRPG::EAction action = GetAction( dst );
	pUS->DoAction( action );
	pUS->DoGameMove( dst );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc570 -- may this move proceed now? On CMR_YES clear the wait and report "go". Otherwise park the
// unit and recover per the ECanMoveRes verdict: CMR_LOCKED -> walk the locker chain + wait 10 ticks;
// CMR_DOOR -> reroute; else (a pathed cell that isn't passable -- shouldn't happen) abort the move.
bool CExecMove::CheckCanDoMove( NAI::SUnitPosition *reqPos )
{
	ECanMoveRes res = pUS->CanDoGameMove( *reqPos );
	if ( res == CMR_YES )
	{
		bWaiting = false;
		return true;
	}
	// blocked: stop any in-progress turn-in-place, then enter the waiting state
	if ( CDynamicCast<CCmdRotate>( pCurCmd ) )
		pUS->animator.EndRotate( pUS->GetPosition() );
	bWaiting = true;
	posToWait = *reqPos;
	if ( res == CMR_LOCKED )
	{
		CheckLockerState();
		nTimeToWait = 10;
		bAfterWaiting = true;
	}
	else if ( res == CMR_DOOR )
	{
		TryToSetNewPath();   // may clear bWaiting on a successful reroute
	}
	else
	{
		// CMR_CANNOT_MOVE / CMR_NOT_PASSABLE: the path tracker should never see these -- abort the move.
		nTimeToWait = 0;
		FullCancel();
		pUS->animator.PlaceUnit( pUS->GetPosition() );
	}
	if ( bWaiting )
		return false;
	bAfterWaiting = true;
	pUS->animator.PlaceUnit( pUS->GetPosition() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc0d0 -- who currently locks the cell we want (posToWait)? null if nobody, or if it is not a live unit.
CUnitServer* CExecMove::GetWhoLocks()
{
	CObjectBase *locker = pUS->GetWorld()->GetPathNetwork()->GetWhoLocksThisPlace( posToWait.pos.p );
	CDynamicCast<CUnitServer> who( locker );
	if ( !IsValid( who ) )
		return 0;
	return who.GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc130 -- reroute around the obstacle: find a fresh path to the desired place and install it
// (clearing the wait), or abort the move if none exists.
void CExecMove::TryToSetNewPath()
{
	NAI::SPathPlace desiredPlace;
	NAI::EFindPathParams eParams2;
	ENeedActiveItem eActive2 = ITEM_NO_MATTER;
	GetDesiredPlace( &desiredPlace, &eParams2, &eActive2 );
	vector<NAI::SPathPlace> dst;
	dst.push_back( desiredPlace );
	NAI::SPathPlace src;
	GetSearchFromPosition( &src );
	CPtr<NAI::CPath> pPath = FindPath( pUS->GetWorld()->GetPathNetwork(), pUS, src, dst, 0, true, eParams2, pUS->IsStrafing() );
	if ( IsValid( pPath ) )
	{
		SetNewPath( pPath, eParams2, eActive2 );   // @0x3cc130 -- preserve the path's active-item constraint on reroute
		bWaiting = false;
	}
	else
		// No route: FullCancel latches the exec FAILED but (retail-faithful, oracle s2_execmove.h FullCancel)
		// leaves bWaiting set, so the unit parks until the cell frees or a new order arrives -- superseding
		// the Jan03 pump's immediate CancelAction()+pCurrentCmd=0 abandon. CheckCmdExecState reaps the exec
		// once a later retry clears bWaiting.
		FullCancel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc280 -- decide whether we may defer to the unit locking us. If it has no remover of its own it
// cannot step aside, so we reroute ourselves and report handled; otherwise we may pass only once it is
// no longer waiting for a path.
bool CExecMove::UseAnotherPCR( CUnitServer *server, CPathConflictsRemover *other )
{
	if ( !other )
	{
		// server == pUS is the degenerate "unit locks himself" case (retail logs it here).
		TryToSetNewPath();
		return true;
	}
	return !other->IsWaitingForPath( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc2d0 -- walk the chain of who-locks-whom from the unit blocking our target cell. A visited list
// breaks cycles (deadlock): if a link has no remover, or is no longer waiting, or the cycle loops back
// to us, we reroute ourselves and stop.
// DELIBERATE DIVERGENCE from retail @0x3cc2d0: retail inserts the running node into its visited set
// BEFORE the membership test, so on the 2nd hop it "finds" the just-inserted node and the walk always
// terminates at depth 2 (never inspecting past the second locker). This does a full cycle-walk instead:
// for chains 3+ deep it reroutes in cases retail would leave waiting -- non-crashing, and a reroute is
// the safer resolution than an unbounded wait. The head-of-chain rotation housekeeping (Hide +
// interrupted-counter bump, retail 0x7cc332-0x7cc384) is animation-only and elided.
void CExecMove::CheckLockerState()
{
	CUnitServer *who = GetWhoLocks();
	if ( !who )
	{
		TryToSetNewPath();
		return;
	}
	CUnitServer *myUnit = pUS;
	vector<CUnitServer*> visited;
	visited.push_back( myUnit );
	for ( ;; )
	{
		bool bSeen = false;
		for ( int i = 0; i < visited.size(); ++i )
			if ( visited[i] == who ) { bSeen = true; break; }
		if ( bSeen )
		{
			if ( who == myUnit )
				TryToSetNewPath();   // deadlock rooted at us -> break it by rerouting
			return;
		}
		CPathConflictsRemover *pcr = who->GetPathConflictsRemover();
		if ( !pcr )
		{
			// the locker has no remover -> it cannot move out of the way; reroute ourselves.
			TryToSetNewPath();
			return;
		}
		NAI::SUnitPosition otherWait;
		if ( !pcr->IsWaitingForPath( &otherWait ) )
			return;   // that locker isn't waiting -> it will free the cell soon
		visited.push_back( who );
		CObjectBase *nextLocker = pUS->GetWorld()->GetPathNetwork()->GetWhoLocksThisPlace( otherWait.pos.p );
		CDynamicCast<CUnitServer> next( nextLocker );
		if ( !IsValid( next ) )
			return;
		who = next.GetPtr();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cc670 -- per-tick service of the wait state: count the timer down, and once it expires while still
// waiting, realign the animation clock and retry the move via CheckCanDoMove. The resume is driven by the
// queued path ender (CCmdEndMove::pWhatToEnd) which DoCommand consumes once bWaiting clears (bAfterWaiting was
// latched at park) -- retail-faithful @0x3cc670, so NO ender is re-emitted here (the Stage-4 dev-preserving
// push is dropped now that CCmdEndMove carries pWhatToEnd).
void CExecMove::Segment()
{
	if ( nTimeToWait > 0 )
		nTimeToWait--;
	if ( bWaiting && nTimeToWait == 0 )
	{
		pUS->animator.AlignTime();
		CheckCanDoMove( &posToWait );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::DoCommand()
{
	if ( commandsQueue.empty() )
	{
		pCurCmd = 0;
		return;
  	ASSERT( commandsQueue.empty() || pCurCmd || !IsExecuting() );
	}
	CUnitAnimator &animator = pUS->animator;
	const NAI::SUnitPosition &position = pUS->GetPosition();
	NAI::SUnitPosition prevPos( position );
	// Hold an OWNING ref to the current command BEFORE the move gate below. CanDoMove -> CheckCanDoMove
	// can now reroute a blocked move (CMR_DOOR/CMR_LOCKED): TryToSetNewPath -> SetNewPath nulls pCurCmd,
	// dropping the SOLE ref and freeing this CCmdMove, then CheckCanDoMove returns true (bWaiting cleared)
	// -- so DoGameMove( pPrevMove->pos ) would read freed memory. Retail's DoCommand @0x3b86e0 AddRefs
	// pCurCmd here (the CStack_40 hold) for exactly this reason; the Jan03 dev tree took the hold too late.
	CObj<CCommand> pHoldPrev( pCurCmd );
	CDynamicCast<CCmdMove> pPrevMove(pCurCmd);
	if (pPrevMove)
	{
		if ( !bAfterWaiting )
		{
			if ( !CanDoMove( pPrevMove ) )
				return;
			DoGameMove( pPrevMove->pos );
		}
	}
	// @0x3b86e0 (0x3b87fc): a CCmdJump left in pCurCmd from the previous pass applies its fall NOW -- at the
	// start of this pass, AFTER the jump animation completed -- from the stored processPos snapshot minus the
	// landing target. The live position has already advanced to the target, so the snapshot is required.
	CDynamicCast<CCmdJump> pJumpCur(pCurCmd);
	if (pJumpCur)
		pUS->FallFromHigh( pJumpCur->processPos.GetCP().z - pJumpCur->pos.GetCP().z );
	// fetch command
	CObj<CCommand> pCmd = commandsQueue.front(), pHoldCmd(pCurCmd);
	commandsQueue.pop_front();
	// GLUE FIX (camera followed every walking enemy): the Jan03 per-move-STEP CUICmdUnit post drove
	// the follow-camera executor once per fetched move command, so a multi-tile walk re-pinned the
	// camera to the unit for the whole move. CUICmdUnit's ONLY consumer is that follow exec
	// (NGame::CreateExecutor). Retail replaced this with a priority-arbitrated, self-terminating,
	// eased CUICmdUnitCamera posted once per move (CExecMove::ProcessMoveCommand @0x3b83a0) -- NOT a
	// per-step hard follow. The genuine "fly to an event" posts survive (unit spotted @wMain.cpp:817,
	// unconscious @wUnitServer.cpp:172); dropping the per-step spam stops the walker glue.
	//pUS->GetWorld()->AddUICommand( new CUICmdUnit( pUS ) );
	// process it
	CDynamicCast<CCmdMove> pMove(pCmd);
	if (pMove)
	{
		CDynamicCast<CCmdMove> pTestPrevMove( pCurCmd );
		if ( pTestPrevMove == 0 || bAfterWaiting )
		{
			// first move
			CheckDoors( pUS->GetPosition() );
			if ( commandsQueue.empty() )
			{
				ASSERT( result == FAILED );
				return;
			}
			if ( !TestSingleGameMove( pMove ) )
			{
				commandsQueue.push_front( pCmd );
				return;
			}
			bAfterWaiting = false;
			animator.StartMove( pMove->bStrafe );
			pCurCmd = pCmd; // for correct Cancel() (possible in DoGameMove()) pCurCmd should be valid before DoGameMove
			pHoldCmd = pCurCmd;
			DoGameMove( pMove->pos );
			// fetch next cmd
			ASSERT( !commandsQueue.empty() );
			pCmd = commandsQueue.front();
			commandsQueue.pop_front();
		}
		CDynamicCast<CCmdMove> pPrevMove( pCurCmd );
		ASSERT( IsValid( pPrevMove ) );
		pCurCmd = pCmd;
		// need to check second time because of first move case
		CDynamicCast<CCmdMove> pNextMove(pCmd);
		if (pNextMove)
		{
			if ( !TestNextGameMove( pNextMove ) )
			{
				// unit should freeze in strange pose
				commandsQueue.push_front( pCmd );
				animator.EndMove( prevPos, pPrevMove->pos, true, pPrevMove->bInterGrid );
				return;
			}
			else
			{
				animator.Move( prevPos, pPrevMove->pos, pNextMove->pos, pPrevMove->bInterGrid );
				// lock next tile on the way
				pUS->LockNextPlace( pNextMove->pos );
			}
		}
		else {
			CDynamicCast<CCmdEndMove> pEndMove(pCmd);
			if (pEndMove)
			{
				// @0x3b86e0 -- terminate the move the ender remembers (pWhatToEnd), robust to a wait/reroute
				// having nulled pCurCmd. A null pWhatToEnd is an end-rotate.
				if ( IsValid( pEndMove->pWhatToEnd ) )
					animator.EndMove( prevPos, pEndMove->pWhatToEnd->pos, false, pEndMove->pWhatToEnd->bInterGrid );
				else
					animator.EndRotate( prevPos );
			}
			else
				ASSERT(0);
		}
	}
	else {
		CDynamicCast<CCmdEndMove> pEndMove(pCmd);
		if (pEndMove)
		{
			// @0x3b86e0 -- the ender remembers the move it terminates (pWhatToEnd), so this survives a
			// path-conflict reroute that nulled pCurCmd (the Jan03 pCurCmd cast wrongly fell to EndRotate).
			if ( IsValid( pEndMove->pWhatToEnd ) )
			{
				pCurCmd = pCmd;
				animator.EndMove(prevPos, pEndMove->pWhatToEnd->pos, false, pEndMove->pWhatToEnd->bInterGrid);
			}
			else
				animator.EndRotate(position);
		}
		else {
			CDynamicCast<CCmdClimb> pClimb(pCmd);
			if (pClimb)
			{
				// retail @0x3b86e0 travel arm: doors on both ends before the test; push the command back on failure
				CheckDoors( pUS->GetPosition() );
				CheckDoors( pClimb->pos );
				if (TestSingleGameMove(pClimb))
				{
					animator.Climb(position, pClimb->pos, pClimb->bRealClimb);
					pCurCmd = pCmd;
					DoGameMove(pClimb->pos);
				}
				else
				{
					commandsQueue.push_front(pCmd);
					return;
				}
			}
			else {
				CDynamicCast<CCmdJump> pJump(pCmd);
				if (pJump)
				{
					// retail @0x3b86e0 travel arm: doors on both ends before the test; push the command back on failure
					CheckDoors( pUS->GetPosition() );
					CheckDoors( pJump->pos );
					if (TestSingleGameMove(pJump))
					{
						pJump->processPos = pUS->GetPosition();   // retail ProcessMe @0x3bb000: snapshot BEFORE DoGameMove
						animator.Jump(position, pJump->pos, pJump->bRealJump, pJump->bJumpBack);	// bJumpBack: tag-3 producer @wUnitMove.cpp
						pCurCmd = pCmd;
						DoGameMove(pJump->pos);
						// fall deferred to the START of the next DoCommand pass (retail @0x3b87fc)
					}
					else
					{
						commandsQueue.push_front(pCmd);
						return;
					}
				}
				else {
					CDynamicCast<CCmdRotate> pRotate(pCmd);
					if (pRotate)
					{
						// CRAP - should test if rotate is actually possible! for lay pose this could be not the fact
						//if ( !CanSpendAP( pWorld, this, NRPG::AC_ROTATE ) )
						//{
						//	animator.EndRotate( position );
						//	commandsQueue.clear();
						//	return;
						//}
						//SpendAP( pWorld, this, NRPG::AC_ROTATE );
						// retail @0x3b86e0 travel arm: prod doors on both ends BEFORE the passability test
						CheckDoors( pUS->GetPosition() );
						CheckDoors( pRotate->pos );
						if (TestSingleGameMove(pRotate))
						{
							pCurCmd = pCmd;
							animator.Rotate(position, pRotate->pos, pRotate->phase == CCmdRotate::START);
							// retail @0x3b86e0 travel arm: rotates go through DoGameMove (doors/action/locks), not bare SetPosition
							DoGameMove(pRotate->pos);
						}
						else
						{
							commandsQueue.push_front(pCmd);
							return;
						}
					}
					else {
						CDynamicCast<CCmdChangePose> pChangePose(pCmd);
						if (pChangePose)
						{
							// retail @0x3b86e0 travel arm: doors on both ends before the test; push the command back on failure
							CheckDoors( pUS->GetPosition() );
							CheckDoors( pChangePose->pos );
							if (TestSingleGameMove(pChangePose))
							{
								animator.ChangePose(position, pChangePose->pos);
								pCurCmd = pCmd;
								DoGameMove(pChangePose->pos);
							}
							else
							{
								commandsQueue.push_front(pCmd);
								return;
							}
						}
						else {
							CDynamicCast<CCmdActivateItem> pActivateItem(pCmd);
							if (pActivateItem)
							{
								ASSERT(!animator.IsActiveItem());
								pCurCmd = pCmd;
								NRPG::IUnitMission* pRPG = pUS->GetUnitRPG();
								NRPG::IInventory* pInventory = pRPG->GetInventory();
								CPtr<NRPG::IInventoryItem> pItem = pInventory->Get((NDb::ESlot)pActivateItem->nSlot);
								if (IsValid(pItem))
								{
									NDb::EItemSubType subType = pItem->GetDBItem()->subType;
									if (subType == NDb::SUBTYPE_HEAVY)
										animator.ActivateItem(position, true, false, NDb::BELT_M1, pRPG->GetWeaponType());
									else if (subType == NDb::SUBTYPE_MINE_DETECTOR)
										animator.ActivateItem(position, true, true, NDb::BELT_M1, pRPG->GetWeaponType());
									else
									{
										int nPlace = pInventory->GetPlaceBySubType(subType);
										animator.ActivateItem(position, false, nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType());
									}
								}
							}
							else {
								CDynamicCast<CCmdDeactivateItem> pDeactivateItem(pCmd);
								if (pDeactivateItem)
								{
									ASSERT(animator.IsActiveItem());
									pCurCmd = pCmd;
									NRPG::IUnitMission* pRPG = pUS->GetUnitRPG();
									NRPG::IInventory* pInventory = pRPG->GetInventory();
									CPtr<NRPG::IInventoryItem> pItem = pInventory->Get((NDb::ESlot)pDeactivateItem->nSlot);
									if (IsValid(pItem))
									{
										NDb::EItemSubType subType = pItem->GetDBItem()->subType;
										if (subType == NDb::SUBTYPE_HEAVY)
											animator.DeactivateItem(position, true, false, NDb::BELT_M1);
										else if (subType == NDb::SUBTYPE_MINE_DETECTOR)
											animator.DeactivateItem(position, true, true, NDb::BELT_M1);
										else
										{
											int nPlace = pInventory->GetPlaceBySubType(subType);
											animator.DeactivateItem(position, false, nPlace == -1, (NDb::EItemPlace)nPlace);
										}
									}
								}
								else {
									CDynamicCast<CCmdMoveLadder> pMoveLadder(pCmd);
									if (pMoveLadder)
									{
										// retail @0x3b86e0 travel arm: doors on both ends before the test
										CheckDoors( pUS->GetPosition() );
										CheckDoors( pMoveLadder->pos );
										if (TestSingleGameMove(pMoveLadder))
										{
											animator.MoveLadder(position, pMoveLadder->pos);
											pCurCmd = pCmd;
											DoGameMove(pMoveLadder->pos);
										}
										else
										{
											commandsQueue.push_front(pCmd);
											return;
										}
									}
									else {
										CDynamicCast<CCmdEnterLadder> pEnterLadder(pCmd);
										if (pEnterLadder)
										{
											// retail @0x3b86e0 travel arm (generic over CCmdTravel, incl. ladders):
											// doors on both ends before the test; push the command back on failure
											CheckDoors( pUS->GetPosition() );
											CheckDoors( pEnterLadder->pos );
											if (TestSingleGameMove(pEnterLadder))
											{
												animator.EnterLadder(position, pEnterLadder->pos, pEnterLadder->bUp);
												pCurCmd = pCmd;
												DoGameMove(pEnterLadder->pos);
											}
											else
											{
												commandsQueue.push_front(pCmd);
												return;
											}
										}
										else {
											CDynamicCast<CCmdLeaveLadder> pLeaveLadder(pCmd);
											if (pLeaveLadder)
											{
												// retail @0x3b86e0 travel arm (generic over CCmdTravel, incl. ladders):
												// doors on both ends before the test; push the command back on failure
												CheckDoors( pUS->GetPosition() );
												CheckDoors( pLeaveLadder->pos );
												if (TestSingleGameMove(pLeaveLadder))
												{
													animator.LeaveLadder(position, pLeaveLadder->pos, pLeaveLadder->bUp);
													pCurCmd = pCmd;
													DoGameMove(pLeaveLadder->pos);
												}
												else
												{
													commandsQueue.push_front(pCmd);
													return;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::Run()
{
	ASSERT( !IsValid( pCurCmd ) );
	if ( IsValid( pCurCmd ) )
		return;
	if ( commandsQueue.empty() )
	{
		// @0x3b8bc0 -- retail latches result-dependent (state=(result!=FINISHED)+1),
		// NOT an unconditional Finished(): a queue drained after a FAILED Cancel must
		// report FAILED, matching the AnimationFinished tail latch.
		if ( result == FINISHED )
			Finished();
		else
			Failed();
		return;
	}
	StartAction( pUS->GetWorld(), SKIPPABLE );
	DoCommand();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecMove::GetStartAP() const
{
	for ( list<CObj<CCommand> >::const_iterator i = commandsQueue.begin(); i != commandsQueue.end(); ++i )
	{
		CDynamicCast<CCmdTravel> p(*i);
		if (p)
		{
			NRPG::EAction action = GetAction( p->pos );
			if ( action == NRPG::AC_NONE && bCheckCanRotate )
				action = NRPG::AC_ROTATE;
			int nAP = pUS->GetActionAP( action );
			return nAP;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecMove::TimeLabelReached()
{
	CDynamicCast<CCmdActivateItem> pActivateItem(pCurCmd);
	if (pActivateItem)
		pUS->SetUndrawItem( false );
	else {
		CDynamicCast<CCmdDeactivateItem> pDeactivateItem(pCurCmd);;
		if (pDeactivateItem)
			pUS->SetUndrawItem(true);
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::AnimationFinished()
{
	pUS->UpdateCriticalsState();
	//ASSERT( pCurCmd->IsValid() );
	if ( IsValid( pCurCmd ) || !commandsQueue.empty() )
	{
		if ( bLastCommand )
		{
			pCurCmd = 0;
			StopAction();
			bLastCommand = false;
		}
		else
			DoCommand();
	}
	pUS->DynamicallyLockWay( GetCurrentPath() );
	if ( !commandsQueue.empty() || IsValid( pCurCmd ) )
		return;
	if ( result == FINISHED )
		Finished();
	else
		Failed();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::Cancel()
{
	// @0x3b8cf0 -- retail makes Cancel a no-op while a CCmdJump is in flight: a jump
	// cannot be aborted mid-air, so leave it to land and be reaped by the Segment loop.
	CDynamicCast<CCmdJump> pWasJump( pCurCmd );
	if ( pWasJump )
		return;
	CDynamicCast<CCmdMove> pWasMove( pCurCmd );
	CDynamicCast<CCmdRotate> pWasRotate( pCurCmd );
	commandsQueue.clear();
	if ( pWasMove || pWasRotate )
		// @0x3b8cf0 -- the ender remembers the move it terminates (null for a rotate -> end-rotate).
		commandsQueue.push_back( new CCmdEndMove( pUS, pWasMove.GetPtr() ) );
	bWaiting = false;
	bAfterWaiting = false;
	nTimeToWait = 0;   // @0x3b8cf0 -- retail also clears the wait timer here
	result = FAILED;
	pUS->DynamicallyLockWay( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::FullCancel()
{
	commandsQueue.clear();
	result = FAILED;
	pCurCmd = 0;
	Finished();   // @0x3b8d50 -- retail latches state=FINISHED unconditionally here
	              // (disasm `[esi-8]=1`), an addition over Jan03, so the Segment loop reaps us.
	//pUS->animator.PlaceUnit( pUS->GetPosition() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive )
{
	CDynamicCast<CCmdMove> pWasMove( pCurCmd );
	CDynamicCast<CCmdRotate> pWasRotate( pCurCmd );
	if ( IsWaitingForPath() )
	{
		bWaiting = false;
		bAfterWaiting = false;
		pCurCmd = 0;
		pUS->animator.PlaceUnit( pUS->GetPosition() );
		nTimeToWait = 0;
		//StopAction();
	}	
	commandsQueue.clear();
#ifdef _DEBUG
	{
		NAI::SPathPlace p, firstPoint;
		GetSearchFromPosition( &p );
		firstPoint = pPath->points[0];
		p.SetMoving( 0 ); firstPoint.SetMoving( 0 );
		ASSERT( pPath->points[0] == p ); // check if starting point for path was taken with GetSearchFromPosition
	}
#endif

	pUS->DynamicallyLockWay( pPath );
	eParams = _eParams;
	eNeedActive = eActive;   // keep the saved active-item constraint current on re-route
	ConvertPath( pPath, eActive );
	if ( !commandsQueue.empty() )
	{
		CDynamicCast<CCmdMove> pStartMove(commandsQueue.front());
		if (pStartMove)
		{
			if ( pWasRotate )
				commandsQueue.push_front( new CCmdEndMove( pUS ) );
			if ( pWasMove && (pWasMove->bStrafe || pStartMove->bStrafe) )
				commandsQueue.push_front( new CCmdEndMove( pUS ) );
		}
		else {
			CDynamicCast<CCmdRotate> pStartRotate(commandsQueue.front());
			if (pStartRotate)
			{
				if (pWasMove)
					commandsQueue.push_front(new CCmdEndMove(pUS));
			}
			else
			{
				if (pWasMove || pWasRotate)
					commandsQueue.push_front(new CCmdEndMove(pUS));
			}
		}
	}
	else
		Cancel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::GetSearchFromPosition( NAI::SPathPlace *pRes )
{
	CDynamicCast<CCmdMove> pCurrentMove( pCurCmd );
	if ( pCurrentMove && !IsWaitingForPath() )
		*pRes = pCurrentMove->pos.pos.p;
	else
		*pRes = pUS->GetPosition().pos.p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::ConvertPath( NAI::CPath *pPath, ENeedActiveItem eActive )
{
	if ( pPath )
	{
	///	OutputDebugString("Converting path:");
	///	pPath->DebugOutput();
	}
	pathActions = pPath->actions;
	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	int nSlot = pInventory->GetActiveSlot();
	CUnitAnimator &animator = pUS->animator;
	bool bActive = animator.IsActiveItem();
	bool bInactivePose = ( pUS->GetPosition().pos.p.GetPose() == NAI::CM_INACTIVE );

	bLastCommand = false;
	ASSERT( IsValid( pPath ) );
	if ( !IsValid( pPath ) || pPath->points.size() < 2 )
	{
		if ( eActive == ITEM_ACTIVE && !bActive && !bInactivePose )
			commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
		else if ( eActive == ITEM_INACTIVE && bActive )
			commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
		desired = pUS->GetPosition().pos.p;
		return;
	}
	desired = pPath->points.back();
	NAI::IPathNetwork *pNet = pPath->pNet;
	//
	NRPG::IInventoryItem *pActiveItem = pInventory->GetActive();
	//CDynamicCast<NRPG::IWeaponItem> pWeapon(pActiveItem);
	bool bWeapon = ( pActiveItem != 0 && !animator.IsCarryingCorpse() );
	if ( !bWeapon && bActive )
	{
		commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
		bActive = false;
	}
	//
	bool bRun = pUS->GetPosition().bRun;
	bool bInterGrid = false;
	bool bMoving = false;
	CCmdMove *pLastMove = 0;   // the last CCmdMove pushed -> pWhatToEnd for the trailing CCmdEndMove
	NAI::SUnitPosition pos;
	NAI::SUnitPosition prevPos;
	for ( int i = 1; i < pPath->points.size(); ++i )
	{
		NAI::SPathPlace &prev = pPath->points[i-1];
		NAI::SPathPlace &cur = pPath->points[i];
		bInactivePose = ( cur.GetPose() == NAI::CM_INACTIVE );
		pos.pos.SetNetwork( pNet );
		pos.pos.p = cur;
		pos.bRun = bRun;
		prevPos.pos.SetNetwork( pNet );
		prevPos.pos.p = prev;
		prevPos.bRun = bRun;
		if ( cur.IsMoving() == false && bMoving )
		{
			if ( bInterGrid )
			{
				pLastMove = new CCmdMove( pUS, prevPos, true, pPath->bStrafePath );
				commandsQueue.push_back( pLastMove );
				bInterGrid = false;
			}
			commandsQueue.push_back( new CCmdEndMove( pUS, pLastMove ) );
			pLastMove = 0;   // retail @0x3b9330: the pending CPtr is released with its ender
			bMoving = false;
		}
		//
		if ( !prev.IsIntegral() || !cur.IsIntegral() )
		{
			if ( bWeapon && bActive )
			{
				commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
				bActive = false;
			}
			if ( prev.IsIntegral() == cur.IsIntegral() )
				commandsQueue.push_back( new CCmdMoveLadder( pUS, pos ) );
			else
			{
				NAI::ETransitionType tt = NAI::GetTransitionType( pNet, prev, cur );
				if ( cur.IsIntegral() )
					commandsQueue.push_back( new CCmdLeaveLadder( pUS, pos, tt == NAI::TT_LADDER_UP ) );
				else
					commandsQueue.push_back( new CCmdEnterLadder( pUS, pos, tt == NAI::TT_LADDER_UP ) );
			}
		}
		else if ( prev.GetLayer() != cur.GetLayer() )
		{
			NAI::ETransitionType type = NAI::GetTransitionType( pNet, prev, cur );
			if ( type == NAI::TT_INTERGRID_SAME )
				continue;
			if ( type == NAI::TT_TURN )
			{
				// retail @0x3b9330: a cross-layer in-place turn goes through CreateRotateQueue @0x3b9000
				// (end the pending move, then rotate to cur's heading) -- NOT the inter-grid fold below,
				// which silently drops the heading change.
				if ( bWeapon && !bActive )
				{
					commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
					bActive = true;
				}
				if ( pLastMove )
				{
					commandsQueue.push_back( new CCmdEndMove( pUS, pLastMove ) );
					pLastMove = 0;
				}
				CreateRotate( (NAI::EDirection)cur.GetDirection(), prevPos );
				bMoving = false;
				continue;
			}
			float fSqrDist = fabs2( pos.GetCPNoHeight() - prevPos.GetCPNoHeight() );
			if ( fSqrDist < sqr(0.2f) )
			{
				bInterGrid = true;
				bMoving = true;
				continue;
			}
			if ( bWeapon && !bActive )
			{
				commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
				bActive = true;
			}
			// change layer
			pLastMove = new CCmdMove( pUS, pos, true, pPath->bStrafePath );
			commandsQueue.push_back( pLastMove );
			bInterGrid = false;
			bMoving = true;
		}
		else if ( cur.GetPose() == NAI::CM_INACTIVE )
		{
			if ( bWeapon && bActive )
			{
				commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
				bActive = false;
			}
			// climb
			NAI::ETransitionType type = NAI::GetTransitionType( pNet, prev, cur );
			bool bReal = true;
			if ( type == NAI::TT_MOVE || type == NAI::TT_MOVE_DIAGONAL )
				bReal = false;
			else if ( type < NAI::TT_CLIMB_1 || type > NAI::TT_CLIMB_4 )
				ASSERT(0);
			commandsQueue.push_back( new CCmdClimb( pUS, pos, bReal ) );
		}
		else if ( prev.GetPose() == NAI::CM_INACTIVE )
		{
			if ( bWeapon && bActive )
			{
				commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
				bActive = false;
			}
			// jump
			NAI::ETransitionType type = NAI::GetTransitionType( pNet, prev, cur );
			bool bReal = true;
			if ( type == NAI::TT_MOVE || type == NAI::TT_MOVE_DIAGONAL )
				bReal = false;
			else if ( type != NAI::TT_JUMP && type != NAI::TT_JUMP_BACK )
				ASSERT(0);
			commandsQueue.push_back( new CCmdJump( pUS, pos, bReal, type == NAI::TT_JUMP_BACK ) );
		}
		else if ( prev.GetPose() != cur.GetPose() )
		{
			if ( bWeapon && !bActive )
			{
				commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
				bActive = true;
			}
			commandsQueue.push_back( new CCmdChangePose( pUS, pos ) );					
		}
		else if ( prev.GetX() == cur.GetX() && prev.GetY() == cur.GetY() && prev.GetDirection() != cur.GetDirection() )
		{
			if ( bWeapon && !bActive )
			{
				commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
				bActive = true;
			}
			// retail CreateRotateQueue @0x3b9000: a pending (unterminated) move is ended before the turn.
			if ( pLastMove )
			{
				commandsQueue.push_back( new CCmdEndMove( pUS, pLastMove ) );
				pLastMove = 0;
			}
			CreateRotate( (NAI::EDirection)cur.GetDirection(), prevPos );
			bMoving = false;   // retail @0x3b9330: pending==null after the rotate queue
		}
		else if ( prev.GetX() != cur.GetX() || prev.GetY() != cur.GetY() )
		{
			if ( bWeapon && !bActive )
			{
				commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
				bActive = true;
			}				
			// move
			pLastMove = new CCmdMove( pUS, pos, bInterGrid, pPath->bStrafePath );
			commandsQueue.push_back( pLastMove );
			bInterGrid = false;
			bMoving = true;
		}
	}
	if ( bInterGrid )
	{
		pLastMove = new CCmdMove( pUS, pos, true, pPath->bStrafePath );
		commandsQueue.push_back( pLastMove );
	}
	if ( bMoving )
		commandsQueue.push_back( new CCmdEndMove( pUS, pLastMove ) );

	if ( eActive == ITEM_ACTIVE && !bActive && !bInactivePose )
		commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
	else if ( eActive == ITEM_INACTIVE && bActive )
		commandsQueue.push_back( new CCmdDeactivateItem( pUS, nSlot ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::CreateRotate( NAI::EDirection dir, const NAI::SUnitPosition &curPos )
{
	if ( dir != curPos.GetDir() )
	{
		int a = dir, b = curPos.GetDir();
		if ( a < b )
			a += 8;
		if ( a - b > 4 )
			b += 8;
		int c = (a - b > 0) ? 1 : -1;
		for ( int i = b; i != a; i += c )
		{
			CCmdRotate::EPhase phase = CCmdRotate::MIDDLE;
			if ( i == b )
				phase = CCmdRotate::START;
			NAI::SUnitPosition newPos( curPos );
			newPos.pos.p.SetDirection( (i+c) & 7 );
			CCmdRotate *pRotate = new CCmdRotate( pUS, newPos, phase );
			commandsQueue.push_back( pRotate );
		}
		commandsQueue.push_back( new CCmdEndMove( pUS ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CPath* CExecMove::GetCurrentPath() const
{
	//if ( pCurrentPath->IsValid() )
	//	return pCurrentPath;
	NAI::CPath *pRes = new NAI::CPath;
	pRes->pNet = pUS->GetWorld()->GetPathNetwork();
	CDynamicCast<CCmdTravel> pCurrent( pCurCmd.GetPtr() );
	if ( pCurrent )
		pRes->points.push_back( pCurrent->pos.pos.p );
	else
		pRes->points.push_back( pUS->GetUnitPosition().pos.p );
	for ( list<CObj<CCommand> >::const_iterator i = commandsQueue.begin(); i != commandsQueue.end(); ++i )
	{
		CDynamicCast<CCmdTravel> p( *i );
		if ( p )
		{
			pRes->points.push_back( p->pos.pos.p );
			CDynamicCast<CCmdMove> pMove( *i );
			if ( pMove )
				pRes->bStrafePath = pMove->bStrafe;
		}
	}
	if ( pRes->points.size() < 2 )
	{
		CObj<NAI::CPath> pHold(pRes);
		return 0;
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathAPCalcer
{
	int nRes;
	NAI::SUnitPosition currentPos;
	bool bCorpse;
	CWorld *pWorld;
	NRPG::IUnitMission *pRPG;
public:
	CPathAPCalcer( CWorld *_pWorld, NRPG::IUnitMission *_pRPG, const NAI::SUnitPosition &_p, bool _bCorpse )
		: pWorld(_pWorld), pRPG(_pRPG), currentPos(_p), bCorpse(_bCorpse), nRes(0) {}
	void AddPoint( const NAI::SUnitPosition &_pos )
	{
		NRPG::EAction action = GetMoveActionType( pWorld->GetPathNetwork(), currentPos, _pos, bCorpse );
		nRes += pRPG->GetActionAP( currentPos.GetPose(), action );
		currentPos = _pos;
	}
	void AddPoint( const NAI::SPathPlace &_p )
	{
		NAI::SUnitPosition pos( currentPos );
		pos.pos.p = _p;;
		AddPoint( pos );
	}
	int GetResult() const { return nRes; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecMove::GetActionAP() const
{
	CPathAPCalcer apCalc( pUS->GetWorld(), pUS->GetUnitRPG(), pUS->GetPosition(), pUS->IsCarryingCorpse() );

	CDynamicCast<CCmdTravel> p( pCurCmd );
	if ( p )
		apCalc.AddPoint( p->pos );
	for ( list<CObj<CCommand> >::const_iterator i = commandsQueue.begin(); i != commandsQueue.end(); ++i )
	{
		CDynamicCast<CCmdTravel> p( *i );
		if ( p )
			apCalc.AddPoint( p->pos );
	}
	// calc actionCommands APs
	return apCalc.GetResult();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams, ENeedActiveItem *pActive )
{
	// @0x3b76f0 -- copy out the three stored search descriptors (retail added the active-item out-param).
	*pRes = desired;
	*pParams = eParams;
	*pActive = eNeedActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::GetPathPoints( list<SPathPoint> *pRes )
{
	CPathAPCalcer apCalc( pUS->GetWorld(), pUS->GetUnitRPG(), pUS->GetPosition(), pUS->IsCarryingCorpse() );

	CVec3 vLastPoint = pUS->GetPosition().GetCP();

	CDynamicCast<CCmdTravel> p( pCurCmd );
	if ( p )
	{
		apCalc.AddPoint( p->pos );

		CVec3 vPoint = p->pos.GetCP();
		pRes->push_back( SPathPoint( apCalc.GetResult(), p->pos.pos.GetFloor(), vPoint ) );
		vLastPoint = vPoint;
	}
	for ( list<CObj<CCommand> >::const_iterator i = commandsQueue.begin(); i != commandsQueue.end(); ++i )
	{
		CDynamicCast<CCmdTravel> p( *i );
		if ( p )
		{
			apCalc.AddPoint( p->pos );

			CVec3 vPoint = p->pos.GetCP();
			if ( fabs2( vPoint - vLastPoint ) > FP_EPSILON2 )
			{
				pRes->push_back( SPathPoint( apCalc.GetResult(), p->pos.pos.GetFloor(), vPoint ) );
				vLastPoint = vPoint;
			}
		}
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateSimpleMoveExecutor(
	CUnitServer *_pUS, NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, bool _bCheckCanRotate )
{
	if ( !_pUS->GetUnitRPG()->CanMove() )
		return 0;
	return new CExecMove( _pUS, pPath, _eParams, eActive, _bCheckCanRotate );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x02731142, CCmdMove )
REGISTER_SAVELOAD_CLASS( 0x02731143, CCmdRotate )
REGISTER_SAVELOAD_CLASS( 0x12681160, CCmdClimb )
REGISTER_SAVELOAD_CLASS( 0x12681161, CCmdJump )
REGISTER_SAVELOAD_CLASS( 0x12541120, CCmdEndMove )
REGISTER_SAVELOAD_CLASS( 0x13151130, CCmdChangePose )
REGISTER_SAVELOAD_CLASS( 0x113B1130, CCmdActivateItem )
REGISTER_SAVELOAD_CLASS( 0x114B1130, CCmdDeactivateItem )
REGISTER_SAVELOAD_CLASS( 0x03112170, CExecMove )
REGISTER_SAVELOAD_CLASS( 0x12352170, CCmdMoveLadder )
REGISTER_SAVELOAD_CLASS( 0x12352171, CCmdEnterLadder )
REGISTER_SAVELOAD_CLASS( 0x12352172, CCmdLeaveLadder )
