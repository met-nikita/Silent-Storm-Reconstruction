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
}
namespace NWorld
{
class CWorld;
class CUnitServer;
IDynamicObject *CreateKnifeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
		NRPG::IInventoryItem *_pIItem, CUnitServer *_pIgnored );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
