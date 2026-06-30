#ifndef __GRenderCore_H_
#define __GRenderCore_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GRenderModes.h"
#include "Pool.h"
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec3 MulPerComp4( const CVec3 &a, const CVec3 &b ) { return CVec3(a.x*b.x*4, a.y*b.y*4, a.z*b.z*4); }
inline CVec3 MulPerComp2( const CVec3 &a, const CVec3 &b ) { return CVec3(a.x*b.x*2, a.y*b.y*2, a.z*b.z*2); }
inline CVec3 MulPerComp( const CVec3 &a, const CVec3 &b ) { return CVec3(a.x*b.x, a.y*b.y, a.z*b.z); }
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
namespace NGfx
{
	class CRenderContext;
	class CGeometry;
	class CTexture;
	class CCubeTexture;
	struct SEffect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class IPart;
class CSceneFragments;
class IMaterial;
////////////////////////////////////////////////////////////////////////////////////////////////////
//const TPartFlags PF_ALL_PARTS = 0xffffffff;
const int N_BLOCKS_IN_PART_FLAGS = 8;//16;
const int PF_MAX_PARTS_PER_COMBINER = N_BLOCKS_IN_PART_FLAGS * 32;
class CPartFlags
{
	int flags[N_BLOCKS_IN_PART_FLAGS];

	void Fill( int n ) { memset( flags, n, sizeof(flags) ); }
public:
	void Clear() { Fill(0); }
	void TakeAll() { Fill( 0xffffffff ); }
	void Set( int nIndex ) 
	{ 
		ASSERT( nIndex >= 0 && nIndex < PF_MAX_PARTS_PER_COMBINER );
		flags[ nIndex / 32 ] |= 1 << ( nIndex & 31 ); 
	}
	void Reset( int nIndex ) 
	{ 
		ASSERT( nIndex >= 0 && nIndex < PF_MAX_PARTS_PER_COMBINER );
		flags[ nIndex / 32 ] &= ~( 1 << ( nIndex & 31 ) );
	}
	int IsSet( int nIndex ) const 
	{ 
		ASSERT( nIndex >= 0 && nIndex < PF_MAX_PARTS_PER_COMBINER ); 
		return flags[ nIndex / 32 ] & ( 1 << nIndex ); 
	}
	bool IsEmpty() const { for ( int k = 0; k < GetBlocksNumber(); ++k ) { if ( flags[k] ) return false; } return true; }
	bool IsFullGet() const { for ( int k = 0; k < GetBlocksNumber(); ++k ) { if ( flags[k] != 0xffffffff ) return false; } return true; }
	void Invert() { for ( int k = 0; k < GetBlocksNumber(); ++k ) flags[k] = ~flags[k]; }
	int GetBlocksNumber() const { return N_BLOCKS_IN_PART_FLAGS; }
	int GetBlock( int n ) const { ASSERT( n >= 0 && n < GetBlocksNumber() ); return flags[n]; }
	void CalcAnd( const CPartFlags &a )
	{
		ASSERT( GetBlocksNumber() == a.GetBlocksNumber() );
		for ( int k = 0; k < GetBlocksNumber(); ++k )
			flags[k] &= a.flags[k];
	}
	void CalcOr( const CPartFlags &a )
	{
		ASSERT( GetBlocksNumber() == a.GetBlocksNumber() );
		for ( int k = 0; k < GetBlocksNumber(); ++k )
			flags[k] |= a.flags[k];
	}
	CPartFlags& operator|=( const CPartFlags &a ) { CalcOr(a); return *this; }
	CPartFlags& operator&=( const CPartFlags &a ) { CalcAnd(a); return *this; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator==( const CPartFlags &a, const CPartFlags &b ) { return memcmp( &a, &b, sizeof(CPartFlags) ) == 0; }
inline bool operator!=( const CPartFlags &a, const CPartFlags &b ) { return memcmp( &a, &b, sizeof(CPartFlags) ) != 0; }
inline CPartFlags operator~( const CPartFlags &f ) { CPartFlags res( f ); res.Invert(); return res; }
inline CPartFlags operator&( const CPartFlags &a, const CPartFlags &b ) { CPartFlags r(a); r &= b; return r; }
inline CPartFlags operator|( const CPartFlags &a, const CPartFlags &b ) { CPartFlags r(a); r |= b; return r; }
inline CPartFlags TakeAllParts() { CPartFlags r; r.TakeAll(); return r; }
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderGeometryInfo;
struct SRenderPartSet
{
	CPtr<CObjectBase> pNode;
	const vector< CPtr<IPart> > *pParts;
	CPartFlags parts, castShadow;
	SRenderGeometryInfo *pGeometry;

	SRenderPartSet() {}
	SRenderPartSet( CObjectBase *_pNode, const vector< CPtr<IPart> > *_pParts, SRenderGeometryInfo *_pGeometry ) 
		: pNode(_pNode), pParts(_pParts), pGeometry(_pGeometry) 
	{ 
		parts.Clear();
	}
	//SRenderPartSet( CObjectBase *_pNode ): pNode(_pNode) {}
	IPart* GetPart( int nIndex ) const { return (*pParts)[ nIndex ]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroupSelect;
class IRender
{
public:
	enum EDepthType
	{
		DT_STATIC,
		DT_DYNAMIC,
		DT_ALL
	};
	
	virtual void FormPartList( CTransformStack *pTS, list<SRenderPartSet> *pRes, EDepthType dt, const SGroupSelect &mask ) = 0;
	virtual void FormDepthList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, EDepthType dt ) = 0;
	virtual void FormDirOccludersList( CTransformStack *pTS, const CVec3 &vDir, CSceneFragments *pRes, const SGroupSelect &gs, bool bFast ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// node that can render something somewhere
enum ETrilistType
{
	TLT_POSITION,
	TLT_GEOM,
	TLT_NUMBER
};
class IVBCombiner : public CPtrFuncBase<NGfx::CGeometry>
{
protected:
	ZDATA
	SBound bound;
	vector<SSphere> partBVs;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bound); f.Add(3,&partBVs); return 0; }
	virtual const SBound& GetBound() { ASSERT( IsFrameMatch() ); return bound; }
	virtual const vector<SSphere>& GetBounds() { ASSERT( IsFrameMatch() ); return partBVs; }
	int GetPartsNum() const { return partBVs.size(); }
};
struct SRenderGeometryInfo
{
	CDGPtr<IVBCombiner> pVertices;
	CDGPtr< CFuncBase<vector<NGfx::STriangleList> > > pTriLists[TLT_NUMBER];

	int operator&( CStructureSaver &f ) 
	{ 
		f.Add(1,&pVertices); 
		for ( int k = 0; k < TLT_NUMBER; ++k )
			f.Add(2,&pTriLists[k], k + 1 );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! intermediate per fragment operations representation
enum EStencilBlendingOp
{
	//	STM_TEST = 1,
	STM_NONE = 0,
	STM_LIGHT = 1, // set 0x80 bit
	STM_STENCIL_LIGHT = 2,
	STM_TEST_STENCIL_LIGHT = 3,
	STM_MARK = 4, // fills with 0x80
	STM_TEST_CLEAR_MARK = 5, // test highest bit and clear
	STM_MARK_2 = 6, // set 0x40
	STM_MASK = 7,

	DPM_NORMAL   = 0,
	DPM_EQUAL    = 8,
	DPM_TESTONLY = 16,
	DPM_NONE     = 24,
	DPM_MASK     = 24,

	ABM_NONE     = 0,
	ABM_ZERO     = 32,
	ABM_ADD      = 64,
	ABM_MUL      = 96,
	ABM_SRC_AMUL = 128,
	ABM_ALPHA_BLEND = 160,
	ABM_SMART = 192,
	ABM_MUL2     = 224,
	ABM_MASK     = 224
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDirectionalDepthInfo
{
	CVec4 vChannelSelect;
	CVec4 vDepth;
	CVec4 vVecU, vVecV;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPerspDirectionalDepthInfo
{
	CVec4 vChannelSelect;
	CVec4 vDepth;
	SHMatrix m;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSkyDepth3Info
{
	SDirectionalDepthInfo *channels[3];
	CVec3 vDirs[3];
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ERenderOperation
{
	RO_NOP,
	//
	// TnL path
	RO_TNL_DIR_AMB_LIT_DIFFUSE_SOLID,
	RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE,
	RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE_AT,
	RO_TNL_DIR_AMB_LIT_DIFFUSE_TEXTURE_DECAL,
	RO_TNL_TEXTURE,
	RO_TNL_TEXTURE_AT,
	RO_TNL_SOLID,
	// general ops
	RO_TEXTURE,
	RO_TEXTURE_AT,
	RO_TEXTURE_DECAL,
	//RO_SKYMAP,
	RO_SOLID_COLOR,
	// ambient
	RO_AMB_LIT_SOLID,
	RO_AMB_LIT_TEXTURE,
	RO_AMB_LIT_TEXTURE_DECAL,
	RO_AMB_LIT_TEXTURE_AT,
	// dynamic lightmaps
	RO_DYNAMIC_LIGHTMAP,
	// lightmapped stuff
	RO_LMPD_SOLID,
	RO_LMPD_TEXTURE,
	RO_LMPD_TEXTURE_AT,
	RO_LMPD_TEXTURE_DECAL,
	RO_DYN_LMPD_SOLID,
	RO_DYN_LMPD_TEXTURE,
	RO_DYN_LMPD_TEXTURE_AT,
	RO_DYN_LMPD_TEXTURE_DECAL,
	// directional
	RO_DIFFUSE_DIR_LIT_TEXTURE, // copies alpha
	RO_DIFFUSE_DIR_LIT_TEXTURE_PP,
	RO_DIFFUSE_BUMP_DIR_LIT_TEXTURE,
	RO_DIFFUSE_BUMP_DIR_LIT_TEXTURE_PP,
	RO_DIR_LIT_SOLID,
	RO_DIR_LIT_SOLID_PP,
	RO_DIR_DEPTH,
	RO_DIR_DEPTH_MAX, // not used
	RO_DIR_SHADOW_TEST,
	RO_DIR_SHADOW_TEST_SMOOTHED,
	RO_TEXTURE_AT_CONSERVATIVE,
	RO_DIR_PARTICLE_LM_SHADOW_TEST,
	RO_DIR_PARTICLE_LM_SOFT_SHADOW_TEST,
	// ambient + directional
	RO_DIR_AMB_LIT_SOLID,
	RO_DIR_AMB_LIT_DIFFUSE_TEXTURE,
	RO_DIR_AMB_LIT_DIFFUSE_TEXTURE_AT,
	RO_DIR_AMB_LIT_DIFFUSE_TEXTURE_DECAL,
	// fast full light ops
	RO_DIFFUSE_FULL_LIT_TEXTURE_PP,
	RO_DIFFUSE_FULL_LIT_BUMP_TEXTURE_PP,
	RO_PP_SPECULAR_FULL_COLOR_DIR,
	RO_PP_SPECULAR_FULL_TEXTURE_DIR,
	RO_FULL_LIT_SOLID_PP,
	RO_DYNLM_LIT_SOLID_PP,
	RO_DIFFUSE_DYNLM_LIT_BUMP_TEXTURE_PP,
	RO_DIFFUSE_DYNLM_LIT_TEXTURE_PP,
	// point light
	RO_PNT_LIT_SOLID,
	RO_PNT_LIT_TEXTURE, // copies alpha
	RO_PNT_LIT_TEXTURE_PBUMP,
	RO_PNT_LIT_SOLID16,
	RO_PNT_LIT_TEXTURE16, // copies alpha
	RO_PNT_LIT_TEXTURE_PBUMP16,
	RO_PNT_CUBEMAP_DEPTH,
	// fog
	RO_FOG_STATIC,
	RO_FOG_DYNAMIC,
	// mirrors
	RO_MIRROR,
	RO_GLOSSED_MIRROR,
	RO_BUMPED_MIRROR,
	RO_BUMPED_FRESNEL,
	RO_REG_MUL_TEXTURE,
	// per pixel specular
	RO_NHCALC,
	RO_NHCALC_BUMP,
	RO_PP_SPECULAR_COLOR_DIR,
	RO_PP_SPECULAR_TEXTURE_DIR,
	RO_PP_SPECULAR_COLOR_PNT,
	RO_PP_SPECULAR_TEXTURE_PNT,
	// cached light calcs
	RO_CL_SKY_3LIGHT,
	RO_CL_SKY_DIR_CHECK,
	RO_CL_SKY_LIGHT,
	RO_CL_SKY_LIGHT_BUMP,
	RO_CL_PNT_LIGHT,
	RO_CL_PNT_DEPTH_CHECK,
	RO_CL_PNT_LIGHT_SHADOWED,
	RO_CL_PNT_LIGHT_BUMP,
	// lightmap calcs
	//RO_LM_MODULATE,
	//RO_LM_SKY_SQRT_MODULATE,
	//RO_LM_SKY_3LIGHT,
	//RO_LM_DIR_LIT_SHADOWED,
	//RO_LM_SPOT_DEPTH_CHECK,
	//RO_LM_SPOT_LIGHT,
	// effects
	RO_ALIEN,
	RO_EXPLOSION_DECAL,
	RO_REGISTER
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_DYNAMIC_AMBIENT_INFO_COMPONENTS = 6;
struct SDynamicAmbientInfo
{
	struct SPad
	{
		CVec3 v;
		float f;
	};
	SPad vXPos, vXNeg, vYPos, vYNeg, vZPos, vZNeg; // CVec4 to make vs registers load easier

	CVec4* GetVec4() const { return (CVec4*)&vXPos; }
	void Clear() { SPad z; z.v = CVec3(0,0,0); vXPos = vXNeg = vYPos = vYNeg = vZPos = vZNeg = z; }
	void AddLight( const CVec3 &vColor, const CVec3 &vDir )
	{
		vXPos.v += vColor * Max( vDir.x, 0.0f );
		vXNeg.v += vColor * Max( -vDir.x, 0.0f );
		vYPos.v += vColor * Max( vDir.y, 0.0f );
		vYNeg.v += vColor * Max( -vDir.y, 0.0f );
		vZPos.v += vColor * Max( vDir.z, 0.0f );
		vZNeg.v += vColor * Max( -vDir.z, 0.0f );
	}
	void Blend( SDynamicAmbientInfo &a, float fSrc )
	{
		vXPos.v = fSrc * vXPos.v + ( 1 - fSrc ) * a.vXPos.v;
		vXNeg.v = fSrc * vXNeg.v + ( 1 - fSrc ) * a.vXNeg.v;
		vYPos.v = fSrc * vYPos.v + ( 1 - fSrc ) * a.vYPos.v;
		vYNeg.v = fSrc * vYNeg.v + ( 1 - fSrc ) * a.vYNeg.v;
		vZPos.v = fSrc * vZPos.v + ( 1 - fSrc ) * a.vZPos.v;
		vZNeg.v = fSrc * vZNeg.v + ( 1 - fSrc ) * a.vZNeg.v;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderStaticInfo
{
	//SRenderGeometryInfo *pGeometry;
	//CPtr<IMaterial> pMaterial;
	CPtr<CObjectBase> pHandle;
	SBound bv;
	//bool bSkipPerPartTests;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderFragmentInfo
{
	struct SElement
	{
		short nGeometry, nBlock;
		int nFlags;

		SElement() {}
		SElement( short _nGeom, short _nBlock, int _nFlags ) : nGeometry(_nGeom), nBlock(_nBlock), nFlags(_nFlags) {}
	};
	vector<SElement> elements;
	CPtr<IMaterial> pMaterial;
	CPtr<NGfx::CTexture> pLightmap;
	const SDynamicAmbientInfo *pLM;

	SRenderFragmentInfo() : pLM(0) {}
};
struct SRenderFragmentKey
{
	IMaterial *pMat;
	NGfx::CTexture *pLightmap;
	const SDynamicAmbientInfo *pLM;
	bool operator==( const SRenderFragmentKey &a ) const { return pMat == a.pMat && pLightmap == a.pLightmap && pLM == a.pLM; }
};
struct SRenderFragmentHash
{
	int operator()( const SRenderFragmentKey &a ) const { return (int)a.pMat + (int)a.pLM; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFragmentsSplit
{
	FST_ACCEPT,
	FST_REJECT,
	FST_SPLIT
};
class CSceneFragments
{
private:
	int nSceneTris;
	CPool<SRenderStaticInfo,128> staticInfos;
	CPool<SDynamicAmbientInfo,128> dynamicLMinfos;
	CPool<SRenderGeometryInfo,128> geometryInfos;
	CPool<SRenderFragmentInfo,128> fragmentInfos;
	
	typedef unordered_map<SRenderFragmentKey, int, SRenderFragmentHash> CFragmentHash;
	CFragmentHash fragmentHash;
	vector<SRenderStaticInfo*> statics;
	vector<SRenderGeometryInfo*> geometries;
	vector<SRenderFragmentInfo*> fragments;
	vector<char> filterFragment;
	vector<char> filterGeometry;
	vector<CPartFlags> selectedParts;
public:
	CSceneFragments();
	int AddGeometry( CObjectBase *pHandle, SRenderGeometryInfo *pGeometry, const SBound &_bv );
	void AddElement( int nGeometryIndex, const CPartFlags &_parts, IMaterial *pMaterial, 
		NGfx::CTexture *pLightmap, const SDynamicAmbientInfo *pLM );
	void AddLitParticles( IVBCombiner *pCombiner, CFuncBase<vector<NGfx::STriangleList> > *pTris, int nPart, const SBound &_bv );
	void SetLitParticlesMaterial( IMaterial *p );
	
	SDynamicAmbientInfo* AllocDynamicAmbient() { return dynamicLMinfos.Alloc(); }
	int GetSceneTris() const { return nSceneTris; }
	// starts with 1st element, 0th is lit particles
	bool IsFilteredFragment( unsigned int n ) const { if ( n >= filterFragment.size() ) return false; return filterFragment[n]; }
	EFragmentsSplit GetGeometryFlags( unsigned int n ) const { if ( n >= filterGeometry.size() ) return FST_ACCEPT; return (EFragmentsSplit)filterGeometry[n]; }
	const CPartFlags& GetGeometryParts( unsigned int n ) const { return selectedParts[n]; }
	bool HasSelectedFragments() const;
	const vector<SRenderFragmentInfo*>& GetFragments() const { return fragments; }
	const SRenderFragmentInfo& GetLitParticles() const { return *fragments[0]; }
	const SRenderStaticInfo& GetStaticInfo( int nGeom ) const { return *statics[ nGeom ]; }
	SRenderGeometryInfo* GetGeometryInfo( int nGeom ) const { return geometries[ nGeom ]; }
	friend class CSelectFragments;
	friend class CSelectGeometries;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelectGeometries
{
	CSceneFragments *pScene;
	vector<char> holdFlags;
	vector<CPartFlags> holdParts;
public:
	template<class T>
		CSelectGeometries( CSceneFragments *_pScene, const T &select ) : pScene(_pScene) 
	{
		holdFlags = pScene->filterGeometry;
		holdParts = pScene->selectedParts;
		if ( holdFlags.empty() )
		{
			pScene->filterGeometry.resize( pScene->geometries.size(), 0 );
			CPartFlags f;
			f.TakeAll();
			pScene->selectedParts.resize( pScene->geometries.size(), f );
		}
		for ( int k = 0; k < pScene->filterGeometry.size(); ++k )
		{
			if ( pScene->filterGeometry[k] == FST_REJECT )
				continue;
			CPartFlags &parts = pScene->selectedParts[k];
			EFragmentsSplit res = select( pScene->statics[k], pScene->geometries[k], &parts );
			if ( res == FST_SPLIT && parts.IsEmpty() )
				res = FST_REJECT;
			pScene->filterGeometry[k] = res;
		}
	}
	~CSelectGeometries() { pScene->filterGeometry = holdFlags; pScene->selectedParts = holdParts; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelectFragments
{
	CSceneFragments *pScene;
	vector<char> holdFilter;
public:
	template<class T>
		CSelectFragments( CSceneFragments *_pScene, const T &select ) : pScene(_pScene) 
	{
		holdFilter = pScene->filterFragment;
		if ( holdFilter.empty() )
			pScene->filterFragment.resize( pScene->fragments.size(), 0 );
		for ( int k = 0; k < pScene->filterFragment.size(); ++k )
		{
			if ( pScene->filterFragment[k] )
				continue;
			pScene->filterFragment[k] = select( pScene->fragments[k] );
		}
	}
	~CSelectFragments() { pScene->filterFragment = holdFilter; }
//	CSceneFragments *pScene;
//	template<class T>
//		SSelectFragments( CSceneFragments *_pScene, const T &select ) : pScene(_pScene) {}// pScene->Select( *this, select ); }
//		~SSelectFragments() {}// pScene->Merge(); }
/*	EFragmentsSplit Split( const SRenderFragmentInfo &original, const CPartFlags &accepted, const CPartFlags &rejected ) const 
	{
		if ( accepted.IsEmpty() )
			return FST_REJECT;
		if ( rejected.IsEmpty() )
			return FST_ACCEPT;
		SRenderFragmentInfo *pRes = pScene->fragments.Alloc();
		*pRes = original;
		pRes->parts = accepted;
		pScene->pTarget->push_back( pRes ); 
		pRes = pScene->fragments.Alloc();
		*pRes = original;
		pRes->parts = rejected;
		pScene->pRejected->push_back( pRes ); 
		return FST_SPLIT;
	}*/
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// bit set to 1 means part is ignored
typedef unordered_map<CPtr<CObjectBase>,CPartFlags,SPtrHash> CFilterPartsHash;
struct SLightmappedFilter
{
	bool operator()( SRenderFragmentInfo *pF ) const { return pF->pLightmap == 0; }
};
struct SNonLightmappedFilter
{
	bool operator()( SRenderFragmentInfo *pF ) const { return pF->pLightmap != 0; }
};
struct SBoundIntersectFilter
{
	const SBound &bv;
	SBoundIntersectFilter( const SBound &_bv ) : bv(_bv) {}
	EFragmentsSplit operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const;
};
struct SFrustrumFilter
{
	CTransformStack *pTS;
	SFrustrumFilter( CTransformStack *_pTS ) : pTS(_pTS) {}
	EFragmentsSplit operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const;
};
struct SSphereFilter
{
	const SSphere &sph;
	SSphereFilter( const SSphere &_sph ) : sph(_sph) {}
	EFragmentsSplit operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const;
};
/*struct SSphereAndIgnoredFilter
{
	const SSphere &sph;
	const CFilterPartsHash &ignoreList;
	SSphereAndIgnoredFilter( const SSphere &_sph, const CFilterPartsHash &_ignoreList )
		: sph(_sph), ignoreList(_ignoreList) {}
	bool operator()( SRenderFragmentInfo *pF, const SSelectFragments &selector ) const;
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderCmdList
{
public:
	union UParameter
	{
		const CVec3 *pVec3;
		const CVec4 *pVec4;
		const SFogParams *pFog;
		NGfx::CTexture *pTex;
		NGfx::CCubeTexture *pCubeTex;
		const SDirectionalDepthInfo *pDirDepth;
		const SPerspDirectionalDepthInfo *pPDirDepth;
		const SDynamicAmbientInfo *pDynamicAmbientInfo;
		const SSkyDepth3Info *pSkyDepth3;
		float f;

		UParameter() : f(0) {}
		UParameter( const CVec3 *v3 ): pVec3(v3) {}
		UParameter( const CVec4 *v4 ): pVec4(v4) {}
		UParameter( const SFogParams *_p ): pFog(_p) {}
		UParameter( NGfx::CTexture *_pTex ): pTex(_pTex) {}
		UParameter( NGfx::CCubeTexture *_pTex ): pCubeTex(_pTex) {}
		UParameter( const SDirectionalDepthInfo *_pDirDepth ): pDirDepth(_pDirDepth) {}
		UParameter( const SPerspDirectionalDepthInfo *_pDirDepth ): pPDirDepth(_pDirDepth) {}
		UParameter( const SDynamicAmbientInfo *_p ): pDynamicAmbientInfo(_p) {}
		UParameter( const SSkyDepth3Info *_p ): pSkyDepth3(_p) {}
		UParameter( float _f ): f(_f) {}
	};
	struct SOperation
	{
		const SRenderFragmentInfo *pFrag;
		ERenderOperation op;
		unsigned char nPass;
		unsigned char nStencilBlendMode; // stencil mode & blend mode
		char nDestRegister;
		UParameter p1, p2, p3;

		SOperation() {}
		SOperation( const SRenderFragmentInfo *_pFrag, ERenderOperation _op, unsigned char _nPass, 
			unsigned char _nSBM, unsigned char _nRegister, UParameter _p1 = UParameter(float(0)), UParameter _p2 = UParameter(float(0)), UParameter _p3 = UParameter(float(0)) )
			: pFrag(_pFrag), op(_op), nPass(_nPass), nStencilBlendMode(_nSBM), nDestRegister(_nRegister),
			p1(_p1), p2(_p2), p3(_p3) {}
		bool IsSame( const SOperation &a ) const
		{
			ASSERT( sizeof(p1.f) == sizeof(p1) );
			return op == a.op && nPass == a.nPass && nStencilBlendMode == a.nStencilBlendMode && 
				nDestRegister == a.nDestRegister && p1.f == a.p1.f && p2.f == a.p2.f && p3.f == a.p3.f;
		}
	};
	vector<SOperation> ops;

	CRenderCmdList() {}
	bool IsEmpty() const { return ops.empty(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SOpGenContext
{
	vector<CRenderCmdList::SOperation> *pRes;
	const SRenderFragmentInfo *pCurFragment;
	bool bHasAddedOps;

	void AddOperation(
		ERenderOperation _op, unsigned char _nPass, int nSBM, char nDestRegister, 
		CRenderCmdList::UParameter p1 = CRenderCmdList::UParameter(), 
		CRenderCmdList::UParameter p2 = CRenderCmdList::UParameter(), 
		CRenderCmdList::UParameter p3 = CRenderCmdList::UParameter() )
	{
		ASSERT( pCurFragment );
		pRes->push_back( 
			CRenderCmdList::SOperation( pCurFragment, _op, _nPass, nSBM, nDestRegister, p1, p2, p3 )
			);
		bHasAddedOps = true;
	}
	SOpGenContext( vector<CRenderCmdList::SOperation> *_pRes, const SRenderFragmentInfo *_pTarget )
		: pRes(_pRes), pCurFragment(_pTarget), bHasAddedOps(false) {}
	bool HasAddedOps() const { return bHasAddedOps; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeSingleOp( CRenderCmdList *pRes, CSceneFragments &src, bool bTakeLitParticles, ERenderOperation op,
	CRenderCmdList::UParameter _p1 = CRenderCmdList::UParameter(),
	CRenderCmdList::UParameter _p2 = CRenderCmdList::UParameter(),
	CRenderCmdList::UParameter _p3 = CRenderCmdList::UParameter(),
	int nStencilOp = 0 );
void AddFinalOps( CRenderCmdList *pRes, CSceneFragments &src, ERenderPath rm, 
	ERenderOperation op, CRenderCmdList::UParameter _p1 );
void SplitOps( CRenderCmdList *pLower, CRenderCmdList *pSrc, int nHighPass );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMaterialInfo
{
	enum EMaterialType
	{
		NORMAL,
		DECAL,
		EXACT_DECAL,
		SELF_ILLUM
	};
	enum EType
	{
		T_NONE,
		T_COLOR,
		T_TEXTURE,
		T_VSPEC  // variable spec power
	};
	struct SColorInfo
	{
		ZDATA
		EType type;
		CVec3 color;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&type); f.Add(3,&color); return 0; }
		NGfx::CTexture *pTex;

		SColorInfo(): type(T_NONE) {}
	};
	ZDATA
	EMaterialType mt;
	bool bAlphaTest;
	SColorInfo diffuse, specular;
	float fSpecPower;
	float fTransp; // for TREE type
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&mt); f.Add(3,&bAlphaTest); f.Add(4,&diffuse); f.Add(5,&specular); f.Add(6,&fSpecPower); f.Add(7,&fTransp); return 0; }
	NGfx::CTexture *pBump;

	SMaterialInfo(): mt(NORMAL), bAlphaTest(false), pBump(0), fSpecPower(0), fTransp(0) {}
	bool IsDecal() const { return mt == DECAL || mt == EXACT_DECAL; }
	bool IsSelfIllum() const { return mt == SELF_ILLUM; }
	bool IsSolid() const { return mt != DECAL && mt != EXACT_DECAL; }
	char GetDecalDepthTest() const { return mt == EXACT_DECAL ? DPM_EQUAL : DPM_TESTONLY; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STransparentMaterialInfo
{
	ZDATA
	float fMetalMirror, fDielMirror;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fMetalMirror); f.Add(3,&fDielMirror); return 0; }
	NGfx::CTexture *pTex;

	STransparentMaterialInfo() : fMetalMirror(0), fDielMirror(0) {}
};
inline bool operator==( const STransparentMaterialInfo &a, const STransparentMaterialInfo &b )
{
	return a.fMetalMirror == b.fMetalMirror && a.fDielMirror == b.fDielMirror && a.pTex == b.pTex;
}
inline bool operator!=( const STransparentMaterialInfo &a, const STransparentMaterialInfo &b ) { return !(a==b); }
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMaterial: public CObjectBase
{
public:
	enum EMaterialType
	{
		MT_NORMAL,
		MT_TRANSPARENT,
		MT_OCCLUDER,
		MT_ALIEN
	};
	virtual EMaterialType GetType() const { return MT_NORMAL; }
	virtual bool IsDecal() const { return false; }
	virtual IMaterial* GetExactDecal() { return 0; }
	//virtual void ExecTransparent( SRenderGeometryInfo *pElement, NGfx::CRenderContext *pRC, NGfx::CTexture *pFog ) { ASSERT( 0 ); }
	virtual bool DoesCastShadow() const = 0;
	virtual bool IsAlphaTest() const { return false; }
	virtual void AddOperations( SOpGenContext *p, ERenderPath rm ) {}
	virtual void AddATOperations( SOpGenContext *p ) {}
	virtual const SMaterialInfo& GetMaterialInfo() { return *(SMaterialInfo*)0; }
	virtual const STransparentMaterialInfo& GetTransparentInfo() { return *(STransparentMaterialInfo*)0; }
	virtual CVec3 GetAverageColor() const { return CVec3(1,1,1); }
	virtual bool Is2Sided() const { return false; }
	virtual bool IsSolid() const { return false; }
	virtual void Precache() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SParticleLMRenderTargetInfo
{
	SHMatrix rootTransform;
	CObj<NGfx::CTexture> pParticleLMs;
	CVec2 vParticleLMSize, vKernelSize;

	SParticleLMRenderTargetInfo() : vParticleLMSize(0,0), vKernelSize(0,0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ILight: public CObjectBase
{
public:
	virtual int GetPriority() const = 0;
	virtual void Render( CTransformStack *pTS, CTransformStack *pClipTS, NGfx::CRenderContext *pRC, ERenderPath renderPath, 
		IRender *pRender, CSceneFragments &scene, const SParticleLMRenderTargetInfo &particleLM ) = 0;
	virtual bool CheckCulling( CTransformStack *pTS ) { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
