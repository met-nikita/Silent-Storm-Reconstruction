#include "StdAfx.h"
#include "wUnitServer.h"
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsBigLocker( CObjectBase *pUnit )
{
	CDynamicCast<NWorld::CUnitServer> pUS( pUnit );
	if ( !pUS )
	{
		ASSERT(0);
		return false;
	}
	if ( pUS->IsEmptyPK() || pUS->IsWearingPK() )
		return true;
	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
bool IsBigLockerPassable( IPathNetwork *pNet, const NAI::SPathPlace &p )
{
	vector<SPathPlace> pts;
	pNet->GetLockArea( &pts, p, true );
	for ( int i = 0; i < pts.size(); ++i )
		if ( pNet->IsLocked( pts[i], true ) )
			return false;
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// @0x5a730 BigLockerPassableState -- granular sibling of IsBigLockerPassable (bool -> EPassable):
// AIP_LOCKED if any place of the big-unit lock footprint at p is locked, else the anchor place's own
// GetPassability verdict. Used by CDumbUnitServer::CheckPassable for big lockers / lay-pose moves.
EPassable BigLockerPassableState( IPathNetwork *pNet, const NAI::SPathPlace &p )
{
	vector<SPathPlace> pts;
	pNet->GetLockArea( &pts, p, true );
	for ( int i = 0; i < pts.size(); ++i )
		if ( pNet->IsLocked( pts[i], true ) )
			return AIP_LOCKED;
	return pNet->GetPassability( p );
}
///////////////////////////////////////////////////////////////////////////////////////////////////
bool IsBigLockerPlacePassable( IPathNetwork *pNet, const SPathPlace &p )
{
	// Flying PKs have no ground footprint; GetLayer encodes their altitude.
	// Match BigLockerPassableState instead of attempting ground relocation.
	if ( p.IsFinal() )
		return pNet->GetPassability( p ) == AIP_YES;
	// Placement needs room for the entire PK, not merely an unlocked anchor.
	// Keep movement's retail passability policy separate from this stricter
	// spawn/relocation check. PKs require standing clearance even when empty.
	if ( !p.IsIntegral() )
		return false;
	SPathPlace center( p );
	center.SetPose( CM_STAND );
	if ( pNet->GetPassability( center ) != AIP_YES )
		return false;
	int nDX = nMoveShift[p.GetDirection()][0], nDY = nMoveShift[p.GetDirection()][1];
	// Unlike GetLockArea (which clips reservations at the map border), reject
	// a placement whose footprint extends off the grid.
	for ( int x = -abs( nDX ); x <= abs( nDX ); ++x )
		for ( int y = -abs( nDY ); y <= abs( nDY ); ++y )
		{
			if ( x && y && x * y != nDX * nDY )
				continue;
			if ( p.GetX() + x < 0 || p.GetY() + y < 0 ||
				p.GetX() + x > 255 || p.GetY() + y > 255 )
				return false;
			SPathPlace cell( center );
			cell.SetXY( p.GetX() + x, p.GetY() + y );
			if ( pNet->GetPassability( cell ) != AIP_YES )
				return false;
		}
	return IsBigLockerPassable( pNet, center );
}
///////////////////////////////////////////////////////////////////////////////////////////////////
}
