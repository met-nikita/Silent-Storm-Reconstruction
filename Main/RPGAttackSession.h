#ifndef __RPGATTACKSESSION_H_
#define __RPGATTACKSESSION_H_
#pragma once
#include "RPGUnitMission.h"
#include "RPGDiplomacy.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
class CUnit;
class CGlobalGame;
// Retail's transient cross-cast interface; not a CObjectBase or a save/load object.
class IUnitMissionForMedals
{
public:
	virtual void FinishAttackSession() = 0;
	virtual void AddAttackToAttackSession( IUnitMission *pTarget, bool bHit, bool bCritical ) = 0;
	virtual void AddPKHitToAttackSession( IUnitMission *pTarget ) = 0;
	virtual void AddWaitForBullet( CObjectBase *pBullet ) = 0;
protected:
	~IUnitMissionForMedals() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitMissionForMedals: public IUnitMissionForMedals
{
public:
	vector<CPtr<IUnitMission> > attackedInSession;
	vector<CPtr<IUnitMission> > hitInSession;
	vector<CPtr<IUnitMission> > criticalHits;
	vector<CPtr<IUnitMission> > hitsPK;
	vector<CPtr<CObjectBase> > waitForBullets;
	bool bFinished;
	CPtr<CUnit> pMedalsGainer;
	CPtr<CGlobalGame> pGame;

	CUnitMissionForMedals( CUnit *pUnit = 0, CGlobalGame *pGlobalGame = 0 );
	virtual void FinishAttackSession() { bFinished = true; }
	virtual void AddAttackToAttackSession( IUnitMission *pTarget, bool bHit, bool bCritical );
	virtual void AddPKHitToAttackSession( IUnitMission *pTarget );
	virtual void AddWaitForBullet( CObjectBase *pBullet ) { waitForBullets.push_back( pBullet ); }
	virtual NDb::EDiplomacyState MedalDiplomacyState( int nPlayer ) = 0;
	virtual NDb::CPanzerklein *GetPanzerklein() = 0;

	// Retail only drains expired weak projectile references here. It neither
	// awards medals nor clears the target lists nor gates world/AI actions.
	void Segment();
	// Present in retail, but has no caller in the inspected Silent Storm binaries.
	void AddMedalPoints();
};
}
#endif
