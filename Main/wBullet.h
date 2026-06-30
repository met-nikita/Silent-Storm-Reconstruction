#ifndef __WBULLET_H_
#define __WBULLET_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "time.h"
#include "RPGGame.h"
namespace NWorld
{
class CWorld;
class CUnitServer;
class IDynamicObject;
IDynamicObject *CreateBulletServer( CWorld *pWorld, const vector<NRPG::STrailPoint> &trail,
		STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed );
}
#endif
