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

#include "aiTaskCommand.h"

namespace NAI
{
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
		// return the next command
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
// // execution starts with a delay
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
REGISTER_SAVELOAD_CLASS( 0x51812130, CTask )
REGISTER_SAVELOAD_CLASS( 0x51812131, CTaskCommandGoto )
REGISTER_SAVELOAD_CLASS( 0x51222141, CTaskCommandChangePose )
REGISTER_SAVELOAD_CLASS( 0x51222142, CTaskCommandChangeDirection )
REGISTER_SAVELOAD_CLASS( 0x51222143, CTaskCommandWait )
REGISTER_SAVELOAD_CLASS( 0x52302190, CTaskCommandSync )
REGISTER_SAVELOAD_CLASS( 0x52302191, CTaskSyncObject )
REGISTER_SAVELOAD_CLASS( 0x52122140, CTaskCommandRoaming )
REGISTER_SAVELOAD_CLASS( 0x52122141, CTaskCommandCustomIdleAnimation )
