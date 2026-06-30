#include "StdAfx.h"
#include "GfxBuffers.h"
#include "GfxRender.h"
#include "GTransparent.h"
#include "GParticleInfo.h"
#include "Transform.h"
#include "GfxUtils.h"
#include "GSceneParticles.h"
#include "GfxEffects.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
const int N_RESERVE_DEPTH_BUFFER = 4096;
const int N_PARTICLES_PER_EFFECT_LG2 = 17;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTransparentRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////
CTransparentRenderer::CTransparentRenderer( const CTransformStack &ts, const CTPoint<int> &vLightBuffersize, 
	bool _bUseFakeLM, DWORD _dwLitColor, DWORD _dwNormalColor )
	: nTotalParticles(0), nLitParticles(0), dwLitColor(_dwLitColor), dwNormalColor(_dwNormalColor),
	nElementPtr(0), nInfoIdx(-(1<<N_PARTICLES_PER_EFFECT_LG2))
{
	infos.reserve( 100 );
	depths.resize( N_RESERVE_DEPTH_BUFFER * 2 );
	sourcePtrs.resize( N_RESERVE_DEPTH_BUFFER * 2 );
	bTnLMode = NGfx::IsTnLDevice();
	SHMatrix invRoot;
	invRoot = ts.Get().backward * ts.GetProjection().forward;
	orientation.vBasic[0] = CVec3( invRoot._11, invRoot._21, invRoot._31 );
	orientation.vBasic[1] = CVec3( invRoot._12, invRoot._22, invRoot._32 );
	orientation.vBasic[2] = CVec3( invRoot._13, invRoot._23, invRoot._33 );
	orientation.vBasic[3] = invRoot.GetTranslation();
	orientation.vDepth = CVec3( -ts.Get().forward.wx, -ts.Get().forward.wy, -ts.Get().forward.wz );;

	bUseFakeLM = _bUseFakeLM;
	if ( bUseFakeLM )
	{
		ASSERT( vLightBuffersize.x == 2 );
		ASSERT( vLightBuffersize.y == 1 );
		litParticlesAlloc.Init( 1, 1, 2, 1, false );
		kernel = litParticlesAlloc;
		litParticlesAlloc.ForcedInc();
	}
	else
	{
		litParticlesAlloc.Init( 4, 4, Float2Int( vLightBuffersize.x ), Float2Int( vLightBuffersize.y ), true );
		kernel = litParticlesAlloc;
		litParticlesAlloc.Inc();
		kernel.StopIncrementing();
	}
	NGfx::CalcCompactVector( &vNormal, -orientation.vBasic[2] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
STransparentInfo* CTransparentRenderer::AddFragment() 
{ 
	nInfoIdx += 1 << N_PARTICLES_PER_EFFECT_LG2;
	// make sure enough place is allocated
	if ( nElementPtr + N_RESERVE_DEPTH_BUFFER > depths.size() )
	{
		depths.resize( depths.size() + N_RESERVE_DEPTH_BUFFER * 2 );
		sourcePtrs.resize( sourcePtrs.size() + N_RESERVE_DEPTH_BUFFER * 2 );
	}
	infoStartIdx.push_back( nElementPtr );
	return &*infos.insert( infos.end(), STransparentInfo());
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::AllocParticlesWriteBuffer()
{
	if ( bTnLMode )
		pParticlesGeometry = pTnLParticlesGeometry = new CTnLParticlesGeometry( orientation );
	else
		pParticlesGeometry = pShaderParticlesGeometry = new CShaderParticlesGeometry( orientation, vNormal );
	pParticlesTrilist = new CParticlesTriList;
	nTargetParticle = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::FinishParticles() 
{
	if ( IsValid(pParticlesGeometry) )
		pParticlesGeometry->FreeWriteBuffer();
	pParticlesGeometry = 0;
	pTnLParticlesGeometry = 0;
	pShaderParticlesGeometry = 0;
	pParticlesTrilist = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::FinishParticlesPiece()
{
	int nParticles = nTargetParticle - nPieceStart;

	pParticlesTrilist->AddPart( nParticles );
	SBound bv;
	pParticlesGeometry->FinishPart( &bv, pLMAlloc );
	if ( pReportParticles )
	{
		pReportParticles->AddParticles( pParticlesGeometry, pParticlesTrilist, pParticlesGeometry->GetPartsNum() - 1, 
			nParticles, bv );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::StartParticlesPiece()
{
	STransparentInfo *pWriteParticles = AddFragment();
	pWriteParticles->pGeom = pParticlesGeometry;
	pWriteParticles->fDepth = currentEffectBV.s.ptCenter * GetDepth();
	pWriteParticles->nOffset = nTargetParticle * 4;
	nPieceStart = nTargetParticle;
	int nEffectParticle = nTargetParticle - nTargetStart;
	pParticlesGeometry->Start( pCurrentEffect, nEffectParticle, currentEffectBV, *pLMAlloc, dwCurrentParticleColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::AddParticles( IParticles *pParticles, bool bIsLit, const SBound &bv, 
	IReportParticlesGeometry *pStore ) 
{
	pCurrentEffect = pParticles->GetEffect();
	if ( pCurrentEffect->IsEmpty() )
		return;
	pReportParticles = pStore;
	currentEffectBV = bv;
	if ( bIsLit )
	{
		dwCurrentParticleColor = dwLitColor;
		pLMAlloc = &litParticlesAlloc;
	}
	else
	{
		dwCurrentParticleColor = dwNormalColor;
		pLMAlloc = &kernel;
	}
	if ( !IsValid(pParticlesGeometry) )
		AllocParticlesWriteBuffer();
	else if ( pParticlesGeometry->GetPartsNum() == PF_MAX_PARTS_PER_COMBINER )
	{
		pParticlesGeometry->FreeWriteBuffer();
		AllocParticlesWriteBuffer();
	}
	nTargetStart = nTargetParticle;
	StartParticlesPiece();
	
	pCurrentEffect->AddParticles( this );
	
	FinishParticlesPiece();
	nTotalParticles += nTargetParticle - nTargetStart;
	if ( bIsLit )
		nLitParticles += nTargetParticle - nTargetStart;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::AddParticleOverflow( const CVec3 vPos[4], DWORD dwColor, const STransparentTexturePlace &tPlace,
	float fDepth )
{
	// buffer is full, start a new one
	if ( bTnLMode )
		pTnLParticlesGeometry->RealAddParticle( vPos, dwColor, tPlace, fDepth );
	else
		pShaderParticlesGeometry->RealAddParticle( vPos, dwColor, tPlace, fDepth );
	FinishParticlesPiece();
	pParticlesGeometry->FreeWriteBuffer();

	int nEffectParticle = nTargetParticle - nTargetStart;
	AllocParticlesWriteBuffer();
	nTargetStart = nTargetParticle - nEffectParticle;

	StartParticlesPiece();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::AddParticle( const CVec3 vPos[4], DWORD dwColor, const STransparentTexturePlace &tPlace,
	float fDepth )
{
	// store particle depth info
	depths[ nElementPtr ] = fDepth;
	sourcePtrs[ nElementPtr ] = nInfoIdx | ( nTargetParticle - nPieceStart + 1 );
	++nElementPtr;
	//
	++nTargetParticle;
	if ( nTargetParticle < N_PARTICLES_BUFFER_SIZE / 4 )
	{
		if ( bTnLMode )
			pTnLParticlesGeometry->RealAddParticle( vPos, dwColor, tPlace, fDepth ); // to enable tail call optimization
		else
			pShaderParticlesGeometry->RealAddParticle( vPos, dwColor, tPlace, fDepth ); // to enable tail call optimization
	}
	else
		AddParticleOverflow( vPos, dwColor, tPlace, fDepth );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::AddElement( SRenderGeometryInfo *pGeometry, IMaterial *pMaterial, int nIndex, float fDepth )
{
	STransparentInfo *pFrag = AddFragment();
	pFrag->fDepth = fDepth;
	pFrag->nOffset = nIndex;
	pFrag->objectInfo.pGeometry = pGeometry;
	pFrag->objectInfo.pMaterial = pMaterial;
	// store depth info
	depths[ nElementPtr ] = fDepth;
	sourcePtrs[ nElementPtr ] = nInfoIdx;
	++nElementPtr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ERadixFetch
{
	RF_NORMAL,
	RF_BACK,
	RF_FLOAT
};
static void RadixSortStage( int shift, const float *pDepths, vector<int> *pRes, ERadixFetch fetch )
{
	int lists[256];
	memset( lists, -1, sizeof(lists) );
	int nCount = pRes->size();
	vector<int> temp( nCount );
	for ( int k = 0; k < nCount; ++k )
	{
		int nIdx = (*pRes)[k];
		int n = ( (*(int*)(&pDepths[ nIdx ])) >> shift ) & 0xff; // determine pile to which this element is to be stored
		temp[nIdx] = lists[n];
		lists[n] = nIdx;
	}
	int *pOut = &(*pRes)[0], *pFinal = pOut + nCount - 1;
	switch ( fetch )
	{
		case RF_NORMAL:
			for ( int i = 0; i < 256; i++ )
			{
				for ( int nIdx = lists[i]; nIdx >= 0; nIdx = temp[nIdx] )
					*pOut++ = nIdx;
			}
			ASSERT( pOut == pFinal + 1 );
			break;
		case RF_BACK:
			for ( int i = 255; i >= 0; i-- )
			{
				for ( int nIdx = lists[i]; nIdx >= 0; nIdx = temp[nIdx] )
					*pOut++ = nIdx;
			}
			ASSERT( pOut == pFinal + 1 );
			break;
		case RF_FLOAT:
			for ( int i = 255; i >= 128; i-- )
			{
				for ( int nIdx = lists[i]; nIdx >= 0; nIdx = temp[nIdx] )
					*pOut++ = nIdx;
			}
			for ( int i = 127; i >= 0; i-- )
			{
				for ( int nIdx = lists[i]; nIdx >= 0; nIdx = temp[nIdx] )
					*pFinal-- = nIdx;
			}
			break;
	}
}
// lowest first
static void DoRadixSort( const float *pSrc, int nSize, vector<int> *pRes )
{
	pRes->resize( nSize );
	if ( nSize <= 0 )
		return;
	for ( int k = 0; k < pRes->size(); ++k )
		(*pRes)[k] = k;
	//RadixSortStage( 0, pSort, pSort1, nElem, 1 );
	RadixSortStage( 8, pSrc, pRes, RF_BACK );
	RadixSortStage( 16, pSrc, pRes, RF_NORMAL );
	RadixSortStage( 24, pSrc, pRes, RF_FLOAT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DoRadixSort( const vector<float> &src, vector<int> *pRes )
{
	DoRadixSort( &src[0], src.size(), pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetObjectEffect( bool bTnLMode, NGfx::CRenderContext *pRC, const STransparentMaterialInfo &info, 
	NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky )
{
	NGfx::SEffect *pEff;
	NGfx::SEffFoggedParticle efFog;
	NGfx::SEffTexture efTexture;
	NGfx::SEffFoggedTextureAndMirror efFoggedSimple;
	NGfx::SEffTextureAndMirror efSimple;
	NGfx::SEffTnLTexture efTnLTexture;
	if ( bTnLMode )
	{
		efTnLTexture.pTex = info.pTex;
		pEff = &efTnLTexture;
	}
	else
	{
		if ( pSky && ( info.fDielMirror > 0 || info.fMetalMirror > 0 ) )
		{
			if ( pFog && NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
			{
				efFoggedSimple.pTex = info.pTex;
				efFoggedSimple.pTexMirror = pSky;
				efFoggedSimple.pFog = pFog;
				efFoggedSimple.fMetalMirror = info.fMetalMirror;
				efFoggedSimple.fDielMirror = info.fDielMirror;
				pEff = &efFoggedSimple;
			}
			else
			{
				efSimple.pTex = info.pTex;
				efSimple.pTexMirror = pSky;
				efSimple.fMetalMirror = info.fMetalMirror;
				efSimple.fDielMirror = info.fDielMirror;
				pEff = &efSimple;
			}
		}
		else
		{
			if ( pFog )
			{
				efFog.pFog = pFog;
				efFog.pTex = info.pTex;
				pEff = &efFog;
			}
			else
			{
				efTexture.pTex = info.pTex;
				pEff = &efTexture;
			}
		}
	}
	pRC->SetEffect( pEff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetParticlesEffect( bool bTnLMode, NGfx::CRenderContext *pRC, NGfx::CTexture *pLight, NGfx::CTexture *pFog )
{
	if ( bTnLMode )
	{
		NGfx::SEffTnLParticles eff;
		pRC->SetEffect( &eff );
	}
	else
	{
		if ( pFog && NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 )
		{
			NGfx::SEffFoggedTransparentParticles eff;
			eff.pLight = pLight;
			eff.pFog = pFog;
			pRC->SetEffect( &eff );
		}
		else
		{
			NGfx::SEffTransparentParticles eff;
			eff.pLight = pLight;
			pRC->SetEffect( &eff );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_UNUSED_SRC_PTR = 0xffffffff;
class CPreciseTranspRender
{
	vector<STransparentInfo> &infos;
	vector<float> &depths;
	vector<int> &sourcePtrs;
	vector<STriangle> tris;
	unsigned int nCurrentSrc, nCurrentBase;
	NGfx::CRenderContext *pRC;
	const STransparentMaterialInfo *pCurrentEffect;

	void Flush()
	{
		if ( nCurrentSrc == N_UNUSED_SRC_PTR )
			return;
		NGfx::STriangleList triList;
		triList.pTri = &tris[0];
		triList.nTris = tris.size();
		triList.nBaseIndex = infos[nCurrentSrc].nOffset;
		CDGPtr<IVBCombiner> pVertices( infos[nCurrentSrc].pGeom );
		pVertices.Refresh();
		pRC->AddPrimitive( pVertices->GetValue(), triList );
		tris.resize( 0 );
		nCurrentSrc = N_UNUSED_SRC_PTR;
	}
public:
	CPreciseTranspRender( NGfx::CRenderContext *_pRC, vector<STransparentInfo> *pInfos, vector<float> *pDepths, 
		vector<int> *pSrcPtrs ) : infos(*pInfos), depths(*pDepths), sourcePtrs(*pSrcPtrs), pRC(_pRC)
	{
	}
	void Render( bool bTnLMode, NGfx::CTexture *pLight, NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky )
	{
		SetParticlesEffect( bTnLMode, pRC, pLight, pFog );
		pCurrentEffect = 0;
		vector<int> sorted;
		DoRadixSort( depths, &sorted );
		nCurrentSrc = N_UNUSED_SRC_PTR;
		for ( int k = 0; k < sorted.size(); ++k )
		{
			unsigned int nSrcPtr = (unsigned int)sourcePtrs[ sorted[k] ];
			int nParticle = nSrcPtr & ( (1<<N_PARTICLES_PER_EFFECT_LG2) - 1 );
			unsigned int nSrc = nSrcPtr >> N_PARTICLES_PER_EFFECT_LG2;
			if ( nParticle == 0 ) // IsObject
			{
				Flush();
				const STransparentInfo &info = infos[nSrc];
				const STransparentMaterialInfo &transpMatInfo = info.objectInfo.pMaterial->GetTransparentInfo();
				if ( !pCurrentEffect || *pCurrentEffect != transpMatInfo )
				{
					pRC->Flush();
					SetObjectEffect( bTnLMode, pRC, transpMatInfo, pFog, pSky );
					pCurrentEffect = &transpMatInfo;
				}
				SRenderGeometryInfo *pGeometry = info.objectInfo.pGeometry;
				pGeometry->pVertices.Refresh();
				pGeometry->pTriLists[TLT_GEOM].Refresh();
				const vector<NGfx::STriangleList> &tris = pGeometry->pTriLists[TLT_GEOM]->GetValue();
				if ( !tris.empty() )
					pRC->AddPrimitive( pGeometry->pVertices->GetValue(), tris[ info.nOffset ] );
			}
			else
			{
				if ( nSrc != nCurrentSrc )
				{
					Flush();
					if ( pCurrentEffect )
					{
						pRC->Flush();
						SetParticlesEffect( bTnLMode, pRC, pLight, pFog );
						pCurrentEffect = 0;
					}
				}
				nCurrentSrc = nSrc;
				int nBase = ( nParticle - 1 ) * 4;
				tris.push_back( STriangle( nBase, nBase + 1, nBase + 2 ) );
				tris.push_back( STriangle( nBase, nBase + 2, nBase + 3 ) );
			}
		}
		Flush();
		pRC->Flush();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCmpFragments
{
	const vector<STransparentInfo> &infos;
	SCmpFragments( const vector<STransparentInfo> &_infos ) : infos(_infos) {}
	bool operator()( int n1, int n2 )
	{
		return infos[n1].fDepth < infos[n2].fDepth;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::RealRender( NGfx::CRenderContext *pRC, NGfx::CTexture *pLight,
	NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky )
{
	NGfx::CRenderContext &rc = *pRC;
	rc.Use();
	rc.SetAlphaCombine( NGfx::COMBINE_SMART_ALPHA );
	rc.SetDepth( NGfx::DEPTH_TESTONLY );
	rc.SetStencil( NGfx::STENCIL_NONE );
	if ( NGfx::CanStreamGeometry() )
	{
		CPreciseTranspRender pr( pRC, &infos, &depths, &sourcePtrs );
		pr.Render( bTnLMode, pLight, pFog, pSky );
	}
	else
	{
		vector<int> fragments;
		fragments.resize( infos.size() );
		for ( int k = 0; k < fragments.size(); ++k )
			fragments[k] = k;
		sort( fragments.begin(), fragments.end(), SCmpFragments( infos ) );
		for ( int k = 0; k < fragments.size(); ++k )
		{
			int nFragment = fragments[k];
			const STransparentInfo &info = infos[ nFragment ];
			if ( info.pGeom == 0 )
			{
				SetObjectEffect( bTnLMode, pRC, info.objectInfo.pMaterial->GetTransparentInfo(), pFog, pSky );
				SRenderGeometryInfo *pGeometry = info.objectInfo.pGeometry;
				pGeometry->pVertices.Refresh();
				pGeometry->pTriLists[TLT_GEOM].Refresh();
				const vector<NGfx::STriangleList> &tris = pGeometry->pTriLists[TLT_GEOM]->GetValue();
				if ( !tris.empty() )
				{
					pRC->AddPrimitive( pGeometry->pVertices->GetValue(), tris[ info.nOffset ] );
					pRC->Flush();
				}
			}
			else
			{
				SetParticlesEffect( bTnLMode, pRC, pLight, pFog );

				int nStart = infoStartIdx[ nFragment ], nFinish = infoStartIdx[ nFragment + 1 ];
				int nParticles = nFinish - nStart;
				nParticles = Min( nParticles, NGfx::N_MAX_RECTANGLES ); // CRAP
				if ( nParticles == 0 )
					continue;

				vector<int> particles( nParticles );
				for ( int i = 0; i < particles.size(); ++i )
					particles[i] = i;
				DoRadixSort( &depths[ nStart ], nParticles, &particles );
				CObj<NGfx::CTriList> pTriList;
				{
					NGfx::CBufferLock<NGfx::S3DTriangle> tris( &pTriList, nParticles * 2 );
					//int nOffset = info.nOffset;
					for ( int i = 0; i < particles.size(); ++i )
					{
						int nBase = particles[i] * 4; //+ nOffset;
						tris[ i*2 + 0 ] = NGfx::S3DTriangle( nBase, nBase + 1, nBase + 2 );
						tris[ i*2 + 1 ] = NGfx::S3DTriangle( nBase, nBase + 2, nBase + 3 );
					}
				}
				CDGPtr<IVBCombiner> pGeom( info.pGeom );
				pGeom.Refresh();
				rc.DrawPrimitive( pGeom->GetValue(), pTriList, info.nOffset, nParticles * 4 );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTransparentRenderer::Render( NGfx::CRenderContext *pRC,
	NGfx::CTexture *pParticleLight, NGfx::CTexture *pFog, NGfx::CCubeTexture *pSky )
{
	FinishParticles();
	depths.resize( nElementPtr );
	sourcePtrs.resize( nElementPtr );
	infoStartIdx.push_back( nElementPtr );
	if ( sourcePtrs.empty() )
		return;
	RealRender( pRC, pParticleLight, pFog, pSky );
	infoStartIdx.pop_back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
