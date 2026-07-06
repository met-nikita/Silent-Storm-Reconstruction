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
}