#ifndef __WINTERFACEVISITORS_H_
#define __WINTERFACEVISITORS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
#include "GSkeleton.h"
#include "../DBFormat/DataFormat.h"

struct SRandomSeed;
class CTerrainPart;
class CMemObject;
class CTerrainInfoHolder;
struct SMapBuilding;
namespace NGfx { class CTexture; }   // IRenderVisitor::AddHead pFaceTexture (live FaceGen preview)
namespace NDb
{
	class CModel;
	class CEffect;
	class CRPGArmor;
	class CTexture;
	class CSound;
	class CSkeleton;
	class CAIGeometry;
	class CComplexHead;
	class CSoundEffect;
	//class CMaterial;
}
namespace NAnimation
{
	struct SSkeletonState;
	class CSkeletonAnimator;
}
namespace NGScene
{
	class CExplosionInfo;
	class CLightGroup;
	struct SRoomInfo;
	class CDecalTarget;
	struct SDecalMappingInfo;
}
namespace NBuilding
{
	class CBuildingInfoHold;
}
namespace NLSHead
{
	class CHeadTransformInfo;   // live head-morph tension source (4-arg AddHead)
}
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit;
struct IRenderVisitor
{
	struct SBoundMesh
	{
		CPtr<NDb::CModel> pModel;
		const char *pszBindBone;

		SBoundMesh() {}
		SBoundMesh( NDb::CModel *_pModel, const char *_pszBindBone ): pModel(_pModel), pszBindBone(_pszBindBone) {}
	};
	// Release-added render-feed record: a particle effect bound to a unit (scriptParticles).
	// Serialized raw (DoDataVector) as a member of CDumbUnitServer::attachedEffects; 8 bytes.
	struct SBoundEffect
	{
		CPtr<NDb::CEffect> pEffect;
		STime tBegin;
	};
	virtual NGScene::CLightGroup* MakeGroup() { return 0; }
	virtual void AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, CFuncBase<SFBTransform> *pPosition, NAnimation::CSkeletonAnimator *pScAnimator = 0 ) {}
	virtual void AddParticleEffect( STime tBegin, NDb::CEffect *pEffect, int nFloor, const SFBTransform &place ) {}
	virtual void AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fRadius, bool bLightmapOnly ) {}
	virtual void AddFlare( CFuncBase<CVec3> *pOrigin, float fFlareRadius, NDb::CTexture *pFlareTexture, int nFloor, float fOnTime = 0.33f, float fOffTime = 0.05f ) {}
	virtual void AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, NDb::CTexture *pMask, bool bLightmapOnly ) {}
	virtual void AddMesh( NDb::CModel *pModel, const SFBTransform &position, NGScene::CLightGroup *pGroup, int nFloor, int nUserID = -1 ) {}
	virtual void AddMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &position, int nUserID = -1 ) {}
	virtual void AddPolyline( const vector<CVec3> &points, const CVec3 &cr ) {}
	virtual void AddItemMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor = -100 ) {}
	virtual void AddItemHead( CUnit *pUnit, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor = -100 ) {}
	virtual void AddMesh( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, CFuncBase<NAnimation::SSkeletonState> *pState, const vector<SBoundMesh> &boundMeshes, NGScene::CLightGroup *pGroup, int nFloor, CUnit *pUnit = 0, int nUserID = -1 ) {}
	virtual void AddBuilding( const SMapBuilding &info ) {}
	virtual void AddBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI ) {}
	virtual void AddTerrainParts( const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, CTerrainInfoHolder *pInfo, CVersioningBase *pUpdateRegion, int nUserID ) {}
	virtual void AddTerrainWallPart( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, CTerrainInfoHolder *pInfo, int nUserID ) {}
	virtual void AddGrass( CTerrainInfoHolder *pInfo ) {}
	virtual void AddGrassEvent( const CVec3 &ptPlace ) {}
	virtual void AddExplosion( NDb::CEffect *pEffect, CFuncBase<NGScene::CExplosionInfo> *pExplosion, const CVec3 &pos ) {}
	virtual void AddHead( CUnit *pUnit, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room ) {}
	virtual void AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room ) {}
	// Release-new: a standalone head with a LIVE macro-muscle morph (the advanced FaceGen editor).
	virtual void AddHead( NDb::CComplexHead *pHead, CFuncBase<SFBTransform> *pPosition, const NGScene::SRoomInfo &room, NLSHead::CHeadTransformInfo *pTransformInfo, CPtrFuncBase<NGfx::CTexture> *pFaceTexture = 0 ) {}
	// release @0x2cda30 (CSetRender vtbl slot 0x50): arm ambient facial idling (blinks) for the unit's
	// shown head. The visited fake unit calls this every Visit, AFTER its AddMesh/AddHead created the
	// head animator record; the destination registers the returned idle token so idling stops when no
	// view renders the head any more. KEYING DEVIATION: retail passes the unit's NLSHead::CHeadInfo*;
	// this tree keys head animators by NWorld::CUnit* (see CHeadsController::PlayIdle).
	virtual void AddHeadIdleAnimator( CUnit *pUnit ) {}
	virtual void AddOccluder( NDb::CAIGeometry *pAIGeom, const SFBTransform &pos, int nFloor ) {}
	virtual void AddOccluder( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, int nFloor ) {}
	virtual NGScene::CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const NGScene::SDecalMappingInfo &_info ) { return 0; }
	virtual void AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial ) {}
	virtual void LoadGeometry( NDb::CModel *pModel ) {}
	virtual void AddColorPostFilter( const CVec4 &vColor ) {}
	virtual void StartAlienStyle() {}
	virtual void FinishAlienStyle() {}
	virtual void SetBaseFogHeight( float f ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct IAIVisitor
{
	struct SPieceMap
	{
		int nPieceID, nUserID;
	};
	virtual CObjectBase* AddHull( NDb::CAIGeometry *pAIGeom, 
		const SFBTransform &pos, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask ) = 0;
	virtual CObjectBase* AddHull( CMemObject *pAIGeom, 
		const SFBTransform &pos, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask, int nUserID ) = 0;
	virtual CObjectBase* AddAnimatedHull( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton, 
		CFuncBase<NAnimation::SSkeletonPose> *pAnimation, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask ) = 0;
	virtual CObjectBase* AddFlippingHull( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton, const SFBTransform &pos,
		CFuncBase<NAnimation::SSkeletonPose> *pAn1, CFuncBase<NAnimation::SSkeletonPose> *pAn2, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask, bool bOpen, int nDoorID, int nDestroyStage ) = 0;
	virtual void AddPieces( NDb::CAIGeometry *pAIGeom, const vector<SPieceMap> &parts,
		const SFBTransform &pos, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask ) = 0;
	virtual void AddTerrainPart( CPtrFuncBase<CTerrainPart> *pPart, 
		NDb::CRPGArmor *pArmor, int nFloor, int nMask ) = 0;
	virtual void LoadGeometry( NDb::CAIGeometry *pAIGeom ) {}
	virtual void LoadSkinGeometry( NDb::CAIGeometry *pAIGeom, NDb::CSkeleton *pSkeleton ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct ISoundVisitor
{
	virtual void Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition ) = 0;
	virtual void AddEffect( STime tStart, NDb::CSoundEffect *pEffect, CFuncBase<CVec3> *pPosition, const vector<int> &flags ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct IVisObj: virtual public CObjectBase
{
	virtual void Visit( IRenderVisitor* ) {}
	virtual void Visit( IAIVisitor* ) {}
	virtual void Visit( ISoundVisitor* ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
};
#endif