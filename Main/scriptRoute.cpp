#include "stdafx.h"
//
#include "A5Script.h"
#include "aiControl.h"
#include "aiCommander.h"
#include "aiPosition.h"
#include "aiRoute.h"
#include "rpgUnitMission.h"
#include "aiUnit.h"
#include "aiTaskCommander.h"
#include "wUnitServer.h"
#include "wMain.h"
#include "wUnitGroup.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wUnitCommands.h"
#include "aiMisc.h"				// NAI::GetAIUnit
#include "aiReactions.h"		// NAI::CAINormalReaction
#include "aiGuardReaction.h"	// NAI::CreateAIGuardReaction
#include "aiFearReaction.h"		// NAI::CreateAIFearReaction
#include "aiScriptReaction.h"	// NAI::CreateAIScriptReaction
//
#include "scriptRoute.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateRoute, "" )
	vector< CPtr<NAI::CAIRouteWaypoint> > waypoints;
	int nWaypoints = luaGetParamCount( pState );
	for ( int i = 1; i <= nWaypoints; ++i )
	{
		string szName = pScript->GetObject( i ).GetString();
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szName );
		if ( IsValid( pWaypoint ) )
			waypoints.push_back( pWaypoint );
	}
	luaPushCObj( pState, new NAI::CAIRoute( waypoints ) );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetRoute, "uub" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NAI::CAIRoute> pRoute( luaParams[ 1 ].p );
	bool bCircled = luaParams[ 2 ].b;
	if ( IsValid( pRoute ) && IsValid( pUS ) )
		NAI::SetUnitRoute( pUS, pRoute, bCircled, NAI::AIM_SCRIPT );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupSetRoute, "uub" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NAI::CAIRoute> pRoute( luaParams[ 1 ].p );
	bool bCircled = luaParams[ 2 ].b;
	if ( IsValid( pGroup ) && IsValid( pRoute ) )
		NAI::SetGroupRoute( pGroup, pRoute, bCircled, NAI::AIM_SCRIPT );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetOneWaypointRoute( NWorld::CUnitServer *pUS, NAI::CAIRouteWaypoint *pWaypoint )
{
	ASSERT( IsValid( pUS ) );
	ASSERT( IsValid( pWaypoint ) );
	if ( !IsValid( pUS ) || !IsValid( pWaypoint ) )
		return;
	//
	vector< CPtr<NAI::CAIRouteWaypoint> > waypoints;
	waypoints.push_back( pWaypoint );
	CPtr<NAI::CAIRoute> pRoute = new NAI::CAIRoute( waypoints );
	NAI::SetUnitRoute( pUS, pRoute, false, NAI::AIM_SCRIPT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND(UnitSetToWaypoint, "us")
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			NAI::SUnitPosition unitPos;
			unitPos.pos = pWaypoint->pos;
			unitPos.bRun = false;
			unitPos.SetPose( NAI::WALK );
			pUS->SetPosition( unitPos );
			pUS->animator.PlaceUnit( unitPos );
			SetOneWaypointRoute( pUS, pWaypoint );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eae20 ("s"): resolve the named waypoint and re-snap its position to the nearest native cell
// of its path network (CAIRouteWaypoint::RecalcPosition @0x95f60).
BEGIN_SCRIPT_COMMAND( RecalcWaypointPos, "s" )
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 0 ].s );
	if ( IsValid( pWaypoint ) )
		pWaypoint->RecalcPosition();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitMoveToWaypoint, "us" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			CDynamicCast<NAI::CAICommander> pAICommander(pUS->GetPlayer()->GetCommander());
			if (pAICommander)
			{
				// AI Unit
				SetOneWaypointRoute( pUS, pWaypoint );			
			}
			else
			{
				// Player unit
				NAI::SPosition pos = pWaypoint->pos;
				pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos ) ) );
				pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
			}
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ea9c0 ("us"): fly the unit to the named waypoint. The waypoint position is snapped to a
// 3D/fly place (NAI::MakeFlyPos) and queued as a CCmdFly (-> CExecFly -> CUnitAnimator::Fly).
BEGIN_SCRIPT_COMMAND( UnitFlyToWaypoint, "us" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			NAI::SPosition pos = pWaypoint->pos;
			NAI::MakeFlyPos( &pos, &pos );
			NAI::SUnitPosition unitPos;
			unitPos.pos = pos;
			unitPos.bRun = false;
			// retail NScript::DoCommand(pUS, new CCmdFly(unitPos), true): set the command, then continue.
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdFly( unitPos ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRoaming, "usn" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
			NAI::SetUnitRoaming( pUS, pWaypoint->pos.p, luaParams[ 2 ].n, NAI::AIM_SCRIPT );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb0e0: install the unit's default ("normal") AI reaction.
BEGIN_SCRIPT_COMMAND( UnitSetNormalLogic, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( new NAI::CAINormalReaction( pAI ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb230 ("un[4]"): install a guard reaction; arg1 (default 4) is the watch radius.
BEGIN_SCRIPT_COMMAND( UnitSetGuardLogic, "un[4]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( NAI::CreateAIGuardReaction( pAI, 0, luaParams[ 1 ].n ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eaf40 ("us"): install a retreat reaction -- the unit falls back to the named waypoint's grid
// place (CreateAIRetreatReaction(ai, SPathPlace); on arrival the reaction becomes a Guard reaction).
BEGIN_SCRIPT_COMMAND( UnitSetRetreatLogic, "us" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pAI ) && IsValid( pWaypoint ) )
			pAI->SetReaction( NAI::CreateAIRetreatReaction( pAI, pWaypoint->pos.p ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb380 ("u"): install the panic/fear reaction -- a unit that flees a known enemy toward cover
// (CreateAIFearReaction(ai, /*bUseCover*/true, /*bRoaming*/false, /*nRadius*/0)). UnitSetFearLogic and
// UnitSetPanicLogic are byte-identical in retail (only the registered command name differs).
BEGIN_SCRIPT_COMMAND( UnitSetFearLogic, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( NAI::CreateAIFearReaction( pAI, true, false, 0 ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb4d0 ("u"): identical to UnitSetFearLogic (same CreateAIFearReaction args in retail).
BEGIN_SCRIPT_COMMAND( UnitSetPanicLogic, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( NAI::CreateAIFearReaction( pAI, true, false, 0 ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb620 ("un"): the civilian variant -- no cover, and when calm it roams the arg1 radius
// (CreateAIFearReaction(ai, /*bUseCover*/false, /*bRoaming*/true, /*nRadius*/luaParams[1].n)).
BEGIN_SCRIPT_COMMAND( UnitSetCivilianLogic, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( NAI::CreateAIFearReaction( pAI, false, true, luaParams[ 1 ].n ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2eb780 ("u"): hand the unit to the mission script -- install the script reaction, which each turn
// (re)installs a CAIScriptLogic that fires the "OnUnitNeedCommand" lua hook + waits ~5 segments for the
// script to command the unit before surrendering the turn.
BEGIN_SCRIPT_COMMAND( UnitSetScriptLogic, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
			pAI->SetReaction( NAI::CreateAIScriptReaction( pAI ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}