#include "StdAfx.h"
#include "wUnitQueue.h"
#include "aiPath.h"
#include "wMain.h"
#include "wOSBase.h"
#include "wObject.h"
#include "..\DBFormat\DataAI.h"
#include "RPGUnitInfo.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSimpleExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AddExecutor( CCommandExecute *pExec )
{
	ASSERT( pExec->GetUnitServer() == pUS );
	execList.push_back( pExec );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AddFrontExecutor( CCommandExecute *pExec )
{
	ASSERT( pExec->GetUnitServer() == pUS );
	execList.push_front( pExec );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleExecQueue::GetStartAP() const 
{ 
	if ( !execList.empty() )
		return execList.front()->GetStartAP();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleExecQueue::GetActionAP() const 
{ 
	int nTemp = 0;
	NAI::SUnitPosition pos = pUS->GetPosition(), dst = pos;
	for( list<CObj<CCommandExecute> >::const_iterator iTemp = execList.begin(); iTemp != execList.end(); iTemp++ )
	{
		nTemp += (*iTemp)->GetActionAP();
		CObj<NAI::CPath> pPath = (*iTemp)->GetCurrentPath();
		if ( pPath )
			dst.pos.p = pPath->points.back();
		pUS->SetTemporaryPosition( dst );
	}
	pUS->SetTemporaryPosition( pos );
	return nTemp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::Run()
{
	while ( !execList.empty() )
	{
		CObj<CSimpleExecQueue> pHold( this ); // на тот случай, если юнит умер в момент выполнения действия
		execList.front()->Run();
		EFinishType f = execList.front()->GetState();
		if ( f == RUNNING )
			return;
		if ( f == FAILED )
		{
			Failed();
			execList.clear();
			return;
		}
		ASSERT( f == FINISHED );
		execList.pop_front();
		if ( !pUS->CanSpendAP( GetStartAP() ) )
			return;
	}
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::TimeLabelReached() 
{ 
	if ( !execList.empty() )
		return execList.front()->TimeLabelReached();
	return false; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AnimationFinished() 
{ 
	if ( !execList.empty() )
	{
		execList.front()->AnimationFinished();
		EFinishType f = execList.front()->GetState();
		if ( f != RUNNING )
		{
			if ( f == FAILED )
			{
				Failed();
				execList.clear();
				return;
			}
			ASSERT( f == FINISHED );
			execList.pop_front();
			if ( pUS->CanSpendAP( GetStartAP() ) )
				Run();
			return;
		}
		else
			return;
	}
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::Cancel()
{
	if ( !execList.empty() )
	{
		CCommandExecute *pFront = execList.front();
		pFront->Cancel();
		if ( pFront->GetState() == FINISHED )
			Finished();
		else if ( pFront->GetState() == FAILED )
			Failed();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::IsExecuting()
{ 
	if ( !execList.empty() )
		return execList.front()->IsExecuting();
	return true; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::IsWaitingForPath( NAI::SUnitPosition *p ) 
{ 
	if ( !execList.empty() )
		return execList.front()->IsWaitingForPath( p );
	return false; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CPath* CSimpleExecQueue::GetCurrentPath() const 
{ 
	if ( !execList.empty() )
	{
		NAI::CPath *pRes = 0;
		list<CObj<CCommandExecute> >::const_iterator it;
		for ( it = execList.begin(); it != execList.end(); ++it )
		{
			CObj<NAI::CPath> pCurrent = (*it)->GetCurrentPath();
			if ( !pCurrent )
				continue;
			if ( !pRes )
			{
				pRes = new NAI::CPath;
				pRes->pNet = pUS->GetWorld()->GetPathNetwork();
				pRes->points.push_back( pUS->GetUnitPosition().pos.p );
				pRes->bStrafePath = pCurrent->bStrafePath;
			}
			for ( int i = 0; i < pCurrent->points.size(); ++i )
			{
				if ( !( pCurrent->points[i] == pUS->GetUnitPosition().pos.p ) )
					pRes->points.push_back( pCurrent->points[i] );
			}
		}
		return pRes;
	}
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetSearchFromPosition( NAI::SPathPlace *pRes )
{
	if ( !execList.empty() )
	{
		CCommandExecute *pFront = execList.front();
		CDynamicCast<IExecMove> pMove(pFront);
		if (pMove)
		{
			pMove->GetSearchFromPosition( pRes );
			return;
		}
	}
	*pRes = pUS->GetPosition().pos.p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams )
{
	// get last move point
	*pRes = pUS->GetPosition().pos.p;
	*pParams = NAI::PF_DEFAULT;
	list<CObj<CCommandExecute> >::iterator i;
	for ( i = execList.begin(); i != execList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
			pMove->GetDesiredPlace( pRes, pParams );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::FullCancel()
{
	if ( !execList.empty() )
	{
		CPtr<CCommandExecute> pFront = execList.front();
		CDynamicCast<IExecMove> pMove(pFront);
		if (pMove)
			pMove->FullCancel();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetPathPoints( list<SPathPoint> *pRes )
{
	for ( list<CObj<CCommandExecute> >::iterator i = execList.begin(); i != execList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
			pMove->GetPathPoints( pRes );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive )
{
	CObj<CCommandExecute> pHold = execList.front(); 
	CDynamicCast<IExecMove> pOldCommand( pHold );
	execList.pop_front();
	list<CObj<CCommandExecute> > oldExecList = execList;
	execList.clear();
	if ( !pOldCommand )
		pHold->Cancel();
	AddPath( pPath, _eParams, eActive, pOldCommand );
	list<CObj<CCommandExecute> >::iterator i, firstToAdd = oldExecList.end(); 
	NAI::SPathPlace p;
	NAI::EFindPathParams params;
	for ( i = oldExecList.begin(); i != oldExecList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
		{
			pMove->GetDesiredPlace( &p, &params );
			if ( p == pPath->points.back() && _eParams == params )
			{
				firstToAdd = i;
				++firstToAdd;
				break;
			}
		}
	}
	for ( i = firstToAdd; i != oldExecList.end(); ++i )
		execList.push_back( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanSimplyOpen( NAI::IPathNetwork *pNet, NAI::SPathPlace &from, IWindowDoor *pDoor, int *pNewDir, bool bOpen )
{
	vector<NAI::SPathPlace> res;
	CDynamicCast<IGetApproaches> pAppr( pDoor );
	pAppr->GetApproaches( &res, pNet );
	int nDir = from.GetDirection(), nBestDiff = 7;
	int &nBestDir = *pNewDir;
	for ( int nRes = 0; nRes < res.size(); ++nRes )
	{
		int nNewDir = pNet->GetClosestDir( from, res[ nRes ] );
		int nDiff = abs( nNewDir - nDir ); 
		if ( nDiff > 4 )
			nDiff = 8 - nDiff;
		if ( nDiff < nBestDiff )
		{
			nBestDiff =  nDiff;
			nBestDir = nNewDir;
		}
	}

	CVec3 ptToOpen = pDoor->GetChangeStateDirection( bOpen );
	NAI::SPosition pos;
	pos.SetNetwork( pNet );
	pos.p = from;
	//pos.p.SetDirection( nBestDir );
	float fDir = pos.GetDirection();
	float fScalarProd = ptToOpen.x * cos( fDir ) + ptToOpen.y * sin( fDir );
	return fScalarProd > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::AddPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, IExecMove *pOldFront,
	bool bCheckCanRotate )
{
	// разбираем путь
	CPtr<NAI::CPath> pSimplePath = new NAI::CPath;
	pSimplePath->pNet = pPath->pNet;
	pSimplePath->bStrafePath = pPath->bStrafePath;
	int nWaitAction = 0;
	bool bFirstPath = true;
	for ( int i = 0; i < pPath->points.size(); ++i )
	{
		NAI::SPathPlace p( pPath->points[i] );
		pSimplePath->points.push_back( p );
		if ( nWaitAction < pPath->actions.size() )
		{
			NAI::CPath::SPathAction &action = pPath->actions[nWaitAction];
			bool bActionNeeded = false;
			/*if ( !pPath->bStrafePath )
				bActionNeeded = action.where == p;
			else*/
				bActionNeeded = action.where.GetX() == p.GetX() && action.where.GetY() == p.GetY() 
				&& action.where.GetLayer() == p.GetLayer();
			int nBestDir;
			if ( !bActionNeeded )
				continue;

			CDynamicCast<IWindowDoor> pDoor( action.pObject );
			if ( CanSimplyOpen( pPath->pNet, action.where, pDoor, &nBestDir, action.action == NAI::PA_OPEN ) )
			{
				pSimplePath->actions.push_back( action );
				++nWaitAction;
				continue;
			}

			//  Добавляем дополнительную точку поворота, чтобы персонаж открывал дверь с правильной стороны
			if ( nBestDir != p.GetDirection() )
			{
				NAI::SPathPlace newPoint( p );
				newPoint.SetDirection( nBestDir );
				pSimplePath->points.push_back( newPoint );
				p = newPoint;
			}

			CDynamicCast<IObject> pObj( action.pObject );
			if ( !pObj )
			{
				ASSERT(0);
				return;
			}
			bool bOpen = ( action.action == NAI::PA_OPEN );
			CPtr<CCmdOpenClose> pOpen = new CCmdOpenClose( pObj, bOpen );

			// move
			if ( !bFirstPath || !pOldFront )
			{
				CCommandExecute *pMove = CreateSimpleMoveExecutor( pUS, pSimplePath, _eParams, ITEM_NO_MATTER, bCheckCanRotate );
				if ( pMove ) 
					execList.push_back( pMove );
				else
				{
					ASSERT(0);
					return;
				}
			}
			else
			{
				pOldFront->SetNewPath( pSimplePath, _eParams, ITEM_NO_MATTER );
				CDynamicCast<CCommandExecute> pPush( pOldFront );
				execList.push_back( pPush.GetPtr() );
			}
			bFirstPath = false;
	
			// check if door is trapped and set off it if player does not see danger
/*			if ( CDynamicCast<CWindowDoor> pDoor( action.pObject ) )
			{
				if ( pDoor->IsMineSet() && !pUS->GetTBSPlayer()->CanSeeTrap( pDoor ) )
					execList.push_back( new CExecBlowTrappedDoor( pUS, pDoor ) );
			}*/

			// open - close door
			CCommandExecute *pOpenClose = new CExecOpenClose( pUS, pOpen );
			execList.push_back( pOpenClose );
			// again
			pSimplePath = new NAI::CPath;
			pSimplePath->pNet = pPath->pNet;
			pSimplePath->bStrafePath = pPath->bStrafePath;
			pSimplePath->points.push_back( p );
			++nWaitAction;
		}
	}
	// last move
	if ( !bFirstPath || !pOldFront )
	{
		CCommandExecute *pMove = CreateSimpleMoveExecutor( pUS, pSimplePath, _eParams, eActive, bCheckCanRotate );
		if ( pMove ) 
			execList.push_back( pMove );
		else
			ASSERT(0);
	}
	else
	{
		pOldFront->SetNewPath( pSimplePath, _eParams, eActive );
		CDynamicCast<CCommandExecute> pPush( pOldFront );
		execList.push_back( pPush.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::CheckOpenCloseOnce()
{
	if ( execList.empty() )
		return;
	CCommandExecute *pExec = execList.back();
	CDynamicCast<CExecOpenClose> pOpen(pExec);
	if (pOpen)
	{
		bool bOpen; 
		IObject *pObj; 
		pOpen->GetParams( &bOpen, &pObj );
		list<CObj<CCommandExecute> >::iterator it;
		for ( it = execList.begin(); it != execList.end(); ++it )
		{
			CDynamicCast<CExecOpenClose> pOpenBefore( *it );
			if ( !pOpenBefore )
				continue;
			bool bOpenBefore;
			IObject *pObjectBefore; 
			pOpenBefore->GetParams( &bOpenBefore, &pObjectBefore );
			if ( bOpenBefore == bOpen && pObjectBefore == pObj )
			{
				++it;
				execList.erase( it, execList.end() );
				return;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecOpenClose
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecOpenClose::CExecOpenClose( CUnitServer *_pUS, CCmdOpenClose *_pCmd )
: CCommandExecute(_pUS), pCmd(_pCmd)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecOpenClose::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	// ... here we check some conditions about window/door
	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	ASSERT( pOS );
	if ( !pOS )
		return UCR_GENERAL_FAILURE;
	return UCR_OK; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecOpenClose::GetStartAP() const
{
	return pUS->GetActionAP( NRPG::AC_OPEN_CLOSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecOpenClose::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	pUS->DoAction( NRPG::AC_OPEN_CLOSE );
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), SKIPPABLE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExecOpenClose::TimeLabelReached()
{
	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	ASSERT( pOS );
	pOS->OpenClose( pCmd->bOpen, false, pUS );
	//
	NDb::CAISound *pAISound;
	if ( pCmd->bOpen )
		pAISound = NDb::GetAISound( 23 );
	else
		pAISound = NDb::GetAISound( 25 );
	pUS->GetWorld()->MakeAISound( pAISound, pUS, 0, 0 );
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecBlowTrappedDoor
////////////////////////////////////////////////////////////////////////////////////////////////////
/*CExecBlowTrappedDoor::CExecBlowTrappedDoor( CUnitServer *_pUS, CWindowDoor *_pTarget )
	: CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecBlowTrappedDoor::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecBlowTrappedDoor::GetStartAP() const
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecBlowTrappedDoor::Run()
{
	if ( IsValid(pTarget) )
		pTarget->GoBoom();
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x00122174, CExecQueue )
REGISTER_SAVELOAD_CLASS( 0x018c2161, CSimpleExecQueue )
//REGISTER_SAVELOAD_CLASS( 0x018c2160, CExecBlowTrappedDoor )
