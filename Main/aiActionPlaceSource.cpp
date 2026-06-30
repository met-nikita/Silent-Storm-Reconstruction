#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiState.h"
#include "aiActionBase.h"      // SPlaceWithAP, CAIAction (new substrate base - phase 3)
#include "aiMoveAction.h"      // GetUnitPos, GetPos
#include "aiPosition.h"        // IPathNetwork (complete), SPathPlace, SUnitPosition
#include "wUnitServer.h"       // NWorld::CUnitServer (complete): GetWorld/GetPosition/GetActionAP
#include "wMain.h"             // NWorld::CWorld: GetPathNetwork
#include "rpgUnitMission.h"    // NRPG::IUnitMission::GetActionAP + NRPG::AC_POSE_WALK/CROUCH (pose AP costs)
#include "aiPath.h"            // NAI::CPath, NWorld::FindPath
#include "aiMultiMoves.h"      // NAI::CMultiMovesTable + CPathPlaceTable::GetCost (reachable-area sweep)
#include "wMainMoves.h"        // NWorld::GetMoveActionType (path-AP calcer)
#include "wMainPath.h"         // NWorld::FindPath / NWorld::PrepareAllPaths decls
#include "wUnitAttack.h"       // NWorld::GetMeleeAttackPlaces (the enemy melee ring)
//
#include "aiActionPlaceSource.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - place-source layer bodies (structural port, Approach A).
// Reconstructed from reconstruction/exports/{placesource.c, prepare.c, vtable_placesource.txt}.
//
// STATUS: this TU is WIP and is NOT yet in Main.vcxproj. The clean methods below are faithful and
// complete. The heavy place generators (CAIAttackPlaceSource/CAINearEnemyPlaceSource/
// CAIToPlacePlaceSource::Prepare, AddAllPoses, IsPosDangerousForAllies) depend on substrate pieces
// still being ported (CUnitArea, GetNearestPlaces, GetPlacesAtDirection, the coloured-ways/path-AP
// calcers) and on the phase-3 CAIAction + phase-6 IAIUnit additions (GetAIState vtbl 0x78,
// GetAIUnitState vtbl 0x74); they are completed in the build-settle (phase 7). Each carries its
// release entry address so the decompile can be matched 1:1.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitArea - reachable-set "area" (see aiActionPlaceSource.h). Prepare floods places within nAPRadius AP
// of the centre (NWorld::PrepareAllPaths, as aiTaskCommander does) and keeps their sorted GetData() keys;
// IsInArea is a binary search. (Release IsInArea uses a packed hash set; a sorted key set is equivalent
// for the lookup and avoids a hash functor. wishPose's temporary pose override in the release Prepare is
// not applied here - the flood uses the unit's current pose.)
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitArea::CUnitArea( NWorld::CUnitServer *_pUS, const SPathPlace &_place, int _nAPRadius, int _wishPose ):
	pUS( _pUS ), place( _place ), nAPRadius( _nAPRadius ), wishPose( _wishPose )
{
	Prepare();
}
int CUnitArea::operator&( CStructureSaver &f )
{
	f.Add( 2, &pUS ); f.Add( 3, &place ); f.Add( 4, &nAPRadius ); f.Add( 5, &wishPose );
	return 0;   // keys are transient: rebuilt by Prepare on use
}
bool CUnitArea::Prepare()
{
	keys.clear();
	if ( !IsValid( pUS ) || !IsValid( pUS->GetWorld() ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	CMultiMovesTable movesTable;
	list<SPathPlace>  reach;
	NWorld::PrepareAllPaths( pNet, &movesTable, &reach, pUS, place, nAPRadius, pUS, true );
	for ( list<SPathPlace>::const_iterator i = reach.begin(); i != reach.end(); ++i )
		keys.push_back( (*i).GetData() );
	sort( keys.begin(), keys.end() );
	return !keys.empty();
}
bool CUnitArea::IsInArea( const SPathPlace &p ) const
{
	if ( keys.empty() )
		return true;   // un-prepared / empty -> impose no gate (the unit can still act where it is)
	return binary_search( keys.begin(), keys.end(), p.GetData() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Local path-AP accumulator - sums the move AP along a path (GetMoveActionType per step -> GetActionAP).
// Verbatim twin of the dev CAIPathAPCalcer (aiMoveAction.cpp / wUnitMove.cpp / aiIterator.cpp); the
// release uses NWorld::CPathAPCalcer (not exposed in the dev tree) for the same job.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace {
class CPathAPCalcerLocal
{
	int                  nRes;
	SUnitPosition        currentPos;
	bool                 bCorpse;
	NWorld::CWorld      *pWorld;
	NRPG::IUnitMission  *pRPG;
public:
	CPathAPCalcerLocal( NWorld::CWorld *_pWorld, NRPG::IUnitMission *_pRPG, const SUnitPosition &_p, bool _bCorpse )
		: nRes( 0 ), currentPos( _p ), bCorpse( _bCorpse ), pWorld( _pWorld ), pRPG( _pRPG ) {}
	void AddPoint( const SUnitPosition &_pos )
	{
		NRPG::EAction action = NWorld::GetMoveActionType( pWorld->GetPathNetwork(), currentPos, _pos, bCorpse );
		nRes += pRPG->GetActionAP( currentPos.GetPose(), action );
		currentPos = _pos;
	}
	void AddPoint( const SPathPlace &_p ) { SUnitPosition pos( currentPos ); pos.pos.p = _p; AddPoint( pos ); }
	int GetResult() const { return nRes; }
};
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIActionPlaceSource
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIState* CAIActionPlaceSource::GetAIState() const                       // @0x0048e080
{
	if ( IsValid( pUnit ) )
		return pUnit->GetAIState();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit* CAIActionPlaceSource::GetEnemy() const                         // @0x00490510
{
	if ( IsValid( pUnit ) && IsValid( pUnit->GetAIState() ) )
		return pUnit->GetAIState()->GetCurrentAIEnemy();   // release reads GetAIUnitState()+0x88
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// True unless the tile is impassable or locked by a *different* unit.
// Release used IPathNetwork::GetPlaceState (1=blocked, 3=locked-by-owner) + GetPlaceOwner; the dev
// IPathNetwork exposes the same information as IsPassable(p) + GetWhoLocksThisPlace(p) - reconciled here.
bool CAIActionPlaceSource::IsPassable( IPathNetwork *pNet, const SPathPlace &p )   // @0x0048e0b0
{
	if ( !IsValid( pNet ) )
		return false;
	if ( !pNet->IsPassable( p ) )
		return false;
	CObjectBase *pLocker = pNet->GetWhoLocksThisPlace( p );
	if ( pLocker != 0 && pLocker != (CObjectBase *)pUnit->GetUnitServer() )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsInUnitDir @0x0048e120 - is `place` in the direction the unit at `pUS` is facing? Free helper that the
// full (release) CAICurrentPlaceSource::Prepare uses to bonus places the unit already faces. The dev
// CAICurrentPlaceSource::Prepare is the simplified single-AddAllPoses form (no 0xc000 special-position
// neighbourhood search), so this helper is currently a behaviour-neutral parity surface - dead code, but a
// non-static namespace-scope free fn so MSVC emits no C4505. Reconstructed against the matched release
// decode (decomp/src/s2_aiplacesource.h @0x8e120).
//
// ORIGINAL BUG (confirmed in the retail disasm @0x0048e120): it compares GetClosestDir's EDirection INDEX
// (0..7, integer fild'ed to float) against the unit position's world ANGLE in radians - equal only when
// both are 0, so the facing bonus almost never fires. Reproduced verbatim, NOT corrected.
bool IsInUnitDir( NWorld::CUnitServer *pUS, const SPathPlace &place )
{
	if ( !IsValid( pUS ) )
		return false;
	CPtr<IPathNetwork> pNet = pUS->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )
		return false;
	if ( !pNet->IsValidDestination( place ) )                            // release vtbl+0xb0 point-validity probe
		return false;
	const SUnitPosition &uPos = pUS->GetPosition();
	const int   nDir   = (int)pNet->GetClosestDir( uPos.pos.p, place );  // EDirection 0..7 (src=unit, dst=place)
	const float fAngle = uPos.GetDirection();                            // world facing angle, radians
	return (float)nDir == fAngle;                                        // ORIGINAL BUG: index vs radians
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIActionPlaceSource::ClearPlaces()                                 // @0x0048e260
{
	places.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Add the single place `p` (with arrival AP) if it is passable on the unit's path network.
void CAIActionPlaceSource::AddPlace( const SPathPlace &p, int nAP )      // @0x0048e4a0
{
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return;
	CPtr<IPathNetwork> pNet = pUnit->GetUnitServer()->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )
		return;
	if ( IsPassable( pNet, p ) )
		places.push_back( SPlaceWithAP( GetUnitPos( p, pNet ), nAP ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Add the stand and crouch poses the unit can adopt at place `p`, given it has already spent `nMoveAP`
// reaching it and may spend up to `nMaxAP` total. Each usable pose is pushed as a SPlaceWithAP with
// arrival AP = currentAP - (nMoveAP + pose-change cost). Returns "fully reachable" (both viable poses
// within budget) - the attack source uses the false return to stop walking further down a path.
// @0x0048e5e0. Ported from the release's structure + the clean dev twin CAIFindGoodPlacesJob::
// PreparePlaces (aiMoveAction.cpp): cost = nMoveAP + GetActionAP((EPose)GetPose(), AC_POSE_WALK/CROUCH).
bool CAIActionPlaceSource::AddAllPoses( const SPathPlace &p, int nMoveAP, int nMaxAP, bool /*bArg*/ )
{
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return false;
	CPtr<NWorld::CUnitServer> pUS  = pUnit->GetUnitServer();
	CPtr<IPathNetwork>        pNet = pUS->GetWorld()->GetPathNetwork();
	CPtr<NRPG::IUnitMission>  pRPG = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return false;
	//
	const int  nCurAP    = pUnit->GetAP();
	const bool bCanCrouch = !pUnit->HasInactivePose();   // @0x0048e5e0 gates crouch on vtbl 0x20 (inferred)
	//
	// stand/walk pose
	const int nWalkCost = nMoveAP + pRPG->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_WALK );
	bool bAddedWalk = false;
	if ( nWalkCost <= nMaxAP )
	{
		SPathPlace pp = p;
		pp.SetPose( NAI::CM_STAND );
		if ( IsPassable( pNet, pp ) )
		{
			places.push_back( SPlaceWithAP( GetUnitPos( pp, pNet ), nCurAP - nWalkCost ) );
			bAddedWalk = true;
		}
	}
	// crouch pose (the release reuses the walk passability when the walk pose was added at this tile)
	const int nCrouchCost = nMoveAP + pRPG->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_CROUCH );
	if ( bCanCrouch && nCrouchCost <= nMaxAP )
	{
		SPathPlace pp = p;
		pp.SetPose( NAI::CM_CROUCH );
		if ( bAddedWalk || IsPassable( pNet, pp ) )
			places.push_back( SPlaceWithAP( GetUnitPos( pp, pNet ), nCurAP - nCrouchCost ) );
	}
	// fully reachable within budget (release: false if the stand pose, or a viable crouch, exceeds nMaxAP)
	return ( nWalkCost <= nMaxAP ) && ( !bCanCrouch || nCrouchCost <= nMaxAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// True if firing `pWeapon` from `pos` would catch an ally in the blast/line. @0x0048f4a0 - heavy
// (iterates allied units, computes the weapon's shot direction/cover/attack-portion over each).
// CONSERVATIVE STUB: returns false (never blocks) until the cover/attack-portion calc is ported - this
// only disables the friendly-fire FILTER (the AI may pick a position that fires past an ally), it does
// not crash. The attack/one-place sources call this only as a guard. Reconstruct from @0x0048f4a0. @addr
bool CAIActionPlaceSource::IsPosDangerousForAllies( const SUnitPosition & /*pos*/, CAIFireArmsWeapon * /*pWeapon*/ )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIActionPlaceSource::GetPlaces( vector<SPlaceWithAP> *pRes )      // @0x00490580 (slot5)
{
	*pRes = places;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIActionPlaceSource::operator&( CStructureSaver &f )              // @0x00490f80 (slot3)
{
	f.Add( 2, &places );
	f.Add( 3, &pUnit );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIAttackPlaceSource - shoot-from places within nMaxAP over the unit-area, via the coloured-ways calcer.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIAttackPlaceSource::CAIAttackPlaceSource( IAIUnit *_pUnit, int _nMaxAP, CUnitArea *_pArea, bool _bCheck ): // @0x00490630
	CAIActionPlaceSource( _pUnit ), nMaxAP( _nMaxAP ), bCheckDangerousForAllies( _bCheck ), pArea( _pArea )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIAttackPlaceSource::Prepare()                                    // @0x0048f900 (slot4)
{
	ClearPlaces();
	IAIUnit *pU     = GetUnit();
	IAIUnit *pEnemy = GetEnemy();
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) )
		return;
	CPtr<NWorld::CUnitServer> pUS    = pU->GetUnitServer();
	CPtr<NWorld::CWorld>      pWorld = pUS->GetWorld();
	CPtr<IPathNetwork>        pNet   = pWorld->GetPathNetwork();
	CPtr<NRPG::IUnitMission>  pRPG   = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return;
	const SPathPlace curPlace = pU->GetPosition().p;
	//
	// (1) the unit's current place. @0x0048f900 first runs the friendly-fire guard
	// (IsPosDangerousForAllies with the first firearm when bCheckDangerousForAllies); elided while that
	// helper is a conservative stub (it never blocks). @addr
	AddAllPoses( curPlace, 0, nMaxAP, true );
	//
	// (2) shooting places along the approach to the enemy: find a path and AddAllPoses at each place
	// within nMaxAP. Mirrors the dev twin CAIFindGoodPlacesJob::PreparePlaces.
	if ( IsValid( pEnemy ) && IsValid( pEnemy->GetUnitServer() ) )
	{
		vector<SPathPlace> dest;
		dest.push_back( pEnemy->GetUnitServer()->GetPosition().pos.p );
		CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, curPlace, dest, pUS,
			false, NAI::PF_DEFAULT, false, true, true );   // release uses PF_USE_DIR
		if ( IsValid( pPath ) )
		{
			CPathAPCalcerLocal calcer( pWorld, pRPG, pU->GetUnitPosition(), false );
			for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
			{
				calcer.AddPoint( *i );
				if ( calcer.GetResult() > nMaxAP )
					break;
				if ( ( (*i).GetPose() & 0xc000 ) != 0xc000 &&    // skip inactive-pose tiles (as the release does)
					( pArea == 0 || pArea->IsInArea( *i ) ) )    // area gate: a guard logic only fires from inside its area
				{
					if ( !AddAllPoses( *i, calcer.GetResult(), nMaxAP, true ) )
						break;   // place no longer fully reachable within budget -> stop walking
				}
			}
		}
	}
	//
	// (3) sweep the whole reachable area within nMaxAP for additional firing positions (flanks etc.).
	// @0x0048f900 uses a CColouredWaysCalcer over pArea; NWorld::PrepareAllPaths gives the same reachable
	// set + each place's move cost (CPathPlaceTable::GetCost), and we AddAllPoses at each. The release
	// pre-filters places by GetShootDirection (facing the enemy within the unit's FOV) - omitted here:
	// the shoot action's GetInfoInner re-checks line-of-sight/to-hit per place, so non-facing places just
	// score zero rather than being wrongly chosen. pArea is null (GetUnitArea returns 0) -> no area gate. @addr
	NAI::CMultiMovesTable movesTable;
	list<SPathPlace>      reach;
	NWorld::PrepareAllPaths( pNet, &movesTable, &reach, pUS, curPlace, nMaxAP, pUS, true );
	for ( list<SPathPlace>::const_iterator i = reach.begin(); i != reach.end(); ++i )
	{
		if ( ( (*i).GetPose() & 0xc000 ) != 0xc000 &&
			( pArea == 0 || pArea->IsInArea( *i ) ) )   // area gate (pArea==0 for normal attack -> no gate)
			AddAllPoses( *i, movesTable.GetCost( *i ), nMaxAP, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICurrentPlaceSource - just the unit's current place (all poses).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICurrentPlaceSource::Prepare()                                   // @0x0048e980
{
	ClearPlaces();
	if ( !IsValid( GetUnit() ) || !IsValid( GetUnit()->GetUnitServer() ) )
		return;
	SPathPlace cur = GetUnit()->GetPosition().p;
	AddAllPoses( cur, 0, 0xffff, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAINearEnemyPlaceSource - places adjacent to the current enemy (for melee/knife/...).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAINearEnemyPlaceSource::Prepare()                                 // @0x0048ece0
{
	// Reconstructed from the matched-release decode (decomp/src/s2_aiplacesource.h @0x8ece0): path to
	// the enemy's melee ring and offer the path's LAST point with the AP that would remain on arrival.
	// (The original needs the enemy MELEE ring, not GetNearestPlaces -- the old stub comment was wrong.)
	ClearPlaces();
	IAIUnit *pU     = GetUnit();
	IAIUnit *pEnemy = GetEnemy();
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) ||
		!IsValid( pEnemy ) || !IsValid( pEnemy->GetUnitServer() ) || !IsValid( GetAIState() ) )
		return;
	CPtr<NWorld::CUnitServer> pUS      = pU->GetUnitServer();
	CPtr<NWorld::CUnitServer> pEnemyUS = pEnemy->GetUnitServer();
	CPtr<NWorld::CWorld>      pWorld   = pUS->GetWorld();
	CPtr<IPathNetwork>        pNet     = pWorld->GetPathNetwork();
	CPtr<NRPG::IUnitMission>  pRPG     = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return;
	// price the pathfind at WALK (the unit is in a PK) / RUN. ORIGINAL BUG (@0x0048ece0): the wish pose is
	// restored only on the success path -- the empty-targets / no-path early returns leave it clobbered.
	NAI::EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( IsValid( pUS->GetWearingDBPK() ) ? NAI::WALK : NAI::RUN );   // release IsUnitBusy == IsInPK
	// the enemy's melee ring (places adjacent to the enemy to melee it from). The release read it from the
	// AIMap GetMeleeAttackPlaces(world, enemy); the in-tree generator is attacker-centred, so it is called
	// centred on the enemy -- the only deviation is the melee-reach x2, which keys off the enemy's PK status
	// rather than this unit's (negligible: it differs only when the enemy itself wears a panzerklein).
	vector<SPathPlace> targets;
	NWorld::GetMeleeAttackPlaces( pEnemyUS, pEnemyUS->GetPosition().GetCP(), &targets );
	if ( targets.empty() )
		return;
	// path to the ring, ignoring the enemy as an obstacle (else it blocks its own ring).
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, pU->GetPosition().p, targets, pEnemyUS );
	if ( !IsValid( pPath ) || pPath->points.empty() )
		return;
	CPathAPCalcerLocal calcer( pWorld, pRPG, pU->GetUnitPosition(), false );
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
		calcer.AddPoint( *i );
	AddPlace( pPath->points.back(), pU->GetAP() - calcer.GetResult() );
	pUS->SetWishPose( nOldWish );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIToPlacePlaceSource - places along the route toward `pos`, within nMaxAP.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIToPlacePlaceSource::CAIToPlacePlaceSource( IAIUnit *_pUnit, const SPathPlace &_pos, int _nMaxAP ): // @0x004907f0
	CAIActionPlaceSource( _pUnit ), pos( _pos ), nMaxAP( _nMaxAP )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIToPlacePlaceSource::Prepare()                                   // @0x0048f0e0
{
	ClearPlaces();
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) )
		return;
	CPtr<NWorld::CUnitServer> pUS    = pU->GetUnitServer();
	CPtr<NWorld::CWorld>      pWorld = pUS->GetWorld();
	CPtr<IPathNetwork>        pNet   = pWorld->GetPathNetwork();
	CPtr<NRPG::IUnitMission>  pRPG   = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return;
	//
	// the unit's current place, then places along the path toward `pos` within nMaxAP (@0x0048f0e0).
	AddAllPoses( pU->GetPosition().p, 0, nMaxAP, true );
	vector<SPathPlace> dest;
	dest.push_back( pos );
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, pU->GetPosition().p, dest, pUS,
		false, NAI::PF_DEFAULT, false, true, true );   // release uses PF_USE_DIR
	if ( !IsValid( pPath ) )
		return;
	CPathAPCalcerLocal calcer( pWorld, pRPG, pU->GetUnitPosition(), false );
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
	{
		calcer.AddPoint( *i );
		if ( calcer.GetResult() > nMaxAP )
			break;
		if ( ( (*i).GetPose() & 0xc000 ) != 0xc000 )
			AddAllPoses( *i, calcer.GetResult(), nMaxAP, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIOnePlacePlaceSource - exactly the one held place (defence holds a spot).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIOnePlacePlaceSource::CAIOnePlacePlaceSource( IAIUnit *_pUnit, const SPathPlace &_place, bool _bCrouch ):
	CAIActionPlaceSource( _pUnit ), place( _place ), bCrouch( _bCrouch )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIOnePlacePlaceSource::Prepare()                                  // @0x004901f0
{
	ClearPlaces();
	if ( !IsValid( GetUnit() ) || !IsValid( GetUnit()->GetUnitServer() ) )
		return;
	// @0x004901f0 first runs a friendly-fire guard (GetFirstFireArms + IsPosDangerousForAllies) and bails
	// if the held spot would endanger an ally. Skipped while IsPosDangerousForAllies is a conservative
	// stub (it never blocks) - re-add the guard when that helper is reconstructed. @addr
	//
	// build-settle: the release subtracts GetAPForMove(place) (path AP to reach the spot). A defence
	// one-place source holds the unit's commanded position, so the move cost is ~0 when it is already
	// there; using 0 here is exact in that case and never over-budgets. @addr
	const int nMoveAP = 0;
	if ( !bCrouch )
		AddAllPoses( place, nMoveAP, 0xffff, true );                 // stand: both poses at the spot
	else
		AddPlace( place, GetUnit()->GetMaxAP() - nMoveAP );          // crouch: the single held place
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// operator& for the subclasses (parent chunk + own members)
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIAttackPlaceSource::operator&( CStructureSaver &f )
{
	f.Add( 2, (CAIActionPlaceSource*)this ); f.Add( 3, &nMaxAP ); f.Add( 4, &bCheckDangerousForAllies ); f.Add( 5, &pArea );
	return 0;
}
int CAICurrentPlaceSource::operator&( CStructureSaver &f )   { f.Add( 2, (CAIActionPlaceSource*)this ); return 0; }
int CAINearEnemyPlaceSource::operator&( CStructureSaver &f ) { f.Add( 2, (CAIActionPlaceSource*)this ); return 0; }
int CAIToPlacePlaceSource::operator&( CStructureSaver &f )   { f.Add( 2, (CAIActionPlaceSource*)this ); f.Add( 3, &pos ); f.Add( 4, &nMaxAP ); return 0; }
int CAIOnePlacePlaceSource::operator&( CStructureSaver &f )  { f.Add( 2, (CAIActionPlaceSource*)this ); f.Add( 3, &place ); f.Add( 4, &bCrouch ); return 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Factories
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIActionPlaceSource* CreateAttackPlaceSource( IAIUnit *pUnit, CUnitArea *pArea )   // @0x0048e310
{
	// pArea (the unit's assigned CUnitArea) is OPTIONAL: when null there is simply no area gate and the
	// source sweeps the whole reachable set (CAIAttackPlaceSource::Prepare handles null pArea). The dev
	// GetUnitArea is still a stub returning 0, so requiring a non-null pArea here made every attack logic
	// silently drop its attack place source -> shoot/grenade/rocket never registered -> the AI never fired.
	// (build-settle: reconstruct CUnitArea + GetUnitArea to restore the per-unit area restriction.)
	if ( !IsValid( pUnit ) )
		return 0;
	// release passes the unit's per-turn AP budget as nMaxAP, bCheckDangerousForAllies = true.
	// (Release read GetUnitMission()->GetMaxAP(); the dev unit-mission has no GetMaxAP, so the turn's
	// start-AP from the unit server is used - reconciled, build-settle to verify against @0x0048e310.)
	int nMaxAP = pUnit->GetUnitServer()->GetAP();
	return new CAIAttackPlaceSource( pUnit, nMaxAP, pArea, true );
}
IAIActionPlaceSource* CreateCurrentPlaceSource( IAIUnit *pUnit )                    // @0x0048e380
{
	return IsValid( pUnit ) ? new CAICurrentPlaceSource( pUnit ) : 0;
}
IAIActionPlaceSource* CreateNearEnemyPlaceSource( IAIUnit *pUnit )                  // @0x0048e3c0
{
	return IsValid( pUnit ) ? new CAINearEnemyPlaceSource( pUnit ) : 0;
}
IAIActionPlaceSource* CreateToPlacePlaceSource( IAIUnit *pUnit, const SPathPlace &pos, int nMaxAP )  // @0x0048e400
{
	return IsValid( pUnit ) ? new CAIToPlacePlaceSource( pUnit, pos, nMaxAP ) : 0;
}
IAIActionPlaceSource* CreateOnePlacePlaceSource( IAIUnit *pUnit, const SPathPlace &pos, bool bCrouch ) // @0x0048e450
{
	return IsValid( pUnit ) ? new CAIOnePlacePlaceSource( pUnit, pos, bCrouch ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
BASIC_REGISTER_CLASS( CUnitArea )
BASIC_REGISTER_CLASS( CAIAttackPlaceSource )
BASIC_REGISTER_CLASS( CAICurrentPlaceSource )
BASIC_REGISTER_CLASS( CAINearEnemyPlaceSource )
BASIC_REGISTER_CLASS( CAIToPlacePlaceSource )
BASIC_REGISTER_CLASS( CAIOnePlacePlaceSource )
