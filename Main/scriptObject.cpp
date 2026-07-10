#include "stdafx.h"
//
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "wMain.h"
#include "wOSBase.h"
#include "wObject.h"
#include "RPGItemSet.h"
#include "rpgAttackMech.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\MiscDll\LogStream.h"
#include "aiRoute.h"
#include "wDebris.h"
//
#include "scriptObject.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetItem, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CDFrozenItem> pItem = pScript->pWorld->GetItemByName( szName );
	luaPushCPtr( pState, pItem );
	if ( !IsValid( pItem ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " item [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// release luaFindItem @0x2e7170 (sig "n"): find the on-ground world item by its RPG db id. NOTE: retail
// pushes the INVENTORY item (frozen->GetInvItem(), oracle s2_nscript_scriptobject.h:832), NOT the
// CDFrozenItem world holder -- the consumers (ItemUnload -> CWeaponItem dyncast, UnitTakeItem) expect the
// NRPG item. Pushing the holder made every such call a silent dyncast miss. Warn on a failed lookup so a
// never-spawned map item is visible in the console.
BEGIN_SCRIPT_COMMAND( FindItem, "n" )
	int nID = luaParams[ 0 ].n;
	CPtr<NWorld::CDFrozenItem> pItem = pScript->pWorld->FindFrozenItem( nID );
	if ( IsValid( pItem ) && pItem->GetInvItem() != 0 )
		luaPushCPtr( pState, pItem->GetInvItem() );
	else
	{
		csSystem << CC_RED << "Script warning: " << CC_GREY << " FindItem(" << nID << ") -- no on-ground item with this RPG item id" << endl;
		pScript->PushNil();
	}
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ItemGetName, "u" )
	CDynamicCast<NWorld::CDFrozenItem> pItem(luaParams[0].p);
	if (pItem)
	{
		string szName;
		if ( pScript->pWorld->GetItemName( pItem, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ItemRemove, "u" )
	CDynamicCast<NWorld::CDFrozenItem> pItem(luaParams[0].p);
	if (pItem)
		pScript->pWorld->RemoveFrozenItem( pItem->GetInvItem() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetObject, "s" )
	string szName = luaParams[ 0 ].s;
	CPtr<NWorld::CObjectServerBase> pObject = pScript->pWorld->GetObjectByName( szName );
	luaPushCPtr( pState, pObject );
	if ( !IsValid( pObject ) )
		csSystem << CC_RED << "Script warning: " << CC_GREY << " object [" << szName << "] not found" << endl;
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
static void OpenCloseObject( CObjectBase *pObject, bool bOpen )
{
	CDynamicCast<NWorld::CWindowDoor> pDoor( pObject );
	if ( IsValid( pDoor ) )
		pDoor->OpenClose( bOpen, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectOpen, "u" )
	OpenCloseObject( luaParams[ 0 ].p, true );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectClose, "u" )
	OpenCloseObject( luaParams[ 0 ].p, false );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectIsOpened, "u" )
	CDynamicCast<NWorld::CWindowDoor> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsOpen() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectDestroy, "u" )
	if ( IsValid( luaParams[ 0 ].p ) )
	{
		CDynamicCast<NRPG::IAttackable> pAtt(luaParams[0].p);
		if (pAtt)
		{
			CRay ray;
			NRPG::CAttackPortion atk;
			ray.ptDir = VNULL3;
			ray.ptOrigin = VNULL3;
			atk.MakeClickOfDeath( ray );
			atk.atkType = NRPG::AT_NORMAL;
			pAtt->ProcessAttack( 0, &atk, NDb::GetArmor( NDb::N_DEFAULT_ARMOR ) );
		}
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectRemove, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pScript->pWorld->KillObject( pObject );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaObjectPlaceInPocket @0x2e9000 ("u"): stash a world object in the strategic pocket -- held
// alive but removed from the world (used by the training script 87 to hide the shooting-range targets).
// Retail gates on the RTTI cast alone (no liveness check); the in-pocket dedup lives in the world method.
BEGIN_SCRIPT_COMMAND( ObjectPlaceInPocket, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( pObject )
		pScript->pWorld->PlaceObjectInPocket( pObject );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaObjectRestoreFromPocket @0x2e9130 ("u"): bring a pocketed object back into the world.
BEGIN_SCRIPT_COMMAND( ObjectRestoreFromPocket, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( pObject )
		pScript->pWorld->RestoreObjectFromPocket( pObject );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( CreateObject, "nsns" )
	// DBID, WAYPOINT, ANGLE, NAME
	int nID = luaParams[ 0 ].n;
	string szWaypoint = luaParams[ 1 ].s;
	int nAngle = luaParams[ 2 ].n;
	string szName = luaParams[ 3 ].s;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( IsValid( pWaypoint ) )
	{
		CDBPtr<NDb::CRndObject> pDBRndObject = NDb::GetDBRndObject( nID );
		if ( IsValid( pDBRndObject ) )
		{
			SRand rand;
			vector<int> params;
			CPtr<NDb::CObject> pDBObject = pDBRndObject->CreateObject( &rand, params );
			if ( IsValid( pDBObject ) )
			{
				NWorld::SObjectPlace pos = pWaypoint->GetObjectPlace( nAngle );
				CPtr<NWorld::CObjectServerBase> pObject = pScript->pWorld->AddObject( pos, pDBObject, szName );
				if ( IsValid( pObject ) )
				{
					luaPushCPtr( pState, pObject );
					return 1;
				}
			}
		}
	}
	//
	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectSetToWaypoint, "usn" )
	string szWaypoint = luaParams[ 1 ].s;
	int nAngle = luaParams[ 2 ].n;
	// retail @0x2e8500 casts the OBJECT from slot 0 (the dev/Jan03 typo cast slot 1, the
	// waypoint-string slot, so the object never resolved and the command was a silent no-op).
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( !IsValid( pObject ) )
		return 0;
	//
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( szWaypoint );
	if ( !IsValid( pWaypoint ) )
		return 0;
	//
	NWorld::SObjectPlace pos = pWaypoint->GetObjectPlace( nAngle );
	pObject->SetPosition( pos );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail luaItemSetToWaypoint @0x2e86c0 ("us"): move an ON-GROUND inventory item to a waypoint. Retail
// reads the waypoint's place (waypoint+0x2c) then moves the item via the debris placer (pWorld vtbl+0xe4
// -> vtbl+0x20/commit). Dev's CDebrisController re-places by remove+re-add; a CPtr keeps the item alive
// across the remove. (Distinct from ObjectSetToWaypoint, which repositions a world OBJECT via SetPosition.)
BEGIN_SCRIPT_COMMAND( ItemSetToWaypoint, "us" )
	CDynamicCast<NRPG::IInventoryItem> pItem( luaParams[ 0 ].p );
	if ( !IsValid( pItem ) )
		return 0;
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 1 ].s );
	if ( !IsValid( pWaypoint ) )
		return 0;
	CPtr<NRPG::IInventoryItem> pHold = pItem;   // survive the remove below
	NWorld::SObjectPlace place = pWaypoint->GetObjectPlace( 0 );
	pScript->pWorld->RemoveFrozenItem( pItem );
	pScript->pWorld->AddFrozenItem( place.ptPos, CQuat( place.fAngle, CVec3( 0, 0, 1 ) ), pItem, false, place.nFloor );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectPlayAnimation, "un" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pObject->PlayDBAnimation( luaParams[ 1 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectIsAction, "u" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsPerformingAction() )
		pScript->PushNumber( 1 );
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectSetDestroyStage, "un" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) )
		pObject->SetDestroyStage( pScript->GetObject( 2 ).GetInteger() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectGetName, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pOS(luaParams[0].p);
	if (pOS)
	{
		string szName;
		if ( pScript->pWorld->GetObjectName( pOS, &szName ) )
		{
			pScript->PushString( szName.c_str() );
			return 1;
		}
	}
	else
		csSystem << CC_RED << "Invalid object : " << IsValid( luaParams[ 0 ].p ) << endl;

	pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectCancelAction, "u" )
	CDynamicCast<NWorld::CAnimObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( IsValid( pObject ) && pObject->IsPerformingAction() )
		pObject->CancelAction();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// ===== LUA convergence batch 2 (retail parity) ====================================================
BEGIN_SCRIPT_COMMAND( ObjectGetDestroyStage, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	if ( !pObject )
		pScript->PushNil();                                  // failed cast -> nil (retail @0x2e8ec0)
	else
		pScript->PushNumber( pObject->GetDestroyStage() );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectGetHP, "u" )
	CDynamicCast<NWorld::CObjectServerBase> pObject( luaParams[ 0 ].p );
	int nHP = 0;
	if ( pObject )
		nHP = pObject->GetHP();
	pScript->PushNumber( nHP );                              // retail @0x2e9260 always pushes (miss -> 0)
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2e7b70 / @0x2e7c90: lock / unlock a door. The "unn" signature is (door, keyID, lockHardness):
// the two ints are the required-key item id and the lockpick difficulty, forwarded to LockDoor and stored
// on the door (retail @0x3813b0). Unlock passes 0,0 (ignored when bLock is false).
BEGIN_SCRIPT_COMMAND( ObjectLockDoor, "unn" )
	CDynamicCast<NWorld::CWindowDoor> pDoor( luaParams[ 0 ].p );
	if ( IsValid( pDoor ) )
		pDoor->LockDoor( true, luaParams[ 1 ].n, luaParams[ 2 ].n );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ObjectUnlockDoor, "u" )
	CDynamicCast<NWorld::CWindowDoor> pDoor( luaParams[ 0 ].p );
	if ( IsValid( pDoor ) )
		pDoor->LockDoor( false, 0, 0 );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( ItemUnload, "u" )
	CDynamicCast<NRPG::CWeaponItem> pWeapon( luaParams[ 0 ].p );   // retail @0x2e76a0: drop loaded ammo
	if ( pWeapon )
		pWeapon->DiscardAmmo();
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}