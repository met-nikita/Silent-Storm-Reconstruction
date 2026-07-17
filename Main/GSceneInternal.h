#ifndef __GSCENEINTERNAL_H_
#define __GSCENEINTERNAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GScene.h"
#include "OcTree.h"
#include "GSceneUtils.h"
#include "GRenderCore.h"
#include "GCombiner.h"
#include "GRenderExecute.h"
#include "GTransparent.h"
#include "GLightmapCalc.h"
#include "DiscretePos.h"
#include "GRenderLight.h"
#include "GDecal.h"
namespace NDb
{
	class CMaterial;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
struct SDynamicLightGrouping;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCombinedKey
{
	ZDATA
	CPtr<IMaterial> pMaterial;
	SFullGroupInfo fullGroupInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMaterial); f.Add(3,&fullGroupInfo); return 0; }
	
	SCombinedKey() {}
	SCombinedKey( IMaterial *_pMaterial, const SFullGroupInfo &_info )
		: pMaterial(_pMaterial), fullGroupInfo(_info)
	{
		SGroupInfo &groupInfo = fullGroupInfo.groupInfo;
		if ( !pMaterial->DoesCastShadow() )
			groupInfo.nObjectGroup &= ~N_MASK_CAST_SHADOW;
		IMaterial::EMaterialType mt = pMaterial->GetType();
		if ( mt == IMaterial::MT_OCCLUDER )
			groupInfo.nObjectGroup |= N_MASK_OCCLUDER;
		else
			groupInfo.nObjectGroup &= ~N_MASK_OCCLUDER;
		if ( (groupInfo.nObjectGroup & N_MASK_CAST_SHADOW) && mt == IMaterial::MT_NORMAL && !pMaterial->IsAlphaTest() )
			groupInfo.nObjectGroup |= N_MASK_OPAQUE;
		else
			groupInfo.nObjectGroup &= ~N_MASK_OPAQUE;
	}
	//
//	bool operator==( const SCombinedKey &k ) const { return pMaterial == k.pMaterial && fullGroupInfo == k.fullGroupInfo; }
};
/*struct SCombinedKeyHash
{
	int operator()( const SCombinedKey &s ) const 
	{ 
		return ((int)s.pMaterial.GetPtr()) ^ s.groupInfo.nLightGroup ^ s.groupInfo.nObjectGroup;
	}
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
class IHZBuffer;
class CVolumeNode;
class CCombinedPart;
class CNonePart : public IPart
{
	OBJECT_NOCOPY_METHODS(CNonePart);
	ZDATA_(IPart)
	SCombinedKey group;
public:
	CPtr<CCombinedPart> pOwner;
	vector<CObj<CObjectBase> > decals;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(IPart*)this); f.Add(2,&group); f.Add(3,&pOwner); f.Add(4,&decals); return 0; }
	CNonePart() {}
	CNonePart( CPtrFuncBase<CObjectInfo> *pData, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo ) 
		: IPart( pData, 0, _pMaterial->IsSolid() ), group( _pMaterial, _gInfo ) {}
	virtual CTRect<int> GetLMRegion() const { return CTRect<int>(0,0,0,0); }
	virtual int GetLMLOD() const { return 0; }
	IMaterial* GetMaterial() const { return group.pMaterial; }
	const SGroupInfo& GetGroupInfo() const { return group.fullGroupInfo.groupInfo; }
	const SFullGroupInfo& GetFullGroupInfo() const { return group.fullGroupInfo; }
	virtual bool Is2Sided() const { return group.pMaterial->Is2Sided(); }
	virtual int GetSortValue() const { return (int)group.pMaterial.GetPtr(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSimplePart : public CNonePart
{
	OBJECT_NOCOPY_METHODS(CSimplePart);
	ZDATA_(CNonePart)
	SFBTransform pos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CNonePart*)this); f.Add(2,&pos); return 0; }
public:
	CSimplePart() {}
	CSimplePart( CPtrFuncBase<CObjectInfo> *pData, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo, const SFBTransform &_pos ) : CNonePart( pData, _pMaterial, _gInfo ), pos(_pos) {}
	virtual ETransformType GetTransformType() const { return TT_SIMPLE; }
	virtual const SFBTransform& GetSimplePos() { return pos; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDiscretePart : public CNonePart
{
	OBJECT_NOCOPY_METHODS(CDiscretePart);
	ZDATA_(CNonePart)
	SDiscretePos dPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CNonePart*)this); f.Add(2,&dPos); return 0; }
public:
	CDiscretePart() {}
	CDiscretePart( CPtrFuncBase<CObjectInfo> *pData, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo, const SDiscretePos &_pos ) : CNonePart( pData, _pMaterial, _gInfo ), dPos(_pos) {}
	virtual ETransformType GetTransformType() const { return TT_SIMPLE_DISCRETE; }
	virtual const SDiscretePos& GetDiscretePos() { return dPos; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGenericDynamicPart : public CNonePart
{
protected:
	ZDATA_(CNonePart)
	int nStillCounter;
	CDGPtr<CPtrFuncBase<CObjectInfo> > pTrackObjInfo;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CNonePart*)this); f.Add(2,&nStillCounter); f.Add(3,&pTrackObjInfo); return 0; }
protected:
	bool bObjectInfoChanged;
public:
	CGenericDynamicPart() : bObjectInfoChanged(false) {}
	CGenericDynamicPart( CPtrFuncBase<CObjectInfo> *pData, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDynamicPart : public CGenericDynamicPart
{
	OBJECT_BASIC_METHODS( CDynamicPart );
private:
	ZDATA_(CGenericDynamicPart)
	SBound bound;
	CDGPtr<CFuncBase<SFBTransform> > pTransform;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CGenericDynamicPart*)this); f.Add(2,&bound); f.Add(3,&pTransform); return 0; }

public:
	CDynamicPart() {}
	CDynamicPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase<SFBTransform> *pPos, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo );
	virtual ETransformType GetTransformType() const { return TT_SIMPLE; }
	virtual const SFBTransform& GetSimplePos() { return pTransform->GetValue(); }
	CFuncBase<SFBTransform>* GetSimplePosNode() { return pTransform; }
	bool Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimatedPart : public CGenericDynamicPart
{
	OBJECT_NOCOPY_METHODS( CAnimatedPart );
	ZDATA_(CGenericDynamicPart)
	CDGPtr<CFuncBase< vector<SHMatrix> > > pAnimation;
	CDGPtr<CFuncBase< vector<NGfx::SCompactTransformer> > > pMMXAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CGenericDynamicPart*)this); f.Add(2,&pAnimation); f.Add(3,&pMMXAnimation); return 0; }

	void EstimateBound( SBound *pRes );
public:
	CAnimatedPart() {}
	CAnimatedPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase< vector<SHMatrix> > *pAnim, CFuncBase<vector<NGfx::SCompactTransformer> > *pMMXAnim, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo );
	virtual ETransformType GetTransformType() const { return TT_SINGLE_SKIN; }
	virtual const vector<SHMatrix>& GetAnimation() { return pAnimation->GetValue(); }
	virtual const vector<NGfx::SCompactTransformer>& GetMMXAnimation() { return pMMXAnimation->GetValue(); }
	virtual CFuncBase< vector<SHMatrix> >* GetAnimationNode() { return pAnimation; }
	virtual CFuncBase< vector<NGfx::SCompactTransformer> >* GetMMXAnimationNode() { return pMMXAnimation; }
	bool Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDynamicGeometryPart : public CGenericDynamicPart
{
	OBJECT_NOCOPY_METHODS( CDynamicGeometryPart );
	ZDATA_(CGenericDynamicPart)
	CDGPtr<CFuncBase<SBound> > pBound;
	// retail CDynamicGeometryPart::operator& @0x16b200 tag 3: the part's placement node (a CDGPtr at
	// runtime -- Update @0x160250 Refresh()es it; the serializer writes its inner CObj). For a head
	// part this is CreateLSHead's CMSRNode chain under the unit's head-bone filter.
	CDGPtr<CFuncBase<SFBTransform> > pTransform;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CGenericDynamicPart*)this); f.Add(2,&pBound); f.Add(3,&pTransform); return 0; }
public:
	CDynamicGeometryPart() {}
	CDynamicGeometryPart( CPtrFuncBase<CObjectInfo> *pData, CFuncBase<SFBTransform> *pPos, CFuncBase<SBound> *pAnim, IMaterial *_pMaterial, const SFullGroupInfo &_gInfo );
	// retail @0x165d90: `return pTransform.pNode != 0` -- 0 (TT_NONE) or 1 (TT_SIMPLE, the matrix type).
	virtual ETransformType GetTransformType() const { return pTransform.Get() ? TT_SIMPLE : TT_NONE; }
	virtual const SFBTransform& GetSimplePos() { return pTransform->GetValue(); }
	bool Update( CVolumeNode *pVolume, SStaticTrackers *pTrackers );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticles: public IParticles
{
	OBJECT_NOCOPY_METHODS(CParticles);
	ZDATA
	SBound bound;
	CDGPtr< CPtrFuncBase<CParticleEffect> > pParticles;
	CDGPtr< CFuncBase<SFBTransform> > pPlacement;
	CPtr<CVolumeNode> pNode;
	SGroupInfo groupInfo;
	int nFlags;
	SBound transformedBound;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bound); f.Add(3,&pParticles); f.Add(4,&pPlacement); f.Add(5,&pNode); f.Add(6,&groupInfo); f.Add(7,&nFlags); f.Add(8,&transformedBound); return 0; }
public:
	CParticles() {}
	CParticles( CPtrFuncBase<CParticleEffect> *_pParticles, CFuncBase<SFBTransform> *_pPlacement, 
		const SBound &_bound, const SGroupInfo &_g, int _nFlags )
		: pParticles(_pParticles), pPlacement(_pPlacement), bound(_bound), groupInfo(_g), nFlags(_nFlags) {}
	const SGroupInfo& GetGroup() const { return groupInfo; }
	const SBound& GetBound() const { return transformedBound; }
	CParticleEffect* GetEffect();
	bool Update( CVolumeNode *pVolume );
	bool IsLit() const { return ( nFlags & PF_LIT ) != 0; }
	bool IsDynamic() const { return ( nFlags & PF_DYNAMIC ) != 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! selection
class CSelection: public CObjectBase
{
	OBJECT_BASIC_METHODS( CSelection );
private:
	ZDATA
	CVec4 vColor;
	CPtr<CNonePart> pTarget;
	CObj<CAutomaticCombiner> pCombo;
	CDGPtr<CVBCombiner> pOffsetVertices;
	CDGPtr< CFuncBase<vector<NGfx::STriangleList> > > pTriList;
	CObj<CVersioningBase> pAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vColor); f.Add(3,&pTarget); f.Add(4,&pCombo); f.Add(5,&pOffsetVertices); f.Add(6,&pTriList); f.Add(7,&pAnimation); return 0; }
public:
	CSelection() {}
	bool Initialize( CObjectBase *pPart, const CVec4 &vColor );
	void Render( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask, bool bOffset );
	bool Update( IGScene *pScene );
	const CVec4& GetColor() const { return vColor; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! bind post process effect to target geometry
class CPostProcessBinder : public CObjectBase
{
	OBJECT_BASIC_METHODS( CPostProcessBinder );
private:
	ZDATA
	CPtr<CNonePart> pTarget;
	CPtr<IPostProcess> pPostProcess;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTarget); f.Add(3,&pPostProcess); return 0; }
public:
	CPostProcessBinder() {}
	bool Initialize( CObjectBase *p, IPostProcess *pPost );
	void Store( vector<IPostProcess::SObject> *pRes, CTransformStack *pTS, const SGroupSelect &mask );
	bool Update( IGScene *pScene ) { return IsValid( pTarget ); }
	IPostProcess* GetPostProcessor() const { return pPostProcess; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCombinedPart: public CObjectBase
{
	OBJECT_BASIC_METHODS(CCombinedPart);
public:
	struct SPartInfo
	{
		ZDATA
		SGroupInfo groupInfo;
		int nMaterial;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&groupInfo); f.Add(3,&nMaterial); return 0; }
	};
	enum EType
	{
		CP_OCCLUDER,
		//CP_LIGHTMAPPED,
		CP_NORMAL
	};
private:
	SRenderGeometryInfo geometryInfo;
	ZDATA
	CDGPtr<CPerMaterialCombiner> pCombiner;
	// retail CCombinedPart::operator& @0x168a70 serializes nFloorMask at tag 3 (int) right after pCombiner.
	// Byte-walking real game.sav records confirms the save carries it (tags 2:4 3:4 4:32 5:4 6:32 7 8 9:4 10:1),
	// and removing it reintroduced the PrecacheMaterials null-deref (GSceneInternal.cpp:1498) by shifting
	// materials off its tag. This layout is verified-correct against the actual save bytes.
	int nFloorMask;
	CPartFlags lastNewFlags;
	int nIgnoreMark;
	CPartFlags ignoredParts;
	vector<SPartInfo> parts;
	vector<CPtr<IMaterial> > materials;
	EType partType;
	bool bWasTnL;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCombiner); f.Add(3,&nFloorMask); f.Add(4,&lastNewFlags); f.Add(5,&nIgnoreMark); f.Add(6,&ignoredParts); f.Add(7,&parts); f.Add(8,&materials); f.Add(9,&partType); f.Add(10,&bWasTnL); return 0; }
	void InitGeometry();
public:

	CCombinedPart() : nFloorMask(0), nIgnoreMark(0) {}
	CCombinedPart( SStaticTrackers *pTrackers, EType t );
	CPerMaterialCombiner* GetCombiner() const { return pCombiner; }
	IVBCombiner* GetVBCombiner() { return GetGeometryInfo()->pVertices; }
	SRenderGeometryInfo* GetGeometryInfo();
	void SetIgnored( int _nIgnoreMark, const CPartFlags &parts );
	int GetIgnoreMark() const { return nIgnoreMark; }
	int GetMaterialsNumber() const { return materials.size(); }
	IMaterial* GetMaterial( int n ) const { return materials[n]; }
	const vector<SPartInfo>& GetPartsInfo() const { return parts; }
	void UpdatePartInfo();
	const CPartFlags& GetIgnoredParts() const { return ignoredParts; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! node size
const int N_MINIMAL_OCTREE_NODE = 8;//16; 
class CVolumeNode : public COcTreeNode<CVolumeNode, N_MINIMAL_OCTREE_NODE>
{
	OBJECT_NOCOPY_METHODS( CVolumeNode );
public:
	struct SPerMaterialHolder
	{
		ZDATA
		vector<CObj<CCombinedPart> > transparent, occluder, normal, normalLM, alien;
		list<CPtr<CCombinedPart> > elements;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&transparent); f.Add(3,&occluder); f.Add(4,&normal); f.Add(5,&normalLM); f.Add(6,&alien); f.Add(7,&elements); return 0; }

		CCombinedPart* AllocCombinerPart( vector<CObj<CCombinedPart> > *pRes, SStaticTrackers *pTrackers, CCombinedPart::EType t )
		{
			for ( int k = 0; k < pRes->size(); ++k )
			{
				if ( (*pRes)[k]->GetCombiner()->GetSize() < PF_MAX_PARTS_PER_COMBINER )
					return (*pRes)[k];
			}
			// have to alloc new one
			CCombinedPart *p = new CCombinedPart( pTrackers, t );
			pRes->push_back( p );
			elements.push_back( p );
			return p;
		}
		CCombinedPart* GetCombinerPartForAdd( IMaterial *pMaterial, SStaticTrackers *pTrackers, bool bIsLightmapped )
		{
			if ( bIsLightmapped )
			{
				ASSERT( pMaterial->GetType() == IMaterial::MT_NORMAL );
				return AllocCombinerPart( &normalLM, pTrackers, CCombinedPart::CP_NORMAL );//CCombinedPart::CP_LIGHTMAPPED );
			}
			switch ( pMaterial->GetType() )
			{
				case IMaterial::MT_TRANSPARENT: return AllocCombinerPart( &transparent, pTrackers, CCombinedPart::CP_NORMAL );
				case IMaterial::MT_OCCLUDER: return AllocCombinerPart( &occluder, pTrackers, CCombinedPart::CP_OCCLUDER );
				case IMaterial::MT_NORMAL: return AllocCombinerPart( &normal, pTrackers, CCombinedPart::CP_NORMAL );
				case IMaterial::MT_ALIEN: return AllocCombinerPart( &alien, pTrackers, CCombinedPart::CP_NORMAL );
				default: ASSERT(0); break;
			}
			return 0;
		}
		bool IsEmpty() const
		{
			for ( list<CPtr<CCombinedPart> >::const_iterator i = elements.begin(); i != elements.end(); ++i )
			{
				ASSERT( IsValid( *i ) );
				if ( (*i)->GetCombiner()->GetSize() != 0 )
					return false;
			}
			return true;
		}
	};
	//
	ZDATA_(CParent)
	int nLastFrame;
	SPerMaterialHolder staticParts, dynamicParts;
	list<CPtr<CParticles> > particles;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CParent*)this); f.Add(2,&nLastFrame); f.Add(3,&staticParts); f.Add(4,&dynamicParts); f.Add(5,&particles); return 0; }

	CVolumeNode() : nLastFrame(0) {}
	CVolumeNode* SelectNode( CFuncBase<SFBTransform> *pTransform, const SBound &bound )
	{
		const SFBTransform &pos = pTransform->GetValue();
		CVec3 ptCenter;
		pos.forward.RotateHVector( &ptCenter, bound.s.ptCenter );
		float fR = sqrt( CalcRadius2( bound, pos.forward ) );
		return GetNode( ptCenter, fR );
	}
	virtual bool IsEmpty();
private:	
	typedef COcTreeNode<CVolumeNode, N_MINIMAL_OCTREE_NODE> CParent;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPolyline: public CObjectBase
{
	OBJECT_BASIC_METHODS(CPolyline);
public:
	CDGPtr< CPtrFuncBase<NGfx::CGeometry> > pGeometry;
	CVec3 color;
	//
	void Render( NGfx::CRenderContext *pRC );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRayInfo
{
	CVec3 vOrigin, vDir, vDirOrt;
	float fLength;

	SRayInfo( const CRay &r ) 
		: vOrigin( r.ptOrigin ), vDir( r.ptDir ), vDirOrt( r.ptDir ), fLength( fabs( r.ptDir ) )
	{
		Normalize( &vDirOrt );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGScene;
class CAmbientAnimator;   // release-new animated-ambient DG node (defined in GSceneInternal.cpp); CObj serializes by id
class CDynamicLightCache
{
	typedef unordered_map<CVec3, SDynamicAmbientInfo, SVec3Hash> CHash;
	ZDATA
	CDGPtr<CVersioningBase> pStaticChanged;
	CHash data;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pStaticChanged); f.Add(3,&data); return 0; }
	CDynamicLightCache() {}
	void SetTracker( CVersioningBase *p ) { pStaticChanged = p; }
	const SDynamicAmbientInfo& Calc( const SSphere &bv, CGScene *pScene );
	bool CheckStatic();
	void Reset() { data.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFakeParticleLMTexture : public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_NOCOPY_METHODS(CFakeParticleLMTexture);
	ZDATA
	// retail (@0x165010): the ambient is now a DG object EDGE, not a stored CVec3 -- it tracks the
	// scene's CAmbientMean node so a save carries the reference and the texel refreshes through it.
	CDGPtr<CFuncBase<CVec3> > pAmbient;
	CDGPtr<CFuncBase<CVec3> > pColor;
	DWORD dwNormalColor, dwParticleColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAmbient); f.Add(3,&pColor); f.Add(4,&dwNormalColor); f.Add(5,&dwParticleColor); return 0; }
	// @0x1645f0: refresh pColor then pAmbient unconditionally, report either changed
	bool NeedUpdate() { bool bColor = pColor.Refresh(); bool bAmbient = pAmbient.Refresh(); return bColor || bAmbient; }
	void Recalc();
public:
	void SetAmbient( CFuncBase<CVec3> *_p ) { pAmbient = _p; }
	void SetColor( CFuncBase<CVec3> *_pColor ) { pColor = _pColor; }
	DWORD GetNormalColor() const { return dwNormalColor; }
	DWORD GetParticleColor() const { return dwParticleColor; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail-new DG node (@0x163c30/@0x163c60/@0x1651c0): the arithmetic mean of the scene top+bottom
// ambient colour nodes ((top+bottom)*0.5). Replaces the mean CGScene::SetAmbient used to inline.
class CAmbientMean : public CFuncBase<CVec3>
{
	OBJECT_BASIC_METHODS(CAmbientMean);
	ZDATA
	CDGPtr<CFuncBase<CVec3> > pTopAmbient, pBottomAmbient;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTopAmbient); f.Add(3,&pBottomAmbient); return 0; }
protected:
	bool NeedUpdate();
	void Recalc();
public:
	CAmbientMean() {}
	CAmbientMean( CFuncBase<CVec3> *_pTop, CFuncBase<CVec3> *_pBottom ): pTopAmbient(_pTop), pBottomAmbient(_pBottom) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDecalsManager;
class CGScene: public IGScene, public ICacheLightRender, public IDecalQuery
{
OBJECT_BASIC_METHODS(CGScene);
	enum ERLRequest
	{
		RN_STATIC = 1,
		RN_DYNAMIC = 2,
		RN_ALL = 3,
		RN_LIGHTMAPS = 4,
		RN_DEPTH = 8,
		RN_LIT_PARTICLES = 16,
		RN_OCCLUDERS = 32,
		RN_HQ_OCCLUDERS = 64
	};

	ZDATA
	SStaticTrackers trackers;
	CObj<CVolumeNode> pVolume;
	list< CPtr<CDynamicGeometryPart> > dynamicFrags;
	list< CPtr<CAnimatedPart> > animatedParts;
	list< CPtr<CDynamicPart> > movingParts;
	list< CPtr<ILight> > lights;
	list< CPtr<CPolyline> > lines;
	list< CPtr<CSelection> > selections;
	list< CPtr<CParticles> > particles;
	ESceneRenderMode renderMode;
	CObj<CCVec3> pCamera;
	CDGPtr<CVersioningBase> pIgnoreStaticTrack;
	SHMatrix mHoldTransform;
	int nCurrentIgnoreMark;
	int nIgnoreListWasCalced;
	SGroupSelect holdMask;
	CObj<CAmbientMean> pAmbient;   // retail: mean of pTopAmbient/pBottomAmbient (was CObj<CCVec3>)
	CObj<CLightmapTracker> pLMTracker;
	CLightState currentLightState;
	vector<int> freeLightGroups;
	vector<CPtr<CLightGroup> > lightGroups;
	CDynamicLightCache dynamicLightCache;
	CObj<IMaterial> pTransparentMaterial;
	CDGPtr<CFakeParticleLMTexture> pFakeParticleLM;
	bool bLightStateCalced;
	int nFrameCounter;
	list< CPtr<CPostProcessBinder> > postprocessors;
	list< CPtr<CNonePart> > toBeLoaded;
	CObj<CDecalsManager> pDecalsManager;
	CObj<CCVec3> pTopAmbient, pBottomAmbient;
	// release save-format tags 33/34: animated top-ambient DG node + lighting-options bitmask.
	// pTopAmbientAnimator is the release-new animated-ambient feature; this predecessor render
	// never creates it (stays a null CObj -> serializes as a null ref).
	CObj<CAmbientAnimator> pTopAmbientAnimator;
	// bit 1 = scene receives no depth shadows (fast interface views), bit 2 = no CL updates
	int nLightingOptions = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&trackers); f.Add(3,&pVolume); f.Add(4,&dynamicFrags); f.Add(5,&animatedParts); f.Add(6,&movingParts); f.Add(7,&lights); f.Add(8,&lines); f.Add(9,&selections); f.Add(10,&particles); f.Add(11,&renderMode); f.Add(12,&pCamera); f.Add(13,&pIgnoreStaticTrack); f.Add(14,&mHoldTransform); f.Add(15,&nCurrentIgnoreMark); f.Add(16,&nIgnoreListWasCalced); f.Add(17,&holdMask); f.Add(18,&pAmbient); f.Add(19,&pLMTracker); f.Add(20,&currentLightState); f.Add(21,&freeLightGroups); f.Add(22,&lightGroups); f.Add(23,&dynamicLightCache); f.Add(24,&pTransparentMaterial); f.Add(25,&pFakeParticleLM); f.Add(26,&bLightStateCalced); f.Add(27,&nFrameCounter); f.Add(28,&postprocessors); f.Add(29,&toBeLoaded); f.Add(30,&pDecalsManager); f.Add(31,&pTopAmbient); f.Add(32,&pBottomAmbient); f.Add(33,&pTopAmbientAnimator); f.Add(34,&nLightingOptions); return 0; }
	int nSlowVolumeWalk;
	SParticleLMRenderTargetInfo particleLM;
	SGroupSelect lastMask;
	CObj<IHZBuffer> pHZBuffer;
	bool bWaitForLoad;
	// retail +0x1b8, not serialized: HSR_DYNAMIC frame counter -- reuse the ignore list while
	// moving, recalc via MakeInvisibleElementsListFast every 2nd frame (or on a big change)
	int nReuseIgnoreList;

	struct SDynamicLightGroup
	{
		SSphere bv;
		SDynamicAmbientInfo *pAmbient;

		SDynamicLightGroup() : pAmbient(0) {}
	};
	struct SSceneFragmentGroupInfo
	{
		SGroupSelect mask;
		CSceneFragments *pList;
		CSceneFragments *pAlienList;
		CTransparentRenderer *pTransp;
		CLightmapTracker *pLMTracker;
		vector<SDynamicLightGroup> groups;
		int nCurrentFrame;
		CVec3 vCamera;
		int nLMTextureUsed;
		IHZBuffer *pHZBuffer;
		NGfx::CTexture *pLMTexture;

		SSceneFragmentGroupInfo( const SGroupSelect &_mask, const CVec3 &_vCamera, CSceneFragments *_pList, CTransparentRenderer *_pTransp ) 
			: mask(_mask), pList(_pList), pTransp(_pTransp), pLMTracker(0), nCurrentFrame(0), vCamera(_vCamera), pAlienList(0), pHZBuffer(0), pLMTexture(0) {}
		SSceneFragmentGroupInfo( const SGroupSelect &_mask, const CVec3 &_vCamera, CSceneFragments *_pList, CTransparentRenderer *_pTransp, 
			CLightmapTracker *_pLMTracker, int _nFrame, 
			CSceneFragments *_pAlienList, int nGroupsNumber, IHZBuffer *_pHZBuffer, NGfx::CTexture *_pLMTexture )
			: mask(_mask), pList(_pList), pTransp(_pTransp), pLMTracker(_pLMTracker), nCurrentFrame(_nFrame), 
				vCamera(_vCamera), nLMTextureUsed(0), pAlienList(_pAlienList),
				groups( nGroupsNumber ), pHZBuffer(_pHZBuffer), pLMTexture(_pLMTexture) {}
	private:
		void FilterParts( vector<CPartFlags> *pRes, CTransformStack *pTS, CCombinedPart *p, ERLRequest req, int _nIgnoreMark );
		void AddTranspElement( CCombinedPart *p, const vector<CPartFlags> &flags );
		void AddDynamicLMElement( CCombinedPart *p, CTransformStack *pTS, ERLRequest req, int _nIgnoreMark );
		void AddStaticLMElement( CCombinedPart *p, CTransformStack *pTS, ERLRequest req, int _nIgnoreMark );
		void AddElement( CSceneFragments *pRes, CTransformStack *pTS, CCombinedPart *p, ERLRequest req, int _nIgnoreMark );
	public:
		void AddMaterialHolder( CTransformStack *pTS, const CVolumeNode::SPerMaterialHolder &h, ERLRequest req, int _nIgnoreMark );
		void CalcLightmaps( CGScene *pScene );
	};

	void AddLight( ILight *pGroup );
	void AddNodeParts( CTransformStack *pTS, list<SRenderPartSet> *pRes, CVolumeNode *pNode, ERLRequest eReq, const SGroupSelect &mask );
	void MakePartList( CTransformStack *pTS, list<SRenderPartSet> *pRes, ERLRequest req, const SGroupSelect &mask );
	void AddNode( CTransformStack *pTS, SSceneFragmentGroupInfo *pFragments, CVolumeNode *pNode, ERLRequest req, int nIgnoreMark );
	void MakeRenderList( CTransformStack *pTS, SSceneFragmentGroupInfo *pTarget, ERLRequest req, int nIgnoreMark );
	//void MakeTransparentList( CTransparentRenderer *pRes, CTransformStack *pTS, const SGroupSelect &mask );
	void RecalcRenderStats( int nSceneTris, int nParticles, int nLitParticles );
	void RecalcCullingInfo();
	void UpdateIgnoreMark( IRender *pRender, CTransformStack *pTS, const SGroupSelect &mask, EHSRMode hsrMode );
	bool TraceParts( ERLRequest req, const SGroupSelect &mask, CVolumeNode *pNode, const SRayInfo &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, CObjectBase **ppObject );
	void ResetDynamicLightmapsCache();
	void CalcNewLightState();
	void CheckDynamicLightmapCache();
	void DrawSelection( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask );
	void DrawPostProcess( CTransformStack *pTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask );
	void DrawLines( NGfx::CRenderContext *pRC );
	void RefreshParticleLMTarget();
	CLightmapTracker* GetLMTracker();
	void WalkNotLoadedObjects();
	void PrecacheMaterials( CVolumeNode *pNode );
	// ICLRender
	void RenderCL( NGfx::CRenderContext *pRC, IRender *pRender, CTransformStack *pTS, 
		CSceneFragments *pGeom, bool bSceneHasChanged );
	// IDecalQuery
	CObjectBase* CreateDecal( CNonePart *pTarget, const vector<CVec3> &srcPositions, const SDecalMappingInfo &_info, IMaterial *pMaterial );
	CObjectBase* CreateStaticDecal( CNonePart *pTarget, CPtrFuncBase<CObjectInfo> *pDecal, IMaterial *pMaterial, const SFullGroupInfo &fg );
	CObjectBase* CreateDynamicDecal( CNonePart *pTarget, CPtrFuncBase<CObjectInfo> *pDecal, IMaterial *pMaterial, const SFullGroupInfo &fg );
	void GetPartsList( const SDecalMappingInfo &_info, const CObjectBaseSet &targets, vector<CPtr<CNonePart> > *pRes );
public:
	CGScene();
	CGScene( int );
	// outer space integration
	virtual CLightGroup* CreateLightGroup();
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SFullGroupInfo &_ginfo );
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SFBTransform &trans, const SFullGroupInfo &_ginfo );
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		const SDiscretePos &trans, const SFullGroupInfo &_ginfo );
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		CFuncBase<SFBTransform> *pPlacement, const SFullGroupInfo &_ginfo );
	virtual CObjectBase* CreateGeometry( CPtrFuncBase<CObjectInfo> *pInfo, IMaterial *pMat, 
		CFuncBase<vector<SHMatrix> > *pPlacement, CFuncBase<vector<NGfx::SCompactTransformer> > *_pMMXAnim, 
		const SFullGroupInfo &_ginfo );
	virtual CObjectBase* CreateDynamicGeometry( CPtrFuncBase<CObjectInfo> *pInfo, CFuncBase<SFBTransform> *pPlacement, IMaterial *pMat,
		CFuncBase<SBound> *pBound, const SFullGroupInfo &_ginfo );
	//
	virtual CObjectBase* CreateParticles( CPtrFuncBase<CParticleEffect> *pInfo,
		CFuncBase<SFBTransform> *pPlacement, const SBound &bound, const SGroupInfo &_ginfo, int nPFlags );
	virtual CPolyline* CreatePolyline( CPtrFuncBase<NGfx::CGeometry> *pGeometry, const CVec3 &color );
	virtual CObjectBase* CreateSelection( CObjectBase *pRenderNode, const CVec4 &vColor );
	virtual CObjectBase* CreatePostProcessor( CObjectBase *pRenderNode, IPostProcess *pProcessor );
	virtual void SetAmbient( const CVec3 &vBottomAmbientColor, const CVec3 &vTopAmbientColor );
	virtual CObjectBase* AddDirectionalLight( CFuncBase<CVec3> *pColor, CFuncBase<CVec3> *pGlossColor, const CVec3 &vShadowColor, const CVec3 &ptLight, const CVec3 &ptOrigin, const CVec2 &ptSize, float fMaxHeight, bool bLightmapOnly, float fBlurShift );
	virtual CObjectBase* AddPointLight( const CVec3 &_vColor, const CVec3 &ptOrigin, float fR, bool bLightmapOnly );
	virtual CObjectBase* AddPointLight( CPtrFuncBase<CAnimLight> *pLight );
	virtual CObjectBase* AddSpotLight( CFuncBase<CVec3> *pColor, const CVec3 &ptOrigin, const CVec3 &ptDir, float fFOV, float fRadius, CPtrFuncBase<NGfx::CTexture> *pMask, bool bLightmapOnly );
	virtual CDecalTarget* CreateDecalTarget( const vector<CObjectBase*> &targets, const SDecalMappingInfo &_info );
	virtual CObjectBase* AddDecal( NGScene::CDecalTarget *pTarget, IMaterial *pMaterial );
	virtual void Draw( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, const SGroupSelect &mask, ERenderPath rp, EFogMode fogMode, const SFogParams &fog, EHSRMode hsrMode, ETransparentMode trMode, NGfx::CCubeTexture *pSky );
	virtual CVec2 GetScreenRect();
	virtual CFuncBase<CVec3>* GetCamera();
	virtual bool TraceScene( const SGroupSelect &mask, const CRay &r, float *pfT, CVec3 *pNormal, CVec3 *pColor, EScenePartsSet ps, CObjectBase **ppObject );
	void TraceDynamicAmbient( SDynamicAmbientInfo *pRes, const SSphere &bv );
	const SDynamicAmbientInfo& GetDynamicAmbient( int nGroup, const SSphere &bv );
	void FreeLightGroup( int n ) { lightGroups[n] = 0; freeLightGroups.push_back( n ); }
	virtual SGroupSelect GetLastMask() { return lastMask; }
	virtual void PrecacheMaterials();
	virtual void SetLightingOptions( int nOptions ) { nLightingOptions = nOptions; }   // retail @0x1673a0
	virtual int GetLightingOptions() { return nLightingOptions; }                      // retail @0x167380
	friend class CRenderWrapper;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderWrapper: public IRender
{
	CGScene *pScene;
public:
	CRenderWrapper( CGScene *_pScene ): pScene(_pScene) {}

	virtual void FormPartList( CTransformStack *pTS, list<SRenderPartSet> *pRes, EDepthType dt, const SGroupSelect &mask );
	virtual void FormDepthList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, EDepthType dt );
	virtual void FormDirOccludersList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, const SGroupSelect &gs, bool bFast );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
