#include "StdAfx.h"
#include "GGeometry.h"
#include "GLightmap.h"
#include "Bound.h"
#include "GShadowMap.h"

namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLMRegion
////////////////////////////////////////////////////////////////////////////////////////////////////
/*CLightmapTextureCache* CLMRegion::GetCache()
{
	CTracker *pTracker = GetTracker();
	if ( !IsValid(pTracker) )
		return 0;
	CDynamicCast<CLightmapTextureCache> pCache(pTracker);
	return pCache;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightmapTextureCache
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTextureCache::InitCache()
{
	NCache::CQuadTreeElement root;
	root.nXSize = root.nYSize = GetMSB( shadowMapsShare.GetLMTexResolution() - 1 ) + 1;//N_LM_SIZE_LOG;
	root.nShiftX = root.nShiftY = 0;
	AddRoot( root );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTextureCache::RefreshTexture()
{
	if ( IsValid( pTexture ) )
		return;
	pTexture = shadowMapsShare.GetDepthShadow();//GetLM();
	Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLightmapTextureCache::operator&( CStructureSaver &f )
{ 
	if ( f.IsReading() )
		InitCache();
	return 0; 
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
/*CLMRegion* CLightmapTracker::AllocRegion( const CTPoint<int> &lmSize )
{
	NCache::CQuadTreeElement elem;
	elem.nXSize = GetMSB( lmSize.x - 1 ) + 1;
	elem.nYSize = GetMSB( lmSize.y - 1 ) + 1;
	CLightmapTextureCache::SCachePlace place;
	if ( !pLMTextureCache->GetPlace( elem, &place ) )
		return 0;
	CLMRegion *pRes = new CLMRegion;
	pLMTextureCache->PerformAlloc( pRes, &place );
	pRes->lmRegion.SetRect( 
		place.resPlace.nShiftX, place.resPlace.nShiftY,
		place.resPlace.nShiftX + (1<<elem.nXSize), place.resPlace.nShiftY + (1<<elem.nYSize)
		);
	return pRes;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
//REGISTER_SAVELOAD_CLASS( 0x00462181, CLightmapTextureCache )
//REGISTER_SAVELOAD_CLASS( 0x00662110, CLMRegion )
