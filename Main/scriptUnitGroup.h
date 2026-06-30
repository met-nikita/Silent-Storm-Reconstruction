#ifndef __SCRIPTUNITGROUP_H_
#define __SCRIPTUNITGROUP_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( CreateGroup );
DECLARE_SCRIPT_COMMAND( GetGroup );
DECLARE_SCRIPT_COMMAND( GroupGetID );
DECLARE_SCRIPT_COMMAND( GroupAddUnit );
DECLARE_SCRIPT_COMMAND( GroupRemoveUnit );
DECLARE_SCRIPT_COMMAND( GroupGetSize );
DECLARE_SCRIPT_COMMAND( GroupGetUnit );
DECLARE_SCRIPT_COMMAND( GroupIsContainUnit );
DECLARE_SCRIPT_COMMAND( GroupMoveToWaypoint );
DECLARE_SCRIPT_COMMAND( GroupGetVisible );
DECLARE_SCRIPT_COMMAND( GroupCheat );
DECLARE_SCRIPT_COMMAND( GroupAddGroup );
DECLARE_SCRIPT_COMMAND( GroupGetCross );
DECLARE_SCRIPT_COMMAND( PlayerGetUnits );
DECLARE_SCRIPT_COMMAND( PlayerGetUnitsEx );
// --- LUA convergence (player-money) ---
DECLARE_SCRIPT_COMMAND( PlayerGetMoney );
DECLARE_SCRIPT_COMMAND( PlayerGiveMoney );
DECLARE_SCRIPT_COMMAND( PlayerTakeMoney );
// --- LUA convergence (turn handoff) ---
DECLARE_SCRIPT_COMMAND( PlayerGiveTurn );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif __SCRIPTUNITGROUP_H_