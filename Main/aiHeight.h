#ifndef __aiHeight_H_
#define __aiHeight_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\misc\2DArray.h"
namespace NAI
{
class IAIMap;
struct SHeightCalcInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeightMapBlockInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeightMapBlockInfo);
	void CalcCoords( float fX, float fY, float *pfCoeff );
public:
	CArray2D<float> height;
	CTPoint<int> ptShift;
	
	void Init( const CTRect<int> &rect );
	//void Move( CTPoint<int> &ptDest, CTRect<int> *pWasted1, CTRect<int> *pWasted2 );
	void Move( int nDestX, int nDestY );
	void ShowSpheres();
	float GetHeight( float fX, float fY );
	CVec3 GetNormal( float fX, float fY );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcHeightMapSize( CTRect<int> *pRes, const CVec3 &ptCenter, float fSize );
void CalcHeightMap( NAI::IAIMap *pMap, CHeightMapBlockInfo *pRes, const SHeightCalcInfo &hInfo );
void CalcHeightMap( NAI::IAIMap *pMap, CHeightMapBlockInfo *pRes, const float fH, const list<CVec3> &specialPoints );
// sampleSize == 0 means default sample size
void CheckGradient( CHeightMapBlockInfo *pRes, float fSampleSize = 0 );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
