#ifndef __wTurnBased_H_
#define __wTurnBased_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETBSEvent
{
	TBS_START_NEW_TURN,
	TBS_FINISH_OWN_TURN,
	TBS_START_REAL_TIME,
	TBS_ACTION_FINISH,
	TBS_CANCEL_ACTION,
	TBS_RECALC_COMMAND,
	// retail ETBSEvent (its value differs in the binary; transient runtime event, never serialized, so the
	// enumerator order here is internal). A global situation change / interrupt broadcasts THIS, not
	// TBS_CANCEL_ACTION: a unit caught MID-MOVE must be snapped to a clean grid cell and have its move
	// executor released (see CUnitServer::OnTBSEvent @0x3c2a90 / CTBSWorld::CancelAllAction @0x36f820).
	TBS_STOP_MOVE_AND_CANCEL_ACTION
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// when someone holds CObj on such object then action is in progress
class CActionCounter: public CObjectBase
{
	OBJECT_BASIC_METHODS(CActionCounter);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommand;
template <class TUnit, class TPlayer>
class CTBSUnit
{
protected:
	ZDATA
	CPtr<TPlayer> pPlayer;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPlayer); return 0; }
	TPlayer* GetTBSPlayer() const { return pPlayer; }
	virtual void Do( CCommand* ) = 0;
	virtual bool IsDead() const = 0;
	virtual bool CanFight() const = 0;
	virtual void OnTBSEvent( ETBSEvent event ) {}
	virtual bool IsPerformingAction() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUnit, class TCommander>
class CPlayerBase
{
protected:
	typedef vector< CMObj<TUnit> > TPlayerUnitSet;
	ZDATA
	int nPlayerID;
	bool bTurnDone;
	ZSKIP
	TPlayerUnitSet units;
	CObj<TCommander> pCommander;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPlayerID); f.Add(3,&bTurnDone); f.Add(5,&units); f.Add(6,&pCommander); return 0; }

	CPlayerBase(): nPlayerID( 0 ), bTurnDone( true ) {}

	int GetPlayerID() const { return nPlayerID; }
	void SetPlayerID( int nID ) { nPlayerID = nID; }
	////
	TCommander *GetCommander() const { ASSERT( pCommander ); return pCommander; }	
	void SetCommander( TCommander *_pCommander ) { pCommander = _pCommander; }
	////
	virtual const TPlayerUnitSet& GetPlayerUnits() const { return units; }
	void AddUnit( TUnit *pUnit ) { units.push_back( pUnit ); }
	void RemoveUnit( TUnit *pUnit )
	{
		TPlayerUnitSet::iterator iTemp = find( units.begin(), units.end(), pUnit );
		if ( iTemp != units.end() )
			units.erase( iTemp );
		else
			ASSERT( 0 );
	}
	////
	bool IsTurnDone()
	{
		return bTurnDone || !HasAlivePeople();
	}
	////
	void OnTBSEvent( ETBSEvent event )
	{
		if ( event == TBS_FINISH_OWN_TURN )
			bTurnDone = true;
		else if ( event == TBS_START_REAL_TIME )
			bTurnDone = false;
		//
		GetCommander()->OnTBSEvent( event );
		//
		for ( unsigned int k = 0; k < units.size(); ++k )
		{
			if ( !units[k]->IsDead() )
				units[k]->OnTBSEvent( event );
		}
	}
	////
	bool HasAlivePeople() const
	{
		for ( int k = 0; k < units.size(); ++k )
		{
			if ( units[k]->CanFight() )
				return true;
		}
		return false;
	}
	//
	void OnUnitDied( TUnit *pUnit )
	{
		pCommander->OnUnitDied( pUnit );
		//
		for ( TPlayerUnitSet::iterator i = units.begin(); i != units.end(); ++i )
			(*i)->OnUnitDied( pUnit );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUnit, class TPlayer, class TCommander>
class CTBSWorld
{
	struct SInterrupt
	{
		ZDATA
		CPtr<TPlayer> pPlayer;
		list<CPtr<TUnit> > units;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPlayer); f.Add(3,&units); return 0; }

		SInterrupt() {}
		SInterrupt( const list<TUnit*> &_units ): pPlayer( _units.front()->GetTBSPlayer() )
		{ 
			for ( list<TUnit*>::const_iterator i = _units.begin(); i != _units.end(); ++i )
			{
				ASSERT( (*i)->GetTBSPlayer() == pPlayer );
				units.push_back( *i ); 
			}
		}
		SInterrupt( TPlayer *_pPlayer ): pPlayer(_pPlayer) 
		{
			const vector<CMObj<TUnit> > &_units = pPlayer->GetPlayerUnits();
			for ( unsigned int i = 0; i < _units.size(); ++i )
				units.push_back( _units[i].GetPtr() );
		}
		void RemoveUnit( TUnit *_pUnit ) { units.remove( _pUnit ); }
		bool HasUnits() const { return !units.empty(); }
	};
		//typedef CInterrupt<TUnit, TPlayer> TInterrupt;
	typedef list< CObj<TPlayer> > TPlayerList;
	typedef list<SInterrupt> TInterruptList;
	ZDATA
	CPtr<CActionCounter> pSkippableCount, pActiveCount;
	TInterruptList interrupts, addInterrupts;
	TPlayerList players;
	int nLastPlayerID, nTurnPlayerID;
	bool bWasAction;
	bool bFirstTurn;
	int nActionLag;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSkippableCount); f.Add(3,&pActiveCount); f.Add(4,&interrupts); f.Add(5,&addInterrupts); f.Add(6,&players); f.Add(7,&nLastPlayerID); f.Add(8,&nTurnPlayerID); f.Add(9,&bWasAction); f.Add(10,&bFirstTurn); f.Add(11,&nActionLag); return 0; }
private:
	TPlayer* GetNextPlayer( int nMinimal )
	{
		TPlayer *pBest = 0;
		int nBest = 0x7fffffff;
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
		{
			TPlayer *p = *i;
			if ( p->GetPlayerID() >= nMinimal && p->GetPlayerID() < nBest )
			{
				nBest = p->GetPlayerID();
				pBest = p;
			}
		}
		return pBest;
	}
	bool IsRealTimePossible() const
	{
		if ( IsForcedRealTime() )
			return true;
		if ( !IsTBSRealTimeModePossible() )
			return false;
		return true;
	}
	void StartPlayerTurn( TPlayer *pPlayer )
	{
		if ( IsForcedRealTime() )
			return;
		if ( !IsValid( pPlayer ) )
			return;
		// Register this player's interrupt BEFORE running the turn-start callbacks. The release Game.exe
		// does this first thing (CTBSWorld::StartPlayerTurn @0x00775e90; it even defers OnNewPlayerTurn onto
		// an STBSEvent queue), whereas this dev snapshot pushed it only AFTER OnNewPlayerTurn. The new-turn
		// event (CEventOnNewPlayerTurnOrTime, thrown from OnNewPlayerTurn) processes periodic damage
		// (bleeding/fire) and can KILL a unit; with the interrupt not yet present that death reached
		// OnUnitDied -> MakeUnitInactive with an empty interrupt list, which called StartNextPlayerTurn ->
		// StartPlayerTurn -> OnNewPlayerTurn again -> re-applied the periodic damage -> another death ->
		// unbounded recursion (observed stack overflow). With the interrupt present the dying unit is just
		// removed from it, and the turn only advances once the player genuinely has no units left.
		interrupts.push_back( SInterrupt( pPlayer ) );
		pPlayer->OnTBSEvent( TBS_START_NEW_TURN );
		OnNewPlayerTurn( pPlayer );
		OnPassControl();
		nTurnPlayerID = pPlayer->GetPlayerID();
 	}
	void StartNextPlayerTurn()
	{
		if ( !IsRealTimePossible() )
		{
			TPlayer *pNext;
			pNext = GetNextPlayer( nTurnPlayerID + 1 );
			if ( !pNext )
			{
				pNext = GetNextPlayer( 0 );
				OnNewTurn();
			}
			StartPlayerTurn( pNext );
		}
	}
	void RecalcCurrentPlayerCommands()
	{
		// forced-RT (sequence): retail's stack top is the OWNERLESS sequence interrupt -- there is no
		// current player to recalc. With the owned turn now PRESERVED beneath the sequence span, this
		// must not zap that player's executors (TBS_RECALC pExec=0) on every mid-cutscene action edge.
		if ( IsForcedRealTime() )
			return;
		if ( !interrupts.empty() )
			interrupts.back().pPlayer->OnTBSEvent( TBS_RECALC_COMMAND );//RecalcCommands();
	}
	void OnPassControl()
	{
		TPlayer *pCurrentPlayer = 0;
		if ( !interrupts.empty() )
			pCurrentPlayer = interrupts.back().pPlayer;

		RecalcCurrentPlayerCommands();
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->OnPassControl( pCurrentPlayer );
	}
	void EndOfTurn()
	{
		//ASSERT( !interrupts.empty() );
		if ( interrupts.empty() )
			return;
		CPtr<TPlayer> pPrevPlayer = interrupts.back().pPlayer;
		interrupts.pop_back();
		//if ( !interrupts.empty() )
		if ( !IsRealTime() )
		{
			OnPassControl();
			return;
		}
		bFirstTurn = false;
		if ( IsValid( pPrevPlayer ) )
			pPrevPlayer->OnTBSEvent( TBS_FINISH_OWN_TURN );//FinishOwnTurn();
		StartNextPlayerTurn();
		//if ( interrupts.empty() )
		if ( IsRealTime() )
		{
			OnRealTimeStarted();
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
				(*i)->OnTBSEvent( TBS_START_REAL_TIME );
		}
	}
	void FetchPlayerCommands( TPlayer *pPlayer, bool bAllowNotSkippable )
	{
		for(;;)
		{
			CObj<CCommand> pCmd = pPlayer->GetCommander()->GetCommand();
			if ( !IsValid( pCmd ) )
				break;
			ASSERT( pCmd->IsSkippable() || bAllowNotSkippable );
			if ( CDynamicCast<CCmdEndOfTurn>(pCmd) )
			{
				EndOfTurn();
				break;
			}
			else if ( CDynamicCast<CCmdQuitGame>(pCmd) )
			{
				ASSERT( 0 );// not implemented yet
				EndOfTurn();
				break;
			}
			else {
				CDynamicCast<CCmdCheat> pCheatCmd(pCmd);
				if (pCheatCmd)
					pPlayer->SetCheat(pCheatCmd->nCheatMask, pCheatCmd->bState);
				else
					ExecuteCommand(pCmd);
			}
		}
	}
	CActionCounter* RenewActionCounter( CPtr<CActionCounter> *pDst )
	{
		if ( !IsValid( *pDst ) )
			(*pDst) = new CActionCounter;
		return *pDst;
	}
protected:
	TPlayer* GetNextPlayer( TPlayer* pPrev )
	{
		int nPlayerID = pPrev? pPrev->GetPlayerID() : -1;
		TPlayer *pBest = GetNextPlayer( nPlayerID + 1 );
		return pBest;
	}
	void CancelAllAction()
	{
		// retail CTBSWorld::CancelAllAction @0x36f820 broadcasts TBS_STOP_MOVE_AND_CANCEL_ACTION (NOT
		// TBS_CANCEL_ACTION). A unit interrupted MID-MOVE must be snapped to a clean grid cell and have its
		// move executor released; with plain TBS_CANCEL_ACTION the unit is left mid-stride with a live,
		// half-cancelled CExecMove (still IsExecuting()), so later MOVE orders re-aim that dead exec and are
		// silently swallowed by CUnitServer::Do's re-route branch -- only an ATTACK (pExec->Cancel()) frees
		// it. (The lighter per-player TBS_CANCEL_ACTION path -- IsRequestCancel / a human's own non-mutual
		// sighting -- is unchanged.)
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->OnTBSEvent( TBS_STOP_MOVE_AND_CANCEL_ACTION );
	}
	virtual bool IsTBSRealTimeModePossible() const = 0;
	virtual void ExecuteCommand( CCommand *_pCmd ) = 0;
	virtual void OnAction( bool bStartAction ) = 0;
	void RegisterPlayer( TPlayer *pPlayer )
	{
		pPlayer->SetPlayerID( nLastPlayerID-- );
		players.push_back( pPlayer );
	}
	void UnregisterPlayer( TPlayer *pPlayer )
	{
		TPlayerList::iterator iTemp = find( players.begin(), players.end(), pPlayer );
		if ( iTemp != players.end() )
			players.erase( iTemp );
		else
			ASSERT( 0 );
	}
	void MakeUnitActive( TUnit *pUnit, TPlayer *pPlayer )
	{
		ASSERT( IsValid( pUnit ) );
		ASSERT( IsValid( pPlayer ) );
		if ( !IsValid( pUnit ) || !IsValid( pPlayer ) )
			return;
		//
		if ( IsRealTime() )
			return;
		//
		addInterrupts.clear();
		for ( TInterruptList::iterator i = interrupts.begin(); i != interrupts.end(); ++i )
		{
			if ( i->pPlayer == pPlayer )
			{
				if ( !IsInSet( i->units, pUnit ) )
					i->units.push_back( pUnit );
			}
		}
	}
	void MakeUnitInactive( TUnit *pUnit )
	{
 		for ( TInterruptList::iterator i = interrupts.begin(); i != interrupts.end(); )
		{
			i->RemoveUnit( pUnit );
			if ( !i->HasUnits() )
				i = interrupts.erase( i );
			else
				++i;
		}
		for ( TInterruptList::iterator i = addInterrupts.begin(); i != addInterrupts.end(); )
		{
			i->RemoveUnit( pUnit );
			if ( !i->HasUnits() )
				i = addInterrupts.erase( i );
			else
				++i;
		}
		if ( interrupts.empty() && addInterrupts.empty() )
			StartNextPlayerTurn();
	}
	void StartTBSGame()
	{
		interrupts.clear();
		addInterrupts.clear();
		nTurnPlayerID = 0;
		if ( !IsRealTimePossible() )
			StartNextPlayerTurn();
		bFirstTurn = !interrupts.empty();
	}
	virtual const bool IsForcedRealTime() const = 0;
	virtual void OnNewTurn() = 0;
public:
	// the EndSequence edge: retail luaEndSequence @0x2f1a60 pops the ownerless sequence top via
	// EndOfTurn (CTBSWorld vtbl+0x1c @0x3776f0), whose !IsRealTime() branch re-fires OnPassControl to
	// the RESUMED owned turn (its commander re-syncs and thinks: lua OnStartTurn, tactical Think). The
	// dev forced-RT model REVEALS the preserved turn instead of popping -- fire the same control-pass.
	void OnSequenceEndPassControl() { if ( !IsRealTime() ) OnPassControl(); }
	CTBSWorld()
	{
		nLastPlayerID = 10000;
		nTurnPlayerID = 0;
		bWasAction = false;
		bFirstTurn = false;
		nActionLag = 0;
	}
	virtual void UpdateVisible() = 0;
	CActionCounter* GetSkippableCounter() { bWasAction = true; return RenewActionCounter( &pSkippableCount ); }
	CActionCounter* GetActiveCounter( int _nLag = 0 ) { nActionLag = Max( _nLag, nActionLag ); bWasAction = true; return RenewActionCounter( &pActiveCount ); }
	bool IsAction() const { return IsValid( pSkippableCount ) || IsValid( pActiveCount ) || nActionLag > 0; }
	bool IsSkippableAction() const { return IsValid( pSkippableCount ) && nActionLag == 0; }
	bool IsFirstTurn() const { return bFirstTurn || IsRealTime(); }
	bool IsInterrupt() const { return interrupts.size() > 1; }
	int GetEnemyPlayerWatchers( const vector<CMObj<TUnit> > &units, TPlayer *pl ) const // "internal heap limit reached" workaround
	{
		int nWatchers = 0;
		const vector<CMObj<TUnit> > &enemies = pl->GetPlayerUnits();
		for ( vector<CMObj<TUnit> >::const_iterator iu = enemies.begin(); iu != enemies.end(); ++iu )
		{
			if ( !(*iu)->CanFight() )
				continue;
			vector<CPtr<CUnit> > visible;
			(*iu)->GetVisible( &visible );
			for ( vector<CMObj<TUnit>>::const_iterator ku = units.begin(); ku != units.end(); ++ku )
			{
				TUnit *pU = *ku;
				// Just to make the compiler shut the fuck up (it can't compare const CPtr<CUnit> with const TUnit*)
				CPtr<TUnit> pCPtr(pU);
				if ( pU->CanFight() && IsInSet( visible, pCPtr) )
						++nWatchers;
			}
		}
		return nWatchers;
	}
	int GetEnemyWatchers( TPlayer *pPlayer ) const
	{
		if ( !IsValid( pPlayer ) )
			return 0;
		const vector<CMObj<TUnit> > &units = pPlayer->GetPlayerUnits();
		int nWatchers = 0;
		for ( TPlayerList::const_iterator i = players.begin(); i != players.end(); ++i )
			if ( *i != pPlayer )
				nWatchers += GetEnemyPlayerWatchers( units, *i );

		return nWatchers;
	}
	void GlobalSituationHasChanged()
	{
		// clear all queued cmds since situation has changed
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->ClearList();
		CancelAllAction();
	}
	// retail CTBSWorld::GivePlayerTurn @0x377650 (ITBSWorld vtbl+0x14): force the active turn to a specific
	// player -- reset the situation, drop the whole interrupt stack, and start that player's fresh base turn.
	// Retail bails during a scripted sequence (CWorld::IsSequence); this dev snapshot has no world-level
	// sequence predicate, so it uses IsForcedRealTime() (set by BeginSequence -- already the guard
	// StartPlayerTurn uses). The retail STBSEvent-7 (TBS_GLOBAL_SITUATION_CHANGED) is applied synchronously
	// here via GlobalSituationHasChanged(). Must be a CTBSWorld member: StartPlayerTurn + interrupts are
	// private (and this mirrors retail, where GivePlayerTurn is itself a CTBSWorld vtbl slot).
	void GivePlayerTurn( TPlayer *pPlayer )
	{
		if ( IsForcedRealTime() )
			return;
		if ( !IsValid( pPlayer ) )
			return;
		GlobalSituationHasChanged();	// retail event 7: clear queued cmds + cancel actions
		interrupts.clear();				// retail drops the SInterrupt stack
		StartPlayerTurn( pPlayer );		// retail StartPlayerTurn @0x375e90 (pushes a fresh interrupt)
	}
	void AddInterrupt( const list<TUnit*> &units )
	{
		if ( IsForcedRealTime() )
			return;
		//
		ASSERT( !units.empty() );
		if ( units.empty() )
			return;
		GlobalSituationHasChanged();
		addInterrupts.clear();
		addInterrupts.push_back( SInterrupt( units ) );
	}
	void OnUnitDied( TUnit *p )
	{
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->OnUnitDied( p );
		MakeUnitInactive( p );
	}
	void GetInterrupts( vector<TPlayer*> *pInterrups ) const
	{
		pInterrups->resize( interrupts.size() );
		int nTemp = 0;
		for ( TInterruptList::const_iterator iTemp = interrupts.begin(); iTemp != interrupts.end(); iTemp++ )
		{
			(*pInterrups)[nTemp] = iTemp->pPlayer;
			nTemp++;
		}
	}
	void Segment()
	{
		// retail StartSequence @0x375dd0 pushes an OWNERLESS interrupt ON TOP of any owned turn and
		// EndSequence pops it back: a turn begun BEFORE the sequence (GFirst starts one at zone load --
		// Diplomacies record 36 makes 0<->2 hostile, so StartTBSGame finds real time impossible)
		// SURVIVES the cutscene and RESUMES afterwards -- the enemies do not act while the sequence
		// fades out and the first post-cutscene turn keeps its retail owner. The Jan03 forced-RT wipe
		// that used to live here destroyed that turn (and the robbers' WantTurnBased then seized the
		// first turn on the EndSequence edge). The bForcedRealTime span now PRESERVES the stack:
		// IsRealTime() is forced-true regardless, the fetch below runs the every-player sequence fetch
		// (retail FetchNewCommands' sequence branch), and IsTBSUnitActive() opens every unit to script
		// commands while forced. Nothing can push onto the stack meanwhile (AddInterrupt /
		// WantTurnBased / StartPlayerTurn / GivePlayerTurn all bail on IsForcedRealTime).

		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->Segment();

		if ( nActionLag > 0 )
			--nActionLag;

		if ( bWasAction && !IsAction() )
		{
			UpdateVisible();
			RecalcCurrentPlayerCommands();
			OnAction( false );
			for ( TPlayerList::const_iterator k = players.begin(); k != players.end(); ++k )
				(*k)->OnTBSEvent( TBS_ACTION_FINISH );//OnActionFinish();
		}
		if ( !addInterrupts.empty() )
		{
			ASSERT( addInterrupts.size() == 1 );
			if ( interrupts.empty() )
			{
				bFirstTurn = true;
				StartPlayerTurn( addInterrupts.front().pPlayer );
				// retail folded the addInterrupts staging away entirely (CTBSWorld::AddInterrupt @0x377400 /
				// WantTurnBased @0x375b10 consume the request synchronously: ONE StartPlayerTurn, ONE
				// pass-control). Leaving the staged entry here made the NEXT Segment take the else-branch
				// on the SAME player -- a duplicate OnPassControl that re-entered every commander's
				// turn-start (double CAICommander::OnPassControl -> double tactical Think / lua OnStartTurn)
				// one segment after the first, wrecking the just-started AI turn's job bookkeeping.
				addInterrupts.clear();
			}
			else
			{
				// no more then one interrupt in stack is allowed
				while ( interrupts.size() > 1 )
					interrupts.pop_back();
				// ignore same player interrupt
				if ( addInterrupts.front().pPlayer == interrupts.back().pPlayer )
				{
//					ASSERT( 0 );
					addInterrupts.clear();
				}
				else
					interrupts.splice( interrupts.end(), addInterrupts );
				OnPassControl();
			}
		}
		// check if someone want interrupt
		if ( interrupts.empty() )
		{
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			{
				TPlayer *pPlayer = *i;
				TCommander *pC = pPlayer->GetCommander();
				if ( pC->IsRequestInterrupt() && !IsForcedRealTime() )
				{
					CancelAllAction();
					bFirstTurn = true;
					StartPlayerTurn( pPlayer );
					break;
				}
				else if ( pC->IsRequestCancel() )
					pPlayer->OnTBSEvent( TBS_CANCEL_ACTION );//CancelAction();
			}
		}
		else if ( !IsForcedRealTime() )
		{
			TCommander *pCommander = interrupts.back().pPlayer->GetCommander();
			if ( pCommander->IsRequestCancel() )//|| pCommander->IsRequestInterrupt() )
				CancelAllAction();
		}
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->ClearRequests();
		// decide on commands for next segment. Keyed on IsRealTime() (forced-RT OR empty stack), NOT on
		// stack emptiness: while a sequence runs over a PRESERVED owned turn the world must fetch from
		// EVERY player (retail FetchNewCommands' sequence branch), not just the interrupt top.
		if ( IsRealTime() )
		{
			// real time mode
			// pick commands from every player
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
				FetchPlayerCommands( *i, IsForcedRealTime() );
		}
		else
		{
			// turn based mode
			ASSERT( !IsForcedRealTime() );
			if ( !IsAction() )
			{
				// pick commands from current player regarding current units
				FetchPlayerCommands( interrupts.back().pPlayer, true );
			}
			else
				OnAction( true );
		}
		bWasAction = IsAction();
	}
	TPlayer* GetTBSCurrentPlayer() const { if ( interrupts.empty() ) return 0; return interrupts.back().pPlayer; }
	bool IsRealTime() const { return IsForcedRealTime() || interrupts.empty(); }
	// The AI world-executor tri-state (retail CTBSWorld vtbl+0x20/+0x24/+0x28 = IsRealTime/IsSequence/IsTurnBased,
	// probed by CAICombatLogic IsNeedToThink/IsEndOfTurn/IsIdleJob). IsTurnBased == !IsRealTime (disasm-exact,
	// @0x00375a50). IsSequence models retail's player-less current interrupt via IsForcedRealTime() -- the a5dll
	// has no world-level sequence predicate (cf. GivePlayerTurn), a documented minor divergence for the idle probe.
	bool IsTurnBased() const { return !IsRealTime(); }
	bool IsSequence() const { return !interrupts.empty() && IsForcedRealTime(); }
	// forced-RT (sequence) opens EVERY unit to commands -- the dev analog of retail's every-player
	// sequence fetch; a preserved owned turn must not block other players' scripted moves mid-cutscene.
	bool IsTBSUnitActive( TUnit *pUnit ) const { if ( IsForcedRealTime() || interrupts.empty() ) return true; return IsInSet( interrupts.back().units, pUnit ); }
	bool CanPlayerSeeAction( TPlayer *_pPlayer ) const
	{
		// check if _pPlayer see any units performing skippable action
		const list<CPtr<TUnit> > &v = _pPlayer->GetTBSVisible();
		for ( list<CPtr<TUnit> >::const_iterator k = v.begin(); k != v.end(); ++k )
		{
			TUnit *pTest = (*k);
			if ( !pTest->CanFight() )
				continue;
			if ( pTest->IsPerformingAction() )
				return true;
		}
		return false;
	}
	bool CanSkip( TPlayer *_pPlayer ) const
	{
		if ( !interrupts.empty() && interrupts.back().pPlayer != _pPlayer )
		{
			// is not real time & not current player is active
			if ( IsSkippableAction() )
			{
				// some skippable action is taking place
				// check if _pPlayer see any units performing skippable action
				return !CanPlayerSeeAction( _pPlayer );
			}
		}
		return false;
	}
	bool HasEnemies( TPlayer *pPlayer )
	{
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
		{
			TPlayer *p = *i;
			if ( pPlayer != p && p->HasAlivePeople() )
				return true;
		}
		return false;
	}
	void WantTurnBased( TPlayer *pPlayer )
	{
		if ( IsForcedRealTime() )
			return;
		if ( interrupts.empty() && addInterrupts.empty() )
		{
			GlobalSituationHasChanged();
			addInterrupts.push_back( SInterrupt( pPlayer ) );
		}
	}
	virtual void OnNewPlayerTurn( TPlayer *pPlayer ) {}
	virtual void OnRealTimeStarted() {}
	void GetPlayersList( vector< CPtr<TPlayer> > *pPlayers ) const
	{
		pPlayers->clear();
		for ( TPlayerList::const_iterator i = players.begin(); i != players.end(); ++i )
			pPlayers->push_back( (*i).GetPtr() );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
