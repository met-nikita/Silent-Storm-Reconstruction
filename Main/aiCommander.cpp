#include "StdAfx.h"
//
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitSounds.h"
#include "wOSBase.h"
#include "wTurnBased.h"
#include "wUnitCommands.h"
//
#include "aiUnit.h"
#include "aiSignal.h"
#include "aiTaskCommander.h"
#include "aiTacticalCommander.h"
#include "aiControl.h"
#include "scriptCallLUA.h"		// NScript::luaCallFunction (OnStartTurn)
//
#include "RPGUnitInfo.h"
#include "RPGItemSet.h"
#include "rpgDiplomacy.h"
#include "rpgCheatConstants.h"
//
#include "aiCommander.h"
//
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataConst.h"
#include "..\DBFormat\DataMap.h"
#include "aiWeapon.h"
#include "aiInventory.h"
//
bool bForbidAI = false;
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICommander
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICommander::CAICommander( NWorld::CWorld *_pWorld, NWorld::CPlayer *_pPlayer ): 
	pTaskCommander(0), pPlayer(_pPlayer), pWorld( _pWorld ), bAITurn( false ), bWantTurnBased( false )
{
	pTaskCommander = new CAITaskCommander( this );
	pTacticalCommander = CreateAITacticalCommander( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnUnitAdded( NWorld::CUnitServer *pUnitServer )
{
	ASSERT( IsValid( pUnitServer ) );
	units.push_back( CreateAIUnit( pUnitServer, pUnitServer->GetPlayer() == pPlayer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::GenerateCommand()
{
	// �������� �� ����� ����
	if ( bForbidAI || IsEndOfTurn() && !pWorld->IsRealTime() )
	{
		Do( new NWorld::CCmdEndOfTurn() );
		return;
	}
	// ���� ����������� �����-���� ��������, �� ������ �� �����
	if ( !pWorld->IsRealTime() && pWorld->IsAction() )
		return;
	// �������� ��������� �������
	CObj<NWorld::CCommand> pCommand = pTacticalCommander->GetCommand();
	// ��������� ������ �������
	if ( !IsValid( pCommand ) )
		pCommand = pTaskCommander->GetCommand();
	// �������� ������� �� ����������
	CDynamicCast<NWorld::CCmdUnit> pCmdUnit(pCommand);
	if (pCmdUnit)
	{
		CDynamicCast<NWorld::CUnitServer> pUS(pCmdUnit->pUnit);
		if (pUS)
		{
			if ( pWorld->IsUnitActive( pUS ) && pUS->CanFight() )
			{
				Do( pCmdUnit );
				Do( new NWorld::CCmdSetCommand( pCmdUnit->pUnit, new NWorld::CCmdContinue() ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsEndOfTurn()
{
	bool bRes;
	//
	if ( pWorld->IsRealTime() )
	{
		bRes = false;
	}
		else
	{
		bRes = true;
		//
		bRes &= pTaskCommander->IsEndOfTurn();
		//
		bRes &= pTacticalCommander->IsEndOfTurn();
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTurnStarted()
{
	UnLockAllObjects();
	pTacticalCommander->OnTurnStarted();
	pTaskCommander->OnTurnStarted();
	bAITurn = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTurnFinished()
{
	bAITurn = false;
	pTacticalCommander->OnTurnFinished();
	pTaskCommander->OnTurnFinished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnCancelAction()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::Synchronize()
{
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( !IsValid( (*i)->GetUnitServer() ) )
			i = units.erase( i );
		else
		{
			if ( (*i)->GetUnitServer()->CanFight() )
				(*i)->Synchronize();
			++i;
		}
	}
	//
	pTacticalCommander->Synchronize();
	pTaskCommander->Synchronize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnPassControl( NWorld::CPlayer *_pPlayer )
{
	if ( _pPlayer == pPlayer  )
		bAITurn = true;
	else
		bAITurn = false;
	//
	if ( bAITurn )
	{
		Synchronize();
		// retail CAICommander::OnPassControl (aiCommander.c:1612): when this commander's player receives control,
		// fire the per-turn lua hook OnStartTurn( scenarioPlayerID ) for the player now taking the turn.
		if ( IsValid( pPlayer ) )
			NScript::luaCallFunction( "OnStartTurn", "i", pPlayer->GetScenarioPlayerID() );
	}
	//
	pTacticalCommander->OnPassControl( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::Segment()
{
	if ( bForbidAI )
		return;
	// release CAICommander::OnAISegment @0x346f0: tick EVERY commanded unit each segment so every unit lazily builds
	// and keeps alive its CAIEventTracker subscription before any world event fires (not just the unit being thought).
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		(*i)->OnAISegment();
	//
	if ( bWantTurnBased )
	{
		if ( !pWorld->IsRealTime() )
			bWantTurnBased = false;
		else
			pWorld->WantTurnBased( pPlayer );
	}
	//
	pTacticalCommander->Segment();
	pTaskCommander->Segment();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnUnitDied( NWorld::CUnitServer *pUnit )
{
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetUnitServer() == pUnit )
			(*i)->OnDied();
	}
	//
	if ( bForbidAI )
		return;
	//
	pTaskCommander->OnUnitWasKilled( pUnit );
	pTacticalCommander->OnUnitWasKilled( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::RemoveUnit( NWorld::CUnitServer *pUS )
{
	pTaskCommander->RemoveUnit( pUS );
	pTacticalCommander->RemoveUnit( pUS );
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( (*i)->GetUnitServer() == pUS )
			i = units.erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnTBSEvent( NWorld::ETBSEvent event )
{
	if ( bForbidAI )
		return;
	//
	switch ( event )
	{
		case NWorld::TBS_START_NEW_TURN:
			OnTurnStarted();
			break;			
		case NWorld::TBS_FINISH_OWN_TURN:
			OnTurnFinished();
			break;
		case NWorld::TBS_CANCEL_ACTION:
			OnCancelAction();
			break;
		case NWorld::TBS_START_REAL_TIME:
			OnStartRealTime();
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnSeeUnit( NWorld::CUnitServer *pWatcher, NWorld::CUnitServer *pTarget ) 
{
	if ( bForbidAI )
		return;
	//
	if ( pWatcher->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) )
		return;
	//
	if ( GetWorld()->GetDiplomacyState( pWatcher, pTarget->GetPlayer() ) != NDb::DS_ENEMY )
		return;
	//
	if ( pTarget->CanFight() && pWatcher->CanFight() )
		bWantTurnBased = true;
	//
	pTacticalCommander->OnSeeUnit( pWatcher, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::LockObject( NWorld::CObjectServerBase *pObject )
{
	if ( !IsObjectLocked( pObject ) )
		LockedObjects.push_back( pObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::UnLockObject( NWorld::CObjectServerBase *pObject )
{
	list< CPtr<NWorld::CObjectServerBase> >::iterator i = 
		find( LockedObjects.begin(), LockedObjects.end(), pObject );
	if ( i != LockedObjects.end() )
		LockedObjects.erase( i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::IsObjectLocked( NWorld::CObjectServerBase *pObject )
{
	return find( LockedObjects.begin(), LockedObjects.end(), pObject ) != LockedObjects.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::UnLockAllObjects()
{
	LockedObjects.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::ProcessAISignals()
{
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetUnitServer()->CanFight() )
		{
			CPtr<IAISignal> pAISignal = pWorld->GetAISignalManager()->Get( *i );
			if ( IsValid( pAISignal ) )
				pAISignal->Process( *i );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *CAICommander::GetAIUnit( NWorld::CUnitServer *pUnit )
{
	for ( list< CObj<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetUnitServer() == pUnit )
			return *i;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICommander::HasVisibleEnemies()
{
	list< CPtr<NWorld::CUnit> > visibleUnits;
	pPlayer->GetVisible( &visibleUnits );
	for ( list< CPtr<NWorld::CUnit> >::const_iterator i = visibleUnits.begin(); i != visibleUnits.end(); ++i )
	{
		NWorld::IPlayer *pEnemyPlayer = (*i)->GetPlayer();
		bool bDiplomacyEnemy = GetWorld()->GetDiplomacyState( pPlayer, pEnemyPlayer ) == NDb::DS_ENEMY;
		if ( pEnemyPlayer != pPlayer && bDiplomacyEnemy )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::OnStartRealTime()
{
	pTacticalCommander->OnStartRealTime();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICommander::WantTurnBased()
{
	GetWorld()->WantTurnBased( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander @0x35b60 -- see aiCommander.h. Release-NEW; parity-only (nothing instantiates it in
// this snapshot). The ctor chains CAICommander( world, 0 ): a sequence commander owns no AI player.
////////////////////////////////////////////////////////////////////////////////////////////////////
CSequenceCommander::CSequenceCommander( NWorld::CWorld *_pWorld ): CAICommander( _pWorld, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequenceCommander::GenerateCommand @0x358d0 -- the release auto-drives the human commander only while
// the world runs a cinematic sequence ( if ( pWorld->IsSequence() ) CAICommander::GenerateCommand(); ).
// This dev snapshot has no world-level sequence predicate (wTurnBased.h:427), so the gate cannot be
// reconstructed against a real method; the override is a documented no-op here (a human commander never
// auto-drives) -- the release-direction-preserving choice. The distinct vtable slot is preserved.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSequenceCommander::GenerateCommand()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(aiCommander)
	REGISTER_VAR_EX( "game_noai", NGlobal::VarBoolHandler, &bForbidAI, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x02731170, CAICommander )
REGISTER_SAVELOAD_CLASS( 0x51823190, CSequenceCommander )
