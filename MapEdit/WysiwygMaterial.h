#ifndef __IWYSIWYGMATERIAL_H_
#define __IWYSIWYGMATERIAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class IGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class IWorld;
	class IBuilding;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera;
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygMaterial: public CObjectBase
{
	OBJECT_BASIC_METHODS(CWysiwygMaterial);
	bool bLBDown;
	bool bBuildingUpdated;
	int nBuildingUpdated;

	void UpdateBuilding( NWorld::IBuilding *pObj );

public:
	CWysiwygMaterial( NWorld::IWorld *pWorld = 0, NGScene::IGameView *pScene = 0, ICamera *pCamera = 0 );

	void Update( CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal );

	void OnLButtonDown( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal );
	bool SetActiveMaterial( const CVec2 &ptPos, CObjectBase *pObj, int nUserID, const CVec3 &ptCrossNormal );
	void OnLButtonUp( const CVec2 &ptPos );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IWYSIWYGMATERIAL_H_
