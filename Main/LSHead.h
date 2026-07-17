#ifndef __LSHEAD_H_
#define __LSHEAD_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
#include "GResource.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataFaceGen.h"   // NDb::CRace complete type (CHeadInfo::pBodyColor CDBPtr factory)
#include <LifeStudioHeadAPI.h>
#include <LifeStudioHeadAPIMMTS.h>
#include <LifeStudioHeadAPITransform.h>   // LifeStudioHeadAPI::ITransformer (CHeadTransformInfo value)
#include <LifeStudioHeadAPIGDP.h>         // LifeStudioHeadAPI::IGDPFile / IGDPObject (morphable-head geometry source)
#include <LifeStudioHeadAPIInit.h>        // LifeStudioHeadAPI::Init() -- mandatory 2004 API init (see LSHead.cpp EnsureLSInit)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CHead;
	class CSequence;
	class CComplexHead;
	class CTMaterial;
	class CTRndModel;
}
namespace NGfx
{
	class CTexture;
}
namespace NGScene
{
	class CObjectInfo;
	class CSWTextureData;   // versioned SW-texture node referenced by NLSHead::SMixTex (LSFaceGen)
}
namespace NLSHead
{
class CHeadTransformInfo;   // the live head-morph tension source (defined below; CHeadAnimator weak-refs it)
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
class CLSPtr
{
	T *ptr;
public:
	CLSPtr( T *_ptr = 0 ): ptr(_ptr) {}
	~CLSPtr() { if ( ptr ) ptr->Destroy(); }

	CLSPtr& operator=( T *_ptr ) { if ( ptr ) ptr->Destroy(); ptr = _ptr; return *this; }
	T* operator->() const { return ptr; }
	operator T*() const { return ptr; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadMeshInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeadMeshInfo);
public:
	vector< CLSPtr<LifeStudioHeadAPI::IAnimator> > pLSAnimators;
	vector< int > nVertices;
	vector< CTPoint<int> > copys;
	vector< CVec2 > UVs;
	vector< WORD > indices;
	vector< int > tris;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadSequenceInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeadSequenceInfo);
public:
	CLSPtr<LifeStudioHeadAPI::ISequencer> pLSSequence;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadMeshLoader: public NGScene::CResourceLoader<int, CHeadMeshInfo>
{
	OBJECT_BASIC_METHODS(CHeadMeshLoader);
protected:
	virtual void Recalc();	
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadSequenceLoader: public NGScene::CResourceLoader<int, CHeadSequenceInfo>
{
	OBJECT_BASIC_METHODS(CHeadSequenceLoader);
protected:
	virtual void Recalc();	
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadBound: public CFuncBase<SBound>
{
	OBJECT_BASIC_METHODS(CHeadBound);
	ZDATA
	CDGPtr< CFuncBase<SFBTransform> > pParent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pParent); return 0; }
protected:
	virtual bool NeedUpdate() { return pParent.Refresh(); }
	virtual void Recalc();
public:
	CHeadBound() {}
	CHeadBound( CFuncBase<SFBTransform> *_pParent ): pParent(_pParent) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Ambient facial-idle state of a head animator (release enum NLSHead::EIdleType, PDB values). The
// refcount-scoped CIdleHead token (LSController.h) flips IDLE_NONE <-> IDLE_NORMAL while some view
// renders the head; CHeadsController::KillHead forces IDLE_DEATH (the frozen death mask).
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EIdleType
{
	IDLE_NORMAL = 0,
	IDLE_NONE   = 1,
	IDLE_DEATH  = 2,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// One playing facial sequence of a CHeadAnimator (release NLSHead::SSequence, 16 bytes; operator&
// @0x2686a0 tags 2..6). bIdle marks the ambient idles Recalc arms/expires itself (blinks); bMask marks
// the mask leg of the release two-sequence PlaySequence (pruned whenever no real sequence is playing).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSequence
{
	ZDATA
	CDGPtr< CPtrFuncBase<CHeadSequenceInfo> > pSequence;
	STime tStart;
	bool bCycle;
	bool bIdle;
	bool bMask;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSequence); f.Add(3,&tStart); f.Add(4,&bCycle); f.Add(5,&bIdle); f.Add(6,&bMask); return 0; }
	SSequence(): tStart(0), bCycle(false), bIdle(false), bMask(false) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadAnimator -- the head-mesh GENERATOR node itself. Retail PDB: base CPtrFuncBase<NGScene::CObjectInfo>
// (size 0x3c; inherited pValue @+0x14), pTime @+0x18, pHead @+0x20, sequences @+0x28, eIdleType @+0x34,
// bDeathMask @+0x38. The render part's IPart::pObjInfo references THIS object (CGameView::CreateLSHead
// @0x188c90 -> CGScene::CreateDynamicGeometry @0x15cd20), and it is saveload-registered (0x10942151,
// factory NewCHeadAnimator @0x267bb0), so a retail save round-trips the whole head graph. The generated
// CObjectInfo (pValue) is NOT serialized: the first post-load IPart::RefreshObjectInfo (@0xfe340)
// regenerates it via Recalc (@0x265a30). Retail has NO separate "CHead" mesh node -- the former dev
// CFuncBase<SHeadFrame>-animator + CHead split could not resolve retail saves' pObjInfo references.
class CHeadAnimator: public CPtrFuncBase<NGScene::CObjectInfo>
{
	OBJECT_BASIC_METHODS(CHeadAnimator);
	ZDATA
	CDGPtr< CFuncBase<STime> > pTime;              // retail +0x18 (CFuncBase<unsigned long>; STime == DWORD)
	CDGPtr< CPtrFuncBase<CHeadMeshInfo> > pHead;   // retail +0x20 (CHeadMeshLoader / CFaceGenMeshHolder)
	vector<SSequence> sequences;                   // retail +0x28: everything playing now (@0x265a30/@0x265890)
	EIdleType eIdleType = IDLE_NONE;               // retail +0x34 (GetAnimator creates with IDLE_NONE)
	bool bDeathMask = false;                       // retail +0x38: death idle finished, face frozen on it
	// retail operator& @0x268530: 2=pTime, 3=pHead (object refs), 4=sequences (DoVector<SSequence>),
	// 5=eIdleType (4B), 6=bDeathMask (1B). pValue is never serialized. (The pre-parity dev tag set --
	// 2 pDbHead / 5-7 legacy single sequence / 8 bStatic / 9-11 parked retail members -- is GONE; dev
	// saves written before this renumbering do not load, retail v1.2 saves do.)
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&pTime); f.Add(3,&pHead); f.Add(4,&sequences); f.Add(5,&eIdleType); f.Add(6,&bDeathMask);
		return 0;
	}
	// Optional live macro-muscle MORPH source (release-new; NOT serialized -- a transient preview ref):
	// when set, Recalc applies its mmTensions (the advanced FaceGen sliders) to the head animators over
	// the idle pose, before ComputePhysics. Weak (CPtr) so it does not own the CHeadTransformInfo.
	CDGPtr< CHeadTransformInfo, CPtr<CHeadTransformInfo> > pHeadTransformInfo;
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	CHeadAnimator() {}
	CHeadAnimator( CFuncBase<STime> *_pTime, NDb::CHead *_pDbHead );
	// Baked static head (advanced FaceGen commit): _pStaticMesh is the CFaceGenMeshHolder carrying the
	// morph. No extra animator state -- retail keys static heads purely off CHeadInfo::pMesh, and the
	// per-segment Recalc loop handles the holder's single whole-head animator naturally.
	CHeadAnimator( CFuncBase<STime> *_pTime, CPtrFuncBase<CHeadMeshInfo> *_pStaticMesh );

	void SetHeadTransformInfo( CHeadTransformInfo *p );
	// release @0x265890 two-sequence form: pDbSeq arms as the real (lipsync) entry, pDbExpr as the
	// MASK entry (bMask=true -- the per-phrase facial expression, GetSequenceByExpression's result).
	// Recalc's existing bMask machinery renders/prunes it.
	void PlaySequence( NDb::CSequence *pDbSeq, NDb::CSequence *pDbExpr, STime tStart, bool bCycle = false );
	void PlaySequence( NDb::CSequence *pDbSeq, STime tStart, bool bCycle = false ) { PlaySequence( pDbSeq, 0, tStart, bCycle ); }
	void StopSequence();
	// release @0x265520: change the ambient-idle mode; on a change the armed idle entries are pruned
	// (a fresh one re-arms on the next Recalc) and the death mask is dropped.
	void SetIdleType( EIdleType e );
	EIdleType GetIdleType() const { return eIdleType; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadInfo -- per-unit resolved "complex head" render description (release-new, reg 0xA2543120).
// Built from a DB CComplexHead: holds the head material/hair/face+interface meshes + a cached shared
// transformed mesh + a per-unit random seed. LifeStudio-free (the live morphing lives in
// CHeadTransformInfo). Every unit renders its head through this (CUnit::pHeadInfo). operator& @0x2655e0;
// ctors @0x266ad0 (default) / @0x2656f0 (from CComplexHead) / @0x266e90 (copy = compiler member-wise).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeadInfo);
	ZDATA
	CDBPtr<NDb::CComplexHead> pHead;
	CDBPtr<NDb::CTMaterial> pMaterial;
	CDBPtr<NDb::CTRndModel> pHair;
	CDBPtr<NDb::CTRndModel> pMeshes[4];
	CDBPtr<NDb::CTRndModel> pIFMeshes[4];
	CDBPtr<NDb::CRace> pBodyColor;
	CDGPtr< CPtrFuncBase<NGfx::CTexture> > pTexture;
	CObj< CPtrFuncBase<CHeadMeshInfo> > pMesh;
	bool bStaticHead = false;
	SRandomSeed seed;
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&pMaterial); f.Add(3,&pHair); f.Add(5,&pBodyColor); f.Add(6,&pTexture); f.Add(7,&pMesh);
		f.Add(8,&bStaticHead); f.Add(9,&pHead);
		for ( int i = 0; i < 4; i++ ) f.Add(10+i,&pMeshes[i]);
		for ( int i = 0; i < 4; i++ ) f.Add(20+i,&pIFMeshes[i]);
		f.Add(30,&seed);
		return 0;
	}
public:
	CHeadInfo() {}
	CHeadInfo( NDb::CComplexHead *pComplexHead );

	NDb::CComplexHead* GetHead() const { return pHead; }        // the source CComplexHead template
	void SetSeed( const SRandomSeed &s ) { seed = s; }          // per-unit head-randomization seed
	const SRandomSeed& GetSeed() const { return seed; }         // retail CreateLSHead seeds the head rnd from this
	// Per-unit editor-customizable models, read by CGameView::CreateLSHead so a save/loaded AdvFaceGen
	// hair / eye-glasses choice survives load (retail @0x188c90 sources hair/pMeshes/pIFMeshes from the
	// per-unit CHeadInfo, NOT the shared CComplexHead DB record). Layout-neutral -- existing ZDATA members.
	const CDBPtr<NDb::CTRndModel>& GetHair() const { return pHair; }
	const CDBPtr<NDb::CTRndModel>* GetMeshes() const { return pMeshes; }       // 4-elem face/glasses mesh array
	const CDBPtr<NDb::CTRndModel>* GetIFMeshes() const { return pIFMeshes; }   // 4-elem interface-view mesh array
	// Static baked head (advanced FaceGen commit): the morph lives in pMesh (a CFaceGenMeshHolder) and
	// bStaticHead gates the in-game render (CHeadsController::GetAnimator) to source the animator from it.
	// CreateHeadInfo sets these; both round-trip via operator& (tags 7/8).
	bool IsStaticHead() const { return bStaticHead; }
	CPtrFuncBase<CHeadMeshInfo>* GetMesh() const { return pMesh; }
	void SetStaticMesh( CPtrFuncBase<CHeadMeshInfo> *p ) { pMesh = p; bStaticHead = true; }
	// Baked FaceGen face texture (advanced FaceGen commit): the recoloured skin lives in pTexture (a
	// CFaceGenTextureHolder); the in-game head material samples it (CGameView::CreateLSHead). CreateHeadInfo sets it.
	CPtrFuncBase<NGfx::CTexture>* GetFaceTexture() const { return pTexture; }
	void SetFaceTexture( CPtrFuncBase<NGfx::CTexture> *p ) { pTexture = p; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTransformInfo -- the per-unit LIVE head-morph state (release-new, reg 0xA1743120). Holds the
// macro-muscle "tension" map the advanced FaceGen editor drives (the face-shape sliders) plus an idle
// face-animation sequence. It is a versioned CFuncBase node so the downstream head mesh/texture
// transformers re-bake whenever a tension changes; CreateHeadInfo() publishes a renderable CHeadInfo.
//
// SENTINELS-LS ADAPTATION (the SS1 binary was built against an OLDER LifeStudio): the SS1 class held
// its own {IMMTree,ITransformer,IAnimator} and pushed SetTension(idx,val) into the ITransformer. Here
// the MESH morph is driven the PROVEN CHeadAnimator way -- macro-muscle expressions applied to the head
// mesh's IAnimators via the shared global pLSTree (IMMTree::FindMacroMuscle -> IAnimator::AddMacroMuscle)
// -- so the per-instance ITransformer/IAnimator in the value are reserved for the texture-morph path
// (CHeadTextureTransformer). The mmTensions map + the DG versioning are the live state the advanced
// editor and the downstream mesh transformer share. operator& matches the release @0x264050 (tags 2-8).
// See docs/CONVERGENCE_PROGRESS.md (SESSION 32 PART B+).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLSHeadTransformInfo
{
	CLSPtr<LifeStudioHeadAPI::IMMTree> pMMTree;            // (Sentinels: the shared global tree is used directly)
	CLSPtr<LifeStudioHeadAPI::ITransformer> pTransformer;  // MESH rig: Generate() -> pAnimator (Saved for the static mesh)
	CLSPtr<LifeStudioHeadAPI::IAnimator> pAnimator;        // MESH rig output (Save()d -> CFaceGenMeshHolder); NEVER collect user-items
	// TEXTURE-WEIGHT rig (release-faithful decoupling): a SEPARATE transformer + output animator that NEVER
	// feed the saved mesh stream, with CollectUserItems(true) so the FaceGen output channels resolve via
	// UserItem. The face-texture composite reads its per-layer weights off pTexTransformer. Keeping it separate
	// is mandatory: enabling CollectUserItems on the MESH rig pollutes pAnimator's Save() stream -> in-game
	// reload heap-faults; and querying UserItem on a non-collect transformer walks an unallocated table.
	CLSPtr<LifeStudioHeadAPI::ITransformer> pTexTransformer;
	CLSPtr<LifeStudioHeadAPI::IAnimator> pTexAnimator;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadTransformInfo: public CFuncBase<SLSHeadTransformInfo>
{
	OBJECT_BASIC_METHODS(CHeadTransformInfo);
	ZDATA
	unordered_map<string, float> mmTensions;             // macro-muscle name -> tension (slider value)
	bool bTensionUpdated;                                // a tension changed -> re-bake the mesh
	CDBPtr<NDb::CComplexHead> pComplexHead;
	CDGPtr< CPtrFuncBase<CHeadSequenceInfo> > pSequence;  // current idle face sequence
	CDGPtr< CFuncBase<STime> > pTime;
	STime tStart;                                        // idle-sequence start time
	bool bPlayIdle;
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&mmTensions); f.Add(3,&bTensionUpdated); f.Add(4,&pComplexHead);
		f.Add(5,&pSequence); f.Add(6,&pTime); f.Add(7,&tStart); f.Add(8,&bPlayIdle);
		return 0;
	}
private:
	// Transient (NOT serialized): the per-instance GDP morph rig lives in the inherited CFuncBase value
	// (value.pMMTree / pTransformer / pAnimator). These flags gate its lazy build; both default false so the
	// rig is rebuilt from the GDP on first Recalc (incl. after a load).
	bool bMorphBuilt = false;   // rig build attempted
	bool bMorphOk    = false;   // rig valid (transformable head + GDP opened + ITransformer::Load ok)
	bool bTexRigOk   = false;   // separate texture-weight rig valid (CollectUserItems transformer built)
	int  nTensionStamp = 0;     // bumps on every SetMMTension -- the live texture-preview node's re-bake key (transient)
	void BuildMorphRig();       // retail ctor tail @0x660826: GDP -> ITransformer -> output IAnimator
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	CHeadTransformInfo(): bTensionUpdated(false), tStart(0), bPlayIdle(false) {}
	CHeadTransformInfo( NDb::CComplexHead *_pHead, CFuncBase<STime> *_pTime );

	float GetMMTension( const string &name );
	void SetMMTension( const string &name, float value );
	void SetPlayIdle( bool b ) { bPlayIdle = b; }
	const unordered_map<string, float>& GetMMTensions() const { return mmTensions; }
	NDb::CComplexHead* GetComplexHead() const { return pComplexHead; }
	// The morphed GDP output animator (value.pAnimator after Generate()), or 0 for a non-transformable head.
	LifeStudioHeadAPI::IAnimator* GetMorphedAnimator() { return bMorphOk ? (LifeStudioHeadAPI::IAnimator*)value.pAnimator : 0; }
	// The face-texture WEIGHT source: the separate CollectUserItems transformer (its FaceGen output channels
	// drive the per-layer texture weights), or 0 if the texture rig didn't build. NEVER the mesh rig.
	LifeStudioHeadAPI::IAnimator* GetTexWeightSource() { return bTexRigOk ? (LifeStudioHeadAPI::IAnimator*)value.pTexTransformer : 0; }
	int GetTensionStamp() const { return nTensionStamp; }   // changes only when a slider moved (live-preview re-bake key)

	CHeadInfo* CreateHeadInfo();   // publish a renderable CHeadInfo (morphed mesh/texture bake -- B1b)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// LSFaceGen value structs (release-new; compiland .\release\LSFaceGen.obj). Small POD records the
// face-generation texture/mesh transformers operate on. They were absent from the dev snapshot (named
// only in the CHeadTransformInfo / CreateHeadInfo comments above); member order/types below match the
// release Game.pdb exactly. The compiler-generated special members reproduce the release bodies:
//   * SMixTex default ctor @0x264a80 / copy ctor @0x2620b0 -- pTex is a CDGPtr, whose copy ctor resets
//     the cached DG version to 0, so a copied layer re-reads the node version on its next Refresh()
//     (release-faithful, not a defect).
//   * SRawHeadMeshInfo copy ctor @0x2623b0 -- memberwise deep-copy of the eight owned vectors.
// (Wiring these into the head texture/mesh transformer subsystem -- CHeadTextureTransformer /
// CHeadMeshTransformer / CFaceGenMeshHolder / CFaceGenTextureHolder + the compositing free fns -- is
// the deferred texture-morph path B1b; see the CHeadTransformInfo banner above.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// SMixTex -- one source-texture layer of a facial feature (face / eye / eyelash_teeth): a versioned
// software-texture dataflow pointer paired with the layer's animation-channel name. (PDB size 20)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMixTex
{
	ZDATA
	CDGPtr< CPtrFuncBase<NGScene::CSWTextureData> > pTex;   // +0x00  source SW-texture node (versioned)
	string                                          szName; // +0x08  animation-channel name
	// retail @0x2646c0: 2=pTex (object ref), 3=szName (string chunk)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTex); f.Add(3,&szName); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Advanced-editor slider-bar entries: each pairs a slider start position with the DB piece it selects.
// (PDB size 8 each; InitBar bit-copies fStartPos from the record's fSliderPos, then sorts by it.)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHair
{
	float                          fStartPos;   // +0x00
	CDBPtr<NDb::CFaceGenHeadHair>  pHair;       // +0x04
};
struct SRace
{
	float               fStartPos;   // +0x00
	CDBPtr<NDb::CRace>  pRace;       // +0x04
};
struct SGlasses
{
	float                             fStartPos;   // +0x00
	CDBPtr<NDb::CFaceGenHeadGlasses>  pGlasses;    // +0x04
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SRawHeadMeshInfo -- the preloaded raw head-mesh description a CFaceGenMeshHolder holds inline: the
// per-animator LifeStudio byte streams plus the seven geometry vectors. The FaceGen-time analogue of
// the data CHeadMeshLoader::Recalc reads from a CResourceOpener (note the release head mesh additionally
// carries trueIndices/trueTris -- the doubled-vertex index buffers). (PDB size 96.)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRawHeadMeshInfo
{
	vector<CMemoryStream> animatorStreams;   // +0x00  per-animator LifeStudio byte streams
	vector<int>           nVertices;         // +0x0c
	vector<CTPoint<int> > copys;             // +0x18
	vector<CVec2>         UVs;               // +0x24
	vector<WORD>          indices;           // +0x30
	vector<int>           tris;              // +0x3c
	vector<WORD>          trueIndices;       // +0x48
	vector<WORD>          trueTris;          // +0x54
	// retail SRawHeadMeshInfo::operator& @0x2636a0 (8 chunks). animatorStreams serialise per-element as a
	// BLOB; the POD vectors as raw blocks (CStructureSaver DoDataVector). trueIndices/trueTris are written
	// for format symmetry though the dev render path never reads them.
	int operator&( CStructureSaver &f )
	{
		f.Add(2,&animatorStreams); f.Add(3,&nVertices); f.Add(4,&copys); f.Add(5,&UVs);
		f.Add(6,&indices); f.Add(7,&tris); f.Add(8,&trueIndices); f.Add(9,&trueTris);
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenMeshHolder -- the baked static head mesh published by CHeadTransformInfo::CreateHeadInfo
// (retail @0x260bf0 -> GetMeshInfo @0x2604d0; reg 0x11042143). Unlike CHeadMeshLoader (which loads the
// BASE "Heads"-pack mesh keyed by record id), this carries the morph INLINE: the base topology copied
// verbatim + ONE animatorStreams[0] = the Save()'d morphed GDP animator (value.pAnimator post-Generate).
// It is the serialisable pointee of CHeadInfo::pMesh, so the committed hero's face round-trips through
// save/load. Recalc rebuilds the live CHeadMeshInfo from info exactly as CHeadMeshLoader::Recalc does
// from a resource; the single whole-head animator drives the positions (CHeadAnimator(static)::Recalc).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceGenMeshHolder: public CPtrFuncBase<CHeadMeshInfo>
{
	OBJECT_BASIC_METHODS(CFaceGenMeshHolder);
	ZDATA
	SRawHeadMeshInfo info;       // base topology + the baked morph animator stream (the PERSISTED state)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&info); return 0; }
protected:
	// Rebuild the live mesh once, and again after a load (pValue not serialized -> null). Mirror CResourceLoader:
	// !IsValid(pValue), NOT GetValue() -- GetValue() ASSERTs IsFrameMatch(), which is false when DoUpdate() calls
	// NeedUpdate() before stamping nFrameCalced (CVersioningBase::DoUpdate / CPtrFuncBase::GetValue in DG.h).
	virtual bool NeedUpdate() { return !IsValid( pValue ); }
	virtual void Recalc();
public:
	SRawHeadMeshInfo& GetInfo() { return info; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sort comparators (release-new). HairAndRaceCmp<SHair/SRace/SGlasses> @0x260fe0 orders slider entries
// ascending by start position; MixTexCmp @0x261160 orders transformable head textures ascending by
// priority. (Used by InitBar / FillTransformableTexs in the texture/mesh transformers -- B1b.)
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline bool HairAndRaceCmp( const T &a, const T &b ) { return a.fStartPos < b.fStartPos; }
inline bool MixTexCmp( const CPtr<NDb::CHeadTexture> &a, const CPtr<NDb::CHeadTexture> &b )
{
	return a->nPriority < b->nPriority;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTextureTransformer -- the LIVE editor-preview face-texture node (retail @0x263480 NeedUpdate /
// @0x25ff00 Recalc). A versioned CPtrFuncBase<CTexture> that re-composites the 256x256 face skin from the
// live morph's FaceGen channels whenever a slider moves, so the AdvFaceGen preview head recolours in real
// time (the static CFaceGenTextureHolder bakes once at commit; this is its live twin). Fed to
// CGameView::CreateLSHead as pFaceTexture in the live AddHead path (CSetRender::AddHead with a
// CHeadTransformInfo); the head material (CDGPtr<CPtrFuncBase<CTexture>> pDiffuseTex) samples it per frame.
// Serialized: retail operator& @0x264540 (LSFaceGen.obj), id 0xA1743122 (LSHead.cpp, landed in W2).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeadTextureTransformer: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CHeadTextureTransformer);
	ZDATA
	// OWNING ref, exactly as retail: PDB type is CDGPtr<CFuncBase<SLSHeadTransformInfo>,
	// CObj<CFuncBase<SLSHeadTransformInfo>>> -- the transformer keeps its morph node alive
	// (was a dev CPtr hedge; reverted to the retail CObj flavor in W3).
	CDGPtr< CHeadTransformInfo > pTransformInfo;  // the live tension/weight source
	vector<SMixTex> face, eye, eyelash;   // DB texture-layer lists (built once in the ctor)
public:
	// retail @0x264540: 2=pTransformInfo, 3=face, 4=eye, 5=eyelash (DoVector<SMixTex> each);
	// nLastStamp is NOT serialized in retail (re-bake key only).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTransformInfo); f.Add(3,&face); f.Add(4,&eye); f.Add(5,&eyelash); return 0; }
private:
	int nLastStamp;                       // last-baked CHeadTransformInfo tension stamp (re-bake only when it changes)
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	CHeadTextureTransformer(): nLastStamp(-1) {}
	CHeadTextureTransformer( CHeadTransformInfo *p, NDb::CHead *pDbHead );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif