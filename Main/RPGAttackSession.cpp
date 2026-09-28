#include "StdAfx.h"
#include "RPGAttackSession.h"
#include "RPGUnit.h"     // complete NRPG::CUnit       (for the CPtr<CUnit> member)
#include "rpgGlobal.h"   // complete NRPG::CGlobalGame  (for the CPtr<CGlobalGame> member)
#include "../DBFormat/DataMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// rpgAttackSession.obj -- NRPG::CUnitMissionForMedals attack-session bookkeeping.
// Transient bookkeeping reconstructed from retail. The decomp
// open-codes the std::vector grow + per-append CPtr AddRef/ReleaseRef churn (net +1
// ref on each stored slot); here that is one push_back of a bare IUnitMission* into
// a vector<CPtr<IUnitMission> >, with CPtr doing the refcounting. See RPGAttackSession.h
// for the reachability audit of the unreferenced AddMedalPoints (VA 0x68fbf0).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitMissionForMedals::CUnitMissionForMedals( CUnit *pUnit, CGlobalGame *pGlobalGame ):
	bFinished( false ), pMedalsGainer( pUnit ), pGame( pGlobalGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMissionForMedals::AddMedalPoints()
{
	if ( IsValid( pMedalsGainer ) )
	{
		const bool bOwnPK = IsValid( GetPanzerklein() );
		for ( int i = 0; i < attackedInSession.size(); ++i )
		{
			IUnitMission *pTarget = attackedInSession[i];
			if ( IsValid( pTarget ) && MedalDiplomacyState( pTarget->GetScenarioPlayerID() ) == NDb::DS_ENEMY )
				pMedalsGainer->AddMedalPoints( pGame,
					IsValid( pTarget->GetPanzerklein() ) && !bOwnPK ? MPC_ATTACK_ENEMY_IN_PK : MPC_ATTACK_ENEMY, 0 );
		}
		for ( int i = 0; i < hitInSession.size(); ++i )
		{
			IUnitMission *pTarget = hitInSession[i];
			if ( !IsValid( pTarget ) )
				continue;
			NDb::EDiplomacyState relation = MedalDiplomacyState( pTarget->GetScenarioPlayerID() );
			if ( relation == NDb::DS_NEUTRAL )
				continue;
			pMedalsGainer->AddMedalPoints( pGame, relation == NDb::DS_ALLY ? MPC_HIT_ALLY :
				( IsValid( pTarget->GetPanzerklein() ) && !bOwnPK ? MPC_HIT_ENEMY_IN_PK : MPC_HIT_ENEMY ), 0 );
		}
		for ( int i = 0; i < criticalHits.size(); ++i )
		{
			IUnitMission *pTarget = criticalHits[i];
			if ( !IsValid( pTarget ) )
				continue;
			NDb::EDiplomacyState relation = MedalDiplomacyState( pTarget->GetScenarioPlayerID() );
			if ( relation == NDb::DS_NEUTRAL )
				continue;
			if ( relation == NDb::DS_ALLY )
				pMedalsGainer->AddMedalPoints( pGame, MPC_CRITICAL_HIT_ALLY, 0 );
			else if ( IsValid( pTarget->GetPanzerklein() ) && !bOwnPK )
				pMedalsGainer->AddMedalPoints( pGame, MPC_CRITICAL_HIT_ENEMY_IN_PK, 0 );
		}
		for ( int i = 0; i < hitsPK.size(); ++i )
		{
			IUnitMission *pTarget = hitsPK[i];
			if ( IsValid( pTarget ) && MedalDiplomacyState( pTarget->GetScenarioPlayerID() ) == NDb::DS_ENEMY )
				pMedalsGainer->AddMedalPoints( pGame, MPC_HIT_PK, 0 );
		}
	}
	attackedInSession.clear();
	hitInSession.clear();
	criticalHits.clear();
	hitsPK.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// True if pAttack is already stored in list (pointer identity), mirroring the
// release linear scan `for ( it = begin; it != end && *it != pAttack; ++it )`.
static bool AlreadyRecorded( const vector<CPtr<IUnitMission> > &list, IUnitMission *pAttack )
{
	for ( vector<CPtr<IUnitMission> >::const_iterator i = list.begin(); i != list.end(); ++i )
	{
		if ( i->GetPtr() == pAttack )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::CUnitMissionForMedals::AddAttackToAttackSession @ rpgAttackSession.obj (release VA 0x68ffc0)
void CUnitMissionForMedals::AddAttackToAttackSession( IUnitMission *pAttack, bool bHit, bool bCritical )
{
	// (1) pick the list, (2)/(3) de-duplicated insert -- a repeat is not re-stored.
	vector<CPtr<IUnitMission> > &target = bHit ? hitInSession : attackedInSession;
	if ( !AlreadyRecorded( target, pAttack ) )
		target.push_back( pAttack );          // CPtr ctor takes one net ref on the stored slot
	// (4) critical hits: gated on bHit AND bCritical, appended WITHOUT de-dup (repeats stack).
	if ( bHit && bCritical )
		criticalHits.push_back( pAttack );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::CUnitMissionForMedals::AddPKHitToAttackSession @ rpgAttackSession.obj (release VA 0x690110)
void CUnitMissionForMedals::AddPKHitToAttackSession( IUnitMission *pAttack )
{
	if ( AlreadyRecorded( hitsPK, pAttack ) )
		return;                               // already recorded -- no double entry
	hitsPK.push_back( pAttack );              // de-duplicated insert, net +1 ref
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::CUnitMissionForMedals::Segment @ rpgAttackSession.obj (release VA 0x68ff40)
void CUnitMissionForMedals::Segment()
{
	if ( !bFinished )
		return;
	// Still pending while ANY awaited bullet is a live object (non-null, destroyed-bit clear).
	for ( vector<CPtr<CObjectBase> >::iterator i = waitForBullets.begin(); i != waitForBullets.end(); ++i )
	{
		if ( IsValid( *i ) )
			return;
	}
	// All awaited bullets resolved -> release the wait list and leave the finished-but-waiting state.
	waitForBullets.clear();
	bFinished = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
