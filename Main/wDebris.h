#ifndef __WDEBRISCONTROLLER_H_
#define __WDEBRISCONTROLLER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wInterface.h"
#include "wInterfaceVisitors.h"
#include "wVision.h"
#include "Bound.h"
#include "RPGAttackMech.h"
namespace NRPG
{
	class CAttackPortion;
}
namespace NDb
{
	class CRPGArmor;
}
namespace NAnimation
{
	class CASphereSet;
}
namespace NWorld
{
class	CDFrozenItem;
class CActionCounter;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDebrisControllerTrash: public CObjectBase
{
	OBJECT_BASIC_METHODS( CDebrisControllerTrash );
	ZDATA
public:
	list< CPtr<CDFrozenItem> > itemsToRemove;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&itemsToRemove); return 0; }
	//
	CDebrisControllerTrash() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SItemRenderInfo
{
	ZDATA
	CPtr<NDb::CModel> pModel;
	CPtr<CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pModel); f.Add(3,&pUnit); return 0; }
	SItemRenderInfo() {}
	SItemRenderInfo( NDb::CModel *_pModel ) : pModel(_pModel) {}
	SItemRenderInfo( CUnit *_pUnit ) : pUnit(_pUnit) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDFrozenItem: public IItem, public NRPG::IAttackable, public IVisObj, public IVisible
{
	OBJECT_NOCOPY_METHODS(CDFrozenItem);
	ZDATA
	SItemRenderInfo model;
	SHMatrix m;
	int nFloor;
	CObj<NRPG::IInventoryItem> pInvItem;
	CSyncSrcBind<IVisObj> bindGlobal;
	int nVP, nMaxVP;
	CPtr<CDebrisControllerTrash> pTrash;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&model); f.Add(3,&m); f.Add(4,&nFloor); f.Add(5,&pInvItem); f.Add(6,&bindGlobal); f.Add(7,&nVP); f.Add(8,&nMaxVP); f.Add(9,&pTrash); return 0; }
public:
	CDFrozenItem() {}
	CDFrozenItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, const SHMatrix &m, 
		int _nFloor, NRPG::IInventoryItem *_pItem, CDebrisControllerTrash *_pTrash );
	const SHMatrix& GetMatrix() const { return m; }
	CVec3 GetPos() const { return m.GetTranslation(); }
	int GetFloor() const { return nFloor; }
	const SItemRenderInfo& GetModel() const { return model; }
	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );
	virtual CVec3 GetVisiblePos() const { return m.GetTranslation(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDItem: public IVisObj
{
	OBJECT_NOCOPY_METHODS(CDItem);
	ZDATA
	SItemRenderInfo model;
	CObj< CFuncBase<NAnimation::SSkeletonPose> > pAnimation;
	int nFloor;
	CObj<NRPG::IInventoryItem> pInvItem;
	CSyncSrcBind<IVisObj> bindGlobal;
		CPtr<CObjectBase> pVisibilityParent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&model); f.Add(3,&pAnimation); f.Add(4,&nFloor); f.Add(5,&pInvItem); f.Add(6,&bindGlobal); f.Add(7,&pVisibilityParent); return 0; }
public:
	CDItem() {}
	CDItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, CFuncBase<NAnimation::SSkeletonPose> *_pAnim,
		int _nFloor, NRPG::IInventoryItem *_pItem );
	CFuncBase<NAnimation::SSkeletonPose>* GetAnimation() const { return pAnimation; }
	const SItemRenderInfo& GetModel() const { return model; }
	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	void SetFloor( int _nFloor ) { nFloor = _nFloor; }
	int GetFloor() const { return nFloor; }
	//
	virtual void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDebrisController
{
	struct STrackItem
	{
		ZDATA
		CPtr<CDItem> pItem;
		CPtr<NAnimation::CASphereSet> pAnim;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); f.Add(3,&pAnim); return 0; }
	};
	ZDATA
	list<STrackItem> items;
	CObj<CActionCounter> pDebrisAction;
	SBoundCalcer bc;
private:
	list<CObj<CDItem> > showItems;
	CObj<CDebrisControllerTrash> pTrash;
	list<CPtr<CDFrozenItem> > visibleItems;
protected:
	list<CObj<CDFrozenItem> > showFrozenItems;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&items); f.Add(3,&pDebrisAction); f.Add(4,&bc); f.Add(5,&showItems); f.Add(6,&pTrash); f.Add(7,&visibleItems); f.Add(8,&showFrozenItems); return 0; }
private:
	void Add( CDItem *pD, NAnimation::CASphereSet *pAnim );
	void InnerSegment( list<STrackItem> *pRes );
	void GetInSphere( const SSphere &sphere, list<CObj<CDFrozenItem> > *pRes );
	void InitAction() { if ( !IsValid( pDebrisAction ) ) pDebrisAction = CreateActionCounter(); }
	CDFrozenItem* AddFrozenItem( const SHMatrix &m, NRPG::IInventoryItem *pInvItem, const SItemRenderInfo &_model, int nFloor );
protected:
	bool Segment( SSphere *pInvalidate );
	bool HasDynamicItems() { return !items.empty(); }
	virtual CActionCounter* CreateActionCounter() = 0;
	virtual CSyncSrc<IVisObj>* GetShowList() = 0;
	virtual CSyncSrc<IVisObj>* GetVisibleShowList() = 0;
	virtual void OnFrozenItemDestroyed( int nItemID ) = 0;
	virtual void CreateParticle( const CVec3 &ptPos, const CQuat &rot, NDb::CEffect *pEffect, int nFloor ) = 0;
	virtual STime GetWorldTime() = 0;
public:
	CDebrisController() { pTrash = new CDebrisControllerTrash(); }
	//! add new piece of debris
	void AddDebris( const SItemRenderInfo &_model, NAI::IAIMap *pMap, const CVec3 &ptCenter, const CQuat &q, const CVec3 &velocity, 
		CFuncBase<STime> *pTime, NRPG::IInventoryItem *pItem = 0 );
	//! turn in radius frozen items into alive ones
	void ActivateDebris( const SSphere &b, NAI::IAIMap *pAIMap, CFuncBase<STime> *pTime );
	//! put RPG item into fixed position
	CDFrozenItem* AddFrozenItem( const CVec3 &pos, const CQuat &rot, NRPG::IInventoryItem *pInvItem, int nFloor = 0 );
	//! remove frozen item by RPG pointer
	void RemoveFrozenItem( NRPG::IInventoryItem *pInvItem );
	//! get items that can be seen in some area
	void GetVisibleItems( const SSphere &sphere, list<IVisible*> *pRes );
	//! the on-ground (frozen) world item carrying a given RPG inventory item, or 0 (release GetWorldItem
	//! @0x774cd0 -> GetFrozenItem @0x749c20: first showFrozenItems entry whose GetInvItem() matches).
	CDFrozenItem* GetFrozenItem( NRPG::IInventoryItem *pInvItem );
	//! the on-ground (frozen) world item whose RPG db-record id == nDbID, or 0 (release luaFindItem
	//! @0x2e7170: first item whose GetInvItem()->GetDBItem() record id matches the script number param).
	CDFrozenItem* FindFrozenItem( int nDbID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif