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
}
struct SStepSound;
namespace NWorld
{
class CWorld;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayer;
struct SInterruptInfo;
class IDynamicObject;
class CTimedObject;
class CUnit;
class CWorld;
class CMine;
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

private:
	void ProcessSteps( const STime tCurrent );
	void PlaySound( NDb::CTSound *pSound );
	void SetPositionCore( const NAI::SUnitPosition &dst );
	NDb::CSound* GetStepSound( NDb::CRPGArmor *pArmor );
	NDb::CAISound* GetStepAISound();
	void MakeStepSound( bool bSound );
	NDb::CRPGArmor* GetArmor();
	void GetUnitPositionForVisit( NAI::SUnitPosition *pPos );
	int GetFloor();
public:
	bool IsAddedToVisitor();   // present in the world visitor set (used by the AI WearPK candidate scan)
private:
	void FallAsIfDead( const CVec3 &ptDir, bool bDropItemsFromBackPack );
	void DropItems( bool bHands, bool bBackPack );
	void BlowUp();
protected:
	bool IsLocker();
	friend bool NAI::IsLockerUnit( CObjectBase *pUnit ); // aiPositionDebug locker-validity probe (retail @0x917d0)
	virtual void Die( bool bRemove = false ) {}
	virtual void OnUnitMadeUnconscious( bool bFromScript = false ) {}
	virtual void OnSuffersDamage( float fAP )	{} // ������� �������� � ���������
	virtual void ProcessCritical( NDb::ECritical eCA ) {}
	virtual void TouchedMines( const vector<CPtr<CMine> > &mines ) {}
	virtual void RemoveFromWorld() {}
	CObjectBase* GetAIMapUnitHull() { return pAIMapHull; }
	NDb::CModel* GetUnitModel() const { return pModel; }
	bool IsEmptyPK() const;
	float GetMaxFallDist() const;
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
	void MakeUnconscious( const CVec3 &ptDir, bool bFromScript = false );
	void SetPosition( const NAI::SUnitPosition &dst );
	void SetTemporaryPosition( const NAI::SUnitPosition &dst ) // to be used only in CExecQueue; in all other cases use SetPosition
	{	position = dst;	}
	void DoGameMove( const NAI::SUnitPosition &dst );
	void LockNextPlace( const NAI::SUnitPosition &dst );
	bool CanDoGameMove( const NAI::SUnitPosition &dst );
	bool CheckPassable( const NAI::SUnitPosition &dst );
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
	void GetRealUnitPosition( CVec3 *pRes );
	CWorld* GetWorld() const { return pWorld; }

	virtual bool IsStrafing() const { return bStrafe; }
	void SetStrafe( bool _bStrafe ) { bStrafe = _bStrafe; }
	NAI::EPose GetWishPose() const { return wishPose; }
	void SetWishPose( NAI::EPose pose ) { wishPose = pose; }
	
	void CreateFlash();
	void Update() { bindGlobal.Update(); }
	// wCheckTooMuchCorpses corpse-density failsafe (compiland wCheckTooMuchCorpses.obj):
	// flag this unit OUT of the world visitor set (retail bNotAddedToVisitors @+0x12c = 1).
	// RemoveOldestCorpse follows it with Update() to refresh the global vis-binding; the
	// bulk-mark path does not (release-faithful asymmetry).
	void MarkNotAddedToVisitors() { bNotAddedToVisitors = true; }
	void SetUndrawItem( bool _bUndraw, bool _bNoHeavyWeapon = false ) { bUndrawWeapon = _bUndraw; bNoHeavyWeapon = _bNoHeavyWeapon; Update(); }
	bool GetUndrawItem() { return bUndrawWeapon; }
	// release @0x74fd50 (UnitHoldItem): park a model in the unit's hand (the "Item" bind bone) + refresh the vis
	// binding so Visit re-feeds it. A null model clears the held model. (The hand EFFECT half stays deferred --
	// the dev's IRenderVisitor::SBoundMesh has no effect field.)
	void SetHandModel( NDb::CModel *_pHandModel ) { pHandModel = _pHandModel; Update(); }
	// release @0x6e96e0 (AttachEffectToUnitBone): register a particle effect to play on the unit; Visit feeds it
	// to the renderer via AddParticleEffect with the unit's skeleton animator (so the effect's glue-to-bone
	// instances attach to the unit's bones). tBegin = the effect's start time.
	void AttachEffect( STime tBegin, NDb::CEffect *pEffect );

	void AddMiscObjects( vector<IVisObj*> *pRes );
	// implement IVisObj
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	// implement IAttackable
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );

	void Segment();
	void PlaceOnPassablePlace();
	virtual CDumbUnitServer *GetCorpse() { return 0; }
	void InitAsCorpse( bool bDead );

	bool IsWearingPK() { return IsValid( GetWearingDBPK() ); }
	virtual NDb::CPanzerklein *GetWearingDBPK() { ASSERT(0); return 0; }
	bool WearAsPK( bool bWear );
	void Hide( bool bHide );
	bool IsJustUnhided() { return bJustUnhided; }
	virtual bool IsDead() const = 0;
	virtual bool IsUnconscious() const = 0;
	virtual bool CanFight() const = 0;
};
void LaunchItem( CWorld *pWorld, const CDumbUnitServer::SResItem &item, const CVec3 &vel = VNULL3 );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
