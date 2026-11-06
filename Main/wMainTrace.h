#ifndef __WMAINTRACE_H_
#define __WMAINTRACE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wInterface.h"
#include "aiPosition.h"
namespace NAI
{
	class IAIMap;
}
namespace NWorld
{
	struct STraceResult;
	// retail v1.2 NWorld::Trace @0x77e200: the cursor trace (tile, point and hovered object in one pass)
	void Trace( IWorld *pWorld, IPlayer *pPlayer, const CRay &ray, int nMaxFloor, STraceResult *pRes, bool bModifier );
	bool TraceRay( IWorld *pWorld, const CRay &ray, int nMaxFloor, CObjectBase **ppUserData, int *pUserID, CVec3 *pPoint );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif