#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GScene.h"
#include "GView.h"
#include "GMaterial.h"
#include "GFileSkin.h"
#include "GMesh.h"
#include "GBuilding.h"
#include "GObjectInfo.h"
#include "GTexture.h"
#include "GMemFormat.h"
//#include "MemObject.h"
#include "GMemBuilder.h"
#include "GBind.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataLight.h"
#include "GAnimFormat.h"
#include "TerrainInfo.h"
#include "grid.h"
#include "GTerrain.h"
#include "GTerrainTexture.h"
#include "GParticleFormat.h"
#include "GParticles.h"
#include "GGrass.h"
#include "2DScene.h"
#include "Transform.h"
#include "MapBuildingInfo.h"
#include "GfxRender.h" // for GetHardwareOnly()
#include "LSHead.h"
#include "GAnimLight.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "GPointLightGlow.h"
#include "GAnimation.h"
#include "GInit.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
	externA5 CBasicShare<int, CFileSkeleton> shareSkeletons;
}
namespace NLSHead
{
	externA5 CBasicShare<int, CHeadMeshLoader> shareHeads;
}
namespace NAI
{
	externA5 CBasicShare<int, NGScene::CFileAIBind> shareAIBinds;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
static bool bShowFPSStats = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
// share objects
CBasicShare<STextureKey, CFileTexture, STextureKeyHash> shareTextures(103);
//CBasicShare<SPartKey, CFileTriList, SPartHash> shareTrilists(102);
static CBasicShare<int, CFileBind> shareBinds(117);
static CBasicShare<int, CParticlesLoader> shareParticles(119);
//static CBasicShare<SPartKey, CFileSkinScene, SPartHash> shareSkins(105);
static CBasicShare<SPartKey, CObjectInfoLoader, SPartHash> shareObjInfo(121);
//static CBasicShare<SVectorKey, CFileTextureComplex, SVectorKeyHash> shareComplexTextures(130);
static CBasicShare<int, CFileCubeTexture> shareCubeTextures(131);
static CBasicShare<int, CLightLoader> shareAnimLights(139);
static CBasicShare<int, CAIGeometryConverter > shareAIObjInfoConverters(142);
static CBasicShare<int, CAISkinObjectConverter > shareAISkinConverters(143);
#ifdef _DEBUG
static EFogMode defaultFogMode = FOG_NONE;
static EHSRMode defaultHSRMode = HSR_FAST;
static ESceneRenderMode defaultRenderMode = SRM_FASTEST;
#else
static EFogMode defaultFogMode = FOG_DYNAMIC;
static EHSRMode defaultHSRMode = HSR_FAST;
static ESceneRenderMode defaultRenderMode = SRM_BEST;
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderNode;
class CGameView: public IGameView
{
	OBJECT_BASIC_METHODS(CGameView);
	ZDATA
	CObj<IGScene> pScene;
	list<CPtr<CRenderNode> > nodes; // if object is just created and no refs are stored it should not become leak
	list<CPtr<CBuilding> > buildings;
	list<CPtr<CGrassTracker> > grassTrackers;
	CObj<CObjectBase> pAmbientDirectional;
	CObj<CMaterialShare> pMaterials;
	CColorMaterialShare colorMaterials;
	CTransparentMaterialShare transparentMaterials;
	int nCutFloor;
	SFogParams fog;
	EFogMode fogMode;
	bool bShowParticles;
	EHSRMode hsrMode;
	CVec3 vCurrentFogColor;
	CVec3 vDefaultClearColor;
	SHMatrix mPrevView;
	int nSameViewCount;
	bool bHasTerrain;
	ETransparentMode trMode;
	CObj<CFuncBase<SFBTransform> > pIdentityTransform;
	CPtr<NDb::CAmbientLightReal> pPrevLight;
	CObj<IMaterial> pAlienMaterial;
	bool bAlienStyle;
	float fFogBaseHeight;
	ESceneRenderMode renderMode;
	bool bForceFastest;
	// release save-format: drops buildings(4)/fogMode(12)/bShowParticles(13)/hsrMode(14) from serialize
	// (members KEPT for behavior), shifts 15-27 -> 14-26, then serializes the existing prevLightMode(27)
	// + 2 new members precacheObjects(28)/bFastMode(29). Renumber independently decode+disasm-verified.
	list<CPtr<CRenderNode> > precacheObjects;
	bool bFastMode = false;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pScene); f.Add(3,&nodes); f.Add(5,&grassTrackers); f.Add(6,&pAmbientDirectional); f.Add(7,&pMaterials); f.Add(8,&colorMaterials); f.Add(9,&transparentMaterials); f.Add(10,&nCutFloor); f.Add(11,&fog); f.Add(14,&vCurrentFogColor); f.Add(15,&vDefaultClearColor); f.Add(16,&mPrevView); f.Add(17,&nSameViewCount); f.Add(18,&bHasTerrain); f.Add(19,&trMode); f.Add(20,&pIdentityTransform); f.Add(21,&pPrevLight); f.Add(22,&pAlienMaterial); f.Add(23,&bAlienStyle); f.Add(24,&fFogBaseHeight); f.Add(25,&renderMode); f.Add(26,&bForceFastest); f.Add(27,&prevLightMode); f.Add(28,&precacheObjects); f.Add(29,&bFastMode); return 0; }
	bool bWaitLoading;
	ELightMode prevLightMode;

	void AddModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
		NDb::CMaterial *pMaterial, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_r );
	void AddModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
		NDb::CMaterial *pMaterial, const SFBTransform &placement, const SFullRoomInfo &_r );
	void AddSkinModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
		NDb::CMaterial *pMaterial, CFuncBase<SSkeletonMatrices> *pAnimation, 
		CFuncBase<vector<NGfx::SCompactTransformer> > *pMMXAnimation, const SFullRoomInfo &_r );
	CRenderNode* NewRenderNode();

	CObjectBase* CreateParticles( int nPFlags, bool bCastShadows, bool bTreeCrown, CPtrFuncBase<CParticleEffect> *pEffect,
		CFuncBase<SFBTransform> *pPlacement,
		const SBound &bound, const SRoomInfo &_g );
	CObjectBase* TrueCreateParticles( bool _bIsDynamic, NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, const SRoomInfo &_g, NAnimation::CSkeletonAnimator *pScAnim = 0 );
	void Draw( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath rp );
	void MakeTargetRect( CTRect<float> *pRes, const SDrawInfo &drawInfo );
	IMaterial* CreateMaterialShared( NDb::CMaterial *p );
	ERenderPath GetRenderPath() const;
public:
	CGameView();
	
	virtual IGScene* GetGScene() { return pScene; }
	virtual CLightGroup* CreateLightGroup();
	virtual CObjectBase* CreateMesh( NDb::CModel *pModel, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_g );
	virtual CObjectBase* CreateMesh( CMemObject *pModel, const CVec4 &color, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_g );
	virtual CObjectBase* CreateMesh( NDb::CModel *pModel, const SFBTransform &placement, const SFullRoomInfo &_g );
	virtual CObjectBase* CreateMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &placement, const SFullRoomInfo &_g );
	virtual CObjectBase* CreateSkin( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SFullRoomInfo &_g );
	virtual CObjectBase* CreateOccluder( NDb::CAIGeometry *pGeom, const SFBTransform &placement, const SRoomInfo &_g );
	virtual CObjectBase* CreateOccluder( NDb::CAIGeometry *pGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SRoomInfo &_g );
	virtual CObjectBase* CreateTerrainRegion( CFuncBase<STerrainInfo> *pInfo, CVersioningBase *pUpdateRegion, const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, CGrassTracker *pGrass, const SFullRoomInfo &_g = SFullRoomInfo() );
	virtual CObjectBase* CreateTerrainWall( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, const SFullRoomInfo &_g = SFullRoomInfo() );
	virtual CObjectBase* CreateGrassSector( CGrassAnimator *pEffect, NDb::CTexture *pTexture, CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SRoomInfo &_g = SRoomInfo() );
	virtual CSelectionNode* CreateSelection( const vector<CObjectBase*> &target, const CVec4 &vColor );
	virtual CObjectBase* CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, const SRoomInfo &_g, NAnimation::CSkeletonAnimator *pScAnim );
	virtual CObjectBase* CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, const SFBTransform &place, const SRoomInfo &_g );
	virtual CBuilding* CreateBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI );
	virtual CPolyline* CreatePolyline( const vector<CVec3> &points, const CVec3 &color );
	virtual CObjectBase* CreateExplosion( CFuncBase<STime> *pTime, NDb::CEffect *pEffect, CFuncBase<CExplosionInfo> *pExplosion, const CVec3 &pos, const SRoomInfo &_g );
	virtual CObjectBase* CreateLSHead( NDb::CComplexHead *pHead, CFuncBase<NLSHead::SHeadFrame> *pAnimation, CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, bool bInterface, const SRandomSeed &headSeed, bool bHasCap, const SRoomInfo &_g, CPtrFuncBase<NGfx::CTexture> *pFaceTexture, const NLSHead::CHeadInfo *pHeadInfo );
	virtual CObjectBase* Precache( NDb::CModel *pModel );
	virtual void StartAlienStyle();
	virtual void FinishAlienStyle();
	virtual void Draw( const SDrawInfo &drawInfo );
	virtual CVec2 GetScreenRect();
	virtual int  GetCutFloor();
	virtual void SetCutFloor( int nFloor );
	virtual bool GetParticleShow() { return bShowParticles; }
	virtual void SetParticleShow( bool bNewState ) { bShowParticles = bNewState; }
	virtual void SetAmbient( const CVec3 &vBottomAmbientColor, const CVec3 &vTopAmbientColor );
	virtual CObjectBase* AddDirectionalLight( const CVec3 &ptColor, const CVec3 &ptLight, const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly );
	virtual CObjectBase* AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly );
	virtual CObjectBase* AddFlare( CFuncBase<CVec3> *pOrigin, CFuncBase<STime> *pTime, int nFloor, float fFlareRadius, NDb::CTexture *pFlareTexture, float fOnTime, float fOffTime );
	virtual CObjectBase* AddPostFilter( const vector<CObjectBase*> &target, IPostProcess *pEffect );
	virtual CObjectBase* AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, NDb::CTexture *pMask, bool bLightmapOnly );
	virtual CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info );
	virtual CObjectBase* AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial );
	virtual void SetAmbient( NDb::CAmbientLightReal *pLight, ELightMode lm );
	virtual ESceneRenderMode GetRenderMode() const;
	virtual void SetRenderMode( ESceneRenderMode mode );
	virtual EFogMode GetFogMode() const;
	virtual void SetFogMode( EFogMode mode );
	virtual bool TraceScene( const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps, CObjectBase **ppObject );
	virtual void SetFogBaseHeight( float f );
	void SetHSRMode( EHSRMode m ) { hsrMode = m; }
	EHSRMode GetHSRMode() const { return hsrMode; }
	void SetTransparentMode( ETransparentMode m ) { trMode = m; }
	ETransparentMode GetTransparentMode() const { return trMode; }
	void SetForceFastest( bool b ) { bForceFastest = b; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectSet: public CObjectBase
{
public:
	vector< CObj<CObjectBase> > parts;
	//
	void AddPart( CObjectBase *pPart ) { parts.push_back( pPart ); }
	int operator&( CStructureSaver &f ) { f.Add( 1, &parts );	return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderNode: public CObjectSet
{
	OBJECT_BASIC_METHODS(CRenderNode);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelectionNode: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSelectionNode);
private:
	vector< CObj<CObjectBase> > selections;

public:
	CSelectionNode() {}
	CSelectionNode( IGScene* pScene, const vector<CObjectBase*> &target, const CVec4 &vColor );

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static SGroupInfo GetGroupInfo( const SRoomInfo &r, bool bCastShadows = true, bool bParticles = false )
{
	if ( IsValid( r.pGroup ) )
		return SGroupInfo( GetGroup( r.pGroup ), GetFloorBit( r.nFloor, bCastShadows, bParticles ) );
	return SGroupInfo( 0, GetFloorBit( r.nFloor, bCastShadows, bParticles ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static SFullGroupInfo GetGroupInfo( const SFullRoomInfo &r, bool bCastShadows = true, bool bParticles = false )
{
	return SFullGroupInfo( GetGroupInfo( r.room, bCastShadows, bParticles ), r.pUser, r.nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// IGameView
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
static void SetShadowsMode( IGameView *p, T take )
{
	ESceneRenderMode r = p->GetRenderMode();
	for(;;)
	{
		r = (ESceneRenderMode) (r + 1);
		if ( r == SRM_LAST ) 
			r = SRM_FASTEST;
		if ( take( r ) )
			break;
	}
	p->SetRenderMode( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsNormalViewMode( ESceneRenderMode m )
{
	return m == SRM_FASTEST || m == SRM_BEST;
}
inline bool IsLightmapViewMode( ESceneRenderMode m )
{
	return !IsNormalViewMode( m );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetNextShadowsMode() 
{
	SetShadowsMode( this, IsNormalViewMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetNextLightmapViewMode()
{
	SetShadowsMode( this, IsLightmapViewMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetNextTranspRenderMode()
{
	ETransparentMode tr = GetTransparentMode();
	tr = (ETransparentMode)( tr + 1 );
	if ( tr == TRM_LAST )
		tr = TRM_NONE;
	SetTransparentMode( tr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetNextFogMode()
{
	EFogMode r = (EFogMode) (GetFogMode() + 1);
	if ( r == FOG_LAST ) 
		r = FOG_NONE;
	SetFogMode( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetFastMode()
{
	SetFogMode( FOG_NONE );
	SetRenderMode( SRM_FASTEST );
	SetHSRMode( HSR_NONE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void IGameView::SetNextHSRMode()
{
	EHSRMode r = (EHSRMode) (GetHSRMode() + 1);
	if ( r == HSR_LAST ) 
		r = HSR_NONE;
	SetHSRMode( r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSelectionNode
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetParts( vector<CObjectBase*> *pRes, const vector<CObjectBase*> &target )
{
	for ( int k = 0; k < target.size(); ++k )
	{
		CDynamicCast<CRenderNode> pNode( target[k] );
		if ( !IsValid( pNode ) )
			continue;

		for( int nTemp = 0; nTemp < pNode->parts.size(); nTemp++ )
			pRes->push_back( pNode->parts[nTemp] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelectionNode::CSelectionNode( IGScene* pScene, const vector<CObjectBase*> &target, const CVec4 &vColor )
{
	vector<CObjectBase*> parts;
	GetParts( &parts, target );
	for ( int k = 0; k < parts.size(); ++k )
	{
		CObjectBase* pSelection = pScene->CreateSelection( parts[k], vColor );
		if( pSelection )
			selections.push_back( pSelection );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSelectionNode::operator&( CStructureSaver &f )
{
	f.Add( 1, &selections );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGameView
////////////////////////////////////////////////////////////////////////////////////////////////////
CGameView::CGameView()
{
	pScene = CreateScene();
	nCutFloor = N_MAX_FLOOR;
	pMaterials = new CMaterialShare;
	SetHSRMode( defaultHSRMode );
	renderMode = defaultRenderMode;
	SetFogMode( defaultFogMode );
	fFogBaseHeight = 0;
	fog.SetDefaults();
	bShowParticles = true;
	vCurrentFogColor = CVec3( 0.25f, 0.25f, 0.25f );
	vDefaultClearColor = CVec3( 0.25f, 0.25f, 0.25f );
	Zero( mPrevView );
	bHasTerrain = false;
	trMode = TRM_NORMAL;
	SFBTransform identity;
	Identity( &identity.forward );
	Identity( &identity.backward );
	pIdentityTransform = new CCFBTransform( identity );
	pPrevLight = 0;
	pAlienMaterial = CreateAlienMaterial( pMaterials->GetSkyBinder() );
	bAlienStyle = false;
	bForceFastest = false;
	bWaitLoading = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::StartAlienStyle()
{
	bAlienStyle = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::FinishAlienStyle()
{
	bAlienStyle = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CGameView::CreateMaterialShared( NDb::CMaterial *p )
{
	if ( bAlienStyle || p->alpha == NDb::CMaterial::A_PREDATOR )
		return pAlienMaterial;
	return pMaterials->CreateMaterial( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLightGroup* CGameView::CreateLightGroup()
{
	return pScene->CreateLightGroup();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderNode* CGameView::NewRenderNode()
{
	CRenderNode *pRes = new CRenderNode;
	nodes.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::AddModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
	NDb::CMaterial *pMaterial, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_r )
{
	if ( !IsValid( pMaterial ) )
		return;
	SPartKey key;
	key.nID = pGeometry->GetRecordID();
	key.nPart = nPart;
	if( CResourceFileOpener::DoesExist( "Geometries", key ) )
	{
		CObjectBase *pPart = pScene->CreateGeometry( 
			shareObjInfo.Get( key ),
			CreateMaterialShared( pMaterial ),
			pPlacement, GetGroupInfo(_r) );
		pRes->AddPart( pPart );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::AddModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
	NDb::CMaterial *pMaterial, const SFBTransform &placement, const SFullRoomInfo &_r )
{
	if ( !IsValid( pMaterial ) )
		return;
	SPartKey key;
	key.nID = pGeometry->GetRecordID();
	key.nPart = nPart;
	if( CResourceFileOpener::DoesExist( "Geometries", key ) )
	{
		CObjectBase *pPart = pScene->CreateGeometry( 
			shareObjInfo.Get( key ),
			CreateMaterialShared( pMaterial ),
			placement, GetGroupInfo(_r) );
		pRes->AddPart( pPart );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::AddSkinModelPart( CRenderNode *pRes, int nPart, NDb::CGeometry *pGeometry, 
	NDb::CMaterial *pMaterial, CFuncBase<SSkeletonMatrices> *pAnimation, 
	CFuncBase<vector<NGfx::SCompactTransformer> > *pMMXAnimation, const SFullRoomInfo &_r )
{
	if ( !IsValid( pMaterial ) )
		return;
	SPartKey key;
	key.nID = pGeometry->GetRecordID();
	key.nPart = nPart;
	if( CResourceFileOpener::DoesExist( "Geometries", key ) )
	{
		CObjectBase *pPart = pScene->CreateGeometry(
			shareObjInfo.Get( key ), CreateMaterialShared( pMaterial ), pAnimation, pMMXAnimation, GetGroupInfo(_r) );
		pRes->AddPart( pPart );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateTerrainWall( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, const SFullRoomInfo &_g )
{
	IMaterial *pMaterial;
	if ( pTexture )
		pMaterial = CreateMaterial( CVec3(0.4f,0.4f,0.1f), shareTextures.Get( STextureKey( pTexture->GetRecordID(), false ) ) );
	else
		pMaterial = CreateMaterial( CVec3(0.4f,0.4f,0.1f) );

	SFullGroupInfo fg = GetGroupInfo( _g );
	CObj<NGScene::CTerrainModelWallPart> pTerrainPart = new NGScene::CTerrainModelWallPart;
	pTerrainPart->pPart = pPart;
	CRenderNode *pRes = NewRenderNode();
	pRes->AddPart( pScene->CreateGeometry( pTerrainPart, pMaterial, fg ) );
	pRes->AddPart( pScene->CreateGeometry( pTerrainPart, pMaterials->CreateOccluderMaterial(), fg ) );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateTerrainRegion( CFuncBase<STerrainInfo> *pInfo, CVersioningBase *pUpdateRegion, const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, CGrassTracker *pGrass, const SFullRoomInfo &_g )
{
	bHasTerrain = true;
	CVec3 ptCenter( ( sRegion.x1 + sRegion.x2 ) * 0.5f * FP_GRID_STEP, ( sRegion.y1 + sRegion.y2 ) * 0.5f * FP_GRID_STEP,	10 );
	CTerrainTexture *pTerrainBump = new CTerrainTexture( true, sSeed, sRegion, new CLODCalcer( ptCenter, pScene->GetCamera() ), pInfo, pUpdateRegion, pGrass );
	CTerrainTexture *pTerrainTexture = new CTerrainTexture( false, sSeed, sRegion, new CLODCalcer( ptCenter, pScene->GetCamera() ), pInfo, pUpdateRegion, pGrass );

	CPtr<IMaterial> pMaterial = CreateMaterial( CVec3(0,0,0), pTerrainTexture, pTerrainBump );
	//pMaterial->light = CMaterial::SC_NORMAL; 

	SFullGroupInfo fg = GetGroupInfo( _g );
	CRenderNode *pRes = NewRenderNode();
	for ( list<CObj<CPtrFuncBase<CTerrainPart> > >::const_iterator iTemp = partsList.begin(); iTemp != partsList.end(); iTemp++ )
	{
		CObj<NGScene::CTerrainModelPart> pTerrainPart = new NGScene::CTerrainModelPart;
		pTerrainPart->pInfo = pInfo;
		pTerrainPart->pPart = (*iTemp);
		pTerrainPart->nrRegion = sRegion;

		pRes->AddPart( pScene->CreateGeometry( pTerrainPart, pMaterial, fg ) );
		pRes->AddPart( pScene->CreateGeometry( pTerrainPart, pMaterials->CreateOccluderMaterial(), fg ) );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateGrassSector( CGrassAnimator *pEffect, NDb::CTexture *pTexture, CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SRoomInfo &_g )
{
	pEffect->SetTexture( shareTextures.Get( STextureKey( pTexture->GetRecordID(), STextureKey::TK_TRANSPARENT ) ) );
	return CreateParticles( PF_STATIC|PF_LIT, false, false, pEffect, pPlacement, bound, _g );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateMesh( NDb::CModel *pModel, CFuncBase<SFBTransform> *pPlacement, 
	const SFullRoomInfo &_r )
{
	CPtr<NDb::CModel> pHoldModel( pModel );
	if ( pPlacement == 0 )
	{
		SFBTransform identity;
		Identity( &identity.forward );
		Identity( &identity.backward );
		return CreateMesh( pModel, identity, _r );
	}
	CRenderNode *pRes = NewRenderNode();
	for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
		AddModelPart( pRes, i, pModel->pGeometry, pModel->pMaterials[i], pPlacement, _r );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateMesh( CMemObject *pModel, const CVec4 &color, 
	CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_r )
{
	CRenderNode *pRes = NewRenderNode();
	IMaterial *pMat = color.w < 1 ? transparentMaterials.CreateMaterial( color ) 
		: colorMaterials.CreateMaterial( CVec3( color.r, color.g, color.b) );
	CObjectBase *pPart;
	if ( pPlacement )
		pPart = pScene->CreateGeometry( CreateObjectInfo( pModel ), pMat, pPlacement, GetGroupInfo(_r) );
	else
		pPart = pScene->CreateGeometry( CreateObjectInfo( pModel ), pMat, GetGroupInfo(_r) );
	pRes->AddPart( pPart );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateMesh( NDb::CModel *pModel, const SFBTransform &placement, 
	const SFullRoomInfo &_r )
{
	CPtr<NDb::CModel> pHoldModel( pModel );
	if ( !IsValid( pModel->pGeometry ) )
		return 0;
	CRenderNode *pRes = NewRenderNode();
	for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
		AddModelPart( pRes, i, pModel->pGeometry, pModel->pMaterials[i], placement, _r );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &placement, 
	const SFullRoomInfo &_r )
{
	CRenderNode *pRes = NewRenderNode();
	IMaterial *pMat = color.w < 1 ? transparentMaterials.CreateMaterial( color ) 
		: colorMaterials.CreateMaterial( CVec3( color.r, color.g, color.b) );
	CObjectBase *pPart = pScene->CreateGeometry(
		CreateObjectInfo( pModel ),
		pMat,
		placement, GetGroupInfo(_r) );
	pRes->AddPart( pPart );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDecalTarget* CGameView::CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info )
{
	return pScene->CreateDecalTarget( targets, _info );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial )
{
	return pScene->AddDecal( pTarget, pMaterials->CreateMaterial( pMaterial ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateSkin( NDb::CModel *pModel, 
	CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SFullRoomInfo &_r )
{
	CPtr<NDb::CModel> pHoldModel( pModel );
	if ( pAnimation && !IsValid( pModel->pSkeleton ) )
	{
		ASSERT( 0 ); // animation for non skinned model
		pAnimation = 0;
	}
	if ( !pAnimation )
	{
		ASSERT( 0 ); // deep shit, we could create non skinned version
		return 0;
	}
	CBind *pBind = new CBind;
	pBind->pBinds = shareBinds.Get( pModel->pGeometry->GetRecordID() );
	pBind->pAnimation = pAnimation;
	pBind->pSkeleton = NAnimation::shareSkeletons.Get( pModel->pSkeleton->GetRecordID() );
	CFuncBase<vector<NGfx::SCompactTransformer> > *pMMXAnimation = MakeMMXAnimation( pBind );

	CRenderNode *pRes = NewRenderNode();
	for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
		AddSkinModelPart( pRes, i, pModel->pGeometry, pModel->pMaterials[i], pBind, pMMXAnimation, _r );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateOccluder( NDb::CAIGeometry *pGeom, const SFBTransform &placement, const SRoomInfo &_r )
{
	if ( !pGeom )
		return 0;
	CRenderNode *pRes = NewRenderNode();
	IMaterial *pMat = pMaterials->CreateOccluderMaterial();
	CObjectBase *pPart = pScene->CreateGeometry(
		shareAIObjInfoConverters.Get( pGeom->GetRecordID() ),
		pMat, 
		placement, SFullGroupInfo( GetGroupInfo( _r ), 0, 0 ) );
	pRes->AddPart( pPart );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateOccluder( NDb::CAIGeometry *pGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SRoomInfo &_r )
{
	CBind *pBind = new CBind;
	pBind->pBinds = NAI::shareAIBinds.Get( pGeom->GetRecordID() );
	pBind->pAnimation = pAnimation;
	pBind->pSkeleton = NAnimation::shareSkeletons.Get( pSkeleton->GetRecordID() );
	CFuncBase<vector<NGfx::SCompactTransformer> > *pMMXAnimation = MakeMMXAnimation( pBind );
	CRenderNode *pRes = NewRenderNode();
	CObjectBase *pPart = pScene->CreateGeometry(
		shareAISkinConverters.Get( pGeom->GetRecordID() ),
		pMaterials->CreateOccluderMaterial(),
		pBind, pMMXAnimation, SFullGroupInfo( GetGroupInfo( _r ), 0, 0 ) );
	pRes->AddPart( pPart );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelectionNode* CGameView::CreateSelection( const vector<CObjectBase*> &target, const CVec4 &vColor )
{
	return new CSelectionNode( pScene, target, vColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateParticles( int nPFlags, bool bCastShadows, bool bCrown, CPtrFuncBase<CParticleEffect> *pEffect, 
	CFuncBase<SFBTransform> *pPlacement, 
	const SBound &bound, const SRoomInfo &_r )
{
	return pScene->CreateParticles( pEffect, pPlacement, bound, GetGroupInfo( _r, bCastShadows, bCrown ), nPFlags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
static void InitParticleTextures( T *pAnimator, NDb::CParticleInstance *pInstance )
{
	pAnimator->textureIDs.resize( NDb::N_PARTICLE_TEXTURES );//pInstance->pTextures)
	int nLast = -1;
	for ( int i = 0; i < pAnimator->textureIDs.size(); ++i )
	{
		if ( !IsValid( pInstance->pTextures[i] ) )
			continue;
		nLast = i;
		pAnimator->textureIDs[i] = shareTextures.Get( STextureKey( pInstance->pTextures[i]->GetRecordID(), STextureKey::TK_TRANSPARENT ) );
	}
	pAnimator->textureIDs.resize( nLast + 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::TrueCreateParticles( bool _bIsDynamic, NDb::CEffect *pEffect, STime stBeginTime, 
	CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, const SRoomInfo &_g, NAnimation::CSkeletonAnimator *pScAnim )
{
	CPtr< CFuncBase<SFBTransform> > pTransformHolder( pPlacement );
	CPtr<NAnimation::CSkeletonAnimator> pAnimatorHolder( pScAnim );
	if ( pEffect->instances.empty() && pEffect->lights.empty() )
		return 0;
	CRenderNode *pRes = NewRenderNode();
	for ( int i = 0; i < pEffect->instances.size(); ++i )
	{
		NDb::CParticleInstance *pInstance = pEffect->instances[i];
		NDb::CParticle *pParticle = pInstance->pParticle;
		if ( !pParticle )
			continue;

		CParticleAnimator *pAnimator = new NGScene::CParticleAnimator( pInstance, stBeginTime );
		pAnimator->pInfo = shareParticles.Get( pParticle->GetRecordID() );
		pAnimator->pTime = pTime;
		SFBTransform trans;
		CVec3 scale = CVec3( pInstance->fScale, pInstance->fScale, pInstance->fScale );
		MakeMatrix( &trans.forward, pInstance->position, pInstance->rotation, scale );
		trans.backward.HomogeneousInverse( trans.forward );
		CCFBTransform *pTrans = new CCFBTransform( trans );
		CMSRNode *pMSR = new CMSRNode;
		if ( pScAnim && pInstance->nGlueToBone )
		{
			char buf[128];
			sprintf( buf, "Effect%d", pInstance->nGlueToBone );
			int nBone = pScAnim->GetBoneIndex( buf );
			NAnimation::CAddBoneFilter *pABF = new NAnimation::CAddBoneFilter( pScAnim, nBone );
			pMSR->pAncestor = pABF;
		}
		else
			pMSR->pAncestor = pPlacement;
		pMSR->pPos = pTrans;
		pAnimator->pPlacement = pMSR;
		InitParticleTextures( pAnimator, pInstance );
		int nPFlags = 0;
		if ( pInstance->light == NDb::CParticleInstance::L_LIT )
			nPFlags |= PF_LIT;
		else
			nPFlags |= PF_SELF_ILLUM;
		if ( pInstance->isStatic == NDb::CParticleInstance::P_STATIC && !_bIsDynamic )
			nPFlags |= PF_STATIC;
		else
			nPFlags |= PF_DYNAMIC;
		pRes->AddPart( CreateParticles( nPFlags, pInstance->bDoesCastShadow, pInstance->bIsCrown, pAnimator, pMSR, pParticle->bound, _g ) );
	}
	for ( int i = 0; i < pEffect->lights.size(); ++i )
	{
		NDb::CLightInstance *pInstance = pEffect->lights[i];
		NDb::CAnimLight *pLight = pInstance->pLight;
		if ( !pLight )
			continue;
		CLightAnimator *pAnimator = new NGScene::CLightAnimator( pInstance, stBeginTime, pInstance->fScale );
		pAnimator->pInfo = shareAnimLights.Get( pLight->GetRecordID() );
		pAnimator->pTime = pTime;
		SFBTransform trans;
		CVec3 scale = CVec3( pInstance->fScale, pInstance->fScale, pInstance->fScale );
		MakeMatrix( &trans.forward, pInstance->position, pInstance->rotation, scale );
		trans.backward.HomogeneousInverse( trans.forward );
		CCFBTransform *pTrans = new CCFBTransform( trans );
		CMSRNode *pMSR = new CMSRNode;
		if ( pScAnim && pInstance->nGlueToBone )
		{
			char buf[128];
			sprintf( buf, "Effect%d", pInstance->nGlueToBone );
			int nBone = pScAnim->GetBoneIndex( buf );
			NAnimation::CAddBoneFilter *pABF = new NAnimation::CAddBoneFilter( pScAnim, nBone );
			pMSR->pAncestor = pABF;
		}
		else
			pMSR->pAncestor = pPlacement;
		pMSR->pPos = pTrans;
		pAnimator->pPlacement = pMSR;

		pRes->AddPart( pScene->AddPointLight( pAnimator ) );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, 
	CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, const SRoomInfo &_r, NAnimation::CSkeletonAnimator *pScAnim )
{
	return TrueCreateParticles( true, pEffect, stBeginTime, pTime, pPlacement, _r, pScAnim );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, 
	const SFBTransform &place, const SRoomInfo &_g )
{
	return TrueCreateParticles( false, pEffect, stBeginTime, pTime, new CCFBTransform( place ), _g );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateExplosion( CFuncBase<STime> *pTime, NDb::CEffect *pEffect,
	CFuncBase<CExplosionInfo> *pExplosion, const CVec3 &pos, const SRoomInfo &_r )
{
	if ( pEffect->instances.empty() )
		return 0;
	NDb::CParticleInstance *pInstance = pEffect->instances[0];
	NDb::CParticle *pParticle = pInstance->pParticle;
	if ( !pParticle )
		return 0;

	CExplosionAnimator *pAnimator = new NGScene::CExplosionAnimator( pInstance );
	pAnimator->pInfo = shareParticles.Get( pParticle->GetRecordID() );
	pAnimator->pTime = pTime;
	pAnimator->pExplosion = pExplosion;
	CPtr<CCFBTransform> pPos = new CCFBTransform( MakeTransform( pos ) );
	bool bLit = (pInstance->light == NDb::CParticleInstance::L_LIT);
	InitParticleTextures( pAnimator, pInstance );
	SBound bound;
	bound.SphereInit( pos, 10.f ); // CRAP
	return CreateParticles( PF_STATIC|PF_SELF_ILLUM, false, false, pAnimator, pPos, bound, _r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding* CGameView::CreateBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI )
{
	CBuilding *pB = new CBuilding;
	//
	pB->Setup( nPartID, info, pBI );

	//vector<SRoomAmbient> rooms;
	//pB->GetRoomLights( &rooms );
	//for( int i = 0; i < rooms.size(); ++i )
	//{
	//	CLightCalcer *pLCalcer = new CLightCalcer( info.pVariant->GetRecordID(), rooms[i].nTargetGroupID, pAmbientColor, rooms[i].pAmbientColor, info.pGrid );
	//	pScene->SetRoomAmbient( rooms[i].nTargetGroupID, pLCalcer );
	//}
	pB->Update( pScene, pMaterials.GetPtr() );
	buildings.push_back( pB );
	return pB;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPolyline* CGameView::CreatePolyline( const vector<CVec3> &points, const CVec3 &cr )
{
	return pScene->CreatePolyline( new CMemGeometry( points ), cr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::CreateLSHead( NDb::CComplexHead *pCHead, CFuncBase<NLSHead::SHeadFrame> *pAnimation,
	CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, bool bInterface, const SRandomSeed &headSeed, bool bHasCap, const SRoomInfo &_r, CPtrFuncBase<NGfx::CTexture> *pFaceTexture, const NLSHead::CHeadInfo *pHeadInfo )
{
	CRenderNode *pRes = NewRenderNode();
	if ( !pCHead )
		return pRes;

	// Seed the head random (hair variant + materials) DETERMINISTICALLY from the head record so the UI
	// views (portrait / inventory) -- which re-create the head render node every frame -- pick the SAME
	// hair each frame instead of RE-ROLLING it (the SESSION-37 "cycling hair" bug on a multi-variant
	// hair model). Retail CreateLSHead takes a CHeadInfo and seeds from its per-unit CHeadInfo::seed
	// (@0x188c90: local_48.nSeed = param_1->seed.nSeed); this reconstruction takes the bare CComplexHead, so
	// the unit callers thread CUnit::GetHeadSeed() (== that per-unit seed); standalone heads pass a per-head seed.
	SRand rnd( headSeed );
	if ( IsValid( pCHead->pHead ) )
	{
		NGScene::CMSRConvert *pConvert = new NGScene::CMSRConvert;
		pConvert->pMove = new NGScene::CCVec3( CVec3( 0, -0.035f, 0 ) );
		pConvert->pScale = new NGScene::CCVec3( CVec3( 0.014f, 0.014f, 0.014f ) );
		pConvert->pRotate = new NGScene::CCVec3( CVec3( 0, -FP_PI2, 0 ) );
		NGScene::CMSRNode *pMSR = new NGScene::CMSRNode;
		pMSR->pAncestor = pPlacement;
		pMSR->pPos = pConvert;

		NDb::CHead *pHead = pCHead->pHead;
		CPtr<NLSHead::CHead> pMesh = new NLSHead::CHead( pMSR, pAnimation, NLSHead::shareHeads.Get( pHead->GetRecordID() ) );
		CPtr<NLSHead::CHeadBound> pBound = new NLSHead::CHeadBound( pPlacement );
		// A committed advanced-FaceGen hero carries a baked face texture (headInfo->pTexture); render the head skin
		// with an UNSHARED white-modulated diffuse material over it (a per-unit texture has no DB record to key a
		// shared material on). Otherwise keep the shared DB-keyed material -- no regression for normal mercs.
		IMaterial *pSkinMat = 0;
		if ( IsValid( pFaceTexture ) )
			pSkinMat = NGScene::CreateMaterial( CVec3( 1, 1, 1 ), pFaceTexture );
		else if ( IsValid( pHead->pMaterial ) )
			pSkinMat = CreateMaterialShared( pHead->pMaterial->GetMaterial( &rnd ) );
		if ( pSkinMat )
		{
			CObjectBase *pPart = pScene->CreateDynamicGeometry( pMesh, pSkinMat, pBound, SFullGroupInfo( GetGroupInfo(_r), 0, 0 ) );
			pRes->AddPart( pPart );
		}
	}

	vector< CPtr<NDb::CModel> > models;

	// Editor-customizable models (hair + the eye-glasses / face meshes) come from the PER-UNIT CHeadInfo
	// when a committed unit is rendered (retail CreateLSHead @0x188c90 reads hair/pMeshes/pIFMeshes from
	// param_1, the per-unit CHeadInfo) -- so an AdvFaceGen hair/eye-glasses choice survives save/load
	// instead of reverting to the shared CComplexHead default. Standalone / editor-preview callers pass no
	// CHeadInfo (pHeadInfo == 0) and keep reading the bare CComplexHead record (the live preview wants the
	// in-memory-mutated shared record). pHairInCap (helmet hair) is a dev-only CComplexHead field with no
	// per-unit CHeadInfo slot, so the cap branch keeps sourcing it from pCHead.
	// NOTE the source pointer types differ -- CComplexHead uses CPtr<CTRndModel>, CHeadInfo uses
	// CDBPtr<CTRndModel> -- so each model is resolved to a raw NDb::CTRndModel* (operator T*) before use.
	NDb::CTRndModel *pHair = pHeadInfo ? (NDb::CTRndModel*)pHeadInfo->GetHair() : (NDb::CTRndModel*)pCHead->pHair;

	if ( bInterface || !bHasCap )
	{
		if ( IsValid( pHair ) )
			models.push_back( pHair->CreateModel(&rnd) );
	}
	else
	{
		if ( IsValid( pCHead->pHairInCap ) )
			models.push_back( pCHead->pHairInCap->CreateModel(&rnd) );
		else if ( IsValid( pHair ) )
			models.push_back( pHair->CreateModel(&rnd) );
	}

	for ( int i = 0; i < NDb::N_HEAD_MESHES; ++i )
	{
		NDb::CTRndModel *pMesh = pHeadInfo
			? (NDb::CTRndModel*)( bInterface ? pHeadInfo->GetIFMeshes()[i] : pHeadInfo->GetMeshes()[i] )
			: (NDb::CTRndModel*)( bInterface ? pCHead->pIFMeshes[i] : pCHead->pMeshes[i] );
		if ( IsValid( pMesh ) )
			models.push_back( pMesh->CreateModel(&rnd) );
	}

	for ( int j = 0; j < models.size(); ++j )
	{
		NDb::CModel *pModel = models[j];
		for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
			AddModelPart( pRes, i, pModel->pGeometry, pModel->pMaterials[i], pPlacement, SFullRoomInfo( _r, 0, 0 ) );
	}

	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline CResourcePrecache<T>* MakePrecache( T *p ) { return new NGScene::CResourcePrecache<T>( p ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::Precache( NDb::CModel *pModel )
{
	if ( !pModel || !pModel->pGeometry )
		return 0;
	int nGeometryID = pModel->pGeometry->GetRecordID();
	CRenderNode *pRes = new CRenderNode;
	for ( int nPart = 0; nPart < NDb::N_MODEL_MATERIALS; ++nPart )
	{
		if ( !pModel->pMaterials[nPart] )
			continue;
		SPartKey key;
		key.nID = nGeometryID;
		key.nPart = nPart;
		if( CResourceFileOpener::DoesExist( "Geometries", key ) )
			pRes->AddPart( MakePrecache( shareObjInfo.Get( key ) ) );
	}
	if ( pModel->pSkeleton )
	{
		pRes->AddPart( MakePrecache( shareBinds.Get( nGeometryID ) ) );
		pRes->AddPart( MakePrecache( NAnimation::shareSkeletons.Get( pModel->pSkeleton->GetRecordID() ) ) );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::Draw( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath rp )
{
	if ( bWaitLoading )
	{
		SetTerrainLoadingMode( true );
		SetTerrainStressMode( true );
		pScene->PrecacheMaterials();
		while ( HasFileRequestsInFly() )
			Sleep(0);
		MarkNewDGFrame();
		pScene->PrecacheMaterials();
		SetTerrainLoadingMode( false );
		SetTerrainStressMode( false );
		bWaitLoading = false;
	}

	//for ( list<CPtr<CGrassTracker> >::iterator i = grassTrackers.begin(); i != grassTrackers.end(); ++i )
	//	(*i)->Update();
	nodes.clear();
	fog.fTime = GetTickCount() / 1024.0f;
	SGroupSelect mask( GetFloorMask( nCutFloor ), GetParticlesRequireFlag( bShowParticles ) );
	EFogMode usedFog = fogMode;
	if ( usedFog == FOG_DYNAMIC && NGfx::GetHardwareLevel() < NGfx::HL_GFORCE3 )
		usedFog = FOG_PERVERTEX;
	NGfx::CCubeTexture *pSky = 0;
	CDGPtr<CPtrFuncBase<NGfx::CCubeTexture> > pSkyNode( pMaterials->GetSky() );
	if ( pSkyNode )
	{
		pSkyNode.Refresh();
		pSky = pSkyNode->GetValue();
	}
	pScene->Draw( pTS, pClipTS, pRC, mask, rp, usedFog, fog, hsrMode, trMode, pSky );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::MakeTargetRect( CTRect<float> *pRes, const SDrawInfo &drawInfo )
{
	CTRect<float> &rFullScreen = *pRes;
	CVec2 vSize = pScene->GetScreenRect();
	rFullScreen.x1 = vSize.x * drawInfo.vOrigin.x; 
	rFullScreen.x2 = rFullScreen.x1 + vSize.x * drawInfo.vSize.x;
	rFullScreen.y1 = vSize.y * drawInfo.vOrigin.y; 
	rFullScreen.y2 = rFullScreen.y1 + vSize.y * drawInfo.vSize.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static ERenderPath GetRenderPath( ESceneRenderMode rm, bool bForceFastest )
{
	if ( NGfx::IsTnLDevice() )
		return RP_TNL;
	if ( bForceFastest || !CanRenderShadows() )
		return RP_FASTEST;
	switch ( rm )
	{
		case SRM_FASTEST: return RP_FASTEST;
		case SRM_SHOWOCCLUDERS: 
			return RP_SHOWOCCLUDERS;
		case SRM_SHOWLIGHTMAP: 
			if ( CanCacheLighting() )
				return RP_UPDATE_CL;
			return RP_FASTEST;
		case SRM_SHOWSKYMAP: 
			if ( CanCacheLighting() )
				return RP_UPDATE_CL;
			return RP_FASTEST;
		case SRM_LIGHTMAPPED: 
			if ( CanCacheLighting() )
				return RP_SHOWLIGHTMAPPED;
			return RP_FASTEST;
		case SRM_BEST:
			if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 && CanCacheLighting() )
				return RP_GF3_CL;
			if ( CanCacheLighting() )
				return RP_GF2_CL;
			if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE )
				return RP_GF2;
			break;
	}
	ASSERT(0);
	return RP_FASTEST;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ERenderPath CGameView::GetRenderPath() const
{
	return NGScene::GetRenderPath( renderMode, bForceFastest );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeClipTS( CTransformStack *pRes, const CTransformStack &ts, const CVec2 &vOrigin, const CVec2 &vSize )
{
	const SFBTransform &projection = ts.GetProjection();
	SHMatrix m = projection.forward;
	m.x = m.x * (1.0f / vSize.x) + m.w * ( ( 1 - 2 * vOrigin.x - vSize.x ) / vSize.x );
	m.y = m.y * (1.0f / vSize.y) + m.w * ( (-1 + 2 * vOrigin.y + vSize.y ) / vSize.y );
	pRes->Make( m );
	pRes->Push43( projection.backward * ts.Get().forward );
}
void CGameView::Draw( const SDrawInfo &drawInfo )
{
	if ( bHasTerrain )
	{
		if ( memcmp( &drawInfo.pTS->Get().forward, &mPrevView, sizeof(mPrevView) ) != 0 )
		{
			mPrevView = drawInfo.pTS->Get().forward;
			nSameViewCount = 0;
		}
		else
			++nSameViewCount;
		SetTerrainStressMode( nSameViewCount < 5 );
	}
	ERenderPath renderPath = GetRenderPath();
	bool bUseRegister = IsUingRegisters( renderPath );
	NGfx::CRenderContext rc;
	if ( bUseRegister )
		rc.SetVirtualRT();
	if ( drawInfo.bOverlay )
	{
		if ( bUseRegister )
			NGScene::Clear( &rc, CVec3(0,0,0) );
	}
	else
	{
		if ( drawInfo.bUseDefaultClearColor )
      NGScene::Clear( &rc, vDefaultClearColor );
		else
			NGScene::Clear( &rc, drawInfo.vClearColor );
	}
	CTransformStack tsClip;
	MakeClipTS( &tsClip, *drawInfo.pTS, drawInfo.vOrigin, drawInfo.vSize );
	if ( bUseRegister )
		Draw( &tsClip, &tsClip, &rc, renderPath );
	else
		Draw( drawInfo.pTS, &tsClip, &rc, renderPath );
	if ( bUseRegister )
	{
		//if ( drawInfo.bOverlay )
		CTRect<float> rFullScreen;
		MakeTargetRect( &rFullScreen, drawInfo );
		switch ( renderMode )
		{
			case SRM_SHOWLIGHTMAP:
				CopyRegisterOnScreen( rFullScreen, RCM_SHOWALPHA, 3 );
				break;
			case SRM_SHOWSKYMAP:
				CopyRegisterOnScreen( rFullScreen, RCM_COPY, 3 );
				break;
			default:
				CopyRegisterOnScreen( rFullScreen, drawInfo.bOverlay ? RCM_TRANSPARENT : RCM_COPY, 0 );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CGameView::GetScreenRect()
{
	return pScene->GetScreenRect();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetAmbient( const CVec3 &vBottomAmbientColor, const CVec3 &vTopAmbientColor )
{
	pScene->SetAmbient( vBottomAmbientColor, vTopAmbientColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddDirectionalLight( const CVec3 &ptColor, const CVec3 &ptLight, const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly ) 
{
	CCVec3 *pCColor = new CCVec3( ptColor );
	return pScene->AddDirectionalLight( pCColor, pCColor, CVec3(0,0,0), ptLight, ptOrigin, ptSize, fMaxHeight, bLightmapOnly, 1.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly )
{
	if ( fR <= 0 )
		return 0;
	return pScene->AddPointLight( ptColor, ptOrigin, fR, bLightmapOnly );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddFlare( CFuncBase<CVec3> *pOrigin, CFuncBase<STime> *pTime, int nFloor, float fFlareRadius, NDb::CTexture *pFlareTexture, float fOnTime, float fOffTime )
{
	//fFlareRadius = 1;
	if ( fFlareRadius <= 0 || pTime == 0 )
		return 0;
	// glow in the dark
	SBound bound;
	bound.SphereInit( CVec3(0,0,0), fFlareRadius );
	int nFlareID = 4282;
	if ( pFlareTexture )
		nFlareID = pFlareTexture->GetRecordID();
	return CreateParticles( 
		PF_DYNAMIC | PF_SELF_ILLUM, false, false, 
		new CPointGlowAnimator( 
			pScene,
			pTime, 
			pOrigin,
			shareTextures.Get( STextureKey( nFlareID, STextureKey::TK_TRANSPARENT ) ),
			fFlareRadius,
			fOnTime,
			fOffTime ),
		new CMNode( pIdentityTransform, pOrigin ), bound, 
		SRoomInfo( nFloor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddPostFilter( const vector<CObjectBase*> &target, IPostProcess *pEffect )
{
	CPtr<IPostProcess> pHold(pEffect);
	vector<CObjectBase*> parts;
	GetParts( &parts, target );
	CRenderNode *pRes = new CRenderNode;
	for ( int k = 0; k < parts.size(); ++k )
	{
		CObjectBase *p = pScene->CreatePostProcessor( parts[k], pEffect );
		if ( p )
			pRes->AddPart( p );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CGameView::AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, 
	float fRadius, NDb::CTexture *pMask, bool bLightmapOnly )
{
	if ( pMask )
		return pScene->AddSpotLight( new CCVec3(ptColor), ptOrigin, ptDir, fFOV, fRadius, shareTextures.Get( pMask->GetRecordID() ), bLightmapOnly );
	return pScene->AddSpotLight( new CCVec3(ptColor), ptOrigin, ptDir, fFOV, fRadius, 0, bLightmapOnly );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CheckCircularGF2LightLink( NDb::CAmbientLightReal *pLight )
{
	if ( pLight && pLight->pGF2Light )
		pLight->pGF2Light->pGF2Light = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetAmbient( NDb::CAmbientLightReal *pLight, ELightMode lm )
{
	CPtr<NDb::CAmbientLightReal> pHold(pLight);
	pPrevLight = pLight;
	prevLightMode = lm;
	if ( !IsValid( pLight ) )
	{
		pAmbientDirectional = 0;
		SetAmbient( CVec3(0.5f, 0.5f, 0.5f), CVec3(0.5f, 0.5f, 0.5f) );
		pMaterials->SetSky( 0 );
		return;
	}
	vDefaultClearColor =  pLight->vBackColor;
	CheckCircularGF2LightLink( pLight );
	
	if ( !IsUsingShadows( GetRenderPath() ) && pLight->pGF2Light )//NGfx::GetHardwareLevel() < NGfx::HL_GFORCE3 
	{
		SetAmbient( pLight->pGF2Light, lm );
		pPrevLight = pLight;
		prevLightMode = lm;
		return;
	}
	CVec3 vBottomAmbientColor( pLight->vGroundAmbientColor ), vTopAmbientColor( pLight->vAmbientColor );
	CVec3 vLightColor( pLight->vLightColor );
	vTopAmbientColor.Minimize( CVec3( 1,1,1 ) );
	vBottomAmbientColor.Minimize( CVec3( 1,1,1 ) );
	vLightColor.Minimize( CVec3( 1,1,1 ) );
	// directional light
	CVec3 vLightDir;
	float fHor = sin( ToRadian( pLight->fPitch ) );
	vLightDir.z = -cos( ToRadian( pLight->fPitch ) );
	vLightDir.x = fHor * cos( ToRadian( pLight->fYaw ) );
	vLightDir.y = fHor * sin( ToRadian( pLight->fYaw ) );
	if ( lm == LT_ZONE )
		pAmbientDirectional = pScene->AddDirectionalLight( 
			new CCVec3( vLightColor ), new CCVec3( pLight->vGlossColor ), 
			pLight->vShadowColor,
			vLightDir, CVec3(0,0,0), CVec2( 150, 150 ), 20, false, pLight->fBlurStrength );
	else
		pAmbientDirectional = pScene->AddDirectionalLight( 
			new CCVec3( vLightColor ), new CCVec3( pLight->vGlossColor ), 
			pLight->vShadowColor,
			vLightDir, CVec3(0,0,1), CVec2(1,1), 3, false, pLight->fBlurStrength );
	// fog
	fog.fDist = pLight->fFogDistance;
	fog.vFogColor = pLight->vFogColor;
	fog.fHeight = pLight->fVapourHeight + fFogBaseHeight;
	fog.fDensity = pLight->fVapourDensity;
	fog.vWaterColor = pLight->vVapourColor;
	fog.fCameraHeight = 10;
	fog.fVapourHeightStart = pLight->fVapourStartHeight + fFogBaseHeight;
	fog.fVapourNoiseParam = pLight->fVapourNoiseParam;
	fog.fVapourSpeed = pLight->fVapourSpeed;
	fog.fVapourSwitchTime = pLight->fVapourSwitchTime;
	fog.fTime = 0;
	fog.fDistStart = pLight->fFogStartDistance;
	vCurrentFogColor = fog.vFogColor;
	// ambient light
	pScene->SetAmbient( vBottomAmbientColor, vTopAmbientColor );
	if ( IsValid( pLight->pSky ) )
		pMaterials->SetSky( shareCubeTextures.Get( pLight->pSky->GetRecordID() ) );
	else
		pMaterials->SetSky( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetFogBaseHeight( float f )
{
	fFogBaseHeight = f;
	if ( pPrevLight )
		SetAmbient( pPrevLight, prevLightMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetRenderMode( ESceneRenderMode mode )
{
	renderMode = mode;
	if ( pPrevLight )
		SetAmbient( pPrevLight, prevLightMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ESceneRenderMode CGameView::GetRenderMode() const
{
	return renderMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EFogMode CGameView::GetFogMode() const
{
	return fogMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetFogMode( EFogMode mode )
{
	fogMode = mode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGameView::TraceScene( const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps, CObjectBase **ppObject )
{
	SGroupSelect mask( GetFloorMask( N_MAX_FLOOR ), N_MASK_CAST_SHADOW | GetParticlesRequireFlag( true ) );
	return pScene->TraceScene( mask, r, pfT, pNormal, pColor, ps, ppObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGameView::GetCutFloor()
{
	return nCutFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameView::SetCutFloor( int _nFloor )
{
	nCutFloor = Clamp( _nFloor, N_MIN_FLOOR, N_MAX_FLOOR );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IGameView* CreateNewView()
{
	return new CGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IGameView* CreateNewFastInterfaceView()
{
	CGameView *pRes = new CGameView;
	pRes->SetForceFastest( true );
	pRes->SetFastMode();
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands/Vars
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSetHSR( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	defaultHSRMode = HSR_NONE;
	if ( sValue.GetFloat() != 0 )
		defaultHSRMode = HSR_FAST;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSetFog( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	defaultFogMode = FOG_NONE;
	if ( sValue.GetFloat() == 2 )
		defaultFogMode = FOG_DYNAMIC;
	else if ( sValue.GetFloat() == 1 )
		defaultFogMode = FOG_PERVERTEX;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSetShadows( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	defaultRenderMode = SRM_FASTEST;//SHADOW_NOSHADOWS;
	if ( sValue.GetFloat() != 0 )
		defaultRenderMode = SRM_BEST;//SHADOW_FULL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(GView)
	REGISTER_VAR( "gfx_hsr", VarSetHSR, 1, true )
	REGISTER_VAR( "gfx_fog", VarSetFog, 0, true )
	REGISTER_VAR( "gfx_shadows", VarSetShadows, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x01741140, CGameView )
REGISTER_SAVELOAD_CLASS( 0x01741141, CRenderNode )
REGISTER_SAVELOAD_CLASS( 0x01741142, CSelectionNode )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x028b2120, CResourcePrecache<CObjectInfoLoader>, CResourcePrecache )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x028b2121, CResourcePrecache<CFileBind>, CResourcePrecache )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x028b2122, CResourcePrecache<NAnimation::CFileSkeleton>, CResourcePrecache )
