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
bool SetModeFromConfig()
{
	NGfx::CheckDeviceCaps();
	NGlobal::CValue sValue;

	int nModeX = 1024, nModeY = 768;
	sValue = NGlobal::GetVar( "gfx_resolution", 1024 );
	if ( sValue.GetFloat() == 320 ) { nModeX = 320; nModeY = 200; }
	else if ( sValue.GetFloat() == 400 ) { nModeX = 400; nModeY = 300; }
	else if ( sValue.GetFloat() == 640 ) { nModeX = 640; nModeY = 480; }
	else if ( sValue.GetFloat() == 800 ) { nModeX = 800; nModeY = 600; }
	else if ( sValue.GetFloat() == 1024 ) { nModeX = 1024; nModeY = 768; }
	else if ( sValue.GetFloat() == 1280 ) { nModeX = 1280; nModeY = 1024; }
	else if ( sValue.GetFloat() == 1600 ) { nModeX = 1600; nModeY = 1200; }
	else { ASSERT( 0 ); }

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
	SetModeFromConfig();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(GInit)
	REGISTER_CMD( "gfx_update", CommandGfxUpdate )
	REGISTER_VAR( "gfx_resolution", 0, 1024, true )
	REGISTER_VAR( "gfx_fullscreen", 0, 0, true )
	REGISTER_VAR( "gfx_refreshlimit", 0, 1000, true )
	REGISTER_VAR( "gfx_depth_tex_resolution", 0, 512, true )
	REGISTER_VAR( "gfx_cl_sky_textures", 0, 0, true )
	REGISTER_VAR( "gfx_cl_cube_resolution", 0, 16, true )
	REGISTER_VAR( "gfx_cl", 0, 1, true )
	REGISTER_VAR( "gfx_fastest", 0, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
