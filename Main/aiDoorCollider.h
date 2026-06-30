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
struct SDoorColliderAnalyzer
{
	CPtr<CObjectBase> pSrc;
	bool bInOpen, bInClosed;
	bool operator()( const SColliderUserInfo& info ) 
	{ 
		pSrc = info.pSrc->pUserData;
		if ( !pSrc )
			return false;
		if ( ( info.pSrc->nTSFlags & NWorld::TS_STATE_OPEN ) == 0 )
			bInClosed = true;
		if ( ( info.pSrc->nTSFlags & NWorld::TS_STATE_CLOSED ) == 0 )
			bInOpen = true;
		return ( bInOpen && bInClosed );
	}
	bool IsCollided() const { return bInOpen && bInClosed; }
	SDoorColliderAnalyzer::SDoorColliderAnalyzer() : bInOpen( false ), bInClosed( false ), pSrc( 0 )  {}
	void Clear() 
	{
		bInOpen = bInClosed = false;
		pSrc = 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

#endif