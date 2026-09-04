#include "StdAfx.h"
#include "aiThreatTracker.h"
#include "aiEvent.h"            // NAI::IAIEvent + the CreateAI*Event factories
#include "aiUnit.h"             // NAI::IAIUnit (GetAIUnitState)
#include "aiMisc.h"             // NAI::GetAIUnit
#include "aiUnitState.h"        // NAI::SAIUnitState (IsKnownCorpse)
#include "aiPosition.h"         // NAI::SUnitPosition (GetCP / GetEyePosition)
#include "wUnitServer.h"        // NWorld::CUnitServer (CanFight/IsAIUnit/GetDiplomacyState/GetPlayer/GetPosition/IsUnitVisible)
#include "wDumbUnit.h"          // NWorld::CDumbUnitServer (OnUnhide dyncast source)
#include "wMain.h"              // NWorld::IPlayer / NWorld::CPlayer
#include "../DBFormat/DataMap.h"// NDb::EDiplomacyState / DS_ENEMY (== 0)
#include "RPGGame.h"            // NRPG::IGame::GetMaxUnitSightDistance / IsCorpseVisible (corpse scan @0xab7a0)
#include "RPGUnitMission.h"     // NRPG::IUnitMission (GetUnitRPG()->GetRPGUnit())
#include "RPGUnit.h"            // NRPG::CUnit (GetMaxUnitSightDistance arg)

using namespace NWorld;

////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- documented absent release world seams (no dev twin); kept behaviour-neutral. ----
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
// Release CWorld range query (world vtbl+0x10c): collect the unit servers whose control point lies
// within fRadius of center. item 7 parity: wired to CWorld::GetUnitsNear (wMain.h:423) -- the exact dev
// twin UpdateVisible itself uses -- adapting its list result to the vector this caller expects.
void GetUnitServersAtRange( CUnitServer *pUS, const CVec3 &center, float fRadius,
	vector< CPtr<CUnitServer> > *pRes )
{
	pRes->clear();
	if ( !IsValid( pUS ) || pUS->GetWorld() == 0 || fRadius <= 0.f )
		return;
	list< CPtr<CUnitServer> > tmp;
	pUS->GetWorld()->GetUnitsNear( center, &tmp, fRadius );
	for ( list< CPtr<CUnitServer> >::iterator i = tmp.begin(); i != tmp.end(); ++i )
		pRes->push_back( *i );
}
// Release corpse-scan radius, decoded @0xab7a0: pWorld->GetGame() (world vtbl+0x1c @0x376e90) ->
// IGame::GetMaxUnitSightDistance (vtbl+0x38 @0x298520, = 2x per-unit sight; night/perk-aware) --
// the same gather radius UpdateVisible uses. The old N_SIGHTDISTANCE approximation is gone.
float GetCorpseScanRange( CUnitServer *pUS )
{
	if ( !IsValid( pUS ) || pUS->GetWorld() == 0 || pUS->GetWorld()->GetGame() == 0 )
		return 0.f;
	return pUS->GetWorld()->GetGame()->GetMaxUnitSightDistance( pUS->GetUnitRPG()->GetRPGUnit() );
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- constructors. Every ctor must initialise all 11 CEventRegister slots in member order (the
// template has no default ctor): each binds its OnXxx handler and auto-subscribes to the global bus. ----
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xab2e0: the default ctor (regs wired, back-pointers unbound).
CAIEventTrackerImpl::CAIEventTrackerImpl()
	: regOnSeeEnemy ( this, &CAIEventTrackerImpl::OnSeeEnemy ),
	  regOnLostEnemy( this, &CAIEventTrackerImpl::OnLostEnemy ),
	  regOnHearEnemy( this, &CAIEventTrackerImpl::OnHearEnemy ),
	  regOnHearAlly ( this, &CAIEventTrackerImpl::OnHearAlly ),
	  regOnGrenade  ( this, &CAIEventTrackerImpl::OnGrenade ),
	  regOnBullet   ( this, &CAIEventTrackerImpl::OnBullet ),
	  regOnDie      ( this, &CAIEventTrackerImpl::OnDie ),
	  regOnNewTurn  ( this, &CAIEventTrackerImpl::OnNewTurn ),
	  regOnStartGame( this, &CAIEventTrackerImpl::OnStartGame ),
	  regOnAttack   ( this, &CAIEventTrackerImpl::OnAttack ),
	  regOnUnhide   ( this, &CAIEventTrackerImpl::OnUnhide )
{
}
// @0xab4b0: the bound ctor used by CAIEventTracker (interface + unit back-pointers).
CAIEventTrackerImpl::CAIEventTrackerImpl( CAIEventTracker *_pInterface, CUnitServer *_pUnit )
	: regOnSeeEnemy ( this, &CAIEventTrackerImpl::OnSeeEnemy ),
	  regOnLostEnemy( this, &CAIEventTrackerImpl::OnLostEnemy ),
	  regOnHearEnemy( this, &CAIEventTrackerImpl::OnHearEnemy ),
	  regOnHearAlly ( this, &CAIEventTrackerImpl::OnHearAlly ),
	  regOnGrenade  ( this, &CAIEventTrackerImpl::OnGrenade ),
	  regOnBullet   ( this, &CAIEventTrackerImpl::OnBullet ),
	  regOnDie      ( this, &CAIEventTrackerImpl::OnDie ),
	  regOnNewTurn  ( this, &CAIEventTrackerImpl::OnNewTurn ),
	  regOnStartGame( this, &CAIEventTrackerImpl::OnStartGame ),
	  regOnAttack   ( this, &CAIEventTrackerImpl::OnAttack ),
	  regOnUnhide   ( this, &CAIEventTrackerImpl::OnUnhide ),
	  pUnit( _pUnit ), pInterface( _pInterface )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIEventTracker @0xab6b0: news the impl for a live unit (interface = this, unit = pUS).
CAIEventTracker::CAIEventTracker( CUnitServer *pUS )
{
	if ( IsValid( pUS ) )
		pImpl = new CAIEventTrackerImpl( this, pUS );
}
// CAIEventTracker copy ctor @0xabfe0: CObj copy -- alias + AddRef the shared impl.
CAIEventTracker::CAIEventTracker( const CAIEventTracker &src )
{
	pImpl = src.pImpl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ThrowAIEvent @0xaa910: own the event for the duration, forward it to the interface only for a live,
// fight-capable, AI-driven unit (a rejected event dies here when the CObj releases it).
void CAIEventTrackerImpl::ThrowAIEvent( IAIEvent *pEvent )
{
	CObj<IAIEvent> ev = pEvent;
	CAIEventTracker *pIface = pInterface;
	CUnitServer *pUS = pUnit;
	if ( IsValid( pIface ) && IsValid( pUS ) && pUS->CanFight() && pUS->IsAIUnit() )
		pIface->Notify( pEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIEventTracker::Notify (release vtbl slot 0): mutate the owning unit's threat state.
void CAIEventTracker::Notify( IAIEvent *pEvent )
{
	if ( !IsValid( pEvent ) || !IsValid( pImpl ) )
		return;
	CUnitServer *pUS = pImpl->GetUnit();
	if ( !IsValid( pUS ) )
		return;
	IAIUnit *pAI = GetAIUnit( pUS );
	if ( !IsValid( pAI ) )
		return;
	SAIUnitState *pState = pAI->GetAIUnitState();
	if ( pState != 0 )
		pEvent->Modify( pState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- handlers (oracle s2_threattracker.h, disasm-verified). The dev event factories take ONE
// IAIUnit* and fold the validity check inside (invalid -> null event), so the release (pAI, bAlive)
// pairs collapse to a single argument here. ----
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnDie @0xaa9d0: someone else died -> enemy-died (an enemy) or lost-ally (anyone else) by diplomacy.
void CAIEventTrackerImpl::OnDie( const CEventOnUnitDiedOrLoseConsciousness &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pWho = e.pWho;
	if ( pWho == pUS )
		return;
	IAIUnit *pAI = GetAIUnit( pWho );
	if ( pUS->GetDiplomacyState( pWho ) == NDb::DS_ENEMY )
		ThrowAIEvent( CreateAIEnemyDiedEvent( pAI ) );
	else
		ThrowAIEvent( CreateAILostAllyEvent( pAI ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnBullet @0xaaaa0: a bullet whose flight passes within 2.5 of the unit (and the unit lies within
// fDistance + 2.5 along the ray). Enemy shooter -> possible enemy (the shooter); friendly shooter at
// one of OUR enemies -> possible enemy (the target); other friendly fire -> ally-needs-help (shooter).
void CAIEventTrackerImpl::OnBullet( const CEventOnBullet &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pShooter = e.pShooter;
	if ( pUS == pShooter )
		return;
	CVec3 d = pUS->GetPosition().GetCP() - e.ray.ptOrigin;
	float fT = d * e.ray.ptDir;                       // projection along the shot ray
	if ( e.fDistance + 2.5f < fT )
		return;
	CVec3 perp = d - e.ray.ptDir * fT;
	float fPerp = (float)sqrt( perp * perp );
	if ( !( fPerp < 2.5f ) )
		return;
	CUnitServer *pCulprit = pShooter;
	if ( pUS->GetDiplomacyState( pShooter ) != NDb::DS_ENEMY )
	{
		CUnitServer *pTarget = e.pTarget;
		bool bTargetEnemy = pTarget != 0 && IsValid( pTarget ) &&
			pUS->GetDiplomacyState( pTarget ) == NDb::DS_ENEMY;
		if ( !bTargetEnemy )
		{
			if ( pShooter != pUS )
				ThrowAIEvent( CreateAIAllyNeedHelpEvent( GetAIUnit( pShooter ) ) );
			return;
		}
		pCulprit = pTarget;
	}
	ThrowAIEvent( CreateAIPossibleEnemyEvent( GetAIUnit( pCulprit ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnAttack @0xaacd0: a NON-enemy attacker (seen) firing at one of OUR enemies -> that target becomes a
// possible enemy.
void CAIEventTrackerImpl::OnAttack( const CEventOnAttackAtUnit &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pAttacker = e.pAttacker;
	if ( pAttacker == pUS || pAttacker == 0 )
		return;
	if ( pUS->GetDiplomacyState( pAttacker ) == NDb::DS_ENEMY )
		return;
	if ( pUS->GetDiplomacyState( e.pTarget ) != NDb::DS_ENEMY )
		return;
	if ( !pUS->IsUnitVisible( pAttacker ) )            // dev vision seam (release world vtbl+0x10)
		return;
	ThrowAIEvent( CreateAIPossibleEnemyEvent( GetAIUnit( e.pTarget ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnGrenade @0xaad90: a grenade from a thrower within 30.0 of the unit (ORIGINAL QUIRK: measured to the
// thrower's control point, the explosion position is ignored). Enemy thrower -> possible enemy; other
// thrower (not us) -> ally-needs-help.
void CAIEventTrackerImpl::OnGrenade( const CEventOnGrenadeExplosion &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pThrower = e.pThrower;
	if ( !IsValid( pThrower ) )
		return;
	CVec3 d = pThrower->GetPosition().GetCP() - pUS->GetPosition().GetCP();
	if ( !( (float)sqrt( d * d ) < 30.0f ) )
		return;
	IAIUnit *pAI = GetAIUnit( pThrower );
	if ( pUS->GetDiplomacyState( pThrower ) == NDb::DS_ENEMY )
		ThrowAIEvent( CreateAIPossibleEnemyEvent( pAI ) );
	else if ( pThrower != pUS )
		ThrowAIEvent( CreateAIAllyNeedHelpEvent( pAI ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnLostEnemy @0xaaeb0: only OUR own loss matters.
void CAIEventTrackerImpl::OnLostEnemy( const CEventOnLostEnemyFromSight &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	if ( e.pWatcher.GetPtr() != pUS )
		return;
	ThrowAIEvent( CreateAILostEnemyEvent( GetAIUnit( e.pTarget ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnSeeEnemy @0xaaf00: somebody non-hostile spots one of OUR enemies. Ourselves -> the full enemy
// event; an ally within 5.0 of us -> a possible-enemy event (unless the target IS us).
void CAIEventTrackerImpl::OnSeeEnemy( const CEventOnSeeNewEnemy &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pWatcher = e.pWatcher;
	CUnitServer *pTarget = e.pTarget;
	if ( !IsValid( pWatcher ) || !IsValid( pTarget ) )
		return;
	if ( pUS->GetDiplomacyState( pWatcher ) == NDb::DS_ENEMY )
		return;
	if ( pUS->GetDiplomacyState( pTarget ) != NDb::DS_ENEMY )
		return;
	if ( pWatcher == pUS )
	{
		ThrowAIEvent( CreateAIEnemyEvent( GetAIUnit( pTarget ) ) );
		return;
	}
	CVec3 d = pWatcher->GetPosition().GetCP() - pUS->GetPosition().GetCP();
	if ( (float)sqrt( d * d ) > 5.0f )
		return;
	if ( pTarget == pUS )
		return;
	ThrowAIEvent( CreateAIPossibleEnemyEvent( GetAIUnit( pTarget ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnHearEnemy @0xab060: we ourselves hear an enemy -> possible enemy.
void CAIEventTrackerImpl::OnHearEnemy( const CEventOnHearEnemy &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pHearer = e.pHearer;
	CUnitServer *pHeard = e.pTarget;
	if ( !IsValid( pHearer ) || !IsValid( pHeard ) )
		return;
	if ( pHearer != pUS )
		return;
	if ( pUS->GetDiplomacyState( pHeard ) != NDb::DS_ENEMY )
		return;
	ThrowAIEvent( CreateAIPossibleEnemyEvent( GetAIUnit( pHeard ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnHearAlly @0xab0f0: we hear a NON-enemy (and not ourselves) -> ally-needs-help.
void CAIEventTrackerImpl::OnHearAlly( const CEventOnHearAlly &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	CUnitServer *pHearer = e.pHearer;
	CUnitServer *pHeard = e.pTarget;
	if ( !IsValid( pHearer ) || !IsValid( pHeard ) )
		return;
	if ( pHearer != pUS )
		return;
	if ( pUS->GetDiplomacyState( pHeard ) == NDb::DS_ENEMY )
		return;
	if ( pHeard == pUS )
		return;
	ThrowAIEvent( CreateAIAllyNeedHelpEvent( GetAIUnit( pHeard ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnNewTurn @0xab180: control passed to OUR player -> begin-turn event.
void CAIEventTrackerImpl::OnNewTurn( const CEventOnPassControl &e )
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) )
		return;
	IPlayer *pOwn = pUS->GetPlayer();
	if ( !IsValid( pOwn ) )
		return;
	IPlayer *pEvt = e.pPlayer.GetPtr();   // CPlayer* -> IPlayer* (unambiguous direct base)
	if ( !IsValid( pEvt ) )
		return;
	if ( pEvt != pOwn )
		return;
	ThrowAIEvent( CreateAIBeginTurnEvent() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnUnhide @0xab260: our own unhide -> update event (the payload dumb server is dyncast to CUnitServer).
void CAIEventTrackerImpl::OnUnhide( const CEventOnUnitUnhide &e )
{
	CUnitServer *pWho = dynamic_cast<CUnitServer*>( e.pWho.GetPtr() );
	if ( !IsValid( pWho ) )
		return;
	if ( pWho != pUnit.GetPtr() )
		return;
	ThrowAIEvent( CreateAIUpdateEvent() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OnStartGame @0xab2c0: unconditional begin-turn.
void CAIEventTrackerImpl::OnStartGame( const CEventOnStartGame & )
{
	ThrowAIEvent( CreateAIBeginTurnEvent() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CheckForVisibleCorpses @0xab7a0 (fully decoded + live): a live, fight-capable, AI unit gathers the
// servers within GetMaxUnitSightDistance of its eye (GetUnitsNear, world vtbl+0x10c); every other unit
// that can no longer fight, is not already a known corpse, is NOT an enemy (enemy corpses ignored),
// and passes the geometric IsCorpseVisible @0x298da0 probe (range/FOV-gated rays at the body's
// corpseHLpos points -- NOT the TBS visible-list, which excludes corpses) raises CAICorpseEvent.
// NOTE: in retail the event only records the corpse (SAIUnitState::knownCorpses); the killer ->
// possibleEnemy arm is inert because CUnitServer::pKiller is never written (byte-scan proven).
void CAIEventTrackerImpl::CheckForVisibleCorpses()
{
	CUnitServer *pUS = pUnit;
	if ( !IsValid( pUS ) || !pUS->CanFight() || !pUS->IsAIUnit() )
		return;
	IAIUnit *pOwnAI = GetAIUnit( pUS );
	SAIUnitState *pState = IsValid( pOwnAI ) ? pOwnAI->GetAIUnitState() : 0;
	float fRange = GetCorpseScanRange( pUS );
	CVec3 eye = pUS->GetPosition().GetEyePosition();
	vector< CPtr<CUnitServer> > found;
	GetUnitServersAtRange( pUS, eye, fRange, &found );
	for ( int i = 0; i < (int)found.size(); ++i )
	{
		CUnitServer *pOther = found[i];
		if ( !IsValid( pOther ) || pOther == pUS )
			continue;
		IAIUnit *pAI = GetAIUnit( pOther );
		if ( !IsValid( pAI ) )
			continue;
		if ( pOther->CanFight() )
			continue;
		if ( pState != 0 && pState->IsKnownCorpse( pAI ) )
			continue;
		if ( pUS->GetDiplomacyState( pOther ) == NDb::DS_ENEMY )   // enemies don't count
			continue;
		if ( !pUS->GetWorld()->GetGame()->IsCorpseVisible( pUS, pOther ) )   // retail IGame vtbl+0x50 @0x298da0
			continue;
		ThrowAIEvent( CreateAICorpseEvent( pAI ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ProcessAISegment @0xabaa0 (impl) / @0xabae0 (the tracker forwarder only adds the impl-alive gate).
void CAIEventTrackerImpl::ProcessAISegment()
{
	CUnitServer *pUS = pUnit;
	if ( IsValid( pUS ) && pUS->CanFight() )
		CheckForVisibleCorpses();
}
void CAIEventTracker::ProcessAISegment()
{
	if ( IsValid( pImpl ) )
		pImpl->ProcessAISegment();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NAI
// retail saveload ids (serialization-convergence W2)
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x51143200, CAIEventTracker )
REGISTER_SAVELOAD_CLASS( 0x52433130, CAIEventTrackerImpl )
