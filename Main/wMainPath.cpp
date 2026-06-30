#include "StdAfx.h"
#include "wMainPath.h"
#include "aiMultiMoves.h"
#include "RPGUnitInfo.h"
#include "wUnitServer.h"
#include "wMainMoves.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void PrepareAllPaths( NAI::IPathNetwork *pPathNetwork, NAI::CMultiMovesTable *pTable, 
	list<NAI::SPathPlace> *pResult, CUnit *pWho, const NAI::SPathPlace &ptSrc, 
	int nPriceLimit, CUnit *pIgnore, bool bCheckSuicide )
{
	static int nCosts[ NAI::N_MOVE_TYPES ];
	vector<NAI::SPoint> points;
	CUnitServer *pUS = dynamic_cast<CUnitServer*>( pWho );
	if ( !IsValid( pUS ) )
		return;
	ASSERT( nPriceLimit >=0 );
	if ( nPriceLimit <= 0 )
	{
		pResult->push_back( ptSrc );
		return;
	}

	pPathNetwork->Unlock( pWho );
	list<CObjectBase*> vis;
	SelectFindPathUnits( pWho, &vis );
	vis.remove( pIgnore );

	NAI::EPose curPose = pUS->GetWishPose();
	const NRPG::IUnitMissionInfo *pRPG = pUS->GetRPG();
	if ( pUS->IsCarryingCorpse() )
	{
		if ( curPose == NAI::RUN )
			nCosts[ NAI::MT_MOVE_STAND ] = pRPG->GetActionAP( NAI::RUN, NRPG::AC_MOVE_CORPSE_SIDE );
		else
			nCosts[ NAI::MT_MOVE_STAND ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_MOVE_CORPSE_SIDE );
		nCosts[ NAI::MT_MOVE_CRAWL ] = pRPG->GetActionAP( NAI::CRAWL, NRPG::AC_MOVE_CORPSE_SIDE );
		nCosts[ NAI::MT_MOVE_CROUCH ] = pRPG->GetActionAP( NAI::CROUCH, NRPG::AC_MOVE_CORPSE_SIDE );

		if ( curPose == NAI::RUN )
			nCosts[ NAI::MT_MOVE_STAND_DIAG ] = pRPG->GetActionAP( NAI::RUN, NRPG::AC_MOVE_CORPSE_DIAGONAL );
		else
			nCosts[ NAI::MT_MOVE_STAND_DIAG ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_MOVE_CORPSE_DIAGONAL );
		nCosts[ NAI::MT_MOVE_CRAWL_DIAG ] = pRPG->GetActionAP( NAI::CRAWL, NRPG::AC_MOVE_CORPSE_DIAGONAL );
		nCosts[ NAI::MT_MOVE_CROUCH_DIAG ] = pRPG->GetActionAP( NAI::CROUCH, NRPG::AC_MOVE_CORPSE_DIAGONAL );
	}
	else
	{
		if ( curPose == NAI::RUN )
			nCosts[ NAI::MT_MOVE_STAND ] = pRPG->GetActionAP( NAI::RUN, NRPG::AC_MOVE_SIDE );
		else
			nCosts[ NAI::MT_MOVE_STAND ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_MOVE_SIDE );
		nCosts[ NAI::MT_MOVE_CRAWL ] = pRPG->GetActionAP( NAI::CRAWL, NRPG::AC_MOVE_SIDE );
		nCosts[ NAI::MT_MOVE_CROUCH ] = pRPG->GetActionAP( NAI::CROUCH, NRPG::AC_MOVE_SIDE );

		if ( curPose == NAI::RUN )
			nCosts[ NAI::MT_MOVE_STAND_DIAG ] = pRPG->GetActionAP( NAI::RUN, NRPG::AC_MOVE_DIAGONAL );
		else
			nCosts[ NAI::MT_MOVE_STAND_DIAG ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_MOVE_DIAGONAL );
		nCosts[ NAI::MT_MOVE_CRAWL_DIAG ] = pRPG->GetActionAP( NAI::CRAWL, NRPG::AC_MOVE_DIAGONAL );
		nCosts[ NAI::MT_MOVE_CROUCH_DIAG ] = pRPG->GetActionAP( NAI::CROUCH, NRPG::AC_MOVE_DIAGONAL );
	}
	nCosts[ NAI::MT_TURN ] = 1;
	nCosts[ NAI::MT_CLIMB_1 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_1 );
	nCosts[ NAI::MT_CLIMB_2 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_2 );
	nCosts[ NAI::MT_CLIMB_3 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_3 );
	nCosts[ NAI::MT_CLIMB_4 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_4 );
	nCosts[ NAI::MT_JUMP ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_JUMP );
	nCosts[ NAI::MT_POSE_WALK_CROUCH ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_POSE_CROUCH );
	nCosts[ NAI::MT_POSE_WALK_CRAWL ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_POSE_CRAWL );
	nCosts[ NAI::MT_POSE_CROUCH_CRAWL ] = pRPG->GetActionAP( NAI::CRAWL, NRPG::AC_POSE_CROUCH );
	nCosts[ NAI::MT_LADDER_UP ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER );
	nCosts[ NAI::MT_LADDER_DOWN ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER );
	nCosts[ NAI::MT_LADDER_MOVE ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER_MOVE );
	nCosts[ NAI::MT_ZERO ] = 1;
	
	pTable->PrepareAllPaths( pResult, pPathNetwork, ptSrc, nCosts, nPriceLimit, vis,
		bCheckSuicide, pUS->IsCarryingCorpse() );

	pPathNetwork->Lock( pWho, pUS->GetPosition().pos.p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CPath* FindPath( NAI::IPathNetwork *pPathNetwork, CUnit *pWho, const NAI::SPathPlace &ptSrc, 
	const vector<NAI::SPathPlace> &ptDst, CUnit *pIgnore, bool bCheckSuicide, 
	NAI::EFindPathParams eParams, bool _bStrafe, bool bCanFindNotExactPath, bool bIgnoreAllUnits )
{
	static int nCosts[NAI::N_MOVE_TYPES];
	vector<NAI::SPoint> points;
	CUnitServer *pUS = dynamic_cast<CUnitServer*>( pWho );
	if ( !IsValid( pUS ) )
		return 0;

	bool bStrafe = _bStrafe	&& ( !pUS->IsWearingPK() );

	pPathNetwork->Unlock( pWho );
	pPathNetwork->ClearDynamicLocks( pWho );

	bool bPKCroucher = pUS->IsWearingPK() && pUS->GetPosition().GetPose() == NAI::CROUCH;
	
	NAI::CPath *pRes = NAI::FindSimplePath( ptSrc, ptDst, eParams, pPathNetwork, bPKCroucher, 
		pUS->IsWearingPK(), pUS->IsCarryingCorpse() );
	if ( pRes || bPKCroucher )
	{
		pPathNetwork->Lock( pWho, pUS->GetPosition().pos.p );
		pPathNetwork->RestoreDynamicLocks( pWho );
		return pRes;
	}

	list<CObjectBase*> vis;
	if ( !bIgnoreAllUnits )
	{		
		SelectFindPathUnits( pWho, &vis );
		vis.remove( pIgnore );
	}

	bool bMoveOnly = pUS->IsCarryingCorpse() || pUS->IsWearingPK();
	if ( bCheckSuicide )
	{
		int nMax = 10;
		int nMaxDiag = 14;
		nCosts[ NAI::MT_MOVE_STAND ] = nMax;
		nCosts[ NAI::MT_MOVE_STAND_DIAG ] = nMaxDiag;
		nCosts[ NAI::MT_MOVE_CROUCH ] = nMax;
		nCosts[ NAI::MT_MOVE_CROUCH_DIAG ] = nMaxDiag;
		nCosts[ NAI::MT_MOVE_CRAWL ] = nMax;
		nCosts[ NAI::MT_MOVE_CRAWL_DIAG ] = nMaxDiag;
		nCosts[ NAI::MT_TURN ] = 1;
		nCosts[ NAI::MT_CLIMB_1 ] = nMax;
		nCosts[ NAI::MT_CLIMB_2 ] = nMax;
		nCosts[ NAI::MT_CLIMB_3 ] = nMax;
		nCosts[ NAI::MT_CLIMB_4 ] = nMax;
		nCosts[ NAI::MT_JUMP ] = nMax;
		nCosts[ NAI::MT_POSE_WALK_CROUCH ] = 1;
		nCosts[ NAI::MT_POSE_WALK_CRAWL ] = nCosts[ NAI::MT_POSE_CROUCH_CRAWL ] = nMaxDiag + 1;			
		nCosts[ NAI::MT_LADDER_UP ] = nMax;
		nCosts[ NAI::MT_LADDER_DOWN ] = nMax;
		nCosts[ NAI::MT_LADDER_MOVE ] = nMax;
		nCosts[ NAI::MT_ZERO ] = 1;
		pRes = NAI::FindPath( pPathNetwork, ptSrc, ptDst, nCosts, nMax * 3 - 1, bStrafe, vis, pWho, true, bMoveOnly, eParams );
	}

	if ( !pRes )
	{
		NAI::EPose curPose = pUS->GetWishPose();
		const NRPG::IUnitMissionInfo *pRPG = pUS->GetRPG();
		int nSideCost, nDiagCost;
		if ( pUS->IsCarryingCorpse() )
			nSideCost = pRPG->GetActionAP( curPose, NRPG::AC_MOVE_CORPSE_SIDE );
		else
			nSideCost = pRPG->GetActionAP( curPose, NRPG::AC_MOVE_SIDE );
		if ( pUS->IsCarryingCorpse() )
			nDiagCost = pRPG->GetActionAP( curPose, NRPG::AC_MOVE_CORPSE_DIAGONAL );
		else
			nDiagCost = pRPG->GetActionAP( curPose, NRPG::AC_MOVE_DIAGONAL );
		int nAddStand = (curPose == NAI::WALK || curPose == NAI::RUN) ? 0 : 2;
		int nAddCrouch = (curPose == NAI::CROUCH) ? 0 : 2;
		int nAddCrawl = (curPose == NAI::CRAWL) ? 0 : 2;
		nCosts[ NAI::MT_POSE_WALK_CROUCH ] = nCosts[ NAI::MT_POSE_WALK_CRAWL ] 
			= nCosts[ NAI::MT_POSE_CROUCH_CRAWL ] = 1;			
		if ( curPose == NAI::CRAWL )
		{
			nCosts[ NAI::MT_POSE_WALK_CRAWL ] = 8;
			nCosts[ NAI::MT_POSE_CROUCH_CRAWL ] = 4;
			nCosts[ NAI::MT_POSE_WALK_CROUCH ] = 5;
			nAddStand = 3;
		}
		nCosts[ NAI::MT_MOVE_STAND ] = nSideCost + nAddStand;
		nCosts[ NAI::MT_MOVE_STAND_DIAG ] = nDiagCost + nAddStand;
		nCosts[ NAI::MT_MOVE_CROUCH ] = nSideCost + nAddCrouch;
		nCosts[ NAI::MT_MOVE_CROUCH_DIAG ] = nDiagCost + nAddCrouch;
		nCosts[ NAI::MT_MOVE_CRAWL ] = nSideCost + nAddCrawl;
		nCosts[ NAI::MT_MOVE_CRAWL_DIAG ] = nDiagCost + nAddCrawl;
		nCosts[ NAI::MT_TURN ] = 1;
		nCosts[ NAI::MT_CLIMB_1 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_1 );
		nCosts[ NAI::MT_CLIMB_2 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_2 );
		nCosts[ NAI::MT_CLIMB_3 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_3 );
		nCosts[ NAI::MT_CLIMB_4 ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_CLIMB_4 );
		nCosts[ NAI::MT_JUMP ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_JUMP );
		nCosts[ NAI::MT_LADDER_UP ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER );
		nCosts[ NAI::MT_LADDER_DOWN ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER );
		nCosts[ NAI::MT_LADDER_MOVE ] = pRPG->GetActionAP( NAI::WALK, NRPG::AC_LADDER_MOVE );
		nCosts[ NAI::MT_ZERO ] = 1;
		pRes = NAI::FindPath( pPathNetwork, ptSrc, ptDst, nCosts, 
			0x7FFFFFFF, bStrafe, vis, pWho, false, bMoveOnly, eParams, bCanFindNotExactPath );
	}

	pPathNetwork->Lock( pWho, pUS->GetPosition().pos.p );
	pPathNetwork->RestoreDynamicLocks( pWho );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsValidPath( NAI::IPathNetwork *pPathNetwork, const NAI::CPath &path, CUnit *pWho )
{
	list<CObjectBase*> vis;
	SelectFindPathUnits( pWho, &vis );
	pPathNetwork->LockSelected( vis );
	bool bRes = NAI::IsValidPath( path );
	pPathNetwork->UnlockSelected();
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}