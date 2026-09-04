#ifndef __RPGGAME_H_
#define __RPGGAME_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataFormat.h"
#include "../DBFormat/DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class IAIMap;
	class IPathNetwork;
	enum EHitLocation;
	enum ETileHitLocation;
	enum EDirection;
	struct SPathPlace;
	struct SPosition;
	struct SUnitPosition;
}
namespace NWorld
{
	class CUnit;
}
namespace NBuilding
{
	class CBuildingGrid;
}
namespace NDb
{
	class CRPGChestReal;	// rolled chest loot (DataChest.h)
}
struct STerrainInfo;
#include "RPGAttackMech.h"
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit;
class CCoverInfo;
class IUnitMission;
class IObject;
class IAttackable;
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
struct SWound
{
	ZDATA
	CVec3 ptWhere, ptSmokeDir;
	CPtr<CObjectBase> pWho;
	CDBPtr<NDb::CRPGArmor> pArmor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptWhere); f.Add(3,&ptSmokeDir); f.Add(4,&pWho); f.Add(5,&pArmor); return 0; }

	SWound() {}
	SWound( CObjectBase *_pWho, const CVec3 &_ptWhere, const CVec3 &_ptSmokeDir, NDb::CRPGArmor *_pArmor )
		: pWho(_pWho), ptWhere(_ptWhere), ptSmokeDir(_ptSmokeDir), pArmor(_pArmor) {}
};
*/
struct STrailPoint
{
	ZDATA
	int nUserID;
	CVec3 vDir;
	CVec3 vPosition;
	CVec3 vNormal;
	CAttackPortion sAttack;
	CPtr<CObjectBase> pAttackTarget;
	CPtr<CObjectBase> pObject;
	CDBPtr<NDb::CRPGArmor> pArmor;
	int nFloor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nUserID); f.Add(3,&vDir); f.Add(4,&vPosition); f.Add(5,&vNormal); f.Add(6,&sAttack); f.Add(7,&pAttackTarget); f.Add(8,&pObject); f.Add(9,&pArmor); f.Add(10,&nFloor); return 0; }

	STrailPoint() {}
	STrailPoint( int _nUserID, const CVec3 &_vDir, const CVec3 &_vPosition, const CAttackPortion &_sAttack, CObjectBase *_pAttackTarget, CObjectBase *_pObject, NDb::CRPGArmor *_pArmor, const CVec3 &_vNormal, int _nFloor ):
		nUserID( _nUserID ), vDir( _vDir ), vPosition( _vPosition ), sAttack( _sAttack ), pAttackTarget(_pAttackTarget), pObject(_pObject), pArmor( _pArmor ), vNormal(_vNormal), nFloor(_nFloor) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVisibilitySpot
{
	CVec3 ptPos;
	int nCanSee; // 0 - see nothing, 1 - see everyone, 2 - see standing & croach, 3 - see standing

	SVisibilitySpot() {}
	SVisibilitySpot( const CVec3 _ptPos, int _nCanSee ): ptPos(_ptPos), nCanSee(_nCanSee) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAttackResult
{
	AR_IGNORE,
	AR_STUCK,
	AR_BOUNCE,
	AR_BOUNCE_BODY,
};
class IVisionTracker;
class IGame: public CObjectBase
{
public:
	virtual CCoverInfo* CalcCovers( const CVec3 &src, const CAttackPortion &attack, 
		NWorld::CUnit *pIgnore, NWorld::CUnit *pDest, int nTargetUserID, float fMinClearDistance, bool bAIMode = false ) = 0;
	virtual CCoverInfo* CalcCoversForTile( const CVec3 &src, const CAttackPortion &attack, NWorld::CUnit *pIgnore,
	const CVec3 &ptTarget, float fMinClearDistance ) = 0;
	virtual void ProcessMeleeAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores ) = 0;
	// retail keeps the per-attack range cap on SAttackRayInfo.fMaxRange (TraceLooseRaySegment @0x292010
	// gates fEnter/fExit on it and places the terminal trail point at origin+dir*fMaxRange); this tree's
	// tracer takes it as a parameter (30 = the old N_WEAPONTRAIL_MAXDISTANCE, retail's own value on the
	// normal/accidental shot paths; splinters pass fFragmentRange*FP_GRID_STEP).
	virtual void ProcessRangedAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores, vector<STrailPoint> *pTrail, float fMaxRange ) = 0;
	virtual EAttackResult ProcessThrowingAttackPortion( CAttackPortion *pA, IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID ) = 0;
	virtual int GetCompositeToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, NAI::EHitLocation eHL, bool bFirstTurn ) = 0;
	virtual int GetGrenadeCompositeToHit( NWorld::CUnit *pAttacker, 
		CVec3 ptTarget, bool bFirstTurn, NDb::CRPGGrenade *pGrenade ) = 0;
	virtual int GetTileCompositeToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos,
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn ) = 0;
	virtual int GetBazookaToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos,
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn ) = 0;
	// retail CGame::CheckVisibility @0x298cb0 (game vtbl+0x10) -- THE per-unit visibility probe:
	// fFOV = bUseFOV ? observer GetSightFOV() (pi, perk 0x52) : 2pi (0x40c90fdb, full circle);
	// fRange = GetUnitSightDistance(observer); routes to the 4-arg CheckPositionVisibility. The
	// perception sweep (CUnitServer::UpdateVisible @0x3c4450) passes bUseFOV = TRUE (disasm-proven
	// push ebx=1 @0x7c46fe). Other retail call sites' bools are unverified -- dev sites pass true
	// (the pre-port legacy leaf was a 180-degree half-space == the FOV-pi default, so true preserves
	// their effective semantics).
	virtual bool CheckVisibility( const NWorld::CUnit *pObserver, const NWorld::CUnit *pDest, bool bUseFOV ) = 0;
	virtual bool CanSee( const NWorld::CUnit *pObserver, const CVec3 &vPos ) = 0;
	// LEGACY 2-arg form: retail's CGame vtable has NO 2-arg overload (every retail caller hoists the
	// observer's real range/FOV and calls the 4-arg one -- all in-tree retail-counterpart sites converged).
	// Kept ONLY for the dev-only aiSignal corpse probe (no retail counterpart; the signal layer is gutted).
	virtual bool CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos ) = 0;
	// retail CGame::CheckPositionVisibility @0x298fa0 (game vtbl+0x1c) -- the 4-arg overload used by the
	// perception probe above AND the AI cover planner (NAI::IsUnitSeePosFromPos @0x3a270): is the target
	// visible from the observer within `fRange` and inside the cone of TOTAL angle `fFOVAngle` off the
	// observer's facing? Per occupied cube of the target: the retail 9-ray IsCubeVisible @0x2c9160
	// (vision vtbl+0x10, disasm-proven -- NOT the single-ray IsPointVisible); first visible cube wins.
	virtual bool CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos,
		float fRange, float fFOVAngle ) = 0;
	// retail CGame::SetVisionMultiplier @0x298740 / SetNight @0x299550. CWorld::UpdateVisible refreshes
	// these before every perception sweep from the world's current time-of-day sentinel.
	virtual void SetVisionMultiplier( float fMultiplier ) = 0;
	virtual void SetNight( bool bIsNight ) = 0;
	// retail CGame::GetUnitSightDistance @0x2984d0 (game vtbl+0x34): the unit's sight range --
	// CUnit::GetSightDistance (flat 20, perk 0x53), x1.5015 at night with the night-vision perk 0x51.
	virtual float GetUnitSightDistance( CUnit *pRPGUnit ) = 0;
	// retail CGame::GetMaxUnitSightDistance @0x298520 (game vtbl+0x38): 2x the sight range -- the
	// perception sweep's CANDIDATE-GATHER radius (matches MakeVisionQuery's 2x cap on the
	// height-stretched effective range).
	virtual float GetMaxUnitSightDistance( CUnit *pRPGUnit ) = 0;
	// retail CGame::IsCorpseVisible @0x298da0 (game vtbl+0x50): can the observer SEE the downed body?
	// One range/FOV-gated 9-ray IsCubeVisible per corpse hit-location point (CUnit::GetCorpseHLs,
	// vtbl+0x94); range = the 1x GetUnitSightDistance (NOT the 2x max); first visible point wins.
	// Consumer: the AI tracker's corpse scan (retail CheckForVisibleCorpses @0xab7a0).
	virtual bool IsCorpseVisible( const NWorld::CUnit *pObserver, const NWorld::CUnit *pCorpse ) = 0;
	virtual void GetVisibilityArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pObserver ) = 0;
	// nPoses - bit mask, 1-lay, 2-croach, 4-stand
	virtual void GetVisibleFromArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pTarget, CVec3 &vNear, float fRadius, int nPoses = 7 ) = 0;
	virtual int GetCoverForAIUnit( CVec3 ptFrom, NWorld::CUnit *pIgnore, 
		NWorld::CUnit *pTarget, const NRPG::CAttackPortion &AttackPortion, NAI::EHitLocation HitLocation ) = 0;

	virtual CVec3 GetIllumination( const vector<CVec3> &unit ) = 0;
	virtual IVisionTracker* GetVisionTracker() = 0;
	// retail CGame::UpdateVision @0x2995a0 (game vtbl+0x4c): forward to the vision tracker's
	// time-sliced changed-cube recalc -- the CWorld::TryUpdateVisible action-finish probe.
	virtual bool UpdateVision( float fTime ) = 0;
	// luaSetMaxCriticalSeverity @0x2ee0e0 -> SetMaxCriticalSeverity (retail CGame vtbl+0x48); read back by
	// the combat critical clamp (retail vtbl+0x44). The severity (DC) of a rolled critical is capped to it.
	virtual int GetMaxCriticalSeverity() const = 0;
	virtual void SetMaxCriticalSeverity( int n ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IUnitMission* CreateUnit( CUnit *pSrc );
// pInHandItem/pBackpack = rolled chest-loot equipment (retail @0x2c4f50 params 4/5, from
// SMapUnit); null = pers defaults
IUnitMission* CreateUnit( NDb::CRPGPers *pSrc, NDb::CRPGItem *pInHandItem = 0, NDb::CRPGChestReal *pBackpack = 0 );
IObject* CreateObject( int nStages, NDb::CModel *pModel, int nStartStage = 0 ); // pModel - for counting the object's hits
IObject* CreateObject( NDb::CObject *pDBObject, int nStartStage = 0 );
CObjectBase* CreateBuilding( NBuilding::CBuildingGrid *pGrid );
NDb::CRPGArmor* GetTerrainArmor();
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanShoot( CCoverInfo *pCover );
bool PeekRayForRocket( CCoverInfo *pCover, CRay *pRes, bool bHit );
bool PeekRay( CCoverInfo *pCover, CRay *pRes, float fHit, bool *bIsMiss, bool bStickTo_pRes = false );
// retail @0x2b5220/@0x2b53d0: the check fns carry the burst bullet index (default 0 -- one-shot
// weapons and the callers that predate the exec-side cursor).
float CheckToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, int nExtraAP, NAI::EHitLocation eHL,
	const vector<int> &accessibleHLs, CCoverInfo *pCover, bool bFirstRound, int *nToHit, int nBullet = 0 );
float CheckTileToHit( NWorld::CUnit *pAttacker, const CVec3 ptTarget, int nExtraAP,
	NAI::ETileHitLocation eHitLocation, CCoverInfo *pCover, bool bFirstRound, int *nToHit, int nBullet = 0 );
void GetOccupiedCubes( vector<CVec3> *pRes, const NAI::SPosition &pos );
void GetTileOccupiedCubes( vector<CVec3> *pRes, const CVec3 &ptPos, NAI::ETileHitLocation eHitLocation );
NAI::EDirection GetShootDirection( NAI::IPathNetwork *pNet, const NAI::SPathPlace &from, const CVec3 &ptTarget );
CVec3 GetMeleeAttackPos( const NWorld::CUnit *pAttacker, const CVec3 &ptTarget );
////////////////////////////////////////////////////////////////////////////////////////////////////
// create mission time RPG info from Merc info or dbms record
// retail NRPG::CreateGame @0x299150 threads the built terrain into the vision tracker.
IGame* CreateGame( NAI::IAIMap *pAIMap, NAI::IPathNetwork *pNet, const STerrainInfo &terrainInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
