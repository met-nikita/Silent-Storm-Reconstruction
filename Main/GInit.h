#ifndef __GInit_H_
#define __GInit_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NGScene
{
const int N_MAX_SKY_TEXTURES = 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetModeFromConfig();
bool CanRenderShadows();
bool CanCacheLighting();
bool CanCalcAmbient();
int GetDepthTexResolution();
int GetCLSkyTexturesNumber();
int GetCLCubeResolution();
}
#endif
