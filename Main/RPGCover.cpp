#include "StdAfx.h"
#include "RPGCover.h"

namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetObjectsThatMayBeDamaged @0x294300
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetObjectsThatMayBeDamaged( CCoverGridInfo *pCover,
	unordered_map<CPtr<CObjectBase>, int, SPtrHash> *pRes )
{
	pRes->clear();
	// Merge hit + loose rays. The binary builds a temp vector seeded from hitRays and splices
	// looseRays into it; this is set-union semantics, so ordering is irrelevant.
	vector<CCoverGridInfo::SRay> rays( pCover->hitRays );
	rays.insert( rays.end(), pCover->looseRays.begin(), pCover->looseRays.end() );
	for ( vector<CCoverGridInfo::SRay>::const_iterator i = rays.begin(); i != rays.end(); ++i )
	{
		// Walk this ray's grid-cell depth-interval list and key on each interval's source object.
		// Decode @0x294300 keys on *(CPtr<CObjectBase>*)p->pSrc->pSrc; since NAI::SSourceInfo's
		// first member is CPtr<CObjectBase> pUserData (aiInterval.h:12), that is exactly
		// p->GetInfo().pUserData over the real dev types.
		for ( NAI::CFastRenderer::SResult *p = pCover->grids[i->nGrid].resGrid[i->y][i->x]; p; p = p->pNext )
			(*pRes)[ p->GetInfo().pUserData ] = 1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetHitIntersections @0x293910
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetHitIntersections( NAI::IAIMap *pMap, vector<STrailPoint> *pTrail,
	CCoverGridInfo *pCover, const CCoverGridInfo::SRay *pRay, SAttackRayInfo *pInfo )
{
	// Faithful cell index (the actual content of @0x293910): re-walk the ray back into the grid
	// cell it was recorded from -- edi = pCover->grids[ray.nGrid].resGrid[ray.y][ray.x].
	NAI::CFastRenderer::SResult *pCell = pCover->grids[pRay->nGrid].resGrid[pRay->y][pRay->x];
	// DEFERRED: the original tail-calls the per-ray overload
	//   GetHitIntersections( pMap, &pInfo->trailPoints, pCell, pInfo );   // RPGBullet.obj @0x2915d0
	// which reads a finished cover cell and appends trail points. That overload is genuinely absent
	// in the dev tree (RPGBullet.h:17-22 defers it -- it depends on the un-converged per-bullet hit
	// roll), so this forward is a documented no-op. Behaviour-neutral: nothing calls this helper.
	(void)pMap; (void)pTrail; (void)pInfo; (void)pCell;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
