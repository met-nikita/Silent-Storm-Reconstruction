#ifndef __GInit_H_
#define __GInit_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NGScene
{
const int N_MAX_SKY_TEXTURES = 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetModeFromConfig( bool bRecreate = false );   // retail @0x1292e0 takes the gfx_recreate flag
void GetConfiguredVideoMode( int *pModeX, int *pModeY );   // WIDESCREEN (Sentinels-style "WxH" gfx_resolution)
bool CanRenderShadows();
bool CanCacheLighting();
bool CanCalcAmbient();
int GetDepthTexResolution();
int GetCLSkyTexturesNumber();
int GetCLCubeResolution();
}
#endif
