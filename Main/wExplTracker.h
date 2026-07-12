#ifndef __WEXPLTRACKER_H_
#define __WEXPLTRACKER_H_

#include "wDynObject.h"
#include "aiCollider.h"
#include "aiVoxelRender.h"
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
// CExplCube
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplCube: public CObjectBase
{
	OBJECT_BASIC_METHODS( CExplCube );
public:
	//
	typedef unsigned char byte;
	//
	struct SExplVoxelCoords
	{
		ZDATA
		byte nX;
		byte nY;
		byte nZ;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nX); f.Add(3,&nY); f.Add(4,&nZ); return 0; }
		//
		SExplVoxelCoords() {}
		SExplVoxelCoords( byte _nX, byte _nY, byte _nZ ):
			nX( _nX ), nY( _nY ), nZ( _nZ ) {}
	};
	//
	enum ENeighbourCube
	{
		NC_LEFT = 0,
		NC_RIGHT,
		NC_UPPER,
		NC_UNDER,
		NC_DISTANT,
		NC_NEAR
	};
	//
	NAI::CExplVoxelRenderer renderer;
	//
	ZDATA
	bool bFinished;
	CVec3 ptCenter;
	CPtr<NAI::IAIMap> pAIMap;
	CPtr<CVoxelExpl> pExplosion;
	vector<SExplVoxelCoords> voxels;
	int nFront;
	int nFrontSize;
	int nEmpty;
	vector< CPtr<CExplCube> > neighborCubes;
	vector< CVec3 > neighborCubesCoords;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bFinished); f.Add(3,&ptCenter); f.Add(4,&pAIMap); f.Add(5,&pExplosion); f.Add(6,&voxels); f.Add(7,&nFront); f.Add(8,&nFrontSize); f.Add(9,&nEmpty); f.Add(10,&neighborCubes); f.Add(11,&neighborCubesCoords); return 0; }
	//
	CVec3 GetDirection( int nX1, int nY1, int nZ1, int nX2, int nY2, int nZ2 );
	CExplCube *GetNeighborCube( ENeighbourCube cube );
	bool ProcessBoundaryVoxel( int nX, int nY, int nZ );
	void SubProcessBoundaryVoxel( CExplCube *pNeighborCube, int nNX, int nNY, int nNZ, bool *bBoundaryVoxel );
	void SubProcessNeighborVoxels( int nPX, int nPY, int nPZ, int nX, int nY, int nZ );
	void ProcessNeighborVoxels( int nX, int nY, int nZ );
	void ExpandFront( int nPX, int nPY, int nPZ, int nX, int nY, int nZ );
public:
	//
	CExplCube() {}
	CExplCube( CVec3 _ptCenter, NAI::IAIMap *_pAIMap, CVoxelExpl *_pExplosion );
	//
	CVec3 GetVoxelCenter( int nX, int nY, int nZ );
	void MakeStep();
	bool IsInCube( CVec3 ptCoords );
	bool IsEmpty( int nX, int nY, int nZ );
	void Wave( int nX, int nY, int nZ );
	void Front( int nX, int nY, int nZ );
	bool IsFinished() { return bFinished; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExpl
////////////////////////////////////////////////////////////////////////////////////////////////////
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
private:
	//
	ZDATA
	bool bFinished;
	CVec3 ptCenter;
	CPtr<NAI::IAIMap> pAIMap;
	CPtr<CUnitServer> pThrower;
	CDBPtr<NDb::CRPGGrenade> pGrenade;
	CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;   // release CVoxelExpl @0x7c -- the engineer-grenade record (either record drives the blast)
	int nEngSkill;                             // release CVoxelExpl @0x80 -- the thrower/placer eng skill the eng blast scales with
	list< CObj<CExplCube> > cubesToAdd;
	CPtr<CVoxelExplTracker> pTracker;
	int nMaxVolume;
	int nWave;
	NHPTimer::STime tOverrun;
	int nCurrentCube;
public:
	vector< CObj<CExplCube> > cubes;
	NAI::CExplVoxelRenderer::CObjectsHash objects; // object at index 0 is reserved, 1 is terrain
	vector<SObjectDamageInfo> damageInfo;
	int nObjectsEnd;
	int nObjectsDestroyed;
	int nEnemyUnitsKilled;
	int nVolume;
	CPtr<CObjectBase> pIgnitionObject;   // release CVoxelExpl ctor param_3 -- the object whose fire/trap set off this blast
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bFinished); f.Add(3,&ptCenter); f.Add(4,&pAIMap); f.Add(5,&pThrower); f.Add(6,&pGrenade); f.Add(7,&cubesToAdd); f.Add(8,&pTracker); f.Add(9,&nMaxVolume); f.Add(10,&nWave); f.Add(11,&tOverrun); f.Add(12,&nCurrentCube); f.Add(13,&cubes); f.Add(14,&objects); f.Add(15,&damageInfo); f.Add(16,&nObjectsEnd); f.Add(17,&nObjectsDestroyed); f.Add(18,&nEnemyUnitsKilled); f.Add(19,&nVolume); f.Add(20,&pIgnitionObject); f.Add(21,&pEngGrenade); f.Add(22,&nEngSkill); return 0; }
	//
	void ApplyWaveDamage();
	void CheckWaveResults();
	void ExplodeWave();
	int GetVolume( float fRadius );
	//
public:
	//
	CVoxelExpl(): nEngSkill( 0 ) {}   // the saveload factory fills tags; old saves (no tag 22) must load a clean 0
	CVoxelExpl( CVec3 _ptCenter, int _nWave, NDb::CRPGGrenade *_pGrenade,
		CUnitServer *_pThrower, NAI::IAIMap *_pAIMap, CVoxelExplTracker* _pTracker, CObjectBase *_pIgnitionObject = 0,
		NDb::CRPGEngGrenade *_pEngGrenade = 0, int _nEngSkill = 0 );
	//
	CExplCube *GetExplCube( CVec3 ptCoords );
	bool IsFinished() { return bFinished; }
	void Segment();
	void MakeDamage();   // release CVoxelExpl::MakeDamage @0x356580: the ring's damage, applied in a SEPARATE per-segment pass
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExplTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CActionCounter;
class CVoxelExplTracker: public IDynamicObject
{
	OBJECT_BASIC_METHODS( CVoxelExplTracker );
public:
	typedef unordered_map< CPtr<CObjectBase>, bool, SPtrHash> CDecalsHash;
private:
	ZDATA
	int nWave;
	CVec3 ptCenter;
	CDBPtr<NDb::CRPGGrenade> pGrenade;
	CDBPtr<NDb::CRPGEngGrenade> pEngGrenade;   // retail tracker @0x7c (operator& @0x35ab10 tag 18) -- the engineer-grenade record
	int nEngSkill;                             // retail tracker @0x80 (tag 19) -- eng skill; scales waves/radius/damage/fragments
	CPtr<CUnitServer> pThrower;
	int nEnemyUnitsKilled;
	int nObjectsDestroyed;
	CObj<CActionCounter> pAction;
	CPtr<CWorld> pWorld;
	CObj<CVoxelExpl> pExpl;
	CPtr<CObjectBase> pIgnitionObject;   // release tracker @120 -- forwarded to each CVoxelExpl wave
public:
	list< CPtr<CUnitServer> > damagedUnits;
	unordered_map< CPtr<CObjectBase>, list<int>, SPtrHash > damagedObjects;
	CDecalsHash drawDecals;
	SPerkMineModifiers sMineModifiers;   // thrower's explosive-perk modifiers, seeded at AddGrenadeExplosion; transient
	                                     // (default {1,1,false} = no-op). Read by CVoxelExpl via pTracker in ExplodeWave.
private:
	// pEngGrenade/nEngSkill carry the RETAIL tag numbers (operator& @0x35ab10 tags 18/19); dev tags 15-17 stay free
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nWave); f.Add(3,&ptCenter); f.Add(4,&pGrenade); f.Add(5,&pThrower); f.Add(6,&nEnemyUnitsKilled); f.Add(7,&nObjectsDestroyed); f.Add(8,&pAction); f.Add(9,&pWorld); f.Add(10,&pExpl); f.Add(11,&damagedUnits); f.Add(12,&damagedObjects); f.Add(13,&drawDecals); f.Add(14,&pIgnitionObject); f.Add(18,&pEngGrenade); f.Add(19,&nEngSkill); return 0; }
	//
	int nLag;                            // retail inter-ring wait: CExplosionMaster::Segment @0x3571d0 parks in S_WAIT with
	                                     // nLag=2 segments after each ring's damage before spawning the next CVoxelExpl.
	                                     // Transient (NOT serialized -- retail keeps it on the master, which this build
	                                     // doesn't have; a mid-blast load just resumes without the residual wait).
	//
	void ExplodeFragments();
	void ApplyWaveDamageToUnits();
	//
public:
	//
	CVoxelExplTracker(): nLag( 0 ), nEngSkill( 0 ) {}
	CVoxelExplTracker( CVec3 _ptCenter, NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, CWorld *_pWorld, CObjectBase *_pIgnitionObject = 0, const SPerkMineModifiers *_pMods = 0,
		NDb::CRPGEngGrenade *_pEngGrenade = 0, int _nEngSkill = 0 );   // retail unified ctor @0x756ff0 takes BOTH records + nEngSkill
	//
	virtual bool Segment();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif