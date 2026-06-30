#include "stdafx.h"
/*#include "A5Script.h"
#include "scriptCommon.h"
//
#include "scriptVector.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 gVecThis;
//
int luaVec( lua_State* state )	// Args:none 
{
	Script script(state);
	script.NewTable();
	script.SetTag( tagVec );
	return 1;
}
void SetVec( Script &script, const CVec3 &vec )
{
	Script::Object o = script.GetTopObject();
	if ( o.Tag() == tagVec )
	{
		o.SetNumber( "x", vec.x );
		o.SetNumber( "y", vec.y );
		o.SetNumber( "z", vec.z );
	}
}
CVec3 ToVec( Script::Object &o )
{
	if ( o.Tag() != tagVec )
		return CVec3(0,0,0);
	return CVec3( o.GetByName("x").GetNumber(), o.GetByName("y").GetNumber(), o.GetByName("z").GetNumber() );
}
int tagVecAdd(lua_State* state)
{
	Script script(state);
	if ( !script.CheckArgs( "c7c7", "Vec::Add", &vector<SLuaParams>() ) )
		return 0;
	CVec3 vOrig = ToVec( script.GetObject(1) );
	Script::Object o = script.GetObject(2);
	ASSERT ( o.Tag() == tagVec );
	vOrig += ToVec(o);
	luaVec( script.GetState() );
	SetVec( script, vOrig );
	return 1;
}
int tagVecSub(lua_State* state)
{
	Script script(state);
	if ( !script.CheckArgs( "c7c7", "Vec::Sub", &vector<SLuaParams>() ) )
		return 0;
	CVec3 vOrig = ToVec( script.GetObject(1) );
	Script::Object o = script.GetObject(2);
	ASSERT ( o.Tag() == tagVec );
	vOrig -= ToVec(o);
	luaVec( script.GetState() );
	SetVec( script, vOrig );
	return 1;
}
int tagVecMul(lua_State* state)
{
	Script script(state);
	if ( !script.CheckArgs( "c7n", "Vec::Mul", &vector<SLuaParams>() ) )
		return 0;
	CVec3 vOrig = ToVec( script.GetObject(1) );
	vOrig *= script.GetObject(2).GetNumber();
	luaVec( script.GetState() );
	SetVec( script, vOrig );
	return 1;
}
int tagVecDiv(lua_State* state)
{
	Script script(state);
	if ( !script.CheckArgs( "c7n", "Vec::Div", &vector<SLuaParams>() ) )
		return 0;
	CVec3 vOrig = ToVec( script.GetObject(1) );
	vOrig /= script.GetObject(2).GetNumber();
	luaVec( script.GetState() );
	SetVec( script, vOrig );
	return 1;
}
int tagVecLen(lua_State* state)
{
	Script script(state);
	script.PushNumber( fabs(gVecThis) );
	return 1;
}
int tagVecMemb( lua_State* state )
{
	Script script(state);
	if ( !script.CheckArgs( "c7s", "tagVecIndex", &vector<SLuaParams>() ) )
		return 0;
	string str = script.GetObject(2).GetString();
	if ( str == "len" )
	{
		gVecThis = ToVec( script.GetObject(1) );
		script.PushCFunction( tagVecLen );
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}*/