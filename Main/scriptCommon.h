#ifndef __SCRIPTCOMMON_H_
#define __SCRIPTCOMMON_H_
//
union ULuaParams;
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaGetParamCount( lua_State *pState );
bool luaPrepareData( lua_State *pState, 
	string szFuncName, string szParams, CScript **ppScript, vector<SLuaParams> *pParams );
bool luaGetBool( const Script::Object &o );
void luaPushBool( lua_State *pState, bool bValue );
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECLARE_SCRIPT_COMMAND( Name )											\
int lua##Name( lua_State* pState );
#define BEGIN_SCRIPT_COMMAND( Name, Params )								\
int lua##Name( lua_State* pState )																			\
{																																				\
	ASSERT( pState != 0 );																								\
	if ( pState == 0 )																										\
		return 0;																														\
	CScript *pScript;																											\
	vector<SLuaParams> luaParams;																					\
	if ( !luaPrepareData( pState, #Name, Params, &pScript, &luaParams ) )	\
		return 0;																								
#define END_SCRIPT_COMMAND }
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( Out );
DECLARE_SCRIPT_COMMAND( Random );
DECLARE_SCRIPT_COMMAND( GetGlobalGameVar );
DECLARE_SCRIPT_COMMAND( SetGlobalGameVar );
DECLARE_SCRIPT_COMMAND( IsRealTime );
DECLARE_SCRIPT_COMMAND( PassCalcerIsActive );
DECLARE_SCRIPT_COMMAND( Explosion );
DECLARE_SCRIPT_COMMAND( GetTurn );
DECLARE_SCRIPT_COMMAND( LuaTest );
DECLARE_SCRIPT_COMMAND( Difficulty );
DECLARE_SCRIPT_COMMAND( IsEqual );
DECLARE_SCRIPT_COMMAND( TableGetSize );
DECLARE_SCRIPT_COMMAND( AttachEffectToWaypoint );
DECLARE_SCRIPT_COMMAND( AttachEffectToUnitBone );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTCOMMON_H_