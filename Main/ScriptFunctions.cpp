#include "StdAfx.h"
#include "A5Script.h"
//
#include "wMain.h"
#include "rpgUnit.h"
#include "..\MiscDll\LogStream.h"
#include "scriptPtr.h"
#include "scriptCommon.h"
#include "scriptUnitGroup.h"
#include "scriptUnit.h"
#include "scriptSequence.h"
#include "scriptDiplomacy.h"
#include "scriptVector.h"
#include "scriptRoute.h"
#include "scriptDialog.h"
#include "scriptObject.h"
#include "scriptScenario.h"
#include "scriptTemplate.h"
#include "scriptPosition.h"
#include "scriptUI.h"		// script-UI property bridge (CreateWindow/GetWindow/windowGet+SetProperty/
							// ButtonGet+CreateState/GetCursorPos) -- declarations; impl in scriptUI.cpp.
//
namespace NScript
{
static int Error_out( lua_State* state )
{
	Script script( state );
	Script::Object obj = script.GetObject(script.GetTop());
	csSystem << CC_RED << "Script error: " << CC_GREY << obj.GetString() << endl;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#define REG_FUNCTION( Name ) { #Name, lua##Name }
Script::SRegFunction pRegList[] =
{
	// Common functions
	{ "_ERRORMESSAGE", Error_out },
	{ "out", luaOut },
	{ "Sleep", LuaCFuncSleep },
	{ "StartThread", LuaCFuncStartThread },
	{ "random", luaRandom },
	{ "HasInventoryItem", luaHasInventoryItem },
	REG_FUNCTION( IsRealTime ),
	REG_FUNCTION( PassCalcerIsActive ),
	REG_FUNCTION( Explosion ),
	REG_FUNCTION( GetTurn ),
	REG_FUNCTION( Difficulty ),
	// Ptr
	{ "Ptr", luaMakeCPtr },
	{ "ObjPtr", luaMakeCObj },
	{ "IsValid", luaIsValid },
	REG_FUNCTION( IsEqual ),
	// Diplomacy
	REG_FUNCTION( GetDiplomacy ),
	REG_FUNCTION( SetDiplomacy ),
	REG_FUNCTION( UnitGetDiplomacy ),
	REG_FUNCTION( UnitSetDiplomacy ),
	// Camera
	REG_FUNCTION( GetCamera ),
	REG_FUNCTION( CameraMove ),
	REG_FUNCTION( CameraSetClipping ),
	REG_FUNCTION( CameraSequence ),
	REG_FUNCTION( IsUIActionIDPresent ),
	// Sequence
	REG_FUNCTION( c_BeginSequence ),
	REG_FUNCTION( EndSequencePart ),
	REG_FUNCTION( EndSequence ),
	REG_FUNCTION( Floor ),
	// Unit
	REG_FUNCTION( GetUnit ),
	REG_FUNCTION( CreateUnit ),
	REG_FUNCTION( GetHero ),
	REG_FUNCTION( UnitApplyCritical ),
	REG_FUNCTION( UnitSetXPLevel ),
	REG_FUNCTION( UnitGetVisible ),
	REG_FUNCTION( UnitIsAction ),
	REG_FUNCTION( UnitMoveToWaypoint ),
	REG_FUNCTION( UnitFlyToWaypoint ),
	REG_FUNCTION( UnitSetToWaypoint ),
	REG_FUNCTION( RecalcWaypointPos ),
	REG_FUNCTION( UnitShoot ),
	REG_FUNCTION( UnitSetShootMode ),
	REG_FUNCTION( UnitSetPose ),
	REG_FUNCTION( UnitSetWishPose ),
	REG_FUNCTION( UnitSetDirection ),
	REG_FUNCTION( UnitIsDead ),
	REG_FUNCTION( UnitIsUnconscious ),
	REG_FUNCTION( UnitSayAck ),
	REG_FUNCTION( UnitSetToHit ),
	REG_FUNCTION( UnitIsSeeUnit ),
	REG_FUNCTION( UnitReload ),
	REG_FUNCTION( UnitCheat ),
	REG_FUNCTION( UnitSetRoute ),
	REG_FUNCTION( UnitKill ),
	REG_FUNCTION( UnitPlayAnimation ),
	REG_FUNCTION( UnitHide ),
	REG_FUNCTION( UnitDrawPerksTree ),
	REG_FUNCTION( UnitTakePerk ),
	REG_FUNCTION( UnitGiveRandomPerks ),
	REG_FUNCTION( UnitHoldItem ),
	REG_FUNCTION( UnitCancelAction ),
	REG_FUNCTION( UnitRemove ),
	REG_FUNCTION( UnitGetName ),
	REG_FUNCTION( UnitSetPlayer ),
	REG_FUNCTION( UnitSetDialog ),
	REG_FUNCTION( UnitSetCanTalk ),
	REG_FUNCTION( UnitRoaming ),
	REG_FUNCTION( UnitTakeCorpse ),
	REG_FUNCTION( UnitDropCorpse ),
	REG_FUNCTION( UnitMakeUnconscious ),
	REG_FUNCTION( UnitActivateWeapon ),
	REG_FUNCTION( UnitPlaceInPocket ),
	REG_FUNCTION( UnitRestoreFromPocket ),
	REG_FUNCTION( UnitGetRoute ),
	REG_FUNCTION( UnitAI ),
	// Inner Route
	REG_FUNCTION( RouteIsFinished ),
	// Player
	REG_FUNCTION( PlayerGetUnits ),
	REG_FUNCTION( PlayerGetUnitsEx ),
	REG_FUNCTION( PlayerGetMoney ),
	REG_FUNCTION( PlayerGiveMoney ),
	REG_FUNCTION( PlayerTakeMoney ),
	REG_FUNCTION( PlayerGiveTurn ),
	// UnitGroup
	REG_FUNCTION( CreateGroup ),
	REG_FUNCTION( GetGroup ),
	REG_FUNCTION( GroupGetID ),
	REG_FUNCTION( GroupAddUnit ),
	REG_FUNCTION( GroupRemoveUnit ),
	REG_FUNCTION( GroupGetSize ),
	REG_FUNCTION( GroupGetUnit ),
	REG_FUNCTION( GroupIsContainUnit ),
	REG_FUNCTION( GroupMoveToWaypoint ),
	REG_FUNCTION( GroupGetVisible ),
	REG_FUNCTION( GroupSetRoute ),
	REG_FUNCTION( GroupCheat ),
	REG_FUNCTION( GroupAddGroup ),
	REG_FUNCTION( GroupGetCross ),
	// Dialog
	REG_FUNCTION( DialogPlay ),
	REG_FUNCTION( DialogPlayAsAcks ),
	// Route
	REG_FUNCTION( CreateRoute ),
	// Object
	REG_FUNCTION( GetObject ),
	REG_FUNCTION( CreateObject ),
	REG_FUNCTION( ObjectOpen ),
	REG_FUNCTION( ObjectClose ),
	REG_FUNCTION( ObjectIsOpened ),
	REG_FUNCTION( ObjectDestroy ),
	REG_FUNCTION( ObjectRemove ),
	REG_FUNCTION( ObjectSetToWaypoint ),
	REG_FUNCTION( ObjectPlayAnimation ),
	REG_FUNCTION( ObjectIsAction ),
	REG_FUNCTION( ObjectSetDestroyStage ),
	REG_FUNCTION( ObjectGetName ),
	REG_FUNCTION( ObjectCancelAction ),
	// Item
	REG_FUNCTION( GetItem ),
	REG_FUNCTION( FindItem ),
	REG_FUNCTION( ItemGetName ),
	REG_FUNCTION( ItemRemove ),
	// Scenario
	REG_FUNCTION( ScenarioGiveClue ),
	REG_FUNCTION( ScenarioAddGoal ),
	REG_FUNCTION( ScenarioSetGoalComplete ),
	REG_FUNCTION( ScenarioSetTaskComplete ),
	REG_FUNCTION( ShowObjectives ),
	REG_FUNCTION( ScenarioOpenZone ),
	REG_FUNCTION( ScenarioBlockZone ),
	REG_FUNCTION( ExitToChapter ),
	REG_FUNCTION( ClueShow ),
	REG_FUNCTION( ClueIsFound ),
	REG_FUNCTION( GetCurrentZoneAILevel ),
	REG_FUNCTION( SetMaxCriticalSeverity ),
	REG_FUNCTION( LeaveToSubZone ),
	REG_FUNCTION( SetFirstMissionMode ),
	REG_FUNCTION( EnableFeature ),
	// Interface
	REG_FUNCTION( uiShowStore ),
	REG_FUNCTION( uiShowTeamMngMenu ),
	// Test
	REG_FUNCTION( LuaTest ),
	// Template
	REG_FUNCTION( PlaceTemplate ),
	// Distance
	REG_FUNCTION( GetPos ),
	REG_FUNCTION( GetWaypointPos ),
	REG_FUNCTION( GetDistance ),
	// ===== LUA convergence batch 1 (retail parity, 2026-06-25) =====
	// Unit RPG / state
	REG_FUNCTION( UnitGetSkill ),
	REG_FUNCTION( UnitSetSkill ),
	REG_FUNCTION( UnitGetSkillMaxValue ),
	REG_FUNCTION( UnitSetSkillMaxValue ),
	REG_FUNCTION( UnitGiveXP ),
	REG_FUNCTION( UnitIsHearUnit ),
	REG_FUNCTION( UnitIsUsingCannon ),
	REG_FUNCTION( UnitIsWearingPK ),
	REG_FUNCTION( GetUnitPK ),
	REG_FUNCTION( UnitSetHideProbability ),
	REG_FUNCTION( UnitMakeRouteInterruptable ),
	REG_FUNCTION( UnitApplyTableCritical ),
	// Scenario / globals / misc
	REG_FUNCTION( GetScenarioNumber ),
	REG_FUNCTION( GetGlobalGameVar ),
	REG_FUNCTION( SetGlobalGameVar ),
	REG_FUNCTION( TableGetSize ),
	{ "WhoHasInventoryItem", luaHasInventoryItem }, // release rename of dev HasInventoryItem
	// ===== LUA convergence batch 2 (NEEDS-CARE tier) =====
	REG_FUNCTION( UnitAttackWaypoint ),
	REG_FUNCTION( HasInventoryItemUnit ),
	REG_FUNCTION( HasInventoryItemGroup ),
	REG_FUNCTION( ObjectGetDestroyStage ),
	REG_FUNCTION( ObjectGetHP ),
	REG_FUNCTION( ObjectLockDoor ),
	REG_FUNCTION( ObjectUnlockDoor ),
	REG_FUNCTION( ItemUnload ),
	REG_FUNCTION( AttachEffectToWaypoint ),
	REG_FUNCTION( AttachEffectToUnitBone ),
	// ===== LUA convergence PART A (cheap remainders) =====
	REG_FUNCTION( UpdateVisible ),
	REG_FUNCTION( WantTurnBased ),
	REG_FUNCTION( SlowSyncAIMap ),
	REG_FUNCTION( c_StartGameEx ),
	REG_FUNCTION( c_DelayGameStartEx ),
	REG_FUNCTION( SetTimeOfDay ),
	REG_FUNCTION( UnitStop ),
	REG_FUNCTION( UnitKeepMoving ),
	REG_FUNCTION( UnitSetNormalLogic ),
	REG_FUNCTION( UnitSetGuardLogic ),
	REG_FUNCTION( UnitSetRetreatLogic ),
	REG_FUNCTION( UnitSetFearLogic ),
	REG_FUNCTION( UnitSetPanicLogic ),
	REG_FUNCTION( UnitSetCivilianLogic ),
	REG_FUNCTION( UnitSetScriptLogic ),
	REG_FUNCTION( UnitIsWeaponInHand ),
	REG_FUNCTION( UnitInArea ),
	REG_FUNCTION( UnitRegenerateVP ),
	REG_FUNCTION( UnitGrenadeToUnit ),
	REG_FUNCTION( UnitGrenadeToWaypoint ),
	REG_FUNCTION( UnitGetToHitUnit ),
	REG_FUNCTION( UnitGetToHitWaypoint ),
	REG_FUNCTION( UnitTakeObject ),
	REG_FUNCTION( KillEmAll ),
	REG_FUNCTION( UnitShootPrepare ),
	REG_FUNCTION( UnitLeavePK ),
	REG_FUNCTION( UnitWearPK ),
	REG_FUNCTION( UnitHealCriticals ),
	REG_FUNCTION( UnitIsCarryingCorpse ),
	REG_FUNCTION( UnitCreateItem ),
	REG_FUNCTION( UnitDrawWeapon ),
	REG_FUNCTION( UnitSwitchToGrenade ),
	REG_FUNCTION( CreateAndActivateItem ),
	REG_FUNCTION( DestroyItemInHand ),
	// ===== LUA convergence PART B (CUICmd* family) =====
	REG_FUNCTION( Pause ),
	REG_FUNCTION( CameraLock ),
	REG_FUNCTION( SetLeaveZoneMode ),
	REG_FUNCTION( PlayVideo ),
	REG_FUNCTION( BeginZone ),
	REG_FUNCTION( PlaySound ),
	REG_FUNCTION( StopSound ),
	REG_FUNCTION( Play3DSound ),
	REG_FUNCTION( PlayEffect ),
	REG_FUNCTION( StopEffect ),
	REG_FUNCTION( SetupAmbientLight ),
	REG_FUNCTION( SetAmbientEffect ),
	REG_FUNCTION( FadeOut ),
	REG_FUNCTION( FadeIn ),
	REG_FUNCTION( ShowLoseDialog ),
	REG_FUNCTION( ShowLeaveZoneDialog ),
	REG_FUNCTION( ShowHint ),
	REG_FUNCTION( AddHints ),
	REG_FUNCTION( SetTutorialMode ),
	// ===== LUA convergence: script-UI property bridge =====
	REG_FUNCTION( CreateWindow ),
	REG_FUNCTION( GetWindow ),
	{ "windowGetProperty", luaWindowGetProperty },
	{ "windowSetProperty", luaWindowSetProperty },
	REG_FUNCTION( ButtonGetState ),
	REG_FUNCTION( ButtonCreateState ),
	REG_FUNCTION( GetCursorPos ),
	REG_FUNCTION( GetUITime ),
	//
	{ 0, 0 } // End
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScript *CreateScript( NWorld::IWorld *pWorld )
{
	ASSERT( IsValid( pWorld ) );
	if ( !IsValid( pWorld ) )
		return 0;
	//
	CScript *pRes = new NScript::CScript();
	CDynamicCast<NWorld::CWorld> pTmpWorld( pWorld );
	pRes->pWorld = pTmpWorld;
	return pRes;
}
//
}