#include "StdAfx.h"
#include "Gfx.h"
#include "GInit.h"
#include "GfxRender.h"
#include "GfxBuffers.h"
#include "GfxEffects.h"
#include "GfxUtils.h"
#include "GPixelFormat.h"
#include "DG.h"
#include "GScene.h"
#include "Transform.h"
#include "GSceneInternal.h"
#include "GMaterial.h"
#include "GClipper.h"
#include "GCombiner.h"
#include "GRenderCore.h"
#include "GRenderLight.h"
#include "GCombiner.h"
#include "..\Misc\HPTimer.h"
#include "..\Misc\2DArray.h"
#include "GfxUtils.h"
#include "GRenderFactor.h"
#include "GParticleInfo.h"
#include "Bound.h"
#include "SuperCollider.h" // for TraceStatic(...)
#include "GShadowMap.h" // CRAP
#include "GDecal.h"
#include "GDecalGeometry.h"
#include "..\Misc\RandomGen.h"

#include "..\MiscDll\Commands.h" // for lm command
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_SELECTION_STEP = 0.03f;
const int N_SKIP_IGNORED_TEST = -1;
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static SRenderStats lastFrameStats;
static bool bWireframe;
static bool bShow2DTextureCache = false, bShowTranspTextureCache = false, bShowParticleLMCache = false;
static int nTotalParts, nTotalElements;
enum EShowLinearCache
{
	SLC_NONE,
	SLC_STATIC,
	SLC_DYNAMIC
};
static EShowLinearCache showLinearCache = SLC_NONE;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGetTranspCache : public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_NOCOPY_METHODS(CGetTranspCache);
	void Recalc() { pValue = NGfx::GetTransparentTextureCache(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightGroup
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLightGroup: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CLightGroup);
	ZDATA
	CPtr<CGScene> pGame;
	int nGroup;
public:
	SDynamicAmbientInfo ambientData;
	CVec3 vPrevPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGame); f.Add(3,&nGroup); return 0; }
	CLightGroup() {}
	CLightGroup( CGScene *p, int _n ) : pGame(p), nGroup(_n), vPrevPos(-1e6f, -1e6f, -1e6f) {}
	~CLightGroup()
	{
		if ( IsValid( pGame ) )
			pGame->FreeLightGroup( nGroup );
	}
	int GetGroup() const { return nGroup; }
};
int GetGroup( CLightGroup *p ) { return p->GetGroup(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicLightCache
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDynamicLightCache::CheckStatic()
{
	bool bRes = pStaticChanged.Refresh();
	if ( bRes )
		data.clear();
	if ( data.size() > 500 )
		data.clear();
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SDynamicAmbientInfo& CDynamicLightCache::Calc( const SSphere &bv, CGScene *pScene )
{
	CHash::iterator i = data.find( bv.ptCenter );
	if ( i == data.end() )
	{
		SDynamicAmbientInfo *pRes = &data[ bv.ptCenter ];
		pScene->TraceDynamicAmbient( pRes, bv );
		return *pRes;
	}
	else
		return i->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AttachPart( CNonePart *pRes, CVolumeNode *pNode, 
	SStaticTrackers *pTrackers, bool bIsDynamic,
	bool bAnimationOnly )
{
	bool bIsLightmapped = !bIsDynamic && pRes->GetGroupInfo().nLightGroup == 0 && pRes->GetMaterial()->GetType() == IMaterial::MT_NORMAL;//!= IMaterial::MT_OCCLUDER;
	CCombinedPart *pCPart;
	if ( bIsDynamic )
		pCPart = pNode->dynamicParts.GetCombinerPartForAdd( pRes->GetMaterial(), 0, bIsLightmapped );
	else
		pCPart = pNode->staticParts.GetCombinerPartForAdd( pRes->GetMaterial(), pTrackers, bIsLightmapped );
	pRes->pOwner = pCPart;
	if ( bAnimationOnly )
		pRes->SetCombiner( pCPart->GetCombiner(), false, true );
	else
		pRes->SetCombiner( pCPart->GetCombiner(), true, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetGeometryObjectInfo( CObjectBase *p, 
	CPtrFuncBase<CObjectInfo> **pGeometry, SFBTransform *pPos, SFullGroupInfo *pGroupInfo )
{
	CDynamicCast<CNonePart> pPart( p );
	if ( !pPart )
		return false;
	switch ( pPart->GetTransformType() )
	{
		case TT_NONE:
			Identity( &pPos->forward );
			Identity( &pPos->backward );
			break;
		case TT_SIMPLE:
			*pPos = pPart->GetSimplePos();
			break;
		case TT_SIMPLE_DISCRETE:
			pPart->GetDiscretePos().MakeMatrix( pPos );
			break;
		default:
			return false;
	}
	*pGeometry = pPart->GetObjectInfoNode();
	*pGroupInfo = pPart->GetFullGroupInfo();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGenericDynamicPart
////////////////////////////////////////////////////////////////////////////////////////////////////
CGenericDynamicPart::CGenericDynamicPart( CPtrFuncBase<CObjectInfo> *pData, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo )
	: CNonePart( pData, _pMaterial, _gInfo ), nStillCounter(0), pTrackObjInfo(pData), bObjectInfoChanged(false)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicPart
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynamicPart::CDynamicPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase<SFBTransform> *pPos, 
	IMaterial *_pMaterial, const SFullGroupInfo &_gInfo )
	: CGenericDynamicPart( pData, _pMaterial, _gInfo ), pTransform(pPos)
{
//	RefreshObjectInfo();
//	GetObjectInfo()->CalcBound( &bound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDynamicPart::Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers )
{
	bObjectInfoChanged |= pTrackObjInfo.Refresh();
	CObjectInfo *pObjInfo = pTrackObjInfo->GetValue();//GetObjectInfo();
	if ( !pObjInfo )
	{
		SetCombiner( 0, true, true );
		return true;
	}
	if ( bObjectInfoChanged )
		pObjInfo->CalcBound( &bound );
	if ( pTransform.Refresh() | bObjectInfoChanged )
	{
		bObjectInfoChanged = false;
		nStillCounter = 0;
		CVolumeNode *pNode = pVolume->SelectNode( pTransform, bound );
		ASSERT( pNode );
		AttachPart( this, pNode, pTrackers, true, true );
	}
	else
	{
		++nStillCounter;
		if ( nStillCounter == 3 && GetGroupInfo().nLightGroup == 0 )
		{
			CVolumeNode *pNode = pVolume->SelectNode( pTransform, bound );
			ASSERT( pNode );
			AttachPart( this, pNode, pTrackers, false, false );
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimatedPart
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimatedPart::CAnimatedPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase< vector<SHMatrix> > *_pAnim,
	CFuncBase<vector<NGfx::SCompactTransformer> > *_pMMXAnim, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo )
	: CGenericDynamicPart( pData, _pMaterial, _gInfo ), pAnimation(_pAnim), pMMXAnimation(_pMMXAnim)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimatedPart::EstimateBound( SBound *pRes )
{
	// estimate bound
	SBoundCalcer bc;
	const vector<SHMatrix> &anim = pAnimation->GetValue();
	for ( int i=0; i<anim.size(); ++i )
		bc.Add( anim[i].GetTranslation(), 0 );
	bc.Make( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_MAX_DISTANCE_TO_BONE = 0.5f;
bool CAnimatedPart::Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers )
{
	bObjectInfoChanged |= pTrackObjInfo.Refresh();//RefreshObjectInfo();
	CObjectInfo *pObjInfo = pTrackObjInfo->GetValue();//GetObjectInfo();
	if ( !pObjInfo )
	{
		SetCombiner( 0, true, true );
		return true;
	}
	if ( pAnimation.Refresh() | bObjectInfoChanged )
	{
		bObjectInfoChanged = false;
		SBound bv;
		EstimateBound( &bv );
		// update position
		nStillCounter = 0;
		CVolumeNode *pNode = pVolume->GetNode( bv.s.ptCenter, bv.s.fRadius );
		ASSERT( pNode );
		AttachPart( this, pNode, pTrackers, true, true );
	}
	else
	{
		++nStillCounter;
		if ( nStillCounter == 3 && GetGroupInfo().nLightGroup == 0 )
		{
			SBound bv;
			EstimateBound( &bv );
			CVolumeNode *pNode = pVolume->GetNode( bv.s.ptCenter, bv.s.fRadius );
			ASSERT( pNode );
			AttachPart( this, pNode, pTrackers, false, false );
		}
	}
	pMMXAnimation.Refresh();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicGeometryPart
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynamicGeometryPart::CDynamicGeometryPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase<SBound> *_pBound, 
	IMaterial *_pMaterial, const SFullGroupInfo &_gInfo )
	: CGenericDynamicPart( pData, _pMaterial, _gInfo ), pBound(_pBound)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDynamicGeometryPart::Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers )
{
	bObjectInfoChanged |= pTrackObjInfo.Refresh();//RefreshObjectInfo();
	CObjectInfo *pObjInfo = pTrackObjInfo->GetValue();//GetObjectInfo();
	if ( !pObjInfo )
	{
		SetCombiner( 0, true, true );
		return true;
	}
	pBound.Refresh();
	if ( bObjectInfoChanged )
	{
		bObjectInfoChanged = false;
		// update position
		nStillCounter = 0;
		const SBound &bv = pBound->GetValue();
		CVolumeNode *pNode = pVolume->GetNode( bv.s.ptCenter, bv.s.fRadius );
		ASSERT( pNode );
		AttachPart( this, pNode, pTrackers, true, false );
	}
	else
	{
		++nStillCounter;
		if ( nStillCounter == 3 && GetGroupInfo().nLightGroup == 0 )
		{
			const SBound &bv = pBound->GetValue();
			CVolumeNode *pNode = pVolume->GetNode( bv.s.ptCenter, bv.s.fRadius );
			ASSERT( pNode );
			AttachPart( this, pNode, pTrackers, false, false );
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CParticles
////////////////////////////////////////////////////////////////////////////////////////////////////
CParticleEffect* CParticles::GetEffect()
{
	pParticles.Refresh();
	return pParticles->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CParticles::Update( CVolumeNode *pVolume )
{
	if ( pPlacement.Refresh() )
	{
		TransformBound( &transformedBound, bound, pPlacement->GetValue().forward );
		CVolumeNode *pNewNode = pVolume->SelectNode( pPlacement, bound );
		if ( pNode != pNewNode )
		{
			pNewNode->particles.push_back( this );
			if ( pNode )
				pNode->particles.remove( this );
			pNode = pNewNode;
		}
	}
	pParticles.Refresh();
	return !pParticles->GetValue()->bEnd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSelection
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::Initialize( CObjectBase *pObject, const CVec4 &_vColor )
{
	vColor = _vColor;
	CDynamicCast<CNonePart> pPart(pObject);
	if (pPart)
	{
		pTarget = pPart;
		if ( 1 )//NGfx::IsTnLDevice() )
		{
			pAnimation = new CVersioningBase;
			pCombo = new CAutomaticCombiner;
			pCombo->AddPart( pTarget );
			pOffsetVertices = new CVBCombiner( pCombo, LT_TNL_SELECTION, CT_DYNAMIC, pAnimation );
			pTriList = new CIBCombiner( pCombo, LT_NONE, IBTT_VERTICES );
			pOffsetVertices->SetOffset( F_SELECTION_STEP );
		}
		return true;
	}
	//ASSERT(0);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSelection::Render( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask, bool bOffset )
{
	if ( !IsValid( pTarget ) || !IsValid( pTarget->pOwner ) )
		return;
	SRenderGeometryInfo *pGeom = pTarget->pOwner->GetGeometryInfo();
	// skip animating not visible selection
	// requires that selection was rendered after normal scene without marking new DG frame
	if ( !pGeom->pVertices->WasRefreshed() )
		return;
	const vector<CPtr<IPart> > &parts = pTarget->pOwner->GetCombiner()->GetValue();
	int nIdx = -1;
	for ( int k = 0; k < parts.size(); ++k )
		if ( parts[k].GetPtr() == pTarget.GetPtr() )
			nIdx = k;
	if ( nIdx < 0 )
		return;
	if ( !pTarget->pOwner->GetPartsInfo()[nIdx].groupInfo.IsMaskMatch( mask ) )
		return;
	if ( !pTS->IsIn( pGeom->pVertices->GetBounds()[ nIdx ] ) )
		return;
	if ( bOffset && NGfx::IsTnLDevice() )
	{
		pAnimation->Updated();
		pOffsetVertices.Refresh();
		pTriList.Refresh();
		pRC->AddPrimitive( pOffsetVertices->GetValue(), pTriList->GetValue()[0] );
	}
	else if ( pGeom->pTriLists[TLT_GEOM] )
	{
		pGeom->pTriLists[TLT_GEOM].Refresh();
		pRC->AddPrimitive( pGeom->pVertices->GetValue(), pGeom->pTriLists[TLT_GEOM]->GetValue()[nIdx] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelection::Update( IGScene *pScene )
{
	return IsValid( pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPostProcessBinder
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPostProcessBinder::Initialize( CObjectBase *_p, IPostProcess *_pPost )
{
	pPostProcess = _pPost;
	CDynamicCast<CNonePart> pPart(_p);
	if (pPart)
	{
		pTarget = pPart;
		return true;
	}
	ASSERT(0);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPostProcessBinder::Store( vector<IPostProcess::SObject> *pRes, CTransformStack *pTS, const SGroupSelect &mask )
{
	if ( !IsValid( pTarget ) || !IsValid( pTarget->pOwner ) )
		return;
	SRenderGeometryInfo *pGeom = pTarget->pOwner->GetGeometryInfo();
	// skip animating not visible selection
	// requires that selection was rendered after normal scene without marking new DG frame
	if ( !pGeom->pVertices->WasRefreshed() )
		return;
	const vector<CPtr<IPart> > &parts = pTarget->pOwner->GetCombiner()->GetValue();
	int nIdx = -1;
	for ( int k = 0; k < parts.size(); ++k )
		if ( parts[k].GetPtr() == pTarget.GetPtr() )
			nIdx = k;
	if ( nIdx < 0 )
		return;
	if ( !pTarget->pOwner->GetPartsInfo()[nIdx].groupInfo.IsMaskMatch( mask ) )
		return;
	if ( !pTS->IsIn( pGeom->pVertices->GetBounds()[ nIdx ] ) )
		return;
	if ( pGeom->pTriLists[TLT_GEOM] )
		pRes->push_back( IPostProcess::SObject( pGeom, nIdx ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCombinedPart
////////////////////////////////////////////////////////////////////////////////////////////////////
CCombinedPart::CCombinedPart( SStaticTrackers *pTrackers, EType t )
	: nIgnoreMark(0), pCombiner( new CPerMaterialCombiner( pTrackers ) ), partType(t)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCombinedPart::InitGeometry()
{
	SRenderGeometryInfo &gi = geometryInfo;
	CVersioningBase *pTracker = pCombiner->GetTracker();
	if ( partType == CP_OCCLUDER )
	{
		gi.pVertices = new CVBCombiner( pCombiner, LT_POSITION, pTracker ? CT_STATIC : CT_DYNAMIC, pCombiner->GetAnimation() );
		gi.pTriLists[TLT_POSITION] = new CIBCombiner( pCombiner, LT_POSITION, IBTT_POSITIONS );
		gi.pTriLists[TLT_GEOM] = 0;
	}
	else
	{
		//ASSERT( partType == CP_NORMAL );
		gi.pVertices = new CVBCombiner( 
			pCombiner, 
			NGfx::IsTnLDevice() ? LT_TNL : LT_NONE, 
			pTracker ? CT_STATIC : CT_DYNAMIC, 
			pCombiner->GetAnimation() );
		gi.pTriLists[TLT_POSITION] = new CIBCombiner( pCombiner, LT_NONE, IBTT_POSITIONS );
		gi.pTriLists[TLT_GEOM] = new CIBCombiner( pCombiner, LT_NONE, IBTT_VERTICES );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SRenderGeometryInfo* CCombinedPart::GetGeometryInfo() 
{
	if ( !geometryInfo.pVertices || bWasTnL != NGfx::IsTnLDevice() )
	{
		InitGeometry();
		bWasTnL = NGfx::IsTnLDevice();
	}
	return &geometryInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCombinedPart::UpdatePartInfo()
{
	if ( !pCombiner.Refresh() )
		return;
	CDGPtr<CFuncBase<vector< CPtr<IPart> > > > pC( GetCombiner() );
	pC.Refresh();
	const vector< CPtr<IPart> > &src = pC->GetValue();
	parts.resize( src.size() );
	materials.resize( 0 );
	for ( int k = 0; k < src.size(); ++k )
	{
		CDynamicCast<CNonePart> p( src[k] );
		IMaterial *pMaterial = p->GetMaterial();
		int nMat = -1;
		for ( int i = 0; i < materials.size(); ++i )
		{
			if ( materials[i] == pMaterial )
			{
				nMat = i;
				break;
			}
		}
		if ( nMat < 0 )
		{
			nMat = materials.size();
			materials.push_back( pMaterial );
		}
		// find material
		parts[k].groupInfo = p->GetGroupInfo();
		parts[k].nMaterial = nMat;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCombinedPart::SetIgnored( int _nIgnoreMark, const CPartFlags &_parts )
{ 
//	pCombiner.Refresh();
	nIgnoreMark = _nIgnoreMark;
	ignoredParts = _parts;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVolumeNode
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVolumeNode::IsEmpty()
{
	EraseInvalidRefs( &particles );
	return staticParts.IsEmpty() && dynamicParts.IsEmpty() && particles.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPolyline
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPolyline::Render( NGfx::CRenderContext *pRC )
{
	pGeometry.Refresh();
	pRC->DrawLineStrip( pGeometry->GetValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPolyline::operator&( CStructureSaver &f )
{
	f.Add( 1, &pGeometry );
	f.Add( 2, &color );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFakeParticleLMTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FAKE_LM_SIZEX = 2;
const int N_FAKE_LM_SIZEY = 1;
void CFakeParticleLMTexture::Recalc()
{
	pValue = NGfx::MakeTexture( N_FAKE_LM_SIZEX, N_FAKE_LM_SIZEY, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
	dwNormalColor = NGfx::GetDWORDColor( CVec4( 0.25f, 0.25f, 0.25f, 1 ) );
	dwParticleColor = NGfx::GetDWORDColor( CVec4( ( vAmbient + pColor->GetValue() ), 1 ) );
	lock[0][0].color = dwNormalColor;
	lock[0][1].color = dwParticleColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGScene
////////////////////////////////////////////////////////////////////////////////////////////////////
CGScene::CGScene() : holdMask(0,0), nFrameCounter(100), lastMask(0,0), bWaitForLoad( true )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CGScene::CGScene( int ) : holdMask(0,0), nFrameCounter(100), lastMask(0,0)
{
	dynamicLightCache.SetTracker( trackers.pSolidTracker );
	pVolume = new CVolumeNode;
	pVolume->SetSize( CVec3( -128, -128, -128 ), 1024 );
#ifdef _DEBUG
	renderMode = SRM_FASTEST;
#else
	renderMode = SRM_BEST;
#endif
	pCamera = new CCVec3;
	nSlowVolumeWalk = 30;
	CVec3 vDefaultAmbient( 0.25f, 0.25f, 0.25f );
	pAmbient = new CCVec3( vDefaultAmbient );
	pTopAmbient = new CCVec3( vDefaultAmbient );
	pBottomAmbient = new CCVec3( vDefaultAmbient );

	//pAmbient->AddLight( new CAmbientLight( pAmbientColor, 0 ) );
	//AddLightGroup( pAmbient );
	nCurrentIgnoreMark = 1;
	pIgnoreStaticTrack = trackers.pTracker;
	lightGroups.push_back( 0 );
	pTransparentMaterial = CreateMaterial( CVec3(1,1,1), new CGetTranspCache, 0, 0, CVec3(0,0,0), 0, 0, 0, 0, 0, MA_OPAQUE|MA_ALPHA_TEST );
	pFakeParticleLM = new CFakeParticleLMTexture;
	pFakeParticleLM->SetAmbient( pAmbient->GetValue() );
	pFakeParticleLM->SetColor( new CCVec3( CVec3(0,0,0) ) );
	bLightStateCalced = false;
	pDecalsManager = new CDecalsManager( this );
	bWaitForLoad = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLightmapTracker* CGScene::GetLMTracker()
{
	if ( !pLMTracker )
		pLMTracker = new CLightmapTracker;
	return pLMTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLightGroup* CGScene::CreateLightGroup()
{
	int n;
	if ( !freeLightGroups.empty() )
	{
		n = freeLightGroups.back();
		freeLightGroups.resize( freeLightGroups.size() - 1 );
	}
	else
	{
		n = lightGroups.size();
		lightGroups.resize( n + 1 );
	}
	CLightGroup *pRes = new CLightGroup( this, n );
	lightGroups[n] = pRes;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// only positive collisions qualify, closest is searched for
static bool Collide( const SRayInfo &r, vector<CVec3> points, const vector<STriangle> &tris, float *pfT, CVec3 *pNormal )
{
	const CVec3 &vRayOrigin = r.vOrigin;
	const CVec3 &vRayDir = r.vDir;
	NCollider::SSegment ray( vRayOrigin, vRayOrigin + vRayDir );
	bool bRes = false;
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &t = tris[k];
		NCollider::SSegment seg1( points[t.i3], points[t.i1] );//points[nPrev], points[ indices[i] ] );
		if ( NCollider::SegmentDotProduct( ray, seg1 ) < 0 )
			continue;
		NCollider::SSegment seg2( points[t.i1], points[t.i2] );//points[nPrev], points[ indices[i] ] );
		if ( NCollider::SegmentDotProduct( ray, seg2 ) < 0 )
			continue;
		NCollider::SSegment seg3( points[t.i2], points[t.i3] );//points[nPrev], points[ indices[i] ] );
		if ( NCollider::SegmentDotProduct( ray, seg3 ) < 0 )
			continue;
		// collides - calc exact place
		CVec3 vNormal;
		vNormal = ( points[t.i3] - points[t.i2] ) ^ ( points[t.i1] - points[t.i2] );
		if ( fabs2( vNormal ) == 0 )
		{
			ASSERT(0);
			continue;
		}
		Normalize( &vNormal );

		float fT = ( ( points[ t.i1 ] - vRayOrigin ) * vNormal ) / ( vRayDir * vNormal );
		if ( fT > 0 && fT < *pfT )
		{
			*pfT = fT;
			//trans.backward.RotateVectorTransposed( pNormal, vNormal );
			*pNormal = vNormal;
			//Normalize( pNormal );
			bRes = true;
		}
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool DoesIntersect( const SSphere &s, const SRayInfo &r )
{
	CVec3 vToCenter( s.ptCenter - r.vOrigin );
	float fDist = r.vDirOrt * vToCenter;
	float fRo = ( vToCenter * vToCenter ) - sqr( fDist );
	if ( fRo > sqr( s.fRadius ) )
		return false;
	float fSide = sqrt( sqr( s.fRadius ) - fRo );
	if ( fDist - fSide > r.fLength )
		return false;
	if ( fDist + fSide < 0 )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool TestParts( const list<CPtr<CCombinedPart> > &elements, const SGroupSelect &mask, const SRayInfo &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, CObjectBase **ppObject )
{
	bool bRes = false;
	for ( list<CPtr<CCombinedPart> >::const_iterator i = elements.begin(); i != elements.end(); ++i )
	{
		CCombinedPart *pCPart = *i;
		IVBCombiner *pVB = pCPart->GetVBCombiner();
		if ( !DoesIntersect( pVB->GetBound().s, r ) )
			continue;
		pCPart->UpdatePartInfo();
		CDGPtr<CFuncBase<vector< CPtr<IPart> > > > pC = pCPart->GetCombiner();
		pC.Refresh();
		const vector< CPtr<IPart> > &parts = pC->GetValue();
		const vector<SSphere> &partBVs = pVB->GetBounds();
		const vector<CCombinedPart::SPartInfo> &partInfos = pCPart->GetPartsInfo();
		for ( int k = 0; k < parts.size(); ++k )
		{
			const CCombinedPart::SPartInfo &pi = partInfos[k];
			if ( !pi.groupInfo.IsMaskMatch( mask ) )
				continue;
			if ( ( mask.nMaskEvery & N_MASK_OCCLUDER ) == 0 && pCPart->GetMaterial( pi.nMaterial )->GetType() == IMaterial::MT_OCCLUDER )
				continue;
			if ( !DoesIntersect( partBVs[k], r ) )
				continue;
			IPart *pPart = parts[k];
			vector<CVec3> points;
			vector<STriangle> tris;
			TransformPart( pPart, &points, &tris );
			if ( Collide( r, points, tris, pfT, pNormal ) )
			{
				if ( ppObject )
					*ppObject = pPart;
				*pColor = pCPart->GetMaterial( pi.nMaterial )->GetAverageColor();
				bRes = true;
			}
		}
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGScene::TraceParts( ERLRequest req, const SGroupSelect &mask, CVolumeNode *pNode, const SRayInfo &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, CObjectBase **ppObject )
{
	if ( !IsValid( pNode ) )
		return false;
	// could be optimized by rejecting volume node by distance
	SSphere nodeBound;
	pNode->GetBound( &nodeBound );
	if ( !DoesIntersect( nodeBound, r ) )
		return false;
	// CRAP - account lit particles in trace
	bool bRes = false;
	if ( req & RN_STATIC )
		bRes |= TestParts( pNode->staticParts.elements, mask, r, pfT, pNormal, pColor, ppObject );
	if ( req & RN_DYNAMIC )
		bRes |= TestParts( pNode->dynamicParts.elements, mask, r, pfT, pNormal, pColor, ppObject );
	for ( int k = 0; k < 8; ++k )
		bRes |= TraceParts( req, mask, pNode->GetNode( k ), r, pfT, pNormal, pColor, ppObject );
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGScene::TraceScene( const SGroupSelect &mask, const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps, CObjectBase **ppObject )
{
	*pfT = 1;
	SRayInfo rayInfo( r );
	int req = 0;
	if ( ps & SPS_STATIC )
		req |= RN_STATIC;
	if ( ps & SPS_DYNAMIC )
		req |= RN_DYNAMIC;
	return TraceParts( (ERLRequest)req, mask, pVolume, rayInfo, pfT, pNormal, pColor, ppObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CheckMaterial( IMaterial *pMat )
{
	ASSERT( IsValid( pMat ) );
	if ( !IsValid( pMat ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CheckOI( CPtrFuncBase<CObjectInfo> *pInfo )//, SBound *pBV )
{
	CDGPtr<CPtrFuncBase<CObjectInfo> > pOI( pInfo );
	pOI.Refresh();
	if ( IsValid( pOI->GetValue() ) && pOI->GetValue()->IsEmpty() )
	{
		pOI.Extract();
		return false;
	}
	//pOI->GetValue()->CalcBound( pBV );
	pOI.Extract();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void PlaceToOctree( CNonePart *pPart, CVolumeNode *pRoot, const CVec3 &vPos, float fR, 
	SStaticTrackers *pTrackers )
{
	CVolumeNode *pNode = pRoot->GetNode( vPos, fR );
	ASSERT( pNode );
	pPart->RefreshObjectInfo();
	AttachPart( pPart, pNode, pTrackers, false, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::WalkNotLoadedObjects()
{
	for ( list< CPtr<CNonePart> >::iterator i = toBeLoaded.begin(); i != toBeLoaded.end(); )
	{
		CNonePart *pPart = *i;
		if ( IsValid(pPart) )
		{
			if ( pPart->HasLoadedObjectInfo() )
			{
				SBound bv;
				pPart->GetObjectInfo()->CalcBound( &bv );
				CVec3 vCenter;
				float fR;
				switch ( pPart->GetTransformType() )
				{
					case TT_SIMPLE:
						{
							const SFBTransform &trans = pPart->GetSimplePos();
							trans.forward.RotateHVector( &vCenter, bv.s.ptCenter );
							fR = sqrt( CalcRadius2( bv, trans.forward ) );
						}
						break;
					case TT_SIMPLE_DISCRETE:
						{
							SFBTransform fbTrans;
							pPart->GetDiscretePos().MakeMatrix( &fbTrans );
							fbTrans.forward.RotateHVector( &vCenter, bv.s.ptCenter );
							fR = sqrt( CalcRadius2( bv, fbTrans.forward ) );
						}
						break;
					case TT_NONE:
						vCenter = bv.s.ptCenter;
						fR = bv.s.fRadius;
						break;
					default:
						ASSERT(0);
						break;
				}
				PlaceToOctree( pPart, pVolume, vCenter, fR, &trackers );
				pDecalsManager->OnCreate( pPart );
				i = toBeLoaded.erase( i );
			}
			else
				++i;
		}
		else
			i = toBeLoaded.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	if ( !CheckOI( pInfo ) )
		return 0;
	CNonePart *pRes = new CNonePart( pInfo, pMat, _ginfo );
	toBeLoaded.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	const SFBTransform &trans, const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	if ( !CheckOI( pInfo ) )
		return 0;
	CSimplePart *pRes = new CSimplePart( pInfo, pMat, _ginfo, trans );
	toBeLoaded.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	const SDiscretePos &trans, const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	if ( !CheckOI( pInfo ) )
		return 0;
	CDiscretePart *pRes = new CDiscretePart( pInfo, pMat, _ginfo, trans );
	toBeLoaded.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	CFuncBase<SFBTransform> *pPlacement, const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	CDynamicPart *pRes = new CDynamicPart( pInfo, pPlacement, pMat, _ginfo );
	movingParts.push_back( pRes );
	pDecalsManager->OnCreate( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	CFuncBase<vector<SHMatrix> > *pPlacement, CFuncBase<vector<NGfx::SCompactTransformer> > *_pMMXAnim, 
	const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	CAnimatedPart *pRes = new CAnimatedPart( pInfo, pPlacement, _pMMXAnim, pMat, _ginfo );
	animatedParts.push_back( pRes );
	pDecalsManager->OnCreate( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateDynamicGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
	CFuncBase<SBound> *pBound, const SFullGroupInfo &_ginfo )
{
	CPtr< CPtrFuncBase<CObjectInfo> > pInfoHolder = pInfo;
	CPtr<IMaterial> pMaterialHolder = pMat;
	//
	if ( !CheckMaterial( pMat ) )
		return 0;
	CDynamicGeometryPart *pRes = new CDynamicGeometryPart( pInfo, pBound, pMat, _ginfo );
	dynamicFrags.push_back( pRes );
	pDecalsManager->OnCreate( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateParticles( CPtrFuncBase<CParticleEffect> *pInfo, 
	CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SGroupInfo &_ginfo, int nPFlags )
{
	CParticles* pRes = new CParticles( pInfo, pPlacement, bound, _ginfo, nPFlags );
	particles.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPolyline* CGScene::CreatePolyline( CPtrFuncBase<NGfx::CGeometry> *pGeometry, const CVec3 &color )
{
	CPolyline *pR = new CPolyline;
	pR->pGeometry = pGeometry;
	pR->color = color;
	lines.push_back( pR );
	return pR;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateSelection( CObjectBase *pRenderNode, const CVec4 &vColor )
{
	CPtr<CSelection> pSelection = new CSelection;
	if ( pSelection->Initialize( pRenderNode, vColor ) )
	{
		selections.push_back( pSelection );
		return pSelection;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreatePostProcessor( CObjectBase *pRenderNode, IPostProcess *pProcessor )
{
	CPtr<CPostProcessBinder> p = new CPostProcessBinder;
	if ( p->Initialize( pRenderNode, pProcessor ) )
	{
		postprocessors.push_back( p );
		return p;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SetAmbient( const CVec3 &_vBottomAmbientColor, const CVec3 &_vTopAmbientColor )
{
	pBottomAmbient->Set( _vBottomAmbientColor );
	pTopAmbient->Set( _vTopAmbientColor );
	CVec3 vAmbientColor = ( _vTopAmbientColor + _vBottomAmbientColor ) * 0.5f;
	pAmbient->Set( vAmbientColor );
	pFakeParticleLM->SetAmbient( vAmbientColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSortLightGroups
{
	bool operator()( const ILight *p1, const ILight *p2 ) const 
	{ 
		return p1->GetPriority() < p2->GetPriority();
	}
};
void CGScene::AddLight( ILight *pGroup )
{
	lights.push_back( pGroup );
	lights.sort( SSortLightGroups() );
	bLightStateCalced = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::AddPointLight( const CVec3 &_vColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly )
{
	if ( fR <= 0 )
		return 0;
	ILight *pRes = new CPointLight( _vColor, ptOrigin, fR, trackers.pSolidTracker, bLightmapOnly );
	AddLight( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::AddPointLight( CPtrFuncBase<CAnimLight> *pLight )
{
	ILight *pRes = new CDynamicPointLight( pLight );
	AddLight( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::AddSpotLight( CFuncBase<CVec3> *pColor, const CVec3 &ptOrigin, const CVec3 &ptDir, 
	float fFOV, float fRadius, CPtrFuncBase<NGfx::CTexture> *pMask, bool bLightmapOnly )
{
	CPtr<CFuncBase<CVec3> > pHold(pColor);
	CPtr<CPtrFuncBase<NGfx::CTexture> > pHold1(pMask);
	if ( fRadius <= 0 )
		return 0;
	return 0;
/*
	CSpotLight *pLight = new CSpotLight( pColor, ptOrigin, ptDir, fFOV, fRadius, pMask, nTargetGroupID );
	pRes->AddLight( pLight );
	AddLight( pRes );
	return pRes;*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::AddDirectionalLight( CFuncBase<CVec3> *pColor, CFuncBase<CVec3> *pGlossColor, 
	const CVec3 &vShadowColor, const CVec3 &ptLight, 
	const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly, float fBlurShift ) 
{
	pFakeParticleLM->SetColor( pColor );
	ILight *pRes = new CDirectionalLight( pColor, pGlossColor, vShadowColor,
		ptLight, ptOrigin, ptSize, fMaxHeight, trackers.pSolidTracker, pAmbient, bLightmapOnly, fBlurShift, this,
		pTopAmbient, pBottomAmbient );
	AddLight( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::RecalcCullingInfo()
{
	UpdateSet( &animatedParts, pVolume.GetPtr(), &trackers );
	UpdateSet( &movingParts, pVolume.GetPtr(), &trackers );
	UpdateSet( &dynamicFrags, pVolume.GetPtr(), &trackers );
	//UpdateSet( &litParticles, pVolume.GetPtr() );
	UpdateSet( &particles, pVolume.GetPtr() );
	if ( --nSlowVolumeWalk < 0 )
	{
		nSlowVolumeWalk = 30;
		pVolume->Walk();
		if ( IsValid(pDecalsManager) )
			pDecalsManager->Walk();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SelectParts( CPartFlags *pRes, CTransformStack *pTS, IVBCombiner *pVB, 
	CCombinedPart *p, const SGroupSelect &mask )
{
	const vector<CCombinedPart::SPartInfo> &partInfos = p->GetPartsInfo();
	int nParts = partInfos.size();
	pRes->Clear();
	if ( pTS->IsFullGet() )
	{
		for ( int i = 0; i < nParts; ++i )
		{
			if ( !partInfos[ i ].groupInfo.IsMaskMatch( mask ) )
				continue;
			pRes->Set( i );
		}
	}
	else
	{
		const vector<SSphere> &partBVs = pVB->GetBounds();
		for ( int i = 0; i < nParts; ++i )
		{
			if ( !partInfos[ i ].groupInfo.IsMaskMatch( mask ) )
				continue;
			if ( pTS->IsIn( partBVs[i] ) )
				pRes->Set( i );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcCastShadow( CPartFlags *pRes, CCombinedPart *p )
{
	pRes->Clear();
	const vector<CCombinedPart::SPartInfo> &partsInfo = p->GetPartsInfo();
	for ( int k = 0; k < partsInfo.size(); ++k )
	{
		if ( partsInfo[k].groupInfo.nObjectGroup & N_MASK_OPAQUE )
			pRes->Set( k );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddParts( CTransformStack *pTS, list<SRenderPartSet> *pRes, 
	const vector<CObj<CCombinedPart> > &elems, const SGroupSelect &mask )
{
	for ( vector<CObj<CCombinedPart> >::const_iterator i = elems.begin(); i != elems.end(); ++i )
	{
		CCombinedPart *pElement = *i;
		if ( !IsValid( pElement ) )
			continue;
		pElement->UpdatePartInfo();
		IVBCombiner *pVB = pElement->GetVBCombiner();

		if ( !pTS->PushClipHint( pVB->GetBound() ) )
			continue;

		CDGPtr<CPerMaterialCombiner> pCombiner = pElement->GetCombiner();
		pCombiner.Refresh();
		const vector< CPtr<IPart> > &listParts = pCombiner->GetValue();
		SRenderPartSet &res = *pRes->insert( pRes->end(), SRenderPartSet( pElement, &listParts, pElement->GetGeometryInfo() ) );
		SelectParts( &res.parts, pTS, pVB, pElement, mask );
		CalcCastShadow( &res.castShadow, pElement );

		pTS->PopClipHint();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::AddNodeParts( CTransformStack *pTS, list<SRenderPartSet> *pRes, CVolumeNode *pNode, ERLRequest eReq, const SGroupSelect &mask )
{
	if ( pNode == 0 )
		return;

	SSphere sClipTest;
	pNode->GetBound( &sClipTest );
	if ( !pTS->PushClipHint( sClipTest ) )
		return;

	if ( eReq & RN_DYNAMIC )
	{
		AddParts( pTS, pRes, pNode->dynamicParts.normalLM, mask );
		AddParts( pTS, pRes, pNode->dynamicParts.normal, mask );
	}
	if ( eReq & RN_STATIC )
	{
		AddParts( pTS, pRes, pNode->staticParts.normalLM, mask );
		AddParts( pTS, pRes, pNode->staticParts.normal, mask );
	}

	for ( int i = 0; i < 8; ++i )
		AddNodeParts( pTS, pRes, pNode->GetNode(i), eReq, mask );

	pTS->PopClipHint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::MakePartList( CTransformStack *pTS, list<SRenderPartSet> *pRes, ERLRequest req, const SGroupSelect &mask )
{
	AddNodeParts( pTS, pRes, pVolume, req, mask );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::FilterParts( vector<CPartFlags> *pRes, CTransformStack *pTS, 
	CCombinedPart *p, ERLRequest req, int _nIgnoreMark )
{
	pRes->clear();
	const SBound &bv = p->GetVBCombiner()->GetBound();
	if ( !pTS->PushClipHint( bv ) )//IsIn( bv ) )//
		return;

	bool bIsMatchingIgnore = p->GetIgnoreMark() == _nIgnoreMark;
	if ( !bIsMatchingIgnore && pHZBuffer && !pHZBuffer->IsVisible( bv.s, pTS ) )
	{
		pTS->PopClipHint();
		return;
	}

	p->UpdatePartInfo();
	int nOriginalPartsNum = p->GetCombiner()->GetValue().size();

	nTotalParts += nOriginalPartsNum;
	++nTotalElements;

	//	bool bSkipPerPartTests = nOriginalPartsNum == 1;
	vector<CPartFlags> &flags = *pRes;
	flags.resize( p->GetMaterialsNumber() );
	for ( int k = 0; k < flags.size(); ++k )
		flags[k].Clear();

	const vector<CCombinedPart::SPartInfo> &partsInfo = p->GetPartsInfo();
	if ( bIsMatchingIgnore )
	{
		const CPartFlags &ignored = p->GetIgnoredParts();
		for ( int k = 0; k < nOriginalPartsNum; ++k )
		{
			const CCombinedPart::SPartInfo &r = partsInfo[ k ];
			if ( ignored.IsSet( k ) )
				continue;
			flags[ r.nMaterial ].Set( k );
		}
	}
	else
	{
		const vector<SSphere> &bounds = p->GetVBCombiner()->GetBounds();
		for ( int k = 0; k < nOriginalPartsNum; ++k )
		{
			const CCombinedPart::SPartInfo &r = partsInfo[ k ];
			if ( !r.groupInfo.IsMaskMatch( mask ) )
				continue;
			if ( pTS->IsIn( bounds[k] ) )
				flags[ r.nMaterial ].Set( k );
		}
	}
	pTS->PopClipHint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::AddElement( CSceneFragments *pRes, CTransformStack *pTS, CCombinedPart *p, ERLRequest req, 
	int _nIgnoreMark )
{
	vector<CPartFlags> take;
	FilterParts( &take, pTS, p, req, _nIgnoreMark );
	int nGeometry = pRes->AddGeometry( p, p->GetGeometryInfo(), p->GetVBCombiner()->GetBound() );
	for ( int k = 0; k < take.size(); ++k )
		pRes->AddElement( nGeometry, take[ k ], p->GetMaterial( k ), 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::AddTranspElement( CCombinedPart *p, const vector<CPartFlags> &flags )
{
	if ( flags.empty() )
		return;
	SRenderGeometryInfo *pGeom = p->GetGeometryInfo();
	const vector<SSphere> &partBVs = pGeom->pVertices->GetBounds();
	for ( int k = 0; k < flags.size(); ++k )
	{
		IMaterial *pMat = p->GetMaterial( k );
		for ( int i = 0; i < PF_MAX_PARTS_PER_COMBINER; i += 32 )
		{
			if ( flags[k].GetBlock( i / 32 ) )
			{
				for ( int n = i; n < i + 32; ++n )
				{
					if ( flags[k].IsSet( n ) )
						pTransp->AddElement( pGeom, pMat, n, partBVs[n].ptCenter * pTransp->GetDepth() );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::AddDynamicLMElement( CCombinedPart *p, 
	CTransformStack *pTS, ERLRequest req, int _nIgnoreMark )
{
	vector<CPartFlags> take;
	FilterParts( &take, pTS, p, req, _nIgnoreMark );
	if ( take.empty() )
		return;
	CPartFlags allTaken( take[0] );
	for ( int k = 1; k < take.size(); ++k )
		allTaken |= take[k];

	CDGPtr<CFuncBase<vector< CPtr<IPart> > > > pCombiner( p->GetCombiner() );
	pCombiner.Refresh();
	const vector< CPtr<IPart> > &parts = pCombiner->GetValue();
	const vector<CCombinedPart::SPartInfo> &partsInfo = p->GetPartsInfo();
	const vector<SSphere> &partBVs = p->GetGeometryInfo()->pVertices->GetBounds();

	int nGeometry = pList->AddGeometry( p, p->GetGeometryInfo(), p->GetVBCombiner()->GetBound() );
	for ( int i = 0; i < parts.size(); ++i )
	{
		if ( !allTaken.IsSet( i ) )
			continue;
		const CCombinedPart::SPartInfo &partInfo = partsInfo[i];
		int nGroup = partInfo.groupInfo.nLightGroup;
		if ( nGroup != 0 )
		{
			ASSERT( nGroup > 0 && nGroup < groups.size() );
			if ( ((unsigned int) nGroup) >= groups.size() )
			{
				ASSERT( 0 && "unregistered light group encountered" );
				continue;
			}
			SDynamicLightGroup &g = groups[ nGroup ];
			if ( g.pAmbient == 0 )
			{
				g.pAmbient = pList->AllocDynamicAmbient();
				g.bv = partBVs[i];
			}
			else if ( g.bv.fRadius < partBVs[i].fRadius )
				g.bv = partBVs[i];

			CPartFlags f;
			f.Clear();
			f.Set( i );
			pList->AddElement( nGeometry, f, p->GetMaterial( partInfo.nMaterial ), 0, g.pAmbient );
		}
		else
		{
			//ASSERT(0);
			// create special group
			SDynamicLightGroup &g = *groups.insert( groups.end(), SDynamicLightGroup());
			g.pAmbient = pList->AllocDynamicAmbient();
			g.bv = partBVs[i];
			CPartFlags f;
			f.Clear();
			f.Set( i );
			pList->AddElement( nGeometry, f, p->GetMaterial( partInfo.nMaterial ), 0, g.pAmbient );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::AddStaticLMElement( CCombinedPart *p, 
	CTransformStack *pTS, ERLRequest req, int _nIgnoreMark )
{
	vector<CPartFlags> take;
	FilterParts( &take, pTS, p, req, _nIgnoreMark );
	if ( take.empty() )
		return;
	CPartFlags allTaken( take[0] );
	for ( int k = 1; k < take.size(); ++k )
		allTaken |= take[k];

	const SBound &bv = p->GetGeometryInfo()->pVertices->GetBound();
	int nGeometry = pList->AddGeometry( p, p->GetGeometryInfo(), bv );
	for ( int k = 0; k < take.size(); ++k )
		pList->AddElement( nGeometry, take[k], p->GetMaterial( k ), pLMTexture, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::AddMaterialHolder( CTransformStack *pTS, 
	const CVolumeNode::SPerMaterialHolder &h, ERLRequest req, int _nIgnoreMark )
{
	if ( pTransp )
	{
		for ( int i = 0; i < h.transparent.size(); ++i )
		{
			CCombinedPart *p = h.transparent[i];
			vector<CPartFlags> flags;
			FilterParts( &flags, pTS, p, req, _nIgnoreMark );
			AddTranspElement( p, flags );
		}
	}
	if ( req & RN_OCCLUDERS ) 
	{
		for ( int i = 0; i < h.occluder.size(); ++i )
			AddElement( pList, pTS,  h.occluder[i], req, _nIgnoreMark );
	}
	else
	{
		for ( int i = 0; i < h.normal.size(); ++i )
		{
			if ( (req & RN_LIGHTMAPS) == 0 )
				AddElement( pList, pTS, h.normal[i], req, _nIgnoreMark );
			else
				AddDynamicLMElement( h.normal[i], pTS, req, _nIgnoreMark );
		}
		for ( int i = 0; i < h.normalLM.size(); ++i )
		{
			if ( (req & RN_LIGHTMAPS) == 0 )
				AddElement( pList, pTS, h.normalLM[i], req, _nIgnoreMark );
			else
				AddStaticLMElement( h.normalLM[i], pTS, req, _nIgnoreMark );
		}
		if ( pAlienList )
		{
			for ( int i = 0; i < h.alien.size(); ++i )
				AddElement( pAlienList, pTS, h.alien[i], req, _nIgnoreMark );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::SSceneFragmentGroupInfo::CalcLightmaps( CGScene *pScene )
{
	for ( int k = 0; k < groups.size(); ++k )
	{
		const SDynamicLightGroup &g = groups[k];
		if ( g.pAmbient )
			*g.pAmbient = pScene->GetDynamicAmbient( k, g.bv );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLitParticlesAdder : public IReportParticlesGeometry
{
	CSceneFragments *pList;
	CTransformStack *pTS;
	SLitParticlesAdder( CSceneFragments *_pList, CTransformStack *_pTS )
		: pList(_pList), pTS(_pTS) {}
	virtual void AddParticles( IVBCombiner *pVertices, CFuncBase<vector<NGfx::STriangleList> > *pTrilists, 
		int nPart, int nParticles, const SBound &bv )
	{
		if ( nParticles )
		{
			if ( pTS->IsIn( bv ) )
				pList->AddLitParticles( pVertices, pTrilists, nPart, bv );
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::AddNode( CTransformStack *pTS, SSceneFragmentGroupInfo *pFragmentsInfo,
	CVolumeNode *pNode, ERLRequest req, int nIgnoreMark )
{
	if ( pNode == 0 )
		return;
	SSphere stest;
	pNode->GetBound( &stest );
	if ( !pTS->PushClipHint( stest ) )
		return;
	bool bDoLightmpasCalcDynamicAmbient = ( req & RN_LIGHTMAPS ) != 0;
	unsigned short nWasMaskEvery = pFragmentsInfo->mask.nMaskEvery;
	if ( req & (RN_HQ_OCCLUDERS|RN_OCCLUDERS) )
	{
		if ( ( req & RN_HQ_OCCLUDERS) && pNode->nLastFrame != nFrameCounter )
		{
			req = (ERLRequest)( RN_OCCLUDERS|RN_STATIC );
			pFragmentsInfo->mask.nMaskEvery = N_MASK_OCCLUDER;
		}
	}
	else
		pNode->nLastFrame = nFrameCounter;
	if ( req & RN_DYNAMIC )
		pFragmentsInfo->AddMaterialHolder( pTS, pNode->dynamicParts, req, nIgnoreMark );//N_SKIP_IGNORED_TEST );
	if ( req & RN_STATIC )
		pFragmentsInfo->AddMaterialHolder( pTS, pNode->staticParts, req, nIgnoreMark );
	if ( ( req & RN_OCCLUDERS ) == 0 )
	{
		for ( list<CPtr<CParticles> >::iterator i = pNode->particles.begin(); i != pNode->particles.end(); ++i )
		{
			CParticles *pPart =*i;
			if ( !IsValid( pPart ) )
				continue;
			if ( !pPart->GetGroup().IsMaskMatch( pFragmentsInfo->mask ) )
				continue;
			// check type
			if ( pPart->IsDynamic() )
			{
				if ( ( req & RN_DYNAMIC ) == 0 )
					continue;
			}
			else
			{
				if ( ( req & RN_STATIC ) == 0 )
					continue;
			}
			const SBound &bv = pPart->GetBound();
			if ( pTS->IsIn( bv.s ) && ( nIgnoreMark == N_SKIP_IGNORED_TEST || !pHZBuffer || pHZBuffer->IsVisible( bv.s, pTS ) ) )
			{
				CTransparentRenderer *pTransp = pFragmentsInfo->pTransp;
				ASSERT( pTransp );
				if ( pTransp )
				{
					if ( req & RN_DEPTH )
					{
						pFragmentsInfo->pList->SetLitParticlesMaterial( pTransparentMaterial );
						SLitParticlesAdder adder( pFragmentsInfo->pList, pTS );
						pTransp->AddParticles( pPart, true, bv, &adder );
					}
					else
					{
						if ( pPart->IsLit() )
						{
							pFragmentsInfo->pList->SetLitParticlesMaterial( pTransparentMaterial );
							SLitParticlesAdder adder( pFragmentsInfo->pList, pTS );
							pTransp->AddParticles( pPart, true, bv, &adder );
						}
						else
							pTransp->AddParticles( pPart, false, bv, 0 );
					}
				}
			}
		}
	}
	for ( int i = 0; i < 8; ++i )
		AddNode( pTS, pFragmentsInfo, pNode->GetNode(i), req, nIgnoreMark );
	pTS->PopClipHint();
	pFragmentsInfo->mask.nMaskEvery = nWasMaskEvery;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::MakeRenderList( CTransformStack *pTS, SSceneFragmentGroupInfo *pTarget, 
	ERLRequest req, int nIgnoreMark )
{
	AddNode( pTS, pTarget, pVolume, req, nIgnoreMark );
	if ( req & RN_LIGHTMAPS )
		pTarget->CalcLightmaps( this );
	if ( pTarget->pTransp )
		pTarget->pTransp->FinishParticles();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> void PrecacheMaterialsForSet( const T &s )
{
	for ( T::const_iterator i = s.begin(); i != s.end(); ++i )
	{
		if ( IsValid( *i ) )
			(*i)->GetMaterial()->Precache();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::PrecacheMaterials( CVolumeNode *pNode )
{
	if ( !IsValid(pNode) )
		return;
	for ( list<CPtr<CCombinedPart> >::iterator i = pNode->staticParts.elements.begin(); i != pNode->staticParts.elements.end(); ++i )
	{
		CCombinedPart *p = *i;
		for ( int i = 0; i < p->GetMaterialsNumber(); ++i )
			p->GetMaterial( i )->Precache();
	}
	for ( int i = 0; i < 8; ++i )
		PrecacheMaterials( pNode->GetNode(i) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::PrecacheMaterials()
{
	PrecacheMaterials( pVolume );
	PrecacheMaterialsForSet( toBeLoaded );
	PrecacheMaterialsForSet( dynamicFrags );
	PrecacheMaterialsForSet( animatedParts );
	PrecacheMaterialsForSet( movingParts );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::RecalcRenderStats( int nSceneTris, int nParticles, int nLitParticles )
{
	lastFrameStats.nSceneTris += nSceneTris;
	lastFrameStats.nParticles += nParticles;
	lastFrameStats.nLitParticles += nLitParticles;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool operator!=( const SHMatrix &a, const SHMatrix &b )
{
	return memcmp( &a, &b, sizeof(a) ) != 0;
}
void CGScene::UpdateIgnoreMark( IRender *pRender, CTransformStack *pTS, const SGroupSelect &mask )
{
	bool bStaticUpdated = pIgnoreStaticTrack.Refresh();
	if ( bStaticUpdated || pTS->Get().forward != mHoldTransform || holdMask != mask )
	{
		++nCurrentIgnoreMark;  
		nIgnoreListWasCalced = 0;//false;
		pHZBuffer = 0;
	}
	else
	{
		if ( nIgnoreListWasCalced == 2 )
		{
			++nCurrentIgnoreMark;
			CIgnorePartsHash res;
			MakeInvisibleElementsList( pRender, pTS, mask, GetScreenRect(), &res, &pHZBuffer );
			for ( CIgnorePartsHash::iterator i = res.begin(); i != res.end(); ++i )
			{
				CDynamicCast<CCombinedPart> pC( i->first );
				pC->SetIgnored( nCurrentIgnoreMark, i->second );
			}
		}
		++nIgnoreListWasCalced;
	}
	mHoldTransform = pTS->Get().forward;
	holdMask = mask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::TraceDynamicAmbient( SDynamicAmbientInfo *pRes, const SSphere &bv )
{
	ASSERT( pLMTracker );
	GetLMTracker()->GetLightState().TraceDynamicLM( pRes, bv, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::ResetDynamicLightmapsCache()
{
	dynamicLightCache.Reset();
	for ( int k = 0; k < lightGroups.size(); ++k )
	{
		if ( IsValid( lightGroups[k] ) )
			lightGroups[k]->vPrevPos = CVec3(1e30f,1e30f,1e30f);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::CheckDynamicLightmapCache()
{
	if ( dynamicLightCache.CheckStatic() )
		ResetDynamicLightmapsCache();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SDynamicAmbientInfo& CGScene::GetDynamicAmbient( int nGroup, const SSphere &bv )
{
	//ASSERT( nGroup >= 0 && nGroup < lightGroups.size() && ( nGroup == 0 || IsValid( lightGroups[nGroup] ) ) );
	if ( nGroup < 1 || nGroup >= lightGroups.size() || !IsValid( lightGroups[nGroup] ) )
		return dynamicLightCache.Calc( bv, this );
	CLightGroup *p = lightGroups[ nGroup ];
	float fDist = fabs2( p->vPrevPos - bv.ptCenter );
	if ( fDist < 0.5f )
		return p->ambientData;
	p->vPrevPos = bv.ptCenter;
	if ( fDist < 1 )
	{
		SDynamicAmbientInfo lm;
		TraceDynamicAmbient( &lm, bv );
		p->ambientData.Blend( lm, 0.5f );
	}
	else
		TraceDynamicAmbient( &p->ambientData, bv );
	return p->ambientData;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::CalcNewLightState()
{
	SGlobalIlluminationInfo lsGlobal;
	lsGlobal.globalBounds = SSphere( CVec3( 10, 10, 10 ), 30 ); // CRAP, currently is not used
	for ( list< CPtr<ILight> >::iterator i = lights.begin(); i != lights.end(); ++i )
	{
		if ( !IsValid( *i) )
			continue;
		CDynamicCast<CDirectionalLight> pDir(*i);
		if (pDir)
		{
			CDirectionalLight::SRadianceInfo info;
			pDir->GetRadianceInfo( &info, RP_GF3_CL );
			lsGlobal.vAmbient = info.vAmbientColor;
			lsGlobal.vUpDifColor = info.vUpDifColor;
			lsGlobal.parallel.push_back( 
				SGlobalIlluminationInfo::SDirectional( info.vColor, info.vDirection, info.bIsRendered ) 
				);
		}
		else {
			CDynamicCast<CPointLight> pPoint(*i);
			if (pPoint)
			{
				CPointLight::SRadianceInfo info;
				pPoint->GetRadianceInfo(&info, RP_GF3_CL);
				lsGlobal.points.push_back(
					SGlobalIlluminationInfo::SPoint(info.vColor, info.vCenter, info.fRadius, info.bIsRendered)
				);
			}
		}
	}
	GetLMTracker()->SetNewIllumination( lsGlobal );
	ResetDynamicLightmapsCache();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVec4Hash
{
	int operator()( const CVec4 &a ) const { int *p = (int*)&a.x; return p[0] ^ p[1] ^ p[2] ^ p[3]; }
};
void CGScene::DrawSelection( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask )
{
	UpdateSet( &selections, this );
	NGfx::CRenderContext &rc = *pRC;

	rc.SetCulling( NGfx::CULL_CW );
	rc.SetAlphaCombine( NGfx::COMBINE_ZERO_ONE );
	rc.SetColorWrite( NGfx::COLORWRITE_NONE );
	rc.SetDepth( NGfx::DEPTH_NONE );
	rc.SetStencil( NGfx::STENCIL_WRITE, 1 );
	NGfx::SEffGlow sGlow;
	NGfx::SEffConstLight tnlGlow;
	if ( NGfx::IsTnLDevice() )
	{
		tnlGlow.color = CVec4(0,0,0,0);
		rc.SetEffect( &tnlGlow );
	}
	else
	{
		sGlow.fScale = 0;
		sGlow.vColor = CVec4(0,0,0,0);
		rc.SetEffect( &sGlow );
	}
	for ( list< CPtr<CSelection> >::iterator i = selections.begin(); i != selections.end(); ++i )
		(*i)->Render( pTS, pRC, mask, false );
	rc.Flush();

	rc.SetCulling( NGfx::CULL_NONE );
	rc.SetAlphaCombine( NGfx::COMBINE_ADD );
	rc.SetColorWrite( NGfx::COLORWRITE_ALL );
	rc.SetDepth( NGfx::DEPTH_NORMAL );
	rc.SetStencil( NGfx::STENCIL_TESTINCR, 0 );//STENCIL_TESTDECR, 1 );
	typedef unordered_map<CVec4, list< CPtr<CSelection> >, SVec4Hash> CColorHash;
	CColorHash hashSel;
	for ( list< CPtr<CSelection> >::iterator i = selections.begin(); i != selections.end(); ++i )
		hashSel[ (*i)->GetColor() ].push_back( *i );

	for ( CColorHash::iterator i = hashSel.begin(); i != hashSel.end(); ++i )
	{
		if ( NGfx::IsTnLDevice() )
		{
			tnlGlow.color = i->first;
			rc.SetEffect( &tnlGlow );
		}
		else
		{
			sGlow.fScale = F_SELECTION_STEP;
			sGlow.vColor = i->first;
			rc.SetEffect( &sGlow );
		}
		for ( list< CPtr<CSelection> >::iterator k = i->second.begin(); k != i->second.end(); ++k )
			(*k)->Render( pTS, pRC, mask, true );
		rc.Flush();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::DrawPostProcess( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask )
{
	if ( postprocessors.empty() )
		return;
	UpdateSet( &postprocessors, this );
	typedef unordered_map<CPtr<IPostProcess>, vector<IPostProcess::SObject>, SPtrHash> CPostHash;
	CPostHash postHash;
	for ( list< CPtr<CPostProcessBinder> >::iterator i = postprocessors.begin(); i != postprocessors.end(); ++i )
		(*i)->Store( &postHash[ (*i)->GetPostProcessor() ], pTS, mask );
	for ( CPostHash::iterator i = postHash.begin(); i != postHash.end(); ++i )
	{
		if ( i->second.empty() )
			continue;
		i->first->Render( pRC, i->second );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::DrawLines( NGfx::CRenderContext *pRC )
{
	pRC->SetAlphaCombine( NGfx::COMBINE_NONE );
	pRC->SetDepth( NGfx::DEPTH_NORMAL );
	pRC->SetStencil( NGfx::STENCIL_NONE );
	typedef unordered_map<CVec3, list<CPolyline*>, SVec3Hash> CColorHash;
	CColorHash hashSel;
	for ( list< CPtr<CPolyline> >::iterator i = lines.begin(); i != lines.end(); )
	{
		if ( IsValid(*i) )
		{
			hashSel[ (*i)->color ].push_back( *i );
			++i;
		}
		else
			i = lines.erase( i );
	}

	for ( CColorHash::iterator k = hashSel.begin(); k != hashSel.end(); ++k )
	{
		NGfx::SEffConstLight d;
		d.color = k->first;
		pRC->SetEffect( &d );
		const list<CPolyline*> &l = k->second;
		for ( list<CPolyline*>::const_iterator i = l.begin(); i != l.end(); ++i )
			(*i)->Render( pRC );
		pRC->Flush();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::RenderCL( NGfx::CRenderContext *pRC, IRender *pRender, CTransformStack *pTS, 
	CSceneFragments *pGeom, bool bSceneHasChanged )
{
	GetLMTracker()->CatchUp( pRC, pRender, pTS, pGeom, bSceneHasChanged, lastMask );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateStaticDecal( CNonePart *pTarget, CPtrFuncBase<CObjectInfo> *pDecal, IMaterial *pMaterial, const SFullGroupInfo &fg )
{
	switch ( pTarget->GetTransformType() )
	{
		case TT_NONE: 
			return CreateGeometry( pDecal, pMaterial, fg );
		case TT_SIMPLE: 
			return CreateGeometry( pDecal, pMaterial, pTarget->GetSimplePos(), fg );
		case TT_SIMPLE_DISCRETE:
			return CreateGeometry( pDecal, pMaterial, pTarget->GetDiscretePos(), fg );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateDynamicDecal( CNonePart *pTarget, CPtrFuncBase<CObjectInfo> *pDecal, IMaterial *pMaterial, const SFullGroupInfo &fg )
{
	CDynamicCast<CDynamicPart> pDynamic(pTarget);
	if (pDynamic)
		return CreateGeometry( pDecal, pMaterial, pDynamic->GetSimplePosNode(), fg );
	CDynamicCast<CAnimatedPart> pAnimated(pTarget);
	if (pAnimated)
		return CreateGeometry( pDecal, pMaterial, pAnimated->GetAnimationNode(), pAnimated->GetMMXAnimationNode(), fg );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::CreateDecal( CNonePart *pTarget, const vector<CVec3> &srcPositions, 
	const SDecalMappingInfo &_info, IMaterial *pMaterial )
{
	const SFullGroupInfo &fgInfo = pTarget->GetFullGroupInfo();
	SFullGroupInfo fg( fgInfo.groupInfo, 0, 0 );
	if ( fabs2( _info.vNormal ) > 0 )
	{
		float f = _info.fRadius;
		CDynamicCast<CGenericDynamicPart> pGeneralDynamics(pTarget);
		if (pGeneralDynamics) // I like name of this company :)
		{
			IMaterial *pExactDecalMat = pMaterial->GetExactDecal();
			if ( !pExactDecalMat )
				return 0;
			CPtr<CPerPolyDecalGeometry> pDecal = new CPerPolyDecalGeometry ( pTarget, srcPositions, _info.vCenter, -_info.vNormal, CVec2(f,f), _info.fRotation, CVec2(0.5f, 0.5f) );
			return CreateDynamicDecal( pTarget, pDecal, pExactDecalMat, fg );
		}
		else
		{
			if ( fgInfo.groupInfo.nObjectGroup & N_MASK_OPAQUE )
			{
				CPtrFuncBase<CObjectInfo> *pGeom;
				SFBTransform transform;
				SFullGroupInfo fgInfo;
				if ( GetGeometryObjectInfo( pTarget, &pGeom, &transform, &fgInfo ) )
				{
					SDiscretePos trans( new CFBTransform( transform ), CVec3(0,0,0), 0 );
					CDecalGeometry *pDecal = new CDecalGeometry( pGeom, trans, _info.vCenter, -_info.vNormal, CVec2(f,f), _info.fRotation, CVec2(0.5f, 0.5f), -0.4f, F_DEPTH_WINDOW );
					return CreateGeometry( pDecal, pMaterial, fg );
				}
			}
			else
			{
				IMaterial *pExactDecalMat = pMaterial->GetExactDecal();
				if ( !pExactDecalMat )
					return 0;
				// alpha tested geometry
				CPtr<CPerPolyDecalGeometry> pDecal = new CPerPolyDecalGeometry ( pTarget, srcPositions, _info.vCenter, -_info.vNormal, CVec2(f,f), _info.fRotation, CVec2(0.5f, 0.5f) );
				return CreateStaticDecal( pTarget, pDecal, pExactDecalMat, fg );
			}
		}
	}
	else
	{
		CPtr<CExplosionDecalGeometry> pDecal = new CExplosionDecalGeometry ( pTarget, srcPositions, _info.vCenter, _info.fRadius, _info.fRotation );
		CDynamicCast<CGenericDynamicPart> pGeneralDynamics(pTarget);
		if (pGeneralDynamics) // I like name of this company :)
			return CreateDynamicDecal( pTarget, pDecal, pMaterial, fg );
		else
			return CreateStaticDecal( pTarget, pDecal, pMaterial, fg );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::GetPartsList( const SDecalMappingInfo &_info, const CObjectBaseSet &targets, vector<CPtr<CNonePart> > *pRes )
{
	WalkNotLoadedObjects();

	list<SRenderPartSet> res;
	SGroupSelect gs = MakeSelectAll();
	//gs.nMaskEvery |= N_MASK_OPAQUE;

	RecalcCullingInfo();
	if ( fabs2( _info.vNormal ) > 0 )
	{
		CTransformStack ts;
		ts.MakeParallel( 2 * _info.fRadius, 2 * _info.fRadius, -0.5f, 0.5f );
		SHMatrix cam;
		MakeMatrix( &cam, _info.vCenter, _info.vNormal );
		ts.SetCamera( cam );
		MakePartList( &ts, &res, CGScene::RN_ALL, gs );
	}
	else
	{
		CRenderWrapper rw( this );
		GeneratePartList( &rw, _info.vCenter, _info.fRadius, &res, IRender::DT_ALL, gs );
	}

	for ( list<SRenderPartSet>::const_iterator i = res.begin(); i != res.end(); ++i )
	{
		const SRenderPartSet &r = *i;
		for ( int k = 0; k < r.pParts->size(); ++k )
		{
			if ( !r.parts.IsSet( k ) )
				continue;
			CDynamicCast<CNonePart> pPart( (*r.pParts)[k] );
			if ( !pPart )
				continue;
			const SFullGroupInfo &fg = pPart->GetFullGroupInfo();
			CObjectBaseSet::const_iterator i = targets.find( fg.pUser );
			if ( i == targets.end() )
				continue;
			if ( pPart->GetFullGroupInfo().nUserID == -1 )
				continue;
			pRes->push_back( pPart.GetPtr() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDecalTarget* CGScene::CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info )
{
	return pDecalsManager->CreateDecalTarget( targets, _info );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGScene::AddDecal( NGScene::CDecalTarget *pTarget, IMaterial *pMaterial )
{
	return pDecalsManager->CreateDecal( pTarget, pMaterial );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::RefreshParticleLMTarget()
{
	if ( IsValid( particleLM.pParticleLMs ) )
		return;
	particleLM.pParticleLMs = shadowMapsShare.GetParticleLM();
	SHMatrix &m = particleLM.rootTransform;
	NGfx::MakeLMToScreenMatrix( &m, N_DEFAULT_RT_RESOLUTION, N_DEFAULT_RT_RESOLUTION );
	particleLM.vParticleLMSize = CVec2( N_DEFAULT_RT_RESOLUTION, N_DEFAULT_RT_RESOLUTION );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline bool IsAllLoaded( const T &stuff )
{
	for ( T::const_iterator i = stuff.begin(); i != stuff.end(); ++i )
	{
		if ( IsValid(*i) && !(*i)->HasLoadedObjectInfo() )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGScene::Draw( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, const SGroupSelect &_mask, 
	ERenderPath renderPath, EFogMode fogMode, const SFogParams &_fog, EHSRMode hsrMode, ETransparentMode trMode, 
	NGfx::CCubeTexture *pSky )
{
	WalkNotLoadedObjects();
	while ( bWaitForLoad )
	{
		WalkNotLoadedObjects();
		bWaitForLoad = !toBeLoaded.empty();
		bWaitForLoad |= !IsAllLoaded( dynamicFrags );
		bWaitForLoad |= !IsAllLoaded( animatedParts );
		bWaitForLoad |= !IsAllLoaded( movingParts );
		if ( bWaitForLoad )
			Sleep( 0 );
	}

	bool bMaskChanged = lastMask != _mask;
	lastMask = _mask;
	SGroupSelect mask(_mask);
	bool bUseFakeParticleLM = renderPath < RP_GF3_CL;
	if ( !bUseFakeParticleLM )
		RefreshParticleLMTarget();

	CPtr<NGfx::CTexture> pFogLookup;
	SFogParams fog( _fog );
	// recalc camera pos
	{
		CVec4 ptCenter;
		pTS->Get().backward.RotateHVector( &ptCenter, CVec4(0,0,1,0) );
		ptCenter.x /= ptCenter.w; ptCenter.y /= ptCenter.w; ptCenter.z /= ptCenter.w;
		pCamera->Set( CVec3( ptCenter.x, ptCenter.y, ptCenter.z ) );
		fog.fCameraHeight = ptCenter.z;
	}
	//
	MarkNewDGFrame();
	RecalcCullingInfo();
	// update lightmaps
	if ( bMaskChanged )
		ResetDynamicLightmapsCache();
	else
		CheckDynamicLightmapCache();

	CSceneFragments geom, alienGeom;
	CObj<CTransparentRenderer> pTransp;
	pFakeParticleLM.Refresh();
	if ( bUseFakeParticleLM )
	{
		CTPoint<int> ptFakeRegisterSize( N_FAKE_LM_SIZEX, N_FAKE_LM_SIZEY );
		pTransp = new CTransparentRenderer( *pTS, ptFakeRegisterSize, true, 
			pFakeParticleLM->GetParticleColor(), pFakeParticleLM->GetNormalColor() );
	}
	else
		pTransp = new CTransparentRenderer( *pTS, CTPoint<int>(N_DEFAULT_RT_RESOLUTION,N_DEFAULT_RT_RESOLUTION), false,
			pFakeParticleLM->GetParticleColor(), pFakeParticleLM->GetNormalColor() );
	CRenderWrapper renderWrapper( this );
	int nUseIgnoreMark = N_SKIP_IGNORED_TEST;
	if ( hsrMode != HSR_NONE )
	{
		UpdateIgnoreMark( &renderWrapper, pClipTS, mask );
		nUseIgnoreMark = nCurrentIgnoreMark;
	}
	ERLRequest rlReq = RN_ALL;
	switch ( renderPath )
	{
		case RP_TNL:
		case RP_FASTEST:
			break;

		case RP_SHOWOCCLUDERS:
			rlReq = (ERLRequest)( RN_OCCLUDERS|RN_STATIC );//|RN_DYNAMIC );
			mask.nMaskEvery = mask.nMaskAny = N_MASK_OCCLUDER;
			break;

		case RP_UPDATE_CL:
		case RP_SHOWLIGHTMAPPED:
		case RP_GF2:
		case RP_GF2_CL:
			rlReq = (ERLRequest)( ((int)rlReq) | RN_LIGHTMAPS );
			break;

		case RP_GF3_CL:
			rlReq = (ERLRequest)( ((int)rlReq) | RN_LIGHTMAPS | RN_LIT_PARTICLES );	
			break;
	}
	// check lights and update light state if needed
	if ( EraseInvalidRefs( &lights ) )
		bLightStateCalced = false;
	if ( IsUsingCacheLighting( renderPath ) && !bLightStateCalced  )
	{
		CalcNewLightState();
		bLightStateCalced = true;
	}
	// form scene
	SSceneFragmentGroupInfo renderList( 
		mask, pCamera->GetValue(), &geom, pTransp, GetLMTracker(), ++nFrameCounter, 
		&alienGeom, lightGroups.size(), 
		nUseIgnoreMark != N_SKIP_IGNORED_TEST ? pHZBuffer : 0,
		IsUsingCacheLighting( renderPath ) ? NGfx::GetRegisterTexture( N_CL_TARGET_REGISTER ) : 0
		);
	MakeRenderList( pClipTS, &renderList, rlReq, nUseIgnoreMark );

	if ( bWireframe )
		NGfx::SetWireframe( NGfx::WIREFRAME_ON );
	else
		NGfx::SetWireframe( NGfx::WIREFRAME_OFF );

	// render all lights
	if ( trMode != TRM_ONLY )
	{
		SParticleLMRenderTargetInfo useParticleTarget;
		if ( !bUseFakeParticleLM )
			useParticleTarget = particleLM;
		useParticleTarget.vKernelSize = pTransp->GetKernelLightInfo().vLightSize;
		for ( list< CPtr<ILight> >::iterator k = lights.begin(); k != lights.end(); ++k )
		{
			ILight *pLight = *k;
			pLight->Render( pTS, pClipTS, pRC, renderPath, &renderWrapper, geom, useParticleTarget );
		}
		// render fog & reflection
		if ( renderPath != RP_TNL )
		{
			CRenderCmdList finalOps, finalAlienOps, finalPreOps;
			switch ( fogMode )
			{
			case FOG_PERVERTEX:
				AddFinalOps( &finalOps, geom, renderPath, RO_FOG_STATIC, &fog );
				AddFinalOps( &finalAlienOps, alienGeom, renderPath, RO_FOG_STATIC, &fog );
				pFogLookup = GetFogLookupTexture( fog );
				break;
			case FOG_DYNAMIC:
				AddFinalOps( &finalOps, geom, renderPath, RO_FOG_DYNAMIC, &fog );
				AddFinalOps( &finalAlienOps, alienGeom, renderPath, RO_FOG_DYNAMIC, &fog );
				pFogLookup = GetFogLookupTexture( fog );
				break;
			default:
				AddFinalOps( &finalOps, geom, renderPath, RO_NOP, 0.0f );
				AddFinalOps( &finalAlienOps, alienGeom, renderPath, RO_NOP, 0.0f );
				break;
			}
			SLightInfo lightInfo;
			SplitOps( &finalPreOps, &finalAlienOps, 30 );
			Execute( &renderWrapper, pRC, *pTS, finalPreOps, alienGeom, lightInfo );
			Execute( &renderWrapper, pRC, *pTS, finalOps, geom, lightInfo );
			Execute( &renderWrapper, pRC, *pTS, finalAlienOps, alienGeom, lightInfo );
		}
		// render post process
		DrawPostProcess( pTS, pRC, mask );
	}
	//
	NGfx::SetWireframe( NGfx::WIREFRAME_OFF );

	if ( pRC->HasRegisters() )
		pRC->SetRegister( 0 );
	// draw lines
	pRC->SetTransform( pTS->Get() );
	DrawLines( pRC );
	// draw transparent stuff
	if ( trMode != TRM_NONE )
	{
		if ( bUseFakeParticleLM )
			pTransp->Render( pRC, pFakeParticleLM->GetValue(), pFogLookup, pSky );
		else
			pTransp->Render( pRC, particleLM.pParticleLMs, pFogLookup, pSky );
	}
	DrawSelection( pTS, pRC, mask );
	//
	RecalcRenderStats( geom.GetSceneTris(), pTransp->GetTotalParticles(), pTransp->GetLitParticles() );
	pRC->SetStencil( NGfx::STENCIL_NONE );
	if ( bShow2DTextureCache )
		NGfx::ShowTexture( pRC, NGfx::GetTextureCache(), CVec2(1600,1200) );
	if ( bShowTranspTextureCache )
		NGfx::ShowTexture( pRC, NGfx::GetTransparentTextureCache(), CVec2(1600,1200) );
	if ( bShowParticleLMCache )
	{
		if ( bUseFakeParticleLM )
			;//NGfx::ShowTexture( pRC, pFakeParticleLM->GetValue(), CVec2(10, 10) );
		else
			NGfx::ShowTexture( pRC, particleLM.pParticleLMs, CVec2(800,600) );
	}
	switch ( showLinearCache )
	{
		case SLC_NONE: break;
		case SLC_DYNAMIC: NGfx::ShowTexture( pRC, NGfx::GetLinearBufferMRU( NGfx::DYNAMIC ), CVec2(1024, 768) ); break;
		case SLC_STATIC:  NGfx::ShowTexture( pRC, NGfx::GetLinearBufferMRU( NGfx::STATIC ), CVec2(1024, 768) ); break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CGScene::GetScreenRect()
{
	return NGfx::GetScreenRect();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFuncBase<CVec3>* CGScene::GetCamera()
{
	return pCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderWrapper
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderWrapper::FormPartList( CTransformStack *pTS, list<SRenderPartSet> *pRes, EDepthType dt, const SGroupSelect &mask )
{
	switch ( dt )
	{
	case DT_STATIC:
		pScene->MakePartList( pTS, pRes, CGScene::RN_STATIC, mask );
		break;
	case DT_DYNAMIC:
		pScene->MakePartList( pTS, pRes, CGScene::RN_DYNAMIC, mask );
		break;
	case DT_ALL:
		pScene->MakePartList( pTS, pRes, CGScene::RN_ALL, mask );
		break;
	default:
		ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CTransparentRenderer* CreateFakeTranspRender( const CVec3 &vLightDir )
{
	CTPoint<int> ptFakeRegisterSize( N_FAKE_LM_SIZEX, N_FAKE_LM_SIZEY );
	CTransformStack particleTS;
	particleTS.MakeParallel( 1, 1,-1000, 1000 );
	SHMatrix particleCam;
	MakeMatrix( &particleCam, CVec3(0,0,0), vLightDir );
	particleTS.SetCamera( particleCam );
	return new CTransparentRenderer( particleTS, ptFakeRegisterSize, true, 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderWrapper::FormDepthList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, IRender::EDepthType dt )
{
	CObj<CTransparentRenderer> pTransp( CreateFakeTranspRender( vDir ) );
	SGroupSelect mask(0x8000, 0);
	CGScene::SSceneFragmentGroupInfo target( mask, VNULL3, pRes, pTransp );
	switch ( dt )
	{
		case DT_STATIC:
			pScene->MakeRenderList( pTS, &target, (CGScene::ERLRequest)(CGScene::RN_STATIC | CGScene::RN_DEPTH), N_SKIP_IGNORED_TEST );
			break;
		case DT_DYNAMIC:
			pScene->MakeRenderList( pTS, &target, (CGScene::ERLRequest)(CGScene::RN_DYNAMIC | CGScene::RN_DEPTH), N_SKIP_IGNORED_TEST );
			break;
		case DT_ALL:
			pScene->MakeRenderList( pTS, &target, (CGScene::ERLRequest)(CGScene::RN_ALL | CGScene::RN_DEPTH), N_SKIP_IGNORED_TEST );
			break;
		default:
			ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderWrapper::FormDirOccludersList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, const SGroupSelect &gs, bool bFast )
{
	if ( bFast )
	{
		SGroupSelect mask( gs.nMaskAny, N_MASK_OCCLUDER );
		CGScene::SSceneFragmentGroupInfo target( mask, VNULL3, pRes, 0 );
		pScene->MakeRenderList( pTS, &target, (CGScene::ERLRequest)(CGScene::RN_OCCLUDERS|CGScene::RN_STATIC), N_SKIP_IGNORED_TEST );
	}
	else
	{
		CObj<CTransparentRenderer> pTransp( CreateFakeTranspRender( vDir ) );
		SGroupSelect mask( gs.nMaskAny, 0x8000 );
		CGScene::SSceneFragmentGroupInfo target( mask, VNULL3, pRes, pTransp );
		pScene->MakeRenderList( pTS, &target, (CGScene::ERLRequest)(CGScene::RN_HQ_OCCLUDERS | CGScene::RN_STATIC | CGScene::RN_DEPTH), N_SKIP_IGNORED_TEST );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IGScene* CreateScene()
{
	return new CGScene(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Is3DActive()
{
	return NGfx::Is3DActive();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Clear( NGfx::CRenderContext *pRC, const CVec3 &vColor )
{
	NGfx::SPixel8888 clearColor( Float2Int( vColor.x * 255 ), Float2Int( vColor.y * 255 ), Float2Int( vColor.z * 255 ), 0 );
	if ( pRC->HasRegisters() )
	{
		pRC->SetRegister( 0 );
		pRC->ClearBuffers( clearColor.color );
		pRC->SetRegister( 1 );
		pRC->ClearTarget( 0 );
	}
	else
		pRC->ClearBuffers( clearColor.color );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ClearScreen( const CVec3 &vColor )
{
	NGfx::CRenderContext rc;
	NGfx::SPixel8888 clearColor( Float2Int( vColor.x * 255 ), Float2Int( vColor.y * 255 ), Float2Int( vColor.z * 255 ), 0 );
	rc.ClearBuffers( clearColor.color );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CopyRegisterOnScreen( const CTRect<float> &rScreenRect, ERegisterCopyMode mode, int nRegister )
{
	NGfx::CRenderContext rcScreen;
	CTRect<float> regSize;
	NGfx::GetRegisterSize( &regSize );

	CObj<NGfx::I2DEffect> pEffect;
	switch ( mode )
	{
		case RCM_COPY:
			rcScreen.SetAlphaCombine( NGfx::COMBINE_NONE );
			break;
		case RCM_TRANSPARENT:
			rcScreen.SetAlphaCombine( NGfx::COMBINE_ALPHA );
			break;
		case RCM_SHOWALPHA:
			pEffect = new NGfx::CShowAlphaEffect;
			rcScreen.SetAlphaCombine( NGfx::COMBINE_NONE );
			break;
	}
	rcScreen.SetStencil( NGfx::STENCIL_NONE );
	NGfx::CopyTexture( rcScreen, NGfx::GetScreenRect(), rScreenRect, NGfx::GetRegisterTexture(nRegister), regSize, CVec4(1,1,1,1), pEffect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Flip()
{
	NGfx::Flip();

	lastFrameStats.nSceneTris = 0;
	lastFrameStats.nParticles = 0;
	lastFrameStats.nLitParticles = 0;
	static NHPTimer::STime timeFrameStart = 0;
	lastFrameStats.fFrameTime = NHPTimer::GetTimePassed( &timeFrameStart );
	lastFrameStats.bStaticShadowDepthRendered = bStaticShadowDepthRendered;
	bStaticShadowDepthRendered = false;

	//char szBuf[ 1024 ];
	//sprintf( szBuf, "%g parts per element, %d elements\n", ((float)nTotalParts) / nTotalElements, nTotalElements );
	//OutputDebugString( szBuf );
	nTotalParts = 0;
	nTotalElements = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CalcTouchedTextureSize()
{
	return NGfx::CalcTouchedTextureSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetWireframe( bool bWire )
{
	bWireframe = bWire;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetRenderStats( SRenderStats *pStats )
{
	lastFrameStats.nTris = NGfx::renderStats.nTris;
	lastFrameStats.nVertices = NGfx::renderStats.nVertices;
	lastFrameStats.bGeometryThrashing = NGfx::IsGeometryThrashing();
	lastFrameStats.b2DTexturesThrashing = NGfx::Is2DTextureThrashing();
	lastFrameStats.bTransparentThrashing = NGfx::IsTransparentThrashing();
	*pStats = lastFrameStats;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetFrameTime()
{
	return lastFrameStats.fFrameTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands/Vars
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSwitchTexCache( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bShow2DTextureCache = false;
	if ( sValue.GetFloat() != 0 )
		bShow2DTextureCache = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSwitchTranspCache( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bShowTranspTextureCache = false;
	if ( sValue.GetFloat() != 0 )
		bShowTranspTextureCache = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSwitchTranspLMCache( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bShowParticleLMCache = false;
	if ( sValue.GetFloat() != 0 )
		bShowParticleLMCache = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSwitchLinearCache( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	showLinearCache = SLC_NONE;
	if ( sValue.GetFloat() != 0 )
	{
		if ( sValue.GetFloat() != 1 )
			showLinearCache = SLC_STATIC;
		else
			showLinearCache = SLC_DYNAMIC;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(GSceneInternal)
	REGISTER_VAR( "gfx_showcache_2d", VarSwitchTexCache, 0.0f, false )
	REGISTER_VAR( "gfx_showcache_transp", VarSwitchTranspCache, 0.0f, false )
	REGISTER_VAR( "gfx_showcache_transplm", VarSwitchTranspLMCache, 0.0f, false )
	REGISTER_VAR( "gfx_showcache_linear", VarSwitchLinearCache, 0.0f, false )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
// release-new animated-ambient DG node (GSceneInternal.obj, operator& @0x165350). Tracks the scene
// top-ambient colour, overriding it with an animated light's colour while one is alive. DEAD in this
// predecessor render: CGScene::pTopAmbientAnimator is never created here (animated-ambient is a
// release-only feature) -> the node is never instantiated/driven, so NeedUpdate/Recalc are faithful-
// stubbed and the CObj serializes as a null ref. Modeled on the sibling CFuncBase<CVec3> DG nodes
// (CLightCalcer, GBuilding.h). Save-format reconcile only; animation behavior deferred.
class CAmbientAnimator: public CFuncBase<CVec3>
{
	OBJECT_BASIC_METHODS(CAmbientAnimator);
protected:
	virtual bool NeedUpdate() { return false; }
	virtual void Recalc() {}
private:
	ZDATA
	CDGPtr< CFuncBase<CVec3> > pTopAmbient;
	CDGPtr< CPtrFuncBase<CAnimLight> > pAnim;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTopAmbient); f.Add(3,&pAnim); return 0; }
	CAmbientAnimator() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x01123131, CAmbientAnimator )
REGISTER_SAVELOAD_CLASS( 0x0251100b, CGScene )
REGISTER_SAVELOAD_CLASS( 0x03111000, CPolyline )
REGISTER_SAVELOAD_CLASS( 0x00661130, CVolumeNode )
REGISTER_SAVELOAD_CLASS( 0x00861121, CSelection )
REGISTER_SAVELOAD_CLASS( 0x11791170, CCombinedPart )
REGISTER_SAVELOAD_CLASS( 0x27041130, CParticles )
REGISTER_SAVELOAD_CLASS( 0x01362120, CLightGroup )
REGISTER_SAVELOAD_CLASS( 0x02662160, CNonePart )
REGISTER_SAVELOAD_CLASS( 0x02662161, CSimplePart )
REGISTER_SAVELOAD_CLASS( 0x02662162, CDiscretePart )
REGISTER_SAVELOAD_CLASS( 0x02662163, CDynamicPart )
REGISTER_SAVELOAD_CLASS( 0x02662164, CAnimatedPart )
REGISTER_SAVELOAD_CLASS( 0x02662165, CDynamicGeometryPart )
REGISTER_SAVELOAD_CLASS( 0x00372110, CGetTranspCache )
REGISTER_SAVELOAD_CLASS( 0x00372140, CFakeParticleLMTexture )
REGISTER_SAVELOAD_CLASS( 0x020a2190, CPostProcessBinder )
