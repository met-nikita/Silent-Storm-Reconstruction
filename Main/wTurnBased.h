#ifndef __wTurnBased_H_
#define __wTurnBased_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NWorld
{
template<class TUnit> struct SAISound;   // wUnitSounds.h (CanPlayerSeeOrHearAction's heard part)
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETBSEvent
{
	TBS_NONE = 0,
	TBS_START_NEW_TURN = 1,
	TBS_FINISH_OWN_TURN = 2,
	TBS_START_REAL_TIME = 3,
	TBS_ACTION_FINISH = 4,
	TBS_CANCEL_ACTION = 5,
	TBS_RECALC_COMMAND = 6,
	TBS_GLOBAL_SITUATION_CHANGED = 7,
	TBS_NEW_LARGE_TURN = 8,
	TBS_PASS_CONTROL = 9,
	TBS_GRID_INFO_UPDATED = 10,
	TBS_STOP_MOVE_AND_CANCEL_ACTION = 11
};
// Retail STBSEvent: queue payload, not an immediate unit callback.
struct STBSEvent
{
	ETBSEvent event;
	CPtr<CObjectBase> pParam;
	STBSEvent() : event(TBS_NONE) {}
	STBSEvent( ETBSEvent e, CObjectBase *p = 0 ) : event(e), pParam(p) {}
	int operator&( CStructureSaver &f ) { f.Add(2,&event); f.Add(3,&pParam); return 0; }
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
		// Retail v1.2 0x76fa70 sends these events to units, not commanders.
		// Commander turn work is driven by OnPassControl instead.
		for ( unsigned int k = 0; k < units.size(); ++k )
		{
			if ( IsValid( units[k] ) && !units[k]->IsDead() )
				units[k]->OnTBSEvent( event );
		}
	}
	////
	bool HasAlivePeople() const
	{
		for ( int k = 0; k < units.size(); ++k )
		{
			// retail @0x36f780: skip null/stale (IsRefInvalid 0x80) refs before CanFight
			if ( !IsValid( units[k] ) )
				continue;
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
	TInterruptList interrupts;
	TPlayerList players;
protected:
	list<STBSEvent> events;
private:
	int nLastPlayerID, nTurnPlayerID;
	bool bWasAction;
	bool bFirstTurn;
	int nActionLag;
	// retail CTBSWorld+0x1c (save tag 8). Write-only in retail too: the ctor (@0x375960) and
	// OnPassControl (@0x372bf0, at its tail) clear it, FetchPlayerCommands (@0x371610) sets it when the
	// fetched command's player IS the stack-top owner -- and NOTHING ever reads it back except
	// operator& (@0x37b300). Carried purely so the tag-8 chunk round-trips (wire-audit UNREAD 1.8:
	// retail wrote 1 byte every slot, this fork consumed none).
	bool bHasCommandFromCurrentPlayer;
public:
	// retail v1.2 save convergence: retail's CTBSWorld::operator& (@0x37b300) serializes
	// {2=interrupts, 3=players, 4=nLastPlayerID, 5=nTurnPlayerID, 6=bFirstTurn, 7=events,
	// 8=bHasCommandFromCurrentPlayer}. Tag 8 is now carried (see the member above).
	// Tag 7 is the deferred event queue, drained at the four retail segment boundaries.
	// The action-counter state (pSkippableCount/pActiveCount/bWasAction/nActionLag) is NOT on retail's
	// CTBSWorld base -- retail refactored it into a standalone CActionTracker sub-object serialized at
	// CWorld tag 4 (operator& @0x37a020); CWorld emits it through ActionTrackerChunk() below.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&interrupts); f.Add(3,&players); f.Add(4,&nLastPlayerID); f.Add(5,&nTurnPlayerID); f.Add(6,&bFirstTurn); f.Add(7,&events); f.Add(8,&bHasCommandFromCurrentPlayer); return 0; }
	// retail CActionTracker (CWorld save tag 4) wire adapter: {2=pSkippableCount, 3=pActiveCount,
	// 4=bWasAction, 5=nActionLag}. Pointer-holder so CWorld (which cannot see these private fields)
	// can emit the chunk without reparenting.
	struct SActionTrackerChunk
	{
		CPtr<CActionCounter> *pSkip, *pActive;
		bool *pbWasAction;
		int *pnActionLag;
		int operator&( CStructureSaver &f ) { f.Add(2,pSkip); f.Add(3,pActive); f.Add(4,pbWasAction); f.Add(5,pnActionLag); return 0; }
	};
	SActionTrackerChunk ActionTrackerChunk() { SActionTrackerChunk c = { &pSkippableCount, &pActiveCount, &bWasAction, &nActionLag }; return c; }
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
		// retail CTBSWorld vtbl+0x3c is PURE -- CWorld::IsRealTimePossible @0x364f10 is the whole body
		// (and has NO sequence term; the sequence gate lives in StartNextPlayerTurn @0x375f80, gate 2).
		// The old forced-RT short-circuit here was the Jan03 flag model.
		return IsTBSRealTimeModePossible();
	}
	void StartPlayerTurn( TPlayer *pPlayer )
	{
		if ( IsSequence() || !IsValid( pPlayer ) )
			return;
		interrupts.push_back( SInterrupt( pPlayer ) );
		events.push_back( STBSEvent( TBS_START_NEW_TURN, pPlayer ) );
		OnPassControl();
		nTurnPlayerID = pPlayer->GetPlayerID();
	}
	void StartNextPlayerTurn()
	{
		// retail @0x375f80: TWO gates in this order -- IsRealTimePossible (vtbl+0x3c) first,
		// IsSequence (vtbl+0x24) second; either true means no rotation.
		if ( !IsRealTimePossible() )
		{
			if ( IsSequence() )
				return;
			TPlayer *pNext;
			pNext = GetNextPlayer( nTurnPlayerID + 1 );
			if ( !pNext )
			{
				pNext = GetNextPlayer( 0 );
				events.push_back( STBSEvent( TBS_NEW_LARGE_TURN ) );
			}
			StartPlayerTurn( pNext );
		}
	}
public:
	void GridInfoUpdated()
	{
		events.push_back( STBSEvent( TBS_GRID_INFO_UPDATED ) );
	}
	void RecalcCurrentPlayerCommands()
	{
		// retail @0x36f850 three-way (disasm-proven, runs during sequences on action-finish edges):
		// empty stack -> every PLAYER recalcs (player-level event, commander included); OWNERLESS top
		// (sequence) -> every player's UNITS recalc (unit-level event 6, no commander notify); owned
		// top -> ONLY the top interrupt's unit set. The unit-level RECALC drop is recoverable by
		// design: pCurrentCmd survives, the next CCmdContinue re-creates the executor (retail
		// IsCancelableExec @0x392fe0 only spares cannon execs). The round-2 forced-RT early-return
		// here protected the BURIED turn owner from the old player-level recalc -- obsolete.
		if ( interrupts.empty() )
		{
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
				(*i)->OnTBSEvent( TBS_RECALC_COMMAND );
			return;
		}
		if ( interrupts.back().pPlayer == 0 )
		{
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			{
				const vector< CMObj<TUnit> > &pUnits = (*i)->GetPlayerUnits();
				for ( unsigned int k = 0; k < pUnits.size(); ++k )
					if ( IsValid( pUnits[k].GetPtr() ) )
						pUnits[k]->OnTBSEvent( TBS_RECALC_COMMAND );
			}
			return;
		}
		for ( list< CPtr<TUnit> >::iterator u = interrupts.back().units.begin(); u != interrupts.back().units.end(); ++u )
			if ( IsValid( *u ) )
				(*u)->OnTBSEvent( TBS_RECALC_COMMAND );
	}
	void OnPassControl()
	{
		events.push_back( STBSEvent( TBS_PASS_CONTROL, GetTBSCurrentPlayer() ) );
		bHasCommandFromCurrentPlayer = false;
	}
	// (EndOfTurn moved to the public section -- retail vtbl+0x1c is public; luaEndSequence calls it.)
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
			// retail @0x371610: flagged for every non-EndOfTurn command fetched from the stack-top
			// owner. A SIDE EFFECT, not a branch -- the dispatch below must still run.
			if ( pPlayer == GetTBSCurrentPlayer() )
				bHasCommandFromCurrentPlayer = true;
			// (CCmdQuitGame dispatch REMOVED -- W5: the never-constructed ASSERT(0) stub died with the class)
			{
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
		// retail RemoveTBSUnit @0x377390 erase condition: units EMPTY *** AND pPlayer != null *** --
		// the OWNERLESS sequence entry (always unit-less) is explicitly exempted, which is how the
		// sequence survives mid-cutscene unit deaths.
 		for ( TInterruptList::iterator i = interrupts.begin(); i != interrupts.end(); )
		{
			i->RemoveUnit( pUnit );
			if ( !i->HasUnits() && i->pPlayer != 0 )
				i = interrupts.erase( i );
			else
				++i;
		}
		if ( interrupts.empty() )
			StartNextPlayerTurn();
	}
	void StartTBSGame()
	{
		interrupts.clear();
		nTurnPlayerID = 0;
		if ( !IsRealTimePossible() )
			StartNextPlayerTurn();
		bFirstTurn = !interrupts.empty();
	}
	virtual void OnNewTurn() = 0;
public:
	// The interrupt stack changes immediately; its notifications are deferred.
	void StartSequence()
	{
		GlobalSituationHasChanged();
		interrupts.push_back( SInterrupt() );
		OnPassControl();
	}
	// retail CTBSWorld::EndOfTurn @0x3776f0 (vtbl+0x1c) -- PUBLIC in retail; the normal CCmdEndOfTurn
	// pop AND the EndSequence edge (luaEndSequence @0x2f1a60 calls it FIRST, before the per-unit
	// OnSequenceFinished notifies). Pop the top: if an owned turn remains (a turn preserved beneath a
	// sequence, or the interrupted turn beneath an interrupt), OnPassControl re-fires to it; else
	// bFirstTurn=false, TBS_FINISH_OWN_TURN to the popped owner (null for the ownerless sequence top
	// -> no event), StartNextPlayerTurn (gated on IsRealTimePossible/IsSequence -- GFirst: both sides
	// saw each other mid-cutscene => the next player's turn starts THAT FRAME), and a realtime
	// broadcast if the rotation decided real time is back.
	void EndOfTurn()
	{
		//ASSERT( !interrupts.empty() );
		if ( interrupts.empty() )
			return;
		CPtr<TPlayer> pPrevPlayer = interrupts.back().pPlayer;
		interrupts.pop_back();
		if ( !IsRealTime() )
		{
			OnPassControl();
			return;
		}
		bFirstTurn = false;
		if ( IsValid( pPrevPlayer ) )
			events.push_back( STBSEvent( TBS_FINISH_OWN_TURN, pPrevPlayer ) );
		StartNextPlayerTurn();
		if ( IsRealTime() )
		{
			events.push_back( STBSEvent( TBS_START_REAL_TIME ) );
		}
	}
	CTBSWorld()
	{
		nLastPlayerID = 10000;
		nTurnPlayerID = 0;
		bWasAction = false;
		bFirstTurn = false;
		nActionLag = 0;
		bHasCommandFromCurrentPlayer = false;   // retail ctor @0x375960
	}
	virtual void UpdateVisible( bool bForce = false ) = 0;
	// retail CWorld::TryUpdateVisible @0x361610: action-finish vision-recalc delayer probe; true = finish now.
	virtual bool TryUpdateVisible() = 0;
	CActionCounter* GetSkippableCounter() { bWasAction = true; return RenewActionCounter( &pSkippableCount ); }
	CActionCounter* GetActiveCounter( int _nLag = 0 ) { nActionLag = Max( _nLag, nActionLag ); bWasAction = true; return RenewActionCounter( &pActiveCount ); }
	// retail IsAction @0x370030 also ORs bWasAction -- there bWasAction is re-latched BEFORE the read
	// (SyncAction @0x36f660), so the term equals the current state; here bWasAction is the previous-tick
	// edge latch (ProcessActionTracker below), so adding it would make the falling edge never fire.
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
		events.push_back( STBSEvent( TBS_GLOBAL_SITUATION_CHANGED ) );
	}
	void GivePlayerTurn( TPlayer *pPlayer )
	{
		if ( IsSequence() )   // retail @0x377650 gate (CWorld::IsSequence)
			return;
		if ( !IsValid( pPlayer ) )
			return;
		GlobalSituationHasChanged();
		interrupts.clear();				// retail drops the SInterrupt stack
		StartPlayerTurn( pPlayer );		// retail StartPlayerTurn @0x375e90 (pushes a fresh interrupt)
	}
	void AddInterrupt( const list<TUnit*> &units )
	{
		if ( IsSequence() || units.empty() )
			return;
		GlobalSituationHasChanged();
		TPlayer *pPlayer = units.front()->GetTBSPlayer();
		if ( interrupts.empty() )
		{
			bFirstTurn = true;
			StartPlayerTurn( pPlayer );
			CheckAutoFirstTurnInterrupts( pPlayer );
			return;
		}
		while ( interrupts.size() > 1 )
			interrupts.pop_back();
		if ( pPlayer != interrupts.back().pPlayer )
			interrupts.push_back( SInterrupt( units ) );
		OnPassControl();
	}
	void CheckAutoFirstTurnInterrupts( TPlayer *pFirst )
	{
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
		{
			if ( *i == pFirst )
				continue;
			list<TUnit*> units;
			const vector<CMObj<TUnit> > &roster = (*i)->GetPlayerUnits();
			for ( int k = 0; k < roster.size(); ++k )
				if ( IsValid( roster[k] ) && roster[k]->HasAutoFirstTurnInterrupt() )
					units.push_back( roster[k] );
			if ( !units.empty() )
				AddInterrupt( units );
		}
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
	virtual void ProcessTBSEvents() = 0;
	// Retail action-window tail runs after the queued notifications at every drain.
	void ProcessActionTracker( bool bRecalc )
	{
		if ( nActionLag > 0 )
			--nActionLag;
		bool bAction = IsAction();
		bool bChanged = bWasAction != bAction;
		bWasAction = bAction;
		if ( bChanged )
		{
			if ( bAction )
				OnAction( true );
			else if ( !TryUpdateVisible() )
			{
				{ CObj<CActionCounter> pTemp = GetActiveCounter( 2 ); }
				bWasAction = IsAction();
				OnAction( true );
				return;
			}
			else
			{
				OnAction( false );
				UpdateVisible();
				for ( TPlayerList::const_iterator k = players.begin(); k != players.end(); ++k )
					(*k)->OnTBSEvent( TBS_ACTION_FINISH );
				bRecalc = true;
			}
		}
		if ( bRecalc )
			RecalcCurrentPlayerCommands();
	}
	void Segment()
	{
		// The OWNERLESS-INTERRUPT sequence model (retail 1:1): StartSequence @0x375dd0 pushes an
		// ownerless entry ON TOP of any owned turn and luaEndSequence pops it via EndOfTurn @0x3776f0.
		// A turn begun BEFORE the sequence (GFirst starts one at zone load -- Diplomacies record 36
		// makes 0<->2 hostile, so StartTBSGame finds real time impossible) stays BURIED beneath it and
		// RESUMES on the pop. While the ownerless top sits: GetTBSCurrentPlayer()==null (retail's
		// ownerless top -- CMission::IsRealTime / CUnitServer::IsMoving consumers see real time),
		// IsRealTime() is true (the fetch below runs the every-player sequence branch of retail
		// FetchNewCommands @0x372950), IsTBSUnitActive() opens every unit to script commands, and
		// nothing can push onto the stack (AddInterrupt / WantTurnBased / StartPlayerTurn /
		// GivePlayerTurn all bail on IsSequence, all retail-gated).

		ProcessTBSEvents();   // retail @0x76bd13: ProcessTBSEvents #1, before the commander segments

		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->Segment();

		ProcessTBSEvents();   // retail @0x76bd39: ProcessTBSEvents #2, after the commander segments

		// check if someone want interrupt
		if ( interrupts.empty() )
		{
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			{
				TPlayer *pPlayer = *i;
				TCommander *pC = pPlayer->GetCommander();
				// retail CheckCancelAndInterruptRequests @0x377830 empty-stack branch: both requests
				// honored, no sequence term (the stack is never empty during a sequence).
				if ( pC->IsRequestInterrupt() )
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
		else if ( interrupts.back().pPlayer != 0 )
		{
			// retail @0x377830 stacked branch: `if (back().pControl != 0 && cancel-requested)
			// CancelAllAction()` -- an ownerless top ignores (and below clears) all requests.
			TCommander *pCommander = interrupts.back().pPlayer->GetCommander();
			if ( pCommander->IsRequestCancel() )//|| pCommander->IsRequestInterrupt() )
				CancelAllAction();
		}
		for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
			(*i)->GetCommander()->ClearRequests();

		ProcessTBSEvents();   // retail @0x76bd4a: ProcessTBSEvents #3, after CheckCancelAndInterruptRequests @0x377830

		// decide on commands for next segment. Keyed on IsRealTime() (empty stack OR ownerless top):
		// retail FetchNewCommands @0x372950 fetches from EVERY player both in real time (arg false)
		// and during a sequence (arg true, fetched even mid-action); an owned top fetches only the
		// current player when idle. (Retail's bool is vestigial -- FetchPlayerCommands @0x371610
		// never reads it -- but the dev ASSERT in FetchPlayerCommands still keys on it.)
		if ( IsRealTime() )
		{
			// real time mode
			// pick commands from every player
			for ( TPlayerList::iterator i = players.begin(); i != players.end(); ++i )
				FetchPlayerCommands( *i, IsSequence() );
		}
		else
		{
			// turn based mode
			ASSERT( !IsSequence() );
			if ( !IsAction() )
			{
				// pick commands from current player regarding current units
				FetchPlayerCommands( interrupts.back().pPlayer, true );
			}
			// NO OnAction(true) here -- retail FetchNewCommands @0x372950 does NOTHING mid-action
			// (disasm 0x7729aa: bIsAction!=0 falls straight to ret). Retail's rising-edge OnAction(true)
			// in ProcessTBSEvents @0x3675d0 is structurally dead for normal actions too: taking a counter
			// (GetSkippableCounter @0x3708f0 / GetActiveCounter @0x370990) already sets bWasAction=1, so
			// the edge flip-flop @0x767816 never sees 0->1. The Jan03 per-segment OnAction(true) that sat
			// here banned E_NO_IDLE_ON_ACTION on every unit for the whole action = all bystanders frozen
			// mid-pose during any TB move/shot (runtime-proven vs retail via d_idle_animation).
		}

		ProcessTBSEvents();   // retail @0x76bd68: ProcessTBSEvents #4, after FetchNewCommands (latch = retail end-of-pump state)
	}
	// retail GetTBSCurrentPlayer @0x375ab0: the stack top's owner -- NULL during a sequence (ownerless
	// top) and in real time (empty stack). THE key consumer fix: CWorld::GetCurrentPlayer and the
	// CMission::IsRealTime / CUnitServer::IsMoving family now see retail's ownerless top mid-cutscene.
	TPlayer* GetTBSCurrentPlayer() const { if ( interrupts.empty() ) return 0; return interrupts.back().pPlayer; }
	// The retail three-way stack-top partition (CTBSWorld vtbl+0x20/+0x24/+0x28 @0x375a10/@0x375a30/
	// @0x375a50): IsRealTime = empty stack OR ownerless top; IsSequence = non-empty stack with an
	// OWNERLESS top; IsTurnBased = owned top. Jan03's bForcedRealTime term is GONE -- the ownerless
	// sequence interrupt (StartSequence @0x375dd0) subsumes it.
	bool IsRealTime() const { return GetTBSCurrentPlayer() == 0; }
	bool IsTurnBased() const { return !IsRealTime(); }
	bool IsSequence() const { return !interrupts.empty() && interrupts.back().pPlayer == 0; }
	// retail IsTBSUnitActive @0x375ad0: everyone acts in real time or under an ownerless top (every
	// unit stays open to script commands mid-cutscene); otherwise only the top interrupt's set.
	bool IsTBSUnitActive( TUnit *pUnit ) const { if ( interrupts.empty() || interrupts.back().pPlayer == 0 ) return true; return IsInSet( interrupts.back().units, pUnit ); }
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
	// retail CWorld::CanSeeOrHearAction @0x36b020: real time / sequence always counts as "seen"
	// (vtbl+0x1a0 IsRealTime covers the ownerless top; retail also probes vtbl+0x1a8 IsSequence,
	// subsumed); else the SEE part (the TBS-visible loop above), else the HEAR part -- collect every
	// sound heard by _pPlayer's units (CSoundsTracker::AddSounds, unit+0x15c) and count it "seen"
	// when a sound's SOURCE unit is alive, fightable, and performing an action (a heard firefight
	// must not be fast-forwarded).
	bool CanPlayerSeeOrHearAction( TPlayer *_pPlayer ) const
	{
		if ( IsRealTime() )
			return true;
		if ( CanPlayerSeeAction( _pPlayer ) )
			return true;
		list< SAISound<TUnit> > sounds;
		const vector< CMObj<TUnit> > &units = _pPlayer->GetPlayerUnits();
		for ( unsigned int k = 0; k < units.size(); ++k )
			if ( IsValid( units[k].GetPtr() ) )
				units[k]->AddSounds( &sounds );
		for ( list< SAISound<TUnit> >::const_iterator i = sounds.begin(); i != sounds.end(); ++i )
		{
			TUnit *pWho = i->pWho;
			if ( IsValid( pWho ) && pWho->CanFight() && pWho->IsPerformingAction() )
				return true;
		}
		return false;
	}
	bool CanSkip( TPlayer *_pPlayer ) const
	{
		// retail CWorld::CanSkip @0x36b1f0 (and the inlined gate in UpdateWorld @0x36c290):
		// GetTBSCurrentPlayer() must be NON-NULL and another player's -- the ownerless sequence top
		// (null) NEVER fast-forwards hidden actions (the Jan03 `back().pPlayer != _pPlayer` let the
		// null top pass, fast-forwarding unseen cutscene movement). Then a running skippable action
		// is skipped iff the player can neither SEE nor HEAR it.
		TPlayer *pCurrent = GetTBSCurrentPlayer();
		if ( pCurrent != 0 && pCurrent != _pPlayer )
		{
			if ( IsSkippableAction() )
				return !CanPlayerSeeOrHearAction( _pPlayer );
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
		if ( IsSequence() || !interrupts.empty() )
			return;
		list<TUnit*> units;
		const vector<CMObj<TUnit> > &roster = pPlayer->GetPlayerUnits();
		for ( int k = 0; k < roster.size(); ++k )
			units.push_back( roster[k] );
		AddInterrupt( units );
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
