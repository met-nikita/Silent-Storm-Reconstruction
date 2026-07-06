#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "aiPosition.h"
#include "aiCommander.h"
#include "rpgUnit.h"
#include "rpgItemInfo.h"
#include "RPGItemSet.h"				// NRPG::CWeaponItem / CGrenadeItem (UnitDrawWeapon / UnitSwitchToGrenade)
#include "rpgUnitMission.h"
#include "rpgCritical.h"
#include "rpgGame.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"		// NDb::CTRndModel::CreateModel (UnitHoldItem)
#include "..\DBFormat\DataMap.h"			// NDb::EDiplomacyState / DS_ENEMY (KillEmAll diplomacy gate)
#include "..\Misc\RandomGen.h"			// SRand (UnitHoldItem)
#include "..\MiscDll\LogStream.h"
#include "wAckBase.h"
#include "wUnitGroup.h"
#include "aiRoute.h"
#include "rpgAttackMech.h"
#include "wUnitCommands.h"
#include "wUnitAttack.h"					// NWorld::UnitThrowGrenade (UnitGrenadeToUnit / UnitGrenadeToWaypoint)
#include "rpgPerk.h"
#include "rpgGlobal.h"
#include "aiUnit.h"
#include "rpgCheatConstants.h"
#include "weActiveItem.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "wUnitStates.h"
#include "aiTaskCommander.h"
#include "aiInventory.h"
#include "aiMisc.h"			// NAI::GetAIUnit
#include "aiLogic.h"		// NAI::IAILogic
//
#include "scriptUnit.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetUnit, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CUnitServer> pUS = pScript->pWorld->GetUnitServer( szName );
	luaPushCPtr( pState, pUS );
	if ( !IsValid( pUS ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " unit [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateUnit, "nnssn" )
//PERS_ID, PLAYER_ID, UNIT_NAME, WAYPOINT_NAME, LEVEL
	int nPersID = luaParams[ 0 ].n;
	int nPlayerID = luaParams[ 1 ].n;
	string szName = luaParams[ 2 ].s;
	string szWaypoint = luaParams[ 3 ].s;
	int nLevel = luaParams[ 4 ].n;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( IsValid( pWaypoint ) )
	{
		CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( nPlayerID );
		if ( IsValid( pPlayer ) )
		{
			CDBPtr<NDb::CRPGPers> pRPGPers = NDb::GetPers( nPersID );
			if ( IsValid( pRPGPers ) )
			{
				CPtr<NRPG::IUnitMission> pRPGUnit = NRPG::CreateUnit( pRPGPers );
				if ( IsValid( pRPGUnit ) )
				{
					pRPGUnit->GetRPGUnit()->SetXPLevel( nLevel );
					CPtr<NWorld::CUnitServer> pUS;
					pUS = pScript->pWorld->AddUnitInGame( pWaypoint->pos.p, pRPGUnit, pPlayer, szName );
					luaPushCPtr( pState, pUS );
					return 1;
				}
			}
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSayAck, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		pScript->pWorld->GetGlobalAck()->SayAck( pUS, luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fb7e0 ("un[-1]"): record a script-forced to-hit on the unit (CUnitServer+0x1e8; -1 clears).
// The value IS read: retail NRPG::RPGUnitGetToHit @0x2b4ae0 and RPGUnitGetTileToHit @0x2b4df0 both open
// with `if (nScriptToHit >= 0) return nScriptToHit;` -- an ABSOLUTE replacement of the whole to-hit
// computation for that attacker (shoot/melee/throw/rocket; grenades excluded -- no read in the grenade
// calcer). The earlier "write-only, FAITHFUL ELISION" claim here was a bad audit: it grepped raw +0x1e8
// offsets, but PDB-typed Ghidra renders the field BY NAME -- grep `nScriptToHit`, not the offset.
BEGIN_SCRIPT_COMMAND( UnitSetToHit, "un[-1]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
		pUS->SetScriptToHit( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaUnitLockPose @0x2fc850 ("ub[false]"): set the unit's pose-lock (CUnitServer+0x1f4,
// bForceNoChangePose). While locked, CannotFreelyChangePoses() forces move-only pathfinding, so the
// unit keeps its current pose along any walk (used by scripts to hold a unit's stance during a scene).
BEGIN_SCRIPT_COMMAND( UnitLockPose, "ub[false]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
		pUS->SetForceNoChangePose( luaParams[ 1 ].b );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( HasInventoryItem, "n" )
	NWorld::IPlayer::CUnitSet units;
	int nHasItem = 0;
	CPtr<NWorld::CWorld> pWorld = pScript->pWorld;
	NWorld::CUnitServer *pWhoHas = 0;
	if ( !pWorld )
	{
		pScript->PushNil();
		pScript->PushNil();
		return 2;
	}
	NWorld::IPlayer *pPlayer = pWorld->GetNextPlayerForScript( 0 );
	while ( pPlayer )
	{
		if ( CDynamicCast<NAI::CAICommander>( pPlayer->GetCommander() ) )
		{
			pPlayer = pWorld->GetNextPlayerForScript( pPlayer );
			continue;
		}
		pPlayer->GetUnits( &units );
		for ( int i = 0; i < units.size(); ++i )
		{
			NWorld::CUnit *pWho = units[i];
			CDynamicCast<NWorld::CUnitServer> pServer( pWho );
			NRPG::IInventoryInfo *pInfo = pWho->GetRPG()->GetInventoryInfo();
			for ( int j = 0; j < pInfo->GetItems().size(); ++j )
			{
				const NRPG::SBackPackItem &item = (pInfo->GetItems())[ j ];
				const NDb::CRPGItem *pDBItem = item.pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					if ( !pWhoHas ) pWhoHas = pServer;   // retail returns the FIRST holder, not the last
				}
			}
			NRPG::IInventoryItem *pItem = pInfo->Get( NDb::SLOT_1 );
			if ( pItem )
			{
				const NDb::CRPGItem *pDBItem = pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					pWhoHas = pServer;
				}
			}
			pItem = pInfo->Get( NDb::SLOT_2 );
			if ( pItem )
			{
				const NDb::CRPGItem *pDBItem = pItem->GetDBItem();
				if ( pDBItem->GetRecordID() == luaParams[ 0 ].n )
				{
					++nHasItem;
					pWhoHas = pServer;
				}
			}
		}
		pPlayer = pWorld->GetNextPlayerForScript( pPlayer );
	}
	if ( nHasItem )
		pScript->PushNumber( nHasItem );
	else
		pScript->PushNil();
	luaPushCPtr( pState, pWhoHas );
	return 2;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetXPLevel, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		int nLevel = (int)( luaParams[ 1 ].f + 0.5f );   // retail @0x2f6840 rounds to nearest (not truncate)
		pUS->GetUnitRPG()->GetRPGUnit()->SetXPLevel( nLevel );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
int luaUnitApplyCritical( lua_State* pState )
{
	ASSERT( pState != 0 );
	if ( pState == 0 )
		return 0;
	//
	CScript *pScript;
	bool bCriticalFromTable = luaGetParamCount( pState ) == 2;
	if ( !bCriticalFromTable )
	{
		if ( !luaPrepareData( pState, "UnitApplyCritical", "unn", &pScript, &vector<SLuaParams>() ) )
			return 0;
	}
	else
	{
		if ( !luaPrepareData( pState, "UnitApplyCritical", "un", &pScript, &vector<SLuaParams>() ) )
			return 0;
	}
	//
	CDynamicCast<NWorld::CUnitServer> pUS( luaGetPtr( pScript->GetObject( 1 ) ) );
	if ( !IsValid( pUS ) )
		return 0;
	//
	NRPG::SCritical rpgCritical;
	if ( bCriticalFromTable )
	{
		int nID = pScript->GetObject(2).GetInteger();
		CDBPtr<NDb::CRPGCritical> pCritical = NDb::GetDBCritical( nID );
		if ( !IsValid( pCritical ) )                 // retail @0x2f6a60 guards an unknown critical id
			return 0;
		rpgCritical = NRPG::SCritical( pCritical->hl,
			pCritical->type, pCritical->nMaxDuration, pCritical->fValue );
	}
	else
	{
		NDb::ECriticalLocation location = (NDb::ECriticalLocation)pScript->GetObject(2).GetInteger();
		NDb::ECritical critical = (NDb::ECritical)pScript->GetObject(3).GetInteger();
		rpgCritical = NRPG::SCritical( location, critical );
	}
	//
	pUS->GetUnitRPG()->ApplyCritical( rpgCritical );
	pUS->ProcessCriticalImmediately( rpgCritical.eCritical );
	csRPG << "<color=red>Critical applied\n";
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsAction, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) && pUS->IsPerformingAction() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitShoot, "uun" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	//
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( !IsValid( pTarget ) )
		return 0;
	//
	NAI::EHitLocation hl = ( NAI::EHitLocation )( int )luaParams[ 2 ].n;
	pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdShootObject( pTarget, 0, hl ) ) );
	pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetShootMode, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NDb::EShootMode mode = ( NDb::EShootMode )luaParams[ 1 ].n;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdShootMode( mode ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetWishPose, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NAI::EPose pose = ( NAI::EPose )luaParams[ 1 ].n;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdWishPose( pose ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetPose, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NAI::EPose pose = ( NAI::EPose )luaParams[ 1 ].n;
		NAI::SUnitPosition pos = pUS->GetPosition();
		pos.SetPose( pose );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos.pos, NAI::PF_USE_POSEDIR ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetDirection, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NAI::EDirection dir = ( NAI::EDirection )luaParams[ 1 ].n;
		NAI::SPosition pos = pUS->GetPosition().pos;
		pos.p.SetDirection( dir );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPath( pos, NAI::PF_USE_DIR ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsDead, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	//
	if ( pUS->IsDead() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsUnconscious, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		if ( pUS->IsUnconscious() )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetVisible, "u" )
	CObj<NWorld::CUnitGroup> pGroup = pScript->pWorld->CreateUnitGroup();
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		for ( list< CPtr<NWorld::CUnitServer> >::const_iterator 
			i = pUS->GetTBSVisible().begin(); i != pUS->GetTBSVisible().end(); ++i )
				pGroup->units.Add( i->GetPtr() );
	}
	luaPushCObj( pState, pGroup );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsSeeUnit, "uu" )
	CDynamicCast<NWorld::CUnitServer> pWatcher( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( IsValid( pWatcher ) && IsValid( pTarget ) )
	{
		if ( pScript->pWorld->GetGame()->CheckVisibility( pWatcher, pTarget ) )
		{
			pScript->PushNumber( 1 );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitReload, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdReload() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitCheat, "unb" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		int nCheat = luaParams[ 1 ].n;
		bool bEnable = luaParams[ 2 ].b;
		pUS->GetUnitRPG()->GetRPGUnit()->SetCheat( nCheat, bEnable );
		//
		if ( nCheat == NRPG::CHEAT_NOAI )
		{
			CDynamicCast<NAI::CAICommander> pAICommander(pUS->GetPlayer()->GetCommander());
			if (pAICommander)
			{
				CPtr<NAI::IAIUnit> pAIUnit = pAICommander->GetAIUnit( pUS );
				ASSERT( IsValid( pAIUnit ) );
				if ( IsValid( pAIUnit ) )
				{
					if ( bEnable ) 
						pAIUnit->DeactivateCurrentControl();
					else
						pAIUnit->ActivateCurrentControl();
				}
			}
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitKill, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NRPG::SCritical rpgCritical( NDb::CL_HEAD, NDb::C_DEATH );
		pUS->GetUnitRPG()->ApplyCritical( rpgCritical );
		pUS->ProcessCriticalImmediately( rpgCritical.eCritical );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaUnitPlayAnimation @0x2f80f0 ("unbb[false]"): the 3rd bool selects the MODE, not looping.
// false -> play as a cancellable COMMAND (4th bool = bLoop). true -> install the clip as the unit's
// CUSTOM IDLE (CUnitAnimator::SetCustomIdleAnimation): no command at all -- the animator plays it
// whenever the unit idles, it can't be cancelled by commands, and the unit auto-returns to it after
// any interruption. The old Jan03 "unb" reading (3rd arg = bCircled command loop) turned scripted
// persistent poses (e.g. EFirst's lying wounded commander, anim 4437) into one-shot commands that
// the first stray order cancelled forever.
BEGIN_SCRIPT_COMMAND( UnitPlayAnimation, "unbb[false]" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		int nDBAnimationID = luaParams[ 1 ].n;
		if ( !luaParams[ 2 ].b )
		{
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdPlayAnimation( nDBAnimationID, luaParams[ 3 ].b ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
		else
			pUS->animator.SetCustomIdleAnimation( NDb::GetAnimation( nDBAnimationID ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitHide, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdHide() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitDrawPerksTree, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree()->Draw();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitTakePerk, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
		pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree()->TakePerk( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fcf20: spend all of the unit's perk points on randomly-chosen available perks
// (GetRPGUnit()->GetPerksTree()->TakeRandomPerks()). The campaign uses it to flesh out scripted units.
BEGIN_SCRIPT_COMMAND( UnitGiveRandomPerks, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
		pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree()->TakeRandomPerks();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fa2c0: park an item's model in the unit's hand (the "Item" bind bone) -- the unit visibly holds it.
// A nil/invalid rpgitem id clears the held model. (CUnitServer IS-A CDumbUnitServer, so SetHandModel is inherited.)
BEGIN_SCRIPT_COMMAND( UnitHoldItem, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NDb::CRPGItem *pItem = NDb::GetRPGItem( luaParams[ 1 ].n );
		NDb::CModel *pModel = 0;
		if ( IsValid( pItem ) && IsValid( pItem->pModel ) )
		{
			SRand rnd;
			pModel = pItem->pModel->CreateModel( &rnd );
		}
		pUS->SetHandModel( pModel );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitCancelAction, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
		pUS->Do( new NWorld::CCmdCancel() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRemove, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		// ���� ��� clue, �� �� ��������� ������������
		CPtr<NScenario::CScenarioTracker> pTracker = pScript->pWorld->GetGlobalGame()->pScenarioTracker;
		if ( IsValid( pTracker ) )
		{
			int nPersID = pUS->GetUnitRPG()->GetRPGPersID();
			CPtr<NScenario::CScenarioClue> pClue = pTracker->GetClueByPersID( nPersID );
			if ( !pTracker->IsClueFound( pClue ) )
				pTracker->OnScenarioClueDestroyed( nPersID, true );
		}
		//
		pScript->pWorld->RemoveUnit( pUS );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetName, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		string szName;
		if ( pScript->pWorld->GetUnitName( pUS, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetPlayer, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( luaParams[ 1 ].n );
		if ( IsValid( pPlayer ) )
		{
			pScript->pWorld->ChangeUnitPlayer( pUS, pPlayer );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetDialog, "us" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		pUS->SetDialog( luaParams[ 1 ].s );
		pUS->SetCanTalk( true );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetCanTalk, "ub[true]" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
		pUS->SetCanTalk( luaParams[ 1 ].b );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetHero, "" )
	CPtr<NWorld::CWorld> pWorld = pScript->pWorld;
	CPtr<NRPG::CUnit> pHero = pWorld->GetGlobalGame()->GetHero();
	if ( IsValid( pHero ) )
		luaPushCPtr( pState, pWorld->GetUnitServerByPersID( pHero->GetPers()->GetRecordID() ) );
	else
		lua_pushnil( pState );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitTakeCorpse, "uu" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pCorpse( luaParams[ 1 ].p );
	if ( IsValid( pUS ) && IsValid( pCorpse ) && pUS->CanFight() && !pCorpse->CanFight() )
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdTakeCorpse( ( NWorld::CUnit * )pCorpse ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitDropCorpse, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdDropCorpse() ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitMakeUnconscious, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
		pUS->MakeUnconscious( VNULL3, true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitActivateWeapon, "ub[true]" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		NWorld::ENeedActiveItem needActiveItem = luaParams[ 1 ].b ? NWorld::ITEM_ACTIVE : NWorld::ITEM_INACTIVE;
		pUS->Do( new NWorld::CCmdSetCommand( pUS, 
			new NWorld::CCmdPath( pUS->GetPosition().pos, NAI::PF_DEFAULT, needActiveItem ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitPlaceInPocket, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		if ( !pScript->pWorld->IsUnitInPocket( pUS ) )
		{
			pUS->SetState( new NWorld::CUnitStateInPocket( pUS ) );
			pUS->Update();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitRestoreFromPocket, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		if ( pScript->pWorld->IsUnitInPocket( pUS ) )
		{
			pUS->SetState( new NWorld::CUnitStateNormal( pUS ) );
			pUS->Update();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetRoute, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CDynamicCast<NAI::CAICommander> pCommander(pUS->GetPlayer()->GetCommander());
		if (pCommander)
		{
			CPtr<NAI::IAIUnit> pUnit( pCommander->GetAIUnit( pUS ) );
			if ( IsValid( pUnit ) )
			{
				luaPushCPtr( pState, pUnit->GetRoute() );
				return 1;
			}
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( RouteIsFinished, "u" )
	bool bFinished = true;
	CDynamicCast<NAI::CTask> pTask(luaParams[0].p);
	if (pTask)
		bFinished = pTask->IsEndOfTask();
	luaPushBool( pState, bFinished );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitAI, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		CDynamicCast<NAI::CAICommander> pCommander(pUS->GetPlayer()->GetCommander());
		if (pCommander)
		{
			CPtr<NAI::IAIUnit> pUnit( pCommander->GetAIUnit( pUS ) );
			if ( IsValid( pUnit ) )
				pUnit->GetAIInventory()->DebugOutput();
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// ===== LUA convergence batch 1 (retail parity, 2026-06-25) ========================================
// Reconstructed from the decoded retail handlers (decomp src/s2_scriptunit.h). The skill-id range
// guard in the retail get/set-skill handlers is an always-true OR / unsatisfiable AND, so the
// "Invalid skill id" warning is dead code and EVERY id is used -- faithfully reproduced here as
// "no guard" (the id is passed straight to Skills()).
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetSkill, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
	{
		pScript->PushNil();                                  // failed cast -> nil (retail @0x2fad00)
		return 1;
	}
	pScript->PushNumber( pUS->GetUnitRPG()->GetRPGUnit()->Skills( luaParams[ 1 ].n ) );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetSkillMaxValue, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;                                            // failed cast -> no push (retail @0x2fb020)
	pScript->PushNumber( pUS->GetUnitRPG()->GetRPGUnit()->Skills( luaParams[ 1 ].n ).GetMaxValue() );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetSkill, "unn" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	NRPG::CDynamicSkill &skill = pUS->GetUnitRPG()->GetRPGUnit()->Skills( luaParams[ 1 ].n );
	int nValue = luaParams[ 2 ].n;
	if ( nValue > skill.GetMaxValue() )                      // clamp to the current max (retail @0x2faea0)
		nValue = skill.GetMaxValue();
	skill.SetValue( nValue );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetSkillMaxValue, "unn" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	// retail @0x2fb1b0: grow the skill's XP-part so its max becomes the requested value, shifting the
	// current value by the same delta. CDynamicSkill::SetNewMaxValue is exactly that math.
	pUS->GetUnitRPG()->GetRPGUnit()->Skills( luaParams[ 1 ].n ).SetNewMaxValue( luaParams[ 2 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGiveXP, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		pUS->GetUnitRPG()->GetRPGUnit()->AddXP( luaParams[ 1 ].f );   // retail @0x2f6960
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsHearUnit, "uu" )
	CDynamicCast<NWorld::CUnitServer> pWatcher( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( IsValid( pWatcher ) && IsValid( pTarget ) && pWatcher->IsAudible( pTarget ) )
		pScript->PushNumber( 1 );                            // retail @0x2fc490 (CAudibleSet membership)
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsUsingCannon, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) && IsValid( pUS->GetUnitRPG()->GetRPGUnit()->GetCannonItem() ) )
		pScript->PushNumber( 1 );                            // retail @0x2fc190
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitIsWearingPK, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) && IsValid( pUS->GetWearingPK() ) )
		pScript->PushNumber( 1 );                            // retail @0x2f9b30
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetUnitPK, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		pScript->PushNil();
	else
		luaPushCPtr( pState, pUS->GetWearingPK() );          // nil-safe push (retail @0x2fd030)
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetHideProbability, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	CDynamicCast<NAI::CAICommander> pCommander( pUS->GetPlayer()->GetCommander() );
	if ( pCommander )
	{
		CPtr<NAI::IAIUnit> pAIUnit( pCommander->GetAIUnit( pUS ) );
		if ( IsValid( pAIUnit ) )
			pAIUnit->SetHideProbability( luaParams[ 1 ].n );  // retail @0x2fd420
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitMakeRouteInterruptable, "u" )
	// retail @0x2ead60: parses the unit arg but performs no action (no-op stub). Reproduced faithfully.
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f6d00: apply a DB-table critical to the unit. The 3rd arg is an optional duration/DC
// override (default -1 -> N_MAX_DC); it lands in SCritical::nDC while the record supplies the rest.
BEGIN_SCRIPT_COMMAND( UnitApplyTableCritical, "unn[-1]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( !IsValid( pUS ) )
		return 0;
	CDBPtr<NDb::CRPGCritical> pCritical = NDb::GetDBCritical( luaParams[ 1 ].n );
	if ( IsValid( pCritical ) )
	{
		int nDC = luaParams[ 2 ].n;
		if ( nDC < 0 )
			nDC = NRPG::N_MAX_DC;                            // retail default 300 (0x12c)
		NRPG::SCritical rpgCritical( pCritical->hl, pCritical->type,
			pCritical->nMaxDuration, pCritical->fValue, nDC );
		pUS->GetUnitRPG()->ApplyCritical( rpgCritical );
		pUS->ProcessCriticalImmediately( rpgCritical.eCritical );
		csRPG << "<color=red>Table critical applied\n";
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// ===== LUA convergence batch 2 (retail parity, NEEDS-CARE tier) ===================================
// Per-unit item count, mirroring the existing HasInventoryItem walk (backpack items + the two fixed
// hand slots). Shared by HasInventoryItemUnit / HasInventoryItemGroup.
static int CountUnitItems( NWorld::CUnit *pWho, int nItemID )
{
	if ( !pWho )
		return 0;
	int nCount = 0;
	NRPG::IInventoryInfo *pInfo = pWho->GetRPG()->GetInventoryInfo();
	const vector<NRPG::SBackPackItem> &items = pInfo->GetItems();
	for ( int j = 0; j < items.size(); ++j )
	{
		if ( items[ j ].pItem->GetDBItem()->GetRecordID() == nItemID )
			++nCount;
	}
	NRPG::IInventoryItem *pItem = pInfo->Get( NDb::SLOT_1 );
	if ( pItem && pItem->GetDBItem()->GetRecordID() == nItemID )
		++nCount;
	pItem = pInfo->Get( NDb::SLOT_2 );
	if ( pItem && pItem->GetDBItem()->GetRecordID() == nItemID )
		++nCount;
	return nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fbb40: fire at the named waypoint's tile, if it resolves & is live.
BEGIN_SCRIPT_COMMAND( UnitAttackWaypoint, "us" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			CVec3 cp = pWaypoint->pos.GetCP();
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdShootTile( cp ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( HasInventoryItemUnit, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );   // retail @0x2f6500
	if ( pUS && CountUnitItems( pUS, luaParams[ 1 ].n ) > 0 )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( HasInventoryItemGroup, "un" )
	CDynamicCast<NWorld::CUnitGroup> pGroup( luaParams[ 0 ].p );   // retail @0x2f6650
	bool bFound = false;
	if ( pGroup )
	{
		for ( int i = 0; i < pGroup->units.GetSize(); ++i )
		{
			if ( CountUnitItems( pGroup->units[ i ], luaParams[ 1 ].n ) > 0 )
			{
				bFound = true;
				break;
			}
		}
	}
	if ( bFound )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fbe00: stop the unit's current AI logic and issue a cancel command.
BEGIN_SCRIPT_COMMAND( UnitStop, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
		{
			NAI::IAILogic *pLogic = pAI->GetLogic();
			if ( IsValid( pLogic ) )
				pLogic->StopThinking();				// drop the current logic (cancel/terminate)
		}
		pUS->Do( new NWorld::CCmdCancel( pUS ) );	// cancel the unit's active command
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fbcc0 ("u") -> NAI::CAIUnit::ContinueRoute @0xadf20 (IAIUnit vtbl+0x40): tell a unit whose
// route was interrupted to keep moving along it. Retail resumes the route-following logic and cancels the
// interrupting combat logic (pCurrentLogic). DEV DELTA: the dev route lives in the IAIControl stack
// (GetRoute/ActivateCurrentControl), NOT as an IAILogic; pLogic is the reaction-installed (combat) logic --
// i.e. the retail "pCurrentLogic" being cancelled, NOT the route. So Resume(routes[live]) maps to
// re-activating the route control, and Cancel(pCurrentLogic) maps to StopThinking() on pLogic. For a
// script-routed unit pLogic is normally null (the AIM_SCRIPT route control outranks the AI reaction), so
// that half is usually a no-op -- which matches retail's null-route-slot path. (Sibling: UnitStop above.)
BEGIN_SCRIPT_COMMAND( UnitKeepMoving, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		NAI::IAIUnit *pAI = NAI::GetAIUnit( pUS );
		if ( IsValid( pAI ) )
		{
			pAI->ActivateCurrentControl();				// resume the route control (retail Resume routes[live])
			NAI::IAILogic *pLogic = pAI->GetLogic();
			if ( IsValid( pLogic ) )
				pLogic->StopThinking();					// cancel the interrupting logic (retail Cancel pCurrentLogic)
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fd180: kill every live enemy unit on the map (any unit not owned by the
// active player).  Retail also AddRef's the global game across the loop -- a lifetime
// hold with no observable effect here, so it is omitted.
BEGIN_SCRIPT_COMMAND( KillEmAll, "" )
	// retail @0x2fd180: kill only units that are DS_ENEMY to the HUMAN player. Retail uses the PLAYER-vs-PLAYER
	// diplomacy -- GetPlayerByID(0) (the human/scenario-player-0, NOT the active/whose-turn player) and the
	// (IPlayer,IPlayer) global-table overload, i.e. GetDiplomacyState( player0, unit->GetPlayer() ) == DS_ENEMY.
	// The earlier reconstruction killed every unit not owned by the active player (wiped the whole squad); the
	// follow-up used the (CUnit,IPlayer) overload which returns DS_ENEMY whenever a unit's RPG does not cast to
	// IUnitMission -- that killed the MAIN HERO specifically. The global table makes each player DS_ALLY to itself
	// (CGlobalDiplomacy ctor MakeSelfAFriend), so the player's own units (incl. the main hero) are correctly spared.
	NWorld::IPlayer *pHuman = pScript->pWorld->GetPlayerByID( 0 );
	if ( !IsValid( pHuman ) )
		return 0;
	vector< CPtr<NWorld::CUnit> > units;
	pScript->pWorld->GetAllUnits( &units );
	for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		CDynamicCast<NWorld::CUnitServer> pUS( *i );
		if ( IsValid( pUS ) && !pUS->IsDead() && IsValid( pUS->GetPlayer() ) &&
			pScript->pWorld->GetDiplomacyState( pHuman, pUS->GetPlayer() ) == NDb::DS_ENEMY )
			pUS->KillUnit( CVec3( 0, 0, 1 ) );		// VNULL3 with z=1 (death direction seed)
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fb330: like UnitShoot but only prepare the shot (no fire).
BEGIN_SCRIPT_COMMAND( UnitShootPrepare, "uun" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( pUS && pTarget )
	{
		NAI::EHitLocation hl = ( NAI::EHitLocation )( int )luaParams[ 2 ].n;
		NWorld::CCmdShootObject *pCmd = new NWorld::CCmdShootObject( pTarget, 0, hl );
		pCmd->bOnlyPrepareToShoot = true;			// public flag: aim/prepare only
		pUS->Do( new NWorld::CCmdSetCommand( pUS, pCmd ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f9da0: leave the PK the unit sits in, or warn if it is not in one; then
// always push the unit.
BEGIN_SCRIPT_COMMAND( UnitLeavePK, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( pUS )
	{
		if ( IsValid( pUS->GetWearingPK() ) )
		{
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdExitPK() ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
		else
		{
			string szName;
			pScript->pWorld->GetUnitName( pUS, &szName );
			csSystem << CC_WHITE << "Script warning: " << "\"" << szName << "\"" << " unit "
				<< "\"" << szName << "\"" << " does not sit in PK" << endl;
		}
	}
	luaPushCPtr( pState, pUS );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f9c80 (helper @0x2f59f0): "wear PK" -- take the corpse body.
// ORIGINAL BUG (confirmed by disasm): the guard takes the corpse when it CAN fight and
// warns "not an empty PK" when it CANNOT -- the inverse of the obvious test.  Kept faithful.
BEGIN_SCRIPT_COMMAND( UnitWearPK, "uu" )
	CDynamicCast<NWorld::CUnitServer> pWho( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pCorpse( luaParams[ 1 ].p );
	if ( pWho && pCorpse )
	{
		if ( pCorpse->CanFight() )						// ORIGINAL BUG: inverted
			pWho->Do( new NWorld::CCmdSetCommand( pWho, new NWorld::CCmdTakeCorpse( ( NWorld::CUnit * )pCorpse ) ) );
		else
		{
			string szWho, szCorpse;
			pScript->pWorld->GetUnitName( pWho, &szWho );
			pScript->pWorld->GetUnitName( pCorpse, &szCorpse );
			csSystem << CC_RED << "Script warning: " << szWho << " unit " << szCorpse
				<< " is not an empty PK" << endl;
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fce00: heal up to `count` criticals on the unit (count<0 -> heal all).
BEGIN_SCRIPT_COMMAND( UnitHealCriticals, "un[-1]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		int nCount = luaParams[ 1 ].n;				// optional, default -1
		if ( nCount < 0 )
			nCount = 0x7fffffff;					// heal ALL criticals
		pUS->GetUnitRPG()->HealCriticals( nCount );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fc300: is the carrier currently hauling exactly this corpse?
BEGIN_SCRIPT_COMMAND( UnitIsCarryingCorpse, "uu" )
	CDynamicCast<NWorld::CUnitServer> pCarrier( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CDumbUnitServer> pCorpse( luaParams[ 1 ].p );
	if ( IsValid( pCarrier ) && IsValid( pCorpse ) && pCarrier->GetCorpse() == pCorpse )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fbfd0: create the named DB item in the unit's inventory, or warn on a bad id.
// NB: the dev CCmdCreateInventoryItem has no bEquip arg, so the spec's 3rd param is
// accepted (compat) but not honored -- the predecessor command always creates unequipped.
BEGIN_SCRIPT_COMMAND( UnitCreateItem, "unb[false]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		NDb::CRPGItem *pItem = NDb::GetRPGItem( luaParams[ 1 ].n );
		if ( IsValid( pItem ) )
		{
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdCreateInventoryItem( pItem ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
		else
			csSystem << CC_RED << "Script warning: Invalid rpgitem id: " << luaParams[ 1 ].n << endl;
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared scan+equip for UnitDrawWeapon / UnitSwitchToGrenade (retail @0x2faa50 / @0x2fa760): find the
// FIRST live backpack item of runtime type TItem and exchange it into the unit's active hand, displacing
// the in-hand item. The target hand slot = (no active item) ? SLOT_2 : SLOT_1 (the retail "(active==0)?1:0",
// disasm @0x6fabc9). The exchange is the new NWorld::CCmdExchangeInventoryItems.
template< class TItem >
static void DrawFirstInventoryItem( NWorld::CUnitServer *pUS )
{
	NRPG::IInventoryInfo *pInfo = pUS->GetRPG()->GetInventoryInfo();
	const vector<NRPG::SBackPackItem> &items = pInfo->GetItems();
	for ( int i = 0; i < items.size(); ++i )
	{
		NRPG::IInventoryItem *pItem = items[ i ].pItem;
		if ( !IsValid( pItem ) || !CDynamicCast<TItem>( pItem ) )
			continue;
		int nSlot = ( pInfo->GetActive() == 0 ) ? NDb::SLOT_2 : NDb::SLOT_1;
		NWorld::SItem sSource( pUS, NWorld::SItem::BACKPACK, pItem );
		NWorld::SItem sTarget( pUS, NWorld::SItem::SLOT, nSlot, pInfo->Get( (NDb::ESlot)nSlot ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdExchangeInventoryItems( sSource, sTarget, true, nSlot ) ) );
		pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		return;		// first match only
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2faa50 ("u"): draw the first firearm from the backpack into the active hand.
BEGIN_SCRIPT_COMMAND( UnitDrawWeapon, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
		DrawFirstInventoryItem<NRPG::CWeaponItem>( pUS );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fa760 ("u"): if neither hand already holds a grenade, draw the first grenade from the
// backpack into the active hand.
BEGIN_SCRIPT_COMMAND( UnitSwitchToGrenade, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		NRPG::IInventoryInfo *pInfo = pUS->GetRPG()->GetInventoryInfo();
		if ( !CDynamicCast<NRPG::CGrenadeItem>( pInfo->Get( NDb::SLOT_1 ) ) &&
			 !CDynamicCast<NRPG::CGrenadeItem>( pInfo->Get( NDb::SLOT_2 ) ) )
			DrawFirstInventoryItem<NRPG::CGrenadeItem>( pUS );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fa400 ("un"): create an item from a DB record id and slot it into the unit's active hand.
BEGIN_SCRIPT_COMMAND( CreateAndActivateItem, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		NDb::CRPGItem *pItem = NDb::GetRPGItem( luaParams[ 1 ].n );
		if ( IsValid( pItem ) )
		{
			int nSlot = pUS->GetRPG()->GetInventoryInfo()->GetActiveSlot();
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdCreateAndActivateInventoryItem( pItem, nSlot ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
		else
			csSystem << CC_RED << "Script warning: Invalid rpgitem id: " << luaParams[ 1 ].n << endl;
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2f10f0 ("u"): destroy the unit's active in-hand item. Retail queues a CCmdMoveInventoryItem
// from the hand SLOT to VACUUM (remove from world) + a Continue. Adapted to the dev's slot-index
// inventory model (the retail nSlot=template-handle quirk is retail-specific): take the active slot's
// item and move it to the new SItem::VACUUM target, which drops it (the visual refreshes next animator
// update, as with the other BATCH-9 inventory commands).
BEGIN_SCRIPT_COMMAND( DestroyItemInHand, "u" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		NRPG::IInventoryInfo *pInfo = pUS->GetRPG()->GetInventoryInfo();
		int nSlot = pInfo->GetActiveSlot();
		if ( nSlot >= 0 && IsValid( pInfo->GetActive() ) )
		{
			NWorld::SItem sSource( pUS, NWorld::SItem::SLOT, nSlot, pInfo->Get( (NDb::ESlot)nSlot ) );
			NWorld::SItem sTarget( pUS, NWorld::SItem::VACUUM, 0 );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdMoveInventoryItem( sSource, sTarget ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fc620 ("un"): is the unit's ACTIVE hand item a weapon of the queried kind? arg1 selects the
// kind: 0 = grenade (IGrenadeItemInfo), 1 = melee weapon (IMeleeWeaponItem). Pushes 1 on a match, else nil.
BEGIN_SCRIPT_COMMAND( UnitIsWeaponInHand, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	int nKind = luaParams[ 1 ].n;
	if ( pUS && nKind >= 0 && nKind < 2 )
	{
		NRPG::IInventoryItem *pItem = pUS->GetRPG()->GetInventoryInfo()->GetActive();
		if ( IsValid( pItem ) )
		{
			if ( nKind == 0 )
			{
				if ( CDynamicCast<NRPG::IGrenadeItemInfo>( pItem ) )
				{
					pScript->PushNumber( 1 );
					return 1;
				}
			}
			else if ( CDynamicCast<NRPG::IMeleeWeaponItem>( pItem ) )
			{
				pScript->PushNumber( 1 );
				return 1;
			}
		}
	}
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fb8e0 ("uss"): is the unit's footprint inside the axis-aligned box spanned by the two named
// waypoints? Pushes 1 if inside, else nil; always returns 1.
// ORIGINAL BUG (confirmed @0x2fb8e0): the inside test is STRICT on all four edges, so a unit standing
// exactly on an area boundary reads as OUTSIDE -- faithfully reproduced.
BEGIN_SCRIPT_COMMAND( UnitInArea, "uss" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	bool bInside = false;
	if ( pUS )
	{
		CPtr<NAI::CAIRouteWaypoint> pC1 = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		CPtr<NAI::CAIRouteWaypoint> pC2 = pScript->pWorld->GetWaypoint( luaParams[ 2 ].s );
		if ( IsValid( pC1 ) && IsValid( pC2 ) )
		{
			CVec2 p = pUS->GetPosition().GetCPNoHeight();   // the height-stripped 2D footprint
			CVec2 a = pC1->pos.GetCPNoHeight();
			CVec2 b = pC2->pos.GetCPNoHeight();
			float maxX = a.x > b.x ? a.x : b.x;
			float minX = a.x < b.x ? a.x : b.x;
			float maxY = a.y > b.y ? a.y : b.y;
			float minY = a.y < b.y ? a.y : b.y;
			bInside = ( minX < p.x && p.x < maxX ) && ( minY < p.y && p.y < maxY );
		}
	}
	if ( bInside )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fccd0 ("un"): regenerate VP on the unit by the given amount -- a one-shot heal of fdVP points
// (capped at nMaxVP=1000, and by the unit's missing VP). Negative amounts are ignored.
BEGIN_SCRIPT_COMMAND( UnitRegenerateVP, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) && luaParams[ 1 ].n >= 0 )
	{
		NRPG::SFirstAid fa;
		fa.fdVP = (float)luaParams[ 1 ].n;
		fa.nMaxVP = 1000;
		pUS->GetUnitRPG()->GetRPGUnit()->RegenerateVP( fa );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fc960 ("uunn"): the unit instantly lobs grenade rpgitem [2] at the target unit. Script throw --
// no equipped grenade, AP or wind-up; NWorld::UnitThrowGrenade spawns + flies the grenade (see wUnitAttackExec).
BEGIN_SCRIPT_COMMAND( UnitGrenadeToUnit, "uunn" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnitServer> pTarget( luaParams[ 1 ].p );
	if ( IsValid( pUS ) && IsValid( pTarget ) )
	{
		NDb::CRPGGrenade *pGrenade = NDb::GetRPGGrenade( luaParams[ 2 ].n );
		NWorld::UnitThrowGrenade( pUS, pGrenade, pTarget->GetPosition().GetCP(), luaParams[ 3 ].n );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fcb00 ("usnn"): the unit instantly lobs grenade rpgitem [2] at the named waypoint's grid cell.
BEGIN_SCRIPT_COMMAND( UnitGrenadeToWaypoint, "usnn" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	if ( IsValid( pUS ) )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( IsValid( pWaypoint ) )
		{
			NDb::CRPGGrenade *pGrenade = NDb::GetRPGGrenade( luaParams[ 2 ].n );
			NWorld::UnitThrowGrenade( pUS, pGrenade, pWaypoint->pos.GetCP(), luaParams[ 3 ].n );
		}
		else
			csSystem << CC_RED << "Script warning: " << CC_GREY << " UnitGrenadeToWaypoint: waypoint [" << luaParams[ 1 ].s << "] not found" << endl;
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fb4b0 ("uun[-1]"): the shooter's chance to hit the target unit. The retail handler aims at the
// target (HL_HEAD = GetHitLocation(0)) and calls the heavy 4-arg NRPG::GetToHit convenience (which builds
// cover + accessible-HLs + the weapon-type dispatch internally) -- mapped here to the dev's purpose-built
// CGame::GetCompositeToHit (same "interface-grade to-hit" intent: it CreateAttack's, CalcCovers, and averages
// over the rate of fire). arg2 (default -1) is the bFirstTurn flag.
BEGIN_SCRIPT_COMMAND( UnitGetToHitUnit, "uun[-1]" )
	CDynamicCast<NWorld::CUnitServer> pShooter( luaParams[ 0 ].p );
	CDynamicCast<NWorld::CUnit> pTarget( luaParams[ 1 ].p );
	int nToHit = 0;
	if ( pShooter && IsValid( pTarget ) )
	{
		NRPG::IGame *pGame = pScript->pWorld->GetGame();
		if ( pGame )
			nToHit = pGame->GetCompositeToHit( pShooter, pTarget, NAI::HL_HEAD, luaParams[ 2 ].n != 0 );
	}
	pScript->PushNumber( nToHit );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fb610 ("usn[0]"): the unit's chance to hit the tile at a named waypoint; warn on a bad waypoint
// name. The retail aims at the waypoint CP and calls GetToHit -- mapped to the dev's CGame::GetTileCompositeToHit
// (THL_MIDDLE = the waypoint centre). arg2 (default 0) is the bFirstTurn flag.
BEGIN_SCRIPT_COMMAND( UnitGetToHitWaypoint, "usn[0]" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	int nToHit = 0;
	if ( pUS )
	{
		CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
		if ( !IsValid( pWaypoint ) )
			csSystem << CC_RED << "Script warning: " << "invalid waypoint name" << endl;
		else
		{
			NRPG::IGame *pGame = pScript->pWorld->GetGame();
			if ( pGame )
				nToHit = pGame->GetTileCompositeToHit( pUS, pWaypoint->pos.GetCP(), NAI::THL_MIDDLE, luaParams[ 2 ].n != 0 );
		}
	}
	pScript->PushNumber( nToHit );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2fa080 ("uu"): the unit picks up a world object -- move the given inventory item from the GROUND
// (its on-ground CDFrozenItem world representation) into the unit's BACKPACK at an auto-placed slot.
// arg1 is the item's NRPG::IInventoryItem; its world body is resolved via the debris controller (CWorld
// IS-A CDebrisController) GetFrozenItem(). Faithfulness: the retail builds the two SItems inline (the decomp's
// "@0x76fcb0" calls are just CPtr<IObject>::operator=, NOT a helper) with src/dst eType stores 3/4 -- which,
// under the release EPlacement enum (a VACUUM=0 prefix the dev enum lacks), are GROUND/BACKPACK; i.e. exactly
// the dev's proven loot-move idiom (aiLootAction.cpp:77-82). Issued via the CCmdSetCommand+CCmdContinue idiom
// the landed UnitCreateItem uses. (Build-verified; user runtime-tests the pickup behaviour.)
BEGIN_SCRIPT_COMMAND( UnitTakeObject, "uu" )
	CDynamicCast<NWorld::CUnitServer> pUS( luaParams[ 0 ].p );
	// GetItem(name) returns the on-ground CDFrozenItem -- an IItem that HOLDS the inventory item, not an
	// IInventoryItem itself. (Retail's GetItem hands back the IInventoryItem directly; the dev hands back the
	// world body, so adapt here.) The old IInventoryItem cast always failed for a CDFrozenItem, so the whole
	// command no-op'd: the boss never queued an action, so it never walked to / picked up the object, and
	// WaitForUnit returned instantly (UnitIsAction was nil). Cast to IItem and pull the held item out.
	CDynamicCast<NWorld::IItem> pWorldItem( luaParams[ 1 ].p );
	if ( pUS && pWorldItem )
	{
		NRPG::IInventoryItem *pInvItem = pWorldItem->GetInvItem();
		if ( IsValid( pInvItem ) )
		{
			NWorld::SItem src( (NWorld::CUnit*)0, NWorld::SItem::GROUND, pInvItem );
			src.nSlot = -1;
			src.pWorldItem = pWorldItem;	// the on-ground body -> GetActionValidPlaces walks the unit to it first
			NWorld::SItem dst( (NWorld::CUnit*)pUS.GetPtr(), NWorld::SItem::BACKPACK );
			dst.sPosition.x = -1;
			dst.sPosition.y = -1;
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdMoveInventoryItem( src, dst ) ) );
			pUS->Do( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdContinue() ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}