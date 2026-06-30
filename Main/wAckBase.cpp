#include "stdafx.h"
#include "wAckBase.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataAck.h"
#include "wInterface.h"
#include "wUnitServer.h"
#include "wAck.h"
#include "RPGUnitMission.h"
#include "wMain.h"
#include "rpgCheatConstants.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalAck
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DEFINE_UNITSERVER_EVENT_HANDLER( HandlerName )                               \
void CGlobalAck::HandlerName( CUnitServer *pUnit )                                   \
{                                                                                    \
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )      \
		(*i)->HandlerName( pUnit );                                                      \
}                                                                                    \
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DEFINE_EVENT_HANDLER( HandlerName )                                          \
void CGlobalAck::HandlerName()                                                       \
{                                                                                    \
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )      \
		(*i)->HandlerName();                                                             \
}                                                                                    \
////////////////////////////////////////////////////////////////////////////////////////////////////
DEFINE_EVENT_HANDLER( OnSegment );
DEFINE_EVENT_HANDLER( OnRealTimeStarted );
DEFINE_UNITSERVER_EVENT_HANDLER( OnLastPieceOfAmmo );
DEFINE_UNITSERVER_EVENT_HANDLER( OnWeaponJammed );
DEFINE_UNITSERVER_EVENT_HANDLER( OnOrderConfirmation );
DEFINE_UNITSERVER_EVENT_HANDLER( OnImpossibleToPerformAction );
DEFINE_UNITSERVER_EVENT_HANDLER( OnTargetHit );
DEFINE_UNITSERVER_EVENT_HANDLER( OnHardTargetHit );
DEFINE_UNITSERVER_EVENT_HANDLER( OnTargetMissed );
DEFINE_UNITSERVER_EVENT_HANDLER( OnSuffersLightDamage );
DEFINE_UNITSERVER_EVENT_HANDLER( OnSuffersHardDamage );
DEFINE_UNITSERVER_EVENT_HANDLER( OnInterrupt );
DEFINE_UNITSERVER_EVENT_HANDLER( OnSkillIncreased );
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnNewTurnStarted( IPlayer *pPlayer )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnNewTurnStarted( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnCannotFinishHeal( CUnitServer *pHealer, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnCannotFinishHeal( pHealer, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnHealFinished( CUnitServer *pHealer, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnHealFinished( pHealer, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnDoCriticalDamage( pAttacker, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnUnitWasKilled( pAttacker, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnDoDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnDoDamage( pAttacker, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnDoAccidentalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnDoAccidentalDamage( pAttacker, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnEnemyBecomesVisible( CUnitServer *pWatcher, 
	CUnitServer *pTarget, bool bRealTime )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnEnemyBecomesVisible( pWatcher, pTarget, bRealTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnGrenadeExplosion( CUnitServer *pUnit,
																		int nUnitsDestroyed, int nObjectsDestroyed )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnGrenadeExplosion( pUnit, nUnitsDestroyed, nObjectsDestroyed );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::OnUnitDied( CUnitServer *pUnit )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
		(*i)->OnUnitDied( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::RemoveUnitAcks( CUnitServer *pUnit )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); )
	{
		CDynamicCast<CAckBase> pAckBase( *i );
		CUnitServer *xpUnit = pAckBase->GetUnit();
		if ( pAckBase->GetUnit() == pUnit )
			i = vAck.erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::AddAckSequence( CUnitServer *pUnit, NDb::CDBAck *pAck )
{
	// Retail @0x339420: guard BOTH the unit (non-null + not the CObjectBase zombie bit) and the ack,
	// then queue the (unit, ack) pair. The CPtr/CDBPtr copies AddRef both (engine net +1 each); the
	// release silently skips an invalid unit/ack (no ASSERT) -- a torn-down speaker just never barks.
	if ( IsValid( pUnit ) && IsValid( pAck ) )
		sequences.push_back( SAck( pUnit, pAck ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::AddAck( CUnitServer *pUnit )
{
	CDBTable<NDb::CDBAck> *pDBTable = NDatabase::GetTable<NDb::CDBAck>();
	CDBIterator<NDb::CDBAck> i(*pDBTable);
	while ( pDBTable && i.MoveNext() )
	{
		NDb::CDBAck *pDBAck = i.Get();
		if ( pDBAck && pDBAck->nRPGPersID == pUnit->GetUnitRPG()->GetRPGPersID() )
		{
			CAckBase *pAck = CreateAck( pUnit, pDBAck );
			if ( pAck )
				vAck.push_back( pAck );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalAck::IsContainUnit( const list< CPtr<CUnit> > &visibleUnits, int nRPGPersID )
{
	if ( !visibleUnits.empty() )
	{
		for ( list< CPtr<CUnit> >::const_iterator i = visibleUnits.begin(); i != visibleUnits.end(); ++i)
		{
			CDynamicCast<CUnitServer> pUnit(*i);
			if ( pUnit && pUnit->GetUnitRPG()->GetRPGPersID() == nRPGPersID )
				return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalAck::IsSequenceVisible( const list< CPtr<CUnit> > &visibleUnits, NDb::CDBAckSequence *pSequence )
{
	for ( int i = 0; i < NDb::N_ACKINFO_MAX_COUNT; ++i )
		if ( pSequence->pDBAckInfo[i] )
		{			
			int &n = pSequence->pDBAckInfo[i]->nRPGPersID;
			if ( !IsContainUnit( visibleUnits, n ) )
				return false;
		}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::RemoveInvisibleSequences( IPlayer *pPlayer )
{
	// ������� ��� ���� ��������� ������ pPlayer
	if ( !sequences.empty() )
	{
		list< CPtr<CUnit> > visible;
		pPlayer->GetVisible( &visible );
	
		for ( list< SAck >::iterator i = sequences.begin(); i != sequences.end(); )	
		{
			if ( !IsSequenceVisible( visible, i->pAck->pAckSequence ) )
				i = sequences.erase( i );
			else 
				++i;
		}		
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::FetchHighestAcks()
{
	// ���� ������������ ���������
	int nMaxPriority = 0;
	list< SAck >::iterator i;
	for ( i = sequences.begin(); i != sequences.end(); ++i )
		if ( i->pAck->pAckSequence->nPriority > nMaxPriority )
			nMaxPriority = i->pAck->pAckSequence->nPriority;
	// ������� ��� ���� � ������� �����������
	for ( i = sequences.begin(); i != sequences.end(); )
		if ( i->pAck->pAckSequence->nPriority < nMaxPriority )
			i = sequences.erase( i );
		else
			++i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CDBAckSequence *CGlobalAck::GetSequence( IPlayer *pPlayer )
{
	NDb::CDBAckSequence *pRes = 0;
	//RemoveInvisibleSequences( pPlayer );
	FetchHighestAcks();
	if ( !sequences.empty() )
	{
		// �������� ���� �� Ack-��
		float fProb = 0;
		CRoulette roulette;
		for ( list< SAck >::iterator i = sequences.begin(); i != sequences.end(); ++i )
		{
			fProb += i->pAck->fProbability;
			roulette.AddSector( i->pAck->fProbability );
		}
		//
		fProb = Max( 0.f, 100 - fProb );
		if ( fProb > 0 )
			roulette.AddSector( fProb );
		//
		int nSector = roulette.GetRandomSector( &SRand() );
		if ( nSector < sequences.size() )
		{
			list< SAck >::iterator i = sequences.begin();
			for ( int k = 0; i != sequences.end() && k < nSector; ++i, ++k );
			pRes = i->pAck->pAckSequence;
		}
		// �������
		sequences.clear();
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::SayAck( CUnitServer *pWho, int nConditionID )
{
	CDBTable<NDb::CDBAck> *pDBTable = NDatabase::GetTable<NDb::CDBAck>();
	CDBIterator<NDb::CDBAck> i(*pDBTable);
	while ( pDBTable && i.MoveNext() )
	{
		NDb::CDBAck *pDBAck = i.Get();
		if ( pDBAck && pDBAck->nRPGPersID == pWho->GetUnitRPG()->GetRPGPersID() && 
				 pDBAck->nConditionID == nConditionID )
		{
			AddAckSequence( pWho, pDBAck );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckBase
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckBase::CAckBase( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): 
	pUnit(_pUnit), pDBAck(_pDBAck) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer *CAckBase::GetUnit()
{ 
	return	pUnit; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CDBAck *CAckBase::GetDBAck() 
{
	return pDBAck; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWorld *CAckBase::GetWorld() 
{ 
	return GetUnit()->GetWorld(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckBase::PlayAck()
{
	// CRASH FIX / Tier-B reconcile toward release @0x339640. This was the Jan03 PREDECESSOR form: it
	// deref'd pUnit->IsCheatEnabled (-> GetUnitRPG()->GetRPGUnit()) with NO liveness guard, so a missed-
	// shot ack fired for a zombie / torn-down unit (whose RPG mission is already gone) crashed on a null
	// GetRPGUnit(). The release guards pUnit (non-null + not the CObjectBase zombie bit) and only barks
	// for a unit that CanFight() before queuing on the global ack. The AddAckSequence overload now
	// carries the speaking unit (retail @0x339640 -> @0x339420 SAck path: sequences stores (unit,ack)
	// pairs). The gate stays the dev's per-unit IsCheatEnabled(CHEAT_SCRIPTSEQUENCE): the release's
	// world-level CWorld::IsSequence has no equivalent in this snapshot (documented aiCommander.h:85 /
	// wTurnBased.h:427), and the per-unit cheat flag is the subsystem-wide behaviour-equivalent.
	if ( !IsValid( pUnit ) || !pUnit->CanFight() )
		return;
	if ( !pUnit->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) )
		GetWorld()->GetGlobalAck()->AddAckSequence( GetUnit(), GetDBAck() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0x52412170, CGlobalAck );