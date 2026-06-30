#include "StdAfx.h"
#include "RPGAttackSession.h"
#include "RPGUnit.h"     // complete NRPG::CUnit       (for the CPtr<CUnit> member)
#include "rpgGlobal.h"   // complete NRPG::CGlobalGame  (for the CPtr<CGlobalGame> member)
////////////////////////////////////////////////////////////////////////////////////////////////////
// rpgAttackSession.obj -- NRPG::CUnitMissionForMedals attack-session bookkeeping.
// Three pure-bookkeeping methods reconstructed from the release decomp. The decomp
// open-codes the std::vector grow + per-append CPtr AddRef/ReleaseRef churn (net +1
// ref on each stored slot); here that is one push_back of a bare IUnitMission* into
// a vector<CPtr<IUnitMission> >, with CPtr doing the refcounting. See RPGAttackSession.h
// for why AddMedalPoints (VA 0x68fbf0) is deferred.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
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
