#ifndef __WEXPLTRACKER_H_
#define __WEXPLTRACKER_H_

#include "wDynObject.h"
#include "aiCollider.h"
#include "aiVoxelRender.h"
#include "aiMap.h"             // NAI::IAIMapTracker (base of the retail CExplosionCube) + IAIMap
#include "wInterface.h"        // NWorld::IExplosionMaster (base of the retail CExplosionMaster)
#include "..\Misc\HPTimer.h"
#include "wExplosionPerks.h"   // NWorld::SPerkMineModifiers (explosive-perk damage modifiers)

namespace NAI
{
	class IAIMap;
}

namespace NDb
{
	class CRPGGrenade;
	class CRPGEngGrenade;
}

namespace NRPG
{
	class IAttackable;
	class CAttackPortion;
}

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorld;
class CVoxelExpl;
class CVoxelExplTracker;
class CUnitServer;
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_VOXEL_SIZE = 0.2f; // meters
const int N_CUBE_SIZE = 16; //30;//16; // voxels // must be a multiple of 2
const float F_CUBE_SIZE = N_CUBE_SIZE * F_VOXEL_SIZE;
const int N_REAL_CUBE_SIZE = N_CUBE_SIZE + 2;
const float F_REAL_CUBE_SIZE = N_REAL_CUBE_SIZE * F_VOXEL_SIZE;
const float F_WAVE_ATTENUATION_COEFF = 0.4f;
const float F_IGNITION_OBJECT_DAMAGE_MULT = 10.0f;   // Game.exe VA 0x8c9fc4: a trapped object self-amplifies its own blast
////////////////////////////////////////////////////////////////////////////////////////////////////
// release NWorld::nBreakCalcs -- the engine-wide "explosion work spent this segment" budget. Every 16^3
// explosion-cube voxel trace bumps it (release CExplosionCube::Recalc @0x355080 tail); CWorld::Segment
// resets it once per world segment (release ResetBreakExplCalcs @0x3549f0, done at the top of
// CExplosionMaster::Segment @0x3571d0). Once MORE than one cube got traced in a segment, the blast's
// damage pass (and any further blast stepping) is postponed to the NEXT segment -- together with the
// inter-ring S_WAIT lag this is what paces retail's slow radial destruction ripple.
void ResetBreakExplCalcs();
////////////////////////////////////////////////////////////////////////////////////////////////////
// SExplVoxelCoords (retail PDB: NAMESPACE-scope, 6 bytes {ushort nX,nY,nZ}) -- one GLOBAL voxel-grid
// coordinate of the retail wavefront (0x400 x 0x400 x 0x80 world voxel space). Written RAW on the
// wire (DoDataVector<NWorld::SExplVoxelCoords> @0x359c00) -- deliberately NO member operator&.
// W5 serialization-convergence: replaces the dev CExplCube::SExplVoxelCoords 3-byte nested struct.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SExplVoxelCoords
{
	unsigned short nX, nY, nZ;
	//
	SExplVoxelCoords() {}
	SExplVoxelCoords( unsigned short _nX, unsigned short _nY, unsigned short _nZ ):
		nX( _nX ), nY( _nY ), nZ( _nZ ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExpl (retail id 0x51682140, 140 bytes) -- ONE wavefront ring of a blast, flood-filled over
// the GLOBAL voxel space (C3DLookupTable resolve over the shared CExplosionSpace grid below).
// W5 serialization-convergence: the dev per-blast CExplCube machine (id 0x52382160, ABSENT from
// retail) is REMOVED; layout, member types and tag numbers now match retail operator& @0x3598d0
// (tags 2-23) so retail v1.2 mid-blast saves load correctly.
////////////////////////////////////////////////////////////////////////////////////////////////////
class C3DLookupTable;
class CExplosionSpace;
class CVoxelExpl: public CObjectBase
{
	OBJECT_BASIC_METHODS( CVoxelExpl );
public:
	struct SObjectDamageInfo
	{
		bool bDestroyed, bPutDecal;
		int nVolume;
		CRay rDir;
		SObjectDamageInfo() : bDestroyed(false), bPutDecal(false), nVolume(0) {}
	};
	// retail hash_map<int,SObjectDamageInfo,hash<int>> (tag 13, DoHashMap): keyed by the SPACE's
	// blast-object id -- only objects actually touched by the wave have an entry.
	typedef unordered_map<int, SObjectDamageInfo> CDamageInfoHash;
private:
	//
	ZDATA
	bool bFinished;                            // +0x0c tag 2
	CVec3 ptCenter;                            // +0x10 tag 3
	CPtr<CUnitServer> pThrower;                // +0x1c tag 4
	CDBPtr<NDb::CRPGGrenade> pGrenade;         // +0x20 tag 5
	CObj<C3DLookupTable> pLookup;              // +0x24 tag 6  this ring's own voxel lookup grid (ctor-built)
	CPtr<CExplosionSpace> pSpace;              // +0x28 tag 7  the blast campaign's shared voxel space
	CPtr<CVoxelExplTracker> pTracker;          // +0x2c tag 8
	int nMaxVolume;                            // +0x30 tag 9  volume budget (voxels)
	int nIteration;                            // +0x34 tag 10 flood-fill wave counter (StartWave seeds 2)
	int nCurrentWave;                          // +0x38 tag 11 0-BASED ring index (retail; the dev nWave was 1-based)
	int nCurrentFrontElement;                  // +0x3c tag 12 resume cursor inside `front` (nBreakCalcs throttle)
public:
	CDamageInfoHash damageInfo;                // +0x40 tag 13
	int nObjectsDestroyed;                     // +0x54 tag 14
	int nEnemyUnitsKilled;                     // +0x58 tag 15
	int nVolume;                               // +0x5c tag 16 voxels claimed so far
private:
	vector<SExplVoxelCoords> front;            // +0x60 tag 17 the previous wave's claimed voxels
	vector<SExplVoxelCoords> newFront;         // +0x6c tag 18 this wave's claim buffer (double buffer)
	int nNewFrontSize;                         // +0x78 tag 19 logical count written into newFront
	CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;   // +0x7c tag 20 the engineer-grenade record (either record drives the blast)
	int nEngSkill;                             // +0x80 tag 21 the thrower/placer eng skill the eng blast scales with
	CPtr<CWorld> pWorld;                       // +0x84 tag 22 (retail CPtr<IWorld>; the pointee type is not on-wire)
	int nIgnitionObjectIdx;                    // +0x88 tag 23 the source object's SPACE id (-1 = none)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bFinished); f.Add(3,&ptCenter); f.Add(4,&pThrower); f.Add(5,&pGrenade); f.Add(6,&pLookup); f.Add(7,&pSpace); f.Add(8,&pTracker); f.Add(9,&nMaxVolume); f.Add(10,&nIteration); f.Add(11,&nCurrentWave); f.Add(12,&nCurrentFrontElement); f.Add(13,&damageInfo); f.Add(14,&nObjectsDestroyed); f.Add(15,&nEnemyUnitsKilled); f.Add(16,&nVolume); f.Add(17,&front); f.Add(18,&newFront); f.Add(19,&nNewFrontSize); f.Add(20,&pEngGrenade); f.Add(21,&nEngSkill); f.Add(22,&pWorld); f.Add(23,&nIgnitionObjectIdx); return 0; }   // retail @0x3598d0
	//
	static CVec3 GetDirection( int nX1, int nY1, int nZ1, int nX2, int nY2, int nZ2 );   // retail @0x354a40
	void StartWave( CObjectBase *pIgnitionObject );                                      // retail @0x355830
	void ProcessNeighborVoxels( int nX, int nY, int nZ );                                // retail @0x355590
	void SubProcessNeighborVoxels( int nPX, int nPY, int nPZ, int nX, int nY, int nZ );  // retail @0x355480
	void ApplyWaveDamage();                                                              // retail @0x355ba0
	void CheckWaveResults();                                                             // retail @0x3553a0
	int GetVolume( float fRadius );
	//
public:
	//
	CVoxelExpl(): nEngSkill( 0 ), nNewFrontSize( 0 ), nIgnitionObjectIdx( -1 ) {}   // the saveload factory fills tags
	// retail ctor @0x3562c0: the ignition object is passed THROUGH to StartWave, not stored
	CVoxelExpl( CWorld *_pWorld, CVec3 _ptCenter, CObjectBase *_pIgnitionObject, int _nCurrentWave,
		NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, CVoxelExplTracker *_pTracker,
		CExplosionSpace *_pSpace, NDb::CRPGEngGrenade *_pEngGrenade = 0, int _nEngSkill = 0 );
	//
	bool IsFinished() { return bFinished; }
	void MakeSingleIteration();                       // retail @0x355610: one wave (or one voxel under throttle)
	void MakeDamage();                                // retail @0x356580: the ring's damage, applied in a SEPARATE per-segment pass
	void GetFront( vector<CVec3> *pRes );             // retail @0x354e10: world-space centres of the live front (AI viewer)
	void GetTouchedObjects( vector<CVec3> *pRes );    // retail @0x354f10: world-space centres of every touched voxel (AI viewer)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExplTracker (retail id 0x52782130, 132 bytes) -- one blast: spawns/drains the per-ring
// CVoxelExpl waves under the master's pacing. W5 serialization-convergence: base is retail's
// CObjectBase (the master's `explosions` vector owns it -- no longer a miscObjects IDynamicObject),
// tags 11-17 now match retail operator& @0x35ab10 (11 sMineModifiers ON-WIRE, 12 bIsFinished,
// 13-15 damagedUnits/damagedObjects/drawDecals, 16 pSpace CObj, 17 pIgnitionObject CObj).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CActionCounter;
class CVoxelExplTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS( CVoxelExplTracker );
public:
	typedef unordered_map< CPtr<CObjectBase>, bool, SPtrHash> CDecalsHash;
private:
	ZDATA
	int nWave;                                 // tag 2: rings COMPLETED, 0-based (MakeDamage increments)
	CVec3 ptCenter;                            // tag 3
	CDBPtr<NDb::CRPGGrenade> pGrenade;         // tag 4
	CPtr<CUnitServer> pThrower;                // tag 5
	int nEnemyUnitsKilled;                     // tag 6
	int nObjectsDestroyed;                     // tag 7
	CObj<CActionCounter> pAction;              // tag 8
	CPtr<CWorld> pWorld;                       // tag 9 (retail CPtr<IWorld>; the pointee type is not on-wire)
	CObj<CVoxelExpl> pExpl;                    // tag 10: the in-flight ring
public:
	SPerkMineModifiers sMineModifiers;         // tag 11 (retail Data(12)): thrower/placer explosive-perk modifiers,
	                                           // received ALREADY FILLED from CWorld::AddGrenadeExplosion (retail shape)
	bool bIsFinished;                          // tag 12: all rings done (the master compacts on it)
	list< CPtr<CUnitServer> > damagedUnits;    // tag 13
	unordered_map< CPtr<CObjectBase>, list<int>, SPtrHash > damagedObjects;   // tag 14
	CDecalsHash drawDecals;                    // tag 15
private:
	CObj<CExplosionSpace> pSpace;              // tag 16: the blast campaign's shared voxel space
	CObj<CObjectBase> pIgnitionObject;         // tag 17: forwarded to each CVoxelExpl ring (retail CObj; was dev CPtr tag 14)
	CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;   // tag 18: the engineer-grenade record
	int nEngSkill;                             // tag 19: eng skill; scales waves/radius/damage/fragments
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nWave); f.Add(3,&ptCenter); f.Add(4,&pGrenade); f.Add(5,&pThrower); f.Add(6,&nEnemyUnitsKilled); f.Add(7,&nObjectsDestroyed); f.Add(8,&pAction); f.Add(9,&pWorld); f.Add(10,&pExpl); f.Add(11,&sMineModifiers); f.Add(12,&bIsFinished); f.Add(13,&damagedUnits); f.Add(14,&damagedObjects); f.Add(15,&drawDecals); f.Add(16,&pSpace); f.Add(17,&pIgnitionObject); f.Add(18,&pEngGrenade); f.Add(19,&nEngSkill); return 0; }   // retail @0x35ab10
	//
	void ExplodeFragments();
	//
public:
	//
	CVoxelExplTracker(): nWave( 0 ), nEnemyUnitsKilled( 0 ), nObjectsDestroyed( 0 ), bIsFinished( false ), nEngSkill( 0 ) {}
	// retail unified ctor @0x356ff0: takes the SPACE + an already-Filled modifiers struct (+ both records + nEngSkill)
	CVoxelExplTracker( CExplosionSpace *_pSpace, CVec3 _ptCenter, CObjectBase *_pIgnitionObject,
		NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, const SPerkMineModifiers &_modifiers,
		CWorld *_pWorld, NDb::CRPGEngGrenade *_pEngGrenade = 0, int _nEngSkill = 0 );
	//
	bool IsFinished() const { return bIsFinished; }
	bool MakeSingleStep();   // retail @0x3565c0: (re)spawn the current ring + one iteration; false == ring settled
	void MakeDamage();       // retail @0x3566d0: apply the settled ring's damage, advance nWave, finish the blast
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ============================== retail wExplTracker voxel subsystem ==============================
// W4 serialization-convergence landed the shared-space classes (5 registered ids
// 0x02443140..0x02443144 + 2 embedded structs); W5 rerouted CVoxelExpl/CVoxelExplTracker onto them
// (dev CExplCube removed): CWorld::AddGrenadeExplosion enqueues into the master (retail vtbl+0x14
// STD / +0x10 ENG), whose Segment drives the trackers with the literal retail loop @0x3571d0.
// Oracles: s2_scratch src/s2_expltracker.h, src/s2_cexplosioncube.h, src/s2_cexplosionmaster.h.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionCube;
class CExplosionSpace;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IFlushCache -- a deferred cache-invalidation callback (retail base of C3DLookupTable). The one
// virtual is DoFlush (retail IFlushCache vtbl+0x10), invoked through SFlushCacheList::Flush.
////////////////////////////////////////////////////////////////////////////////////////////////////
class IFlushCache: public CObjectBase
{
public:
	virtual void DoFlush() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SFlushCacheList (retail 4 bytes) -- the deferred-flush registry a CExplosionSpace embeds.
// Serialized inline by CExplosionSpace (retail CallObjectSerialize<SFlushCacheList>, tag 4).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFlushCacheList
{
	ZDATA
	list< CPtr<IFlushCache> > caches;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&caches); return 0; }   // retail inlined operator& (instantiation @0x35b790)
	//
	// retail @0x359460: append the cache, holding one reference (a null cache still links a null node)
	void RegisterCache( IFlushCache *pCache );
	// retail @0x358a90: one pass -- erase dead entries (null / CObjectBase dead-flag, == !IsValid),
	// DoFlush the live ones. The next node is fetched BEFORE acting so a re-entrant DoFlush is safe.
	void Flush();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CIndexCube (retail id 0x02443142, 24 bytes) -- one 16^3 cube of the C3DLookupTable voxel grid;
// a flat vector of shorts (0xFFFF == "touched by the blast").
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIndexCube: public CObjectBase
{
	OBJECT_BASIC_METHODS( CIndexCube );
public:
	ZDATA
	vector<unsigned short> data;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&data); return 0; }   // retail @0x358d30 (tag 2 = DoDataVector<ushort>)
	//
	CIndexCube() {}
	// retail @0x358360: nResolution^3 voxels, all filled with nFill
	CIndexCube( int nResolution, const unsigned short &nFill ) { data.resize( nResolution * nResolution * nResolution, nFill ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// C3DLookupTable (retail id 0x02443143, 64 bytes) -- the per-explosion voxel lookup grid: a 64x64x8
// CArray3D of CObj<CIndexCube> plus a one-cube resolve cache (nGCx == 0x7fffffff => nothing cached).
// It is an IFlushCache: it registers itself in its space's cacheList at construction, and DoFlush
// (retail FlushCache @0x3587b0) drops the resolve cache so the next lookup re-resolves.
////////////////////////////////////////////////////////////////////////////////////////////////////
class C3DLookupTable: public IFlushCache
{
	OBJECT_BASIC_METHODS( C3DLookupTable );
public:
	struct SIndexCubeInfo
	{
		int x, y, z;
		CIndexCube *p;
	};
	//
	ZDATA
	CArray3D< CObj<CIndexCube> > grid;
	int nGCx, nGCy, nGCz;               // cached cube grid coords (nGCx==0x7fffffff => none)
	CPtr<CExplosionCube> pExplCube;     // cached explosion cube
	CPtr<CIndexCube> pIndexCube;        // cached index cube
	CPtr<CExplosionSpace> pSpace;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&grid); f.Add(3,&nGCx); f.Add(4,&nGCy); f.Add(5,&nGCz); f.Add(6,&pExplCube); f.Add(7,&pIndexCube); f.Add(8,&pSpace); return 0; }   // retail @0x35c150
	//
	C3DLookupTable(): nGCx( 0x7fffffff ), nGCy( 0 ), nGCz( 0 ) {}   // retail @0x3594f0 ("no cube cached" sentinel armed)
	C3DLookupTable( CExplosionSpace *pSpace );                      // retail @0x35a0d0 (cache registration + 64x64x8 grid)
	//
	// retail @0x3583d0: refill *pRes with one entry per non-null grid cell, z/y/x order
	void GetAllCubes( vector<SIndexCubeInfo> *pRes );
	// retail C3DLookupTable::FlushCache @0x3587b0 (the IFlushCache::DoFlush override): drop the resolve cache
	virtual void DoFlush();
	// retail C3DLookupTable::GetObject @0x358d60 (W5): resolve one GLOBAL voxel -- cube coords =
	// voxel>>4; on a resolve-cache miss FetchCube the explosion cube + lazily create the 16^3
	// CIndexCube grid slot. Returns *pObj = the cube's rasterised voxel value (0 empty, 1 terrain,
	// >1 blast-object id) and *ppCell = a WRITABLE pointer into CIndexCube::data (the cell the
	// flood-fill marks with the wave number / the 0xFFFF touched sentinel).
	void GetObject( int nX, int nY, int nZ, unsigned short *pObj, unsigned short **ppCell );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionCube (retail id 0x02443140, 60 bytes) -- one 16^3 voxel cube of the explosion space's
// AI grid, an IAIMapTracker over its world region: whenever the AI map under it changes, OnChange
// marks it for recalc and the next Recalc re-rasterises the cube from the AI map. NOT the dev
// CExplCube above (id 0x52382160, a different per-blast wavefront cube).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionCube: public NAI::IAIMapTracker
{
	OBJECT_BASIC_METHODS( CExplosionCube );
public:
	ZDATA
	CVec3 vCenter;
	CPtr<NAI::IAIMap> pAIMap;
	vector<NAI::SExplVoxel> rvoxels;    // 0x1000 == 16^3 voxels, X<->Z-swapped copy of the renderer grid
	int nDeltaX, nDeltaY, nDeltaZ;      // this cube's voxel offset in the 0x400 x 0x400 x 0x80 world grid
	CPtr<CExplosionSpace> pSpace;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vCenter); f.Add(3,&pAIMap); f.Add(4,&rvoxels); f.Add(5,&nDeltaX); f.Add(6,&nDeltaY); f.Add(7,&nDeltaZ); f.Add(8,&pSpace); return 0; }   // retail @0x359e00
	//
	bool bCalced;       // NOT serialized (retail too): a loaded cube re-rasterises on first use
	bool bNeedRecalc;
	//
	CExplosionCube(): nDeltaX( 0 ), nDeltaY( 0 ), nDeltaZ( 0 ), bCalced( false ), bNeedRecalc( false ) {}
	// retail @0x358240: store centre/deltas/space/aimap, register on the AI map as a region tracker
	// (the copy ctor @0x3586b0 is the member-wise default -- no tracker registration; compiler-generated)
	CExplosionCube( CExplosionSpace *_pSpace, NAI::IAIMap *_pAIMap, const CVec3 &_vCenter, int _nDeltaX, int _nDeltaY, int _nDeltaZ );
	//
	virtual void OnChange() { bNeedRecalc = true; }   // retail @0x3581a0 (IAIMapTracker callback)
	// retail @0x355080: rasterise the AI map into a transient 16^3 CExplVoxelRenderer over vCenter,
	// copy it into rvoxels with the X<->Z reindex, force world-boundary voxels to terrain (=1),
	// spend one unit of the per-segment work budget (nBreakCalcs).
	void Recalc();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionSpace (retail id 0x02443141, 72 bytes) -- the shared voxel/AI-map space of one blast
// campaign: a 64x64x8 grid of CObj<CExplosionCube> (lazily filled), the AI map, the deferred-flush
// registry and the blast-object registry (index 0 reserved, 1 == terrain).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionSpace: public CObjectBase
{
	OBJECT_BASIC_METHODS( CExplosionSpace );
public:
	ZDATA
	CArray3D< CObj<CExplosionCube> > grid;
	CPtr<NAI::IAIMap> pAIMap;
	SFlushCacheList cacheList;
	NAI::CExplVoxelRenderer::CObjectsHash objects;   // blast-object registry (the renderer's user-object hash)
	int nObjectsEnd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&grid); f.Add(3,&pAIMap); f.Add(4,&cacheList); f.Add(5,&objects); f.Add(6,&nObjectsEnd); return 0; }   // retail @0x35b710
	//
	CExplosionSpace(): nObjectsEnd( 0 ) {}   // retail @0x35a8d0 (empty grid; nObjectsEnd = 0)
	// retail @0x35a960: store the AI map, allocate the 64x64x8 (0x8000 null slots) cube grid
	CExplosionSpace( NAI::IAIMap *_pAIMap );
	//
	// retail @0x358c60: drain the deferred-flush registry, then re-arm every changed registered cube
	// (bNeedRecalc set => clear bCalced so the next resolve re-rasterises it).
	void SyncWithAIMap();
	//
	// retail @0x358ae0 (W5): bounds-checked cube resolve -- lazily `new CExplosionCube(this, pAIMap,
	// centre=(g+0.5)*F_CUBE_SIZE, deltas=g<<4)` into the grid slot, then Recalc (self-gated on
	// bCalced) so the cube is rasterised. Null on out-of-grid coords.
	CExplosionCube* FetchCube( int nX, int nY, int nZ );
	// retail @0x357840 (W5): world position -> GLOBAL voxel coords, ROUND(coord/F_VOXEL_SIZE - 0.5)
	// clamped to [1,0x3fe] (x/y) / [1,0x7e] (z).
	void GetCoord( const CVec3 *pPos, SExplVoxelCoords *pRes );
	// retail @0x357790 (W5): GLOBAL voxel coords -> the voxel's world-space centre.
	void GetCenter( int nX, int nY, int nZ, CVec3 *pRes );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionMaster (retail id 0x02443144, 44 bytes) -- the engine-wide explosion scheduler: a queue
// of pending blasts (toBeStarted), the in-flight tracker list, the shared space and the
// S_IDLE/S_ACTIVE/S_WAIT state machine, driven once per world segment.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionMaster: public IExplosionMaster
{
	OBJECT_BASIC_METHODS( CExplosionMaster );
public:
	enum EState
	{
		S_IDLE   = 0,
		S_ACTIVE = 1,
		S_WAIT   = 2,
	};
	// one queued explosion request (retail 44 bytes; unregistered, serialized inline by the master)
	struct SStartInfo
	{
		ZDATA
		CVec3 vCenter;
		CDBPtr<NDb::CRPGGrenade> pGrenade;
		CPtr<CUnitServer> pThrower;
		SPerkMineModifiers modifiers;
		CPtr<CObjectBase> pIgnitionObject;
		CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;
		int nEngSkill;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vCenter); f.Add(3,&pGrenade); f.Add(4,&pThrower); f.Add(5,&modifiers); f.Add(6,&pIgnitionObject); f.Add(7,&pEngGrenade); f.Add(8,&nEngSkill); return 0; }   // retail @0x35b5b0
		//
		// retail @0x35b560: pointers null, modifiers identity ({1,1,false} == the SPerkMineModifiers
		// default). NB retail leaves vCenter/nEngSkill indeterminate here -- every producer overwrites
		// them; kept faithful (a deserialized node loads all tags anyway).
		SStartInfo() {}
		// retail @0x358480 (ordinary grenade, no eng record)
		SStartInfo( const CVec3 &_vCenter, CObjectBase *_pIgnitionObject, NDb::CRPGGrenade *_pGrenade,
			CUnitServer *_pThrower, const SPerkMineModifiers &_modifiers ):
			vCenter( _vCenter ), pGrenade( _pGrenade ), pThrower( _pThrower ), modifiers( _modifiers ),
			pIgnitionObject( _pIgnitionObject ), nEngSkill( 0 ) {}
		// retail @0x358500 (engineer grenade)
		SStartInfo( const CVec3 &_vCenter, CObjectBase *_pIgnitionObject, NDb::CRPGEngGrenade *_pEngGrenade,
			CUnitServer *_pThrower, const SPerkMineModifiers &_modifiers, int _nEngSkill ):
			vCenter( _vCenter ), pThrower( _pThrower ), modifiers( _modifiers ),
			pIgnitionObject( _pIgnitionObject ), pEngGrenade( _pEngGrenade ), nEngSkill( _nEngSkill ) {}
	};
	//
	ZDATA
	CPtr<IWorld> pWorld;
	vector< CObj<CVoxelExplTracker> > explosions;   // in-flight blast trackers
	CObj<CExplosionSpace> pSpace;                   // shared voxel space (built on first AddExplosion, dropped at idle)
	list<SStartInfo> toBeStarted;                   // queued, not-yet-spawned blasts
	int nLag;                                       // S_WAIT countdown (segments)
	EState state;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorld); f.Add(3,&explosions); f.Add(4,&pSpace); f.Add(5,&toBeStarted); f.Add(6,&nLag); f.Add(7,&state); return 0; }   // retail @0x35b160
	//
	// retail @0x359630: empty lists, null world/space. NB retail leaves nLag/state indeterminate in
	// THIS ctor (only the IWorld one seeds them); the saveload factory fills both from tags 6/7.
	CExplosionMaster() {}
	CExplosionMaster( IWorld *_pWorld ): pWorld( _pWorld ), nLag( 0 ), state( S_IDLE ) {}   // retail @0x355ab0
	//
	// IExplosionMaster (retail vtbl+0x10 / +0x14 / +0x18)
	virtual void AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGEngGrenade *pEngGrenade,
		CUnitServer *pThrower, const SPerkMineModifiers &modifiers, int nEngSkill );   // retail @0x357620
	virtual void AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGGrenade *pGrenade,
		CUnitServer *pThrower, const SPerkMineModifiers &modifiers );                  // retail @0x357520
	virtual void Segment();                                                            // retail @0x3571d0
private:
	// shared head of both AddExplosion overloads: an idle master goes active and builds the shared space
	void BeginIfIdle();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CreateExplosionMaster @0x355b40: new CExplosionMaster(pWorld) (null on OOM)
IExplosionMaster* CreateExplosionMaster( IWorld *pWorld );
// retail NWorld::CreateExplosionSpace @0x357730: new CExplosionSpace(pAIMap) (null on OOM).
// Consumers: CExplosionMaster::BeginIfIdle + CAIViewer::VerifyVoxelExpl (retail @0x195f70).
CExplosionSpace* CreateExplosionSpace( NAI::IAIMap *pAIMap );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif