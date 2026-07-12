#ifndef __wUnitServer_H_
#define __wUnitServer_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "wDumbUnit.h"
#include "wUnitSounds.h"
#include "wTurnBased.h"
#include "wVision.h"
#include "wInterface.h"
#include "..\Misc\EventsBase.h"
#include "eventPlayer.h"
namespace NDb
{
	class CAISound;
	enum ECritical;
	enum EDiplomacyState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
//
//class CEventOnNewPlayerTurnOrTime;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitServer;
class CUnitStateSniping;
class CPathConflictsRemover;
class CCommandExecute: public CObjectBase
{
public:
	enum EFinishType
	{
		RUNNING,
		FINISHED,
		FAILED
	};
private:
	ZDATA
	CObj<CActionCounter> pAction;
	EFinishType state;
protected:
	CPtr<CUnitServer> pUS;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAction); f.Add(3,&state); f.Add(4,&pUS); return 0; }
protected:
	enum EActionType
	{
		NORMAL,
		SKIPPABLE,
		NOBLOCK
	};
	void StartAction( CWorld *pWorld, EActionType actionType );
	void StopAction() { pAction = 0; }
	void Finished() { state = FINISHED; }
	void Failed() { /*ASSERT( 0 );*/ state = FAILED; }
public:
	CCommandExecute( CUnitServer *_pUS = 0 ): state(RUNNING), pUS(_pUS) {}
	EFinishType GetState() const { return state; }
	virtual int GetStartAP() const { return 0; }
	virtual int GetActionAP() const { return GetStartAP(); }
	virtual void Run() { Finished(); }
	virtual bool TimeLabelReached() { return false; }
	virtual void AnimationFinished() { Finished(); }
	virtual void Cancel() {}
	virtual bool IsExecuting() { return IsValid( pAction ); }
	virtual bool IsWaitingForPath( NAI::SUnitPosition *p = 0 ) { return false; }
	// @0x3cc670 -- per-tick service of the move wait-state (path-conflict recovery). Base no-op;
	// CPathConflictsRemover/CExecMove tick the wait timer + retry, CSimpleExecQueue fans out to its front.
	virtual void Segment() {}
	// @0x3bc3f0/@0x3bd240 -- the CPathConflictsRemover that services this executor's move wait-state
	// (CExecMove returns itself, CExecQueue its front mover's, others none). Kept on the executor base
	// rather than IExecMove so CUnitServer/CExecQueue reach it by a plain virtual, no IExecMove cross-cast.
	virtual CPathConflictsRemover* GetPathConflictsRemover() { return 0; }
	virtual NAI::CPath* GetCurrentPath() const { return 0; }
	CUnitServer* GetUnitServer() const { return pUS; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPathViewer: public IPathViewer
{
	OBJECT_BASIC_METHODS(CPathViewer)
private:
	ZDATA
	CPtr<CUnitServer> pUS;
	CObj<CCommandExecute> pMove;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUS); f.Add(3,&pMove); return 0; }

public:
	CPathViewer() {}
	CPathViewer( CUnitServer *_pUS );

	void SetPath( NAI::CPath *_pPath );

	int GetResult() const;
	void GetPoints( vector<SPathPoint> *pRes );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitState;
class CUnitServer: public CDumbUnitServer, public CUnit, public CTBSUnit<CUnitServer, CPlayer>,
	public CSoundsTracker<CUnitServer>, public CAudibleSet<CUnitServer>, public CTBSUnitVision<CUnitServer,CPlayer>
{
	OBJECT_NOCOPY_METHODS(CUnitServer);
	typedef CTBSUnit<CUnitServer, CPlayer> TTBSUnit;
	typedef CSoundsTracker<CUnitServer> CUSSoundTracker;
	typedef CAudibleSet<CUnitServer> CASet;
	typedef CTBSUnitVision<CUnitServer,CPlayer> TTBSUnitVision;
	//
	NGlobal::CEventRegister< CUnitServer, NWorld::CEventOnNewPlayerTurnOrTime > registerOnNewPlayerTurnOrTime;
	NGlobal::CEventRegister< CUnitServer, NWorld::CEventOnNewPlayerFastTurnOrTime > registerOnNewPlayerFastTurnOrTime;   // @0x3c0300: per-fast-turn hide re-arm
	//
	ZDATA
	ZPARENT( CDumbUnitServer )
	ZPARENT( TTBSUnit )
	ZPARENT( CUSSoundTracker )
	ZPARENT( CASet )
	ZPARENT( TTBSUnitVision )
	CObj<CCommandExecute> pExec;
	bool bCallTimeLabel;
	CObj<CCmd> pCurrentCmd;
	CObj<CUnitState> pState;
	list<NDb::ECritical> criticals; // pending criticals
	bool bIsRunningForcedAction;
	CObj<CCmd> pAutoRunCmd;
	list<CPtr<CUnitServer> > wasInterruptedList; // has had interrupt during current turn
	list< CPtr<CUnitServer> > lostUnits;
	float fLastHeight;
	NAI::SPathPlace plLast;
	CPtr<CUnitServer> pWearingPK;
	bool bIsPK;
	STime tPrev;
	int nDialog;
	bool bCanTalk;
	int nScriptToHit = -1;	// UnitSetToHit override (retail CUnitServer+0x1e8); -1 = use the computed to-hit
	bool bForceNoChangePose = false;	// retail CUnitServer+0x1f4 (luaUnitLockPose writes it): a script "pose lock".
										// Consumed by CannotFreelyChangePoses() -> FindPath move-only (keeps the pose).
public:
	CPtr<CUnitServer> pKiller;	// retail CUnitServer+0x1f8 (serialized tag 0x19): read by CAICorpseEvent::Modify
								// @0x3c470 (killer -> possibleEnemy). ⚠ retail NEVER writes it (byte-scan proven:
								// only ctor-null + serializer) -- the arm is retail-inert; kept 1:1.
private:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDumbUnitServer *)this); f.Add(3,(TTBSUnit *)this); f.Add(4,(CUSSoundTracker *)this); f.Add(5,(CASet *)this); f.Add(6,(TTBSUnitVision *)this); f.Add(7,&pExec); f.Add(8,&bCallTimeLabel); f.Add(9,&pCurrentCmd); f.Add(10,&pState); f.Add(11,&criticals); f.Add(12,&bIsRunningForcedAction); f.Add(13,&pAutoRunCmd); f.Add(14,&wasInterruptedList); f.Add(15,&lostUnits); f.Add(16,&fLastHeight); f.Add(17,&plLast); f.Add(18,&pWearingPK); f.Add(19,&bIsPK); f.Add(20,&tPrev); f.Add(21,&nDialog); f.Add(22,&bCanTalk); f.Add(23,&nScriptToHit); f.Add(24,&bForceNoChangePose); f.Add(25,&pKiller); return 0; }
	void RefreshExecutor();
	void CheckCmdExecState();
	void Fall();
	void ForcedMove();
	// CDumbUnit callbacks
	virtual void OnUnitMadeUnconscious( bool bFromScript = false );
	virtual void OnSuffersDamage( float fAP );
	virtual void ProcessCritical( NDb::ECritical eCA );
	virtual void TouchedMines( const vector<CPtr<CMine> > &mines );
	virtual void RemoveFromWorld();
	void CancelAction();
	void FetchRPGAcks();
	void CancelHeal();
	
public:
	int GetScriptToHit() const { return nScriptToHit; }			// UnitSetToHit override (-1 = none)
	void SetScriptToHit( int n ) { nScriptToHit = n; }
	// --- wCheckTooMuchCorpses corpse-density failsafe convergence surface ---
	// Release CUnitServer carried a death timestamp (retail +0x228) and a quest-clue
	// counter (retail +0x1e4); the dev class had not yet reconstructed them. Appended as
	// non-serialized fields (operator& tags unchanged, so existing member offsets are
	// untouched) and read by the corpse helpers to flag the OLDEST corpse first and spare
	// clue-carrying corpses. Nothing sets them yet (parity surface only).
	unsigned long tDeathTime = 0;	// retail +0x228: tick the unit became a corpse
	int nClueCount = 0;				// retail +0x1e4: number of quest clues carried
	unsigned long GetDeathTime() const { return tDeathTime; }
	bool IsClueUnit() const { return nClueCount > 0; }	// retail CUnitServer::IsClueUnit @0x3c6900 (setg: > 0)
	// retail CUnitServer::CanBlowUp @0x3c0420 (unit vtbl+0x28): a unit may be gibbed unless it is
	// a quest-clue corpse (CUnit vtbl+0x14 IsClueUnit), wears a live PK shell (vtbl+0x34
	// GetWearingDBPK), or IS an empty PK shell (CUnit vtbl+0x10 IsEmptyPK).
	virtual bool CanBlowUp() { return !IsClueUnit() && !IsValid( GetWearingDBPK() ) && !IsEmptyPK(); }
	CUnitServer();
	CUnitServer( CWorld *pWorld, NRPG::IUnitMission *_pRPG, NDb::CModel *pModel, CPlayer *pPlayer, const NAI::SUnitPosition &pos );
	// events
	void OnNewPlayerTurnOrTime( const CEventOnNewPlayerTurnOrTime &event );
	void OnNewPlayerFastTurnOrTime( const CEventOnNewPlayerFastTurnOrTime &event );   // @0x3c0300: re-arm hiding
	//
	// CDumdUnit callbacks
	virtual void Die( bool bRemove = false );
	// implement CTBSUnit
	virtual void Do( CCommand* );
	virtual bool IsMoving() const; 
	virtual bool IsDead() const;
	virtual bool IsUnconscious() const;
	virtual bool CanFight() const { return !IsDead() && !IsUnconscious(); }
	virtual bool IsHiding() const;
	virtual bool IsEmptyPK() const { return bIsPK; }
	virtual void OnTBSEvent( ETBSEvent event );
	virtual bool IsPerformingAction() const;
	// implement CUnit	
	virtual bool IsUnitVisible( const CUnit *pUnit ) const;
	virtual bool IsUnitAudible( const CUnit *pUnit ) const;
	virtual void GetVisible( vector<CPtr<CUnit> > *pTarget ) const;
	virtual bool IsStrafing() const { return CDumbUnitServer::IsStrafing(); }
	virtual bool IsCarryingCorpse() const { return animator.IsCarryingCorpse(); }
	virtual CUnit* GetCorpseCarrier() const { return animator.GetCorpseCarrier(); }
	virtual const vector<CVec3>* GetCorpseHLs() const { return CDumbUnitServer::GetCorpseHLs(); }   // retail CUnit vtbl+0x94 @0x3c68e0
	// retail CUnitServer::CannotFreelyChangePoses @0x37d550: the unit may not switch pose freely --
	// carrying a corpse, wearing a live PK, or a script pose-lock (luaUnitLockPose). FindPath passes
	// this as bMoveOnly so a locked/loaded unit keeps its current pose along the path.
	bool CannotFreelyChangePoses() { return IsCarryingCorpse() || IsWearingPK() || bForceNoChangePose; }
	void SetForceNoChangePose( bool b ) { bForceNoChangePose = b; }   // luaUnitLockPose
	virtual NDb::CModel* GetModel() const { return GetUnitModel(); }
	virtual void GetInfo( NRPG::SUnitInfo *pInfo ) const;
	virtual IPlayer* GetPlayer() const;
	virtual NAI::CPath* GetCurrentPath();
	virtual IPathViewer* CreatePathViewer();
	virtual NRPG::IUnitMissionInfo* GetRPG() const;
	virtual const NAI::SUnitPosition& GetPosition() const { return GetUnitPosition(); }
	virtual void GetRealPosition( CVec3 *pRes ) { GetRealUnitPosition( pRes ); }
	virtual void AddVisitableChildren( vector<IVisObj*> *pRes ) { AddMiscObjects( pRes ); }
	virtual bool GetCurrentCommandName( string *pName ) const;
	virtual CVec3 GetAttackOrigin() const;
	virtual CVec3 GetAttackOrigin( const NAI::SUnitPosition &from ) const;
	virtual float GetMinClearDistance() const;
	virtual const CObjectBase* GetAttackIgnore() const;
	virtual EUnitCommandResult CanDo( CCmd *p, int *pnStartAP = 0, int *pnFullAP = 0 );
	virtual bool HasEnoughAP();
	virtual EState GetState();
	virtual NDb::CComplexHead* GetDBHead();
	virtual SRandomSeed GetHeadSeed();
	virtual NLSHead::CHeadInfo* GetHeadInfo();
	virtual bool IsCapPresent();
	virtual CObjectBase* GetAIMapHull() { return GetAIMapUnitHull(); }

	void Segment();
	void UpdateVisible( SInterruptInfo *pRes );
	bool HasCommand() const { return IsValid( pCurrentCmd ); } //pExec->IsValid(); }// && commandsQueue.empty(); }
	// The path-conflict remover of this unit's current executor (pExec -> IExecMove), or null.
	// Used by CPathConflictsRemover::CheckLockerState to walk the who-locks-whom chain.
	CPathConflictsRemover* GetPathConflictsRemover();
	bool CanHearSound( const CVec3 &ptFrom, NDb::CAISound *pSound, int nSoundType, CUnitServer *pWho );
	void CallTimeLabel() { bCallTimeLabel = true; }
	void RunCriticalExecutor( CCommandExecute *p );
	void PostponeCritical( NDb::ECritical critical ) { criticals.push_front( critical ); }
	void ProcessCriticalImmediately( NDb::ECritical eCA );
	void SetState( CUnitState *_pState );
	bool WasInterrupted( CUnitServer *pWho ) const { return find( wasInterruptedList.begin(), wasInterruptedList.end(), pWho ) != wasInterruptedList.end(); }
	void MarkInterrupted( CUnitServer *pWho ) { ASSERT( !WasInterrupted( pWho ) ); wasInterruptedList.push_back( pWho ); }
	void UpdateCriticalsState();
	void DynamicallyLockWay( CPtr<NAI::CPath> pPath );
	void HearUnit( CUnitServer *pSource );
	virtual bool CanSnipe() const;
	virtual void CancelSnipe();
	virtual bool IsSniping() const;
	virtual void CollectSnipeAP( int nExtraAP );
	CUnitStateSniping *GetSnipingState(); // the docked snipe state (0 if not sniping) -- for the AI snipe actions

	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	virtual int GetCarefulShotExtraAP();
	virtual CDumbUnitServer *GetCorpse();
	// retail @0x3c0370: blast-wave ragdoll impulse for an already-downed body (dead or unconscious,
	// not an empty/worn PK, not being carried) -- normalizes the ray dir and launches the clipless
	// animator Die (bPlayDeath=false). Called by CVoxelExpl::ApplyWaveDamage on the FIRST wave.
	void AddImpulse( const CRay &rImpulse );
	bool HasLostFromSightAliveUnits();
	bool GetBarrelDir( CRay *pRay );
	virtual bool IsCheatEnabled( int nCheat );
	virtual bool CanStrafe() { return !pWearingPK; };

	virtual NDb::CPanzerklein *GetWearingDBPK();
	void FlipPanzerklein( CUnitServer *pPK, bool bUnloadWeapons = true );
	CUnitServer *GetWearingPK() { return pWearingPK; }
	void OnUnitDied( CUnitServer *pUS );
	void CheckStability();
	NDb::EDiplomacyState GetDiplomacyState( CUnitServer *pTarget ) const;
	void SetPlayer( CPlayer *_pPlayer );
	virtual bool CanTalk() const { return bCanTalk; }
	int GetDialog() const { return nDialog; }
	void SetDialog( const string &szDialogCode );
	void SetCanTalk( bool _bCanTalk ) { bCanTalk = _bCanTalk; }
	void FallFromHigh( float fHeightDiff );
	bool IsAIUnit() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BeginDeactivatingItem( CUnitServer *pUS, NDb :: EItemSubType subType );
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif