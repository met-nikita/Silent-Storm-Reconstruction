#ifndef __AIGUARDREACTION_H_
#define __AIGUARDREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiReaction.h"                 // NAI::CAIReaction (the reflex base)
#include "aiPosition.h"                 // NAI::SUnitPosition / SPathPlace / EPose
#include "aiActionPlaceSource.h"        // NAI::CUnitArea (the guarded area, a CObj member)
#include "../DBFormat/DataAnimation.h"  // CDBPtr<NDb::CAnimation>
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiGuardReaction (oracle: decomp/src/s2_aiguardreaction.h, all disasm-verified @0x50ef0..0x51810).
// The stand-and-watch reaction of a unit holding a position. It lazily floods a CUnitArea around the post,
// then each think: escalate to the take-cover (Defence) reaction when a fresh cover/attack pair exists;
// else investigate the most-pressing contact from inside the area (a SUSPECTED enemy wins only when it is
// much closer than the known one), hold + engage when a known enemy is up, glance toward a calling ally,
// regroup after combat, or walk back to the post when displaced; an idle guard shows a custom idle animation.
// Retail-NEW (no Jan03 counterpart); absent from the dev tree.
//
// TRANSIENT like the other dev reactions (CAINormalReaction/CAIDefenceReaction): registered with
// BASIC_REGISTER_CLASS (cast-only), NOT saveload-serialized. The release IS saveload-registered
// (REGISTER_SAVELOAD_CLASS id 0x52443110 + operator& @0x51810), but the dev CAIUnit does NOT serialize its
// reaction (aiUnit.cpp:65 -- pReaction is transient, rebuilt each think), so the id/operator& are moot here
// and not reproduced (the same simplification CAIDefenceReaction makes).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGuardReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIGuardReaction );
	CDBPtr<NDb::CAnimation> pGuardAnimation;  // +0x10  custom idle animation (map-placed guards; null for the cornered Normal->Guard handoff)
	CObj<CUnitArea>         pArea;            // +0x14  the guarded area (flooded lazily on the first Update)
	int                     nAPRadius;        // +0x18  AP radius of the area
	SUnitPosition           initialPos;       // +0x1c  the post (snapshotted from the unit's position when the area is built)
	bool                    bWasCombat;       // +0x28  saw a live enemy -> regroup (AfterCombat) once it is gone
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); f.Add( 3, &pGuardAnimation ); f.Add( 4, &pArea ); f.Add( 5, &nAPRadius ); f.Add( 6, &initialPos ); f.Add( 7, &bWasCombat ); return 0; }   // retail @0x51810
public:
	CAIGuardReaction(): nAPRadius( 0 ), bWasCombat( false ) {}
	CAIGuardReaction( IAIUnit *pUnit, NDb::CAnimation *pGuardAnimation, int nRadius );   // @0x50f40
	//
	virtual void Update();   // @0x510e0 -- the stand-and-watch dance
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIGuardReaction @0x50ef0 -- a live unit -> a new reaction; the area is deferred to the first
// Update, the post starts at the invalid place, a negative radius clamps to 0. (The release factory carries
// a 4th `bUnitAlive` arg precomputed by the caller; folded into IsValid(pUnit) here, like CreateAIDefenceReaction.)
CAIReaction* CreateAIGuardReaction( IAIUnit *pUnit, NDb::CAnimation *pGuardAnimation, int nRadius );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIGUARDREACTION_H_
