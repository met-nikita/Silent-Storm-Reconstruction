#ifndef __RPGUNITINFO_H_
#define __RPGUNITINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	enum EWeaponType;
	enum ESkillType;
	enum ECritical;
	enum ECriticalLocation;
	class CModel;
	class CRPGArmor;
	class CRPGPers;
	class CRPGGrenade;
	class CAnimWeaponType;
	class CComplexHead;
}
namespace NAI
{
	enum EPose;
	enum EHitLocation;
	enum ETileHitLocation;
	struct SPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// information about unit for interface
struct SUnitInfo
{
public:
	int nHP, nHealedHP, nMaxHP;
	int nAP, nMaxAP;
	int nSightDistance;
	bool bWearingPK;          // unit is piloting a Panzerklein -> show the PK-armor bar
	int nPKLife, nMaxPKLife;  // worn Panzerklein's current / max armor (VP)
	// retail CUnitServer::GetInfo @0x3bfd50 PK-HP HUD layering. SUnitInfo is a TRANSIENT interface
	// record (no operator&/ZDATA -- NOT serialized), so these are a plain, additive struct extension.
	// bUnitInfo => this record describes a normal (non-empty-PK) unit; bPKInfo => the nPKHP/nMaxPKHP
	// pair below is valid this frame (an empty-PK shell's own HP, or a worn PK unit's HP).
	bool bPKInfo;             // nPKHP/nMaxPKHP are valid this frame
	bool bUnitInfo;           // the record describes a normal (non-empty-PK) unit
	int nPKHP, nMaxPKHP;      // Panzerklein current / max HP for the PK-HP bar
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Actions that an RPG unit can perform
enum EAction // Do not change this enum or reorder its lines without agreeing with Epik! (or only add at the end)
{
	AC_NONE,
	AC_MOVE_SIDE,
	AC_MOVE_DIAGONAL,
	AC_MOVE_CORPSE_SIDE,
	AC_MOVE_CORPSE_DIAGONAL,
	AC_ROTATE,
	AC_PREPARE_AND_SHOOT,
	AC_SHOOT,
	AC_EXPLODE,
	AC_POSE_CRAWL,
	AC_POSE_CROUCH,
	AC_POSE_WALK,
	AC_POSE_RUN,
	AC_THROW_GRENADE,
	AC_CLIMB_1,
	AC_CLIMB_2,
	AC_CLIMB_3,
	AC_CLIMB_4,
	AC_JUMP,
	AC_FIRSTAID,
	AC_MELEE,
	AC_TAKE_CORPSE,
	AC_BURST,
	AC_RELOAD,
	AC_OPEN_CLOSE,
	AC_APPROACH_CANNON,
	AC_LADDER,
	AC_LADDER_MOVE,
	AC_END_SHOOT,
	AC_THROW_KNIFE,
	AC_PREPARE,
	AC_ENTER_PK,
	AC_LEAVE_PK,
	AC_HIDE,
	AC_TRAP_OBJECT,
	AC_DISARM_TRAP,
	AC_SET_MINE,
	AC_DISARM_MINE,               // = 37 (0x25) last Jan03/dev code
	// Retail (Game.exe) fills 38/39 with the locked-door actions and appends the inventory-move reach
	// actions at 42..44 (see NRPG::CUnitMission::GetActionAP @0x2c0bd0; 41 stays a retail AC_SWAP-family
	// gap). Explicit ordinals keep every serialized action code on its exact decoded value.
	AC_USE_KEY = 38,              // 0x26  open a locked door with its key (AP = RPGAP table record 9; CExecOpenClose::GetStartAP @0x3bd340)
	AC_PICK_LOCK,                 // 0x27 (39)  pick a locked door (AP = the active picklock record's nAPToUse)
	AC_REPAIR_PK = 0x28,          // 0x28 (40) CExecHeal power-armour REPAIR branch
	AC_ITEM_TAKE = 42,            // 0x2a  ground/other -> hand pickup
	AC_ITEM_SLOT,                 // 0x2b  slot (re)placement
	AC_ITEM_TRANSFER              // 0x2c  cross-unit hand transfer
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Critical info
class ICriticalInfo: public CObjectBase
{
public:
	// release vtable order (CCritical impls @0x695f90..0x695fd0): IsTemporarily, GetRemainingTime,
	// GetDifficultyClass, GetValue, GetCriticalType, GetCriticalLocation. The UI (iCriticalIcons)
	// pumps IsTemporarily (icon variant), GetRemainingTime ("critdur"), GetValue ("bleedspeed").
	virtual bool IsTemporarily() const = 0;
	virtual int GetRemainingTime() const = 0;
	virtual int GetDifficultyClass() const = 0;
	virtual float GetValue() const = 0;
	virtual NDb::ECritical GetCriticalType() const = 0;
	virtual NDb::ECriticalLocation GetCriticalLocation() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// mission time RPG information handler
class CUnit;
class IInventoryInfo;
class IWeaponItemInfo;
class IUnitMissionInfo: virtual public CObjectBase
{
public:
	virtual int GetActionAP( NAI::EPose pose, EAction action ) const = 0;
	virtual bool CanSpendAP( int nAP ) const = 0;
	virtual int GetMaxExtraAP() const = 0;
	virtual int GetAP() const = 0;

	virtual int GetInterrupt() const = 0;
	virtual int GetIC() const = 0;
	// release migration (session 25): the virtual Get{,Tile,Grenade,RLauncher}ToHit interface was
	// replaced by the free-fn + RTTI dispatch NRPG::Get*ToHit(const NWorld::CUnit*,...) declared in
	// RPGUnitMission.h (they RTTI-cast the unit to its CUnitServer and build the right ToHitCalcer).

	virtual int  GetBulletsQuantityInShot() const = 0;
	virtual void GetInfo( NAI::EPose pose, SUnitInfo *pInfo ) const = 0;
	virtual float GetSightDistance( NAI::EPose pose ) const = 0;
	virtual const wstring& GetName() const = 0;
	virtual NDb::CModel* GetModel() const = 0;

	virtual IInventoryInfo* GetInventoryInfo() const = 0;
	virtual NDb::EWeaponType GetWeaponType() const = 0;
	virtual IWeaponItemInfo* GetCannonItemInfo() const = 0;
	virtual NDb::CAnimWeaponType* GetDBAnimWeapon() const = 0;

	virtual NDb::CRPGArmor* GetRPGArmor() const = 0;
	virtual int GetSkillValue( NDb::ESkillType skill ) const = 0;
	virtual int GetSkillMaxValue( NDb::ESkillType skill ) const = 0;
	virtual float GetSkillProgress( NDb::ESkillType skill ) const = 0;
	virtual void DumpStats() const = 0;
	virtual void PrintLog( bool bPrint ) {};
	virtual CVec3 GetTurnStartCP() const = 0;
	virtual NDb::CRPGPers* GetRPGPers() const = 0;
	virtual NDb::CComplexHead* GetRPGPersHead() const = 0;
	virtual NRPG::CUnit* GetRPGUnit() const = 0;

	virtual bool IsHero() const = 0;
	virtual bool IsHiding() const = 0;
	// retail slot 25 (CUnitMission::IsAIPlayer @0x2c5b40 = getter for bIsAIUnit); CalcStructDmg
	// @0x28f960 reads it to pick the difficulty's enemy-vs-our damage multiplier.
	virtual bool IsAIPlayer() const = 0;

	virtual void GetCriticalsList( list<CPtr<ICriticalInfo> > *pListCriticals ) const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif