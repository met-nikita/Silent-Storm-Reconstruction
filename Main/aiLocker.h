#ifndef __AILOCKER_H_
#define __AILOCKER_H_
#include "aiPosition.h"   // NAI::EPassable / SPathPlace (BigLockerPassableState returns EPassable by value)
namespace NAI
{
	class IPathNetwork;
	bool IsBigLocker( CObjectBase *pUnit );
	bool IsBigLockerPassable( IPathNetwork *pNet, const NAI::SPathPlace &p );
	// @0x5a730 granular sibling of IsBigLockerPassable (bool -> EPassable): AIP_LOCKED if any place of
	// the big-unit lock footprint at p is locked, else the anchor place's own GetPassability verdict.
	EPassable BigLockerPassableState( IPathNetwork *pNet, const NAI::SPathPlace &p );
////////////////////////////////////////////////////////////////////////////////////////////////////
};
#endif