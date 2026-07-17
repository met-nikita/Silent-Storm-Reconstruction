#ifndef __WTERRAIN_H_
#define __WTERRAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "wInterfaceVisitors.h"
#include "sync.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CGrassTracker;
};
namespace NTerrain
{
	class CBuilder;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapHole;
struct SMapWall;
namespace NWorld
{
class CTerrainRegion;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrain : public IVisObj
{
	OBJECT_NOCOPY_METHODS(CTerrain);
protected:
	ZDATA
	CSyncSrcBind<IVisObj> bindGlobal;
	CPtr<CFuncBase<STime> > pTime;
	CObj<NTerrain::CBuilder> pBuilder;
	CPtr<CTerrainInfoHolder> pInfo;
	vector<CObj<CTerrainRegion> > regions;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bindGlobal); f.Add(3,&pTime); f.Add(4,&pBuilder); f.Add(5,&pInfo); f.Add(6,&regions); return 0; }

public:
	CTerrain() {}
	CTerrain( CSyncSrc<IVisObj> *pShow, CTerrainInfoHolder *pTerrainInfo, NAI::IAIMap *pAIMap, CFuncBase<STime> *_pTime, int nDefaultFloor, const list<SMapHole> &holesList, const list<SMapWall> &wallsList );

	void Visit( IAIVisitor *pVisitor );
	void Visit( IRenderVisitor *pVisitors );

	void Update( bool bVisible = true ); // MapEditor

	// retail @0x38ab50: burn an explosion crater into the grass -- scorch the grass-colour grid toward
	// black by 1-(d/r)^4 within fRadius, clear density cells + prune blades inside the r*0.4 core, then
	// UpdateRegionGrass on the grown bbox. Called from the explosion tracker's finish (MakeDamage tail).
	void DrawExplosion( const CVec3 &vCenter, float fRadius );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} /// NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
