#include "StdAfx.h"
//
#include "wUnitServer.h"     // NWorld::CUnitServer (+ CUnit base), GetWishPose/SetWishPose/SetStrafe
#include "RPGUnitMission.h"  // NRPG::IUnitMission (GetUnitRPG()->GetRPGUnit())
#include "RPGUnit.h"         // NRPG::CUnit::GetSightFOV (@0x2ba6c0) for the retail sight cone
#include "wMain.h"           // NWorld::CWorld::GetPathNetwork / GetGame (GetCheckPosition)
#include "wMainPath.h"       // NWorld::PrepareAllPaths
#include "aiMultiMoves.h"    // NAI::CMultiMovesTable (CPathPlaceTable::GetCost)
#include "aiTaskCommand.h" // NAI::CTaskCommand family (ChangePose / ChangeWishPose / ChangeDirection / Wait / Roaming)
#include "wUnitCommands.h"   // NWorld::CCmd complete (CTaskCommand::operator& serializes list<CPtr<CCmd>>)
#include "aiUnit.h"          // NAI::IAIUnit (GetUnitServer / GetUnitPosition / GetAP) -- GetCheckPosition
#include "RPGGame.h"         // NRPG::IGame::CheckPositionVisibility (GetCheckPosition)
#include "aiActionPlaceSource.h" // NAI::CUnitArea::IsInArea (GetCheckPosition)
#include "aiActionBase.h"    // NAI::SPlaceWithAP (complete) -- before aiMoveAction.h (C2036 guard)
#include "aiMoveAction.h"    // NAI::GetUnitPos / NAI::GetPos (place -> SUnitPosition / SPosition)
#include "aiPath.h"          // NAI::CPath (IsNear over a path / GetRestoreAPPoint)
#include "aiRouteLogic.h"    // NAI::CTaskCommandLookToPosition (RouteAddRoundUp's CreateRCLookToPos)
#include "RPGGlobal.h"       // NRPG::CGlobalGame::pDifficulty (RouteAddRoundUp avoid-friends gate)
#include "../DBFormat/DataDifficulty.h" // NDb::CDBDifficulty::bAICheckCorpses
#include "../DBFormat/DataMap.h"        // NDb::DS_ALLY (GetRoundUpPlaces friend diplomacy gate)
//
#include "aiRouteMisc.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetNearestPlaces -- the reachable-places flood. Reconstructed from the matched-release decode
// (s2_routemisc.h @0xa05d0), wired onto the in-tree path-wave machinery. See aiRouteMisc.h for the two
// release-vs-dev faithfulness refinements (bNoDynamicLocks, EPassable-vs-IsPassable).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
void GetNearestPlaces( NWorld::CUnitServer *pServer, const SPathPlace &place, int nMaxCost, EPose nPose,
	vector<SPathPlace> *pRes, CMultiMovesTable *pTable )
{
	pRes->clear();
	if ( !IsValid( pServer ) )
		return;
	// price the whole flood at the requested pose: swap the wish pose (writing CRAWL clears the run byte).
	EPose nOldWish = pServer->GetWishPose();
	pServer->SetWishPose( nPose );
	if ( nPose == CRAWL )
		pServer->SetStrafe( false );
	{
		list<SPathPlace> places;
		CMultiMovesTable localTable;
		IPathNetwork *pNet = pServer->GetWorld()->GetPathNetwork();
		CMultiMovesTable *pT = pTable ? pTable : &localTable;
		NWorld::CUnit *pU = static_cast<NWorld::CUnit*>( pServer );   // the server's unit component
		// retail @0xa05d0 (disasm-verified, both refinements were flagged in aiRouteMisc.h):
		//  * bNoDynamicLocks = TRUE -- the wave IGNORES transient unit locks (dev's account-units gather
		//    pruned places another unit stood on, shrinking the cover-wave target set that GetCoveredPosition
		//    feeds FindPath -- a different chosen cover flips the Defence-entry verdict);
		//  * keep a reached place unless GetPassability says AIP_NOT_PASSABLE or AIP_CANNOT_LAY (locked and
		//    door places PASS as targets; dev's IsPassable == AIP_YES-only was stricter).
		NWorld::PrepareAllPaths( pNet, pT, &places, pU, place, nMaxCost, pU, false, /*bNoDynamicLocks*/ true );
		for ( list<SPathPlace>::iterator it = places.begin(); it != places.end(); ++it )
		{
			unsigned short nP = it->GetPose();
			if ( nP == 3 || nP == 0 )      // drop the CM_INACTIVE(3) and CM_LAY(0) check-poses
				continue;
			EPassable e = pNet->GetPassability( *it );
			if ( e != AIP_NOT_PASSABLE && e != AIP_CANNOT_LAY )
				pRes->push_back( *it );
		}
	}
	pServer->SetWishPose( nOldWish );
	if ( nOldWish == CRAWL )
		pServer->SetStrafe( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddLookAround @0xa2180 -- see aiRouteMisc.h. The random draws map EXACTLY to the release: the
// decode draws a raw ISAAC word and takes `% n`, which is precisely random.Get(n) (CRandomGenerator::Get(n)
// == Get() % n, one word per draw). ORIGINAL QUIRK reproduced: random.Get(7) yields 0..6 so the NW spoke
// (direction 7) is never drawn. Glance order is Look-then-Wait (the release order; the dev predecessor
// CTask::AddLookAround uses Wait-then-Direction with a different random.Get(3,6) count -- not reused).
////////////////////////////////////////////////////////////////////////////////////////////////////
void RouteAddLookAround( bool bAddPose, vector< CPtr<CTaskCommand> > &cmds, int nCount )
{
	if ( bAddPose )
		cmds.push_back( new CTaskCommandChangePose( WALK ) );
	int nHi = nCount + 1;
	if ( nHi < 1 )
		nHi = 1;
	int nLo = nCount - 2;
	if ( nLo < 0 )
		nLo = 0;
	unsigned nSpan = (unsigned)( nHi - nLo );
	int n = nLo + ( nSpan != 0 ? (int)random.Get( nSpan ) : 0 );
	if ( n < 1 )
		n = 1;
	for ( ; n > 0; --n )
	{
		cmds.push_back( new CTaskCommandChangeDirection( (int)random.Get( 7 ) ) );
		cmds.push_back( new CTaskCommandWait( (int)random.Get( 5 ) + 1 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddRoaming @0xa2420 -- see aiRouteMisc.h.
////////////////////////////////////////////////////////////////////////////////////////////////////
void RouteAddRoaming( const SPathPlace &center, int nRadius, bool bAddPose, vector< CPtr<CTaskCommand> > &cmds )
{
	cmds.push_back( new CTaskCommandRoaming( center, nRadius ) );
	RouteAddLookAround( bAddPose, cmds, 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddGoAndCheck @0xa24e0 -- see aiRouteMisc.h. Face the position, change the WISH pose to RUN/WALK
// (bRun) for the move (retail CreateRCChangeWishPose -- @0xa24e0 is one of its four callers), go there
// (release strafe arg elided), optional full look-around (bAddPose=true, nCount=6), settle at WALK
// (CreateRCChangePose). Command order + the nCount=6 / bAddPose=true look-around args are disasm-verified.
void RouteAddGoAndCheck( const SPosition &pos, bool bLookAround, bool bRun, vector< CPtr<CTaskCommand> > &cmds )
{
	cmds.push_back( new CTaskCommandLookToPosition( pos.p ) );
	cmds.push_back( new CTaskCommandChangeWishPose( bRun ? RUN : WALK ) );
	cmds.push_back( new CTaskCommandGoto( pos ) );
	if ( bLookAround )
		RouteAddLookAround( true, cmds, 6 );
	cmds.push_back( new CTaskCommandChangePose( WALK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddWalkToPosition @0xa2f20 -- see aiRouteMisc.h. Face it, WALK, go (release strafe arg elided).
void RouteAddWalkToPosition( const SPosition &pos, vector< CPtr<CTaskCommand> > &cmds )
{
	cmds.push_back( new CTaskCommandLookToPosition( pos.p ) );
	cmds.push_back( new CTaskCommandChangePose( WALK ) );
	cmds.push_back( new CTaskCommandGoto( pos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetCheckPosition @0xa1000 -- see aiRouteMisc.h. Reconstructed from the matched-release decode
// (s2_routemisc.h) + disasm-verified (the budget stats chain, the strict-greater max-cost selection, the
// direction-fold on the result, and the absence of GetSafePosition's 28-AP floor were all confirmed in the
// disasm). Seams resolved to the in-tree calls already used by aiDefenceReaction/aiSnipeAction.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetCheckPosition( IAIUnit *pUnit, CUnitArea *pArea, const SPosition &pos, SPathPlace *pResult )
{
	if ( !IsValid( pArea ) )                 // dead-area guard (CObjectBase null + bit31)
		return false;
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) || !IsValid( pUS->GetWorld() ) )
		return false;
	IPathNetwork *pNet  = pUS->GetWorld()->GetPathNetwork();
	NRPG::IGame  *pGame = pUS->GetWorld()->GetGame();
	if ( pNet == 0 || pGame == 0 )
		return false;
	// retail @0xa1000 hoists the observer's real range/FOV once before the loop (game vtbl+0x34
	// GetUnitSightDistance + rpg GetSightFOV @0x2ba6c0) and probes with the 4-arg vtbl+0x1c.
	NRPG::CUnit *pRPG = pUnit->GetRPGUnit();
	float fRange = pGame->GetUnitSightDistance( pRPG );
	float fFOV = IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI;
	// flood the reachable places at RUN within the unit's current AP (NO 28 floor -- faithful to @0xa1000),
	// keeping our own moves table to read each place's accumulated wave cost.
	vector<SPathPlace> places;
	CMultiMovesTable table;
	GetNearestPlaces( pUS, pUnit->GetUnitPosition().pos.p, pUnit->GetAP(), RUN, &places, &table );
	bool bFound = false;
	int nBest = 0;   // the binary inits 0x100000, dead before first use (the !bFound term guards it)
	for ( vector<SPathPlace>::iterator it = places.begin(); it != places.end(); ++it )
	{
		if ( !pArea->IsInArea( *it ) )       // release IsInArea @0x73e30: hash lookup (empty set matches nothing)
			continue;
		SUnitPosition cand = GetUnitPos( *it, pNet );
		cand.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( cand.pos.p, pos.p ) & 7 ) );
		if ( !pGame->CheckPositionVisibility( cand, pos, fRange, fFOV ) )   // retail 4-arg @0x298fa0
			continue;
		int nCost = (int)table.GetCost( *it );
		if ( !bFound || nBest < nCost )      // strict-greater: ties keep the earlier place
		{
			*pResult = cand.pos.p;
			nBest = nCost;
			bFound = true;
		}
	}
	return bFound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The geometry bedrock (oracle: s2_routemisc.h slice 2). All disasm-verified; every reach maps to an
// in-tree call the landed legs already use (no pfn hooks). See aiRouteMisc.h for the shared range/FOV
// elision on the 2-arg CheckPositionVisibility. CVec3 uses the .u/.v/.q union aliases (Geom.h).
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsNear @0x9fe30 -- squared-vs-squared distance compare (no sqrt), early-out on the first hit.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsNear( const SPosition &pos, const vector<CVec3> &points, float fRadius )
{
	CVec3 cp = pos.GetCP();
	for ( int i = 0; i < (int)points.size(); ++i )
	{
		float du = cp.u - points[i].u;
		float dv = cp.v - points[i].v;
		float dq = cp.q - points[i].q;
		if ( du * du + dv * dv + dq * dq < fRadius * fRadius )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsNear @0xa0080 -- any point of the path near any of the points.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsNear( IPathNetwork *pNet, const CPath &path, const vector<CVec3> &points, float fRadius )
{
	for ( int i = 0; i < (int)path.points.size(); ++i )
	{
		SPosition pos = GetPos( path.points[i], pNet );
		if ( IsNear( pos, points, fRadius ) )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetDirectedPos @0xa0130 -- the place's unit position turned toward `target`. GetClosestDir resolves
// to the (SPathPlace,SPathPlace) overload (aiPosition.h:310); the result is folded &7 into nDirection.
////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitPosition GetDirectedPos( const SPathPlace &place, const SPathPlace &target, IPathNetwork *pNet )
{
	SUnitPosition res = GetUnitPos( place, pNet );
	res.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( place, target ) & 7 ) );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetAvgPos @0x9fed0 -- distance-weighted average direction toward the READY crowd (CanFight) within
// 15m. Constants 15.0 / 16.666666 / 1.6666666 disasm-verified; Normalize is Geom.h:143 (bool, ignored).
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 GetAvgPos( const vector< CPtr<IAIUnit> > &units, const CVec3 &cp )
{
	CVec3 res;
	res.u = res.v = res.q = 0.0f;
	for ( int i = 0; i < (int)units.size(); ++i )
	{
		IAIUnit *pU = units[i].GetPtr();
		if ( !IsValid( pU ) )
			continue;
		NWorld::CUnitServer *pUS = pU->GetUnitServer();
		if ( !IsValid( pUS ) || !pUS->CanFight() )   // the server vtbl+0x44 "ready" gate
			continue;
		CVec3 cu = pUS->GetPosition().pos.GetCP();
		CVec3 d;
		d.u = cu.u - cp.u;
		d.v = cu.v - cp.v;
		d.q = cu.q - cp.q;
		float fDist = sqrtf( d.u * d.u + d.v * d.v + d.q * d.q );
		if ( fDist > 15.0f )
			continue;
		float fW = 16.666666f / ( fDist + 1.6666666f );
		Normalize( &d );
		res.u += d.u * fW;
		res.v += d.v * fW;
		res.q += d.q * fW;
	}
	float fLenSq = res.u * res.u + res.v * res.v + res.q * res.q;
	if ( fLenSq != 0.0f )
	{
		float fInv = 1.0f / sqrtf( fLenSq );
		res.u *= fInv;
		res.v *= fInv;
		res.q *= fInv;
	}
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetPlacesAtDirection @0xa0800 -- the forward-cone filter over the reachable flood. The binary folds
// the dot v+q first then u; the empty-cone fallback returns the single farthest in-band place (FIRST on
// ties), seeded from the 0xfdffffff sentinel via the SPathPlace(int) ctor (nData is private).
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetPlacesAtDirection( NWorld::CUnitServer *pServer, const SPathPlace &place, CVec3 vDir, int nSpread,
	float fMinDist, float fMaxDist, int nMaxCost, EPose nPose, vector<SPathPlace> *pRes )
{
	pRes->clear();
	if ( !IsValid( pServer ) )
		return;
	IPathNetwork *pNet = pServer->GetWorld()->GetPathNetwork();
	vector<SPathPlace> places;
	GetNearestPlaces( pServer, place, nMaxCost, nPose, &places, 0 );
	CVec3 cpSelf = GetUnitPos( place, pNet ).pos.GetCP();
	SPathPlace best( (int)0xfdffffff );
	float fBest = 0.0f;
	for ( int i = 0; i < (int)places.size(); ++i )
	{
		CVec3 cp = GetUnitPos( places[i], pNet ).pos.GetCP();
		CVec3 d;
		d.u = cp.u - cpSelf.u;
		d.v = cp.v - cpSelf.v;
		d.q = cp.q - cpSelf.q;
		float fDot = vDir.v * d.v + vDir.q * d.q + vDir.u * d.u;
		if ( fDot < 0.0f )
			continue;
		if ( fDot < fMinDist )
			continue;
		if ( fDot > fMaxDist )
			continue;
		if ( fBest < fDot )
		{
			fBest = fDot;
			best = places[i];
		}
		CVec3 perp;
		perp.u = d.u - vDir.u * fDot;
		perp.v = d.v - vDir.v * fDot;
		perp.q = d.q - vDir.q * fDot;
		float fPerp = sqrtf( perp.u * perp.u + perp.v * perp.v + perp.q * perp.q );
		if ( (float)nSpread * fDot * 0.01f > fPerp )
			pRes->push_back( places[i] );
	}
	if ( pRes->empty() && fBest > 0.0f )
		pRes->push_back( best );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetFearPosition @0xa0b20 -- flee the crowd. Band upper = 4095.0f (disasm 0x457ff000 -- the answer-
// key comment's 4095.5 was wrong); one cone place picked with a raw ISAAC draw (random.Get(size)). The
// unit position is read twice, faithful to the binary.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetFearPosition( NWorld::CUnitServer *pServer, const vector< CPtr<IAIUnit> > &units, SPathPlace *pResult,
	float fMinDist, int nMaxCost )
{
	CVec3 cp = pServer->GetPosition().pos.GetCP();
	CVec3 vDir = GetAvgPos( units, cp );
	vDir.u = -vDir.u;
	vDir.v = -vDir.v;
	vDir.q = -vDir.q;
	vector<SPathPlace> res;
	GetPlacesAtDirection( pServer, pServer->GetPosition().pos.p, vDir, 40, fMinDist, 4095.0f, nMaxCost,
		RUN, &res );
	if ( res.empty() )
		return false;
	*pResult = res[ random.Get( (unsigned)res.size() ) ];
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetSafePosition @0xa0ca0 -- the reachable RUN flood (AP budget clamped UP to >=28) minus every place
// any live unit can SEE. Budget = pServer->GetAP() (current AP; disasm-confirmed -- NOT GetAPForMove). The
// first survivor in wave order wins (no random pick).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetSafePosition( NWorld::CUnitServer *pServer, const vector< CPtr<IAIUnit> > &units, SPathPlace *pResult )
{
	if ( !IsValid( pServer ) )
		return false;
	IPathNetwork *pNet = pServer->GetWorld()->GetPathNetwork();
	int nBudget = pServer->GetAP();
	if ( nBudget <= 28 )       // disasm-confirmed AP floor (cmp eax,0x1c; mov eax,0x1c)
		nBudget = 28;
	vector<SPathPlace> safe;
	GetNearestPlaces( pServer, pServer->GetPosition().pos.p, nBudget, RUN, &safe, 0 );
	for ( int i = 0; i < (int)units.size(); ++i )
	{
		IAIUnit *pU = units[i].GetPtr();
		if ( !IsValid( pU ) )
			continue;
		NWorld::CUnitServer *pSeer = pU->GetUnitServer();
		if ( !IsValid( pSeer ) )
			continue;
		NRPG::IGame *pGame = pSeer->GetWorld()->GetGame();
		SUnitPosition seer = pSeer->GetPosition();
		// retail @0xa0ca0 recomputes the seer's range/FOV per seer (game vtbl+0x34 + GetSightFOV @0x2ba6c0)
		NRPG::CUnit *pRPG = pU->GetRPGUnit();
		float fRange = pGame->GetUnitSightDistance( pRPG );
		float fFOV = IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI;
		for ( int j = 0; j < (int)safe.size(); )
		{
			SPosition cand = GetPos( safe[j], pNet );
			seer.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( seer.pos.p, safe[j] ) & 7 ) );
			if ( pGame->CheckPositionVisibility( seer, cand, fRange, fFOV ) )   // retail 4-arg @0x298fa0
				safe.erase( safe.begin() + j );
			else
				++j;
		}
	}
	if ( safe.empty() )
		return false;
	*pResult = safe[0];
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The masked place-equality used by GetRestoreAPPoint's backward-walk de-dup (mask 0x1feffff). The dev tree
// has no shared masked IsSamePlace (only a file-local one in UnitTracker.cpp / aiDefenceReaction.cpp) -- so
// inline the same XOR-and-mask compare.
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline bool SamePlaceMasked( const SPathPlace &a, const SPathPlace &b )
{
	return ( ( a.GetData() ^ b.GetData() ) & 0x1feffff ) == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetRestoreAPPoint @0xa01e0 -- walk the from->target path backward and return the first point pEnemy
// can NOT see. FindPath flags disasm-confirmed (false / PF_DEFAULT / false / false / true); pIgnore is the
// unit ITSELF (not null) and bIgnoreAllUnits=true. No original bug -- the empty/invalid-path guard is
// present (the counter-example aiDefenceReaction's GetCoveredPosition bug note cites).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetRestoreAPPoint( NWorld::CUnitServer *pServer, NWorld::CUnitServer *pEnemy, const SPosition &from,
	const SPosition &target, SPosition *pResult, int *pnVisible )
{
	if ( !IsValid( pServer ) )
		return false;
	if ( !IsValid( pEnemy ) )
		return false;
	IPathNetwork *pNet = pServer->GetWorld()->GetPathNetwork();
	if ( pNet == 0 )
		return false;
	vector<SPathPlace> targets;
	targets.push_back( target.p );
	NWorld::CUnit *pU = static_cast<NWorld::CUnit*>( pServer );  // both FindPath CUnit args are self
	CPtr<CPath> path( NWorld::FindPath( pNet, pU, from.p, targets, pU, false, NAI::PF_DEFAULT, false,
		false, true ) );
	if ( !IsValid( path ) || path->points.empty() )
		return false;
	NRPG::IGame *pGame = pServer->GetWorld()->GetGame();
	// retail @0xa01e0: observer = pEnemy; his range/FOV hoisted once before the backward walk
	NRPG::CUnit *pRPG = IsValid( pEnemy->GetUnitRPG() ) ? pEnemy->GetUnitRPG()->GetRPGUnit() : 0;
	float fRange = pGame->GetUnitSightDistance( pRPG );
	float fFOV = IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI;
	*pnVisible = 0;
	int nLast = (int)path->points.size() - 1;
	for ( int i = nLast; i >= 0; --i )
	{
		if ( i == nLast || !SamePlaceMasked( path->points[i], path->points[i + 1] ) )
		{
			SPosition pos = GetPos( path->points[i], pNet );
			SUnitPosition seer = GetDirectedPos( pEnemy->GetPosition().pos.p, pos.p, pNet );
			if ( !pGame->CheckPositionVisibility( seer, pos, fRange, fFOV ) )   // retail 4-arg @0x298fa0
			{
				*pResult = pos;
				return true;
			}
			++*pnVisible;
		}
		if ( *pnVisible >= 20 )
			return false;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetRoundUpPlaces @0xa1320 -- see aiRouteMisc.h for the full contract, the GetWearingDBPK mislabel
// correction, the ORIGINAL BUG, and the control-point elision. All floats verified against Game.exe raw bytes
// (1.6=0x3fcccccd, 8.0=0x41000000, 0.33333334=0x3eaaaaab, 0.5=0x3f000000, 5.0=0x40a00000, 4.0=0x40800000).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetRoundUpPlaces( NWorld::CUnitServer *pServer, const SUnitPosition &enemyPos, SPosition *pStop,
	SPosition *pFlank, SPosition *pAttack, bool bCheckFriends )
{
	if ( !IsValid( pServer ) )
		return false;
	SUnitPosition ownPos = pServer->GetPosition();
	CVec3 cpOwn   = ownPos.pos.GetCP();
	CVec3 cpEnemy = enemyPos.pos.GetCP();
	CVec3 d;
	d.u = cpEnemy.u - cpOwn.u;
	d.v = cpEnemy.v - cpOwn.v;
	d.q = cpEnemy.q - cpOwn.q;
	float fDist = sqrtf( d.u * d.u + d.v * d.v + d.q * d.q );
	// primary vtbl+0x34 == GetWearingDBPK (NOT an attack target): a panzerklein-wearing unit moves at WALK,
	// others RUN (the same gate as aiActionPlaceSource.cpp:318). See aiRouteMisc.h for the mislabel note.
	EPose nPose = IsValid( pServer->GetWearingDBPK() ) ? WALK : RUN;
	vector<CVec3> points;
	if ( bCheckFriends )
	{
		CVec3 mid;
		mid.u = ( cpOwn.u + cpEnemy.u ) * 0.5f;
		mid.v = ( cpOwn.v + cpEnemy.v ) * 0.5f;
		mid.q = ( cpOwn.q + cpEnemy.q ) * 0.5f;
		list< CPtr<NWorld::CUnitServer> > others;
		pServer->GetWorld()->GetUnitsNear( mid, &others, fDist * 0.5f + 5.0f );
		for ( list< CPtr<NWorld::CUnitServer> >::iterator it = others.begin(); it != others.end(); ++it )
		{
			NWorld::CUnitServer *pOther = it->GetPtr();
			if ( !IsValid( pOther ) )
				continue;
			if ( pOther->CanFight() )                                  // skip combat-capable: only DOWNED allies are avoided
				continue;
			if ( pServer->GetDiplomacyState( pOther ) != NDb::DS_ALLY )
				continue;
			// ELIDED control-point vector (+0x14c posComp CP[0] has no dev accessor) -> the unit centre
			// approximates CP[0] (sub-metre; only feeds the 4m friend-avoidance IsNear below).
			points.push_back( pOther->GetPosition().GetCenter() );
		}
	}
	if ( fDist < 8.0f )
	{
		*pFlank  = ownPos.pos;
		*pStop   = *pFlank;
		*pAttack = enemyPos.pos;
		if ( bCheckFriends &&
			 ( IsNear( *pFlank, points, 4.0f ) || IsNear( *pStop, points, 4.0f ) ||
			   IsNear( *pAttack, points, 4.0f ) ) )
			return true;                                              // allies already crowd a spot -> accept the defaults
	}
	IPathNetwork *pNet  = pServer->GetWorld()->GetPathNetwork();
	NRPG::IGame  *pGame = pServer->GetWorld()->GetGame();
	// retail @0xa1320: observer = pServer; range/FOV hoisted once before the candidate loop
	NRPG::CUnit *pRPG = IsValid( pServer->GetUnitRPG() ) ? pServer->GetUnitRPG()->GetRPGUnit() : 0;
	float fRange = pGame->GetUnitSightDistance( pRPG );
	float fFOV = IsValid( pRPG ) ? pRPG->GetSightFOV() : FP_2PI;
	int nBudget    = (int)( fDist * 1.6f );
	int nBudgetMid = nBudget;
	if ( nBudget > 20 )
		nBudget = 20;
	if ( nBudgetMid > 25 )
		nBudgetMid = 25;
	// the ATTACK spot: a reachable place near the enemy (turned toward him) from which he is visible.
	vector<SPathPlace> cand;
	GetNearestPlaces( pServer, enemyPos.pos.p, nBudget, nPose, &cand, 0 );
	// the binary re-probes GetWearingDBPK (identical result) for the check pose: STAND(2) for a PK, else CROUCH(1).
	int nCheckPose = IsValid( pServer->GetWearingDBPK() ) ? 2 : 1;
	bool bGot = false;
	while ( !cand.empty() )
	{
		int i = (int)( random.Get( (unsigned)cand.size() ) );
		SUnitPosition up = GetUnitPos( cand[i], pNet );
		up.pos.p.SetDirection( (unsigned short)( (int)pNet->GetClosestDir( up.pos.p, enemyPos.pos.p ) & 7 ) );
		if ( (int)up.pos.p.GetPose() == nCheckPose &&
			 pGame->CheckPositionVisibility( up, enemyPos.pos, fRange, fFOV ) )   // retail 4-arg @0x298fa0
		{
			*pAttack = up.pos;
			bGot = true;
			break;
		}
		cand.erase( cand.begin() + i );
	}
	if ( !bGot )
		return false;
	NWorld::CUnit *pU = static_cast<NWorld::CUnit*>( pServer );       // pIgnore = self (both CUnit args)
	vector<SPathPlace> targets;
	targets.push_back( pAttack->p );
	CPtr<CPath> path( NWorld::FindPath( pNet, pU, ownPos.pos.p, targets, pU, false, NAI::PF_DEFAULT, false,
		false, true ) );
	if ( !IsValid( path ) || path->points.empty() )
		return false;
	int nMid = (int)( (float)path->points.size() * 0.33333334f );
	if ( nMid < 0 )
		nMid = 0;
	SPathPlace midPlace = path->points[nMid];
	CVec3 n = d;
	Normalize( &n );
	CVec3 cpAtk = pAttack->GetCP();
	CVec3 e;
	e.u = cpAtk.u - cpOwn.u;
	e.v = cpAtk.v - cpOwn.v;
	e.q = cpAtk.q - cpOwn.q;
	// ORIGINAL BUG (confirmed @0x4a1aa1-0x4a1b0a, fmul @0x4a1ab9): the u-term multiplies by cpOwn.u (the
	// unit's own world u-coordinate) instead of n.u -- so `perp` is not the true perpendicular component.
	// (The decomp answer-key miscopies this as an unscaled u-term; the raw bytes are authoritative.)
	float fS = n.q * e.q + n.v * e.v + cpOwn.u * e.u;
	CVec3 perp;
	perp.u = e.u - cpOwn.u * fS;
	perp.v = e.v - n.v * fS;
	perp.q = e.q - n.q * fS;
	// the FLANK spot: a reachable place off the path's 1/3 waypoint, on the attack side of the own->enemy axis.
	cand.clear();
	GetNearestPlaces( pServer, midPlace, nBudgetMid, nPose, &cand, 0 );
	bGot = false;
	while ( !cand.empty() )
	{
		int i = (int)( random.Get( (unsigned)cand.size() ) );
		SUnitPosition up = GetUnitPos( cand[i], pNet );
		CVec3 cp = up.pos.GetCP();
		if ( perp.u * ( cp.u - cpOwn.u ) + perp.v * ( cp.v - cpOwn.v ) + perp.q * ( cp.q - cpOwn.q ) > 0.0f )
		{
			*pFlank = up.pos;
			bGot = true;
			break;
		}
		cand.erase( cand.begin() + i );
	}
	if ( !bGot )
		return false;
	// the STOP spot: the first ladder/door transition (CM_INACTIVE) of the own->flank path, else the flank.
	targets.clear();
	targets.push_back( pFlank->p );
	path = CPtr<CPath>( NWorld::FindPath( pNet, pU, ownPos.pos.p, targets, pU, false, NAI::PF_DEFAULT, false,
		false, true ) );
	if ( !IsValid( path ) || path->points.empty() )
		return false;
	*pStop = *pFlank;
	for ( int i = 0; i < (int)path->points.size(); ++i )
	{
		if ( (int)path->points[i].GetPose() == 3 )                   // CM_INACTIVE -- a transition waypoint
		{
			*pStop = GetPos( path->points[i], pNet );
			break;
		}
	}
	if ( bCheckFriends )
	{
		if ( IsNear( pNet, *path, points, 4.0f ) )                   // own->flank path passes a downed ally
			return false;
		SPathPlace lastPlace = path->points.back();
		targets.clear();
		targets.push_back( pAttack->p );
		path = CPtr<CPath>( NWorld::FindPath( pNet, pU, lastPlace, targets, pU, false, NAI::PF_DEFAULT, false,
			false, true ) );
		if ( !IsValid( path ) || path->points.empty() )
			return false;
		if ( IsNear( pNet, *path, points, 4.0f ) )                   // flank->attack path passes a downed ally
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::RouteAddRoundUp @0xa2720 -- see aiRouteMisc.h. Reuses the CTaskCommand family (CreateRC* factories ->
// new CTaskCommand*). Retail's pose steps are matched exactly: the nWish step before the stop-Goto is
// CreateRCChangeWishPose (@0xa2720 is one of its four callers); the nPoseLow settle step and the final WALK
// are CreateRCChangePose. The final Goto's "looked" flag is elided with the strafe arg (the Look command it
// gates is still queued).
////////////////////////////////////////////////////////////////////////////////////////////////////
void RouteAddRoundUp( NWorld::CUnitServer *pServer, NWorld::CUnitServer *pEnemy, const SUnitPosition &enemyPos,
	EPose nPose, vector< CPtr<CTaskCommand> > &cmds, bool bCheck )
{
	if ( !IsValid( pServer ) )
		return;
	// avoid-friends gate: the bCheck arg ANDed with the difficulty option bAICheckCorpses (world+0x2c ->
	// vtbl+0xec GetGlobalGame -> +0x48 pDifficulty -> +0x6a bAICheckCorpses, all disasm-confirmed).
	bool bAvoid = bCheck && pServer->GetWorld()->GetGlobalGame()->pDifficulty->bAICheckCorpses;
	cmds.push_back( new CTaskCommandLookToPosition( enemyPos.pos.p ) );
	SPosition stop, flank, attack;
	stop.p   = SPathPlace( (int)0xfdffffff );                        // the invalid sentinel (nData is private)
	flank.p  = SPathPlace( (int)0xfdffffff );
	attack.p = SPathPlace( (int)0xfdffffff );
	if ( GetRoundUpPlaces( pServer, enemyPos, &stop, &flank, &attack, bAvoid ) )
	{
		// pose floors: a PK unit moves at WALK; both clamp DOWN to the requested nPose. (GetWearingDBPK is the
		// real +0x34 callee, probed twice as in the binary.)
		int nPoseLow = IsValid( pServer->GetWearingDBPK() ) ? 2 : 1; // WALK(2) / CROUCH(1)
		if ( nPose < nPoseLow )
			nPoseLow = nPose;
		int nWish = IsValid( pServer->GetWearingDBPK() ) ? 2 : 3;    // WALK(2) / RUN(3)
		if ( nPose < nWish )
			nWish = nPose;
		cmds.push_back( new CTaskCommandLookToPosition( enemyPos.pos.p ) );
		if ( !SamePlaceMasked( stop.p, pServer->GetPosition().pos.p ) )
		{
			cmds.push_back( new CTaskCommandChangeWishPose( (EPose)nWish ) );  // retail CreateRCChangeWishPose (@0xa2720)
			cmds.push_back( new CTaskCommandGoto( stop ) );               // release strafing Goto (prefix elided)
		}
		cmds.push_back( new CTaskCommandChangePose( (EPose)nPoseLow ) );
		cmds.push_back( new CTaskCommandGoto( flank ) );
		SPosition restore;
		restore.p = SPathPlace( (int)0xfdffffff );
		int nVisible = 0;
		if ( IsValid( pEnemy ) &&
			 GetRestoreAPPoint( pServer, pEnemy, flank, attack, &restore, &nVisible ) )
		{
			if ( !SamePlaceMasked( restore.p, pServer->GetPosition().pos.p ) )
			{
				if ( nVisible <= 3 && random.Get( 0, 100 ) < 40 )    // a 40% pause when few points were watched
					cmds.push_back( new CTaskCommandWait( 1 ) );
				cmds.push_back( new CTaskCommandGoto( restore ) );
				cmds.push_back( new CTaskCommandWait( 1 ) );
			}
			// with NO worn PK, look toward the enemy from the restore spot (release: this also sets the final
			// Goto's "looked" flag, which is elided with the strafe arg -- the Look command itself is faithful).
			if ( !IsValid( pServer->GetWearingDBPK() ) )
			{
				IPathNetwork *pNet = pServer->GetWorld()->GetPathNetwork();
				// byte-faithful: no &7 mask here (GetClosestDir already returns 0..7; unlike GetDirectedPos's sites).
				cmds.push_back( new CTaskCommandChangeDirection(
					(int)pNet->GetClosestDir( restore.p, enemyPos.pos.p ) ) );
			}
		}
		cmds.push_back( new CTaskCommandGoto( attack ) );            // release Goto(attack, bLooked) -- flag elided
		cmds.push_back( new CTaskCommandLookToPosition( enemyPos.pos.p ) );
	}
	// the tail runs even when the plan failed: a short wait, a randomized look-around, settle at WALK.
	cmds.push_back( new CTaskCommandWait( (int)( random.Get( 2 ) ) + 2 ) );  // Wait(2 + draw&1) (Get(2)==raw&1)
	RouteAddLookAround( false, cmds, 6 );
	cmds.push_back( new CTaskCommandChangePose( WALK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
