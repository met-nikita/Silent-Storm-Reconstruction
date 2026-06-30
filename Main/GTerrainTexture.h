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
class CTerrainTexture: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CTerrainTexture);
private:
	int nDetail;
	CObj<NGfx::CTexture> pTex128, pTex256;
	bool bHasToRecalc128, bHasToRecalc256;

	ZDATA
	bool bBumpTexture;
	SRandomSeed sSeed;
	CTRect<int> nrRegion;
	CDGPtr< CFuncBase<int> > pLOD;
	CDGPtr< CFuncBase< STerrainInfo > > pInfo;
	CPtr<CGrassTracker> pGrass;
	float fWorldToScreen;
	CDGPtr<CVersioningBase> pUpdateRegion;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bBumpTexture); f.Add(3,&sSeed); f.Add(4,&nrRegion); f.Add(6,&pInfo); f.Add(7,&pGrass); f.Add(8,&fWorldToScreen); f.Add(9,&pUpdateRegion); f.Add(10,&nDetail); return 0; }
	
	struct SSpotTextures
	{
		CPtrFuncBase<CSWTextureData> *pTex, *pBump;
	};
	bool CalcNewTexture( int nSize );
	void UseFake();
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
	CTerrainTexture(): bBumpTexture( false ), sSeed( 0 ) {}
	CTerrainTexture( bool _bBump, SRandomSeed _sSeed, const CTRect<int> &_nrRegion, 
		CFuncBase<int> *_pLOD, CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *_pGridNode, CGrassTracker *_pGrass );
	void FreeTexture( NGfx::CTexture *_pTex );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetTerrainStressMode( bool bStress );
void SetTerrainLoadingMode( bool bLoad );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif