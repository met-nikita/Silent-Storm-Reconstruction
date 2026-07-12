#include "StdAfx.h"
#include "wHintsFunc.h"
#include "aiMap.h"		// NAI::IAIMap, NAI::SInterval (via aiInterval.h), CFloorsSet
#include "wDebris.h"		// NWorld::CDebrisController
#include "RPGItem.h"		// NRPG::IInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::CreateHintItem @0x6a21a0 is now REAL (RPGItemSet.cpp, declared in RPGItem.h): db item 0x1b6
// wrapped in the CSimpleItem<IHintItem> analog CHintItem. The former deferred stub here is gone --
// the hint-item subsystem (NRPG::IHintItem + the CMissionUI CHintIcon overlay pass, retail
// UpdateVisibleItems @0x2130c0) exists in this fork now. NOTE: PlaceHintsToMap still has no caller
// in this fork (the retail world-side hint-slot feed is not wired yet), so no pickups spawn until
// that lands.
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
			pDebris->AddFrozenItem( pMap, ptOnSurface, rot, pItem, false, slot->pos.nFloor );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
