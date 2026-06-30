#include "StdAfx.h"
#include "GRenderFactor.h"
#include "GRenderExecute.h"
#include "GfxRender.h"
#include "GfxEffects.h"
#include "GfxBuffers.h"
#include "Transform.h"
#include "GfxShaders.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCompareOps
{
	bool operator()( const CRenderCmdList::SOperation *pA, const CRenderCmdList::SOperation *pB )
	{
		if ( pA->nPass != pB->nPass )
			return pA->nPass < pB->nPass;
		if ( pA->nDestRegister != pB->nDestRegister )
			return pA->nDestRegister < pB->nDestRegister;
		if ( pA->op != pB->op )
			return pA->op < pB->op;
		if ( pA->nStencilBlendMode != pB->nStencilBlendMode )
			return pA->nStencilBlendMode < pB->nStencilBlendMode;
		if ( pA->p1.f != pB->p1.f )
			return pA->p1.f < pB->p1.f;
		if ( pA->p2.f != pB->p2.f )
			return pA->p2.f < pB->p2.f;
		if ( pA->p3.f != pB->p3.f )
			return pA->p3.f < pB->p3.f;
		if( pA->pFrag != pB->pFrag )
			return pA->pFrag < pB->pFrag;
		//ASSERT(0);
		return false;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec4 GetNormalized( const CVec4 &v, float _f )
{
	float f = _f / sqrt( sqr(v.x) + sqr(v.y) + sqr(v.z) );
	return v * f;
}
static CVec3 GetVec3( const CVec4 v ) { return CVec3( v.x, v.y, v.z ); }
static void SetPSConstBx2( NGfx::CRenderContext *pRC, int nTarget, const CVec4 &v )
{
	pRC->SetPSConst( nTarget, v * 2 - CVec4(1,1,1,1) );
}
static void SetDepthCmpVal( NGfx::CRenderContext *pRC, const CVec4 &vChannelSelect, int nTarget )
{
	if ( NGfx::GetHardwareLevel() > NGfx::HL_GFORCE3 )
		pRC->SetPSConst( nTarget, vChannelSelect );
	else
	{
		if ( fabs2( vChannelSelect ) > 1 )
			SetPSConstBx2( pRC, nTarget, vChannelSelect * 0.75f ); // for 2 channel simultaneous check
		else
			SetPSConstBx2( pRC, nTarget, CVec4(0.25f, 0.25f, 0.25f,0) + vChannelSelect * 0.75f );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddTriangles( NGfx::CRenderContext *pRC, const SRenderFragmentInfo &fragment, 
	const CSceneFragments &scene, ETrilistType triListType )
{
	for ( int k = 0; k < fragment.elements.size(); ++k )
	{
		const SRenderFragmentInfo::SElement &element = fragment.elements[k];
		EFragmentsSplit filter = scene.GetGeometryFlags( element.nGeometry );
		if ( filter == FST_REJECT )
			continue;
		SRenderGeometryInfo *pGeometryInfo = scene.GetGeometryInfo( element.nGeometry );
		pGeometryInfo->pTriLists[triListType].Refresh();
		pGeometryInfo->pVertices.Refresh();
		const vector<NGfx::STriangleList> &tris = pGeometryInfo->pTriLists[triListType]->GetValue();
		ASSERT( element.nBlock * 32 < tris.size() );
		int nBase = element.nBlock * 32;
		int nFlags = element.nFlags;
		if ( filter == FST_SPLIT )
			nFlags &= scene.GetGeometryParts( element.nGeometry ).GetBlock( element.nBlock );
		pRC->AddPrimitive( pGeometryInfo->pVertices->GetValue(), &tris[0] + nBase, Min( 32, int(tris.size() - nBase) ), nFlags );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ExecOps( NGfx::CRenderContext *pRC, const vector<CRenderCmdList::SOperation> &ops,
	const CSceneFragments &scene, const SLightInfo &lightInfo )
{
	if ( ops.empty() )
		return;
	vector<const CRenderCmdList::SOperation*> renderThem;
	for ( vector<CRenderCmdList::SOperation>::const_iterator i = ops.begin(); i != ops.end(); ++i )
		renderThem.push_back( &(*i) );

	sort( renderThem.begin(), renderThem.end(), SCompareOps() );
	//renderThem.sort( SCompareOps() );

	const CRenderCmdList::SOperation *pPrevOp = 0;
	bool bSetLightParams = lightInfo.bNeedSet;
	ETrilistType triListType = TLT_GEOM;
	for ( vector<const CRenderCmdList::SOperation*>::iterator i = renderThem.begin(); i != renderThem.end(); ++i )
	{
		const CRenderCmdList::SOperation &op = *(*i);
		if ( op.op == RO_NOP )
			continue;
		if ( pPrevOp )
		{
			if ( op.IsSame( *pPrevOp ) )
			{
				AddTriangles( pRC, *op.pFrag, scene, triListType );
				continue;
			}
			else
				pRC->Flush();
		}
		pPrevOp = &op;
		if ( pRC->HasRegisters() )
			pRC->SetRegister( op.nDestRegister );
		else
			ASSERT( op.nDestRegister == 0 );
		// set blending
		switch ( op.nStencilBlendMode & ABM_MASK )
		{
			case ABM_NONE: pRC->SetAlphaCombine( NGfx::COMBINE_NONE ); break;
			case ABM_ZERO: pRC->SetAlphaCombine( NGfx::COMBINE_ZERO_ONE ); break;
			case ABM_ADD:  pRC->SetAlphaCombine( NGfx::COMBINE_ADD ); break;
			case ABM_MUL:  pRC->SetAlphaCombine( NGfx::COMBINE_MUL ); break;
			case ABM_SRC_AMUL: pRC->SetAlphaCombine( NGfx::COMBINE_SRC_ALPHA_MUL ); break;
			case ABM_MUL2: pRC->SetAlphaCombine( NGfx::COMBINE_MUL2 ); break;
			case ABM_ALPHA_BLEND: pRC->SetAlphaCombine( NGfx::COMBINE_ALPHA ); break;
			case ABM_SMART: pRC->SetAlphaCombine( NGfx::COMBINE_SMART_ALPHA ); break;
			default: ASSERT( 0 ); break;
		}
		// set stencil op
		switch ( op.nStencilBlendMode & STM_MASK )
		{
			case STM_NONE: pRC->SetStencil( NGfx::STENCIL_NONE ); break;
			case STM_LIGHT: pRC->SetStencil( NGfx::STENCIL_TESTNE_WRITE, 0x80, 0x80 ); break;
			case STM_STENCIL_LIGHT: pRC->SetStencil( NGfx::STENCIL_TEST_REPLACE, 0x80, 0x7f ); break;
			case STM_TEST_STENCIL_LIGHT: pRC->SetStencil( NGfx::STENCIL_TEST, 0x80, 0x80 ); break;
			case STM_MARK: pRC->SetStencil( NGfx::STENCIL_WRITE, 0x80 ); break;
			case STM_TEST_CLEAR_MARK: pRC->SetStencil( NGfx::STENCIL_TESTNE_WRITE, 0, 0x80 ); break;
			case STM_MARK_2: pRC->SetStencil( NGfx::STENCIL_WRITE, 0x40, 0x40 ); break;
			//case STM_SHADOW: pRC->SetStencil( NGfx::STENCIL_TESTNE_WRITE, 0, 0x80 ); break;
			//case STM_MARK: pRC->SetStencil( NGfx::STENCIL_TESTNE_WRITE, 0x80, 0x80 ); break;
			//case STM_TEST_SHADOW: pRC->SetStencil( NGfx::STENCIL_TESTNE_WRITE, 0, 0x80 ); break;
			//case STM_TESTONLY_SHADOW: pRC->SetStencil( NGfx::STENCIL_NOTEQUAL, 0, 0x80 ); break;
			//case STM_MARK_STENCIL_SHADOW: pRC->SetStencil( NGfx::STENCIL_TEST_INVERT_LAST, 0 ); break;
			//case STM_MARK_LIMITED: pRC->SetStencil( NGfx::STENCIL_TEST_INVERT_LAST, 1 ); break;
			//case STM_CLEAR: pRC->SetStencil( NGfx::STENCIL_WRITE, 0 ); break;
			default: ASSERT( 0 ); break;
		}

		switch ( op.nStencilBlendMode & DPM_MASK )
		{
			case 0: pRC->SetDepth( NGfx::DEPTH_NORMAL ); break;
			case DPM_EQUAL:	pRC->SetDepth( NGfx::DEPTH_EQUAL ); break;
			case DPM_TESTONLY: pRC->SetDepth( NGfx::DEPTH_TESTONLY ); break;
			case DPM_NONE: pRC->SetDepth( NGfx::DEPTH_NONE ); break;
			default: ASSERT(0); break;
		}

		pRC->Use();

		if ( bSetLightParams )
		{
			bSetLightParams = false;
			if ( NGfx::IsTnLDevice() )
			{
				pRC->SetLightParams( lightInfo.vAmbientColor, 
					CVec3( lightInfo.vLightColor.x, lightInfo.vLightColor.y, lightInfo.vLightColor.z ),
					CVec3( lightInfo.vLightPos.x, lightInfo.vLightPos.y, lightInfo.vLightPos.z ) );
			}
			else
			{
				pRC->SetVSConst( 14, lightInfo.vLightColor );
				pRC->SetVSConst( 15, lightInfo.vLightPos );
				//pRC->SetVSConst( 20, lightInfo.vBackColor );
				pRC->SetVSConst( 21, lightInfo.vRadius );
			}
		}

		switch ( op.op )
		{
			// TnL path
		case RO_TNL_DIR_AMB_LIT_DIFFUSE_SOLID:
			pRC->SetPixelShader( psDiffuseTFactor4 );
			pRC->SetPSConst( 0, *op.p1.pVec4 );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE:
			pRC->SetPixelShader( psDiffuseTexture4 );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestDiffuseTex4 );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE_DECAL:
			pRC->SetPixelShader( psDiffuseTexture4CopyAlpha );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_TEXTURE:
			pRC->SetPixelShader( psTextureColor );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TNL_SOLID:
			pRC->SetPixelShader( psTFactor );
			pRC->SetPSConst( 0, *op.p1.pVec4 );
			triListType = TLT_POSITION;
			break;
			// general ops
		/*case RO_SKYMAP:
			pRC->SetPixelShader( psTextureAlpha );
			pRC->SetVertexShader( vsTextureLM );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_LM;
			break;*/
		case RO_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestTexture );
			pRC->SetVertexShader( vsTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TEXTURE_AT_CONSERVATIVE:
			pRC->SetPixelShader( psAlphaTestTextureCons );
			pRC->SetVertexShader( vsTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TEXTURE:
			pRC->SetPixelShader( psTextureColor );
			pRC->SetVertexShader( vsTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_TEXTURE_DECAL:
			pRC->SetPixelShader( psTextureCopyAlpha );
			pRC->SetVertexShader( vsTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_SOLID_COLOR:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsConstLight );
			pRC->SetVSConst( 16, *op.p1.pVec4 );
			triListType = TLT_POSITION;
			break;

			// lightmapped stuff
		case RO_LMPD_SOLID:
			pRC->SetPixelShader( psLMPDSolid );//DiffuseTexture4 );
			pRC->SetVertexShader( vsLMPDSolid );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			pRC->SetPSConst( 0, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;
		case RO_LMPD_TEXTURE:
			pRC->SetPixelShader( psLMPDTexture );//TexTexture4 );
			pRC->SetVertexShader( vsLMPDTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetTexture( 1, op.p2.pTex );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			triListType = TLT_GEOM;
			break;
		case RO_LMPD_TEXTURE_AT:
			pRC->SetPixelShader( psLMPDTextureAT );//AlphaTestTexTexture4 );
			pRC->SetVertexShader( vsLMPDTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetTexture( 1, op.p2.pTex );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			triListType = TLT_GEOM;
			break;
		case RO_LMPD_TEXTURE_DECAL:
			pRC->SetPixelShader( psLMPDTextureCopyAlpha );//TexTexture4CopyAlpha );
			pRC->SetVertexShader( vsLMPDTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetTexture( 1, op.p2.pTex );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			triListType = TLT_GEOM;
			break;
		case RO_DYN_LMPD_SOLID:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsDynLMPDSolid );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			pRC->SetVSConst( 16, (*op.p2.pVec3) * 4 );
			triListType = TLT_GEOM;
			break;
		case RO_DYN_LMPD_TEXTURE:
			pRC->SetPixelShader( psDiffuseTexture4 );
			pRC->SetVertexShader( vsDynLMPDTexture );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			triListType = TLT_GEOM;
			break;
		case RO_DYN_LMPD_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestDiffuseTex4 );
			pRC->SetVertexShader( vsDynLMPDTexture );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			triListType = TLT_GEOM;
			break;
		case RO_DYN_LMPD_TEXTURE_DECAL:
			pRC->SetPixelShader( psDiffuseTexture4CopyAlpha );
			pRC->SetVertexShader( vsDynLMPDTexture );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			triListType = TLT_GEOM;
			break;
		case RO_DYNAMIC_LIGHTMAP:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsDynamicAmbient );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			triListType = TLT_GEOM;
			break;

			// ambient
		case RO_AMB_LIT_SOLID:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsConstLight );
			pRC->SetVSConst( 16, MulPerComp4( *op.p2.pVec3, *op.p1.pVec3 ) );
			triListType = TLT_POSITION;
			break;
		case RO_AMB_LIT_TEXTURE:
			pRC->SetPixelShader( psDiffuseTexture4 );
			pRC->SetVertexShader( vsConstLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;
		case RO_AMB_LIT_TEXTURE_DECAL:
			pRC->SetPixelShader( psDiffuseTexture4CopyAlpha );
			pRC->SetVertexShader( vsConstLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;
		case RO_AMB_LIT_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestDiffuseTex4 );
			pRC->SetVertexShader( vsConstLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;

			// directional light
		case RO_DIFFUSE_DIR_LIT_TEXTURE:
			pRC->SetPixelShader( psDiffuseTexture4CopyAlpha );
			pRC->SetVertexShader( vsDirectionalLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_DIR_LIT_TEXTURE_PP:
			pRC->SetPixelShader( psPerPixelDiffuseTex4 );
			pRC->SetVertexShader( vsDirectionalLightTexturePP );
			SetPSConstBx2( pRC, 0, lightInfo.vLightPos * 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_LIT_SOLID:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsLight );
			pRC->SetVSConst( 16, MulPerComp4( GetVec3( lightInfo.vLightColor ), *op.p1.pVec3 ) );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_LIT_SOLID_PP:
			pRC->SetPixelShader( psPerPixelDiffuseTex4 );
			pRC->SetVertexShader( vsPerPixelDiffuse );
			pRC->SetVSConst( 16, MulPerComp( GetVec3( lightInfo.vLightColor ), *op.p1.pVec3 ) );
			SetPSConstBx2( pRC, 0, lightInfo.vLightPos * 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_BUMP_DIR_LIT_TEXTURE:
			pRC->SetPixelShader( psFastBump4 );
			pRC->SetVertexShader( vsFastDirBumpLight );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetTexture( 1, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_BUMP_DIR_LIT_TEXTURE_PP:
			pRC->SetPixelShader( psPreciseBump4 );
			pRC->SetVertexShader( vsPreciseDirBumpLightTex );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, op.p2.pTex );
			pRC->SetTexture( 2, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_DEPTH:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsDepth );
			pRC->SetVSConst( 16, op.p1.pDirDepth->vDepth );
			triListType = TLT_POSITION;
			break;
		case RO_DIR_DEPTH_MAX:
			pRC->SetPixelShader( psDiffuseTexMax );
			pRC->SetVertexShader( vsDepthTextureMax );
			pRC->SetVSConst( 16, op.p1.pDirDepth->vDepth );
			pRC->SetVSConst( 17, op.p1.pDirDepth->vVecU );
			pRC->SetVSConst( 18, op.p1.pDirDepth->vVecV );
			pRC->SetTexture( 0, op.p2.pTex );
			triListType = TLT_POSITION;
			break;
		case RO_DIR_SHADOW_TEST:
			pRC->SetPixelShader( psShadowTestP );
			pRC->SetVertexShader( vsDirectionalTestP );
			pRC->SetVSConst( 16, op.p1.pPDirDepth->vDepth );
			pRC->SetVSConst( 19, CVec4(1,1,1,1) - op.p1.pPDirDepth->vChannelSelect );
			pRC->SetVSConst( 25, &op.p1.pPDirDepth->m.x, 4 );
			SetDepthCmpVal( pRC, op.p1.pPDirDepth->vChannelSelect, 0 );
			pRC->SetTexture( 0, op.p2.pTex );
			triListType = TLT_POSITION;
			break;
		case RO_DIR_SHADOW_TEST_SMOOTHED:
			pRC->SetPixelShader( psShadowTestSmoothed );
			pRC->SetVertexShader( vsDirectionalTestSmoothed );
			pRC->SetVSConst( 16, op.p1.pPDirDepth->vDepth );
			pRC->SetVSConst( 19, CVec4(1,1,1,-op.p3.f) - op.p1.pPDirDepth->vChannelSelect );
			pRC->SetVSConst( 25, &op.p1.pPDirDepth->m.x, 4 );
			pRC->SetPSConst( 0, op.p1.pDirDepth->vChannelSelect );
			pRC->SetPSConst( 1, CVec4( ( 1 - op.p3.f ) * lightInfo.vShadowColor, 124/256.0f ) );
			pRC->SetTexture( 0, op.p2.pTex );
			triListType = TLT_POSITION;
			break;
		case RO_DIR_PARTICLE_LM_SHADOW_TEST:
			pRC->SetPixelShader( psShadowTestP );
			pRC->SetVertexShader( vsParticleLMDirectionalTest );
			pRC->SetVSConst( 16, op.p1.pPDirDepth->vDepth );
			pRC->SetVSConst( 19, CVec4(1,1,1,1) - op.p1.pPDirDepth->vChannelSelect );
			pRC->SetVSConst( 25, &op.p1.pPDirDepth->m.x, 4 );
			SetDepthCmpVal( pRC, op.p1.pPDirDepth->vChannelSelect, 0 );
			pRC->SetTexture( 0, op.p2.pTex );
			triListType = TLT_POSITION;
			break;
		case RO_DIR_PARTICLE_LM_SOFT_SHADOW_TEST:
			pRC->SetPixelShader( psSoftShadowTest );
			pRC->SetVertexShader( vsParticleLMDirectionalTest );
			pRC->SetVSConst( 16, op.p1.pPDirDepth->vDepth );
			pRC->SetVSConst( 19, CVec4(1,1,1,1) - op.p1.pDirDepth->vChannelSelect );
			pRC->SetVSConst( 25, &op.p1.pPDirDepth->m.x, 4 );
			SetDepthCmpVal( pRC, op.p1.pPDirDepth->vChannelSelect, 0 );
			pRC->SetPSConst( 1, CVec4( *op.p3.pVec3, 0 ) );
			pRC->SetTexture( 0, op.p2.pTex );
			triListType = TLT_POSITION;
			break;

			// directional + ambient
		case RO_DIR_AMB_LIT_SOLID:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsLightPlusAmbient );
			pRC->SetVSConst( 16, MulPerComp4( GetVec3( lightInfo.vLightColor ), *op.p1.pVec3 ) );
			pRC->SetVSConst( 17, MulPerComp4( *op.p2.pVec3, *op.p1.pVec3 ) );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_AMB_LIT_DIFFUSE_TEXTURE:
			pRC->SetPixelShader( psDiffuseTexture4 );
			pRC->SetVertexShader( vsDirectionalAmbientLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_AMB_LIT_DIFFUSE_TEXTURE_AT:
			pRC->SetPixelShader( psAlphaTestDiffuseTex4 );
			pRC->SetVertexShader( vsDirectionalAmbientLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;
		case RO_DIR_AMB_LIT_DIFFUSE_TEXTURE_DECAL:
			pRC->SetPixelShader( psDiffuseTexture4CopyAlpha );
			pRC->SetVertexShader( vsDirectionalAmbientLightTexture );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetVSConst( 16, *op.p2.pVec3 );
			triListType = TLT_GEOM;
			break;

			// full fast render
		case RO_DIFFUSE_FULL_LIT_TEXTURE_PP:
			pRC->SetPixelShader( psPerPixelFullTex4 );
			pRC->SetVertexShader( vsPerPixelFullTex );//DirectionalLightTexturePP );
			pRC->SetPSConst( 0, lightInfo.vLightPos );// * 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetPSConst( 1, lightInfo.vLightColor );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			pRC->SetTexture( 0, op.p1.pTex );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, NGfx::GetRegisterTexture(3), true );//op.p2.pTex );
			pRC->SetTexture( 3, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_FULL_LIT_BUMP_TEXTURE_PP:
			pRC->SetPixelShader( psFastFullBump4 );
			pRC->SetVertexShader( vsFastFullBumpLight );
			pRC->SetPSConst( 1, lightInfo.vAmbientColor );
			pRC->SetPSConst( 2, lightInfo.vUpDifColor );
			pRC->SetPSConst( 3, lightInfo.vLightColor );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetTexture( 1, op.p1.pTex );
			pRC->SetTexture( 2, NGfx::GetRegisterTexture(3), true );//op.p3.pTex );
			pRC->SetTexture( 3, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_PP_SPECULAR_FULL_TEXTURE_DIR:
			pRC->SetPixelShader( psFullPerPixelTexSpecular );
			pRC->SetVertexShader( vsFullPPTexSpecular );
			pRC->SetVSConst( 17, lightInfo.vGlossColor );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			pRC->SetTexture( 2, op.p2.pTex );
			pRC->SetTexture( 3, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_PP_SPECULAR_FULL_COLOR_DIR:
			pRC->SetPixelShader( psFullPerPixelSpecular );
			pRC->SetVertexShader( vsFullPPSpecular );
			pRC->SetVSConst( 17, MulPerComp( lightInfo.vGlossColor, *op.p2.pVec3 ) );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			pRC->SetTexture( 2, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_FULL_LIT_SOLID_PP:
			pRC->SetPixelShader( psFullPerPixel4 );
			pRC->SetVertexShader( vsFullPerPixel );
			pRC->SetPSConst( 0, lightInfo.vLightPos );//* 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetPSConst( 1, lightInfo.vLightColor );
			pRC->SetPSConst( 2, *op.p1.pVec3 );
			pRC->SetVSConst( 16, lightInfo.vAmbientColor - lightInfo.vUpDifColor );
			pRC->SetVSConst( 17, lightInfo.vUpDifColor * 2 );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, NGfx::GetRegisterTexture(3), true );//op.p2.pTex );
			pRC->SetTexture( 2, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;

		case RO_DYNLM_LIT_SOLID_PP:
			pRC->SetPixelShader( psDynLMPerPixel4 );
			pRC->SetVertexShader( vsDynLMPerPixel );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			pRC->SetPSConst( 0, lightInfo.vLightPos );// * 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetPSConst( 1, lightInfo.vLightColor );
			pRC->SetPSConst( 2, *op.p2.pVec3 );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_DYNLM_LIT_TEXTURE_PP:
			pRC->SetPixelShader( psDynLMPerPixelTex4 );
			pRC->SetVertexShader( vsDynLMPerPixelTex );//DirectionalLightTexturePP );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			pRC->SetPSConst( 0, lightInfo.vLightPos );//* 0.5f + CVec4( 0.5f, 0.5f, 0.5f, 0.5f ) );
			pRC->SetPSConst( 1, lightInfo.vLightColor );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;
		case RO_DIFFUSE_DYNLM_LIT_BUMP_TEXTURE_PP:
			pRC->SetPixelShader( psDynLMPreciseBumpTex4 );
			pRC->SetVertexShader( vsDynLMPreciseBumpTex );
			pRC->SetVSConst( 25, op.p1.pDynamicAmbientInfo->GetVec4(), 6 );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, op.p3.pTex );
			pRC->SetTexture( 2, op.p2.pTex );
			pRC->SetTexture( 3, NGfx::GetRegisterTexture(1), true );
			triListType = TLT_GEOM;
			break;

			// point light
		case RO_PNT_CUBEMAP_DEPTH:
			pRC->SetPixelShader( psPointCubeMapDepth );//psDiffuse );//
			pRC->SetVertexShader( vsPointCubeMapDepth );//vsConstLight );//
			pRC->SetVSConst( 16, CVec4( 0.5f, 1 / ( lightInfo.vRadius.x + 4 ), 0, 0 ) );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			triListType = TLT_POSITION;
			break;
		case RO_PNT_LIT_SOLID:
			pRC->SetPixelShader( psDiffuseProjectedTexture4 );
			pRC->SetVertexShader( vsPointLight );
			pRC->SetVSConst( 16, MulPerComp( GetVec3( lightInfo.vLightColor ), *op.p1.pVec3 ) );
			pRC->SetTexture( 0, GetLightCircle() );
			triListType = TLT_GEOM;
			break;
		case RO_PNT_LIT_TEXTURE:
			pRC->SetPixelShader( psDiffuseProjTextureTex4 );
			pRC->SetVertexShader( vsPointLightTex );
			pRC->SetTexture( 0, GetLightCircle() );
			pRC->SetTexture( 1, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_PNT_LIT_TEXTURE_PBUMP:
			pRC->SetPixelShader( psPrecisePointBumpTex );
			pRC->SetVertexShader( vsPrecisePointBumpTex );
			pRC->SetTexture( 0, GetLightFalloff() );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, op.p1.pTex );
			pRC->SetTexture( 3, op.p2.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_PNT_LIT_SOLID16:
			pRC->SetPixelShader( psDiffuseProjectedTexture16 );
			pRC->SetVertexShader( vsPointLight );
			pRC->SetVSConst( 16, MulPerComp( GetVec3( lightInfo.vLightColor ), *op.p1.pVec3 ) );
			pRC->SetTexture( 0, GetLightCircle() );
			if ( NGfx::GetHardwareLevel() < NGfx::HL_GFORCE3 )
				pRC->SetPSConst( 0, CVec4(1,1,1,1) );
			triListType = TLT_GEOM;
			break;
		case RO_PNT_LIT_TEXTURE16:
			pRC->SetPixelShader( psDiffuseProjTextureTex16 );
			pRC->SetVertexShader( vsPointLightTex );
			pRC->SetTexture( 0, GetLightCircle() );
			pRC->SetTexture( 1, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_PNT_LIT_TEXTURE_PBUMP16:
			pRC->SetPixelShader( psPrecisePointBumpTex16 );
			pRC->SetVertexShader( vsPrecisePointBumpTex );
			pRC->SetTexture( 0, GetLightFalloff() );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, op.p1.pTex );
			pRC->SetTexture( 3, op.p2.pTex );
			triListType = TLT_GEOM;
			break;
	
			// fog
		case RO_FOG_STATIC:
			pRC->SetPixelShader( psTextureCopyAlpha );
			pRC->SetVertexShader( vsJustFog );
			pRC->SetVSConst( 14, CVec4( 1 / NGfx::F_FOG_HEIGHT, 1 / NGfx::F_FOG_DISTANCE, 0, 0 ) );
			pRC->SetTexture( 0, GetFogLookupTexture( *op.p1.pFog ) );
			triListType = TLT_POSITION;
			break;
		case RO_FOG_DYNAMIC:
			{
				const SFogParams &fog = *op.p1.pFog;
				float fShift = fog.fTime * fog.fVapourSpeed;
				float fScale = 30;
				CVec4 vMappingParams( 1 / fScale, 1 / fScale, fShift / fScale, fShift / fScale );
				float fTest = fog.fTime / fog.fVapourSwitchTime;
				int nTest = (int)fTest;
				float fRest = fTest - nTest;
				if ( nTest & 1 )
					fRest = 1 - fRest;
				CVec4 vDepthParams( 1 / NGfx::F_FOG_HEIGHT, 1 / NGfx::F_FOG_DISTANCE, 0.1f * fRest, 0.1f * (1 - fRest) );
				CVec4 vParams( 1 / NGfx::F_FOG_HEIGHT, 1 / NGfx::F_FOG_DISTANCE, 0, 0.01f );
				pRC->SetPixelShader( psTexturedFog );
				pRC->SetVertexShader( vsTexturedFog );
				pRC->SetVSConst( 14, vParams );
				pRC->SetVSConst( 15, vDepthParams );
				pRC->SetVSConst( 16, vMappingParams );
				pRC->SetTexture( 0, GenerateFogTexture( fog.fVapourNoiseParam ) );
				pRC->SetTexture( 2, GetFogLookupTexture( fog ) );
				triListType = TLT_POSITION;
			}
			break;
			
			// mirrors
		case RO_MIRROR:
			pRC->SetPixelShader( psMirror );
			pRC->SetVertexShader( vsMirror );
			pRC->SetVSConst( 14, *op.p2.pVec4 );
			pRC->SetTexture( 0, op.p1.pCubeTex );
			triListType = TLT_GEOM;
			break;
		case RO_GLOSSED_MIRROR:
			pRC->SetPixelShader( psGlossedMirror );
			pRC->SetVertexShader( vsGlossedMirror );
			pRC->SetVSConst( 14, *op.p3.pVec4 );
			pRC->SetTexture( 0, op.p1.pCubeTex );
			pRC->SetTexture( 1, op.p2.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_BUMPED_MIRROR:
			pRC->SetPixelShader( psBumpedMirror );
			pRC->SetVertexShader( vsBumpedMirror );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetTexture( 3, op.p1.pCubeTex );
			triListType = TLT_GEOM;
			break;
		case RO_BUMPED_FRESNEL:
			pRC->SetPixelShader( psBumpedFresnel );
			pRC->SetVertexShader( vsBumpedFresnel );
			pRC->SetPSConst( 0, *op.p2.pVec4 );
			pRC->SetTexture( 0, op.p1.pTex ); // bump
			pRC->SetTexture( 1, NGfx::GetRegisterTexture( 1 ), true );
			pRC->SetTexture( 2, GetNormalizeTexture() );
			triListType = TLT_GEOM;
			break;
		case RO_REG_MUL_TEXTURE:
			pRC->SetPixelShader( psPTextureTexture );
			pRC->SetVertexShader( vsRegisterMapTex );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( 1 ), true );
			pRC->SetTexture( 1, op.p1.pTex );
			triListType = TLT_GEOM;
			break;

			// per pixel specular
		case RO_NHCALC:
			{
				NGfx::CCubeTexture *pNormalize = GetNormalizeTexture();
				pRC->SetPixelShader( psCalcNH );
				pRC->SetVertexShader( vsNHCalcer );
				pRC->SetTexture( 0, pNormalize );
				pRC->SetTexture( 1, pNormalize );
				pRC->SetTexture( 2, pNormalize );
				float f = op.p1.f;
				CVec4 ro[3];
				ro[0] = CVec4(f,f,f,0.5f);
				ro[1] = CVec4(f,f,f,0.5f);
				ro[2] = CVec4(0,0,1,0);
				pRC->SetPSConst( 0, ro, 3 );
				triListType = TLT_GEOM;
			}
			break;
		case RO_NHCALC_BUMP:
			{
				NGfx::CCubeTexture *pNormalize = GetNormalizeTexture();
				pRC->SetPixelShader( psCalcNHBumped );
				pRC->SetVertexShader( vsNHCalcBumped );
				pRC->SetVSConst( 16, CVec3(0,0,1) );
				pRC->SetTexture( 0, op.p1.pTex );
				pRC->SetTexture( 1, pNormalize );
				pRC->SetTexture( 2, pNormalize );
				CVec4 ro[3];
				ro[0] = CVec4(0.5f,0.5f,0.5f,0.5f);
				ro[1] = CVec4(1,1,1,0.5f);
				ro[2] = CVec4(0,0,1,0);
				pRC->SetPSConst( 0, ro, 3 );
				triListType = TLT_GEOM;
			}
			break;
		case RO_PP_SPECULAR_COLOR_DIR:
			pRC->SetPixelShader( psPerPixelSpecular );
			pRC->SetVertexShader( vsDirPPSpecular );
			pRC->SetVSConst( 17, MulPerComp( lightInfo.vGlossColor, *op.p2.pVec3 ) );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			triListType = TLT_GEOM;
			break;
		case RO_PP_SPECULAR_TEXTURE_DIR:
			pRC->SetPixelShader( psPerPixelTexSpecular );
			pRC->SetVertexShader( vsDirPPTexSpecular );
			pRC->SetVSConst( 17, lightInfo.vGlossColor );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			pRC->SetTexture( 2, op.p2.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_PP_SPECULAR_COLOR_PNT:
			pRC->SetPixelShader( psPointPerPixelSpecular );
			pRC->SetVertexShader( vsPointPPSpecular );
			pRC->SetVSConst( 17, MulPerComp( GetVec3( lightInfo.vLightColor ), *op.p2.pVec3 ) );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			pRC->SetTexture( 2, GetLightFalloff() );
			triListType = TLT_GEOM;
			break;
		case RO_PP_SPECULAR_TEXTURE_PNT:
			pRC->SetPixelShader( psPointPerPixelTexSpecular );
			pRC->SetVertexShader( vsPointPPSpecularTex );
			//pRC->SetVSConst( 17, lightInfo.vGlossColor );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			pRC->SetTexture( 1, GetSpecularResponse() );
			pRC->SetTexture( 2, GetLightFalloff() );
			pRC->SetTexture( 3, op.p2.pTex );
			triListType = TLT_GEOM;
			break;

			// cached light
		case RO_CL_PNT_LIGHT:
			pRC->SetPixelShader( psDiffuseProjectedTexture );
			pRC->SetVertexShader( vsCLPointLight );
			pRC->SetTexture( 0, GetLightCircle() );
			triListType = TLT_GEOM;
			break;
		case RO_CL_PNT_DEPTH_CHECK:
			pRC->SetPixelShader( psCLPointDepthCheck );
			pRC->SetAlphaRef( Float2Int( op.p2.f ) );
			pRC->SetVertexShader( vsCLPointDepthCheck );
			pRC->SetVSConst( 16, CVec4( 0.5f, 1 / ( lightInfo.vRadius.x + 4 ), 0, 0 ) );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, op.p1.pCubeTex );
			triListType = TLT_GEOM;
			break;
		case RO_CL_PNT_LIGHT_SHADOWED:
			pRC->SetPixelShader( psCLPointLightShadowed );
			pRC->SetAlphaRef( Float2Int( op.p2.f ) );
			pRC->SetVertexShader( vsCLPointLightShadowed );
			pRC->SetVSConst( 16, CVec4( 0.5f, 1 / ( lightInfo.vRadius.x + 4 ), 0, 0 ) );
			pRC->SetTexture( 0, GetLightCircle() );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, op.p1.pCubeTex );
			triListType = TLT_GEOM;
			break;
		case RO_CL_PNT_LIGHT_BUMP:
			pRC->SetPixelShader( psPrecisePointBump );
			pRC->SetVertexShader( vsPrecisePointBump );
			pRC->SetTexture( 0, GetLightFalloff() );
			pRC->SetTexture( 1, GetNormalizeTexture() );
			pRC->SetTexture( 2, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_CL_SKY_DIR_CHECK:
			pRC->SetPixelShader( psShadowTest );
			pRC->SetVertexShader( vsCLSkyCheck );
			pRC->SetVSConst( 16, op.p1.pDirDepth->vDepth );
			pRC->SetVSConst( 17, op.p1.pDirDepth->vVecU );
			pRC->SetVSConst( 18, op.p1.pDirDepth->vVecV );
			pRC->SetVSConst( 19, CVec4(1,1,1,1) - op.p1.pDirDepth->vChannelSelect );
			pRC->SetTexture( 0, op.p2.pTex );
			SetDepthCmpVal( pRC, op.p1.pPDirDepth->vChannelSelect, 0 );
			triListType = TLT_GEOM;
			break;
		case RO_CL_SKY_LIGHT:
			pRC->SetPixelShader( psDiffuse );
			pRC->SetVertexShader( vsCLSkyLight );
			triListType = TLT_GEOM;
			break;
		case RO_CL_SKY_LIGHT_BUMP:
			pRC->SetPixelShader( psPrecisePerPixelDiffuse );
			pRC->SetVertexShader( vsPreciseDirBumpLight );
			pRC->SetTexture( 0, GetNormalizeTexture() );
			pRC->SetTexture( 1, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_CL_SKY_3LIGHT:
			pRC->SetPixelShader( psCLSkyLight3 );
			pRC->SetVertexShader( vsCLSkyLight3 );
			pRC->SetVSConst( 16, op.p1.pSkyDepth3->channels[0]->vDepth + CVec4( 0, 0, 0, 1.0f / 255 ) );
			pRC->SetVSConst( 17, op.p1.pSkyDepth3->channels[0]->vVecU );
			pRC->SetVSConst( 18, op.p1.pSkyDepth3->channels[0]->vVecV );
			pRC->SetVSConst( 25, op.p1.pSkyDepth3->channels[1]->vDepth + CVec4( 0, 0, 0, 1.0f / 255 ) );
			pRC->SetVSConst( 26, op.p1.pSkyDepth3->channels[1]->vVecU );
			pRC->SetVSConst( 27, op.p1.pSkyDepth3->channels[1]->vVecV );
			pRC->SetVSConst( 28, op.p1.pSkyDepth3->channels[2]->vDepth + CVec4( 0, 0, 0, 1.0f / 255 ) );
			pRC->SetVSConst( 29, op.p1.pSkyDepth3->channels[2]->vVecU );
			pRC->SetVSConst( 30, op.p1.pSkyDepth3->channels[2]->vVecV );
			pRC->SetVSConst( 31, op.p1.pSkyDepth3->vDirs[0] );
			pRC->SetVSConst( 32, op.p1.pSkyDepth3->vDirs[1] );
			pRC->SetVSConst( 33, op.p1.pSkyDepth3->vDirs[2] );
			pRC->SetPSConst( 0, CVec4( 1, 0.01f, 0.01f, 0 ) );
			pRC->SetPSConst( 1, CVec4( 0.01f, 0.01f, 1, 0 ) );
			pRC->SetTexture( 0, op.p2.pTex );
			pRC->SetTexture( 1, op.p2.pTex );
			pRC->SetTexture( 2, op.p2.pTex );
			triListType = TLT_GEOM;
			break;

		case RO_ALIEN:
			{
				pRC->SetPixelShader( psAlienEffect );
				pRC->SetVertexShader( vsAlienEffect );
				pRC->SetTexture( 0, NGfx::GetRegisterTexture(0), true );
				pRC->SetTexture( 1, op.p1.pCubeTex );
				float f = 0.05f;
				if ( NGfx::IsNVidiaNP2Bug() )
					f *= 1000;
				pRC->SetVSConst( 14, CVec4(0,0,1,0) );
				pRC->SetVSConst( 16, GetNormalized( pRC->GetTransform().forward.x, 0.1f ) );
				pRC->SetVSConst( 17, GetNormalized( pRC->GetTransform().forward.y, 0.1f ) );
			}
			break;
		case RO_EXPLOSION_DECAL:
			pRC->SetPixelShader( psDiffuseTexture );
			pRC->SetVertexShader( vsTextureTexU );
			pRC->SetTexture( 0, op.p1.pTex );
			triListType = TLT_GEOM;
			break;
		case RO_REGISTER:
			pRC->SetPixelShader( psProjectedTexture );
			pRC->SetVertexShader( vsRegisterMap );
			pRC->SetTexture( 0, NGfx::GetRegisterTexture( Float2Int( op.p1.f ) ), true );
			break;
		default: ASSERT(0); break;
		}
		AddTriangles( pRC, *op.pFrag, scene, triListType );
	}
	if ( pPrevOp )
		pRC->Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Execute( IRender *pRender, NGfx::CRenderContext *pRC, const CTransformStack &ts, const CRenderCmdList &cl,
	const CSceneFragments &scene, const SLightInfo &lightInfo )
{
	pRC->SetTransform( ts.Get() );
	ExecOps( pRC, cl.ops, scene, lightInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
