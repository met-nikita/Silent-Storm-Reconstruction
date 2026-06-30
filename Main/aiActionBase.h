#ifndef __AIACTIONBASE_H_
#define __AIACTIONBASE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the new CAIAction base + SActionInfo<> cache (structural port).
//
// This SUPERSEDES the dev a5dll/Main/aiAction.h CAIAction (dev: `:CObjectBase` + CPtr<IAIState> pState,
// Do(IAILogContainer*)). The release rebases the action on the unit (IAIUnit), logs through CAILog, and
// adds the per-action SActionInfo<> memoized result cache the CDecision engine scores against. Held in a
// separate file so the dev build stays green; at the phase-7 swap this content folds into aiAction.h.
//
// Verified from the release Game.exe - reconstruction/exports/{sactioninfo.c, vtable_action.txt}.
// CAIAction vtable: slot4 CanPerform() [base impl], slot5 Do(CAILog*), slot6 ComparePlaces(p1,p2),
// slot7 CanDo(place), slot8 ResetInfoHash() - the last four pure (each concrete action overrides them;
// CanDo/ResetInfoHash delegate to the action's SActionInfo member). SActionInfo<TAction,TInfo> memoizes
// TAction::GetInfoInner keyed by the place's path-id; CanDo() = GetInfo().bCanDo.
//
// WIP for the substrate port - NOT yet in Main.vcxproj.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiPosition.h"    // SUnitPosition, SPathPlace, SPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IAIUnit;
class IAIState;
class CAILog;
////////////////////////////////////////////////////////////////////////////////////////////////////
// SPlaceWithAP - a unit pose/position paired with the AP the unit would have on arrival.
// (Carried over verbatim from the dev aiAction.h - identical layout + serialization tags 2,3.)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPlaceWithAP
{
	ZDATA
	SUnitPosition place;
	int nUnitAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&place); f.Add(3,&nUnitAP); return 0; }
	SPlaceWithAP() {}
	SPlaceWithAP( const SUnitPosition &_place, int _nUnitAP ): place( _place ), nUnitAP( _nUnitAP ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIAction - abstract action a unit can perform from a place (shoot/heal/loot/move/snipe/...).
// A logic binds the action to a place source; CAIChoosePlaceJob scores each candidate place via
// CanDo()/ComparePlaces(); the CDecision picks the best action; Do() emits the CAILog records.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIAction: public CObjectBase
{
	ZDATA
	CPtr<IAIUnit> pUnit;          // +0x0c  acting unit
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pUnit ); return 0; }   // slot3
	//
protected:
	IAIUnit*      GetUnit() const;              // pUnit (null/weak-ref guarded)
	IAIUnit*      GetEnemy() const;             // current target of pUnit's AI state
	SPlaceWithAP  GetCurrentPlace() const;      // pUnit's present place + remaining AP
	//
public:
	CAIAction() {}
	CAIAction( IAIUnit *_pUnit ): pUnit( _pUnit ) {}
	//
	virtual bool CanPerform();                                          // slot4: prerequisites hold (base: true)
	virtual void Do( CAILog *pLog ) const = 0;                          // slot5: emit the log records
	virtual bool ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const = 0; // slot6
	virtual bool CanDo( const SPlaceWithAP &place ) = 0;                // slot7: delegates to SActionInfo
	virtual void ResetInfoHash() = 0;                                   // slot8: clears the SActionInfo cache
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SActionInfo<TAction, TInfo> - per-action result cache embedded in each concrete action. Each action
// declares a nested `struct SInfo` (first member `bool bCanDo`) + a `GetInfoInner(place, SInfo*)`; the
// cache memoizes it keyed by the place's path-id. CanDo()/GetInfo() drive the CDecision scoring cheaply.
// VERIFIED from SActionInfo<CAIHealAction,...> (exports/sactioninfo.c).
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TAction, class TInfo>
struct SActionInfo
{
	CPtr<TAction>            pOwner;             // owning action (provides GetInfoInner)
	unordered_map<int, TInfo>     hash;              // place-path-id -> cached TInfo
	//
	SActionInfo(): hash( 10 ) {}
	SActionInfo( TAction *_pOwner ): pOwner( _pOwner ), hash( 10 ) {}
	//
	// aiAttackLogic SActionInfo<TAction,TInfo>::GetInfo instantiations (VA = RVA + 0x400000):
	//   @0x00423730 <CAICollectSnipeAPAction> @0x004237c0 <CAISnipeShotAction>   @0x00423840 <CAICancelSnipeAction>
	//   @0x004238c0 <CAILeavePKAction>         @0x004249e0 <CAIDockWithHGAction>  @0x00424a90 <CAIUndockFromHGAction>
	//   @0x00425830 <CAIBeginSnipeAction>      @0x004258b0 <CAIShootFromHGAction> @0x00425930 <CAITerrorPKAction>
	//   @0x004259b0 <CAIWearPKAction>
	void GetInfo( const SPlaceWithAP &place, TInfo *pInfo )
	{
		const int nKey = place.place.pos.p.GetData();            // the SPathPlace integer id
		unordered_map<int, TInfo>::iterator it = hash.find( nKey );
		if ( it != hash.end() )
			*pInfo = (*it).second;                               // cached evaluation
		else
		{
			pOwner->GetInfoInner( place, pInfo );                // compute once...
			hash[ nKey ] = *pInfo;                               // ...and memoize it
		}
	}
	// aiAttackLogic SActionInfo<TAction,TInfo>::CanDo instantiations (VA = RVA + 0x400000):
	//   @0x00425cf0 <CAIDockWithHGAction>  @0x00425e70 <CAIUndockFromHGAction> @0x00426aa0 <CAIBeginSnipeAction>
	//   @0x004270c0 <CAIShootFromHGAction> @0x00427230 <CAITerrorPKAction>     @0x004273c0 <CAIWearPKAction>
	bool CanDo( const SPlaceWithAP &place )
	{
		TInfo info;
		GetInfo( place, &info );
		return info.bCanDo;
	}
	void ResetInfoHash() { hash.clear(); }
	int operator&( CStructureSaver &f )
	{
		f.Add( 2, &pOwner );
		f.Add( 3, &hash );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AIACTIONBASE_H_
