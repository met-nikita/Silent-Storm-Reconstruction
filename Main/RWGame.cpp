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
#include "..\DBFormat\DataLight.h"	// CAmbientLightReal (SyncWeather sun/rain reference lights)
#include "GMatShare.h"
#include "InventoryUnit.h"
#include "..\MiscDll\Commands.h"      // REGISTER_VAR_EX (v1.2 game_selectionmode / game_showweathereffect)
#include "..\FileIO\BasicChunk1.h"    // START_REGISTER / FINISH_REGISTER
///
#include "GPostProcessors.h"
#include "GGrass.h"
#include "GParticles.h"
#include "GParticleInfo.h"

#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"

#include "LSHead.h"
#include "LSController.h"
#include "Sync.h"
#include "RWSound.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MemObject.h"
vector<SSphere> sphereParticles;	// test sphere visualization
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRender
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FOV = 60;
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2-only config (registrar v1.2 @0x6cff40): game_selectionmode -> int @0x9c70cc (VarIntHandler,
// default 0, saved), game_showweathereffect -> bool @0x97fec8 (VarBoolHandler, default 1, saved).
int nSelectionMode = 0;
static bool bShowWeatherEffect = true;
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
// retail NRender::SSelectionInfo (PDB: 20 bytes, vColor @+0, bIgnoreFloorMask @+16): the visual
// description of a selection highlight. bIgnoreFloorMask makes the scene-side selection node skip
// the per-face floor-mask/frame-skip gates (retail NGScene::CSelection +0x38); the dev scene node
// has no such gate yet, so the flag is carried (save wire + CSelection state) but not consumed.
struct SSelectionInfo
{
	CVec4 vColor;
	bool bIgnoreFloorMask;

	// retail CSelection default ctor @0x2cfa90 seeds ((0,1,1,1), false)
	SSelectionInfo(): vColor( 0, 1, 1, 1 ), bIgnoreFloorMask( false ) {}
	SSelectionInfo( const CVec4 &_vColor, bool _bIgnoreFloorMask ): vColor( _vColor ), bIgnoreFloorMask( _bIgnoreFloorMask ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelection: public CObjectBase
{
	OBJECT_BASIC_METHODS( CSelection );
public:
	ZDATA
	SSelectionInfo selectionInfo;
	// v1.2 widened the node slot: mode 0 stores the scene CSelectionNode, mode 1 the AddPostFilter
	// handle (CreateSelection v1.2 @0x6cbf60 stores either at +0x20)
	CObj<CObjectBase> pSelection;
	// retail operator& @0x2d0600: tag 2 = the WHOLE 20-byte SSelectionInfo as ONE raw chunk
	// (byte-walked retail saves: {2:20 3:4} on every instance), tag 3 = the scene node. The dev
	// used to write only the 16-byte CVec4 -> the x123 all-slots SIZE divergence in the wire
	// audit, and bIgnoreFloorMask was dropped on load.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&selectionInfo); f.Add(3,&pSelection); return 0; }

	CSelection() {}	// retail @0x2cfa90: selectionInfo default-constructs to ((0,1,1,1), false)
	// retail @0x2cfaf0: copies the 20-byte info field-for-field + takes an owning node ref
	CSelection( const SSelectionInfo &_info, CObjectBase *_pSelection ): selectionInfo( _info ), pSelection( _pSelection ) {}
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
	CSelection* CreateSelection( int nID, const SSelectionInfo &info, CSelection *pSource = 0 );
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
	virtual void AddHeadIdleAnimator( NWorld::CUnit *pUnit );
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
	CObjectBase* Select( CObjectBase *pSelect, const SSelectionInfo &info = SSelectionInfo() );
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
// retail @0x2cb440 takes the full SSelectionInfo; on the reuse path it copies the WHOLE 20-byte
// info (colour + floor-mask flag) into the existing CSelection. The scene call still hands over
// only the colour: the dev IGameView::CreateSelection has no bIgnoreFloorMask parameter yet
// (retail scene vtbl+0x40 takes it and stores it at NGScene::CSelection+0x38 for the mask gate).
CSelection* CSetRender::CreateSelection( int nID, const SSelectionInfo &info, CSelection *pSource )
{
	const vector<CObj<CObjectBase> > &ob = GetObjects( nID );
	vector<CObjectBase*> t;
	for ( int k = 0; k < ob.size(); ++k )
		t.push_back( ob[k] );

	// v1.2 @0x6cbf60: game_selectionmode gates the node type -- 2 = no selection visuals at all,
	// 1 = a flat premultiplied post-colorer over the unit's nodes (scene AddPostFilter, vtbl+0x90)
	if ( nSelectionMode == 2 )
		return pSource;
	if ( nSelectionMode == 1 )
	{
		const CVec4 &c = info.vColor;
		CObjectBase *pNode = pScene->AddPostFilter( t,
			new NGScene::CPostColorer( new NGScene::CCVec4( CVec4( c.r * c.a, c.g * c.a, c.b * c.a, 0 ) ) ) );
		if ( pSource )
		{
			pSource->selectionInfo = info;
			pSource->pSelection = pNode;
			return pSource;
		}
		return new CSelection( info, pNode );
	}

	if ( pSource )
	{
		pSource->selectionInfo = info;
		pSource->pSelection = pScene->CreateSelection( t, info.vColor );
		return pSource;
	}
	return new CSelection( info, pScene->CreateSelection( t, info.vColor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSetRender::PostVisit( int nID, NWorld::IVisObj *pObject )
{
	if ( nID >= objects.size() )
		objects.resize( nID + 1 );
	objects[nID] = pObject;
	CSelectionHash::iterator i = selections.find( pObject );
	if ( i != selections.end() && IsValid( i->second ) )
		CreateSelection( nID, i->second->selectionInfo, i->second );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CSetRender::Select( CObjectBase *pSelect, const SSelectionInfo &info )
{
	CSelectionHash::iterator iTemp = selections.find( pSelect );
	if ( iTemp != selections.end() )
	{
		if ( IsValid( iTemp->second ) && iTemp->second->selectionInfo.vColor == info.vColor )
			return iTemp->second;
	}
	for ( int k = 0; k < objects.size(); ++k )
	{
		if ( objects[k] == pSelect )
		{
			CSelection *pRes = CreateSelection( k, info );
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

	// retail 4-arg AddHead @0x2cda60: records are keyed by the unit's per-unit CHeadInfo (the render
	// side resolves it -- CFakeWorldUnit::GetHeadInfo @0x2cf9c0); a null animator (no CHead record)
	// still falls through so the hair / face meshes render (retail @0x2cd7f0 -> @0x188c90 skips only
	// the head-skin part).
	NLSHead::CHeadInfo *pHI = pUnit->GetHeadInfo();
	NLSHead::CHeadAnimator *pAnimator = pHeadsController->GetAnimator( pHI );

	bool bHasCap = pUnit->IsCapPresent();
	// A committed advanced-FaceGen hero carries a baked face texture (CHeadInfo::pTexture); pass it so the head
	// skin renders the recoloured texture instead of the base DB material.
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
	// retail: the editor preview head plays the ambient facial idles too (CHeadTransformInfo ctor
	// @0x2606e0 arms bPlayIdle; the dev morph rides CHeadAnimator, whose idle machine keys on
	// eIdleType) -- without this the AdvFaceGen head sits frozen between slider drags.
	pAnimator->SetIdleType( NLSHead::IDLE_NORMAL );

	bool bHasCap = false;
	// pFaceTexture is the caller's PERSISTENT live texture-preview node (CFakeRPGUnit owns one across re-Visits,
	// which happen on every slider move / rotation) -- the head material samples it so the preview retints live.
	Register( pScene->CreateLSHead( pHead, pAnimator, pTime, pPosition, false, SRandomSeed( pHead->GetRecordID() ), bHasCap, room, pFaceTexture ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2cda30: fetch-or-create the head's ambient-idle token (CHeadsController::PlayIdle) and
// register it into this visit's objects, so the token -- and with it the animator's IDLE_NORMAL state
// and the armed blink sequences -- lives exactly as long as some view renders the head. A null return
// (no animator record: the unit has no head) registers nothing.
void CSetRender::AddHeadIdleAnimator( NWorld::CUnit *pUnit )
{
	if ( !IsValid( pHeadsController ) )
		return;
	// retail keys the record by the unit's CHeadInfo (controller PlayIdle @0x25dda0 takes CHeadInfo*).
	CObjectBase *pIdler = pHeadsController->PlayIdle( pUnit->GetHeadInfo() );
	if ( pIdler )
		RegisterBase( pIdler );
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
// retail @0x2cb380: the angle-change branch is the ONLY body -- re-stand the EXISTING animation at
// the new angle and re-front the sync bind so the scene re-Visits ONCE. (dev previously re-fronted
// UNCONDITIONALLY every frame -> the FaceGen doll's mesh was destroyed+recreated per frame.)
void CFakeRPGUnit::Update( float _fAngle )
{
	if ( fAngle != _fAngle && IsValid( pAnimation ) )
	{
		fAngle = _fAngle;
		pAnimation->SetStand( 0, CVec3( 0, 0, 0 ), fAngle );
		bindGlobal.Update();
	}
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
	// Member set + order + tags = retail CFakeWorldUnit (PDB layout 0x4c bytes; operator&
	// @0x2d3560). Byte-walked retail saves carry exactly
	//   {2:4 3:1 4:1 5:4 7:12 8:4 9:4 10:4 11:4 12:1 13:1 14:1 15:4 16:1}
	// -- note there is NO tag 6 (retail skips it). The old dev wire {2 nAnimFlags, 3 fAngle,
	// 4 bindGlobal, 5 pAnimator, 6 pUnit, 7 pTime} misread every retail chunk from tag 3 on
	// (the x26 SIZE/RAWSZ/UNREAD/MISS block in the wire audit).
	ZDATA
	int nAnimFlags;											// tag 2
	// bItems  -- pose with the active item's weapon anim flags (retail Update @0x2cc1a0 gates the
	//            weapon read on it; false = the bare-stand body pose the HUD face uses)
	// bPlayIdle -- stand animation type = INTERFACE_IDLE instead of POSE (retail @0x2cbe30)
	bool bItems;											// tag 3
	bool bPlayIdle;											// tag 4
	float fAngle;											// tag 5
	CSyncSrcBind<NWorld::IVisObj> bindGlobal;				// tag 7 (subtree {2 pSync, 3 nID} = 12B)
	CObj<NAnimation::CSkeletonAnimator> pAnimator;			// tag 8
	CPtr<NWorld::CUnit> pUnit;								// tag 9
	CDGPtr< CFuncBase<STime> > pTime;						// tag 10 (retail CDGPtr; wire = the owning CObj node)
	// Live head-morph state (retail +0x2c, CPtr like retail): built by the ctor for a non-static,
	// transformable complex head; driven by Set/GetLSHeadParam (retail @0x2cb260/@0x2cb240).
	CPtr<NLSHead::CHeadTransformInfo> pHeadTransformInfo;	// tag 11
	// bCanFight/bInPK -- the unit's live fight/PK state cached by CreateAnimation's commit
	// (retail @0x2cbe30); joining the Update change-gate makes a downed/mounted unit re-pose.
	bool bCanFight;											// tag 12
	bool bShowCap;											// tag 13 (Visit passes !bShowCap as GetItemsBindPlaces' bNoCap)
	bool bInPK;												// tag 14
	// expiry of the current stand/idle clip (retail @0x2cbe30 tail: sEndTime = anim->tLength +
	// curTime). Update re-rolls the weighted-random clip ONLY when now passes it.
	STime sEndTime;											// tag 15
	bool bNoAnimationUpdate;								// tag 16 (set by PlayAnimation() to suppress Update's auto-pose reset)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nAnimFlags); f.Add(3,&bItems); f.Add(4,&bPlayIdle); f.Add(5,&fAngle); f.Add(7,&bindGlobal); f.Add(8,&pAnimator); f.Add(9,&pUnit); f.Add(10,&pTime); f.Add(11,&pHeadTransformInfo); f.Add(12,&bCanFight); f.Add(13,&bShowCap); f.Add(14,&bInPK); f.Add(15,&sEndTime); f.Add(16,&bNoAnimationUpdate); return 0; }
	//
	void CreateAnimation( int nNewAnimFlags, int nAnimFlagsClassSex, float _fAngle, bool _bCanFight, bool _bInPK );
public:
	CFakeWorldUnit(): nAnimFlags( -1 ), bItems( true ), bPlayIdle( false ), fAngle( 0 ),
		bCanFight( true ), bShowCap( true ), bInPK( false ), sEndTime( 0 ), bNoAnimationUpdate( false ) {}
	CFakeWorldUnit( CSyncSrc<NWorld::IVisObj> *pSrc, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime,
		bool _bItems, bool _bPlayIdle, bool _bShowCap );

	virtual void Visit( NWorld::IRenderVisitor *p );
	void Update( float fAngle );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );

	// Live head-morph accessors (retail @0x2cb240/@0x2cb260/@0x2cb280) -- same shape as the
	// CFakeRPGUnit triple; CShowWorldUnit forwards its IShowUnit virtuals here.
	float GetLSHeadParam( const char *szName );
	void SetLSHeadParam( const char *szName, float fValue );
	NLSHead::CHeadInfo* CreateLSHeadInfo();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2ce440: __thiscall(pSrc, pUnit, pTime, bool bItems, bool bPlayIdle, bool bShowCap).
// Seeds nAnimFlags=-1, bCanFight=true, bInPK=false, bNoAnimationUpdate=false, then lazily builds
// the live head-morph state, links the sync source and kicks the pose. (Retail leaves sEndTime
// UNINITIALIZED here -- the guaranteed nAnimFlags mismatch short-circuits the change-gate before
// it is read; the dev zero-init keeps that behaviour while staying deterministic.)
CFakeWorldUnit::CFakeWorldUnit( CSyncSrc<NWorld::IVisObj> *pSrc,
	NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime, bool _bItems, bool _bPlayIdle, bool _bShowCap )
: pUnit( _pUnit ), pTime( _pTime ), fAngle( 0 ), nAnimFlags( -1 ), bNoAnimationUpdate( false ),
	bItems( _bItems ), bPlayIdle( _bPlayIdle ), bShowCap( _bShowCap ),
	bCanFight( true ), bInPK( false ), sEndTime( 0 )
{
	// retail @0x2ce440 tail: a NON-static head whose complex-head record carries a valid,
	// transformable NDb::CHead gets a live morph state over (complex head, time source).
	NLSHead::CHeadInfo *pHI = pUnit->GetHeadInfo();
	NDb::CComplexHead *pCH = pHI ? pHI->GetHead() : 0;
	if ( ( !pHI || !pHI->IsStaticHead() ) && IsValid( pCH ) && IsValid( pCH->pHead ) && pCH->pHead->isTransformable )
		pHeadTransformInfo = new NLSHead::CHeadTransformInfo( pCH, pTime );

	bindGlobal.Link( pSrc, this );
	Update( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail Update @0x2cc1a0: re-evaluate the pose key + the unit's live fight/PK state and rebuild
// via CreateAnimation only on a material change OR when the current clip EXPIRES (`sEndTime <
// now`). The expiry re-roll is what rotates the weighted INTERFACE_IDLE clips: CSkeleton::
// GetAnimation random-picks per call (roulette over fRndWeight, DataFormat.cpp), so it must run
// ONCE per clip lifetime -- never per frame, or the face flip-flops between clips.
// NOTE: retail does NOT re-front bindGlobal here -- the sync update lives in CreateAnimation's
// commit tail (@0x2cbe30), firing only when the animator was actually replaced. PlayAnimation
// needs none at all (it schedules onto the EXISTING animator the scene already holds).
void CFakeWorldUnit::Update( float _fAngle )
{
	if ( !IsValid( pUnit ) )	// retail @0x2cc1a0: silent guard (null or finalizing unit)
		return;

	pTime.Refresh();	// retail: once-per-frame CDGPtr refresh, then the frame-cached value
	STime tNow = pTime->GetValue();

	CPtr<NRPG::IInventoryItem> pActiveItem = pUnit->GetRPG()->GetInventoryInfo()->GetActive();
	NDb::EWeaponType eWeaponType = NDb::WT_DEFAULT;
	// retail Update @0x2cc1a0: the active item's weapon shapes the pose only in item mode (bItems);
	// in body mode (the HUD face) the flags degrade to the bare-stand key.
	const bool bHaveItem = bItems && IsValid( pActiveItem );
	if ( bHaveItem )
		eWeaponType = pActiveItem->GetWeaponType();

	int nNewAnimFlags = NDb::CAnimation::POSE_STAND;
	nNewAnimFlags |= NDb::WeaponTypeToAnimFlags( eWeaponType, bHaveItem, false );

	int nAnimFlagsClassSex = NDb::CAnimation::IN_REALTIME;
	nAnimFlagsClassSex |=	pUnit->GetRPG()->GetRPGPers()->bIsFemale? NDb::CAnimation::SEX_FEMALE : NDb::CAnimation::SEX_MALE;

	// retail @0x2cc1a0: the unit's live fight/PK state joins the change gate, so a unit going
	// down (or into/out of a Panzerklein) re-poses even though the weapon key is unchanged.
	const bool bFight = pUnit->CanFight();
	const bool bPK = IsValid( pUnit->GetWearingDBPK() );

	if ( !bNoAnimationUpdate	// a PlayAnimation() override is active -> keep it, don't reset to the auto pose
		&& ( ( fAngle != _fAngle ) || ( nNewAnimFlags != nAnimFlags )
			|| ( bCanFight != bFight ) || ( bInPK != bPK ) || ( sEndTime < tNow ) ) )
	{
		CreateAnimation( nNewAnimFlags, nAnimFlagsClassSex, _fAngle, bFight, bPK );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2cbe30: __thiscall(nAnimFlags, nAnimFlagsClassSex, fAngle, bCanFight, bInPK). Builds a
// FRESH CSkeletonAnimator and schedules the best matching clip:
//   1. a unit that can no longer fight plays a fixed DB clip first (disasm @0x6cbf3b): animation
//      4493 (0x118d) on the fight->downed TRANSITION (the stored bCanFight is still true), 4514
//      (0x11a2) while it stays downed; non-looped, so the fall plays once and freezes.
//   2. else/on failure the three-key fallback chain (EType,flags,classSex) -> (EType,POSE_STAND)
//      -> (POSE,POSE_STAND), EType = bPlayIdle ? INTERFACE_IDLE : POSE (the last leg retries
//      unconditionally in retail, a harmless repeat when EType == POSE). The last leg rescues
//      units whose skeleton has no InterfaceIdle clips (only skeleton 8 -- the human rig --
//      carries them; a PK-mounted unit falls back to its static stand pose).
//   3. commit ONLY on success: cache bInPK/bCanFight/fAngle + the REQUESTED flags (retail stores
//      param_1 no matter which fallback leg supplied the clip -- the JITTER FIX: storing the
//      DEGRADED key made the change-gate mismatch every frame -> per-frame roulette re-roll),
//      schedule at tNow, sEndTime = tLength + tNow, clear bNoAnimationUpdate, and re-front the
//      sync source so the scene re-Visits the REPLACED animator.
// Every clip starts at tNow (retail passes curTime into CreateAnimation/AddAnimator/SetStand), so
// an expiry re-roll begins at the clip's first key instead of a mid-phase snap.
void CFakeWorldUnit::CreateAnimation( int nNewAnimFlags, int nAnimFlagsClassSex, float _fAngle, bool _bCanFight, bool _bInPK )
{
	pTime.Refresh();
	STime tNow = pTime->GetValue();

	// retail @0x2cbe30: body mode reads the RPG unit's pers model (NRPG::CUnit+0x6c) -- a PK-wearing
	// pilot shows the HUMAN in dialog/portrait views; item mode reads the live world model
	NDb::CModel* pModel = bItems ? pUnit->GetModel() : pUnit->GetRPG()->GetModel();
	pAnimator = new NAnimation::CSkeletonAnimator( pModel->pSkeleton );
	pAnimator->pTime = pTime;
	pAnimator->bServer = false;

	const NDb::CAnimation::EType eType = bPlayIdle ? NDb::CAnimation::INTERFACE_IDLE : NDb::CAnimation::POSE;

	NAnimation::CAnimation *pAnimation = 0;
	if ( !_bCanFight )
		pAnimation = pAnimator->CreateAnimation( NDb::GetAnimation( bCanFight ? 0x118d : 0x11a2 ), tNow, false );
	if ( !IsValid( pAnimation ) )
		pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( eType, nNewAnimFlags, 0, nAnimFlagsClassSex ), tNow, true );
	if ( !IsValid( pAnimation ) )
		pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( eType, NDb::CAnimation::POSE_STAND ), tNow, true );
	if ( !IsValid( pAnimation ) )
		pAnimation = pAnimator->CreateAnimation( pModel->pSkeleton->GetAnimation( NDb::CAnimation::POSE, NDb::CAnimation::POSE_STAND ), tNow, true );

	if ( IsValid( pAnimation ) )
	{
		bInPK = _bInPK;
		bCanFight = _bCanFight;
		fAngle = _fAngle;
		nAnimFlags = nNewAnimFlags;
		pAnimator->AddAnimator( tNow, pAnimation );
		pAnimation->SetStand( tNow, CVec3( 0, 0, 0 ), fAngle );
		sEndTime = pAnimation->GetTime() + tNow;	// retail @0x2cbe30: sEndTime = tLength + curTime
		bNoAnimationUpdate = false;
		bindGlobal.Update();	// retail tail: re-front in the sync source ONLY on a rebuild
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2cb860: schedule a custom clip onto the EXISTING animator (the scene already holds
// it, so no sync re-front is needed -- retail has none here). On success the clip's expiry is
// recorded and bCanFight is forced true so the next auto-pose change-gate re-fires cleanly.
// (The IsValid(pAnimator) leg is a dev guard: retail trusts pAnimator, but a default-constructed
// dev instance can reach here before its first Update.)
void CFakeWorldUnit::PlayAnimation( NDb::CAnimation *pAnim, bool bLoop )
{
	if ( !IsValid( pAnim ) || !IsValid( pAnimator ) )
	{
		bNoAnimationUpdate = false;	// release: let Update resume the auto pose
		return;
	}

	pTime.Refresh();
	STime tNow = pTime->GetValue();
	NAnimation::CAnimation *pAnimation = pAnimator->CreateAnimation( pAnim, tNow, bLoop );
	if ( IsValid( pAnimation ) )
	{
		pAnimator->AddAnimator( tNow, pAnimation );
		sEndTime = pAnimation->GetTime() + tNow;	// retail @0x2cb860 tail: tLength + curTime
		bCanFight = true;
		bNoAnimationUpdate = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Live head-morph accessors: retail @0x2cb240 / @0x2cb260 / @0x2cb280 -- IsValid-guarded calls
// into the CHeadTransformInfo built by the ctor (same triple as CFakeRPGUnit's).
float CFakeWorldUnit::GetLSHeadParam( const char *szName )
{
	if ( IsValid( pHeadTransformInfo ) )
		return pHeadTransformInfo->GetMMTension( szName );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFakeWorldUnit::SetLSHeadParam( const char *szName, float fValue )
{
	if ( IsValid( pHeadTransformInfo ) )
		pHeadTransformInfo->SetMMTension( szName, fValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NLSHead::CHeadInfo* CFakeWorldUnit::CreateLSHeadInfo()
{
	if ( IsValid( pHeadTransformInfo ) )
		return pHeadTransformInfo->CreateHeadInfo();
	return 0;
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
	NWorld::GetItemsBindPlaces( &boundMeshes, pUnit->GetRPG(), 0, pUnit->GetWearingDBPK(), false,
		!bShowCap, !bItems );
	// retail @0x2cc650: showing an ITEM pose while the unit wears a HEADLESS Panzerklein suppresses
	// the live head (the head sits inside the shell) -- the head info is dropped before AddMesh.
	NWorld::CUnit *pHeadUnit = pUnit;
	NDb::CPanzerklein *pDBPK = pUnit->GetWearingDBPK();
	if ( bItems && IsValid( pDBPK ) && pDBPK->bHasNoHead )
		pHeadUnit = 0;
	// retail: the body pose (dialog/portrait) renders the PERS model; only the item pose renders
	// the world model (the PK shell when piloted)
	NDb::CModel *pMeshModel = bItems ? pUnit->GetModel() : pUnit->GetRPG()->GetModel();
	p->AddMesh( pMeshModel, pAnimator, 0, boundMeshes, 0, 0, pHeadUnit );
	// release @0x2cc650 tail: unconditionally arm the ambient facial idle (blink) token for the shown
	// head, AFTER AddMesh's internal AddHead created the animator record. This is what makes the
	// mission-HUD face (and every other shown world unit) blink between spoken sequences.
	p->AddHeadIdleAnimator( pUnit );
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
	void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 );
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
void CShowRPGUnit::SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression )
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
	{
		// retail @0x2ce730: a valid render game REPLACES the caller's clock with the mission
		// render clock (vtbl+0x14 GetTime) before taking its head controller -- post-load this is
		// what keeps a show unit animating (the caller's private deserialized CCTime is dead).
		pTime = pRenderGame->GetTime();
		pController = pRenderGame->GetHeadController();
	}
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
	CShowWorldUnit( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController,
		bool bItems = true, bool bPlayIdle = false, bool bShowCap = true );
	void Update( float fAngle );
	void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );
	// retail @0x2cb320 / @0x2cb2f0 / @0x2d1ff0: the live head-morph IShowUnit virtuals reach the
	// wrapped CFakeWorldUnit's pHeadTransformInfo (retail reads pFakeUnit+0x2c directly; the dev
	// forwards through the accessor triple, same as the CShowRPGUnit sibling).
	void SetLSHeadParam( const char *szName, float fValue );
	float GetLSHeadParam( const char *szName );
	NLSHead::CHeadInfo* CreateLSHeadInfo();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2ce850: ctor(view, unit, time, controller, b1, b2, b3) forwards the three bools verbatim
// into the CFakeWorldUnit ctor (@0x2ce440: pushes syncsrc, unit, time, b1, b2, b3, 1).
CShowWorldUnit::CShowWorldUnit( NGScene::IGameView *pView, NWorld::CUnit *_pUnit, CFuncBase<STime>* _pTime, NLSHead::CHeadsController *pHdController,
	bool bItems, bool bPlayIdle, bool bShowCap )
: r( pShow, pView ), pUnit( _pUnit ), pController( pHdController )
{
	r.SetTimer( _pTime, _pTime );
	r.SetHeadsController( pHdController );
	pFakeUnit = new CFakeWorldUnit( pShow, _pUnit, _pTime, bItems, bPlayIdle, bShowCap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::Update( float fAngle )
{
	pFakeUnit->Update( fAngle );
	r.Sync();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression )
{
	// retail @0x2cb110: resolve the unit's per-unit CHeadInfo (NRPG::CUnit +0x90) for the controller key.
	pController->PlaySequence( pUnit->GetHeadInfo(), pSequence, pExpression );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::PlayAnimation( NDb::CAnimation *pAnim, bool bLoop )
{
	pFakeUnit->PlayAnimation( pAnim, bLoop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowWorldUnit::SetLSHeadParam( const char *szName, float fValue )
{
	if ( IsValid( pFakeUnit ) )
		pFakeUnit->SetLSHeadParam( szName, fValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CShowWorldUnit::GetLSHeadParam( const char *szName )
{
	return IsValid( pFakeUnit ) ? pFakeUnit->GetLSHeadParam( szName ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NLSHead::CHeadInfo* CShowWorldUnit::CreateLSHeadInfo()
{
	return IsValid( pFakeUnit ) ? pFakeUnit->CreateLSHeadInfo() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2ce9c0: CreateShowUnit(view, unit, time, renderGame, b1=bItems, b2=bPlayIdle, b3=bShowCap).
// A valid render game REPLACES the passed pTime with pRenderGame->GetTime() (vtbl+0x14, before
// GetHeadController) -- the show unit rides the MISSION render clock, not the caller's widget timer.
// [The old "KNOWN DELTA" pass-through was the post-load doll killer: after a load the CUnitView's
// deserialized private CCTime is not the live clock, so the rebuilt animator evaluated dead time and
// the doll rendered black. Retail's swap makes every show unit follow the restored, live clock.]
IShowUnit* CreateShowUnit( NGScene::IGameView *pView, NWorld::CUnit *pUnit, CFuncBase<STime>* pTime, IRenderGame *pRenderGame,
	bool bItems, bool bPlayIdle, bool bShowCap )
{
	CPtr<NLSHead::CHeadsController> pController;
	if ( IsValid( pRenderGame ) )
	{
		pTime = pRenderGame->GetTime();
		pController = pRenderGame->GetHeadController();
	}
	else
		pController = new NLSHead::CHeadsController;

	return new CShowWorldUnit( pView, pUnit, pTime, pController, bItems, bPlayIdle, bShowCap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderGame
////////////////////////////////////////////////////////////////////////////////////////////////////
// Weather-light helpers (retail NRender free functions, RWGame.obj).
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CopyLight @0x2cb940: clone a light record via MakeCopy, then replace the shallow-shared
// GF2 sub-light with its own copy (so the clone can be mutated independently).
static NDb::CAmbientLightReal* CopyLight( NDb::CAmbientLightReal *p )
{
	NDb::CAmbientLightReal *pRes = p->Duplicate();	// public OBJECT-macro clone (MakeCopy is protected)
	if ( IsValid( p->pGF2Light ) )
		pRes->pGF2Light = p->pGF2Light->Duplicate();
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail MakeRainLight @0x2cad20: darken a light into its overcast variant IN PLACE --
// ambient += 0.5 * diffuse, diffuse *= 0.25 (null-safe, retail checks too).
static void MakeRainLight( NDb::CAmbientLightReal *p )
{
	if ( !p )
		return;
	p->vAmbientColor += p->vLightColor * 0.5f;
	p->vLightColor *= 0.25f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail Blend @0x2cada0: dst = f*a + (1-f)*b elementwise over the ambient + diffuse colours
// (only those two triples; disasm touches offsets 0xc..0x20 exclusively).
static void Blend( NDb::CAmbientLightReal *pDst, const NDb::CAmbientLightReal *pA, const NDb::CAmbientLightReal *pB, float f )
{
	pDst->vAmbientColor = pA->vAmbientColor * f + pB->vAmbientColor * ( 1.0f - f );
	pDst->vLightColor   = pA->vLightColor   * f + pB->vLightColor   * ( 1.0f - f );
}
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
	// retail CRenderGame weather state (PDB +0xf0..+0x108; operator& @0x2d5110 tags 13/14/17/18/19):
	//   wasWeather      -- the last IWorld::GetWeather() seen (SUNNY=0/SNOW=1/RAIN=2)
	//   pWeatherEffect  -- the live precipitation renderable (rain/snow particles)
	//   pSunLight       -- the scene's reference ambient light, latched lazily by SyncWeather
	//   pRainLight      -- MakeRainLight'd copy of pSunLight (the overcast destination light)
	//   tWeatherChange  -- world time the current 1500ms light cross-fade started (0 = idle)
	NWorld::IWorld::EWeather wasWeather;
	CObj<CObjectBase> pWeatherEffect;
	// retail @0x2ceae0: the two sound mixers live IN CRenderGame -- pSound over the always-on
	// GetActive() (world/misc sounds), pUnitSounds over GetUnits() (unit-emitted sounds --
	// CDumbUnitServer::AttachMiscObject attaches voice/footstep C3DSounds to GetUnits), so
	// UpdateVisible can re-point pUnitSounds at the same visibility-filtered source as rUnits.
	CObj<NRender::IRenderSound> pSound;
	CObj<NRender::IRenderSound> pUnitSounds;
	CObj<NDb::CAmbientLightReal> pSunLight;
	CObj<NDb::CAmbientLightReal> pRainLight;
	STime tWeatherChange;
	// retail operator& @0x2d5110: {1 base, 2 testSpheres, 3 pWorld, 4 pScene, 5 timer, 6 r,
	// 7 rUnits, 8 bPrevShowUnits, 9 pPrevViewFrom, 10 pGrass, 11 pHeadsController,
	// 12 bombSelections, 13 wasWeather, 14 pWeatherEffect, 15 pSound, 16 pUnitSounds,
	// 17 pSunLight, 18 pRainLight, 19 tWeatherChange} -- the audit's UNREAD 13/14/17/18/19.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(SVisibleHolder*)this); f.Add(2,&testSpheres); f.Add(3,&pWorld); f.Add(4,&pScene); f.Add(5,&timer); f.Add(6,&r); f.Add(7,&rUnits); f.Add(8,&bPrevShowUnits); f.Add(9,&pPrevViewFrom); f.Add(10,&pGrass); f.Add(11,&pHeadsController); f.Add(12,&bombSelections); f.Add(13,&wasWeather); f.Add(14,&pWeatherEffect); f.Add(15,&pSound); f.Add(16,&pUnitSounds); f.Add(17,&pSunLight); f.Add(18,&pRainLight); f.Add(19,&tWeatherChange); return 0; }
	//
	void UpdateVisible( NWorld::IPlayer *pViewFrom, bool bShowUnits );
	void SyncWeather();
public:
	CRenderGame(): wasWeather( NWorld::IWorld::WEATHER_SUNNY ), tWeatherChange( 0 ) {}	// retail default ctor zeroes the weather state
	CRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene, NSound::ISoundScene *_pSoundScene );

	CObjectBase* Select( CObjectBase *pSelect, const CVec4 &vColor, bool bIgnoreFloorMask = false );

	CCTime* GetTime() { return timer.GetTime(); }
	NLSHead::CHeadsController* GetHeadController() const { return pHeadsController; }

	void UpdateViewWorld( bool bAdvanceTime, STime currentTime, NWorld::IPlayer *pViewFrom, bool bShowAllUnits );
	void FastUpdate( STime currentTime );
	void ResetTiming();
	void UpdateSound( CTransformStack *pTS, STime currentTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderGame::CRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene, NSound::ISoundScene *_pSoundScene )
:
r( _pWorld->GetActive(), _pScene ),
rUnits( _pWorld->GetUnits(), _pScene ),
pWorld(_pWorld), pScene(_pScene), bPrevShowUnits( true ),
wasWeather( NWorld::IWorld::WEATHER_SUNNY ), tWeatherChange( 0 )
{
	r.SetTimer( timer.GetTime(), pWorld->GetAimTime() );
	rUnits.SetTimer( timer.GetTime(), pWorld->GetAimTime() );

	pGrass = new NGScene::CGrass( pWorld->GetAIMap() );
	r.SetGrass( pGrass );
	rUnits.SetGrass( pGrass );

	pHeadsController = new NLSHead::CHeadsController;
	r.SetHeadsController( pHeadsController );
	rUnits.SetHeadsController( pHeadsController );

	// retail @0x2ceae0 tail: two mixers, sources mirroring r/rUnits (GetActive / GetUnits)
	if ( _pSoundScene )
	{
		pSound = CreateRenderSound( pWorld->GetActive(), _pSoundScene );
		pUnitSounds = CreateRenderSound( pWorld->GetUnits(), _pSoundScene );
	}
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
// retail @0x2cc850 packs colour + floor-mask flag into an SSelectionInfo and probes the UNITS set
// first (rUnits.Find, r.Find, rUnits.New, r.New). The dev CSetRender::Select folds Find+New per
// set, so the retail order maps onto rUnits-then-r here.
CObjectBase* CRenderGame::Select( CObjectBase *pSelect, const CVec4 &vColor, bool bIgnoreFloorMask )
{
	SSelectionInfo info( vColor, bIgnoreFloorMask );
	CObjectBase* pSelection;

	pSelection = rUnits.Select( pSelect, info );
	if ( pSelection )
		return pSelection;

	pSelection = r.Select( pSelect, info );
	if ( pSelection )
		return pSelection;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2cb190: the mixers' private timers reset with the render timer
void CRenderGame::ResetTiming()
{
	timer.ResetTiming();
	if ( IsValid( pSound ) )
		pSound->ResetTiming();
	if ( IsValid( pUnitSounds ) )
		pUnitSounds->ResetTiming();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2cb1c0
void CRenderGame::UpdateSound( CTransformStack *pTS, STime currentTime )
{
	if ( IsValid( pSound ) )
		pSound->Update( pTS, currentTime );
	if ( IsValid( pUnitSounds ) )
		pUnitSounds->Update( pTS, currentTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2cb9b0: drive the day's weather lighting + precipitation effect. Called from the
// UpdateViewWorld tail (@0x2cf620, after both Syncs).
//   1. lazy one-time init: pSunLight = the scene's current ambient (scene vtbl+0xa0 = the
//      pPrevLight SetAmbient latched); pRainLight = MakeRainLight(CopyLight(sun)) with the GF2
//      sub-light darkened too (disasm 0x6cba3f: MakeRainLight on the copy AND on copy->pGF2Light).
//   2. weather unchanged + a cross-fade running (tWeatherChange != 0): elapsed >= 1500ms locks in
//      the destination light (rain for SNOW/RAIN else sun) and stops the fade; otherwise
//      SetAmbient(Blend(rain, sun, elapsed/1500)) -- inverted to 1-f when CLEARING so the rain
//      light fades out (Blend weights its FIRST light by f; disasm 0x6cbadf: EAX=pRainLight).
//   3. weather changed: stamp tWeatherChange/wasWeather and rebuild the precipitation effect.
//      Retail (disasm 0x6cbb5f): RAIN -> pScene->CreateRain( NDb::GetParticleInstance(3407),
//      GetTime(), new CParticleFilter( pWorld->GetHeightLayers(1) ) ); SNOW -> pScene->
//      CreateParticles( NDb::GetTEffect(732)->GetEffect(&rnd), identity place, GetTime(), filter ).
//      The dev scene has no CreateRain/CParticleFilter/height-layer plumbing yet, so the effect
//      cannot be BUILT here; pWeatherEffect still round-trips the save wire (tag 14).
// v1.2 @0x6cbb90 restructured step 3: ANY weather change just drops the effect + stamps, and a
// trailing reconcile block (@0x6cbd73) compares IsValid(pWeatherEffect) against the new
// game_showweathereffect toggle -- build for RAIN/SNOW when on, drop otherwise. The build arm is
// blocked on the same missing scene factories; the drop arm is live.
void CRenderGame::SyncWeather()
{
	if ( !IsValid( pSunLight ) )
	{
		pSunLight = pScene->GetPrevLight();
		if ( IsValid( pSunLight ) )
		{
			pRainLight = CopyLight( pSunLight );
			MakeRainLight( pRainLight );
			MakeRainLight( pRainLight->pGF2Light );
		}
	}

	NWorld::IWorld::EWeather eWeather = pWorld->GetWeather();
	if ( wasWeather == eWeather )
	{
		if ( tWeatherChange != 0 && IsValid( pSunLight ) )
		{
			int nElapsed = (int)( timer.GetTime()->GetValue() - tWeatherChange );
			const bool bWet = eWeather == NWorld::IWorld::WEATHER_SNOW || eWeather == NWorld::IWorld::WEATHER_RAIN;
			if ( nElapsed >= 1500 )
			{
				// transition complete: lock in the destination light, stop the fade
				NDb::CAmbientLightReal *pFinal = pSunLight;
				if ( bWet )
					pFinal = pRainLight;
				pScene->SetAmbient( pFinal );
				tWeatherChange = 0;	// v1.2: falls through to the reconcile tail (no early return)
			}
			else
			{
				if ( nElapsed < 0 )
					nElapsed = 0;
				float fFactor = nElapsed * ( 1.0f / 1500.0f );	// retail const 0x3a2ec33e
				if ( !bWet )
					fFactor = 1.0f - fFactor;	// clearing: the rain light fades OUT
				CPtr<NDb::CAmbientLightReal> pWorking = CopyLight( pSunLight );
				Blend( pWorking, pRainLight, pSunLight, fFactor );
				if ( IsValid( pWorking->pGF2Light ) )
					Blend( pWorking->pGF2Light, pRainLight->pGF2Light, pSunLight->pGF2Light, fFactor );
				pScene->SetAmbient( pWorking );
			}
		}
	}
	else
	{
		// v1.2 @0x6cbb90: ANY weather change drops the effect + stamps; the tail rebuilds it
		wasWeather = eWeather;
		pWeatherEffect = 0;
		tWeatherChange = timer.GetTime()->GetValue();
	}

	// v1.2 reconcile tail @0x6cbd73: keep the live effect in step with game_showweathereffect --
	// build it for RAIN/SNOW when the toggle is on, drop it otherwise
	bool bHaveEffect = IsValid( pWeatherEffect );
	if ( bHaveEffect != bShowWeatherEffect )
	{
		if ( ( eWeather == NWorld::IWorld::WEATHER_RAIN || eWeather == NWorld::IWorld::WEATHER_SNOW ) && bShowWeatherEffect )
		{
			// retail builds the rain (scene vtbl+0x4c) / snow (vtbl+0x44) renderable here (see the
			// function banner) -- blocked on the missing scene factories, so the effect stays absent
		}
		else
		{
			pWeatherEffect = 0;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderGame::UpdateVisible( NWorld::IPlayer *pViewFrom, bool bShowUnits )
{
	if ( !bShowUnits )
	{
		if ( bPrevShowUnits != bShowUnits )
		{
			// retail @0x2cee50: the unit-sound mixer follows rUnits onto the SAME (empty) source
			CPtr<CSyncSrc<NWorld::IVisObj> > pNewSrc = new CSetSyncSrc<NWorld::IVisObj>();
			rUnits.SetNewSource( pNewSrc );
			if ( IsValid( pUnitSounds ) )
				pUnitSounds->SetNewSource( pNewSrc );
		}

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
		// retail @0x2cee50: whatever source rUnits gets, pUnitSounds shares the SAME instance --
		// that intersection (GetUnits AND the viewer's visible+heard set, filled below) is the
		// voice fog-of-war gate: an unseen+unheard unit's grunt C3DSound sits on GetUnits only,
		// so the mixer drops it; a visible unit's sounds ride in via AddVisitableChildren/
		// AddMiscObjects and a heard unit's via GetSounds.
		// retail's no-viewer branch (show-all cheat / cinematic view): NOT raw GetUnits but
		// CBoolSyncSrc<IVisObj,CSubtractFunc>( GetUnits(), pVisible ) with pVisible re-fed each call
		// from the world's heard-marker set (GetAllSoundStuff, IWorld vtbl+0xac) -- every real unit
		// is already shown, so the heard-not-seen CDMesh silhouettes must NOT draw.
		CPtr<CSyncSrc<NWorld::IVisObj> > pNewSrc;
		if ( pViewFrom )
			pNewSrc = new CBoolSyncSrc<NWorld::IVisObj, CIntersectionFunc>( pWorld->GetUnits(), pVisible );
		else
			pNewSrc = new CBoolSyncSrc<NWorld::IVisObj, CSubtractFunc>( pWorld->GetUnits(), pVisible );
		rUnits.SetNewSource( pNewSrc );
		if ( IsValid( pUnitSounds ) )
			pUnitSounds->SetNewSource( pNewSrc );
		pPrevViewFrom = pViewFrom;
	}
	if ( !pViewFrom )
	{
		// retail @0x2cee50 no-viewer tail (runs EVERY call, not only on a source switch): pVisible :=
		// the world's live heard-marker weak refs, so freshly created markers are subtracted too.
		vector<CPtr<NWorld::IVisObj> > soundStuff;
		pWorld->GetAllSoundStuff( &soundStuff );
		vector<NWorld::IVisObj*> vis;
		vis.reserve( soundStuff.size() );
		for ( int k = 0; k < soundStuff.size(); ++k )
		{
			NWorld::IVisObj *p = soundStuff[k];
			if ( p )
				vis.push_back( p );
		}
		pVisible->Set( vis );
	}
	else
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

	// retail @0x2cf620 frame order: UpdateWorld -> pGrass/testSpheres -> UpdateVisible -> Sync (the Jan03
	// source ran UpdateVisible first). Match retail so the visibility feed is refreshed against the world
	// state produced THIS frame.
	if ( bShowAllUnits )
		UpdateVisible( 0, true );//bShowUnits );
	else
		UpdateVisible( pViewFrom, true );//bShowUnits );

	// render them all
	r.Sync();
	rUnits.Sync();

	// retail @0x2cf620 tail: after both Syncs the weather lighting/effect state is advanced.
	// (Retail also prunes rUnits' expired unit-flashes here -- RemoveObsoleteFlashes @0x2cc0b0;
	// nothing in this predecessor ever pushes a flash, so there is nothing to prune yet.)
	SyncWeather();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderGame* CreateRenderGame( NWorld::IWorld *_pWorld, NGScene::IGameView *_pScene, NSound::ISoundScene *_pSoundScene )
{
	return new CRenderGame( _pWorld, _pScene, _pSoundScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2-only registrar (v1.2 @0x6cff40): game_selectionmode -> int @0x9c70cc (consumers: palette
// dispatcher @0x5d6130 + CreateSelection @0x6cbf60) and game_showweathereffect -> bool @0x97fec8
// (consumer: SyncWeather @0x6cbb90). Vars defined at the top of this namespace.
START_REGISTER(RWGame)
	REGISTER_VAR_EX( "game_selectionmode", NGlobal::VarIntHandler, &nSelectionMode, 0, true )
	REGISTER_VAR_EX( "game_showweathereffect", NGlobal::VarBoolHandler, &bShowWeatherEffect, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRender;
BASIC_REGISTER_CLASS( IShowUnit );
BASIC_REGISTER_CLASS( IRenderGame );
REGISTER_SAVELOAD_CLASS( 0x01941130, CRenderGame );
REGISTER_SAVELOAD_CLASS( 0x01941131, CSelection );
REGISTER_SAVELOAD_CLASS( 0x01941132, CShowWorldUnit );
REGISTER_SAVELOAD_CLASS( 0x01941133, CFakeWorldUnit );
REGISTER_SAVELOAD_CLASS( 0x01941135, CShowRPGUnit );
REGISTER_SAVELOAD_CLASS( 0x01941136, CFakeRPGUnit );
