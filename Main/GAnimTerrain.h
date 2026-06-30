#ifndef __GANIMTERRAIN_H_
#define __GANIMTERRAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GAnimBase.h"
#include "GAnimParticles.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTestTerrain : public ITerrainFunction
{
	OBJECT_BASIC_METHODS(CTestTerrain);
public:
	virtual float GetHeight( float fX, float fY );
	virtual CVec3 GetNormal( float fX, float fY );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CATerrain : public CAnimator
{
	OBJECT_BASIC_METHODS(CATerrain);
public:
	CPtr<CAnimator> pInput;
	CPtr<ITerrainFunction> pTerrain;

	CATerrain() {}

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CATerrainCrawl : public CAParticle
{
	OBJECT_BASIC_METHODS(CATerrainCrawl);
public:
	CPtr<CAnimator> pInput;
	CPtr<ITerrainFunction> pTerrain;

	CATerrainCrawl() {}

	void Init();
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CATerrainSimple : public CAnimator
{
	OBJECT_BASIC_METHODS(CATerrainSimple);
public:
	CPtr<CAnimator> pInput;
	CPtr<ITerrainFunction> pTerrain;

	CATerrainSimple() {}

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif