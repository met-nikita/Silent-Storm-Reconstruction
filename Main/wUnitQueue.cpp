#include "StdAfx.h"
#include "wUnitQueue.h"
#include "aiPath.h"
#include "wMain.h"
#include "wOSBase.h"
#include "wObject.h"
#include "..\DBFormat\DataAI.h"
#include "..\DBFormat\DataRPG.h"    // NDb::CRPGKey (HasKey @0x3bcfb0 key-number match)
#include "..\DBFormat\DataMisc.h"   // NDb::CRPGPicklock (the pick attempt @0x3bdb70)
#include "..\Misc\RandomGen.h"      // SRand (the tick-seeded local pick roll)
#include "..\MiscDll\LogStream.h"   // csSystem pick-attempt log lines (retail verbatim)
#include "RPGUnitInfo.h"
#include "RPGUnitMission.h"         // NRPG::IUnitMission (GetInventory/GetSkillValue/HasPerk)
#include "RPGItemSet.h"             // NRPG::CPicklockItem (WorkingPicklock @0x3bcd60)
#include "RPGMedals.h"              // NRPG::MPC_PICK_LOCK
#include "RPGUnit.h"                // NRPG::CUnit::AddMedalPoints

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSimpleExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AddExecutor( CCommandExecute *pExec )
{
	ASSERT( pExec->GetUnitServer() == pUS );
	execList.push_back( pExec );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AddFrontExecutor( CCommandExecute *pExec )
{
	ASSERT( pExec->GetUnitServer() == pUS );
	execList.push_front( pExec );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleExecQueue::GetStartAP() const 
{ 
	if ( !execList.empty() )
		return execList.front()->GetStartAP();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleExecQueue::GetActionAP() const 
{ 
	int nTemp = 0;
	NAI::SUnitPosition pos = pUS->GetPosition(), dst = pos;
	for( list<CObj<CCommandExecute> >::const_iterator iTemp = execList.begin(); iTemp != execList.end(); iTemp++ )
	{
		nTemp += (*iTemp)->GetActionAP();
		CObj<NAI::CPath> pPath = (*iTemp)->GetCurrentPath();
		if ( pPath )
			dst.pos.p = pPath->points.back();
		pUS->SetTemporaryPosition( dst );
	}
	pUS->SetTemporaryPosition( pos );
	return nTemp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::Run()
{
	while ( !execList.empty() )
	{
		CObj<CSimpleExecQueue> pHold( this ); // in case the unit dies while the action is being executed
		execList.front()->Run();
		EFinishType f = execList.front()->GetState();
		if ( f == RUNNING )
			return;
		if ( f == FAILED )
		{
			Failed();
			execList.clear();
			return;
		}
		ASSERT( f == FINISHED );
		execList.pop_front();
		if ( !pUS->CanSpendAP( GetStartAP() ) )
			return;
	}
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::TimeLabelReached() 
{ 
	if ( !execList.empty() )
		return execList.front()->TimeLabelReached();
	return false; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::AnimationFinished() 
{ 
	if ( !execList.empty() )
	{
		execList.front()->AnimationFinished();
		EFinishType f = execList.front()->GetState();
		if ( f != RUNNING )
		{
			if ( f == FAILED )
			{
				Failed();
				execList.clear();
				return;
			}
			ASSERT( f == FINISHED );
			execList.pop_front();
			if ( pUS->CanSpendAP( GetStartAP() ) )
				Run();
			return;
		}
		else
			return;
	}
	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::Cancel()
{
	if ( !execList.empty() )
	{
		CCommandExecute *pFront = execList.front();
		pFront->Cancel();
		if ( pFront->GetState() == FINISHED )
			Finished();
		else if ( pFront->GetState() == FAILED )
			Failed();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::IsExecuting()
{ 
	if ( !execList.empty() )
		return execList.front()->IsExecuting();
	return true; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleExecQueue::IsWaitingForPath( NAI::SUnitPosition *p )
{
	if ( !execList.empty() )
		return execList.front()->IsWaitingForPath( p );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleExecQueue::Segment()
{
	if ( !execList.empty() )
		execList.front()->Segment();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathConflictsRemover* CExecQueue::GetPathConflictsRemover()
{
	// @0x3bd240 -- the remover of the first execList entry that has one (movers return theirs, others 0).
	list<CObj<CCommandExecute> >::iterator it;
	for ( it = execList.begin(); it != execList.end(); ++it )
	{
		CPathConflictsRemover *pcr = (*it)->GetPathConflictsRemover();
		if ( pcr )
			return pcr;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CPath* CSimpleExecQueue::GetCurrentPath() const 
{ 
	if ( !execList.empty() )
	{
		NAI::CPath *pRes = 0;
		list<CObj<CCommandExecute> >::const_iterator it;
		for ( it = execList.begin(); it != execList.end(); ++it )
		{
			CObj<NAI::CPath> pCurrent = (*it)->GetCurrentPath();
			if ( !pCurrent )
				continue;
			if ( !pRes )
			{
				pRes = new NAI::CPath;
				pRes->pNet = pUS->GetWorld()->GetPathNetwork();
				pRes->points.push_back( pUS->GetUnitPosition().pos.p );
				pRes->bStrafePath = pCurrent->bStrafePath;
			}
			for ( int i = 0; i < pCurrent->points.size(); ++i )
			{
				if ( !( pCurrent->points[i] == pUS->GetUnitPosition().pos.p ) )
					pRes->points.push_back( pCurrent->points[i] );
			}
		}
		return pRes;
	}
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecQueue
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetSearchFromPosition( NAI::SPathPlace *pRes )
{
	if ( !execList.empty() )
	{
		CCommandExecute *pFront = execList.front();
		CDynamicCast<IExecMove> pMove(pFront);
		if (pMove)
		{
			pMove->GetSearchFromPosition( pRes );
			return;
		}
	}
	*pRes = pUS->GetPosition().pos.p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams, ENeedActiveItem *pActive )
{
	// @0x3bd180 -- seed the current place + defaults, then let each queued mover refine (last wins). pActive
	// (retail's added active-item out-param) is forwarded verbatim.
	*pRes = pUS->GetPosition().pos.p;
	*pParams = NAI::PF_DEFAULT;
	*pActive = ITEM_NO_MATTER;
	list<CObj<CCommandExecute> >::iterator i;
	for ( i = execList.begin(); i != execList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
			pMove->GetDesiredPlace( pRes, pParams, pActive );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::FullCancel()
{
	if ( !execList.empty() )
	{
		CPtr<CCommandExecute> pFront = execList.front();
		CDynamicCast<IExecMove> pMove(pFront);
		if (pMove)
			pMove->FullCancel();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::GetPathPoints( list<SPathPoint> *pRes )
{
	for ( list<CObj<CCommandExecute> >::iterator i = execList.begin(); i != execList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
			pMove->GetPathPoints( pRes );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive )
{
	CObj<CCommandExecute> pHold = execList.front(); 
	CDynamicCast<IExecMove> pOldCommand( pHold );
	execList.pop_front();
	list<CObj<CCommandExecute> > oldExecList = execList;
	execList.clear();
	if ( !pOldCommand )
		pHold->Cancel();
	AddPath( pPath, _eParams, eActive, pOldCommand );
	list<CObj<CCommandExecute> >::iterator i, firstToAdd = oldExecList.end(); 
	NAI::SPathPlace p;
	NAI::EFindPathParams params;
	ENeedActiveItem active = ITEM_NO_MATTER;
	for ( i = oldExecList.begin(); i != oldExecList.end(); ++i )
	{
		CDynamicCast<IExecMove> pMove(*i);
		if (pMove)
		{
			pMove->GetDesiredPlace( &p, &params, &active );
			if ( p == pPath->points.back() && _eParams == params )
			{
				firstToAdd = i;
				++firstToAdd;
				break;
			}
		}
	}
	for ( i = firstToAdd; i != oldExecList.end(); ++i )
		execList.push_back( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CanSimplyOpen( NAI::IPathNetwork *pNet, NAI::SPathPlace &from, IWindowDoor *pDoor, int *pNewDir, bool bOpen )
{
	vector<NAI::SPathPlace> res;
	CDynamicCast<IGetApproaches> pAppr( pDoor );
	pAppr->GetApproaches( &res, pNet );
	int nDir = from.GetDirection(), nBestDiff = 7;
	int &nBestDir = *pNewDir;
	for ( int nRes = 0; nRes < res.size(); ++nRes )
	{
		int nNewDir = pNet->GetClosestDir( from, res[ nRes ] );
		int nDiff = abs( nNewDir - nDir ); 
		if ( nDiff > 4 )
			nDiff = 8 - nDiff;
		if ( nDiff < nBestDiff )
		{
			nBestDiff =  nDiff;
			nBestDir = nNewDir;
		}
	}

	CVec3 ptToOpen = pDoor->GetChangeStateDirection( bOpen );
	NAI::SPosition pos;
	pos.SetNetwork( pNet );
	pos.p = from;
	//pos.p.SetDirection( nBestDir );
	float fDir = pos.GetDirection();
	float fScalarProd = ptToOpen.x * cos( fDir ) + ptToOpen.y * sin( fDir );
	return fScalarProd > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::AddPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, IExecMove *pOldFront,
	bool bCheckCanRotate )
{
	// break the path apart into segments
	CPtr<NAI::CPath> pSimplePath = new NAI::CPath;
	pSimplePath->pNet = pPath->pNet;
	pSimplePath->bStrafePath = pPath->bStrafePath;
	int nWaitAction = 0;
	bool bFirstPath = true;
	for ( int i = 0; i < pPath->points.size(); ++i )
	{
		NAI::SPathPlace p( pPath->points[i] );
		pSimplePath->points.push_back( p );
		if ( nWaitAction < pPath->actions.size() )
		{
			NAI::CPath::SPathAction &action = pPath->actions[nWaitAction];
			bool bActionNeeded = false;
			/*if ( !pPath->bStrafePath )
				bActionNeeded = action.where == p;
			else*/
				bActionNeeded = action.where.GetX() == p.GetX() && action.where.GetY() == p.GetY() 
				&& action.where.GetLayer() == p.GetLayer();
			int nBestDir;
			if ( !bActionNeeded )
				continue;

			CDynamicCast<IWindowDoor> pDoor( action.pObject );
			if ( CanSimplyOpen( pPath->pNet, action.where, pDoor, &nBestDir, action.action == NAI::PA_OPEN ) )
			{
				pSimplePath->actions.push_back( action );
				++nWaitAction;
				continue;
			}

			// add an extra turn point so the character opens the door from the correct side
			if ( nBestDir != p.GetDirection() )
			{
				NAI::SPathPlace newPoint( p );
				newPoint.SetDirection( nBestDir );
				pSimplePath->points.push_back( newPoint );
				p = newPoint;
			}

			CDynamicCast<IObject> pObj( action.pObject );
			if ( !pObj )
			{
				ASSERT(0);
				return;
			}
			bool bOpen = ( action.action == NAI::PA_OPEN );
			CPtr<CCmdOpenClose> pOpen = new CCmdOpenClose( pObj, bOpen );

			// move
			if ( !bFirstPath || !pOldFront )
			{
				CCommandExecute *pMove = CreateSimpleMoveExecutor( pUS, pSimplePath, _eParams, ITEM_NO_MATTER, bCheckCanRotate );
				if ( pMove ) 
					execList.push_back( pMove );
				else
				{
					ASSERT(0);
					return;
				}
			}
			else
			{
				pOldFront->SetNewPath( pSimplePath, _eParams, ITEM_NO_MATTER );
				CDynamicCast<CCommandExecute> pPush( pOldFront );
				execList.push_back( pPush.GetPtr() );
			}
			bFirstPath = false;
	
			// check if door is trapped and set off it if player does not see danger
/*			if ( CDynamicCast<CWindowDoor> pDoor( action.pObject ) )
			{
				if ( pDoor->IsMineSet() && !pUS->GetTBSPlayer()->CanSeeTrap( pDoor ) )
					execList.push_back( new CExecBlowTrappedDoor( pUS, pDoor ) );
			}*/

			// open - close door
			CCommandExecute *pOpenClose = new CExecOpenClose( pUS, pOpen );
			execList.push_back( pOpenClose );
			// again
			pSimplePath = new NAI::CPath;
			pSimplePath->pNet = pPath->pNet;
			pSimplePath->bStrafePath = pPath->bStrafePath;
			pSimplePath->points.push_back( p );
			++nWaitAction;
		}
	}
	// last move
	if ( !bFirstPath || !pOldFront )
	{
		CCommandExecute *pMove = CreateSimpleMoveExecutor( pUS, pSimplePath, _eParams, eActive, bCheckCanRotate );
		if ( pMove ) 
			execList.push_back( pMove );
		else
			ASSERT(0);
	}
	else
	{
		pOldFront->SetNewPath( pSimplePath, _eParams, eActive );
		CDynamicCast<CCommandExecute> pPush( pOldFront );
		execList.push_back( pPush.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecQueue::CheckOpenCloseOnce()
{
	if ( execList.empty() )
		return;
	CCommandExecute *pExec = execList.back();
	CDynamicCast<CExecOpenClose> pOpen(pExec);
	if (pOpen)
	{
		bool bOpen; 
		IObject *pObj; 
		pOpen->GetParams( &bOpen, &pObj );
		list<CObj<CCommandExecute> >::iterator it;
		for ( it = execList.begin(); it != execList.end(); ++it )
		{
			CDynamicCast<CExecOpenClose> pOpenBefore( *it );
			if ( !pOpenBefore )
				continue;
			bool bOpenBefore;
			IObject *pObjectBefore; 
			pOpenBefore->GetParams( &bOpenBefore, &pObjectBefore );
			if ( bOpenBefore == bOpen && pObjectBefore == pObj )
			{
				++it;
				execList.erase( it, execList.end() );
				return;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecOpenClose -- retail grew the locked-door key/picklock flow on top of the Jan03 open/close:
// CanDoIt @0x3bd290, GetStartAP @0x3bd340, Run @0x3bd3f0, TimeLabelReached @0x3bdb70, helpers
// HasKey @0x3bcfb0 / WorkingPicklock @0x3bcd60, GetActive @0x3bcdb0. (Retail also registers a
// vestigial CCmdPickLock class NOTHING ever creates -- the whole flow lives here; not ported.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::HasKey @0x3bcfb0: does the inventory hold the key matching the door's key number?
// Searches the BACKPACK items then the TWO equip slots ONLY (belts / the drag holder are not
// searched); the match is CRPGKey::nKeyID (the Key table's "KeyID" column) == the door's nKeyID.
static bool HasKey( int nKeyID, NRPG::IInventory *pInventory )
{
	const vector<NRPG::SBackPackItem> &items = pInventory->GetItems();
	for ( int nTemp = 0; nTemp < (int)items.size(); ++nTemp )
	{
		CDynamicCast<NRPG::IKeyItem> pKey( items[nTemp].pItem );
		if ( IsValid( pKey ) && pKey->GetDBItemInfo()->nKeyID == nKeyID )
			return true;
	}
	for ( int nSlot = 0; nSlot <= 1; ++nSlot )   // retail: SLOT_1 / SLOT_2
	{
		CDynamicCast<NRPG::IKeyItem> pKey( pInventory->Get( (NDb::ESlot)nSlot ) );
		if ( IsValid( pKey ) && pKey->GetDBItemInfo()->nKeyID == nKeyID )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::WorkingPicklock @0x3bcd60: the given item is a picklock with charges left.
// Always probed on the ACTIVE item -- a picklock in the backpack does not count.
static NRPG::CPicklockItem* WorkingPicklock( NRPG::IInventoryItem *pItem )
{
	CDynamicCast<NRPG::CPicklockItem> pPick( pItem );
	if ( IsValid( pPick ) && pPick->GetIncQuantity() >= 1 )
		return pPick.GetPtr();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExecOpenClose::CExecOpenClose( CUnitServer *_pUS, CCmdOpenClose *_pCmd )
: CCommandExecute(_pUS), pCmd(_pCmd)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3bcdb0 (retail-new virtual): the item the executor works with = the inventory's active item
NRPG::IInventoryItem* CExecOpenClose::GetActive() const
{
	return pUS->GetUnitRPG()->GetInventory()->GetActive();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecOpenClose::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	// ... here we check some conditions about window/door
	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	ASSERT( pOS );
	if ( !pOS )
		return UCR_GENERAL_FAILURE;
	// retail @0x3bd290: the locked-door probe -- key in inventory or a charged picklock in the
	// ACTIVE hand allows the command; otherwise UCR_DOOR_LOCKED (blocked-but-available in the UI).
	if ( !pOS->IsLockedDoor() )
		return UCR_OK;
	NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
	if ( HasKey( pOS->GetKeyID(), pInventory ) )
		return UCR_OK;
	if ( WorkingPicklock( pInventory->GetActive() ) )
		return UCR_OK;
	return UCR_DOOR_LOCKED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecOpenClose::GetStartAP() const
{
	// retail @0x3bd340: key -> AC_USE_KEY (RPGAP record 9), picklock -> AC_PICK_LOCK (the picklock
	// record's nAPToUse), plain -> AC_OPEN_CLOSE.
	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	if ( IsValid( pOS ) && pOS->IsLockedDoor() )
	{
		NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
		if ( HasKey( pOS->GetKeyID(), pInventory ) )
			return pUS->GetActionAP( NRPG::AC_USE_KEY );
		if ( WorkingPicklock( pInventory->GetActive() ) )
			return pUS->GetActionAP( NRPG::AC_PICK_LOCK );
	}
	return pUS->GetActionAP( NRPG::AC_OPEN_CLOSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecOpenClose::Run()
{
	ASSERT( CanDoIt( pUS->GetPosition() ) == UCR_OK );

	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	// retail @0x3bd3f0: per-case AP spend
	if ( !IsValid( pOS ) || !pOS->IsLockedDoor() )
		pUS->DoAction( NRPG::AC_OPEN_CLOSE );
	else
	{
		NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
		// ORIGINAL QUIRK (retail, no else between the two): key in inventory AND a working
		// picklock in hand runs BOTH DoActions -- a double AP spend. Reproduced 1:1.
		if ( HasKey( pOS->GetKeyID(), pInventory ) )
			pUS->DoAction( NRPG::AC_USE_KEY );
		if ( WorkingPicklock( pInventory->GetActive() ) )
			pUS->DoAction( NRPG::AC_PICK_LOCK );
	}
	// retail: door already in the requested state -> finish without the animation
	if ( IsValid( pOS ) && pCmd->bOpen == pOS->IsOpen() )
	{
		Finished();
		return;
	}
	pUS->animator.OpenWindowDoor( pUS->GetPosition() );
	StartAction( pUS->GetWorld(), SKIPPABLE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3bdb70 -- the time label: the key silently unlocks (nothing consumed); a picklock runs
// the pick attempt (charge spent BEFORE the roll, tick-seeded local SRand, skill formula, burglar
// perk 52 bonus, MPC_PICK_LOCK medal on success); the plain/unlocked tail opens the door + AI sound.
bool CExecOpenClose::TimeLabelReached()
{
	CDynamicCast<CWindowDoor> pOS( pCmd->pObject );
	ASSERT( pOS );
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	if ( pOS->IsLockedDoor() )
	{
		NRPG::IInventory *pInventory = pRPG->GetInventory();
		if ( HasKey( pOS->GetKeyID(), pInventory ) )
		{
			pOS->LockDoor( false, 0, 1 );   // unlock; the key stays in the inventory
		}
		else
		{
			NRPG::CPicklockItem *pPick = WorkingPicklock( pInventory->GetActive() );
			if ( !pPick )
				return false;               // retail: nothing happens -- no sound, door stays locked
			pPick->SpendCharge();           // retail: the charge is spent BEFORE the roll
			int nSkill = pRPG->GetSkillValue( NDb::ST_ENGINEERING );
			SRand rnd;                      // retail: a fresh tick-seeded LOCAL rng, not the synced game rng
			int nTry = ( rnd.Get( 100 ) * nSkill ) / 100 + pPick->GetDBPicklock()->nAddToEngSkill + nSkill / 2;
			csSystem << CC_GREEN << L"Picklock used: skill " << nSkill << L", item bonus " << pPick->GetDBPicklock()->nAddToEngSkill << endl;
			if ( pPick->GetIncQuantity() < 1 )
			{
				// the drained picklock is ripped out of the hand and destroyed (retail: TakeOff + ReleaseObj
				// + animator active-item clear; dev's Update() refreshes the same visuals)
				CObj<NRPG::IInventoryItem> pErase = pInventory->TakeOff( (NDb::ESlot)pInventory->GetActiveSlot() );
				pUS->Update();
			}
			float fBonus = 0;
			if ( pRPG->HasPerk( 52, &fBonus ) )   // the burglar perk (DB id 52)
			{
				csSystem << CC_GREEN << L"Unit has perk burglar, added " << fBonus << L" to try number" << endl;
				nTry = (int)( nTry + fBonus );    // retail truncates (fldcw RC=CHOP)
			}
			csSystem << CC_WHITE << L"Total try number " << nTry << endl;
			csSystem << CC_GREEN << L"Lock hardness " << pOS->GetLockHardness() << endl;
			if ( nTry < pOS->GetLockHardness() )
				return false;                     // FAIL: charge lost, door stays locked and closed
			pRPG->GetRPGUnit()->AddMedalPoints( pUS->GetWorld()->GetGlobalGame(), NRPG::MPC_PICK_LOCK, (float)pOS->GetLockHardness() );
			pOS->LockDoor( false, 0, 1 );
		}
	}
	pOS->OpenClose( pCmd->bOpen, false, pUS );   // unlocking does NOT auto-open -- the exec opens it
	//
	NDb::CAISound *pAISound;
	if ( pCmd->bOpen )
		pAISound = NDb::GetAISound( 23 );
	else
		pAISound = NDb::GetAISound( 25 );
	NDb::SAISound sound = { pAISound, 0, 1.0f };   // retail @0x3bdb70: no silencer on open/close
	pUS->GetWorld()->MakeAISound( sound, pUS, 0 );
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecBlowTrappedDoor
////////////////////////////////////////////////////////////////////////////////////////////////////
/*CExecBlowTrappedDoor::CExecBlowTrappedDoor( CUnitServer *_pUS, CWindowDoor *_pTarget )
	: CCommandExecute(_pUS), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecBlowTrappedDoor::CanDoIt( const NAI::SUnitPosition &from, bool bIgnoreTarget ) const
{
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecBlowTrappedDoor::GetStartAP() const
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecBlowTrappedDoor::Run()
{
	if ( IsValid(pTarget) )
		pTarget->GoBoom();
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x00122174, CExecQueue )
REGISTER_SAVELOAD_CLASS( 0x018c2161, CSimpleExecQueue )
//REGISTER_SAVELOAD_CLASS( 0x018c2160, CExecBlowTrappedDoor )
