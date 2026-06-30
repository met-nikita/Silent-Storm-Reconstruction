#ifndef __GRenderModes_H_
#define __GRenderModes_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NGfx
{
	class CTexture;
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESceneRenderMode
{
	SRM_FASTEST,
	SRM_SHOWOCCLUDERS,
	SRM_SHOWLIGHTMAP,
	SRM_SHOWSKYMAP,
	SRM_LIGHTMAPPED,
	SRM_BEST,
	SRM_LAST
};
enum ERenderPath
{
	RP_TNL,
	RP_FASTEST,
	RP_SHOWOCCLUDERS,
	RP_GF2,
	RP_UPDATE_CL,
	RP_SHOWLIGHTMAPPED,
	RP_GF2_CL,
	RP_GF3_CL
};
inline bool IsUingRegisters( ERenderPath rp ) { return rp >= RP_GF2; }
inline bool IsUsingCacheLighting( ERenderPath rp ) { return rp >= RP_UPDATE_CL; }
inline bool IsUsingShadows( ERenderPath rp ) { return rp >= RP_GF2; }
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFogMode
{
	FOG_NONE,
	FOG_PERVERTEX,
	FOG_DYNAMIC,
	FOG_LAST
};
enum EHSRMode
{
	HSR_NONE,
	HSR_FAST,
	HSR_LAST
};
enum ETransparentMode
{
	TRM_NONE,
	TRM_NORMAL,
	TRM_ONLY,
	TRM_LAST
};
enum EScenePartsSet
{
	SPS_STATIC = 1,
	SPS_DYNAMIC = 2,
	SPS_ALL = 3
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFogParams
{
	float fDist;
	CVec3 vFogColor;
	float fHeight, fDensity;
	CVec3 vWaterColor;
	float fCameraHeight;
	float fVapourNoiseParam, fVapourSpeed, fVapourSwitchTime;
	float fTime;
	float fDistStart;
	float fVapourHeightStart;
	
	void SetDefaults()
	{
		memset( this, 0, sizeof(SFogParams) );
		fDist = 10000; fDistStart = 100;
	}
	bool IsSameParams( const SFogParams &a ) const 
	{ 
		return 
			fDist == a.fDist && 
			vFogColor == a.vFogColor && 
			fHeight == a.fHeight && 
			fDensity == a.fDensity && 
			vWaterColor == a.vWaterColor &&
			fabs( fCameraHeight - a.fCameraHeight ) < 0.1f &&
			fDistStart == a.fDistStart &&
			fVapourHeightStart == a.fVapourHeightStart;
	}
	bool operator==( const SFogParams &a ) const { return memcmp( &a, this, sizeof(SFogParams) ) == 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
