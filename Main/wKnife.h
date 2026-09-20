#ifndef __wKnife_H_
#define __wKnife_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "time.h"
namespace NDb
{
	class CModel;
}
namespace NRPG
{
	class IInventoryItem;
	class CAttackPortion;
	struct SAttackRayInfo;
}
namespace NWorld
{
class CWorld;
class CUnitServer;
IDynamicObject *CreateKnifeServer( CWorld *pWorld, const NRPG::SAttackRayInfo &rayInfo, float fSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::IInventoryItem *pIItem );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
