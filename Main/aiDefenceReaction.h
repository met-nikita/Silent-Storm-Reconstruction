#ifndef __AIDEFENCEREACTION_H_
#define __AIDEFENCEREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiReaction.h"    // NAI::CAIReaction (the reflex base)
#include "aiPosition.h"    // NAI::SUnitPosition / SPathPlace
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiDefenceReaction (oracle: decomp/src/s2_aidefencereaction.h, all disasm-verified @0x3a270..0x3b4d0).
// The take-cover reaction: find a fully covered CROUCH spot next to a firing position, then alternate
// between hiding there and popping out to shoot. Retail-NEW (no Jan03 counterpart); absent from the dev
// tree. Reconstructed onto the now-present substrate: GetCoveredPosition uses GetNearestPlaces (aiRouteMisc)
// + NWorld::FindPath + IGame::CheckPositionVisibility; the cover-dance installs CreateAIStrafeToPositionLogic
// (aiRouteLogic) / CreateAIDefenceLogic (aiCombatLogic) on the unit's AP budget.
//
// Like the other dev reactions (CAINormalReaction/CAIRetreatReaction) this is TRANSIENT -- registered with
// BASIC_REGISTER_CLASS (cast-only), NOT saveload-serialized; the release id 0x2306ab00 + operator& are not
// reproduced (the dev unit rebuilds its reaction each think; pUnit is a non-serialized weak back-ref). The
// reaction is NOT yet wired into the commander's reaction-selection (that needs the Tier-A threat brain) --
// it is build-verifiable + complete, awaiting the selection hookup.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIDefenceReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIDefenceReaction );
	SUnitPosition       coveredPos;       // +0x10  the full-cover spot
	SUnitPosition       attackPos;        // +0x1c  the pop-out firing spot
	int                 nAPForTakeCover;  // +0x28  AP to move cover->attack (the pop-out price)
	int                 nPrevAP;          // +0x2c  anti-cycling: AP at the last defence-logic install
	bool                bJustStarted;     // +0x30
	CObj<CAIReaction>   pPrevReaction;    // +0x34  the reaction to fall back to when the plan dies
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); f.Add( 3, &coveredPos ); f.Add( 4, &attackPos ); f.Add( 5, &nAPForTakeCover ); f.Add( 6, &nPrevAP ); f.Add( 7, &bJustStarted ); f.Add( 8, &pPrevReaction ); return 0; }   // retail @0x3b630
public:
	CAIDefenceReaction(): nAPForTakeCover( 0xffff ), nPrevAP( 0xffff ), bJustStarted( true ) {}
	CAIDefenceReaction( IAIUnit *pUnit, CAIReaction *pPrevReaction );   // @0x3b4d0 (plans on construction)
	//
	virtual void Update();   // @0x3b160 -- the cover dance
private:
	void StrafeToCover( IAIUnit *pU, int nAP );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIDefenceReaction @0x3af10 -- a live unit + a live fall-back reaction, then plan immediately.
CAIReaction* CreateAIDefenceReaction( IAIUnit *pUnit, CAIReaction *pPrevReaction );
// NAI::CanUseDefenceReaction @0x3add0 -- a live known enemy + a fresh cover/attack pair that passes the probe.
bool CanUseDefenceReaction( IAIUnit *pUnit );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIDEFENCEREACTION_H_
