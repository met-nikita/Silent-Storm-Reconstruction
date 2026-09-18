#include "StdAfx.h"
#include "wDynObject.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "wBullet.h"
#include "wMain.h"
#include "GAnimation.h"
#include "GAnimParticles.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataRPG.h"
#include "aiMap.h"
#include "wUnitServer.h"
#include "RPGUnitMission.h"
#include "wAckBase.h"
#include "wDecal.h"
#include "wExplosionPerks.h"
#include "..\Misc\EventsBase.h"   // NGlobal::ThrowEvent
#include "eventUnit.h"            // NWorld::CEventOnBullet (AI bullet-perception event)

namespace NWorld
{
extern bool bShowBlood;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBulletServer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBulletServer: public IDynamicObject, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CBulletServer);
private:
	ZDATA
	int nTrailCount;
	float fTrailSpeed;
	STime sCast, sLastTrailTime;
	CPtr<CWorld> pWorld;
	vector<STime> sPassTime;
	CPtr<NDb::CModel> pModel;
	CObj<CActionCounter> pAction;
	CSyncSrcBind<IVisObj> bindGlobal;
	vector<NRPG::STrailPoint> trailpointsSet;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<CUnitServer> pShooter;
	CPtr<NDb::CRPGGrenade> pGrenade;
	int nEffectType = 0;
	CPtr<CUnitServer> pNearestTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nTrailCount); f.Add(3,&fTrailSpeed); f.Add(4,&sCast); f.Add(5,&sLastTrailTime); f.Add(6,&pWorld); f.Add(7,&sPassTime); f.Add(8,&pModel); f.Add(9,&pAction); f.Add(10,&bindGlobal); f.Add(11,&trailpointsSet); f.Add(12,&pAnimator); f.Add(13,&pShooter); f.Add(14,&pGrenade); f.Add(15,&nEffectType); f.Add(16,&pNearestTarget); return 0; }

protected:
	const CVec3 GetPosition();

public:
	CBulletServer() {}
	CBulletServer( CWorld *pWorld, const vector<NRPG::STrailPoint> &trail, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed, NDb::CRPGGrenade *pGrenade, int nEffectType );
	//
	bool Segment();
	void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBulletServer::CBulletServer( CWorld *_pWorld, const vector<NRPG::STrailPoint> &_trail, STime _sCast, NDb::CModel *_pTrailModel, float _fTrailSpeed, NDb::CRPGGrenade *_pGrenade, int _nEffectType )
	:pWorld(_pWorld), trailpointsSet( _trail ), sCast( _sCast ), pModel( _pTrailModel ), fTrailSpeed( _fTrailSpeed ), nTrailCount( 0 ), pGrenade( _pGrenade ), nEffectType( _nEffectType )
{
	sPassTime.resize( trailpointsSet.size() );
	vector<NAnimation::CATrailPath::STrailPoint> animTrailPoints( trailpointsSet.size() );

	STime sPass = sCast;
	for ( int nTemp = 0; nTemp < trailpointsSet.size(); nTemp++ )
	{
		if ( pModel && ( nTemp > 0 ) )
		{
			// Retail v1.2 0x7472f0: round/cap each leg, then accumulate its travel time.
			float fTravelTime = fabs( trailpointsSet[nTemp].vPosition - trailpointsSet[nTemp - 1].vPosition ) / fTrailSpeed * 1000.0f;
			sPass += Float2Int( Min( 3000.0f, fTravelTime ) );
		}
		sPassTime[nTemp] = sPass;

		CVec3 vDir( trailpointsSet[nTemp].vDir );
		Normalize( &vDir );

		animTrailPoints[nTemp].vDir = vDir * fTrailSpeed;
		animTrailPoints[nTemp].vPosition = trailpointsSet[nTemp].vPosition;
		animTrailPoints[nTemp].sPassTime = sPassTime[nTemp];
	}

	if ( pModel )
	{
		NAnimation::CATrailPath *pRay = new NAnimation::CATrailPath;
		pRay->Init( sCast, animTrailPoints );

		pAnimator = new NAnimation::CSkeletonAnimator(0);
		pAnimator->pTime = pWorld->GetTime();
		pAnimator->AddAnimator( pWorld->GetTime()->GetValue(), pRay );
	}

	bindGlobal.Link( pWorld->GetActive(), this );
	pAction = pWorld->GetActiveCounter();
	pShooter = pWorld->GetUnitServer( trailpointsSet[0].sAttack.pAttacker );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBulletServer::Visit( IRenderVisitor *pVisitor )
{
	if ( pModel )
		pVisitor->AddItemMesh( pModel, pAnimator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBulletServer::Segment()
{
	SRand rnd;
	STime sSegmentTime = pWorld->GetTime()->GetValue();
	for ( int nTemp = nTrailCount + 1; nTemp < trailpointsSet.size(); nTemp++ )
	{
		if ( sPassTime[nTemp] > sSegmentTime )
			break;

		nTrailCount = nTemp;
		NRPG::STrailPoint &sCurrent = trailpointsSet[nTemp];
		NDb::CRPGArmor *pArmor = sCurrent.pArmor;
		// Retail v1.2 0x746891: explosive ammo queues a neutral, ownerless blast
		// before processing the direct impact. Do not apply the shooter's grenade perks.
		if ( IsValid( pGrenade ) )
		{
			SPerkMineModifiers mods;
			pWorld->AddGrenadeExplosion( sCurrent.vPosition, pGrenade, 0, 0, &mods );
		}
		if ( sCurrent.pObject == 0 )
		{
			// terrain
			CDGPtr<CFuncBase<STerrainInfo> > pInfo = pWorld->GetTerrainInfo();
			if ( pInfo )
			{
				pInfo.Refresh();
				int nX = Float2Int( sCurrent.vPosition.x / FP_GRID_STEP );
				int nY = Float2Int( sCurrent.vPosition.y / FP_GRID_STEP );
				SStepSound ss = pInfo->GetValue().GetStepSound( nX, nY );
				if ( ss.pArmor )
					pArmor = ss.pArmor;
			}
		}
		CObjectBase *pCatcher = trailpointsSet[nTemp].pAttackTarget;
		CVec3 vPlace = sCurrent.vPosition;
		CVec3 vNormal = sCurrent.vNormal;
		bool bShowImpact = nEffectType >= 0;
		CDynamicCast<NRPG::IAttackable> pAttackCatcher(pCatcher);
		if (pAttackCatcher)
		{
			if ( IsValid( pCatcher ) )
			{
				// v1.2 0x746a40: resolve damage before deciding on flesh effects.
				int nDamage = pAttackCatcher->ProcessAttack( pWorld, sCurrent.nUserID, &sCurrent.sAttack, sCurrent.vDir, sCurrent.pArmor );
		
				if ( IsValid( pCatcher ) )
				{
					CDynamicCast<NWorld::CUnitServer> pUS(pCatcher);
					if (pUS)
					{
						if ( !IsValid( pNearestTarget ) )
							pNearestTarget = pUS;
						bShowImpact = nDamage > 0;
						pUS->GetUnitRPG()->BulletHit();
						CPtr<NWorld::CUnitServer> pTarget = pWorld->GetUnitServer( trailpointsSet[nTemp].sAttack.pTarget );
						if ( IsValid( pShooter ) )
						{
							if ( pTarget.GetPtr() == pUS.GetPtr() )
								pWorld->GetGlobalAck()->OnDoDamage( pShooter, pUS );
							else
								pWorld->GetGlobalAck()->OnDoAccidentalDamage( pShooter, pUS );
						}
					}
				}
			}
		}
		// v1.2 0x746b1c: a unit's mesh produces an impact only at its damaging
		// entry point, not at a visual-only exit or a different attack receiver.
		CDynamicCast<CUnitServer> pObjectUnit( sCurrent.pObject );
		if ( pObjectUnit )
			bShowImpact = bShowImpact && sCurrent.pObject == sCurrent.pAttackTarget;
		if ( !bShowImpact || !pArmor || sCurrent.nFloor >= 100 )
			continue;

		bool bBloodMaterial = pArmor->pMaterial && pArmor->pMaterial->GetRecordID() == NDb::CRPGMaterial::HUMAN_BODY;
		if ( !bBloodMaterial || bShowBlood )
		{
			NDb::CTEffect *pShotEffect = pArmor->GetShotEffect( nEffectType );
			if ( pShotEffect )
			{
				CVec3 dir = -vNormal;
				CQuat rndX( random.GetFloat(0,10000), CVec3(1,0,0) );
				if ( fabs( dir.x ) < 0.999f )
					rndX = CQuat( acos( dir.x ), CVec3( 0, -dir.z, dir.y ), true ) * rndX;
				else if ( dir.x < 0 )
					rndX = CQuat( FP_PI, CVec3(0,1,0) ) * rndX;
				pWorld->CreateParticle( vPlace, rndX, pShotEffect->GetEffect( &rnd ), sCurrent.nFloor );
			}
			if ( pArmor->pSoundShot )
				pWorld->MakeSound( vPlace, NDb::GetSound( pArmor->pSoundShot ) );
		}
		// Decals follow the surface object, not the damage receiver (exit points
		// can have a surface even though pAttackTarget is null).
		if ( pArmor->pShotMaterial && IsValid( sCurrent.pObject ) )
		{
			CDynamicCast<IBuilding> pBuilding( sCurrent.pObject );
			CObjectBase *pDecalTarget = pBuilding ? pBuilding->GetSceneHandle() : sCurrent.pObject.GetPtr();
			new CDecal( pWorld, vPlace, vNormal, pArmor->fShotRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pDecalTarget );
		}
		else if ( pArmor->pShotMaterial && sCurrent.pObject == 0 )
		{
			new CDecal( pWorld, vPlace, vNormal, pArmor->fShotRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pWorld->GetTerrainInfo() );
		}
	}

	// Retail v1.2 0x746f5e: an explosive bullet retires after its first reached point.
	if ( nTrailCount >= trailpointsSet.size() - 1 || ( nTrailCount > 0 && IsValid( pGrenade ) ) )
	{
		if ( IsValid( pShooter ) )
		{
			CRay ray;
			int nLast = trailpointsSet.size() - 1;
			ray.ptOrigin = trailpointsSet[0].vPosition;
			ray.ptDir = trailpointsSet[nLast].vPosition - ray.ptOrigin;
			float fDistance = fabs( ray.ptDir );   // shooter -> last-trail-point length, BEFORE normalize
			Normalize( &( ray.ptDir ) );
			// retail CBulletServer::Segment @0x3463a0: tell every subscribed AI unit about the shot. OnBullet adds the
			// shooter as a possibleEnemy when the ray passed within 2.5 of the unit -- being shot FROM CONCEALMENT now
			// alerts the AI (the marquee reactive-perception fix; the suspect now survives Populate, see aiUnitState).
			NGlobal::ThrowEvent( NWorld::CEventOnBullet( pShooter, pNearestTarget, ray, fDistance ) );
		}
		return true; // = erase
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateBulletServer( CWorld *pWorld, const vector<NRPG::STrailPoint> &trail,
		STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed, NDb::CRPGGrenade *pGrenade, int nEffectType )
{
	return new CBulletServer( pWorld, trail, sCast, pTrailModel, fTrailSpeed, pGrenade, nEffectType );
}
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0xB18B1160, CBulletServer )
