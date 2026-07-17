#ifndef __GPARTICLES_H_
#define __GPARTICLES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Time.h"
// IParticleFilter must be COMPLETE here, not forward-declared: CParticleAnimator serializes a
// CObj<IParticleFilter> (tag 8), and CastToObjectBase picks its overload on whether T* converts to
// CObjectBase*. Against an incomplete type that conversion is invisible, so it silently selects the
// void* overload, which is declaration-only -- an unresolved CastToObjectBaseImpl at link time.
#include "GParticleInfo.h"

#include "..\Misc\2DArray.h"
namespace NDb
{
	class CGrass;
	class CParticleInstance;
}
namespace NGfx
{
	class CTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class CParticleEffect;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticlesInfo;
class CParticleAnimator: public CPtrFuncBase<CParticleEffect>
{
	OBJECT_BASIC_METHODS(CParticleAnimator);
protected:
	virtual bool NeedUpdate() { return pTime.Refresh() | pInfo.Refresh() | pPlacement.Refresh(); }
	virtual void Recalc();
private:	
	ZDATA
	STime stBeginTime;
	CDBPtr<NDb::CParticleInstance> pInstance;
public:
	CDGPtr< CFuncBase<STime> > pTime;
	CDGPtr< CFuncBase<SFBTransform> > pPlacement;
	CDGPtr< CPtrFuncBase<CParticlesInfo> > pInfo;
	vector<CObj<CPtrFuncBase<NGfx::CTexture> > > textureIDs;
	// pFilter @+0x44 (last member): the release ADDED this over Jan03 -- an optional cull/transform
	// hook handed down to the produced effect by Recalc @0x143b50 (value.pFilter = pFilter), where
	// CStandardParticleEffect::AddParticles @0x141360 applies it (`if (pFilter) pFilter->Filter()`).
	// Retail has NO code-path setter: the only producers are DoPtr<IParticleFilter> @0x145c10
	// (this chunk) and MakeCopy @0x145050 -- and every tag-8 payload in all 9 v1.2 saves is null.
	// Serialized regardless, so the chunk is consumed and dev-written saves round-trip retail's shape.
	CObj<IParticleFilter> pFilter;
	// retail @0x145ab0: 2=stBeginTime(4), 3=pInstance, 4=pTime, 5=pPlacement, 6=pInfo,
	// 7=textureIDs (DoVector), 8=pFilter
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&stBeginTime); f.Add(3,&pInstance); f.Add(4,&pTime); f.Add(5,&pPlacement); f.Add(6,&pInfo); f.Add(7,&textureIDs); f.Add(8,&pFilter); return 0; }

	CParticleAnimator() {}
	CParticleAnimator( NDb::CParticleInstance *_pInstance, STime t ): pInstance(_pInstance), stBeginTime(t) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrassTracker;
class CGrassPosition;
class CGrassAnimator : public CPtrFuncBase<CParticleEffect>
{
	OBJECT_BASIC_METHODS(CGrassAnimator);
	ZDATA
	CDBPtr<NDb::CGrass> pDBGrass;
	CPtr<CGrassTracker> pGrassTracker;
	CDGPtr< CPtrFuncBase<CGrassPosition> > pGrassPos;
	CDGPtr< CFuncBase<STime> > pTime;
	int nLayer;
	CDGPtr< CPtrFuncBase<NGfx::CTexture> > pGrassTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pDBGrass); f.Add(3,&pGrassTracker); f.Add(4,&pGrassPos); f.Add(5,&pTime); f.Add(6,&nLayer); f.Add(7,&pGrassTexture); return 0; }
protected:
	virtual bool NeedUpdate() { return pTime.Refresh() | pGrassPos.Refresh(); }
	virtual void Recalc();
public:
	CGrassAnimator() {}
	CGrassAnimator( NDb::CGrass *_pDBGrass, CGrassTracker *_pGrassTracker, 
		CPtrFuncBase<CGrassPosition> *_pGrassPos, CFuncBase<STime> *_pTime, int _nLayer ):
		pDBGrass(_pDBGrass), pGrassTracker(_pGrassTracker), pGrassPos(_pGrassPos), pTime(_pTime), nLayer(_nLayer) {}
	void SetTexture( CPtrFuncBase<NGfx::CTexture> *_pTex ) { pGrassTexture = _pTex; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleBorn
{
	ZDATA
	CVec3 pos;
	STime tBorn;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pos); f.Add(3,&tBorn); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionInfo
{
public:
	ZDATA
	vector<SParticleBorn> particles;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&particles); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionAnimator : public CPtrFuncBase<CParticleEffect>
{
	OBJECT_BASIC_METHODS(CExplosionAnimator);
protected:
	virtual bool NeedUpdate() { return pTime.Refresh() | pInfo.Refresh() | pExplosion.Refresh(); }
	virtual void Recalc();
private:
	ZDATA
	STime tLastBorn;
	list<SParticleBorn> particles;
	CDBPtr<NDb::CParticleInstance> pInstance;
public:
	CDGPtr< CFuncBase<CExplosionInfo> > pExplosion;
	CDGPtr< CFuncBase<STime> > pTime;
	CDGPtr< CPtrFuncBase<CParticlesInfo> > pInfo;
	vector<CObj<CPtrFuncBase<NGfx::CTexture> > > textureIDs;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tLastBorn); f.Add(3,&particles); f.Add(4,&pInstance); f.Add(5,&pExplosion); f.Add(6,&pTime); f.Add(7,&pInfo); f.Add(8,&textureIDs); return 0; }

	CExplosionAnimator() { tLastBorn = 0; }
	CExplosionAnimator( NDb::CParticleInstance *_pInstance ): pInstance(_pInstance) { tLastBorn = 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRainAnimator -- drives a rain CRainParticleEffect from a time node + a camera-position node,
// through an optional IParticleFilter. Serialized: retail operator& @0x146e50 (GParticlesRain.obj),
// registered under 0x01063120 (GParticles.cpp, landed in serialization-convergence W2).
class CRainAnimator : public CPtrFuncBase<CParticleEffect>
{
	OBJECT_BASIC_METHODS(CRainAnimator);
protected:
	virtual bool NeedUpdate() { return pCamera.Refresh() | pTime.Refresh(); }
	virtual void Recalc();
public:
	ZDATA
	CDGPtr< CFuncBase<unsigned long> > pTime;
	CDGPtr< CFuncBase<CVec3> > pCamera;
	CObj<IParticleFilter> pFilter;
	vector<CObj<CPtrFuncBase<NGfx::CTexture> > > textureIDs;
	unsigned long tStart;
	// retail @0x146e50: 2=pTime, 3=pCamera, 4=pFilter, 5=textureIDs (DoVector), 6=tStart
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTime); f.Add(3,&pCamera); f.Add(4,&pFilter); f.Add(5,&textureIDs); f.Add(6,&tStart); return 0; }

	CRainAnimator() { tStart = 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif