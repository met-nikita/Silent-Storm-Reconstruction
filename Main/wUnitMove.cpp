#include "StdAfx.h"
#include "wUnitMove.h"
#include "wUICommands.h"
#include "wUnitServer.h"
#include "Grid.h"
#include "wMain.h"
#include "wMainMoves.h"
#include "aiPath.h"
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdTravel*)this); f.Add(2,&bRealJump); return 0; }
	//
	CCmdJump() {}
	CCmdJump( CUnit *_pUnit, NAI::SUnitPosition &_pos, bool b ): CCmdTravel(_pUnit,_pos), bRealJump(b) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmdEndMove: public CCmdUnit
{
	OBJECT_BASIC_METHODS(CCmdEndMove);
public:
	ZDATA_(CCmdUnit)
	bool bFreeze;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCmdUnit*)this); f.Add(2,&bFreeze); return 0; }
	
	CCmdEndMove() {}
	CCmdEndMove( CUnit *_pUnit, bool _bFreeze = false ): CCmdUnit(_pUnit), bFreeze(_bFreeze) {}
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

	CPathConflictsRemover( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS), bWaiting(false) {}
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
	bool CanDoMove( CCmdTravel *pCmd )
	{
		if ( !pUS->CanDoGameMove( pCmd->pos ) )
		{
			if ( CDynamicCast<CCmdRotate>( pCurCmd ) )
				pUS->animator.EndRotate( pUS->GetPosition() );
			bWaiting = true;
			posToWait = pCmd->pos;
			return false;
		}
		else 
		{
			bWaiting = false;
			return true;
		}
	}
	
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
	virtual bool IsWaitingForPath( NAI::SUnitPosition *p = 0 ); 
	virtual void Run();
	virtual bool TimeLabelReached();
	virtual void AnimationFinished();
	virtual void Cancel();
	virtual NAI::CPath* GetCurrentPath() const;
	virtual int GetActionAP() const;
	// IExecMove
	void GetSearchFromPosition( NAI::SPathPlace *pRes );
	void SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive );
	void GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams );
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
bool CExecMove::IsWaitingForPath( NAI::SUnitPosition *p )
{
	CUnitAnimator &animator = pUS->animator;
	if ( bWaiting )
	{
		bool bCanDo = pUS->CanDoGameMove( posToWait );
		if ( bCanDo )
		{
			if (nTimeToWait > 0)
				nTimeToWait--;
			if ( nTimeToWait == 0 )
			{
				animator.AlignTime();
				animator.PlaceUnit( pUS->GetPosition() );

				CDynamicCast<CCmdMove> pWasMove( pCurCmd );
				CDynamicCast<CCmdRotate> pWasRotate( pCurCmd );
				if ( pWasMove || pWasRotate )
					commandsQueue.push_back( new CCmdEndMove( pUS, true ) );
			}
		}
		else
		{
			nTimeToWait = 10;
			bAfterWaiting = true;
			bWaiting = true;
		}
		if ( p )
			*p = posToWait;
		return ( !bCanDo || nTimeToWait != 0 );
	}
	return false;
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
	// fetch command
	CObj<CCommand> pCmd = commandsQueue.front(), pHoldCmd(pCurCmd);
	commandsQueue.pop_front();
	pUS->GetWorld()->AddUICommand( new CUICmdUnit( pUS ) );
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
				animator.EndMove(prevPos, pPrevMove->pos, false, pPrevMove->bInterGrid);
			}
			else
				ASSERT(0);
		}
	}
	else {
		CDynamicCast<CCmdEndMove> pEndMove(pCmd);
		if (pEndMove)
		{
			CDynamicCast<CCmdMove> pPrevMove(pCurCmd);
			if (pPrevMove)
			{
				pCurCmd = pCmd;
				animator.EndMove(prevPos, pPrevMove->pos, false, pPrevMove->bInterGrid);
			}
			else
				animator.EndRotate(position);
		}
		else {
			CDynamicCast<CCmdClimb> pClimb(pCmd);
			if (pClimb)
			{
				if (TestSingleGameMove(pClimb))
				{
					animator.Climb(position, pClimb->pos, pClimb->bRealClimb);
					pCurCmd = pCmd;
					DoGameMove(pClimb->pos);
				}
			}
			else {
				CDynamicCast<CCmdJump> pJump(pCmd);
				if (pJump)
				{
					if (TestSingleGameMove(pJump))
					{
						animator.Jump(position, pJump->pos, pJump->bRealJump);
						//��� ��������, ���� ����� ���������� (���� ����� ��������, �������)
						//animator.Fall( pJump->pos, position.GetCP().z );
						pCurCmd = pCmd;
						float fLastH = position.GetCP().z;
						float fCurrH = pJump->pos.GetCP().z;
						DoGameMove(pJump->pos);
						pUS->FallFromHigh(fLastH - fCurrH);
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
						if (TestSingleGameMove(pRotate))
						{
							pCurCmd = pCmd;
							animator.Rotate(position, pRotate->pos, pRotate->phase == CCmdRotate::START);
							pUS->SetPosition(pRotate->pos);
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
							if (TestSingleGameMove(pChangePose))
							{
								animator.ChangePose(position, pChangePose->pos);
								pCurCmd = pCmd;
								DoGameMove(pChangePose->pos);
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
											if (TestSingleGameMove(pEnterLadder))
											{
												animator.EnterLadder(position, pEnterLadder->pos, pEnterLadder->bUp);
												pCurCmd = pCmd;
												DoGameMove(pEnterLadder->pos);
											}
										}
										else {
											CDynamicCast<CCmdLeaveLadder> pLeaveLadder(pCmd);
											if (pLeaveLadder)
											{
												if (TestSingleGameMove(pLeaveLadder))
												{
													animator.LeaveLadder(position, pLeaveLadder->pos, pLeaveLadder->bUp);
													pCurCmd = pCmd;
													DoGameMove(pLeaveLadder->pos);
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
		Finished();
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
	CDynamicCast<CCmdMove> pWasMove( pCurCmd );
	CDynamicCast<CCmdRotate> pWasRotate( pCurCmd );
	commandsQueue.clear();
	if ( pWasMove || pWasRotate )
		commandsQueue.push_back( new CCmdEndMove( pUS, true ) );
	bWaiting = false;
	bAfterWaiting = false;
	result = FAILED;
	pUS->DynamicallyLockWay( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecMove::FullCancel()
{
	commandsQueue.clear();
	result = FAILED;
	pCurCmd = 0;
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
				commandsQueue.push_back( new CCmdMove( pUS, prevPos, true, pPath->bStrafePath ) );
				bInterGrid = false;
			}
			commandsQueue.push_back( new CCmdEndMove( pUS ) );
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
			commandsQueue.push_back( new CCmdMove( pUS, pos, true, pPath->bStrafePath ) );
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
			else if ( type != NAI::TT_JUMP )
				ASSERT(0);
			commandsQueue.push_back( new CCmdJump( pUS, pos, bReal ) );
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
			// rotate
			CreateRotate( (NAI::EDirection)cur.GetDirection(), prevPos );
		}
		else if ( prev.GetX() != cur.GetX() || prev.GetY() != cur.GetY() )
		{
			if ( bWeapon && !bActive )
			{
				commandsQueue.push_back( new CCmdActivateItem( pUS, nSlot ) );
				bActive = true;
			}				
			// move
			commandsQueue.push_back( new CCmdMove( pUS, pos, bInterGrid, pPath->bStrafePath ) );
			bInterGrid = false;
			bMoving = true;
		}
	}
	if ( bInterGrid )
		commandsQueue.push_back( new CCmdMove( pUS, pos, true, pPath->bStrafePath ) );
	if ( bMoving )
		commandsQueue.push_back( new CCmdEndMove( pUS ) );

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
void CExecMove::GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams )
{
	*pRes = desired;
	*pParams = eParams;
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
