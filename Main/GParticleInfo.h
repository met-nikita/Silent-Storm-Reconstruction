#ifndef __GPARTICLEINFO_H_
#define __GPARTICLEINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CGeometry;
	class CTexture;
}
namespace NGScene
{
class CGrassLayers;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleLightInfo
{
	// size of single particle in texels
	CVec2 vLightSize; 
	// current alloc position and size of whole stuff
	NGfx::SShortTextureUV vStart, vSize, vStep, vTrueStep;
	short nCheckStart;

	void Init( int nSizeX, int nSizeY, int nTotalX, int nTotalY, bool bIncremental )
	{
		vLightSize = CVec2( nSizeX, nSizeY );
		int nDU = 65536 / nTotalX, nDV = 65536 / nTotalY;
		vStart.nU = nDU / 2 - 4 - 0x8000; vStart.nV = nDV / 2 - 4 - 0x8000;
		nCheckStart = vStart.nU;
		vSize.nU = nDU * (nSizeX - 1) + 8;
		vSize.nV = nDV * (nSizeY - 1) + 8;
		vTrueStep.nU = nDU * nSizeX; 
		vTrueStep.nV = nDV * nSizeY;
		if ( bIncremental )
		{
			vStep = vTrueStep;
		}
		else
			StopIncrementing();
	}
	void StopIncrementing() { vStep.nU = 0; vStep.nV = 0; }
	void ForcedInc()
	{
		vStart.nU += vTrueStep.nU;
		if ( vStart.nU == nCheckStart )
			vStart.nV += vTrueStep.nV;
	}
	void Inc() 
	{ 
		vStart.nU += vStep.nU;
		if ( vStart.nU == nCheckStart )
			vStart.nV += vStep.nV;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STransparentTexturePlace
{
	NGfx::SShortTextureUV vUVs[4];
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleOrientationInfo
{
	CVec3 vBasic[4];
	CVec3 vDepth;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderGeometryInfo;
class IMaterial;
class IParticleOutput
{
public:
	virtual const SParticleOrientationInfo& GetOrientationInfo() const = 0;
	const CVec3& GetDepth() const { return GetOrientationInfo().vDepth; }
	virtual void AddParticle( const CVec3 vPos[4], DWORD dwColor, const STransparentTexturePlace &tex,
		float fDepth ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IParticleFilter : virtual public CObjectBase
{
public:
	// vtbl[0]: cull/transform a freshly generated particle cloud. pFlags is written in
	// lockstep with pPositions; a non-zero flag marks an entry to be removed by the caller.
	// Concrete implementations live in the separate GParticleFilter module.
	virtual void Filter( vector<CVec3> *pPositions, vector<unsigned char> *pFlags ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleEffect : public CObjectBase
{
public:
	bool bEnd; // true if effect finished
	int nGrassSize;
	vector<CObj<CPtrFuncBase<NGfx::CTexture> > > textures;
	CObj<IParticleFilter> pFilter;

	// function should be const but it is impossible due to DGPtrs presence
	virtual void AddParticles( IParticleOutput *pRender ) = 0;
	virtual bool IsEmpty() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticlesInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleFrame
{
	float fT;
	bool bLastCycle;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStandardParticleEffect: public CParticleEffect
{
	OBJECT_BASIC_METHODS(CStandardParticleEffect);
public:
	CVec2 pivot;
	float fEndCycle;
	float fScale;
	SHMatrix transform;
	vector<SParticleFrame> frames;
	CDGPtr< CPtrFuncBase<CParticlesInfo>, CPtr<CPtrFuncBase<CParticlesInfo> > > pInfo;

	void AddParticles( IParticleOutput *pRender );
	virtual bool IsEmpty() const { return frames.empty(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrassParticleEffect: public CParticleEffect
{
	OBJECT_BASIC_METHODS(CGrassParticleEffect);
public:
	float fTEffect;
	float fXPivot;
	float fYPivot;
	CVec2 scale;
	float fScaleRange;
	vector<CVec3> positions;
	vector<NGfx::SPixel8888> colors;
	vector<float> waveAmps;

	void AddParticles( IParticleOutput *pRender );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionParticleEffect: public CParticleEffect
{
	OBJECT_BASIC_METHODS(CExplosionParticleEffect);
	void AddParticles( IParticleOutput *pRender );
public:
	CVec2 pivot;
	vector<CVec3> positions;
	vector<float> fTimes;
	CDGPtr< CPtrFuncBase<CParticlesInfo>, CPtr<CPtrFuncBase<CParticlesInfo> > > pInfo;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRainParticleEffect: public CParticleEffect
{
	OBJECT_BASIC_METHODS(CRainParticleEffect);
public:
	vector<CVec3> positions;
	vector<char> faces;
	vector<CVec3> directions;

	void AddParticles( IParticleOutput *pRender );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTransparentTexturePlace( STransparentTexturePlace *pRes, NGfx::CTexture *pTex );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif