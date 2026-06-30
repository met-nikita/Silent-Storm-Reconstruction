#ifndef __RPGCOVER_H_
#define __RPGCOVER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// RPGCover.obj -- NRPG per-shot cover-grid query helpers (the two RPGCover free functions
// the shipped engine added on top of CalcCovers: GetObjectsThatMayBeDamaged @0x294300 and
// GetHitIntersections @0x293910).
//
// The shipped engine's NRPG::CCoverInfo is a *grid-bearing* cover object: it keeps the two
// NAI::CFastRenderer rasterizer grids that CalcCovers traced (grids[0]=hi-res, grids[1]=lo-res)
// AND a wide SRay that remembers which grid+cell each ray came from (nGrid,x,y), so a later
// pass can re-walk that cell's SResult depth-interval list.
//
// The dev tree's NRPG::CCoverInfo (RPGGame.cpp:36) is the EARLIER, narrow, *serialized* variant
// -- REGISTER_SAVELOAD_CLASS(0x02841161), { hitRays, src, looseRays, obstRays, fSummAPA } with a
// 3-field SRay -- and CGame::CalcCovers builds the CFastRenderer grids as locals and discards
// them. Widening that live save/load record to carry the grids is a forbidden change, so the
// grid-bearing release layout is reconstructed here under a DISTINCT additive name,
// CCoverGridInfo, over the real dev NAI::CFastRenderer. It is transient (never serialized,
// never factory-created), so it is an unregistered POD carrier -- it does NOT collide with the
// live CCoverInfo save id and adds no class-registry entry.
//
// Nothing in the dev tree calls these two helpers yet (the sole release caller is the
// not-yet-converged aiPlaceSource path), so this is a behaviour-neutral parity surface.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RPGBullet.h"   // NRPG::SAttackRayInfo, NRPG::STrailPoint (via RPGGame.h), NAI::CFastRenderer::SResult
                         // + NAI::SSourceInfo (via aiRender.h->aiInterval.h), NAI::IAIMap (fwd-declared there)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// The shipped grid-bearing cover object (release NRPG::CCoverInfo layout). Field IDENTITY mirrors
// the release class; exact byte offsets are NOT reproduced (purely transient runtime state).
struct CCoverGridInfo
{
	struct SRay
	{
		CVec3 ptDir;			// ray direction
		bool  isPenetrate;
		float fDeviation;		// squared projection onto the target direction
		float fResMaxEnter;		// release-new: nearest blocker enter distance on this ray
		int   nGrid;			// release-new: which grid this ray came from (0=hi-res, 1=lo-res)
		int   x, y;				// release-new: that grid's cell coordinates
	};

	NAI::CFastRenderer grids[2];	// [0]=hi-res, [1]=lo-res rasterizer grids (kept, not discarded)
	vector<SRay> hitRays;
	CVec3 src;
	vector<SRay> looseRays;
	vector<SRay> obstRays;
	float fSummAPA;
	float fMinClearDistance;	// release-new
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x294300 -- collect every distinct source object lying on a hit-or-loose ray into *pRes (an
// unordered_map used as a set: value 1). For each ray, re-walk its source grid cell's SResult
// depth-interval list and add the interval's source object.
void GetObjectsThatMayBeDamaged( CCoverGridInfo *pCover,
	unordered_map<CPtr<CObjectBase>, int, SPtrHash> *pRes );
// @0x293910 -- index a finished ray back into its grid cell, then hand that cell's SResult list to
// the per-ray GetHitIntersections overload (RPGBullet.obj @0x2915d0). The cell indexing is
// reconstructed faithfully; the per-ray forward is DEFERRED -- that overload is genuinely absent in
// the dev tree (RPGBullet.h:17-22 explicitly defers it).
void GetHitIntersections( NAI::IAIMap *pMap, vector<STrailPoint> *pTrail,
	CCoverGridInfo *pCover, const CCoverGridInfo::SRay *pRay, SAttackRayInfo *pInfo );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
