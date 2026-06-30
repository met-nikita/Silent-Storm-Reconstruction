#ifndef __MakeBuildingInternal_H_
#define __MakeBuildingInternal_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MakeBuilding.h"
#include "BuildingInfo.h"
namespace NDb
{
	class CConstructionPart;
}
namespace NBuilding
{
struct SBuildFragment;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Вспомогательный класс, используемый для построения карты стен, перекрытий
template<class TValue> class CNodeMap
{
	int nW, nH;
	int nMinF, nMaxF;
	int nPlaneSz;
	vector<TValue> cells;

	int MakeInd( int nFloor, int nx, int ny ) const 
	{
		ASSERT( nx >= 0 && nx < nW && ny >= 0 && ny <= nH && nFloor >= nMinF && nFloor <= nMaxF );
		return (nFloor-nMinF) * nPlaneSz + ny * nW + nx; 
	}
public:
	CNodeMap() { Clear();}
	void Resize( int nMinFloor, int nMaxFloor, int nWidth, int nHeight )
	{
		ASSERT( nMinFloor <= nMaxFloor );
		nW = nWidth;
		nH = nHeight;
		nMinF = nMinFloor;
		nMaxF = nMaxFloor;
		nPlaneSz = nW * nH;
		cells.resize( nPlaneSz * ( nMaxF - nMinF + 1 ), TValue() );
	}
	void Clear()
	{
		nW = nH = 1; nMinF = nMaxF = 0;
		nPlaneSz = 0;
		cells.resize( 1 );
	}
	int GetWidth() const { return nW; }
	int GetHeight() const { return nH; }
	int GetMinFloor() const { return nMinF; }
	int GetMaxFloor() const { return nMaxF; }
	const vector<TValue>& GetValues() const { return cells; }
	//
	bool IsValidCoord( int nFloor, int nx, int ny ) const
	{
		const int ind = MakeInd( nFloor, nx, ny );
		return 0 <= ind && ind < cells.size();
	}
	//
	TValue& At( int nFloor, int nx, int ny )
	{
		const int ind = MakeInd( nFloor, nx, ny );
		ASSERT( ind >= 0 && ind < cells.size() );
		return cells[ind];
	}
	//
	const TValue& At( int nFloor, int nx, int ny ) const
	{
		return const_cast<CNodeMap<TValue>*>( this )->At( nFloor, nx, ny );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPriority // solid part prority
{
	SP_LOW = 0,
	SP_SECONDARY = 1,
	SP_PRIMARY = 2,
};
struct SFragmentPos
{
	const SBuildFragment *pFr;
	CPtr<NDb::CConstructionPart> pCPart;
	CVec3 nSubPos;
	int   nIndex;
	int   nHashID;
	SFragmentPos(): pFr(0), nSubPos(VNULL3) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSolidElement
{
	vector<SFragmentPos> fragments;
	EPriority nPrority;
	vector<SProjectedSpot> spots;
	SSolidElement(): nPrority(SP_LOW) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// структуры, используемые во время вычисления clip info
struct SGridElement
{
	SBuildFragment *pFragment;
	int nFragmentID;
	float fLength;
	float fThickness;
	float fHeight;
	int nClipGroup;
	ESide side;
	SGridElement() { memset( this, 0, sizeof(*this) ); nFragmentID = -1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGridNode
{
	vector<SGridElement> elems[4]; // по 4ем напрявлениям
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLRNeighbs
{
	CPtr<NDb::CConstructionPart> pCPart;
	SGridNode *pLeft;
	SGridNode *pRight;
	vector<SProjectedSpot> spots;
	SLRNeighbs() : pLeft(0), pRight(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSolidAndWallMap : public CVersioningBase
{
	OBJECT_NOCOPY_METHODS(CSolidAndWallMap);
	unordered_map<int, CNodeMap<SSolidElement> > solidMap;
	CArray2D<float> bottom;
	CNodeMap<SGridNode> wallGrid;
	vector<SLRNeighbs> neighbs;
	ZDATA
	CDGPtr< CPtrFuncBase<CBuildInfo> > pBuildInfo;
	SRandomSeed seed;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pBuildInfo); f.Add(3,&seed); return 0; }

	void MakeSolidMap( SRand *pRand, const vector<SBuildFragment> &solids, 
		int nMinFloor, int nMaxFloor, int nXSize, int nYSize,
		const vector<NBuilding::SLayerGroup> &groups );
	void MakeWallMap( SRand *pRand, vector<SBuildFragment> *pWallFrags );
	void MakeMaps( SRand *pRand, const vector<SBuildFragment> &solids, vector<SBuildFragment> *pWallFrags,
		int nMinFloor, int nMaxFloor, int nXSize, int nYSize,
		const vector<NBuilding::SLayerGroup> &groups );
	bool NeedUpdate() { return pBuildInfo.Refresh(); }
	void Recalc();
public:
	CSolidAndWallMap() {}
	CSolidAndWallMap( CPtrFuncBase<CBuildInfo> *_pBuildInfo, const SRandomSeed &_seed )
		: pBuildInfo(_pBuildInfo), seed(_seed) {}
	const unordered_map<int, CNodeMap<SSolidElement> >& GetSolidMap() const { return solidMap; }
	const CNodeMap<SGridNode>& GetWallGrid() const { return wallGrid; }
	const vector<SLRNeighbs>& GetNeighbs() const { return neighbs; }
	const CArray2D<float>& GetBottom() const { return bottom; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
