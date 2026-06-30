#include "StdAfx.h"
//
#include "..\DBFormat\DataRPG.h"
#include "aiPosition.h"
#include "aiUnit.h"
#include "aiState.h"
#include "aiNearestPosition.h" // NAI::GetNearestPosition (TerrorPK rampage target)
#include "aiMoveAction.h"      // NAI::GetUnitPos
#include "aiInventory.h"
#include "aiWeapon.h"
#include "AILog.h"             // dev log records (reused)
#include "aiCombatLog.h"       // CAILog (release container)
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"
#include "rpgItem.h"
#include "RPGItemSet.h"       // full NRPG::CGrenadeItem / CWeaponItem defs (the IGrenadeItem/IWeaponItem
                              // bases must be VISIBLE here, else the item->interface upcast silently fails)
#include "rpgUnitMission.h"
#include "rpgUnitInfo.h"
#include "RPGUnit.h"          // NRPG::CUnit::Skills (suit HP via ST_VP), CDynamicSkill
//
#include "aiActions.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - concrete action bodies (structural port). WIP - NOT yet in
// Main.vcxproj. Phase 3b: the attack family (shoot/grenade/rocket) faithfully adapted from the dev
// aiAttackAction.cpp to the release CAIAction (Do(CAILog*) via operator<<, SInfo+SActionInfo cache,
// rebased on IAIUnit). The enemy-group helpers are carried over verbatim. The remaining 16 actions
// follow in the phase-3b continuation.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	EUnitCommandResult CanUnitThrowGrenade( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IGrenadeItem *pGrenade );
	EUnitCommandResult CanUnitLaunchRocket( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, int nExtraAP, NRPG::IWeaponItem *pBazooka );
	EUnitCommandResult CanUnitThrowKnife( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IMeleeWeaponItem *pMelee );
	bool IsWithinHumanReach( const CVec3 &ptFrom, const CVec3 &ptTarget, float fPlaneDist );
	EUnitCommandResult CanDoFirstAid( CUnitServer *pUS, const NAI::SUnitPosition &from, CUnitServer *pTarget );
}
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIShootAction
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIShootAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	if ( !IsValid( GetUnit() ) || !IsValid( GetEnemy() ) )
		return;
	//
	// GetBestFireArms reports cover (int) + a quality/expected-damage estimate (int) used only to decide
	// bKillTargetCertainly; the release SInfo keeps neither raw value (cover is stored as the float fCover).
	int nCover = 0, nQuality = 0;
	pInfo->pWeapon =
		GetUnit()->GetAIInventory()->GetBestFireArms( place.place, GetEnemy(), place.nUnitAP, &nCover, &nQuality, &pInfo->shootMode, &pInfo->nToHit );
	if ( IsValid( pInfo->pWeapon ) )
	{
		pInfo->bCanDo = true;
		bool bNeedReload = pInfo->pWeapon->GetCurrentClip()->GetAmmoCount() <= 0;
		if ( bNeedReload && !IsValid( pInfo->pWeapon->GetNextClip() ) )
			pInfo->bCanDo = false;
		pInfo->fCover = (float)nCover;
		pInfo->hitLocation = HL_ANY;            // release computes an aimed body part; the dev path fires center-mass
		pInfo->nAPToSpend = place.nUnitAP;        // the shot budget Do() spends down (release reads this field)
		pInfo->bKillTargetCertainly = GetEnemy()->GetHP() * 2.5f < nQuality;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIShootAction::Do( CAILog *pLog ) const
{
	ASSERT( IsValid( pLog ) );
	if ( !IsValid( pLog ) )
		return;
	//
	SInfo info;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	GetInfoInner( GetCurrentPlace(), &info );
	ASSERT( info.bCanDo );
	if ( info.bCanDo && IsValid( pUnit ) && IsValid( pEnemy ) && IsValid( info.pWeapon ) && info.nToHit > 0 )
	{
		// release @0041cb10: switch to the weapon + shoot-mode ONCE, then log shots while the unit can still
		// afford one. The AP budget (nAP) is a LOCAL counter decremented per shot. Logged records do NOT mutate
		// the live unit (they apply only when the commander later executes them), so re-reading pUnit->GetAP()
		// inside the loop would never decrease -> infinite loop (the "AI freezes on attack, turn never ends" bug).
		if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
			*pLog << new CAILogChangeWeapon( pUnit, info.pWeapon );
		*pLog << new CAILogChangeShootMode( pUnit, info.pWeapon, info.shootMode );
		//
		// release @0041cb10: the shot budget is the cached info.nAPToSpend (the AP at the chosen place), a
		// LOCAL counter decremented per shot - re-reading the live unit AP would never decrease (records
		// apply only when the commander executes them) -> infinite loop.
		int nAP = info.nAPToSpend;
		bool bNeedReload;
		int nAmmo, nShotHP, nShotAP;
		info.pWeapon->GetShotParameters( pUnit->GetUnitPosition(), pEnemy, (int)info.fCover, nAP, &nShotAP, &nAmmo, &nShotHP, &bNeedReload );
		while ( nAmmo > 0 && nShotAP <= nAP )
		{
			*pLog << new CAILogShot( pUnit, pEnemy, info.hitLocation );
			*pLog << new CAILogSpendAP( pUnit, nShotAP );
			*pLog << new CAILogSpendAmmo( info.pWeapon->GetCurrentClip(), nAmmo );
			nAP -= nShotAP;
			info.pWeapon->GetShotParameters( pUnit->GetUnitPosition(), pEnemy, (int)info.fCover, nAP, &nShotAP, &nAmmo, &nShotHP, &bNeedReload );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIShootAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	const int N_GOOD_TOHIT = 20;
	//
	SInfo info1, info2;
	GetInfoInner( p1, &info1 );
	GetInfoInner( p2, &info2 );
	//
	if ( !info1.bCanDo )
		return false;
	if ( !info2.bCanDo )
		return true;
	if ( info2.nToHit >= N_GOOD_TOHIT )
		return false;
	ECheckMove pose = ( ECheckMove )( p1.place.pos.p.GetPose() );
	if ( pose == CM_INACTIVE || pose == CM_LAY )
		return false;
	if ( info1.nToHit < info2.nToHit )
		return false;
	if ( info1.nToHit > 0 && info1.nToHit > info2.nToHit )
		return true;
	else
	{
		// tie on to-hit: prefer the place closer to the enemy. fDistance is no longer cached in SInfo
		// (release layout), so compute it from the place here.
		if ( !IsValid( GetEnemy() ) )
			return false;
		CVec3 ptEnemy = GetEnemy()->GetPosition().GetCP();
		return fabs( ptEnemy - p1.place.GetCP() ) < fabs( ptEnemy - p2.place.GetCP() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowGrenadeAction - enemy-group helpers (carried over verbatim from dev aiAttackAction.cpp)
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsGoodGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, CAIFireArmsWeapon *pLauncher, const SAIUnitGroup &group, CVec3 *pTarget )
{
	if ( !IsValid( pLauncher ) )
		return false;
	if ( !group.allies.empty() || group.enemies.empty() )
		return false;
	//
	// CWeaponItem publicly derives NRPG::IWeaponItem - implicit upcast (not CDynamicCast); guard null.
	NRPG::CWeaponItem *pItem = pLauncher->GetItem();
	if ( !IsValid( pItem ) )
		return false;
	*pTarget = group.ptCenter;
	if ( NWorld::CanUnitLaunchRocket( pUS, pos, *pTarget, 0, pItem ) != NWorld::UCR_OK )
	{
		*pTarget = group.ptCenter + CVec3( 0, 0, 0.6f );
		CVec3 ptDir = pos.GetCP() - *pTarget;
		Normalize( &ptDir );
		*pTarget += ptDir * 1.7f;
		if ( NWorld::CanUnitLaunchRocket( pUS, pos, *pTarget, 0, pItem ) != NWorld::UCR_OK )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsGoodGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, CAIGrenadeWeapon *pGrenade, const SAIUnitGroup &group, CVec3 *pTarget )
{
	if ( !IsValid( pGrenade ) )
		return false;
	if ( !group.allies.empty() || group.enemies.empty() )
		return false;
	//
	// CGrenadeItem publicly derives NRPG::IGrenadeItem, so pass it via the plain (guaranteed) implicit
	// upcast - NOT CDynamicCast, which was returning null here and made CanUnitThrowGrenade deref a null
	// IGrenadeItem (crash on pGrenade->GetDBGrenade()). Guard the null item too, since CanUnitThrowGrenade
	// does not.
	NRPG::CGrenadeItem *pItem = pGrenade->GetItem();
	if ( !IsValid( pItem ) )
		return false;
	NWorld::EUnitCommandResult res = NWorld::CanUnitThrowGrenade( pUS, pos, group.ptCenter, pItem );
	if ( res != NWorld::UCR_OK )
		return false;
	//
	*pTarget = group.ptCenter;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static int GetNearestGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, T *pWeapon, const vector<SAIUnitGroup> &groups, CVec3 *pTarget )
{
	int nBestGroup = -1;
	int nGroup = 0;
	while ( nGroup < groups.size() )
	{
		const SAIUnitGroup &group = groups[ nGroup ];
		CVec3 ptTarget;
		if ( IsGoodGroup( pUS, pos, pWeapon, group, &ptTarget ) )
		{
			bool bBestGroup = true;
			if ( nBestGroup >= 0 )
			{
				const SAIUnitGroup &bestGroup = groups[ nBestGroup ];
				CVec3 unitPos = pos.GetCP();
				float fDistance = fabs2( group.ptCenter - unitPos );
				float fBestDistance = fabs2( bestGroup.ptCenter - unitPos );
				if ( !( fDistance < fBestDistance || ( fDistance == fBestDistance && group.enemies.size() > bestGroup.enemies.size() ) ) )
					bBestGroup = false;
			}
			if ( bBestGroup )
			{
				nBestGroup = nGroup;
				*pTarget = ptTarget;
			}
		}
		++nGroup;
	}
	return nBestGroup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsBadGroupHealth( const SAIUnitGroup &group )
{
	const int N_BAD_HEALTH = 15;
	vector< CPtr<IAIUnit> >::const_iterator i;
	int nHealth = 0;
	for ( i = group.enemies.begin(); i != group.enemies.end(); ++i )
		nHealth = Max( nHealth, (*i)->GetAP() );
	return nHealth <= N_BAD_HEALTH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIThrowGrenadeAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	pInfo->bBadGroupHealth = false;
	pInfo->nTargetSize = 0;
	// release @0x0041bdd0 guards BOTH the unit (IsValid) AND a non-null AI state up front, before touching
	// the inventory: CAIUnit::GetAIState is a weak back-pointer that is null for a unit not currently in an
	// AI state, and GetEnemyGroups() below would null-deref it.
	CPtr<IAIUnit> pUnit = GetUnit();
	IAIState *pState = IsValid( pUnit ) ? pUnit->GetAIState() : 0;
	if ( !IsValid( pUnit ) || pState == 0 )
		return;
	pInfo->pGrenade = pUnit->GetAIInventory()->GetBestGrenade( VNULL3 );
	if ( !IsValid( pInfo->pGrenade ) )
		return;
	// affordable from this place? release gates GetActionAP(pose, AC_THROW_GRENADE) <= place AP BEFORE the
	// (heavier) nearest-group search, so an unaffordable grenade never wins the decision - the turn then
	// falls through to shoot/advance instead of being spent doing nothing.
	NRPG::IUnitMission *pMission = pUnit->GetUnitMission();
	if ( IsValid( pMission ) && pMission->GetActionAP( place.place.GetPose(), NRPG::AC_THROW_GRENADE ) > place.nUnitAP )
		return;
	//
	CVec3 ptTarget;
	const vector<SAIUnitGroup> &groups = pState->GetEnemyGroups();
	int nGroup = GetNearestGroup( pUnit->GetUnitServer(), place.place, pInfo->pGrenade.GetPtr(), groups, &ptTarget );
	if ( nGroup >= 0 )
	{
		pInfo->bCanDo = true;
		const SAIUnitGroup &group = groups[ nGroup ];
		pInfo->ptTarget = ptTarget;             // throw point (release stores this; Do throws at it)
		pInfo->nTargetSize = group.enemies.size();
		pInfo->bBadGroupHealth = IsBadGroupHealth( group );
		pInfo->nAPToSpend = place.nUnitAP;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIThrowGrenadeAction::Do( CAILog *pLog ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pLog ) )
		return;
	//
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	ASSERT( info.bCanDo );
	if ( info.bCanDo )
	{
		CPtr<CAIGrenadeWeapon> pGrenade = info.pGrenade;
		NAI::EPose pose = pUnit->GetUnitPosition().GetPose();
		int nGrenadeThrowAP = pUnit->GetUnitMission()->GetActionAP( pose, NRPG::AC_THROW_GRENADE );
		while ( IsValid( pGrenade ) && pUnit->GetAP() >= nGrenadeThrowAP )
		{
			if ( !pUnit->GetAIInventory()->IsCurrentItem( pGrenade ) )
				*pLog << new CAILogChangeWeapon( pUnit, pGrenade );
			*pLog << new CAILogThrowGrenade( pUnit, info.ptTarget, pGrenade );   // cached throw point (release SInfo field)
			*pLog << new CAILogSpendAP( pUnit, nGrenadeThrowAP );
			pGrenade = pUnit->GetAIInventory()->GetBestGrenade( VNULL3 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIThrowGrenadeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	SInfo info1, info2;
	GetInfoInner( p1, &info1 );
	GetInfoInner( p2, &info2 );
	//
	if ( !info1.bCanDo )
		return false;                  // can't throw from p1 -> p1 not preferred (both-false -> equivalent)
	if ( !info2.bCanDo )
		return true;
	return p1.nUnitAP > p2.nUnitAP;    // both viable: prefer the place with more AP left
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILaunchRocketAction
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILaunchRocketAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	pInfo->bCanDo = false;
	pInfo->bBadGroupHealth = false;
	pInfo->nTargetSize = 0;
	// same up-front guard as the grenade action: a null AI-state weak back-pointer would null-deref at the
	// GetEnemyGroups() below (the combat logic only runs for AI-controlled units that are in a state).
	IAIState *pState = IsValid( pUnit ) ? pUnit->GetAIState() : 0;
	if ( !IsValid( pUnit ) || pState == 0 )
		return;
	//
	pInfo->pWeapon = pUnit->GetAIInventory()->GetBestRocketLaunchers();
	if ( !IsValid( pInfo->pWeapon ) )
		return;
	if ( pInfo->pWeapon->GetCurrentClip()->GetAmmoCount() <= 0 )
		return;   // empty launcher: not do-able (release SInfo has no bNeedReload; reloading is the reload action's job)
	//
	const vector<SAIUnitGroup> &groups = pState->GetEnemyGroups();
	int nGroup = GetNearestGroup( pUnit->GetUnitServer(), place.place, pInfo->pWeapon.GetPtr(), groups, &pInfo->ptTarget );
	if ( nGroup >= 0 )
	{
		const SAIUnitGroup &group = groups[ nGroup ];
		pInfo->bCanDo = true;
		pInfo->nTargetSize = group.enemies.size();
		pInfo->bBadGroupHealth = IsBadGroupHealth( group );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILaunchRocketAction::Do( CAILog *pLog ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pLog ) || !IsValid( pUnit ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pWeapon ) )   // GetInfoInner now leaves bCanDo=false for an empty launcher
		return;
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
		*pLog << new CAILogChangeWeapon( pUnit, info.pWeapon );
	*pLog << new CAILogChangeShootMode( pUnit, info.pWeapon, NDb::SM_Snap );
	*pLog << new CAILogShotPoint( pUnit, info.ptTarget );
	*pLog << new CAILogSpendAP( pUnit, info.pWeapon->GetShotAP() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILaunchRocketAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	SInfo info1, info2;
	GetInfoInner( p1, &info1 );
	GetInfoInner( p2, &info2 );
	//
	if ( !info1.bCanDo )
		return false;
	if ( !info2.bCanDo )
		return true;
	return p1.nUnitAP > p2.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Support actions (reconstruction/exports/actions_support.c). NOTE on dependencies still to port:
// release CAIInventory adds GetBestWeaponForReload/GetBestFirstAid/GetBestMeleeWeapon/GetBestThrowingWeapon;
// release weapon types CAIFireArmsWeaponBase/CAIFirstAid/CAIMeleeWeapon/CAIThrowingWeapon; release-new log
// records Heal/Melee/ThrowKnife (the release uses CreateAILog* factories - here `new CAILog*` on the dev
// records where present). GetActionAP takes (pose, AC, weaponLen) in the release; the 2-arg dev form +
// the AC code are used below and reconciled in the build-settle.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIReloadAction @0x0041c0d0/0x0041d2a0/0x0041b9c0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIReloadAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x0041c0d0
{
	pInfo->bCanDo = false;
	pInfo->pWeapon = 0;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetAIInventory() ) )
		return;
	// can't reload from an inactive pose (release gates on the pose category bits)
	if ( ( place.place.GetPose() & 0xc000 ) == 0xc000 )
		return;
	CAIFireArmsWeapon *pWeapon = pUnit->GetAIInventory()->GetBestWeaponForReload();
	pInfo->pWeapon = pWeapon;
	if ( IsValid( pWeapon ) )
	{
		// affordable from this place? release computes GetActionAP(pose, AC_RELOAD, weaponLen); the dev
		// firearm exposes the equivalent reload cost via GetReloadAP(). @addr (pose-dependence elided)
		if ( pWeapon->GetReloadAP() <= place.nUnitAP )
			pInfo->bCanDo = true;
	}
}
void CAIReloadAction::Do( CAILog *pLog ) const   // @0x0041d2a0
{
	ASSERT( IsValid( pLog ) );
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pWeapon ) )
		return;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetAIInventory() ) )
		return;
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
		*pLog << new CAILogChangeWeapon( pUnit, info.pWeapon );
	*pLog << new CAILogReloadWeapon( pUnit, info.pWeapon );
	*pLog << new CAILogSpendAP( pUnit, info.pWeapon->GetReloadAP() );
}
bool CAIReloadAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	return p2.nUnitAP < p1.nUnitAP;   // prefer the place with more AP left
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIHealAction @0x00451ae0/0x00451c90/0x00451a50
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIHealAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x00451ae0
{
	pInfo->bCanDo = false;
	pInfo->pFirstAid = 0;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) )
		return;
	pInfo->pFirstAid = pUnit->GetAIInventory()->GetBestFirstAid();
	if ( !IsValid( pInfo->pFirstAid ) )
		return;
	// only heal when badly hurt: currentHP <= 30% of max (release @0x00451ae0).
	if ( (float)pUnit->GetHP() > pUnit->GetMaxHP() * 0.3f )
		return;
	// not while the current enemy is close (release: bail if the enemy is within ~4 units).
	CPtr<IAIUnit> pEnemy = GetEnemy();
	if ( IsValid( pEnemy ) && fabs( pEnemy->GetPosition().GetCP() - pUnit->GetPosition().GetCP() ) < 4.0f )
		return;
	// affordable from this place? (release: GetActionAP(pose, AC_FIRSTAID, firstAidLen); dev 2-arg form.)
	NRPG::IUnitMission *pMission = pUnit->GetUnitMission();
	if ( IsValid( pMission ) && pMission->GetActionAP( place.place.GetPose(), NRPG::AC_FIRSTAID ) > place.nUnitAP )
		return;
	// world allows first-aid here (self target)?
	if ( NWorld::CanDoFirstAid( pUnit->GetUnitServer(), place.place, pUnit->GetUnitServer() ) == NWorld::UCR_OK )
		pInfo->bCanDo = true;
}
void CAIHealAction::Do( CAILog *pLog ) const   // @0x00451c90
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pFirstAid ) )
		return;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) )
		return;
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pFirstAid ) )
		*pLog << new CAILogChangeWeapon( pUnit, info.pFirstAid );
	*pLog << new CAILogHeal( pUnit, pUnit );   // self-heal
	*pLog << new CAILogSpendAP( pUnit, pUnit->GetUnitMission()->GetActionAP( pUnit->GetUnitPosition().GetPose(), NRPG::AC_FIRSTAID ) );
}
bool CAIHealAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	bool b1Crouch = ( p1.place.GetPose() == CROUCH ), b2Crouch = ( p2.place.GetPose() == CROUCH );
	if ( b1Crouch && !b2Crouch ) return true;     // prefer crouching to heal
	if ( !b1Crouch && b2Crouch ) return false;
	return p2.nUnitAP < p1.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMeleeAction @0x0041c3d0/0x0041d5b0/0x0041ba00
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIMeleeAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x0041c3d0
{
	pInfo->bCanDo = false;
	pInfo->pWeapon = 0;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	if ( !IsValid( pUnit ) || !IsValid( pEnemy ) )
		return;
	pInfo->pWeapon = pUnit->GetAIInventory()->GetBestMeleeWeapon();
	if ( !IsValid( pInfo->pWeapon ) )
		return;
	// affordable from this place? (release: GetActionAP(pose, AC_MELEE, weaponLen) <= place AP; the dev
	// 2-arg GetActionAP elides the weapon length, as the grenade/reload actions already do.)
	NRPG::IUnitMission *pMission = pUnit->GetUnitMission();
	if ( IsValid( pMission ) && pMission->GetActionAP( place.place.GetPose(), NRPG::AC_MELEE ) > place.nUnitAP )
		return;
	// enemy within melee reach of this place? (release computes the enemy's collider point; the enemy
	// centre is a faithful stand-in for the F_MELEE_DISTANCE test.)
	if ( NWorld::IsWithinHumanReach( place.place.GetCP(), pEnemy->GetPosition().GetCP(), F_MELEE_DISTANCE ) )
		pInfo->bCanDo = true;
}
void CAIMeleeAction::Do( CAILog *pLog ) const   // @0x0041d5b0
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pWeapon ) )
		return;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	if ( !IsValid( pUnit ) || !IsValid( pEnemy ) )
		return;
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
		*pLog << new CAILogChangeWeapon( pUnit, info.pWeapon );
	*pLog << new CAILogSpendAP( pUnit, pUnit->GetUnitMission()->GetActionAP( pUnit->GetUnitPosition().GetPose(), NRPG::AC_MELEE ) );
	// the strike: a weapon-agnostic CCmdShootObject against the enemy with the now-current melee weapon.
	// (release logs a dedicated CAILogMelee record; CAILogShot emits the same attack-object command.)
	*pLog << new CAILogShot( pUnit, pEnemy, HL_ANY );
}
bool CAIMeleeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	return p2.nUnitAP < p1.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowKnifeAction @0x0041c210/0x0041d420/0x0041b9e0 - GetInfoInner/Do parallel melee (throwing
// weapon + CanUnitThrow reach check + a ThrowKnife record). Body reconstructed structurally; the exact
// throw-reach gate is in the decompile @0x0041c210.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIThrowKnifeAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x0041c210
{
	pInfo->bCanDo = false;
	pInfo->pWeapon = 0;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	IAIState *pState = IsValid( pUnit ) ? pUnit->GetAIState() : 0;
	if ( !IsValid( pUnit ) || !IsValid( pEnemy ) || pState == 0 )
		return;
	pInfo->pWeapon = pUnit->GetAIInventory()->GetBestThrowingWeapon();
	if ( !IsValid( pInfo->pWeapon ) )
		return;
	NRPG::IUnitMission *pMission = pUnit->GetUnitMission();
	if ( IsValid( pMission ) && pMission->GetActionAP( place.place.GetPose(), NRPG::AC_THROW_KNIFE ) > place.nUnitAP )
		return;
	// the knife's RPG item as IMeleeWeaponItem (CMeleeWeaponItem publicly derives it -> plain upcast, not
	// CDynamicCast; CanUnitThrowKnife does the throw line-of-fire/range check).
	NRPG::CMeleeWeaponItem *pItem = pInfo->pWeapon->GetItem();
	if ( !IsValid( pItem ) )
		return;
	if ( NWorld::CanUnitThrowKnife( pUnit->GetUnitServer(), place.place, pEnemy->GetPosition().GetCP(), pItem ) == NWorld::UCR_OK )
		pInfo->bCanDo = true;
}
void CAIThrowKnifeAction::Do( CAILog *pLog ) const   // @0x0041d420
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( !info.bCanDo || !IsValid( info.pWeapon ) )
		return;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	if ( !IsValid( pUnit ) || !IsValid( pEnemy ) )
		return;
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
		*pLog << new CAILogChangeWeapon( pUnit, info.pWeapon );
	*pLog << new CAILogSpendAP( pUnit, pUnit->GetUnitMission()->GetActionAP( pUnit->GetUnitPosition().GetPose(), NRPG::AC_THROW_KNIFE ) );
	// throw at the enemy: same weapon-agnostic CCmdShootObject after the knife is the current weapon.
	*pLog << new CAILogShot( pUnit, pEnemy, HL_ANY );
}
bool CAIThrowKnifeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	return p2.nUnitAP < p1.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMoveToEnemyAction @0x00475110/0x00475030/0x00475040 - the place-move IS the effect (logged by
// CAICombatLogic::DoAction); the action's own Do() is empty.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIMoveToEnemyAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const
{
	// bCanDo iff the unit has any weapon and an enemy exists (advance to engage). @0x00475110
	CPtr<IAIUnit> pUnit = GetUnit();
	pInfo->bCanDo = IsValid( pUnit ) && IsValid( GetEnemy() ) && pUnit->GetAIInventory()->HasAnyWeapon();
}
void CAIMoveToEnemyAction::Do( CAILog *pLog ) const
{
	// empty - the move to the chosen place is emitted by CAICombatLogic::DoAction. @0x00475030
}
bool CAIMoveToEnemyAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	return p2.nUnitAP < p1.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILootAction @0x00463210/0x00463a80/0x004631c0 - GetInfoInner walks nearby frozen items, keeps the
// ones CAIInventory::IsItemNecessary wants (+ computes the items to drop to make room); Do logs the
// pickups/drops. GetInfoInner + Do are reconstructed in aiLootAction.cpp. ComparePlaces prefers more AP.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILootAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	return p2.nUnitAP < p1.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The whole snipe state machine (CAIBeginSnipeAction / CAICollectSnipeAPAction / CAISnipeShotAction /
// CAICancelSnipeAction -- GetInfoInner + Do) is reconstructed in aiSnipeAction.cpp; the heavy-gun (cannon)
// actions in aiHeavyGunAction.cpp. Their ComparePlaces + saveload registration stay below.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIWearPKAction - climb into the best reachable free panzerklein. Reconstructed from the matched-
// release decode (oracle: decomp/src/s2_aipkaction.h: GetInfoInner @0x0048d8b0, Do @0x0048dd10).
// Scans every unit server for candidate suits: empty suits with a live hull (IsEmptyPK + IsAddedToVisitor
// + HP != 0) are preferred; occupied suits whose pilot cannot fight come after. With the unit's wish-pose
// temporarily forced to RUN (reachability judged as if running), the first candidate with positive HP the
// unit can reach + take (CanDo(CCmdTakeCorpse) == UCR_OK) wins. Hook resolution: the suit-server wish-pose
// scratch (+0x28) is CDumbUnitServer::Get/SetWishPose, the run byte (+0x24) is Set/IsStrafing, the
// candidate HP is its RPG unit's ST_VP, the corpse mounted is the suit cast to NWorld::CUnit.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int WearPK_SuitHP( NWorld::CUnitServer *pS )
{
	NRPG::IUnitMissionInfo *pRPG = pS->GetRPG();
	NRPG::CUnit *pUnit = IsValid( pRPG ) ? pRPG->GetRPGUnit() : 0;
	return IsValid( pUnit ) ? (int)pUnit->Skills( NDb::ST_VP ) : 0;
}
void CAIWearPKAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x0048d8b0
{
	pInfo->bCanDo = false;
	pInfo->pPK = 0;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) )
		return;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// already wearing a live PK -> nothing to climb into.
	if ( IsValid( pUS->GetWearingDBPK() ) )
		return;
	NWorld::CWorld *pWorld = pUS->GetWorld();
	if ( !IsValid( pWorld ) )
		return;
	list< CPtr<NWorld::CUnitServer> > all;
	pWorld->GetAllUnits( &all );
	vector< NWorld::CUnitServer* > cands;    // empty suits (preferred)
	vector< NWorld::CUnitServer* > pilots;   // occupied suits whose pilot cannot fight
	for ( list< CPtr<NWorld::CUnitServer> >::iterator it = all.begin(); it != all.end(); ++it )
	{
		NWorld::CUnitServer *pS = it->GetPtr();
		if ( !IsValid( pS ) )
			continue;
		bool bEmpty = pS->IsEmptyPK();
		if ( bEmpty && pS->IsAddedToVisitor() && WearPK_SuitHP( pS ) != 0 )
		{
			cands.push_back( pS );
			continue;
		}
		if ( !bEmpty && IsValid( pS->GetWearingDBPK() ) && !pS->CanFight() )
			pilots.push_back( pS );
	}
	for ( int i = 0; i < (int)pilots.size(); ++i )
		cands.push_back( pilots[i] );
	if ( cands.empty() )
		return;
	// judge reachability as if the unit runs to the suit.
	NAI::EPose oldPose = pUS->GetWishPose();
	pUS->SetWishPose( NAI::RUN );
	for ( int i = 0; i < (int)cands.size(); ++i )
	{
		CObj<NWorld::CCmd> cmd = new NWorld::CCmdTakeCorpse( (NWorld::CUnit*)cands[i] );
		if ( WearPK_SuitHP( cands[i] ) > 0 && pUS->CanDo( cmd.GetPtr() ) == NWorld::UCR_OK )
		{
			pInfo->bCanDo = true;
			pInfo->pPK = cands[i];
			break;
		}
	}
	pUS->SetWishPose( oldPose );
	if ( oldPose == NAI::CRAWL )            // restoring CRAWL also clears the run byte
		pUS->SetStrafe( false );
}
void CAIWearPKAction::Do( CAILog *pLog ) const   // @0x0048dd10
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	NWorld::CUnitServer *pPK = info.pPK.GetPtr();
	if ( !info.bCanDo || !IsValid( pPK ) )
		return;
	SPlaceWithAP fresh = GetCurrentPlace();
	// normalise the pose to RUN at the current place, then mount the suit.
	*pLog << new CAILogPosition( GetUnit(), GetUnit()->GetPosition(), fresh.place.pos, NAI::RUN );
	*pLog << new CAILogWearPK( GetUnit(), pPK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITerrorPKAction - in a dying panzerklein, walk into the thick of the nearest enemy group (the
// terror weapon works by proximity in the world layer). Reconstructed from the matched-release decode
// (oracle: decomp/src/s2_aiterrorpkaction.h: GetInfoInner @0x004aa420, Do @0x004aa770). Hook
// resolution: the decode's IsUnitBusy gate is the release IAIUnit::IsInPK (DIA-confirmed @0x4ad4f0 ==
// valid unit-server wearing a live PK record); the suit-HP read is the suit-server RPG unit's ST_VP
// skill (current via operator int, max via GetMaxValue); enemy-group centroids come from
// IAIState::GetEnemyGroups; the rampage target is GetUnitPos(GetNearestPosition(centroid)).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITerrorPKAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x004aa420
{
	pInfo->bCanDo = false;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) )
		return;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// IsInPK gate (release IAIUnit::IsInPK == valid unit-server wearing a live PK record).
	if ( !IsValid( pUS->GetWearingDBPK() ) )
		return;
	NWorld::CUnitServer *pPK = pUS->GetWearingPK();
	if ( !IsValid( pPK ) )
		return;
	if ( !IsValid( pUS->GetWearingDBPK() ) )           // the binary re-checks the worn record here
		return;
	// the rampage is a DESPERATION move: only once the suit is hurt to <= 25% of its max HP.
	NRPG::IUnitMissionInfo *pRPG = pPK->GetRPG();
	NRPG::CUnit *pRPGUnit = IsValid( pRPG ) ? pRPG->GetRPGUnit() : 0;
	if ( !IsValid( pRPGUnit ) )
		return;
	int nMax = pRPGUnit->Skills( NDb::ST_VP ).GetMaxValue();
	int nCur = pRPGUnit->Skills( NDb::ST_VP );           // CDynamicSkill::operator int() == current
	if ( (float)nMax * 0.25f < (float)nCur )
		return;   // still healthy -> no rampage yet
	// walk into the nearest enemy group, unless already standing in it (< 1.0 m^2 away).
	CVec3 cp = pUnit->GetPosition().GetCP();
	IAIState *pState = pUnit->GetAIState();
	if ( !IsValid( pState ) )
		return;
	const vector<SAIUnitGroup> &groups = pState->GetEnemyGroups();
	int nBest = -1;
	float fBest = 65535.0f;
	for ( int i = 0; i < (int)groups.size(); ++i )
	{
		const CVec3 &c = groups[i].ptCenter;
		float fDX = cp.x - c.x, fDY = cp.y - c.y, fDZ = cp.z - c.z;
		float fSq = fDX * fDX + fDY * fDY + fDZ * fDZ;
		if ( fSq < fBest )
		{
			fBest = fSq;
			nBest = i;
		}
	}
	if ( nBest < 0 || fBest < 1.0f )
		return;   // no group, or already standing in it
	NWorld::CWorld *pWorld = pUS->GetWorld();
	if ( !IsValid( pWorld ) )
		return;
	NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
	const CVec3 &ptCenter = groups[nBest].ptCenter;
	SPosition nearPos = GetNearestPosition( ptCenter, pNet, false, ptCenter );
	pInfo->place = GetUnitPos( nearPos.p, pNet );
	pInfo->bCanDo = true;
}
void CAITerrorPKAction::Do( CAILog *pLog ) const   // @0x004aa770
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( info.bCanDo )
		*pLog << new CAILogPosition( GetUnit(), GetUnit()->GetPosition(), info.place.pos, NAI::WALK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILeavePKAction - climb out of the worn panzerklein once it is broken. Reconstructed from the
// matched-release decode (oracle: decomp/src/s2_aipkaction.h: GetInfoInner @0x0048d680, Do
// @0x0048d820). SInfo stays minimal {bCanDo}. The decode's opaque suit-HP reach (the suit-server's
// own HP skill, "< 1" => broken) is expressed in-tree as its RPG unit's ST_VP current value (the
// engine's current-HP skill, the same getter the dev HP path uses).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILeavePKAction::GetInfoInner( const SPlaceWithAP &place, SInfo *pInfo ) const   // @0x0048d680
{
	pInfo->bCanDo = false;
	CPtr<IAIUnit> pUnit = GetUnit();
	if ( !IsValid( pUnit ) )
		return;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// must be wearing a live panzerklein (DB record + worn suit-server both alive).
	if ( !IsValid( pUS->GetWearingDBPK() ) )
		return;
	NWorld::CUnitServer *pPK = pUS->GetWearingPK();
	if ( !IsValid( pPK ) )
		return;
	// climb out once the suit's own current HP has dropped below 1 (the suit is broken).
	NRPG::IUnitMissionInfo *pRPG = pPK->GetRPG();
	NRPG::CUnit *pRPGUnit = IsValid( pRPG ) ? pRPG->GetRPGUnit() : 0;
	if ( IsValid( pRPGUnit ) && (int)pRPGUnit->Skills( NDb::ST_VP ) < 1 )
		pInfo->bCanDo = true;
}
void CAILeavePKAction::Do( CAILog *pLog ) const   // @0x0048d820
{
	if ( !IsValid( pLog ) )
		return;
	SInfo info;
	GetInfoInner( GetCurrentPlace(), &info );
	if ( info.bCanDo )
		*pLog << new CAILogLeavePK( GetUnit() );
}
//
// BeginSnipe prefers a crouched place, then more AP (@0x004a4120); the others prefer more AP.
bool CAIBeginSnipeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	if ( p1.place.GetPose() == CROUCH && p2.place.GetPose() != CROUCH )
		return true;
	return p2.nUnitAP < p1.nUnitAP;
}
bool CAICollectSnipeAPAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAISnipeShotAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAICancelSnipeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAIDockWithHGAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAIUndockFromHGAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAIShootFromHGAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAITerrorPKAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAIWearPKAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
bool CAILeavePKAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const { return p2.nUnitAP < p1.nUnitAP; }
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
BASIC_REGISTER_CLASS( CAIShootAction )
BASIC_REGISTER_CLASS( CAIThrowGrenadeAction )
BASIC_REGISTER_CLASS( CAILaunchRocketAction )
BASIC_REGISTER_CLASS( CAIReloadAction )
BASIC_REGISTER_CLASS( CAIHealAction )
BASIC_REGISTER_CLASS( CAIMeleeAction )
BASIC_REGISTER_CLASS( CAIThrowKnifeAction )
BASIC_REGISTER_CLASS( CAIMoveToEnemyAction )
BASIC_REGISTER_CLASS( CAILootAction )
BASIC_REGISTER_CLASS( CAIBeginSnipeAction )
BASIC_REGISTER_CLASS( CAICollectSnipeAPAction )
BASIC_REGISTER_CLASS( CAISnipeShotAction )
BASIC_REGISTER_CLASS( CAICancelSnipeAction )
BASIC_REGISTER_CLASS( CAIDockWithHGAction )
BASIC_REGISTER_CLASS( CAIUndockFromHGAction )
BASIC_REGISTER_CLASS( CAIShootFromHGAction )
BASIC_REGISTER_CLASS( CAITerrorPKAction )
BASIC_REGISTER_CLASS( CAIWearPKAction )
BASIC_REGISTER_CLASS( CAILeavePKAction )
