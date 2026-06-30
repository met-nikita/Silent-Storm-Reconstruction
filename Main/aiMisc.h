#ifndef __AIMISC_H_
#define __AIMISC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "aiPosition.h"      // NAI::SPathPlace, EPose, IPathNetwork
//
namespace NWorld { class CUnitServer; class IPlayer; }
//
namespace NAI
{
class IAIUnit;
class CAICommander;
class CPath;                 // NAI::CPath (the NWorld::FindPath result)
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiMisc -- the release-new NAI world-glue helpers (oracle: decomp/src/s2_aimisc.h, all disasm-
// verified @0x73df0..0x74700). These are the path-AP / move utilities the whole tactical-AI layer
// reaches (HasPath / GetAPForMove / GetPathAP / IsNullAPPath / CanAIOperateThisUnit) plus the
// server->AI-unit resolver (GetAICommander / GetAIUnit). They are release-NEW (MISSING_IN_DEV) and
// purely additive: reconstructed onto the in-tree path infrastructure (NWorld::FindPath, the local
// move-AP accumulator, the wish-pose pose-override pair) -- no new subsystem.
//
// The release aiMisc.obj also exports NAI::IsAIPlayer @0x73df0 -- now reconstructed below. The dyncast
// TARGET it needs, NAI::CSequenceCommander, has landed in aiCommander.h (a thin CAICommander subclass).
// It tells an AI side from a scripted/human one: a live player whose commander is NOT a CSequenceCommander
// is AI-driven.
////////////////////////////////////////////////////////////////////////////////////////////////////

// NAI::GetPathAP @0x740a0 -- price a whole path with the move-AP accumulator (the release uses
// NWorld::CPathAPCalcer; the dev twin is the file-local accumulator). Starts AND first-steps at
// points[0] (a same-place hop). Invalid/empty path or server -> 0.
int GetPathAP( NWorld::CUnitServer *pUS, CPath *pPath );

// NAI::IsNullAPPath @0x741b0 -- true while the running path cost never goes positive (a missing or
// empty path is trivially free -> true, NOT false).
bool IsNullAPPath( NWorld::CUnitServer *pUS, CPath *pPath );

// NAI::HasPath @0x74360 -- is there a path from `from` to `to`, priced at wish-pose `nPose` (the pose
// is forced around NWorld::FindPath and restored after)? `bCanFindNotExactPath` passes through to
// FindPath (accept a path that does not exactly reach the target).
bool HasPath( IAIUnit *pUnit, const SPathPlace &from, const SPathPlace &to, EPose nPose,
	bool bCanFindNotExactPath );

// NAI::GetAPForMove @0x74520 -- the AP to move from `from` to `to` at wish-pose `nPose`, priced via
// GetPathAP on the probe path; 0xffff on ANY failure (dead unit/server/net or no path).
int GetAPForMove( IAIUnit *pUnit, const SPathPlace &from, const SPathPlace &to, EPose nPose );

// NAI::CanAIOperateThisUnit @0x74700 -- the unit is alive, AI-driven (release IAIUnit vtbl 0x1c
// IsAIUnit -> the dev-native IsUnderAIControl), and its server can fight.
bool CanAIOperateThisUnit( IAIUnit *pUnit );

// NAI::IsAIPlayer @0x73df0 -- is this a live AI-controlled side? True iff the player's commander is NOT a
// CSequenceCommander (the scripted/human flavour). Faithful to the release: only the cast's null-ness is
// tested (no alive-bit gate on the cast result, unlike GetAICommander's commander check).
bool IsAIPlayer( NWorld::IPlayer *pPlayer );

// NAI::GetAICommander @0x73ea0 -- the unit server's player's commander, dyncast to CAICommander
// (0 if the side is not AI-commanded). One release-only gate is elided -- see the .cpp.
CAICommander* GetAICommander( NWorld::CUnitServer *pUS );

// NAI::GetAIUnit @0x742c0 -- the AI-unit wrapper for a server, via its AI commander's server->unit map
// (CAICommander::GetAIUnit). 0 when the side is not AI-commanded (e.g. a human-controlled unit).
IAIUnit* GetAIUnit( NWorld::CUnitServer *pUS );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __AIMISC_H_
