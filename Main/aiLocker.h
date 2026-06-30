#ifndef __AILOCKER_H_
#define __AILOCKER_H_
namespace NAI
{
	class IPathNetwork;
	bool IsBigLocker( CObjectBase *pUnit );
	bool IsBigLockerPassable( IPathNetwork *pNet, const NAI::SPathPlace &p );
////////////////////////////////////////////////////////////////////////////////////////////////////
};
#endif