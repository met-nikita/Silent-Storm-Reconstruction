#ifndef __AIWEAPON_H_
#define __AIWEAPON_H_
//
namespace NRPG
{
	class CClipItem;
	class CWeaponItem;
	class CGrenadeItem;
	class CMeleeWeaponItem;
	class CFirstAidItem;
	class IInventoryItem;
}
//
namespace NDb
{
	enum EShootMode;
}
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPose;
class IAIUnit;
class CAILogRecord;
struct SUnitPosition;
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIInventoryItem: public CObjectBase
{
public:
	virtual NRPG::IInventoryItem* GetInventoryItem() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFireArmsWeaponClip: public CObjectBase
{
	OBJECT_BASIC_METHODS( CAIFireArmsWeaponClip );
	ZDATA
	CPtr<NRPG::CClipItem> pClipItem;
	int nAmmoCount;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pClipItem); f.Add(3,&nAmmoCount); return 0; }
	//
public:
	CAIFireArmsWeaponClip() {}
	CAIFireArmsWeaponClip( NRPG::CClipItem *_pClipItem );
	//
	int GetAmmoCount() const;
	void SetAmmoCount( int nCount );
	void SpendAmmo( int nCount );
	bool IsEmpty() const;
	NRPG::CClipItem* GetItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFireArmsWeaponBase - the serialized core of the AI fire-arms weapon. Retail base/leaf split:
// the leaf CAIFireArmsWeapon::operator& @0xb7fa0 writes ONLY tag 2 = this base sub-object; the base
// @0xb7c90 persists pWeaponItem(2)/pCurrentClip(3)/clips(4)/pOwner(5). Retail saveload ids:
// base 0x52642100, leaf 0x53133163, rocket-launcher leaf CAIRocketLauncherWeapon 0x53133162.
// Clip management is a BASE responsibility in retail (GetClipCount @0xb5890, SetCurrentClip
// @0xb61e0, RemoveClip @0xb6760, AddClip @0xb6a90, IsWorseThen @0xb5a60, ...), shared by both
// leaves -- CAIInventory's clip routing (GetSuitableWeapon @0x55790 / AddItem @0x55e20) and the
// reload scan (GetBestWeaponForReload @0x56450) operate on this base.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFireArmsWeaponBase: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIFireArmsWeaponBase );
	ZDATA
protected:
	CPtr<NRPG::CWeaponItem> pWeaponItem;
	CObj<CAIFireArmsWeaponClip> pCurrentClip; // magazine currently loaded in the weapon
	vector< CObj<CAIFireArmsWeaponClip> > clips; // spare magazines ( the loaded one not included )
	CPtr<IAIUnit> pOwner; // whose inventory it is stored in
	ZEND
public:
	int operator&( CStructureSaver &f ) { f.Add(2,&pWeaponItem); f.Add(3,&pCurrentClip); f.Add(4,&clips); f.Add(5,&pOwner); return 0; }
	CAIFireArmsWeaponBase() {}
	NRPG::CWeaponItem* GetItem() const { return pWeaponItem; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
	// clip management (retail base methods)
	int GetClipCount() const;                                    // @0xb5890
	CAIFireArmsWeaponClip* GetCurrentClip() const;
	void SetCurrentClip( CAIFireArmsWeaponClip *pClip );         // @0xb61e0
	CAIFireArmsWeaponClip* GetNextClip() const;
	void RemoveClip( CAIFireArmsWeaponClip *pClip );             // @0xb6760
	void AddClip( CAIFireArmsWeaponClip *pClip );                // @0xb6a90
	bool IsSuitableClip( CAIFireArmsWeaponClip *pClip ) const;
	// @0xb5a60: my DB AIRating < the candidate's DB AIRating (a null/dead candidate reads as
	// "I am worse"). CAIInventory::IsItemNecessary's firearm-swap comparisons run through this.
	bool IsWorseThen( NRPG::CWeaponItem *pCandidate ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFireArmsWeapon: public CAIFireArmsWeaponBase
{
	OBJECT_BASIC_METHODS( CAIFireArmsWeapon );
	ZDATA_(CAIFireArmsWeaponBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIFireArmsWeaponBase*)this); return 0; }
	//
	bool IsBurstMode( NDb::EShootMode eShotMode ) const;
	int GetMeanDamage() const;
	int GetMinAPToShoot( int nUnitAP ) const;
	int GetLongBurstAmmoCountPerShot( int nUnitAP ) const;
	int GetAmmoCountPerShot( int nUnitAP ) const;
	int GetAmmoCountPerAP( int nAP ) const;
	//
public:
	CAIFireArmsWeapon() {}
	CAIFireArmsWeapon( IAIUnit *_pOwner, NRPG::CWeaponItem *_pWeaponItem );
	//
	int GetShotAP() const;
	int GetBurstAP() const;
	int GetReloadAP() const;
	//
	// (release) the weapon's animation hold-type (NDb::CAnimWeaponType::type) as int: WT_PISTOL=1 / WT_SUB_MACHINE_
	// GUN=3 are what the assassin gate accepts; returns -1 when there is no DB weapon/anim model.
	int GetAnimType() const;
	bool IsRocketLauncher() const;
	//
	bool IsSameWeapon( CAIFireArmsWeapon *pAIWeapon ) const;
	void GetShotParameters( const NAI::SUnitPosition &pos, IAIUnit *pTarget,
		int nHitCover, int nAvailableAP, int *nAP, int *nAmmo, int *nDamage, bool *bNeedReload ) const;
	int GetDamage( const NAI::SUnitPosition &pos, IAIUnit *pTarget,
		int nHitCover, NAI::EPose ePose, int nAP, NDb::EShootMode eShootMode, int *nMaxToHit ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRocketLauncherWeapon - the AI wrapper for a bazooka-logic weapon (retail leaf 0x53133162,
// ctor @0xb7950 = base init + own vftable; its operator& is byte-identical to the firearms leaf's
// "tag 2 = base sub-object" and is COMDAT-folded with it in the retail binary -- the same fold is
// visible on CAIThrowingWeapon vs CAIMeleeWeapon). CAIInventory::CreateWeapon @0x554f0 dispatches
// on NDb::CRPGWeapon::bBazookaLogic between this leaf and CAIFireArmsWeapon.
// DEV DIVERGENCE (typing only): retail derives this leaf DIRECTLY from CAIFireArmsWeaponBase (a
// SIBLING of CAIFireArmsWeapon, so retail's dynamic_cast<CAIFireArmsWeapon> never yields a rocket
// launcher). Here it derives from CAIFireArmsWeapon because unowned call sites (aiActions.cpp /
// aiAttackAction.cpp CAILaunchRocketAction) hold rocket launchers as CAIFireArmsWeapon* and drive
// the leaf's AP/damage methods. CAIInventory's firearms probes compensate with IsRocketLauncher()
// (see CastInventoryItem<CAIFireArmsWeapon> in aiInventory.cpp); the wire is unaffected (this leaf
// adds no members and writes the same tag-2 base chunk retail does).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRocketLauncherWeapon: public CAIFireArmsWeapon
{
	OBJECT_BASIC_METHODS( CAIRocketLauncherWeapon );
	ZDATA_(CAIFireArmsWeapon)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIFireArmsWeaponBase*)this); return 0; }
	//
public:
	CAIRocketLauncherWeapon() {}
	CAIRocketLauncherWeapon( IAIUnit *_pOwner, NRPG::CWeaponItem *_pWeaponItem ): CAIFireArmsWeapon( _pOwner, _pWeaponItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGrenadeWeapon: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIGrenadeWeapon );
	ZDATA
	CPtr<NRPG::CGrenadeItem> pGrenade;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGrenade); return 0; }  // retail @0xb7970 (save: {2:4}, byte-walked slot 1)
	//
public:
	CAIGrenadeWeapon() {}
	CAIGrenadeWeapon( NRPG::CGrenadeItem *_pGrenade ): pGrenade( _pGrenade ) {}
	//
	NRPG::CGrenadeItem* GetItem() const { return pGrenade; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMeleeWeaponBase - the serialized core shared by the melee and throwing wrappers (retail base
// 0x53133160; operator& @0xb7a20 persists pMelee at tag 2, GetInventoryItem @0xb5940). The leaves
// write ONLY tag 2 = this base sub-object (CAIMeleeWeapon::operator& @0xb7c70; the byte-identical
// CAIThrowingWeapon body is COMDAT-folded with it in the retail binary -- retail saves show both
// leaves as {2: {2: pMelee}}, byte-walked slot 1). GetBestMeleeWeapon<T> @0x55850/@0x55960 scores
// over this base's DB record.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIMeleeWeaponBase: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIMeleeWeaponBase );
	ZDATA
protected:
	CPtr<NRPG::CMeleeWeaponItem> pMelee;
	ZEND
public:
	int operator&( CStructureSaver &f ) { f.Add(2,&pMelee); return 0; }
	CAIMeleeWeaponBase() {}
	CAIMeleeWeaponBase( NRPG::CMeleeWeaponItem *_pMelee ): pMelee( _pMelee ) {}
	//
	NRPG::CMeleeWeaponItem* GetItem() const { return pMelee; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMeleeWeapon - AI wrapper over a melee weapon item (default / knife / katana). Used by
// CAIMeleeAction. Retail CAIInventory::CreateWeapon @0x554f0 wraps a melee item as EITHER a
// throwing weapon (DB CRPGMeleeWeapon::bThrowing set) OR a melee weapon -- never both.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIMeleeWeapon: public CAIMeleeWeaponBase
{
	OBJECT_BASIC_METHODS( CAIMeleeWeapon );
	ZDATA_(CAIMeleeWeaponBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIMeleeWeaponBase*)this); return 0; }
	//
public:
	CAIMeleeWeapon() {}
	CAIMeleeWeapon( NRPG::CMeleeWeaponItem *_pMelee ): CAIMeleeWeaponBase( _pMelee ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowingWeapon - AI wrapper over a thrown melee weapon (DB bThrowing). Used by CAIThrowKnifeAction.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIThrowingWeapon: public CAIMeleeWeaponBase
{
	OBJECT_BASIC_METHODS( CAIThrowingWeapon );
	ZDATA_(CAIMeleeWeaponBase)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIMeleeWeaponBase*)this); return 0; }
	//
public:
	CAIThrowingWeapon() {}
	CAIThrowingWeapon( NRPG::CMeleeWeaponItem *_pMelee ): CAIMeleeWeaponBase( _pMelee ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFirstAid - AI wrapper over a first-aid item. Release type, used by CAIHealAction (in CAIAfterCombatLogic).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFirstAid: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIFirstAid );
	ZDATA
	CPtr<NRPG::CFirstAidItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pItem); return 0; }  // retail @0xb7ad0 (save: {2:4}, byte-walked slot 1)
	//
public:
	CAIFirstAid() {}
	CAIFirstAid( NRPG::CFirstAidItem *_pItem ): pItem( _pItem ) {}
	//
	NRPG::CFirstAidItem* GetItem() const { return pItem; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CreateAIFireArmsWeaponClip( NRPG::CClipItem *pItem );
CAIFireArmsWeapon* CreateAIFireArmsWeapon( IAIUnit *pOwner, NRPG::CWeaponItem *pItem );
CAIRocketLauncherWeapon* CreateAIRocketLauncherWeapon( IAIUnit *pOwner, NRPG::CWeaponItem *pItem );
CAIGrenadeWeapon* CreateAIGrenadeWeapon( NRPG::CGrenadeItem *pItem );
CAIMeleeWeapon* CreateAIMeleeWeapon( NRPG::CMeleeWeaponItem *pItem );
CAIThrowingWeapon* CreateAIThrowingWeapon( NRPG::CMeleeWeaponItem *pItem );
CAIFirstAid* CreateAIFirstAid( NRPG::CFirstAidItem *pItem );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif