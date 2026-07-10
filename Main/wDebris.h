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
	// retail CDFrozenItem +0x88 (PDB), ctor @0x34aaf0 trailing bool param (arg 7, stored @[ebx+0x88]),
	// save tag 10 (operator& @0x34c800, 1-byte chunk). Routes the item into the unit's per-update
	// tempVisibleObjects instead of the persistent visibleObjects in UpdateVisible @0x3c4450.
	// (Init'd false here for pre-tag-10 dev saves; retail's default ctor leaves it untouched, but every
	// retail save carries the tag so garbage is unobservable there.)
	bool bIsTemporaryVisible;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&model); f.Add(3,&m); f.Add(4,&nFloor); f.Add(5,&pInvItem); f.Add(6,&bindGlobal); f.Add(7,&nVP); f.Add(8,&nMaxVP); f.Add(9,&pTrash); f.Add(10,&bIsTemporaryVisible); return 0; }
public:
	CDFrozenItem() : bIsTemporaryVisible( false ) {}
	CDFrozenItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, const SHMatrix &m,
		int _nFloor, NRPG::IInventoryItem *_pItem, CDebrisControllerTrash *_pTrash, bool _bIsTemporaryVisible = false );
	const SHMatrix& GetMatrix() const { return m; }
	CVec3 GetPos() const { return m.GetTranslation(); }
	int GetFloor() const { return nFloor; }
	const SItemRenderInfo& GetModel() const { return model; }
	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );
	// retail IVisible triple (see wVision.h): probe points @0x349e50 (mass-sphere centres through the
	// item matrix, fallback the translation), the temporary flag @0x34c2d0 (+0x88 through the
	// IVisible-base this), no visibility parent @0x3538d0 (xor eax,eax).
	virtual void GetVisiblePos( vector<CVec3> *pRes ) const;
	virtual bool IsTemporaryVisible() const { return bIsTemporaryVisible; }
	virtual CObjectBase* GetVisibilityParent() const { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CDItem (PDB): bases IVisObj @0, IVisible @8 -- the in-flight item IS an IVisible so
// CUnitServer::UpdateVisible's dynamic-items loop (@0x7c5341) can query it via the interface.
class CDItem: public IVisObj, public IVisible
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
	// retail ctor @0x34acc0 takes a trailing CObjectBase* pVisibilityParent (stored @+0x2c, save tag 7):
	// the unit the item flew off of. Non-null == "this flying item is fog-gated"; Segment reads it back
	// at physics-settle so the frozen form inherits the gate (see AddFrozenItem @0x34b100).
	CDItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, CFuncBase<NAnimation::SSkeletonPose> *_pAnim,
		int _nFloor, NRPG::IInventoryItem *_pItem, CObjectBase *_pVisibilityParent = 0 );
	CFuncBase<NAnimation::SSkeletonPose>* GetAnimation() const { return pAnimation; }
	const SItemRenderInfo& GetModel() const { return model; }
	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	void SetFloor( int _nFloor ) { nFloor = _nFloor; }
	int GetFloor() const { return nFloor; }
	// retail CDItem::GetPos @0x34a030: the CURRENT physics position -- refresh the skeleton animation
	// and read the root bone (used by FilterVisibleItems<CDItem> @0x34bd40 for the gather range test).
	CVec3 GetPos() const;
	//
	virtual void Visit( IRenderVisitor *p );
	// retail IVisible triple (disasm-proven ICF stubs, see wVision.h): NO probe points (@0x58b790
	// `ret 4` -- an in-flight item is never LOS-rayed), always "temporary" (@0x488d00 `mov al,1`),
	// parent = pVisibilityParent (@0x773810 `mov eax,[ecx+0x24]` == +0x2c through the IVisible base).
	virtual void GetVisiblePos( vector<CVec3> *pRes ) const {}
	virtual bool IsTemporaryVisible() const { return true; }
	virtual CObjectBase* GetVisibilityParent() const { return pVisibilityParent; }
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
	// retail CDebrisController +0x38 (PDB), save tag 10 (operator& @0x37ab30): the in-flight items
	// published as vision candidates (AddDebris @0x74b04a pushes here under the fog gate). Walked by
	// GetVisibleDynamicItems @0x34a420 for UpdateVisible's dynamic-items loop.
	// (Retail also carries clueItems @+0x30 tag 8, showFrozenItems tag 9 -- NOT ported: dev's clue
	// lookup lives in nameToObj; dev keeps showFrozenItems at its historical tag 8.)
	list<CPtr<CDItem> > visibleDynamicItems;
protected:
	list<CObj<CDFrozenItem> > showFrozenItems;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&items); f.Add(3,&pDebrisAction); f.Add(4,&bc); f.Add(5,&showItems); f.Add(6,&pTrash); f.Add(7,&visibleItems); f.Add(8,&showFrozenItems); f.Add(10,&visibleDynamicItems); return 0; }
private:
	void Add( CDItem *pD, NAnimation::CASphereSet *pAnim );
	void InnerSegment( list<STrackItem> *pRes );
	void GetInSphere( const SSphere &sphere, list<CObj<CDFrozenItem> > *pRes );
	void InitAction() { if ( !IsValid( pDebrisAction ) ) pDebrisAction = CreateActionCounter(); }
	// retail inner @0x34b100: AddFrozenItem(pMap, m, pInvItem, model, nFloor, bTemporaryVisible,
	// bVisibleGated) -- disasm-mapped: show-list select is (pInvItem==0 && !bVisibleGated) ?
	// GetShowList : GetVisibleShowList, the item is published into visibleItems (the LOS/vision
	// candidate list) when (pInvItem!=0 || bVisibleGated) -- so a fog-gated capless item stays hidden
	// until a unit gains LOS on it -- and bTemporaryVisible feeds the CDFrozenItem ctor's trailing
	// flag (@0x74b15b, ctor arg 7 -> +0x88). (dev has no pMap param: the AI-hull registration runs in
	// CDFrozenItem::Visit instead of retail's pMap->...->VisitObject tail.)
	CDFrozenItem* AddFrozenItem( const SHMatrix &m, NRPG::IInventoryItem *pInvItem, const SItemRenderInfo &_model, int nFloor, bool bTemporaryVisible = false, bool bVisibleGated = false );
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
	//! add new piece of debris. pVisibilityParent: retail AddDebris @0x34ade0 carries a CObjectBase*
	//! visibility-parent (the dying unit, DropItems @0x3502c0 passes its this-adjusted CObjectBase for
	//! every drop) SEPARATE from the inventory item -- the uniform CAP has no IInventoryItem but must
	//! still bind to the fog-gated list on an unseen death (the `pItem != 0` test alone let it leak).
	//! The parent is STORED on the CDItem (+0x2c, save tag 7) so Segment can re-derive the gate at
	//! physics-settle and route the frozen form to the fog-gated list + vision-candidate visibleItems.
	void AddDebris( const SItemRenderInfo &_model, NAI::IAIMap *pMap, const CVec3 &ptCenter, const CQuat &q, const CVec3 &velocity,
		CFuncBase<STime> *pTime, NRPG::IInventoryItem *pItem = 0, CObjectBase *pVisibilityParent = 0 );
	//! turn in radius frozen items into alive ones
	void ActivateDebris( const SSphere &b, NAI::IAIMap *pAIMap, CFuncBase<STime> *pTime );
	//! put RPG item into fixed position. retail public overload @0x34b340:
	//! (pMap, pos, rot, pInvItem, bool bTemporaryVisible, int nFloor) -- the bool threads to the inner
	//! overload's bTemporaryVisible (@0x74b45d passes it as arg 6, bVisibleGated hardwired 0).
	CDFrozenItem* AddFrozenItem( const CVec3 &pos, const CQuat &rot, NRPG::IInventoryItem *pInvItem, bool bTemporaryVisible = false, int nFloor = 0 );
	//! remove frozen item by RPG pointer
	void RemoveFrozenItem( NRPG::IInventoryItem *pInvItem );
	//! get frozen (on-ground) items that can be seen in some area (retail @0x34a400, ctrl vtbl+0x1c)
	void GetVisibleItems( const SSphere &sphere, list<IVisible*> *pRes );
	//! get in-flight (dynamic) vision-candidate items in some area (retail @0x34a420, ctrl vtbl+0x20:
	//! FilterVisibleItems<CDItem> over visibleDynamicItems; range test via CDItem::GetPos @0x34a030)
	void GetVisibleDynamicItems( const SSphere &sphere, list<IVisible*> *pRes );
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