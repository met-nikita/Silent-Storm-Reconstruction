#ifndef __BSCHEMAVIEWER_H_
#define __BSCHEMAVIEWER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	class CBuildInfo;
	class CBuildingGrid;
	class CSolidAndWallMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class IGameView;
	class I2DGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* ViewBuildingSchema( NGScene::IGameView *pScene, NBuilding::CSolidAndWallMap *pSWMap, NBuilding::CBuildInfo *pBInfo, NBuilding::CBuildingGrid *pBGrid, const SFBTransform &pos );
CObjectBase* ViewBuildingSchema( NGScene::IGameView *pScene, NBuilding::CSolidAndWallMap *pSWMap, int nBuildingID, NBuilding::CBuildingGrid *pBGrid, const SFBTransform &pos );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BSCHEMAVIEWER_H_