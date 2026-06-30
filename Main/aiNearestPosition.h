#ifndef __AINEARESTPOSITION_H_
#define __AINEARESTPOSITION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class IPathNetwork;
SPosition GetNearestPosition( CVec3 ptPos, IPathNetwork *pPathNetwork,
	bool bMustHaveLink = false, const CVec3 &ptLink = CVec3(), bool bNative = false );
// retail @0x7ef80: nearest NATIVE-passable cell (accepts cells passable on the static native grid even
// if dynamically blocked). Thin wrapper over GetNearestPosition with bNative=true, no link probe.
SPosition GetNearestNativePosition( CVec3 ptPos, IPathNetwork *pPathNetwork );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif