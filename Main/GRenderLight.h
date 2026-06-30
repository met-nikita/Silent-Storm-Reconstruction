#ifndef __GRenderLight_H_
#define __GRenderLight_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GRenderCore.h"
#include "Transform.h"
#include "GShadowVolume.h"
namespace NGfx
{
	class CTexture;
	class CTriList;
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextureChannel;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLightInfo;
externA5 bool bStaticShadowDepthRendered;
class ICacheLightRender : virtual public CObjectBase
{
public:
	virtual void RenderCL( NGfx::CRenderContext *pRC, IRender *pRender, CTransformStack *pTS, 
		CSceneFragments *pGeom, bool bSceneHasChanged ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDirectionalLight: public ILight
{
	OBJECT_BASIC_METHODS(CDirectionalLight);

	ZDATA
	CDGPtr<CVersioningBase> pStaticTracker;
	CDGPtr<CFuncBase<CVec3> > pColor;
	CDGPtr<CFuncBase<CVec3> > pGlossColor;
	CVec3 vShadowColor;
	CDGPtr<CFuncBase<CVec3> > pAmbient;
	CVec4 vDepth;
	bool bLightmapOnly;
	CVec3 vLightDir;
	SHMatrix mPrevCamera;
	float fMaxHeight;
	float fBlurStrength;
	CPtr<ICacheLightRender> pCLRender;
	CDGPtr<CVersioningBase> pCLStaticTrack;
	CDGPtr<CFuncBase<CVec3> > pTopAmbient, pBottomAmbient;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pStaticTracker); f.Add(3,&pColor); f.Add(4,&pGlossColor); f.Add(5,&vShadowColor); f.Add(6,&pAmbient); f.Add(7,&vDepth); f.Add(8,&bLightmapOnly); f.Add(9,&vLightDir); f.Add(10,&mPrevCamera); f.Add(11,&fMaxHeight); f.Add(12,&fBlurStrength); f.Add(13,&pCLRender); f.Add(14,&pCLStaticTrack); f.Add(15,&pTopAmbient); f.Add(16,&pBottomAmbient); return 0; }
	//
	void PrepareLightInfo( SLightInfo *pLightInfo );

	template<class TParam, class TOp>
	void Render( CTransformStack *pTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, const CSceneFragments &scene, const SLightInfo &lightInfo, TOp process,
		const TParam &param )
	{
		CRenderCmdList lightOps;
		const vector<SRenderFragmentInfo*> &fragments = scene.GetFragments();
		for ( int i = 1; i < fragments.size(); ++i )
		{
			if ( scene.IsFilteredFragment( i ) )
				continue;
			const SRenderFragmentInfo &frag = *fragments[i];
			SOpGenContext op( &lightOps.ops, &frag );
			const SMaterialInfo &info = frag.pMaterial->GetMaterialInfo();
			process( op, info, param, renderPath );
		}
		Execute( pRender, pRC, *pTS, lightOps, scene, lightInfo );
	}
	void FinalPass( NGfx::CRenderContext *pRC, ERenderPath renderPath );
	void RenderPPShadowOps( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, CSceneFragments &scene, const SLightInfo &lightInfo, const SParticleLMRenderTargetInfo &particleLM );
	void BlurLight( NGfx::CRenderContext *pRC );
	void RenderOccluders( CTransformStack *pTS, NGfx::CRenderContext *pRC, IRender *pRender, CSceneFragments &scene );
	void RenderAmbientLightmaps( CTransformStack *pTS, NGfx::CRenderContext *pRC, IRender *pRender, CSceneFragments &scene, const SLightInfo &lightInfo );
	void RenderInSinglePass( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, IRender *pRender, CSceneFragments &scene, const SParticleLMRenderTargetInfo &particleLM, const SLightInfo &lightInfo );
public:
	CDirectionalLight() {}
	CDirectionalLight( CFuncBase<CVec3> *pColor, CFuncBase<CVec3> *pGlossColor, 
		const CVec3 &vShadowColor, const CVec3 &ptLight, 
		const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, 
		CVersioningBase *_pStaticTracker, CFuncBase<CVec3> *pAmbient, bool bLightmapOnly, float _fBlurStrength,
		ICacheLightRender *_pCLRender,
		CFuncBase<CVec3> *pTopAmbient, CFuncBase<CVec3> *pBottomAmbient );
	int GetPriority() const { return 2; }
	void Render( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, CSceneFragments &scene, const SParticleLMRenderTargetInfo &particleLM );
	struct SRadianceInfo
	{
		bool bIsRendered;
		CVec3 vColor, vAmbientColor, vUpDifColor;
		CVec3 vDirection;
	};
	void GetRadianceInfo( SRadianceInfo *pRes, ERenderPath renderPath );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPointLight: public ILight
{
	OBJECT_BASIC_METHODS(CPointLight);
	CObj<NGfx::CTriList> pStaticTris;
	CObj<NGfx::CGeometry> pStaticGeom;
	CObj<NGfx::CTriList> pDynamicTris;
	CObj<NGfx::CGeometry> pDynamicGeom;
	float fStaticHullRadius;
	ZDATA
	SBound sBound;
	CVec4 ptCenter;
	CVec4 vRadius;
	CTransformStack sTransform;
	CDGPtr<CVersioningBase> pStaticTracker;
	CObj<CFuncBase<CVec3> > pWhite;
	CFilterPartsHash ignoreSet;
	ZSKIP
	bool bLightmapOnly;
	CVec3 vColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sBound); f.Add(3,&ptCenter); f.Add(4,&vRadius); f.Add(5,&sTransform); f.Add(6,&pStaticTracker); f.Add(7,&pWhite); f.Add(10,&bLightmapOnly); f.Add(11,&vColor); f.Add(12,&bCastShadow); return 0; }
	bool bCastShadow = false;

	void CalcShadowVolumes( const vector<CVec3> &points, const vector<STriangle> &tris, CObj<NGfx::CGeometry> *pGeom, CObj<NGfx::CTriList> *pTris, bool bDynamic );
	void CreateShadowVolumes( IRender *pRender, float *pHullRadius );
	void RenderShadowVolumes( NGfx::CRenderContext *pRC );
	void Add( SOpGenContext &op, ERenderPath renderPath, const SMaterialInfo &info );
	float GetFRadius() const { return vRadius.x; }
	void FinalPass( NGfx::CRenderContext *pRC, float fHullRadius );
public:
	CPointLight() {}
	CPointLight( const CVec3 &_vColor, const CVec3 &ptCenter, float fRadius, CVersioningBase *_pStaticTracker, bool bLightmapOnly );
	virtual int GetPriority() const { return 3; }
	virtual void Render( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, CSceneFragments &scene, const SParticleLMRenderTargetInfo &particleLM );
	virtual bool CheckCulling( CTransformStack *pTS );
	struct SRadianceInfo
	{
		bool bIsRendered;
		CVec3 vCenter;
		CVec3 vColor;
		float fRadius;
	};
	void GetRadianceInfo( SRadianceInfo *pRes, ERenderPath renderPath );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDynamicPointLight : public ILight
{
	OBJECT_BASIC_METHODS(CDynamicPointLight);
	ZDATA
	CDGPtr<CPtrFuncBase<CAnimLight> > pLight;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pLight); return 0; }
	int nTargetReg;

	void Add( SOpGenContext &op, ERenderPath renderPath, const SMaterialInfo &info );
	void FinalPass( NGfx::CRenderContext *pRC );
public:
	CDynamicPointLight() {}
	CDynamicPointLight( CPtrFuncBase<CAnimLight> *_p ) : pLight(_p) {}
	virtual int GetPriority() const { return 4; }
	virtual void Render( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, CSceneFragments &scene, const SParticleLMRenderTargetInfo &particleLM );
	virtual bool CheckCulling( CTransformStack *pTS );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
/*class CSpotLight: public CLightBase
{
	OBJECT_BASIC_METHODS(CSpotLight);
	CObj<NGfx::CTexture> pDepth;
	ZDATA_(CLightBase)
	SBound bv;
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pMask;
	CTransformStack ts;
	//CObj<IRenderFactor> pDiffuse;
	//CObj<CFacSpotShadow> pShadow;
	//CObj<CFacPerspDepth> pDepthFactor;
	//CObj<CFacConstColor> pParticleLight;
	CVec4 ptCenter;
	float fRadius;
	//CObj<IRenderFactor> pFalloff, pProject;
	CObj<CFuncBase<CVec3> > pWhite;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CLightBase*)this); f.Add(2,&bv); f.Add(3,&pMask); f.Add(4,&ts); f.Add(5,&ptCenter); f.Add(6,&fRadius); f.Add(7,&pWhite); return 0; }
	
	void RenderFrustrum( NGfx::SEffect *pEffect, NGfx::CRenderContext *pRC );
	void CalcWorldToLight( SHMatrix *pRes );
public:
	CSpotLight() {}
	CSpotLight( CFuncBase<CVec3> *_pColor, const CVec3 &ptCenter, const CVec3 &ptDir, float fFov, float fRadius, CPtrFuncBase<NGfx::CTexture> *_pMask );
	virtual const CVec4& GetPos() const { return ptCenter; }
	//virtual void Prepare( NGfx::CRenderContext *pRC );
	//virtual void Cleanup( NGfx::CRenderContext *pRC );
	virtual bool Update( NGfx::CRenderContext *pRC, ERenderPath renderPath, IRender *pRender );
	virtual bool CheckCulling( CTransformStack *pTS );
	virtual void Select( CRenderList *pList );
	virtual EShadowType GetShadowType() { return SHT_LIMITED_AT; }
	//virtual IRenderFactor* GetShadowFactor();
	//virtual IRenderFactor* GetDiffuseFactor();
	//virtual IRenderFactor* GetParticleDiffuseFactor();
	//virtual IRenderFactor* CreateSpecularFactor( float fRo );
	//virtual IRenderFactor* CreateSpecularFactor( float fRo, CPtrFuncBase<NGfx::CTexture> *_pBump, bool bWrap );
	//virtual IRenderFactor* CreateDiffuseFactor( CPtrFuncBase<NGfx::CTexture> *_pBump, bool bWrap, float fTransp );
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
