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
}