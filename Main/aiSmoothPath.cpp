#include "StdAfx.h"
#include "aiSmoothPath.h"
#include "aiMoves.h"   // GetTransitionType( const IPathNetwork*, const SPathPlace&, const SPathPlace& )
#include <math.h>

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3ab0: is point i a corner? Compares the backward differences around i per axis; an axis is
// "smooth" when its delta repeats and the point actually moves along it. The inner equal-deltas
// recheck catches the all-three-points-coincide case.
bool NotSmooth( const vector<SPathPlace> &pts, int i )
{
	int nDXF = (int)pts[i].nX - (int)pts[i + 1].nX;
	int nDXB = (int)pts[i - 1].nX - (int)pts[i].nX;
	int nDYF = (int)pts[i].nY - (int)pts[i + 1].nY;
	int nDYB = (int)pts[i - 1].nY - (int)pts[i].nY;
	// @0xa3ab0: an axis is a "corner axis" when its backward delta CHANGES, OR the point actually MOVES along
	// that axis (`!=`). FIX (2026-07-01): the original reconstruction used `==` here, which inverted the
	// movement test -> NotSmooth only fired on full 90-degree L-corners (both axes reverse) and MISSED the
	// horizontal->diagonal junction (one axis keeps moving), so SmoothenPath left the dog-leg uncut. The retail
	// disasm is `!=` on both axes.
	if ( ( nDXF != nDXB || pts[i].nX != pts[i + 1].nX ) &&
	     ( nDYF != nDYB || pts[i].nY != pts[i + 1].nY ) )
	{
		if ( nDXF == nDXB && nDYF == nDYB )
			return false;
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Only walking steps may be redistributed by smoothing. Retail also accepts runs
// of stationary pose/turn points; interleaving those with movement can put a pose
// change on another tile. ConvertPath then emits only ChangePose for that edge,
// leaving the main animation at the old tile while the logical/idle pose advances.
static bool IsSmoothableStep( const SPathPlace &a, const SPathPlace &b )
{
	return a.nIntegral && b.nIntegral && !a.nFinal && !b.nFinal &&
		a.nLayer == b.nLayer && a.nPose == b.nPose && a.nPose != CM_INACTIVE &&
		( a.nX != b.nX || a.nY != b.nY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3b20: length of the straight run ENDING at i. Stop at stationary actions,
// pose changes and navigation-layer boundaries as well as a changed step vector.
int FindMaxPreSize( const vector<SPathPlace> &pts, int i )
{
	if ( !IsSmoothableStep( pts[i - 1], pts[i] ) )
		return 0;
	int nDX = (int)pts[i].nX - (int)pts[i - 1].nX;
	int nDY = (int)pts[i].nY - (int)pts[i - 1].nY;
	int j = i - 1;
	while ( j > 0 )
	{
		if ( !IsSmoothableStep( pts[j - 1], pts[j] ) ||
		     (int)pts[j].nX - (int)pts[j - 1].nX != nDX ||
		     (int)pts[j].nY - (int)pts[j - 1].nY != nDY )
			break;
		--j;
	}
	return i - j;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3be0: length of the straight run STARTING at i (forward steps), stopping one short of the
// path's end; the same action boundaries apply as for the backward scan.
int FindMaxPostSize( const vector<SPathPlace> &pts, int i )
{
	if ( !IsSmoothableStep( pts[i], pts[i + 1] ) )
		return 0;
	int nDX = (int)pts[i + 1].nX - (int)pts[i].nX;
	int nDY = (int)pts[i + 1].nY - (int)pts[i].nY;
	int j = i + 1;
	int nLast = (int)pts.size() - 1;
	while ( j < nLast )
	{
		if ( !IsSmoothableStep( pts[j], pts[j + 1] ) ||
		     (int)pts[j + 1].nX - (int)pts[j].nX != nDX ||
		     (int)pts[j + 1].nY - (int)pts[j].nY != nDY )
			break;
		++j;
	}
	return j - i;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa3cc0: replace the span (i-nPre, i+nPost-1) -- exclusive ends -- with an interleave of nPre-1
// "pre" steps and nPost-1 "post" steps, scheduled proportionally (epsilon 0.01; ties prefer the
// longer remainder). Each generated point takes the stepped coords, point i's layer, and the
// pose/moving bits of the original point it replaces; the previous point's direction is set to the
// step's direction. The candidate sequence must be all-passable, internally connected, and
// connected to both untouched neighbours; otherwise the LONGER side shrinks by one (pre also slides
// the span start) and it retries while both sides still have >= 2 steps. The binary builds the
// candidates in a fixed 1000-place scratch with no bounds check (kept).
bool SmoothenPrePost( vector<SPathPlace> &pts, IPathNetwork *pNet, int nPre, int nPost, int i )
{
	SPathPlace buf[1000];
	int nStart = i - nPre;   // element index before the replaced span
	while ( nPre >= 2 )
	{
		if ( nPost < 2 )
			return false;
		double fRatio = (double)( nPre - 1 ) / (double)( nPost - 1 );
		int nA = nPre - 1, nB = nPost - 1;
		int nLayer = pts[i].nLayer;
		unsigned nDirPre = pts[i - 1].nDirection;
		unsigned nDirPost = pts[i].nDirection;
		int nDXPre = (int)pts[i].nX - (int)pts[i - 1].nX;
		int nDYPre = (int)pts[i].nY - (int)pts[i - 1].nY;
		int nDXPost = (int)pts[i + 1].nX - (int)pts[i].nX;
		int nDYPost = (int)pts[i + 1].nY - (int)pts[i].nY;
		buf[0] = pts[nStart + 1];
		int nX = pts[nStart + 1].nX;
		int nY = pts[nStart + 1].nY;
		int nOrig = nStart + 1;   // original element whose pose/moving flags feed buf[k] (retail @0xa3deb: buf[1]
		                          // inherits from pts[nStart+1], not +2 -- flag-source off-by-one, geometry-neutral)
		int k = 0;
		while ( nA > 0 || nB > 0 )
		{
			bool bPre;
			double fShare = (double)nB * fRatio;
			if ( fabs( (double)nA - fShare ) < 0.01 )
				bPre = nA > nB;
			else
				bPre = (double)nA > fShare;
			if ( nA == 0 )
				bPre = false;
			unsigned nDir;
			if ( nB == 0 || bPre )
			{
				nDir = nDirPre;
				nX += nDXPre;
				nY += nDYPre;
				--nA;
			}
			else
			{
				nDir = nDirPost;
				nX += nDXPost;
				nY += nDYPost;
				--nB;
			}
			buf[k].nDirection = (unsigned short)nDir;
			SPathPlace p = pts[nOrig];   // pose/moving inherited
			p.nX = (unsigned short)( nX & 0xff );
			p.nY = (unsigned short)( nY & 0xff );
			p.nLayer = (unsigned short)nLayer;
			p.nIntegral = 1;
				p.nFinal = 0;   // retail masks off the nFinal/n3D bit (word & 0xfc01 @0xa3ed0) on interleaved points
			++nOrig;
			buf[++k] = p;
		}
		int nCount = nPre + nPost - 2;   // == k; elements written back
		bool bOK = true;
		int j = 0;
		for ( ; j < nCount; ++j )
			if ( !pNet->IsPassable( buf[j] ) )
				break;
		if ( j == nCount )
		{
			for ( j = 0; j < nCount - 1; ++j )
				if ( GetTransitionType( pNet, buf[j], buf[j + 1] ) == TT_NO_WAY )
					break;
			if ( j == nCount - 1 )
			{
				if ( GetTransitionType( pNet, pts[nStart], buf[0] ) == TT_NO_WAY )
					bOK = false;
				if ( GetTransitionType( pNet, buf[nCount - 1], pts[i + nPost - 1] ) != TT_NO_WAY && bOK )
				{
					for ( int n = 0; n < nCount; ++n )
						pts[nStart + 1 + n] = buf[n];
					return true;
				}
			}
		}
		if ( nPre > nPost )
		{
			--nPre;
			++nStart;
		}
		else
		{
			--nPost;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa4060: one smoothing pass -- the first fixable corner wins.
bool SmoothenPathIter( CPath &path )
{
	vector<SPathPlace> &pts = path.points;
	if ( (int)pts.size() < 5 )
		return false;
	IPathNetwork *pNet = path.pNet.GetPtr();
	if ( pNet == 0 )
		return false;
	for ( int i = 2; i < (int)pts.size() - 2; ++i )
	{
		if ( !NotSmooth( pts, i ) )
			continue;
		int nPre = FindMaxPreSize( pts, i );
		if ( nPre <= 1 )
			continue;
		int nPost = FindMaxPostSize( pts, i );
		if ( nPost <= 1 )
			continue;
		if ( SmoothenPrePost( pts, pNet, nPre, nPost, i ) )
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xa40f0: iterate to a fixpoint, capped at ~100 passes.
void SmoothenPath( CPath &path )
{
	int n = 0;
	bool b = SmoothenPathIter( path );
	while ( b )
	{
		if ( n > 100 )
			return;
		++n;
		b = SmoothenPathIter( path );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NAI
