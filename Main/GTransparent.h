#ifndef __GTransparent_H_
#define __GTransparent_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Cache.h"
#include "GParticleInfo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
namespace NGfx
{
	class CRenderContext;
	class CTexture;
	class CTriList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class CParticleEffect;
class IVBCombiner;
class IMaterial;
struct SRenderGeometryInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STransparentObjectInfo
{
	SRenderGeometryInfo *pGeometry;
	IMaterial *pMaterial;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IParticles: public CObjectBase
{
public:
	virtual CParticleEffect* GetEffect() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTranspLightTexture : 
	public NCache::CGatherElementBase<NCache::CShortPtrAllocator,NCache::CQuadTreeElement, CTranspLightTexture>
{
	typedef NCache::CGatherElementBase<NCache::CShortPtrAllocator,NCache::CQuadTreeElement, CTranspLightTexture> CBase;
	OBJECT_NOCOPY_METHODS(CTranspLightTexture);
public:
	CTRect<int> region;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IReportParticlesGeometry
{
public:
	virtual void AddParticles( IVBCombiner *pVertices, CFuncBase<vector<NGfx::STriangleList> > *pTrilists, 
		int nPart, int nParticles, const SBound &bv ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STransparentInfo
{
	CObj<IVBCombiner> pGeom;
	STransparentObjectInfo objectInfo;
	float fDepth;
	int nOffset; // address of first particle or part in object combiner
	//vector<float> particleDepths; // empty for geometry
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseParticlesGeometry;
class CShaderParticlesGeometry;
class CTnLParticlesGeometry;
class CParticlesTriList;
class CTransparentRenderer : public CObjectBase, public IParticleOutput
{
	OBJECT_NOCOPY_METHODS(CTransparentRenderer);
	SParticleLightInfo litParticlesAlloc, kernel, *pLMAlloc;
	SParticleOrientationInfo orientation;

	vector<STransparentInfo> infos;
	vector<float> depths;
	vector<int> sourcePtrs;
	vector<int> infoStartIdx;
	unsigned int nElementPtr, nInfoIdx;
	bool bUseFakeLM;

	NGfx::SCompactVector vNormal;
	CObj<CBaseParticlesGeometry> pParticlesGeometry;
	CObj<CShaderParticlesGeometry> pShaderParticlesGeometry;
	CObj<CTnLParticlesGeometry> pTnLParticlesGeometry;
	CObj<CParticlesTriList> pParticlesTrilist;
	int nTargetParticle, nTargetStart, nPieceStart;//nEffectParticle;
	//STransparentInfo *pWriteParticles;
	CParticleEffect *pCurrentEffect;
	SBound currentEffectBV;
	DWORD dwCurrentParticleColor;
	IReportParticlesGeometry *pReportParticles;
	int nTotalParticles, nLitParticles;
	bool bTnLMode;
	DWORD dwLitColor, dwNormalColor;

	STransparentInfo* AddFragment();
	void AllocParticlesWriteBuffer();
	void FinishParticlesPiece();
	void StartParticlesPiece();
	void RealRender( NGfx::CRenderContext *pRC, NGfx::CTexture *pLight, NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky );
	void AddParticleOverflow( const CVec3 vPos[4], DWORD dwColor, const STransparentTexturePlace &tPlace,
		float fDepth );
	ZDATA
public:
	ZEND int operator&( CStructureSaver &f ) { return 0; }
public:
	CTransparentRenderer() : nTotalParticles(0), nLitParticles(0) {}
	CTransparentRenderer( const CTransformStack &ts, const CTPoint<int> &vLightBuffersize, bool bUseFakeLM,
		DWORD dwLitColor, DWORD dwNormalColor );
	virtual const SParticleOrientationInfo& GetOrientationInfo() const { return orientation; }
	virtual const SParticleLightInfo& GetKernelLightInfo() const { return kernel; }
	virtual void AddParticle( const CVec3 vPos[4], DWORD dwColor, const STransparentTexturePlace &tex,
		float fDepth );

	void AddParticles( IParticles *pParticles, bool bIsLit, const SBound &bv, IReportParticlesGeometry *pStore );
	void AddElement( SRenderGeometryInfo *pGeometry, IMaterial *pMaterial, int nIndex, float fDepth );
	void FinishParticles();
	void Render( NGfx::CRenderContext *pRC, NGfx::CTexture *pParticleLight, NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky );
	int GetTotalParticles() const { return nTotalParticles; }
	int GetLitParticles() const { return nLitParticles; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
