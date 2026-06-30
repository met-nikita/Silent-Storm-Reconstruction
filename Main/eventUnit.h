#ifndef __EVENT_UNIT_H__
#define __EVENT_UNIT_H__
//
#include "..\Misc\Geom.h"          // CRay / CVec3 (by-value event-payload members)
//
namespace NWorld
{
//
class CUnitServer;
class CDumbUnitServer;
class CPlayer;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnUnitLongBurst
{
public:
	CPtr<CUnitServer> pWho;
	CEventOnUnitLongBurst( CUnitServer *_pWho ): pWho( _pWho ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnUnitUnhide
{
public:
	CPtr<CDumbUnitServer> pWho;
	CEventOnUnitUnhide( CDumbUnitServer *_pWho ): pWho( _pWho ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ---- world AI-trigger event payloads (PDB-exact fields). HOISTED here from aiThreatTracker.h so the
// world producers (wBullet / wExplTracker / wUnitServer / wMain / wUnitAttackExec) can construct + throw
// them via NGlobal::ThrowEvent without dragging the NAI tracker header into world TUs. Consumed by
// CAIEventTrackerImpl's OnXxx handlers (aiThreatTracker.cpp). ----
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnSeeNewEnemy
{
public:
	bool bRealTime;
	CPtr<CUnitServer> pWatcher;
	CPtr<CUnitServer> pTarget;
	CEventOnSeeNewEnemy( CUnitServer *_pWatcher = 0, CUnitServer *_pTarget = 0, bool _bRealTime = false )
		: bRealTime( _bRealTime ), pWatcher( _pWatcher ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnLostEnemyFromSight
{
public:
	CPtr<CUnitServer> pWatcher;
	CPtr<CUnitServer> pTarget;
	CEventOnLostEnemyFromSight( CUnitServer *_pWatcher = 0, CUnitServer *_pTarget = 0 )
		: pWatcher( _pWatcher ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnHearEnemy
{
public:
	CPtr<CUnitServer> pHearer;
	CPtr<CUnitServer> pTarget;
	CEventOnHearEnemy( CUnitServer *_pHearer = 0, CUnitServer *_pTarget = 0 )
		: pHearer( _pHearer ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnHearAlly
{
public:
	CPtr<CUnitServer> pHearer;
	CPtr<CUnitServer> pTarget;
	CEventOnHearAlly( CUnitServer *_pHearer = 0, CUnitServer *_pTarget = 0 )
		: pHearer( _pHearer ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnGrenadeExplosion
{
public:
	CVec3 ptPos;
	CPtr<CUnitServer> pThrower;
	CEventOnGrenadeExplosion( CUnitServer *_pThrower = 0, const CVec3 &_ptPos = CVec3() )
		: ptPos( _ptPos ), pThrower( _pThrower ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnBullet
{
public:
	CRay ray;
	float fDistance;
	CPtr<CUnitServer> pShooter;
	CPtr<CUnitServer> pTarget;
	CEventOnBullet( CUnitServer *_pShooter = 0, CUnitServer *_pTarget = 0,
		const CRay &_ray = CRay(), float _fDistance = 0 )
		: ray( _ray ), fDistance( _fDistance ), pShooter( _pShooter ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnUnitDiedOrLoseConsciousness
{
public:
	CPtr<CUnitServer> pWho;
	CEventOnUnitDiedOrLoseConsciousness( CUnitServer *_pWho = 0 ): pWho( _pWho ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnPassControl
{
public:
	CPtr<CPlayer> pPlayer;
	CEventOnPassControl( CPlayer *_pPlayer = 0 ): pPlayer( _pPlayer ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnStartGame
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEventOnAttackAtUnit
{
public:
	CPtr<CUnitServer> pAttacker;
	CPtr<CUnitServer> pTarget;
	CEventOnAttackAtUnit( CUnitServer *_pAttacker = 0, CUnitServer *_pTarget = 0 )
		: pAttacker( _pAttacker ), pTarget( _pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __EVENT_UNIT_H__
