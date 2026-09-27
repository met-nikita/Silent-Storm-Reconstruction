#include "StdAfx.h"
#include "GAutoDetect.h"
#include "Gfx.h"
#include "GfxBuffers.h"
#include "..\MiscDll\Commands.h"   // NGlobal::GetVar / SetVar / CValue
#include "..\Misc\HPTimer.h"       // NHPTimer::GetClockRate
#include <math.h>                  // fabs -- v1.2 @0xf69b0 epsilon preset match
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Preset tables -- transcribed verbatim from engine .rdata
// (speedConfig @0x954128, lightingQualityConfig @0x953f98, textureConfig @0x954218,
//  fsaaConfig @0x954288).  Each axis holds one NUL-terminated SCfgValue[] per
// EConfigValue (CV_LOW..CV_VHIGH).
////////////////////////////////////////////////////////////////////////////////////////////////////
// speedConfig: HSR / effect axis.  Index 0 keeps the most effects; index 3 is "fastest".
static SCfgValue speedLow[] = {
	{ "gfx_fog", 2.0f }, { "gfx_specular", 1.0f }, { "gfx_shadows", 1.0f }, { "gfx_cl", 1.0f },
	{ "gfx_blur_sun", 1.0f }, { "gfx_fastest", 0.0f }, { "gfx_cl_use_bump", 1.0f }, { "gfx_tnl_mode", -1.0f },
	{ "gfx_point_specular", 1.0f }, { "gfx_cl_blur", 1.0f }, { "gfx_decals", 1.0f }, { 0, 0.0f }
};
static SCfgValue speedMed[] = {
	{ "gfx_fog", 1.0f }, { "gfx_specular", 1.0f }, { "gfx_shadows", 1.0f }, { "gfx_cl", 1.0f },
	{ "gfx_blur_sun", 0.0f }, { "gfx_fastest", 0.0f }, { "gfx_cl_use_bump", 0.0f }, { "gfx_tnl_mode", -1.0f },
	{ "gfx_point_specular", 0.0f }, { "gfx_cl_blur", 1.0f }, { "gfx_decals", 1.0f }, { 0, 0.0f }
};
static SCfgValue speedHigh[] = {
	{ "gfx_fog", 0.0f }, { "gfx_specular", 0.0f }, { "gfx_shadows", 1.0f }, { "gfx_cl", 0.0f },
	{ "gfx_blur_sun", 0.0f }, { "gfx_fastest", 0.0f }, { "gfx_cl_use_bump", 0.0f }, { "gfx_tnl_mode", -1.0f },
	{ "gfx_point_specular", 0.0f }, { "gfx_cl_blur", 0.0f }, { "gfx_decals", 1.0f }, { 0, 0.0f }
};
static SCfgValue speedVHigh[] = {
	{ "gfx_fog", 0.0f }, { "gfx_specular", 0.0f }, { "gfx_shadows", 0.0f }, { "gfx_cl", 0.0f },
	{ "gfx_blur_sun", 0.0f }, { "gfx_fastest", 1.0f }, { "gfx_cl_use_bump", 0.0f }, { "gfx_tnl_mode", 0.0f },
	{ "gfx_point_specular", 0.0f }, { "gfx_cl_blur", 0.0f }, { "gfx_decals", 0.0f }, { 0, 0.0f }
};
static SCfgValue *speedConfig[4] = { speedLow, speedMed, speedHigh, speedVHigh };
////////////////////////////////////////////////////////////////////////////////////////////////////
static SCfgValue lightingLow[] = {
	{ "gfx_cl_sky_textures", 0.0f }, { "gfx_cl_use_precise_shadows", 0.0f }, { "gfx_cl_use_bump_always", 1.0f }, { 0, 0.0f }
};
static SCfgValue lightingMed[] = {
	{ "gfx_cl_sky_textures", 0.0f }, { "gfx_cl_use_precise_shadows", 1.0f }, { "gfx_cl_use_bump_always", 0.0f }, { 0, 0.0f }
};
static SCfgValue lightingHigh[] = {
	{ "gfx_cl_sky_textures", 0.0f }, { "gfx_cl_use_precise_shadows", 1.0f }, { "gfx_cl_use_bump_always", 1.0f }, { 0, 0.0f }
};
static SCfgValue lightingVHigh[] = {
	{ "gfx_cl_sky_textures", 4.0f }, { "gfx_cl_use_precise_shadows", 1.0f }, { "gfx_cl_use_bump_always", 0.0f }, { 0, 0.0f }
};
static SCfgValue *lightingQualityConfig[4] = { lightingLow, lightingMed, lightingHigh, lightingVHigh };
////////////////////////////////////////////////////////////////////////////////////////////////////
static SCfgValue textureLow[] = {
	{ "gfx_cl_cube_resolution", 32.0f }, { "gfx_texture_usedxt", 1.0f }, { "gfx_depth_tex_resolution", 512.0f },
	{ "gfx_texture_mip", 1.0f }, { "gfx_terrain_565", 1.0f }, { "gfx_16bit_textures", 1.0f }, { 0, 0.0f }
};
static SCfgValue textureMed[] = {
	{ "gfx_cl_cube_resolution", 32.0f }, { "gfx_texture_usedxt", 1.0f }, { "gfx_depth_tex_resolution", 512.0f },
	{ "gfx_texture_mip", 0.0f }, { "gfx_terrain_565", 1.0f }, { "gfx_16bit_textures", 0.0f }, { 0, 0.0f }
};
static SCfgValue textureHigh[] = {
	{ "gfx_cl_cube_resolution", 64.0f }, { "gfx_texture_usedxt", 1.0f }, { "gfx_depth_tex_resolution", 1024.0f },
	{ "gfx_texture_mip", 0.0f }, { "gfx_terrain_565", 1.0f }, { "gfx_16bit_textures", 0.0f }, { 0, 0.0f }
};
static SCfgValue textureVHigh[] = {
	{ "gfx_cl_cube_resolution", 128.0f }, { "gfx_texture_usedxt", 0.0f }, { "gfx_depth_tex_resolution", 1024.0f },
	{ "gfx_texture_mip", 0.0f }, { "gfx_terrain_565", 0.0f }, { "gfx_16bit_textures", 0.0f }, { 0, 0.0f }
};
static SCfgValue *textureConfig[4] = { textureLow, textureMed, textureHigh, textureVHigh };
////////////////////////////////////////////////////////////////////////////////////////////////////
static SCfgValue fsaaLow[] = {
	{ "gfx_fsaa", 0.0f }, { "gfx_register_resolution", 0.5f }, { 0, 0.0f }
};
static SCfgValue fsaaMed[] = {
	{ "gfx_fsaa", 0.0f }, { "gfx_register_resolution", 1.0f }, { 0, 0.0f }
};
static SCfgValue fsaaHigh[] = {
	{ "gfx_fsaa", 2.0f }, { "gfx_register_resolution", 2.0f }, { 0, 0.0f }
};
static SCfgValue fsaaVHigh[] = {
	{ "gfx_fsaa", 4.0f }, { "gfx_register_resolution", 4.0f }, { 0, 0.0f }
};
static SCfgValue *fsaaConfig[4] = { fsaaLow, fsaaMed, fsaaHigh, fsaaVHigh };
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsLowRAM  @0xf68e0
//   GlobalMemoryStatus(&ms); return 1 - (0x10000000 < dwTotalPhys);
// True when the machine has at most 256 MB of physical RAM.
bool IsLowRAM()
{
	MEMORYSTATUS ms;
	GlobalMemoryStatus( &ms );
	return ms.dwTotalPhys <= 0x10000000;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FindCfgMode  @0xf69b0
//   For preset i in [0,count): every (name,value) pair must equal the var's live
//   value (GetVar(name, value).GetFloat()).  First all-matching preset wins; else
//   CV_CUSTOM.  GetVar's default is the preset's own value, so an unregistered var
//   trivially matches.
EConfigValue FindCfgMode( SCfgValue **modes, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		SCfgValue *e = modes[i];
		bool bMatch = true;
		for ( ; e->pszName != 0; ++e )
		{
			float fLive = NGlobal::GetVar( e->pszName, NGlobal::CValue( e->fValue ) ).GetFloat();
			if ( !( fabs( fLive - e->fValue ) < 1e-12 ) )   // v1.2 @0xf69b0: epsilon compare (was exact ==); mismatch also for NaN operands
			{
				bMatch = false;
				break;
			}
		}
		if ( bMatch )
			return (EConfigValue)i;
	}
	return CV_CUSTOM;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplyCfgValues  @0xf6900
//   if (mode == CV_CUSTOM) return;
//   if (count <= mode) mode = count;   // clamp down to the table size
//   if (mode < 0) return;
//   SetVar(name, value) for every pair of modes[mode].
void ApplyCfgValues( SCfgValue **modes, EConfigValue mode, int count )
{
	int m = (int)mode;
	if ( m == CV_CUSTOM )
		return;
	if ( count <= m )
		m = count;
	if ( m < 0 )
		return;
	for ( SCfgValue *e = modes[m]; e->pszName != 0; ++e )
		NGlobal::SetVar( e->pszName, NGlobal::CValue( e->fValue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetSpeedMode  @0xf6ae0
//   ApplyCfgValues(speedConfig, mode, 4); then derive gfx_hsr:
//   hsr = 0 on <=256MB; on bigger boxes 1 for the very-high preset, else 2 for
//   >1.2 GHz CPUs and 1 otherwise.
void SetSpeedMode( EConfigValue mode )
{
	ApplyCfgValues( speedConfig, mode, CV_CUSTOM );

	MEMORYSTATUS ms;
	GlobalMemoryStatus( &ms );
	int nHSR = 0;
	if ( ms.dwTotalPhys > 0x10000000 )           // > 256 MB
	{
		if ( mode != CV_VHIGH )
			nHSR = ( NHPTimer::GetClockRate() > 1.2e9 ) ? 2 : 1;
		else
			nHSR = 1;
	}
	NGlobal::SetVar( "gfx_hsr", NGlobal::CValue( (float)nHSR ) );
}
EConfigValue GetSpeedMode()                       { return FindCfgMode( speedConfig, CV_CUSTOM ); }            // @0xf6ad0
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetLightingQualityMode @0xf6ab0 / GetLightingQualityMode @0xf6aa0
void SetLightingQualityMode( EConfigValue mode )  { ApplyCfgValues( lightingQualityConfig, mode, CV_CUSTOM ); }
EConfigValue GetLightingQualityMode()             { return FindCfgMode( lightingQualityConfig, CV_CUSTOM ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetTextureMode @0xf6bc0 / GetTextureMode @0xf6bb0
void SetTextureMode( EConfigValue mode )          { ApplyCfgValues( textureConfig, mode, CV_CUSTOM ); }
EConfigValue GetTextureMode()                     { return FindCfgMode( textureConfig, CV_CUSTOM ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetFSAAMode @0xf6bf0 / GetFSAAMode @0xf6be0
void SetFSAAMode( EConfigValue mode )             { ApplyCfgValues( fsaaConfig, mode, CV_CUSTOM ); }
EConfigValue GetFSAAMode()                        { return FindCfgMode( fsaaConfig, CV_CUSTOM ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
void AutoDetectVideoConfig()
{
	const NGfx::SVideoConfigInfo info = NGfx::GetVideoConfigInfo();
	// v1.2 0x9542a8: card, hardware level (-1 = any), lighting, speed, texture, 16-bit mode.
	static const int config[][6] = {
		{1,-1,0,3,1,1}, {2,-1,0,3,1,1}, {4,-1,0,2,1,0}, {5,-1,0,2,2,0},
		{7,-1,3,0,2,0}, {3,-1,0,3,0,1}, {6,-1,0,2,1,1}, {8,-1,0,3,1,1},
		{9,-1,0,3,1,1}, {10,-1,0,3,2,0}, {11,-1,3,1,2,0}, {12,-1,3,0,2,0},
		{0,4,3,1,2,0}, {0,3,0,2,2,0}, {0,2,0,2,2,0}, {0,1,0,3,1,1}, {0,0,0,3,1,1}
	};
	int nSpeed = 3, nTexture = 1, nLighting = 0, n16Bit = 1;
	for ( int i = 0; i < ARRAY_SIZE(config); ++i )
		if ( ( config[i][0] == 0 || config[i][0] == info.nCard ) &&
			( config[i][1] == -1 || config[i][1] == info.nHardwareLevel ) )
		{
			nLighting = config[i][2]; nSpeed = config[i][3]; nTexture = config[i][4]; n16Bit = config[i][5];
			break;
		}
	if ( !NGfx::CanStreamGeometry() )
		nSpeed = 3;
	NGlobal::SetVar( "gfx_16bit_mode", n16Bit );
	bool bLowRAM = IsLowRAM();
	NGlobal::SetVar( "gfx_low_ram", bLowRAM ? 1 : 0 );
	int nResolution = info.nDesktopWidth < 1024 ? 800 : 1024;
	if ( bLowRAM )
	{
		nSpeed = 3; nTexture = 1;
	}
	else
	{
		if ( info.fVideoMemoryMB < 32 )
		{
			nSpeed = 3; nTexture = 1;
			if ( info.fVideoMemoryMB < 16 )
				nResolution = 800;
		}
		else if ( info.fVideoMemoryMB < 64 )
			nTexture = 1;
		else if ( info.fVideoMemoryMB < 128 )
			nTexture = 2;
		else if ( info.fVideoMemoryMB + info.fSharedMemoryMB > 160 )
			nTexture = 3;
		if ( info.nHardwareLevel >= 4 )
		{
			if ( info.fVideoMemoryMB > 80 && info.nDesktopWidth > 1024 && info.nDesktopWidth < 1600 )
				nResolution = info.nDesktopWidth;
			NGlobal::SetVar( "gfx_anisotropic_filter", 2 );
		}
	}
	NGlobal::SetVar( "gfx_resolution", nResolution );
	SetSpeedMode( (EConfigValue)nSpeed );
	SetTextureMode( (EConfigValue)nTexture );
	SetLightingQualityMode( (EConfigValue)nLighting );
	// Retail intentionally leaves the smoothness / FSAA preset unchanged.
}
static void CommandGfxAutodetect( const string &, const vector<wstring> &, void * )
{
	AutoDetectVideoConfig();
}
START_REGISTER(GAutoDetect)
	REGISTER_CMD( "gfx_autodetect", CommandGfxAutodetect )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
