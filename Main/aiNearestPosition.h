#ifndef __AINEARESTPOSITION_H_
#define __AINEARESTPOSITION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IPathNetwork;
struct SUnitPosition;
enum ENearestUnitPosResult
{
	NPR_MOVE,
	NPR_FALL,
	NPR_FAILED
};
// Retail v1.1 0x47e500 / v1.2 0x47e530: unit-aware relocation within 3m.
// Preserve pose/facing, require movement exits, and never choose a surface above fHeight.
ENearestUnitPosResult LookWhereToMoveUnit( const SUnitPosition &from, SUnitPosition *pTo,
	float fMaxFallDist, float fHeight );
SPosition GetNearestPosition( CVec3 ptPos, IPathNetwork *pPathNetwork,
	bool bMustHaveLink = false, const CVec3 &ptLink = CVec3(), bool bNative = false );
// retail @0x7ef80: nearest NATIVE-passable cell (accepts cells passable on the static native grid even
// if dynamically blocked). Thin wrapper over GetNearestPosition with bNative=true, no link probe.
SPosition GetNearestNativePosition( CVec3 ptPos, IPathNetwork *pPathNetwork );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
