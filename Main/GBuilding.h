#ifndef __GBUILDING_H_
#define __GBUILDING_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GResource.h"
#include "GMatShare.h"
#include "GScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CGeometry;
}
namespace NDb
{
	class CModel;
	class CMaterial;
}
namespace NBuilding
{
	class CBuildingGrid;
	class CBuildInfo;
	class CBuildingInfoHold;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class IGScene;
class IGameView;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLightCalcer: public CFuncBase<CVec3>
{
	OBJECT_BASIC_METHODS(CLightCalcer);
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();	
private:
	ZDATA
	int nBuildingID;
	int nLightGroupID;
	CDGPtr< CFuncBase<CVec3> > pGlobalAmbient;
	CDGPtr< CFuncBase<CVec3> > pLocalAmbient;
	CDGPtr<NBuilding::CBuildingGrid> pBuildingGrid;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBuildingID); f.Add(3,&nLightGroupID); f.Add(4,&pGlobalAmbient); f.Add(5,&pLocalAmbient); f.Add(6,&pBuildingGrid); return 0; }

	CLightCalcer() {}
	CLightCalcer( int nBuildingID, int nLightGroup, 
		CFuncBase<CVec3> *pGlobalAmb, CFuncBase<CVec3> *pLocalAmb, NBuilding::CBuildingGrid *pGrid );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLocalAmbientCalcer: public CFuncBase<CVec3>
{
	OBJECT_BASIC_METHODS(CLocalAmbientCalcer);
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();	
private:
	ZDATA
	vector<CDGPtr< CFuncBase<CVec3> > > roomLights;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&roomLights); return 0; }

	void AddLight( CFuncBase<CVec3> *pLight );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRoomAmbient
{
	int nTargetGroupID;
	CPtr<CFuncBase<CVec3> > pAmbientColor;

	SRoomAmbient() {}
	SRoomAmbient( int nTargetRoom, CFuncBase<CVec3> *pAmbient ): nTargetGroupID(nTargetRoom), pAmbientColor(pAmbient) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilding : public CObjectBase
{
	OBJECT_BASIC_METHODS(CBuilding);

	typedef unordered_map<int, CPtr<CLocalAmbientCalcer> > CRoomLightsHash;
	ZDATA
	CObj<CFBTransform> pPlace;
	CDGPtr<NBuilding::CBuildingGrid> pBuildingGrid;
	list< CObj<CObjectBase> > renderParts;
	//CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo;
	CPtr<NBuilding::CBuildingInfoHold> pBInfo;

	CRoomLightsHash lights;
	// данные, которые не подлежат сериализации
	bool bParts; // показывать кусками или одной моделькой
	bool bRefresh;
	int  nBuildInfoID;
	int  nPartID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPlace); f.Add(3,&pBuildingGrid); f.Add(4,&renderParts); f.Add(5,&pBInfo); f.Add(6,&lights); f.Add(7,&bParts); f.Add(8,&bRefresh); f.Add(9,&nBuildInfoID); f.Add(10,&nPartID); return 0; }

	//IMaterial* CreateMaterial( CMaterialShare *pMaterials, NDb::CMaterial *pMaterial, int nRoom, int nFloor );
	SGroupInfo GetGroupInfo( /*int _nRoom, */int nFloor, bool bIndestructible = false );
	void Build( IGScene *pScene, CMaterialShare *pMaterials );
public:
	CBuilding();
	bool Update( IGScene *pScene, CMaterialShare *pMaterials );
	void Setup( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI );
	void ToggleParts() { bParts = !bParts; bRefresh = true; }
	int  GetBuildInfoID() const { return nBuildInfoID; }
	void GetRoomLights( vector<SRoomAmbient> *pLights );
	void CheckHP();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GBUILDING_H_
