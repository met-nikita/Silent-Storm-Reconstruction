#ifndef __SCRIPTOBJECT_H_
#define __SCRIPTOBJECT_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( GetObject );
DECLARE_SCRIPT_COMMAND( CreateObject );
DECLARE_SCRIPT_COMMAND( ObjectOpen );
DECLARE_SCRIPT_COMMAND( ObjectClose );
DECLARE_SCRIPT_COMMAND( ObjectIsOpened );
DECLARE_SCRIPT_COMMAND( ObjectDestroy );
DECLARE_SCRIPT_COMMAND( ObjectRemove );
DECLARE_SCRIPT_COMMAND( ObjectSetToWaypoint );
DECLARE_SCRIPT_COMMAND( ObjectPlayAnimation );
DECLARE_SCRIPT_COMMAND( ObjectIsAction );
DECLARE_SCRIPT_COMMAND( ObjectSetDestroyStage );
DECLARE_SCRIPT_COMMAND( ObjectGetName );
DECLARE_SCRIPT_COMMAND( ObjectCancelAction );
DECLARE_SCRIPT_COMMAND( GetItem );
DECLARE_SCRIPT_COMMAND( FindItem );
DECLARE_SCRIPT_COMMAND( ItemGetName );
DECLARE_SCRIPT_COMMAND( ItemRemove );
// --- LUA convergence batch 2 ---
DECLARE_SCRIPT_COMMAND( ObjectGetDestroyStage );
DECLARE_SCRIPT_COMMAND( ObjectGetHP );
DECLARE_SCRIPT_COMMAND( ObjectLockDoor );
DECLARE_SCRIPT_COMMAND( ObjectUnlockDoor );
DECLARE_SCRIPT_COMMAND( ItemUnload );
// --- LUA convergence batch 3 (dev-bug session 2026-07-03) ---
DECLARE_SCRIPT_COMMAND( ObjectPlaceInPocket );      // retail @0x2e9000
DECLARE_SCRIPT_COMMAND( ObjectRestoreFromPocket );  // retail @0x2e9130
DECLARE_SCRIPT_COMMAND( ItemSetToWaypoint );        // retail @0x2e86c0
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTOBJECT_H_