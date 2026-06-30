#ifndef __AISMOOTHPATH_H_
#define __AISMOOTHPATH_H_
#if _MSC_VER > 1000
#pragma once
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiPath.h"   // NAI::CPath, SPathPlace, IPathNetwork
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
// aiSmoothPath (retail-new, @0xa3aa0..0xa40f0) -- a post-pass over a found CPath that
// replaces L-shaped corners (a straight run, a turn, another straight run) with a
// Bresenham-style interleave of the two step vectors, so units cut diagonals instead of
// walking the grid axes. Reconstructed from the disasm-verified decode; de-hooked onto the
// dev tree: the release per-place tri-state GetPassability(EPassable) becomes the present
// IPathNetwork::IsPassable, and the free GetTransitionType(IPathNetwork*,...) is used
// directly. The release dynamic_casts pNet to CPathNetwork; smoothing only needs the
// IPathNetwork interface + the free transition query, so we operate on IPathNetwork directly.
bool NotSmooth( const vector<SPathPlace> &pts, int i );
int  FindMaxPreSize( const vector<SPathPlace> &pts, int i );
int  FindMaxPostSize( const vector<SPathPlace> &pts, int i );
bool SmoothenPrePost( vector<SPathPlace> &pts, IPathNetwork *pNet, int nPre, int nPost, int i );
bool SmoothenPathIter( CPath &path );
void SmoothenPath( CPath &path );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
