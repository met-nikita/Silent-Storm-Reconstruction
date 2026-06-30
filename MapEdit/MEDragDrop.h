#ifndef __MEDRAGDROP_H_
#define __MEDRAGDROP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NGScene
{
	class IGameView;
}
namespace NWysiwyg
{
	class ISelection;
}
namespace NBuilding
{
	class CBuildingGrid;
	class CSolidAndWallMap;
}
class ICamera;
class IDragDrop: public CObjectBase
{
public:
	virtual void Update() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IDragDrop* CreateDragAndDrop( int nWorldID, NGScene::IGameView *pScene, ICamera *pCamera, NWysiwyg::ISelection *pSelection, NBuilding::CBuildingGrid *pGrid, NBuilding::CSolidAndWallMap *pSWMap );
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MEDRAGDROP_H_
