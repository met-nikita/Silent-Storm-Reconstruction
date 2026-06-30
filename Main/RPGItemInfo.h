#ifndef __RPGITEMINFO_H_
#define __RPGITEMINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NDb
{
	enum ESlot;
	enum ESkillType;
	enum EItemSubType;
	enum EShootMode;
	enum EWeaponType;
	class CRPGItem;
	class CRPGAmmo;
	class CRPGClip;
	class CRPGGrenade;
	class CRPGUniform;
	class CRPGWeapon;
	class CRPGFirstAid;
	class CRPGMeleeWeapon;
	class CRPGMineDetector;
	class CRPGMine;
	class CRPGTool;
	class CRPGKey;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
class CUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Базовый интерфейс для перекладывания
class IItem: virtual public CObjectBase
{
public:
	virtual int GetWeight() const = 0;
	virtual int GetQuantity() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IInventoryItem: virtual public IItem
{
public:
	virtual NDb::CRPGItem *GetDBItem() const = 0;
	virtual const CTPoint<int>& GetSize() const = 0;

	virtual NDb::EWeaponType GetWeaponType() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IItemContainerInfo: virtual public IInventoryItem
{
public:
	virtual int GetIncQuantity() const = 0;
	virtual int GetMaxIncQuantity() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBackPackItem
{
	ZDATA
	CTPoint<int> sPos;
	CObj<IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sPos); f.Add(3,&pItem); return 0; }

	SBackPackItem() {}
	SBackPackItem( const CTPoint<int> &_sPos, IInventoryItem* _pItem ): sPos( _sPos ), pItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Information about inventory
class IInventoryInfo: public CObjectBase
{
public:
	virtual bool CanPlace( const CTPoint<int> &sPos, const IInventoryItem *pItem, bool bCheckSpace = true ) const = 0;
	virtual bool FindPlace( const IInventoryItem *pItem, CTPoint<int> *pPos ) const = 0;
	virtual const vector<SBackPackItem>& GetItems() const = 0;

	virtual bool CanEquip( NDb::ESlot where, const IInventoryItem *pWhat ) const = 0;
	virtual IInventoryItem *Get( NDb::ESlot where ) const = 0;
	virtual int GetActiveSlot() const = 0;
	virtual IInventoryItem *GetActive() const = 0;

	virtual IInventoryItem *GetHandItem() const = 0;

	virtual int GetPlaceBySubType( NDb::EItemSubType subType ) const = 0;
	virtual NDb::CRPGUniform* GetUniform() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAmmoItem: virtual public IItem
{
public:
	virtual NDb::CRPGAmmo *GetDBAmmo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IClipItem: virtual public IItemContainerInfo
{
public:
	virtual NDb::CRPGAmmo* GetDBAmmo() const = 0;
	virtual NDb::CRPGClip* GetDBClip() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Оружие
class CClipItem;
class CAttackPortion;
struct SWeaponInfo
{
	int nRoF;
	int nQuality;
	int nShotAP, nTargetingAP;
	int nRecoil;
	int nMinRange, nMaxRange;
	int nDmgMin, nDmgMax;
	int nArmorPiercingAbility;
	float fScopeFactor;
};
class IWeaponItemInfo: virtual public IInventoryItem
{
public:
	virtual bool CanLoad( IClipItem *pClip ) const = 0;
	virtual bool CanReload( IInventoryInfo *pInventory ) const = 0;

	virtual IClipItem *GetInnerClip() const  = 0;
	virtual int GetAmmoQuantity() const = 0;
	virtual int GetShootAP() const = 0;
	virtual void GetInfo( SWeaponInfo *pInfo ) const = 0;
	virtual NDb::ESkillType GetSkillIndex() const = 0;
	virtual NDb::EShootMode GetShootMode() const = 0;
	virtual bool IsShootModeSupported( NDb::EShootMode eMode ) const = 0;

	virtual NDb::CRPGWeapon* GetDBWeapon() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EGrenadeMode
{
	GM_THROW,
	GM_SETTRAP
};
////
class IGrenadeItemInfo: virtual public IInventoryItem
{
public:
	virtual EGrenadeMode GetMode() const = 0;
	virtual NDb::CRPGGrenade *GetDBGrenade() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IFirstAidItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGFirstAid* GetDBFirstAid() const = 0;
	virtual bool IsEmpty() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMeleeWeaponItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMeleeWeapon* GetDBMeleeWeapon() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMineDetectorItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMineDetector* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMineItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGMine* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IToolItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGTool* GetDBItemInfo() const = 0;
	virtual bool CanBeUsed( NRPG::CUnit *pUnit ) const = 0;
	virtual int GetSkillModifForMineCleaning() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IKeyItem: virtual public IInventoryItem
{
public:
	virtual NDb::CRPGKey* GetDBItemInfo() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif