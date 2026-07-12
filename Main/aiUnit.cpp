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
#include "aiCommander.h"      // NAI::CAICommander (control-stack back-reference)

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
	int nHurtHP; // damage we dealt during this turn
	vector< CObj<IAIControl> > controls;
	bool bUnderAIControl;
	int nAdditionalExpediency;
	int nMaxToHit;
	CObj<IAILogic> pLogic;
	// retail CAIUnit routes[2] (@0xad340/@0xad3a0/@0xadb90): the ROUTE slots, separate from the
	// reaction-installed combat logic above. routeNormal = routes[0] (deploy/script patrols; PAUSED under
	// a combat logic @0xaddc0, RESUMED when it clears @0xadcd0, suspended across sequences @0xadec0/@0xad3f0);
	// routeSequence = routes[1] (cutscene-scripted actions; the ONLY behaviour the unit runs while the
	// world is in sequence mode; ends with its sequence). The dev predecessor folded all three into the
	// single pLogic slot, so a reaction firing at sequence end DESTROYED the unit's scripted/deploy route
	// (the GFirst body-carrier froze; patrols never resumed after combat).
	CObj<IAILogic> routeNormal;
	CObj<IAILogic> routeSequence;
	SAIState *pAIState;   // weak back-ref to the tactical AI state, set when the unit joins it (transient - not serialized)
	CObj<CAIReaction> pReaction;   // the unit's reflex layer; installed by the tactical commander (transient - not serialized)
	SAIUnitState state;            // per-unit threat tracker (release: by-value member; transient here)
	CObj<CAIEventTracker> pEventTracker;   // release: CAIEventTracker base subobject @CAIUnit+0x8 -- a MEMBER here (dev
	                               // CObjectBase is non-virtual so it can't be a 2nd base). Subscribes the 11 NWorld AI
	                               // event handlers; transient (lazy-built on first tick, RAII-unsubscribes on release).
	int nNonFreezeCounter;         // release CAIUnit: per-unit runaway-AI guard tally (IAIUnit vtbl 0x90/0x94).
	                               // Bumped on every per-unit logic/reaction churn (SetReaction @0xad7d0,
	                               // SetLogic path @0xadb20); the commander sums it (x200) in IsPossibleFreeze
	                               // and zeroes it each real-time segment (ClearNonFreezeCounters). Transient
	                               // (per-turn, re-zeroed each RT segment) -- NOT added to operator& (same
	                               // treatment as nHideProbability; the dev CAIUnit save format is the
	                               // predecessor's tags 2-19).
	int nHideProbability;          // release CAIUnit+0x100: AI hide-roll %, seeded from the difficulty DB in the
	                               // ctor (read by the route-AI check-position / check-for-enemy hide roll). NOT
	                               // added to operator&: the dev CAIUnit save format is the predecessor's (tags
	                               // 2-19, divergent from the release's) -- reconciling CAIUnit serialization is a
	                               // separate Tier-B task; the value is re-seeded from difficulty on construction.
	unordered_map<int,int> disabledShootModes;   // release CAIUnit+0xec hash_map<int,int>: per-mode AI shoot
	                               // disable (Defence dance @0x3b160 writes, GetBestFireArms @0x560e0 reads).
	                               // Retail SAVES it (operator& @0xafb70 chunk 11 DoHashMap<int,int>) --
	                               // appended here as tag 22 (absent on old saves -> stays empty).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptPosition); f.Add(3,&nAP); f.Add(4,&nMaxAP); f.Add(5,&nHP); f.Add(6,&nMaxHP); f.Add(7,&pUnitServer); f.Add(8,&pCannon); f.Add(9,&LastSeenEnemyPosition); f.Add(10,&pInventory); f.Add(11,&ptPrevPosition); f.Add(12,&nTurnStartHP); f.Add(13,&pLastSeenEnemy); f.Add(14,&nHurtHP); f.Add(15,&controls); f.Add(16,&bUnderAIControl); f.Add(17,&nAdditionalExpediency); f.Add(18,&nMaxToHit); f.Add(19,&pLogic); f.Add(20,&routeNormal); f.Add(21,&routeSequence); f.Add(22,&disabledShootModes); return 0; }
	//
	void GetUnitSkillValues();
public:
	CAIUnit(): nHideProbability( 0 ), nNonFreezeCounter( 0 ) {}   // release default ctor zeroes the scalar tail (CAIUnit+0x100)
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
	virtual void OnSequenceStarted();    // retail @0xadec0
	virtual void OnSequenceFinished();   // retail @0xad3f0
	// retail CAIUnit::IsAIUnit @0xad4c0 (IAIUnit vtbl 0x1c) is DYNAMIC: pUS->IsAIUnit() == NAI::IsAIPlayer(
	// the unit's CURRENT player). The frozen ctor flag is wrong under the retail wrapper model: the OWNING
	// commander creates every shared CAIUnit (so the flag would read true even for the human's units), and
	// a ChangeUnitPlayer hand-over must re-classify the unit. bUnderAIControl stays serialized for save
	// compatibility but is no longer consulted.
	virtual bool IsUnderAIControl() { return IsValid( pUnitServer ) && pUnitServer->IsAIUnit(); }
	virtual int GetAdditionalExpediency() { return nAdditionalExpediency; }
	virtual void SetAdditionalExpediency( int nExpediency ) { nAdditionalExpediency = nExpediency; }
	virtual int GetMaxAP() { return nMaxAP; }
	virtual int GetMaxHP()  { return nMaxHP; }
	virtual void SetMaxToHit( int _nMaxToHit ) { nMaxToHit = _nMaxToHit; }
	virtual int GetMaxToHit() { return nMaxToHit; }
	virtual int GetHideProbability() { return nHideProbability; }   // @0xaef00 (CAIUnit+0x100)
	virtual void SetHideProbability( int n ) { nHideProbability = n; }   // @0xaef10 (the lua UnitSetHideProbability setter deferred)
	virtual int GetNonFreezeCounter() { return nNonFreezeCounter; }   // @0xaef20
	virtual void ClearNonFreezeCounter() { nNonFreezeCounter = 0; }   // @0xaef30
	virtual bool IsShootModeDisabled( NDb::EShootMode eMode ) const { return disabledShootModes.find( (int)eMode ) != disabledShootModes.end(); }   // @0xad590
	virtual void DisableShootMode( NDb::EShootMode eMode ) { disabledShootModes[ (int)eMode ] = 1; }   // @0xad740
	virtual void EnableShootMode( NDb::EShootMode eMode ) { disabledShootModes.erase( (int)eMode ); }   // @0xad770
	virtual bool HasVisibleEnemies();
	virtual void DebugOutput();
	virtual void OnDied();
	virtual CTask* GetRoute() const;
	// retail CAIUnit::IsSequence @0xad230: the per-unit sequence predicate (world sequence mode).
	bool IsWorldSequence() const
	{
		return IsValid( pUnitServer ) && pUnitServer->GetWorld()->IsSequence();
	}
	// retail GetLogic @0xad340: the ACTIVE behaviour. In a sequence ONLY the sequence route runs (a
	// cutscene actor is untouchable by the combat layer); otherwise the reaction-installed combat logic
	// takes priority over the normal route.
	virtual IAILogic* GetLogic() const
	{
		if ( IsWorldSequence() )
			return routeSequence;
		if ( IsValid( pLogic ) )
			return pLogic;
		return routeNormal;
	}
	// retail GetRoute @0xad3a0 (vtbl 0x54): the mode-selected route slot.
	virtual IAILogic* GetRouteLogic() const
	{
		return IsWorldSequence() ? routeSequence.GetPtr() : routeNormal.GetPtr();
	}
	// retail SetRoute @0xadb90 (vtbl 0x5c): install a ROUTE into the mode-selected slot. Only an AI-driven
	// unit may take a normal route (a non-AI unit only ever gets a SEQUENCE route); the unit's running
	// command is cancelled and any combat logic is dropped outright (StopThinking + clear, churn-bumped).
	virtual void SetRouteLogic( IAILogic *_pRoute )
	{
		if ( !IsValid( pUnitServer ) )
			return;
		if ( !IsUnderAIControl() && !IsWorldSequence() )
			return;
		CancelCommand();
		if ( pLogic.GetPtr() != 0 )
			++nNonFreezeCounter;
		if ( IsValid( pLogic ) )
			pLogic->StopThinking();
		pLogic = 0;
		if ( IsWorldSequence() )
			routeSequence = _pRoute;
		else
			routeNormal = _pRoute;
	}
	virtual void SetLogic( IAILogic *_pLogic );
	void SetCurrentLogicInner( IAILogic *_pLogic );   // retail @0xadb20
	void CancelCurrentLogic();                        // retail @0xadcd0
	// the unit's tactical AI state (release IAIUnit vtbl 0x78); set by the tactical commander when the
	// unit joins the state. The substrate's actions/place-sources read the enemy + enemy-groups through it.
	virtual SAIState* GetAIState() { return pAIState; }
	virtual void SetAIState( SAIState *_pAIState ) { pAIState = _pAIState; }
	virtual CAIReaction* GetReaction() const { return pReaction; }
	// release CAIUnit::SetReaction @0xad7d0: SetLogic(0) FIRST (drop the old reaction's combat logic --
	// with the route slots this is now safe: CancelCurrentLogic resumes the suspended route instead of
	// wiping the deploy patrol, which is why the dev predecessor had to omit it), then set pReaction,
	// MARK THE THREAT STATE MODIFIED, bump the runaway-AI guard. The state.Modified() is load-bearing:
	// the commander's per-segment reaction pump (CheckForUpdates -> GetReactionForUpdate, gated on
	// state.IsModified()) is what hands the NEWLY-installed reaction to the update tracker so it gets
	// Update()'d and installs its logic.
	virtual void SetReaction( CAIReaction *_pReaction )
	{
		SetLogic( 0 );
		pReaction = _pReaction;
		state.selfModified.SetModified();
		++nNonFreezeCounter;
	}
	virtual SAIUnitState* GetAIUnitState() { return &state; }
	// AI-convergence Stage 2 reaction pump (see aiUnit.h). GetReactionForUpdate @0xad260: only hand the
	// reaction to the commander when the threat state changed, CLEARING the dirty flag (retail state.Reset()
	// -- here ClearModified, NOT the full list wipe: the poll-based lists must survive for the reaction that
	// fires this same segment to read pEnemy).
	virtual CAIReaction* GetReactionForUpdate()
	{
		if ( !IsValid( pUnitServer ) || !pUnitServer->CanFight() || !IsUnderAIControl() )
			return 0;
		if ( !state.IsModified() )
			return 0;
		state.ClearModified();
		return IsValid( pReaction ) ? pReaction.GetPtr() : 0;
	}
	// OnLogicFinished @0xad1f0 (vtbl 0x60): a finished ROUTE clears its slot; a finished combat logic is
	// dropped via SetLogic(0) (-> CancelCurrentLogic -> the suspended route RESUMES). Either way the state
	// is marked modified so the reaction re-decides.
	virtual void OnLogicFinished( IAILogic *pL )
	{
		if ( GetRouteLogic() == pL )
		{
			SetRouteLogic( 0 );
			state.selfModified.SetModified();
			return;
		}
		if ( pLogic.GetPtr() == pL )
			SetLogic( 0 );
		state.selfModified.SetModified();
	}
	// CancelCommand @0xad820 (vtbl 0x28): cancel the unit's current world command (the reaction is about to
	// re-decide). Dev analog of CAIUnit::DeactivateCurrentControl's CCmdCancel; the exit-cannon tail is elided.
	virtual void CancelCommand()
	{
		if ( IsValid( pUnitServer ) && pUnitServer->CanFight() )
			pUnitServer->Do( new NWorld::CCmdCancel( pUnitServer ) );
	}
	// release CAIUnit::OnAISegment @0xad2c0: per-segment tick. Lazily build the event tracker (covers BOTH the fresh
	// ctor path and the deserialization path, where the (CUnitServer*,bool) ctor never runs and the tracker is not
	// serialized), then ProcessAISegment (the corpse scan @0xab7a0, now LIVE -- IsCorpseVisible probe over the
	// body's corpseHLpos points, radius GetMaxUnitSightDistance). Constructing the
	// tracker subscribes the unit to the global event bus; nothing throws during this tick, so no dispatch reentrancy.
	virtual void OnAISegment()
	{
		// retail CAIUnit::OnAISegment @0xad2c0 opens with `if (IsSequence()) return;` (@0xad230): no
		// per-segment threat-tracker/state processing while a scripted sequence runs.
		if ( IsValid( pUnitServer ) && pUnitServer->GetWorld()->IsSequence() )
			return;
		if ( !IsValid( pEventTracker ) && IsValid( pUnitServer ) )
			pEventTracker = new CAIEventTracker( pUnitServer );
		if ( IsValid( pEventTracker ) )
			pEventTracker->ProcessAISegment();
		// AI-convergence Stage 2: refresh the poll-based threat state each segment (release maintains it
		// event-driven; the dev sources it from the AI players via Populate). This is what the commander's
		// per-segment reaction pump reads -- Update() re-derives pEnemy and SETS the modified flag on a change,
		// which GetReactionForUpdate then consumes to enqueue this unit's reaction. Only own (AI-driven)
		// fightable units are ticked here (the commander gates OnAISegment on CanAIOperateThisUnit).
		if ( IsUnderAIControl() && IsValid( pUnitServer ) && pUnitServer->CanFight() && IsValid( pAIState ) )
		{
			// item 7 parity: retail maintains SAIUnitState EVENT-DRIVEN, not by a per-segment poll. The begin-turn
			// enemy re-seed Populate() now runs via CAIBeginTurnEvent (fired by CEventOnStartGame + the newly-thrown
			// CEventOnPassControl); the See/Lost/Hear/Bullet/Grenade/Die/Attack producers maintain it intra-turn.
			// Only Update() stays per-segment -- it re-derives pEnemy/pAlly and sets the modified flag the
			// commander's reaction pump (GetReactionForUpdate) consumes. (Poll Populate() removed for parity.)
			state.Update();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIUnit::CAIUnit( NWorld::CUnitServer *_pUnitServer, bool _bUnderAIControl ) :
	pUnitServer(_pUnitServer), pCannon(0), pLastSeenEnemy( 0 ),
	bUnderAIControl( _bUnderAIControl ), pAIState( 0 ), nNonFreezeCounter( 0 )
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
// retail SetCurrentLogicInner @0xadb20: bump the runaway-AI guard ONLY on an actual logic change (a
// no-op re-set does not count as churn), stop the outgoing logic's think job, swap.
void CAIUnit::SetCurrentLogicInner( IAILogic *_pLogic )
{
	if ( pLogic.GetPtr() != _pLogic )
		++nNonFreezeCounter;
	if ( IsValid( pLogic ) )
		pLogic->StopThinking();
	pLogic = _pLogic;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CancelCurrentLogic @0xadcd0: for an AI-driven unit, cancel the running world command; if a
// combat logic was installed, clear it and RESUME the route it had suspended (Pause/Resume nest -- the
// route's pause count returns to its pre-combat level).
void CAIUnit::CancelCurrentLogic()
{
	if ( !IsValid( pUnitServer ) || !IsUnderAIControl() )
		return;
	CancelCommand();
	if ( IsValid( pLogic ) )
	{
		SetCurrentLogicInner( 0 );
		IAILogic *pRoute = GetRouteLogic();
		if ( IsValid( pRoute ) && !pRoute->IsActive() )
			pRoute->Resume();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetLogic @0xaddc0: the combat-logic slot install. NO-OP while the world runs a scripted
// sequence (the AI may never touch a cutscene actor -- the GFirst body-carrier fix); gated to a live,
// fightable, AI-driven unit. Installing NULL == cancel + resume the suspended route; installing a real
// logic cancels the old one (resuming the route) then PAUSES the route under the new logic.
void CAIUnit::SetLogic( IAILogic *_pLogic )
{
	if ( IsWorldSequence() )
		return;
	if ( !IsValid( pUnitServer ) || !pUnitServer->CanFight() || !IsUnderAIControl() )
		return;
	CancelCurrentLogic();
	if ( IsValid( _pLogic ) )
	{
		IAILogic *pRoute = GetRouteLogic();
		if ( IsValid( pRoute ) )
			pRoute->Pause();
		SetCurrentLogicInner( _pLogic );
	}
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
	// retail CAIUnit::SetLogic @0xaddc0 opens with `if (IsSequence()) return;` (IsSequence @0xad230 =
	// world sequence mode): the AI may NEVER install its own (non-script) logic on a unit while a
	// scripted sequence runs -- only script routes (SetRoute @0xadb90) get through. The dev control-
	// stack analog: refuse any control below the AIM_SCRIPT manager while the unit is script-locked
	// (CHEAT_SCRIPTSEQUENCE is set on every unit for exactly the c_BeginSequence..EndSequence span,
	// the same lifetime as retail's sequence predicate) -- same shape as the existing CHEAT_NOAI gate.
	if ( ( GetUnitServer()->IsCheatEnabled( NRPG::CHEAT_NOAI ) ||
		   GetUnitServer()->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) ) && manager < AIM_SCRIPT )
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
	// CAITaskCommander removal: routes no longer install a CAITaskControl wrapping a CTask -- they install a
	// per-unit CAIRouteLogic (SetLogic). There is therefore no CTask to hand back, so GetRoute returns 0. The
	// only caller is the lua UnitGetRoute/RouteIsFinished pair (scriptUnit.cpp): UnitGetRoute pushes nil and
	// RouteIsFinished(nil) then reports the route "finished" (its CDynamicCast<CTask> fails). Retail's
	// CAIRouteLogic-based route-finished query is not reconstructed in this stage (runtime is deferred until
	// the AI layer is fully converged); the signature is kept so the lua binding + IAIUnit vtable stay intact.
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
// helper for OnSequenceStarted/Finished: erase every AIM_SCRIPT control from the stack -- the dev
// control-stack analog of the retail sequence-route slot (CAIUnit::routes[1], cleared @0xadec0 and
// deactivated @0xad3f0; a retail sequence route never outlives its sequence). Deactivates live ones and
// pops each AIM_SCRIPT control off the stack. Returns true when an ACTIVE control was ended (the caller
// then cancels the in-flight unit command, matching retail's CancelCommand). CAITaskCommander removal:
// there is no longer a per-player task queue to un-register a CTask from -- any per-unit route logic is
// cleared directly by SetLogic(0) in OnSequenceStarted/Finished (below).
static bool EraseScriptControls( vector< CObj<IAIControl> > *pControls )
{
	bool bEndedActive = false;
	for ( vector< CObj<IAIControl> >::iterator i = pControls->begin(); i != pControls->end(); )
	{
		IAIControl *pControl = *i;
		if ( IsValid( pControl ) && pControl->GetManager() == AIM_SCRIPT )
		{
			if ( pControl->IsActive() )
			{
				pControl->DeActivate();
				bEndedActive = true;
			}
			i = pControls->erase( i );
		}
		else
			++i;
	}
	return bEndedActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAIUnit::OnSequenceStarted @0xadec0: SetLogic(0) [drop the combat logic -- retail runs this
// BEFORE the world enters sequence mode (luac_BeginSequence @0x2f1890 notifies units, THEN calls
// CTBSWorld::StartSequence), and the dev Begin edge now matches that order, so the sequence-gated
// SetLogic works], then routes[0]->Pause() [the NORMAL route is suspended for the whole sequence]
// and routes[1] = 0 [clear any stale sequence route]. The dev control-stack handling is kept alongside:
// erase stale AIM_SCRIPT controls and suspend the current control (script controls assigned DURING the
// sequence stack on top through the CHEAT_SCRIPTSEQUENCE gate in AssignControl).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::OnSequenceStarted()
{
	SetLogic( 0 );                 // retail @0xadec0 (the Begin edge now runs pre-push, so this works)
	if ( IsValid( routeNormal ) )
		routeNormal->Pause();
	routeSequence = 0;
	bool bCancel = EraseScriptControls( &controls );
	if ( !controls.empty() && controls.back()->IsActive() )
	{
		controls.back()->DeActivate();
		bCancel = true;
	}
	if ( bCancel && GetUnitServer()->CanFight() )
		GetUnitServer()->Do( new NWorld::CCmdCancel( GetUnitServer() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CAIUnit::OnSequenceFinished @0xad3f0: SetLogic(0) [luaEndSequence @0x2f1a60 pops the ownerless
// top (EndOfTurn) BEFORE the notifies, so the virtual works], routes[1]->Pause() [the sequence route
// ENDS with its sequence: paused, never resumed -- the next OnSequenceStarted clears the slot],
// routes[0]->Resume() [the normal route RESUMES], state.Modified() [the reaction re-decides -- combat
// entry replaces/suspends the route through the normal SetLogic path]. The dev control-stack handling
// is kept alongside: erase the sequence's AIM_SCRIPT controls, re-activate the remaining control.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnit::OnSequenceFinished()
{
	SetLogic( 0 );
	if ( IsValid( routeSequence ) )
		routeSequence->Pause();
	if ( routeNormal.GetPtr() != 0 )
		routeNormal->Resume();
	if ( EraseScriptControls( &controls ) && GetUnitServer()->CanFight() )
		GetUnitServer()->Do( new NWorld::CCmdCancel( GetUnitServer() ) );
	if ( !controls.empty() && !controls.back()->IsActive() )
		controls.back()->Activate();
	state.selfModified.SetModified();   // retail tail: SAIUnitState::Modified()
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
		atts.push_back( NRPG::CAttackPortion( 1, 0, 0.0f, 0, 0, 0 ) );   // retail GetCover @0xae290 probe: fPushCoeff=0
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