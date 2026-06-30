#ifndef __A5_INVENTORYUNIT_H__
#define __A5_INVENTORYUNIT_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wInterfaceVisitors.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	enum ESlot;
	class CPanzerklein;
}
namespace NRPG
{
	class IUnitMissionInfo;
}
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitItemType
{
	UIT_WEAPON_HEAVY = 0,
	//	UIT_WEAPON_LIGHT,
	UIT_HAND,
	UIT_BELT_L1,
	UIT_BELT_R1,
	UIT_BELT_M1,
	UIT_BELT_MEDIUM_L1,
	UIT_BELT_MEDIUM_R1,
	UIT_BELT_MEDIUM_L2,
	UIT_BELT_MEDIUM_R2,
	UIT_WAIST_BELT_L1,
	UIT_WAIST_BELT_R1,
	UIT_CAP,
	UIT_BACKPACK,
	UIT_PK_LEFT_HAND,
	N_UNIT_ITEM_TYPES,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetItemsBindPlaces( vector<IRenderVisitor::SBoundMesh> *pRes, NRPG::IUnitMissionInfo *p, 
	bool bUndrawWeapon, NDb::CPanzerklein *pPanzerklein, bool bNoHeavyWeapon = false );
const char* GetBoneName( EUnitItemType t, NRPG::IInventoryItem *pItem, bool bIsPK = false );
const char* GetBoneName( NDb::ESlot slot, NRPG::IUnitMissionInfo *pRPG, bool bUndrawWeapon );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif