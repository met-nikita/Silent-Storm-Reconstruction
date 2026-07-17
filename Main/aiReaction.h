#ifndef __AIREACTION_H_
#define __AIREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release AI reaction layer (aiReaction.obj). A "reaction" is the per-unit reflex above the active logic:
// each think the unit's reaction Update()s, inspects the situation, and may swap the unit's logic via
// SetLogic(). The release chooses ALL combat logics this way (xref: CreateAIAttackLogic <-
// CAINormalReaction::Update @0x0047f190, Defence/Guard/Retreat <- their reactions) - there is no
// ChooseLogic in the commander.
//
// SCOPE: this ports the framework + the core of CAINormalReaction. The release's deeper machinery - the
// per-unit threat state SAIUnitState (pEnemy/pPossibleEnemy/pAlly/bScared), the event system, and the
// sibling reactions (Defence/Assassin/Guard/Retreat/Fear) - is the absent Tier-A threat-tracker brain and
// is NOT ported yet; CAINormalReaction here drives the Attack/AfterCombat path off the commander's
// existing enemy detection (pAIState->GetCurrentAIEnemy), with bWasCombat state.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld { class CUnitServer; }
struct SMapUnit;   // MapBuild.h (global) -- the map-prescribed unit spec ( eLogic / nRoamingRadius / pGuardAnimation )
namespace NAI
{
class IAIUnit;
class IAILogic;
struct SAIState;
struct SAIUnitState;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIReaction - abstract reflex behaviour. Layout: [CObjectBase][pUnit @0xc] (a weak back-ref to the
// owning unit). Retail serializes the reaction base as a nested chunk {tag 2 = pUnit ref} (the
// CAILogRecord chunk every concrete reaction's operator& writes at its own tag 2, e.g. @0x7f780) --
// pUnit is a CPtr and this operator& reproduces that chunk (serialization-convergence Wave 2; the
// old dev shape was a raw unserialized IAIUnit*).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIReaction: public virtual CObjectBase   // virtual base (like IAILogic) so CObj<CAIReaction> needs no base cast-registration
{
public:
	int operator&( CStructureSaver &f ) { f.Add( 2, &pUnit ); return 0; }
protected:
	CPtr<IAIUnit> pUnit;
	//
	IAIUnit*             GetUnit() const { return pUnit; }
	SAIUnitState*        GetAIUnitState() const;         // pUnit->GetAIUnitState()  (release vtbl 0x74)
	SAIState*            GetAIState() const;             // pUnit->GetAIState()
	bool                 SetLogic( IAILogic *pLogic );   // pUnit->SetLogic(); false if the unit is gone
	NWorld::CUnitServer* GetUnitServer() const;          // pUnit->GetUnitServer()
public:
	CAIReaction(): pUnit( 0 ) {}
	CAIReaction( IAIUnit *_pUnit ): pUnit( _pUnit ) {}
	//
	virtual void Update() = 0;                           // the one behaviour hook
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateUnitReaction @0x919d0 -- install the reaction the map prescribes (sMapUnit.eLogic) onto the
// unit's AI object (pUnit->SetReaction, release vtbl 0x64): UL_EMPTY -> guard, UL_DEFAULT/UL_ROAMING ->
// normal, UL_FEAR -> fear; a dead/missing unit, AI object, or reaction is a no-op. Release-NEW: the live
// install site (CreateUnitAI/CreateUnitRoute) is the separate aiCommander comp, so this lands dead.
void CreateUnitReaction( NWorld::CUnitServer *pUnitServer, const SMapUnit &sMapUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AIREACTION_H_
