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
			// per-step start hook (retail CRouteCommand vtbl+0x10, fired by the route walker at its advance --
			// GenerateCommand @0x9a170; mirrored here so CTaskCommandWait::OnCommandStarted captures its
			// tTime/nTurnID when the wait becomes current on the legacy CTask path too)
			if ( nCurrentCommand > -1 && nCurrentCommand < (int)Commands.size() )
				Commands[ nCurrentCommand ]->OnCommandStarted();
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
	// retail @0x99280 (CRouteCommandGoto::Do): an INVALID unit server queues NOTHING (the whole body is inside
	// the IsValid gate). Before walking to the target, an infantryman who is NOT already running and is NOT
	// inside a Panzerklein first issues a strafe command, so he side-steps to the position while keeping his
	// weapon trained on the threat (the defence cover-dance / round-up flank). A unit wearing a PK can't
	// strafe -- hence the GetWearingDBPK gate (the dev's own established idiom, cf. aiActionPlaceSource.cpp:343).
	if ( !IsValid( pUnitServer ) )
		return;
	if ( pUnitServer->GetPosition().GetPose() != NAI::RUN &&
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
// retail @0x994c0 (CRouteCommandChangePose::Do): invalid unit -> nothing. A panzerklein wearer clamps the
// pose to WALK (pose > 1, PERSISTED into the member -- a PK never runs). Queue the wish pose; then, when the
// current place is real (pose bits 30-31 not both set), re-pose in place via a PF_USE_POSEDIR path (retail
// ctor args: bLeaveZone=false, PF_USE_POSEDIR, ITEM_NO_MATTER, bCanFindNotExact=false = dev ctor defaults).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandChangePose::Do()
{
	if ( !IsValid( pUnitServer ) )
		return;
	if ( IsValid( pUnitServer->GetWearingDBPK() ) && (int)nPose > 1 )
		nPose = WALK;
	DoCommand( new NWorld::CCmdWishPose( nPose ) );
	SUnitPosition ptPosition = pUnitServer->GetPosition();
	if ( ( (unsigned)ptPosition.pos.p.GetData() & 0xc0000000u ) != 0xc0000000u )   // a real (non-sentinel) place
	{
		ptPosition.SetPose( nPose );
		DoCommand( new NWorld::CCmdPath( ptPosition.pos, PF_USE_POSEDIR, NWorld::ITEM_NO_MATTER ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeWishPose
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x99600 (CRouteCommandChangeWishPose::Do): invalid unit -> nothing. The same PK WALK-clamp as
// ChangePose (persisted). Queue the wish pose; then ONLY for a panzerklein wearer (the second GetWearingDBPK
// probe) queue the in-place PF_USE_POSEDIR re-path -- a PK ignores the bare wish pose, everyone else adopts
// it on the next move (no re-path, unlike ChangePose).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandChangeWishPose::Do()
{
	if ( !IsValid( pUnitServer ) )
		return;
	if ( IsValid( pUnitServer->GetWearingDBPK() ) && (int)nPose > 1 )
		nPose = WALK;
	DoCommand( new NWorld::CCmdWishPose( nPose ) );
	if ( IsValid( pUnitServer->GetWearingDBPK() ) )
	{
		SUnitPosition ptPosition = pUnitServer->GetPosition();
		ptPosition.SetPose( nPose );
		DoCommand( new NWorld::CCmdPath( ptPosition.pos, PF_USE_POSEDIR, NWorld::ITEM_NO_MATTER ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandWait -- retail CRouteCommandWait has NO Do (the base empty Do runs; the wait works purely
// through IsEndOfCommand gating) and NO OnNewTurn. The lifecycle is: OnCommandStarted captures the
// deadline/turn, IsWaiting computes the live state from the captured pair (so a mid-wait SAVE resumes
// exactly where it stopped -- the pair is the wire state, tags 3/4/5).
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x98db0: tTime = world-now + tLength*1000; nTurnID = world turn id (both read through the unit
// server's world, not the global current world).
void CTaskCommandWait::OnCommandStarted()
{
	NWorld::CWorld *pWorld = pUnitServer->GetWorld();
	tTime = pWorld->GetTime()->GetValue() + tLength*1000;
	nTurnID = pWorld->GetTurnID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x98e50: a sentinel unit position (pose bits 30-31 both set) is never waiting; otherwise
// waiting == (TBS: still the captured turn) OR (real-time: the deadline is ahead).
bool CTaskCommandWait::IsWaiting()
{
	NWorld::CWorld *pWorld = pUnitServer->GetWorld();
	SUnitPosition pos = pUnitServer->GetPosition();
	if ( ( (unsigned)pos.pos.p.GetData() & 0xc0000000u ) == 0xc0000000u )
		return false;
	bool bTurnWait = !pWorld->IsRealTime() && pWorld->GetTurnID() == nTurnID;
	bool bTimeWait = pWorld->IsRealTime() && pWorld->GetTime()->GetValue() < tTime;
	return bTurnWait || bTimeWait;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandWait::IsEndOfCommand()
{
	return !IsWaiting();   // retail @0x98f60
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandWait::IsEndOfUnitTurn()
{
	return IsWaiting();    // retail @0x98f70 (same body as IsWaiting)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandChangeDirection
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x99740 (CRouteCommandLook::Do): invalid unit -> nothing; sentinel place (pose bits 30-31 both
// set) -> nothing. Otherwise fold the stored direction into the CURRENT place and queue a CCmdLook -- an
// in-place turn command, NOT a PF_USE_DIR path (the dev predecessor's shape).
void CTaskCommandChangeDirection::Do()
{
	if ( !IsValid( pUnitServer ) )
		return;
	SUnitPosition ptPosition = pUnitServer->GetPosition();
	if ( ( (unsigned)ptPosition.pos.p.GetData() & 0xc0000000u ) != 0xc0000000u )
	{
		ptPosition.pos.p.SetDirection( (unsigned short)nDirection );
		DoCommand( new NWorld::CCmdLook( ptPosition.pos ) );
	}
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
	CTaskCommand( 0 ), pSync( _pSync ), bLocked( true ),
	regOnDie( this, &CTaskCommandSync::OnUnitDiedOrLoseConsciousness )   // retail ctor @0x9a310 wires regEvent
{
	ASSERT( IsValid( pSync ) );
	if ( IsValid( pSync ) )
		pSync->Register( ( CTaskSyncObjectClient * )this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x99d30: vote "ready" UNCONDITIONALLY when the step becomes current (the dev predecessor gated on
// CanFight -- retail instead drops a dead/unconscious performer from the barrier via the event below, so a
// downed unit can never deadlock the group).
void CTaskCommandSync::Do()
{
	pSync->Unlock( ( CTaskSyncObjectClient * )this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandSync::IsEndOfCommand()
{
	return !IsValid( pSync ) || !bLocked;   // retail @0x98cf0: end iff the sync is gone or unlocked
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskCommandSync::IsEndOfUnitTurn()
{
	return !IsEndOfCommand();   // retail @0x988e0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskCommandSync::OnCommandFinished()
{
	bLocked = true;   // retail @0x9d950: re-arm for the next circled pass (the dev predecessor re-armed in OnTaskStarted)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x99cf0: when THIS step's performer dies or loses consciousness, drop it from the sync barrier
// (CRouteSyncObject::UnRegister) so the remaining group members are not deadlocked waiting on it.
void CTaskCommandSync::OnUnitDiedOrLoseConsciousness( const NWorld::CEventOnUnitDiedOrLoseConsciousness &event )
{
	if ( event.pWho == pUnitServer )
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
// retail v1.2 id map (gen/classreg.json): 0x51812131=CRouteCommandGoto, 0x51222141=CRouteCommandChangePose,
// 0x51222142=CRouteCommandLook, 0x51222143=CRouteCommandWait, 0x2305EC00=CRouteCommandChangeWishPose,
// 0x52302190=CRouteCommandSync, 0x52302191=CRouteSyncObject, 0x52122140=CRouteCommandRoaming,
// 0x52122141=CRouteCommandCustomIdleAnimation. 0x51812130 (CTask) is dev-only -- retail registers nothing there.
REGISTER_SAVELOAD_CLASS( 0x51812130, CTask )
REGISTER_SAVELOAD_CLASS( 0x51812131, CTaskCommandGoto )
REGISTER_SAVELOAD_CLASS( 0x51222141, CTaskCommandChangePose )
REGISTER_SAVELOAD_CLASS( 0x2305EC00, CTaskCommandChangeWishPose )
REGISTER_SAVELOAD_CLASS( 0x51222142, CTaskCommandChangeDirection )
REGISTER_SAVELOAD_CLASS( 0x51222143, CTaskCommandWait )
REGISTER_SAVELOAD_CLASS( 0x52302190, CTaskCommandSync )
REGISTER_SAVELOAD_CLASS( 0x52302191, CTaskSyncObject )
REGISTER_SAVELOAD_CLASS( 0x52122140, CTaskCommandRoaming )
REGISTER_SAVELOAD_CLASS( 0x52122141, CTaskCommandCustomIdleAnimation )
