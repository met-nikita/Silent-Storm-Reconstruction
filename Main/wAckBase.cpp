#include "stdafx.h"
#include "wAckBase.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataMap.h"	// NDb::EDiplomacyState / DS_ENEMY (CAckBase::IsEnemy gate)
#include "wInterface.h"
#include "wUnitServer.h"
#include "wAck.h"
#include "RPGUnitMission.h"
#include "RPGUnit.h"       // NRPG::CUnit::GetAckPersID (hero voice-donor pers)
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
// retail CGlobalAck::OnNoPlaceInInventory @0x338c30: a plain vAck broadcast (GlobalGame slot +0x5c).
DEFINE_UNITSERVER_EVENT_HANDLER( OnNoPlaceInInventory );
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
bool CGlobalAck::HasAcksFor( CUnitServer *pUnit )
{
	for ( vector< CObj<IAck> >::iterator i = vAck.begin(); i != vAck.end(); ++i )
	{
		CDynamicCast<CAckBase> pAckBase( *i );
		if ( pAckBase && pAckBase->GetUnit() == pUnit )
			return true;
	}
	return false;
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
	// retail GetAckHolder @0x2ba8f0: hero -> side defaultPersesSet donor matched by voice;
	// non-hero -> pPers->pAcksHolder ("AckUnit"); null holder -> own pers (CUnit::GetAckPersID)
	const int nAckPersID = pUnit->GetUnitRPG()->GetRPGUnit()->GetAckPersID();
	int nRows = 0, nListeners = 0;
	while ( pDBTable && i.MoveNext() )
	{
		NDb::CDBAck *pDBAck = i.Get();
		if ( pDBAck && pDBAck->nRPGPersID == nAckPersID )
		{
			++nRows;
			CAckBase *pAck = CreateAck( pUnit, pDBAck );
			if ( pAck )
			{
				vAck.push_back( pAck );
				++nListeners;
			}
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
	// remove all acks not visible to player pPlayer
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
// retail NWorld::GetAckPriority @0x338e40 (oracle s2_ackutil.h:43): the sequence's priority when
// it is > 0, else the CONDITION record's priority, else 0. CRITICAL with the Steam game.db: EVERY
// AckSeqs row carries Priority 0, so the retail priorities (death=10, order-confirm=1, ...) come
// ENTIRELY from the AckConditions fallback -- without it the whole competition ties at 0.
static int GetAckPriority( NDb::CDBAck *pAck )
{
	if ( !IsValid( pAck ) )
		return 0;
	NDb::CDBAckSequence *pSeq = pAck->pAckSequence;
	if ( !IsValid( pSeq ) )
		return 0;
	if ( pSeq->nPriority > 0 )
		return pSeq->nPriority;
	NDb::CDBAckCondition *pCond = pAck->pCondition;	// plain extraction (no ternary over CPtr -- UAF)
	if ( IsValid( pCond ) )
		return pCond->nPriority;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail roulette weight (CGlobalAck::GetSequence @0x339230, oracle s2_globalack.h:251): the ack's
// own probability SCALED by its condition's probability factor (1.0 when absent) -- e.g. order
// confirmations carry factor 0.1, enemy-spotted 0.2 in the retail AckConditions table.
static float GetAckWeight( NDb::CDBAck *pAck )
{
	if ( !IsValid( pAck ) )
		return 0.f;
	float fFactor = 1.f;
	NDb::CDBAckCondition *pCond = pAck->pCondition;	// plain extraction (no ternary over CPtr -- UAF)
	if ( IsValid( pCond ) )
		fFactor = pCond->fProbability;
	return fFactor * pAck->fProbability;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGlobalAck::FetchHighestAcks()
{
	// find the maximum priority -- retail @0x339150: the per-entry value is GetAckPriority
	// (sequence priority with the CONDITION fallback), not the bare sequence field.
	int nMaxPriority = 0;
	list< SAck >::iterator i;
	for ( i = sequences.begin(); i != sequences.end(); ++i )
		if ( GetAckPriority( i->pAck ) > nMaxPriority )
			nMaxPriority = GetAckPriority( i->pAck );
	// remove all acks with lower priority (ties at the max are kept)
	for ( i = sequences.begin(); i != sequences.end(); )
		if ( GetAckPriority( i->pAck ) < nMaxPriority )
			i = sequences.erase( i );
		else
			++i;
	return nMaxPriority;	// retail @0x339150 writes the highest through its out param
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CDBAckSequence *CGlobalAck::GetSequence( IPlayer *pPlayer, CUnitServer **ppSpeaker, int *pnPriority )
{
	NDb::CDBAckSequence *pRes = 0;
	//RemoveInvisibleSequences( pPlayer );
	const int nHighest = FetchHighestAcks();
	// retail @0x339230 reports the fetched highest priority through its int* out param; CheckForAcks
	// feeds THAT into the CAckEvent (the AckSeqs rows all carry 0 -- see GetAckPriority above).
	if ( pnPriority )
		*pnPriority = nHighest;
	if ( !sequences.empty() )
	{
		// Select one of the Acks -- retail weight: ack probability x condition factor
		float fProb = 0;
		CRoulette roulette;
		for ( list< SAck >::iterator i = sequences.begin(); i != sequences.end(); ++i )
		{
			const float fWeight = GetAckWeight( i->pAck );
			fProb += fWeight;
			roulette.AddSector( fWeight );
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
			// retail @0x339230: the winning ack's queued SPEAKER rides along with the sequence
			if ( ppSpeaker )
				*ppSpeaker = i->pUS;
		}
		// clear
		sequences.clear();
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalAck::SayAck( CUnitServer *pWho, int nConditionID )
{
	CDBTable<NDb::CDBAck> *pDBTable = NDatabase::GetTable<NDb::CDBAck>();
	CDBIterator<NDb::CDBAck> i(*pDBTable);
	// retail GetAckHolder @0x2ba8f0: a hero's ack rows come from his VOICE-DONOR pers (the chargen
	// voice pick), not his own single-voice persona
	const int nAckPersID = pWho->GetUnitRPG()->GetRPGUnit()->GetAckPersID();
	while ( pDBTable && i.MoveNext() )
	{
		NDb::CDBAck *pDBAck = i.Get();
		if ( pDBAck && pDBAck->nRPGPersID == nAckPersID &&
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
// retail CAckBase::IsThis @0x338ec0: the ack owner is alive AND is exactly this unit
bool CAckBase::IsThis( CUnitServer *pWho )
{
	if ( !IsValid( pUnit ) )
		return false;
	return pWho == pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAckBase::IsFriend @0x338ef0: both units alive, DIFFERENT units, same player
// (the "team" probe is the CUnit-base vtbl call resolved to CUnitServer::GetPlayer @0x3bf680;
// vftable 0x8cd084 slot +0x34)
bool CAckBase::IsFriend( CUnitServer *pWho )
{
	if ( !IsValid( pUnit ) || !IsValid( pWho ) )
		return false;
	if ( pWho == pUnit )
		return false;
	return pUnit->GetPlayer() == pWho->GetPlayer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAckBase::IsEnemy @0x338f80: owner alive, different unit, diplomacy toward pWho == DS_ENEMY.
// NOTE: unlike IsFriend, retail does NOT gate pWho's liveness here -- kept faithful.
bool CAckBase::IsEnemy( CUnitServer *pWho )
{
	if ( !IsValid( pUnit ) )
		return false;
	if ( pWho == pUnit )
		return false;
	return pUnit->GetDiplomacyState( pWho ) == NDb::DS_ENEMY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAckBase::CanSee @0x338fe0: owner alive AND pWho is a member of the owner's TBS
// visible-unit set (the same list CUnitServer::IsUnitVisible @0x3bfb70 walks).
bool CAckBase::CanSee( CUnitServer *pWho )
{
	if ( !IsValid( pUnit ) )
		return false;
	return pUnit->IsUnitVisible( pWho );
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