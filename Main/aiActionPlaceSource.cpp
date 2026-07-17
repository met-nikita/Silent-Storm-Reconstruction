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
#include "aiRouteMisc.h"       // NAI::GetNearestPlaces (special-position neighbourhood search @0x8e980)
#include "wMainMoves.h"        // NWorld::GetMoveActionType (path-AP calcer)
#include "wMainPath.h"         // NWorld::FindPath / NWorld::PrepareAllPaths decls
#include "wUnitAttack.h"       // NWorld::GetMeleeAttackPlaces (the enemy melee ring)
#include "RPGGame.h"           // NRPG::GetShootDirection (face the candidate place at the enemy) @0x8e5e0
#include "aiMisc.h"            // NAI::GetAPForMove @0x74520 (price the held-spot move at CROUCH)
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
// of the centre (NWorld::PrepareAllPaths, as aiTaskCommander does) and keeps the normalized GetHash key of
// every kept place in the `places` hash set (release Prepare @0x74880: `places[GetHash(*i)] = 1`);
// IsInArea is the hash lookup @0x73e30. The set is SERIALIZED (release operator& @0x74e60 tag 6 =
// DoHashMap), so an area restored from a save gates exactly like the saved one without a re-flood.
// (The release Prepare's wishPose temporary pose override IS applied here -- see Prepare below, which
// forces the server into wishPose around the flood.)
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitArea::CUnitArea( NWorld::CUnitServer *_pUS, const SPathPlace &_place, int _nAPRadius, int _wishPose ): // @0x747f0
	pUS( _pUS ), place( _place ), nAPRadius( _nAPRadius ), wishPose( _wishPose )
{
	// @0x747f0 -- the release ctor does NOT flood here; the owner Prepares explicitly
	// (CAIGuardReaction::Update: `new CUnitArea(...); if ( !pNew->Prepare() ) pArea = 0;`). Flooding in the
	// ctor double-builds the set and (with the wishPose override below) would mutate the server pose during
	// construction. Leave `places` empty; the caller calls Prepare().
}
int CUnitArea::operator&( CStructureSaver &f )
{
	// release @0x74e60: 2 pUS, 3 place (raw 4), 4 nAPRadius, 5 wishPose, 6 places (DoHashMap<ulong,int>)
	f.Add( 2, &pUS ); f.Add( 3, &place ); f.Add( 4, &nAPRadius ); f.Add( 5, &wishPose );
	f.Add( 6, &places );
	return 0;
}
// CUnitArea::GetHash @0x73d60 -- the normalized place key (pose<<24 | layer<<16 | y<<8 | x). It strips the
// direction / moving / integral / final bits, so the SAME tile reached from any direction maps to ONE key.
// (NOT a real hash -- the release hash_map<ulong,int> hashes it again; it is the map key.)
// MISSING in the a5dll -- the old port keyed on raw GetData(), which kept those extra bits and fragmented the
// set so an in-area tile queried with a different direction wrongly missed the gate.
static inline unsigned long AreaHash( const SPathPlace &p )
{
	unsigned h = (unsigned)p.GetPose();
	h = ( h << 8 ) | (unsigned)p.GetLayer();
	h = ( h << 8 ) | (unsigned)p.GetY();
	h = ( h << 8 ) | (unsigned)p.GetX();
	return (unsigned long)h;
}
bool CUnitArea::Prepare()                                                    // @0x74880
{
	// @0x74880 -- the release does NOT clear `places` on the early-outs below: the clear happens only
	// after the flood, right before the fill loop (a failed re-Prepare keeps the previous set).
	if ( !IsValid( pUS ) || !IsValid( pUS->GetWorld() ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )   // @0x74880 -- release tests the dead-bit (+4 & 0x80000000) too, not just null
		return false;
	// @0x74880 -- bail before flooding if the centre place is CM_INACTIVE or hard-unpassable.
	// Release rejects ONLY GetPassability in {1,2} (NOT_PASSABLE/CANNOT_LAY); AIP_LOCKED(3)/AIP_DOOR(4)
	// PASS -- the guard's own post is locked by the unit ITSELF, so the strict IsPassable (==AIP_YES)
	// failed here every time and Prepare returned false -> a permanently inert guard reaction (the
	// EFirst Logic='Guard' enemies skipping every TB turn).
	if ( place.GetPose() == CM_INACTIVE )
		return false;
	EPassable ePassCentre = pNet->GetPassability( place );
	if ( ePassCentre == AIP_NOT_PASSABLE || ePassCentre == AIP_CANNOT_LAY )
		return false;
	// @0x74880 -- flood with the unit temporarily forced into wishPose; the release writes the server's
	// path-pose scratch (+0x28) and, for CRAWL, clears the run byte (+0x24). SetWishPose carries both in-tree
	// (same idiom CAINearEnemyPlaceSource::Prepare uses); restore after the flood.
	NAI::EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( (NAI::EPose)wishPose );
	CMultiMovesTable movesTable;
	list<SPathPlace>  reach;
	NWorld::PrepareAllPaths( pNet, &movesTable, &reach, pUS, place, nAPRadius, pUS, true );
	pUS->SetWishPose( nOldWish );
	// @0x74880 -- clear, then keep every reachable place whose pose is neither CM_INACTIVE(3) nor
	// CM_LAY(0); key on the normalized GetHash so direction/moving bits do not fragment the set
	// (release: `places[GetHash(*i)] = 1`).
	// ORIGINAL BUG (confirmed in the retail disasm @0x4749d2): the loop's passability call pushes
	// `lea eax,[esi+0x10]` = &this->place -- the CENTRE, not *i -- so the per-place passability filter
	// is inert (the centre already passed above). Reproduced verbatim, NOT corrected.
	places.clear();
	for ( list<SPathPlace>::const_iterator i = reach.begin(); i != reach.end(); ++i )
	{
		EPassable ePass = pNet->GetPassability( place );   // sic: the centre, not *i (retail @0x4749d2)
		if ( ePass == AIP_NOT_PASSABLE || ePass == AIP_CANNOT_LAY )
			continue;
		if ( (*i).GetPose() == CM_INACTIVE || (*i).GetPose() == CM_LAY )
			continue;
		places[ AreaHash( *i ) ] = 1;
	}
	return !places.empty();
}
bool CUnitArea::IsInArea( const SPathPlace &p ) const                        // @0x73e30
{
	// @0x73e30 -- a plain hash lookup, NO empty-set special case: an empty/un-prepared area matches
	// nothing (owners drop an area whose Prepare failed, and a loaded area restores its set from
	// operator& tag 6, so a live area is never empty). Key on the normalized GetHash (pose|layer|y|x),
	// NOT raw GetData(): the release hashes the same normalized key, so a tile queried with a different
	// direction/moving bit still matches its area entry.
	return places.find( AreaHash( p ) ) != places.end();
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
SAIState* CAIActionPlaceSource::GetAIState() const                       // @0x0048e080
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
// IPathNetwork exposes the same information as GetPassability(p) + GetWhoLocksThisPlace(p).
// ‼️ FIXED (2026-07-10): the old form checked the strict bool IsPassable(p) FIRST -- that is
// GetPassability(p)==AIP_YES only, and a tile LOCKED by the unit ITSELF reports AIP_LOCKED, so the
// unit's OWN place always failed BEFORE the own-lock exemption below could run. Result: every place
// source rejected the unit's own tile (the shoot action could never choose "stand still" -> the
// step-happy creep) and CAICurrentPlaceSource yielded ZERO places (reload/heal/loot/move-to-enemy/
// snipe all permanently bCanDo=false -- the dry-clip no-reload bug: CHOOSE act=3 places=0). Retail's
// GetPlaceState form passes owner-locked tiles (its lock verdict carries WHO, judged against self).
bool CAIActionPlaceSource::IsPassable( IPathNetwork *pNet, const SPathPlace &p )   // @0x0048e0b0
{
	if ( !IsValid( pNet ) )
		return false;
	EPassable e = pNet->GetPassability( p );
	if ( e == AIP_NOT_PASSABLE || e == AIP_CANNOT_LAY )
		return false;
	// AIP_LOCKED/AIP_DOOR fall through to the ownership test: only a FOREIGN lock rejects.
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
bool CAIActionPlaceSource::AddAllPoses( const SPathPlace &p, int nMoveAP, int nMaxAP, bool bArg )
{
	if ( !IsValid( pUnit ) || !IsValid( pUnit->GetUnitServer() ) )
		return false;
	CPtr<NWorld::CUnitServer> pUS  = pUnit->GetUnitServer();
	CPtr<IPathNetwork>        pNet = pUS->GetWorld()->GetPathNetwork();
	CPtr<NRPG::IUnitMission>  pRPG = pUS->GetUnitRPG();
	if ( !IsValid( pNet ) || !IsValid( pRPG ) )
		return false;
	//
	// @0x0048e5e0 - bArg (bFaceEnemy, passed true at every call site) orients each added place toward the
	// current enemy: the release sets the place's direction from NRPG::GetShootDirection before adding it,
	// so the stored SUnitPosition already aims at the enemy. GetUnitPos copies the place verbatim and
	// GetShootDirection quantizes only the tile->enemy vector (the from-place's own direction/pose are
	// irrelevant), so one direction serves both poses. Behaviour-neutral when there is no live enemy
	// (matches the release `if ( bFaceEnemy && enemy alive )` guard).
	bool           bFaced   = false;
	unsigned short nFaceDir = 0;
	if ( bArg )
	{
		IAIUnit *pEnemy = GetEnemy();
		if ( IsValid( pEnemy ) && IsValid( pEnemy->GetUnitServer() ) )
		{
			nFaceDir = ( unsigned short )NRPG::GetShootDirection( pNet, p, pEnemy->GetUnitPosition().GetCP() );
			bFaced   = true;
		}
	}
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
		if ( bFaced ) pp.SetDirection( nFaceDir );   // @0x0048e5e0 face the enemy before storing
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
		if ( bFaced ) pp.SetDirection( nFaceDir );   // @0x0048e5e0 face the enemy before storing
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
				// @0x0048f900: skip the unit's own tile (already added by (1)) and special/climbing-pose
				// tiles (nPose==3). The old `& 0xc000` mask never matched (GetPose() is a 2-bit field).
				if ( (*i).GetPose() != 3 &&
					!( (*i).GetX() == curPlace.GetX() && (*i).GetY() == curPlace.GetY() &&
					   (*i).GetLayer() == curPlace.GetLayer() ) &&
					( pArea == 0 || pArea->IsInArea( *i ) ) )    // area gate (pArea==0 -> no gate)
				{
					if ( !AddAllPoses( *i, calcer.GetResult(), nMaxAP, true ) )
						break;   // place no longer fully reachable within budget -> stop walking
				}
			}
		}
	}
	//
	// (3) @0x0048f900 — the FLANKING wave. The release floods the reachable set within only min(nMaxAP,8)
	// wave-AP (NOT the full nMaxAP) and keeps a place only when its direction from the unit is within
	// +-10 degrees of PERPENDICULAR to the unit->enemy axis (|cos| <= sin(10deg) = 0.1737, the load-bearing
	// constant @0x4b42b0): it offers FLANKING arcs, it does not blanket every reachable tile. Two release
	// pre-filters remain deferred to the unported weapon layer -- IsPosDangerousForAllies (friendly-fire,
	// conservative stub) and CanHitFromPlace (weapon range + sight FOV engage probe); omitting them only
	// WIDENS the set (the shoot action's GetInfoInner re-scores LOS/to-hit per place), so it stays
	// behaviour-safe, while the wave geometry + 8-AP budget are restored to match the release. pArea==0
	// (GetUnitArea stub) -> no area gate.
	if ( IsValid( pEnemy ) && IsValid( pEnemy->GetUnitServer() ) )
	{
		const int   nBudget = nMaxAP > 7 ? 8 : nMaxAP;                       // @0x0048f900 wave budget cap (8 AP)
		const CVec2 unitCP  = pU->GetUnitPosition().GetCPNoHeight();
		CVec2       dirUE   = pEnemy->GetUnitPosition().GetCPNoHeight() - unitCP;   // unit->enemy axis
		Normalize( &dirUE );
		NAI::CMultiMovesTable movesTable;
		list<SPathPlace>      reach;
		NWorld::PrepareAllPaths( pNet, &movesTable, &reach, pUS, curPlace, nBudget, pUS, true );
		for ( list<SPathPlace>::const_iterator i = reach.begin(); i != reach.end(); ++i )
		{
			if ( (*i).GetPose() == 3 )                                       // skip special/climbing-pose tiles
				continue;
			if ( (*i).GetX() == curPlace.GetX() && (*i).GetY() == curPlace.GetY() &&
				(*i).GetLayer() == curPlace.GetLayer() )                     // own tile already added by (1)
				continue;
			if ( !( pArea == 0 || pArea->IsInArea( *i ) ) )                  // area gate (pArea==0 -> no gate)
				continue;
			CVec2 dirUP = GetUnitPos( *i, pNet ).GetCPNoHeight() - unitCP;
			Normalize( &dirUP );
			const float fDot    = dirUP * dirUE;                            // cos(angle to the unit->enemy axis)
			const float fAbsDot = fDot < 0.0f ? -fDot : fDot;
			if ( fAbsDot > 0.1737f )                                         // keep only ~perpendicular (flanking)
				continue;
			AddAllPoses( *i, movesTable.GetCost( *i ), nMaxAP, true );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICurrentPlaceSource - just the unit's current place (all poses).
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x0048e980 - restored the special-position neighbourhood search (was the simplified single-AddAllPoses
// form). A unit on a NORMAL pose offers itself (cost 0, no AP cap). A unit on a special/INACTIVE pose
// (CM_INACTIVE == nPose 3: climbing / ladder / ...) instead floods the 15-AP neighbourhood and offers the
// CHEAPEST UNLOCKED place (with an inert facing bonus -- see the IsInUnitDir bug). All over the real types:
// GetNearestPlaces (aiRouteMisc), IPathNetwork::IsLocked, CMultiMovesTable::GetCost, in-file IsInUnitDir.
void CAICurrentPlaceSource::Prepare()                                   // @0x0048e980
{
	ClearPlaces();
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) || !IsValid( pU->GetUnitServer() ) || !IsValid( GetAIState() ) )
		return;
	CPtr<NWorld::CUnitServer> pUS = pU->GetUnitServer();
	SPathPlace cur = pU->GetPosition().p;
	if ( cur.GetPose() != NAI::CM_INACTIVE )
	{
		// normal position: offer itself, both poses, no AP cap.
		AddAllPoses( cur, 0, 0xffff, true );
		return;
	}
	// special/INACTIVE position (climbing etc.): search the 15-AP neighbourhood for the cheapest unlocked
	// place. Price the flood at WALK (the unit is in a PK) / RUN -- release IsUnitBusy == IsInPK, EPose 2/3.
	NAI::EPose nPose = IsValid( pUS->GetWearingDBPK() ) ? NAI::WALK : NAI::RUN;
	vector<SPathPlace> near_;
	CMultiMovesTable   table;
	GetNearestPlaces( pUS, cur, 15, nPose, &near_, &table );
	CPtr<IPathNetwork> pNet = pUS->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )
		return;
	// ORIGINAL BUG (confirmed in the retail disasm @0x0048e975 data): `int nBest = 1e10` truncates to
	// 0x540be400, and the final acceptance compares (double)nBest < 1e10 -- ALWAYS true for any int, so even
	// a search with NO candidates calls AddAllPoses on the invalid place 0xfdffffff (harmless: IsValidPoint
	// inside AddAllPoses rejects it). Reproduced verbatim, NOT corrected.
	int        nBest = 0x540be400;
	SPathPlace best( (int)0xfdffffff );
	for ( vector<SPathPlace>::const_iterator i = near_.begin(); i != near_.end(); ++i )
	{
		const SPathPlace &p = *i;
		if ( pNet->IsLocked( p, false ) )
			continue;
		int nCost = table.GetCost( p );
		if ( IsInUnitDir( pUS, p ) )                       // facing bonus (inert -- see IsInUnitDir bug)
			nCost = (int)( (double)nCost - 100000.0 );
		if ( nCost < nBest )
		{
			nBest = nCost;
			best  = p;
		}
	}
	if ( (double)nBest < 1.0e10 )                          // ORIGINAL BUG: always true
		AddAllPoses( best, nBest, 0xffff, true );
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
	// @0x0048ece0 -- pass the tail bools 0,0,0,1,1 (bCanFindNotExactPath + bIgnoreAllUnits), exactly as
	// the decode and the other two FindPath sites in this module do. The bare 5-arg form defaults
	// bCanFindNotExactPath/bIgnoreAllUnits to FALSE, so the melee approach only pathed when it could
	// EXACTLY reach the ring and let other units block it -- diverging from retail.
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, pU->GetPosition().p, targets, pEnemyUS,
		false, NAI::PF_DEFAULT, false, true, true );
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
	const SPathPlace selfPlace = pU->GetPosition().p;
	AddAllPoses( selfPlace, 0, nMaxAP, true );
	vector<SPathPlace> dest;
	dest.push_back( pos );
	// @0x0048f0e0 -- price the pathfind at WALK (in a PK) / RUN by swapping the server wish pose around
	// FindPath, exactly as CAINearEnemyPlaceSource::Prepare does (IsUnitBusy == IsInPK).
	// ORIGINAL BUG (confirmed @0x0048f0e0): the wish pose is restored ONLY on the success path -- the
	// no-path early return below leaves it clobbered. Reproduced verbatim, NOT corrected.
	NAI::EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( IsValid( pUS->GetWearingDBPK() ) ? NAI::WALK : NAI::RUN );
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pNet, pUS, selfPlace, dest, pUS,
		false, NAI::PF_DEFAULT, false, true, true );   // release uses PF_USE_DIR
	if ( !IsValid( pPath ) )
		return;   // ORIGINAL BUG (@0x0048f0e0): wish pose intentionally NOT restored on this early return
	CPathAPCalcerLocal calcer( pWorld, pRPG, pU->GetUnitPosition(), false );
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
	{
		calcer.AddPoint( *i );
		if ( calcer.GetResult() > nMaxAP )
			break;
		// @0x0048f0e0 -- skip the unit's OWN tile (already offered above; release masks pose/dir/moving via
		// IsSamePlace mask 0x1feffff) and CM_INACTIVE tiles (release: nPose != 3). The prior
		// `GetPose() & 0xc000` was a no-op: GetPose() is the 2-bit nPose, so the skip never fired.
		if ( ( ( (*i).GetData() ^ selfPlace.GetData() ) & 0x01feffff ) != 0 &&
			(*i).GetPose() != NAI::CM_INACTIVE )
			AddAllPoses( *i, calcer.GetResult(), nMaxAP, true );
	}
	pUS->SetWishPose( nOldWish );   // restored only here on success (the early returns above leave it clobbered)
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
	// @0x004901f0 -- price the move to the held spot at CROUCH pose via NAI::GetAPForMove (path AP from the
	// unit's current place to `place`), as the release does, instead of assuming 0. For a defence source
	// already standing on the spot this is 0 (exact); otherwise it debits the travel. GetAPForMove returns
	// 0xffff when the spot is unreachable -> the stand branch then offers nothing (faithful: nWalk =
	// poseAP + 0xffff exceeds the 0xffff cap, so AddAllPoses adds no place) and the crouch branch stores a
	// negative AP-left (faithful: the choose-place job rejects it on affordability).
	const int nMoveAP = GetAPForMove( GetUnit(), GetUnit()->GetUnitPosition().pos.p, place, NAI::CROUCH );
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
	// @0x0048e310 reads nMaxAP = the unit's MAX AP skill (its full AP bar, release GetUnitMission()->GetMaxAP()),
	// NOT the current remaining AP -- using GetUnitServer()->GetAP() shrank the attack-place budget as the unit
	// spent AP during its turn. IAIUnit::GetMaxAP() is the in-tree equivalent of the AP-skill max.
	// bCheckDangerousForAllies = true.
	int nMaxAP = pUnit->GetMaxAP();
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
REGISTER_SAVELOAD_CLASS( 0x50843120, CUnitArea )
REGISTER_SAVELOAD_CLASS( 0x51833130, CAIAttackPlaceSource )
REGISTER_SAVELOAD_CLASS( 0x51833131, CAICurrentPlaceSource )
REGISTER_SAVELOAD_CLASS( 0x52533190, CAINearEnemyPlaceSource )
REGISTER_SAVELOAD_CLASS( 0x52443140, CAIToPlacePlaceSource )
REGISTER_SAVELOAD_CLASS( 0x2306BC80, CAIOnePlacePlaceSource )
