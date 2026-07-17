#include "stdafx.h"
#include "A5Script.h"
#include "scriptPtr.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wUnitGroup.h"
#include "wUnitServer.h"
#include "aiRoute.h"
#include "rpgUnitMission.h"
#include "rpgUnit.h"
#include "rpgGlobal.h"			// NRPG::CGlobalPlayer money accessors (Player*Money)
#include "wUnitCommands.h"
//
#include "scriptUnitGroup.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateGroup, "" )
	NWorld::CUnitGroup *pGroup = new NWorld::CUnitGroup();
	if ( IsValid( pGroup ) )
	{
		int nUnits = luaGetParamCount( pState );
		for ( int i = 1; i <= nUnits; ++i )
		{
			// retail luaCreateGroup @0x2ff240 NULL-guards lua_tostring: a non-string arg becomes an
			// EMPTY name whose lookup fails, so the unit is silently skipped. Scripts hit this path
			// live -- Common.l UnitCanSeeUnit does CreateGroup(UnitGetName(u2)), and UnitGetName of a
			// dead/removed unit is nil (GFirst repro: a big eng blast kills a trigger's unit, the
			// trigger thread resumes in CWorld::Segment and crashed here in string(NULL)).
			const char *pszName = pScript->GetObject( i ).GetString();
			string szName = pszName ? pszName : "";
			CPtr<NWorld::CUnitServer> pUS = pScript->pWorld->GetUnitServer( szName );
			if ( IsValid( pUS ) )
				pGroup->units.Add( pUS );
		}
		luaPushCObj( pState, pGroup );
	}
	else
		pScript->PushNil();
	//
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetGroup, "n" )
	int nGroupID = luaParams[ 0 ].n;
	NWorld::CUnitGroup *pGroup = pScript->pWorld->GetUnitGroup( nGroupID );
	if ( IsValid( pGroup ) )
		luaPushCPtr( pState, pGroup );
	else
	{
		char szGroupID[ 12 ];
		sprintf( szGroupID, "%d", nGroupID );
		ScriptWarning( string( "group number " ) + string( szGroupID ) + string( " not found" )  );
		pScript->PushNil();
	}
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetID, "u" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( IsValid( pGroup ) )
		pScript->PushNumber( pGroup->GetID() );
	else
		pScript->PushNumber( 0 );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupAddUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
		pGroup->units.Add( pUS );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupRemoveUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
		pGroup->units.Remove( pUS );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetSize, "u" )
	CDynamicCast<NWorld::CUnitGroup> pGroup(luaParams[0].p);
	if (pGroup)
		pScript->PushNumber( pGroup->units.GetSize() );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetUnit, "un" )
	CDynamicCast<NWorld::CUnitGroup> pGroup(luaParams[0].p);
	if (pGroup)
	{
		int n = luaParams[ 1 ].n;
//		ASSERT( n >= 0 && n < pGroup->units.GetSize() );
		if ( n >= 0 && n < pGroup->units.GetSize() )
		{
			NWorld::CUnitServer *pUS = pGroup->units[ n ];
			if ( IsValid( pUS ) )
			{
				luaPushCPtr( pState, pUS );
				return 1;
			}
		}
		else
		{
			char szIndex[12];
			sprintf( szIndex, "%d", n );
			ScriptWarning( string( "Unit index in group is out of range ( " ) + string( szIndex ) + string( " )" ) );
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupIsContainUnit, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 1 ].p );
	if ( IsValid( pGroup ) && IsValid( pUS ) )
	{
		if ( pGroup->units.IsContain( pUS ) )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupMoveToWaypoint, "us" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( !IsValid( pGroup ) )
		return 0;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
	if ( !IsValid( pWaypoint ) )
		return 0;
	//
	NAI::SPosition pos = pWaypoint->pos;
	vector< NAI::SPosition > unitPlaces;
	for ( int i = 0; i < pGroup->units.GetSize(); ++i )
		unitPlaces.push_back( pGroup->units[ i ]->GetPosition().pos );
	pScript->pWorld->GetPathNetwork()->FormationMoveTo( &unitPlaces, pos  );
	//
	for ( int i = 0; i < pGroup->units.GetSize(); ++i )
	{
		pGroup->units[ i ]->Do( 
			new NWorld::CCmdSetCommand( pGroup->units[ i ], new NWorld::CCmdPath( unitPlaces[ i ] ) ) );
		pGroup->units[ i ]->Do( 
			new NWorld::CCmdSetCommand( pGroup->units[ i ], new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetVisible, "u" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( IsValid( pGroup ) )
	{
		CObj<NWorld::CUnitGroup> pVisible = pScript->pWorld->CreateUnitGroup();
		for ( int u = 0; u < pGroup->units.GetSize(); ++u )
		{
			CPtr<NWorld::CUnitServer> pUS = pGroup->units[ u ];
			for ( list< CPtr<NWorld::CUnitServer> >::const_iterator 
				i = pUS->GetTBSVisible().begin(); i != pUS->GetTBSVisible().end(); ++i )
					if ( !pVisible->units.IsContain( *i ) )
						pVisible->units.Add( i->GetPtr() );
		}
		luaPushCObj( pState, pVisible );
		return 1;
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupCheat, "unb" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );
	if ( !IsValid( pGroup ) )
		return 0;
	//
	int nCheat = luaParams[ 1 ].n;
	bool bEnable = luaParams[ 2 ].b;
	for ( int u = 0; u < pGroup->units.GetSize(); ++u )
		pGroup->units[u]->GetUnitRPG()->GetRPGUnit()->SetCheat( nCheat, bEnable );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupAddGroup, "uu" )
	CDynamicCast<NWorld::CUnitGroup> pLeft( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitGroup> pRight( luaParams[ 1 ].p );
	if ( !IsValid( pLeft ) || !IsValid( pRight ) )
	{
		pScript->PushNil();
		return 1;
	}
	//
	CPtr<NWorld::CUnitGroup> pGroup = new NWorld::CUnitGroup();
	for ( int i = 0; i < pLeft->units.GetSize(); ++i )
		pGroup->units.Add( pLeft->units[ i ] );
	for ( int i = 0; i < pRight->units.GetSize(); ++i )
		pGroup->units.Add( pRight->units[ i ] );
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GroupGetCross, "uu" )
	CPtr<NWorld::CUnitGroup> pGroup = new NWorld::CUnitGroup();
	CDynamicCast<NWorld::CUnitGroup> pLeft( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitGroup> pRight( luaParams[ 1 ].p );
	if ( IsValid( pLeft ) && IsValid( pRight ) )
	{
		for ( int i = 0; i < pLeft->units.GetSize(); ++i )
		{
			if ( pRight->units.IsContain( pLeft->units[ i ] ) )
				pGroup->units.Add( pLeft->units[ i ] );
		}
	}
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( PlayerGetUnits, "n" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	CPtr<NWorld::CUnitGroup> pResult = new NWorld::CUnitGroup();
	if ( IsValid( pPlayer ) )
	{
		vector< CPtr<NWorld::CUnit> > units;
		pPlayer->GetUnits( &units );
		for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		{
			CDynamicCast<NWorld::CUnitServer> pUS(i->GetPtr());
			if (pUS)
				pResult->units.Add( pUS );
		}
	}
	//
	luaPushCObj( pState, pResult );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fffe0: like PlayerGetUnits, but the second arg selects WHICH player with that scenario id
// (GetPlayerByID(id, index)) -- several CPlayer objects can share a scenario id. Pushes the indexed
// player's units as a CUnitGroup; an empty group when no such player exists.
BEGIN_SCRIPT_COMMAND( PlayerGetUnitsEx, "nn" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n, luaParams[ 1 ].n );
	CPtr<NWorld::CUnitGroup> pResult = new NWorld::CUnitGroup();
	if ( IsValid( pPlayer ) )
	{
		vector< CPtr<NWorld::CUnit> > units;
		pPlayer->GetUnits( &units );
		for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
		{
			CDynamicCast<NWorld::CUnitServer> pUS( i->GetPtr() );
			if ( pUS )
				pResult->units.Add( pUS );
		}
	}
	//
	luaPushCObj( pState, pResult );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fec30: push the player's money, or push nothing (return 0) when the player is missing.
BEGIN_SCRIPT_COMMAND( PlayerGetMoney, "n" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	if ( IsValid( pPlayer ) && IsValid( pPlayer->GetGlobalPlayer() ) )
	{
		pScript->PushNumber( pPlayer->GetGlobalPlayer()->GetMoney() );
		return 1;
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fed90: add money to the player (unclamped).
BEGIN_SCRIPT_COMMAND( PlayerGiveMoney, "nn" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	if ( IsValid( pPlayer ) && IsValid( pPlayer->GetGlobalPlayer() ) )
		pPlayer->GetGlobalPlayer()->GiveMoney( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2feec0: subtract money from the player (clamped to >= 0).
BEGIN_SCRIPT_COMMAND( PlayerTakeMoney, "nn" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	if ( IsValid( pPlayer ) && IsValid( pPlayer->GetGlobalPlayer() ) )
		pPlayer->GetGlobalPlayer()->TakeMoney( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fea90 ("n"): hand the active turn to the player with scenario id n. Retail walks the player's
// first unit -> its owner to reach the registered TBS CPlayer (its GetPlayerByID returns a scenario wrapper);
// this dev's GetPlayerByID already returns the registered TBS CPlayer, so route straight to
// CWorld/CTBSWorld::GivePlayerTurn (which resets the situation + starts that player's fresh turn).
BEGIN_SCRIPT_COMMAND( PlayerGiveTurn, "n" )
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 0 ].n );
	if ( IsValid( pPlayer ) )
		pScript->pWorld->GivePlayerTurn( pPlayer );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}