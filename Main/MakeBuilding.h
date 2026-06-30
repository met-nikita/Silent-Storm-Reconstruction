#ifndef __MakeBuilding_H_
#define __MakeBuilding_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "BuildingClip.h"
#include "BuildingPart.h"
#include "dg.h"
#include "../DBFormat/DataFormat.h"
#include "../DBFormat/DataRPG.h"
namespace NBuilding
{
class CBuildingGrid;
class CBuildInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
const int WALL_THIN_ID  = 1;
const int WALL_MED_ID   = 2;
const int WALL_THICK_ID = 3;
const float WALL_THIN  = 0.1f;
const float WALL_MED   = 0.2f;
const float WALL_THICK = 0.3f;
////////////////////////////////////////////////////////////////////////////////////////////////////
float ID2Width( int nWidthID );
int   Width2ID( float fWidth );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStoreyInfo
{
	struct SSpot
	{
		CVec3 ptOrigin;
		CVec3 ptNormal;
		CVec2 ptSize;
		int   nRotation;
		CDBPtr<NDb::CMaterial> pMaterial;
		int   nMaterialMask; // если бит включен - на этот материал спот не накладываетс€
		SSpot() {}
		SSpot( const CVec3 &ptO, const CVec3 &ptN, const CVec2 &ptS, int _nRotation, NDb::CMaterial *pM, int nMatMask )
			: ptOrigin(ptO), ptNormal(ptN), ptSize(ptS), nRotation(_nRotation), pMaterial(pM), nMaterialMask(nMatMask)
		{
		}
	};
	struct SFragment
	{
		int nFragmentID;
		CDBPtr<NDb::CRPGArmor> pArmor;
		SClipInfo info;
		vector<SSpot> spots;

		SFragment() {}
		SFragment( const SClipInfo &clip, int nFragID, NDb::CRPGArmor *_pArmor )
			: info(clip), nFragmentID( nFragID ), pArmor(_pArmor) {}
	};
	int nFloor;
	vector<SFragment> fragments;
	vector<SFragment> walls;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBuildingInfo
{
	unordered_map<SPart, SStoreyInfo, SPart> info;
	SStoreyInfo& GetPart( const SPart &part )
	{
		SStoreyInfo &s = info[part];
		s.nFloor = part.nFloor;
		return s;
	}
	//void Clear() { stories.clear(); }
	void Clear() { info.clear(); }
	void Erase( const SPart &part )
	{
		unordered_map<SPart, SStoreyInfo, SPart>::iterator i = info.find( part );
		if ( i != info.end() )
			info.erase( i );
	}

};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSolidAndWallMap;
class CBuildingInfoHold: public CObjectBase
{
	OBJECT_BASIC_METHODS(CBuildingInfoHold);
	SBuildingInfo info;
	ZDATA
	CDGPtr<CBuildingGrid> pBuildingGrid;
	CDGPtr< CPtrFuncBase<CBuildInfo> > pBuildInfo;
	bool bSplitParts;
	CObj<CSolidAndWallMap> pSWMap;
	CObj<CObjectBase> pPrecacheBuilding;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pBuildingGrid); f.Add(3,&pBuildInfo); f.Add(4,&bSplitParts); f.Add(5,&pSWMap); f.Add(6,&pPrecacheBuilding); return 0; }
private:
	void RecalcInfo();
public:
	CBuildingInfoHold() {}
	CBuildingInfoHold( CBuildingGrid *pGrid, CSolidAndWallMap *pSWMap, int nBuildingID, bool bSplitParts = false );
	const SBuildingInfo& GetInfo() const { return info; }
	void UpdateInfo();
	void UpdateInfo( const vector<SPart> &parts );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildingHP( CBuildInfo *pBuildInfo, CBuildingGrid *pGrid, CSolidAndWallMap *pSWMap );
bool UpdateBuildingStability( int nBuildingID, CBuildingGrid *pGrid, CSolidAndWallMap *pSWMap, int nMaxIterations = 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
// объ€вление вынесено дл€ WYSIWYG
class CBuildingSchema;
void MakeBuildingSchema( CBuildingSchema *pSchema, CBuildingGrid *pGrid, CBuildInfo *pBuildInfo, CSolidAndWallMap *pSWMap );
CSolidAndWallMap* MakeSWMap( int nBuildingID, const SRandomSeed &_seed );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
