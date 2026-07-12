#ifndef __RPGITEMSET_H_
#define __RPGITEMSET_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "RPGItem.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
class CUnit;
class CAttackPortion;
////////////////////////////////////////////////////////////////////////////////////////////////////
//  
class CItem: virtual public IItem, public IJoinSplit
{
public:
	ZDATA
	int nQuantity;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nQuantity); return 0; }

	CItem(): nQuantity( 0 ) {}
	virtual bool Join( IJoinSplit *pItem );
	virtual IJoinSplit *Split( int nQuantity = 1 );
	
	virtual int GetQuantity() const { return nQuantity; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//      
class CInventoryItem: virtual public IInventoryItem, public CItem
{
	//OBJECT_BASIC_METHODS(CInventoryItem)
	ZDATA_(CItem)
	CDBPtr<NDb::CRPGItem> pDBItem;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CItem*)this); f.Add(2,&pDBItem); return 0; }

public:
	CInventoryItem() {}
	CInventoryItem( NDb::CRPGItem *pItem ): pDBItem( pItem ) {}

	int GetWeight() const;
	NDb::CRPGItem *GetDBItem() const { ASSERT(pDBItem); return pDBItem; };
	const CTPoint<int>& GetSize() const;
	NDb::EWeaponType GetWeaponType() const;

	virtual void OnEquip( CUnit *pOwner ) {}
	virtual void OnUnEquip( CUnit *pOwner ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//     (CIncItem)
template<class T>
class CItemContainer: public CInventoryItem, public IItemContainer
{
private:
	ZDATA_(CInventoryItem)
	CPtr<T> pItem;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CInventoryItem*)this); f.Add(2,&pItem); return 0; }

public:
	CItemContainer() {}
	CItemContainer( NDb::CRPGItem *pItem ): CInventoryItem( pItem ) {}

	void Load( T *p ) { pItem = p; }
	T* GetItem() const { return pItem; }
	virtual int GetIncQuantity() const { return pItem->nQuantity; }
	virtual IJoinSplit* SplitItem( int nQ )
	{
		nQ = Min( nQ, pItem->nQuantity );
		T *pResItem = pItem->Duplicate();
		pResItem->nQuantity = nQ;
		pItem->nQuantity -= nQ;
		return pResItem;
	}
	virtual bool JoinItem( IJoinSplit *pJoinItem )
	{
		CItem *pJItem = dynamic_cast<CItem*>(pJoinItem);
		if ( !pJItem )
			false;
		int nQ = Min( pJItem->nQuantity, GetMaxIncQuantity() - pItem->nQuantity );
		pItem->nQuantity += nQ;
		pJItem->nQuantity -= nQ;
		return true;
	}
	virtual bool Join( IJoinSplit *pJoinItem )
	{
		if ( !CInventoryItem::Join( pJoinItem ) )
			return false;
		CItemContainer<T> *pCJItem = dynamic_cast<CItemContainer<T>*>(pJoinItem);
		pItem->nQuantity += pCJItem->GetIncQuantity();
		return true;
	}
	virtual IJoinSplit* Split( int nQuantity = 1 )
	{
		int nQ = Min( nQuantity * GetMaxIncQuantity(), pItem->nQuantity );
		CItemContainer<T> *pNewCont = dynamic_cast<CItemContainer<T>*>( CInventoryItem::Split( nQuantity ) );
		pNewCont->pItem = pItem->Duplicate();
		pNewCont->pItem->nQuantity = nQ;
		pItem->nQuantity -= nQ;
		return pNewCont;
	}
	// retail CItemContainer<CSimpleCharge>::SpendCharge @0x2a6fa0: unless the container is
	// already empty, split one charge off and discard it.
	void SpendCharge()
	{
		if ( GetIncQuantity() <= 0 )
			return;
		CObj<IJoinSplit> pSpent = SplitItem( 1 );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// -----------------------------   ----------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////
// 
class CAmmoItem: public CItem, virtual public IAmmoItem
{
	OBJECT_BASIC_METHODS(CAmmoItem);
private:
	ZDATA_(CItem)
	CDBPtr<NDb::CRPGAmmo> pDBAmmo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CItem*)this); f.Add(2,&pDBAmmo); return 0; }

public:
	CAmmoItem( NDb::CRPGAmmo *_pDBAmmo = 0): CItem(), pDBAmmo(_pDBAmmo) {}

	int GetWeight() const { return /*CRAP*/1; }
	NDb::CRPGAmmo *GetDBAmmo() const { return pDBAmmo; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (   )   
class CClipItem: public CItemContainer<CAmmoItem>, public IClipItem
{
	OBJECT_BASIC_METHODS(CClipItem);
private:
	typedef CItemContainer<CAmmoItem> TAmmoContainer;
	ZDATA
	ZPARENT( TAmmoContainer );
	CDBPtr<NDb::CRPGClip> pDBClip;
	int nMaxAmmoQuantity;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(TAmmoContainer *)this); f.Add(3,&pDBClip); f.Add(4,&nMaxAmmoQuantity); return 0; }

public:
	CClipItem() {}
	CClipItem( NDb::CRPGClip *pClip );

	int GetDBClipID() const;
	NDb::CRPGAmmo* GetDBAmmo() const;
	virtual NDb::CRPGClip *GetDBClip() const { return pDBClip; }
	//
	virtual int GetMaxIncQuantity() const;
	virtual void SetMaxIncQuantity( int _nMaxAmmoQuantity );
	int GetIncQuantity() { return GetItem()->GetQuantity(); }
	//
	bool IsCompatible( CClipItem *pClip, bool bCheckSameColor );
	void LoadAmmoFromClip( CClipItem *pClip );
	void LoadAmmo( CAmmoItem *pAmmo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFindClipResult;
class CWeaponItem: public CInventoryItem, public IWeaponItem
{
	OBJECT_BASIC_METHODS(CWeaponItem);
private:
	ZDATA_(CInventoryItem)
	CObj<CClipItem> pInnerClip;
	NDb::EShootMode eShootMode;
	CDBPtr<NDb::CRPGWeapon> pDBWeapon;
	bool bWorking;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CInventoryItem*)this); f.Add(2,&pInnerClip); f.Add(3,&eShootMode); f.Add(4,&pDBWeapon); f.Add(5,&bWorking); return 0; }
	
private:
	bool FindProperClip( IInventoryInfo *pInventory, SFindClipResult *pResult, bool bCheckSameColor ) const;

public:
	CWeaponItem() {}
	CWeaponItem( NDb::CRPGWeapon *_pWeapon );

	int GetShootAP() const;
	int GetReloadAP() const;
	void GetInfo( SWeaponInfo *pInfo ) const;
	NDb::ESkillType GetSkillIndex() const;

	NDb::EShootMode GetShootMode() const { return eShootMode; }
	bool SetShootMode( NDb::EShootMode _eShootMode );
	bool IsShootModeSupported( NDb::EShootMode eMode ) const;

	virtual void CreateNewAttackPortion( vector<CAttackPortion> *pRes, bool bSpendAmmo );
	virtual bool HasAmmo() const;
	virtual bool IsWorking() const { return bWorking; }
	virtual int GetAmmoQuantity() const;
	int GetClipType() const;
	bool CanLoad( IClipItem *pClip ) const;
	bool CanReload( IInventoryInfo *pInventory ) const;
	bool Load( IClipItem *pClip );
	bool Reload( IInventory *pInventory );
	bool Unload( IInventory *pInventory );
	void Damage() { bWorking = false; }
	void DiscardAmmo() { pInnerClip = 0; }   // luaItemUnload @0x2e76a0: drop the loaded clip
	virtual IClipItem *GetInnerClip() const { return pInnerClip; }

	NDb::CRPGWeapon* GetDBWeapon() const { return pDBWeapon; }
	NDb::EWeaponType GetWeaponType() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrenadeItem: public CInventoryItem, public IGrenadeItem
{
	OBJECT_BASIC_METHODS(CGrenadeItem);
	ZDATA_(CInventoryItem)
	EGrenadeMode eMode;
	CDBPtr<NDb::CRPGGrenade> pDBGrenade;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CInventoryItem*)this); f.Add(2,&eMode); f.Add(3,&pDBGrenade); f.Add(4,&pDBEngGrenade); return 0; }
	CDBPtr<NDb::CRPGEngGrenade> pDBEngGrenade;

public:
	CGrenadeItem() {}
	CGrenadeItem( NDb::CRPGGrenade *_pDBGrenade );
	CGrenadeItem( NDb::CRPGEngGrenade *_pDBEngGrenade );   // retail @0x2a11d0: engineer-grenade flavour

	EGrenadeMode GetMode() const { return eMode; }
	void SetMode( EGrenadeMode _eMode ) { eMode = _eMode; }

	NDb::CRPGGrenade *GetDBGrenade() const { return pDBGrenade; }
	// @0x2a3c70 -- engineer-grenade DB descriptor accessor (returns the already-serialized pDBEngGrenade,
	// operator& tag 4). Consumed by CExecSetTrap's engineer-grenade trap branch.
	NDb::CRPGEngGrenade *GetDBEngGrenade() const { return pDBEngGrenade; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CSimpleCharge (@0x2a2a40 ctor, @0x2a2f30 operator&, save id 0xA2312140): a bare countable
// CItem -- the generic "charges" inside first-aid kits and picklocks. This IS the Jan03
// CPotionItem renamed (retail reuses the 0xA2312140 registration id for it).
class CSimpleCharge: public CItem
{
	OBJECT_BASIC_METHODS(CSimpleCharge)
	ZDATA_(CItem)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CItem*)this); return 0; }
	virtual int GetWeight() const { return /*CRAP*/ 1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CFirstAidItem (ctor @0x2a17a0, operator& @0x2ac050, save id 0xA1112153 -- dev already used
// the retail id): the medkit carries its charges as ONE CSimpleCharge (seeded with the record's
// nQuantity) in the CItemContainer base -- the exact CPicklockItem/CToolItem shape. Replaces the
// Jan03 CInventoryItem + CObj<CPotionContainer> pClip model (dev-only save id 0xA2312141, class
// deleted -- absent in retail; old dev saves' medkits will not round-trip).
class CFirstAidItem: public CItemContainer<CSimpleCharge>, public IFirstAidItem
{
	OBJECT_BASIC_METHODS(CFirstAidItem);
	typedef CItemContainer<CSimpleCharge> TChargeContainer;
	ZDATA_(TChargeContainer)
	CDBPtr<NDb::CRPGFirstAid> pDBFirstAid;
public:
	// retail @0x2ac050: tag 2 = the container base (which serializes the charge), tag 3 = the DB record
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(TChargeContainer*)this); f.Add(3,&pDBFirstAid); return 0; }

public:
	CFirstAidItem() {}
	CFirstAidItem( NDb::CRPGFirstAid *_pDBFirstAid );

	// retail spend = the container's SpendCharge @0x2a6fa0 (the old dev SpendPotion had an INVERTED
	// empty-guard and never actually consumed a charge -- infinite medkits)
	void SpendPotion() { SpendCharge(); }
	virtual bool IsEmpty() { return GetIncQuantity() < 1; }   // retail @0x2a4780 (IItemContainerInfo slot 2)
	virtual int GetMaxIncQuantity() const;                    // retail @0x2a0740: pDBFirstAid->nQuantity
	NDb::CRPGFirstAid *GetDBFirstAid() const { return pDBFirstAid; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMeleeWeaponItem: public CInventoryItem, public IMeleeWeaponItem
{
	OBJECT_BASIC_METHODS(CMeleeWeaponItem);
	ZDATA_(CInventoryItem)
	CDBPtr<NDb::CRPGMeleeWeapon> pDBMelee;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CInventoryItem*)this); f.Add(2,&pDBMelee); return 0; }

public:
	CMeleeWeaponItem() {}
	CMeleeWeaponItem( NDb::CRPGMeleeWeapon *_pDBMelee );

	void CreateNewAttackPortion( vector<CAttackPortion> *pRes );

	NDb::EWeaponType GetWeaponType() const;
	NDb::CRPGMeleeWeapon *GetDBMeleeWeapon() const { return pDBMelee; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TDBRec, class TInterface>
class CSomeItem: public CInventoryItem, public TInterface
{
	OBJECT_BASIC_METHODS(CSomeItem);
	ZDATA_(CInventoryItem)
	CDBPtr<TDBRec> pDBItemInfo;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CInventoryItem*)this); f.Add(2,&pDBItemInfo); return 0; }

	CSomeItem() {}
	CSomeItem( TDBRec *_pDB ) : CInventoryItem( _pDB->pItem ), pDBItemInfo(_pDB) {}
	TDBRec* GetDBItemInfo() const { return pDBItemInfo; }
};
typedef CSomeItem<NDb::CRPGMineDetector, IMineDetectorItem> CMineDetectorItem;
typedef CSomeItem<NDb::CRPGMine, IMineItem> CMineItem;
typedef CSomeItem<NDb::CRPGKey, IKeyItem> CKeyItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToolItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CToolItem: public CInventoryItem, public IToolItem
{
	OBJECT_BASIC_METHODS( CToolItem )
	ZDATA
	ZPARENT( CInventoryItem );
	CDBPtr<NDb::CRPGTool> pDBTool;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CInventoryItem *)this); f.Add(3,&pDBTool); return 0; }
	//
public:
	CToolItem() {}
	CToolItem( NDb::CRPGTool *_pDBTool );
	//
	virtual bool CanBeUsed( NRPG::CUnit *pUnit ) const;
	virtual int GetSkillModifForMineCleaning() const;
	NDb::CRPGTool* GetDBItemInfo() const { return pDBTool; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CPicklockItem (ctor @0x2a1c80, operator& @0x2abaa0, save id 0xA0523091): a lockpick with
// its uses stored as CSimpleCharge charges in the CItemContainer base (the record's nQuantity).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPicklockItem: public CItemContainer<CSimpleCharge>, public IPicklockItem
{
	OBJECT_BASIC_METHODS( CPicklockItem )
	typedef CItemContainer<CSimpleCharge> TChargeContainer;
	ZDATA_(TChargeContainer)
	CDBPtr<NDb::CRPGPicklock> pDBPicklock;
public:
	// retail @0x2abaa0 (out-of-line there too): tag 2 = the container base, tag 3 = the DB record
	ZEND int operator&( CStructureSaver &f );

	CPicklockItem() {}
	CPicklockItem( NDb::CRPGPicklock *_pDBPicklock );
	//
	virtual NDb::CRPGPicklock* GetDBPicklock() const { return pDBPicklock; }
	virtual int GetMaxIncQuantity() const;   // retail @0x2a0750: pDBPicklock->nQuantity
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
