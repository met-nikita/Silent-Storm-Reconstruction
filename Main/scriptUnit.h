#ifndef __SCRIPTUNIT_H_
#define __SCRIPTUNIT_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( GetUnit );
DECLARE_SCRIPT_COMMAND( UnitGrenadeToUnit );
DECLARE_SCRIPT_COMMAND( UnitGrenadeToWaypoint );
DECLARE_SCRIPT_COMMAND( CreateUnit );
DECLARE_SCRIPT_COMMAND( GetHero );
DECLARE_SCRIPT_COMMAND( HasInventoryItem );
DECLARE_SCRIPT_COMMAND( UnitSayAck );
DECLARE_SCRIPT_COMMAND( UnitSetXPLevel );
DECLARE_SCRIPT_COMMAND( UnitApplyCritical );
DECLARE_SCRIPT_COMMAND( UnitIsAction );
DECLARE_SCRIPT_COMMAND( UnitShoot );
DECLARE_SCRIPT_COMMAND( UnitSetShootMode );
DECLARE_SCRIPT_COMMAND( UnitSetPose );
DECLARE_SCRIPT_COMMAND( UnitSetWishPose );
DECLARE_SCRIPT_COMMAND( UnitSetDirection );
DECLARE_SCRIPT_COMMAND( UnitIsDead );
DECLARE_SCRIPT_COMMAND( UnitIsUnconscious );
DECLARE_SCRIPT_COMMAND( UnitGetVisible );
DECLARE_SCRIPT_COMMAND( UnitIsSeeUnit );
DECLARE_SCRIPT_COMMAND( UnitReload );
DECLARE_SCRIPT_COMMAND( UnitCheat );
DECLARE_SCRIPT_COMMAND( UnitKill );
DECLARE_SCRIPT_COMMAND( UnitMakeUnconscious );
DECLARE_SCRIPT_COMMAND( UnitPlayAnimation );
DECLARE_SCRIPT_COMMAND( UnitHide );
DECLARE_SCRIPT_COMMAND( UnitDrawPerksTree ); // DEBUG
DECLARE_SCRIPT_COMMAND( UnitTakePerk );
DECLARE_SCRIPT_COMMAND( UnitGiveRandomPerks );
DECLARE_SCRIPT_COMMAND( UnitHoldItem );
DECLARE_SCRIPT_COMMAND( UnitCancelAction );
DECLARE_SCRIPT_COMMAND( UnitRemove );
DECLARE_SCRIPT_COMMAND( UnitGetName );
DECLARE_SCRIPT_COMMAND( UnitSetPlayer );
DECLARE_SCRIPT_COMMAND( UnitSetDialog );
DECLARE_SCRIPT_COMMAND( UnitSetCanTalk );
DECLARE_SCRIPT_COMMAND( UnitTakeCorpse );
DECLARE_SCRIPT_COMMAND( UnitTakeObject );
DECLARE_SCRIPT_COMMAND( UnitDropCorpse );
DECLARE_SCRIPT_COMMAND( UnitActivateWeapon );
DECLARE_SCRIPT_COMMAND( UnitPlaceInPocket );
DECLARE_SCRIPT_COMMAND( UnitRestoreFromPocket );
DECLARE_SCRIPT_COMMAND( UnitGetRoute );
DECLARE_SCRIPT_COMMAND( RouteIsFinished );
DECLARE_SCRIPT_COMMAND( UnitAI );
// --- LUA convergence batch 1 (retail parity) ---
DECLARE_SCRIPT_COMMAND( UnitGetSkill );
DECLARE_SCRIPT_COMMAND( UnitSetSkill );
DECLARE_SCRIPT_COMMAND( UnitGetSkillMaxValue );
DECLARE_SCRIPT_COMMAND( UnitSetSkillMaxValue );
DECLARE_SCRIPT_COMMAND( UnitGiveXP );
DECLARE_SCRIPT_COMMAND( UnitSetToHit );
DECLARE_SCRIPT_COMMAND( UnitIsHearUnit );
DECLARE_SCRIPT_COMMAND( UnitIsUsingCannon );
DECLARE_SCRIPT_COMMAND( UnitIsWearingPK );
DECLARE_SCRIPT_COMMAND( GetUnitPK );
DECLARE_SCRIPT_COMMAND( UnitSetHideProbability );
DECLARE_SCRIPT_COMMAND( UnitMakeRouteInterruptable );
DECLARE_SCRIPT_COMMAND( UnitApplyTableCritical );
// --- LUA convergence batch 2 ---
DECLARE_SCRIPT_COMMAND( UnitAttackWaypoint );
DECLARE_SCRIPT_COMMAND( HasInventoryItemUnit );
DECLARE_SCRIPT_COMMAND( HasInventoryItemGroup );
// --- LUA convergence (PART A: cheap remainders) ---
DECLARE_SCRIPT_COMMAND( UnitStop );
DECLARE_SCRIPT_COMMAND( UnitKeepMoving );
DECLARE_SCRIPT_COMMAND( KillEmAll );
DECLARE_SCRIPT_COMMAND( UnitShootPrepare );
DECLARE_SCRIPT_COMMAND( UnitLeavePK );
DECLARE_SCRIPT_COMMAND( UnitWearPK );
DECLARE_SCRIPT_COMMAND( UnitHealCriticals );
DECLARE_SCRIPT_COMMAND( UnitIsCarryingCorpse );
DECLARE_SCRIPT_COMMAND( UnitCreateItem );
DECLARE_SCRIPT_COMMAND( UnitDrawWeapon );
DECLARE_SCRIPT_COMMAND( UnitSwitchToGrenade );
DECLARE_SCRIPT_COMMAND( CreateAndActivateItem );
DECLARE_SCRIPT_COMMAND( DestroyItemInHand );
DECLARE_SCRIPT_COMMAND( UnitIsWeaponInHand );
DECLARE_SCRIPT_COMMAND( UnitInArea );
DECLARE_SCRIPT_COMMAND( UnitRegenerateVP );
DECLARE_SCRIPT_COMMAND( UnitGetToHitUnit );
DECLARE_SCRIPT_COMMAND( UnitGetToHitWaypoint );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTUNIT_H_