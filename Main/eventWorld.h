#ifndef __EVENT_WORLD_H__
#define __EVENT_WORLD_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventOnSegment -- broadcast once per world Segment() tick (release: the command-pump per-segment
// broadcast, type_info @0x951da0). Carries nothing; subscribers (NAI::CAIScriptLogic) just count segments
// elapsed within a turn. Thrown from CWorld::Segment() (wMain.cpp), mirroring the release. The dev tree had
// no per-segment global event before this (only the per-turn CEventOnNewPlayerTurn in eventPlayer.h).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnSegment
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif // __EVENT_WORLD_H__
