#ifndef __AIINVENTORY_H_
#define __AIINVENTORY_H_
//
namespace NDB
{
	enum EShootMode;
}
//
namespace NRPG
{
	class IUnitMission;
	class CAttackPortion;
	class IInventoryItem;
	class CGrenadeItem;
}
//
namespace NWorld { class CDFrozenItem; }
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
class CAIFireArmsWeapon;
class CAIFireArmsWeaponClip;
class CAIGrenadeWeapon;
class CAIMeleeWeapon;
class CAIThrowingWeapon;
class CAIFirstAid;
class IAIInventoryItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIInventory
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIInventory: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAIInventory);
	ZDATA
	CPtr<IAIUnit> pOwner;
	vector< CObj<CAIFireArmsWeapon> > fireArms;
	vector< CObj<CAIGrenadeWeapon> > grenades;
	vector< CObj<CAIFireArmsWeapon> > rocketLaunchers;
	vector< CObj<CAIMeleeWeapon> > meleeWeapons;
	vector< CObj<CAIThrowingWeapon> > throwingWeapons;
	vector< CObj<CAIFirstAid> > firstAids;
	CObj<IAIInventoryItem> pCurrent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pOwner); f.Add(3,&fireArms); f.Add(4,&grenades); f.Add(5,&rocketLaunchers); f.Add(6,&pCurrent); f.Add(7,&meleeWeapons); f.Add(8,&throwingWeapons); f.Add(9,&firstAids); return 0; }
	//
	void GetInventoryItems( list< CPtr<NRPG::IInventoryItem> > *pItems ) const;
	void FetchInventoryItems();
	CAIFireArmsWeapon* GetSuitableWeapon( CAIFireArmsWeaponClip *pClip ) const;
	IAIInventoryItem* GetAIInventoryItem( NRPG::IInventoryItem *pItem ) const;
	//
public:
	CAIInventory() {}
	CAIInventory( IAIUnit *_pOwner );
	//
	void AddFireArms( CAIFireArmsWeapon *pWeapon );
	void RemoveFireArms( CAIFireArmsWeapon *pWeapon );
	void AddRocketLaunchers( CAIFireArmsWeapon *pWeapon );
	void RemoveRocketLaunchers( CAIFireArmsWeapon *pWeapon );
	void AddGrenade( CAIGrenadeWeapon *pGrenade );
	void RemoveGrenade( CAIGrenadeWeapon *pGrenade );
	void AddMeleeWeapon( CAIMeleeWeapon *pWeapon );
	void AddThrowingWeapon( CAIThrowingWeapon *pWeapon );
	void RemoveThrowingWeapon( CAIThrowingWeapon *pWeapon );
	void AddFirstAid( CAIFirstAid *pFirstAid );
	void AddClip( CAIFireArmsWeaponClip *pClip );
	//
	void SetCurrentItem( IAIInventoryItem *pItem );
	IAIInventoryItem* GetCurrentItem() const;
	CAIFireArmsWeapon* GetCurrentFireArms() const;
	CAIFireArmsWeapon* GetFirstFireArms() const;   // release @0x56300 -- the first held firearm (dead until the assassin reaction)
	CAIGrenadeWeapon* GetCurrentGrenade() const;
	CAIFireArmsWeapon* GetCurrentRocketLauncher() const;
	bool IsCurrentItem( IAIInventoryItem *pItem ) const;
	//
	CAIFireArmsWeapon* GetBestFireArms( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
		int nAP, int *pBestWeaponHitCover, int *pQuality,	NDb::EShootMode *shootMode, int *nMaxToHit ) const;
	CAIGrenadeWeapon* GetBestGrenade( CVec3 ptTarget ) const;
	CAIFireArmsWeapon* GetBestRocketLaunchers() const;
	CAIMeleeWeapon* GetBestMeleeWeapon() const;
	CAIThrowingWeapon* GetBestThrowingWeapon() const;
	CAIFirstAid* GetBestFirstAid() const;
	// the best firearm that needs (and can) reload - an empty current clip with a usable spare. The
	// release CAIInventory exposes this (used by CAIReloadAction); reconstructed over the dev firearms.
	CAIFireArmsWeapon* GetBestWeaponForReload() const;
	// the best firearm for a precision snipe - the first firearm whose weapon item supports the snipe
	// shoot-mode and has a non-empty current clip. Release CAIInventory::GetBestWeaponForSnipe @0x56540
	// (used by CAIBeginSnipeAction); reconstructed over the dev firearms.
	CAIFireArmsWeapon* GetBestWeaponForSnipe() const;
	// the loot scorers (release CAIInventory @0x56810 / @0x55680, used by CAILootAction). IsItemNecessary:
	// is a ground item worth picking up given what I carry (and, if so, *ppToDrop = the held item to drop
	// to make room). GetMostNecessaryItem: of a wanted list, the highest-priority by item type.
	bool IsItemNecessary( NWorld::CDFrozenItem *pFrozen, IAIInventoryItem **ppToDrop ) const;
	NWorld::CDFrozenItem* GetMostNecessaryItem( const list< CPtr<NWorld::CDFrozenItem> > &items ) const;
	bool HasAnyWeapon() const;
	//
	void DebugOutput() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIInventory* CreateAIInventory( IAIUnit *pOwner );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif