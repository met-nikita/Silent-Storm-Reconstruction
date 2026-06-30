#ifndef __wGrenade_H_
#define __wGrenade_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "time.h"
namespace NDb
{
	class CModel;
	class CRPGGrenade;
}
namespace NWorld
{
class IDynamicObject;
class CWorld;
class CActionCounter;
class CUnitServer;
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateGrenadeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fTFly, NDb::CModel *pModel, NDb::CRPGGrenade *_pRPGGrenade,
		CUnitServer *_pUnitServer = 0 );
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateClickOfDeath( CActionCounter *pC, CObjectBase *pTarget, int _nUserID, const CRay &ray );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
