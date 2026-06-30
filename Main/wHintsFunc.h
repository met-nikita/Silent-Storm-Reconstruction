#ifndef __WHINTSFUNC_H_
#define __WHINTSFUNC_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// wHintsFunc -- retail-new module (release compiland wHintsFunc.obj, no Jan03 counterpart).
// Scatters freshly created in-world hint pickups over the map. Additive parity surface: the
// hint-item factory is a deferred stub (this fork carries no in-world hint-item subsystem),
// so the placement is behaviour-neutral, but the module's source/types converge.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MapBuildingInfo.h"		// SMapPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class IAIMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
class CDebrisController;
////////////////////////////////////////////////////////////////////////////////////////////////////
// 40-byte map slot describing one scattered in-world hint pickup (retail ::SHintSlot).
// Its leading fields mirror SClueSlot so the same map data can feed both; fRotation sits at
// offset 0x18 and the slot stride is 0x28, matching the retail layout.
struct SHintSlot
{
	SMapPosition pos;
	CVec2 ptAlignTo;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// PlaceHintsToMap @0x35e840: for each slot create one hint item, drop it onto the surface
// under the slot position, rotate it by the slot angle (Z axis) and freeze it into the debris
// controller. Behaviour-faithful to retail; the hint-item factory is a deferred stub (see
// wHintsFunc.cpp), so with no item created the per-slot placement branch is skipped.
void PlaceHintsToMap( CDebrisController *pDebris, NAI::IAIMap *pMap, const vector<SHintSlot> &hints );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// FindClosePositionOnSurface @0x645b0: drop ptFrom straight down onto the highest surface below
// it via IAIMap::Trace (ray origin ptFrom, dir (0,0,-1), mask 0x8000). pRes.z becomes the max
// of (ptFrom.z - depth) over the non-negative hit depths; pRes stays == ptFrom when nothing is hit.
void FindClosePositionOnSurface( IAIMap *pMap, const CVec3 &ptFrom, CVec3 *pRes );
} // namespace NAI
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WHINTSFUNC_H_
