#include "StdAfx.h"
#include "aiVision.h"
#include "aiMap.h"
#include "Transform.h"
#include "aiRender.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceSide( NAI::IAIMap *pMap, CFastRenderer *pRes, const CVec3 &ptFrom, int nSide, 
	int fMaxDistance, int nHalfSize, int nTSFlags )
{
	CVec3 ptDir;
	switch ( nSide )
	{
		case 0: ptDir = CVec3(1,0,0); break;
		case 1: ptDir = CVec3(-1,0,0); break;
		case 2: ptDir = CVec3(0,1,0); break;
		case 3: ptDir = CVec3(0,-1,0); break;
		case 4: ptDir = CVec3(0,0,1); break;
		case 5: ptDir = CVec3(0,0,-1); break;
		default: ASSERT(0); break;
	}
	SHMatrix cameraPos;
	MakeMatrix( &cameraPos, ptFrom, ptDir );
	pRes->InitProjective( cameraPos, fMaxDistance, 90, nHalfSize );
	pMap->TraceGrid( pRes, nTSFlags, IAIMap::STH_SORT_INTERVALS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetIndex( SCalcIndex *sIndex, const CVec3 &ptFrom, const CVec3 &ptTarget, int nHalfSize )
{
	float fdx = ptTarget.x - ptFrom.x;
	float fdy = ptTarget.y - ptFrom.y;
	float fdz = ptTarget.z - ptFrom.z;
	int nSide;
	float fLu4x, fLu4y;
	CVec3 pt(0, 0, 0);
	if ( (fabs(fdx) >= fabs(fdy)) && (fabs(fdx) >= fabs(fdz)) ) 
	{
		float f1dx = 1 / fdx;
		if ( fdx > 0 )
		{
			nSide = 0;
			fLu4y = nHalfSize + fdz * f1dx * nHalfSize;
			fLu4x = nHalfSize - fdy * f1dx * nHalfSize;
		}
		else
		{
			nSide = 1;
			fLu4y = nHalfSize - fdz * f1dx * nHalfSize;
			fLu4x = nHalfSize - fdy * f1dx * nHalfSize;
		}
	}
	else if( (fabs(fdy) >= fabs(fdx)) && (fabs(fdy) >= fabs(fdz)) ) 
	{
		float f1dy = 1/fdy;
		if( fdy > 0 )
		{
			nSide = 2;
			fLu4y = nHalfSize + fdz * f1dy * nHalfSize;
			fLu4x = nHalfSize + fdx * f1dy * nHalfSize;
		}
		else
		{
			nSide = 3;
			fLu4y = nHalfSize - fdz * f1dy * nHalfSize;
			fLu4x = nHalfSize + fdx * f1dy * nHalfSize;
		}
	}
	else//if( (fabs(dz) > fabs(dx)) && (fabs(dz) > fabs(dy)) )
	{
		float f1dz = 1/fdz;
		if( fdz > 0 )
		{
			nSide = 4;
			fLu4y = nHalfSize - fdx * f1dz * nHalfSize;
			fLu4x = nHalfSize - fdy * f1dz * nHalfSize;
		}
		else
		{
			nSide = 5;
			fLu4y = nHalfSize - fdx * f1dz * nHalfSize;
			fLu4x = nHalfSize + fdy * f1dz * nHalfSize;
		}
	}
	sIndex->nX = Clamp( Float2Int( fLu4x - 0.5f ), 0, nHalfSize * 2 - 1 );;
	sIndex->nY = Clamp( Float2Int( fLu4y - 0.5f ), 0, nHalfSize * 2 - 1 );
	sIndex->nSide = nSide;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NAI;
