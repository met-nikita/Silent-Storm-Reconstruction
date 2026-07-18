#ifndef __wDumbUnit_H_
#define __wDumbUnit_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#include "wInterface.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "wAnimation.h"
#include "wDynObject.h"
#include "RPGAttackMech.h"

namespace NDb
{
	class CRPGArmor;
	class CTSound;
	class CAISound;
	enum ECritical;
	enum ESlot;
	class CPanzerklein;
}

namespace NAI
{
	enum EHitLocation;
}
namespace NRPG
{
	enum EAction;
	//enum ECriticalAction;
	class IUnitMission;
	class IInventoryInfo;   // NWorld::IsActiveItemToShow arg
}
struct SStepSound;
namespace NWorld
{
class CWorld;
// @0x34ef50 -- is the inventory's active item one to SHOW in the unit's hand (its DB record's bPlaceInHand)?
bool IsActiveItemToShow( NRPG::IInventoryInfo *pInv );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayer;
struct SInterruptInfo;
class IDynamicObject;
class CTimedObject;
class C3DSound;
class CUnit;
class CWorld;
class CMine;
// @PDB NWorld::ECanMoveRes (gen/include/s2_types.h:3711) -- retail widened the movement
// gate from bool to this 5-valued verdict so the wait-state recovery can branch on
// LOCKED (locker chain) vs DOOR (reroute) vs impassable. Values are ordinal-sensitive
// (CanDoGameMove composes CMR_CANNOT_MOVE==1 via (ECanMoveRes)(CanMove()==0); CheckCanDoMove
// switches on LOCKED==3 / DOOR==4). NOTE: producing LOCKED/DOOR needs the per-place NAI
// EPassable subsystem the dev IPathNetwork lacks (see BLUEPRINT_cpathconflicts_ecanmoveres
// sec 3) -- until then CheckPassable only emits CMR_YES / CMR_NOT_PASSABLE.
enum ECanMoveRes
{
	CMR_YES          = 0,
	CMR_CANNOT_MOVE  = 1,
	CMR_NOT_PASSABLE = 2,
	CMR_LOCKED       = 3,
	CMR_DOOR         = 4,
};
class CDumbUnitServer: public IVisObj, public NRPG::IAttackable
{
	ZDATA
	CObj<NRPG::IUnitMission> pRPG;
	NAI::SPathPlace nextLock;
	bool bLocksTwoPlaces;
	NAI::SUnitPosition position;
	bool bStrafe;
	NAI::EPose wishPose;
	CPtr<CWorld> pWorld;
	CSyncSrcBind<IVisObj> bindGlobal;
	bool bUndrawWeapon;
	bool bNoHeavyWeapon;
	list<CObj<IDynamicObject> > miscObjects;
protected:
	CPtr<NDb::CModel> pModel;
	bool bIsPKWhichIsWeared;
	// --- release scriptParticles / hand-attach save-format members (Tier-B; behavior deferred) ---
	// The release inserted these between bIsPKWhichIsWeared (tag 14) and animator (re-tagged 15->19),
	// dropping bJustUnhided/pAIMapHull from serialization. Decoded byte-exact from
	// NWorld::CDumbUnitServer::operator& @0x753aa0 (tags 2-28). No live code references them yet --
	// added for save-format convergence only; setters/render-feed wiring (AttachEffect/SetHand*/
	// SetBloodyDeath/corpse-AI tracking) reach absent subsystems and stay deferred.
	CPtr<NDb::CModel> pHandModel;        // tag 15
	CDBPtr<NDb::CEffect> pHandEffect;    // tag 16
	STime tBeginHandEffect;              // tag 17
	bool bBloodyDeath;                   // tag 18
public:
	CUnitAnimator animator;              // tag 19 (was tag 15 in the predecessor)
private:
	int nPrevFloor;                      // tag 20 (was 18)
	bool bHeadless;                      // tag 21 (was 19)
	bool bCanHide;                       // tag 22
	bool bTemporaryAimed;                // tag 23
	CVec3 vPrevGetCorpseAIPosition;      // tag 24
	bool bNotAddedToVisitors;            // tag 25
	vector<IRenderVisitor::SBoundEffect> attachedEffects;  // tag 26 (scriptParticles)
	bool bTrackSequence;                 // tag 27
	vector<CVec3> corpseHLpos;           // tag 28
	// dev-only members: kept (used by IsJustUnhided/Hide/GetAIMapUnitHull) but NOT serialized by
	// the release operator& (the release tracks hiding via bCanHide instead of bJustUnhided).
	bool bJustUnhided;
	CPtr<CObjectBase> pAIMapHull;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pRPG); f.Add(3,&nextLock); f.Add(4,&bLocksTwoPlaces); f.Add(5,&position); f.Add(6,&bStrafe); f.Add(7,&wishPose); f.Add(8,&pWorld); f.Add(9,&bindGlobal); f.Add(10,&bUndrawWeapon); f.Add(11,&bNoHeavyWeapon); f.Add(12,&miscObjects); f.Add(13,&pModel); f.Add(14,&bIsPKWhichIsWeared); f.Add(15,&pHandModel); f.Add(16,&pHandEffect); f.Add(17,&tBeginHandEffect); f.Add(18,&bBloodyDeath); f.Add(19,&animator); f.Add(20,&nPrevFloor); f.Add(21,&bHeadless); f.Add(22,&bCanHide); f.Add(23,&bTemporaryAimed); f.Add(24,&vPrevGetCorpseAIPosition); f.Add(25,&bNotAddedToVisitors); f.Add(26,&attachedEffects); f.Add(27,&bTrackSequence); f.Add(28,&corpseHLpos); return 0; }

public:
	// retail CUnit vtbl+0x94 thunk @0x3c68e0 (lea eax,[corpseHLpos]): CUnitServer forwards its CUnit
	// override here (CUnit is a SIBLING base, so this plain accessor is not itself the virtual).
	const vector<CVec3>* GetCorpseHLs() const { return &corpseHLpos; }
private:
	void ProcessSteps( const STime tCurrent );
	void PlaySound( NDb::CTSound *pSound );
	void SetPositionCore( const NAI::SUnitPosition &dst );
	NDb::CSound* GetStepSound( NDb::CRPGArmor *pArmor );
	NDb::CAISound* GetStepAISound();
	void MakeStepSound( bool bSound );
	NDb::CRPGArmor* GetArmor();
	void GetUnitPositionForVisit( NAI::SUnitPosition *pPos );
public:
	int GetFloor();   // retail @0x34f610; public: LostWeapon @0x3b5c80 / LaunchItem @0x3a7fe0 call it
	bool IsAddedToVisitor();   // present in the world visitor set (used by the AI WearPK candidate scan)
private:
	void FallAsIfDead( const CVec3 &ptDir, bool bDropItemsFromBackPack, bool bPlayDeathAnim );   // retail @0x350770 3-arg
	void DropItems( bool bDropHands, bool bDropCap, bool bDropBackPack );                        // retail @0x3502c0 3-arg (split bHands -> hands/cap)
	void BlowUp();
protected:
	bool IsLocker();
	friend bool NAI::IsLockerUnit( CObjectBase *pUnit ); // aiPositionDebug locker-validity probe (retail @0x917d0)
	// retail @0x3c2190 Die(bool,bool) -- arg1 bDeathBeauty ORs into the beauty-cam condition
	// (the gib path forces it); arg2 bRemove = the silent removal flavor (no ack/state/camera).
	virtual void Die( bool bDeathBeauty = false, bool bRemove = false ) {}
	virtual void OnUnitMadeUnconscious( bool bFromScript = false ) {}
	virtual void OnSuffersDamage( float fAP )	{} // how much is left, in percent
	virtual void ProcessCritical( NDb::ECritical eCA ) {}
	virtual void TouchedMines( const vector<CPtr<CMine> > &mines ) {}
	virtual void RemoveFromWorld() {}
	// retail unit vtbl+0x24, tail of every KillUnit/MakeUnconscious branch; CUnitServer @0x3c3cb0
	// flushes the heard-sounds list (a downed unit stops reacting to queued noise).
	virtual void OnLifeLost() {}
	// retail unit vtbl+0x28 (purecall in the CDumbUnitServer vftable @0x8c9f5c; implemented by
	// CUnitServer @0x3c0420): may this unit be gibbed by a heavy hit?
	virtual bool CanBlowUp() { return false; }
	CObjectBase* GetAIMapUnitHull() { return pAIMapHull; }
	NDb::CModel* GetUnitModel() const { return pModel; }
	bool IsEmptyPK() const;
	float GetMaxFallDist( float extraDrop ) const;   // @0x34f1c0: ray-origin z = extraDrop + 0.2f
public:
	struct SResItem
	{
		CVec3 ptCenter;
		CQuat q;
		CPtr<NDb::CModel> pModel;
		CObj<NRPG::IInventoryItem> pItem;
	};

	CDumbUnitServer() {}
	CDumbUnitServer( CWorld *pWorld, NRPG::IUnitMission *_pRPG, NDb::CModel *pModel, const NAI::SUnitPosition &pos );
	void KillUnit( const CVec3 &ptDir );
	bool SetBloodyDeath( bool b );   // retail @0x34ec90 get/set: install the bloody-death flag, return the prior value
	// retail @0x350dc0 3-arg: the 3rd arg is FallAsIfDead's bPlayDeathAnim -- a combat knock-out
	// (ProcessAttack @0x75130e pushes 1) DOES play the death clip; lua passes its optional 2nd param.
	void MakeUnconscious( const CVec3 &ptDir, bool bFromScript = false, bool bPlayDeathAnim = true );
	void SyncConscious();   // retail @0x3515a0: reconcile world alive/conscious state with the RPG persona
	void SetPosition( const NAI::SUnitPosition &dst );
	void SetTemporaryPosition( const NAI::SUnitPosition &dst ) // to be used only in CExecQueue; in all other cases use SetPosition
	{	position = dst;	}
	void DoGameMove( const NAI::SUnitPosition &dst );
	void LockNextPlace( const NAI::SUnitPosition &dst );
	ECanMoveRes CanDoGameMove( const NAI::SUnitPosition &dst );  // @0x34f100 retail widened bool->ECanMoveRes
	ECanMoveRes CheckPassable( const NAI::SUnitPosition &dst );  // @0x34efd0 retail widened bool->ECanMoveRes
	int GetActionAP( NRPG::EAction action ) const;
	int GetAP() const;
	bool CanSpendAP( int nAP ) const;
	void SpendAP( int nAP );
	void DoAction( NRPG::EAction action ); // register action & spends AP
	void GetBonePos( CVec3 *pRes, CQuat *pQuat, const char *pszBoneName );
	bool TearOffItem( SResItem *pRes, NDb::ESlot slot, bool bPlaceNextSameItem = false );
	void AttachMiscObject( CTimedObject *p );
	void SetRunning( bool bRun ) { position.bRun = bRun; }

	NRPG::IUnitMission* GetUnitRPG() const { return pRPG; }
	const NAI::SUnitPosition& GetUnitPosition() const { return position; }
	NAI::SUnitPosition GetUnitSetPosePosition() const;   // retail @0x34f430: pose-change anchor (nextLock mid-step in realtime)
	void GetRealUnitPosition( CVec3 *pRes );
	CWorld* GetWorld() const { return pWorld; }

	virtual bool IsStrafing() const { return bStrafe; }
	void SetStrafe( bool _bStrafe ) { bStrafe = _bStrafe; }
	NAI::EPose GetWishPose() const { return wishPose; }
	void SetWishPose( NAI::EPose pose ) { wishPose = pose; }
	
	// @0x351630 -- retail returns the created burst C3DSound* (0 when no sound was emitted) for
	// CExecShoot's long-burst retention slot (SLongBurstSnd, save tag 3)
	C3DSound* CreateFlash( bool bLeft, bool bFirstBullet );
	void Update() { bindGlobal.Update(); }
	// wCheckTooMuchCorpses corpse-density failsafe (compiland wCheckTooMuchCorpses.obj):
	// flag this unit OUT of the world visitor set (retail bNotAddedToVisitors @+0x12c = 1).
	// RemoveOldestCorpse follows it with Update() to refresh the global vis-binding; the
	// bulk-mark path does not (release-faithful asymmetry).
	void MarkNotAddedToVisitors() { bNotAddedToVisitors = true; }
	// the pocket path is the only place the flag is cleared again: retail CUnitStateInPocket
	// OnStateStarted @0x3c9fb0 sets +0x12c = 1, OnStateFinished @0x3ca060 sets it back to 0.
	void ClearNotAddedToVisitors() { bNotAddedToVisitors = false; }
	void SetUndrawItem( bool _bUndraw, bool _bNoHeavyWeapon = false ) { bUndrawWeapon = _bUndraw; bNoHeavyWeapon = _bNoHeavyWeapon; Update(); }
	bool GetUndrawItem() { return bUndrawWeapon; }
	// release @0x74fd50 (UnitHoldItem): park a model in the unit's hand (the "Item" bind bone) + refresh the vis
	// binding so Visit re-feeds it. A null model clears the held model. (The hand EFFECT half stays deferred --
	// the dev's IRenderVisitor::SBoundMesh has no effect field.)
	void SetHandModel( NDb::CModel *_pHandModel ) { pHandModel = _pHandModel; Update(); }
	// release @0x34fd90 (UnitHoldEffect): park a particle EFFECT in the unit's hand + refresh the vis
	// binding so Visit re-feeds it; tBegin stamps the effect's start time. Mirror of SetHandModel
	// @0x34fd50; writes the already-serialized pHandEffect(tag16)/tBeginHandEffect(tag17). The
	// CDBPtr assignment does the AddRef-new / Release-old the decode shows.
	void SetHandEffect( NDb::CEffect *_pHandEffect, STime tBegin ) { pHandEffect = _pHandEffect; tBeginHandEffect = tBegin; Update(); }
	// release @0x6e96e0 (AttachEffectToUnitBone): register a particle effect to play on the unit; Visit feeds it
	// to the renderer via AddParticleEffect with the unit's skeleton animator (so the effect's glue-to-bone
	// instances attach to the unit's bones). tBegin = the effect's start time.
	void AttachEffect( STime tBegin, NDb::CEffect *pEffect );

	void AddMiscObjects( vector<IVisObj*> *pRes );
	// implement IVisObj
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	virtual void Visit( ISoundVisitor* );   // @0x34fdd0: PK engine loop + hand-effect 3D sound
	// implement IAttackable
	virtual int ProcessAttack( NWorld::IWorld *_pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
		const CVec3 &vDir, NDb::CRPGArmor *pArmor );

	void Segment();
	void PlaceOnPassablePlace();
	virtual CDumbUnitServer *GetCorpse() { return 0; }
	void InitAsCorpse( bool bDead );

	bool IsWearingPK() { return IsValid( GetWearingDBPK() ); }
	virtual NDb::CPanzerklein *GetWearingDBPK() { ASSERT(0); return 0; }
	bool WearAsPK( bool bWear );
	void Hide( bool bHide, bool bThrowEvent = true );   // @0x34fb10: bCanHide-gated 2-arg form
	void EnableHide();                                   // @0x34ed00: re-arm hiding (per-fast-turn)
	bool IsJustUnhided() { return bJustUnhided; }
	virtual bool IsDead() const = 0;
	virtual bool IsUnconscious() const = 0;
	virtual bool CanFight() const = 0;
};
// retail LaunchItem @0x34f500 (IWorld*, SResItem&, CVec3&, bool, CObjectBase*, int): forwards
// bFallFromBody (-> CASphereSet::Init phase PH_THROW_OUT+bool), the visibility parent (the
// launching/dying unit; fog-gates the flying item AND its settled frozen form) and nFloor
// (-> CDItem floor, roof-cut culling) to AddDebris @0x34ade0, then UpdateVisible(0).
void LaunchItem( CWorld *pWorld, const CDumbUnitServer::SResItem &item, const CVec3 &vel, bool bFallFromBody, CObjectBase *pVisibilityParent, int nFloor );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
