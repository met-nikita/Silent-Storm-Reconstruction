#include "StdAfx.h"
#include "RPGGame.h"
#include "aiMap.h"
#include "aiRender.h"
#include "..\Misc\RandomGen.h"
#include "wTSFlags.h"
#include "wInterface.h"
#include "RPGUnitMission.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"   // NDb::EDiplomacyState / DS_ALLY -- friendly-fire ally check in cover penetration
#include "..\MiscDll\LogStream.h"
#include "aiGrid.h"
#include "RPGVision.h"
#include "rpgCheatConstants.h"
#include "rpgUnit.h"
#include "wUnitServer.h"   // NWorld::CUnitServer::GetDiplomacyState -- friendly-fire ally block in cover penetration
#include "wObject.h"       // NWorld::CCannon::GetCurrentUnit -- resolve a cannon/mech shooter for the ally block

// NWorld::CanMeleeAttack @0x3a1db0 (wUnitAttackExec.cpp) -- melee-swing reach gate in the composite
// tile to-hit. Prototyped here instead of including wUnitAttackExec.h (that header needs the full
// executor type set this TU does not pull in).
namespace NWorld
{
	bool CanMeleeAttack( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget );
}

//#define OUTPUT_GRID_TO_FILE

const float F_TILE_HALF_VIEW_BOUND = 1.25f;
const int N_TILE_LOW_HALF_GRID = 10;
const int N_TILE_HIT_HALF_GRID = 2;
static const int N_TILE_DELTA_MINUS = N_TILE_LOW_HALF_GRID - N_TILE_HIT_HALF_GRID - 1;
static const int N_TILE_DELTA_PLUS = N_TILE_LOW_HALF_GRID + N_TILE_HIT_HALF_GRID;


const float FP_TAN_PI8 = tan( FP_PI8 );

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_WEAPONTRAIL_MAXDISTANCE = 30;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCoverInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCoverInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS( CCoverInfo );
public:
	struct SRay
	{
		CVec3 ptDir;		// direction of deflection
		bool  isPenetrate;
		float fDeviation;	// magnitude of deviation from the ideal hit
	};
	ZDATA
	vector<SRay> hitRays;	// set of rays that hit
	CVec3 src;				// point from which the shot is fired
	vector<SRay> looseRays;	// set of rays that missed
	vector<SRay> obstRays; // set of rays that ran into an obstacle at a certain distance from the source point
	float fSummAPA;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&hitRays); f.Add(3,&src); f.Add(4,&looseRays); f.Add(5,&obstRays); f.Add(6,&fSummAPA); return 0; }
};
static bool operator < ( const CCoverInfo::SRay &right, const CCoverInfo::SRay &left ) { return right.fDeviation < left.fDeviation; }
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanShoot( CCoverInfo *pCover )
{
	return !pCover->hitRays.empty() || !pCover->looseRays.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGame
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGame: public IGame
{
	OBJECT_BASIC_METHODS( CGame );
	ZDATA
	CPtr<NAI::IAIMap> pAIMap;
	CObj<IVisionTracker> pVision;
	CPtr<NAI::IPathNetwork> pNet;
		bool bNight = false;
	int nMaxCriticalSeverity = 300;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIMap); f.Add(3,&pVision); f.Add(4,&pNet); f.Add(5,&bNight); f.Add(6,&nMaxCriticalSeverity); return 0; }

	bool IsVisible( const CVec3 &vFrom, const vector<CVec3> &testPoints );
public:
	CGame() {}
	CGame( NAI::IAIMap *_pAIMap, NAI::IPathNetwork *_pNet ): pAIMap(_pAIMap), pNet(_pNet) { if ( _pAIMap ) pVision = CreateVisionTracker( _pAIMap ); }
	virtual CCoverInfo* CalcCovers( const CVec3 &src, const CAttackPortion &attack, 
		NWorld::CUnit *pIgnore, NWorld::CUnit *pDest, int nTargetUserID, float fMinClearDistance, bool bAIMode = false );
	virtual CCoverInfo* CalcCoversForTile( const CVec3 &src, const CAttackPortion &attack, NWorld::CUnit *pIgnore,
		const CVec3 &ptTarget, float fMinClearDistance );
	virtual void ProcessMeleeAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores );
	virtual void ProcessRangedAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores, vector<STrailPoint> *pTrail, float fMaxRange );
	virtual EAttackResult ProcessThrowingAttackPortion( CAttackPortion *pA, IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID );
	virtual int GetCompositeToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, NAI::EHitLocation eHL, bool bFirstTurn );
	virtual int GetGrenadeCompositeToHit( NWorld::CUnit *pAttacker, 
		CVec3 ptTarget, bool bFirstTurn, NDb::CRPGGrenade *pGrenade );
	virtual int GetTileCompositeToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos, 
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn );
	virtual int GetBazookaToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos, 
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn );
	virtual bool CheckVisibility( const NWorld::CUnit *pObserver, const NWorld::CUnit *pDest, bool bUseFOV );   // retail @0x298cb0
	virtual bool CanSee( const NWorld::CUnit *pObserver, const CVec3 &vPos );
	virtual bool CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos );
	virtual bool IsCorpseVisible( const NWorld::CUnit *pObserver, const NWorld::CUnit *pCorpse );   // retail @0x298da0
	virtual bool CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos,
		float fRange, float fFOVAngle );   // retail @0x298fa0 (4-arg)
	virtual float GetUnitSightDistance( NRPG::CUnit *pRPGUnit );   // retail @0x2984d0
	virtual float GetMaxUnitSightDistance( NRPG::CUnit *pRPGUnit );   // retail @0x298520 (vtbl+0x38)
	virtual void GetVisibilityArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pObserver );
	virtual void GetVisibleFromArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pTarget, CVec3 &vNear, float fRadius, int nPoses );
	virtual int GetCoverForAIUnit( CVec3 ptFrom, NWorld::CUnit *pIgnore, 
		NWorld::CUnit *pTarget, const NRPG::CAttackPortion &AttackPortion, NAI::EHitLocation HitLocation );

	virtual CVec3 GetIllumination( const vector<CVec3> &unit ) { return CVec3(1,1,1); }
	virtual IVisionTracker* GetVisionTracker() { return pVision; }
	virtual bool UpdateVision( float fTime ) { return pVision->UpdateVision( fTime ); }   // retail @0x2995a0
	virtual int GetMaxCriticalSeverity() const { return nMaxCriticalSeverity; }
	virtual void SetMaxCriticalSeverity( int n ) { if ( n < 1 ) n = 300; nMaxCriticalSeverity = n; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddGridToCovers( CCoverInfo *pRes, const NAI::CFastRenderer &res, const CVec3 &vTargetDir,
	const CObjectBase *pIgnore, const CObjectBase *pTarget, int nTargetUserID, 
	float fDistance, const CAttackPortion &att, float fMinClearDistance )
{
	float _fArmorPiercingAbility = att.nK;
	NDb::CRPGArmor *pDefaultArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );
	// retail threads the SHOOTER into the penetration walk (CanHitTarget) for the friendly-fire ally check.
	// pIgnore is the attacker's GetAttackIgnore() -> the firing unit for infantry, but the mounted CCannon for a
	// unit on a cannon/Panzerklein (CUnitServer::GetAttackIgnore returns animator.GetCannon()). The plain cast is
	// therefore null for cannon/mech shooters, so resolve the cannon's operator and use it -- otherwise a mech
	// would shoot THROUGH its own ally (the ally block below would be skipped on the null pAttackerSrv).
	NWorld::CUnitServer *pAttackerSrv = CDynamicCast<NWorld::CUnitServer>( (CObjectBase*)pIgnore );
	if ( !pAttackerSrv )
	{
		CDynamicCast<NWorld::CCannon> pCannon( (CObjectBase*)pIgnore );
		if ( pCannon )
			pAttackerSrv = CDynamicCast<NWorld::CUnitServer>( pCannon->GetCurrentUnit() );
	}
#ifdef OUTPUT_GRID_TO_FILE
	FILE *f;
	if ( res.resGrid.GetXSize() > 10 )
		f = fopen( "cover_l.txt", "wt" );
	else
		f = fopen( "cover_s.txt", "wt" );
	fprintf( f, "Legend:\n# - hit\n. - too close to shoot\n? - not penetrate\n" );
	for ( int i = 0; i < res.resGrid.GetXSize(); ++i )
		fputc( '-', f );
	fputc( '\n', f );
	for ( int y = res.resGrid.GetYSize() - 1; y >= 0; --y )
#else
	for ( int y = 0; y < res.resGrid.GetYSize(); ++y )
#endif
	{
		for ( int x = 0; x < res.resGrid.GetXSize(); ++x )
		{
			CVec3 ptRayDir;
			res.GetDir( &ptRayDir, x, y );
			CCoverInfo::SRay ray;
			ray.isPenetrate = false;
			ray.ptDir = ptRayDir;
			float fRayProjection = vTargetDir * ptRayDir;
			ray.fDeviation = sqr( fRayProjection );
// Obstacle rays
			bool bObstacle = false;
			for ( NAI::CFastRenderer::SResult *p = res.resGrid[y][x]; p; p = p->pNext )
			{
				if ( p->fEnter >= fMinClearDistance )
					break;
				else if ( ( p->GetInfo().nTSFlags & NWorld::TS_WEAPON_BLOCKER ) && p->GetInfo().pUserData != pIgnore )
				{
					bObstacle = true;
					break;
				}
			}
			if ( bObstacle )
			{
				pRes->obstRays.push_back(ray);
#ifdef OUTPUT_GRID_TO_FILE
				fputc( '.', f );
#endif
				continue;
			}
// Hit target rays
			bool bHitTarget = false;
			if ( pTarget ) // shoot unit
			{
				for ( NAI::CFastRenderer::SResult *p = res.resGrid[y][x]; p; p = p->pNext )
					if ( p->GetInfo().pUserData == pTarget &&
						( nTargetUserID == -1 || p->pSrc->nUserID == nTargetUserID ) )
					{
						bHitTarget = true;
						break;
					}
			}
			else
			{
				if ( x > N_TILE_DELTA_MINUS && x < N_TILE_DELTA_PLUS &&
					y > N_TILE_DELTA_MINUS && y < N_TILE_DELTA_PLUS )
						bHitTarget = true;
			}
			//
			if ( !bHitTarget )
			{
				// missed rays
				pRes->looseRays.push_back(ray);
#ifdef OUTPUT_GRID_TO_FILE
				fputc( ' ', f );
#endif
				continue;
			}

			// retail NRPG::CanHitTarget @0x290b70, inlined: the walk is penetrate-by-DEFAULT — it breaks
			// UNCONDITIONALLY once fEnter reaches the target range (`if (!(fEnter < fMaxRange)) break;`
			// — tiles too; CalcCoversForTile's fDistance pullback keeps the target tile's own ground
			// surface past fTestDistance so it can never count as its own cover), and only an explicit
			// block clears the penetrate: CanDealDmg-false cover, an ALLY of the shooter in the line,
			// or armour-piercing exhaustion. The previous dev walk (a) gated hulls on TS_UNITS where
			// retail tests 0x200 == TS_COVER, so terrain/wall nodes were skipped before the tile-
			// penetrate branch could run — every bare-ground ray ended not-penetrating -> GetHitCover's
			// -1 blocked sentinel -> 0% for all ground aiming; and (b) required an in-loop condition to
			// MARK tile penetration instead of retail's default-open shape.
			float fTempPiercing = _fArmorPiercingAbility;
			float fTestDistance = fDistance / fRayProjection;
			bool bBlocked = false, bTargetReached = false;
			vector<const CObjectBase*> ignore;
			ignore.push_back( pIgnore );
			for ( const NAI::CFastRenderer::SResult *p = res.resGrid[y][x]; p; p = p->pNext )
			{
				if ( p->fEnter >= fTestDistance )
					break;
				if ( p->fExit < fMinClearDistance )		// retail @0x290b70 (was fExit < 0)
					continue;
				if ( ( p->GetInfo().nTSFlags & NWorld::TS_COVER ) == 0 )	// retail 0x200 == TS_COVER, NOT TS_UNITS
					continue;
				if ( find( ignore.begin(), ignore.end(), p->GetInfo().pUserData ) != ignore.end() )
					continue;
				CDynamicCast<NWorld::CUnit> pHitUnit( p->GetInfo().pUserData );
				if ( pHitUnit || p->GetInfo().pUserData == pTarget )   // retail also seeds the target into ignore (@0x290c5e)
					ignore.push_back( p->GetInfo().pUserData );
				//
				NDb::CRPGArmor *pArmor = p->GetInfo().pArmor;
				if ( !pArmor )
					pArmor = pDefaultArmor;
				if ( !att.IsArmorIgnored( pArmor ) )
				{
					// retail checks the TARGET hit FIRST (@0x290c98): reaching the target is an UNCONDITIONAL
					// penetrate; CanDealDmg only vetoes intervening NON-target cover.
					if ( pTarget && p->GetInfo().pUserData == pTarget )
					{
						bTargetReached = true;
						break;
					}
					if ( !att.CanDealDmg( pArmor ) )
					{
						bBlocked = true;
						break;
					}
					// retail CanHitTarget @0x290cce: never penetrate THROUGH a unit that is an ALLY of the shooter
					// (friendly fire). Enemies/neutrals pass with armour-piercing attrition; allies hard-stop the
					// ray (diplomacy DS_ALLY). pAttackerSrv is resolved above even for cannon/mech shooters.
					if ( pAttackerSrv )
					{
						CDynamicCast<NWorld::CUnitServer> pHitSrv( p->GetInfo().pUserData );
						if ( pHitSrv && pAttackerSrv->GetDiplomacyState( pHitSrv ) == NDb::DS_ALLY )
						{
							bBlocked = true;
							break;
						}
					}
				}
				//
				fTempPiercing -= GetAPASubstraction( p->fEnter, p->fExit, pArmor );
				// the bullet got stuck, no need to damage anything further
				if ( fTempPiercing <= 0 )
				{
					bBlocked = true;
					break;
				}
			}
			if ( bTargetReached || !bBlocked )		// retail: CanHitTarget true && bCanHit
			{
				pRes->fSummAPA += fTempPiercing;
				ray.isPenetrate = true;
			}
			pRes->hitRays.push_back( ray );
#ifdef OUTPUT_GRID_TO_FILE
			fputc( ray.isPenetrate ? '#' : '?', f );
#endif
		}
#ifdef OUTPUT_GRID_TO_FILE
		fputc( '\n', f );
#endif
	}
#ifdef OUTPUT_GRID_TO_FILE
	for ( int i = 0; i < res.resGrid.GetXSize(); ++i )
		fputc( '-', f );
	fputc( '\n', f );
	fclose(f);
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGame::GetCoverForAIUnit( CVec3 ptFrom, NWorld::CUnit *pIgnore, 
	NWorld::CUnit *pTarget, const NRPG::CAttackPortion &AttackPortion, NAI::EHitLocation HitLocation )
{
	int nHitCover = 0;
	CObj<NRPG::CCoverInfo> pCover = CalcCovers( ptFrom, 
		AttackPortion, pIgnore, pTarget, HitLocation, 1.f, true );
	//
	int nPenetrateCount = 0;
	for ( vector<NRPG::CCoverInfo::SRay>::const_iterator i = pCover->hitRays.begin(); i != pCover->hitRays.end(); ++i )
		if ( i->isPenetrate )
			++nPenetrateCount;
	float fAverageAPA = ( pCover->fSummAPA / float(nPenetrateCount) ) / AttackPortion.nK;
	if ( !pCover->hitRays.empty() )
	{
		nHitCover = (100 * nPenetrateCount) / pCover->hitRays.size();
		nHitCover = int( nHitCover * fAverageAPA );
	}
	return nHitCover;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// nTargetUserID == -1 means without hit location targeting
CCoverInfo* CGame::CalcCovers( const CVec3 &src, const CAttackPortion &attack, NWorld::CUnit *pIgnore,
	NWorld::CUnit *pDest, int nTargetUserID, float fMinClearDistance, bool bAIMode )
{
	const float F_VIEW_BOUND = 2.f;			// grid diameter
	const int N_LOW_HALF_GRID = 5;
	int N_HALF_GRID;
	if ( !bAIMode )
		N_HALF_GRID = 20;
	else
		N_HALF_GRID = 8;
	// trace some rays and calc covers
	CCoverInfo *pRes = new CCoverInfo;
	pRes->fSummAPA = 0;
	CVec3 ptTarget;
	pAIMap->GetUnitHLPos( &ptTarget, pAIMap->GetHull(pDest), nTargetUserID );//pDest->GetPosition().GetCenter();
	CVec3 ptFrom = src;
	CVec3 vTargetDir = ptTarget - ptFrom;
	float fXYDistance = fabs( vTargetDir.x, vTargetDir.y );
	float fDistance = fabs( vTargetDir ) + 1; // +1 to make sure whole target is considered
	Normalize( &vTargetDir );

	float fSquareLimit = fXYDistance * FP_TAN_PI8;
	CVec2 viewSquare;
	viewSquare.x = Min( F_VIEW_BOUND * 0.5f, fSquareLimit );
	viewSquare.y = F_VIEW_BOUND * 0.5f;
	NAI::CFastRenderer res, resLow;
	res.InitProjective( src, ptTarget, viewSquare, N_HALF_GRID );
	pAIMap->TraceGrid( &res, NWorld::TS_COVER, NAI::IAIMap::STH_SORT_INTERVALS );
	AddGridToCovers( pRes, res, vTargetDir, pIgnore->GetAttackIgnore(), CastToObjectBase(pDest), nTargetUserID, fDistance, attack, fMinClearDistance );
	// add low res grid
	if ( !bAIMode )
	{
		viewSquare.x = fSquareLimit;
		viewSquare.y = Max( F_VIEW_BOUND * 0.5f, fSquareLimit );
		resLow.InitProjective( src, ptTarget, viewSquare, N_LOW_HALF_GRID );
		pAIMap->TraceGrid( &resLow, NWorld::TS_COVER, NAI::IAIMap::STH_SORT_INTERVALS );
		AddGridToCovers( pRes, resLow, vTargetDir, pIgnore->GetAttackIgnore(), CastToObjectBase(pDest), nTargetUserID, fDistance, attack, fMinClearDistance );
	}
	pRes->src = src;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCoverInfo* CGame::CalcCoversForTile( const CVec3 &src, const CAttackPortion &attack, NWorld::CUnit *pIgnore,
	const CVec3 &ptTarget, float fMinClearDistance )
{
	// trace some rays and calc covers
	float fDelta = N_TILE_HIT_HALF_GRID * F_TILE_HALF_VIEW_BOUND / N_TILE_LOW_HALF_GRID;
	CCoverInfo *pRes = new CCoverInfo;
	pRes->fSummAPA = 0;
	CVec3 ptFrom = src;
	CVec3 vTargetDir = ptTarget - ptFrom;
	float fDistance = fabs( vTargetDir ) - fDelta;
	Normalize( &vTargetDir );
	CVec3 ptRealTarget = ptTarget - vTargetDir * fDelta;
	CVec2 viewSquare;
	NAI::CFastRenderer resLow;
	viewSquare.x = F_TILE_HALF_VIEW_BOUND;
	viewSquare.y = F_TILE_HALF_VIEW_BOUND;
	resLow.InitProjective( src, ptRealTarget, viewSquare, N_TILE_LOW_HALF_GRID );
	pAIMap->TraceGrid( &resLow, NWorld::TS_COVER, NAI::IAIMap::STH_SORT_INTERVALS );
	AddGridToCovers( pRes, resLow, vTargetDir, pIgnore->GetAttackIgnore(), 0, -1, fDistance, attack, fMinClearDistance );
	pRes->src = src;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGame::ProcessMeleeAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &_ignore )
{
	// every unit/object on path receives damage
	vector<NAI::SInterval> intersect;
	vector<IAttackable*> ignore;
	for ( vector<IAttackable*>::const_iterator it = _ignore.begin(); it != _ignore.end(); ++it )
		ignore.push_back( *it );
	CAttackPortion tmpAttackPortion( a );
	tmpAttackPortion.rTtrajectory = ray;   // Jan03 write the combat code reads (see ProcessRangedAttackPortion note)
	pAIMap->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );

	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( i->enter.fT > 0 )	// because this is actually not a ray but a straight line
		{
			CDynamicCast<IAttackable> pAttackCatcher( i->pSrc->pUserData );
			if ( pAttackCatcher )
			{
				if ( find( ignore.begin(), ignore.end(), pAttackCatcher ) != ignore.end() )
					continue;
				CDynamicCast<NWorld::CUnit> pTargetUnit( i->pSrc->pUserData );
				if ( IsValid( pTargetUnit ) )
				{
					// Let's try playing supermen. "Cheaters are cheaters even in Africa ;)"
					if ( pTargetUnit->IsCheatEnabled( CHEAT_GODMODE ) )
						continue;
					ignore.push_back(pAttackCatcher);
				}
			}
			NDb::CRPGArmor *pArmor = i->pSrc->pArmor;
			if ( !pArmor )
				pArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );
			if ( !tmpAttackPortion.IsArmorIgnored(pArmor) && !tmpAttackPortion.CanDealDmg(pArmor) )
				return;
			// tally up the damage inflicted
			if ( pAttackCatcher && IsValid( i->pSrc->pUserData ) )
				// dead Jan03 ancestor (retail has only the free PerformMeleeAttackPortion @0x290f70, and
			// nothing here calls this): no IWorld in scope -> 0 skips the difficulty multiplier.
			pAttackCatcher->ProcessAttack( 0, i->nUserID, &tmpAttackPortion, ray.ptDir, pArmor );
			tmpAttackPortion.nK -= GetAPASubstraction( i->enter.fT, i->exit.fT, pArmor );
			//sTrail.explosions.push_back( SWound( i->pUserData, ray.Get( i->enter.fT ), -ray.ptDir, pArmor ) );
			if ( tmpAttackPortion.nK <= 0 )
				return;	// the bullet got stuck, no need to damage anything further
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGame::ProcessRangedAttackPortion( const CAttackPortion &a, const CRay &ray, const vector<IAttackable*> &ignores, vector<STrailPoint> *pTrails, float fMaxRange )
{
	// every unit/object on path receives damage
	vector<NAI::SInterval> intersect;
	vector<IAttackable*> ignore;
	for ( vector<IAttackable*>::const_iterator it = ignores.begin(); it != ignores.end(); ++it )
		ignore.push_back( *it );
	CAttackPortion tmpAttackPortion( a );
	// Jan03 wrote the fired ray into the attack portion here; the combat code (ProcessAttack corpse-push,
	// KillUnit, CreateBloodyMess) reads pAttack->rTtrajectory.ptDir for the shot direction. Retail moved
	// that direction to a separate `dir` argument of ProcessAttack @0x350e20 (s2_dumbunit.h:2320) and
	// dropped this write; the dev tree kept the Jan03 rTtrajectory READER but the reconstructed-from-retail
	// body lost the WRITER, so every downed unit was pushed/killed with a ZERO direction (no ragdoll launch,
	// no directional blood). Restore the Jan03 write so the trail-carried attack portion keeps the direction.
	tmpAttackPortion.rTtrajectory = ray;
	pAIMap->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );

	pTrails->clear();
	pTrails->push_back( STrailPoint( 0, ray.ptDir, ray.ptOrigin, tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );

	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( i->enter.fT > 0 && i->enter.fT < fMaxRange )	// because this is actually not a ray but a straight line
		{
			CDynamicCast<IAttackable> pAttackCatcher( i->pSrc->pUserData );
			if ( pAttackCatcher )
			{
				if ( find( ignore.begin(), ignore.end(), pAttackCatcher ) != ignore.end() )
					continue;
				CDynamicCast<NWorld::CUnit> pTargetUnit( i->pSrc->pUserData );
				if ( IsValid( pTargetUnit ) )
				{
					// Let's try playing supermen. "Cheaters are cheaters even in Africa ;)"
					if ( pTargetUnit->IsCheatEnabled( CHEAT_GODMODE ) )
						continue;
					ignore.push_back(pAttackCatcher);
				}
			}
			NDb::CRPGArmor *pArmor = i->pSrc->pArmor;
			if ( !pArmor )
				pArmor = NDb::GetArmor( NDb::N_DEFAULT_ARMOR );
			bool bDrawExit = false;
			if ( !tmpAttackPortion.IsArmorIgnored(pArmor) )
			{
				bDrawExit = true;
				pTrails->push_back( STrailPoint( i->nUserID, ray.ptDir, ray.Get( i->enter.fT ), tmpAttackPortion, i->pSrc->pUserData, i->pSrc->pUserData, pArmor, -i->enter.ptNormal, i->pSrc->nFloor ) );
				if ( !tmpAttackPortion.CanDealDmg(pArmor) )
					return;
			}
			tmpAttackPortion.nK -= GetAPASubstraction( i->enter.fT, i->exit.fT, pArmor );
			if ( tmpAttackPortion.nK <= 0 )
				return;	// the bullet got stuck, no need to damage anything further
			if ( bDrawExit && i->exit.fT > 0 && i->exit.fT < fMaxRange )
				pTrails->push_back( STrailPoint( i->nUserID, ray.ptDir, ray.Get( i->exit.fT ), tmpAttackPortion, 0, i->pSrc->pUserData, pArmor, -i->exit.ptNormal, i->pSrc->nFloor ) );
		}
	}
	pTrails->push_back( STrailPoint( 0, ray.ptDir, ray.Get( fMaxRange ), tmpAttackPortion, 0, 0, 0, CVec3(0,0,1), 100 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EAttackResult CGame::ProcessThrowingAttackPortion( CAttackPortion *pA, IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID )
{
	if ( pArmor == NDb::GetArmor( NDb::N_HUMAN_BODY_ARMOR ) )
	{
		pTarget->ProcessAttack( 0, nUserID, pA, pA->rTtrajectory.ptDir, pArmor );
		return AR_BOUNCE_BODY;
	}
	// Foliage
	if ( pArmor->pMaterial->nDR == 10 )
		return AR_IGNORE;
	// Glass
	if ( pArmor->pMaterial->nDR == 0 )
	{
		pTarget->ProcessAttack( 0, nUserID, pA, pA->rTtrajectory.ptDir, pArmor );
		return AR_IGNORE;
	}
	// Wood
	if ( pArmor->pMaterial->nDR == 1 || pArmor->pMaterial->nDR == 2 )
		return AR_STUCK;
	return AR_BOUNCE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static SRand rand;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool PeekRayForRocket( CCoverInfo *pCover, CRay *pRes, bool bHit )
{
	float fHitFlag;
	if ( ( bHit && !pCover->hitRays.empty() ) || pCover->looseRays.empty() )
		fHitFlag = 2.f;
	else
		fHitFlag = 0.f;
	bool bIsMiss;
	return PeekRay( pCover, pRes, fHitFlag, &bIsMiss );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool PeekRay( CCoverInfo *pCover, CRay *pRes, float fHit, bool *bIsMiss, bool bStickTo_pRes )
{
	*bIsMiss = true;
	pRes->ptOrigin = pCover->src;
	float fMaxAngle = cos(ToRadian(8.f));
	if ( fHit > 1 )
	{
		ASSERT( pCover->hitRays.size() > 0 );	// super strange: cover is only passed here once at least 20 rays have been collected
		if ( pCover->hitRays.empty() )
		{
			// translated dev diagnostic (the original Russian text was U+FFFD-corrupted in the dev copy).
			// RETAIL-AUTHENTIC: NRPG::RealPeekRay @0x2b3f00 prints the same (Russian) line under the
			// identical condition -- a guaranteed-hit ray requested while hitRays is empty (target fully
			// obstructed) -- and returns false. The print is benign; the real bug was upstream: the
			// ToHit calcer gate had `< 0` where retail has `<= 0` (see RPGToHit.cpp GetToHit overloads),
			// so a fully-blocked target still showed a non-zero % and the shot could be ordered at all.
			csRPG << "Kick the programmers!!! Do it right now!!!" << endl;
			return false;
		}

		CRoulette roulette;
		for ( vector<CCoverInfo::SRay>::const_iterator i = pCover->hitRays.begin(); i != pCover->hitRays.end(); ++i )
		{
			float fK = i->fDeviation;
			if ( bStickTo_pRes )
			{
				fK = pRes->ptDir * i->ptDir;
				fK *= fK * fK;
				if ( fK < fMaxAngle )
					fK /= 1000;
			}
			// RealPeekRay @0x2b3f00: on a rolled hit, only PENETRATING rays are eligible. Zero the weight of any
			// obstacle-blocked (non-penetrating) ray so the roulette can never pick it as the fired trajectory
			// while *bIsMiss (which drives the shooter's hit/miss bark) reports a hit -- the reported bug where a
			// bullet flew into cover yet the shooter barked a hit. Retail does NOT scale the penetrating weight by fHit.
			if ( !i->isPenetrate )
				fK = 0;
			else
				*bIsMiss = false;
			roulette.AddSector(fK);
		}
		if ( *bIsMiss )
			csRPG << CC_RED << "\tCan't hit target!\n";

		pRes->ptDir = pCover->hitRays[roulette.GetRandomSector( &rand )].ptDir;
	}
	else
	{
		ASSERT( pCover->looseRays.size() > 0 );	// same as above
		if ( pCover->looseRays.empty() )
			return false;

		CRoulette roulette;
		sort( pCover->looseRays.begin(), pCover->looseRays.end() ); // sort from worst to best
		float fStep = 1.f / float( pCover->looseRays.size() );
		int n = 0;
		for ( vector<CCoverInfo::SRay>::const_iterator i = pCover->looseRays.begin(); i != pCover->looseRays.end(); ++i, ++n )
		{
			float fK = sqr( fHit - fabs( float(n) * fStep - fHit ) * i->fDeviation );
			if ( bStickTo_pRes )
			{
				fK = pRes->ptDir * i->ptDir;
				fK *= fK * fK;
				if ( fK < fMaxAngle )
					fK /= 1000;
			}
			roulette.AddSector(fK);
		}
		int nPick = roulette.GetRandomSector( &rand );
//		csRPG << "\tMiss index " << nPick << ", average = " << int( float(n) * fHit ) << " from " << n << "\n";
		pRes->ptDir = pCover->looseRays[nPick].ptDir;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetHitCover( const NWorld::CUnit *pAttacker, CCoverInfo *pCover )
{
	ASSERT(pCover);
	//
	int nPenetrateCount = 0;
	for ( vector<CCoverInfo::SRay>::const_iterator i = pCover->hitRays.begin(); i != pCover->hitRays.end(); ++i )
		if ( i->isPenetrate )
			++nPenetrateCount;
	//
	CDynamicCast<NRPG::IUnitMission> pRealAttacker( pAttacker->GetRPG() );
	vector<NRPG::CAttackPortion> attack;
	pRealAttacker->CreateAttack( &attack, false );
	if ( attack.empty() )
		return 0;
	// Rays were cast but NONE penetrate: the target/tile is fully walled off. Report the BLOCKED
	// sentinel. NOTE (2026-07-05): the retail calcer gate @0x2b85e0/0x2b86c0/0x2b8a50 is
	// `fHitCover <= 0 -> 0%` (fcomp vs the 0.0f global @0x8d54dc, disasm-verified), so retail's
	// plain 0.0 return covers this case AND the empty-hitRays case (all rays muzzle-obstructed --
	// the RealPeekRay @0x2b3f00 "Kick the programmers" scenario, where we fall through below and
	// return 0). The -1 sentinel is kept as a stronger-typed BLOCKED marker; both -1 and 0 now
	// zero the displayed chance, matching retail. The old unguarded 0/0-NaN ->
	// int(0*NaN)=INT_MIN accidentally produced this for blocked tiles while ALSO poisoning open
	// ground; the plain 0.0f guard alone lost the blocked case (good % through two walls).
	if ( !pCover->hitRays.empty() && nPenetrateCount == 0 )
		return -1;
	// retail GetHitCover @0x2b4390 (disasm 0x6b4467): the average-APA division runs ONLY when
	// nPenetrateCount > 0, else it defaults to 0.0f.
	float fAverageAPA = 0.0f;
	if ( nPenetrateCount > 0 )
		fAverageAPA = ( pCover->fSummAPA / float(nPenetrateCount) ) / attack.front().nK;
	// percentage of rays that hit
	int nHitCover = 0;
	if ( !pCover->hitRays.empty() )
	{
		nHitCover = (100 * nPenetrateCount) / pCover->hitRays.size();
		nHitCover = int( float(nHitCover) * fAverageAPA );
	}
	return nHitCover;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2b52e0/@0x2b5090: both take the burst bullet index and thread it into the calcers
// (the Jan03 mission-side bullet cursor is gone).
int GetAttackerTileToHit( const NWorld::CUnit *pAttacker, const CVec3 ptTarget, int nExtraAP,
	NAI::ETileHitLocation eHitLocation, CCoverInfo *pCover, bool bFirstRound, int nBullet )
{
	int nHitCover = GetHitCover( pAttacker, pCover );
	int nDistance = fabs(pAttacker->GetPosition().GetCP() - ptTarget ) / FP_GRID_STEP;
	CVec3 ptAttacker = pAttacker->GetPosition().GetCenter();
	//
	return NRPG::GetTileToHit( pAttacker, pAttacker->GetPose(), nDistance, ptAttacker,
		ptTarget, eHitLocation, nExtraAP, nHitCover, bFirstRound, CVec3(1,1,1), nBullet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetAttackerToHit( const NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, int nExtraAP,
	NAI::EHitLocation eHL, const vector<int> &accessibleHLs, CCoverInfo *pCover, bool bFirstRound, int nBullet )
{
	ASSERT(pCover);
	if ( !IsValid( pTarget ) )
		return 0;
	//
	int nHitCover = GetHitCover( pAttacker, pCover );
	int nDistance = fabs(pAttacker->GetPosition().GetCP() - pTarget->GetPosition().GetCP()) / FP_GRID_STEP;
	// CRAP: this parameter is no longer used anywhere
	CVec3 ptAttacker = pAttacker->GetPosition().GetCenter();
	//
	bool bBackStab = !pTarget->IsUnitAudible( pAttacker ) && !pTarget->IsUnitVisible( pAttacker );
	return NRPG::GetToHit( pAttacker, pAttacker->GetPose(), nDistance, ptAttacker,
		pTarget->GetPosition().pos, eHL, nExtraAP,
		pTarget, accessibleHLs, nHitCover, bFirstRound, CVec3( 1, 1, 1 ), bBackStab, nBullet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Used only for inteface tasks. Returns not exactly correct value.
int CGame::GetCompositeToHit( NWorld::CUnit *pAttacker, 
	NWorld::CUnit *pTarget, NAI::EHitLocation eHL, bool bFirstTurn )
{
	vector<NRPG::CAttackPortion> attack;
	CDynamicCast<NRPG::IUnitMission> pRealAttacker( pAttacker->GetRPG() );
	ASSERT( pRealAttacker );
	pRealAttacker->PrintLog( false );
	pRealAttacker->CreateAttack( &attack, false );
	if ( attack.empty() )
	{
		pRealAttacker->PrintLog( true );
		return 0;
	}
	CVec3 ptAttackPos;
	// retail NRPG::GetToHit @0x2b5ee0 and the executor CExecMeleeUnit::OnLabel (wUnitAttackExec.cpp:1645) pass
	// min-clear 0 for melee -- there is no muzzle to pull the cover-walk origin back from. Using the ranged
	// GetMinClearDistance() here culled every cover ray at arm's reach, so GetHitCover returned <=0 and the
	// cover-gated melee routing (throwing knives / destructible CUnitServer objects) displayed 0% on the attack
	// cursor even though the executor (min-clear 0) computed and landed a non-zero chance. Mirrors the already-
	// faithful sibling CGame::GetTileCompositeToHit.
	float fMinClearDistance;
	if ( NDb::IsMeleeWeapon( pRealAttacker->GetWeaponType() ) )
	{
		CVec3 pTargetHL;
		pAIMap->GetUnitHLPos( &pTargetHL, pAIMap->GetHull(pTarget), eHL );
		ptAttackPos = GetMeleeAttackPos( pAttacker, pTargetHL );
		fMinClearDistance = 0;
	}
	else
	{
		NAI::SUnitPosition pos = pAttacker->GetPosition();
		NAI::EDirection dir = GetShootDirection( pos.pos.pNet, pos.pos.p, pTarget->GetPosition().GetCP() );
		pos.pos.p.SetDirection( dir );
		ptAttackPos = pAttacker->GetAttackOrigin( pos );
		fMinClearDistance = pAttacker->GetMinClearDistance();
	}
	CObj<NRPG::CCoverInfo> pCover = CalcCovers( ptAttackPos, attack.front(), pAttacker, pTarget, eHL, fMinClearDistance );
	vector<int> accessibleHLs; // needed only for computing Melee ToHit
	if ( NDb::IsMeleeWeapon( pRealAttacker->GetWeaponType() ) )
	{
		pAIMap->GetAccessibleUnitHL( &accessibleHLs, pAttacker->GetPosition().GetCenter(), pAIMap->GetHull(pAttacker), F_MELEE_DISTANCE );
		if ( NAI::HL_ANY != eHL && find( accessibleHLs.begin(), accessibleHLs.end(), eHL ) != accessibleHLs.end() )
		{
			accessibleHLs.clear();
			accessibleHLs.push_back( eHL );
		}
		if ( accessibleHLs.empty() )
			return 0;
	}
	int nToHit = 0;
	int nRof = pRealAttacker->GetBulletsQuantityInShot();
	// retail @0x2b5ee0: the burst preview passes the LOOP INDEX as the bullet number (no
	// mission-side StartAttack/NextBullet cursor exists in retail).
	for ( int i = 0; i < nRof; ++i )
	{
		nToHit += GetAttackerToHit( pAttacker, pTarget, pAttacker->GetCarefulShotExtraAP(),
			eHL, accessibleHLs, pCover, bFirstTurn, i );
	}
	nToHit /= nRof;
	pRealAttacker->PrintLog( true );
	return nToHit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGame::GetGrenadeCompositeToHit( NWorld::CUnit *pAttacker, 
	CVec3 ptTarget, bool bFirstTurn, NDb::CRPGGrenade *pGrenade )
{
	int nDistance = fabs( pAttacker->GetPosition().GetCP() - ptTarget ) / FP_GRID_STEP;

	// release: the grenade calcer derives the thrown grenade from the unit's ACTIVE item, so the
	// explicit pGrenade arg is no longer forwarded (documented elision -- the active item is the
	// grenade on the normal throw path).
	return NRPG::GetGrenadeToHit( pAttacker, pAttacker->GetPose(),
		nDistance, pAttacker->GetPosition().GetCP(), bFirstTurn, ptTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGame::GetTileCompositeToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos, 
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn )
{
	int nDistance = fabs( pAttacker->GetPosition().GetCP() - ptTilePos ) / FP_GRID_STEP;
	//
	CDynamicCast<NRPG::IUnitMission> pRPG( pAttacker->GetRPG() );
	vector<NRPG::CAttackPortion> attack;
	pRPG->CreateAttack( &attack, false );
	if ( attack.empty() )
		return 0;
	//
	// retail composite tile to-hit @0x2b54a0 + RealCalcTileCovers @0x2b4700: a SWUNG melee weapon
	// (TH_MELEE) first gates on NWorld::CanMeleeAttack -- out of arm's reach returns the -1
	// sentinel (the retail cursor @0x1da200 SKIPS -1 results instead of printing them) -- and its
	// cover walk starts at the blow-height melee attack pos (GetMeleeAttackPos) with NO min-clear
	// pullback. Only ranged types (and thrown knives, TH_THROWING) cast from GetAttackOrigin with
	// GetMinClearDistance. The previous unconditional muzzle-origin+min-clear walk produced no
	// usable hit rays at melee range, so GetHitCover fed a constant 0 into the TH_MELEE 0/100
	// rule of GetTileToHit (@0x2b4df0) -> melee at ground/walls/objects always displayed 0%.
	CVec3 ptFrom;
	float fMinClearDistance;
	if ( GetToHitType( pAttacker ) == TH_MELEE )
	{
		CDynamicCast<NWorld::CUnitServer> pUS( pAttacker );
		if ( pUS && !NWorld::CanMeleeAttack( pUS, pAttacker->GetPosition(), ptTilePos ) )
			return -1;
		ptFrom = GetMeleeAttackPos( pAttacker, ptTilePos );
		fMinClearDistance = 0;
	}
	else
	{
		ptFrom = pAttacker->GetAttackOrigin( pAttacker->GetPosition() );
		fMinClearDistance = pAttacker->GetMinClearDistance();
	}
	//
	CObj<NRPG::CCoverInfo> pCover = CalcCoversForTile( ptFrom, attack[0], pAttacker,
		ptTilePos, fMinClearDistance );
	int nHitCover = GetHitCover( pAttacker, pCover );
	//
	return NRPG::GetTileToHit( pAttacker, pAttacker->GetPose(), nDistance, pAttacker->GetPosition().GetCP(),
		ptTilePos, eHitLocation, pAttacker->GetCarefulShotExtraAP(), nHitCover, bFirstTurn );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGame::GetBazookaToHit(  NWorld::CUnit *pAttacker, CVec3 ptTilePos, 
		NAI::ETileHitLocation eHitLocation, bool bFirstTurn )
{
	int nDistance = fabs( pAttacker->GetPosition().GetCP() - ptTilePos ) / FP_GRID_STEP;
	//
	CDynamicCast<NRPG::IUnitMission> pRPG( pAttacker->GetRPG() );
	vector<NRPG::CAttackPortion> attack;
	pRPG->CreateAttack( &attack, false );
	if ( attack.empty() )
		return 0;
	//
	CVec3 ptFrom = pAttacker->GetAttackOrigin( pAttacker->GetPosition() );
	return NRPG::GetRLauncherToHit( pAttacker, pAttacker->GetPose(), nDistance, pAttacker->GetPosition().GetCP(),
		ptTilePos, eHitLocation, pAttacker->GetCarefulShotExtraAP(), bFirstTurn );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetRandomForToHit( const NWorld::CUnit *pAttacker )
{ 
	if ( pAttacker->GetRPG()->GetRPGUnit()->IsCheatEnabled( CHEAT_TOHIT ) )
		return 1;
	//
	static int nLastRndForToHit = 200;
	int nRnd;
	while ( abs( (nRnd = random.Get(0,100)) - nLastRndForToHit ) < 6 );
	return nLastRndForToHit = nRnd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CheckToHit( NWorld::CUnit *pAttacker, NWorld::CUnit *pTarget, int nExtraAP, NAI::EHitLocation eHL,
	const vector<int> &accessibleHLs, CCoverInfo *pCover, bool bFirstRound, int *nToHit, int nBullet )
{
	*nToHit = GetAttackerToHit( pAttacker, pTarget, nExtraAP, eHL, accessibleHLs, pCover, bFirstRound, nBullet );
	csRPG << "\tToHit = " << *nToHit;
	int nCheck = GetRandomForToHit( pAttacker );
	csRPG << "\tCheck = " << nCheck << " Hit: " << bool(nCheck < *nToHit) << "\n";
	return float(*nToHit) / float(nCheck);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CheckTileToHit( NWorld::CUnit *pAttacker, const CVec3 ptTarget, int nExtraAP,
	NAI::ETileHitLocation eHitLocation, CCoverInfo *pCover, bool bFirstRound, int *nToHit, int nBullet )
{
	*nToHit = GetAttackerTileToHit( pAttacker, ptTarget, nExtraAP, eHitLocation, pCover, bFirstRound, nBullet );
	csRPG << "\tTileToHit = " << *nToHit;
	int nCheck = GetRandomForToHit( pAttacker );
	csRPG << "\tCheck = " << nCheck << " Hit: " << bool(nCheck < *nToHit) << "\n";
	return float(*nToHit) / float(nCheck);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::EDirection GetShootDirection( NAI::IPathNetwork *pNet, const NAI::SPathPlace &from, const CVec3 &ptTarget )
{
	NAI::SPosition fromPos;
	fromPos.p = from;
	fromPos.SetNetwork( pNet );
	CVec3 ptDir( ptTarget - fromPos.GetCP() );
	float fAngle = atan2( ptDir.y, ptDir.x );
	return pNet->GetClosestDir( from.GetLayer(), fAngle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 GetMeleeAttackPos( const NWorld::CUnit *pAttacker, const CVec3 &ptTarget )
{
	const NAI::SUnitPosition &pos = pAttacker->GetPosition();
	CVec3 ptAttackPos = pos.GetEyePosition();
	NAI::EBlowHeight eBH = NAI::GetBlowHeight( pos, ptTarget );
	switch ( eBH )
	{
		case NAI::BH_TOP:
			break;
		case NAI::BH_MIDDLE:
			ptAttackPos = pos.GetCenter();
			break;
		case NAI::BH_BOTTOM:
			ptAttackPos = pos.GetCP();
			break;
	}
	return ptAttackPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// VISION
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 GetForwardDir( const NAI::SUnitPosition &observerPos )
{
	float fAngle = observerPos.GetDirection();
	CVec3 ptDir( cos(fAngle), sin(fAngle), 0 );
	return ptDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_LOWER_CUBE_DH = 0.5f;
const float F_NEXT_CUBE_DH = 0.5f;
// retail CGame::GetVisibilityArea @0x298840 (IGame vtbl+0x20, disasm-proven): gather radius =
// NRPG::GetMaxSightDistance() (40, @0x2ba2e0) NOT N_SIGHTDISTANCE; pre-cull = the vision tracker's
// height-stretched IsWithinSightRange @0x2c7fa0 with the observer's REAL GetUnitSightDistance; the
// per-point probe = the range/FOV-gated 5-arg IsCubeVisible with cos(GetSightFOV()*0.5) (recomputed
// per point in retail -- hoisted here, same value).
void CGame::GetVisibilityArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pObserver )
{
	CVec3 ptFwdDir = GetForwardDir( pObserver->GetPosition() );
	const CVec3 ptFrom = pObserver->GetPosition().GetEyePosition();
	NRPG::CUnit *pRPG = IsValid( pObserver->GetRPG() ) ? pObserver->GetRPG()->GetRPGUnit() : 0;
	const float fRange = GetUnitSightDistance( pRPG );
	const float fCosHalfFOV = cos( ( IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI ) * 0.5f );
	vector<NAI::SPathPlace> testPlaces;
	pNet->GetNearPlaces( SSphere( ptFrom, NRPG::GetMaxSightDistance() ), &testPlaces );
	pRes->clear();
	for ( int k = 0; k < testPlaces.size(); ++k )
	{
		NAI::SPosition pos;
		pos.p = testPlaces[k];
		pos.SetNetwork( pNet );
		CVec3 ptTest = pos.GetCP();
		ptTest += CVec3( 0,0,F_LOWER_CUBE_DH );
		if ( !pVision->IsWithinSightRange( ptFrom, ptTest, fRange ) )
			continue;
		for ( int j = 0; j < 3; ++j )
		{
			CVec3 ptTestPoint = ptTest + CVec3(0,0,F_NEXT_CUBE_DH * j );
			if ( !pVision->IsCubeVisible( ptFrom, ptTestPoint, ptFwdDir, fRange, fCosHalfFOV ) )
				pRes->push_back( SVisibilitySpot( ptTestPoint, j ) ); // designers asked for it
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGame::IsVisible( const CVec3 &vFrom, const vector<CVec3> &testPoints )
{
	for ( int k = 0; k < testPoints.size(); ++k )
	{
		if ( pVision->IsCubeVisible( vFrom, testPoints[k], CVec3(0,0,0) ) )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*static bool IsWithin90Degrees( const CVec3 &vFrom, const CVec3 &vTo, const CVec3 &vFwd )
{
	CVec3 vDir = vTo - vFrom;
	float f = fabs2( vDir ) * fabs2( vFwd );
	float fDot = vFwd * vDir;
	return fDot * fabs(fDot) > 0.5f * f;
}*/
void CGame::GetVisibleFromArea( vector<SVisibilitySpot> *pRes, const NWorld::CUnit *pTarget, CVec3 &vNear, float fRadius, int nPoses )
{
	CVec3 ptFwdDir = GetForwardDir( pTarget->GetPosition() );
	const CVec3 vTargetUnit = pTarget->GetPosition().GetEyePosition();
	//CObj<NAI::CAIVisionCube> pCube = pAIMap->GetVisionCube( ptFrom );
	vector<NAI::SPathPlace> testPlaces;
	pNet->GetNearPlaces( SSphere( vNear, fRadius ), &testPlaces );
	pRes->clear();
	vector<CVec3> testPoints;
	const NAI::SUnitPosition &targetPos = pTarget->GetPosition();
	GetOccupiedCubes( &testPoints, targetPos.pos );
	CVec3 vFwdDir = GetForwardDir( targetPos );
	for ( int k = 0; k < testPlaces.size(); ++k )
	{
		CVec3 vFrom;
		NAI::SUnitPosition pos;
		pos.pos.p = testPlaces[k];
		pos.pos.SetNetwork( pNet );
		int nRes = 0;

		pos.pos.p.SetPose( NAI::CM_LAY );
		vFrom = pos.GetEyePosition();
		if ( /*IsWithin90Degrees( vTargetUnit, vFrom, vFwdDir ) &&*/ IsVisible( vFrom, testPoints ) && (nPoses&1) )
			pRes->push_back( SVisibilitySpot( vFrom, 0 ) );
		
		pos.pos.p.SetPose( NAI::CM_CROUCH );
		vFrom = pos.GetEyePosition();
		if ( /*IsWithin90Degrees( vTargetUnit, vFrom, vFwdDir ) &&*/ IsVisible( vFrom, testPoints ) && (nPoses&2) )
			pRes->push_back( SVisibilitySpot( vFrom, 1 ) );
		
		pos.pos.p.SetPose( NAI::CM_STAND );
		vFrom = pos.GetEyePosition();
		if ( /*IsWithin90Degrees( vTargetUnit, vFrom, vFwdDir ) &&*/ IsVisible( vFrom, testPoints ) && (nPoses&4) )
			pRes->push_back( SVisibilitySpot( vFrom, 2 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetOccupiedCubes( vector<CVec3> *pRes, const NAI::SPosition &pos )
{
	CVec3 ptBase = pos.GetCP() + CVec3( 0,0,F_LOWER_CUBE_DH );
	switch ( pos.p.GetPose() )
	{
	case NAI::CRAWL:
		{
			float fAngle = pos.GetDirection();
			CVec3 ptShift( cos( fAngle ) * FP_GRID_STEP, sin( fAngle ) * FP_GRID_STEP, 0 );
			pRes->push_back( ptBase );
			pRes->push_back( ptBase + ptShift );
			pRes->push_back( ptBase - ptShift );
		}
		break;
	case NAI::CROUCH:
		pRes->push_back( ptBase );
		pRes->push_back( ptBase + CVec3(0,0,F_NEXT_CUBE_DH * 1 ) );
		break;
	case NAI::WALK:
	case NAI::RUN:
		pRes->push_back( ptBase );
		pRes->push_back( ptBase + CVec3(0,0,F_NEXT_CUBE_DH * 1 ) );
		pRes->push_back( ptBase + CVec3(0,0,F_NEXT_CUBE_DH * 2 ) );
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTileOccupiedCubes( vector<CVec3> *pRes, const CVec3 &ptPos, NAI::ETileHitLocation eHitLocation )
{
	CVec3 ptBase = ptPos;
	switch ( eHitLocation )
	{
	case NAI::THL_LOWER:
		pRes->push_back( ptBase );
		break;
	case NAI::THL_MIDDLE:
		pRes->push_back( ptBase + CVec3(0,0,F_NEXT_CUBE_DH * 1 ) );
		break;
	case NAI::THL_UPPER:
		pRes->push_back( ptBase + CVec3(0,0,F_NEXT_CUBE_DH * 2 ) );
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CGame::CheckVisibility @0x298cb0 (IGame vtbl+0x10; disasm-proven): the per-unit visibility
// probe -- FOV = bUseFOV ? observer's GetSightFOV() (pi default, perk 0x52) : 2pi (0x40c90fdb);
// range = GetUnitSightDistance(observer) (20 x perk 0x53, night x1.5015 perk 0x51); then the 4-arg
// CheckPositionVisibility (per-cube retail 9-ray). Replaces the legacy 2-arg CheckPositionVisibility
// route (hardcoded N_SIGHTDISTANCE + 180-degree half-space + flat range).
bool CGame::CheckVisibility( const NWorld::CUnit *pObserver, const NWorld::CUnit *pDest, bool bUseFOV )
{
	NRPG::CUnit *pRPG = IsValid( pObserver->GetRPG() ) ? pObserver->GetRPG()->GetRPGUnit() : 0;
	float fFOV = ( bUseFOV && IsValid( pRPG ) ) ? pRPG->GetSightFOV() : FP_2PI;
	float fRange = GetUnitSightDistance( pRPG );
	return CheckPositionVisibility( pObserver->GetPosition(), pDest->GetPosition().pos, fRange, fFOV );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGame::CanSee( const NWorld::CUnit *pObserver, const CVec3 &vPos )
{	// retail: CalcViewerInfo @0x298570 (range = GetUnitSightDistance, cosHalfFOV = cos(GetSightFOV*0.5))
	// + CanSeeCenter @0x298750 -> the SINGLE-RAY IsPointVisible @0x2c9440 (vision vtbl+0x14).
	const NAI::SUnitPosition &observerPos = pObserver->GetPosition();
	NRPG::CUnit *pRPG = IsValid( pObserver->GetRPG() ) ? pObserver->GetRPG()->GetRPGUnit() : 0;
	return pVision->IsPointVisible( observerPos.GetEyePosition(), vPos, GetForwardDir( observerPos ),
		GetUnitSightDistance( pRPG ), cos( ( IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI ) * 0.5f ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGame::CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos )
{
	CVec3 ptFwdDir = GetForwardDir( observerPos );
	CVec3 ptFrom = observerPos.GetEyePosition();
	vector<CVec3> testPoints;
	GetOccupiedCubes( &testPoints, targetPos );
	for ( int k = 0; k < testPoints.size(); ++k )
	{
		if ( pVision->IsCubeVisible( ptFrom, testPoints[k], ptFwdDir ) )
			return true;
	}
	return false;
}
// (CheckAIPositionVisibility deleted: retail has no such function -- absent from the CGame vtable --
// and the dev tree had ZERO callers; its body even read testPoints[1] unchecked.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CGame::IsCorpseVisible @0x298da0 (game vtbl+0x50): eye + facing of the observer; range = the
// 1x GetUnitSightDistance (vtbl+0x34); cosHalfFOV = cos(GetSightFOV*0.5); one 9-ray IsCubeVisible per
// corpse hit-location point (CUnit vtbl+0x94 GetCorpseHLs = CDumbUnitServer::corpseHLpos); first
// visible point wins.
bool CGame::IsCorpseVisible( const NWorld::CUnit *pObserver, const NWorld::CUnit *pCorpse )
{
	if ( pObserver == 0 || pCorpse == 0 )
		return false;
	const NAI::SUnitPosition &pos = pObserver->GetPosition();
	CVec3 ptFwdDir = GetForwardDir( pos );
	CVec3 ptFrom = pos.GetEyePosition();
	NRPG::CUnit *pRPG = IsValid( pObserver->GetRPG() ) ? pObserver->GetRPG()->GetRPGUnit() : 0;
	float fRange = GetUnitSightDistance( pRPG );
	float fCosHalfFOV = cos( ( IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI ) * 0.5f );
	const vector<CVec3> *pts = pCorpse->GetCorpseHLs();
	for ( int k = 0; pts && k < (int)pts->size(); ++k )
		if ( pVision->IsCubeVisible( ptFrom, (*pts)[k], ptFwdDir, fRange, fCosHalfFOV ) )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CGame::CheckPositionVisibility @0x298fa0 -- the 4-arg overload (oracle s2_rpggame.h:219-250):
//   ptFwdDir   = (cos dir, sin dir, 0) off the observer's facing
//   ptFrom     = the observer's eye position
//   cosHalfFOV = cos( fFOVAngle * 0.5 )
//   for each occupied cube of the target: ONE range/FOV-gated ray -> the first visible cube wins.
bool CGame::CheckPositionVisibility( const NAI::SUnitPosition observerPos, const NAI::SPosition targetPos,
	float fRange, float fFOVAngle )
{
	CVec3 ptFwdDir = GetForwardDir( observerPos );
	CVec3 ptFrom = observerPos.GetEyePosition();
	const float fCosHalfFOV = cos( fFOVAngle * 0.5f );
	vector<CVec3> testPoints;
	GetOccupiedCubes( &testPoints, targetPos );
	for ( int k = 0; k < testPoints.size(); ++k )
	{
		// retail leaf (disasm @0x298fa0: vision vtbl+0x10) = the 9-ray IsCubeVisible @0x2c9160, NOT the
		// single-ray IsPointVisible the first port assumed.
		if ( pVision->IsCubeVisible( ptFrom, testPoints[k], ptFwdDir, fRange, fCosHalfFOV ) )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CGame::GetUnitSightDistance @0x2984d0: the unit's flat sight range (CUnit::GetSightDistance
// @0x2ba680 -- 20.0, perk 0x53), boosted x1.5015 (0x3fc03133) at night for a unit with the night-vision
// perk 0x51.
float CGame::GetUnitSightDistance( NRPG::CUnit *pRPGUnit )
{
	if ( !IsValid( pRPGUnit ) )
		return 0.0f;
	float fDist = pRPGUnit->GetSightDistance();
	if ( bNight && pRPGUnit->HasPerk( 0x51 ) )
		return fDist * 1.5015015f;   // 0x3fc03133
	return fDist;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CGame::GetMaxUnitSightDistance @0x298520 (vtbl+0x38): 2x the unit's sight range -- the
// perception sweep's candidate-gather radius (decomp: fVar1 + fVar1).
float CGame::GetMaxUnitSightDistance( NRPG::CUnit *pRPGUnit )
{
	float f = GetUnitSightDistance( pRPGUnit );
	return f + f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IGame* CreateGame( NAI::IAIMap *pAIMap, NAI::IPathNetwork *pNet )
{
	return new CGame( pAIMap, pNet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x02841121, CGame );
REGISTER_SAVELOAD_CLASS( 0x02841161, CCoverInfo );
BASIC_REGISTER_CLASS( IGame )
//BASIC_REGISTER_CLASS( IAttackable )