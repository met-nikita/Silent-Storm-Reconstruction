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
	virtual void ProcessRangedAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores, vector<STrailPoint> *pTrail ) = 0;
	virtual EAttackResult ProcessThrowingAttackPortion( CAttackPortion *pA, IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID ) = 0;
	virtual int GetCompositeToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, NAI::EHitLocation eHL, bool bFirstTurn ) = 0;
	virtual int GetGrenadeCompositeToHit( NWorld::CUnit *pAttacker, 
		CVec3 ptTarget, bool bFirstTurn, NDb::CRPGGrenade *pGrenade ) = 0;
	virtual int GetTileCompositeToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos,
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn ) = 0;
	virtual int GetBazookaToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos,
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn ) = 0;
	virtual bool CheckVisibility( const NWorld::CUnit *pObserver, const NWorld::CUnit *pDest ) = 0;
	virtual bool CanSee( const NWorld::CUnit *pObserver, const CVec3 &vPos ) = 0;
	virtual bool CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos ) = 0;
	virtual bool CheckAIPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos ) = 0;
	virtual void GetVisibilityArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pObserver ) = 0;
	// nPoses - bit mask, 1-lay, 2-croach, 4-stand
	virtual void GetVisibleFromArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pTarget, CVec3 &vNear, float fRadius, int nPoses = 7 ) = 0;
	virtual int GetCoverForAIUnit( CVec3 ptFrom, NWorld::CUnit *pIgnore, 
		NWorld::CUnit *pTarget, const NRPG::CAttackPortion &AttackPortion, NAI::EHitLocation HitLocation ) = 0;

	virtual CVec3 GetIllumination( const vector<CVec3> &unit ) = 0;
	virtual IVisionTracker* GetVisionTracker() = 0;
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
IObject* CreateObject( int nStages, NDb::CModel *pModel, int nStartStage = 0 ); // pModel - ��� �������� hit-�� �������
IObject* CreateObject( NDb::CObject *pDBObject, int nStartStage = 0 );
CObjectBase* CreateBuilding( NBuilding::CBuildingGrid *pGrid );
NDb::CRPGArmor* GetTerrainArmor();
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanShoot( CCoverInfo *pCover );
bool PeekRayForRocket( CCoverInfo *pCover, CRay *pRes, bool bHit );
bool PeekRay( CCoverInfo *pCover, CRay *pRes, float fHit, bool *bIsMiss, bool bStickTo_pRes = false );
float CheckToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, int nExtraAP, NAI::EHitLocation eHL, 
	const vector<int> &accessibleHLs, CCoverInfo *pCover, bool bFirstRound, int *nToHit );
float CheckTileToHit( NWorld::CUnit *pAttacker, const CVec3 ptTarget, int nExtraAP,
	NAI::ETileHitLocation eHitLocation, CCoverInfo *pCover, bool bFirstRound, int *nToHit );
void GetOccupiedCubes( vector<CVec3> *pRes, const NAI::SPosition &pos );
void GetTileOccupiedCubes( vector<CVec3> *pRes, const CVec3 &ptPos, NAI::ETileHitLocation eHitLocation );
NAI::EDirection GetShootDirection( NAI::IPathNetwork *pNet, const NAI::SPathPlace &from, const CVec3 &ptTarget );
CVec3 GetMeleeAttackPos( const NWorld::CUnit *pAttacker, const CVec3 &ptTarget );
////////////////////////////////////////////////////////////////////////////////////////////////////
// create mission time RPG info from Merc info or dbms record
IGame* CreateGame( NAI::IAIMap *pAIMap, NAI::IPathNetwork *pNet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif