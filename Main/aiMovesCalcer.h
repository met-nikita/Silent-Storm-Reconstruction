#ifndef __aiMovesCalcer_H_
#define __aiMovesCalcer_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiGrid.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCollider;
struct SDoorColliderAnalyzer;
class CMovesCalcer
{
	typedef CNodesLayer::STile STile;
	STempArrayGroup<STile> &tempArrays;
	CPtr<IAIMap> pMap;
	CTRect<int> region;
	CArray2D<char> sphereHeight, flags;
	CPtr<CNodesLayer> pLayer;
	int nGroupLayer;
	int nGroupFloor;
	CPtr<CPathNetwork> pNet;
	CNodesLayer::CTransitionsHash transitions;
	STile fake;
	CPtr<CCollider> pCollider;
	struct SPossibleTransition
	{
		SPathPlace from, to;
	};
	
	void TestMove( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, const CTRect<int> &region,
		float fHeightDiffLimit, int nSphereHeight, 
		float fHeight, char nFwdFlag, char nBackFlag, CArray2D<char> *pTested );
	void TestMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, const CTRect<int> &region,
		float fHeightDiffLimit, char nFwdFlag, char nBackFlag );
	void TestHCMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CTPoint<int> &shift, 
		const CTRect<int> &region, float fHeightDiffLimit,
		char nFwdFlag, char nBackFlag );
	//void TestLadders( NAI::CCollider *pCollider, vector<char> *pRes );
	//void TestWLadders( NAI::CCollider *pCollider, vector<char> *pRes );
	void CreateGrid2GridCandidates( IAIMap *pMap, 
		const CTRect<int> &src, CNodesLayer *pSrc, CArray2D<STile> &srcTiles, CTPoint<int> &srcOrigin,
		const CTRect<int> &dest, CNodesLayer *pDst, CArray2D<STile> &dstTiles, CTPoint<int> &dstOrigin,
		vector<SSphere> *pSpheres, vector<CVec3> *pMoves, vector<SPossibleTransition> *pPossibilities,
		bool bDoRefresh );
	void NormalizeTransitions( vector<SPossibleTransition> *pRes );
	void MarkNativePoints( CArray2D<STile> *pRes );
	void MarkSame( CArray2D<STile> *pRes );
	STile &GetFlipperTile( int x, int y, const SDoorColliderAnalyzer &analyzer, const STile &t, unsigned char *pNFlipper );
public:
	CMovesCalcer( CNodesLayer *_pLayer, IAIMap *_pMap, const CTRect<int> &_region, 
		STempArrayGroup<STile> &_tempArrays, int _nGroupLayer, int _nGroupFloor, CCollider *_pCollider );
	void Calc();
};
//////////////////////////////////////////////////////////////////////////
class CLadderCalcer
{
	CPtr<CNodesLayer> pLayer;
	CPtr<CPathNetwork> pNet;
	CPtr<IAIMap> pMap;
	CNodesLayer::SLadder *pLadder;
	vector<char> *pRes;
public:
	CLadderCalcer( CNodesLayer *_pLayer, IAIMap *_pMap, CNodesLayer::SLadder *_pLadder, vector<char> *_pRes );
	void Calc();
};	
}
#endif
