#ifndef __AILOG_H_
#define __AILOG_H_

#include "aiPosition.h"

namespace NDb
{
	enum EShootMode;
}

namespace NRPG
{
	class IInventoryItem;
	class CWeaponItem;
	class CGrenadeItem;
	class CClipItem;
	class IUnitMission;
}

namespace NWorld
{
	class CCmd;
	class CCommand;
	class CCannon;
	class CUnitServer;
}

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EHitLocation;
class IAIUnit;
class CAIFireArmsWeapon;
class CAIFireArmsWeaponClip;
class CAIGrenadeWeapon;
class CAIThrowingWeapon;
class CAIMeleeWeapon;
class IAIInventoryItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
//	IAILogRecord
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAILogRecord: public CObjectBase
{
public:
	virtual void RollBack() = 0; // roll back
	virtual void Commit() = 0;
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) = 0; // issue commands for execution
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//	IAILogContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAILogContainer: public IAILogRecord
{
public:
	virtual void Add( IAILogRecord *pAILogRecord, bool bCommit = false ) = 0; // add a record
	virtual void Add( IAILogContainer *pAILogContainer ) = 0; // add all records of the container
	virtual void Clear() = 0; // delete all records without rolling back
	virtual list< CObj<IAILogRecord> > *GetLogRecords() = 0;
	virtual bool IsEmpty() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogRecord
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogRecord: public IAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogRecord);
	ZDATA
public:
	CPtr<IAIUnit> pAIUnit; // whom the action is performed on
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIUnit); return 0; }
	CAILogRecord() {}
	CAILogRecord( IAIUnit *_pAIUnit ) : pAIUnit(_pAIUnit) {}
	// IAILogRecord
	virtual void RollBack() {}
	virtual void Commit() {}
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogPosition: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogPosition);
	ZDATA
	ZPARENT(CAILogRecord)
	NAI::EPose pose;
	SPosition pSourcePosition;
	SPosition pTargetPosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pSourcePosition); f.Add(4,&pTargetPosition); return 0; }
public:
	CAILogPosition() {}
	CAILogPosition(	IAIUnit *_pAIUnit, SPosition _pSourcePosition, SPosition _pTargetPosition, NAI::EPose _pose = NAI::RUN );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogShot - a shot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogShot: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogShot);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<IAIUnit> pTarget;
	NAI::EHitLocation eHitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pTarget); f.Add(4,&eHitLocation); return 0; }
public:
	CAILogShot() {}
	CAILogShot(	IAIUnit *_pAIUnit, IAIUnit *_pTarget, NAI::EHitLocation _eHitLocation );
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogMelee - AI melee strike (release-new; retail id 0x52533166). Mirrors CAILogShot but carries the
// melee weapon used (pWeapon) beyond the target enemy + aimed hit location. GetCommands emits the same
// CCmdShootObject( pEnemy->GetUnitServer(), 0, eHitLocation ) as the shot record -- the replayed command is
// identical; this dedicated record exists for save-format fidelity (the melee weapon is preserved). Faithful
// to the matched-release decode (ctor @0x45c900, GetWorldCommands @0x460d20, operator& tags 2/pAIUnit,
// 3/pEnemy, 4/pWeapon, 5/eHitLocation). Emitted by CAIMeleeAction::Do @0x41d5b0 with hitLoc = HL_HEAD (=1).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogMelee: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogMelee);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<IAIUnit> pEnemy;
	CPtr<CAIMeleeWeapon> pWeapon;
	NAI::EHitLocation eHitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pEnemy); f.Add(4,&pWeapon); f.Add(5,&eHitLocation); return 0; }
public:
	CAILogMelee() {}
	CAILogMelee( IAIUnit *_pAIUnit, IAIUnit *_pEnemy, CAIMeleeWeapon *_pWeapon, NAI::EHitLocation _eHitLocation );
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogShotPoint
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogShotPoint: public CAILogRecord
{
	OBJECT_BASIC_METHODS( CAILogShotPoint );
	ZDATA
	ZPARENT( CAILogRecord )
	CVec3 ptTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord *)this); f.Add(3,&ptTarget); return 0; }
public:
	CAILogShotPoint() {}
	CAILogShotPoint( IAIUnit *_pAIUnit, CVec3 ptTarget );
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogHeal - self/ally first-aid (release CAILogHeal; emits a CCmdHeal on the patient).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogHeal: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogHeal);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<IAIUnit> pPatient;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pPatient); return 0; }
public:
	CAILogHeal() {}
	CAILogHeal( IAIUnit *_pAIUnit, IAIUnit *_pPatient );
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogLeavePK - climb OUT of the worn panzerklein (release-new; emits a CCmdExitPK on the unit
// server, but only when the recorded unit is still validly seated in a live PK at replay time). No
// extra payload beyond the inherited pUnit. Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_cailogleavepk.h, GetWorldCommands @0x61400). Registered id 0x23072480.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogLeavePK: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogLeavePK);
	ZDATA
	ZPARENT(CAILogRecord)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); return 0; }
public:
	CAILogLeavePK() {}
	CAILogLeavePK( IAIUnit *_pAIUnit ): CAILogRecord( _pAIUnit ) {}
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogWearPK - climb INTO a panzerklein (release-new; replays as a CCmdTakeCorpse aimed at the chosen
// suit's embedded CUnit -- "mount this body"). Carries the chosen suit's unit server beyond the inherited
// pUnit. Reconstructed from the matched-release decode (oracle: decomp/src/s2_cailogwearpk.h,
// GetWorldCommands @0x61110). Registered id 0x23069ac1.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogWearPK: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogWearPK);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NWorld::CUnitServer> pPK;       // the chosen suit's unit server
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pPK); return 0; }
public:
	CAILogWearPK() {}
	CAILogWearPK( IAIUnit *_pAIUnit, NWorld::CUnitServer *_pPK ): CAILogRecord( _pAIUnit ), pPK( _pPK ) {}
	// IAILogRecord
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogUseCannon / CAILogExitCannon - man / abandon a stationary cannon. Release-new AI plan records
// (the dev versions below were commented-out WIP); reconstructed for the heavy-gun actions. UseCannon
// replays as WishPose(RUN) + CCmdCannon on the unit server; ExitCannon as CCmdExitCannon. Each carries
// the chosen cannon. Registered 0x50442130 / 0x50442131.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogUseCannon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogUseCannon);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NWorld::CCannon> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pCannon); return 0; }
public:
	CAILogUseCannon() {}
	CAILogUseCannon( IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon ): CAILogRecord( _pAIUnit ), pCannon( _pCannon ) {}
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
class CAILogExitCannon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogExitCannon);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NWorld::CCannon> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pCannon); return 0; }
public:
	CAILogExitCannon() {}
	CAILogExitCannon( IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon ): CAILogRecord( _pAIUnit ), pCannon( _pCannon ) {}
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// The snipe-state-machine plan records (release-new; the whole AILog journal is retail-new). Reconstructed
// from the matched-release decode (oracle: decomp/src/s2_cailogmelee.h, s2_cailogcollectsnipeap.h,
// s2_cailogcancelaction.h + s2_ailog.h GetWorldCommands @0x60e10/@0x60f00/@0x61050). Each replays one
// world command, expressed in the in-tree CCmdSetCommand idiom. Registered 0x52253090/0x52253091/0x52353090.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogBeginSnipe - begin sniping an enemy: emit a CCmdShootObject aimed at the recorded enemy / hit
// location (the snipe shot, after the weapon + shoot-mode switch the BeginSnipe action also logs). Carries
// the target enemy + aimed hit location. (Emitted by CAIBeginSnipeAction::Do, whose GetInfoInner kill-zone
// scan is still deferred -- see aiSnipeAction.cpp -- so this record is wired + registered but not yet produced.)
class CAILogBeginSnipe: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogBeginSnipe);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<IAIUnit> pEnemy;
	NAI::EHitLocation eHitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pEnemy); f.Add(4,&eHitLocation); return 0; }
public:
	CAILogBeginSnipe() {}
	CAILogBeginSnipe( IAIUnit *_pAIUnit, IAIUnit *_pEnemy, NAI::EHitLocation _eHitLocation );
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
// CAILogCollectSnipeAP - bank sniper AP this turn: emit a CCmdCollectSnipeAP on the unit server. Carries the
// AP amount to collect. (Release emits CCmdCollectSnipeAP{ CSAP_PRECISE, nAP }; the dev command carries only
// the ECollectSnipeAP mode, so GetCommands uses CSAP_ALL -- documented divergence, see AILog.cpp.)
class CAILogCollectSnipeAP: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogCollectSnipeAP);
	ZDATA
	ZPARENT(CAILogRecord)
	int nAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nAP); return 0; }
public:
	CAILogCollectSnipeAP(): nAP(0) {}
	CAILogCollectSnipeAP( IAIUnit *_pAIUnit, int _nAP ): CAILogRecord( _pAIUnit ), nAP( _nAP ) {}
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
// CAILogCancelAction - cancel the unit's current action: emit a CCmdCancel on the unit server. No payload
// beyond the inherited pUnit. (Release names the command CCmdUnitCancelAction; the in-tree equivalent is
// CCmdCancel, itself a top-level interface command.)
class CAILogCancelAction: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogCancelAction);
	ZDATA
	ZPARENT(CAILogRecord)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); return 0; }
public:
	CAILogCancelAction() {}
	CAILogCancelAction( IAIUnit *_pAIUnit ): CAILogRecord( _pAIUnit ) {}
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogReloadWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogReloadWeapon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogReloadWeapon);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<CAIFireArmsWeapon> pWeapon;
	CObj<CAIFireArmsWeaponClip> pOldClip;
	CPtr<CAIFireArmsWeaponClip> pNewClip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pWeapon); f.Add(4,&pOldClip); f.Add(5,&pNewClip); return 0; }
public:
	CAILogReloadWeapon() {}
	CAILogReloadWeapon(	IAIUnit *_pAIUnit, CAIFireArmsWeapon *_pWeapon );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendAP
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogSpendAP: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogSpendAP);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldAP, nNewAP, nMaxAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldAP); f.Add(4,&nNewAP); f.Add(5,&nMaxAP); return 0; }
public:
	CAILogSpendAP() {}
	CAILogSpendAP(	IAIUnit *_pAIUnit, int nSpendAP );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendHP
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogSpendHP: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogSpendHP);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldHP, nNewHP, nMaxHP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldHP); f.Add(4,&nNewHP); f.Add(5,&nMaxHP); return 0; }
public:
	CAILogSpendHP() {}
	CAILogSpendHP(	IAIUnit *_pAIUnit, int nSpendHP );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogHurt
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogHurt: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogHurt);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldHurtHP, nNewHurtHP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldHurtHP); f.Add(4,&nNewHurtHP); return 0; }
public:
	CAILogHurt() {}
	CAILogHurt(	IAIUnit *_pAIUnit, int nHurtHP );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendAmmo
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogSpendAmmo: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogSpendAmmo);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldAmmo, nNewAmmo;
	CPtr<CAIFireArmsWeaponClip> pClip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldAmmo); f.Add(4,&nNewAmmo); f.Add(5,&pClip); return 0; }
public:
	CAILogSpendAmmo() {}
	CAILogSpendAmmo( CAIFireArmsWeaponClip *_pClip, int nSpendAmmo );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogChangeWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogChangeWeapon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogChangeWeapon);
	ZDATA
	ZPARENT(CAILogRecord)
	CObj<IAIInventoryItem> pOldWeapon;
	CObj<IAIInventoryItem> pNewWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pOldWeapon); f.Add(4,&pNewWeapon); return 0; }
	//
	void GetItemPosition( NRPG::IInventoryItem *pItem, CTPoint<int> *Position );
public:
	//
	CAILogChangeWeapon() {}
	CAILogChangeWeapon(	IAIUnit *_pAIUnit, IAIInventoryItem *_pWeapon );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogAddWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogAddWeapon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogAddWeapon);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<CAIFireArmsWeapon> pWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pWeapon); return 0; }
public:
	//
	CAILogAddWeapon() {}
	CAILogAddWeapon(	IAIUnit *_pAIUnit, CAIFireArmsWeapon *_pWeapon );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogAddWeaponClip
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogAddWeaponClip: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogAddWeaponClip);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<CAIFireArmsWeapon> pWeapon;
	CPtr<CAIFireArmsWeaponClip> pClip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pWeapon); f.Add(4,&pClip); return 0; }
public:
	//
	CAILogAddWeaponClip() {}
	CAILogAddWeaponClip(	IAIUnit *_pAIUnit, CAIFireArmsWeapon *_pWeapon, CAIFireArmsWeaponClip *_pClip );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogPickUpItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogPickUpItem: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogPickUpItem);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NRPG::IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pItem); return 0; }
public:
	//
	CAILogPickUpItem() {}
	CAILogPickUpItem(	IAIUnit *_pAIUnit, NRPG::IInventoryItem *_pItem );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogDropItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogDropItem: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogDropItem);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NRPG::IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pItem); return 0; }
public:
	//
	CAILogDropItem() {}
	CAILogDropItem(	IAIUnit *_pAIUnit, NRPG::IInventoryItem *_pItem );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogThrowGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogThrowGrenade: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogThrowGrenade);
	ZDATA
	ZPARENT(CAILogRecord)
	CObj<CAIGrenadeWeapon> pGrenade;
	CVec3 ptTarget;
	CPtr<IAIUnit> pEnemy;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pGrenade); f.Add(4,&ptTarget); f.Add(5,&pEnemy); return 0; }
public:
	CAILogThrowGrenade() {}
	CAILogThrowGrenade(	IAIUnit *_pUnit, CVec3 _ptTarget, CAIGrenadeWeapon *_pGrenade );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogThrowKnife - throw a knife at the enemy (release-new; retail NAI::CAILogThrowKnife @0x45c770).
// GetCommands (retail GetWorldCommands @0x460c30) emits the SAME weapon-agnostic CCmdShootObject as
// CAILogShot(pEnemy,HL_ANY) -- eHL is PINNED to -1/HL_ANY (a knife throw is not aimed at a body zone).
// UNLIKE CAILogShot it ALSO carries the thrown knife, so Commit (retail ModifyState @0x45c7f0) removes it
// from the AI inventory and empties the hand -- the simulated-state effect CAILogShot's base no-op Commit
// silently dropped. Members mirror the decode: pEnemy @+0x10, pWeapon @+0x14, eHitLocation @+0x18. Reg 0x52533165.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogThrowKnife: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogThrowKnife);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<IAIUnit> pEnemy;
	CPtr<CAIThrowingWeapon> pWeapon;
	NAI::EHitLocation eHitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pEnemy); f.Add(4,&pWeapon); f.Add(5,&eHitLocation); return 0; }
public:
	CAILogThrowKnife() {}
	CAILogThrowKnife( IAIUnit *_pAIUnit, IAIUnit *_pEnemy, CAIThrowingWeapon *_pWeapon, NAI::EHitLocation _eHitLocation );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogExpediency
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogExpediency: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogExpediency);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldExpediency;
	int nNewExpediency;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldExpediency); f.Add(4,&nNewExpediency); return 0; }
public:
	CAILogExpediency() {}
	CAILogExpediency(	IAIUnit *_pAIUnit, int nExpediency );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogChangeShootMode: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogChangeShootMode);
	ZDATA
	ZPARENT(CAILogRecord)
	CPtr<NRPG::CWeaponItem> pWeaponItem;
	NDb::EShootMode eOldShootMode;
	NDb::EShootMode eNewShootMode;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&pWeaponItem); f.Add(4,&eOldShootMode); f.Add(5,&eNewShootMode); return 0; }
public:
	CAILogChangeShootMode() {}
	CAILogChangeShootMode( IAIUnit *_pUnit, CAIFireArmsWeapon *pWeapon, NDb::EShootMode _eShootMode );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogChangeMaxToHit: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogChangeMaxToHit);
	ZDATA
	ZPARENT(CAILogRecord)
	int nOldToHit;
	int nNewToHit;	
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord*)this); f.Add(3,&nOldToHit); f.Add(4,&nNewToHit); return 0; }
public:
	CAILogChangeMaxToHit() {}
	CAILogChangeMaxToHit( IAIUnit *_pAIUnit, int nMaxToHit );
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogHide: public CAILogRecord
{
	OBJECT_BASIC_METHODS( CAILogHide );
	ZDATA
	ZPARENT( CAILogRecord )
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogRecord *)this); return 0; }
	//
public:
	CAILogHide() {}
	CAILogHide( IAIUnit *_pAIUnit );
	//
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
/*
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogUseCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogUseCannon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogUseCannon);
	ZDATA_(CAILogRecord)
	CPtr<NWorld::CCannon> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAILogRecord*)this); f.Add(2,&pCannon); return 0; }
public:
	CAILogUseCannon() {}
	CAILogUseCannon(	IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon );
	// IAILogRecord
	virtual void RollBack() {}
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogExitCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogExitCannon: public CAILogRecord
{
	OBJECT_BASIC_METHODS(CAILogExitCannon);
	ZDATA_(CAILogRecord)
	CPtr<NWorld::CCannon> pCannon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAILogRecord*)this); f.Add(2,&pCannon); return 0; }
public:
	CAILogExitCannon() {}
	CAILogExitCannon(	IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon );
	// IAILogRecord
	virtual void RollBack() {}
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
};
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogContainer *CreateAILogContainer();
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif