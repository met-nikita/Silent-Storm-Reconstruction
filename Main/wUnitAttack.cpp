#include "StdAfx.h"
#include "wUnitAttack.h"
#include "wUnitMove.h"
#include "wUnitServer.h"
#include "Grid.h"
#include "wMain.h"
#include "wMainPath.h"
#include "RPGItem.h"
//#include "RPGItemSet.h" // CRAP
#include "RPGUnitMission.h"
#include "RPGGame.h"
#include "aiMap.h"
#include "aiCollider.h"
#include "..\misc\RandomGen.h"
#include "wObject.h"
#include "wUnitStates.h"
#include "wAckBase.h"
#include "RPGToHit.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataAI.h"
#include "RPGCritical.h"
#include "aiPath.h"
#include "wUnitAttackExec.h"
#include "wUnitQueue.h"
#include "wMine.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
// Retail 1.1 @0x792ed0 / 1.2 @0x793120: own units are targetable;
// other units require actual sight, ignoring facing (not the player's shared visible list).
static bool IsVisibleTarget( CUnitServer *pUS, CUnitServer *pTarget )
{
	return pUS->GetPlayer() == pTarget->GetPlayer() ||
		pUS->GetWorld()->GetGame()->CheckVisibility( pUS, pTarget, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAllowPoseMask
{
	PM_LAY     = 1,
	PM_CROUCH  = 2,
	PM_STAND   = 4,
	PM_ALL     = 7
};
////////////////////////////////////////////////////////////////////////////////////////////////////
EActionType GetActionType( CUnitServer *pUS )
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	if ( !pRPG )
	{
		ASSERT(0);
		return AT_NONE;
	}
	if ( pRPG->GetCannonItem() )
		return AT_CANNON;
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	NRPG::IInventoryItem *pItem = pInventory->GetActive();
	//

	if ( pItem == 0 )
		return AT_MELEE;
	else {
		CDynamicCast<NRPG::IWeaponItem>pWeapon(pItem);
		if (pWeapon)
		{
			if (pWeapon->GetDBWeapon()->bBazookaLogic)
				return AT_BAZOOKA;
			else
			{
				if (pWeapon->GetShootMode() == NDb::SM_Snipe && pUS->IsSniping())
					return AT_SNIPE;
				else
					return AT_SHOOT;
			}
		}
		else if (CDynamicCast<NRPG::IFirstAidItem>(pItem))
			return AT_FIRSTAID;
		else if (CDynamicCast<NRPG::IGrenadeItem>(pItem))
			return AT_GRENADE;
		else if (CDynamicCast<NRPG::IMineItem>(pItem))
			return AT_MINE;
		else if (CDynamicCast<NRPG::IToolItem>(pItem))
			return AT_TOOL;
		else if (CDynamicCast<NRPG::IKeyItem>(pItem))
			return AT_KEY;
		else {
			CDynamicCast<NRPG::IMeleeWeaponItem> pMelee(pItem);
			if (pMelee)
			{
				if (pMelee->GetDBMeleeWeapon()->bThrowing)
					return AT_THROW;

				return AT_KNIFE;
			}
		}
	}

	return AT_NONE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetDirectedPoints( NAI::IPathNetwork *pNet, const NAI::SPathPlace &from, const CVec3 &ptTarget, 
	vector<NAI::SPathPlace> *pRes, int nPoseMask = PM_ALL )
{
	NAI::EDirection dir = NRPG::GetShootDirection( pNet, from, ptTarget ) ;
	// Retail v1.1 0x793310 / v1.2 0x793560 preserves the fly bit in every pose.
	// Without it, a flying attack's encoded altitude becomes a movement-layer index.
	NAI::SPathPlace place( from.GetX(), from.GetY(), from.GetLayer(), dir, NAI::CM_LAY, false );
	place.SetFinal( from.IsFinal() );
	if ( nPoseMask & PM_LAY )
		pRes->push_back( place );
	place.SetPose( NAI::CM_CROUCH );
	if ( nPoseMask & PM_CROUCH )
		pRes->push_back( place );
	place.SetPose( NAI::CM_STAND );
	if ( nPoseMask & PM_STAND )
		pRes->push_back( place );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetMeleeAttackPlaces( CUnitServer *pUS, const CVec3 &ptTarget, vector<NAI::SPathPlace> *pRes )
{
	NAI::IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	SSphere s;
	s.ptCenter = pUS->GetPosition().GetCP();
	s.fRadius = 5; // CRAP need more grounded number then this
	vector<NAI::SPathPlace> res;
	pNet->GetNearPlaces( s, &res );
	NAI::SUnitPosition from( pUS->GetPosition() );
	for ( int k = 0; k < res.size(); ++k )
	{
		if ( !pNet->IsNativePassable( res[k] ) )
			continue;
		from.pos.p = res[k];
		float fDist = F_MELEE_DISTANCE;
		if ( pUS->IsWearingPK() )
			fDist *= 2;
		if ( IsWithinHumanReach( from.GetCP(), ptTarget, fDist ) )
			GetDirectedPoints( pNet, from.pos.p, ptTarget, pRes );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetSnipeAttackPlaces( CUnitServer *pUS, CUnitServer *pTarget, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pTarget ) || !IsValid( pUS ) )
		return;
	//
	CWorld *pWorld = pUS->GetWorld();
	NAI::SPathPlace p = pUS->GetPosition().pos.p;
	p.SetDirection( pWorld->GetPathNetwork()->GetClosestDir( p,	pTarget->GetPosition().pos.p ) );
	pRes->push_back( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetHumanReachPlaces( CUnitServer *pUS, const CVec3 &ptTarget, vector<NAI::SPathPlace> *pRes, float fPlaneDist )
{
	NAI::IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	SSphere s;
	s.ptCenter = ptTarget; 
	s.fRadius = 5; // CRAP need more grounded number then this
	vector<NAI::SPathPlace> res;
	pNet->GetNearPlaces( s, &res );
	NAI::SUnitPosition from( pUS->GetPosition() );
	for ( int k = 0; k < res.size(); ++k )
	{
		if ( !pNet->IsNativePassable( res[k] ) )
			continue;
		from.pos.p = res[k];
		if ( IsWithinHumanReach( from.GetCP(), ptTarget, fPlaneDist ) )
			GetDirectedPoints( pNet, from.pos.p, ptTarget, pRes );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetHumanReachPlaces( CUnitServer *pUS, const CVec3 &ptTarget, vector<NAI::SPathPlace> *pRes )
{
	GetHumanReachPlaces( pUS, ptTarget, pRes, F_HEAL_DISTANCE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetAttackPoseMask( const NAI::SPathPlace &place )
{
	// Retail v1.2 0x793a31/0x793b2a keeps directed attacks in the current pose.
	switch ( place.GetPose() )
	{
		case NAI::CM_LAY: return PM_LAY;
		case NAI::CM_CROUCH: return PM_CROUCH;
		case NAI::CM_STAND: return PM_STAND;
		default: return PM_ALL;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdShootTile *pCmd, vector<NAI::SPathPlace> *pRes )
{
	CWorld *pWorld = pUS->GetWorld();
	NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
	switch ( GetActionType( pUS ) )
	{
		case AT_MELEE:
		case AT_KNIFE:
			GetMeleeAttackPlaces( pUS, pCmd->ptTarget, pRes );
			break;
		case AT_SHOOT:
		case AT_GRENADE:
		case AT_THROW:
		case AT_BAZOOKA:
			GetDirectedPoints( pNet, pUS->GetPosition().pos.p, pCmd->ptTarget, pRes,
				GetAttackPoseMask( pUS->GetPosition().pos.p ) );
			break;
		default:
			ASSERT( 0 );
			break;
	}

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdShootObject *pCmd, vector<NAI::SPathPlace> *pRes )
{
	// Retail v1.2 0x793ae0: doors and other object targets also need facing/item
	// preparation. A unit-only cast incorrectly leaves the queue at UCR_NO_TARGET.
	CPtr<CObjectBase> pTarget = pCmd->pTarget;
	if ( !IsValid( pTarget ) )
		return UCR_NO_TARGET;

	CWorld *pWorld = pUS->GetWorld();
	NAI::IPathNetwork *pNet = pWorld->GetPathNetwork();
	CVec3 ptTo;
	pWorld->GetAIMap()->GetUnitHLPos( &ptTo, pWorld->GetAIMap()->GetHull(pTarget), pCmd->eHL == NAI::HL_ANY ? NAI::HL_BODY : pCmd->eHL );
	switch ( GetActionType( pUS ) )
	{
		case AT_MELEE:
		case AT_KNIFE:
			GetMeleeAttackPlaces( pUS, ptTo, pRes );
			break;
		case AT_SNIPE:
		case AT_SHOOT:
		case AT_BAZOOKA:
		case AT_THROW:
			GetDirectedPoints( pNet, pUS->GetPosition().pos.p, ptTo, pRes,
				GetAttackPoseMask( pUS->GetPosition().pos.p ) );
			break;
		default:
			ASSERT( 0 );
	}

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, IGetApproaches *pObject, vector<NAI::SPathPlace> *pRes )
{
	ASSERT( pObject );
	pObject->GetApproaches( pRes, pUS->GetWorld()->GetPathNetwork() );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdHeal *pCmd, vector<NAI::SPathPlace> *pRes )
{
	CDynamicCast<CUnitServer> pTarget( pCmd->pTarget );
	if ( !IsValid( pTarget ) )
		return UCR_NO_TARGET;

	if ( pTarget == pUS )
		pRes->push_back( pUS->GetPosition().pos.p );
	else if ( IsValid( pTarget->GetWearingPK() ) || pTarget->IsEmptyPK() )
	{
		// Retail v1.2 0x793cf4: repair from behind cardinal-facing suits,
		// or from four standing approaches around diagonal-facing suits.
		NAI::IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
		const NAI::SPathPlace target = pTarget->GetPosition().pos.p;
		const int dx = NAI::nMoveShift[target.GetDirection()][0];
		const int dy = NAI::nMoveShift[target.GetDirection()][1];
		NAI::SPathPlace place = target;
		if ( dx == 0 || dy == 0 )
		{
			place.SetXY( target.GetX() - 2 * dx, target.GetY() - 2 * dy );
			if ( pNet->IsNativePassable( place ) )
				pRes->push_back( place );
		}
		else
		{
			const int offsets[4][2] = { { -2 * dx, 0 }, { 2 * dx, 0 }, { 0, -2 * dy }, { 0, 2 * dy } };
			for ( int i = 0; i < 4; ++i )
			{
				place.SetXY( target.GetX() + offsets[i][0], target.GetY() + offsets[i][1] );
				if ( pNet->IsNativePassable( place ) )
					GetDirectedPoints( pNet, place, pTarget->GetPosition().GetCP(), pRes, PM_STAND );
			}
		}
	}
	else
		GetHumanReachPlaces( pUS, pTarget->GetPosition().GetEyePosition(), pRes, GetHealOrRepairPKDistance( pTarget ) );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetTalkValidPlaces( CUnitServer *pUS, CCmdTalk *pCmd, vector<NAI::SPathPlace> *pRes, float fPlaneDist )
{
	CDynamicCast<CUnitServer> pTarget( pCmd->pTarget );
	if ( !IsValid( pTarget ) || pUS == pTarget.GetPtr() )
		return UCR_NO_TARGET;
	// Retail v1.2 0x793f20: dialogue approaches must be standing.
	vector<NAI::SPathPlace> places;
	GetHumanReachPlaces( pUS, pTarget->GetPosition().GetEyePosition(), &places, fPlaneDist );
	for ( int k = 0; k < places.size(); ++k )
		if ( places[k].GetPose() == NAI::CM_STAND )
			pRes->push_back( places[k] );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdTalk *pCmd, vector<NAI::SPathPlace> *pRes )
{
	return GetTalkValidPlaces( pUS, pCmd, pRes, 0.8f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdSetGrenadeOnObject *pCmd, vector<NAI::SPathPlace> *pRes )
{
	CDynamicCast<CWindowDoor> pTarget( pCmd->pTarget );
	if ( !IsValid( pTarget ) )
		return UCR_NO_TARGET;

	CDynamicCast<IGetApproaches> pAppr( pCmd->pTarget );
	return GetActionValidPlaces( pUS, pAppr, pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetMinePlaces( const CVec3 &ptTarget, NAI::IPathNetwork *pNet, vector<NAI::SPathPlace> *pRes )
{
	SSphere s;
	s.ptCenter = ptTarget; 
	s.fRadius = 5; // CRAP need more grounded number then this
	vector<NAI::SPathPlace> res;
	pNet->GetNearPlaces( s, &res );
	NAI::SPosition from;
	from.SetNetwork( pNet );
	for ( int k = 0; k < res.size(); ++k )
	{
		if ( !pNet->IsNativePassable( res[k] ) )
			continue;
		from.p = res[k];
		CVec3 vFrom = from.GetCP();
		if ( fabs( vFrom.x - ptTarget.x ) <= 0.5f && fabs( vFrom.y - ptTarget.y ) <= 0.5f )
			continue;
		if ( IsWithinHumanReach( vFrom, ptTarget, F_HEAL_DISTANCE ) )
			GetDirectedPoints( pNet, from.p, ptTarget, pRes, PM_STAND | PM_CROUCH );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdUntrapObject *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pTarget ) )
		return UCR_NO_TARGET;
	CDynamicCast<CWindowDoor> pTarget(pCmd->pTarget);
	if (pTarget)
	{
		CDynamicCast<IGetApproaches> pAppr( pCmd->pTarget );
		return GetActionValidPlaces( pUS, pAppr, pRes );
	}
	CDynamicCast<CMine> pMine(pCmd->pTarget);
	if (pMine)
	{
		GetMinePlaces(pMine->GetMinePos(), pUS->GetWorld()->GetPathNetwork(), pRes );
		return UCR_OK;
	}
	ASSERT(0);
	return UCR_GENERAL_FAILURE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdSetMineOnTile *pCmd, vector<NAI::SPathPlace> *pRes )
{
	GetMinePlaces( pCmd->ptDst.GetCP(), pUS->GetWorld()->GetPathNetwork(), pRes );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdCannon *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pObject ) )
		return UCR_NO_TARGET;

	CDynamicCast<IGetApproaches> pAppr( pCmd->pObject );
	return GetActionValidPlaces( pUS, pAppr, pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdOpenClose *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pObject ) )
		return UCR_NO_TARGET;

	CDynamicCast<IGetApproaches> pAppr( pCmd->pObject );
	return GetActionValidPlaces( pUS, pAppr, pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdUsePassage *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pPassageObject ) )
		return UCR_NO_TARGET;
	//
	pCmd->pPassageObject->GetObjectApproaches( pRes );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdTakeCorpse *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pCorpse ) )
		return UCR_NO_TARGET;

	CDynamicCast<CUnitServer> pDeadUnit( pCmd->pCorpse );
	NAI::IPathNetwork *pNet = pUS->GetWorld()->GetPathNetwork();
	CVec3 ptTarget(0,0,0);
	pUS->GetWorld()->GetAIMap()->GetUnitHLPos( &ptTarget, pUS->GetWorld()->GetAIMap()->GetHull(pDeadUnit), -1 );
	SSphere s;
	s.ptCenter = ptTarget - CVec3(0,0,0.5f);
	s.fRadius = 1; // CRAP need more grounded number then this
	if ( pDeadUnit->IsEmptyPK() )
		s.fRadius = 1.5f;
	vector<NAI::SPathPlace> res;
	pNet->GetNearPlaces( s, &res );
	for ( int k = 0; k < res.size(); ++k )
		GetDirectedPoints( pNet, res[k], ptTarget, pRes, PM_STAND );

	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.1 0x7940f0 / v1.2 0x794340: keep the carrier's tile and pose,
// collect every direction in which the corpse's swept sphere is unobstructed.
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdDropCorpse *pCmd, vector<NAI::SPathPlace> *pRes )
{
	if ( !IsValid( pCmd->pCorpse ) )
		return UCR_NO_TARGET;
	NAI::SUnitPosition from = pUS->GetPosition();
	EUnitCommandResult result = UCR_GENERAL_FAILURE;
	for ( int nDir = 0; nDir < 8; ++nDir )
	{
		from.pos.p.SetDirection( nDir );
		if ( CanDropCorpse( from, pUS->GetWorld()->GetAIMap() ) )
		{
			pRes->push_back( from.pos.p );
			result = UCR_OK;
		}
	}
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetMoveIItemValidPlaces( CUnitServer *pUS, CCmdMoveInventoryItem *pCmd, vector<NAI::SPathPlace> *pRes, float fPlaneDist )
{
	if ( pCmd->GetSource().eType == SItem::GROUND )
	{
		if ( !IsValid( pCmd->GetSource().pWorldItem ) )
			return UCR_NO_TARGET;

		CVec3 ptTo = pCmd->GetSource().pWorldItem->GetPos();
		// Retail v1.2 0x794520..0x7945e7: a nearby pickup keeps the current
		// pose and heading, including items above/below the unit within these bounds.
		CVec2 delta = pUS->GetPosition().GetCPNoHeight() - CVec2( ptTo.x, ptTo.y );
		float fHeight = pUS->GetPosition().GetCP().z - ptTo.z;
		if ( fabs( delta ) * 1.6f <= 2.0f && fHeight < 0.5f && fHeight > -2.0f )
			pRes->push_back( pUS->GetPosition().pos.p );
		else
			GetHumanReachPlaces( pUS, ptTo, pRes, fPlaneDist );
		return UCR_OK;
	}

	GetHumanReachPlaces( pUS, pCmd->GetTarget().pUnit->GetPosition().GetCP(), pRes, fPlaneDist + 0.1f );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdReload *pCmd, vector<NAI::SPathPlace> *pRes )
{
	// Retail v1.2 0x79b62e..0x79b669: reload keeps the current tile, pose and direction.
	pRes->push_back( pUS->GetPosition().pos.p );
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static EUnitCommandResult GetActionValidPlaces( CUnitServer *pUS, CCmdMoveInventoryItem *pCmd, vector<NAI::SPathPlace> *pRes )
{
	// Retail v1.2 0x794650: hint pickups use a larger initial reach.
	CDynamicCast<NRPG::IHintItem> pHint( pCmd->GetSource().pItem );
	return GetMoveIItemValidPlaces( pUS, pCmd, pRes, pHint ? 1.3f : 0.625f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCommand>
static EUnitCommandResult GetValidPlacesInDoubleRadius( CUnitServer *, TCommand *, vector<NAI::SPathPlace> * )
{
	return UCR_OK;
}
static EUnitCommandResult GetValidPlacesInDoubleRadius( CUnitServer *pUS, CCmdTalk *pCmd, vector<NAI::SPathPlace> *pRes )
{
	return GetTalkValidPlaces( pUS, pCmd, pRes, 1.6f );
}
static EUnitCommandResult GetValidPlacesInDoubleRadius( CUnitServer *pUS, CCmdMoveInventoryItem *pCmd, vector<NAI::SPathPlace> *pRes )
{
	return GetMoveIItemValidPlaces( pUS, pCmd, pRes, 1.6f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Execute creators
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TExec>
CCommandExecute* CreateSimpleAction( CUnitServer *pUS, TExec *pExec, EUnitCommandResult *pError )
{
	CPtr<TExec> p( pExec );
	*pError = pExec->CanDoIt( pUS->GetPosition() );
	if ( ( *pError != UCR_OK ) && ( *pError != UCR_NO_TARGET ) )
		return 0;
	return p.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateMoveExecutor( CUnitServer *pUS, NAI::CPath *pPath, NAI::EFindPathParams eParams,
	ENeedActiveItem eActive, EUnitCommandResult *pError, bool bCheckCanRotate )
{
	if ( !pUS->GetUnitRPG()->CanMove() )
	{
		*pError = UCR_GENERAL_FAILURE;
		return 0;
	}

	CExecQueue *pCommand = new CExecQueue( pUS );
	pCommand->AddPath( pPath, eParams, eActive, 0, bCheckCanRotate );
	return pCommand;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CCommandExecute* CreateActionExecMove( CUnitServer *pUS, const vector<NAI::SPathPlace> &dst, ENeedActiveItem eActive, EUnitCommandResult *pError )
{
	CWorld *pWorld = pUS->GetWorld();
	bool bStrafe = pUS->IsStrafing() && ( !pUS->IsWearingPK() );;
	CObj<NAI::CPath> pPath = FindPath( pWorld->GetPathNetwork(), pUS, pUS->GetPosition().pos.p,
		dst, 0, false, NAI::PF_USE_POSEDIR, bStrafe );
	if ( IsValid( pPath ) )
		return CreateMoveExecutor( pUS, pPath, NAI::PF_USE_POSEDIR, eActive, pError );

	*pError = UCR_PATH_NOT_FOUND;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TExecAction>
static CCommandExecute* CreateActionMoveTo( CUnitServer *pUS, TExecAction *pAction, ENeedActiveItem eActive, EUnitCommandResult *pError, vector<NAI::SPathPlace> &dst )
{
	if ( *pError == UCR_NO_TARGET )
		dst.push_back( pUS->GetPosition().pos.p );
	else if ( dst.empty() )
		return 0;

	NAI::SUnitPosition from = pUS->GetPosition();
	EUnitCommandResult eResult = UCR_GENERAL_FAILURE;
	vector<NAI::SPathPlace> spots;
	for ( unsigned int k = 0; k < dst.size(); ++k )
	{
		from.pos.p = dst[k];
		eResult = pAction->CanDoIt( from );
		if ( eResult == UCR_OK || eResult == UCR_NO_TARGET )
			spots.push_back( dst[k] );
	}
	if ( spots.empty() )
	{
		*pError = eResult;
		return 0;
	}
	return CreateActionExecMove( pUS, spots, eActive, pError );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCommand, class TExecAction>
static CCommandExecute* CreateActionQueue( CUnitServer *pUS, TCommand *pCmd, TExecAction *pAction, ENeedActiveItem eActive, EUnitCommandResult *pError, bool bTryDoubleRadius = false )
{
	CObj<CCommandExecute> pHold( pAction );
	CPtr<CCmd> pCmdHolder( pCmd );

	vector<NAI::SPathPlace> dst;
	*pError = GetActionValidPlaces( pUS, pCmd, &dst );
	// Retail v1.2 0x798d13..0x798d97: a matching action place needs no
	// path, provided the held-item state is already suitable. Ignore only
	// integral/final/moving bits, not facing or pose.
	bool bNeedMove = *pError != UCR_NO_TARGET;
	for ( unsigned int k = 0; bNeedMove && k < dst.size(); ++k )
		if ( NAI::IsSamePlace( pUS->GetPosition().pos.p, dst[k], 0xf9feffff ) )
			bNeedMove = false;
	if ( ( eActive == ITEM_ACTIVE && !pUS->animator.IsActiveItem() ) ||
		( eActive == ITEM_INACTIVE && pUS->animator.IsActiveItem() ) || pUS->GetUndrawItem() )
		bNeedMove = true;
	if ( !bNeedMove )
	{
		*pError = pAction->CanDoIt( pUS->GetPosition() );
		if ( *pError != UCR_OK && *pError != UCR_NO_TARGET )
			return 0;
		CExecQueue *pRes = new CExecQueue( pUS );
		pRes->AddExecutor( pAction );
		return pRes;
	}

	CCommandExecute *pMove = CreateActionMoveTo( pUS, pAction, eActive, pError, dst );
	if ( !pMove && bTryDoubleRadius )
	{
		// Retail v1.2 0x79bab4 / 0x79c10d: retry only after movement preparation
		// fails, not after the direct-action check. Do not retain the old candidates.
		vector<NAI::SPathPlace>().swap( dst );
		*pError = GetValidPlacesInDoubleRadius( pUS, pCmd, &dst );
		pMove = CreateActionMoveTo( pUS, pAction, eActive, pError, dst );
	}
	if ( !pMove )
		return 0;

	CDynamicCast<CExecQueue> pQueue(pMove);
	if (pQueue)
	{
		pQueue->AddExecutor( pAction );
		return pMove;
	}

	CExecQueue *pRes = new CExecQueue( pUS );
	pRes->AddExecutor( pMove );
	pRes->AddExecutor( pAction );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CCommandExecute* CreateAttackReload( CUnitServer *pUS, EUnitCommandResult *pError )
{
	// retail v1.1 0x794460 / v1.2 0x7946b0: replace the attack, not queue it after reload.
	CObj<CCmdReload> pCmd = new CCmdReload;
	CObj<CCommandExecute> pExec = pUS->CreateExecutor( pCmd, pError );
	if ( *pError == UCR_OK )
		*pError = UCR_OK_RELOAD;
	return pExec.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCommand, class TExecAction>
static CCommandExecute* CreateSimpleActionOrReload( CUnitServer *pUS, TCommand *pCmd,
	TExecAction *pAction, EUnitCommandResult *pError )
{
	CObj<CCommandExecute> pExec = CreateSimpleAction( pUS, pAction, pError );
	if ( *pError == UCR_NEED_RELOAD && pCmd->bCanBeReplacedByReload )
		return CreateAttackReload( pUS, pError );
	return pExec.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TCommand, class TExecAction>
static CCommandExecute* CreateActionQueueOrReload( CUnitServer *pUS, TCommand *pCmd,
	TExecAction *pAction, EUnitCommandResult *pError )
{
	CObj<CCommandExecute> pExec = CreateActionQueue( pUS, pCmd, pAction, ITEM_ACTIVE, pError );
	if ( *pError == UCR_NEED_RELOAD && pCmd->bCanBeReplacedByReload )
		return CreateAttackReload( pUS, pError );
	return pExec.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateActionExecutor( CUnitServer *pUS, CCmd *pCmd, EUnitCommandResult *pError )
{
	*pError = UCR_OK;
	CDynamicCast<CCmdReload> pReload( pCmd );
	if ( pReload )
	{
		// Retail v1.2 0x79e6ad/0x79e6ba: prepare an active item before reloading,
		// including drawing a holstered weapon without moving the unit.
		return CreateActionQueue( pUS, pReload.GetPtr(), new CExecReload( pUS, pReload->nSlot ), ITEM_ACTIVE, pError );
	}
	CDynamicCast<CCmdSwap> pSwap( pCmd );
	if ( pSwap )
	{
		CDynamicCast<CUnitServer> pTarget( pSwap->pTarget );
		if ( !IsValid( pTarget ) )
		{
			*pError = UCR_NO_TARGET;
			return 0;
		}
		return CreateSimpleAction( pUS, new CExecSwap( pUS, pTarget ), pError );
	}
	CDynamicCast<CCmdPlayAnimation> pAnim(pCmd);
	if (pAnim)
		return new CExecPlayAnimation(pUS, pAnim->nDBAnimationID, pAnim->bFreezeAfterLastFrame);
	else {
		CDynamicCast<CCmdShootTile> pAttackTile(pCmd);
		if (pAttackTile)
		{
			// retail @0x39d4f0: every attack branch opens with the world NoAttack gate (base zones) --
			// !IsAttackAllowed() (IWorld vtbl+0xc8, CWorld::bAttackAllowed) -> UCR_GENERAL_FAILURE, no executor
			if ( !pUS->GetWorld()->IsAttackAllowed() )
			{
				*pError = UCR_GENERAL_FAILURE;
				return 0;
			}
			EActionType eType = GetActionType(pUS);
			switch (eType)
			{
			case AT_NONE:
			case AT_FIRSTAID:
				*pError = UCR_UNAVAILABLE;
				return 0;
			case AT_MELEE:
			case AT_KNIFE:
			{
				NRPG::IUnitMission* pRPG = pUS->GetUnitRPG();
				ENeedActiveItem eActive = (pRPG->GetWeaponType() == NDb::WT_DEFAULT ? ITEM_INACTIVE : ITEM_ACTIVE);
				return CreateActionQueue(pUS, pAttackTile.GetPtr(), new CExecMeleeTile(pUS, pAttackTile->ptTarget), eActive, pError);
			}
			case AT_GRENADE:
			{
				CDynamicCast<NRPG::IGrenadeItemInfo> pGrenade(pUS->GetRPG()->GetInventoryInfo()->GetActive());
				if (IsValid(pGrenade) && (pGrenade->GetMode() == NRPG::GM_SETTRAP))
				{
					*pError = UCR_UNAVAILABLE;   // retail @0x39d4f0: set-trap-mode grenade on the tile path -> UCR_UNAVAILABLE(5), not GENERAL_FAILURE
					return 0;
				}

				return CreateActionQueue(pUS, pAttackTile.GetPtr(), new CExecThrowGrenade(pUS, pAttackTile->ptTarget), ITEM_ACTIVE, pError);
			}
			case AT_SHOOT:
			{
				// Retail v1.2 0x79d9bd..0x79da02 rejects tile targeting in
				// Snipe mode even before the unit enters the sniping state.
				CDynamicCast<NRPG::IWeaponItem> pWeapon(pUS->GetUnitRPG()->GetInventory()->GetActive());
				if ( IsValid( pWeapon ) && pWeapon->GetShootMode() == NDb::SM_Snipe )
				{
					*pError = UCR_UNAVAILABLE;
					return 0;
				}
				return CreateActionQueueOrReload(pUS, pAttackTile.GetPtr(), new CExecShootTile(pUS, pAttackTile->ptTarget), pError);
			}
			case AT_THROW:
				return CreateActionQueue(pUS, pAttackTile.GetPtr(), new CExecThrowKnife(pUS, pAttackTile->ptTarget + CVec3(0, 0, 0.5f)), ITEM_ACTIVE, pError);
			case AT_BAZOOKA:
				return CreateActionQueueOrReload(pUS, pAttackTile.GetPtr(), new CExecLaunchRocket(pUS, pAttackTile->ptTarget + CVec3(0, 0, 0.5f)), pError);
			case AT_CANNON:
				return CreateSimpleActionOrReload(pUS, pAttackTile.GetPtr(), new CExecShootTile(pUS, pAttackTile->ptTarget), pError);
			case AT_MINE:
				*pError = UCR_INVALID_COMMAND;
				return 0;
			default:
				// Retail v1.2 0x79d821: non-attack items (including keys and tools)
				// have no attack action, rather than an available but disabled one.
				*pError = UCR_UNAVAILABLE;
				return 0;
			}
		}
		else {
			CDynamicCast<CCmdShootObject> pAttackObject(pCmd);
			if (pAttackObject)
			{
				// retail @0x39d4f0 CCmdShootObject entry: the same world NoAttack gate (base zones)
				if ( !pUS->GetWorld()->IsAttackAllowed() )
				{
					*pError = UCR_GENERAL_FAILURE;
					return 0;
				}
				CDynamicCast<NWorld::CUnitServer> pUnitTarget(pAttackObject->pTarget);
				EActionType eType = GetActionType(pUS);
				if ( IsValid( pUnitTarget ) &&
					( eType == AT_SHOOT || eType == AT_CANNON || eType == AT_SNIPE ) &&
					pAttackObject->eHL != NAI::HL_ANY && !IsVisibleTarget( pUS, pUnitTarget ) )
				{
					*pError = UCR_CANT_SEE_TARGET;
					return 0;
				}
				CDynamicCast<NRPG::IWeaponItem> pWeapon(pUS->GetUnitRPG()->GetInventory()->GetActive());
				if (pWeapon)
				{
					if (IsValid(pWeapon) && pWeapon->GetShootMode() == NDb::SM_Snipe && !pUS->IsSniping())
					{
						if ( IsValid( pUnitTarget ) && !IsVisibleTarget( pUS, pUnitTarget ) )
						{
							*pError = UCR_CANT_SEE_TARGET;
							return 0;
						}
						return CreateActionQueueOrReload(pUS, pAttackObject.GetPtr(), new CExecSnipeAim(pUS, pUnitTarget), pError);
					}
				}

				if ( eType == AT_GRENADE )
				{
					CDynamicCast<NRPG::IGrenadeItemInfo> pGrenade(pUS->GetRPG()->GetInventoryInfo()->GetActive());
					if (IsValid(pGrenade) && (pGrenade->GetMode() == NRPG::GM_SETTRAP))
					{
						// Retail ShootObject rejects trap mode; its dedicated command is
						// queried separately by the order bar and trap cursor.
						*pError = UCR_UNAVAILABLE;
						return 0;
					}
				}

				if (IsValid(pUnitTarget) || !IsValid(pAttackObject->pTarget)) //// CRAP!: for CanDo
				{
					switch (eType)
					{
					case AT_NONE:
					case AT_FIRSTAID:
						*pError = UCR_UNAVAILABLE;
						return 0;
					case AT_MELEE:
					case AT_KNIFE:
					{
						NRPG::IUnitMission* pRPG = pUS->GetUnitRPG();
						ENeedActiveItem eActive = (pRPG->GetWeaponType() == NDb::WT_DEFAULT ? ITEM_INACTIVE : ITEM_ACTIVE);
						return CreateActionQueue(pUS, pAttackObject.GetPtr(), new CExecMeleeUnit(pUS, pUnitTarget, pAttackObject->eHL, pAttackObject->nExtraAttackAP), eActive, pError);
					}
					case AT_SHOOT:
					case AT_SNIPE:
						if ( eType == AT_SNIPE && IsValid( pUnitTarget ) && !IsVisibleTarget( pUS, pUnitTarget ) )
						{
							*pError = UCR_CANT_SEE_TARGET;
							return 0;
						}
						return CreateActionQueueOrReload(pUS, pAttackObject.GetPtr(), new CExecShootUnit(pUS, pUnitTarget, pAttackObject->eHL, pAttackObject->nExtraAttackAP), pError);
					case AT_CANNON:
						return CreateSimpleActionOrReload(pUS, pAttackObject.GetPtr(), new CExecShootUnit(pUS, pUnitTarget, pAttackObject->eHL, pAttackObject->nExtraAttackAP), pError);
					case AT_THROW:
						// Retail v1.2 0x79dc83 / 0x79dd35: turn toward the target
						// before throwing. A targetless order-bar query falls through
						// to the self-tile probe below; it must not construct a unit throw.
						if ( IsValid( pUnitTarget ) )
							return CreateActionQueue(pUS, pAttackObject.GetPtr(), new CExecThrowKnife(pUS, pAttackObject->eHL, pUnitTarget), ITEM_ACTIVE, pError);
						break;
					}
				}

				if ( IsValid( pAttackObject->pTarget ) && (eType == AT_SHOOT || eType == AT_CANNON) )
				{
					CExecShootObject *pShot = new CExecShootObject( pUS, pAttackObject->pTarget, pAttackObject->nExtraAttackAP );
					if ( eType == AT_CANNON )
						return CreateSimpleActionOrReload( pUS, pAttackObject.GetPtr(), pShot, pError );
					return CreateActionQueueOrReload( pUS, pAttackObject.GetPtr(), pShot, pError );
				}
				if ( IsValid( pAttackObject->pTarget ) && (eType == AT_MELEE || eType == AT_KNIFE) )
				{
					// Sentinels keeps the object target for melee too: do not reduce
					// doors/windows to an unfiltered swing at their (possibly empty) centre.
					// 0x7fe14d..0x7fe157 supplies the object; SS v1.2 0x79db52
					// instead passes null to its otherwise matching object executor.
					ENeedActiveItem eActive = pUS->GetUnitRPG()->GetWeaponType() == NDb::WT_DEFAULT
						? ITEM_INACTIVE : ITEM_ACTIVE;
					return CreateActionQueue( pUS, pAttackObject.GetPtr(),
						new CExecMeleeObject( pUS, pAttackObject->pTarget, pAttackObject->nExtraAttackAP ), eActive, pError );
				}
				// Explosives and other point-target actions still use a tile executor.
				if (!IsValid(pAttackObject->pTarget))
				{
					EActionType eType = GetActionType(pUS);
					if (eType == AT_BAZOOKA)
						return CreateSimpleAction(pUS, new CExecLaunchRocket(pUS, CExecLaunchRocket::TEST), pError);
					CObj<CCmdShootTile> pShoot(new CCmdShootTile(pUS->GetPosition().GetCP()));
					CCommandExecute* pExecutor = CreateActionExecutor(pUS, pShoot, pError);
					if (*pError == UCR_OK)
						*pError = UCR_NO_TARGET;

					return pExecutor;
				}

				CVec3 ptTarget(0, 0, 0);
				NAI::IAIMap* pAIMap = pUS->GetWorld()->GetAIMap();
				pAIMap->GetUnitHLPos(&ptTarget, pAIMap->GetHull(pAttackObject->pTarget), -1);
				CObj<CCmdShootTile> pShoot(new CCmdShootTile(ptTarget));
				// Firearm object attacks use the same retail reload policy as unit attacks.
				// Dev still represents their aiming executor as a tile shot (retail CExecShootObject).
				if ( eType == AT_SHOOT || eType == AT_CANNON )
					pShoot->bCanBeReplacedByReload = pAttackObject->bCanBeReplacedByReload;
				return CreateActionExecutor(pUS, pShoot, pError);
			}
			else {
				CDynamicCast<CCmdSetGrenadeOnObject> pSetTrap(pCmd);
				if (pSetTrap)
				{
					// retail @0x39d4f0 CCmdSetGrenadeOnObject entry: world NoAttack gate (base zones)
					if ( !pUS->GetWorld()->IsAttackAllowed() )
					{
						*pError = UCR_GENERAL_FAILURE;
						return 0;
					}
					if (GetActionType(pUS) != AT_GRENADE)
					{
						*pError = UCR_INVALID_COMMAND;
						return 0;
					}
					CDynamicCast<NWorld::CWindowDoor> pTarget(pSetTrap->pTarget);
					if (pSetTrap->pTarget == 0 || pTarget != 0)
						return CreateActionQueue(pUS, pSetTrap.GetPtr(), new CExecSetTrap(pUS, pTarget), ITEM_ACTIVE, pError);
					else
					{
						*pError = UCR_INVALID_COMMAND; // asked to set the trap on something that isn't a door
						return 0;
					}
				}
				else
				{
					CDynamicCast<CCmdUntrapObject> pDisarm(pCmd);
					if (pDisarm)
					{
						if (!IsValid(pDisarm->pTarget))
							return CreateActionQueue(pUS, pDisarm.GetPtr(), new CExecDisarmTrap(pUS, 0), ITEM_ACTIVE, pError);
						else
						{
							CDynamicCast<CWindowDoor> pDoor(pDisarm->pTarget);
							if (pDoor)
								return CreateActionQueue(pUS, pDisarm.GetPtr(), new CExecDisarmTrap(pUS, pDoor), ITEM_ACTIVE, pError);
							else {
								CDynamicCast<CMine> pMine(pDisarm->pTarget);
								if (pMine)
									return CreateActionQueue(pUS, pDisarm.GetPtr(), new CExecDisarmMine(pUS, pMine), ITEM_ACTIVE, pError);
								else
								{
									*pError = UCR_INVALID_COMMAND; // only mines and traps can be disarmed
									return 0;
								}
							}
						}
					}
					else {
						CDynamicCast<CCmdSetMineOnTile> pSetTrap(pCmd);
						if (pSetTrap)
						{
							// retail @0x39d4f0 CCmdSetMineOnTile entry: world NoAttack gate (base zones)
							if ( !pUS->GetWorld()->IsAttackAllowed() )
							{
								*pError = UCR_GENERAL_FAILURE;
								return 0;
							}
							if (GetActionType(pUS) != AT_MINE)
							{
								*pError = UCR_INVALID_COMMAND;
								return 0;
							}
							return CreateActionQueue(pUS, pSetTrap.GetPtr(), new CExecSetMine(pUS, pSetTrap), ITEM_ACTIVE, pError);
						}
						else {
							CDynamicCast<CCmdHeal> pHeal(pCmd);
							if (pHeal)
							{
								CDynamicCast<NWorld::CUnitServer> pTarget(pHeal->pTarget);
								return CreateActionQueue(pUS, pHeal.GetPtr(), new CExecHeal(pUS, pTarget), ITEM_ACTIVE, pError);
							}
							else {
								CDynamicCast<CCmdCannon> pCannonAtk(pCmd);
								if (pCannonAtk)
									return CreateActionQueue(pUS, pCannonAtk.GetPtr(), new CExecCannon(pUS, pCannonAtk->pObject, true, pCannonAtk.GetPtr()), ITEM_INACTIVE, pError);   // retail: the exec carries the reserving cmd (@0x3a5e50)
								else {
									CDynamicCast<CCmdExitCannon> pCannonExit(pCmd);
									if (pCannonExit)
										return CreateSimpleAction(pUS, new CExecCannon(pUS, pCannonExit->pCannon, false), pError);
									else {
										CDynamicCast<CCmdOpenClose> pOpenClose(pCmd);
										if (pOpenClose)
										{
											CCommandExecute* pExec = CreateActionQueue(pUS, pOpenClose.GetPtr(),
												new CExecOpenClose(pUS, pOpenClose), ITEM_NO_MATTER, pError);
											CDynamicCast<CExecQueue> pQueue(pExec);
											if (pQueue)
												pQueue->CheckOpenCloseOnce();
											else if (pExec != 0)
											{
												ASSERT(0);
											}
											return pExec;
										}
										else {
											CDynamicCast<CCmdUsePassage> pUsePassage(pCmd);
											if (pUsePassage)
												return CreateActionQueue(pUS, pUsePassage.GetPtr(), new CExecUsePassage(pUS, pUsePassage), ITEM_INACTIVE, pError);
											else {
												CDynamicCast<CCmdCreateInventoryItem> pCreateItem(pCmd);
												if (pCreateItem)
													return CreateSimpleAction(pUS, new CExecCreateInventoryItem(pUS, pCreateItem), pError);
												else {
													// (CCmdCreateAndActivateInventoryItem AND CCmdExchangeInventoryItems are handled in
													//  CreateExecutor -- retail @0x3b37b0 builds CExecQueues of registered execs there)
													CDynamicCast<CCmdMoveInventoryItem> pMoveItem(pCmd);
													if (pMoveItem)
													{
														if (pMoveItem->GetSource().eType == SItem::GROUND)
															return CreateActionQueue(pUS, pMoveItem.GetPtr(), new CExecMoveInventoryItem(pUS, pMoveItem), ITEM_NO_MATTER, pError, true);
														else if ((pMoveItem->GetSource().eType == SItem::STORAGE) || (pMoveItem->GetTarget().eType == SItem::STORAGE))
															return CreateSimpleAction(pUS, new CExecMoveInventoryItem(pUS, pMoveItem), pError);
														else if (IsValid(pMoveItem->GetSource().pUnit) && IsValid(pMoveItem->GetTarget().pUnit) &&
															(pMoveItem->GetSource().pUnit != pMoveItem->GetTarget().pUnit))
														{
															CDynamicCast<CUnitServer> pUSSource(pMoveItem->GetSource().pUnit);
															return CreateActionQueue(pUSSource, pMoveItem.GetPtr(), new CExecMoveInventoryItem(pUSSource, pMoveItem),
																ITEM_NO_MATTER, pError, true);
														}

														return CreateSimpleAction(pUS, new CExecMoveInventoryItem(pUS, pMoveItem), pError);
													}
													else {
														CDynamicCast<CCmdTakeCorpseOnDeploy> pCmdCorpse(pCmd);
														if (pCmdCorpse)
															return new CExecTakeCorpseOnDeploy(pCmdCorpse->pCarrier, pCmdCorpse->pCorpse);
														else {
															CDynamicCast<CCmdTakeCorpse> pCmdCorpse(pCmd);
															if (pCmdCorpse)
															{
																CDynamicCast<CUnitServer> pDeadUnit(pCmdCorpse->pCorpse);
																if (pDeadUnit->IsEmptyPK())
																	return CreateActionQueue(pUS, pCmdCorpse.GetPtr(), new CExecPanzerklein(pUS, pCmdCorpse), ITEM_INACTIVE, pError);
																return CreateActionQueue(pUS, pCmdCorpse.GetPtr(), new CExecCorpse(pUS, pDeadUnit, true, pCmdCorpse.GetPtr()), ITEM_INACTIVE, pError);   // retail: the exec carries the reserving cmd (@0x3a6160)
															}
															else {
																CDynamicCast<CCmdDropCorpse> pCmdCorpse(pCmd);
																if (pCmdCorpse)
																{
																	CDynamicCast<CUnitServer> pDeadUnit(pCmdCorpse->pCorpse);
																	// v1.2 0x79eb5c..0x79eb6e: turn to a valid drop facing before the action.
																	return CreateActionQueue(pUS, pCmdCorpse.GetPtr(), new CExecCorpse(pUS, pDeadUnit, false), ITEM_INACTIVE, pError);
																}
																else {
																	CDynamicCast<CCmdExitPK> pExitPK(pCmd);
																	if (pExitPK)
																		return CreateSimpleAction(pUS, new CExecPanzerklein(pUS, 0), pError);
																	else {
																		CDynamicCast<CCmdCollectSnipeAP> pCollectSnipeAP(pCmd);
																		if (pCollectSnipeAP)
																			return CreateSimpleAction(pUS, new CExecCollectSnipeAP(pUS, pCollectSnipeAP->eAP), pError);
																		else {
																			CDynamicCast<CCmdTalk> pTalk(pCmd);
																			if (pTalk)
																			{
																				CDynamicCast<CUnitServer> pTarget(pTalk->pTarget);
																				return CreateActionQueue(pUS, pTalk.GetPtr(), new CExecTalk(pUS, pTarget), ITEM_NO_MATTER, pError, true);
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	*pError = UCR_INVALID_COMMAND;
	ASSERT( 0 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
