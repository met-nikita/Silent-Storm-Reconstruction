#ifndef __GShadowMap_H_
#define __GShadowMap_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GInit.h"
namespace NGfx
{
	class CTexture;
	class CCubeTexture;
	class CRenderContext;
}
class CTransformStack;
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_DEFAULT_RT_RESOLUTION = 512;
class CShadowMapsShare
{
	CObj<NGfx::CTexture> pDepthShadow, pParticleLM, pLMDepthBuffers[ N_MAX_SKY_TEXTURES ];
	CObj<NGfx::CCubeTexture> pCubeDepth;
	int nDepthResolution, nCLSkyTextures;

	void Refresh();
public:
	CShadowMapsShare();
	NGfx::CTexture* GetDepthShadow() { Refresh(); return pDepthShadow; }
	NGfx::CTexture* GetParticleLM() { Refresh(); return pParticleLM; }
	NGfx::CTexture* GetLMDepthBuffer( int nBuffer ) 
	{ 
		ASSERT( nBuffer >= 0 && nBuffer < ARRAY_SIZE(pLMDepthBuffers) && nBuffer < nCLSkyTextures ); 
		Refresh(); 
		return pLMDepthBuffers[ nBuffer ]; 
	}
	NGfx::CCubeTexture* GetCubeDepth() { Refresh(); return pCubeDepth; }
	int GetDepthResolution() { Refresh(); return nDepthResolution; }
	int GetCLSkyTexturesNumber() { Refresh(); return nCLSkyTextures; }
	int GetCubeDepthResolution() const { return 256; }
};
externA5 CShadowMapsShare shadowMapsShare;
void DrawBorder( NGfx::CRenderContext *pRC, int nSize );
void SetAndClearRT( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, int nWriteMask, int nSize );
void MakeShadowMatrix( CTransformStack *pRes, const CTransformStack &ts, const CVec3 &ptDir, float fMaxHeight );
void MakeSceneGeometryBound( SBound *pRes, const CTransformStack &ts, float fMaxHeight );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif