#ifndef __AIROUTELOGIC_H_
#define __AIROUTELOGIC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiLogic.h"         // NAI::CAILogic (the AI-logic base); brings SPosition/EPose via aiPosition.h
#include "aiTaskCommand.h" // NAI::CTaskCommand (the reused route-command family base) + LookToPosition's home
//
namespace NWorld { class CUnitServer; }
//
namespace NAI
{
class IAIUnit;
class CUnitArea;   // aiActionPlaceSource.h -- the reachable-area gate for CreateAICheckPositionLogic
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiRouteLogic -- the route-following AI logic (oracle: decomp/src/s2_routelogic.h). The release
// NAI::CAIRouteLogic IS the dev tree's CTask command-list walker (aiTaskCommander.h) RE-PARENTED as a
// CAILogic, so a reaction can install it on a unit via SetLogic. It walks a vector of CTaskCommand steps;
// each step's GetCommand() yields a unit CCmd which is wrapped in CCmdSetCommand and fed to the unit's
// world-command queue (the same CCmd->CCommand bridge CAITaskCommander::GetCommand uses).
//
// This REUSES the existing CTaskCommand command family (whose saveload ids already match the release
// exactly) -- only this logic wrapper + the CreateAI*Logic factories are release-new. The predecessor
// CTask walker (a CObjectBase driven by CAITaskCommander) stays as-is.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRouteLogic: public CAILogic
{
	OBJECT_BASIC_METHODS( CAIRouteLogic );
	ZDATA
	bool                            bCircled;         // the route loops back to the start
	vector< CObj<CTaskCommand> >    routeCommands;    // the ordered route steps
	int                             nCurrentCommand;  // -1 before the first step
	bool                            bContinueCommand; // resume (don't advance) the current step -- set on Pause
	ZEND int operator&( CStructureSaver &f );         // id 0x51443110 (defined in the .cpp)
public:
	CAIRouteLogic(): bCircled( false ), nCurrentCommand( -1 ), bContinueCommand( false ) {}
	CAIRouteLogic( IAIUnit *pUnit ): CAILogic( pUnit ), bCircled( false ), nCurrentCommand( -1 ), bContinueCommand( false ) {}
	//
	void SetCircled( bool _bCircled ) { bCircled = _bCircled; }
	bool IsCircled() const { return bCircled; }   // a circled route never Finish()es -- the squad-alarm guard excludes it
	void AddCommand( CTaskCommand *pCmd );   // append a route step (wires nothing; the factory sets the server)
	//
	// CAILogic overrides
	virtual void GenerateCommand();   // @0x9a170 -- advance the route + queue the next wrapped command
	virtual bool IsEndOfTurn();       // @0x98c50
	virtual void Pause();             // @0x98d10 (PauseInner) -- rewind to the last Goto so resume re-walks it
private:
	bool IsCommandInRange() const { return nCurrentCommand > -1 && nCurrentCommand < (int)routeCommands.size(); }
	bool IsCurrentCommandFinished();  // @0x98f80
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandLookToPosition (release CRouteCommandLookToPosition, id 0x52443111) -- a release-new route
// command (decode home: aiRouteLogic.obj). Faces a stored target PLACE: at Do() time it asks the path
// network for the closest spoke direction from the unit's CURRENT place toward the target, folds it into
// the current place, and queues a CCmdLook. Contrast the dev CTaskCommandChangeDirection, which turns to a
// FIXED direction -- this one re-resolves the direction toward a target place each time. Reuses the
// CTaskCommand base + saveload idiom. (Faithfulness note: the release creator passes a constant second
// ctor arg that Do() never reads -- behaviourally inert, so it is not modelled.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandLookToPosition: public CTaskCommand
{
	OBJECT_BASIC_METHODS( CTaskCommandLookToPosition );
	ZDATA
	ZPARENT( CTaskCommand )
	SPathPlace pos;   // the target place to face
	ZEND int operator&( CStructureSaver &f );   // id 0x52443111 (defined in the .cpp)
public:
	CTaskCommandLookToPosition() {}
	CTaskCommandLookToPosition( const SPathPlace &_pos ): CTaskCommand(), pos( _pos ) {}
	//
	virtual void Do();   // @0x99840
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandHide (release CRouteCommandHide, id 0x23068341) -- a release-new route command (decode home
// aiRouteLogic.obj). Queues a CCmdHide so the unit takes cover (a prone/crouched STANCE toggle performed by
// the in-engine CExecHide handler -- there is NO world cover-finder), but only when the unit can fight, is
// not already hidden, scenario player 0 (the human player) is its ENEMY, and the engine accepts the hide.
// Carries no own fields (decode size 32 == the CTaskCommand base). See the .cpp for the answer-key mislabel.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskCommandHide: public CTaskCommand
{
	OBJECT_BASIC_METHODS( CTaskCommandHide );
	ZDATA
	ZPARENT( CTaskCommand )
	ZEND int operator&( CStructureSaver &f );   // id 0x23068341 (defined in the .cpp)
public:
	CTaskCommandHide() {}
	//
	virtual void Do();   // @0x99b40
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskCommandAlarm (release CRouteCommandAlarm @0x99990, id 0x23062480) -- the route step that RAISES THE GARRISON.
// On arrival beside an ally it re-resolves its stored enemy server to an IAIUnit, tells every ally within 5 m about it
// (each ally AddPossibleEnemy(enemy) via CreateAIPossibleEnemyEvent), and raises the alarming unit's own help flag
// (CreateAIHelpCalledEvent). IAIUnit exposes no Notify slot, so the per-unit events are delivered by event->Modify(state)
// synchronously -- the identical SAIUnitState mutation (the dev's own idiom, cf. aiReactions.cpp RemovePossibleEnemy).
class CTaskCommandAlarm: public CTaskCommand
{
	OBJECT_BASIC_METHODS( CTaskCommandAlarm );
	ZDATA
	ZPARENT( CTaskCommand )
	CPtr<NWorld::CUnitServer> pEnemy;   // release CRouteCommandAlarm::pEnemy (serialization-stable; re-resolved at Do)
	ZEND int operator&( CStructureSaver &f );   // id 0x23062480 (defined in the .cpp)
public:
	CTaskCommandAlarm() {}
	CTaskCommandAlarm( NWorld::CUnitServer *_pEnemy ): CTaskCommand(), pEnemy( _pEnemy ) {}
	//
	virtual void Do();   // @0x99990
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateAIRouteLogic @0x9a680 -- a live unit + a non-empty command list, else 0. Wires the unit's server
// into every command (CTaskCommand::SetUnitServer).
CAIRouteLogic* CreateAIRouteLogic( IAIUnit *pUnit, const vector< CPtr<CTaskCommand> > &cmds, bool bCircled );
// CreateAIStrafeToPositionLogic @0x9b8d0 -- strafe to `pos`: gated on a path to it at the wish pose, then
// [change wish pose, change end pose, goto]. Not circled. (See the .cpp for the two documented dev<->release
// divergences: the strafe prefix + the ChangeWishPose/ChangePose split.)
IAILogic* CreateAIStrafeToPositionLogic( IAIUnit *pUnit, const SPosition &pos, EPose wishPose, EPose endPose );
////////////////////////////////////////////////////////////////////////////////////////////////////
// The composite route-logic factories (oracle: decomp/src/s2_routelogic.h ~880-1148). Each builds a
// CTaskCommand step list and wraps it in a CAIRouteLogic. They are dead code until a reaction/commander
// SetLogic's one (like CAIDefenceReaction), so landing them changes no live behaviour.
//
// CreateAILookToPositionLogic @0x9a920 -- face `place`, then wait 3s. Not circled.
IAILogic* CreateAILookToPositionLogic( IAIUnit *pUnit, const SPathPlace &place );
// CreateAIMoveToPositionLogic @0x9aad0 -- gated on a path to `place` at movePose; queues [look at it,
// wish-pose for the move, goto, end-pose]. Not circled. Inherits the strafe factory's two divergences
// (ChangeWishPose~=ChangePose; the goto's strafe prefix elided -- absent GetAttackObject).
IAILogic* CreateAIMoveToPositionLogic( IAIUnit *pUnit, const SPathPlace &place, EPose movePose, EPose endPose, bool bCanFindNotExactPath );
// CreateAIAlarmLogic @0x9b3d0 -- a scared, supported unit RUNs to a place beside its nearest ally, then the
// CTaskCommandAlarm step raises the garrison. Gated on a runnable path to that place; else 0.
IAILogic* CreateAIAlarmLogic( IAIUnit *pUnit, IAIUnit *pEnemy );
// CreateAILookRoundLogic @0x9ad50 -- change to `pose`, then a randomized look-around (nCount=6). Circled.
IAILogic* CreateAILookRoundLogic( IAIUnit *pUnit, EPose pose );
// CreateAIRoamingLogic @0x9aec0 -- one roam step + a short look-around, looping. Circled.
IAILogic* CreateAIRoamingLogic( IAIUnit *pUnit, const SPathPlace &center, int nRadius );
// CreateAIHideLogic @0x9b7b0 -- a single hide command (the unit takes cover). Not circled.
IAILogic* CreateAIHideLogic( IAIUnit *pUnit );
// CreateAICheckPositionLogic @0x9afb0 -- inspect `pos` (a noise/clue spot). Optional hide prefix; if a reachable
// check spot exists inside `pArea` (GetCheckPosition + HasPath) walk there facing it; else, only when the unit has
// no enemy, look + crouch (unless in a PK) + a jittered glance. Not circled. Needs GetCheckPosition (aiRouteMisc)
// + GetHideProbability (CAIUnit). See the .cpp for the two answer-key mislabels corrected from the disasm.
IAILogic* CreateAICheckPositionLogic( IAIUnit *pUnit, CUnitArea *pArea, const SPosition &pos, EPose pose );
// CreateAICheckForEnemyLogic @0x9a710 -- "go check it out": an optional hide-probability prefix, then the
// RouteAddRoundUp flanking script against `enemyPos` (silent when the enemy is not currently audible). Builds a
// logic only if the round-up queued anything (then a trailing 6s wait). Not circled. The release `IsAudible`
// reach is the CAudibleSet membership query pUS->IsAudible(enemyServer) (NOT a hearing-probability roll).
IAILogic* CreateAICheckForEnemyLogic( IAIUnit *pUnit, const SUnitPosition &enemyPos, EPose pose, IAIUnit *pEnemy );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIROUTELOGIC_H_
