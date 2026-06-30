#ifndef __wBuilding_H_
#define __wBuilding_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "MapBuildingInfo.h"
#include "wInterface.h"
#include "RPGAttackMech.h"
#include "wDynObject.h"
#include "wInterfaceVisitors.h"
#include "MapBuildingInfo.h"

namespace NBuilding
{
	class CBuildingInfoHold;
}
namespace NWorld
{
class CBuildingPart;
class CActionCounter;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilding: public NRPG::IAttackable, public IDynamicObject, public IBuilding, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CBuilding);
public:
	typedef unordered_map<int, CObj<CBuildingPart> > CPartsHash;
private:
	void ToggleUpdateFlag();
	void UpdateBuildingParts();
	void RenderDestructionEffects();   // retail @0x343310 -- dust/debris FX + collapse sounds at destroyed voxels
	ZDATA
	CObj<CActionCounter> pAction;
public:
	SMapBuilding info;
	CObj<CObjectBase> pRPG;
	CSyncSrcBind<IVisObj> bindGlobal;
	int nSegemntCnt;
	CPartsHash parts;
	CPtr<CSyncSrc<IVisObj> > pShow;
	CObj<NBuilding::CBuildingInfoHold> pBInfo;
	CObj<NBuilding::CBuildingInfoHold> pSplitBInfo;
	CPtr<IWorld> pWorld;
	bool bUpdatingStability;
	int  nActionCnt;
	SFBTransform originalpos;
	bool bNoAI;
	CObj<NBuilding::CSolidAndWallMap> pSWMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAction); f.Add(3,&info); f.Add(4,&pRPG); f.Add(5,&bindGlobal); f.Add(6,&nSegemntCnt); f.Add(7,&parts); f.Add(8,&pShow); f.Add(9,&pBInfo); f.Add(10,&pSplitBInfo); f.Add(11,&pWorld); f.Add(12,&bUpdatingStability); f.Add(13,&nActionCnt); f.Add(14,&originalpos); f.Add(15,&bNoAI); f.Add(16,&pSWMap); return 0; }
	
	CBuilding() {}
	CBuilding( CSyncSrc<IVisObj> *pShow, const SMapBuilding &info, IWorld *pWorld, bool bNoAI = false );
	// implement IAttackable
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	//NRPG::IAttackable* GetAttackable() { return pRPG; } /// for special purpose only!!!!
	void Explode( const CVec3 &ptEpic, int nPower );
	virtual const SMapBuilding& GetInfo() const { return info; }
	IWorld* GetWorld() const;
	void SetPosition( const SFBTransform &pos );
	//
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	virtual void Update();
	virtual CObjectBase* GetSceneHandle() const { return pBInfo.GetBarePtr(); }
	virtual bool Segment();
	virtual void UpdateAllParts();
};
	
}
#endif
