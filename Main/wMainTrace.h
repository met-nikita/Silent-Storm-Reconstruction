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
	bool TraceTile( IWorld *pWorld, const CRay &ray, NAI::SPosition *pRes, int nMaxFloor );
	void TraceObjects( IWorld *pWorld, const CRay &ray, vector<CObjectBase*> *pRes, int nMaxFloor );
	bool TraceRay( IWorld *pWorld, const CRay &ray, int nMaxFloor, CObjectBase **ppUserData, int *pUserID, CVec3 *pPoint );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif