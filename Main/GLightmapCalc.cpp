#include "StdAfx.h"
#include "GfxBuffers.h"
#include "GLightmapCalc.h"
#include "GGeometry.h"
#include "GCombiner.h"
#include "..\Misc\RandomGen.h"
#include "GRenderExecute.h"
#include "GShadowMap.h"
#include "GShadowVolume.h"
#include "GfxUtils.h"
#include "Transform.h"
#include "GfxEffects.h"
#include "RectLayout.h"
#include "GRects.h"
#include "GMaterial.h"
#include "GScene.h"
#include "Gfx.h"
#include "..\MiscDll\Commands.h"      // REGISTER_VAR (gfx_cl_use_bump*)
#include "..\FileIO\BasicChunk1.h"    // START_REGISTER / FINISH_REGISTER

// number of sky directions used for dynamic lightmaps
const int N_SKY_DIRECTIONS = 12;
const float F_SKY_SINGLE_STRENGTH_MUL = 2.0f;// / N_SKY_DIRECTIONS;
const int N_DEPTH_CHANNELS_PER_TEX = 3;
const int N_ALL_DEPTH_CHANNELS = 0xffffffff;
const float F_MAX_SCENE_HEIGHT = 20; // CRAP need to store max height in single place
const int N_POINT_LIGHT_RECALC_STEPS = 4;
//! brightness in the darkest area, max is 255
const int N_DARKEST_AREA = 16;
//!!! maximal triangle extent, needed to avoid problems
//!!! with D3DColor saturating to 0 or 1 when it is used as depth
const float F_MAXIMAL_ELEMENT_SIZE = 4;

static NGfx::EColorWriteMask depthChannels[3] = 
{
	NGfx::COLORWRITE_RED, NGfx::COLORWRITE_GREEN, NGfx::COLORWRITE_BLUE
};
// radius / F_POINT_LIGHT_FALLOFF - nominal brightness distance
const float F_POINT_LIGHT_FALLOFF = 6;

////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsInside( const SBound &hold, const SBound &test )
{
	float f = fabs2( test.s.ptCenter - hold.s.ptCenter );
	return f < sqr( hold.s.fRadius - test.s.fRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static SGroupSelect MakeSelectOccluders( IGScene *p )
{
	SGroupSelect s( p->GetLastMask() );
	s.nMaskEvery = N_MASK_OCCLUDER;
	return s;
}
inline bool CanDrawSky() { return shadowMapsShare.GetCLSkyTexturesNumber() > 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
/*static void LoadShit( NGfx::CTexture *p )
{
	NGfx::CTextureLock<NGfx::SPixel8888> l( p, 0, NGfx::WRITEONLY );//INPLACE );
	for ( int y = 0; y < l.GetYSize(); ++y )
	{
		for ( int x = 0; x < l.GetXSize(); ++x )
		{
			if ( ( x + y ) & 1 )
				l[y][x] = NGfx::SPixel8888( 255, 255, 255, 255 );
			else
				l[y][x] = NGfx::SPixel8888( 0, 0, 0, 0 );
		}
	}
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightState
////////////////////////////////////////////////////////////////////////////////////////////////////
static float Halton( int b, int i )
{
	float x = 0, fBInv = 1.0f / b, f = fBInv;
	while ( i )
	{
		x += f * ( i % b );
		i /= b;
		f *= fBInv;
	}
	return x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GenerateRandomSphereVector( CVec3 *pRes )
{
	for(;;)
	{
		CVec3 v( random.GetFloat(-1,1), random.GetFloat(-1,1), random.GetFloat(-1,1) );
		float f = fabs2( v );
		if ( f == 0 || f > 1 )
			continue;
		*pRes = v / sqrt( f );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_POINT_RADIUS = 16;
const float F_POINT_STRENGTH = 0.5f * 4 * 3.14f * F_POINT_RADIUS * F_POINT_RADIUS / F_POINT_LIGHT_FALLOFF / F_POINT_LIGHT_FALLOFF;
void CLightState::AddRay( const CVec3 &vFrom, const CVec3 &vDir, const CVec3 &_vColor )
{
	if ( !IsValid(pVis) )
		return;
	if ( random.GetFloat( 0, 1 ) < 0.3f ) // absorbtion
		return;
	CRay r;
	r.ptOrigin = vFrom;
	r.ptDir = vDir;
	r.ptDir *= 1000;
	CVec3 vPoint, vNormal, vReflectColor;
	float fT;
	if ( !pVis->TraceScene( MakeSelectAll(), r, &fT, &vNormal, &vReflectColor, SPS_STATIC ) )
		return;
	vPoint = r.Get( fT ) + vNormal * 0.01f;
	float fRadius = F_POINT_RADIUS;
	//vReflectColor *= 2;
	ASSERT( vReflectColor.x <= 1 );
	ASSERT( vReflectColor.y <= 1 );
	ASSERT( vReflectColor.z <= 1 );
	//vReflectColor.Minimize( CVec3(1,1,1) );
	CVec3 vRefColor( _vColor.r * vReflectColor.r, _vColor.g * vReflectColor.g, _vColor.b * vReflectColor.b );
	CVec3 vColor(vRefColor);
	if ( fabs2(vColor) == 0 )
		return;
	while ( fabs2(vColor) < 0.01f )
	{
		vColor *= 2;
		fRadius /= 1.41f;
	}
	if ( fRadius < 1 )
		return;
	semiPoints.push_back( SSemiPointLight( vColor, vPoint, vNormal, fRadius ) );
	CVec3 vReflect;
	GenerateRandomSphereVector( &vReflect );
	if ( vReflect * vNormal < 0 )
		vReflect -= ( 2 * ( vReflect * vNormal ) ) * vNormal;
	AddRay( vPoint + vReflect * 0.01f, vReflect, vRefColor * 0.9f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::AddParallel( bool bDoRender, const SSphere &_bound, const CVec3 &vDir, const CVec3 &_vColor )
{
	CVec3 vColor = _vColor;
	if ( fabs2( vColor ) < 1e-6f )
		return;
	if ( bDoRender )
		parallel.push_back( SParallelLight( vColor, vDir ) );
	if ( !pVis )
		return;
	CVec3 vCenter = _bound.ptCenter;
	float fWidth = _bound.fRadius * 2;
	float fTest = random.GetFloat( 0, F_POINT_STRENGTH );
	float fStrength = fWidth * fWidth;
	while ( fabs2( vColor ) > 0.1f )
	{
		vColor = vColor * 0.5f;
		fStrength = fStrength * 2;
	}
	while ( fabs2( vColor ) < 0.02f )
	{
		vColor = vColor * 2;
		fStrength = fStrength * 0.5f;
	}
	CVec3 vRight = CVec3(0,0,1) ^ vDir;
	if ( fabs2( vRight ) < 0.001f )
		vRight = CVec3(0,1,0) ^ vDir;
	Normalize( &vRight );
	CVec3 vUp = vRight ^ vDir;
	static int nFake = 0;
	for ( ; fTest < fStrength; fTest += F_POINT_STRENGTH )
	{
		++nFake;
		CVec3 vShifted = vCenter +
			vRight * ( Halton( 5, nFake ) * fWidth - fWidth / 2 ) +
			vUp    * ( Halton( 7, nFake ) * fWidth - fWidth / 2 );
		vShifted -= vDir * 100;
		AddRay( vShifted, vDir, vColor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::AddPoint( bool bDoRender, const CVec3 &vCenter, float fRadius, const CVec3 &_vColor, bool bCastShadow )
{
	// v1.2 0x52c330: discard tiny lights before adding them to the saved cache.
	if ( fRadius <= 0.5f )
		return;
	if ( bDoRender )
		points.push_back( SPointLight( _vColor, vCenter, fRadius, bCastShadow ) );
	if ( !pVis )
		return;
	float fTest = random.GetFloat( 0, F_POINT_STRENGTH );
	float fStrength = sqr( fRadius ) / sqr( F_POINT_RADIUS ) * F_POINT_STRENGTH;
	CVec3 vColor(_vColor);
	while ( fabs2( vColor ) > 0.1f )
	{
		vColor = vColor * 0.5f;
		fStrength = fStrength * 2;
	}
	while ( fabs2( vColor ) < 0.02f )
	{
		vColor = vColor * 2;
		fStrength = fStrength * 0.5f;
	}
	for ( ; fTest < fStrength; fTest += F_POINT_STRENGTH )
	{
		CVec3 vDir;
		GenerateRandomSphereVector( &vDir );
		AddRay( vCenter, vDir, vColor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::CreateSimple( SLightStateCalcSeed *pSeed, const SGlobalIlluminationInfo &l )
{
	CreateScattered( pSeed, l, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CLightState::GenerateSkyDir( SLightStateCalcSeed *pSeed )
{
	CVec3 vSky;
	int &nSeed = pSeed->nSeed;
	int nTexs = Max( 1, shadowMapsShare.GetCLSkyTexturesNumber() );
	int nDirBeta = nTexs * N_DEPTH_CHANNELS_PER_TEX;
	for (;;)
	{
		CVec3 vAmbDir;
		float fTeta = 1 - Halton( 2, nSeed ) * 0.7f; // never less then 0.3f;
		fTeta = acos( fTeta );
		float fOmega = Halton( nDirBeta, nSeed ) * FP_2PI;
		vSky = CVec3( cos(fOmega) * sin(fTeta), sin(fOmega) * sin(fTeta), -cos(fTeta) );
		++nSeed;
		if ( nSeed == 100000 )
			nSeed = 0;
		if ( vSky.z <= -0.3f )
			break;
		ASSERT( 0 );
	}
	return vSky;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::CreateScattered( SLightStateCalcSeed *pSeed, const SGlobalIlluminationInfo &l, IGScene *_pVis )
{
	pVis = _pVis;
	vAmbientColor = l.vAmbient;
	vUpDifColor = l.vUpDifColor;
	skyDirections.resize( N_SKY_DIRECTIONS );
	for ( int i = 0; i < skyDirections.size(); ++i )
	{
		skyDirections[i] = GenerateSkyDir( pSeed );
		AddParallel( false, l.globalBounds, skyDirections[i], l.vAmbient * F_SKY_SINGLE_STRENGTH_MUL / N_SKY_DIRECTIONS );
	}
	// sun is treated as parallel light source, usually single in scene
	for ( int k = 0; k < l.parallel.size(); ++k )
	{
		const SGlobalIlluminationInfo::SDirectional &d = l.parallel[k];
		AddParallel( !d.bIsRendered, l.globalBounds, d.vDir, d.vColor );
	}
	for ( int k = 0; k < l.points.size(); ++k )
	{
		const SGlobalIlluminationInfo::SPoint &p = l.points[k];
		AddPoint( !p.bIsRendered, p.vCenter, p.fRadius, p.vColor, p.bCastShadow );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::TraceDynamicLMPointLight( SDynamicAmbientInfo *pRes, const CVec3 &vTarget, float fTargetR,
	const CVec3 &vCenter, float fRadius, const CVec3 &_vColor, const CVec3 &vSemiNormal, IGScene *pVis ) const
{
	CVec3 vDir = vTarget - vCenter;
	float fDist2 = fabs2( vDir );
	if ( fDist2 > sqr( fRadius ) )
		return;
	if ( fDist2 == 0 )
		return;
	if ( vDir * vSemiNormal < 0 )
		return;
	float fDist = sqrt( fDist2 );
	float fK = fDist / fRadius * F_POINT_LIGHT_FALLOFF;
	float fFalloff = 1 / sqr( 1 + fK );
	if ( fFalloff < 0.1f )
		return;
	float f;
	CVec3 vA, vColor;
	CRay r;
	// Retail v1.2 0x52b109..0x52b12b: a null scene means no shadow test.
	if ( pVis && fDist > fTargetR )
	{
		float fStep = fTargetR / fDist;
		r.ptOrigin = vCenter;
		r.ptDir = vDir * ( 1 - fStep );
		if ( pVis->TraceScene( MakeSelectOccluders(), r, &f, &vA, &vColor, SPS_STATIC ) && f < 1 )
			return;
	}
	CVec3 vNormalDir = vDir / fDist;
	pRes->AddLight( _vColor * fFalloff, -vNormalDir );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::TraceDynamicLM( SDynamicAmbientInfo *pRes, const SSphere &bv, IGScene *pVis ) const
{
	pRes->Clear();
	CRay r;
	float f;
	CVec3 vA, vColor;
	for ( int k = 0; k < parallel.size(); ++k )
	{
		const SParallelLight &p = parallel[k];
		r.ptOrigin = bv.ptCenter - p.vDir * bv.fRadius;
		r.ptDir = -p.vDir * 1000;
		if ( !pVis->TraceScene( MakeSelectOccluders( pVis ), r, &f, &vA, &vColor, SPS_STATIC )  )
			pRes->AddLight( p.vColor, -p.vDir );
	}
	// Retail v1.2 0x52b453..0x52b463 / 0x52b80f: without sky textures,
	// dynamic objects receive the full ambient, just like static lightmaps.
	if ( !skyDirections.empty() && !CanDrawSky() )
	{
		pRes->vZPos.v += vAmbientColor + vUpDifColor;
		pRes->vXPos.v += vAmbientColor;
		pRes->vYPos.v += vAmbientColor;
		pRes->vXNeg.v += vAmbientColor;
		pRes->vYNeg.v += vAmbientColor;
		pRes->vZNeg.v += vAmbientColor - vUpDifColor;
	}
	else if ( !skyDirections.empty() )
	{
		SDynamicAmbientInfo ambient;
		ambient.Clear();
		for ( int k = 0; k < skyDirections.size(); ++k )
		{
			r.ptOrigin = bv.ptCenter -skyDirections[k] * bv.fRadius;
			r.ptDir = -skyDirections[k] * 1000;
			if ( !pVis->TraceScene( MakeSelectOccluders( pVis ), r, &f, &vA, &vColor, SPS_STATIC ) )
			{
				ambient.AddLight( CVec3( sqr( F_SKY_SINGLE_STRENGTH_MUL / N_SKY_DIRECTIONS ), 0, 0 ), -skyDirections[k] );
				ambient.AddLight( CVec3( sqr( F_SKY_SINGLE_STRENGTH_MUL / N_SKY_DIRECTIONS ), 0, 0 ), CVec3(0,0,-1) );
			}
		}
		pRes->vZPos.v += sqrt( ambient.vZPos.v.x ) * ( vAmbientColor + vUpDifColor );
		pRes->vXPos.v += sqrt( ambient.vXPos.v.x ) * vAmbientColor;
		pRes->vYPos.v += sqrt( ambient.vYPos.v.x ) * vAmbientColor;
		pRes->vXNeg.v += sqrt( ambient.vXNeg.v.x ) * vAmbientColor;
		pRes->vYNeg.v += sqrt( ambient.vYNeg.v.x ) * vAmbientColor;
		pRes->vZNeg.v += sqrt( ambient.vZNeg.v.x ) * ( vAmbientColor - vUpDifColor );
	}
	for ( int k = 0; k < points.size(); ++k )
	{
		const SPointLight &p = points[k];
		// Retail v1.2 0x52b930..0x52b94c forwards the saved shadow flag.
		TraceDynamicLMPointLight( pRes, bv.ptCenter, bv.fRadius, p.vCenter, p.fRadius, p.vColor, CVec3(0,0,0), p.bCastShadow ? pVis : 0 );
	}
	for ( int k = 0; k < semiPoints.size(); ++k )
	{
		const SSemiPointLight &p = semiPoints[k];
		TraceDynamicLMPointLight( pRes, bv.ptCenter, bv.fRadius, p.vCenter, p.fRadius, p.vColor, p.vNormal, pVis );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CLightState::TracePerspective()
{
	CVec3 vEnter, vNormal;
	if ( !TraceFromCamera( &vEnter, &vNormal ) )
		return;

	SLightSet l;
	l.points.push_back( SSemiPointLight( CVec3(0,1,0), vEnter, vNormal, 4 ) );
	UpdateLightmaps( l );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightState::TraceParallel()
{
	CVec3 vEnter, vNormal;
	if ( !TraceFromCamera( &vEnter, &vNormal ) )
		return;
	CVec3 vDir = vEnter - pCamera->GetCP(); // for sun direction will look down
	Normalize( &vDir );

	SLightSet l;
	l.parallel.push_back( SParallelLight( CVec3(0,1,0), vEnter, vDir, 11 ) );
	UpdateLightmaps( l );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightmapTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeLMTS( CTransformStack *pTS, CVec4 &vZ )
{
	CTransformStack &ts = *pTS;
	SHMatrix m;
	int nRes = 1024;//shadowMapsShare.GetLMTexResolution();
	NGfx::MakeLMToScreenMatrix( &m, nRes, nRes );
	m.z = vZ;
	ts.Init( m );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLightmapTracker::CLightmapTracker() : nLights(1), nPassesPerCalc(2), groupSelect(0,0), bLightStateUpdated(true)
{
	//LoadShit( pLMTextureCache->GetTexture() );
	currentBound.SphereInit( CVec3(10,10,10), 20 );
	Zero( mPrevView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLightmapTracker::GetSkyTexturesNum()
{
	return shadowMapsShare.GetCLSkyTexturesNumber();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderLight( 
	SLightmapTargetGeom *pTarget, const SLightInfo &lightInfo,
	ERenderOperation op, CRenderCmdList::UParameter param1, CRenderCmdList::UParameter param2,
	int nStencilOp, CRenderCmdList::UParameter param3 )
{
	NGfx::CRenderContext &rc = *pTarget->pRC;
	//render;
	//rc.SetCulling( NGfx::CULL_NONE );

	CRenderCmdList res;
	const vector<SRenderFragmentInfo*> &fragments = pTarget->pGeom->GetFragments();
	for ( int i = 1; i < fragments.size(); ++i )
	{
		if ( pTarget->pGeom->IsFilteredFragment( i ) )
			continue;
		const SRenderFragmentInfo &frag = *fragments[i];
		if ( (op == RO_CL_COPY_LAST || op == RO_CL_TEST_PREV_FRAME) &&
			!frag.pMaterial->GetMaterialInfo().IsSolid() )
			continue;
		SOpGenContext fi( &res.ops, &frag );
		fi.AddOperation( op, 100, nStencilOp, pTarget->nTargetRegister, param1, param2, param3 );
	}
	Execute( 0, &rc, *pTarget->pTS, res, *pTarget->pGeom, lightInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitLightInfo( SLightInfo *pRes, const CVec3 &_vCenter, float fRadius, const CVec3 &_vColor )
{
	SLightInfo &lightInfo = *pRes;
	lightInfo.bNeedSet = true;
	lightInfo.vLightColor = CVec4( _vColor, 0 );
	lightInfo.vLightPos = CVec4( _vCenter, 0 );
	// Retail v1.2 0x52b279: cached lights use inverse-square distance
	// attenuation, not the legacy projected-circle light's radius constants.
	const float fInvRadius = 1 / fRadius;
	lightInfo.vRadius = CVec4( fRadius, 1, fInvRadius, 36 * fInvRadius * fInvRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderCubeMapDepth(
	SLightmapTargetGeom *pTarget, 
	const CVec3 &_vCenter, float fRadius, int nDir, CCubeTextureChannel *pChannel )
{
	SLightInfo lightInfo;
	InitLightInfo( &lightInfo, _vCenter, fRadius, CVec3(0,0,0) );
	// render occluders
	NGfx::CCubeTexture *pDepth = pChannel ? pChannel->pTexture.GetPtr() : shadowMapsShare.GetCubeDepth();
	if ( !IsValid(pDepth) )
		return;
	if ( nDir == 0 )
	{
		// Retail v1.2 0x52e4e3..0x52e5fe builds receiver visibility once
		// per cube, independent of the viewing camera and active floor.
		CPointHSRParts &receivers = pointHSR[SPointLightPos(_vCenter, fRadius)];
		list<SRenderPartSet> parts;
		GeneratePartList( pRender, _vCenter, fRadius, &parts, IRender::DT_STATIC, SGroupSelect(0xfff, 0) );
		for ( list<SRenderPartSet>::const_iterator i = parts.begin(); i != parts.end(); ++i )
			receivers[i->pNode].Init( i->parts, *i->pParts );
	}
	CDynamicCast<NGfx::ICubeBuffer> pDepthBuffer( pDepth );
	const int nResolution = pDepthBuffer->GetSize();
	if ( pChannel )
		shadowMapsShare.TouchCubeChannel(pChannel);
	//for ( int k = 0; k < 6; ++k )
	{
		NGfx::CRenderContext rc;
		NGfx::EFace face;
		CTransformStack ts;
		ts.MakeProjective( 1 );
		SHMatrix camera;
		CVec4 vX( 1, 0, 0, -_vCenter.x );
		CVec4 vY( 0, 1, 0, -_vCenter.y );
		CVec4 vZ( 0, 0, 1, -_vCenter.z );
		switch ( nDir )
		{
			case 0: face = NGfx::POSITIVE_X; camera.x = -vZ; camera.y = -vY; camera.z =  vX; break;
			case 1: face = NGfx::POSITIVE_Y; camera.x =  vX; camera.y =  vZ; camera.z =  vY; break;
			case 2: face = NGfx::POSITIVE_Z; camera.x =  vX; camera.y = -vY; camera.z =  vZ; break;
			case 3: face = NGfx::NEGATIVE_X; camera.x =  vZ; camera.y = -vY; camera.z = -vX; break;
			case 4: face = NGfx::NEGATIVE_Y; camera.x =  vX; camera.y = -vZ; camera.z = -vY; break;
			case 5: face = NGfx::NEGATIVE_Z; camera.x = -vX; camera.y = -vY; camera.z = -vZ; break;
			default: ASSERT(0);
		}
		// gather occluders
		CVec3 vDir = CVec3( camera.zx, camera.zy, camera.zz );
		CSceneFragments geom;
		CTransformStack tsFrustrum;
		tsFrustrum.MakeProjective( 1, 90, 0.01f, fRadius );
		SHMatrix mCubeCenter;
		MakeMatrix( &mCubeCenter, _vCenter, vDir );
		tsFrustrum.SetCamera( mCubeCenter );
		// Retail v1.2 0x52eb54 calls IRender::FormDepthList (vtbl +4), not
		// FormDirOccludersList. The latter substitutes simplified occluders for
		// off-camera nodes, making cached lamp shadows depend on camera history.
		pRender->FormDepthList( &tsFrustrum, vDir, &geom, IRender::DT_STATIC );
		
		// form transform stack for render
		camera.y = -camera.y;
		camera.w = CVec4(0,0,0,1);
		ts.Push43( camera );
		camera = ts.Get().forward;
		camera.x = camera.x - (1.0f / nResolution ) * camera.w;
		camera.y = camera.y + (1.0f / nResolution ) * camera.w;
		ts.Init( camera );
		rc.SetCubeTextureRT( pDepth, face, 0 );
		if ( pChannel )
		{
			// D3D Clear ignores COLORWRITEENABLE. Clear only this lease's
			// channel with a fullscreen quad, preserving the other three lights.
			rc.ClearZBuffer();
			rc.SetColorWrite( (NGfx::EColorWriteMask)pChannel->GetWriteMask() );
			CRectLayout white;
			white.AddRect(0, 0, nResolution, nResolution, CTRect<float>(0,0,nResolution,nResolution), NGfx::SPixel8888(255,255,255,255));
			NGfx::C2DQuadsRenderer qr(rc, CVec2(nResolution,nResolution), NGfx::QRM_OVERWRITE|NGfx::QRM_SOLID);
			RenderRectLayout(&qr, 0, white);
		}
		else
			rc.ClearBuffers( 0xffffffff );
		rc.SetCulling( NGfx::CULL_CCW );

		CRenderCmdList dp;
		MakeSingleOp( &dp, geom, true, RO_PNT_CUBEMAP_DEPTH );
		Execute( 0, &rc, ts, dp, geom, lightInfo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::DownsampleCubeMapDepth( const CVec3 &_vCenter, float _fRadius )
{
	CCubeTextureChannel *pChannel = GetPointDepth( _vCenter, _fRadius );
	NGfx::CCubeTexture *pDepth = pChannel->pTexture;
	shadowMapsShare.TouchCubeChannel(pChannel);
	NGfx::CCubeTexture *pSrc = shadowMapsShare.GetCubeDepth();
	NGfx::CRenderContext rc;
	SFBTransform id;
	Identity( &id.forward );
	Identity( &id.backward );
	rc.SetTransform( id );
	NGfx::STriangleList quad;
	// Retail v1.2 0x52d679 passes one RECT (two triangles), not two rects.
	// There are only four vertices below; a second quad reads unrelated pooled
	// vertices and corrupts the coarse cache with camera-dependent shapes.
	NGfx::MakeQuadTriList( 1, &quad );
	NGfx::SGeomVecFull v;
	Zero( v );
	CVec3 vX(1,0,0), vY(0,1,0), vZ(0,0,1);
	for ( int k = 0; k < 6; ++k )
	{
		CVec3 vCX, vCY, vCZ;
		NGfx::EFace face;
		switch ( k )
		{
			case 0: face = NGfx::POSITIVE_X; vCX = -vZ; vCY = -vY; vCZ =  vX; break;
			case 1: face = NGfx::POSITIVE_Y; vCX =  vX; vCY =  vZ; vCZ =  vY; break;
			case 2: face = NGfx::POSITIVE_Z; vCX =  vX; vCY = -vY; vCZ =  vZ; break;
			case 3: face = NGfx::NEGATIVE_X; vCX =  vZ; vCY = -vY; vCZ = -vX; break;
			case 4: face = NGfx::NEGATIVE_Y; vCX =  vX; vCY = -vZ; vCZ = -vY; break;
			case 5: face = NGfx::NEGATIVE_Z; vCX = -vX; vCY = -vY; vCZ = -vZ; break;
			default: ASSERT(0);
		}
		CVec3 vShift( -0.48f * 2.0f / GetCLCubeResolution(), +0.48f * 2.0f / GetCLCubeResolution(), 0 );
//		CVec3 vShift( 0,0,0 );
		CObj<NGfx::CGeometry> pGeom;
		{
			NGfx::CBufferLock<NGfx::SGeomVecFull> geom( &pGeom, 4 );
			v.pos = CVec3(-1,-1,0.5f) + vShift;
			NGfx::CalcCompactVector( &v.normal, vCZ - vCX + vCY );
			geom[0] = v;
			v.pos = CVec3( 1,-1,0.5f) + vShift;
			NGfx::CalcCompactVector( &v.normal, vCZ + vCX + vCY );
			geom[1] = v;
			v.pos = CVec3( 1, 1,0.5f) + vShift;
			NGfx::CalcCompactVector( &v.normal, vCZ + vCX - vCY );
			geom[2] = v;
			v.pos = CVec3(-1, 1,0.5f) + vShift;
			NGfx::CalcCompactVector( &v.normal, vCZ - vCX - vCY );
			geom[3] = v;
		}
		rc.SetCubeTextureRT( pDepth, face, 0 );
		rc.SetColorWrite( (NGfx::EColorWriteMask)pChannel->GetWriteMask() );
		{
			// Retail 0x52de5b clears only this channel, including its depth,
			// before copying. D3D Clear would overwrite the other three lights.
			const int nResolution = GetCLCubeResolution();
			CRectLayout black;
			black.AddRect( 0, 0, nResolution, nResolution,
				CTRect<float>(0, 0, nResolution, nResolution), NGfx::SPixel8888(0,0,0,0) );
			NGfx::C2DQuadsRenderer qr( rc, CVec2(nResolution, nResolution), NGfx::QRM_OVERWRITE|NGfx::QRM_SOLID );
			RenderRectLayout( &qr, 0, black );
		}
		NGfx::SEffRenderCubemap effScale;
		effScale.pTex = pSrc;
		rc.SetEffect( &effScale );
		rc.DrawPrimitive( pGeom, quad );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderPointLightShadowed( 
	SLightmapTargetGeom *pTarget, 
	const CVec3 &_vCenter, float fRadius, const CVec3 &_vColor,
	NGfx::CCubeTexture *pDepth, int nDepthBias, bool bFast, int nChannel )
{
	if ( fabs2(_vColor) == 0 || fRadius < 0.1f )
		return;
	if ( !IsValid( pDepth ) )
	{
		RenderPointNoShadows( pTarget, _vCenter, fRadius, _vColor );
		return;
	}
	SBound bTarget;
	bTarget.SphereInit( _vCenter, fRadius );
	auto receivers = pointHSR.find(SPointLightPos(_vCenter, fRadius));
	CSelectGeometries selector( pTarget->pGeom,
		SIgnoredSphereFilter(receivers == pointHSR.end() ? 0 : &receivers->second, bTarget.s) );
	if ( !pTarget->pGeom->HasSelectedFragments() )
		return;
	SLightInfo lightInfo;
	InitLightInfo( &lightInfo, _vCenter, fRadius, _vColor );

	if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
	{
		// Retail v1.2 0x52d114: the fast update still uses bump mapping when
		// the quality preset requests it on every cached-light update.
		const bool bUseBump = NGlobal::GetVar( "gfx_cl_use_bump", 1 ).GetFloat() != 0 &&
			( !bFast || NGlobal::GetVar( "gfx_cl_use_bump_always", 1 ).GetFloat() != 0 );
		if ( !bUseBump )
		{
			pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
			RenderLight( pTarget, lightInfo, RO_CL_PNT_LIGHT_SHADOWED, pDepth, (float)nDepthBias, DPM_EQUAL|ABM_ADD, float(nChannel) );
		}
		else
		{
			pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_NONE );
			RenderLight( pTarget, lightInfo, RO_CL_PNT_DEPTH_CHECK, pDepth, (float)nDepthBias, DPM_EQUAL|STM_LIGHT, float(nChannel) );
			pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
			CRenderCmdList alphaTestOps;
			const vector<SRenderFragmentInfo*> &fragments = pTarget->pGeom->GetFragments();
			for ( int i = 1; i < fragments.size(); ++i )
			{
				if ( pTarget->pGeom->IsFilteredFragment( i ) )
					continue;
				const SRenderFragmentInfo &frag = *fragments[i];
				SOpGenContext op( &alphaTestOps.ops, &frag );
				const SMaterialInfo &info = frag.pMaterial->GetMaterialInfo();
				if ( info.pBump )
					op.AddOperation( RO_CL_PNT_LIGHT_BUMP, 10, DPM_EQUAL|STM_TEST_CLEAR_MARK|ABM_ADD, pTarget->nTargetRegister, info.pBump );
				else
					op.AddOperation( RO_CL_PNT_LIGHT, 10, DPM_EQUAL|STM_TEST_CLEAR_MARK|ABM_ADD, pTarget->nTargetRegister );
			}
			Execute( 0, pTarget->pRC, *pTarget->pTS, alphaTestOps, *pTarget->pGeom, lightInfo );
		}
	}
	else
	{
		pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_NONE );
		RenderLight( pTarget, lightInfo, RO_CL_PNT_DEPTH_CHECK, pDepth, (float)nDepthBias, DPM_EQUAL|STM_LIGHT );

		pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
		RenderLight( pTarget, lightInfo, RO_CL_PNT_LIGHT, 0.0f, 0.0f, DPM_EQUAL|ABM_ADD|STM_TEST_CLEAR_MARK );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderPointNoShadows( SLightmapTargetGeom *pTarget, 
	const CVec3 &_vCenter, float fRadius, const CVec3 &_vColor )
{
	if ( fabs2(_vColor) == 0 || fRadius < 0.1f )
		return;
	SBound bTarget;
	bTarget.SphereInit( _vCenter, fRadius );
	CSelectGeometries selector( pTarget->pGeom, SBoundIntersectFilter( bTarget ) );
	if ( !pTarget->pGeom->HasSelectedFragments() )
		return;
	SLightInfo lightInfo;
	InitLightInfo( &lightInfo, _vCenter, fRadius, _vColor );

	pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
	RenderLight( pTarget, lightInfo, RO_CL_PNT_LIGHT, 0.0f, 0.0f, DPM_EQUAL|ABM_ADD );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static void RenderParallelDepth( IRender *pRender, NGfx::CRenderContext *pRC, 
	const SSphere &_bound, const CVec3 &vDir, const SGroupSelect &_groupSelect,
	SDirectionalDepthInfo *pDepthInfo, bool bFast )
{
	//	const float F_RANGE = 40;
	const CVec3 &vEnter = _bound.ptCenter;
	float fWidth = 2 * _bound.fRadius;//p.fWidth;
	float fRange = 2 * _bound.fRadius;
	// setup projection for depth map
	CTransformStack ts;
	ts.MakeParallel( fWidth, fWidth, -1000, 1000 );
	// fill depth info
	SHMatrix cameraPos;
	MakeMatrix( &cameraPos, vEnter, vDir );
	ts.SetCamera( cameraPos );
	//vLightDir = ptCenter;
	SHMatrix mTrans = ts.Get().forward;
	SDirectionalDepthInfo &depthInfo = *pDepthInfo;
	NGfx::GetTexMapFromProjection( &mTrans, N_DEFAULT_RT_RESOLUTION );
	depthInfo.vVecU = mTrans.x;
	depthInfo.vVecV = mTrans.y;
	float fRange1 = 1.0f / fRange;
	//depthInfo.vDepth = CVec4( -vDir * fRange1, 0.5f + fRange1 * ( vDir * vEnter ) );//0, 0, 1 / 20.0f, 0 ); //_fMaxHeight
	depthInfo.vDepth = CVec4( 0, 0, 1 / F_MAX_SCENE_HEIGHT, 0 );

	SLightInfo lightInfo;
	CSceneFragments geom;
	CRenderCmdList res;
	pRender->FormDirOccludersList( &ts, vDir, &geom, _groupSelect, bFast );
	MakeSingleOp( &res, geom, true, RO_DIR_DEPTH, &depthInfo );
	Execute( pRender, pRC, ts, res, geom, lightInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RecalcDepthChannel( int nBuffer, int nChannel, bool bFast )
{
	NGfx::CRenderContext rcDepth;
	NGfx::CTexture *pDepth = shadowMapsShare.GetLMDepthBuffer( nBuffer );
	if ( nChannel == N_ALL_DEPTH_CHANNELS )
	{
		rcDepth.SetTextureRT( pDepth );
		rcDepth.ClearBuffers( 0 );
	}
	else
	{
		int nMask = 0;
		for ( int i = 0; i < N_DEPTH_CHANNELS_PER_TEX; ++i )
		{
			if ( nChannel & (1<<i)  )
				nMask |= depthChannels[i];
		}
		SetAndClearRT( &rcDepth, pDepth, nMask, N_DEFAULT_RT_RESOLUTION );
	}
	bool bHasRendered = false;
	for ( int i = 0; i < N_DEPTH_CHANNELS_PER_TEX; ++i )
	{
		if ( ( nChannel & (1<<i) ) == 0 )
			continue;
		int nInfoIdx = nBuffer * N_DEPTH_CHANNELS_PER_TEX + i;
		const CVec3 &vDir = skyDirs[ nInfoIdx ];
		rcDepth.SetColorWrite( depthChannels[ i ] );
		if ( bHasRendered )
			rcDepth.ClearZBuffer();
		RenderParallelDepth( pRender, &rcDepth, currentBound.s, vDir, groupSelect, &depthInfos[ nInfoIdx ], bFast );
		bHasRendered = true;
		CVec4 &vChannel = depthInfos[ nInfoIdx ].vChannelSelect;
		vChannel = CVec4(0,0,0,0);
		vChannel.m[i] = 1;
	}
	rcDepth.SetColorWrite( NGfx::COLORWRITE_ALL );
	// depth map border
	DrawBorder( &rcDepth, N_DEFAULT_RT_RESOLUTION );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderSkyCheck( SLightmapTargetGeom *pTarget, float fStrength, int nBuffer, bool bFast )
{
	NGfx::CRenderContext &rc = *pTarget->pRC;
	SLightInfo lightInfo;
	lightInfo.bNeedSet = true;
	float f = fStrength;
	lightInfo.vLightColor = CVec4(f,f,f,f);

	int nBase = nBuffer * N_DEPTH_CHANNELS_PER_TEX;

	if ( 1 )//bFast )
	{
		rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
		if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
		{
			SSkyDepth3Info depthInfo;
			depthInfo.channels[0] = &depthInfos[ nBase + 0 ];
			depthInfo.channels[1] = &depthInfos[ nBase + 1 ];
			depthInfo.channels[2] = &depthInfos[ nBase + 2 ];
			depthInfo.vDirs[0] = -skyDirs[ nBase + 0 ];
			depthInfo.vDirs[1] = -skyDirs[ nBase + 1 ];
			depthInfo.vDirs[2] = -skyDirs[ nBase + 2 ];
			RenderLight( pTarget, lightInfo, RO_CL_SKY_3LIGHT, 
				&depthInfo, shadowMapsShare.GetLMDepthBuffer( nBuffer ), DPM_EQUAL|ABM_ADD );
		}
		else
		{
			for ( int k = 0; k < 3; ++k )
			{
				lightInfo.vLightPos = CVec4( -skyDirs[ nBase + k ], 0 );
				rc.SetColorWrite( NGfx::COLORWRITE_NONE );
				RenderLight( pTarget, lightInfo, RO_CL_SKY_DIR_CHECK, 
					&depthInfos[ nBase + k ], shadowMapsShare.GetLMDepthBuffer( nBuffer ), DPM_EQUAL|STM_LIGHT );
				rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
				RenderLight( pTarget, lightInfo, RO_CL_SKY_LIGHT, 
					0.0f, 0.0f, DPM_EQUAL|STM_TEST_CLEAR_MARK|ABM_ADD );
			}
		}
	}
	else
	{
		for ( int k = 0; k < 3; ++k )
		{
			lightInfo.vLightPos = CVec4( -skyDirs[ nBase + k ], 0 );
			rc.SetColorWrite( NGfx::COLORWRITE_NONE );
			RenderLight( pTarget, lightInfo, RO_CL_SKY_DIR_CHECK, 
				&depthInfos[ nBase + k ], shadowMapsShare.GetLMDepthBuffer( nBuffer ), DPM_EQUAL|STM_LIGHT );
			rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
			CRenderCmdList alphaTestOps;
			const vector<SRenderFragmentInfo*> &fragments = pTarget->pGeom->GetFragments();
			for ( int i = 1; i < fragments.size(); ++i )
			{
				if ( pTarget->pGeom->IsFilteredFragment( i ) )
					continue;
				const SRenderFragmentInfo &frag = *fragments[i];
				SOpGenContext op( &alphaTestOps.ops, &frag );
				const SMaterialInfo &info = frag.pMaterial->GetMaterialInfo();
				if ( info.pBump && NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
					op.AddOperation( RO_CL_SKY_LIGHT_BUMP, 10, DPM_EQUAL|STM_TEST_CLEAR_MARK|ABM_ADD, pTarget->nTargetRegister, info.pBump );
				else
					op.AddOperation( RO_CL_SKY_LIGHT, 10, DPM_EQUAL|STM_TEST_CLEAR_MARK|ABM_ADD, pTarget->nTargetRegister );
			}
			Execute( 0, pTarget->pRC, *pTarget->pTS, alphaTestOps, *pTarget->pGeom, lightInfo );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::ChooseNewSkyDirection( int nBuffer, int nTarget )
{
	skyDirs[ nBuffer * N_DEPTH_CHANNELS_PER_TEX + nTarget ] = lightState.GenerateSkyDir( &ambientLightSeed );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::ChooseNewSkyDirections()
{
	for ( int k = 0; k < GetSkyTexturesNum(); ++k )
		for ( int i = 0; i < N_DEPTH_CHANNELS_PER_TEX; ++i )
			ChooseNewSkyDirection( k, i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::FinishRecalc()
{
	rs.bColorReady = false;
	rs.nState = RC_START;
	rs.bCalcSky = true;
	rs.bCalcColor = !rs.bCalcColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCubeTextureChannel *CLightmapTracker::GetPointDepth( const CVec3 &vCenter, float fRadius )
{
	CObj<CCubeTextureChannel> &p = pointDepths[SPointLightPos(vCenter, fRadius)];
	if ( !IsValid(p) || !IsValid(p->pTexture) )
		p = shadowMapsShare.AllocCubeChannel();
	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::SortLights( vector<int> *pOrder )
{
	pOrder->resize(lightState.points.size());
	vector<int> keys(lightState.points.size());
	for ( int i = 0; i < lightState.points.size(); ++i )
	{
		(*pOrder)[i] = i;
		const CLightState::SPointLight &light = lightState.points[i];
		int key = 10;
		if ( light.bCastShadow )
		{
			CCubeTextureChannel *p = pointDepths[SPointLightPos(light.vCenter, light.fRadius)];
			key = 0;
			if ( IsValid(p) && IsValid(p->pTexture) )
			{
				shadowMapsShare.TouchCubeChannel(p);
				key = (int)(size_t)p->pTexture.GetPtr();
				if ( NGfx::GetHardwareLevel() == NGfx::HL_GFORCE3 && p->nChannel == 0 )
					key |= 0x80000000;
			}
		}
		keys[i] = key;
	}
	std::sort(pOrder->begin(), pOrder->end(), [&keys](int a, int b) { return keys[a] > keys[b]; });
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderCachedPoints( SLightmapTargetGeom *pTarget )
{
	vector<int> order;
	SortLights(&order);
	for ( int k = 0; k < order.size(); ++k )
	{
		const CLightState::SPointLight &p = lightState.points[order[k]];
		CCubeTextureChannel *channel = p.bCastShadow ? pointDepths[SPointLightPos(p.vCenter, p.fRadius)].GetPtr() : 0;
		RenderPointLightShadowed( pTarget, p.vCenter, p.fRadius, p.vColor,
			IsValid(channel) ? channel->pTexture.GetPtr() : 0, 3, true, IsValid(channel) ? channel->nChannel : 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::BeginPointRecalc()
{
	rs.nState = UsePrecisePointShadows() ? RC_COLOR_POINT : RC_DEPTH_POINT;
	rs.nStep = 0;
	if ( !lightState.points.empty() )
		nPointLight %= lightState.points.size();
	else
		nPointLight = 0;
	nPointLightStart = nPointLight;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RecalcStep( NGfx::CRenderContext *pRC, CSceneFragments *pScene, CTransformStack *pTS, bool bSoftApply, int nScratchRegister )
{
	ASSERT( GetSkyTexturesNum() == 0 || ( nPassesPerCalc % GetSkyTexturesNum() ) == 0 );
	SLightmapTargetGeom lmTarget( pScene, pRC, pTS, N_CL_TEMP_REGISTER );
	switch ( rs.nState )
	{
		case RC_START:
			{
				rs.nStep = 0;
				if ( rs.bCalcSky )
					rs.nState = RC_SKY_DEPTH;
				else if ( rs.bCalcColor )
					BeginPointRecalc();
				else
				{
					ASSERT( 0 && "asked to recalc no lightmaps" );
					return; 
				}
				lmTarget.pRC->SetRegister( N_CL_TEMP_REGISTER );
				if ( CanDrawSky() )
					lmTarget.pRC->ClearTarget( N_DARKEST_AREA << 24 );
				else
					lmTarget.pRC->ClearTarget( 0xff000000 );
			}
			break;
		case RC_SKY_DEPTH:
			// calc sky map
			{
				if ( !CanDrawSky() )
					BeginPointRecalc();
				else
				{
					int nChannel = rs.nStep % ( N_DEPTH_CHANNELS_PER_TEX + 1 );
					int nBuf = ( rs.nStep / ( N_DEPTH_CHANNELS_PER_TEX + 1 ) ) % GetSkyTexturesNum();
					if ( nChannel < N_DEPTH_CHANNELS_PER_TEX && rs.nStep < nPreparedSkySteps )
					{
						++rs.nStep;
						RecalcStep( pRC, pScene, pTS, bSoftApply, nScratchRegister );
						return;
					}
					if ( nChannel < N_DEPTH_CHANNELS_PER_TEX )
					{
						if ( nChannel == 0 && nBuf == 0 )
							ChooseNewSkyDirections();
						RecalcDepthChannel( nBuf, 1 << nChannel, false );
					}
					else
						RenderSkyCheck( &lmTarget, F_SKY_SINGLE_STRENGTH_MUL / nPassesPerCalc / N_DEPTH_CHANNELS_PER_TEX, nBuf, false );
					++rs.nStep;
					if ( rs.nStep == (N_DEPTH_CHANNELS_PER_TEX+1) * nPassesPerCalc )
					{
						nPreparedSkySteps = 0;
						if ( rs.bCalcColor )
						{
							BeginPointRecalc();
						}
						else
							rs.nState = RC_APPLY;
					}
				}
			}
			break;
		case RC_COLOR_POINT:
			{
				int nStep = rs.nStep % N_POINT_LIGHT_RECALC_STEPS;
				int nCount = lightState.points.size();
				if ( nCount > 0 )
				{
					const CLightState::SPointLight &p = lightState.points[ nPointLight % nCount ];
					if ( !p.bCastShadow )
					{
						RenderPointNoShadows( &lmTarget, p.vCenter, p.fRadius, p.vColor );
						rs.nStep += N_POINT_LIGHT_RECALC_STEPS;
						++nPointLight;
					}
					else if ( nStep < 3 )
					{
						RenderCubeMapDepth( &lmTarget, p.vCenter, p.fRadius, nStep * 2 );
						RenderCubeMapDepth( &lmTarget, p.vCenter, p.fRadius, nStep * 2 + 1 );
						++rs.nStep;
					}
					else
					{
						DownsampleCubeMapDepth( p.vCenter, p.fRadius );
						RenderPointLightShadowed( &lmTarget, p.vCenter, p.fRadius, p.vColor,
							shadowMapsShare.GetCubeDepth(), 2, false );
						++nPointLight;
						++rs.nStep;
					}
				}
				if ( nCount == 0 || ( nPointLight != nPointLightStart && nPointLight % nCount == nPointLightStart % nCount ) )
				{
					nPointLight = nPointLightStart = 0;
					rs.nStep = 0;
					rs.nState = RC_APPLY;
					rs.bColorReady = true;
				}
			}
			break;
		case RC_APPLY:
			// store to lightmap
			{
				NGfx::CRenderContext rc( *lmTarget.pRC );
				rc.SetAlphaCombine( NGfx::COMBINE_NONE );
				rc.SetDepth( NGfx::DEPTH_NONE );
				rc.SetStencil( NGfx::STENCIL_NONE );
				rc.SetColorWrite( NGfx::COLORWRITE_ALL );
				const int nApplyRegister = bSoftApply ? nScratchRegister : N_CL_TARGET_REGISTER;
				if ( bSoftApply )
					NGfx::CopyLightmapRegister( &rc, nApplyRegister, N_CL_TARGET_REGISTER );
				if ( !rs.bCalcSky )
				{
					rc.SetColorWrite( NGfx::COLORWRITE_COLOR );
					NGfx::AlphaSqrtModulateRegister( &rc, nApplyRegister, N_CL_TEMP_REGISTER, 1 );
				}
				else
				{
					if ( nLights > 0 )
					{
						// blend with previous result
						nLights = Min( nLights, 64 );
						float fNewBlend = ((float)nPassesPerCalc ) / ( nPassesPerCalc + nLights );
						if ( rs.bColorReady )
						{
							CVec4 vOldBlend( 0, 0, 0, 1 - fNewBlend );
							NGfx::ModulateRegister( &rc, nApplyRegister, vOldBlend );
							rc.SetAlphaCombine( NGfx::COMBINE_ADD );
							NGfx::AlphaSqrtModulateRegister( &rc, nApplyRegister, N_CL_TEMP_REGISTER, fNewBlend );
						}
						else
						{
							CVec4 vOldBlend( 1, 1, 1, 1 - fNewBlend );
							NGfx::ModulateRegister( &rc, nApplyRegister, vOldBlend );
							rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
							rc.SetAlphaCombine( NGfx::COMBINE_ADD );
							NGfx::AlphaSqrtModulateRegister( &rc, nApplyRegister, N_CL_TEMP_REGISTER, fNewBlend );
						}
					}
					else
					{
						// simple store
						if ( !rs.bColorReady )
							rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
						NGfx::AlphaSqrtModulateRegister( &rc, nApplyRegister, N_CL_TEMP_REGISTER, 1 );
					}
					nLights += nPassesPerCalc;
					nPassesPerCalc *= 2;
					if ( nPassesPerCalc > 32 )
						nPassesPerCalc = 32;
				}
				if ( bSoftApply )
				{
					rc.SetColorWrite( NGfx::COLORWRITE_ALL );
					rc.SetAlphaCombine( NGfx::COMBINE_NONE );
					NGfx::CopyLightmapRegister( &rc, N_CL_TEMP_REGISTER, nApplyRegister );
					rs.nState = RC_SOFT_APPLY;
					rs.nStep = 0;
				}
				else
					FinishRecalc();
			}
			break;
		case RC_DEPTH_POINT:
			{
				const int nCount = lightState.points.size();
				const int nFace = rs.nStep % 6;
				if ( nCount > 0 )
				{
					const CLightState::SPointLight &p = lightState.points[nPointLight % nCount];
					if ( p.bCastShadow )
						RenderCubeMapDepth( &lmTarget, p.vCenter, p.fRadius, nFace, GetPointDepth(p.vCenter, p.fRadius) );
					if ( nFace == 5 )
						++nPointLight;
					++rs.nStep;
				}
				if ( nCount == 0 || ( nPointLight != nPointLightStart && nPointLight % nCount == nPointLightStart % nCount ) )
				{
					nPointLight = nPointLightStart = 0;
					rs.nStep = 0;
					rs.nState = RC_APPLY;
					RenderCachedPoints(&lmTarget);
					rs.bColorReady = true;
				}
			}
			break;
		case RC_SOFT_APPLY:
			{
				// Retail 0x52f5d4: prepare next sky sweep while blending this one.
				if ( CanDrawSky() )
				{
					if ( rs.nStep == 0 )
						ChooseNewSkyDirections();
					int nBuf = rs.nStep / N_DEPTH_CHANNELS_PER_TEX;
					int nChannel = rs.nStep % N_DEPTH_CHANNELS_PER_TEX;
					if ( nBuf < GetSkyTexturesNum() )
						RecalcDepthChannel( nBuf, 1 << nChannel, false );
					nPreparedSkySteps = nBuf * (N_DEPTH_CHANNELS_PER_TEX + 1) + nChannel + 1;
				}
				NGfx::CRenderContext rc( *pRC );
				rc.SetColorWrite( NGfx::COLORWRITE_ALL );
				rc.SetDepth( NGfx::DEPTH_NONE );
				rc.SetStencil( NGfx::STENCIL_NONE );
				if ( ++rs.nStep >= 12 )
				{
					rc.SetAlphaCombine( NGfx::COMBINE_NONE );
					NGfx::CopyLightmapRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER );
					FinishRecalc();
				}
				else
				{
					float fRemaining = 1 - rs.nStep * (1.f / 12.f);
					float fOld = fRemaining / ( fRemaining + 1.f / 12.f );
					NGfx::ModulateRegister( &rc, N_CL_TARGET_REGISTER, CVec4(fOld,fOld,fOld,fOld) );
					rc.SetAlphaCombine( NGfx::COMBINE_ADD );
					NGfx::CopyLightmapRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, 1 - fOld );
				}
			}
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The screen registers belong to the most recently rendered scene, not to its tracker.
static CPtr<CLightmapTracker> pPreviousCLTracker;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void PrepareCLHistory( NGfx::CRenderContext *pRC, int nHistoryRegister )
{
	NGfx::CRenderContext rc( *pRC );
	CTRect<float> size;
	NGfx::GetRegisterSize( &size );
	rc.SetVirtualRT();
	rc.SetRegister( nHistoryRegister );
	rc.SetColorWrite( NGfx::COLORWRITE_ALL );
	rc.SetStencil( NGfx::STENCIL_NONE );
	rc.SetAlphaCombine( NGfx::COMBINE_NONE );
	{
		// White depth cannot match ordinary scene geometry. Clamp sampling outside
		// the old viewport must hit this border, never reuse an edge pixel forever.
		NGfx::C2DQuadsRenderer qr( rc, CVec2(size.x2, size.y2), NGfx::QRM_SOLID );
		qr.AddRect( CTRect<float>(0, 0, size.x2, 1), 0, size );
		qr.AddRect( CTRect<float>(0, size.y2 - 1, size.x2, size.y2), 0, size );
		qr.AddRect( CTRect<float>(0, 0, 1, size.y2), 0, size );
		qr.AddRect( CTRect<float>(size.x2 - 1, 0, size.x2, size.y2), 0, size );
	}
	// Clear only our scratch stencil bit, preserving the sun/point-light bit.
	rc.SetColorWrite( NGfx::COLORWRITE_NONE );
	rc.SetStencil( NGfx::STENCIL_WRITE, 0, 0x40 );
	NGfx::C2DQuadsRenderer qr( rc, CVec2(size.x2, size.y2), NGfx::QRM_SOLID );
	qr.AddRect( size, 0, size );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::CatchUp( NGfx::CRenderContext *_pRC, IRender *_pRender, CTransformStack *pTS, CSceneFragments *pScene,
	bool bHasNewLightmaps, const SGroupSelect &_gs, bool bFirstCatch,
	const CVec4 &vDepth, bool bReuseLight, int nScratchRegister )
{
	shadowMapsShare.NextCubeFrame();
	NGfx::CRenderContext rc( *_pRC );
	pRender = _pRender;
	bReuseLight = bReuseLight && NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3;
	const bool bHistoryValid = bReuseLight && pPreviousCLTracker == this &&
		pHistoryDepth == NGfx::GetRegisterTexture( N_CL_DEPTH_REGISTER ) &&
		pHistoryLight == NGfx::GetRegisterTexture( N_CL_TARGET_REGISTER );
	// Retail CatchUp (v1.2 0x52fb72) reprojects even when static geometry or
	// illumination requests a refresh. A bullet impact/muzzle flash must restart
	// refinement, not replace every unchanged surface with the coarse sky pass.
	// The depth test rejects newly exposed surfaces; point RGB is rebuilt below.
	const bool bKeepPrevious = bHistoryValid && !(_gs != groupSelect);
	if ( pPreviousCLTracker != this || (bReuseLight && !bHistoryValid) || bLightStateUpdated )
		bHasNewLightmaps = true;
	bLightStateUpdated = false;
	bool bRecalcAllDepth = false;
	if ( _gs != groupSelect )
	{
		// floor changed - everything must be recalculated
		bRecalcAllDepth = true;
		groupSelect = _gs;
		bHasNewLightmaps = true;
	}
	if ( memcmp( &pTS->Get().forward, &mPrevView, sizeof(mPrevView) ) != 0 )
	{
		bHasNewLightmaps = true;
	}
	CSelectFragments filterLightmapped( pScene, SLightmappedFilter() );
	// on first update select sky directions
	if ( depthInfos.empty() )
	{
		depthInfos.resize( GetSkyTexturesNum() * N_DEPTH_CHANNELS_PER_TEX );
		skyDirs.resize( GetSkyTexturesNum() * N_DEPTH_CHANNELS_PER_TEX );
		for ( int k = 0; k < GetSkyTexturesNum(); ++k )
		{
			for ( int i = 0; i < N_DEPTH_CHANNELS_PER_TEX; ++i )
				ChooseNewSkyDirection( k, i );
		}
		bRecalcAllDepth = true;
	}
	// calc new scene bound & check if need to recalc depths
	SBound bNew;
	MakeSceneGeometryBound( &bNew, *pTS, F_MAX_SCENE_HEIGHT );
	if ( !IsInside( currentBound, bNew ) )
	{
		currentBound.SphereInit( bNew.s.ptCenter, bNew.s.fRadius + 10 );
		bRecalcAllDepth = true;
	}

	// recalc directional depths
	if ( bRecalcAllDepth )
	{
		for ( int k = 0; k < GetSkyTexturesNum(); ++k )
			RecalcDepthChannel( k, N_ALL_DEPTH_CHANNELS, true );
	}

	// Retail v1.2 0x52fad3: the first catch builds every coarse cube face, even
	// while the camera moves. Later stationary frames refine these cached maps.
	if ( bFirstCatch )
	{
		SLightmapTargetGeom target( pScene, &rc, pTS, N_CL_TEMP_REGISTER );
		for ( int i = 0; i < lightState.points.size(); ++i )
		{
			const CLightState::SPointLight &p = lightState.points[i];
			if ( !p.bCastShadow )
				continue;
			CCubeTextureChannel *pChannel = GetPointDepth( p.vCenter, p.fRadius );
			for ( int nFace = 0; nFace < 6; ++nFace )
				RenderCubeMapDepth( &target, p.vCenter, p.fRadius, nFace, pChannel );
		}
	}

	// calc lightmap for new stuff
	//CalcCorrectZBuffer( &rc, _pRender, pTS, pScene );
	if ( bHasNewLightmaps )
	{
		SLightmapTargetGeom lmTarget( pScene, &rc, pTS, N_CL_TEMP_REGISTER );
		if ( bKeepPrevious )
		{
			// Retail v1.2 CatchUp @0x52fb90: reproject last light onto the current
			// geometry; only matching previous surface depths set stencil bit 0x40.
			PrepareCLHistory( &rc, nScratchRegister );
			rc.SetColorWrite( NGfx::COLORWRITE_NONE );
			RenderLight( &lmTarget, SLightInfo(), RO_CL_TEST_PREV_FRAME, &vDepth, &mPrevView.x,
				DPM_EQUAL | STM_MARK_2, float(nScratchRegister) );
			rc.SetColorWrite( NGfx::COLORWRITE_ALL );
			RenderLight( &lmTarget, SLightInfo(), RO_CL_COPY_LAST, &mPrevView.x,
				float(N_CL_TARGET_REGISTER), DPM_EQUAL );
			CTRect<float> size;
			NGfx::GetRegisterSize( &size );
			rc.SetRegister( N_CL_TARGET_REGISTER );
			NGfx::CopyTexture( rc, CVec2(size.x2, size.y2), size,
				NGfx::GetRegisterTexture(N_CL_TEMP_REGISTER), size );
		}
		// clear
		lmTarget.pRC->SetRegister( N_CL_TEMP_REGISTER );
		if ( CanDrawSky() )
			lmTarget.pRC->ClearTarget( N_DARKEST_AREA << 24 );
		else
			lmTarget.pRC->ClearTarget( 0xff000000 );
		// sky map
		if ( CanDrawSky() )
		{
			for ( int nBuf = 0; nBuf < GetSkyTexturesNum(); ++nBuf )
				RenderSkyCheck( &lmTarget, F_SKY_SINGLE_STRENGTH_MUL / N_DEPTH_CHANNELS_PER_TEX / GetSkyTexturesNum(), nBuf, true );
		}
		// color map
		// Keep the reprojected sky alpha; point-light color is updated independently.
		lmTarget.nTargetRegister = N_CL_TARGET_REGISTER;
		lmTarget.pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
		lmTarget.pRC->SetStencil( NGfx::STENCIL_NONE );
		NGfx::ModulateRegister( lmTarget.pRC, N_CL_TARGET_REGISTER, CVec4(0,0,0,0) );
		RenderCachedPoints(&lmTarget);
		// copy to origin
		lmTarget.pRC->SetColorWrite( NGfx::COLORWRITE_ALPHA );
		lmTarget.pRC->SetAlphaCombine( NGfx::COMBINE_NONE );
		lmTarget.pRC->SetStencil( bKeepPrevious ? NGfx::STENCIL_TEST_CLEAR : NGfx::STENCIL_NONE, 0, 0x40 );
		lmTarget.pRC->SetDepth( NGfx::DEPTH_NONE );
		NGfx::AlphaSqrtModulateRegister( lmTarget.pRC, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, 1 );
		// initiate recalc
		nPreparedSkySteps = 0;
		rs.nState = RC_START;
		rs.bCalcSky = true;
		rs.bCalcColor = true;
		rs.bColorReady = false;
		nLights = GetSkyTexturesNum();
		nPassesPerCalc = Max( 4, GetSkyTexturesNum() * 2 );
	}
	if ( pScene->HasSelectedFragments() && !bHasNewLightmaps && 1 ) // if not stress mode
		RecalcStep( &rc, pScene, pTS, bReuseLight, nScratchRegister );
	if ( bReuseLight )
	{
		// The existing sun pass emits this depth together with shadow RGB.
		// RenderPPShadowOps preserves the old alpha until reuse has finished.
		pHistoryDepth = NGfx::GetRegisterTexture( N_CL_DEPTH_REGISTER );
		pHistoryLight = NGfx::GetRegisterTexture( N_CL_TARGET_REGISTER );
	}
	else
	{
		pHistoryDepth = 0;
		pHistoryLight = 0;
	}
	pPreviousCLTracker = this;
	mPrevView = pTS->Get().forward;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::SetNewIllumination( const SGlobalIlluminationInfo &gl )
{
	rs.bColorReady = false;
	nPreparedSkySteps = 0;
	bLightStateUpdated = true;
	globalIllumination = gl;
	rs.nState = RC_START;
	rs.bCalcColor = true;
	rs.bCalcSky = false;
	lightState.Clear();
	SLightStateCalcSeed seed( ambientLightSeed );
	lightState.CreateSimple( &seed, globalIllumination );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GLightmapCalcInit: two plain value vars (no handler), both default 1.0, saved
START_REGISTER(GLightmapCalc)
	REGISTER_VAR( "gfx_cl_use_bump_always", 0, 1, true )
	REGISTER_VAR( "gfx_cl_use_bump", 0, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02592130, CLightmapTracker )
