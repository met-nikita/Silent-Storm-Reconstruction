#include "StdAfx.h"
// ============================================================================
//  scriptUI -- the script-UI property bridge (LUA convergence)
//
//  Reconstructs the release `scriptUI.obj` family: the layer that lets a lua UI
//  script create/find windows and read/write their named properties. Ported 1:1
//  from the decoded retail bodies (decomp src/s2_scriptui.h) onto the REAL
//  dev engine types -- NUI::CWindow/CText/CImage/CModel/CButton/CInterface and
//  the real csScript log stream -- so NO hooks are needed (every seam the
//  answer-key modelled with a pfn* is a real engine call here).
//
//  Registered global lua C-functions (the 7 pRegList entries, declared in
//  scriptUI.h, registered in ScriptFunctions.cpp):
//    CreateWindow  GetWindow  windowGetProperty  windowSetProperty
//    ButtonGetState  ButtonCreateState  GetCursorPos
//
//  Internal machinery reconstructed (the "absent infra" from the worklist):
//    SRegProperty / GetRegPropMap   -- the registered-property table (17 props)
//    ShowPropertyError              -- the red csScript diagnostic line
//    GetChildByPath                 -- dotted child-id walk
//    CheckProperty                  -- the get/set lua call-frame validator
//    + the 17 property get/set thunks (x/y/width/height/visible/enabled/
//      topmost/bottommost/transparent/text/image/state/ismousecover/parent/
//      onmessage/show/color)
//
//  This TU owns the heavy UI include chain (mirrors UICommCtrls.cpp) so the rest
//  of the script layer stays UI-free.
//
//  DEV DELTAS (faithful reconciles, documented):
//   * retail's 'o' arg-spec (generic userdata parent) was absent from
//     Script::CheckArgs -> added (case 'o' stores raw lua_touserdata).
//   * window userdata tag == retail's 8 (tagLuaWindow, a third RegisterNewTag).
//   * `onmessage` needs the lua callinfo C-API (lua_tocallinfo/pushcallinfo),
//     ABSENT in this lua build -> both onmessage thunks report "unsupported"
//     (so CWindow's save format is untouched -- no eventsMap member added).
//   * text property: retail SetText(GetDBString(s)); dev GetDBString takes an
//     int id -> SetText(GetDBString(atoi(s))). image likewise GetUITexture(atoi).
//   * CScript::pInterface is never assigned in dev (no lua-driven screen yet),
//     so GetWindow/GetCursorPos root at null and no-op gracefully until a
//     UI-script context wires it. CreateWindow (explicit parent) is fully live.
// ============================================================================
//
#include "Gfx.h"
#include "GfxBuffers.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "..\Misc\StrProc.h"			// NStr::ToAscii
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"		// NDb::GetUITexture
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"					// NUI::SPoint/SRect + NUI::CWindow/CText/CImage/CModel/CButton/CInterface, NUI::GetDBString
#include "..\MiscDll\LogStream.h"		// csScript, CC_RED, endl
//
#include "A5Script.h"					// NScript::CScript, GetScript, Script (lua stack)
#include "scriptCommon.h"				// luaPrepareData, BEGIN_SCRIPT_COMMAND, SLuaParams
#include "scriptPtr.h"					// tagLuaWindow (lua.h), luaGetPtr
#include "scriptUI.h"
//
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Registered property accessor + table. Each property exposes a get and a set thunk sharing one
// signature: (script, window, propName, valueStackIdx). Getters push their result and return the lua
// result count; setters read the value at lua stack index `nValIdx` (== 3, the windowSetProperty value
// slot) and return 0. This mirrors the retail SRegProperty { name; get; set } table built by
// GetRegPropMap @0x2f46e0.
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef int (*PFN_RegProp)( CScript *pScript, NUI::CWindow *pWindow, const string &szName, int nValIdx );
struct SRegProperty
{
	const char	*name;
	PFN_RegProp	get;
	PFN_RegProp	set;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// x86 fistp == round-to-nearest-even; nearbyint honours the default rounding mode.
static inline int ScriptUI_Round( double v )		{ return (int)nearbyint( v ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::ShowPropertyError @0x2f3740 -- one red, newline-terminated diagnostic into csScript.
//   <RED>ERROR: Property <get|set>failed! Index = <propIndex> Window = <id> error: <errorText>
// Always returns 0 (matches retail).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int ShowPropertyError( NUI::CWindow *pWindow, const string &szPropIndex, bool bIsGet,
	const string &szErrorText )
{
	csScript << CC_RED << "ERROR: Property " << ( bIsGet ? "get " : "set " )
		<< "failed! Index = " << szPropIndex
		<< " Window = " << ( IsValid( pWindow ) ? pWindow->GetWindowID() : string( "<null>" ) )
		<< " error: " << szErrorText << endl;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::GetChildByPath @0x2f3db0 -- walk the child tree by a dotted id path. Empty path -> root.
////////////////////////////////////////////////////////////////////////////////////////////////////
static NUI::CWindow* GetChildByPath( NUI::CWindow *pRoot, const string &szPath )
{
	if ( szPath.empty() )
		return pRoot;
	//
	string::size_type nPos = 0;
	for ( ;; )
	{
		string::size_type nDot = szPath.find( '.', nPos );
		string szSeg = ( nDot == string::npos ) ? szPath.substr( nPos ) : szPath.substr( nPos, nDot - nPos );
		//
		if ( pRoot == 0 )
			return 0;
		pRoot = pRoot->GetChildByID( szSeg );
		if ( pRoot == 0 )
			return 0;
		//
		if ( nDot == string::npos )
			break;
		nPos = nDot + 1;
	}
	return pRoot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::CheckProperty @0x2f4620 -- validate the lua get/set call frame for a window property.
//   [1] = window userdata (tagLuaWindow), [2] = property-name string, [3] = value (set only).
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CheckProperty( lua_State *pState, bool bIsGet, CScript **ppScript, NUI::CWindow **ppWindow,
	string *pszName )
{
	CScript *pScript = GetScript();
	if ( pScript == 0 )
		return false;
	if ( lua_gettop( pState ) < ( bIsGet ? 2 : 3 ) )
		return false;
	if ( !lua_isstring( pState, 2 ) )
		return false;
	if ( lua_tag( pState, 1 ) != tagLuaWindow )
		return false;
	CDynamicCast<NUI::CWindow> pWindow( lua_touserdata( pState, 1 ) );
	if ( !pWindow )
		return false;
	//
	*pszName = lua_tostring( pState, 2 );
	*ppWindow = pWindow;
	*ppScript = pScript;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared helpers for the property thunks.
////////////////////////////////////////////////////////////////////////////////////////////////////
// Decode a lua number argument into a style on/off flag: isnumber && round(value) != 0.
static bool ScriptUI_LuaToStyleBool( CScript *pScript, int nIdx )
{
	Script::Object o = pScript->GetObject( nIdx );
	return o.IsNumber() && ScriptUI_Round( o.GetNumber() ) != 0;
}
// Read t[k] (1-based) from the table at stack index nTableIdx as a number (luaSetcolor).
static double ScriptUI_ReadTableNumber( CScript *pScript, int nTableIdx, int k )
{
	lua_rawgeti( pScript->m_state, nTableIdx, k );		// pushes t[k] on top
	double v = lua_tonumber( pScript->m_state, lua_gettop( pScript->m_state ) );
	pScript->Pop( 1 );
	return v;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Geometry getters (push one coordinate as a lua number; ret 1).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetx( CScript *s, NUI::CWindow *w, const string &, int )
	{ s->PushNumber( w->GetPosition().x ); return 1; }
static int luaGety( CScript *s, NUI::CWindow *w, const string &, int )
	{ s->PushNumber( w->GetPosition().y ); return 1; }
static int luaGetwidth( CScript *s, NUI::CWindow *w, const string &, int )
	{ s->PushNumber( w->GetSize().x ); return 1; }
static int luaGetheight( CScript *s, NUI::CWindow *w, const string &, int )
	{ s->PushNumber( w->GetSize().y ); return 1; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Geometry setters (round-to-even one coordinate, keep the other; ret 0). Non-number -> error.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaSetx( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	Script::Object o = s->GetObject( idx );
	if ( !o.IsNumber() )
		return ShowPropertyError( w, name, false, "incorrect type" );
	NUI::SPoint p = w->GetPosition();
	w->SetPosition( NUI::SPoint( ScriptUI_Round( o.GetNumber() ), p.y ) );
	return 0;
}
static int luaSety( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	Script::Object o = s->GetObject( idx );
	if ( !o.IsNumber() )
		return ShowPropertyError( w, name, false, "incorrect type" );
	NUI::SPoint p = w->GetPosition();
	w->SetPosition( NUI::SPoint( p.x, ScriptUI_Round( o.GetNumber() ) ) );
	return 0;
}
static int luaSetwidth( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	Script::Object o = s->GetObject( idx );
	if ( !o.IsNumber() )
		return ShowPropertyError( w, name, false, "incorrect type" );
	NUI::SPoint sz = w->GetSize();
	w->SetSize( NUI::SPoint( ScriptUI_Round( o.GetNumber() ), sz.y ) );
	return 0;
}
static int luaSetheight( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	Script::Object o = s->GetObject( idx );
	if ( !o.IsNumber() )
		return ShowPropertyError( w, name, false, "incorrect type" );
	NUI::SPoint sz = w->GetSize();
	w->SetSize( NUI::SPoint( sz.x, ScriptUI_Round( o.GetNumber() ) ) );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Style-flag getters/setters over the real CWindow::nStyle bitmask (GetStyle/SetStyle).
//   getter: GetStyle(flag) ? push 1.0 : pushnil; ret 1.   setter: SetStyle(flag, luaBool); ret 0.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int ScriptUI_PushStyle( CScript *s, NUI::CWindow *w, int nFlag )
{
	if ( w->GetStyle( nFlag ) )	s->PushNumber( 1.0 );
	else						s->PushNil();
	return 1;
}
static int luaGetvisible( CScript *s, NUI::CWindow *w, const string &, int )	{ return ScriptUI_PushStyle( s, w, NUI::STYLE_VISIBLE ); }
static int luaSetvisible( CScript *s, NUI::CWindow *w, const string &, int i )	{ w->SetStyle( NUI::STYLE_VISIBLE, ScriptUI_LuaToStyleBool( s, i ) ); return 0; }
static int luaGetenabled( CScript *s, NUI::CWindow *w, const string &, int )	{ return ScriptUI_PushStyle( s, w, NUI::STYLE_ENABLED ); }
static int luaSetenabled( CScript *s, NUI::CWindow *w, const string &, int i )	{ w->SetStyle( NUI::STYLE_ENABLED, ScriptUI_LuaToStyleBool( s, i ) ); return 0; }
static int luaGettopmost( CScript *s, NUI::CWindow *w, const string &, int )	{ return ScriptUI_PushStyle( s, w, NUI::STYLE_TOPMOST ); }
static int luaSettopmost( CScript *s, NUI::CWindow *w, const string &, int i )	{ w->SetStyle( NUI::STYLE_TOPMOST, ScriptUI_LuaToStyleBool( s, i ) ); return 0; }
static int luaGetbottommost( CScript *s, NUI::CWindow *w, const string &, int )	{ return ScriptUI_PushStyle( s, w, NUI::STYLE_BOTTOMMOST ); }
static int luaSetbottommost( CScript *s, NUI::CWindow *w, const string &, int i ){ w->SetStyle( NUI::STYLE_BOTTOMMOST, ScriptUI_LuaToStyleBool( s, i ) ); return 0; }
static int luaGettransparent( CScript *s, NUI::CWindow *w, const string &, int )	{ return ScriptUI_PushStyle( s, w, NUI::STYLE_TRANSPARENT ); }
static int luaSettransparent( CScript *s, NUI::CWindow *w, const string &, int i ){ w->SetStyle( NUI::STYLE_TRANSPARENT, ScriptUI_LuaToStyleBool( s, i ) ); return 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// text (CText): get -> push ascii of GetText(); set -> SetText(GetDBString(atoi(s))).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGettext( CScript *s, NUI::CWindow *w, const string &name, int )
{
	CDynamicCast<NUI::CText> t( w );
	if ( !t )
		return ShowPropertyError( w, name, false, "invalid window type" );
	s->PushString( NStr::ToAscii( t->GetText() ).c_str() );
	return 1;
}
static int luaSettext( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	CDynamicCast<NUI::CText> t( w );
	Script::Object o = s->GetObject( idx );
	if ( t && o.IsString() )
	{
		t->SetText( NUI::GetDBString( atoi( o.GetString() ) ) );
		return 0;
	}
	return ShowPropertyError( w, name, false, "invalid window type or parameter" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// image (CImage): get -> unsupported; set -> SetImage(GetUITexture(atoi(s))).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetimage( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
static int luaSetimage( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	CDynamicCast<NUI::CImage> im( w );
	Script::Object o = s->GetObject( idx );
	if ( im && o.IsString() )
	{
		im->SetImage( NDb::GetUITexture( atoi( o.GetString() ) ) );
		return 0;
	}
	return ShowPropertyError( w, name, false, "invalid window type or parameter" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// state (CButton): get -> push "%d" of GetActiveState(); set -> SetActiveState(atoi(s)).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetstate( CScript *s, NUI::CWindow *w, const string &name, int )
{
	CDynamicCast<NUI::CButton> b( w );
	if ( !b )
		return ShowPropertyError( w, name, false, "invalid window type" );
	char buf[ 16 ];
	sprintf( buf, "%d", b->GetActiveState() );
	s->PushString( buf );
	return 1;
}
static int luaSetstate( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	CDynamicCast<NUI::CButton> b( w );
	Script::Object o = s->GetObject( idx );
	if ( b && o.IsString() )
	{
		b->SetActiveState( atoi( o.GetString() ) );
		return 0;
	}
	return ShowPropertyError( w, name, false, "invalid window type or parameter" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ismousecover (CButton): get -> IsMouseCover() ? push 1.0 : pushnil; set -> unsupported.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetismousecover( CScript *s, NUI::CWindow *w, const string &name, int )
{
	CDynamicCast<NUI::CButton> b( w );
	if ( !b )
		return ShowPropertyError( w, name, false, "invalid window type" );
	if ( b->IsMouseCover() )	s->PushNumber( 1.0 );
	else					s->PushNil();
	return 1;
}
static int luaSetismousecover( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// parent: get -> push GetParent() as a window userdata; set -> unsupported.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetparent( CScript *s, NUI::CWindow *w, const string &, int )
	{ s->PushUserTag( w->GetParent(), tagLuaWindow ); return 1; }
static int luaSetparent( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// onmessage: needs the lua callinfo C-API (lua_tocallinfo/pushcallinfo) which this lua build lacks ->
// both report "unsupported" (so CWindow's save format is left untouched).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetonmessage( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
static int luaSetonmessage( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// show: get -> unsupported; set -> ShowWindow(SWTYPE_SHOW) (ignores the lua value, like retail).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetshow( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
static int luaSetshow( CScript *, NUI::CWindow *w, const string &, int )
	{ w->ShowWindow( NUI::SWTYPE_SHOW ); return 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// color (CImage): get -> unsupported; set -> SetColor from a {R,G,B,A} table (each *255, round-even).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaGetcolor( CScript *, NUI::CWindow *w, const string &name, int )
	{ return ShowPropertyError( w, name, false, "unsupported" ); }
static int luaSetcolor( CScript *s, NUI::CWindow *w, const string &name, int idx )
{
	CDynamicCast<NUI::CImage> im( w );
	Script::Object o = s->GetObject( idx );
	if ( im && o.IsTable() )
	{
		NGfx::SPixel8888 px(
			(unsigned char)( ScriptUI_Round( ScriptUI_ReadTableNumber( s, idx, 1 ) * 255.0 ) & 0xff ),	// R
			(unsigned char)( ScriptUI_Round( ScriptUI_ReadTableNumber( s, idx, 2 ) * 255.0 ) & 0xff ),	// G
			(unsigned char)( ScriptUI_Round( ScriptUI_ReadTableNumber( s, idx, 3 ) * 255.0 ) & 0xff ),	// B
			(unsigned char)( ScriptUI_Round( ScriptUI_ReadTableNumber( s, idx, 4 ) * 255.0 ) & 0xff ) );	// A
		im->SetColor( px );
		return 0;
	}
	return ShowPropertyError( w, name, false, "invalid window type or parameter" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::GetRegPropMap @0x2f46e0 -- the registered-property table (17 entries). A linear scan over
// 17 fixed entries reproduces the retail hash_map lookup behaviour (keys are the lowercased names).
////////////////////////////////////////////////////////////////////////////////////////////////////
static const SRegProperty* GetRegPropMap( int *pnCount )
{
	static const SRegProperty s_props[] =
	{
		{ "x",				luaGetx,			luaSetx				},
		{ "y",				luaGety,			luaSety				},
		{ "width",			luaGetwidth,		luaSetwidth			},
		{ "height",			luaGetheight,		luaSetheight		},
		{ "visible",		luaGetvisible,		luaSetvisible		},
		{ "enabled",		luaGetenabled,		luaSetenabled		},
		{ "topmost",		luaGettopmost,		luaSettopmost		},
		{ "bottommost",		luaGetbottommost,	luaSetbottommost	},
		{ "transparent",	luaGettransparent,	luaSettransparent	},
		{ "text",			luaGettext,			luaSettext			},
		{ "image",			luaGetimage,		luaSetimage			},
		{ "state",			luaGetstate,		luaSetstate			},
		{ "ismousecover",	luaGetismousecover,	luaSetismousecover	},
		{ "parent",			luaGetparent,		luaSetparent		},
		{ "onmessage",		luaGetonmessage,	luaSetonmessage		},
		{ "show",			luaGetshow,			luaSetshow			},
		{ "color",			luaGetcolor,		luaSetcolor			},
	};
	if ( pnCount )
		*pnCount = sizeof( s_props ) / sizeof( s_props[ 0 ] );
	return s_props;
}
static const SRegProperty* FindRegProp( const string &szName )
{
	int nCount = 0;
	const SRegProperty *pProps = GetRegPropMap( &nCount );
	for ( int i = 0; i < nCount; ++i )
		if ( szName == pProps[ i ].name )
			return &pProps[ i ];
	return 0;
}
static string ScriptUI_ToLower( const string &szIn )
{
	string sz = szIn;
	for ( string::size_type i = 0; i < sz.size(); ++i )
		sz[ i ] = (char)tolower( (unsigned char)sz[ i ] );
	return sz;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::luaWindowGetProperty @0x2f47c0 / luaWindowSetProperty @0x2f48b0 -- the get/set dispatchers.
//   validate the frame -> lowercase the name -> look up the property -> invoke its get/set thunk.
// (Registered as plain global functions, exactly as in retail's pRegList.)
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaWindowGetProperty( lua_State *pState )
{
	CScript *pScript = 0;
	NUI::CWindow *pWindow = 0;
	string szName;
	if ( !CheckProperty( pState, true, &pScript, &pWindow, &szName ) )
		return 0;
	//
	const SRegProperty *p = FindRegProp( ScriptUI_ToLower( szName ) );
	if ( p == 0 || p->get == 0 )
		return 0;
	return p->get( pScript, pWindow, szName, 0 );
}
int luaWindowSetProperty( lua_State *pState )
{
	CScript *pScript = 0;
	NUI::CWindow *pWindow = 0;
	string szName;
	if ( !CheckProperty( pState, false, &pScript, &pWindow, &szName ) )
		return 0;
	//
	const SRegProperty *p = FindRegProp( ScriptUI_ToLower( szName ) );
	if ( p == 0 || p->set == 0 )
		return 0;
	return p->set( pScript, pWindow, szName, 3 );		// the value lives at lua stack index 3
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::luaCreateWindow @0x2f4bf0 -- create a typed window under an explicit parent.
//   spec "sonnnnsb[true]b[true]": type, parent, x, y, w, h, id, visible, enabled.
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateWindow, "sonnnnsb[true]b[true]" );
	CDynamicCast<NUI::CWindow> pParent( luaParams[ 1 ].p );
	if ( !pParent || !IsValid( pParent ) )
	{
		csScript << CC_RED << "ERROR: Invalid parent object in CreateWindow" << endl;
		return 0;
	}
	//
	NUI::SWindowInfo wi;
	wi.nStyle = 0;
	wi.pParent = pParent;
	wi.szID = luaParams[ 6 ].s;
	wi.sPosition = NUI::SPoint( luaParams[ 2 ].n, luaParams[ 3 ].n );
	wi.sSize = NUI::SPoint( luaParams[ 4 ].n, luaParams[ 5 ].n );
	if ( luaParams[ 7 ].b )	wi.nStyle |= NUI::STYLE_VISIBLE;
	if ( luaParams[ 8 ].b )	wi.nStyle |= NUI::STYLE_ENABLED;
	//
	string szType = ScriptUI_ToLower( luaParams[ 0 ].s );
	NUI::CWindow *pNew = 0;
	if ( szType == "window" )		pNew = new NUI::CWindow( wi );
	else if ( szType == "image" )	pNew = new NUI::CImage( wi );
	else if ( szType == "text" )	pNew = new NUI::CText( wi );
	else if ( szType == "model" )	pNew = new NUI::CModel( wi );
	else if ( szType == "button" )	pNew = new NUI::CButton( wi );
	else							return 0;					// unknown type -> no window
	//
	CPtr<NUI::CWindow> hold( pNew );							// retail's CPtr= on the new window
	pScript->PushUserTag( pNew, tagLuaWindow );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::luaGetWindow @0x2f4ad0 -- find a window by dotted id path under the script's interface.
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetWindow, "s" );
	NUI::CWindow *pWin = GetChildByPath( pScript->GetScriptInterface(), luaParams[ 0 ].s );
	if ( pWin == 0 )
		return 0;
	pScript->PushUserTag( pWin, tagLuaWindow );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared body for ButtonGetState @0x2f5130 / ButtonCreateState @0x2f52f0: resolve/create a CButton
// state window and push it (or nil) as a window userdata.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int luaButtonState_impl( vector<SLuaParams> &luaParams, CScript *pScript, const char *szErrLit,
	bool bCreate )
{
	CDynamicCast<NUI::CButton> b( luaParams[ 0 ].p );
	if ( !b || !IsValid( b ) )
	{
		csScript << CC_RED << szErrLit << endl;
		return 0;
	}
	int nID = atoi( luaParams[ 1 ].s.c_str() );
	NUI::CWindow *pStateWin = bCreate ? b->AddState( nID ) : b->GetState( nID );
	if ( !pStateWin || !IsValid( pStateWin ) )
		pScript->PushNil();
	else
		pScript->PushUserTag( pStateWin, tagLuaWindow );
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ButtonGetState, "os" );
	return luaButtonState_impl( luaParams, pScript, "ERROR: Invalid object in ButtonGetState", false );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ButtonCreateState, "os" );
	return luaButtonState_impl( luaParams, pScript, "ERROR: Invalid object in ButtonCreateState", true );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::luaGetCursorPos @0x2f4fb0 -- build { x=, y= } from the interface cursor point.
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetCursorPos, "" );
	Script::Object t = pScript->NewTable();
	NUI::CInterface *pIface = pScript->GetScriptInterface();
	NUI::SPoint c = IsValid( pIface ) ? pIface->GetCursorPos() : NUI::SPoint( 0, 0 );
	t.SetNumber( "x", c.x );
	t.SetNumber( "y", c.y );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScript::luaGetUITime @0x2f49b0 -- push the interface's UI millisecond clock (CInterface::sLastTime,
// retail @+0xb8, refreshed each Step). Pushed UNSIGNED-widened: retail's `if(n<0) n+=2^32` is the
// int->unsigned fixup the compiler emits for (double)(unsigned long); reproduced by casting the DWORD
// clock through unsigned long into the double PushNumber takes. Roots at the script interface, so it
// no-ops to 0 until a UI-script context wires CScript::pInterface (same as GetWindow/GetCursorPos).
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetUITime, "" );
	NUI::CInterface *pIface = pScript->GetScriptInterface();
	unsigned long nTime = IsValid( pIface ) ? (unsigned long)pIface->GetLastTime() : 0;
	pScript->PushNumber( (double)nTime );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScript::Get/SetScriptInterface -- the release CScript::pInterface as an NUI::CInterface*. Set by
// CMission to the in-mission HUD interface, so GetWindow/GetCursorPos/CreateWindow are LIVE in-mission.
// Defined here because the NUI types are complete in this TU.
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CInterface* CScript::GetScriptInterface()
{
	return CDynamicCast<NUI::CInterface>( (CObjectBase*)pInterface );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScript::SetScriptInterface( NUI::CInterface *pIface )
{
	pInterface = pIface;		// CPtr<CObjectBase> = CInterface* (upcast); a weak ref -- the mission owns it
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// RegisterScriptUITagMethods -- make the window property "leaf" accessors reachable via lua's `window.x`
// / `window.x = v` syntax (the retail wiring): set windowGetProperty / windowSetProperty as the
// tagLuaWindow gettable / settable tag methods. (Called once from A5Script.cpp SharedInit, after the
// window tag is registered.) Without this the thunks are reachable only through explicit
// windowGetProperty(w,"x") global calls.
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterScriptUITagMethods( Script *scr )
{
	scr->PushCFunction( luaWindowGetProperty );
	scr->SetTagMethod( tagLuaWindow, "gettable" );
	scr->PushCFunction( luaWindowSetProperty );
	scr->SetTagMethod( tagLuaWindow, "settable" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NScript
