#include "StdAfx.h"
#include "wUICommands.h"
#include "wUnitServer.h"
#include "RPGItem.h"
#include "RPGUnitMission.h"
#include "RPGGame.h"
#include "wObject.h"
#include "wMain.h"
#include "wMainPath.h"
#include "wAckBase.h"
#include "wUnitStates.h"
#include "wUnitMove.h"
#include "wUnitAttack.h"
#include "wUnitExec.h"
#include "..\DBFormat\DataAI.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\RandomGen.h"
#include "RPGUnit.h"
#include "aiPath.h"
#include "aiMap.h"                // NAI::IAIMap::GetObjectBound (mine-LOS probe pull-back)
#include "scScenarioTracker.h"
#include "scriptCallLUA.h"		// NScript::luaCallFunction (OnClickUsable)
#include "RPGGlobal.h"
#include "RPGDiplomacy.h"
#include "..\MiscDll\LogStream.h"
#include "rpgCheatConstants.h"
#include "rpgCritical.h"
#include "wUnitCommands.h"
#include "..\Misc\EventsBase.h"   // NGlobal::ThrowEvent
#include "eventUnit.h"            // NWorld::CEventOnSeeNewEnemy / CEventOnUnitDiedOrLoseConsciousness (AI events)
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataMap.h"
#include "eventPlayer.h"
#include "..\DBFormat\DataAck.h"
#include "RPGVision.h"
#include "aiCommander.h"
#include "aiMisc.h"               // NAI::IsAIPlayer (retail IsAIUnit @0x3c0340 probe)
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCommandExecute
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCommandExecute::StartAction( CWorld *pWorld, EActionType actionType )
{
	switch ( actionType )
	{
		case NORMAL:
			pAction = pWorld->GetActiveCounter();
			break;
		case SKIPPABLE:
			pAction = pWorld->GetSkippableCounter();
			break;
		default:
			ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPathViewer
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathViewer::CPathViewer( CUnitServer *_pUS ):
	pUS( _pUS )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathViewer::SetPath( NAI::CPath *pPath )
{
	/*if ( IsValid( pPath ) )
	{
		NAI::SPathPlace &pt = pPath->points.back();
		if ( pt.GetPose() == NAI::CM_LAY )
		{
			int dir = pt.GetDirection();
			pPath->points.push_back( NAI::SPathPlace( pt.GetX() + NAI::nMoveShift[dir][0], pt.GetY() + NAI::nMoveShift[dir][1],
				pt.GetLayer(), dir, pt.GetPose(), pt.IsMoving() ) );
		}
	}*/
	if ( IsValid( pMove ) )
	{
		IExecMove *pExec = dynamic_cast<IExecMove*>( pMove.GetPtr() );
		pExec->SetNewPath( pPath, NAI::PF_DEFAULT );
	}
	else
		pMove = CreateSimpleMoveExecutor( pUS, pPath, NAI::PF_DEFAULT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathViewer::GetResult() const
{
	if ( !IsValid( pMove ) )
	{
		ASSERT( 0 );
		return 0;
	}

	return pMove->GetActionAP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathViewer::GetPoints( vector<SPathPoint> *pRes )
{
	if ( !IsValid( pMove ) )
	{
		ASSERT( 0 );
		return;
	}

	IExecMove *pExec = dynamic_cast<IExecMove*>( pMove.GetPtr() );
	list<IExecMove::SPathPoint> pointsList;
	pExec->GetPathPoints( &pointsList );

	pRes->resize( pointsList.size() );

	int nCount = 0;
	for ( list<IExecMove::SPathPoint>::const_iterator iTemp = pointsList.begin(); iTemp != pointsList.end(); iTemp++ )
	{
		(*pRes)[nCount] = SPathPoint( iTemp->nAP, iTemp->nFloor, iTemp->vPoint );
		nCount++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitServer
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer::CUnitServer():
	registerOnNewPlayerTurnOrTime( this, &CUnitServer::OnNewPlayerTurnOrTime ),
	registerOnNewPlayerFastTurnOrTime( this, &CUnitServer::OnNewPlayerFastTurnOrTime )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer::CUnitServer( CWorld *pWorld, NRPG::IUnitMission *_pRPG, NDb::CModel *pModel,
	CPlayer *_pPlayer, const NAI::SUnitPosition &pos, bool bClueUnit )
	:CDumbUnitServer( pWorld, _pRPG, pModel, pos ), bIsPK( false ),
	registerOnNewPlayerTurnOrTime( this, &CUnitServer::OnNewPlayerTurnOrTime ),
	registerOnNewPlayerFastTurnOrTime( this, &CUnitServer::OnNewPlayerFastTurnOrTime ), bCanTalk( false ), nDialog( 0 )
{
	pPlayer = _pPlayer;
	bCallTimeLabel = false;
	SetState( new CUnitStateNormal( this ) );
	bIsRunningForcedAction = false;
	fLastHeight = pos.GetCP().z;
	plLast = pos.pos.p;
	if ( bClueUnit )
		nClueCount = 1;   // retail ctor @0x3c3cc0: the trailing bool marks a quest-clue carrier
	if ( _pRPG->GetRPGPers()->pPanzerklein )
	{
//		_pRPG->GetRPGPers()->pHead = 0;
		bIsPK = true;
		animator.SetPose( NAI::CRAWL );
		NAI::SUnitPosition animPos = pos;
		animPos.pos.p.SetPose( NAI::CRAWL );
		animator.PlaceUnit( animPos );
	}
	// retail @0x3c3cc0 (disasm 0x7c4072): seed this unit's per-unit diplomacy mask from the zone's
	// CGlobalDiplomacy row for its player. Every zone loads its OWN table and CreateUnit() rebuilds
	// the CUnitMission wrapper with a zeroed mask -- and DS_ENEMY == 0, i.e. "enemy toward all".
	// Without this ctor seed, any unit not otherwise touched (party deploy in AddPlayer, base-dialog
	// recruits via CCmdAddUnit) keeps the all-enemy default and paints ALLIES red / triggers combat.
	if ( IsValid( pPlayer ) && pPlayer->GetScenarioPlayerID() >= 0 )
		_pRPG->SetDiplomacy( pWorld->GetDiplomacy()->GetPlayerDiplomacy( pPlayer->GetScenarioPlayerID() ) );
	// bind the campaign game onto the mission (retail threads it through CreateUnit @0x2c4f50; this fork
	// binds it here) so CreateAttack's backstab-damage multipliers can read pGlobalGame->pDifficulty.
	_pRPG->SetGlobalGame( pWorld->GetGlobalGame() );
	// retail ctor @0x3c3cc0 tail: both pass timestamps are seeded from the current time (the time
	// getter is called TWICE, once per member).
	tCriticalPrev = GetWorld()->GetTime()->GetValue();
	tStatePrev = GetWorld()->GetTime()->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnSuffersDamage( float fAP )
{
	if ( fAP > 0.2f )
		GetWorld()->GetGlobalAck()->OnSuffersLightDamage( this );
	else
		GetWorld()->GetGlobalAck()->OnSuffersHardDamage( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnUnitMadeUnconscious( bool bFromScript )
{
	lostUnits.clear();
	hiddenAtSight.clear();   // retail @0x3c2010 clears BOTH sight-bookkeeping lists (lostUnits @+0x1c0, hiddenAtSight @+0x1f0)
	NGlobal::ThrowEvent( NWorld::CEventOnUnitDiedOrLoseConsciousness( this ) );   // retail @0x3c2010: other units' trackers drop this unit (enemy-died / lost-ally)
	pExec = 0;
	pCurrentCmd = 0;
	SetState( new CUnitStateUnconscious( this ) );
	if ( !bFromScript )
	{
		// BUG 5 (auto-focus): retail OnUnitMadeUnconscious @0x3c2010 posts CUICmdUnitCamera(self,
		// PR_UNIT_IS_DEAD, false, 1.0, null) -- the arbitrated auto-focus, not the old one-unit CUICmdUnit.
		GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( this, NWorld::PR_UNIT_IS_DEAD, false, 1.0f, 0 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnLifeLost()
{
	// retail @0x3c3cb0: a downed unit forgets every sound it had heard
	GetSounds()->clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnUnitDied( CUnitServer *pUS )
{
	// @0x3bf470 -- retail DROPPED the Jan03 GetWorld()->AddUICommand( new CUICmdUnit( this ) )
	// here (redundant per-observer UI refresh on every death); body is only the self-skip + forward.
	if ( pUS != this )
		pState->OnUnitDied( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::Die( bool bDeathBeauty, bool bRemove )
{
	lostUnits.clear();
	hiddenAtSight.clear();   // retail @0x3c2190 clears BOTH sight-bookkeeping lists (lostUnits @+0x1c0, hiddenAtSight @+0x1f0)
	NGlobal::ThrowEvent( NWorld::CEventOnUnitDiedOrLoseConsciousness( this ) );   // retail @0x3c2190: other units' trackers drop this dead unit (enemy-died / lost-ally)
	GetWorld()->OnUnitDied( this );
	if ( !bRemove )
		GetWorld()->GetGlobalAck()->OnUnitDied( this );
	// remove this unit's acks
	GetWorld()->GetGlobalAck()->RemoveUnitAcks( this );
	//
	if ( !bRemove )
	{
		// new state
		pState->OnDeath();
		pExec = 0;
		pCurrentCmd = 0;
		SetState( new CUnitStateDeath( this ) );
		// BUG 5 (auto-focus): retail CUnitServer::Die @0x3c2190 posts CUICmdUnitCamera(self,
		// beauty?PR_UNIT_DIED_BEAUTY:PR_UNIT_IS_DEAD, useSloMo=beauty, 3.0, null) -- beauty =
		// IsHero() || bDeathBeauty (retail @0x7c22f6: a hero death OR the gib path's forced flag).
		bool bBeauty = bDeathBeauty || ( IsValid( GetRPG() ) && GetRPG()->IsHero() );
		GetWorld()->AddUICommand( new NWorld::CUICmdUnitCamera( this,
			bBeauty ? NWorld::PR_UNIT_DIED_BEAUTY : NWorld::PR_UNIT_IS_DEAD, bBeauty, 3.0f, 0 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::AddImpulse( const CRay &rImpulse )
{
	// retail @0x3c0370 gate cascade (disasm-verified against the CUnit-base vftable @0x4cd084):
	//   CUnit vtbl+0x10 IsEmptyPK  -> TRUE skips (an empty shell doesn't ragdoll);
	//   server vtbl+0x34 GetWearingPK -> live shell skips (the PK armor absorbs the wave);
	//   vtbl+0x3c/+0x40 IsDead || IsUnconscious -> one must hold (only downed bodies get pushed);
	//   CUnit vtbl+0x24 GetCorpseCarrier -> being carried skips.
	if ( IsEmptyPK() )
		return;
	if ( IsValid( GetWearingPK() ) )
		return;
	if ( !IsDead() && !IsUnconscious() )
		return;
	if ( GetCorpseCarrier() )
		return;
	CVec3 vDir = rImpulse.ptDir;
	Normalize( &vDir );
	animator.Die( GetPosition(), vDir, false, this );   // clipless ragdoll launch (bPlayDeath=false); retail @0x7c03fa passes the server
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::ProcessCritical( NDb::ECritical eCA )
{
	ASSERT( eCA < NDb::N_CRIT_TYPES );
	if ( eCA == NDb::C_NONE ||  eCA >= NDb::N_CRIT_TYPES )
		return;
	if ( IsPerformingAction() )
		criticals.push_back( eCA );
	else
		ProcessCriticalImmediately( eCA );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::TouchedMines( const vector<CPtr<CMine> > &mines )
{
	for ( int k = 0; k < mines.size(); ++k )
	{
		//if ( pPlayer->CanSeeTrap( mines[k] ) )
		//	continue;
		mines[k]->GoBoom( this );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::RemoveFromWorld()
{
	GetWorld()->RemoveUnit( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::ProcessCriticalImmediately( NDb::ECritical eCA )
{
	ASSERT( eCA < NDb::N_CRIT_TYPES );
	if ( eCA == NDb::C_NONE ||  eCA >= NDb::N_CRIT_TYPES )
		return;
	//
	pState->ProcessCritical( eCA );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::RunCriticalExecutor( CCommandExecute *p )
{
	ASSERT( p );
	if ( !p )
		return;
	ASSERT( !IsPerformingAction() );
	pExec = p;
	animator.AlignTime();
	ASSERT( pExec->GetStartAP() == 0 ); // actually critical stuff should not spend APs
	pExec->Run();
	if ( pExec->GetState() != CCommandExecute::RUNNING )
		pExec = 0;
	else
		bIsRunningForcedAction = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::SetState( CUnitState *_pState ) 
{ 
	ASSERT( IsValid( _pState ) );
	if ( !IsValid( _pState ) )
		return;
	//
	if ( IsValid( pState ) )
		pState->OnStateFinished();
	pState = _pState;
	pState->OnStateStarted();
	pState->FilterCriticals();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CheckCmdExecState()
{
	if ( !IsValid(pExec) )
	{
		pExec = 0;
		return;
	}
	if ( pExec->GetState() != CCommandExecute::RUNNING )
	{
		//ASSERT( pExec->GetState() != CCommandExecute::FAILED );
		// retail release semantics (same rationale as the session-6 TBS_STOP fix @0x3c2a90 port: when an
		// executor is released, the command it executed is GONE with it -- the unit's route/logic
		// re-decides from scratch): a FAILED retire must ALSO drop the command. Keeping it made
		// CTask::GetCommand's HasCommand() branch pump CCmdContinue forever, re-Running the dead
		// command's executor every segment (the GFirst RF1 wedge) and starving every later task of the
		// same commander through the one-decision-per-segment latch.
		if ( ( pExec->GetState() == CCommandExecute::FINISHED || pExec->GetState() == CCommandExecute::FAILED )
			&& !bIsRunningForcedAction )
			pCurrentCmd = 0;
		pExec = 0;
		bIsRunningForcedAction = false;
		while ( !criticals.empty() && !IsPerformingAction() )
		{
			NDb::ECritical eCA = criticals.front();
			criticals.pop_front();
			pState->ProcessCritical( eCA );
		}
	}
	else
		CallTimeLabel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::Do( CCommand *_pCmd )
{
	ASSERT( criticals.empty() );
	CObj<CCommand> pCmdNew( _pCmd );
	// check is dead
	ASSERT( CanFight() );
	if ( !CanFight() )
		return;
	// TURN-STUCK FIX (dev-bugs s7): retail CUnitServer::Do @0x3c2390 has NO bIsRunningForcedAction
	// refusal -- its only gate is CanFight + not-cannon (oracle s2_unitserver.h Do mirror). The Jan03
	// early-return here was FATAL when the flag leaked: any direct pExec=0 drop of a RUNNING forced
	// critical executor (e.g. the old TBS_RECALC_COMMAND arm, fired by OnPassControl right after
	// turn-start bleed damage rolled a critical) left bIsRunningForcedAction latched TRUE with no
	// executor -- and with this gate the unit then silently ignored EVERY later command forever
	// (CheckCmdExecState, the only place that clears the flag, is unreachable without an executor).
	// Retail self-heals instead: commands are accepted, and the next executor retire clears the flag.
	ASSERT( !bIsRunningForcedAction );
	CDynamicCast<CCmdCancel> pCancel(_pCmd);
	if (pCancel)
	{
		//OutputDebugString(" CCmdCancel \n");
		CancelAction();
		//
		CancelSnipe(); // cannot be called from within CancelAction()
		CancelHeal();
		//
		pCurrentCmd = 0;
		return;
	}
	else {
		CDynamicCast<CCmdSetCommand> p(_pCmd);
		if (p)
		{
			// retail CUnitServer::Do (wUnitServer.c:3467): if the ordered command is a use-object
			// (open/close) command, fire OnClickUsable( unit, object ) before the normal handling.
			CDynamicCast<CCmdOpenClose> pOpenClose( p->GetCmd() );
			if ( pOpenClose )
				NScript::luaCallFunction( "OnClickUsable", "pp", CastToObjectBase( this ), pOpenClose->pObject.GetBarePtr() );
			//
			CDynamicCast<CCmdEmpty> pEmpty(p->GetCmd());
			if (pEmpty)
			{
				//OutputDebugString(" CCmdEmpty \n");
				return;
			}
			if (IsValid(pCurrentCmd) && IsValid(pExec) && pExec->IsExecuting())
			{
				// some command is being executed - have to cancel previous and set new target
				CDynamicCast<CCmdContinue> pContinue(p->GetCmd());
				if (pContinue)
				{
					//OutputDebugString(" CCmdContinue, pExec is valid \n");
					return;
				}
				CDynamicCast<CCmdPath> pCmdPath(p->GetCmd());
				if (pCmdPath)
				{
					EUnitCommandResult eResult;
					if (pState->IsCriticalsFailCommand(pCmdPath, &eResult))
						return;
					CDynamicCast<IExecMove> pMove(pExec);
					if (pMove)
					{
						//OutputDebugString(" CCmdPath, pExec is valid and is a MoveExec\n");
						vector<NAI::SPathPlace> dst;
						dst.push_back(pCmdPath->ptDst.p);
						NAI::SPathPlace src;
						pMove->GetSearchFromPosition(&src);
						CPtr<NAI::CPath> pPath = FindPath(GetWorld()->GetPathNetwork(), this, src, dst,
							0, true, pCmdPath->eParams, IsStrafing());
						if (IsValid(pPath))
							pMove->SetNewPath(pPath, pCmdPath->eParams);
						/*else
						{
							CancelAction();
							pCurrentCmd = 0;
						}*/
						return;
					}
					/// else OutputDebugString(" CCmdPath, pExec is valid and is not a MoveExec\n");
				}
				pExec->Cancel();
				pAutoRunCmd = p->GetCmd();
			}
			else {
				CDynamicCast<CCmdContinue> pContinue(p->GetCmd());
				if (pContinue)
				{
					//OutputDebugString(" CCmdContinue, else \n");
		//			ASSERT( IsValid( pCurrentCmd ) );
					RefreshExecutor();
					if (IsValid(pExec))
					{
						if (!pExec->IsExecuting())
						{
							if (CanSpendAP(pExec->GetStartAP()))
							{
								animator.AlignTime();
								pExec->Run();
								CheckCmdExecState();
							}
						}
						else
						{
							//OutputDebugString(" ASSERT(0) \n");
							ASSERT(0);
						}
					}
					//			else
					//				ASSERT( 0 );
				}
				else
				{
					/*if ( CDynamicCast<CCmdPath> pCmdPath(p->GetCmd()) )
						OutputDebugString(" CCmdPath, pExec is not valid \n");
					else
						OutputDebugString(" Some other command \n");*/
					pCurrentCmd = p->GetCmd();
					EUnitCommandResult eResult;
					pExec = pState->CreateExecutor(pCurrentCmd, &eResult);
					if (!IsValid(pExec))
						pCurrentCmd = 0; // impossible
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::DynamicallyLockWay( CPtr<NAI::CPath> pPath )
{
	NAI::IPathNetwork *pNet = GetWorld()->GetPathNetwork();
	pNet->ClearDynamicLocks( this );
	if ( CanFight() && IsValid( pPath ) )
	{
		pNet->ChangeDynamicLocks( this, pPath->points );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CUnitServer::CanDo( CCmd *p, int *pnStartAP, int *pnFullAP )
{
	CPtr<CCmd> pCmdHolder = p;

	if ( pnStartAP )
		*pnStartAP = -1;
	if ( pnFullAP )
		*pnFullAP = -1;
	//
	CDynamicCast<CCmdEmpty> pEmpty(p);
	if (pEmpty)
		return UCR_OK;
	/*
	// Reload
	CDynamicCast<CCmdShootTile> pShootTile( p );
	CDynamicCast<CCmdShootObject> pShootObject( p );
	if ( IsValid( pShootTile ) || IsValid( pShootObject ) )
	{
		if ( GetActionType( this ) == AT_SHOOT )
		{
			CDynamicCast<NRPG::IWeaponItem> pWeaponItem( GetUnitRPG()->GetWeaponItem() );
			if ( IsValid( pWeaponItem ) )
			{
				if ( !pWeaponItem->IsWorking() )
					return UCR_WEAPON_JAMMED;
				else if ( !pWeaponItem->HasAmmo() )
					return UCR_NEED_RELOAD;
			}
		}
	}
	*/
	//
	// @0x3c1570 -- retail PK-ban (absent in Jan03): a unit WEARING a live Panzerklein
	// (pWearingPK @+0x1cc, [ecx+0x80] off the CUnit base @+0x14c) that is CROUCHing may not
	// be given a look-around (CCmdLook) command -> UCR_PK_BAN. Ordinary crouched units CAN look.
	// Placed here because a CCmdLook is never a CCmdContinue, so the decode's "not continue"
	// gate is implicit. (The binary computes a CCmdPath cast too, but it does NOT gate this
	// return; in this tree CCmdLook : CCmd, so the CCmdLook cast alone is the faithful gate.)
	if ( IsValid( pWearingPK ) && GetPosition().GetPose() == NAI::CROUCH )
	{
		CDynamicCast<CCmdLook> pLook( p );
		if ( pLook )
			return UCR_PK_BAN;
	}
	//
	CDynamicCast<CCmdContinue> pContinue(p);
	if (pContinue)
	{
		if ( IsPerformingAction() )
			return UCR_UNAVAILABLE;

		RefreshExecutor();
		if ( IsValid( pExec ) )
		{
			if ( IsExecStartCombat( pExec ) )
				return UCR_UNAVAILABLE;
			//
			int nActionAP = pExec->GetActionAP();
			if ( pnStartAP )
				*pnStartAP = pExec->GetStartAP();
			if ( pnFullAP )
				*pnFullAP = nActionAP;

			if ( !CanSpendAP( nActionAP ) )
				return UCR_NOT_ENOUGH_AP;

			return UCR_OK;
		}
		return UCR_UNAVAILABLE;
	}

	CDynamicCast<CCmdPath> pMove(p);
	if ( pMove && ( !pnStartAP ) )
	{
		// Finding a path is much faster when the unit is standing rather than lying down, so the check for whether a path exists is
		// done with WishPose = STAND.
		vector<NAI::SPathPlace> dst;
		dst.push_back( pMove->ptDst.p );
		NAI::EPose curPose = GetWishPose();
		SetWishPose( NAI::WALK );
		const NAI::SUnitPosition &pos = GetPosition();
		CPtr<NAI::CPath> pPath = FindPath( GetWorld()->GetPathNetwork(), this, pos.pos.p, dst, 
			0, true, pMove->eParams, IsStrafing(), true );
		SetWishPose( curPose );
		if ( IsValid( pPath ) )
			return UCR_OK;
		return UCR_PATH_NOT_FOUND;
	}

	EUnitCommandResult eResult = UCR_OK;
	CObj<CCommandExecute> pHold = pState->CreateExecutor( p, &eResult );
	if ( IsValid( pHold ) )
	{
		int nActionAP = pHold->GetActionAP();

		if ( pnStartAP )
			*pnStartAP = pHold->GetStartAP();
		if ( pnFullAP )
			*pnFullAP = nActionAP;

		if ( !CanSpendAP( nActionAP ) )
			return UCR_NOT_ENOUGH_AP;

		return eResult;
	}

	if ( eResult == UCR_OK ) // CRAP exact reason should be returned by CreateExecutor
		eResult = UCR_GENERAL_FAILURE;

	return eResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::HasEnoughAP()
{
	ASSERT( !IsPerformingAction() );
	RefreshExecutor();
	if ( IsValid( pExec ) )
		return CanSpendAP( pExec->GetStartAP() );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit::EState CUnitServer::GetState()
{
	if ( typeid(*pState) == typeid(CUnitStateSniping) )
		return ST_SNIPE;
	if ( typeid(*pState) == typeid(CUnitStateUsingCannon) )
		return ST_MACHINE_GUN;
	if ( typeid(*pState) == typeid(CUnitStateCorpseCarrier) )
		return ST_CARRY_CORPSE;
	if ( typeid(*pState) == typeid(CUnitStateHealer) )
		return ST_HEALER;
	switch ( GetActionType( this ) )
	{
		case AT_NONE: return ST_NORMAL_DEFAULT;
		case AT_THROW:
		case AT_SHOOT:
		case AT_BAZOOKA:
			break;//return ST_NORMAL;
		case AT_MELEE: return ST_NORMAL_MELEE;
		case AT_GRENADE: return ST_NORMAL_GRENADE;
		case AT_FIRSTAID: return ST_NORMAL_MEDKIT;
		case AT_MINE: return ST_NORMAL_MINE;
		case AT_TOOL: return ST_NORMAL_TOOL;
		case AT_KEY: return ST_NORMAL_KEY;
		default:
			ASSERT( 0 );
			break;
	}
	switch ( GetUnitRPG()->GetWeaponType() )
	{
		case NDb::WT_PISTOL: return ST_NORMAL_PISTOL;
		case NDb::WT_RIFLE: return ST_NORMAL_RIFLE;
		case NDb::WT_SUB_MACHINE_GUN: return ST_NORMAL_SUB_MACHINE_GUN;
		case NDb::WT_KNIFE: return ST_NORMAL_KNIFE;
		case NDb::WT_PLAZMAGUN:
		case NDb::WT_MACHINE_GUN: return ST_NORMAL_HAND_MACHINE_GUN;
		case NDb::WT_RLAUNCHER: return ST_NORMAL_RLAUNCHER;
		case NDb::WT_MINE_DETECTOR:
		case NDb::WT_DEFAULT:
			break;
		default:
			ASSERT( 0 );
			break;
	}
	return ST_NORMAL_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::RefreshExecutor()
{
	if ( IsValid( pExec ) )
		return;
	if ( !IsValid( pCurrentCmd ) )
		return;

	EUnitCommandResult eResult;
	pExec = pState->CreateExecutor( pCurrentCmd, &eResult );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CancelAction()
{
	if ( bIsRunningForcedAction )
		return;
	if ( IsValid( pExec ) && pExec->IsExecuting() )
		pExec->Cancel();
	else
	{
		// NOTE: this DROP branch keeps pCurrentCmd (retail @0x3c0a20 does the same).
		//		pCurrentCmd = 0;
		pExec = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::FallFromHigh( float fHeightDiff )
{
	if ( fHeightDiff <= 2.8f /* F_MAX_CLIMB_HEIGHT */ )
		return;
	int nDamage = GetUnitRPG()->GetFallDamage( fHeightDiff );
	NRPG::CAttackPortion att( 1, 0, 0.0f, nDamage, -1, 0 );   // retail FallFromHigh @0x3c0570: fPushCoeff=0
	// retail @0x3c0570 passes this->pWorld and a straight-down direction; pWorld is private on the
	// CDumbUnitServer base, so reach it through the accessor.
	ProcessAttack( GetWorld(), NAI::HL_BODY, &att, CVec3( 0, 0, -1 ), GetUnitRPG()->GetRPGArmor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::Fall()
{
	csSystem << CC_RED << "FORCED FALL\n";
	float fHeightDiff = fLastHeight - GetPosition().GetCP().z;
	animator.Fall( GetPosition(), fLastHeight );
	Update();
	GetWorld()->UpdateVisible();
	FallFromHigh( fHeightDiff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::ForcedMove()
{
	csSystem << CC_RED << "FORCED MOVE\n";
	NAI::IPathNetwork *pNet = GetWorld()->GetPathNetwork();
	SSphere s( GetPosition().GetCP(), 3 );
	vector<NAI::SPathPlace> res;
	pNet->GetNearPlaces( s, &res );
	float fMinDist = 1000, fMinHorDist = 1000;
	NAI::SUnitPosition dst( GetPosition() );
	int pose = GetPosition().pos.p.GetPose();
	if ( pose == NAI::CM_INACTIVE )
		pose = NAI::CM_CROUCH;
	float fMaxFall = GetMaxFallDist( fLastHeight );   // retail @0x3c0b80 passes fLastHeight as the ray-origin z
	for ( int i = 0; i < res.size(); ++i )
	{
		res[i].SetPose( pose );
		res[i].SetDirection( GetPosition().GetDir() );
		if ( !pNet->IsPassable( res[i] ) )
			continue;
		NAI::SPosition pos;
		pos.p = res[i];
		pos.SetNetwork( pNet );
		CVec3 cp = pos.GetCP();
		CVec3 desired = GetPosition().GetCP();
		if ( cp.z > fLastHeight + 0.01f )
			continue;
		CVec3 vDist = desired - cp; 
		float fZDist = fLastHeight - cp.z;
		float fHorDist = vDist.x * vDist.x + vDist.y * vDist.y;
		float fDist;
		if ( fZDist < fMaxFall ) 
		{
			if ( fHorDist < 0.01f )
				fDist = fHorDist + fZDist * fZDist * 0.2f * 0.2f;
			else
				fDist = fHorDist + fZDist * fZDist;
		}
		else
			fDist = fHorDist + fZDist * fZDist * 4;
		if ( fDist < fMinDist )
		{
			fMinDist = fDist;
			fMinHorDist = vDist.x * vDist.x + vDist.y * vDist.y;
			dst.pos.p = res[i];
		}
	}
	if ( fMinDist < 100 )
	{
		if ( fMinHorDist < 0.01f )
		{
			SetPosition( dst );
			Fall();
		}
		else
		{
			animator.ForcedMove( dst );
			Update();
			GetWorld()->UpdateVisible();
			SetPosition( dst );
		}
		return;
	}
	// retail @0x3c0b80: nowhere to land at all -> the unit dies FALLING. Stash the pre-fall height
	// into animator.fDeathFall BEFORE the kill dispatch (disasm: the +0x114 float store precedes the
	// pUnit vtbl+0x94 Kill call); CUnitAnimator::Die @0x33bb90 reads it and takes the SetMove
	// falling-death branch instead of SetStand+PutOnTerrain.
	animator.fDeathFall = fLastHeight;
	GetUnitRPG()->Kill();
	KillUnit( CVec3(0,0,1) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnTBSEvent( ETBSEvent event )
{
	switch ( event )
	{
		case TBS_START_NEW_TURN:
			lostUnits.clear();
			if ( CanFight() )
			{
				GetUnitRPG()->StartNewTurn( GetPosition().GetCP() );
				animator.StartNewTurn( GetPosition() );
				pState->OnStartNewTurn();
				wasInterruptedList.clear();
			}
			animator.CalculateAnimFlags();
			break;			
		case TBS_FINISH_OWN_TURN:
			if ( !IsDead() )
				GetUnitRPG()->DoRegenerations();
			pState->OnFinishOwnTurn();
			pState->OnFinishTimeOrTurn( false );
			break;			
		case TBS_START_REAL_TIME:
			if ( CanFight() )
				GetUnitRPG()->StartRealTime();
			animator.CalculateAnimFlags();
			pState->OnStartRealTime();
			break;			
		case TBS_ACTION_FINISH:
			ASSERT( !IsPerformingAction() );
			// retail CUnitServer::OnTBSEvent @0x3c2a90 TBS_ACTION_FINISH: store height/place +
			// OnActionFinish ONLY. The locker grid-consistency block that used to live here (an a5dll
			// relocation from the then-undelivered TBS_GRID_INFO_UPDATED, later gated on !IsAiming to
			// stop it from clearing a held aim on every shot) has moved to its RETAIL home below --
			// CWorld::Segment now delivers TBS_GRID_INFO_UPDATED only when the path network actually
			// changed, so the reseat no longer fires on every action-finish edge and needs no aim gate.
			fLastHeight = GetPosition().GetCP().z;
			plLast = GetPosition().pos.p;
			pState->OnActionFinish();
			break;
		case TBS_GRID_INFO_UPDATED:
			// retail @0x3c2a90 case 10: a locker unit re-seats on the changed grid -- release the lock,
			// force-move off a now-impassable tile; otherwise fall/snap when the ground height under the
			// SAME place drifted (> 0.01), store the new height/place, and re-lock.
			if ( IsLocker() )
			{
				NAI::IPathNetwork *pNet = GetWorld()->GetPathNetwork();
				pNet->Unlock( this );
				if ( !pNet->IsPassable( GetPosition().pos.p ) )
					ForcedMove();
				else
				{
					float fCurrentHeight = GetPosition().GetCP().z;
					NAI::SPathPlace plCur = GetPosition().pos.p;
					if ( plLast == plCur && fabs( fLastHeight - fCurrentHeight ) > 0.01f )
						Fall();
					fLastHeight = fCurrentHeight;
					plLast = plCur;
					pNet->Lock( this, GetPosition().pos.p );
				}
			}
			break;
		case TBS_CANCEL_ACTION:
			CancelAction();
			break;
		case TBS_RECALC_COMMAND:
			// is called when something happens that might affect current command execution plan.
			// TURN-STUCK FIX (dev-bugs s7): retail @0x3c2a90 does NOT drop the executor unconditionally:
			//   if ( !cannon && IsCancelableExec( pExec ) ) {           // NWorld::IsCancelableExec @0x392fe0
			//       if ( CanFight() && visible ) animator.PlaceUnit( GetPosition() );  // re-seat the model
			//       pExec = 0;
			//   }
			// The dev bare `pExec = 0` (a) dropped cannon actions a recalc must NOT drop, and (b) never
			// re-seated a unit whose move executor it released, leaving the model mid-stride off its
			// grid cell. NOTE: dropping a RUNNING forced-critical executor here (RECALC fires from
			// OnPassControl right after turn-start bleed damage rolls a critical) leaves
			// bIsRunningForcedAction latched in retail too -- retail tolerates that because its Do()
			// accepts commands regardless and the next executor retire clears the flag; the matching
			// dev-only hard refusal in Do() is removed this session (see the comment there).
			// (The cannon-action guard is carried by IsCancelableExec's CExecCannon check.)
			// DEV DEVIATION (hold-aim safety): the PlaceUnit re-seat is gated on the exec being an
			// IExecMove (same pattern as the TBS_STOP arm below). Retail's arm re-seats whenever
			// visible, but retail coalesces RECALC through its deferred STBSEvent queue -- this dev
			// tree fires RecalcCurrentPlayerCommands on EVERY action-end edge, which would re-seat a
			// FINISHED shoot executor holding its aiming pose after every shot. Re-seating is only
			// MEANT for a dropped mid-stride mover; non-movers are dropped without a re-seat exactly
			// as the pre-session dev code did (hold-aim visuals confirmed working under that drop).
			if ( IsCancelableExec( pExec ) )
			{
				if ( CDynamicCast<IExecMove>( pExec ) && CanFight() && IsAddedToVisitor() )
					animator.PlaceUnit( GetPosition() );
				pExec = 0;
			}
			break;
		case TBS_STOP_MOVE_AND_CANCEL_ACTION:
			// retail CUnitServer::OnTBSEvent @0x3c2a90 snaps a unit interrupted MID-MOVE to its current grid
			// cell (animator.PlaceUnit) and then RELEASES its move executor (pExec = 0), so it ends on a
			// valid, pathable place -- not mid-stride/off-grid with a half-cancelled CExecMove that would
			// make every later move order FindPath-fail and silently drop.
			//
			// HOWEVER: in this tree the sighting interrupt can fire RE-ENTRANTLY from inside the unit's own
			// move processing (CExecMove::DoCommand -> ... -> AddInterrupt -> CancelAllAction -> here), so
			// releasing the executor outright would FREE it while its own DoCommand is still on the call
			// stack -> use-after-free (double-free of pCurCmd in CObjectBase::ReleaseObj, then heap
			// corruption). Retail can free here because it never delivers the interrupt re-entrantly. So we
			// keep the SNAP (PlaceUnit is already invoked mid-exec safely by IsWaitingForPath) but tear the
			// move down the SAFE way via CancelAction -> CExecMove::Cancel(), which only MARKS the exec
			// FAILED (no free); CheckCmdExecState then drops pExec on a later Segment tick. Same on-grid end
			// state, no re-entrant free.
			if ( CDynamicCast<IExecMove>( pExec ) && CanFight() && IsAddedToVisitor() )
				animator.PlaceUnit( GetPosition() );
			CancelAction();
			// TURN-STALL FIX (dev-bugs s6 retest#5, bug B): retail @0x3c2a90 RELEASES the move executor
			// here, so the interrupted move COMMAND is gone with it. The dev tree keeps pExec alive (the
			// re-entrant-free hazard documented above) but must still abandon the command: CancelAction's
			// own `pCurrentCmd = 0` is commented out and CheckCmdExecState clears pCurrentCmd only on
			// FINISHED -- so a move cancelled by this global broadcast (WantTurnBased ->
			// GlobalSituationHasChanged -> CancelAllAction, i.e. the real-time -> turn-based transition)
			// left a STALE pCurrentCmd. CAICombatLogic::HasCommandToExecute() (@0x432da0) reads it via
			// HasCommand(), which made the unit's think-job IsIdleJob()==true forever (@0x4331c0), the
			// job manager never ran it, the tactical commander's WaitForJob dependency never finished,
			// bEndOfTurn never latched -- the AI player's first turn never ended. Clearing the command
			// here mirrors retail's executor release; the unit's route/logic re-decides from scratch.
			pCurrentCmd = 0;
			break;
		default:
			ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsPerformingAction() const
{
	return IsValid( pExec ) && pExec->IsExecuting();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::GetVisible( vector<CPtr<CUnit> > *pTarget ) const
{
	pTarget->resize( visible.size() );
	int nTemp = 0;
	for ( list<CPtr<CUnitServer> >::const_iterator i = visible.begin(); i != visible.end(); ++i )
	{
		(*pTarget)[nTemp] = i->GetPtr();
		nTemp++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::GetInfo( NRPG::SUnitInfo *pInfo ) const
{
	// retail @0x3bfd50: base CUnitMission fill, then overlay the PK-HP bar.
	pInfo->bPKInfo = false;
	pInfo->bUnitInfo = false;
	GetUnitRPG()->GetInfo( GetPose(), pInfo );
	if ( IsEmptyPK() )
	{
		// the unit's body literally IS a Panzerklein -> PK-HP == the unit's own HP.
		pInfo->nPKHP = pInfo->nHP;
		pInfo->nMaxPKHP = pInfo->nMaxHP;
		pInfo->bPKInfo = true;
		return;
	}
	pInfo->bUnitInfo = true;
	if ( IsValid( pWearingPK ) )
	{
		// a soldier piloting a PK -> the PK-HP bar shows the worn PK unit's HP.
		NRPG::SUnitInfo tmp = {};
		pWearingPK->GetUnitRPG()->GetInfo( GetPose(), &tmp );
		pInfo->nPKHP = tmp.nHP;
		pInfo->nMaxPKHP = tmp.nMaxHP;
		pInfo->bPKInfo = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IPlayer* CUnitServer::GetPlayer() const 
{ 
	return pPlayer; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IUnitMissionInfo* CUnitServer::GetRPG() const 
{ 
	return GetUnitRPG(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The path-conflict remover of this unit's current executor (pExec -> IExecMove), or null. Used by
// CExecMove::CheckLockerState to step the who-locks-whom chain onto the locking unit's own remover.
CPathConflictsRemover* CUnitServer::GetPathConflictsRemover()
{
	return IsValid( pExec ) ? pExec->GetPathConflictsRemover() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::Segment()
{
	STime tCur = GetWorld()->GetTime()->GetValue();
	int nDoNotFreeze = 0;
	while ( IsValid( pExec ) && pExec->IsExecuting() )
	{
		// Service the move wait-state (path-conflict recovery) each tick. The Jan03 inline locker/reroute
		// pump now lives in CExecMove::CheckCanDoMove/CheckLockerState/TryToSetNewPath, driven from
		// CPathConflictsRemover::Segment (@0x3cc670). Still waiting after servicing -> hold this tick.
		pExec->Segment();
		// RETAIL RETIRE PREDICATE (disasm @0x3c2f00, re-derived dev-bugs s7 after the hold-aim
		// regression): the retail pump retires an executor (AnimationFinished vtbl+0x20 +
		// CheckCmdExecState @0x3c08b0) ONLY at an animation edge -- `if (tCur < animator.TimeEnd
		// [this+0x5c]) break;` guards both calls, and the loop's only other exits are pExec
		// null/zombie, !IsExecuting (vtbl+0x28) and the PCR wait probe (vtbl+0x14). There is NO
		// state-based reap anywhere in the retail loop: a FINISHED executor installed while the
		// animator's end-time stays ahead IS retail's hold-aim -- the shoot exec goes FINISHED at
		// the shot and keeps the aiming pose until the anim edge / a new command tears it down.
		// (The first s7 attempt reaped ANY non-RUNNING exec every pass; that retired healthy
		// FINISHED attack execs instantly = pose reset after every shot. REGRESSION -- narrowed.)
		//
		// Two dev-necessary deviations remain, both because dev cancels DEFER the release (the TBS
		// stop paths here cannot free the exec re-entrantly the way retail @0x3c2a90 does), so a
		// DEAD exec can linger installed where retail structurally cannot:
		//   (s7) FAILED = cancelled/failed -- never a hold-pose state (hold-aim is FINISHED,
		//        regression-trace-proven) -> reap promptly. This is the wedge net for a cancelled
		//        exec (e.g. a queue Cancel latching FAILED while its front's clip still plays)
		//        sitting under an ever-ahead TimeEnd, swallowing MOVE re-aims until an attack.
		if ( pExec->GetState() == CCommandExecute::FAILED )
		{
			CheckCmdExecState();
			break;
		}
		if ( pExec->IsWaitingForPath() )
		{
			//   (s6, retest#5 bug B) a wait-PARKED mover whose move already terminated
			//        (TryToSetNewPath found no route -> FullCancel latched state=FINISHED but,
			//        retail-faithfully, left bWaiting set) can never reach the animation pump
			//        below, so it was NEVER reaped: its CObj<CActionCounter> pinned
			//        CTBSWorld::IsAction() true forever and froze the first enemy turn. Reap the
			//        dead park; only a still-RUNNING park (live 10-tick locker retry) holds the tick.
			if ( pExec->GetState() != CCommandExecute::RUNNING )
				CheckCmdExecState();
			break;
		}
		if ( bCallTimeLabel && tCur >= animator.GetTimeLabel1() )
			bCallTimeLabel = pExec->TimeLabelReached();
		if ( tCur >= animator.GetTimeEnd() )
		{
			if ( bCallTimeLabel )
			{
				// prevent situation when time mark is set wrong (behind animation finish)
				bCallTimeLabel = pExec->TimeLabelReached();
				// new animation could be set
				if ( tCur < animator.GetTimeEnd() )
					break;
			}
			pExec->AnimationFinished();
			CheckCmdExecState();
		}
		else
			break;
		++nDoNotFreeze;
		if ( nDoNotFreeze > 100 )
			__debugbreak(); // somehow we freezed here?
		if ( nDoNotFreeze > 150 )
			break;
	}
	if ( IsValid(pAutoRunCmd) && ( !IsPerformingAction() )  )
	{
		pCurrentCmd = pAutoRunCmd;
		pAutoRunCmd = 0;
		Do( new CCmdSetCommand( this, new CCmdContinue ) );
	}
	CDumbUnitServer::Segment();
	if ( GetWorld()->IsRealTime() )
	{
		if ( tCur - tCriticalPrev >= 3000 )   // retail keeps two throttles (tCriticalPrev/tStatePrev); dev's merged pass rides the criticals one
		{
			if ( !IsDead() )
				GetUnitRPG()->DoRegenerations();
			pState->OnFinishTimeOrTurn( true );
			tCriticalPrev = tCur;
			tStatePrev = tCur;
		}
	}
	pState->Segment();
	FetchRPGAcks();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::HearUnit( CUnitServer *pSource )
{
	vector<CObj<CTimedObject> > stuff;
	GetWorld()->CreateSoundStuff( pSource, &stuff, pSource->GetPosition().GetCP() );
	HearSound( stuff, pSource, pSource->GetPosition().pos.p );
	SetAudible( pSource, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::AddMedalPointsForNoticedMines @0x3c3190 (free fn, wUnitServer.obj): for every mine
// in the unit's freshly rebuilt trappedObjects that the PLAYER did not already know (player
// GetTrappedObjectsList, vtbl+0x40), credit MPC_NOTICE_TRAP scaled by the mine's DC into the RPG
// unit's medals gainer (retail CMedalsGainer::AddMedalPoints @0x2ac660; dev's is the documented
// behaviour-neutral stub until NDb::CSide::medals lands -- the notice detection + event stay live).
// Returns whether anything NEW was noticed; the caller throws CEventOnSpotMineOrTrap on true.
static bool AddMedalPointsForNoticedMines( NRPG::CGlobalGame *pGame, NRPG::CUnit *pRPGUnit,
	IPlayer *pPlayer, const list<CPtr<CObjectBase> > &trappedObjects )
{
	bool bRes = false;
	list<CPtr<CObjectBase> > known;
	pPlayer->GetTrappedObjectsList( &known );
	for ( list<CPtr<CObjectBase> >::const_iterator i = trappedObjects.begin(); i != trappedObjects.end(); ++i )
	{
		CObjectBase *p = i->GetPtr();
		if ( IsInSet( known, p ) )
			continue;                                     // the player already knows this one
		CDynamicCast<IMine> pMine( p );
		if ( IsValid( pMine ) && pMine->IsMineSet() )
		{
			pRPGUnit->AddMedalPoints( pGame, NRPG::MPC_NOTICE_TRAP, (float)pMine->GetMineDC() );
			bRes = true;
		}
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::UpdateVisible( SInterruptInfo *pRes )
{
	bool bCanSee = !GetUnitRPG()->HasCritical( NDb::C_BLIND ) && CanFight();
	if ( bCanSee && !IsCheatEnabled( NRPG::CHEAT_SEEALL ) )
	{
		list< CPtr<CUnit> > playerVisible;
		GetPlayer()->GetVisible( &playerVisible );
		list<CPtr<CUnitServer> > origVis = visible;
		list<CPtr<CUnitServer> > res, oldVisible = visible;
		// retail UpdateVisible @0x3c4450 (disasm): the candidate-gather radius is
		// CGame::GetMaxUnitSightDistance @0x298520 = 2x GetUnitSightDistance (vtbl+0x38) -- matching
		// MakeVisionQuery's 2x cap on the height-stretched effective range. The old per-pose
		// GetSightDistance(GetPose()) getter is a Jan03-ism (retail's sight getters take no pose) and
		// under-gathered: a unit above/below can be visible out to 2x the flat range.
		GetWorld()->GetUnitsNear( GetPosition().GetEyePosition(), &res,
			GetWorld()->GetGame()->GetMaxUnitSightDistance( GetUnitRPG()->GetRPGUnit() ) );
		
		// retail @0x7c459c: drop hiders that left the GetUnitsNear candidate set
		for ( list<CPtr<CUnitServer> >::iterator h = hiddenAtSight.begin(); h != hiddenAtSight.end(); )
			if ( find( res.begin(), res.end(), h->GetPtr() ) == res.end() )
				h = hiddenAtSight.erase( h );
			else
				++h;
		visible.clear();
		list<CPtr<CUnitServer> > carriedBodies;   // retail @0x7c460f: carried non-fighting bodies, adopted after the loop
		for ( list<CPtr<CUnitServer> >::iterator i = res.begin(); i != res.end(); ++i )
		{
			CUnitServer *pEnemy = *i;
			// retail @0x7c4655: the candidate loop dispatches on CanFight() FIRST -- only a FIGHTING
			// candidate takes the same-player skip (@0x7c4823) and the DS_ALLY leg (@0x7c483d). A
			// non-fighting body (corpse, unconscious, empty-PK shell) bypasses both: carried -> the
			// carrier-adoption pass below (@0x7c466c); already-seen -> dropped here (@0x7c46e5, the
			// player-side addToVisible corpse persistence keeps it); new -> straight to the
			// CheckVisibility probe. The old unconditional same-player skip swallowed a console-summoned
			// empty PK (its owner IS the summoning player) forever.
			if ( !pEnemy->CanFight() )
			{
				if ( pEnemy->GetCorpseCarrier() )
				{
					carriedBodies.push_back( pEnemy );
					continue;
				}
				if ( find( oldVisible.begin(), oldVisible.end(), pEnemy ) != oldVisible.end() )
					continue;
			}
			else
			{
				if ( pEnemy->pPlayer == pPlayer )
					continue;
				//
				if ( GetDiplomacyState( pEnemy ) == NDb::DS_ALLY )
				{
					visible.push_back( pEnemy );
					continue;
				}
			}
			//
			// retail perception probe: CheckVisibility(observer, candidate, bUseFOV=TRUE) -- disasm-proven
			// push 1 @0x7c46fe (the observer's real FOV cone applies; facing matters).
			if ( !GetWorld()->GetGame()->CheckVisibility( this, pEnemy, true ) )
			{
				hiddenAtSight.erase( remove( hiddenAtSight.begin(), hiddenAtSight.end(), pEnemy ), hiddenAtSight.end() );  // retail @0x7c473f: lost unit leaves hiddenAtSight
				if ( find( oldVisible.begin(), oldVisible.end(), pEnemy ) != oldVisible.end() )
				{
					// enemy lost from sight
					if ( pEnemy->CanFight() )
						HearUnit( pEnemy );
					if ( find( lostUnits.begin(), lostUnits.end(), pEnemy ) == lostUnits.end() )
						lostUnits.push_back( pEnemy );
					// item 7 parity: retail CUnitServer::UpdateVisible @0x3c4450 raises the lost-from-sight event
					// (symmetric with the CEventOnSeeNewEnemy throw below); CAIEventTrackerImpl::OnLostEnemy
					// downgrades enemy -> possible-enemy. Retail @0x7c47e7 throws it UNCONDITIONALLY (no diplomacy gate).
					NGlobal::ThrowEvent( NWorld::CEventOnLostEnemyFromSight( this, pEnemy ) );
				}
				continue;
			} 
			//
			if ( GetDiplomacyState( pEnemy ) != NDb::DS_ENEMY )
			{
				visible.push_back( pEnemy );
				continue;
			}
			//
//			if ( !IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE ) && !IsCheatEnabled( NRPG::CHEAT_NOAI ) )
//			{
			bool bWasHidden = ( find( hiddenAtSight.begin(), hiddenAtSight.end(), pEnemy ) != hiddenAtSight.end() );
			if ( pEnemy->GetUnitRPG()->IsHiding() )
			{
				if ( IsAudible( pEnemy ) || !bWasHidden )   // @0x7c48cb: no re-roll for a tracked silent hider
					CheckSpot( pEnemy );                    // @0x7c48da CUnitServer::CheckSpot @0x3bfe30 (may unhide)
				if ( pEnemy->GetUnitRPG()->IsHiding() )     // @0x7c48df re-check after the roll
				{
					if ( !bWasHidden )                      // @0x7c48f0
						hiddenAtSight.push_back( pEnemy );  // @0x7c4913
					continue;                               // still hidden -> not added to visible
				}
			}
			hiddenAtSight.erase( remove( hiddenAtSight.begin(), hiddenAtSight.end(), pEnemy ), hiddenAtSight.end() );  // @0x7c492e spotted -> drop
			// (no commander notify here: retail UpdateVisible @0x3c4450 has no OnSeeUnit vcall -- the
			// realtime->TBS arm is ONLY the transition-gated AddEvent below)
			//
			//if ( !IsAudible( pEnemy ) || pEnemy->IsJustUnhided() )
			//{
			if ( find_if( playerVisible.begin(), playerVisible.end(), SPtrTest(pEnemy) ) == playerVisible.end() )
			{
				lostUnits.erase( remove( lostUnits.begin(), lostUnits.end(), pEnemy ), lostUnits.end() );
				if ( pEnemy->CanFight() )
				{
					GetWorld()->GetGlobalAck()->OnEnemyBecomesVisible( this, pEnemy, GetWorld()->IsRealTime() );
					pRes->AddEvent( this, pEnemy );
				}
			}
			//
			// retail CUnitServer::UpdateVisible: a unit that NEWLY sees an enemy raises CEventOnSeeNewEnemy, whose
			// handler relays the enemy to allies within 5m as a possibleEnemy (squad spotting). Gate on the unit not
			// having seen it last update so it fires once per acquisition.
			if ( pEnemy->CanFight() && find( oldVisible.begin(), oldVisible.end(), pEnemy ) == oldVisible.end() )
				NGlobal::ThrowEvent( NWorld::CEventOnSeeNewEnemy( this, pEnemy, GetWorld()->IsRealTime() ) );
			visible.push_back( pEnemy );
		}
		// retail @0x7c4abe..0x7c4b5b: adopt carried bodies -- a carried corpse is never probed; it is
		// visible iff its carrier is ME or a unit I now see.
		for ( list<CPtr<CUnitServer> >::iterator i = carriedBodies.begin(); i != carriedBodies.end(); ++i )
		{
			CUnit *pCarrier = (*i)->GetCorpseCarrier();
			bool bSeeCarrier = ( pCarrier == static_cast<CUnit*>( this ) );
			for ( list<CPtr<CUnitServer> >::iterator v = visible.begin(); !bSeeCarrier && v != visible.end(); ++v )
				bSeeCarrier = ( static_cast<CUnit*>( v->GetPtr() ) == pCarrier );
			if ( bSeeCarrier )
				visible.push_back( *i );
		}
		// look at mines -- retail @0x7c4b74..0x7c4f2b:
		list<CPtr<IMine> > traps;
		trappedObjects.clear();
		// gather radius = the unit's OWN max mine-spot range, GetMineSpotRange(0) (rpg vtbl+0x168,
		// @0x7c4bca pushes literal 0), NOT a hardcoded constant -- per-unit, skill/perk-driven.
		GetWorld()->GetMinesNear( GetPosition().GetEyePosition(), &traps, GetUnitRPG()->GetMineSpotRange( 0 ) );
		for ( list<CPtr<IMine> >::const_iterator i = traps.begin(); i != traps.end(); ++i )
		{
			IMine *pMine = *i;
			CObjectBase *pObj = i->GetPtr();
			// @0x7c4c11: mines already queued in addToVisibleTraps skip the see-check entirely
			// (the addToVisibleTraps adoption loop below picks them up regardless).
			if ( IsInSet( addToVisibleTraps, pObj ) )
				continue;
			float fDist = fabs( pMine->GetMinePos() - GetPosition().GetEyePosition() );
			// per-DC skill check (CanSeeMine @0x2bec30 == fDist <= GetMineSpotRange(GetMineDC()))
			if ( !GetUnitRPG()->CanSeeMine( fDist, pMine->GetMineDC() ) )
				continue;
			// @0x7c4cc1: the LOS gate -- a CanSeeCenter ray to the mine point LIFTED by 0.1625
			// (imm @0x8cd1a0), pulled back toward the eye by HALF the object's AI-hull
			// bounding-sphere radius (GetObjectBound @0x465800; @0x7c4d83 fRadius*0.5 (imm
			// @0x8b19ec), Max(0, d - 0.5r)/d along the eye ray, Max<float> @0x42ab90) so the ray
			// doesn't end inside a trapped door/chest's own hull. No pull-back when the object has
			// no hulls or d == 0. Without this gate mines were spotted THROUGH WALLS.
			CVec3 pt = pMine->GetMinePos() + CVec3( 0, 0, 0.1625f );
			CVec3 ptEye = GetPosition().GetEyePosition();
			CVec3 delta = pt - ptEye;
			float fD = fabs( delta );
			SBound bound;
			if ( GetWorld()->GetAIMap()->GetObjectBound( &bound, pObj ) && fD > 0 )
				pt = ptEye + delta * ( Max( 0.0f, fD - 0.5f * bound.s.fRadius ) / fD );
			if ( !GetWorld()->GetGame()->CanSee( this, pt ) )
				continue;
			trappedObjects.push_back( pObj );
		}
		// @0x7c4eac: medal credit for NEWLY noticed set mines + the squad "spotted a trap" event
		// (@0x7c4f04 throws CEventOnSpotMineOrTrap(this); retail consumer CAckDiscoveringMineNearby).
		if ( AddMedalPointsForNoticedMines( GetWorld()->GetGlobalGame(), GetUnitRPG()->GetRPGUnit(),
				GetPlayer(), trappedObjects ) )
			NGlobal::ThrowEvent( NWorld::CEventOnSpotMineOrTrap( this ) );
		// @0x7c4f30: adopt/prune the script-forced visible traps.
		for ( list<CPtr<CObjectBase> >::iterator i = addToVisibleTraps.begin(); i != addToVisibleTraps.end(); )
		{
			CObjectBase *p = *i;
			CDynamicCast<IMine> pMine( p );
			if ( IsValid(pMine) && pMine->IsMineSet() )
			{
				if ( !IsInSet( trappedObjects, p ) )
					trappedObjects.push_back( p );
				++i;
			}
			else
				i = addToVisibleTraps.erase( i );
		}
		// look at items -- retail @0x7c5027..0x7c54f7. KEY semantics (all disasm-verified):
		//  * visibleObjects is NOT cleared: EraseInvalidRefs (@0x7c502d, call 0x564c60) drops dead
		//    entries and everything else PERSISTS -- a discovered item stays discovered; only
		//    tempVisibleObjects (+0x174) restarts empty (@0x7c5032).
		//  * gather radius = GetMaxUnitSightDistance (2x per-unit, game vtbl+0x38), centre = eye.
		//  * per item: skip if already in visibleObjects (@0x7c513b); else ray EVERY point of the
		//    item's GetVisiblePos vector (mass-sphere centres), each LIFTED by 0.1625 (imm @0x8cd1a0)
		//    -- a point ON the floor grazes the ground voxel and always fails -- via CanSeeCenter
		//    against the viewer info computed once per update (dev CGame::CanSee == CalcViewerInfo
		//    @0x298570 + CanSeeCenter @0x298750 fused); first visible point wins (@0x7c521d
		//    `or bl,al`). CHEAT_SEEALL (IsCheatEnabled(2) @0x7c5185) bypasses the rays entirely.
		//  * IsTemporaryVisible routes to tempVisibleObjects (@0x7c5259) else visibleObjects (@0x7c528e).
		EraseInvalidRefs( &visibleObjects );
		tempVisibleObjects.clear();
		SSphere area( GetPosition().GetEyePosition(),
			GetWorld()->GetGame()->GetMaxUnitSightDistance( GetUnitRPG()->GetRPGUnit() ) );
		list<IVisible*> items;
		GetWorld()->GetVisibleItems( area, &items );
		for ( list<IVisible*>::iterator i = items.begin(); i != items.end(); ++i )
		{
			IVisible *p = *i;
			CObjectBase *pObj = p;                        // the vbase upcast retail does inline
			if ( IsInSet( visibleObjects, pObj ) )
				continue;
			bool bSee = IsCheatEnabled( NRPG::CHEAT_SEEALL );
			vector<CVec3> pts;
			p->GetVisiblePos( &pts );
			for ( int k = 0; !bSee && k < (int)pts.size(); ++k )
				bSee = GetWorld()->GetGame()->CanSee( this, pts[k] + CVec3( 0, 0, 0.1625f ) );
			if ( !bSee )
				continue;
			if ( p->IsTemporaryVisible() )
				tempVisibleObjects.push_back( pObj );
			else
				visibleObjects.push_back( pObj );
		}
		// dynamic (in-flight) items -- retail @0x7c5341..0x7c54f7 (GetVisibleDynamicItems, ctrl
		// vtbl+0x20 @0x34a420, SAME sphere): never rayed (CDItem::GetVisiblePos adds no points);
		// instead an in-flight item is visible iff CHEAT_SEEALL (@0x7c53a9), OR its visibility
		// parent dyncasts to a CUnitServer of MY player (@0x7c53cb RTDynamicCast -> CUnitServer,
		// @0x7c53e1 GetPlayer compare), OR the parent is among the units I currently SEE
		// (@0x7c5404 scans this->visible). Success feeds the PERSISTENT visibleObjects (@0x7c5456).
		list<IVisible*> dynItems;
		GetWorld()->GetVisibleDynamicItems( area, &dynItems );
		for ( list<IVisible*>::iterator i = dynItems.begin(); i != dynItems.end(); ++i )
		{
			IVisible *p = *i;
			CObjectBase *pObj = p;
			if ( IsInSet( visibleObjects, pObj ) )
				continue;
			bool bSee = IsCheatEnabled( NRPG::CHEAT_SEEALL );
			CObjectBase *pParent = p->GetVisibilityParent();
			if ( !bSee )
			{
				CDynamicCast<CUnitServer> pParentUS( pParent );
				if ( IsValid( pParentUS ) && pParentUS->GetPlayer() == GetPlayer() )
					bSee = true;
			}
			if ( !bSee && pParent )
			{
				for ( list<CPtr<CUnitServer> >::const_iterator v = visible.begin(); v != visible.end(); ++v )
					if ( static_cast<CObjectBase*>( v->GetPtr() ) == pParent )
					{
						bSee = true;
						break;
					}
			}
			if ( bSee )
				visibleObjects.push_back( pObj );
		}
	}
	else
	{
		visible.clear();
		trappedObjects.clear();
		visibleObjects.clear();
	}
	if ( IsCheatEnabled( NRPG::CHEAT_SEEALL ) )
	{
		GetWorld()->GetAllUnits( &visible );
		trappedObjects.clear();
		list<CPtr<IMine> > traps;
		GetWorld()->GetMinesNear( CVec3(0,0,0), &traps, 1e10f );
		for ( list<CPtr<IMine> >::const_iterator i = traps.begin(); i != traps.end(); ++i )
			trappedObjects.push_back( i->GetPtr() );
	}
	//
	FilterSounds( visible );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitServer::CheckSpot @0x3bfe30 (this=spotter, pTarget=hider): one distance roll to reveal a
// spotted hiding enemy. Returns true iff pTarget was unhidden.
bool CUnitServer::CheckSpot( CUnitServer *pTarget )
{
	if ( IsValid( pTarget ) && CanFight() && pTarget->CanFight() &&
		GetDiplomacyState( pTarget ) == NDb::DS_ENEMY &&
		GetWorld()->GetGame()->CheckVisibility( this, pTarget, true ) )
	{
		float fDistance = fabs( GetPosition().GetCP() - pTarget->GetPosition().GetCP() ) / FP_GRID_STEP;
		int nProbability = GetUnitRPG()->GetUnhideProbability( pTarget->GetUnitRPG(), fDistance );
		int nCheck = random.Get( 0, 100 );   // @0x7bff43: shared global RNG (retail &random@0x9c9978)
		if ( nCheck >= nProbability && !IsAudible( pTarget ) )
			return false;
		pTarget->Hide( false, true );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CPath* CUnitServer::GetCurrentPath()
{
	RefreshExecutor();
	if ( IsValid( pExec ) )
		return pExec->GetCurrentPath();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IPathViewer* CUnitServer::CreatePathViewer()
{
	return new CPathViewer( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::GetCurrentCommandName( string *pName ) const
{
	if ( !IsValid( pExec ) )
		return false;
	
	const char *pszName = typeid( *pExec ).name();
	const char *pszRealName = strstr( pszName, "::" );
	if ( !pszRealName )
		(*pName) = pszName;
	else
		(*pName) = pszRealName + 2;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3c32e0: +0x164 audibleUnits is cleared first when the unit HasCritical(C_DEAF); +0x168 is
// CTBSUnitVision::visible (the TB visible set) -- a unit I already SEE, I trivially hear (no roll).
bool CUnitServer::CanHearSound( const CVec3 &ptFrom, const NDb::SAISound &sound, CUnitServer *pWho )
{
	if ( !CanFight() )
		return false;
	// retail @0x3c32e0: a DEAF unit forgets its per-turn heard-set first (HasCritical(C_DEAF) -> clear +0x164).
	if ( GetUnitRPG()->HasCritical( NDb::C_DEAF ) )
		ClearAudible();
	if ( IsAudible( pWho ) )                 // +0x164 audibleUnits: already heard this turn
		return true;
	// +0x168 visible: a unit I can SEE, I trivially hear (no distance roll).
	{
		const list< CPtr<CUnitServer> > &vis = GetTBSVisible();
		if ( find( vis.begin(), vis.end(), pWho ) != vis.end() )
			return true;
	}
	return GetUnitRPG()->CanHearSound( ptFrom, GetPosition().GetCP(), sound, pWho->GetUnitRPG() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CUnitServer::GetAttackOrigin() const
{
	return GetAttackOrigin( GetPosition() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CUnitServer::GetAttackOrigin( const NAI::SUnitPosition &from ) const
{
	if ( CCannon *pCannon = animator.GetCannon() )
		return pCannon->GetPosition() + pCannon->GetCannonAttackOrigin();   // retail @0x3bf8d0: cannon pos + DB muzzle offset (CCannon+0xb4)
	NDb::CAnimWeaponType *pType = GetUnitRPG()->GetDBAnimWeapon();
	if ( !pType )
		return GetPosition().GetEyePosition();
	CVec3 rel;
	switch ( from.GetPose() )
	{
		case NAI::CRAWL:
			rel = pType->crawl;
			break;
		case NAI::CROUCH:
			rel = pType->crouch;
			break;
		case NAI::WALK:
		case NAI::RUN:
			rel = pType->stand;
			break;
	}
	CQuat q( from.GetDirection(), CVec3(0,0,1) );
	return from.GetCP() + q.Rotate(rel);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitServer::GetMinClearDistance() const
{
	if ( CCannon *pCannon = animator.GetCannon() )
		return pCannon->GetMinClearDistance();   // retail @0x3bf6a0: reads cannon+0xb0 (DB MinClearDistance)
	NDb::CAnimWeaponType *pType = GetUnitRPG()->GetDBAnimWeapon();
	if ( !pType )
		return 0;
	return pType->fMinDistance;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CObjectBase* CUnitServer::GetAttackIgnore() const
{
	if ( CCannon *pCannon = animator.GetCannon() )
		return pCannon;
	return this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::UpdateCriticalsState()
{ 
	pState->FilterCriticals(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::CanSnipe() const
{
	// has a scoped (sniper) weapon
	if ( !IsValid( GetUnitRPG()->GetWeaponItem() ) || !GetUnitRPG()->GetWeaponItem()->GetDBWeapon()->bScope )
		return false;
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsSniping() const
{
	CDynamicCast<CUnitStateSniping> pTmpState(pState);
	if (pTmpState)
		return true;
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateSniping* CUnitServer::GetSnipingState()
{
	CDynamicCast<CUnitStateSniping> pSnipingState(pState);
	if (pSnipingState)
		return pSnipingState;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CollectSnipeAP( int nExtraAP )
{
	CDynamicCast<CUnitStateSniping> pSnipingState(pState);
	if (pSnipingState)
		pSnipingState->CollectSnipeAP( nExtraAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CancelSnipe()
{
	CDynamicCast<CUnitStateSniping> pSnipingState(pState);
	if (pSnipingState)
		pSnipingState->CancelSnipe();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CComplexHead* CUnitServer::GetDBHead()
{
	return GetUnitRPG()->GetRPGPersHead();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetHeadSeed -- the wrapped RPG unit per-unit head seed (retail CGameView::CreateLSHead @0x188c90 seeds
// the head hair/material rnd from CHeadInfo::seed; threading it keeps each unit hair stable + per-unit
// distinct -- no per-frame cycling in the portrait / inventory views).
SRandomSeed CUnitServer::GetHeadSeed()
{
	return GetUnitRPG()->GetRPGUnit()->GetHeadSeed();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetHeadInfo -- the wrapped RPG unit's live head info. A committed advanced-FaceGen hero carries a baked
// static head (CHeadInfo::pMesh = CFaceGenMeshHolder); CHeadsController::GetAnimator renders it.
NLSHead::CHeadInfo* CUnitServer::GetHeadInfo()
{
	return GetUnitRPG()->GetRPGUnit()->GetHeadInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsCapPresent()
{
	NDb::CRPGUniform *pUniform = GetUnitRPG()->GetRPGPers()->pUniform; 
	if ( pUniform )
	{
		if ( !pUniform->pCapModel )
			return false;
	}
	return CanFight();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::FetchRPGAcks()
{
	int nAckID;
	NRPG::IUnitMissionInfo *pUnitMission;
	while ( GetUnitRPG()->GetAck( &nAckID, &pUnitMission ) )
	{
		CPtr<CUnitServer> pAttacker = GetWorld()->GetUnitServer( pUnitMission );
		CPtr<CUnitServer> pTarget = GetWorld()->GetUnitServer( GetUnitRPG() );
		//
		switch( nAckID )
		{
			case N_ACK_CRITICAL:
				GetWorld()->GetGlobalAck()->OnDoCriticalDamage( pAttacker, pTarget );
				break;
			case N_ACK_DEATH:
				GetWorld()->GetGlobalAck()->OnUnitWasKilled( pAttacker, pTarget );
				break;
			case N_ACK_SKILL:
				GetWorld()->GetGlobalAck()->OnSkillIncreased( pTarget );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CancelHeal()
{
	CDynamicCast<CUnitStateHealer> pHealer(pState);
	if (pHealer)
		SetState( new CUnitStateNormal( this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3c33a0: forwards to the CDumbUnitServer base with pWorld/vDir unchanged.
int CUnitServer::ProcessAttack( NWorld::IWorld *pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	if ( bIsPKWhichIsWeared )
		return 0;
	bool isDead = GetUnitRPG()->IsDead();
	int nRet = CDumbUnitServer::ProcessAttack( pWorld, nUserID, pAttack, vDir, pArmor );
	if ( !IsValid( this ) )
		return nRet;
	CUnitServer *pPKUnit = 0;
	if ( IsWearingPK() )
		pPKUnit = pWearingPK;
	if ( IsEmptyPK() )
		pPKUnit = this;
	NDb::CPanzerklein *pPK = 0;
	if ( pPKUnit )
		pPK = pPKUnit->GetUnitRPG()->GetRPGPers()->pPanzerklein;
	if ( pPK )
	{	
		NRPG::CDynamicSkill &pkVP = pPKUnit->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_VP );
		if ( pPK->pSelfExplosion && pkVP < 0 )
		{
			GetWorld()->AddGrenadeExplosion( GetPosition().GetCP(), pPK->pSelfExplosion );
			if ( IsEmptyPK() )
				WearAsPK( true ); // deletes from AIMap and doesn't render
			else
			{
				FlipPanzerklein( 0 );
				if ( GetUnitRPG()->IsDead() )
				{
					animator.Die( GetPosition(), VNULL3, true, this );   // retail @0x3c33a0: PK self-explosion DOES play the pilot death clip (bPlayDeath=true)
					GetWorld()->GetPathNetwork()->Unlock( this );
				}
			}
		}
		if ( pkVP < 0 )
			animator.IdleBan( NAnimation::E_NO_IDLE_WHEN_STUNNED, true );
	}

	if ( !isDead && GetUnitRPG()->IsDead() )
	{
		if ( IsValid( GetCorpseCarrier() ) )
		{
			CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = GetCorpseCarrier()->GetPlayer()->GetGlobalPlayer();
			if ( IsValid( pGlobalPlayer ) )
					pGlobalPlayer->deployData.unitsDeployData[ GetUnitRPG()->GetRPGUnit() ].bCorpseAlive = false;
		}
		GetWorld()->GetGlobalGame()->pScenarioTracker->OnScenarioClueDestroyed( this->GetUnitRPG()->GetRPGPersID(), true );
		//
		if ( IsValid( pAttack->pAttacker ) )
		{
			list< CPtr<CUnitServer> > units;
			GetWorld()->GetUnitServer(pAttack->pAttacker)->GetTBSPlayer()->GetUnitsThatCanFight( &units );
			float fXP = GetUnitRPG()->GetXP( units.size() );
			for ( list< CPtr<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
				(*i)->GetUnitRPG()->GetRPGUnit()->AddXP(fXP);
		}
	}
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsMoving() const 
{ 
	CDynamicCast<IExecMove> pMove(pExec);
	if ( GetWorld()->GetCurrentPlayer() ) // turn-based mode
	{
		return false;
	}
	if ( !pMove )
		return false;
	else
		return ( !pExec->IsWaitingForPath() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitServer::GetCarefulShotExtraAP()
{
	CDynamicCast<NRPG::IWeaponItem> pWeaponItem( GetUnitRPG()->GetWeaponItem() );
		if ( IsValid( pWeaponItem ) && pWeaponItem->GetShootMode() != NDb::SM_Careful )
			return 0;
	//
	int nRes = 0;
	//
	if ( animator.IsAiming() )
		nRes = GetActionAP( NRPG::AC_SHOOT );
	else
		nRes = GetActionAP( NRPG::AC_PREPARE_AND_SHOOT );
	//
	NRPG::SUnitInfo Info;
	GetInfo( &Info );
	nRes = max( 0, Info.nAP - nRes );
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::HasLostFromSightAliveUnits() 
{ 
	for ( list< CPtr<CUnitServer> >::iterator i = lostUnits.begin(); i != lostUnits.end(); ++i )
	{
		if ( (*i)->CanFight() )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsUnconscious() const
{
	return CDynamicCast<CUnitStateUnconscious>( pState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsHiding() const
{
	return GetRPG()->IsHiding();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::GetBarrelDir( CRay *pRay )
{
	ASSERT( pRay != 0 );
	if ( pRay == 0 )
		return false;
	//
	CPtr<NRPG::IWeaponItem> pWeaponItem = GetUnitRPG()->GetWeaponItem();
	if ( !IsValid( pWeaponItem ) )
		return false;
	//
	CDBPtr<NDb::CRPGWeapon> pDBWeapon = pWeaponItem->GetDBWeapon();
	if ( !IsValid( pDBWeapon ) )
		return false;
	//
	NAnimation::SBonePose barrel;
	if ( !animator.GetBarrelPos( pDBWeapon->GetModel()->pGeometry, &barrel, false ) )
		return false;
	//
	pRay->ptOrigin = barrel.pos;
	barrel.rot.GetXAxis( &pRay->ptDir );
	Normalize( &pRay->ptDir );
//	if ( GetUnitRPG()->GetWeaponType() == NDb::WT_RLAUNCHER )
//		pRay->ptDir = -pRay->ptDir; // this is not a bug, the artists did it this way // it actually was a bug after all :)
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDumbUnitServer *CUnitServer::GetCorpse()
{
	CDynamicCast<CUnitStateCorpseCarrier> pCarrier(pState);
	if (pCarrier)
		return pCarrier->GetCorpse();
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsCheatEnabled( int nCheat )
{
	return GetUnitRPG()->GetRPGUnit()->IsCheatEnabled( nCheat );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsFlyingBoss() const
{
	// retail @0x3c1250
	if ( !IsValid( pWearingPK ) )
		return false;
	NDb::CRPGPers *pPKPers = pWearingPK->GetRPG()->GetRPGPers();
	if ( !IsValid( pPKPers ) )
		return false;
	if ( IsValid( pPKPers->pName ) && pPKPers->pName->szStr == L"Boss" )
		return true;
	return pPKPers->szUserName == "Boss";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::FlipPanzerklein( CUnitServer *pPK, bool bUnloadWeapons, bool bTakeInventory )
{
	NDb::CModel *pPKModel;
	if ( !pPK ) // return to non-PK mode
		pPKModel = GetUnitRPG()->GetRPGUnit()->pModel;
	else
		pPKModel = pPK->GetUnitRPG()->GetRPGUnit()->pModel;
	CUnitServer *pOldPK = pWearingPK;
	pWearingPK = pPK;
	NAI::SUnitPosition pos = GetPosition();
	pos.pos.p.SetPose( NAI::CM_STAND );
	pos.bRun = false;
	fLastHeight = pos.GetCP().z;	// retail @0x3c1a90 refreshes it here (no fall from the pre-flip height)
	SetPosition( pos );
	pModel = pPKModel;
	NDb::CRPGPers *pPKPers = 0;
	if ( pPK )
		pPKPers = pPK->GetUnitRPG()->GetRPGPers();
	GetUnitRPG()->GetRPGUnit()->pPanzerklein = pPKPers;
	if ( pPK && bUnloadWeapons )
	{
		NRPG::IInventory* pInv = GetUnitRPG()->GetInventory();
		for ( int i = 0; i < NDb::N_SLOTS; ++i )
		{
			NRPG::IInventoryItem *pItem = pInv->Get( (NDb::ESlot)i );
			if ( IsValid( pItem ) )
			{
				pInv->TakeOff( (NDb::ESlot)i );
				CTPoint<int> pos;
				if ( pInv->FindPlace( pItem, &pos ) )
					pInv->Place( pos, pItem );
				else 
				{
					CVec3 shift;
					shift.x = random.GetFloat( - FP_GRID_STEP * 0.5f, FP_GRID_STEP * 0.5f );
					shift.y = random.GetFloat( - FP_GRID_STEP * 0.5f, FP_GRID_STEP * 0.5f );
					shift.z = 0.1f;
					GetWorld()->AddFrozenItem( GetWorld()->GetAIMap(), GetPosition().GetCP() + shift, QNULL, pItem );
				}
			}
		}
	}
	// retail @0x3c1a90: the skeleton PK flags, decided before the criticals/SetPanzerklein block
	bool bBoss = IsFlyingBoss();
	bool bTerrorPK = pPKPers && !bBoss && pPKPers->pPanzerklein->bHasNoHead;
	if ( pWearingPK )
		GetUnitRPG()->ApplyCritical( NRPG::SCritical( NDb::CL_ANY, NDb::C_PANZERKLEIN_AXIS ) );
	else
	{
		for ( int nCrit = NDb::C_PANZERKLEIN_AXIS; nCrit <= NDb::C_PANZERKLEIN_TERRORS_HWG; ++nCrit )
			GetUnitRPG()->RemoveCritical( (NDb::ECritical)nCrit );
	}

	if ( pWearingPK )
	{
		NDb::CPanzerklein *pDbPK = pWearingPK->GetRPG()->GetRPGPers()->pPanzerklein;
		NRPG::CDynamicSkill *pVP = &pWearingPK->GetRPG()->GetRPGUnit()->Skills( NDb::ST_VP );
		NRPG::IInventory *pPKInv = 0;
		if ( bTakeInventory )
			pPKInv = pWearingPK->GetUnitRPG()->GetInventory();
		GetUnitRPG()->SetPanzerklein( pDbPK, pVP, pPKInv );
		SetUndrawItem( false );
		if ( *pVP < 0 )
			animator.IdleBan( NAnimation::E_NO_IDLE_WHEN_STUNNED, true );
	}
	else
		GetUnitRPG()->SetPanzerklein( 0, 0, pOldPK->GetUnitRPG()->GetInventory() );
	animator.ChangeSkeleton( pPKModel->pSkeleton, pPK != 0, bBoss, bTerrorPK );
	animator.SetWeaponAnimation( GetUnitRPG()->GetWeaponType() );
	animator.SetActiveItem( pWearingPK != 0 );	// retail: active while worn -- keys the PK_WEAPON_* pose flags
	animator.PlaceUnit( GetPosition() );
	Update();
	GetWorld()->UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CPanzerklein *CUnitServer::GetWearingDBPK()
{ 
	if ( !IsValid( pWearingPK ) )
		return 0;
	return pWearingPK->GetRPG()->GetRPGPers()->pPanzerklein;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsUnitVisible( const CUnit *pUnit ) const
{
	const list< CPtr<CUnitServer> > &visible = GetTBSVisible();
	CDynamicCast<CUnitServer> pTargetUS( pUnit );
	return find( visible.begin(), visible.end(), pTargetUS.GetPtr() ) != visible.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsUnitAudible( const CUnit *pUnit ) const
{
	CDynamicCast<CUnitServer> pTargetUS( pUnit );
	return IsAudible( pTargetUS.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::CheckStability()
{
	// retail @0x3bf360: EMPTY -- corpse re-drops are exclusively the stability tracker's corpses
	// branch (NAI::CStabilityTracker::OnChange @0xa59a0 -> CUnitAnimator::BeDropped, ported in
	// aiStability.cpp). The Jan03 body here re-dropped instable corpses from the world sweep.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsDead() const 
{ 
	return CDynamicCast<CUnitStateDeath>( pState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnNewPlayerFastTurnOrTime( const CEventOnNewPlayerFastTurnOrTime &event )
{
	// retail @0x3c0300: re-arm hiding at the start of THIS unit's own player fast-turn (or a player-less
	// forced-real-time tick). Pairs with Hide()'s bCanHide=false-on-unhide so a unit that unhid can hide again
	// only after its next turn -- WITHOUT this the gate would permanently lock out re-hiding.
	if ( !IsValid( event.pPlayer ) || event.pPlayer == GetPlayer() )
		EnableHide();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::OnNewPlayerTurnOrTime( const CEventOnNewPlayerTurnOrTime &event )
{
	if ( CanFight() && ( !IsValid( event.pPlayer ) || event.pPlayer == GetPlayer() ) )
	{
		// Bleeding
		NRPG::CCritical *pCritical;
		if ( GetUnitRPG()->HasCritical( NDb::C_BLEEDING, &pCritical ) )
		{
			int nDamage = pCritical->GetCritical().fValue;
			NRPG::CAttackPortion att( 1, 0, 0.0f, nDamage, -1, 0 );   // retail ProcessCriticalsAndRegenerations @0x3c2770: fPushCoeff=0
			// retail ProcessCriticalsAndRegenerations @0x3c2770: this->pWorld, straight-down direction.
			ProcessAttack( GetWorld(), NAI::HL_BODY, &att, CVec3( 0, 0, -1 ), GetUnitRPG()->GetRPGArmor() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CUnitServer::GetDiplomacyState( CUnitServer *pTarget ) const
{
	if ( !pTarget->GetPlayer() )
	{
		ASSERT( pTarget->IsEmptyPK() );
		return NDb::DS_NEUTRAL;
	}
	return GetUnitRPG()->GetDiplomacy().GetDiplomacyState( pTarget->GetPlayer()->GetScenarioPlayerID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::SetPlayer( CPlayer *_pPlayer )
{
	pPlayer = _pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitServer::SetDialog( const string &szDialogCode )
{
	CDBPtr<NDb::CDBDialog> pDBDialog = NDb::GetDBDialogByCode( szDialogCode );
	if ( IsValid( pDBDialog ) )
		nDialog = pDBDialog->GetRecordID();
	else
		nDialog = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitServer::IsAIUnit() const
{
	// retail @0x3c0340: IsAIUnit == NAI::IsAIPlayer( unit's player ) -- "the commander is NOT a
	// CSequenceCommander". The old raw CDynamicCast<CAICommander> probe becomes WRONG once the human
	// player carries its retail CSequenceCommander (a CAICommander subclass): it would classify the
	// human's units as AI units.
	return NAI::IsAIPlayer( GetPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BeginDeactivatingItem( CUnitServer *pUS, NDb :: EItemSubType subType )
{
	// @0x3bf800 -- retail deactivates the active item for ANY subtype (incl. SUBTYPE_NONE,
	// which short-circuits to the "no place" sentinel -1 -> stowed to backpack, WITHOUT querying
	// the inventory). Jan03's `subType != SUBTYPE_NONE` outer gate (returned false for NONE) was
	// dropped; follow the decode (authoritative).
	if ( !pUS->animator.IsActiveItem() )
		return false;
	if ( subType == NDb::SUBTYPE_HEAVY || subType == NDb::SUBTYPE_MINE_DETECTOR )
		pUS->animator.DeactivateItem( pUS->GetPosition(), true, true, NDb::BELT_M1 );
	else
	{
		int nPlace = -1;
		if ( subType != NDb::SUBTYPE_NONE )
			nPlace = pUS->GetUnitRPG()->GetInventory()->GetPlaceBySubType( subType );
		pUS->animator.DeactivateItem( pUS->GetPosition(), false, nPlace == -1, (NDb::EItemPlace)nPlace );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x0251101c, CUnitServer )
BASIC_REGISTER_CLASS( CCommandExecute )
REGISTER_SAVELOAD_CLASS( 0x02682140, CPathViewer )
