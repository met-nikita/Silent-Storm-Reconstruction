#ifndef __GDXUTILS_H_
#define __GDXUTILS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GfxRender.h"
#include "GfxBuffers.h"

namespace NGfx
{
class CTexture;
class CRenderContext;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct S2DRectInfoLock;
enum EDepth
{
	QRM_DEPTH_NONE = 0,
	QRM_OVERWRITE = 1,
	QRM_DEPTH_MASK = 1,
	QRM_SIMPLE = 0,
	QRM_NOCOLOR = 2,
	QRM_SOLID = 4,
	QRM_USER_EFFECT = 6,
	QRM_RENDER_MASK = 14,
	QRM_KEEP_STENCIL = 0,
	QRM_TEST_STENCIL = 16,
	QRM_STENCIL_MASK = 16,
};
class I2DEffect : public CObjectBase
{
public:
	virtual void SetEffect( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, float fScaleU, float fScaleV ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class C2DQuadsRenderer: INew2DTexAllocCallback
{
public:

	C2DQuadsRenderer(): pLock(0) {}
	C2DQuadsRenderer( const NGfx::CRenderContext &rc, const CVec2 &vSize, int _dm );
	~C2DQuadsRenderer() { Flush(); }
	void SetTarget( const CVec2 &vSize, int _dm );
	void SetTarget( NGfx::CTexture *pTarget, const CVec2 &vSize, int _dm );
	void SetTarget( const NGfx::CRenderContext &rc, const CVec2 &vSize, int _dm );
	void AddRect( const CTRect<float> &rTarget, NGfx::CTexture *pTex, const CTRect<float> &rSrc, 
		SPixel8888 color = NGfx::SPixel8888(255,255,255,255), float fZ = 1.0f );
	void Flush();
	void SetUserEffect( I2DEffect *p ) { pUserEffect = p; }

private:
	int dm; // combination of QRM_*
	NGfx::CRenderContext rc;
	CPtr<CTexture> pPrevContainer;
	S2DRectInfoLock *pLock;
	float fUVMult, fScaleU, fScaleV;
	CObj<I2DEffect> pUserEffect;

	C2DQuadsRenderer( const C2DQuadsRenderer &a ) { ASSERT(0); }
	void operator=( const C2DQuadsRenderer &a ) { ASSERT(0); }
	void SetupRC( const CVec2 &vSize );
	S2DRectInfoLock* GetRectInfoLock( CTexture *pContainer, const STexturePlaceInfo &region );
	virtual void NewTextureWasAllocated() { Flush(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowAlphaEffect : public I2DEffect
{
	OBJECT_NOCOPY_METHODS(CShowAlphaEffect);
public:
	virtual void SetEffect( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, float fScaleU, float fScaleV );
};
class CCLAmbientMulDiffuseEffect : public I2DEffect
{
	OBJECT_NOCOPY_METHODS(CCLAmbientMulDiffuseEffect);
	int nReg;
	CVec3 vAmbient;
public:
	CCLAmbientMulDiffuseEffect() {}
	CCLAmbientMulDiffuseEffect( int _nReg, const CVec3 &_vAmbient ) : nReg(_nReg), vAmbient(_vAmbient) {}
	virtual void SetEffect( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, float fScaleU, float fScaleV );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowTexture( NGfx::CTexture *pTex, float fpMag = 1 );
void ShowTexture( NGfx::CRenderContext *pRC, NGfx::CTexture *pTex, const CVec2 &vScreenSize );
// direct transofrm should be setup
void CopyTexture( const NGfx::CRenderContext &_rc, const CVec2 &vTargetViewport, const CTRect<float> &rTarget, 
	NGfx::CTexture *pTex, const CTRect<float> &rSrc, const CVec4 &vColor = CVec4(1,1,1,1), I2DEffect *pEffect = 0 );
void BlurLight( NGfx::CRenderContext *pRC, int nSrcRegister, int nDestRegister, float fBlurStrength = 1.5f );
void ModulateRegister( NGfx::CRenderContext *pRC, int nDestRegister, const CVec4 &vColor );
void AlphaSqrtModulateRegister( NGfx::CRenderContext *pRC, int nDestRegister, int nSrcRegister, float fMul );
// maximal number of rects in single tri list
const int N_MAX_RECTANGLES = 16000;
void MakeQuadTriList( int nRects, STriangleList *pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif