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
	// The release sets bFirstTurn from the shooter's "is it the first turn" status. The dev tree
	// has no direct CUnit/CUnitServer first-turn accessor (IsFirstTurn lives on NWorld::IWorld /
	// CTBSWorld, which this ctor does not have), so bFirstTurn is left false here. Behaviour-neutral:
	// SAttackRayInfo is not wired into any call path yet.
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
// (unread) vPlace argument; the body is otherwise identical.
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
		if ( ray.atk.IsArmorIgnored( pArmor ) || !ray.atk.CanDealDmg( pArmor ) )
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
// NRPG::TraceLooseRaySegment @0x292010
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceLooseRaySegment( NAI::IAIMap *pAIMap, const SAttackRayInfo &rayInfo, vector<STrailPoint> *pTrail, const CVec3 &vOrigin, const CVec3 &vDir, float fRange )
{
	if ( !pAIMap || !pTrail )
		return;

	CRay ray; ray.ptOrigin = vOrigin; ray.ptDir = vDir;
	CAttackPortion tmpAttackPortion( rayInfo.atk );
	tmpAttackPortion.rTtrajectory = ray;

	vector<NAI::SInterval> intersect;
	pAIMap->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );

	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( i->enter.fT > 0 && i->enter.fT < fRange )
		{
			CObjectBase *pUD = i->pSrc->pUserData;
			if ( pUD && pUD == rayInfo.pIgnore.GetPtr() )
				continue;

			NDb::CRPGArmor *pArmor = i->pSrc->pArmor;
			if ( !pArmor )
				pArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );

			bool bDrawExit = false;
			if ( !tmpAttackPortion.IsArmorIgnored( pArmor ) )
			{
				bDrawExit = true;
				// pAttackTarget is NULL so no intended target damage is resolved for loose ray
				pTrail->push_back( STrailPoint( i->nUserID, ray.ptDir, ray.Get( i->enter.fT ), tmpAttackPortion, 0, pUD, pArmor, -i->enter.ptNormal, i->pSrc->nFloor ) );
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
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::TraceLooseRay @0x292830 -- build loose fly-past tracer trail for missed shots
////////////////////////////////////////////////////////////////////////////////////////////////////
void TraceLooseRay( NAI::IAIMap *pAIMap, const SAttackRayInfo &rayInfo, vector<STrailPoint> *pTrail )
{
	if ( !pTrail || !pAIMap )
		return;

	pTrail->clear();
	float fRange = rayInfo.fMaxRange > 0.0f ? rayInfo.fMaxRange : 30.0f;
	CRay ray; ray.ptOrigin = rayInfo.vOrigin; ray.ptDir = rayInfo.vDir;
	CAttackPortion tmpAttackPortion( rayInfo.atk );
	tmpAttackPortion.rTtrajectory = ray;

	// Initial start point
	pTrail->push_back( STrailPoint( 0, ray.ptDir, ray.ptOrigin, tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );

	// Trace loose ray through geometry
	TraceLooseRaySegment( pAIMap, rayInfo, pTrail, rayInfo.vOrigin, rayInfo.vDir, fRange );

	// Terminal end point
	pTrail->push_back( STrailPoint( 0, ray.ptDir, ray.Get( fRange ), tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::PerformRangedAttack @0x2929e0
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase * PerformRangedAttack( NWorld::IWorld *pWorld, const SAttackRayInfo &rayInfo, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed, NDb::CRPGGrenade *pGrenade, int nFloor )
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

	NWorld::IDynamicObject *pBulletServer = NWorld::CreateBulletServer( pCWorld, trail, sCast, pTrailModel, fTrailSpeed );
	if ( pBulletServer )
	{
		pCWorld->GetMiscObjects()->push_back( pBulletServer );
		return pBulletServer;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
