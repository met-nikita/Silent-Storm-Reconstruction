#include "StdAfx.h"
#include "GfxBuffers.h"
#include "GLightmapCalc.h"
#include "GGeometry.h"
#include "..\Misc\RandomGen.h"
#include "GRenderExecute.h"
#include "GShadowMap.h"
#include "GfxUtils.h"
#include "Transform.h"
#include "GfxEffects.h"
#include "GMaterial.h"
#include "GScene.h"
#include "Gfx.h"

// number of sky directions used for dynamic lightmaps
const int N_SKY_DIRECTIONS = 12;
const float F_SKY_SINGLE_STRENGTH_MUL = 2.0f;// / N_SKY_DIRECTIONS;
const int N_DEPTH_CHANNELS_PER_TEX = 3;
const int N_ALL_DEPTH_CHANNELS = 0xffffffff;
const float F_MAX_SCENE_HEIGHT = 20; // CRAP need to store max height in single place
const int N_POINT_LIGHT_RECALC_STEPS = 4;
//! brightness in the darkest area, max is 255
const int N_DARKEST_AREA = 16;
//!!! максимальная протяженность треугольника, нужна чтобы избегать проблем 
//!!! с насыщением D3DColor на 0 или 1 при использовании последнего как глубины
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
void CLightState::AddPoint( bool bDoRender, const CVec3 &vCenter, float fRadius, const CVec3 &_vColor )
{
	if ( bDoRender )
		points.push_back( SPointLight( _vColor, vCenter, fRadius ) );
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
	int nDirBeta = Min( 6, nTexs * N_DEPTH_CHANNELS_PER_TEX );
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
		AddPoint( !p.bIsRendered, p.vCenter, p.fRadius, p.vColor );
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
	if ( fDist > fTargetR )
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
	if ( !skyDirections.empty() )
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
		TraceDynamicLMPointLight( pRes, bv.ptCenter, bv.fRadius, p.vCenter, p.fRadius, p.vColor, CVec3(0,0,0), pVis );
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
CLightmapTracker::CLightmapTracker() : nLights(1), nPassesPerCalc(2), groupSelect(0,0)
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
	int nStencilOp )
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
		SOpGenContext fi( &res.ops, &frag );
		fi.AddOperation( op, 100, nStencilOp, pTarget->nTargetRegister, param1, param2 );
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
	NGfx::InitRadius( &lightInfo.vRadius, fRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::RenderCubeMapDepth(
	SLightmapTargetGeom *pTarget, 
	const CVec3 &_vCenter, float fRadius, int nDir )
{
	SLightInfo lightInfo;
	InitLightInfo( &lightInfo, _vCenter, fRadius, CVec3(0,0,0) );
	// render occluders
	NGfx::CCubeTexture *pDepth = shadowMapsShare.GetCubeDepth();
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
		pRender->FormDirOccludersList( &tsFrustrum, vDir, &geom, MakeSelectAll(), false );
		
		// form transform stack for render
		camera.y = -camera.y;
		camera.w = CVec4(0,0,0,1);
		ts.Push43( camera );
		camera = ts.Get().forward;
		camera.x = camera.x - (1.0f / shadowMapsShare.GetCubeDepthResolution() ) * camera.w;
		camera.y = camera.y + (1.0f / shadowMapsShare.GetCubeDepthResolution() ) * camera.w;
		ts.Init( camera );
		rc.SetCubeTextureRT( pDepth, face, 0 );
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
	CObj<NGfx::CCubeTexture> &pDepth = pointDepths[ SPointLightPos( _vCenter, _fRadius ) ];
	if ( !IsValid( pDepth ) )
		pDepth = NGfx::MakeCubeTexture( GetCLCubeResolution(), 1, NGfx::SPixel8888::ID, NGfx::TARGET );
	NGfx::CCubeTexture *pSrc = shadowMapsShare.GetCubeDepth();
	NGfx::CRenderContext rc;
	SFBTransform id;
	Identity( &id.forward );
	Identity( &id.backward );
	rc.SetTransform( id );
	NGfx::STriangleList quad;
	NGfx::MakeQuadTriList( 2, &quad );
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
		rc.ClearBuffers(0);//ZBuffer();
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
	NGfx::CCubeTexture *pDepth, int nDepthBias, bool bFast )
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
	CSelectGeometries selector( pTarget->pGeom, SBoundIntersectFilter( bTarget ) );
	if ( !pTarget->pGeom->HasSelectedFragments() )
		return;
	SLightInfo lightInfo;
	InitLightInfo( &lightInfo, _vCenter, fRadius, _vColor );

	if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
	{
		if ( bFast )
		{
			pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
			RenderLight( pTarget, lightInfo, RO_CL_PNT_LIGHT_SHADOWED, pDepth, (float)nDepthBias, DPM_EQUAL|ABM_ADD );
		}
		else
		{
			pTarget->pRC->SetColorWrite( NGfx::COLORWRITE_NONE );
			RenderLight( pTarget, lightInfo, RO_CL_PNT_DEPTH_CHECK, pDepth, (float)nDepthBias, DPM_EQUAL|STM_LIGHT );
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
void CLightmapTracker::RecalcStep( NGfx::CRenderContext *pRC, CSceneFragments *pScene, CTransformStack *pTS )
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
					rs.nState = RC_COLOR_POINT;
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
					rs.nState = RC_COLOR_POINT;
				else
				{
					int nChannel = rs.nStep % ( N_DEPTH_CHANNELS_PER_TEX + 1 );
					int nBuf = ( rs.nStep / ( N_DEPTH_CHANNELS_PER_TEX + 1 ) ) % GetSkyTexturesNum();
					if ( nChannel < N_DEPTH_CHANNELS_PER_TEX )
					{
						// pick new channel & new dir
						ChooseNewSkyDirection( nBuf, nChannel );
						RecalcDepthChannel( nBuf, 1 << nChannel, false );
					}
					else
						RenderSkyCheck( &lmTarget, F_SKY_SINGLE_STRENGTH_MUL / nPassesPerCalc / N_DEPTH_CHANNELS_PER_TEX, nBuf, false );
					++rs.nStep;
					if ( rs.nStep == (N_DEPTH_CHANNELS_PER_TEX+1) * nPassesPerCalc )
					{
						if ( rs.bCalcColor )
						{
							rs.nStep = 0;
							rs.nState = RC_COLOR_POINT;
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
				int nLight = rs.nStep / N_POINT_LIGHT_RECALC_STEPS;
				if ( nLight < lightState.points.size() )
				{
					const CLightState::SPointLight &p = lightState.points[ nLight ];
					if ( nStep < 3 )
					{
						RenderCubeMapDepth( &lmTarget, p.vCenter, p.fRadius, nStep * 2 );
						RenderCubeMapDepth( &lmTarget, p.vCenter, p.fRadius, nStep * 2 + 1 );
					}
					else
					{
						DownsampleCubeMapDepth( p.vCenter, p.fRadius );
						RenderPointLightShadowed( &lmTarget, p.vCenter, p.fRadius, p.vColor,
							shadowMapsShare.GetCubeDepth(), 2, false );
					}
					++rs.nStep;
				}
				else
				{
					rs.nStep = 0;
					rs.nState = RC_APPLY;//RC_COLOR_SEMI;
				}
			}
			break;
		case RC_APPLY:
			// store to lightmap
			{
				NGfx::CRenderContext rc( *lmTarget.pRC );
				if ( !rs.bCalcSky )
				{
					rc.SetColorWrite( NGfx::COLORWRITE_COLOR );
					NGfx::AlphaSqrtModulateRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, 1 );
				}
				else
				{
					if ( nLights > 0 )
					{
						// blend with previous result
						nLights = Min( nLights, 64 );
						float fNewBlend = ((float)nPassesPerCalc ) / ( nPassesPerCalc + nLights );
						if ( rs.bCalcColor )
						{
							CVec4 vOldBlend( 0, 0, 0, 1 - fNewBlend );
							NGfx::ModulateRegister( &rc, N_CL_TARGET_REGISTER, vOldBlend );
							rc.SetAlphaCombine( NGfx::COMBINE_ADD );
							NGfx::AlphaSqrtModulateRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, fNewBlend );
						}
						else
						{
							CVec4 vOldBlend( 1, 1, 1, 1 - fNewBlend );
							NGfx::ModulateRegister( &rc, N_CL_TARGET_REGISTER, vOldBlend );
							rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
							rc.SetAlphaCombine( NGfx::COMBINE_ADD );
							NGfx::AlphaSqrtModulateRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, fNewBlend );
						}
					}
					else
					{
						// simple store
						if ( !rs.bCalcColor )
							rc.SetColorWrite( NGfx::COLORWRITE_ALPHA );
						NGfx::AlphaSqrtModulateRegister( &rc, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, 1 );
					}
					nLights += nPassesPerCalc;
					nPassesPerCalc *= 2;
					if ( nPassesPerCalc > 32 )
						nPassesPerCalc = 32;
				}
				rs.nState = RC_START; // circle on the sand round and round ( (C)Belinda Carl..)
				rs.bCalcSky = true;
				rs.bCalcColor = !rs.bCalcColor; // calc color every second attempt
			}
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::CatchUp( NGfx::CRenderContext *_pRC, IRender *_pRender, CTransformStack *pTS, CSceneFragments *pScene, bool bHasNewLightmaps, const SGroupSelect &_gs )
{
	NGfx::CRenderContext rc( *_pRC );
	pRender = _pRender;
	bool bRecalcAllDepth = false;
	if ( _gs != groupSelect )
	{
		// этаж сменили - надо все пересчитать
		bRecalcAllDepth = true;
		groupSelect = _gs;
		bHasNewLightmaps = true;
	}
	if ( memcmp( &pTS->Get().forward, &mPrevView, sizeof(mPrevView) ) != 0 )
	{
		mPrevView = pTS->Get().forward;
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

	// calc lightmap for new stuff
	//CalcCorrectZBuffer( &rc, _pRender, pTS, pScene );
	if ( bHasNewLightmaps )
	{
		SLightmapTargetGeom lmTarget( pScene, &rc, pTS, N_CL_TEMP_REGISTER );
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
		lmTarget.pRC->SetColorWrite( NGfx::COLORWRITE_COLOR );
		for ( int k = 0; k < lightState.points.size(); ++k )
		{
			const CLightState::SPointLight &p = lightState.points[ k ];
			RenderPointLightShadowed( &lmTarget, p.vCenter, p.fRadius, p.vColor,
				pointDepths[ SPointLightPos( p.vCenter, p.fRadius ) ], 3, false );//true );
		}
		// copy to origin
		lmTarget.pRC->SetColorWrite( NGfx::COLORWRITE_ALL );
		lmTarget.pRC->SetAlphaCombine( NGfx::COMBINE_NONE );
		lmTarget.pRC->SetStencil( NGfx::STENCIL_NONE );
		lmTarget.pRC->SetDepth( NGfx::DEPTH_NONE );
		NGfx::AlphaSqrtModulateRegister( lmTarget.pRC, N_CL_TARGET_REGISTER, N_CL_TEMP_REGISTER, 1 );
		// initiate recalc
		rs.nState = RC_START;
		rs.bCalcSky = true;
		rs.bCalcColor = true;
		nLights = GetSkyTexturesNum();
		nPassesPerCalc = Max( 4, GetSkyTexturesNum() * 2 );
	}
	if ( pScene->HasSelectedFragments() && !bHasNewLightmaps && 1 ) // if not stress mode
		RecalcStep( &rc, pScene, pTS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightmapTracker::SetNewIllumination( const SGlobalIlluminationInfo &gl )
{
	globalIllumination = gl;
	rs.nState = RC_START;
	rs.bCalcColor = true;
	rs.bCalcSky = false;
	lightState.Clear();
	SLightStateCalcSeed seed( ambientLightSeed );
	lightState.CreateSimple( &seed, globalIllumination );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02592130, CLightmapTracker )

