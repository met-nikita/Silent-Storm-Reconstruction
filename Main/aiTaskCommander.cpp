#include "stdafx.h"

#include "wMain.h"
#include "wMainPath.h"
#include "wInterface.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"

#include "rpgUnitInfo.h"
#include "rpgCheatConstants.h"

#include "aiUnit.h"
#include "aiCommander.h"
#include "aiControl.h"
#include "aiRoute.h"
#include "aiMultiMoves.h"
#include "aiPosition.h"

#include "..\dbformat\datamap.h"
#include "..\dbformat\DataAnimation.h"

#include "MapBuild.h"
#include "BuildingInfo.h"

#include "aiTaskCommander.h"

namespace NAI
{
//////////////////////////////////////////////////////////////////////////////////////	
// CAITaskCommander
//////////////////////////////////////////////////////////////////////////////////////	
NWorld::CWorld *CAITaskCommander::GetWorld() const
{ 
	return pAICommander->GetWorld(); 
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAITaskCommander::RemoveTask( CTask *pTask )
{
	ASSERT( IsValid( pTask ) );
	//
	TasksToAddOrRemove.push_back( pTask );
	TasksAddFlag.push_back( false );
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAITaskCommander::AddTask( CTask *pTask )
{
	ASSERT( IsValid( pTask ) );
	ASSERT( !pTask->IsEmpty() );
	//
	TasksToAddOrRemove.push_back( pTask );
	TasksAddFlag.push_back( true );
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAITaskCommander::Segment()
{
	list< CObj<CTask> >::iterator i = TasksToAddOrRemove.begin();
	list<bool>::iterator b = TasksAddFlag.begin();
	for ( ; i != TasksToAddOrRemove.end(); ++i, ++b )
	{
		if ( *b && IsValid( *i ) )
			Tasks.push_back( *i );
		else
		{
			list< CObj<CTask> >::iterator k = find( Tasks.begin(), Tasks.end(), *i );
			ASSERT( k !=  Tasks.end() );
			if ( k != Tasks.end() )
				Tasks.erase( k );
		}
	}
	//
	TasksToAddOrRemove.clear();
	TasksAddFlag.clear();
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAITaskCommander::HasActiveTasks() const
{
	for ( list< CObj<CTask> >::const_iterator i = Tasks.begin(); i != Tasks.end(); ++i )
		if (  IsValid( *i ) && (*i)->IsActive() )
			return true;
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAITaskCommander::CanGetCommand() const
{
	if ( Tasks.empty() || !HasActiveTasks() )
		return false;
	//
	if ( GetWorld()->IsRealTime() )
		return true;
	//
	for ( list< CObj<CTask> >::const_iterator i = Tasks.begin(); i != Tasks.end(); ++i )
		if (  IsValid( *i ) && (*i)->IsActive() && 
			(*i)->GetUnitServer()->IsPerformingAction() )
				return false;
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////	
NWorld::CCommand* CAITaskCommander::GetCommand()
{
	if ( !CanGetCommand() )
		return 0;
	//
	for ( list< CObj<CTask> >::iterator i = Tasks.begin(); i != Tasks.end(); ++i )
	{
		if ( IsValid( *i ) && (*i)->IsActive() &&
			!(*i)->IsEndOfTask() && ( GetWorld()->IsRealTime() || !(*i)->IsEndOfTurn() ) )
		{
			NWorld::CCmd *pCmd = (*i)->GetCommand();
			if ( IsValid( pCmd ) )
			{
				NWorld::CCommand *pRes = new NWorld::CCmdSetCommand( (*i)->GetUnitServer(), pCmd );
				// ROUND-ROBIN (retail parity): retail serves AI units through SAIUnitsTracker with a
				// Next() rotation after every real-time serve (CAICommander::GenerateCommand @0x353d0
				// tail) -- no unit can monopolize its commander. The dev task list was scanned
				// front-first every segment, so one greedy task (a unit whose kept command re-pumps
				// CCmdContinue each segment) combined with the one-decision-per-segment latch STARVED
				// every later task of the same player -- the GFirst safe-run freeze (RB's fresh route
				// never pumped while RF1 spun). Rotate the served task to the back of the scan order.
				Tasks.splice( Tasks.end(), Tasks, i );
				return pRes;
			}
		}
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITaskCommander::IsUnderControl( IAIUnit *pAIUnit ) const
{
	ASSERT( IsValid( pAIUnit ) );
	//
	return IsUnderControl( pAIUnit->GetUnitServer() );
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAITaskCommander::OnTurnStarted()
{
	for ( list< CObj<CTask> >::iterator i = Tasks.begin(); i != Tasks.end(); ++i )
		(*i)->OnNewTurn();
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAITaskCommander::IsEndOfTurn() const
{
	for ( list< CObj<CTask> >::const_iterator i = Tasks.begin(); i != Tasks.end(); ++i )
	{
		if ( IsValid( *i ) && (*i)->IsActive() && !(*i)->IsEndOfTurn() )
			return false;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAITaskCommander::IsUnderControl( NWorld::CUnitServer *pUnitServer ) const
{
	ASSERT( IsValid( pUnitServer ) );
	//
	for ( list< CObj<CTask> >::const_iterator i = Tasks.begin(); i != Tasks.end(); ++i )
	{
		if ( (*i)->GetUnitServer() == pUnitServer )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////	
void CAITaskCommander::CreateRoute( NWorld::CUnitServer *pUnitServer, SMapUnit sMapUnit )
{
	ASSERT( IsValid( pUnitServer ) );
	//
	vector< CPtr<CTaskSyncObject> > syncs;
	CObj<CAIRoute> pRoute = new CAIRoute( pUnitServer->GetWorld(), sMapUnit.route );
	CPtr<CTask> pTask = pRoute->GetTask( pUnitServer, 0, syncs, true );
	if ( !IsValid( pTask ) || pTask->IsEmpty() )
	{
		pTask = new CTask( pUnitServer, true );
		if ( sMapUnit.eLogic == NDb::UL_EMPTY )
		{
			// ��������� �� ����� ����� ����� ������ ������ unit ����������� �������
			//pTask->AddCommand( new CTaskCommandGoto( pUnitServer->GetPosition().pos ) );
			pTask->AddCommand( new CTaskCommandCustomIdleAnimation( sMapUnit.pGuardAnimation ) );
			pTask->AddCommand( new CTaskCommandWait( 3000 ) );
		}
		else if ( sMapUnit.eLogic == NDb::UL_DEFAULT )
		{
			// Jan03 gave a routeless UL_DEFAULT unit a random look-around (AddLookAround: 3-6 x
			// Wait(1-6s) + ChangeDirection(random)). RETAIL DELETED IT: CAITaskCommander does not
			// exist in the release binary at all -- the retail deploy glue (NAI::CreateUnitRoute
			// @0x96d20) does NOTHING for a routeless default-logic unit, so map units without a
			// route stand still. The look-around also CANCELLED scripted animations (each
			// ChangeDirection is a command -- e.g. EFirst's lying wounded commander popped back to
			// the standing idle on his first random turn). A plain idle wait keeps the task valid.
			pTask->AddCommand( new CTaskCommandWait( 3000 ) );
		}
		else if ( sMapUnit.eLogic == NDb::UL_ROAMING )
			pTask->AddRoaming( pUnitServer->GetPosition().pos.p, sMapUnit.nRoamingRadius );
	}
	ASSERT( IsValid( pTask ) );
	if ( !IsValid( pTask ) )
		return;
	//
	pTask->DelayExecution( random.Get( 1, 20 ) );
	IAIControl *pAIControl = 
		CreateAITaskControl( pAICommander, pTask, AI_CONTROL_INTERRUPTABLE, AIM_AI );
	pAICommander->GetAIUnit( pUnitServer )->AssignControl( pAIControl );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITaskCommander::Synchronize()
{
	for ( list< CObj<CTask> >::iterator i = Tasks.begin(); i != Tasks.end(); ++i )
	{
		if ( !IsValid( *i ) )
			RemoveTask( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITaskCommander::RemoveUnit( NWorld::CUnitServer *pUS )
{
	for ( list< CObj<CTask> >::iterator i = Tasks.begin(); i != Tasks.end(); ++i )
	{
		if ( IsValid( *i ) && (*i)->GetUnitServer() == pUS )
			RemoveTask( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITaskCommander::OnUnitWasKilled( NWorld::CUnitServer *pUS )
{
	RemoveUnit( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTask
////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::CTask( NWorld::CUnitServer *_pUnitServer, bool _bCircled ): 
	pUnitServer(_pUnitServer),	bCircled(_bCircled), 
	nCurrentCommand(-1), tTime(0), bActive( true )
{	
	ASSERT( IsValid( pUnitServer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::OnPerformerDied()
{
	for ( vector< CObj<CTaskCommand> >::iterator i = Commands.begin(); i != Commands.end(); ++i )
		(*i)->OnPerformerDied();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::AddLookAround( bool bWalk )
{
	if ( bWalk )
		AddCommand( new CTaskCommandChangePose( NAI::WALK ) );
	int n = random.Get( 3, 6 );
	for ( int i = 0; i < n; ++i )
	{
		AddCommand( new CTaskCommandWait( random.Get( 1, 6 ) ) );
		AddCommand( new CTaskCommandChangeDirection( random.Get( 1, 8 ) - 1 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::AddRoaming(	const NAI::SPathPlace &_p, int _nAPRadius )
{
	AddCommand( new CTaskCommandRoaming( _p, _nAPRadius ) );
	AddLookAround( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::DebugOutput()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::AddCommand( CTaskCommand *pCmd )
{
	ASSERT( IsValid( pCmd ) );
	//
	pCmd->SetUnitServer( pUnitServer );
	Commands.push_back( pCmd );
	SetToBeginning();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::OnNewTurn()
{
	if ( nCurrentCommand >= 0 && nCurrentCommand < (int)Commands.size() )
		Commands[nCurrentCommand]->OnNewTurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTask::OnTaskStarted()
{
	for ( vector< CObj<CTaskCommand> >::iterator i = Commands.begin(); i != Commands.end(); ++i )
		(*i)->OnTaskStarted();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd *CTask::GetCommand()
{
	if ( !IsValid( pUnitServer ) )
		return 0;
	if ( !pUnitServer->GetWorld()->IsRealTime() )	tTime = 0;
	if ( pUnitServer && pUnitServer->GetWorld()->GetTime()->GetValue() < tTime )
		return 0;

	NWorld::CCmd *pCmd = 0;
	if ( !pUnitServer->IsPerformingAction() && !( nCurrentCommand >= ( int )Commands.size() ) ) 
	{
		if ( pUnitServer->HasCommand() && pUnitServer->HasEnoughAP() )
			return new NWorld::CCmdContinue();

		if ( nCurrentCommand == -1 || !pUnitServer->HasCommand() && Commands[nCurrentCommand]->IsEndOfCommand() )
		{
			if ( nCurrentCommand != -1 )
				Commands[ nCurrentCommand ]->OnCommandFinished();
			++nCurrentCommand;
			if ( bCircled && nCurrentCommand == (int)Commands.size() && Commands.size() > 1 )
				nCurrentCommand = 0;
		}
		// ���������� ��������� ��������
		if ( nCurrentCommand <= 0 )
			OnTaskStarted();
		//
		if ( nCurrentCommand > -1 && nCurrentCommand < (int)Commands.size() )
			pCmd = Commands[nCurrentCommand]->GetCommand();
	}
	//
	if ( IsEndOfTask() )
	{
		CDynamicCast<CAICommander> pAICommander( pUnitServer->GetPlayer()->GetCommander() );
		if ( IsValid( pAICommander ) )
			pAICommander->GetAIUnit( pUnitServer )->OnControlFinished();
	}
	//
	return pCmd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTask::IsEndOfTurn() const
{
	ASSERT( IsValid( pUnitServer ) );
	if ( !IsValid( pUnitServer ) )
		return true;
	//
	CPtr<NWorld::CWorld> pWorld = pUnitServer->GetWorld();
	if ( !pWorld->IsUnitActive( pUnitServer ) || !pUnitServer->CanFight() )
		return true;
	//
	if ( pWorld->GetTime()->GetValue() < tTime && !pWorld->IsRealTime() )
		return true;
	//
	if ( nCurrentCommand >= (int)Commands.size() )
		return true;
	//
	if ( !pUnitServer->IsPerformingAction() && pUnitServer->HasCommand() && !pUnitServer->HasEnoughAP() ||
		   nCurrentCommand >= 0 && Commands[nCurrentCommand]->IsEndOfUnitTurn() )
		return true;
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// // ���������� ���������� � �����������
void CTask::DelayExecution( int nTime ) 
{ 
	tTime = pUnitServer->GetWorld()->GetTime()->GetValue() + nTime*1000; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommand
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd *CTaskCommand::GetCommand()
{
	if ( Commands.empty() )
		Do();

	NWorld::CCmd *pRes = 0;
	if ( !Commands.empty() )
	{
		pRes = Commands.front().Extract();
		Commands.pop_front();
	}

	return pRes;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandGoto
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandGoto::Do()
{
	// retail @0x99280 (CRouteCommandGoto::Do): before walking to the target, an infantryman who is NOT already
	// running and is NOT inside a Panzerklein first issues a strafe command, so he side-steps to the position while
	// keeping his weapon trained on the threat (the defence cover-dance / round-up flank). A unit wearing a PK can't
	// strafe -- hence the GetWearingDBPK gate (the dev's own established idiom, cf. aiActionPlaceSource.cpp:343).
	if ( IsValid( pUnitServer ) &&
	     pUnitServer->GetPosition().GetPose() != NAI::RUN &&
	     !IsValid( pUnitServer->GetWearingDBPK() ) )
		DoCommand( new NWorld::CCmdStrafe( bStrafe ) );
	DoCommand( new NWorld::CCmdPath( ptPosition ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandRoaming
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandRoaming::Do()
{
	ASSERT( IsValid( pUnitServer ) );
	if ( IsValid( pUnitServer ) )
	{
		// get all possible places
		NAI::CMultiMovesTable movesTable;
		list<NAI::SPathPlace> places;
		CPtr<NWorld::CWorld> pWorld = pUnitServer->GetWorld();
		NWorld::PrepareAllPaths( pWorld->GetPathNetwork(), &movesTable, &places, pUnitServer, p, nAPRadius, pUnitServer, true );
		if ( !places.empty() )
		{
			// get random place for path command
			int n = random.Get( 0, places.size() );
			list<NAI::SPathPlace>::const_iterator i = places.begin();
			for ( ; n > 0; --n, ++i );
			// do path command
			NAI::SPosition pos;
			pos.p = *i;
			pos.SetNetwork( pWorld->GetPathNetwork() );
			pos.p.SetPose( NAI::CM_STAND );
			DoCommand( new NWorld::CCmdPath( pos ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangePose
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandChangePose::Do() 
{
	DoCommand( new NWorld::CCmdWishPose( nPose ) );
	SUnitPosition ptPosition = pUnitServer->GetPosition(); ptPosition.SetPose( nPose );
	DoCommand( new NWorld::CCmdPath( ptPosition.pos, PF_USE_POSE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandWait
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandWait::Do()
{
	if ( !bIsWaiting )
	{
		tTime = NWorld::pCurrentWorld->GetTime()->GetValue() + tLength*1000;
		nStartTurnID = NWorld::pCurrentWorld->GetTurnID();   // release OnCommandStarted @0x498db0: capture the start turn
		bNewTurnStarted = false;
		bIsWaiting = true;
	}

	if ( NWorld::pCurrentWorld->IsRealTime() && tTime < NWorld::pCurrentWorld->GetTime()->GetValue() )
		bIsWaiting = false;
	// TBS: complete when the turn id has advanced (release IsWaiting @0x498e50: waiting while GetTurnID()==nStartTurnID).
	// OR-ing bNewTurnStarted keeps the old CTask path's OnNewTurn() honoured; the turn-id test is what fixes the route
	// path (CAIRouteLogic never propagates OnNewTurn), so a Wait inside a route now completes on the next turn boundary.
	if ( !NWorld::pCurrentWorld->IsRealTime() &&
	     ( bNewTurnStarted || NWorld::pCurrentWorld->GetTurnID() != nStartTurnID ) )
		bIsWaiting = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandWait::IsEndOfCommand()
{
	return !bIsWaiting;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandWait::IsEndOfUnitTurn()
{
	// "still waiting on the start turn" (release IsWaiting @0x498e50 TBS branch: GetTurnID()==nTurnID). Keeps the flag
	// for the old CTask path while making the route path correct (it never sets bNewTurnStarted).
	return !bNewTurnStarted && NWorld::pCurrentWorld->GetTurnID() == nStartTurnID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandWait::OnNewTurn() 
{
	bNewTurnStarted = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeDirection
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandChangeDirection::Do()	
{
	SUnitPosition ptPosition = pUnitServer->GetPosition();
	ptPosition.pos.p.SetDirection( nDirection );
	DoCommand( new NWorld::CCmdPath( ptPosition.pos, PF_USE_DIR ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandCustomIdleAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandCustomIdleAnimation::Do()
{
	if( IsValid( pAnimation ) )
	{
		pUnitServer->animator.SetCustomIdleAnimation( pAnimation );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandSync
////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskCommandSync::CTaskCommandSync( CTaskSyncObject *_pSync ): 
	CTaskCommand( 0 ), pSync( _pSync ), bLocked( true )
{
	ASSERT( IsValid( pSync ) );
	if ( IsValid( pSync ) )
		pSync->Register( ( CTaskSyncObjectClient * )this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandSync::Do()
{
	if ( pUnitServer->CanFight() )
		pSync->Unlock( ( CTaskSyncObjectClient * )this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandSync::IsEndOfCommand()
{
	return !IsValid( pSync ) || !bLocked;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandSync::IsEndOfUnitTurn()
{
	return !IsEndOfCommand();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandSync::OnTaskStarted()
{
	bLocked = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandSync::OnPerformerDied()
{
	if ( IsValid( pSync ) )
		pSync->UnRegister( ( CTaskSyncObjectClient * )this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandSync::OnUnlock()
{
	bLocked = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskSyncObject
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskSyncObject::Unlock( CTaskSyncObjectClient *pClient )
{
	ASSERT( IsValid( pClient ) );
	if ( !IsValid( pClient ) )
		return;
	//
	if ( find( unlockedClients.begin(), unlockedClients.end(), pClient ) != unlockedClients.end() )
		return;
	//
	unlockedClients.push_back( pClient );
	if ( unlockedClients.size() >= clients.size() )
	{
		vector< CPtr<CTaskSyncObjectClient> >::const_iterator i;
		for ( i = clients.begin(); i != clients.end(); ++i )
			(*i)->OnUnlock();
		unlockedClients.clear();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskSyncObject::Register( CTaskSyncObjectClient *pClient )
{
	ASSERT( IsValid( pClient ) );
	ASSERT( find( clients.begin(), clients.end(), pClient ) == clients.end() );
	if ( IsValid( pClient ) && find( clients.begin(), clients.end(), pClient ) == clients.end() )
		clients.push_back( pClient );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskSyncObject::UnRegister( CTaskSyncObjectClient *pClient )
{	
	ASSERT( IsValid( pClient ) );
	clients.erase( remove( clients.begin(), clients.end(), pClient ), clients.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51222140, CAITaskCommander )
REGISTER_SAVELOAD_CLASS( 0x51812130, CTask )
REGISTER_SAVELOAD_CLASS( 0x51812131, CTaskCommandGoto )
REGISTER_SAVELOAD_CLASS( 0x51222141, CTaskCommandChangePose )
REGISTER_SAVELOAD_CLASS( 0x51222142, CTaskCommandChangeDirection )
REGISTER_SAVELOAD_CLASS( 0x51222143, CTaskCommandWait )
REGISTER_SAVELOAD_CLASS( 0x52302190, CTaskCommandSync )
REGISTER_SAVELOAD_CLASS( 0x52302191, CTaskSyncObject )
REGISTER_SAVELOAD_CLASS( 0x52122140, CTaskCommandRoaming )
REGISTER_SAVELOAD_CLASS( 0x52122141, CTaskCommandCustomIdleAnimation )