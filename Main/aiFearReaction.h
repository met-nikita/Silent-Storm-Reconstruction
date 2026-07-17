#ifndef __AIFEARREACTION_H_
#define __AIFEARREACTION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiReaction.h"   // NAI::CAIReaction (the reflex base)
//
namespace NAI
{
class IAIUnit;
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiFearReaction (oracle: decomp/src/s2_aifearreaction.h; CreateAIFearReaction @0x3cab0 + ctor @0x3ccb0
// + Update @0x3ce10, all disasm-verified). The panic reaction of a unit broken by morale or flagged UL_FEAR.
// Each think: a live known enemy -> flee toward a place away from the crowd (a hidden one first when
// bUseCover), crouch-watching if already standing on it; only a SUSPECTED contact -> glance toward it from
// where it stands; nobody about -> a roaming civilian wanders nRadius, everyone else crouches and looks round.
// Installed from script by UnitSetFearLogic / UnitSetPanicLogic (the cover-flee variant: bUseCover=true,
// bRoaming=false, nRadius=0 -- the two commands are byte-identical in retail) and UnitSetCivilianLogic (the
// roaming-civilian variant: bUseCover=false, bRoaming=true, nRadius=script arg). Retail-NEW; absent from the
// dev tree.
//
// TRANSIENT like the other dev reactions (CAINormalReaction / CAIGuardReaction / CAIDefenceReaction):
// registered with BASIC_REGISTER_CLASS (cast-only), NOT saveload-serialized. The release IS saveload-
// registered (REGISTER_SAVELOAD_CLASS id 0x53043120 + operator& @0x3d080), but the dev CAIUnit does not
// serialize its reaction (aiUnit.cpp: pReaction is transient, rebuilt each think), so the id/operator& are
// moot here and not reproduced -- the same simplification CAIGuardReaction makes.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFearReaction: public CAIReaction
{
	OBJECT_BASIC_METHODS( CAIFearReaction );
	bool bUseCover;   // +0x10  prefer a hidden place (GetSafePosition) before the run-away place when fleeing
	bool bRoaming;    // +0x11  when calm, wander nRadius rather than just looking round (civilians)
	int  nRadius;     // +0x14  the roam radius
	int operator&( CStructureSaver &f ) { f.Add( 2, (CAIReaction*)this ); f.Add( 3, &bUseCover ); f.Add( 4, &bRoaming ); f.Add( 5, &nRadius ); return 0; }   // retail @0x3d080
public:
	CAIFearReaction(): bUseCover( false ), bRoaming( false ), nRadius( 0 ) {}
	CAIFearReaction( IAIUnit *pUnit, bool _bUseCover, bool _bRoaming, int _nRadius )
		: CAIReaction( pUnit ), bUseCover( _bUseCover ), bRoaming( _bRoaming ), nRadius( _nRadius ) {}
	//
	virtual void Update();   // @0x3ce10
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateAIFearReaction @0x3cab0 -- a live unit -> a new fear reaction. (The release factory carries a
// 5th `bUnitAlive` arg the CALLER precomputes as IsValid(pUnit); it is folded into IsValid(pUnit) here, like
// CreateAIGuardReaction / CreateAIDefenceReaction.)
CAIReaction* CreateAIFearReaction( IAIUnit *pUnit, bool bUseCover, bool bRoaming, int nRadius );
bool IsFearReaction( CAIReaction *p );   // @0x0003ca80 -- RTTI probe
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIFEARREACTION_H_
