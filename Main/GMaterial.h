#ifndef __GMATERIAL_H_
#define __GMATERIAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CTexture;
	class CCubeTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
enum EMaterialAlpha
{
	MA_OPAQUE = 0,
	MA_TRANSPARENT = 1,
	MA_OVERLAY = 2,
	MA_TYPE_MASK = 3,
	MA_ALPHA_TEST = 4,
	MA_SELF_ILLUM = 8,
	MA_2SIDED = 16
};
/*struct SMaterialLayer
{
	enum EType
	{
		DIFFUSE,
		SPECULAR
	};
	CVec3 ptColor;
	float fSpecPower;
	CObj<CPtrFuncBase<NGfx::CTexture> > pTexture;
	CObj<CPtrFuncBase<NGfx::CTexture> > pBump;
	//EMaterialAddrMode addrMode;
};*/
class IMaterial;
IMaterial* CreateMaterial( 
	const CVec3 &color,
	CPtrFuncBase<NGfx::CTexture> *pTexture = 0, CPtrFuncBase<NGfx::CTexture> *pBump = 0,
	float fSpecPower = 0, const CVec3 &glossColor = CVec3(0,0,0),
	CPtrFuncBase<NGfx::CTexture> *pGlossTexture = 0,
	CPtrFuncBase<NGfx::CTexture> *pMirrorTexture = 0,
	CPtrFuncBase<NGfx::CCubeTexture> *pSky = 0,
	float fMetalMirror = 0, float fDielMirror = 0,
	int alphaMode = MA_OPAQUE, bool bDoesCastShadow = true );
IMaterial* CreateOccluderMaterial();
IMaterial* CreateAlienMaterial( CPtrFuncBase<NGfx::CCubeTexture> *_pSky );
IMaterial* CreateExplosionDecal( CPtrFuncBase<NGfx::CTexture> *pTexture );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif