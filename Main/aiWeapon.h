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
class CAIFireArmsWeapon: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIFireArmsWeapon );
	ZDATA
	CPtr<NRPG::CWeaponItem> pWeaponItem;
	CObj<CAIFireArmsWeaponClip> pCurrentClip; // magazine currently loaded in the weapon
	vector< CObj<CAIFireArmsWeaponClip> > clips; // spare magazines ( the loaded one not included )
	CPtr<IAIUnit> pOwner; // whose inventory it is stored in
	ZEND int operator&( CStructureSaver &f ) { return 0; }
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
	NRPG::CWeaponItem* GetItem() const { return pWeaponItem; }
	// (release) the weapon's animation hold-type (NDb::CAnimWeaponType::type) as int: WT_PISTOL=1 / WT_SUB_MACHINE_
	// GUN=3 are what the assassin gate accepts; returns -1 when there is no DB weapon/anim model. Dead until consumed.
	int GetAnimType() const;
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
	bool IsRocketLauncher() const;
	// clip management
	int GetClipCount() const;
	CAIFireArmsWeaponClip* GetCurrentClip() const;
	void SetCurrentClip( CAIFireArmsWeaponClip *pClip );
	CAIFireArmsWeaponClip* GetNextClip() const;
	void RemoveClip( CAIFireArmsWeaponClip *pClip );
	void AddClip( CAIFireArmsWeaponClip *pClip );
	bool IsSuitableClip( CAIFireArmsWeaponClip *pClip ) const;
	//
	bool IsSameWeapon( CAIFireArmsWeapon *pAIWeapon ) const;
	void GetShotParameters( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
		int nHitCover, int nAvailableAP, int *nAP, int *nAmmo, int *nDamage, bool *bNeedReload ) const;
	int GetDamage( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
		int nHitCover, NAI::EPose ePose, int nAP, NDb::EShootMode eShootMode, int *nMaxToHit ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGrenadeWeapon: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIGrenadeWeapon );
	ZDATA
	CPtr<NRPG::CGrenadeItem> pGrenade;
	ZEND
	//
public:
	CAIGrenadeWeapon() {}
	CAIGrenadeWeapon( NRPG::CGrenadeItem *_pGrenade ): pGrenade( _pGrenade ) {}
	//
	NRPG::CGrenadeItem* GetItem() const { return pGrenade; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMeleeWeapon - AI wrapper over a melee weapon item (default / knife / katana). Release type, used by
// CAIMeleeAction. Knives (WT_KNIFE) are ALSO wrapped as CAIThrowingWeapon for the throw-knife action.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIMeleeWeapon: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIMeleeWeapon );
	ZDATA
	CPtr<NRPG::CMeleeWeaponItem> pMelee;
	ZEND
	//
public:
	CAIMeleeWeapon() {}
	CAIMeleeWeapon( NRPG::CMeleeWeaponItem *_pMelee ): pMelee( _pMelee ) {}
	//
	NRPG::CMeleeWeaponItem* GetItem() const { return pMelee; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowingWeapon - AI wrapper over a knife used as a thrown weapon. Release type, used by CAIThrowKnifeAction.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIThrowingWeapon: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIThrowingWeapon );
	ZDATA
	CPtr<NRPG::CMeleeWeaponItem> pMelee;
	ZEND
	//
public:
	CAIThrowingWeapon() {}
	CAIThrowingWeapon( NRPG::CMeleeWeaponItem *_pMelee ): pMelee( _pMelee ) {}
	//
	NRPG::CMeleeWeaponItem* GetItem() const { return pMelee; }
	virtual NRPG::IInventoryItem* GetInventoryItem() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFirstAid - AI wrapper over a first-aid item. Release type, used by CAIHealAction (in CAIAfterCombatLogic).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFirstAid: public IAIInventoryItem
{
	OBJECT_BASIC_METHODS( CAIFirstAid );
	ZDATA
	CPtr<NRPG::CFirstAidItem> pItem;
	ZEND
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
CAIGrenadeWeapon* CreateAIGrenadeWeapon( NRPG::CGrenadeItem *pItem );
CAIMeleeWeapon* CreateAIMeleeWeapon( NRPG::CMeleeWeaponItem *pItem );
CAIThrowingWeapon* CreateAIThrowingWeapon( NRPG::CMeleeWeaponItem *pItem );
CAIFirstAid* CreateAIFirstAid( NRPG::CFirstAidItem *pItem );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif