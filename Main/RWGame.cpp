#include "StdAfx.h"
#include "wInterface.h"
#include "wInterfaceVisitors.h"
#include "GView.h"
#include "GSceneUtils.h"
#include "Transform.h"
#include "RPGUnit.h"
#include "RWGame.h"
#include "GAnimFormat.h"
#include "GAnimation.h"
#include "GAnimPath.h"
#include "..\Misc\BasicShare.h"
#include "TerrainInfo.h"
#include "GTerrain.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataAnimation.h"
#include "GMatShare.h"
#include "InventoryUnit.h"
///
#include "GPostProcessors.h"
#include "GGrass.h"
#include "GParticles.h"
#include "GParticleInfo.h"

#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"

#include "LSHead.h"
#include "LSController.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MemObject.h"
vector<SSphere> sphereParticles;	// test sphere visualization
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRender
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOV = 60;
////////////////////////////////////////////////////////////////////////////////////////////////////
// ChooseBodyColor @0x2cb6b0 -- overwrite the body model's SKIN material slot (pMaterials[1] = the neck/hands skin)
// with the chosen race's material, so the body skin tone tracks the FaceGen Nationality slider (in retail the head
// FACE recolour ALSO recolours the neck/hands; the dev dropped this). Reads the race off the head's CComplexHead
// (CHeadInfo->GetHead()->pBodyColor, set by SetMMTension "Nationality") -> CRace::pMaterial -> GetMaterial. A
// pre-mutation of the (shared) body CModel that CreateSkin then renders; call it before each body-skin submit.
void ChooseBodyColor( NDb::CModel *pModel, NLSHead::CHeadInfo *pHead )
{
	if ( !pModel || !IsValid( pHead ) )
		return;
	NDb::CComplexHead *ch = pHead->GetHead();
	if ( !IsValid( ch ) || !IsValid( ch->pBodyColor ) )
		return;
	NDb::CRace *race = ch->pBodyColor;
	if ( !IsValid( race->pMaterial ) )
		return;
	SRand rnd( SRandomSeed( race->GetRecordID() ) );   // deterministic seed -> stable skin variant across re-renders
	NDb::CMaterial *m = race->pMaterial->GetMaterial( &rnd );
	if ( m )
		pModel->pMaterials[1] = m;                     // index 1 = the skin slot (neck/hands)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelection: public CObjectBase
{
	OBJECT_BASIC_METHODS( CSelection );
public:
	ZDATA
	CVec4 vColor;
	CObj<NGScene::CSelectionNode> pSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vColor); f.Add(3,&pSelection); return 0; }

	CSelection() {}
	CSelection( const CVec4 &_vColor, NGScene::CSelectionNode *_pSelection ): vColor( _vColor ), pSelection( _pSelection ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSetRender: public COrdinarySyncDst<NWorld::IVisObj,CSetRender>, public NWorld::IRenderVisitor
{
	typedef COrdinarySyncDst<NWorld::IVisObj,CSetRender> TParent;
	typedef unordered_map<CPtr<CObjectBase>, CPtr<CSelection>, SPtrHash> CSelectionHash;
	ZDATA_(TParent)
	CPtr<NGScene::IGameView> pScene;
	CPtr<NGScene::CGrass> pGrass;
	CPtr<CFuncBase<STime> > pTime, pAimTime;
	vector<CPtr<NWorld::IVisObj> > objects;
	CSelectionHash selections;
	CPtr<NLSHead::CHeadsController> pHeadsController;
	// release CSetRender tag 9: transient unit muzzle/effect flashes (scriptParticles-era).
	// Dead in this predecessor (nothing pushes one) -> always empty, save-format member only.
	struct SUnitFlash { CObj<CObjectBase> pFlash; STime tEnd; SUnitFlash() {} int operator&( CStructureSaver &f ) { f.Add(2,&pFlash); f.Add(3,&tEnd); return 0; } };
	list<SUnitFlash> unitFlashes;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TParent*)this); f.Add(2,&pScene); f.Add(3,&pGrass); f.Add(4,&pTime); f.Add(5,&pAimTime); f.Add(6,&objects); f.Add(7,&selections); f.Add(8,&pHeadsController); f.Add(9,&unitFlashes); return 0; }
private:
	virtual void PostVisit( int nID, NWorld::IVisObj *pObject );
	CSelection* CreateSelection( int nID, const CVec4 &vColor, CSelection *pSource = 0 );
	void AddFilter( NGScene::IPostProcess *p, int nFloor );
public:
	CSetRender() {}
	CSetRender( CSyncSrc<NWorld::IVisObj> *pSrc, NGScene::IGameView *_pScene )
		: TParent(pSrc), pScene(_pScene) {}
	virtual void SetNewSource( CSyncSrc<NWorld::IVisObj> *_pSrc );
	void SetTimer( CFuncBase<STime> *_pTime, CFuncBase<STime> *_pAimTime ) { pTime = _pTime; pAimTime = _pAimTime; }
	void SetHeadsController( NLSHead::CHeadsController *_pHeadsController ) { pHeadsController = _pHeadsController; }
	void SetGrass( NGScene::CGrass *_pGrass ) { pGrass = _pGrass; }
	virtual NGScene::CLightGroup* MakeGroup();
	virtual void AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, CFuncBase<SFBTransform> *pPosition,
		NAnimation::CSkeletonAnimator *pScAnimator = 0 );
	virtual void AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, const SFBTransform &place );
	virtual void AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fRadius, bool bLightmapOnly );
	virtual void AddFlare( CFuncBase<CVec3> *pOrigin, float fFlareRadius, NDb::CTexture *pFlareTexture, int nFloor, float fOnTime, float fOffTime );
	virtual void AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, NDb::CTexture *pMask, bool bLightmapOnly );
	virtual void AddMesh( NDb::CModel *pModel, const SFBTransform &position, NGScene::CLightGroup *pGroup, int nFloor, int nUserID );
	virtual void AddMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &position, int nUserID );
	virtual void AddItemMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor );
	virtual void AddItemHead( NWorld::CUnit *pUnit, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor );
	virtual void AddMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, CFuncBase<NAnimation::SSkeletonState> *pState, const vector<SBoundMesh> &boundMeshes, NGScene::CLightGroup *pGroup, int nFloor, NWorld::CUnit *pHead = 0, int nUserID = 0 );
	virtual void AddBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI );
	virtual void AddTerrainParts( const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, CTerrainInfoHolder *pInfo, CVersioningBase *pUpdateRegion, int nUserID );
	virtual void AddTerrainWallPart( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, CTerrainInfoHolder *pInfo, int nUserID );
	virtual void AddGrass( CTerrainInfoHolder *pInfo );
	virtual void AddGrassEvent( const CVec3 &ptPlace );
	virtual void AddExplosion( NDb::CEffect *pEffect, CFuncBase<NGScene::CExplosionInfo> *pExplosion, const CVec3 &pos );
	virtual void AddPolyline( const vector<CVec3> &points, const CVec3 &cr );
	virtual void AddHead( NWorld::CUnit *pUnit, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room );
	virtual void AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room );
	virtual void AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room, NLSHead::CHeadTransformInfo *pTransformInfo, CPtrFuncBase<NGfx::CTexture> *pFaceTexture = 0 );
	virtual void AddOccluder( NDb::CAIGeometry *pAIGeom, const SFBTransform &pos, int nFloor );
	virtual void AddOccluder( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor );
	virtual NGScene::CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const NGScene::SDecalMappingInfo &_info );
	virtual void AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial );
	virtual void LoadGeometry( NDb::CModel *pModel );
	virtual void AddColorPostFilter( const CVec4 &vColor );
	virtual void StartAlienStyle();
	virtual void FinishAlienStyle();
	virtual void SetBaseFogHeight( float f );
	//
	CObjectBase* Select( CObjectBase *pSelect, const CVec4 &vColor = CVec4( 0, 1, 1, 1 ) );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSetRender
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::SetNewSource( CSyncSrc<NWorld::IVisObj> *_pSrc )
{
	TParent::SetNewSource( _pSrc );
	objects.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelection* CSetRender::CreateSelection( int nID, const CVec4 &vColor, CSelection *pSource )
{
	const vector<CObj<CObjectBase> > &ob = GetObjects( nID );
	vector<CObjectBase*> t;
	for ( int k = 0; k < ob.size(); ++k )
		t.push_back( ob[k] );

	if ( pSource )
	{
		pSource->vColor = vColor;
		pSource->pSelection = pScene->CreateSelection( t, vColor );
		return pSource;
	}
	return new CSelection( vColor, pScene->CreateSelection( t, vColor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::PostVisit( int nID, NWorld::IVisObj *pObject )
{
	if ( nID >= objects.size() )
		objects.resize( nID + 1 );
	objects[nID] = pObject;
	CSelectionHash::iterator i = selections.find( pObject );
	if ( i != selections.end() && IsValid( i->second ) )
		CreateSelection( nID, i->second->vColor, i->second );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CSetRender::Select( CObjectBase *pSelect, const CVec4 &_vColor )
{
	CSelectionHash::iterator iTemp = selections.find( pSelect );
	if ( iTemp != selections.end() )
	{
		if ( IsValid( iTemp->second ) && iTemp->second->vColor == _vColor )
			return iTemp->second;
	}
	for ( int k = 0; k < objects.size(); ++k )
	{
		if ( objects[k] == pSelect )
		{
			CSelection *pRes = CreateSelection( k, _vColor );
			selections[pSelect] = pRes;
			return pRes;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::CLightGroup* CSetRender::MakeGroup()
{
	NGScene::CLightGroup *pRes = pScene->CreateLightGroup();
	Register( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, const SFBTransform &place )
{
	if ( !IsValid( pEffect ) )
		return;
	Register( pScene->CreateParticles( pEffect, tBegin, pTime, place, NGScene::SRoomInfo( nFloor ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, CFuncBase<SFBTransform> *pPosition,
	NAnimation::CSkeletonAnimator *pScAnimator )
{
	if ( !IsValid( pEffect ) )
		return;
	Register( pScene->CreateParticles( pEffect, tBegin, pTime, pPosition, NGScene::SRoomInfo( nFloor ), pScAnimator ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fRadius, bool bLightmapOnly )
{
	Register( pScene->AddPointLight( ptColor, ptOrigin, fRadius, bLightmapOnly ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddFlare( CFuncBase<CVec3> *pOrigin, float fFlareRadius, NDb::CTexture *pFlareTexture, int nFloor, float fOnTime, float fOffTime )
{
	Register( pScene->AddFlare( pOrigin, pTime, nFloor, fFlareRadius, pFlareTexture, fOnTime, fOffTime ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, NDb::CTexture *pMask, bool bLightmapOnly )
{
	Register( pScene->AddSpotLight( ptColor, ptOrigin, ptDir, fFOV, fRadius, pMask, bLightmapOnly ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddMesh( NDb::CModel *pModel, const SFBTransform &position, NGScene::CLightGroup *pGroup, int nFloor, int nUserID )
{
	NGScene::SRoomInfo room( pGroup, nFloor );
	Register( pScene->CreateMesh( pModel, position, NGScene::SFullRoomInfo( room, GetCurrentSrcObject(), nUserID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddPolyline( const vector<CVec3> &points, const CVec3 &cr )
{
	Register( pScene->CreatePolyline( points, cr ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &position, int nUserID )
{
	Register( pScene->CreateMesh( pModel, color, position, NGScene::SFullRoomInfo( NGScene::SRoomInfo(), GetCurrentSrcObject(), nUserID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddItemMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor )
{
	NAnimation::CSkeletonAnimator *pAnimator = new NAnimation::CSkeletonAnimator( 0 );
	pAnimator->pTime = pTime;
	pAnimator->bServer = false;
	pAnimator->bItem = true;
	pAnimator->AddAimer( 0, pAnimation, pAimTime );
	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimator );
	Register( pScene->CreateMesh( pModel, pFilter, nFloor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddItemHead( NWorld::CUnit *pUnit, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor )
{
	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimation, 0 );
	AddHead( pUnit, pFilter, nFloor );
/*	NAnimation::CSkeletonAnimator *pAnimator = new NAnimation::CSkeletonAnimator( 0 );
	pAnimator->pTime = pTime;
	pAnimator->bServer = false;
	pAnimator->bItem = true;
	pAnimator->AddAimer( 0, pAnimation, pAimTime );
	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimator );
	Register( pScene->CreateMesh( pModel, pFilter, room ) );*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, 
	CFuncBase<NAnimation::SSkeletonState> *pState, const vector<SBoundMesh> &boundMeshes, NGScene::CLightGroup *pGroup, 
	int nFloor, NWorld::CUnit *pHead, int nUserID )
{
	NGScene::SRoomInfo room( pGroup, nFloor );
	NGScene::SFullRoomInfo fakeRoom( room, 0, -1 );
	CPtr<NAnimation::CSkeletonAnimator> pAnimator = new NAnimation::CSkeletonAnimator( pModel->pSkeleton );
	pAnimator->pTime = pTime;
	pAnimator->AddSmartAimer( 0, pAnimation, pState, pAimTime, pAnimator, pModel->pSkeleton );
	pAnimator->bServer = false;
	// Recolour the body skin (neck/hands) to the committed hero's race before the skin render (retail AddMesh
	// @0x2ccf60 calls ChooseBodyColor when a head info is passed). pHead is the unit -> its CHeadInfo's race.
	if ( pHead )
		ChooseBodyColor( pModel, pHead->GetHeadInfo() );
	Register( pScene->CreateSkin( pModel, pAnimator, NGScene::SFullRoomInfo( room, GetCurrentSrcObject(), nUserID ) ) );

	for ( int k = 0; k < boundMeshes.size(); ++k )
	{
		const SBoundMesh &m = boundMeshes[k];
		int nIndex = pAnimator->GetBoneIndex( m.pszBindBone );
		if ( nIndex < 0 )
			continue;
		NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimator, nIndex );
			
		/*NAnimation::CAddBoneLocators *pLocators = new NAnimation::CAddBoneLocators( nIndex, pModel->pGeometry );
		pLocators->pAnimation = pO->pAnimator;
		pO->pLocators = pLocators;*/
			
		Register( pScene->CreateMesh( m.pModel, pFilter, fakeRoom ) );
	}

	if ( pHead )
	{
		NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimator, 12 ); // CRAP - head bone
		AddHead( pHead, pFilter, room );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddTerrainParts( const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, CTerrainInfoHolder *pInfo, CVersioningBase *pUpdateRegion, int nUserID )
{
	Register( pScene->CreateTerrainRegion( pInfo, pUpdateRegion, sSeed, sRegion, partsList, pGrass->CreateTracker( pInfo ), NGScene::SFullRoomInfo( NGScene::SRoomInfo(0), pInfo, nUserID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddTerrainWallPart( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, CTerrainInfoHolder *pInfo, int nUserID )
{
	Register( pScene->CreateTerrainWall( pPart, pTexture, NGScene::SFullRoomInfo( NGScene::SRoomInfo(0), pInfo, nUserID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddGrass( CTerrainInfoHolder *pInfo )
{
	CPtr<NGScene::CGrassTracker> pGrassTracker = pGrass->CreateTracker( pInfo );

	SBound bound;
	SFBTransform transform;
	for ( int nLayer = 0; nLayer < pGrassTracker->GetNumLayers(); ++nLayer )
	{
		int nTexID = pGrassTracker->GetTextureLayerID( nLayer );
		if ( nTexID < 0 )
			continue;
		for ( int nY = 0; nY < pGrassTracker->GetNumSectorsY(); ++nY )
		{
			for ( int nX = 0; nX < pGrassTracker->GetNumSectorsX(); ++nX )
			{
				CPtrFuncBase<NGScene::CGrassPosition> *pGrassPos = pGrassTracker->GetGrassPosCalcer( nLayer, nX, nY );
				if ( pGrassPos )
				{
					NGScene::CGrassAnimator *pAnimator = new NGScene::CGrassAnimator(
						pGrassTracker->GetGrass( nLayer ), pGrassTracker, pGrassPos,
						pTime, nLayer );
					pGrassTracker->GetSectorBound( nLayer, nX, nY, &bound );
					pGrassTracker->GetBoundTransform( nX, nY, &transform );
					NGScene::CCFBTransform *pPlace = new NGScene::CCFBTransform( transform );
					Register( pScene->CreateGrassSector( pAnimator, NDb::GetTexture( nTexID ), pPlace, bound ) );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI )
{
	Register( pScene->CreateBuildingPart( nPartID, info, pBI ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddGrassEvent( const CVec3 &ptPlace )
{
	pGrass->Wave( ptPlace );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddExplosion( NDb::CEffect *pEffect, CFuncBase<NGScene::CExplosionInfo> *pExplosion, const CVec3 &pos )
{
	Register( pScene->CreateExplosion( pTime, pEffect, pExplosion, pos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddHead( NWorld::CUnit *pUnit, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room )
{
	if ( !IsValid(pHeadsController) )
		return;

	NLSHead::CHeadAnimator *pAnimator = pHeadsController->GetAnimator(pUnit);
	if ( !pAnimator )
		return;

	bool bHasCap = pUnit->IsCapPresent();
	// A committed advanced-FaceGen hero carries a baked face texture (CHeadInfo::pTexture); pass it so the head
	// skin renders the recoloured texture instead of the base DB material.
	NLSHead::CHeadInfo *pHI = pUnit->GetHeadInfo();
	CPtrFuncBase<NGfx::CTexture> *pFaceTex = IsValid( pHI ) ? pHI->GetFaceTexture() : 0;
	Register( pScene->CreateLSHead( pUnit->GetDBHead(), pAnimator, pHeadsController->GetTime(), pPosition, false, pUnit->GetHeadSeed(), bHasCap, room, pFaceTex, pHI ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room )
{
	if ( !IsValid(pHeadsController) )
		return;

	NLSHead::CHeadAnimator *pAnimator = new NLSHead::CHeadAnimator( pTime, pHead->pHead );
	if ( !pAnimator )
		return;

	bool bHasCap = false; //pUnit->IsCapPresent();
	Register( pScene->CreateLSHead( pHead, pAnimator, pTime, pPosition, false, SRandomSeed( pHead->GetRecordID() ), bHasCap, room ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-new: a standalone head with a LIVE macro-muscle morph (the advanced FaceGen editor). Same as
// the 3-arg CComplexHead overload, but hands the CHeadTransformInfo to the head animator so it lays the
// editor's tension sliders over the idle pose. (Sentinels-LS: the morph rides the proven CHeadAnimator.)
void CSetRender::AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room, NLSHead::CHeadTransformInfo *pTransformInfo, CPtrFuncBase<NGfx::CTexture> *pFaceTexture )
{
	if ( !IsValid(pHeadsController) )
		return;

	NLSHead::CHeadAnimator *pAnimator = new NLSHead::CHeadAnimator( pTime, pHead->pHead );
	if ( !pAnimator )
		return;
	pAnimator->SetHeadTransformInfo( pTransformInfo );

	bool bHasCap = false;
	// pFaceTexture is the caller's PERSISTENT live texture-preview node (CFakeRPGUnit owns one across re-Visits,
	// which happen on every slider move / rotation) -- the head material samples it so the preview retints live.
	Register( pScene->CreateLSHead( pHead, pAnimator, pTime, pPosition, false, SRandomSeed( pHead->GetRecordID() ), bHasCap, room, pFaceTexture ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddOccluder( NDb::CAIGeometry *pAIGeom, const SFBTransform &pos, int nFloor )
{
	NGScene::SRoomInfo room( 0, nFloor );
	Register( pScene->CreateOccluder( pAIGeom, pos, room ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddOccluder( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor )
{
	NGScene::SRoomInfo room( 0, nFloor );
	Register( pScene->CreateOccluder( pAIGeom, pSkeleton, pAnimation, room ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::CDecalTarget* CSetRender::CreateDecalTarget( const vector<CObjectBase*> &targets, const NGScene::SDecalMappingInfo &_info )
{
	return pScene->CreateDecalTarget( targets, _info );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial )
{
	Register( pScene->AddDecal( pTarget, pMaterial ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::LoadGeometry( NDb::CModel *pModel ) 
{
	Register( pScene->Precache( pModel ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddFilter( NGScene::IPostProcess *p, int nFloor )
{
	const vector<CObj<CObjectBase> > &src = GetCurrentObjects();
	vector<CObjectBase*> stuff( src.size() );
	for ( int k = 0; k < stuff.size(); ++k )
		stuff[k] = src[k];
	Register( pScene->AddPostFilter( stuff, p ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::AddColorPostFilter( const CVec4 &vColor )
{
	AddFilter( new NGScene::CPostColorer( new NGScene::CCVec4( vColor ) ), 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::StartAlienStyle() 
{
	pScene->StartAlienStyle();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::FinishAlienStyle() 
{
	pScene->FinishAlienStyle();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::SetBaseFogHeight( float f ) 
{
	pScene->SetFogBaseHeight( f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFakeRPGUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFakeRPGUnit: public NWorld::IVisObj
{
	OBJECT_NOCOPY_METHODS(CFakeRPGUnit);
	ZDATA
	float fAngle;
	CPtr<NGScene::IGameView> pView;
	CPtr<NDb::CModel> pModel;
	CPtr<NRPG::CUnit> pUnit;
	CObj<CFuncBase<STime> > pTime;
	CObj<NAnimation::CAnimation> pAnimation;
	CObj<NAnimation::CSkeletonAnimator> pAnimator;
	CSyncSrcBind<NWorld::IVisObj> bindGlobal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fAngle); f.Add(3,&pView); f.Add(4,&pModel); f.Add(5,&pUnit); f.Add(6,&pTime); f.Add(7,&pAnimation); f.Add(8,&pAnimator); f.Add(9,&bindGlobal); return 0; }
	// Live head-morph state for the advanced FaceGen editor (release-new; NOT serialized -- the FaceGen
	// preview is transient). Sliders drive it via SetLSHeadParam; Visit hands it to the 4-arg AddHead so
	// the head animator lays the tensions over the idle pose.
	CObj<NLSHead::CHeadTransformInfo> pHeadTransformInfo;
	// Live face-texture preview node (release-new; NOT serialized). PERSISTENT across re-Visits (every slider
	// move / rotation re-Visits) so its CTexture isn't churned -- it re-composites only when a slider changes.
	CObj<NLSHead::CHeadTextureTransformer> pHeadTextureTransformer;
public:
	CFakeRPGUnit() {}
	CFakeRPGUnit( NGScene::IGameView *pView, CSyncSrc<NWorld::IVisObj> *pSrc, NRPG::CUnit *_pUnit, CFuncBase<STime>* _pTime );

	float GetLSHeadParam( const char *szName );
	void SetLSHeadParam( const char *szName, float fValue );
	NLSHead::CHeadInfo* CreateLSHeadInfo();

	virtual void Visit( NWorld::IRenderVisitor *p );
	void Update( float fAngle );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFakeRPGUnit::CFakeRPGUnit( NGScene::IGameView *_pView, CSyncSrc<NWorld::IVisObj> *pSrc, NRPG::CUnit *_pUnit, CFuncBase<STime>* _pTime ):
	pView( _pView ), pModel( _pUnit->pModel ), pUnit( _pUnit ), pTime( _pTime ), fAngle( 0 )
{
	int nAnimFlagsClassSex = NDb::CAnimation::IN_REALTIME;
	nAnimFlagsClassSex |=	pUnit->GetPers()->bIsFemale? NDb::CAnimation::SEX_FEMALE : NDb::CAnimation::SEX_MALE;

	pAnimator = new NAnimation::CSkeletonAnimator( pModel->pSkeleton );
	pAnimator->pTime = _pTime;
	pAnimator->bServer = false;
	pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( NDb::CAnimation::POSE, NDb::CAnimation::POSE_STAND | NDb::CAnimation::WEAPON_NONE, 0, nAnimFlagsClassSex ), 0, true );
	pAnimator->AddAnimator( 0, pAnimation );

	// build the live head-morph state so the advanced FaceGen sliders have something to drive
	if ( IsValid( pUnit->GetHead() ) )
		pHeadTransformInfo = new NLSHead::CHeadTransformInfo( pUnit->GetHead(), pTime );

	bindGlobal.Link( pSrc, this );
	Update( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CFakeRPGUnit::GetLSHeadParam( const char *szName )
{
	if ( IsValid( pHeadTransformInfo ) )
		return pHeadTransformInfo->GetMMTension( szName );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeRPGUnit::SetLSHeadParam( const char *szName, float fValue )
{
	if ( IsValid( pHeadTransformInfo ) )
	{
		pHeadTransformInfo->SetMMTension( szName, fValue );
		bindGlobal.Update();   // re-emit so the head re-deforms this frame
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NLSHead::CHeadInfo* CFakeRPGUnit::CreateLSHeadInfo()
{
	if ( IsValid( pHeadTransformInfo ) )
		return pHeadTransformInfo->CreateHeadInfo();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeRPGUnit::Update( float _fAngle )
{
	if ( fAngle != _fAngle)
	{
		fAngle = _fAngle;
		pAnimation->SetStand( 0, CVec3( 0, 0, 0 ), fAngle );
	}

	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeRPGUnit::Visit( NWorld::IRenderVisitor *p )
{
	vector<NWorld::IRenderVisitor::SBoundMesh> boundMeshes;
	// Recolour the body skin (neck/hands) to the chosen race BEFORE submitting the body mesh -- AddMesh's pHead
	// arg is 0 here, so the body skin is overridden by pre-mutating the model (retail CFakeRPGUnit::Visit @0x2cdda0).
	ChooseBodyColor( pModel, pUnit->GetHeadInfo() );
	p->AddMesh( pModel, pAnimator, 0, boundMeshes, 0, 0 );

	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter( pAnimator, 12 ); // CRAP - head bone
	// Build the live face-texture preview node ONCE (persists across re-Visits), then hand it to AddHead so the
	// preview head's material samples it -> the skin recolours live as the sliders move.
	if ( IsValid( pHeadTransformInfo ) && !IsValid( pHeadTextureTransformer )
		&& IsValid( pUnit->GetHead() ) && IsValid( pUnit->GetHead()->pHead )
		&& IsValid( pUnit->GetHead()->pHead->pTransformableTextures ) )
		pHeadTextureTransformer = new NLSHead::CHeadTextureTransformer( pHeadTransformInfo, pUnit->GetHead()->pHead );
	p->AddHead( pUnit->GetHead(), pFilter, NGScene::SRoomInfo(), pHeadTransformInfo, pHeadTextureTransformer );
/*
	NAnimation::CAddBoneFilter *pFilter = new NAnimation::CAddBoneFilter(12); // CRAP - head bone
	pFilter->pAnimation = pAnimator;

	NLSHead::CHeadAnimator *pHeadAnimator = new NLSHead::CHeadAnimator( pTime, pUnit->GetPers()->pHead->pHead );
	pView->CreateLSHead( pUnit->GetPers()->pHead, pHeadAnimator, pTime, pFilter, false, false );
*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFakeWorldUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFakeWorldUnit: public NWorld::IVisObj
{
	OBJECT_NOCOPY_METHODS(CFakeWorldUnit);
	ZDATA
	int nAnimFlags;
	float fAngle;
	CSyncSrcBind<NWorld::IVisObj> bindGlobal;
	CObj<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<NWorld::CUnit> pUnit;
	CPtr< CFuncBase<STime> > pTime;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nAnimFlags); f.Add(3,&fAngle); f.Add(4,&bindGlobal); f.Add(5,&pAnimator); f.Add(6,&pUnit); f.Add(7,&pTime); return 0; }
	bool bNoAnimationUpdate;	// non-serialized: set by PlayAnimation() to suppress Update's auto-pose reset
public:
	CFakeWorldUnit() {}
	CFakeWorldUnit( CSyncSrc<NWorld::IVisObj> *pSrc, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime );

	virtual void Visit( NWorld::IRenderVisitor *p );
	void Update( float fAngle );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFakeWorldUnit::CFakeWorldUnit( CSyncSrc<NWorld::IVisObj> *pSrc, 
	NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime )
: pUnit( _pUnit ), pTime( _pTime ), fAngle( 0 ), nAnimFlags( -1 ), bNoAnimationUpdate( false )
{
	bindGlobal.Link( pSrc, this );
	Update( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeWorldUnit::Update( float _fAngle )
{
	if ( !IsValid( pUnit ) )
	{
		ASSERT(0);
		return;
	}
	if ( bNoAnimationUpdate )	// a PlayAnimation() override is active -> keep it, don't reset to the auto pose
	{
		bindGlobal.Update();
		return;
	}
	CPtr<NRPG::IInventoryItem> pActiveItem = pUnit->GetRPG()->GetInventoryInfo()->GetActive();
	NDb::EWeaponType eWeaponType = NDb::WT_DEFAULT;
	if ( IsValid( pActiveItem ) )
		eWeaponType = pActiveItem->GetWeaponType();

	int nNewAnimFlags = NDb::CAnimation::POSE_STAND;
	nNewAnimFlags |= NDb::WeaponTypeToAnimFlags( eWeaponType, IsValid( pActiveItem ), false );

	int nAnimFlagsClassSex = NDb::CAnimation::IN_REALTIME;
	nAnimFlagsClassSex |=	pUnit->GetRPG()->GetRPGPers()->bIsFemale? NDb::CAnimation::SEX_FEMALE : NDb::CAnimation::SEX_MALE;

	if ( ( fAngle != _fAngle ) || ( nNewAnimFlags != nAnimFlags ) )
	{
		NDb::CModel* pModel = pUnit->GetModel();
		pAnimator = new NAnimation::CSkeletonAnimator( pModel->pSkeleton );
		pAnimator->pTime = pTime;
		pAnimator->bServer = false;

		NAnimation::CAnimation *pAnimation;
		pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( NDb::CAnimation::POSE, nNewAnimFlags, 0, nAnimFlagsClassSex ), 0, true );
		if ( !pAnimation )
		{
			nNewAnimFlags = NDb::CAnimation::POSE_STAND;
			pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( NDb::CAnimation::POSE, nNewAnimFlags ), 0, true );
		}

		fAngle = _fAngle;
		nAnimFlags = nNewAnimFlags;
		pAnimator->AddAnimator( 0, pAnimation );
		pAnimation->SetStand( 0, CVec3( 0, 0, 0 ), fAngle );
	}

	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeWorldUnit::PlayAnimation( NDb::CAnimation *pAnim, bool bLoop )
{
	if ( !IsValid( pAnim ) || !IsValid( pAnimator ) )
	{
		bNoAnimationUpdate = false;	// release: let Update resume the auto pose
		return;
	}

	STime tNow = pTime->GetValue();
	NAnimation::CAnimation *pAnimation = pAnimator->CreateAnimation( pAnim, tNow, bLoop );
	if ( IsValid( pAnimation ) )
	{
		pAnimator->AddAnimator( tNow, pAnimation );
		bNoAnimationUpdate = true;
	}

	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeWorldUnit::Visit( NWorld::IRenderVisitor *p )
{
	if ( !IsValid( pUnit ) )
	{
		ASSERT(0);
		return;
	}
	vector<NWorld::IRenderVisitor::SBoundMesh> boundMeshes;
	NWorld::GetItemsBindPlaces( &boundMeshes, pUnit->GetRPG(), 0, pUnit->GetWearingDBPK() );
	p->AddMesh( pUnit->GetModel(), pAnimator, 0, boundMeshes, 0, 0, pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCreateSyncSrc
{
	ZDATA
	CObj<CSyncSrc<NWorld::IVisObj> > pShow;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pShow); return 0; }
	SCreateSyncSrc(): pShow( new NWorld::CWorldSyncSrc ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowRPGUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowRPGUnit: public IShowUnit, public SCreateSyncSrc
{
	OBJECT_NOCOPY_METHODS(CShowRPGUnit);
	ZDATA_(SCreateSyncSrc)
	CSetRender r;
	CObj<CFakeRPGUnit> pUnit;	
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(SCreateSyncSrc*)this); f.Add(2,&r); f.Add(3,&pUnit); return 0; }
public:
	CShowRPGUnit() {}
	CShowRPGUnit( NGScene::IGameView *pView, NRPG::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController );
	void Update( float fAngle );
	void SetSequence( NDb::CSequence *pSequence );
	void SetLSHeadParam( const char *szName, float fValue );
	float GetLSHeadParam( const char *szName );
	NLSHead::CHeadInfo* CreateLSHeadInfo();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowRPGUnit::CShowRPGUnit( NGScene::IGameView *pView, NRPG::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController ):
	r( pShow, pView )
{
	r.SetTimer( _pTime, _pTime );
	r.SetHeadsController( pHdController );
	pUnit = new CFakeRPGUnit( pView, pShow, _pUnit, _pTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowRPGUnit::Update( float fAngle )
{
	pUnit->Update( fAngle );
	r.Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowRPGUnit::SetSequence( NDb::CSequence *pSequence )
{
	ASSERT( 0 && "Unsupported!" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Live head-morph forwarders -> the wrapped CFakeRPGUnit (the advanced FaceGen editor drives these
// through pUnitView->pInventoryUnit, which is this IShowUnit).
void CShowRPGUnit::SetLSHeadParam( const char *szName, float fValue )
{
	if ( IsValid( pUnit ) )
		pUnit->SetLSHeadParam( szName, fValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CShowRPGUnit::GetLSHeadParam( const char *szName )
{
	return IsValid( pUnit ) ? pUnit->GetLSHeadParam( szName ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NLSHead::CHeadInfo* CShowRPGUnit::CreateLSHeadInfo()
{
	return IsValid( pUnit ) ? pUnit->CreateLSHeadInfo() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NRPG::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame )
{
	CPtr<NLSHead::CHeadsController> pController;
	if ( IsValid( pRenderGame ) )
		pController = pRenderGame->GetHeadController();
	else
		pController = new NLSHead::CHeadsController;

	return new CShowRPGUnit( pView, pUnit, pTime, pController );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowWorldUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowWorldUnit: public IShowUnit, public SCreateSyncSrc
{
	OBJECT_NOCOPY_METHODS(CShowWorldUnit);
	ZDATA_(SCreateSyncSrc)
	CSetRender r;
	CPtr<NWorld::CUnit> pUnit;	
	CObj<CFakeWorldUnit> pFakeUnit;	
	CPtr<NLSHead::CHeadsController> pController;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(SCreateSyncSrc*)this); f.Add(2,&r); f.Add(3,&pUnit); f.Add(4,&pFakeUnit); f.Add(5,&pController); return 0; }
public:
	CShowWorldUnit() {}
	CShowWorldUnit( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController );
	void Update( float fAngle );
	void SetSequence( NDb::CSequence *pSequence );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowWorldUnit::CShowWorldUnit( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController )
: r( pShow, pView ), pUnit( _pUnit ), pController( pHdController )
{
	r.SetTimer( _pTime, _pTime );
	r.SetHeadsController( pHdController );
	pFakeUnit = new CFakeWorldUnit( pShow, _pUnit, _pTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::Update( float fAngle )
{
	pFakeUnit->Update( fAngle );
	r.Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::SetSequence( NDb::CSequence *pSequence )
{
	pController->PlaySequence( pUnit, pSequence );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::PlayAnimation( NDb::CAnimation *pAnim, bool bLoop )
{
	pFakeUnit->PlayAnimation( pAnim, bLoop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NWorld::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame )
{
	CPtr<NLSHead::CHeadsController> pController;
	if ( IsValid( pRenderGame ) )
		pController = pRenderGame->GetHeadController();
	else
		pController = new NLSHead::CHeadsController;

	return new CShowWorldUnit( pView, pUnit, pTime, pController );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowUnitHead
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowUnitHead: public IShowUnitHead
{
	OBJECT_NOCOPY_METHODS(CShowUnitHead);
	ZDATA
	CPtr<NWorld::CUnit> pUnit;
	CObj<CObjectBase> pRenderNode;
	CPtr<NLSHead::CHeadsController> pController;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pRenderNode); f.Add(4,&pController); return 0; }
public:
	CShowUnitHead() {}
	CShowUnitHead( NGScene::IGameView *pView, NWorld::CUnit *pUnit, NLSHead::CHeadsController *pController, CFuncBase<SFBTransform> *pTransform );

	void SetSequence( NDb::CSequence *pSequence );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowUnitHead::CShowUnitHead( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, NLSHead::CHeadsController *_pController, CFuncBase<SFBTransform> *pTransform ):
	pUnit( _pUnit ), pController( _pController )
{
	NLSHead::CHeadAnimator *pAnimator = pController->GetAnimator( pUnit );
	// Portrait / inventory head: pass the committed hero's baked face texture so it shows the recoloured skin too.
	NLSHead::CHeadInfo *pHI = pUnit->GetHeadInfo();
	CPtrFuncBase<NGfx::CTexture> *pFaceTex = IsValid( pHI ) ? pHI->GetFaceTexture() : 0;
	pRenderNode = pView->CreateLSHead( pUnit->GetDBHead(), pAnimator, pController->GetTime(), pTransform, true, pUnit->GetHeadSeed(), false, NGScene::SRoomInfo(), pFaceTex, pHI );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowUnitHead::SetSequence( NDb::CSequence *pSequence )
{
	pController->PlaySequence( pUnit, pSequence );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IShowUnitHead* CreateShowUnitHead( NGScene::IGameView *pView, NWorld::CUnit *pUnit, NLSHead::CHeadsController *pController, CFuncBase<SFBTransform> *pTransform )
{
	return new CShowUnitHead( pView, pUnit, pController, pTransform );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderGame
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVisibleHolder
{
	ZDATA
	CObj<CSetSyncSrc<NWorld::IVisObj> > pVisible;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pVisible); return 0; }
	
	SVisibleHolder(): pVisible( new CSetSyncSrc<NWorld::IVisObj> ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderGame: public IRenderGame, public SVisibleHolder
{
	OBJECT_BASIC_METHODS(CRenderGame);
	struct SBombSelection
	{
		ZDATA
		CPtr<CObjectBase> pBomb;
		CObj<CObjectBase> pSelection;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pBomb); f.Add(3,&pSelection); return 0; }

		SBombSelection() {}
		SBombSelection( CObjectBase *_pBomb, CObjectBase *_pSelection ) : pBomb(_pBomb), pSelection(_pSelection) {}
	};
	ZDATA_(SVisibleHolder)
		// test sphere visualization
	list< CObj<CObjectBase> > testSpheres;
	
	CPtr<NWorld::IWorld> pWorld;
	CPtr<NGScene::IGameView> pScene;
	CTimeCounter timer;
	CSetRender r, rUnits;
	bool bPrevShowUnits;
	CPtr<NWorld::IPlayer> pPrevViewFrom;
	CObj<NGScene::CGrass> pGrass;
	CObj<NLSHead::CHeadsController> pHeadsController;
	list<SBombSelection> bombSelections;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(SVisibleHolder*)this); f.Add(2,&testSpheres); f.Add(3,&pWorld); f.Add(4,&pScene); f.Add(5,&timer); f.Add(6,&r); f.Add(7,&rUnits); f.Add(8,&bPrevShowUnits); f.Add(9,&pPrevViewFrom); f.Add(10,&pGrass); f.Add(11,&pHeadsController); f.Add(12,&bombSelections); return 0; }
	//
	void UpdateVisible( NWorld::IPlayer *pViewFrom, bool bShowUnits );
public:
	CRenderGame() {}
	CRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene );
	
	CObjectBase* Select( CObjectBase *pSelect, const CVec4 &vColor );
	
	CCTime* GetTime() { return timer.GetTime(); }
	NLSHead::CHeadsController* GetHeadController() const { return pHeadsController; }
	
	void UpdateViewWorld( bool bAdvanceTime, STime currentTime, NWorld::IPlayer *pViewFrom, bool bShowAllUnits );
	void FastUpdate( STime currentTime );
	void ResetTiming();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderGame::CRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene )
: 
r( _pWorld->GetActive(), _pScene ), 
rUnits( _pWorld->GetUnits(), _pScene ), 
pWorld(_pWorld), pScene(_pScene), bPrevShowUnits( true )
{
	r.SetTimer( timer.GetTime(), pWorld->GetAimTime() );
	rUnits.SetTimer( timer.GetTime(), pWorld->GetAimTime() );

	pGrass = new NGScene::CGrass( pWorld->GetAIMap() );
	r.SetGrass( pGrass );
	rUnits.SetGrass( pGrass );

	pHeadsController = new NLSHead::CHeadsController;
	r.SetHeadsController( pHeadsController );
	rUnits.SetHeadsController( pHeadsController );
/*
	if ( pScene != 0 )
	{
		pTerrain = pScene->CreateTerrain( pWorld->GetTerrain()->pInfo, timer.GetTime() );
		r.SetTerrain( pTerrain );
		rUnits.SetTerrain( pTerrain );
		/ *for ( int x = 0; x < 50; ++x )
		{
			for ( int y = 0; y < 50; ++y )
				pScene->AddPointLight( CVec3(0.5f,0.5f,0.5f), CVec3(2 + 11 * x,2 + 11 * y, 6), 8 );
		}* /
		//pScene->AddSpotLight( CVec3(1,1,1), CVec3( 2, 0, 6 ), CVec3( 0, 2, -1 ), 70, 10, NDb::GetTexture(13), -1 );//92) );
	}
*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CRenderGame::Select( CObjectBase *pSelect, const CVec4 &vColor )
{
	CObjectBase* pSelection;

	pSelection =  r.Select( pSelect, vColor );
	if ( pSelection )
		return pSelection;

	pSelection = rUnits.Select( pSelect, vColor );
	if ( pSelection )
		return pSelection;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderGame::ResetTiming()
{
	timer.ResetTiming();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderGame::UpdateVisible( NWorld::IPlayer *pViewFrom, bool bShowUnits )
{
	if ( !bShowUnits )
	{
		if ( bPrevShowUnits != bShowUnits )
			rUnits.SetNewSource( new CSetSyncSrc<NWorld::IVisObj>() );

		bPrevShowUnits = bShowUnits;
		return;
	}

	NWorld::IPlayer::CUnitSet units;
	if ( pViewFrom )
	{
		pViewFrom->GetUnits(&units);
		if ( units.empty() )
			pViewFrom = 0; // show everything for dead players
	}
	if ( pPrevViewFrom != pViewFrom || bPrevShowUnits != bShowUnits )
	{
		if ( pViewFrom )
			rUnits.SetNewSource( 
				new CBoolSyncSrc<NWorld::IVisObj, CIntersectionFunc>( pWorld->GetUnits(), pVisible )  
				);
		else
			rUnits.SetNewSource( pWorld->GetUnits() );
		pPrevViewFrom = pViewFrom;
	}
	if ( pViewFrom )
	{
		list<CPtr<NWorld::CUnit> > res;
		pViewFrom->GetVisible( &res );
		vector<NWorld::IVisObj*> vis;
		for ( list<CPtr<NWorld::CUnit> >::iterator i = res.begin(); i != res.end(); ++i )
		{
			vis.push_back( CDynamicCast<NWorld::IVisObj>( *i ) );
			(*i)->AddVisitableChildren( &vis );
		}
		pViewFrom->GetSounds( &vis );
		list<CPtr<CObjectBase> > resObj;
		pViewFrom->GetVisibleObjects( &resObj );
		for ( list<CPtr<CObjectBase> >::iterator i = resObj.begin(); i != resObj.end(); ++i )
			vis.push_back( CDynamicCast<NWorld::IVisObj>( *i ) );
		pVisible->Set( vis );
		// show bombs
		list<CPtr<CObjectBase> > bombs;
		list<SBombSelection> newBombSelections;
		pViewFrom->GetTrappedObjectsList( &bombs );
		for ( list< CPtr<CObjectBase> >::const_iterator i = bombs.begin(); i != bombs.end(); ++i )
		{
			CObjectBase *pBomb = *i;
			list<SBombSelection>::iterator k;
			for ( k = bombSelections.begin(); k != bombSelections.end(); ++k )
			{
				if ( k->pBomb == pBomb )
					break;
			}
			if ( k == bombSelections.end() )
			{
				CObjectBase *pSelection = Select( pBomb, CVec4(1,1,1,1) );
				if ( pSelection )
					newBombSelections.push_back( SBombSelection( pBomb, pSelection ) );
			}
			else
				newBombSelections.splice( newBombSelections.end(), bombSelections, k );
		}
		bombSelections.swap( newBombSelections );
	}
	bPrevShowUnits = bShowUnits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderGame::FastUpdate( STime currentTime )
{
	UpdateVisible( 0, false );
	timer.Advance( true, currentTime );
	// render them all
	r.Sync();
	rUnits.Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderGame::UpdateViewWorld( bool bAdvanceTime, STime currentTime, NWorld::IPlayer *pViewFrom, bool bShowAllUnits )
{
	if ( bShowAllUnits )
		UpdateVisible( 0, true );//bShowUnits );
	else
		UpdateVisible( pViewFrom, true );//bShowUnits );

	timer.Advance( bAdvanceTime, currentTime );

	pHeadsController->Advance( currentTime );

	STime t = timer.GetTime()->GetValue();
	pWorld->UpdateWorld( t, pViewFrom );
	pGrass->Update( t );

	// test sphere visualization
	testSpheres.clear();
	for ( int i=0; i<sphereParticles.size(); ++i )
	{
		CPtr<CMemObject> pModel = new CMemObject;
		pModel->CreateSphere( sphereParticles[i].ptCenter, sphereParticles[i].fRadius, 1 );
		CVec4 color( 1, 0.3f, 0.3f, 1.0f );
		testSpheres.push_back( pScene->CreateMesh( pModel, color, 0 ) );
	}
	//sphereParticles.clear();
	// render them all
	r.Sync();
	rUnits.Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderGame* CreateRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene )
{
	return new CRenderGame( _pWorld, _pScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRender;
BASIC_REGISTER_CLASS( IShowUnit );
BASIC_REGISTER_CLASS( IRenderGame );
REGISTER_SAVELOAD_CLASS( 0x01941130, CRenderGame );
REGISTER_SAVELOAD_CLASS( 0x01941131, CSelection );
REGISTER_SAVELOAD_CLASS( 0x01941132, CShowWorldUnit );
REGISTER_SAVELOAD_CLASS( 0x01941133, CFakeWorldUnit );
REGISTER_SAVELOAD_CLASS( 0x01941134, CShowUnitHead );
REGISTER_SAVELOAD_CLASS( 0x01941135, CShowRPGUnit );
REGISTER_SAVELOAD_CLASS( 0x01941136, CFakeRPGUnit );
