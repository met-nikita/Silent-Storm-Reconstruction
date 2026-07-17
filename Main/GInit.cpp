#include "StdAfx.h"
#include "GInit.h"
#include "Gfx.h"
#include "GfxRender.h"
#include "..\MiscDll\Commands.h"

namespace NGScene
{
static int nDepthTexResolution = 512, nCLSkyTextures = 1, nCLCubeResolution = 16;
static bool bCanCacheLighting = false, bCanCalcAmbient = false, bCanRenderShadows = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetDepthTexResolution() { return nDepthTexResolution; }
int GetCLSkyTexturesNumber() { return nCLSkyTextures; }
int GetCLCubeResolution() { return nCLCubeResolution; }
bool CanRenderShadows() { return bCanRenderShadows; }
bool CanCacheLighting() { return bCanCacheLighting; }
bool CanCalcAmbient() { return bCanCalcAmbient; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// WIDESCREEN (Sentinels design): gfx_resolution accepts a "WxH" string (Sentinels stores it that
// way -- its "gfx_resolution" @rdata 0x90b8d0 sits in a cmdline map of L"WxH" values, e.g.
// "-1280" -> L"1280x960"); a plain number keeps the legacy width whitelist.
void GetConfiguredVideoMode( int *pModeX, int *pModeY )
{
	int nModeX = 1024, nModeY = 768;
	NGlobal::CValue sValue = NGlobal::GetVar( "gfx_resolution", 1024 );

	int nW = 0, nH = 0;
	if ( swscanf( sValue.GetString().c_str(), L"%dx%d", &nW, &nH ) == 2 && nW > 0 && nH > 0 )
	{
		nModeX = nW; nModeY = nH;
	}
	// legacy width whitelist -- retail @0x1292e0 (note 1280 maps to 1280x960, NOT 1280x1024)
	else if ( sValue.GetFloat() == 320 ) { nModeX = 320; nModeY = 200; }
	else if ( sValue.GetFloat() == 400 ) { nModeX = 400; nModeY = 300; }
	else if ( sValue.GetFloat() == 640 ) { nModeX = 640; nModeY = 480; }
	else if ( sValue.GetFloat() == 800 ) { nModeX = 800; nModeY = 600; }
	else if ( sValue.GetFloat() == 1024 ) { nModeX = 1024; nModeY = 768; }
	else if ( sValue.GetFloat() == 1152 ) { nModeX = 1152; nModeY = 864; }
	else if ( sValue.GetFloat() == 1280 ) { nModeX = 1280; nModeY = 960; }
	else if ( sValue.GetFloat() == 1600 ) { nModeX = 1600; nModeY = 1200; }
	else { ASSERT( 0 ); }

	*pModeX = nModeX;
	*pModeY = nModeY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetModeFromConfig( bool bRecreate )
{
	// retail @0x1292e0 head: the gfx_recreate path tears the device down and re-initializes
	// before applying the mode (this is what makes an in-game resolution switch take effect)
	if ( bRecreate )
	{
		NGfx::Done3D();
		NGfx::Init3D( NGfx::GetHWND() );
	}
	NGfx::CheckDeviceCaps();
	NGlobal::CValue sValue;

	int nModeX = 1024, nModeY = 768;
	GetConfiguredVideoMode( &nModeX, &nModeY );

	NGfx::EFS fullScreen = NGfx::WINDOWED;
	sValue = NGlobal::GetVar( "gfx_fullscreen", 0 );
#ifndef _MAPEDIT
	if ( sValue.GetFloat() == 1 )
		fullScreen = NGfx::FULL_SCREEN;
#endif

	NGfx::SRenderTargetsInfo rtInfo;
	nDepthTexResolution = Float2Int( NGlobal::GetVar( "gfx_depth_tex_resolution", 512 ).GetFloat() );
	nCLSkyTextures = Float2Int( NGlobal::GetVar( "gfx_cl_sky_textures", 0 ).GetFloat() );
	nCLSkyTextures = Max( 0, nCLSkyTextures );
	nCLSkyTextures = Min( nCLSkyTextures, N_MAX_SKY_TEXTURES );
	if ( nDepthTexResolution != 1024 )
		nDepthTexResolution = 512;

	// select feature set
	NGfx::EHardwareLevel hl = NGfx::GetHardwareLevel();
	sValue = NGlobal::GetVar( "gfx_fastest", 0 );
	if ( sValue.GetFloat() != 0 || hl == NGfx::HL_TNL_DEVICE )
	{
		bCanCacheLighting = false;
		bCanRenderShadows = false;
		bCanCalcAmbient = false;
		nCLSkyTextures = 0;
	}
	else
	{
		bCanRenderShadows = true;
		sValue = NGlobal::GetVar( "gfx_cl", 1 );
		bCanCacheLighting = ( sValue.GetFloat() != 0 ) || hl >= NGfx::HL_GFORCE3;
		bCanCalcAmbient = nCLSkyTextures != 0;
	}
	nCLCubeResolution = Float2Int( NGlobal::GetVar( "gfx_cl_cube_resolution", 16 ).GetFloat() );
	nCLCubeResolution = Clamp( nCLCubeResolution, 16, 256 );
	nCLCubeResolution = GetNextPow2( nCLCubeResolution );

	// determine number and types of buffers
	rtInfo.Clear();
	if ( bCanRenderShadows )
	{
		if ( hl >= NGfx::HL_GFORCE3 )
			rtInfo.nRegisters = 5;
		else
		{
			if ( bCanCacheLighting )
				rtInfo.nRegisters = 4;
			else
				rtInfo.nRegisters = 2;
		}
		rtInfo.AddTex( 512, 1 + nCLSkyTextures ); // for particles and ambient
		rtInfo.AddTex( nDepthTexResolution, 1 );
	}
	if ( bCanCacheLighting )
	{
		rtInfo.AddCube( GetCLCubeResolution(), 100 );
		rtInfo.AddCube( 256, 1 );
	}

	bool bRes = NGfx::SetMode( NGfx::SVideoMode( nModeX, nModeY, 32, fullScreen ), rtInfo );
	if ( !bRes )
	{
		// in case of failure try to create device limited to fastest mode
		bCanCacheLighting = false;
		bCanRenderShadows = false;
		bCanCalcAmbient = false;
		nCLSkyTextures = 0;
		rtInfo.Clear();
		bRes = NGfx::SetMode( NGfx::SVideoMode( nModeX, nModeY, 32, fullScreen ), rtInfo );
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandGfxUpdate( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	SetModeFromConfig( false );   // retail @0x12a250
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CommandGfxRecreate( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	SetModeFromConfig( true );    // retail @0x12a260
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// gfx_16bit_mode backing flag, retail @0x99cf93 (Is16BitMode @0x10cde0 returns it; this tree's
// render path is still always 32-bit -- the flag only feeds the saved config / options packing).
static bool bUse16BitMode = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GInitInit @0x12a320: 2 cmds + 9 vars; Jan03's gfx_refreshlimit was DROPPED in retail and
// gfx_fullscreen defaults to 1.0 there.
START_REGISTER(GInit)
	REGISTER_CMD( "gfx_update", CommandGfxUpdate )
	REGISTER_CMD( "gfx_recreate", CommandGfxRecreate )
	REGISTER_VAR( "gfx_resolution", 0, 1024, true )
	REGISTER_VAR( "gfx_fullscreen", 0, 1, true )
	REGISTER_VAR( "gfx_depth_tex_resolution", 0, 512, true )
	REGISTER_VAR( "gfx_cl_sky_textures", 0, 0, true )
	REGISTER_VAR( "gfx_cl_cube_resolution", 0, 16, true )
	REGISTER_VAR( "gfx_cl", 0, 1, true )
	REGISTER_VAR( "gfx_fastest", 0, 0, true )
	REGISTER_VAR( "gfx_cl_use_precise_shadows", 0, 1, true )
	REGISTER_VAR_EX( "gfx_16bit_mode", NGlobal::VarBoolHandler, &bUse16BitMode, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
