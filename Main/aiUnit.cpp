#include "StdAfx.h"

#include "aiLog.h"
#include "aiMap.h"
#include "aiState.h"
#include "aiPlayer.h"
#include "aiWeapon.h"
#include "aiPosition.h"
#include "aiInventory.h"
#include "aiReaction.h"
#include "aiUnitState.h"
#include "aiThreatTracker.h"   // NAI::CAIEventTracker -- per-unit world-event subscriber
#include "aiScaleConverter.h"
#include "aiControl.h"
#include "aiTaskCommander.h"

#include "RPGGame.h"
#include "RPGItem.h"
#include "RPGToHit.h"
#include "RPGItemSet.h"
#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"
#include "RPGUnitMission.h"
#include "RPGAttackMech.h"
#include "RPGUnit.h"
#include "rpgCheatConstants.h"

#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataDifficulty.h"   // NDb::CDBDifficulty::nHideProbability (ctor hide-prob seed)

#include "RPGGlobal.h"                     // NRPG::CGlobalGame::pDifficulty (ctor hide-prob seed)
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitAttack.h"

#include "aiLogic.h"          // IAILogic (phase-7 supersede: was aiCompoundAction.h CAILogic)

#include "aiUnit.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit: public IAIUnit
{
	OBJECT_BASIC_METHODS( CAIUnit );
	ZDATA
	SPosition ptPosition;
	int nAP, nMaxAP, nHP, nMaxHP;
	CPtr<NWorld::CUnitServer> pUnitServer;
	CPtr<NWorld::CCannon> pCannon;
	SPosition LastSeenEnemyPosition;
	CObj<CAIInventory> pInventory;
	SPosition ptPrevPosition;
	int nTurnStartHP;
	CPtr<IAIUnit> pLastSeenEnemy;
	int nHurtHP; // ����������� ���������� ���� �� ������ ���
	vector< CObj<IAIControl> > controls;
	bool bUnderAIControl;
	int nAdditionalExpediency;
	int nMaxToHit;
	CObj<IAILogic> pLogic;
	IAIState *pAIState;   // weak back-ref to the tactical AI state, set when the unit joins it (transient - not serialized)
	CObj<CAIReaction> pReaction;   // the unit's reflex layer; installed by the tactical commander (transient - not serialized)
	SAIUnitState state;            // per-unit threat tracker (release: by-value member; transient here)
	CObj<CAIEventTracker> pEventTracker;   // release: CAIEventTracker base subobject @CAIUnit+0x8 -- a MEMBER here (dev
	                               // CObjectBase is non-virtual so it can't be a 2nd base). Subscribes the 11 NWorld AI
	                               // event handlers; transient (lazy-built on first tick, RAII-unsubscribes on release).
	int nHideProbability;          // release CAIUnit+0x100: AI hide-roll %, seeded from the difficulty DB in the
	                               // ctor (read by the route-AI check-position / check-for-enemy hide roll). NOT
	                               // added to operator&: the dev CAIUnit save format is the predecessor's (tags
	                               // 2-19, divergent from the release's) -- reconciling CAIUnit serialization is a
	                               // separate Tier-B task; the value is re-seeded from difficulty on construction.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptPosition); f.Add(3,&nAP); f.Add(4,&nMaxAP); f.Add(5,&nHP); f.Add(6,&nMaxHP); f.Add(7,&pUnitServer); f.Add(8,&pCannon); f.Add(9,&LastSeenEnemyPosition); f.Add(10,&pInventory); f.Add(11,&ptPrevPosition); f.Add(12,&nTurnStartHP); f.Add(13,&pLastSeenEnemy); f.Add(14,&nHurtHP); f.Add(15,&controls); f.Add(16,&bUnderAIControl); f.Add(17,&nAdditionalExpediency); f.Add(18,&nMaxToHit); f.Add(19,&pLogic); return 0; }
	//
	void GetUnitSkillValues();
public:
	CAIUnit(): nHideProbability( 0 ) {}   // release default ctor zeroes the scalar tail (CAIUnit+0x100)
	CAIUnit( NWorld::CUnitServer *_pUnitServer, bool _bUnderAIControl );
	// IAIUnit
	virtual NWorld::CUnitServer *GetUnitServer() const { return pUnitServer; }
	virtual NRPG::IUnitMission* GetUnitMission() const { return pUnitServer->GetUnitRPG(); }
	virtual NRPG::CUnit* GetRPGUnit() const { return pUnitServer->GetUnitRPG()->GetRPGUnit(); }
	virtual SPosition GetPosition() { return ptPosition; } 
	virtual SUnitPosition GetUnitPosition();
	virtual void SetPosition( SPosition _ptPrevPosition );
	virtual void SetPosition( SPathPlace _ptPrevPosition );
	virtual SPosition GetPrevPosition();
	virtual void SavePrevPosition();
	virtual void GetHP( int *_nHP, int *_nMaxHP )
	{ *_nHP = nHP; *_nMaxHP = nMaxHP; }
	virtual int GetHP() { return nHP; }
	virtual void SetHP( int _nHP, int _nMaxHP );
	virtual void GetAP( int *_nAP, int *_nMaxAP)
	{ *_nAP = nAP; *_nMaxAP = nMaxAP; }
	virtual int GetAP() { return nAP; }
	virtual void SetAP( int _nAP, int _nMaxAP );
	virtual void SpendHP( int _nHP );
	virtual void SpendAP( int _nAP );
	virtual void Synchronize();
	virtual bool IsDead() { return nHP <= 0; }
	virtual int GetRemainAP() { return Max( 0, nMaxAP - nAP ); }
	virtual int GetToHit( IAIUnit *pTarget, const NAI::SUnitPosition &pos, NAI::EHitLocation hl = NAI::HL_ANY );
	virtual void SetPose( int pose );
	virtual bool IsPerformingAction() { return pUnitServer->IsPerformingAction(); }
	virtual bool IsUsingCannon() { return ( pCannon != 0 ); }
	virtual void SetCannon( NWorld::CCannon *_pCannon ) { pCannon = _pCannon; }
	virtual NWorld::CCannon *GetCannon() { return pCannon; }
	virtual bool IsMovedThisTurn() { return GetUnitServer()->GetUnitRPG()->GetMoveInLastTurn(); }
	virtual void OnTurnStarted() {}
	virtual void GetLastSeenEnemy( SPosition *Position, IAIUnit **ppAIUnit );
	virtual void SetLastSeenEnemy( IAIUnit *pAIUnit );
	virtual CAIInventory* GetAIInventory() { return pInventory; }
	virtual int GetTurnStartHP() { return nTurnStartHP; }
	virtual int GetHurtHP() { return nHurtHP; }
	virtual void SetHurtHP( int _nHurtHP ) { nHurtHP = _nHurtHP; }
	virtual int GetCoverForFixedUnit( const NAI::SUnitPosition &pos,
		NWorld::CUnitServer *pTarget, NRPG::CWeaponItem *pWeaponItem, NAI::EHitLocation HitLocation );
	virtual bool HasInactivePose();
	virtual void AssignControl( IAIControl *pAIControl );
	virtual void OnControlFinished();
	virtual void ActivateCurrentControl();
	virtual void DeactivateCurrentControl();
	virtual bool IsUnderAIControl() { return bUnderAIControl; }
	virtual int GetAdditionalExpediency() { return nAdditionalExpediency; }
	virtual void SetAdditionalExpediency( int nExpediency ) { nAdditionalExpediency = nExpediency; }
	virtual int GetMaxAP() { return nMaxAP; }
	virtual int GetMaxHP()  { return nMaxHP; }
	virtual void SetMaxToHit( int _nMaxToHit ) { nMaxToHit = _nMaxToHit; }
	virtual int GetMaxToHit() { return nMaxToHit; }
	virtual int GetHideProbability() { return nHideProbability; }   // @0xaef00 (CAIUnit+0x100)
	virtual void SetHideProbability( int n ) { nHideProbability = n; }   // @0xaef10 (the lua UnitSetHideProbability setter deferred)
	virtual bool HasVisibleEnemies();
	virtual void DebugOutput();
	virtual void OnDied();
	virtual CTask* GetRoute() const;
	virtual IAILogic* GetLogic() const { return pLogic; }
	virtual void SetLogic( IAILogic *_pLogic );
	// the unit's tactical AI state (release IAIUnit vtbl 0x78); set by the tactical commander when the
	// unit joins the state. The substrate's actions/place-sources read the enemy + enemy-groups through it.
	virtual IAIState* GetAIState() { return pAIState; }
	virtual void SetAIState( IAIState *_pAIState ) { pAIState = _pAIState; }
	virtual CAIReaction* GetReaction() const { return pReaction; }
	virtual void SetReaction( CAIReaction *_pReaction ) { pReaction = _pReaction; }
	virtual SAIUnitState* GetAIUnitState() { return &state; }
	// release CAIUnit::OnAISegment @0xad2c0: per-segment tick. Lazily build the event tracker (covers BOTH the fresh
	// ctor path and the deserialization path, where the (CUnitServer*,bool) ctor never runs and the tracker is not
	// serialized), then ProcessAISegment (the corpse scan, currently inert -- range query stubbed). Constructing the
	// tracker subscribes the unit to the global event bus; nothing throws during this tick, so no dispatch reentrancy.
	virtual void OnAISegment()
	{
		if ( !IsValid( pEventTracker ) && IsValid( pUnitServer ) )
			pEventTracker = new CAIEventTracker( pUnitServer );
		if ( IsValid( pEventTracker ) )
			pEventTracker->ProcessAISegment();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIUnit::CAIUnit( NWorld::CUnitServer *_pUnitServer, bool _bUnderAIControl ) :
	pUnitServer(_pUnitServer), pCannon(0), pLastSeenEnemy( 0 ),
	bUnderAIControl( _bUnderAIControl ), pAIState( 0 )
{ 
	GetUnitSkillValues();
	ptPosition = pUnitServer->GetPosition().pos;
	state.SetUnit( this );
	pInventory = CreateAIInventory( this );
	pUnitServer->GetRPG()->PrintLog( false );
	// release CAIUnit ctor tail (@0xae420): seed the AI hide-roll chance from the current difficulty record
	// (world->GetGlobalGame()->pDifficulty->nHideProbability == CDbDifficulty "HideProbability" DB field).
	nHideProbability = 0;
	if ( IsValid( pUnitServer ) )
		nHideProbability = pUnitServer->GetWorld()->GetGlobalGame()->pDifficulty->nHideProbability;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetLogic( IAILogic *_pLogic )
{
	pLogic = _pLogic;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::GetUnitSkillValues()
{
	CPtr<NRPG::CUnit> pRPGUnit = GetUnitServer()->GetUnitRPG()->GetRPGUnit();
	SetAP( pRPGUnit->Skills(NDb::ST_AP), pRPGUnit->Skills(NDb::ST_AP).GetCurrentMaxValue() );
	SetHP( pRPGUnit->Skills(NDb::ST_VP), pRPGUnit->Skills(NDb::ST_VP).GetCurrentMaxValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::DebugOutput()
{
	OutputDebugString( "[AI UNIT] [\n" );
	for ( vector< CObj<IAIControl> >::reverse_iterator i = controls.rbegin(); i != controls.rend(); ++i )
		(*i)->DebugOutput();
	OutputDebugString( "[AI UNIT] ]\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIUnit::HasVisibleEnemies()
{
	const list< CPtr<NWorld::CUnitServer> > &units = GetUnitServer()->GetTBSVisible();
	for ( list< CPtr<NWorld::CUnitServer> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		bool bDiplomacyEnemy = pUnitServer->GetWorld()->GetDiplomacyState( pUnitServer, (*i)->GetPlayer() ) == NDb::DS_ENEMY;
		if ( bDiplomacyEnemy && (*i)->CanFight() && (*i)->GetPlayer() != GetUnitServer()->GetPlayer() )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::AssignControl( IAIControl *pAIControl )
{
	CPtr<IAIControl> pHolder = pAIControl;
	//
	ASSERT( IsValid( pAIControl ) );
	ASSERT( IsUnderAIControl() );
	if ( !IsValid( pAIControl ) || !IsUnderAIControl() )
		return;
	//
	EAIManager manager = pAIControl->GetManager();
	EAIManager currentManager = AIM_AI;
	if ( !controls.empty() )
		currentManager = controls.back()->GetManager();
	//
	if ( currentManager > manager && pAIControl->GetType() != AI_CONTROL_UNINTERRUPTABLE )
		return;
	//
	if ( GetUnitServer()->IsCheatEnabled( NRPG::CHEAT_NOAI ) && manager < AIM_SCRIPT )
		return;
	//
	if ( !controls.empty() )
	{
		CPtr<IAIControl> pAICurrentControl = controls.back();
		EAIControlType type = pAICurrentControl->GetType();
		if ( type == AI_CONTROL_UNINTERRUPTABLE && manager > currentManager )
			type = AI_CONTROL_ERASABLE;
		//
		if ( type == AI_CONTROL_UNINTERRUPTABLE )
		{
			return;
		}
		else 
		{
			if ( pAICurrentControl->IsActive() )
				pAICurrentControl->DeActivate();
			if ( type == AI_CONTROL_ERASABLE )
				controls.pop_back();
		}
	}
	//
	GetUnitServer()->Do( new NWorld::CCmdCancel( GetUnitServer() ) );
	controls.push_back( pAIControl );
	pAIControl->Activate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::OnControlFinished()
{
	if ( !IsUnderAIControl() )
		return;
	//
	EAIManager currentManager = AIM_AI;
	if ( !controls.empty() )
	{
		CPtr<IAIControl> pControl = controls.back();
		currentManager = pControl->GetManager();
		if ( pControl->IsActive() )
			pControl->DeActivate();
		controls.pop_back();
	}
	if ( GetUnitServer()->IsCheatEnabled( NRPG::CHEAT_NOAI ) )
		currentManager = AIM_SCRIPT;
	//
	if ( !controls.empty() )
	{
		CPtr<IAIControl> pControl = controls.back();
		EAIManager manager = pControl->GetManager();
		if ( manager >= currentManager )
			controls.back()->Activate();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::ActivateCurrentControl()
{
	if ( !controls.empty() && !controls.back()->IsActive() )
		controls.back()->Activate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTask* CAIUnit::GetRoute() const
{
	int nSize = controls.size();
	for ( int i = nSize - 1; i >= 0; --i )
	{
		CPtr<CTask> pTask = GetTaskFromControl( controls[ i ] );
		if ( IsValid( pTask ) )
			return pTask;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::DeactivateCurrentControl()
{
	if ( !controls.empty() && controls.back()->IsActive() )
	{
		controls.back()->DeActivate();
		GetUnitServer()->Do( new NWorld::CCmdCancel( GetUnitServer() ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIUnit::GetCoverForFixedUnit( const NAI::SUnitPosition &pos, 
	NWorld::CUnitServer *pTarget, NRPG::CWeaponItem *pWeaponItem, NAI::EHitLocation HitLocation )
{
	ASSERT( IsValid( pTarget ) );
	if ( !IsValid( pTarget ) )
		return 0;
	//
	vector<NRPG::CAttackPortion> atts;
	if ( IsValid( pWeaponItem ) )
		pWeaponItem->CreateNewAttackPortion( &atts, false );
	if ( atts.empty() )
		atts.push_back( NRPG::CAttackPortion( 1, 0, 0, 0, 0 ) );
	//
	ASSERT( !atts.empty() );
	if ( !atts.empty() )
	{
		CVec3 ptEye = pos.GetEyePosition();
		CPtr<NRPG::IGame> pGame = GetUnitServer()->GetWorld()->GetGame();
		return pGame->GetCoverForAIUnit( ptEye, GetUnitServer(), pTarget, atts.front(), HitLocation );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::GetLastSeenEnemy( SPosition *Position, IAIUnit **ppAIUnit )
{
	*Position = LastSeenEnemyPosition;
	*ppAIUnit = pLastSeenEnemy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetLastSeenEnemy( IAIUnit *pAIUnit )
{
	LastSeenEnemyPosition = pAIUnit->GetUnitServer()->GetPosition().pos;
	pLastSeenEnemy = pAIUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIUnit::HasInactivePose()
{
	return ptPosition.p.GetPose() == NAI::CM_INACTIVE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitPosition CAIUnit::GetUnitPosition()
{
	NAI::SUnitPosition res;
	res.pos = ptPosition;
	res.bRun = false; // CRAP
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetPosition( SPosition _ptPosition ) 
{ 
	ptPosition = _ptPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetPosition( SPathPlace _pPosition )
{
	SPosition s;
	s.p = _pPosition;
	s.SetNetwork( GetUnitServer()->GetWorld()->GetPathNetwork() );
	SetPosition( s );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SPosition CAIUnit::GetPrevPosition()
{
	return ptPrevPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SavePrevPosition()
{
	ptPrevPosition = ptPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::Synchronize()
{
	GetUnitSkillValues();
	ptPosition = pUnitServer->GetPosition().pos; 
	pCannon = GetUnitServer()->animator.GetCannon();
	nTurnStartHP = GetHP();
	SetHurtHP( 0 );
	SetMaxToHit( 0 );
	nAdditionalExpediency = 0;
	pInventory = CreateAIInventory( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetHP( int _nHP, int _nMaxHP )
{ 
	nHP = _nHP; nMaxHP = _nMaxHP; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetAP( int _nAP, int _nMaxAP )
{ 
	nAP = _nAP; nMaxAP = _nMaxAP; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SpendHP( int _nHP )
{
	SetHP( Max( 0, nHP - _nHP ), nMaxHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SpendAP( int _nAP )
{
	SetAP( Max( 0, nAP - _nAP ), nMaxAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIUnit::GetToHit( IAIUnit *pTarget, const NAI::SUnitPosition &pos, NAI::EHitLocation hl )
{
	int nToHit = 100;
	CPtr<CAIFireArmsWeapon> pWeapon = pTarget->GetAIInventory()->GetCurrentFireArms();
	if ( IsValid( pWeapon ) )
	{
		nToHit = GetCoverForFixedUnit( GetUnitPosition(), pTarget->GetUnitServer(), pWeapon->GetItem(), hl );
		CPtr<NRPG::CAIUnitToHitCalcer> pCalcer = new NRPG::CAIUnitToHitCalcer( this, pos, pTarget, nToHit, hl, 0, pWeapon->GetItem(), 0 );
		return pCalcer->GetToHit();
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::SetPose( int pose )
{
	ptPosition.p.SetPose( pose );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::OnDied()
{
	for ( vector< CObj<IAIControl> >::iterator i = controls.begin(); i != controls.end(); ++i )
		(*i)->OnPerformerDied();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *CreateAIUnit( NWorld::CUnitServer *pUnitServer, bool bUnderAIControl ) 
{
	ASSERT( IsValid( pUnitServer ) );
	//
	return new CAIUnit( pUnitServer, bUnderAIControl );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822100, CAIUnit );