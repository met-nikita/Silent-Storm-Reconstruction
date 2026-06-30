#ifndef __GSCENE_H_
#define __GSCENE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GGeometry.h"
#include "DiscretePos.h"
#include "GRenderModes.h"
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
class CRectLayout;
class CFontFormatInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CTexture;
	class CGeometry;
	class CRenderContext;
	class CCubeTexture;
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderPart;
class CPolyline;
class CRects;
class IMaterial;
class CParticles;
class CParticleEffect;
struct SRenderStats;
class ITextureLoader;
class CLightGroup;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroupSelect
{
	unsigned short nMaskAny, nMaskEvery;

	SGroupSelect( unsigned short _nMaskAny, unsigned short _nMaskEvery ): nMaskAny(_nMaskAny), nMaskEvery(_nMaskEvery) {}
};
inline bool operator!=( const SGroupSelect &a, const SGroupSelect &b ) { return a.nMaskAny != b.nMaskAny || a.nMaskEvery != b.nMaskEvery; }
const int N_MASK_CAST_SHADOW = 0x8000;
const int N_MASK_TREECROWN = 0x4000;
const int N_MASK_OCCLUDER = 0x2000;
const int N_MASK_OPAQUE = 0x1000;
const int N_MASK_FLOORS = 0x0fff;
inline SGroupSelect MakeSelectAll() { return SGroupSelect( N_MASK_FLOORS, 0 ); }
inline SGroupSelect MakeSelectOccluders() { return SGroupSelect( N_MASK_OCCLUDER, N_MASK_OCCLUDER ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroupInfo
{
	unsigned short nLightGroup, nObjectGroup;
	//
	SGroupInfo(): nLightGroup(0), nObjectGroup(0xffff) {}
	SGroupInfo( int _nLG, int _nOG ): nLightGroup(_nLG), nObjectGroup(_nOG) {}
	bool operator==( const SGroupInfo &a ) const { return a.nLightGroup == nLightGroup && a.nObjectGroup == nObjectGroup; }
	bool IsMaskMatch( const SGroupSelect &m ) const { return ( nObjectGroup & m.nMaskAny ) != 0 && ( nObjectGroup & m.nMaskEvery ) == m.nMaskEvery; }
};
int GetGroup( CLightGroup *p );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFullGroupInfo
{
	ZDATA
	SGroupInfo groupInfo;
	CPtr<CObjectBase> pUser;
	int nUserID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&groupInfo); f.Add(3,&pUser); f.Add(4,&nUserID); return 0; }
	SFullGroupInfo() : nUserID(0) {}
	SFullGroupInfo( const SGroupInfo &_g, CObjectBase *_p, int _nUserID ) : groupInfo(_g), pUser(_p), nUserID(_nUserID) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimLight: public CObjectBase
{
	OBJECT_BASIC_METHODS(CAnimLight);
public:
	CVec3 position;
	CVec3 color;
	float fRadius;
	bool bActive;
	bool bEnd;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EParticleFlags
{
//	PF_NOSHADOWS = 0,
//	PF_CAST_SHADOW = 1,
	PF_SELF_ILLUM = 0,
	PF_LIT = 2,
	PF_STATIC = 0,
	PF_DYNAMIC = 4
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderGeometryInfo;
class IPostProcess : public CObjectBase
{
public:
	struct SObject
	{
		SRenderGeometryInfo *pInfo;
		int nIdx;

		SObject( SRenderGeometryInfo *_pInfo, int _nIdx ) : pInfo(_pInfo), nIdx(_nIdx) {}
	};
	virtual void Render( NGfx::CRenderContext *pRC, const vector<SObject> &render ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDecalMappingInfo;
class CDecalTarget;
class IGScene : virtual public CObjectBase
{
public:
	virtual CLightGroup* CreateLightGroup() = 0;
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SFBTransform &trans, const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SDiscretePos &trans, const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		CFuncBase<SFBTransform> *pPlacement, const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		CFuncBase<vector<SHMatrix> > *pPlacement, CFuncBase<vector<NGfx::SCompactTransformer> > *_pMMXAnim, 
		const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateDynamicGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		CFuncBase<SBound> *pBound, const SFullGroupInfo &_ginfo ) = 0;
	virtual CObjectBase* CreateParticles( CPtrFuncBase<CParticleEffect> *pInfo,
		CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SGroupInfo &_ginfo, int nPFlags ) = 0;
	virtual CPolyline* CreatePolyline( CPtrFuncBase<NGfx::CGeometry> *pGeometry, const CVec3 &color ) = 0;
	virtual CObjectBase* CreateSelection( CObjectBase *pRenderNode, const CVec4 &vColor ) = 0;
	virtual CObjectBase* CreatePostProcessor( CObjectBase *pRenderNode, IPostProcess *pProcessor ) = 0;
	virtual void SetAmbient( const CVec3 &vBottomAmbientColor, const CVec3 &vTopAmbientColor ) = 0;
	virtual CObjectBase* AddDirectionalLight( CFuncBase<CVec3> *pColor, CFuncBase<CVec3> *pGlossColor, const CVec3 &vShadowColor, 
		const CVec3 &ptLight, const CVec3 &ptOrigin, 
		const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly, float fBlurShift ) = 0; 
	virtual CObjectBase* AddPointLight( const CVec3 &_vColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly ) = 0;
	virtual CObjectBase* AddPointLight( CPtrFuncBase<CAnimLight> *pLight ) = 0;
	virtual CObjectBase* AddSpotLight( CFuncBase<CVec3> *pColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, 
		float fRadius, CPtrFuncBase<NGfx::CTexture> *pMask, bool bLightmapOnly ) = 0;
	virtual CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info ) = 0;
	virtual CObjectBase* AddDecal( NGScene::CDecalTarget *pTarget, IMaterial *pMaterial ) = 0;
	virtual void Draw( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask, ERenderPath rp, EFogMode fogMode, const SFogParams &fog, EHSRMode hsrMode, ETransparentMode trMode, NGfx::CCubeTexture *pSky ) = 0;
	virtual CFuncBase<CVec3>* GetCamera() = 0;
	virtual CVec2 GetScreenRect() = 0;
	virtual bool TraceScene( const SGroupSelect &mask, const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps = SPS_STATIC, CObjectBase **ppElement = 0 ) = 0;
	virtual SGroupSelect GetLastMask() = 0;
	virtual void PrecacheMaterials() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IGScene* CreateScene();
// wrappers to Gfx to exclude interface on Gfx dependency
bool Is3DActive();
void SetWireframe( bool bWire );
void Clear( NGfx::CRenderContext *pRC, const CVec3 &vColor );
void ClearScreen( const CVec3 &vColor );
enum ERegisterCopyMode
{
	RCM_COPY,
	RCM_TRANSPARENT,
	RCM_SHOWALPHA
};
void CopyRegisterOnScreen( const CTRect<float> &rScreenRect, ERegisterCopyMode mode, int nRegister );
void GetRenderStats( SRenderStats *pStats );
float GetFrameTime();
void Flip();
int CalcTouchedTextureSize();
bool GetGeometryObjectInfo( CObjectBase *p, 
	CPtrFuncBase<CObjectInfo> **pGeometry, SFBTransform *pPos, SFullGroupInfo *pGroupInfo );
CFuncBase<vector<NGfx::SCompactTransformer> >* MakeMMXAnimation( CFuncBase<vector<SHMatrix> > *pAnim );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif




















