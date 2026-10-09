#ifndef __aiDoorCollider_H_
#define __aiDoorCollider_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiCollider.h"
#include "wTSFlags.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
struct SDoorCollision
{
	CPtr<CObjectBase> pSrc;
	bool bInOpen, bInClosed;
	SDoorCollision( CObjectBase *p ): pSrc(p), bInOpen(false), bInClosed(false) {}
};
struct SDoorColliderAnalyzer
{
	// States belong to a particular door, not to the whole collision query.
	// A closed hull of A and an open hull of B can both be inactive at once.
	vector<SDoorCollision> doors;
	bool bBlocked;
	bool operator()( const SColliderUserInfo& info ) 
	{ 
		CObjectBase *pSrc = info.pSrc->pUserData;
		if ( !pSrc )
			return false;
		int i = 0;
		while ( i < doors.size() && doors[i].pSrc != pSrc )
			++i;
		if ( i == doors.size() )
			doors.push_back( SDoorCollision(pSrc) );
		SDoorCollision &door = doors[i];
		if ( ( info.pSrc->nTSFlags & NWorld::TS_STATE_OPEN ) == 0 )
			door.bInClosed = true;
		if ( ( info.pSrc->nTSFlags & NWorld::TS_STATE_CLOSED ) == 0 )
			door.bInOpen = true;
		bBlocked = bBlocked || ( door.bInOpen && door.bInClosed );
		return bBlocked;
	}
	bool IsCollided() const { return bBlocked; }
	SDoorColliderAnalyzer(): bBlocked(false) {}
	void Clear() 
	{
		bBlocked = false;
		doors.clear();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

#endif
