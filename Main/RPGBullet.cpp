#include "StdAfx.h"
#include "RPGBullet.h"
#include "aiMap.h"				// NAI::IAIMap::Trace
#include "aiGrid.h"				// NAI::CPathNetwork (complete -- for SPosition::pNet refcount)
#include "wInterface.h"			// NWorld::CUnit / IWorld
#include "wMain.h"				// NWorld::CWorld
#include "wBullet.h"				// NWorld::CreateBulletServer
#include "wUnitServer.h"			// NWorld::CUnitServer (complete -- for CObj<CUnitServer>)
#include "wTSFlags.h"			// NWorld::TS_*
#include "RPGUnitMission.h"		// NRPG::IUnitMissionInfo (complete -- for CAttackPortion's CPtr members)
#include "rpgCheatConstants.h"	// CHEAT_GODMODE
#include "../DBFormat/DataMap.h"

namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAttackRayInfo ctors
////////////////////////////////////////////////////////////////////////////////////////////////////
SAttackRayInfo::SAttackRayInfo()
	: bFirstTurn( false ), nExtraAP( 0 ), nBullet( 0 ), bTargetIsHit( false ),
	  fMinClearDistance( 0 ), fMaxRange( 0 )
{
	// pUS / pIgnore / pTarget = null, trailPoints empty, atk default. `from` default-constructs:
	// from.pos.p is the SPathPlace default sentinel (nData = -1 with nFinal cleared == 0xFDFFFFFF).
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// 12-arg ctor @0x291330
SAttackRayInfo::SAttackRayInfo( const CAttackPortion &_atk, const CVec3 &_vOrigin, const CVec3 &_vDir,
	NWorld::CUnitServer *_pUS, const NAI::SUnitPosition &_from, int _nBullet, int _nExtraAP,
	bool _bTargetIsHit, float _fMinClearDistance, float _fMaxRange, CObjectBase *_pTarget )
	: pUS( _pUS ), from( _from ), bFirstTurn( false ), nExtraAP( _nExtraAP ), nBullet( _nBullet ),
	  vOrigin( _vOrigin ), vDir( _vDir ), bTargetIsHit( _bTargetIsHit ),
	  fMinClearDistance( _fMinClearDistance ), fMaxRange( _fMaxRange ), atk( _atk ), pTarget( _pTarget )
{
	// Retail v1.2 0x69118a: the shooter's RPG turn counter, not the world's turn.
	if ( pUS )
		bFirstTurn = pUS->GetUnitRPG()->IsFirstTurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// 8-arg ctor @0x291450 (no shooter / no from-position)
SAttackRayInfo::SAttackRayInfo( const CAttackPortion &_atk, const CVec3 &_vOrigin, const CVec3 &_vDir,
	bool _bTargetIsHit, float _fMinClearDistance, float _fMaxRange, CObjectBase *_pTarget )
	: bFirstTurn( false ), nExtraAP( 0 ), nBullet( 0 ), vOrigin( _vOrigin ), vDir( _vDir ),
	  bTargetIsHit( _bTargetIsHit ), fMinClearDistance( _fMinClearDistance ), fMaxRange( _fMaxRange ),
	  atk( _atk ), pTarget( _pTarget )
{
	// pUS / pIgnore = null. `from` default-constructs: from.pos.p is the SPathPlace default sentinel
	// (0xFDFFFFFF), from.pos.pNet = null.
	// ORIGINAL BUG (confirmed via disasm @0x291450): from.bRun is never written by this ctor, so it
	// is left indeterminate -- reproduced by not touching from.bRun.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::MakeSplinter @0x291f00 -- initialize *dst as a vertical splinter ray.
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeSplinter( SAttackRayInfo *dst, const CAttackPortion *atk, const CVec3 *origin, float fMaxRange )
{
	if ( !dst )
		return;
	dst->pUS = 0;
	// from.pos.p = SPathPlace() invalid sentinel (0xFDFFFFFF), from.pos.pNet = null.
	dst->from.pos = NAI::SPosition();
	// ORIGINAL BUG (confirmed @0x291f00): from.bRun is never written -> left as-is.
	dst->bFirstTurn = false;
	dst->nExtraAP = 0;
	dst->nBullet = 0;
	dst->vOrigin = origin ? *origin : CVec3( 0, 0, 0 );
	dst->vDir = CVec3( 0, 0, 1 );		// unit ray pointing up +Z
	dst->bTargetIsHit = false;
	dst->pIgnore = 0;
	dst->fMinClearDistance = 0;
	dst->fMaxRange = fMaxRange;
	if ( atk )
		dst->atk = *atk;
	dst->pTarget = 0;
	dst->trailPoints.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::MakeAccidentalShot @0x2914f0
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeAccidentalShot( SAttackRayInfo *dst, NWorld::CUnitServer *pUnit, const NAI::SUnitPosition &from,
	const CAttackPortion &atk, const CVec3 &vOrigin, const CVec3 &vDir, float fMaxRange )
{
	if ( !dst )
		return;
	float fClear = pUnit ? pUnit->GetMinClearDistance() : 0.0f;
	*dst = SAttackRayInfo( atk, vOrigin, vDir, pUnit, from, 0, 0, false, fClear, fMaxRange, 0 );
	dst->pIgnore = pUnit ? const_cast<CObjectBase*>( pUnit->GetAttackIgnore() ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::IsObstacleRay @0x290740 -- mirror of the obstacle test in AddGridToCovers.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsObstacleRay( NAI::CFastRenderer::SResult *pList, float fMaxDist, CObjectBase *pIgnore )
{
	for ( NAI::CFastRenderer::SResult *p = pList; p; p = p->pNext )
	{
		if ( p->fEnter >= fMaxDist )		// distance-sorted: nothing else is nearer
			return false;
		if ( ( p->GetInfo().nTSFlags & NWorld::TS_WEAPON_BLOCKER ) && p->GetInfo().pUserData != pIgnore )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::PerformThrowingAttackPortion @0x290780 -- free-fn form of
// CGame::ProcessThrowingAttackPortion. The release added the leading IWorld* and the
// flight direction argument, forwarded to the target's damage handler.
////////////////////////////////////////////////////////////////////////////////////////////////////
EAttackResult PerformThrowingAttackPortion( NWorld::IWorld *pWorld, CAttackPortion *pA,
	const CVec3 &vDir, IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID )
{
	if ( pArmor == NDb::GetArmor( NDb::N_HUMAN_BODY_ARMOR ) )
	{
		pTarget->ProcessAttack( pWorld, nUserID, pA, vDir, pArmor );
		return AR_BOUNCE_BODY;
	}
	if ( pArmor->pMaterial->nDR == 10 )
		return AR_IGNORE;
	if ( pArmor->pMaterial->nDR == 0 )
	{
		pTarget->ProcessAttack( pWorld, nUserID, pA, vDir, pArmor );
		return AR_IGNORE;
	}
	if ( pArmor->pMaterial->nDR == 1 || pArmor->pMaterial->nDR == 2 )
		return AR_STUCK;
	return AR_BOUNCE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::PerformMeleeAttackPortion @0x290f70 -- free-fn form of CGame::ProcessMeleeAttackPortion.
// The release added explicit IWorld* (unread) + IAIMap* + a pFilter object that, when non-null,
// restricts the ProcessAttack callback to that one hit object.
// DECOMP-vs-dev divergence (followed the release): the dev CGame version `continue`s on a
// god-moded unit; the release only withholds it from the ignore list and still runs the
// armor/damage path.
////////////////////////////////////////////////////////////////////////////////////////////////////
void PerformMeleeAttackPortion( NWorld::IWorld *pWorld, NAI::IAIMap *pAIMap, const CAttackPortion &a,
	const CRay &ray, const vector<IAttackable*> &ignores, CObjectBase *pFilter )
{
	vector<NAI::SInterval> intersect;
	vector<IAttackable*> ignore;
	for ( vector<IAttackable*>::const_iterator it = ignores.begin(); it != ignores.end(); ++it )
		ignore.push_back( *it );
	CAttackPortion tmp( a );
	tmp.rTtrajectory = ray;   // Jan03 write the combat code reads (see CGame::ProcessRangedAttackPortion note)
	pAIMap->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );

	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( !( i->enter.fT > 0 ) )
			continue;
		CObjectBase *pUD = i->pSrc->pUserData;
		CDynamicCast<IAttackable> pCatcher( i->pSrc->pUserData );
		if ( pCatcher )
		{
			if ( find( ignore.begin(), ignore.end(), pCatcher ) != ignore.end() )
				continue;
			CDynamicCast<NWorld::CUnit> pUnit( i->pSrc->pUserData );
			if ( pUnit && !pUnit->IsCheatEnabled( CHEAT_GODMODE ) )
				ignore.push_back( pCatcher );
		}
		NDb::CRPGArmor *pArmor = i->pSrc->pArmor;
		if ( !pArmor )
			pArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );
		if ( !tmp.IsArmorIgnored( pArmor ) && !tmp.CanDealDmg( pArmor ) )
			return;
		if ( ( !pFilter || pFilter == pUD ) && pCatcher && IsValid( pUD ) )
			// retail @0x290f70 passes ray.ptDir, NOT the per-hit point ray.Get(i->enter.fT).
			pCatcher->ProcessAttack( pWorld, i->nUserID, &tmp, ray.ptDir, pArmor );
		tmp.nK -= GetAPASubstraction( i->enter.fT, i->exit.fT, pArmor );
		if ( tmp.nK <= 0 )
			return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::CalcCoverIntervals @0x290d80 -- cumulative AP-left profile across the cover chain.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcCoverIntervals( NAI::CFastRenderer::SResult *pList, const SAttackRayInfo &ray,
	NDb::CRPGArmor *pFallbackArmor, vector<SCoverInterval> *pOut )
{
	float fAP = (float)ray.atk.nK;
	pOut->push_back( SCoverInterval( -1e30f, fAP ) );	// sentinel: profile before anything is hit
	vector<CObjectBase*> seen;
	seen.push_back( ray.pIgnore );
	for ( NAI::CFastRenderer::SResult *node = pList; node; node = node->pNext )
	{
		if ( node->fEnter >= ray.fMaxRange )
			break;
		if ( node->fExit < ray.fMinClearDistance )
			continue;
		const NAI::SSourceInfo &si = node->GetInfo();
		if ( !( si.nTSFlags & NWorld::TS_COVER ) )		// not a damageable body
			continue;
		CObjectBase *pObj = si.pUserData;
		if ( find( seen.begin(), seen.end(), pObj ) != seen.end() )
			continue;									// charge each unit once
		CDynamicCast<NWorld::CUnit> pUnit( si.pUserData );
		if ( pUnit || pObj == ray.pTarget.GetPtr() )
			seen.push_back( pObj );
		NDb::CRPGArmor *pArmor = si.pArmor;
		if ( !pArmor )
			pArmor = pFallbackArmor;
		float fNext;
		if ( !ray.atk.IsArmorIgnored( pArmor ) && !ray.atk.CanDealDmg( pArmor ) )
			fNext = -100.0f;							// blocked: hard stop at this cover
		else
			fNext = fAP - GetAPASubstraction( node->fEnter, node->fExit, pArmor );
		fAP = fNext;
		pOut->push_back( SCoverInterval( node->fEnter + 0.05f, fAP ) );	// interval begins just inside
		if ( fAP <= 0 )
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail CalcNormals v1.1 0x690820: the precise trace supplies surface normals ONLY.
// It must not replace the selected grid's object, body part, or damage decision.
static void CalcNormals( NAI::IAIMap *pAIMap, vector<STrailPoint> *pTrail,
	const CVec3 &vOrigin, const CVec3 &vDir )
{
	CRay ray; ray.ptOrigin = vOrigin; ray.ptDir = vDir;
	vector<NAI::SInterval> intersections;
	pAIMap->Trace( ray, &intersections, NWorld::TS_FRAGMENTED );
	for ( vector<STrailPoint>::iterator i = pTrail->begin(); i != pTrail->end(); ++i )
	{
		if ( i->nFloor == 100 )
			continue;
		bool bExit = vDir * i->vNormal <= 0;
		float fNearest = 1e38f;
		for ( vector<NAI::SInterval>::const_iterator j = intersections.begin(); j != intersections.end(); ++j )
		{
			if ( j->pSrc->pUserData != i->pObject )
				continue;
			const NAI::SInterval::SCrossPoint &point = bExit ? j->exit : j->enter;
			float fDistance = fabs2( i->vPosition - ray.Get( point.fT ) );
			if ( fDistance < fNearest )
			{
				fNearest = fDistance;
				i->vNormal = -point.ptNormal;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x691340 / v1.1 0x6915d0. The hit grid is authoritative; the
// separate precise trace above only refines entry/exit particle normals.
void GetHitIntersections( NAI::IAIMap *pAIMap, vector<STrailPoint> *pTrail,
	NAI::CFastRenderer::SResult *pList, const SAttackRayInfo &rayInfo )
{
	NDb::CRPGArmor *pDefaultArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );
	vector<SCoverInterval> cover;
	CalcCoverIntervals( pList, rayInfo, pDefaultArmor, &cover );
	CRay ray = rayInfo.GetRay();
	CAttackPortion attack( rayInfo.atk );
	attack.rTtrajectory = ray;
	vector<CObjectBase*> ignored;
	ignored.push_back( rayInfo.pIgnore );
	pTrail->clear();
	pTrail->push_back( STrailPoint( 0, ray.ptDir, ray.Get( rayInfo.fMinClearDistance ),
		attack, 0, 0, 0, CVec3(0,0,1), 100 ) );
	float fEnd = rayInfo.fMaxRange;
	for ( int i = 0; i < cover.size(); ++i )
		if ( cover[i].fK < 0 )
			fEnd = Min( fEnd, cover[i].fEnter );
	int nCover = 0;
	for ( NAI::CFastRenderer::SResult *p = pList; p; p = p->pNext )
	{
		if ( p->fEnter >= rayInfo.fMaxRange )
			break;
		const NAI::SSourceInfo &src = p->GetInfo();
		if ( p->fExit < rayInfo.fMinClearDistance || !( src.nTSFlags & NWorld::TS_FRAGMENTED ) )
			continue;
		NDb::CRPGArmor *pArmor = src.pArmor;
		if ( !pArmor )
			pArmor = pDefaultArmor;
		while ( nCover + 1 < cover.size() && p->fEnter > cover[nCover + 1].fEnter )
			++nCover;
		attack.nK = int( cover[nCover].fK );
		if ( attack.nK < 1 )
		{
			fEnd = cover[nCover].fEnter;
			break;
		}
		CObjectBase *pObject = src.pUserData;
		if ( find( ignored.begin(), ignored.end(), pObject ) != ignored.end() )
			continue;
		CDynamicCast<NWorld::CUnit> pUnit( pObject );
		if ( pUnit || pObject == rayInfo.pTarget )
			ignored.push_back( pObject );
		int nUserID = p->pSrc->nUserID;
		bool bHit = true;
		if ( IsValid( pUnit ) && pObject != rayInfo.pTarget )
		{
			CDynamicCast<NWorld::CUnitServer> pServer( pObject );
			if ( rayInfo.pUS && pServer && rayInfo.pUS->GetDiplomacyState( pServer ) == NDb::DS_ALLY )
				bHit = false;
			else
				bHit = CheckBulletToHit( rayInfo, pObject, (NAI::EHitLocation)nUserID );
		}
		if ( rayInfo.atk.IsArmorIgnored( pArmor ) )
			continue;
		bool bCanDealDmg = rayInfo.atk.CanDealDmg( pArmor );
		bool bDraw = bHit && bCanDealDmg || !pUnit;
		if ( bDraw )
			pTrail->push_back( STrailPoint( nUserID, ray.ptDir, ray.Get( p->fEnter ), attack,
				bHit && bCanDealDmg ? pObject : 0, pObject, pArmor, ray.ptDir, src.nFloor ) );
		if ( !bCanDealDmg )
		{
			fEnd = p->fEnter + 0.01f;
			break;
		}
		if ( bDraw && p->fExit > 0 && p->fExit < rayInfo.fMaxRange )
			pTrail->push_back( STrailPoint( nUserID, ray.ptDir, ray.Get( p->fExit ), attack,
				0, pObject, pArmor, -ray.ptDir, src.nFloor ) );
	}
	pTrail->push_back( STrailPoint( 0, ray.ptDir, ray.Get( fEnd ), attack, 0, 0, 0, CVec3(0,0,1), 100 ) );
	CalcNormals( pAIMap, pTrail, rayInfo.vOrigin, rayInfo.vDir );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::TraceLooseRaySegment @0x292010
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceLooseRaySegment( NAI::IAIMap *pAIMap, const SAttackRayInfo &rayInfo, vector<STrailPoint> *pTrail,
	const CVec3 &vOrigin, const CVec3 &vDir, float fRange, const CAttackPortion &attack, float fMinClearDistance )
{
	if ( !pAIMap || !pTrail )
		return;

	CRay ray; ray.ptOrigin = vOrigin; ray.ptDir = vDir;
	CAttackPortion tmpAttackPortion( attack );
	tmpAttackPortion.rTtrajectory = ray;

	vector<NAI::SInterval> intersect;
	pAIMap->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );
	vector<CObjectBase*> seen;
	seen.push_back( rayInfo.pIgnore );

	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		// Retail 1.2 0x691e60: skip the surface just left by a reflected ray,
		// while retaining intervals that straddle the clear-distance boundary.
		if ( i->exit.fT < fMinClearDistance + 0.01f )
			continue;
		if ( i->enter.fT > fRange )
			break;
		{
			CObjectBase *pUD = i->pSrc->pUserData;
			if ( find( seen.begin(), seen.end(), pUD ) != seen.end() )
				continue;
			CDynamicCast<NWorld::CUnit> pUnit( pUD );
			if ( pUnit )
				seen.push_back( pUD );

			NDb::CRPGArmor *pArmor = i->pSrc->pArmor;
			if ( !pArmor )
				pArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );

			// Retail 1.2 0x691f06 / 0x69237a: test incidence before penetration.
			// Continue from the impact with the remaining range and kinetic energy.
			float fCos = -vDir * i->enter.ptNormal;
			if ( fCos < pArmor->fRicochetMaxCos && random.GetFloat( 0, 1 ) < pArmor->fRicochetProbability )
			{
				CVec3 vImpact = ray.Get( i->enter.fT );
				CVec3 vReflected = vDir + i->enter.ptNormal * ( 2 * fCos );
				pTrail->push_back( STrailPoint( i->nUserID, vReflected, vImpact, tmpAttackPortion,
					0, pUD, pArmor, -i->enter.ptNormal, i->pSrc->nFloor ) );
				TraceLooseRaySegment( pAIMap, rayInfo, pTrail, vImpact, vReflected,
					fRange - i->enter.fT, tmpAttackPortion, Max( 0.0f, fMinClearDistance - i->enter.fT ) );
				return;
			}

			// Retail 1.2 0x691f60: never turn a rolled miss into a hit on its
			// intended target. Other units get one independent roll per traced leg.
			bool bHit = pUD != rayInfo.pTarget.GetPtr();
			if ( bHit && IsValid( pUnit ) )
				bHit = CheckBulletToHit( rayInfo, pUD, (NAI::EHitLocation)i->nUserID );
			bool bDrawExit = false;
			if ( !tmpAttackPortion.IsArmorIgnored( pArmor ) )
			{
				bDrawExit = bHit || !pUnit;
				if ( bDrawExit )
					pTrail->push_back( STrailPoint( i->nUserID, ray.ptDir, ray.Get( i->enter.fT ), tmpAttackPortion, bHit ? pUD : 0, pUD, pArmor, -i->enter.ptNormal, i->pSrc->nFloor ) );
				if ( !tmpAttackPortion.CanDealDmg( pArmor ) )
					return;
			}
			tmpAttackPortion.nK -= GetAPASubstraction( i->enter.fT, i->exit.fT, pArmor );
			if ( tmpAttackPortion.nK <= 0 )
				return;	// bullet stopped by obstacle
			if ( bDrawExit && i->exit.fT > 0 && i->exit.fT < fRange )
				pTrail->push_back( STrailPoint( i->nUserID, ray.ptDir, ray.Get( i->exit.fT ), tmpAttackPortion, 0, pUD, pArmor, -i->exit.ptNormal, i->pSrc->nFloor ) );
		}
	}
	// Only a ray that exhausted its intersections reaches its range endpoint.
	// A stopped or reflected leg must not append an endpoint along the old direction.
	pTrail->push_back( STrailPoint( 0, vDir, ray.Get( fRange ), tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::TraceLooseRay @0x292830 -- build loose fly-past tracer trail for missed shots
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceLooseRay( NAI::IAIMap *pAIMap, const SAttackRayInfo &rayInfo, vector<STrailPoint> *pTrail )
{
	if ( !pTrail || !pAIMap )
		return;

	pTrail->clear();
	CRay ray; ray.ptOrigin = rayInfo.vOrigin; ray.ptDir = rayInfo.vDir;
	CAttackPortion tmpAttackPortion( rayInfo.atk );
	tmpAttackPortion.rTtrajectory = ray;

	// Initial start point
	pTrail->push_back( STrailPoint( 0, ray.ptDir, ray.Get( rayInfo.fMinClearDistance ), tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );

	// Trace loose ray through geometry
	TraceLooseRaySegment( pAIMap, rayInfo, pTrail, rayInfo.vOrigin, rayInfo.vDir,
		rayInfo.fMaxRange, rayInfo.atk, rayInfo.fMinClearDistance );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::PerformRangedAttack @0x2929e0
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase * PerformRangedAttack( NWorld::IWorld *pWorld, const SAttackRayInfo &rayInfo, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed, NDb::CRPGGrenade *pGrenade, int nEffectType )
{
	NWorld::CWorld *pCWorld = dynamic_cast<NWorld::CWorld*>( pWorld );
	if ( !pCWorld )
		return 0;

	vector<STrailPoint> trail;
	if ( !rayInfo.bTargetIsHit )
	{
		// MISS branch: build loose fly-past tracer without dealing target damage
		TraceLooseRay( pCWorld->GetAIMap(), rayInfo, &trail );
	}
	else
	{
		// HIT branch: if trailPoints already populated, use them; otherwise process attack portion along the ray
		if ( !rayInfo.trailPoints.empty() )
		{
			trail = rayInfo.trailPoints;
		}
		else
		{
			vector<IAttackable*> ignores;
			if ( rayInfo.pIgnore )
			{
				CDynamicCast<IAttackable> pAtt( rayInfo.pIgnore.GetPtr() );
				if ( pAtt )
					ignores.push_back( pAtt );
			}
			CRay ray; ray.ptOrigin = rayInfo.vOrigin; ray.ptDir = rayInfo.vDir;
			float fMaxRange = rayInfo.fMaxRange > 0.0f ? rayInfo.fMaxRange : 30.0f;
			pCWorld->GetGame()->ProcessRangedAttackPortion( rayInfo.atk, ray, ignores, &trail, fMaxRange );
		}
	}

	// Retail v1.2 0x69271d..0x69272e suppresses impact particles for shooterless rays.
	if ( !rayInfo.pUS )
		nEffectType = -1;
	NWorld::IDynamicObject *pBulletServer = NWorld::CreateBulletServer( pCWorld, trail, sCast, pTrailModel, fTrailSpeed, pGrenade, nEffectType );
	if ( pBulletServer )
	{
		pCWorld->GetMiscObjects()->push_back( pBulletServer );
		return pBulletServer;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
