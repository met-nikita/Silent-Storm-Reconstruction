#ifndef __PLACEMENTDEFS_
#define __PLACEMENTDEFS_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

extern string IToA( int n );
extern void RotatePt( CVec2 *pVec, int nAngle );
const char TERRAIN_DIR[] = "Terrain\\";
const char BUILDING_DIR[] = "Buildings\\";
const int MIN_FLOOR = -10;
const int MAX_FLOOR = 10;

#include "..\Main\BuildingInfo.h"
void MakeBuildingInfo( const CPlacement *pPl, NBuilding::CBuildInfo *pInfo );
inline bool operator==( const NBuilding::SBuildFragment &a, const NBuilding::SBuildFragment &b )
{
	return a.nConstructionPartID == b.nConstructionPartID && a.nSubBlockID == b.nSubBlockID && a.ptPos == b.ptPos
		&& a.nRotationID == b.nRotationID && a.nFragmentID == b.nFragmentID;
}
#endif // __PLACEMENTDEFS_
