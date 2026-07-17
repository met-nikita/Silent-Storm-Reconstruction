#ifndef __AILOGIC_H_
#define __AILOGIC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release AI behaviour layer base - IAILogic (command-driven interface) + CAILogic (base). Structural
// port (Approach A). SUPERSEDES the dev aiCompoundAction.h CAILogic (dev: CAILogic : CAIJob, job-based
// DoJob/Think/MakeDecision). Held in this new file so the dev build stays green; the dev CAILogic is
// superseded at the phase-7 swap.
//
// Verified from the release Game.exe. CAILogic uses VIRTUAL inheritance of CObjectBase via IAILogic
// (the ctor stores the vbtable displacement -0x3c). IAILogic vtable (14 slots) and CAILogic members +
// operator& chunk tags are authoritative (decompile). The concrete combat logics multiply-inherit
// CAILogic + CAIJob (see aiCombatLogic.h).
//
// operator& chunk order: tag2 nPause, tag3 pUnit, tag4 commands, tag5 bFinished, tag6 cyclingTracker,
// tag7 bHasPointOfInterest, tag8 vPointOfInterest.
//
// WIP - NOT yet in Main.vcxproj.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiActionBase.h"  // SPlaceWithAP, IAIUnit (new substrate)
#include "aiPosition.h"    // SUnitPosition, SPathPlace
namespace NWorld { class CCommand; class CUnitServer; class IWorld; }
namespace NAI
{
class IAIUnit;
struct SAIState;
////////////////////////////////////////////////////////////////////////////////////////////////////
// anti-cycling tracker: remembers the unit's last place+AP to detect a logic looping in place.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCyclingTracker
{
	SPlaceWithAP place;
	int nSame;
	SCyclingTracker() : nSame( 0 ) {}
	void Init( const SPlaceWithAP &p ) { place = p; nSame = 0; }
	// retail NAI::SCyclingTracker::operator& @0x18410: {2 place (SPlaceWithAP chunk), 3 nSame int4}.
	// CAILogic serializes the tracker as ONE nested tag-6 chunk (33 bytes in the retail saves).
	int operator&( CStructureSaver &f ) { f.Add( 2, &place ); f.Add( 3, &nSame ); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAILogic - abstract command-driven behaviour interface (14-slot vtable, verified).
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAILogic: public virtual CObjectBase
{
public:
	virtual NWorld::CCommand* GetCommand() = 0;                // slot0
	virtual bool IsEndOfTurn() = 0;                            // slot1
	virtual bool IsNeedToThink() = 0;                          // slot2
	virtual void Think() = 0;                                  // slot3
	virtual bool IsThinking() = 0;                             // slot4
	virtual void StopThinking() = 0;                           // slot5
	virtual bool IsFinished() = 0;                             // slot6
	virtual void Pause() = 0;                                  // slot7
	virtual void Resume() = 0;                                 // slot8
	virtual bool IsActive() const = 0;                        // slot9
	virtual bool GetPointOfInterest( CVec3 *pOut ) const = 0;  // slot10
	virtual void GenerateCommand() = 0;                        // slot11
	virtual void OnNewTurn() = 0;                              // slot12
	virtual void OnSegment() = 0;                              // slot13
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogic - base command-driven logic. Members authoritative (decompile).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogic: public IAILogic
{
	OBJECT_BASIC_METHODS( CAILogic );
public:
	CPtr<IAIUnit> pUnit;
	list< CPtr<NWorld::CCommand> > commands;
	bool bFinished;
	SCyclingTracker cyclingTracker;
	CVec3 vPointOfInterest;
	bool bHasPointOfInterest;
	int nPause;
	//
	CAILogic();
	CAILogic( IAIUnit *pUnit );
	//
	virtual NWorld::CCommand* GetCommand();  // pops next command (anti-cycling check)
	virtual bool IsEndOfTurn();
	virtual bool IsNeedToThink();            // base: false
	virtual void Think();                    // base: no-op
	virtual bool IsThinking();               // base: false
	virtual void StopThinking();             // base: no-op
	virtual bool IsFinished();
	virtual void Pause();
	virtual void Resume();
	virtual bool IsActive() const;
	virtual bool GetPointOfInterest( CVec3 *pOut ) const;
	virtual void GenerateCommand();          // base: no-op
	virtual void OnNewTurn() {}              // base: no-op
	virtual void OnSegment() {}              // base: no-op
	//
	void Finish();
	bool HasCommands() const;
	void ClearCommands();
	void DoCommand( NWorld::CCommand *pCmd );
	bool CanGetCommand() const;
	void CheckCycling();
	//
	IAIUnit*             GetUnit() const;
	NWorld::CUnitServer* GetUnitServer() const;
	NWorld::IWorld*      GetWorld() const;
	SAIState*            GetAIState() const;
	//
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif // __AILOGIC_H_
