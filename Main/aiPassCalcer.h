#ifndef __aiPassCalcer_H_
#define __aiPassCalcer_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiGrid.h"
#include "aiRender.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
//class CHeightMapBlockInfo;
class CGroupHeightInfo
{
	CArray2D<unsigned short> heights;
public:
	void Init( int nXSize, int nYSize ) { heights.SetSizes( nXSize, nYSize ); heights.FillZero(); }
	void Set( int nX, int nY, float f ) { unsigned short &nRes = heights[nY][nX]; nRes = Max( nRes, NAI::GetIHeight( f ) ); }
	unsigned short GetIHeight( int nX, int nY ) const { return heights[nY][nX]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail height-field element (PDB SPCHeight, 4 bytes): per supersample per (floor,layer) slot the
// pass calcer records the surface height AND the surface's native source floor (terrain = 0,
// object = its record floor; 100 = empty slot). CreateAdditionalLayers @0x86c50 fills it (marking
// a supersample NATIVE when height OR floor changes vs the slot below) and CalcPoseInPoint
// @0x862e0 stamps the floor into STile::nFloor -- the channel CPathNetwork::GetFloor reads.
struct SPCHeight
{
	unsigned short nHeight;
	short nFloor;
	SPCHeight(): nHeight( 0 ), nFloor( 100 ) {}
	bool operator==( const SPCHeight &op ) const { return nHeight == op.nHeight && nFloor == op.nFloor; }
	bool operator!=( const SPCHeight &op ) const { return !( *this == op ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCollider;
struct SDoorColliderAnalyzer;
class CPassCalcer
{
	struct SLadderInfo
	{
		int x;
		int y;
		int upX;
		int upY;
		float fHeight;
		int nNativeFloor;
		SLadderInfo( int _x, int _y, int _upX, int _upY, float _fHeight, int nF ): 
			x(_x), y(_y), upX(_upX), upY(_upY), fHeight(_fHeight), nNativeFloor( nF ) {}
	};
// 1x1 - sized arrays
	STempArrayGroup<STile> &tempArrays;
	STempArrayGroup<char> sphereHeights;
// 3x3 - sized arrays
	STempArrayGroup<char> tempFlags;
	STempArrayGroup<SPCHeight> tempHeights;   // retail: height + native source floor per slot
	CArray2D<char> nIntersectsOnFloor[ N_MAX_FLOORS ];
// non-arrays
	CPtr<IAIMap> pMap;
	CPtr<CLayersGroup> pGroup;
	CPtr<CPathNetwork> pNet;
	CFastRenderer render;
	char nLayersMustHave[ N_MAX_FLOORS ];
	CTRect<int> region;
	vector< SLadderInfo > ladders;
	STile fake;
	CPtr<CCollider> pCollider;

	void CutBorders( int nFloor, int nLayer );
	//! return number of normal points
	int MarkIgnored( int nFloor, int nLayer, const CArray2D<STile> & temp );
	void TestLayMovesInDirection( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CArray2D<STile> *pResPrev, 
		const CTPoint<int> &shift, float fHDiffLimit, const CArray2D<SDoorColliderAnalyzer> &checkSphOk, 
		char nFwdFlag, char nBackFlag, int nLayer );
	void TestLayMoves( NAI::CCollider *pCollider, CArray2D<STile> *pRes, const CArray2D<STile> *pResPrev, 
		const CArray2D<char> &flags, const CArray2D<SDoorColliderAnalyzer> &checkSphOk, int nLayer );
	void KillNonNativeLayMoves( CArray2D<STile> *pRes, const CTPoint<int> &shift, char nFwdFlag, char nBackFlag, const CArray2D<char> &flags );
	int CalcColliderSize( int nFloor, int nLayer, float *pMinH, float *pMaxH, CVec2 *pPMin, CVec2 *pPMax );
	int CalcPoseInPoint( int x, int y, NAI::CCollider *pCollider, const CArray2D<SPCHeight> &tempH,
		CArray2D<STile> *pTemp, const CArray2D<char> &flags, int nLayer, int nDisplacement, bool bLastLayer );
	void CalcDisplacements(
		NAI::CCollider *pCollider, const CArray2D<SPCHeight> &tempH, const CArray2D<SPCHeight> *pTempPrevH,
		CArray2D<STile> *pTemp, const CArray2D<STile> *pTempPrev, const CArray2D<char> &flags,
		const CArray2D<char> &sphereHeight, const CArray2D<char> *pSphereHeightPrev, int nLayer );
	float InactivePointHeight( int x, int y, const CArray2D<SPCHeight> &tempH );
	int GetNFloor( CFastRenderer::SResult *p );
	void CreateAdditionalLayers();
	bool IsInactivePoint( int x, int y, const CArray2D<SPCHeight> &tempH, const CArray2D<char> &flags );
	bool IsInactivePoint( int x, int y, float fH, const CArray2D<char> &flags );
	void MarkLadderUps();
	void MarkInactiveEnlargements( CArray2D<char> *pRes, const CTPoint<int> &shift,
		const CArray2D<SPCHeight> &tempH, const CFastRenderer &render );
	STile &GetFlipperTile( int x, int y, int nLayer, const SDoorColliderAnalyzer &analyzer, const STile &t, unsigned char *pNFlipper );
public:
	typedef CNodesLayer::STile STile;
	CPassCalcer( CPathNetwork *_pNet, CLayersGroup *_pGroup, IAIMap *_pMap,	STempArrayGroup<CNodesLayer::STile> &_tempArrays, const CTRect<int> &_region );
	void MakeInactiveForLadder( int x, int y, int upX, int upY, float fHeight, int nF ) 
	{	
		ladders.push_back( SLadderInfo( x, y, upX, upY, fHeight, nF ) );
	}
	void Calc();
	CCollider* GetCollider() { return pCollider; }
};
}
#endif
