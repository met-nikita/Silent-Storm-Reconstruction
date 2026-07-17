#ifndef __GRenderFactor_H_
#define __GRenderFactor_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GRenderCore.h"
#include "GRenderModes.h"
namespace NGfx
{
	struct SEffPointLight;
}
namespace NGScene
{
NGfx::CCubeTexture* GetNormalizeTexture();
NGfx::CTexture* GetFogLookupTexture( const SFogParams &_fog );
NGfx::CTexture* GenerateFogTexture( float fNoiseParam );
NGfx::CTexture* GetLightCircle();
NGfx::CTexture* GetLightFalloff();
NGfx::CTexture* GetSpecularResponse();
NGfx::CTexture* GetUniformBump();
NGfx::CTexture* GetBlackTexture();   // retail @0x14f6d0: 1x1 zero texture, the "no shadow" depth stand-in
NGfx::CTexture* GetDefaultLightmap();   // retail @0x14f750: 1x1 solid 0xFF000000, the "no CL" register stand-in
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
