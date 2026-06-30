#include "StdAfx.h"
//
#include "aiAction.h"
#include "aiPosition.h"
#include "aiPath.h"
#include "rpgUnitMission.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "aiUnit.h"
#include "wMainMoves.h"
#include "wMainPath.h"
//
#include "aiMoveAction.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
SPosition GetPos( const SPathPlace &place, IPathNetwork *pPathNetwork )
{
	ASSERT( IsValid( pPathNetwork ) );
	SPosition pos;
	pos.SetNetwork( pPathNetwork );
	pos.p = place;
	return pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SUnitPosition GetUnitPos( const SPathPlace &place, IPathNetwork *pPathNetwork )
{
	SUnitPosition pos;
	pos.pos = GetPos( place, pPathNetwork );
	pos.bRun = false;
	return pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIPathAPCalcer - CRAP for test. The same class was defined at wUnitMove.cpp
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIPathAPCalcer
{
	int nRes;
	NAI::SUnitPosition currentPos;
	bool bCorpse;
	NWorld::CWorld *pWorld;
	NRPG::IUnitMission *pRPG;
public:
	CAIPathAPCalcer( NWorld::CWorld *_pWorld, NRPG::IUnitMission *_pRPG, const NAI::SUnitPosition &_p, bool _bCorpse )
		: pWorld(_pWorld), pRPG(_pRPG), currentPos(_p), bCorpse(_bCorpse), nRes(0) {}
	void AddPoint( const NAI::SUnitPosition &_pos )
	{
		NRPG::EAction action = NWorld::GetMoveActionType( pWorld->GetPathNetwork(), currentPos, _pos, bCorpse );
		nRes += pRPG->GetActionAP( currentPos.GetPose(), action );
		currentPos = _pos;
	}
	void AddPoint( const NAI::SPathPlace &_p )
	{
		NAI::SUnitPosition pos( currentPos );
		pos.pos.p = _p;;
		AddPoint( pos );
	}
	int GetResult() const { return nRes; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFindGoodPlacesJob
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFindGoodPlacesJob::CAIFindGoodPlacesJob( IAIUnit *_pUnit, IAIUnit *_pEnemy, int _nMaxAP, CAIJob *pParentJob ): 
	CAIJob( pParentJob ), pUnit( _pUnit ), pEnemy( _pEnemy ), nCurrentPlace( -1 ), bPathFound( false ), nMaxAP( _nMaxAP )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pEnemy ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFindGoodPlacesJob::DoJob()
{
	ASSERT( IsValid( pUnit ) );
	if ( !IsValid( pUnit ) )
		return;

	if ( !bPathFound )
	{
		PreparePlaces();
		bPathFound = true;
		nCurrentPlace = places.size() - 1;
		nBestPlace = -1;
		nBestCover = 0;
	}
	else if ( nCurrentPlace >= 0 )
	{
		SPlaceWithAP &place = places[ nCurrentPlace ];
		if ( place.nUnitAP == pUnit->GetAP() )
		{
			goodPlaces.push_back( place );
		}
		else
		{
			int nCover = pUnit->GetCoverForFixedUnit( place.place,	pEnemy->GetUnitServer(), 0, NAI::HL_ANY );
			if ( nCover > nBestCover )
			{
				nBestCover = nCover;
				nBestPlace = nCurrentPlace;
			}
		}
		--nCurrentPlace;
	}
	else
	{
		if ( nBestPlace >= 0 )
			goodPlaces.push_back( places[ nBestPlace ] );
		Finish();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFindGoodPlacesJob::PreparePlaces()
{
	places.clear();
	CPtr<NWorld::CUnitServer> pUS = pUnit->GetUnitServer();
	CPtr<NWorld::CUnitServer> pEnemyUS = pEnemy->GetUnitServer();
	CPtr<NWorld::CWorld> pWorld = pUS->GetWorld();
	CPtr<NAI::IPathNetwork> pPathNetwork = pWorld->GetPathNetwork();
	CPtr<NRPG::IUnitMission> pUnitRPG = pUS->GetUnitRPG();
	CAIPathAPCalcer calcer( pWorld, pUnitRPG, pUnit->GetUnitPosition(), false );
	//
	vector<NAI::SPathPlace> dest;
	dest.push_back( pEnemyUS->GetPosition().pos.p );
	pUS->SetWishPose( NAI::RUN );
	CPtr<NAI::CPath> pPath = NWorld::FindPath( pWorld->GetPathNetwork(), pUS, pUnit->GetPosition().p, 
		dest, pUS, false, NAI::PF_DEFAULT, false, true, true );
	if ( IsValid( pPath ) )
	{
		for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
		{
			NAI::SPathPlace p = *i;
			calcer.AddPoint( p );
			int nCostWalk = calcer.GetResult() + pUnitRPG->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_WALK );
			int nCostCrouch = calcer.GetResult() + pUnitRPG->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_CROUCH );
			//
			if ( nCostWalk <= nMaxAP )
			{
				p.SetPose( NAI::CM_STAND );	
				if ( pPathNetwork->IsNativePassable( p ) )
					places.push_back( SPlaceWithAP( GetUnitPos( p, pPathNetwork ) , pUnit->GetAP() - nCostWalk ) );
			}
			if ( nCostCrouch <= nMaxAP )
			{
				p.SetPose( NAI::CM_CROUCH ); 
				if ( pPathNetwork->IsNativePassable( p ) )
					places.push_back( SPlaceWithAP( GetUnitPos( p, pPathNetwork ), pUnit->GetAP() - nCostCrouch ) );
			}
			if ( nCostWalk > nMaxAP && nCostCrouch > nMaxAP )
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForActionsJob
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIChoosePlaceForActionsJob::CAIChoosePlaceForActionsJob( const vector< CPtr<CAIAction> > &_actions, CAIJob *pParentJob ):
	actions( _actions ), nCurPlace( 0 ), nCurAction( 0 )
{
	for ( vector< CPtr<CAIAction> >::const_iterator i = actions.begin(); i != actions.end(); ++i )
		actionToChosen[ *i ] = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceForActionsJob::DoJob()
{
	if ( nCurAction < actions.size() )
	{
		if ( nCurPlace < places.size() )
		{
			CPtr<CAIAction> pAction = actions[ nCurAction ];
			SPlaceWithAP &place = places[ nCurPlace ];
			if ( !actionToChosen[ pAction ] )
			{
				if ( pAction->CanDo( place ) )
				{
					actionToChosen[ pAction ] = true;
					actionToPlace[ pAction ] = nCurPlace;
				}
			}
			else
			{
				SPlaceWithAP &prevPlace = places[ actionToPlace[ pAction ] ];
				if ( pAction->ComparePlaces( place, prevPlace ) )
				{
					actionToPlace[ pAction ] = nCurPlace;
				}
			}
		}
		//
		++nCurPlace;
		if ( nCurPlace >= places.size() )
		{
			++nCurAction;
			nCurPlace = 0;
		}
	}
	else
	{
		Finish();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIChoosePlaceForActionsJob::IsPlaceChosen( CAIAction *pAction )
{
	ASSERT( IsValid( pAction ) );
	if ( IsValid( pAction ) )
		return actionToChosen[ pAction ];
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceForActionsJob::GetPlace( CAIAction *pAction, SPlaceWithAP *pPlace )
{
	ASSERT( IsValid( pAction ) );
	ASSERT( IsPlaceChosen( pAction ) );
	if ( IsValid( pAction ) && IsPlaceChosen( pAction ) )
		*pPlace = places[ actionToPlace[ pAction ] ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51313140, CAIFindGoodPlacesJob );
REGISTER_SAVELOAD_CLASS( 0x51313170, CAIChoosePlaceForActionsJob );