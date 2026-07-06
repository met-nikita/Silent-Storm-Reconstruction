#include "StdAfx.h"
#include "LSHead.h"
#include "GResource.h"
#include "GGeometry.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "GfxBuffers.h"      // NGfx::MakeTexture / CTextureLock / CTexture (face-texture bake upload)
#include "GPixelFormat.h"    // NGfx::SPixel8888
#include "SWTexture.h"       // NGScene::CSWTextureData (source texture-layer pixels)
#include "2DSceneSW.h"       // NGScene::GetSWTex
#include "..\ADOImport\BasicDB.h"   // NDatabase::GetTable / CDBTable / CDBIterator (FaceGen hair/glasses bars)
#include "ModManager.h"      // v1.2 @0x6602e0: CModManager::GetBaseVersion keys the idle-table rebuild

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NLSHead
{
CBasicShare<int, CHeadMeshLoader> shareHeads(135);
static CBasicShare<int, CHeadSequenceLoader> shareSequences(137);
static CLSPtr<LifeStudioHeadAPI::IMMTree> pLSTree;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void EnsureLSInit()
{
	static bool bInited = false;
	if ( !bInited )
	{
		LifeStudioHeadAPI::Init();
		bInited = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void LoadLSTree()
{
	EnsureLSInit();
	if ( pLSTree )
		return;
	try
	{
		pLSTree = LifeStudioHeadAPI::IMMTree::Create();
		pLSTree->Load( "tree.mma" );
	}
	catch(...)
	{
		OutputDebugString( "Exception: LoadLSTree()" );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadMeshLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadMeshLoader::Recalc()
{
	EnsureLSInit();   // IAnimator::Create() below is a main-API call -> Init() must precede it
	try
	{
		NGScene::CResourceOpener file( "Heads", GetKey() );
		pValue = new CHeadMeshInfo;
		vector<CMemoryStream> streams;
		file->Add( 1, &streams );
		file->Add( 2, &pValue->nVertices );
		file->Add( 3, &pValue->copys );
		file->Add( 4, &pValue->UVs );
		file->Add( 5, &pValue->indices );
		file->Add( 6, &pValue->tris );
		pValue->pLSAnimators.resize( streams.size() );
		for ( int i = 0; i < streams.size(); ++i )
		{
			pValue->pLSAnimators[i] = LifeStudioHeadAPI::IAnimator::Create();
			pValue->pLSAnimators[i]->Load( (const char *)streams[i].GetBuffer(), streams[i].GetSize() );
			LoadLSTree();
			if ( pLSTree )
				pValue->pLSAnimators[i]->RegisterMacroMuscle( pLSTree->RootMacroMuscle() );
		}
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadMeshLoader::Recalc()" );
		return;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadSequenceLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadSequenceLoader::Recalc()
{
	EnsureLSInit();   // ISequencer::Create() below is a main-API call -> Init() must precede it
	try
	{
		NGScene::CResourceOpener file( "Sequences", GetKey() );
		pValue = new CHeadSequenceInfo;
		CMemoryStream stream;
		file->Add( 1, &stream );
		pValue->pLSSequence = LifeStudioHeadAPI::ISequencer::Create();
		pValue->pLSSequence->Load( (const char *)stream.GetBuffer(), stream.GetSize() );
		LoadLSTree();
		if ( pLSTree )
			pValue->pLSSequence->RegisterMMTree( pLSTree );
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadSequenceLoader::Recalc()" );
		return;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadBound
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadBound::Recalc()
{
	const SFBTransform &trans = pParent->GetValue();
	CVec3 pt = trans.forward.GetTranslation();
	value.SphereInit( pt, 1 ); // CRAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Ambient facial-idle tables (release NLSHead::idleAnimations / idleDeathAnimations globals, filled by
// InitializeIdleAnimations @0x2602f0): every HeadSeqs record flagged IsIdleAnimation, bucketed by its
// IdleType (retail data: Blink/Blink01 -> normal, Death -> death). Built on first use; the head
// animator picks a uniformly random entry whenever no idle sequence is in flight (Recalc @0x265a30).
// v1.2 @0x6602e0: the once-only guard is version-keyed on CModManager::GetBaseVersion() (bumped by
// every mod Activate) -- on mismatch both lists are cleared and rebuilt from the re-imported
// HeadSeqs table, so no stale CSequence refs survive a runtime DB reload.
////////////////////////////////////////////////////////////////////////////////////////////////////
static vector< CPtr<NDb::CSequence> > idleAnimations;
static vector< CPtr<NDb::CSequence> > idleDeathAnimations;
static bool bIdleInitialized = false;
static void InitializeIdleAnimations()
{
	// v1.2 @0x6602e0: function-local static caches the DB version this ran at
	static int nBaseVersion = CModManager::GetBaseVersion();
	if ( bIdleInitialized && nBaseVersion == CModManager::GetBaseVersion() )
		return;
	if ( !bIdleInitialized )
		EnsureLSInit();   // release calls LifeStudioHeadAPI::Init() on the FIRST init only, not on version-change rebuilds (v1.2 @0x6602e0)
	CDBTable<NDb::CSequence> *pTable = NDatabase::GetTable<NDb::CSequence>();
	if ( !pTable )
		return;        // DB not up yet -> retry on a later Recalc (release assumes it is always up)
	nBaseVersion = CModManager::GetBaseVersion();
	idleAnimations.clear();        // v1.2 @0x6602e0: drop the previous DB's sequence refs before re-bucketing
	idleDeathAnimations.clear();
	bIdleInitialized = true;
	CDBIterator<NDb::CSequence> it( *pTable );
	while ( it.MoveNext() )
	{
		NDb::CSequence *r = it.Get();
		if ( !IsValid( r ) || !r->bIdleAnimation )
			continue;
		if ( r->eIdleType == NDb::SIT_NORMAL )
			idleAnimations.push_back( r );
		else if ( r->eIdleType == NDb::SIT_DEATH )
			idleDeathAnimations.push_back( r );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadAnimator::CHeadAnimator( CFuncBase<STime> *_pTime, NDb::CHead *_pDbHead ): pTime(_pTime), pDbHead(_pDbHead)
{
	pHead = shareHeads.Get( pDbHead->GetRecordID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Static baked head: pHead is the CFaceGenMeshHolder (CreateHeadInfo's morph bake) instead of the shared
// base mesh; pDbHead stays null (only the ctor used it). bStatic routes Recalc to the whole-head morph
// branch sourced from the holder's pLSAnimators[0]. (retail static CHeadAnimator branch @0x2cd7f0.)
CHeadAnimator::CHeadAnimator( CFuncBase<STime> *_pTime, CPtrFuncBase<CHeadMeshInfo> *_pStaticMesh ): pTime(_pTime), bStatic(true)
{
	pHead = _pStaticMesh;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadAnimator::SetHeadTransformInfo( CHeadTransformInfo *p )
{
	pHeadTransformInfo = p;   // weak ref; re-deform tracks the morph node's version (see NeedUpdate)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeadAnimator::NeedUpdate()
{
	// release @0x264e40: headChanged | (timeChanged & !bDeathMask), with a bare time tick suppressed
	// when there is nothing to animate (no playing sequences AND idle mode off) -- a quiescent head
	// stops recomputing; the CIdleHead token flipping eIdleType back on re-awakens it.
	bool bTimeChanged = pTime.Refresh();
	bool bHeadChanged = pHead.Refresh();
	if ( sequences.empty() && eIdleType == IDLE_NONE )
		bTimeChanged = false;
	if ( bDeathMask )
		bTimeChanged = false;
	bool bChanged = bHeadChanged | bTimeChanged;
	if ( IsValid( pHeadTransformInfo ) )
		bChanged = bChanged | pHeadTransformInfo.Refresh();   // a slider moved -> re-deform (dev live-morph extra)
	return bChanged;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x265890 (both legs): stop whatever non-idle sequence is playing, then arm the new one as
// a sequences-vector entry; the optional SECOND sequence (the per-phrase facial expression) arms as a
// MASK entry (bMask=true) that Recalc's existing bMask machinery renders/prunes. Armed idles keep
// running underneath (blinks layer over lipsync, exactly the release behavior).
void CHeadAnimator::PlaySequence( NDb::CSequence *pDbSeq, NDb::CSequence *pDbExpr, STime _tStart, bool _bCycle )
{
	StopSequence();
	if ( pDbSeq )
	{
		CPtrFuncBase<CHeadSequenceInfo> *pNode = shareSequences.Get( pDbSeq->GetRecordID() );
		if ( IsValid( pNode ) )
		{
			SSequence s;
			s.pSequence = pNode;
			s.tStart = _tStart;
			s.bCycle = _bCycle;
			s.bIdle = false;
			s.bMask = false;
			sequences.push_back( s );
			// legacy dev-format mirror (save tags 5/6/7; see the operator& note in LSHead.h)
			pSequence = pNode;
			tStart = _tStart;
			bCycle = _bCycle;
		}
	}
	// release @0x265890 second leg: the expression MASK entry (same tStart/bCycle, bMask=true)
	if ( pDbExpr )
	{
		CPtrFuncBase<CHeadSequenceInfo> *pNode = shareSequences.Get( pDbExpr->GetRecordID() );
		if ( IsValid( pNode ) )
		{
			SSequence s;
			s.pSequence = pNode;
			s.tStart = _tStart;
			s.bCycle = _bCycle;
			s.bIdle = false;
			s.bMask = true;
			sequences.push_back( s );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x2654d0: prune every NON-idle entry (spoken/mask sequences); the armed ambient idles keep
// playing. Also clears the legacy dev-format mirror.
void CHeadAnimator::StopSequence()
{
	for ( vector<SSequence>::iterator it = sequences.begin(); it != sequences.end(); )
	{
		if ( !it->bIdle )
			it = sequences.erase( it );
		else
			++it;
	}
	pSequence = 0;
	tStart = 0;
	bCycle = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x265520: on an idle-mode CHANGE drop the death mask and prune the armed idle entries, so
// the next Recalc arms a fresh idle of the new mode (or none for IDLE_NONE). Non-idle sequences keep playing.
void CHeadAnimator::SetIdleType( EIdleType e )
{
	if ( e == eIdleType )
		return;
	eIdleType = e;
	bDeathMask = false;
	for ( vector<SSequence>::iterator it = sequences.begin(); it != sequences.end(); )
	{
		if ( it->bIdle )
			it = sequences.erase( it );
		else
			++it;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadAnimator::Recalc()
{
	CHeadMeshInfo *pMesh = pHead->GetValue();
	if ( !pMesh )
		return;   // holder rebuild threw / stale stream -> keep the prior frame rather than deref null
	value.mesh.resize( pMesh->UVs.size() );
	value.normals.resize( pMesh->UVs.size() );
	memset( &value.normals[0], 0, sizeof(CVec3) * value.normals.size() );

	STime t = pTime->GetValue();

	// ==== release @0x265a30 sequence maintenance over the retail-style vector ====================
	// One macro-muscle render to apply to every mesh animator this frame: which sequence, at which
	// sequence-local time.
	struct SSeqFrame
	{
		CHeadSequenceInfo *pSeq;
		int nFrameTime;
	};
	vector<SSeqFrame> frames;

	// (1) mask entries live only while a real (non-idle, non-mask) sequence is playing
	bool bHasReal = false;
	for ( int i = 0; i < sequences.size(); ++i )
		bHasReal |= !sequences[i].bMask && !sequences[i].bIdle;
	if ( !bHasReal )
	{
		for ( vector<SSequence>::iterator it = sequences.begin(); it != sequences.end(); )
		{
			if ( it->bMask )
				it = sequences.erase( it );
			else
				++it;
		}
	}

	// (2) refresh/expire each entry; collect this frame's renders. STime is unsigned, so the
	// comparisons below are the release's unsigned jb/jbe.
	bool bIdleActive = false;
	for ( vector<SSequence>::iterator it = sequences.begin(); it != sequences.end(); )
	{
		if ( !IsValid( it->pSequence ) )
		{
			// dead/unloaded loader node: keep the record, render nothing (release keeps these too)
			bIdleActive |= it->bIdle;
			++it;
			continue;
		}
		it->pSequence.Refresh();
		CHeadSequenceInfo *pSeqInfo = it->pSequence->GetValue();
		int nSeqTime = pSeqInfo ? pSeqInfo->pLSSequence->SequenceTime() : 0;
		if ( pSeqInfo && nSeqTime != 0 && ( it->bCycle || t < it->tStart || t - it->tStart <= (STime)nSeqTime || it->bMask ) )
		{
			// live entry: render once started; a mask past its end clamps to its tail frame
			if ( t >= it->tStart )
			{
				SSeqFrame fr;
				fr.pSeq = pSeqInfo;
				if ( !it->bMask || t - it->tStart < (STime)nSeqTime )
					fr.nFrameTime = it->bCycle ? (int)( ( t - it->tStart ) % (STime)nSeqTime ) : (int)( t - it->tStart );
				else
					fr.nFrameTime = nSeqTime - 2;
				frames.push_back( fr );
			}
			bIdleActive |= it->bIdle;
			++it;
		}
		else if ( pSeqInfo && it->bIdle && eIdleType == IDLE_DEATH )
		{
			// finished death idle: freeze on its last frame and raise the death mask (NeedUpdate then
			// stops the per-tick recompute, so the corpse keeps this face)
			SSeqFrame fr;
			fr.pSeq = pSeqInfo;
			fr.nFrameTime = nSeqTime - 1;
			frames.push_back( fr );
			bDeathMask = true;
			bIdleActive = true;
			++it;
		}
		else
		{
			// expired / valueless: drop it (and the legacy dev-format mirror when it was the spoken one)
			if ( !it->bIdle )
			{
				pSequence = 0;
				tStart = 0;
				bCycle = false;
			}
			it = sequences.erase( it );
		}
	}

	// (3) release @0x265a30 tail: nothing idle in flight -> arm one random ambient idle (a blink) with
	// a random 0..999 ms lead-in, so shown heads keep blinking at natural, unsynchronized intervals.
	if ( !bIdleActive )
	{
		InitializeIdleAnimations();
		static SRand rnd;   // release: one lazily-seeded static generator shared by all heads
		NDb::CSequence *pPick = 0;
		if ( eIdleType == IDLE_NORMAL && !idleAnimations.empty() )
			pPick = idleAnimations[ rnd.Get( idleAnimations.size() ) ];
		else if ( eIdleType == IDLE_DEATH && !idleDeathAnimations.empty() )
			pPick = idleDeathAnimations[ rnd.Get( idleDeathAnimations.size() ) ];
		if ( IsValid( pPick ) )
		{
			SSequence s;
			s.pSequence = shareSequences.Get( pPick->GetRecordID() );
			s.tStart = t + rnd.Get( 1000 );
			s.bCycle = false;
			s.bIdle = true;
			s.bMask = false;
			sequences.push_back( s );
		}
	}
	// =============================================================================================

	// RETAIL morph path (CHeadTransformInfo::Recalc @0x660000 + CHeadMeshTransformer @0x65fab0): for a
	// transformable head the rendered POSITIONS come from a per-instance ITransformer baked from the head's
	// TransformableGDP (Res\FaceGenHead.gdp) and morphed by the slider tensions -- NOT from the base "Heads"-pack
	// animators (whose geometry the FaceGenHead.mmt deltas were NOT authored against -> applying them there did
	// nothing visible). pHeadTransformInfo owns that rig + its Generate()'d output animator; Process IT into
	// value.mesh. The base pack still supplies the UVs/indices/tris/copys topology used by CHead::Recalc below.
	int nReal = 0;
	for ( int i = 0; i < pMesh->nVertices.size(); ++i )
		nReal += pMesh->nVertices[i];
	if ( bStatic )
	{
		// Baked STATIC head (advanced FaceGen commit): the committed morph is the single whole-head GDP animator
		// pLSAnimators[0], reloaded from the saved stream. Unlike the live editor pMorph (Generate()'d THIS frame
		// by CHeadTransformInfo::Recalc), a just-Load()ed animator has computed nothing, so it must
		// ClearAllMacroMuscles + ComputePhysics before Process -- exactly retail CHeadAnimator::Recalc @0x265a30
		// (every mesh animator: ClearAllMacroMuscles -> ComputePhysics -> FillUnused -> Process). The morph rides in
		// the GDP base geometry, not in macro-muscle tensions, so clearing them is safe. One animator over ALL UVs
		// (never the per-segment loop, which would fill only segment 0 of a single-stream baked mesh).
		// NB: extract the raw pointer with operator T*() via a plain assignment -- do NOT use a
		// `cond ? 0 : pLSAnimators[0]` ternary: that yields a temporary CLSPtr<IAnimator> (the only common
		// type of `0` and CLSPtr), whose destructor calls ptr->Destroy() on the animator the vector STILL
		// owns -> use-after-free (latent since a6fb87c; the texture rig's allocations now reuse the freed
		// memory and make it a deterministic crash in CHeadAnimator::Recalc). The non-static fallback below
		// uses this same direct-assignment form and never crashed.
		LifeStudioHeadAPI::IAnimator *pStatic = 0;
		if ( !pMesh->pLSAnimators.empty() )
			pStatic = pMesh->pLSAnimators[0];
		if ( pStatic && pStatic->VerticesCount() == nReal && nReal > 0 )
		{
			pStatic->ClearAllMacroMuscles();
			// release @0x265a30 renders every playing sequence into every mesh animator before
			// ComputePhysics -- the baked hero head blinks/lip-syncs exactly like a base head (the
			// morph rides in the GDP base geometry; sequence macro-muscles layer on top).
			for ( int r = 0; r < frames.size(); ++r )
				frames[r].pSeq->pLSSequence->RenderMacroMuscles( pStatic, frames[r].nFrameTime );
			pStatic->ComputePhysics();
			pStatic->FillUnused( true );
			pStatic->Process( &(value.mesh[0].x), 3 );
		}
		// (count mismatch -> leave value.mesh zeroed rather than corrupt memory; never the per-segment path)
	}
	else
	{
		LifeStudioHeadAPI::IAnimator *pMorph = IsValid( pHeadTransformInfo ) ? pHeadTransformInfo->GetMorphedAnimator() : 0;
		if ( pMorph && pMorph->VerticesCount() == nReal && nReal > 0 )
		{
			// Whole head = the single live morphed GDP animator (positions); base pack supplies topology only.
			// pMorph was Generate()'d this frame (CHeadTransformInfo::Recalc) with the slider tensions.
			// release layers the playing sequences (armed ambient idles / expressions) ON TOP of the morph
			// -- RenderMacroMuscles adds to the tensions Generate() installed (NO ClearAllMacroMuscles here,
			// that would wipe the sliders), then one re-solve. This is what makes the AdvFaceGen preview
			// head blink/emote instead of sitting frozen.
			for ( int r = 0; r < frames.size(); ++r )
				frames[r].pSeq->pLSSequence->RenderMacroMuscles( pMorph, frames[r].nFrameTime );
			if ( !frames.empty() )
				pMorph->ComputePhysics();
			pMorph->FillUnused( true );
			pMorph->Process( &(value.mesh[0].x), 3 );
		}
		else
		{
			// Fallback: non-transformable head (or a GDP/base vertex-count mismatch) -> the base-pack idle render.
			int nVert = 0;
			for ( int i = 0; i < pMesh->pLSAnimators.size(); ++i )
			{
				LifeStudioHeadAPI::IAnimator *pLSAnimator = pMesh->pLSAnimators[i];
				pLSAnimator->ClearAllMacroMuscles();
				// release @0x265a30: every playing sequence (spoken + armed blinks + masks) renders its
				// macro-muscles into every segment animator at its own sequence-local time.
				for ( int r = 0; r < frames.size(); ++r )
					frames[r].pSeq->pLSSequence->RenderMacroMuscles( pLSAnimator, frames[r].nFrameTime );
				pLSAnimator->ComputePhysics();
				pLSAnimator->FillUnused( true );
				pLSAnimator->Process( &(value.mesh[nVert].x), 3 );
				nVert += pMesh->nVertices[i];
			}
		}
	}

	for ( int i = 0; i < pMesh->copys.size(); ++i )
		value.mesh[ pMesh->copys[i].y ] = value.mesh[ pMesh->copys[i].x ];

	// NB (nose-tip-at-origin, 2026-07-02): a runtime session with the origin-vertex diagnostic here
	// showed value.mesh is CLEAN while the on-screen nose still shows one vertex at the model
	// center -- the defect is DOWNSTREAM of NLSHead (AssignFast -> CreateDynamicGeometry ->
	// GfxBuffers vertex packer). Offline audits (all 134 retail heads + a real-DLL harness over all
	// four fill branches) also cleared this stage. Hunt there next; nose tip = source vertex 117.

	int nFrom = 0;
	int nTo = 0;
	for ( int i = 0; i < pMesh->tris.size(); ++i )
	{
		nTo = pMesh->tris[i];
		CVec3 &v1 = value.mesh[ pMesh->indices[nFrom] ];
		CVec3 &v2 = value.mesh[ pMesh->indices[nFrom+1] ];
		CVec3 &v3 = value.mesh[ pMesh->indices[nFrom+2] ];
		CVec3 normal = (v2 - v1) ^ (v3 - v1);
		Normalize(&normal);
		for ( int j = nFrom; j < nTo; ++j )
			value.normals[ pMesh->indices[j] ] += normal;
		nFrom = nTo;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHead
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHead::Recalc()
{
	CHeadMeshInfo *pMesh = pHead->GetValue();
	const SHeadFrame &frame = pAnimator->GetValue();
	int nVertices = frame.mesh.size();
	ASSERT( nVertices == pMesh->UVs.size() );
	if ( nVertices > pMesh->UVs.size() )
		nVertices = pMesh->UVs.size();
	pValue = new NGScene::CObjectInfo;
	NGScene::CObjectInfo::SData res;
	res.verts.resize( nVertices );
	NGScene::SVertex *pRes = &res.verts[0];
	// The head pack's indices/tris are the NORMAL-SMOOTHING data: each poly range is
	// [a,b,c, +every smoothing-group duplicate of a/b/c], and tris[] holds cumulative END
	// offsets with no leading 0. That layout is only meant for the face-normal accumulation
	// above (CalcHeadGeometry, release @0x265100). The renderer's SPolygonIndices fan-walker
	// expects an N+1 sentinel table of pure triangles, so feeding the raw data merged the
	// first two triangles into one fan (hole on the nose of every head) and emitted the
	// duplicate-vertex spokes as coincident triangles with the seam's other UVs (z-fighting).
	// Release derives a TRUE triangle list instead -- CHeadMeshLoader::Recalc @0x266070
	// builds trueIndices/trueTris = leading 0 + the first 3 indices of each range, and
	// CHeadMeshTransformer::Recalc @0x25fab0 renders THOSE. Build them inline here so both
	// mesh sources (base pack + baked FaceGen heads) get the clean list.
	res.geometry.indices.clear();
	res.geometry.polys.clear();
	res.geometry.polys.push_back( 0 );				// release trueTris leading sentinel
	int nBase = 0;
	for ( int k = 0; k < pMesh->tris.size(); ++k )
	{
		res.geometry.indices.push_back( pMesh->indices[nBase] );
		res.geometry.indices.push_back( pMesh->indices[nBase + 1] );
		res.geometry.indices.push_back( pMesh->indices[nBase + 2] );
		res.geometry.polys.push_back( res.geometry.indices.size() );
		nBase = pMesh->tris[k];
	}

	const SFBTransform &trans = pParent->GetValue();

	for ( int i = 0; i < nVertices; ++i, ++pRes )
	{
		CVec3 normal;
		trans.forward.RotateVector( &normal, frame.normals[i] );
		Normalize( &normal );
		CVec3 b1 = CVec3( 1, 0, 0 ), b2;
		if ( fabs2(normal.x) > 1e-12f )
		{
			b1 = CVec3( normal.y, -normal.x, 0 );
			Normalize( &b1 );
		}
		b2 = b1 ^ normal;
		// fill vertex
		trans.forward.RotateHVector( &(pRes->pos), frame.mesh[i] );
		pRes->normal = normal;
		pRes->tex = pMesh->UVs[i];
		pRes->texU = b1;
		pRes->texV = b2;
	}
	pValue->AssignFast( res );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadInfo::CHeadInfo( CComplexHead* )                                                     @0x2656f0
// Resolve a unit's head resources from the DB record: material + shared transformed mesh (from the
// source CHead), hair, and the face/interface mesh sets. pTexture/pBodyColor stay null (set elsewhere);
// the seed default-constructs (tick-count seeded); bStaticHead = false (default member initializer).
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadInfo::CHeadInfo( NDb::CComplexHead *pComplexHead ):
	pHead( pComplexHead )
{
	if ( IsValid( pHead ) && IsValid( pHead->pHead ) )
	{
		NDb::CHead *pDbHead = pHead->pHead;
		pMaterial = pDbHead->pMaterial;
		pMesh = shareHeads.Get( pDbHead->GetRecordID() );   // shared transformed mesh (resource cache)
	}
	if ( IsValid( pHead ) )
	{
		pHair = pHead->pHair;
		for ( int i = 0; i < 4; i++ )
		{
			pMeshes[i]   = pHead->pMeshes[i];
			pIFMeshes[i] = pHead->pIFMeshes[i];
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTransformInfo                                                                       @0x2606e0
// The per-unit live head-morph state (reg 0xA1743120). See the LSHead.h banner for the Sentinels-LS
// adaptation (the mesh morph rides the shared pLSTree + the mesh IAnimators; the per-instance
// ITransformer/IAnimator value slots are reserved for the texture path).
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadTransformInfo::CHeadTransformInfo( NDb::CComplexHead *_pHead, CFuncBase<STime> *_pTime ):
	pComplexHead(_pHead), pTime(_pTime), bTensionUpdated(true), tStart(0), bPlayIdle(true)
{
	Updated();   // bump the DG version so the first downstream Refresh re-bakes the head
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// BuildMorphRig -- mirror retail CHeadTransformInfo::CHeadTransformInfo tail @0x660826: build the per-instance
// IMMTree (head TransformableMMT) + ITransformer (loaded from the head TransformableGDP) + output IAnimator.
// The slider tensions later drive the transformer (Recalc) which Generate()s the morphed GDP vertices into
// value.pAnimator; CHeadAnimator::Recalc Process()es THAT animator into the rendered mesh (positions), reusing
// the base "Heads"-pack UVs/indices/tris for topology -- exactly retail's CHeadMeshTransformer split.
void CHeadTransformInfo::BuildMorphRig()
{
	bMorphBuilt = true;
	if ( !IsValid( pComplexHead ) || !IsValid( pComplexHead->pHead ) )
		return;
	NDb::CHead *pDbHead = pComplexHead->pHead;
	if ( !pDbHead->isTransformable || pDbHead->szTransformableGDP.empty() )
		return;
	EnsureLSInit();
	try
	{
		value.pMMTree = LifeStudioHeadAPI::IMMTree::Create();
		value.pMMTree->Load( pDbHead->szTransformableMMT.c_str() );        // "Res\FaceGenHead.mmt"
		value.pTransformer = LifeStudioHeadAPI::ITransformer::Create();
		value.pAnimator = LifeStudioHeadAPI::IAnimator::Create();

		LifeStudioHeadAPI::IGDPFile *pGDP = LifeStudioHeadAPI::IGDPFile::Create( pDbHead->szTransformableGDP.c_str() );  // "Res\FaceGenHead.gdp"
		if ( pGDP )
		{
			LifeStudioHeadAPI::IGDPObject *pObj = pGDP->Object( 0 );
			if ( pObj && pObj->IsTransformable() && value.pTransformer->Load( pObj ) )
			{
				value.pTransformer->OutputAnimator( value.pAnimator );
				value.pTransformer->RegisterMacroMuscle( value.pMMTree->RootMacroMuscle() );
				bMorphOk = true;

				// Parallel TEXTURE-WEIGHT rig (NEVER Saved): its own transformer + output animator, driven from the
				// SAME GDP + MM root, with CollectUserItems(true) so the FaceGen output channels resolve via UserItem.
				// The mesh rig above stays collection-OFF so pAnimator->Save() is mesh-only (in-game reload safe).
				// Own try/catch: a texture-rig failure must NOT clear bMorphOk (the validated mesh morph).
				try
				{
					value.pTexTransformer = LifeStudioHeadAPI::ITransformer::Create();
					value.pTexAnimator = LifeStudioHeadAPI::IAnimator::Create();
					if ( value.pTexTransformer && value.pTexAnimator && value.pTexTransformer->Load( pObj ) )
					{
						value.pTexTransformer->OutputAnimator( value.pTexAnimator );
						value.pTexTransformer->RegisterMacroMuscle( value.pMMTree->RootMacroMuscle() );
						value.pTexTransformer->CollectUserItems( true );   // SAFE here: this rig is never serialized
						bTexRigOk = true;
					}
				}
				catch(...)
				{
					OutputDebugString( "Exception: CHeadTransformInfo texture-weight rig" );
					bTexRigOk = false;
				}
			}
			if ( pObj )
				pObj->Destroy();
			pGDP->Destroy();
		}
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadTransformInfo::BuildMorphRig()" );
		bMorphOk = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetMMTension @0x25ee60 -- a macro-muscle's current tension, or -1.0f when it has never been set.
float CHeadTransformInfo::GetMMTension( const string &name )
{
	unordered_map<string, float>::iterator it = mmTensions.find( name );
	if ( it == mmTensions.end() )
		return -1.0f;
	return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FaceGen MODEL-SWAP slider bars (retail InitBar @0x263c00). Process-global singletons sorted ascending by
// fSliderPos: the "Women Hair" slider picks a hair MESH from TransformableHeadHairs, "Eye-glasses" picks a
// glasses mesh from TransformableHeadGlasses. ("Men Hair" is a hair-COLOR texture, handled by the face-texture
// bake -- it has no mesh bar, matching retail.) Built lazily on first SetMMTension (the DB is loaded by then).
static vector<SHair> g_hairBar;
static vector<SGlasses> g_glassesBar;
static vector<SRace> g_raceBar;
static bool g_faceGenBarsInited = false;
static void InitFaceGenBars()
{
	if ( g_faceGenBarsInited )
		return;
	g_faceGenBarsInited = true;
	CDBTable<NDb::CRace> *pRaceTable = NDatabase::GetTable<NDb::CRace>();
	if ( pRaceTable )
	{
		CDBIterator<NDb::CRace> it( *pRaceTable );
		while ( it.MoveNext() )
		{
			NDb::CRace *r = it.Get();
			SRace e; e.fStartPos = r->fSliderPos; e.pRace = r;
			g_raceBar.push_back( e );
		}
		sort( g_raceBar.begin(), g_raceBar.end(), HairAndRaceCmp<SRace> );
	}
	CDBTable<NDb::CFaceGenHeadHair> *pHairTable = NDatabase::GetTable<NDb::CFaceGenHeadHair>();
	if ( pHairTable )
	{
		CDBIterator<NDb::CFaceGenHeadHair> it( *pHairTable );
		while ( it.MoveNext() )
		{
			NDb::CFaceGenHeadHair *r = it.Get();
			SHair e; e.fStartPos = r->fSliderPos; e.pHair = r;
			g_hairBar.push_back( e );
		}
		sort( g_hairBar.begin(), g_hairBar.end(), HairAndRaceCmp<SHair> );
	}
	CDBTable<NDb::CFaceGenHeadGlasses> *pGlassesTable = NDatabase::GetTable<NDb::CFaceGenHeadGlasses>();
	if ( pGlassesTable )
	{
		CDBIterator<NDb::CFaceGenHeadGlasses> it( *pGlassesTable );
		while ( it.MoveNext() )
		{
			NDb::CFaceGenHeadGlasses *r = it.Get();
			SGlasses e; e.fStartPos = r->fSliderPos; e.pGlasses = r;
			g_glassesBar.push_back( e );
		}
		sort( g_glassesBar.begin(), g_glassesBar.end(), HairAndRaceCmp<SGlasses> );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Pick the bar entry with the greatest fStartPos still <= val (a floor bucket); null if val is below the first
// entry (= the "removed/none" extreme). (retail SetMMTension scan.)
template<class T>
static const T* PickFaceGenBucket( const vector<T> &bar, float val )
{
	for ( int i = 0; i < (int)bar.size(); ++i )
		if ( bar[i].fStartPos <= val && ( i + 1 == (int)bar.size() || val < bar[i+1].fStartPos ) )
			return &bar[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetMMTension @0x25f730 -- set a macro-muscle's tension (a FaceGen face-shape slider) and flag the
// mesh dirty so the downstream transformer re-bakes. The hair/glasses sliders ALSO pick a DB model record by
// position and install it on the head (retail's record-selection specials): the editor's re-Visit rebuilds the
// head from pComplexHead, so mutating pHair/pMeshes[0] swaps the model live; CreateHeadInfo copies them to the commit.
void CHeadTransformInfo::SetMMTension( const string &name, float value )
{
	mmTensions[name] = value;
	bTensionUpdated = true;
	++nTensionStamp;   // signal the live texture-preview node to re-composite (it only re-bakes when this changes)

	if ( !IsValid( pComplexHead ) )
		return;
	InitFaceGenBars();
	if ( name == "WomanHair" )
	{
		// Women Hair -> swap the hair MESH. Pre-clear so the bald extreme (HairFake/null at +1.0) removes it.
		pComplexHead->pHair = 0;
		const SHair *e = PickFaceGenBucket( g_hairBar, value );
		if ( e && IsValid( e->pHair ) )
			pComplexHead->pHair = e->pHair->pHair;
	}
	else if ( name == "EyeGlasses" )
	{
		// Eye-glasses -> install/remove the glasses mesh (pMeshes[0], a rendered head-mesh slot). Pre-clear so the
		// low extreme = no glasses.
		pComplexHead->pMeshes[0] = 0;
		const SGlasses *e = PickFaceGenBucket( g_glassesBar, value );
		if ( e && IsValid( e->pGlasses ) )
			pComplexHead->pMeshes[0] = e->pGlasses->pGlasses;
	}
	else if ( name == "Nationality" )
	{
		// Nationality -> pick the race; pComplexHead->pBodyColor drives the BODY skin material (neck/hands) via
		// NRender::ChooseBodyColor at render. No pre-clear (race is never "none"). The face texture handles the
		// FACE skin separately (the European.0/Arab.0 texture layers).
		const SRace *e = PickFaceGenBucket( g_raceBar, value );
		if ( e && IsValid( e->pRace ) )
			pComplexHead->pBodyColor = e->pRace;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NeedUpdate @0x2628c0 -- refresh the time node (so the idle anim advances) and OR in the tension-dirty
// flag; when true the DG bumps our version and the mesh/texture transformers re-bake.
bool CHeadTransformInfo::NeedUpdate()
{
	bool bTimeChanged = IsValid( pTime ) ? pTime.Refresh() : false;
	return bTimeChanged | bTensionUpdated;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Recalc @0x260000 -- the macro-muscle tensions + the idle sequence are applied to the head mesh by the
// downstream mesh transformer (which reads this node); here we only clear the tension-dirty flag once
// the version bump (CVersioningBase::DoUpdate) has signalled the change. (Sentinels-LS adaptation: the
// SS1 Recalc pushed tensions into a per-instance ITransformer; the mesh path uses the shared tree +
// the mesh IAnimators instead -- see the LSHead.h banner.)
void CHeadTransformInfo::Recalc()
{
	// retail @0x660000: drive the per-instance transformer from the slider tensions, then Generate() the
	// morphed GDP geometry into value.pAnimator (which CHeadAnimator::Recalc renders).
	if ( !bMorphBuilt )
		BuildMorphRig();
	if ( bMorphOk )
	{
		value.pTransformer->ClearAllMacroMuscles();
		for ( unordered_map<string, float>::const_iterator it = mmTensions.begin(); it != mmTensions.end(); ++it )
		{
			LifeStudioHeadAPI::IMacroMuscle *pMM = value.pMMTree->FindMacroMuscle( it->first.c_str() );
			if ( pMM )
				value.pTransformer->AddMacroMuscle( pMM, it->second );
		}
		value.pTransformer->ComputePhysics();
		value.pTransformer->Generate();        // -> value.pAnimator now holds the morphed GDP vertices
	}
	if ( bTexRigOk )
	{
		// Mirror the same tensions into the separate texture-weight rig so its FaceGen output channels
		// (read via UserItem in the face-texture bake) reflect the current sliders. Never Saved. Own
		// try/catch so a texture-rig fault can't crash the DG update or disturb the mesh morph above.
		try
		{
			value.pTexTransformer->ClearAllMacroMuscles();
			for ( unordered_map<string, float>::const_iterator it = mmTensions.begin(); it != mmTensions.end(); ++it )
			{
				LifeStudioHeadAPI::IMacroMuscle *pMM = value.pMMTree->FindMacroMuscle( it->first.c_str() );
				if ( pMM )
					value.pTexTransformer->AddMacroMuscle( pMM, it->second );
			}
			value.pTexTransformer->ComputePhysics();
			value.pTexTransformer->Generate();
		}
		catch(...)
		{
			OutputDebugString( "Exception: CHeadTransformInfo texture-weight Recalc" );
			bTexRigOk = false;
		}
	}
	bTensionUpdated = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenMeshHolder::Recalc -- rebuild the live CHeadMeshInfo from the baked info (base topology + the
// morphed animator stream), exactly as CHeadMeshLoader::Recalc does from a CResourceOpener. Runs once on
// first access and again after a load (pValue starts null). The single animatorStreams[0] is the whole-head
// morph; CHeadAnimator(static)::Recalc Process()es it over all UVs.
void CFaceGenMeshHolder::Recalc()
{
	EnsureLSInit();   // IAnimator::Create() below is a main-API call -> Init() must precede it
	try
	{
		pValue = new CHeadMeshInfo;
		pValue->nVertices = info.nVertices;
		pValue->copys     = info.copys;
		pValue->UVs       = info.UVs;
		pValue->indices   = info.indices;
		pValue->tris      = info.tris;
		pValue->pLSAnimators.resize( info.animatorStreams.size() );
		for ( int i = 0; i < info.animatorStreams.size(); ++i )
		{
			pValue->pLSAnimators[i] = LifeStudioHeadAPI::IAnimator::Create();
			pValue->pLSAnimators[i]->Load( (const char *)info.animatorStreams[i].GetBuffer(), info.animatorStreams[i].GetSize() );
			LoadLSTree();
			if ( pLSTree )
				pValue->pLSAnimators[i]->RegisterMacroMuscle( pLSTree->RootMacroMuscle() );
		}
	}
	catch(...)
	{
		OutputDebugString( "Exception: CFaceGenMeshHolder::Recalc()" );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FaceGen FACE-TEXTURE bake (retail CHeadTextureTransformer / CreateFaceTexture @0x25fda0 / MixTexture
// @0x25f120). The committed hero's recoloured skin is composited on the CPU into a 256x256 ARGB image and
// persisted as CHeadInfo::pTexture. Per-layer weights come from the morphed transformer's FaceGen OUTPUT
// channels (LifeStudio UserItem API) keyed by each DB layer's szName -- the slider->channel mapping lives in
// Res\FaceGenHead.gdp, so we query the live transformer rather than reconstruct it.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenTextureHolder -- holds the baked 256x256 face texture (CArray2D<SPixel8888>) and lazily uploads it
// to an NGfx::CTexture on Recalc. Serialisable pointee of CHeadInfo::pTexture so the skin round-trips through
// save/load (the composited pixels ARE the persisted state -- no LifeStudio needed at load). reg 0x11042144.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceGenTextureHolder: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CFaceGenTextureHolder);
	ZDATA
	CArray2D<NGfx::SPixel8888> image;   // the composited face texture (CPU pixels; the PERSISTED state)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&image); return 0; }
protected:
	virtual bool NeedUpdate() { return !IsValid( pValue ); }   // upload once, and again after load (mirror CResourceLoader)
	virtual void Recalc();
public:
	CArray2D<NGfx::SPixel8888>& Image() { return image; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenTextureHolder::Recalc()
{
	if ( image.GetXSize() <= 0 || image.GetYSize() <= 0 )
		return;
	if ( !IsValid( pValue ) )
		pValue = NGfx::MakeTexture( image.GetXSize(), image.GetYSize(), 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	if ( !IsValid( pValue ) )
		return;   // device-lost / texture-pool exhaustion -> skip the upload (mirror CBinkVideoPlayer::Recalc); CTextureLock would deref null
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
	// Bound by BOTH surfaces (CArray2D release operator[] is unchecked): equal at 256 (pow2, no NP2 padding),
	// but defensive against any lock/image size divergence.
	int ny = lock.GetYSize() < image.GetYSize() ? lock.GetYSize() : image.GetYSize();
	int nx = lock.GetXSize() < image.GetXSize() ? lock.GetXSize() : image.GetXSize();
	for ( int y = 0; y < ny; ++y )
		for ( int x = 0; x < nx; ++x )
			lock[y][x] = image[y][x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Build one face/eye/eyelash layer list: copy the DB CHeadTexture records, sort ascending by nPriority
// (MixTexCmp), resolve each to its software-pixel source (GetSWTex); drop layers with no texture / SW data.
static void BuildTexLayers( vector<SMixTex> &out, const vector< CPtr<NDb::CHeadTexture> > &src )
{
	vector< CPtr<NDb::CHeadTexture> > tmp( src.begin(), src.end() );
	sort( tmp.begin(), tmp.end(), MixTexCmp );
	for ( int i = 0; i < tmp.size(); ++i )
	{
		if ( !IsValid( tmp[i] ) || !IsValid( tmp[i]->pTexture ) )
			continue;
		CPtrFuncBase<NGScene::CSWTextureData> *sw = NGScene::GetSWTex( tmp[i]->pTexture );
		if ( !sw )
			continue;
		SMixTex layer;
		layer.pTex   = sw;
		layer.szName = tmp[i]->szName;
		out.push_back( layer );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Per-layer weight from the transformer's FaceGen output channel (UserItem) named by the layer szName:
// weight = (avg + 1)/2, mapping the channel's [-1,1] -> [0,1]; absent channel -> 0. (retail GetTextureWeights.)
static void GetTextureWeights( vector<float> &out, LifeStudioHeadAPI::IAnimator *pAnim, vector<SMixTex> &layers )
{
	out.resize( layers.size() );
	for ( int i = 0; i < layers.size(); ++i )
	{
		float w = 0.0f;
		if ( pAnim )
		{
			LifeStudioHeadAPI::UserID id = pAnim->UserItem( layers[i].szName.c_str() );
			// Defensive: treat any out-of-range id as "absent" and cap the value count. A heap-corrupting
			// garbage return from the LifeStudio query does NOT throw, so the surrounding try/catch can't catch
			// it -- bounding id/n here keeps a bad return from being walked into an OOB read of UserValue.
			if ( id >= 0 && id < 0x10000 )
			{
				int n = pAnim->UserValuesCount( id );
				if ( n > 0 && n <= 256 )
				{
					float sum = 0.0f;
					for ( int j = 0; j < n; ++j )
						sum += pAnim->UserValue( id, j );
					w = ( sum + n ) / ( 2.0f * n );
				}
			}
		}
		out[i] = w;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Fill a dst rect with `fill`, then V-flip src-over-blend each weighted layer's source pixels into it
// (alpha = src.a * weight / 255; dst.a forced opaque). (retail MixTexture @0x25f120.)
// Returns true if at least one layer actually blended (a real recolour happened, vs. just the base fill).
static bool MixTexture( CArray2D<NGfx::SPixel8888> &dst, int x0, int y0, int x1, int y1,
                        vector<SMixTex> &layers, const vector<float> &weights, DWORD fill )
{
	bool bBlended = false;
	for ( int y = y0; y < y1; ++y )
		for ( int x = x0; x < x1; ++x )
			dst[y][x].color = fill;
	for ( int i = 0; i < layers.size(); ++i )
	{
		float weight = ( i < weights.size() ) ? weights[i] : 0.0f;
		if ( weight < 1e-12f )
			continue;
		if ( !IsValid( layers[i].pTex ) )
			continue;
		layers[i].pTex.Refresh();
		NGScene::CSWTextureData *pSWD = layers[i].pTex->GetValue();
		if ( !pSWD || pSWD->mips.empty() )
			continue;
		bBlended = true;
		CArray2D<NGfx::SPixel8888> &srcImg = pSWD->mips[0];
		int rectW = x1 - x0, rectH = y1 - y0;
		int w = ( rectW < srcImg.GetXSize() ) ? rectW : srcImg.GetXSize();
		int h = ( rectH < srcImg.GetYSize() ) ? rectH : srcImg.GetYSize();
		for ( int ry = 0; ry < h; ++ry )
		{
			for ( int rx = 0; rx < w; ++rx )
			{
				NGfx::SPixel8888 &dp = dst[ (y1 - 1) - ry ][ x0 + rx ];   // V-flip on dst
				const NGfx::SPixel8888 &sp = srcImg[ry][rx];
				float alpha = sp.a * weight * ( 1.0f / 255.0f );
				float inv = 1.0f - alpha;
				int r = (int)( sp.r * alpha + dp.r * inv + 0.5f );
				int g = (int)( sp.g * alpha + dp.g * inv + 0.5f );
				int b = (int)( sp.b * alpha + dp.b * inv + 0.5f );
				dp.r = r < 0 ? 0 : ( r > 255 ? 255 : r );
				dp.g = g < 0 ? 0 : ( g > 255 ? 255 : g );
				dp.b = b < 0 ? 0 : ( b > 255 ? 255 : b );
				dp.a = 0xFF;
			}
		}
	}
	return bBlended;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Composite the 256x256 FaceGen face texture: face quadrant + eye + eyelash/teeth sub-rects, each weighted
// by the morph channels, over a skin-brown base fill. (retail CreateFaceTexture @0x25fda0; rects/fills disasm.)
// Returns true iff at least one layer actually blended -- i.e. the morph channels drove a real recolour. When
// false the image is only the flat base fills (NOT the head's real skin), so the caller must NOT apply it.
static bool CreateFaceTexture( CArray2D<NGfx::SPixel8888> &dst, LifeStudioHeadAPI::IAnimator *pAnim,
                               vector<SMixTex> &face, vector<SMixTex> &eye, vector<SMixTex> &eyelash )
{
	bool bBlended = false;
	vector<float> w;
	GetTextureWeights( w, pAnim, face );
	bBlended |= MixTexture( dst, 0x00, 0x80, 0x80, 0x100, face,    w, 0xFF808080 );    // FACE           x[0,128) y[128,256)
	GetTextureWeights( w, pAnim, eye );
	bBlended |= MixTexture( dst, 0x40, 0x40, 0x80, 0x080, eye,     w, 0x00000000 );    // EYE            x[64,128) y[64,128)
	GetTextureWeights( w, pAnim, eyelash );
	bBlended |= MixTexture( dst, 0x00, 0x00, 0x40, 0x040, eyelash, w, 0x00000000 );    // EYELASH/TEETH  x[0,64)  y[0,64)
	vector<SMixTex> none; vector<float> nw;
	MixTexture( dst, 0x00, 0x40, 0x40, 0x080, none,    nw, 0xFF5F1D20 );   // SKIN-BROWN base x[0,64) y[64,128)
	return bBlended;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadTextureTransformer -- the LIVE editor-preview face-texture node (see the LSHead.h banner). Shares the
// composite core with the static bake; re-composites only when a slider moved, so the AdvFaceGen preview
// retints in real time without re-baking every frame.
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadTextureTransformer::CHeadTextureTransformer( CHeadTransformInfo *p, NDb::CHead *pDbHead ): nLastStamp(-1)
{
	pTransformInfo = p;
	if ( IsValid( pDbHead ) && IsValid( pDbHead->pTransformableTextures ) )
	{
		NDb::CHeadTextures *pTT = pDbHead->pTransformableTextures;
		BuildTexLayers( face,    pTT->face );
		BuildTexLayers( eye,     pTT->eye );
		BuildTexLayers( eyelash, pTT->eyelash_teeth );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeadTextureTransformer::NeedUpdate()
{
	// Refresh the morph node so its texture-weight rig reflects the latest sliders (and builds lazily on the
	// first frame). Re-bake only when a slider actually moved (the tension stamp changed) or there's no texture
	// yet -- NOT every frame (the idle anim bumps the morph node's version each frame, but the skin depends only
	// on the tensions). NB: do NOT call our own GetValue() here -- it ASSERTs IsFrameMatch() inside DoUpdate;
	// pValue and GetTensionStamp() are plain reads.
	pTransformInfo.Refresh();
	if ( !IsValid( pTransformInfo ) )
		return false;
	if ( !IsValid( pValue ) )
		return true;
	return nLastStamp != pTransformInfo->GetTensionStamp();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadTextureTransformer::Recalc()
{
	if ( !IsValid( pTransformInfo ) )
		return;
	LifeStudioHeadAPI::IAnimator *pW = pTransformInfo->GetTexWeightSource();
	if ( !pW )
		return;   // morph rig not built yet -> retry next frame (pValue stays null)
	try
	{
		CArray2D<NGfx::SPixel8888> image( 256, 256 );
		image.FillZero();
		bool bRec = CreateFaceTexture( image, pW, face, eye, eyelash );
		nLastStamp = pTransformInfo->GetTensionStamp();
		if ( !bRec )
			return;   // channels didn't resolve -> leave pValue (head keeps its base material)
		if ( !IsValid( pValue ) )
			pValue = NGfx::MakeTexture( 256, 256, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
		if ( !IsValid( pValue ) )
			return;
		NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
		int ny = lock.GetYSize() < image.GetYSize() ? lock.GetYSize() : image.GetYSize();
		int nx = lock.GetXSize() < image.GetXSize() ? lock.GetXSize() : image.GetXSize();
		for ( int y = 0; y < ny; ++y )
			for ( int x = 0; x < nx; ++x )
				lock[y][x] = image[y][x];
	}
	catch(...)
	{
		OutputDebugString( "Exception: CHeadTextureTransformer::Recalc()" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateHeadInfo @0x260bf0 -- publish a renderable CHeadInfo for this morphed head, BAKING the live slider
// morph into a serialisable static mesh so the committed hero keeps its face (and it survives save/load).
// Mirrors retail GetMeshInfo @0x2604d0: copy the base "Heads"-pack TOPOLOGY (nVertices/copys/UVs/indices/
// tris) and serialise the morphed GDP animator (value.pAnimator, post-Generate) into animatorStreams[0].
// The morph rides in the VERTEX POSITIONS that the in-game CHeadAnimator(static) Process()es from that
// animator; topology is identical to the base, so the render path needs no further change. (The face
// TEXTURE bake -- headInfo->pTexture via CHeadTextureTransformer -- is the deferred follow-up B1b.)
CHeadInfo* CHeadTransformInfo::CreateHeadInfo()
{
	CHeadInfo *pHeadInfo = new CHeadInfo( pComplexHead );   // base resolve (material/hair/face+IF meshes)

	if ( !bMorphBuilt )
		BuildMorphRig();
	if ( bMorphOk && IsValid( pComplexHead ) && IsValid( pComplexHead->pHead ) )
	{
		Recalc();   // drive value.pTransformer from mmTensions, Generate() -> value.pAnimator (morphed verts)
		LifeStudioHeadAPI::IAnimator *pAnim = GetMorphedAnimator();
		if ( pAnim )
		{
			CObj<CFaceGenMeshHolder> pHolder = new CFaceGenMeshHolder;
			SRawHeadMeshInfo &info = pHolder->GetInfo();

			// (1) base topology -- deep-copy the live-render vectors from the shared base head mesh.
			CDGPtr< CPtrFuncBase<CHeadMeshInfo> > pBase = shareHeads.Get( pComplexHead->pHead->GetRecordID() );
			if ( IsValid( pBase ) )
			{
				pBase.Refresh();
				CHeadMeshInfo *pBaseMesh = pBase->GetValue();
				if ( pBaseMesh )
				{
					info.nVertices = pBaseMesh->nVertices;
					info.copys     = pBaseMesh->copys;
					info.UVs       = pBaseMesh->UVs;
					info.indices   = pBaseMesh->indices;
					info.tris      = pBaseMesh->tris;
				}
			}

			// (2) bake the morphed animator into animatorStreams[0] (LSConverter idiom: Save -> CMemoryStream::Write).
			int nSize = pAnim->SaveBufferSize();
			if ( nSize > 0 )
			{
				vector<char> buf( nSize );
				if ( pAnim->Save( &buf[0] ) )
				{
					info.animatorStreams.resize( 1 );
					info.animatorStreams[0].Write( &buf[0], nSize );
				}
			}

			// Commit a static head ONLY if BOTH the morph stream AND the base topology resolved. A missing base
			// mesh (shareHeads.Get failed / its Recalc threw -> pBaseMesh null) would otherwise persist an
			// empty-topology head that OOB-writes on every render and round-trips corrupt. Empty -> bStaticHead
			// stays false -> safe un-morphed base fallback. (retail GetMeshInfo bakes+commits only inside `if pBaseMesh`.)
			if ( !info.animatorStreams.empty() && !info.nVertices.empty() && !info.UVs.empty() )
				pHeadInfo->SetStaticMesh( pHolder );   // pMesh = holder; bStaticHead = true (persists, tags 7/8)
		}

		// --- TEXTURE BAKE: composite the FaceGen face texture from the morphed channels; persist as pTexture. ---
		// The per-layer weights come from the SEPARATE texture-weight transformer (GetTexWeightSource() -- a
		// CollectUserItems rig, NOT the mesh transformer: querying UserItem on a non-collect transformer walks an
		// unallocated table and corrupts the heap). CreateFaceTexture composites face/eye/eyelash over a skin base
		// into a 256x256 image the committed hero renders (GView material branch). pTexture serialises (tag 6).
		LifeStudioHeadAPI::IAnimator *pTexWeights = GetTexWeightSource();
		if ( pTexWeights && IsValid( pComplexHead->pHead->pTransformableTextures ) )
		{
			// Own try/catch: the morph mesh is already committed above; a texture-bake fault must never break
			// the commit (the hero keeps its persisted morph + correct base skin).
			try
			{
				NDb::CHeadTextures *pTT = pComplexHead->pHead->pTransformableTextures;
				vector<SMixTex> face, eye, eyelash;
				BuildTexLayers( face,    pTT->face );
				BuildTexLayers( eye,     pTT->eye );
				BuildTexLayers( eyelash, pTT->eyelash_teeth );

				CObj<CFaceGenTextureHolder> pTexHolder = new CFaceGenTextureHolder;
				pTexHolder->Image().SetSizes( 256, 256 );
				pTexHolder->Image().FillZero();   // CreateFaceTexture only writes 4 sub-rects; zero the rest so unwritten atlas pixels aren't heap garbage
				bool bRecoloured = CreateFaceTexture( pTexHolder->Image(), pTexWeights, face, eye, eyelash );
				// Apply the baked texture ONLY if the morph channels actually drove a recolour; otherwise the image is
				// just flat base fills (NOT the head's real skin), so leave pTexture null -> the head keeps its correct
				// base material (GView falls back to CreateMaterialShared). Self-activates once the weight source works.
				if ( bRecoloured )
					pHeadInfo->SetFaceTexture( pTexHolder );   // pTexture = holder; rendered via the GView pTexture material branch
			}
			catch(...)
			{
				OutputDebugString( "Exception: CHeadTransformInfo face-texture bake" );
			}
		}
	}

	return pHeadInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NLSHead;
REGISTER_SAVELOAD_CLASS( 0x10942150, CHeadBound )
REGISTER_SAVELOAD_CLASS( 0x10942151, CHeadAnimator )
REGISTER_SAVELOAD_CLASS( 0x10942152, CHead )
REGISTER_SAVELOAD_CLASS( 0x11042141, CHeadMeshLoader )
REGISTER_SAVELOAD_CLASS( 0x11042142, CHeadSequenceLoader )
REGISTER_SAVELOAD_CLASS( 0x11042143, CFaceGenMeshHolder )
REGISTER_SAVELOAD_CLASS( 0x11042144, CFaceGenTextureHolder )
REGISTER_SAVELOAD_CLASS( 0xA2543120, CHeadInfo )
REGISTER_SAVELOAD_CLASS( 0xA1743120, CHeadTransformInfo )
