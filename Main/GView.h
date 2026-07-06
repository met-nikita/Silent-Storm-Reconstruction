#ifndef __GVIEW_H_
#define __GVIEW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GSkeleton.h"
#include "Time.h"
#include "GRenderModes.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRandomSeed;
class CTransformStack;
class CMemObject;
class CTerrainPart;
namespace NAnimation
{
	class CSkeletonAnimator;
}
namespace NBuilding
{
	class CBuildingGrid;
	class CBuildingInfoHold;
}
namespace NDb
{
	class CTexture;
	class CModel;
	class CTemplVariant;
	class CEffect;
	class CAmbientLightReal;
	class CComplexHead;
	class CAIGeometry;
	class CSkeleton;
	class CMaterial;
};
namespace NLSHead
{
	struct SHeadFrame;
	class CHeadInfo;
};
struct STerrainInfo;
struct SMapBuilding;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class CSelectionNode;
class CBuilding;
class CObjectInfo;
class CPolyline;
class CParticles;
class CParticleEffect;
struct SRenderStats;
class CGrassTracker;
class CExplosionInfo;
class IGScene;
class CLightGroup;
class CGrassAnimator;
class IPostProcess;
class CDecalTarget;
struct SDecalMappingInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRoomInfo
{
	ZDATA
	CPtr<CLightGroup> pGroup;
	short nFloor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGroup); f.Add(3,&nFloor); return 0; }
	//
	SRoomInfo(): nFloor(-100) {}
	SRoomInfo( int _nFloor ): nFloor(_nFloor) {}
	SRoomInfo( CLightGroup *_p, int _nFloor = -100 ): pGroup(_p), nFloor(_nFloor) {}
	bool operator==( const SRoomInfo &a ) const { return a.pGroup == pGroup && a.nFloor == nFloor; }
};
//////////////////////////////////////////////////////////////////////////////////////
struct SFullRoomInfo
{
	ZDATA
	SRoomInfo room;
	CPtr<CObjectBase> pUser;
	int nUserID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&room); f.Add(3,&pUser); f.Add(4,&nUserID); return 0; }
	//
	SFullRoomInfo() : nUserID(0) {}
	SFullRoomInfo( int _nFloor ) : room(_nFloor), nUserID(0) {}
	SFullRoomInfo( CLightGroup *_p, int _nFloor = -100 ): room( _p, _nFloor ), pUser(0), nUserID(0) {}
	SFullRoomInfo( const SRoomInfo &_r, CObjectBase *_p, int _nUserID ) : room(_r), pUser(_p), nUserID(_nUserID) {}
};
//////////////////////////////////////////////////////////////////////////////////////
class IGameView: public CObjectBase
{
public:
	enum ELightMode
	{
		LT_ZONE,
		LT_INVENTORY
	};
	struct SDrawInfo
	{
		CTransformStack *pTS; // pTS �� ���� �����
		CVec2 vOrigin, vSize; // in [0,1] diapason
		CVec3 vClearColor;
		bool bOverlay, bUseDefaultClearColor;

		SDrawInfo() : pTS(0), vOrigin(0,0), vSize(1,1), vClearColor(0,0,0), bOverlay(false), bUseDefaultClearColor(true) {}
	};
	virtual IGScene* GetGScene() = 0;
	virtual CLightGroup* CreateLightGroup() = 0;
	virtual CObjectBase* CreateMesh( NDb::CModel *pModel, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateMesh( CMemObject *pModel, const CVec4 &color, CFuncBase<SFBTransform> *pPlacement, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateMesh( NDb::CModel *pModel, const SFBTransform &placement, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateMesh( CMemObject *pModel, const CVec4 &color, const SFBTransform &placement, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateSkin( NDb::CModel *pModel, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateOccluder( NDb::CAIGeometry *pGeom, const SFBTransform &placement, const SRoomInfo &_g = SRoomInfo() ) = 0;
	virtual CObjectBase* CreateOccluder( NDb::CAIGeometry *pGeom, NDb::CSkeleton *pSkeleton, CFuncBase<NAnimation::SSkeletonPose> *pAnimation, const SRoomInfo &_g = SRoomInfo() ) = 0;
	virtual CObjectBase* CreateTerrainRegion( CFuncBase<STerrainInfo> *pInfo, CVersioningBase *pUpdateRegion, const SRandomSeed &sSeed, const CTRect<int> &sRegion, const list<CObj<CPtrFuncBase<CTerrainPart> > > &partsList, NGScene::CGrassTracker *pGrass, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateTerrainWall( CPtrFuncBase<CTerrainPart> *pPart, NDb::CTexture *pTexture, const SFullRoomInfo &_g = SFullRoomInfo() ) = 0;
	virtual CObjectBase* CreateGrassSector( CGrassAnimator *pEffect, NDb::CTexture *pTexture, CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SRoomInfo &_g = SRoomInfo() ) = 0;
	virtual CSelectionNode* CreateSelection( const vector<CObjectBase*> &target, const CVec4 &vColor ) = 0;
	virtual CObjectBase* CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, const SRoomInfo &_g = SRoomInfo(), NAnimation::CSkeletonAnimator *pScAnim = 0 ) = 0;
	virtual CObjectBase* CreateParticles( NDb::CEffect *pEffect, STime stBeginTime, CFuncBase<STime> *pTime, const SFBTransform &place, const SRoomInfo &_g = SRoomInfo() ) = 0;
	virtual CBuilding* CreateBuildingPart( int nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI ) = 0;
	virtual CPolyline* CreatePolyline( const vector<CVec3> &points, const CVec3 &color ) = 0;
	virtual CObjectBase* CreateExplosion( CFuncBase<STime> *pTime, NDb::CEffect *pEffect, CFuncBase<CExplosionInfo> *pExplosion, const CVec3 &pos, const SRoomInfo &_g = SRoomInfo() ) = 0;
	virtual CObjectBase* CreateLSHead( NDb::CComplexHead *pHead, CFuncBase<NLSHead::SHeadFrame> *pAnimation, CFuncBase<STime> *pTime, CFuncBase<SFBTransform> *pPlacement, bool bInterface, const SRandomSeed &headSeed, bool bHasCap = false, const SRoomInfo &_g = SRoomInfo(), CPtrFuncBase<NGfx::CTexture> *pFaceTexture = 0, const NLSHead::CHeadInfo *pHeadInfo = 0 ) = 0;
	virtual CObjectBase* Precache( NDb::CModel *pModel ) = 0;
	virtual void StartAlienStyle() = 0;
	virtual void FinishAlienStyle() = 0;
	virtual void Draw( const SDrawInfo &drawInfo ) = 0;
	virtual CVec2 GetScreenRect() = 0;
	virtual int  GetCutFloor() = 0;
	virtual void SetCutFloor( int nFloor ) = 0;
	// retail CBaseCamera::SetCutFloorRange @0xcba10: install the INCLUSIVE [min,max] cut-floor
	// range (the mission adopts it from the template variant, @0x200690) and re-clamp the current
	// floor into it; every later SetCutFloor clamps against this range (retail @0xd0050).
	virtual void SetCutFloorRange( int nMinFloor, int nMaxFloor ) = 0;
	// retail CBaseCamera::GetCutFloorRange (the level-switch bar reads it every Draw @0x254420 to
	// recompute the 8 buttons' floor/basement + visible/hidden image states)
	virtual void GetCutFloorRange( int *pMinFloor, int *pMaxFloor ) = 0;
	// retail SetCutFloor @0xd0050 opens with `cmp [this+0xd4],0; jg ret` -- while the camera is
	// FROZEN (script CameraLock -> FreezeCamera @0xcffc0) every floor change is a hard NO-OP.
	// Dev keeps floors on the scene, so the freeze count is MIRRORED here from the CUICmdLockCamera
	// dispatch. This is the base's one-floor lock: EBase's OnEnterZone calls CameraLock() once.
	virtual void SetCutFloorLock( bool bLock ) = 0;
	virtual bool GetParticleShow() = 0;
	virtual void SetParticleShow( bool bNewState ) = 0;
	virtual void SetAmbient( const CVec3 &vBottomAmbientColor, const CVec3 &vTopAmbientColor ) = 0;
	virtual CObjectBase* AddDirectionalLight( const CVec3 &ptColor, const CVec3 &ptLight, const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly = false ) = 0;
	virtual CObjectBase* AddPointLight( const CVec3 &ptColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly ) = 0;
	virtual CObjectBase* AddFlare( CFuncBase<CVec3> *pOrigin, CFuncBase<STime> *pTime, int nFloor, float fFlareRadius, NDb::CTexture *pFlareTexture, float fOnTime, float fOffTime ) = 0;
	virtual CObjectBase* AddPostFilter( const vector<CObjectBase*> &target, IPostProcess *pEffect ) = 0;
	virtual CObjectBase* AddSpotLight( const CVec3 &ptColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, NDb::CTexture *pMask, bool bLightmapOnly = false ) = 0;
	virtual CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info ) = 0;
	virtual CObjectBase* AddDecal( NGScene::CDecalTarget *pTarget, NDb::CMaterial *pMaterial ) = 0;
	virtual void SetAmbient( NDb::CAmbientLightReal *pLight, ELightMode lm = LT_ZONE ) = 0;
	virtual ESceneRenderMode GetRenderMode() const = 0;
	virtual void SetRenderMode( ESceneRenderMode mode ) = 0;
	virtual EFogMode GetFogMode() const = 0;
	virtual void SetFogMode( EFogMode mode ) = 0;
	virtual void SetHSRMode( EHSRMode m ) = 0;
	virtual EHSRMode GetHSRMode() const = 0;
	virtual void SetTransparentMode( ETransparentMode m ) = 0;
	virtual ETransparentMode GetTransparentMode() const = 0;
	virtual bool TraceScene( const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps = SPS_STATIC, CObjectBase **ppObject = 0 ) = 0;
	virtual void SetFogBaseHeight( float f ) = 0;
	void SetNextFogMode();
	void SetNextShadowsMode();
	void SetNextLightmapViewMode();
	void SetNextTranspRenderMode();
	void SetNextHSRMode();
	void SetFastMode();
};
IGameView* CreateNewView();
IGameView* CreateNewFastInterfaceView();
// wrappers to Gfx to exclude interface on Gfx dependency, realisation in GScene.cpp
bool Is3DActive();
void SetWireframe( bool bWire );
void Flip();
void GetRenderStats( SRenderStats *pStats );
float GetFrameTime();
int CalcTouchedTextureSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
