#ifndef __wRocket_H_
#define __wRocket_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
 
#include "time.h"
namespace NDb
{
	class CModel;
	class CEffect;
}
namespace NRPG
{
	class IClipItem;
	class CAttackPortion;
}
namespace NWorld
{
	class IDynamicObject;
	class CWorld;
	class CUnitServer; 
	IDynamicObject *CreateRocketServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
		NRPG::IClipItem *_pRocket, CUnitServer *_pIgnored, NDb::CEffect *_pEffect = 0 );
}
#endif
