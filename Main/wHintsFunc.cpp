#include "StdAfx.h"
#include "wHintsFunc.h"
#include "aiMap.h"		// NAI::IAIMap, NAI::SInterval (via aiInterval.h), CFloorsSet
#include "wDebris.h"		// NWorld::CDebrisController
#include "RPGItem.h"		// NRPG::IInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateHintItem @0x6a21a0 -- DEFERRED STUB.
// Retail builds an in-world hint pickup from db item 0x1b6 ( NDb::GetRPGItem(0x1b6,1) ) wrapped in
// CSimpleItem<IHintItem>. That IHintItem interface / in-world hint-item subsystem does not exist in
// this fork (cf. wMain.cpp CWorld::AddNextUIHint: "this predecessor fork has no in-world hint-item
// subsystem, so there is nothing to remove"). Returning 0 keeps PlaceHintsToMap behaviour-neutral:
// with no item created the per-slot placement branch is skipped, so no hint pickups are spawned --
// exactly matching a fork that has none. Promote to RPGItem.h + a real CSimpleItem<IHintItem> only
// if/when the hint-item subsystem is reconstructed.
IInventoryItem *CreateHintItem()
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void FindClosePositionOnSurface( IAIMap *pMap, const CVec3 &ptFrom, CVec3 *pRes )
{
	*pRes = ptFrom;
	if ( !pMap )
		return;
	// cast a ray straight down from ptFrom
	CRay ray;
	ray.ptOrigin = ptFrom;
	ray.ptDir = CVec3( 0, 0, -1 );
	vector<SInterval> hits;
	pMap->Trace( ray, &hits, 0x8000 );	// hg / shg defaults (shg == STH_UNION_TERR_HG, retail arg 1)
	// res.z = highest surface below the origin; enter.fT is the depth along the (downward) ray
	bool bFirst = true;
	for ( vector<SInterval>::const_iterator it = hits.begin(); it != hits.end(); ++it )
	{
		const float fDepth = it->enter.fT;
		if ( fDepth < 0 )
			continue;
		const float fZ = ptFrom.z - fDepth;
		if ( bFirst )
		{
			bFirst = false;
			pRes->z = fZ;
		}
		else
			pRes->z = Max( pRes->z, fZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NAI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void PlaceHintsToMap( CDebrisController *pDebris, NAI::IAIMap *pMap, const vector<SHintSlot> &hints )
{
	for ( vector<SHintSlot>::const_iterator slot = hints.begin(); slot != hints.end(); ++slot )
	{
		// CPtr does the retail AddRef-on-create / ReleaseRef-on-exit (dead items are released too).
		CPtr<NRPG::IInventoryItem> pItem = NRPG::CreateHintItem();
		if ( IsValid( pItem ) )
		{
			// Z-axis quaternion from the slot heading: (0,0,sin(a/2),cos(a/2)), a == ToRadian(fRotation).
			CQuat rot( ToRadian( slot->pos.fRotation ), CVec3( 0, 0, 1 ) );
			CVec3 ptOnSurface;
			NAI::FindClosePositionOnSurface( pMap, slot->pos.ptPos, &ptOnSurface );
			pDebris->AddFrozenItem( ptOnSurface, rot, pItem, slot->pos.nFloor );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
