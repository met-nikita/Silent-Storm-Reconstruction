#include "StdAfx.h"
//
#include "aiUnit.h"          // NAI::IAIUnit (GetUnitServer / IsUnderAIControl)
#include "aiActionBase.h"    // NAI::SPlaceWithAP (complete) -- aiMoveAction.h has vector<SPlaceWithAP> members
#include "aiMoveAction.h"    // NAI::GetUnitPos (path place -> unit position)
#include "aiPath.h"          // NAI::CPath, NAI::PF_DEFAULT
#include "aiCommander.h"     // NAI::CAICommander::GetAIUnit
#include "wUnitServer.h"     // NWorld::CUnitServer: GetWorld/GetUnitRPG/GetPlayer/CanFight + wish-pose pair
#include "wMain.h"           // NWorld::CWorld::GetPathNetwork
#include "wMainPath.h"       // NWorld::FindPath
#include "wMainMoves.h"      // NWorld::GetMoveActionType (path-AP step action)
#include "rpgUnitMission.h"  // NRPG::IUnitMission::GetActionAP
//
#include "aiMisc.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiMisc -- the release-new NAI world-glue helpers. Reconstructed from the matched-release decode
// (oracle: decomp/src/s2_aimisc.h, all disasm-verified @0x73df0..0x74700) onto the in-tree path
// infrastructure -- no new subsystem. See aiMisc.h for the per-helper summary + the IsAIPlayer gap
// (absent CSequenceCommander).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
namespace {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Local move-AP accumulator -- sums the move AP along a path (GetMoveActionType per step -> GetActionAP).
// The release prices paths with NWorld::CPathAPCalcer (not exposed in the dev tree); this is the same
// file-local twin the dev tree already carries in aiActionPlaceSource.cpp / aiMoveAction.cpp / aiIterator.cpp.
////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////
// The probe shared by HasPath / GetAPForMove: force the FindPath pose to `nPose` for the duration
// (writing CRAWL also clears the strafe/run byte), then restore -- the wish-pose pair at CUnitServer
// +0x28 / +0x24 (the same pair aiRouteMisc::GetNearestPlaces drives). `bCanFindNotExactPath` is the
// only FindPath flag the callers vary (HasPath threads its argument through; GetAPForMove passes false).
////////////////////////////////////////////////////////////////////////////////////////////////////
static CPtr<CPath> ProbePath( NWorld::CUnitServer *pUS, IPathNetwork *pNet, const SPathPlace &from,
	const SPathPlace &to, EPose nPose, bool bCanFindNotExactPath )
{
	vector<SPathPlace> dest;
	dest.push_back( to );
	EPose nOldWish = pUS->GetWishPose();
	pUS->SetWishPose( nPose );
	if ( nPose == CRAWL )
		pUS->SetStrafe( false );
	CPtr<CPath> pPath = NWorld::FindPath( pNet, static_cast<NWorld::CUnit*>( pUS ), from, dest, 0,
		false, NAI::PF_DEFAULT, false, bCanFindNotExactPath, false );
	pUS->SetWishPose( nOldWish );
	if ( nOldWish == CRAWL )
		pUS->SetStrafe( false );
	return pPath;
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetPathAP @0x740a0
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetPathAP( NWorld::CUnitServer *pUS, CPath *pPath )
{
	if ( !IsValid( pPath ) || !IsValid( pUS ) || pPath->points.empty() )
		return 0;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	CPathAPCalcerLocal calc( pUS->GetWorld(), pUS->GetUnitRPG(), GetUnitPos( pPath->points[0], pNet ), false );
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
		calc.AddPoint( *i );
	return calc.GetResult();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsNullAPPath @0x741b0 -- a missing/empty/dead path is trivially free (true).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsNullAPPath( NWorld::CUnitServer *pUS, CPath *pPath )
{
	if ( !IsValid( pPath ) || !IsValid( pUS ) || pPath->points.empty() )
		return true;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	CPathAPCalcerLocal calc( pUS->GetWorld(), pUS->GetUnitRPG(), GetUnitPos( pPath->points[0], pNet ), false );
	for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
	{
		calc.AddPoint( *i );
		if ( calc.GetResult() > 0 )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::HasPath @0x74360
////////////////////////////////////////////////////////////////////////////////////////////////////
bool HasPath( IAIUnit *pUnit, const SPathPlace &from, const SPathPlace &to, EPose nPose,
	bool bCanFindNotExactPath )
{
	if ( !IsValid( pUnit ) )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )
		return false;
	return IsValid( ProbePath( pUS, pNet, from, to, nPose, bCanFindNotExactPath ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetAPForMove @0x74520
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetAPForMove( IAIUnit *pUnit, const SPathPlace &from, const SPathPlace &to, EPose nPose )
{
	if ( !IsValid( pUnit ) )
		return 0xffff;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return 0xffff;
	IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	if ( !IsValid( pNet ) )
		return 0xffff;
	CPtr<CPath> pPath = ProbePath( pUS, pNet, from, to, nPose, false );
	if ( !IsValid( pPath ) )
		return 0xffff;
	return GetPathAP( pUS, pPath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CanAIOperateThisUnit @0x74700. The release reads IAIUnit vtbl 0x1c (IsAIUnit); the dev-native
// IsUnderAIControl is the same "the AI may drive this unit" predicate.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanAIOperateThisUnit( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return false;
	if ( !pUnit->IsUnderAIControl() )
		return false;
	NWorld::CUnitServer *pUS = pUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return false;
	return pUS->CanFight();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::IsAIPlayer @0x73df0. Reconstructed now that the dyncast target NAI::CSequenceCommander exists
// (aiCommander.h). Release semantics: a live player whose commander is NOT a CSequenceCommander is an
// AI-controlled side. Faithful to the disasm -- only the cast result's null-ness is tested (there is NO
// alive-bit gate on the cast result here, unlike the sibling GetAICommander's commander check).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsAIPlayer( NWorld::IPlayer *pPlayer )
{
	if ( !IsValid( pPlayer ) )
		return false;
	return !CDynamicCast<CSequenceCommander>( pPlayer->GetCommander() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetAICommander @0x73ea0. ELIDED (documented): the release additionally gates on a unit-component
// predicate (server+0x14c vtbl 0x10) before resolving the commander. The dev tree resolves the AI
// commander via this exact dyncast WITHOUT that gate in 6+ places (aiRoute / aiSignal / aiTacticalCommander
// / aiTaskCommander / aiSnipeAction), so omitting it is dev-consistent; the gate only ADDS a rejection.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICommander* GetAICommander( NWorld::CUnitServer *pUS )
{
	if ( !IsValid( pUS ) || pUS->GetPlayer() == 0 )
		return 0;
	CDynamicCast<CAICommander> pCommander( pUS->GetPlayer()->GetCommander() );
	if ( !IsValid( pCommander ) )
		return 0;
	return pCommander;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::GetAIUnit @0x742c0
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit* GetAIUnit( NWorld::CUnitServer *pUS )
{
	CPtr<CAICommander> pCommander = GetAICommander( pUS );
	if ( !IsValid( pCommander ) )
		return 0;
	return pCommander->GetAIUnit( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
