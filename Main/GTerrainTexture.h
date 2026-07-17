#ifndef __GTERRAINTEXTURE_H_
#define __GTERRAINTEXTURE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Transform.h"
#include "TerrainInfo.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\HPTimer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CTexture;
	class CGeometry;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISW2DScene;
class CGrassTracker;
class CSWTextureData;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail split the Jan03 dual-resolution CTerrainTexture into a single-resolution LEAF (this class,
// @0x17b060/@0x17d1f0, size 0x4c) plus an outer CTerrainTextureBlend (below) that owns a 128 and a
// 256 leaf, cross-fades between them and drives the LOD. A leaf computes exactly ONE resolution
// (nDetail): pTex128/pTex256/pLOD and the bHasToRecalc128/256 flags are gone. @0x17b320.
class CTerrainTexture: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CTerrainTexture);
private:
	ZDATA
	bool bBumpTexture;
	SRandomSeed sSeed;
	CTRect<int> nrRegion;
	CDGPtr< CFuncBase< STerrainInfo > > pInfo;
	CPtr<CGrassTracker> pGrass;
	float fWorldToScreen;
	CDGPtr<CVersioningBase> pUpdateRegion;
	int nDetail;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bBumpTexture); f.Add(3,&sSeed); f.Add(4,&nrRegion); f.Add(6,&pInfo); f.Add(7,&pGrass); f.Add(8,&fWorldToScreen); f.Add(9,&pUpdateRegion); f.Add(10,&nDetail); return 0; }

	struct SSpotTextures
	{
		CPtrFuncBase<CSWTextureData> *pTex, *pBump;
	};
	bool CalcNewTexture( int nSize );
	CVec2 GetScreenCoords( const CVec2 &ptWorld );
	CVec2 GetScreenSize( const CVec2 &ptWorld );
	bool GetSpotTextures( NDb::CMaterial *pMat, SSpotTextures *pRes );
	void DrawSpot( ISW2DScene *p2DScene, const SSpotTextures &textures, const CVec2 &ptPos, const CVec2 &ptSize, float fAngle );
	void CreateColorMask( ISW2DScene *p2DScene, int nSize );
	void DrawGrassSpots( ISW2DScene *p2DScene );
	void DrawSpots( ISW2DScene *p2DScene, const STerrainInfo &info );
protected:
	void Recalc();
	bool NeedUpdate();
public:
	CTerrainTexture(): bBumpTexture( false ), sSeed( 0 ), nDetail( 0 ) {}
	CTerrainTexture( bool _bBump, SRandomSeed _sSeed, const CTRect<int> &_nrRegion,
		CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *_pUpdateRegion, CGrassTracker *_pGrass, int _nDetail );
	void FreeTexture( NGfx::CTexture *_pTex );
	//! this+0x14 valid? (@0x17d840) -- used by CTerrainTextureBlend
	bool IsValidValue() { return IsValid( pValue ); }
	//! best-effort "fake" texture without caching in pValue (@0x17d0b0) -- used by UseAnything
	NGfx::CTexture* CalcFake();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail-new value produced by CTerrainTextureBlend: the primary shown texture (pTex[0]), the
// previous texture being blended out (pTex[1]) and the cross-fade weight (fBlend).
struct STerrainTextureBlend
{
	CObj<NGfx::CTexture> pTex[2];
	float fBlend;
	STerrainTextureBlend(): fBlend( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail-new outer node (@0x17d240) : CFuncBase<STerrainTextureBlend>. Owns a 128 and a 256 leaf
// sharing one pLOD; picks by nDetail and cross-fades (GetBlend @0x17af50 over F_TRANSFER_TIME).
class CTerrainTextureBlend: public CFuncBase<STerrainTextureBlend>
{
	OBJECT_BASIC_METHODS(CTerrainTextureBlend);
private:
	ZDATA
	CDGPtr<CTerrainTexture> pTex128, pTex256;
	CDGPtr< CFuncBase<int> > pLOD;
	bool bBumpTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTex128); f.Add(3,&pTex256); f.Add(4,&pLOD); f.Add(5,&bBumpTexture); return 0; }

	int nDetail;
	int nPrevDetail;
	NHPTimer::STime tCalced256;
	CObj<NGfx::CTexture> pFake;

	void UseAnything();          // @0x17d370
	float GetBlend();            // @0x17af50
protected:
	void Recalc();               // @0x17d470
	bool NeedUpdate();           // @0x17b180
public:
	CTerrainTextureBlend(): bBumpTexture( false ), nDetail( 0 ), nPrevDetail( -1 ) { tCalced256 = 0; }
	CTerrainTextureBlend( bool _bBump, SRandomSeed _sSeed, const CTRect<int> &_nrRegion,
		CFuncBase<int> *_pLOD, CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *_pUpdateRegion, CGrassTracker *_pGrass );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail-new fetch leaf (@0x17e5f0/@0x17e640) : exposes the n-th blend-layer texture of an upstream
// CTerrainTextureBlend as its own CPtrFuncBase<NGfx::CTexture>.
class CTerrainTextureFetch: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CTerrainTextureFetch);
private:
	ZDATA
	CDGPtr< CFuncBase<STerrainTextureBlend> > p;
	int n;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&p); f.Add(3,&n); return 0; }
protected:
	bool NeedUpdate() { return p.Refresh(); }             // @0x17e5f0
	void Recalc() { pValue = p->GetValue().pTex[n]; }     // @0x17e640: pValue = p->value.pTex[n]
public:
	CTerrainTextureFetch(): n( 0 ) {}
	CTerrainTextureFetch( CFuncBase<STerrainTextureBlend> *_p, int _n ): p( _p ), n( _n ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail-new second-diffuse blend factor (@0x17f0b0/@0x17de90) : CFuncBase<float> tracking an
// upstream blend's fBlend.
class CTerrainTextureBlendColor: public CFuncBase<float>
{
	OBJECT_BASIC_METHODS(CTerrainTextureBlendColor);
private:
	ZDATA
	CDGPtr< CFuncBase<STerrainTextureBlend> > p;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&p); return 0; }
protected:
	bool NeedUpdate() { return p.Refresh(); }             // @0x6a8f0
	void Recalc() { value = p->GetValue().fBlend; }       // @0x17de90: value = p->value.fBlend
public:
	CTerrainTextureBlendColor() {}
	CTerrainTextureBlendColor( CFuncBase<STerrainTextureBlend> *_p ): p( _p ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetTerrainStressMode( bool bStress );
void SetTerrainLoadingMode( bool bLoad );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
