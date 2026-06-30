#ifndef __SCRIPTUI_H_
#define __SCRIPTUI_H_
//
// scriptUI -- the script-UI property bridge (LUA convergence). Declarations of the 7 registered global
// lua C-functions; the implementation (+ the absent infra: SRegProperty/GetRegPropMap/ShowPropertyError/
// GetChildByPath/CheckProperty + the 17 property thunks) lives in scriptUI.cpp, which owns the heavy UI
// include chain (Interface.h et al). Include this from ScriptFunctions.cpp for the pRegList references.
//
namespace NScript
{
int luaCreateWindow( lua_State* pState );		// "sonnnnsb[true]b[true]" -- create a typed window
int luaGetWindow( lua_State* pState );			// "s" -- find a window by dotted id path
int luaWindowGetProperty( lua_State* pState );	// get-dispatcher (window, propName)
int luaWindowSetProperty( lua_State* pState );	// set-dispatcher (window, propName, value)
int luaButtonGetState( lua_State* pState );		// "os" -- resolve a CButton state window
int luaButtonCreateState( lua_State* pState );	// "os" -- create a CButton state window
int luaGetCursorPos( lua_State* pState );		// "" -- { x=, y= } from the interface cursor
int luaGetUITime( lua_State* pState );			// "" -- the interface UI ms clock (CInterface::sLastTime)
// Set windowGet/SetProperty as the tagLuaWindow gettable/settable tag methods (lua `window.x` syntax).
// Call once at script init, after the window tag is registered.
void RegisterScriptUITagMethods( Script *scr );
}
//
#endif // __SCRIPTUI_H_
