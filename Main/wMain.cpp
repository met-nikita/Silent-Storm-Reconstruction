#include "StdAfx.h"
#include "Transform.h"
#include "InterfaceConst.h"
#include "MapBuild.h"
#include "wUnitServer.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "RPGUnitInfo.h"
#include "wObject.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataAI.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataSound.h"
#include "RPGUnitMission.h"
#include "RPGItemInfo.h"
#include "RPGAttackMech.h"
#include "RPGItem.h"
#include "GSceneUtils.h"
#include "aiMap.h"
#include "wDebris.h"
#include "BuildingGrid.h"
#include "GAnimation.h"
#include "MakeBuilding.h"
#include "wExplTracker.h"
#include "wBuilding.h"
#include "wGrenade.h"
#include "wKnife.h"
#include "wRocket.h"
#include "wBullet.h"
#include "wMisc.h"
#include "aiCommander.h"
#include "aiTaskCommander.h"
#include "wAckBase.h"
#include "wAck.h"
#include "wTerrain.h"
#include "..\DBFormat\DataAck.h"
#include "wMainMoves.h"
#include "wMainTrace.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataRPG.h"
#include "aiJob.h"
#include "aiSignal.h"
#include "A5Script.h"
#include "scriptCallLua.h"
#include "..\DBFormat\DataScenario.h"
#include "..\DBFormat\DataMisc.h"			// NDb::CUIHint + NDatabase::GetTable/CDBIterator (AddNextUIHint)
#include "scFlowChartItems.h"
#include "scScenarioTracker.h"
#include "RPGDiplomacy.h"
#include "RPGUnit.h"
#include "..\MiscDll\LogStream.h"
#include "RPGMerc.h"
#include "wUnitStates.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataCamera.h"
#include "..\DBFormat\DataScript.h"
#include "wUnitGroup.h"
#include "wUICommands.h"
#include "..\Misc\set.h"
#include "aiRoute.h"
#include "rpgCheatConstants.h"
#include "wUnitCommands.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\Misc\EventsBase.h"
#include "eventUnit.h"   // NWorld::CEventOnHearEnemy / CEventOnHearAlly / CEventOnStartGame (AI perception events)
#include "eventPlayer.h"
#include "eventWorld.h"     // NWorld::CEventOnSegment (thrown per segment for NAI::CAIScriptLogic)
#include "aiControl.h"
#include "wDecal.h"
#include "phCollider.h"
//
#include "wMain.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_REALTIME_TURN = 20000; // 20 � // turn, WaitForTurn, ...
const int N_REALTIME_FAST_TURN = 6000; // 6 � // heal, unhide, ...
externA5 vector<SSphere> sphereParticles; // for AI Sound test visualisation
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 CPtr<NScript::CScript> pScript;
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace NWorld
{
const int DW_SEGMENT_TIME = 50;
const int N_SOUND_PARTICLE_ID = 48;
const int N_MAX_MOVE_TO_BLOCK_REAL_TIME = 10;
const int N_STOREPLACE_WIDTH = 10; // If you change this, also you must change N_STORESLOT_DEFWIDTH in iStorePanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CWorld *pCurrentWorld = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcTransform( SObjectPlace *pRes, const SMapPosition &p )
{
	pRes->ptPos = p.ptPos; 
	pRes->ptScale = p.ptScale; 
	pRes->fAngle = ToRadian( p.fRotation );
	pRes->nFloor = p.nFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayer::CPlayer( const wstring &_wsName, NRPG::CGlobalGame *_pGlobalGame, NRPG::CGlobalPlayer *_pGlobalPlayer, int _nScenarioPlayerID ): 
	wsName( _wsName ), pGlobalGame( _pGlobalGame ), pGlobalPlayer( _pGlobalPlayer ), nScenarioPlayerID( _nScenarioPlayerID )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::SetCheat( int nCheat, bool bOn )
{
	vector< CPtr<CUnit> > units;
	GetUnits( &units );
	for (vector< CPtr<CUnit> >::iterator i = units.begin(); i != units.end(); ++i)
	{
		CDynamicCast<CUnitServer> pUS((*i).GetPtr());
		if (pUS)
			pUS->GetUnitRPG()->GetRPGUnit()->SetCheat(nCheat, bOn);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetVisible( list< CPtr<CUnit> > *pRes ) const
{
	pRes->clear();
	for ( TUnitList::const_iterator i = visible.begin(); i != visible.end(); ++i )
		pRes->push_back( i->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetVisibleObjects( list< CPtr<CObjectBase> > *pRes ) const
{
	pRes->clear();
	for ( TObjectList::const_iterator i = visibleObjects.begin(); i != visibleObjects.end(); ++i )
		pRes->push_back( i->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetTrappedObjectsList( list< CPtr<CObjectBase> > *pRes ) const
{
	pRes->clear();
	for ( TObjectList::const_iterator i = trappedObjects.begin(); i != trappedObjects.end(); ++i )
	{
		CObjectBase *p = *i;
		if ( !IsValid(p) )
			continue;
		CDynamicCast<IMine> pMine(p);
		if (pMine)
		{
			if ( pMine->IsMineSet() )
				pRes->push_back( p );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetSounds( vector<IVisObj*> *pRes )
{
	typedef SAISound<CUnitServer> TPlayer;
	list<TPlayer> sounds;
	for ( int k = 0; k < units.size(); ++k )
		units[k]->AddSounds( &sounds );
	FilterSounds( &sounds, visible );
	for ( list<TPlayer>::iterator i = sounds.begin(); i != sounds.end(); ++i )
	{
		const TPlayer &s = *i;
		for ( int k = 0; k < s.objects.size(); ++k )
			pRes->push_back( CDynamicCast<IVisObj>( s.objects[k] ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetUnits( vector<CPtr<CUnitServer> > *pRes ) const
{
	pRes->clear();
	for ( int k = 0; k < units.size(); ++k )
		pRes->push_back( units[k].GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetUnits( CUnitSet *pRes ) const
{
	ASSERT( pRes != 0 );
	if ( pRes == 0 )
		return;
	//
	pRes->clear();
	for ( int k = 0; k < units.size(); ++k )
		pRes->push_back( units[k].GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetUnitsRPGs( vector< CPtr<NRPG::IUnitMission> > *pRes ) const
{
	ASSERT( pRes != 0 );
	if ( pRes == 0 )
		return;
	//
	pRes->clear();
	for ( int k = 0; k < units.size(); ++k )
		pRes->push_back( units[k]->GetUnitRPG() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayer::GetInHandItem( SItemInfo *pInfo ) const
{
	if ( IsValid( pInHandItem ) )
	{
		pInfo->pItem = pInHandItem;
		return true;
	}

	for ( int nTemp = 0; nTemp < units.size(); nTemp++ )
	{
		CPtr<CUnit> pUnit = units[nTemp];
		if ( pUnit->IsDead() )
			continue;

		CPtr<NRPG::IInventoryItem> pItem = pUnit->GetRPG()->GetInventoryInfo()->GetHandItem();
		if ( IsValid( pItem ) )
		{
			pInfo->pUnit = pUnit;
			pInfo->pItem = pItem;
			return true;
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::SetInHandItem( const SItemInfo &sInfo )
{
	CDynamicCast<CUnitServer> pUS( sInfo.pUnit );
	if ( IsValid( pUS ) )
		pUS->GetUnitRPG()->GetInventory()->SetHandItem( sInfo.pItem );
	else
		pInHandItem = sInfo.pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetStoreItems( list<CPtr<NRPG::IInventoryItem> > *pItems )
{
	if ( !pGlobalGame->pScenarioTracker->IsScenarioAvailable() )
		return;

	int nGameRating = pGlobalGame->pScenarioTracker->GetMaxDifficulty();
	list<NRPG::SStoreItem> &storeItemsList = pGlobalPlayer->storeItemsList;

	{
		CDBTable<NDb::CRPGStoreItem> *pStoreItemsTable = NDatabase::GetTable<NDb::CRPGStoreItem>();
		CDBIterator<NDb::CRPGStoreItem> iTempItem( *pStoreItemsTable );

		while( iTempItem.MoveNext() )
		{
			CPtr<NDb::CRPGStoreItem> pItem = iTempItem.Get();
			if ( !IsValid( pItem->pItem ) )
			{
				ASSERT( 0 );
				continue;
			}
			if ( !IsValid( pItem->pItem->pSuccessor ) )
			{
				ASSERT( 0 );
				continue;
			}
			if ( pItem->pSide.GetPtr() != pGlobalPlayer->pSide.GetPtr() )
				continue;

			bool bFound = false;
			for( list<NRPG::SStoreItem>::const_iterator iTemp = storeItemsList.begin(); iTemp != storeItemsList.end(); iTemp++ )
			{
				if ( iTemp->pStoreItem != pItem )
					continue;

				bFound = true;
				break;
			}

			if ( !bFound )
			{
				NRPG::SStoreItem &sItem = *storeItemsList.insert( storeItemsList.end(), NRPG::SStoreItem());
				sItem.eType = NRPG::SStoreItem::REGEN_QUANTITY;
				sItem.nRating = pItem->nRating;
				sItem.fQuantity = pItem->fQuantity;
				sItem.pRPGItem = pItem->pItem;
				sItem.pStoreItem = pItem;

				int nCount = int( sItem.fQuantity );
				for ( int nTemp = 0; nTemp < nCount; nTemp++ )
					sItem.itemsList.push_back( NRPG::CreateItem( sItem.pRPGItem->pSuccessor ) );

				CDynamicCast<NDb::CRPGWeapon> pWeapon( pItem->pItem->pSuccessor );
				if ( IsValid( pWeapon ) && IsValid( pWeapon->pInnerClip ) && IsValid( pWeapon->pInnerClip->pItem ) )
				{
					int nCount = Max( 0, pItem->pItem->sSize.y * ( N_STORESLOT_DEFWIDTH - pItem->pItem->sSize.x ) );

					if ( nCount > 0 )
					{
						NRPG::SStoreItem &sItem = *storeItemsList.insert( storeItemsList.end(), NRPG::SStoreItem());
						sItem.eType = NRPG::SStoreItem::CONST_QUANTITY;
						sItem.nRating = -1;
						sItem.fQuantity = Max( 0, pItem->pItem->sSize.y * ( N_STORESLOT_DEFWIDTH - pItem->pItem->sSize.x ) );
						sItem.pRPGItem = pWeapon->pInnerClip->pItem;
						sItem.pStoreItem = pItem;

						for ( int nTemp = 0; nTemp < nCount; nTemp++ )
							sItem.itemsList.push_back( NRPG::CreateItem( sItem.pRPGItem->pSuccessor ) );
					}
				}
			}
		}
	}

	for( list<NRPG::SStoreItem>::iterator iTemp = storeItemsList.begin(); iTemp != storeItemsList.end(); iTemp++ )
	{
		if ( IsValid( iTemp->pStoreItem ) )
		{
			switch( iTemp->eType )
			{
			case NRPG::SStoreItem::CONST_QUANTITY:
				{
					int nCount = int( iTemp->fQuantity );
					for ( int nTemp = iTemp->itemsList.size(); nTemp < nCount; nTemp++ )
						iTemp->itemsList.push_back( NRPG::CreateItem( iTemp->pRPGItem->pSuccessor ) );

					break;
				}
			case NRPG::SStoreItem::REGEN_QUANTITY:
				{
					iTemp->fQuantity = ( nGameRating - iTemp->nRating ) * iTemp->pStoreItem->fQuantity;

					int nCount = int( iTemp->fQuantity );
					for ( int nTemp = iTemp->itemsList.size(); nTemp < nCount; nTemp++ )
						iTemp->itemsList.push_back( NRPG::CreateItem( iTemp->pRPGItem->pSuccessor ) );

					break;
				}
			}
		}

		for ( list<CObj<NRPG::IInventoryItem> >::const_iterator iItem = iTemp->itemsList.begin(); iItem != iTemp->itemsList.end(); iItem++ )
			pItems->push_back( iItem->GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayer::TakeStoreItem( NRPG::IInventoryItem *pItem )
{
	list<NRPG::SStoreItem> &storeItemsList = pGlobalPlayer->storeItemsList;
	for( list<NRPG::SStoreItem>::iterator iTemp = storeItemsList.begin(); iTemp != storeItemsList.end(); iTemp++ )
	{
		list<CObj<NRPG::IInventoryItem> >::iterator iItem = find( iTemp->itemsList.begin(), iTemp->itemsList.end(), pItem );
		if ( iItem == iTemp->itemsList.end() )
			continue;

		iTemp->itemsList.erase( iItem );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::PlaceStoreItem( NRPG::IInventoryItem *pItem )
{
	NRPG::SStoreItem sItem;
	sItem.eType = NRPG::SStoreItem::NONE;
	sItem.nRating = 0;
	sItem.itemsList.push_back( pItem );
	pGlobalPlayer->storeItemsList.push_back( sItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayer::GetUnitsThatCanFight( list< CPtr<CUnitServer> > *pRes ) const
{
	pRes->clear();
	for ( TPlayerUnitSet::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->CanFight() )
			pRes->push_back( i->GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayer::HasLostFromSightAliveUnits()
{ 
	if ( !HasAlivePeople() )
		return false;
	for ( TPlayerUnitSet::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->HasLostFromSightAliveUnits() )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeUpperName( const string &szName, string *pUpperName )
{
	*pUpperName = szName;
	NStr::TrimBoth( *pUpperName );
	NStr::ToUpper( *pUpperName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWorld
////////////////////////////////////////////////////////////////////////////////////////////////////
CWorld::CWorld(): 
	registerOnNewPlayerFastTurnOrTime( this, &CWorld::OnNewPlayerFastTurnOrTime )
{
}
//
const int N_TEST_HIDDEN_DELTA = 10000;
CWorld::CWorld( NRPG::CGlobalGame *_pGlobalGame ):
	registerOnNewPlayerFastTurnOrTime( this, &CWorld::OnNewPlayerFastTurnOrTime ),
	CDebrisController(), pGlobalGame( _pGlobalGame ), bForcedRealTime( false ), nTurnID( 0 ), bLeanAndMean( false )
{ 
	tPrev = 0; 
	tHiddenDelta = N_TEST_HIDDEN_DELTA;
	pTime = new CCTime( N_TEST_HIDDEN_DELTA ); 
	pAimTime = new CCTime(0);
	pShow = new CWorldSyncSrc;
	pShowUnits = new CWorldSyncSrc;
	pGlobalAck = new CGlobalAck();
	pAIJobManager = NAI::CreateAIJobManager();
	pAISignalManager = NAI::CreateAISignalManager( this );
	pOwnScript = NScript::CreateScript( this );
	pDiplomacy = NRPG::CreateGlobalDiplomacy();
	RunAutoLoadScripts();
	pMineTracker = new CMineTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RunAutoLoadScripts()
{
	CDBTable<NDb::CDBAutoLoadScript> *pTable = NDatabase::GetTable<NDb::CDBAutoLoadScript>();
	CDBIterator<NDb::CDBAutoLoadScript> i(*pTable);
	while ( pTable && i.MoveNext() )
	{
		CDBPtr<NDb::CDBAutoLoadScript> pScriptName = i.Get();
		if ( IsValid( pScriptName ) )
			pOwnScript->RunScriptFile( pScriptName->szFileName );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::GetUnit( CUnit *pUnit ) const
{
	CUnitServer *pRes = dynamic_cast<CUnitServer*>(pUnit); 
	ASSERT( pRes ); 
	return pRes; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectServerBase* CWorld::AddObject( const SObjectPlace &pos, 
	NDb::CObject *pDBObject, string szName )
{
	SMapElement mapElement;
	mapElement.pObject = pDBObject;
	mapElement.szName = szName;
	mapElement.bOpen = false;
	mapElement.nRelFloor = 0;
	mapElement.bLightmap = true;
	mapElement.ptAlignTo = CVec2( pos.ptPos.x, pos.ptPos.y );
	//
	CPtr<NRPG::IObject> pRPGObject = NRPG::CreateObject( pDBObject );
	ASSERT( IsValid( pRPGObject ) );
	if ( !IsValid( pRPGObject ) )
		return 0;
	return AddObject( pos, pRPGObject, mapElement );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectServerBase *CWorld::AddObject( const SObjectPlace &pos, 
	NRPG::IObject *pRPGObject, const SMapElement &mapElement, CPostWorldCreateInfo *pPostInfo )
{
	CPtr<CObjectServerBase> pResult = 0;
	CDBPtr<NDb::CObject> pDBObject = mapElement.pObject;
	if ( pDBObject->pDoor || pDBObject->pGun )
	{
		if ( !pDBObject->pModels[0] || !pDBObject->pModels[0]->pModel || !pDBObject->pModels[0]->pModel->pSkeleton )
		{
			ASSERT(0);
			return 0;
		}
	}
	//
	if ( pDBObject->pDoor )
	{
		CWindowDoor *pWD = new CWindowDoor( this, pos, 
			mapElement.bLightmap, pDBObject, pRPGObject, GetTime(), mapElement.flags, mapElement.bOpen );
		if ( IsValid( mapElement.pGrenade ) )
			if ( pPostInfo )
				pPostInfo->traps.push_back( SDoorTrap( pWD, mapElement.pGrenade, mapElement.nDC ) );
			else
				pWD->SetTrap( mapElement.pGrenade, mapElement.nDC );
		pResult = pWD;
		objects.push_back( pWD );
		miscObjects.push_back( pWD );
	}
	else if ( pDBObject->pGun )
	{
		CCannon *pGun = new CCannon( this, pos, mapElement.bLightmap, pDBObject, pRPGObject, GetTime(), mapElement.flags );
		pResult = pGun;
		objects.push_back( pGun );
		miscObjects.push_back( pGun );
	}
	else if ( IsValid( pDBObject->pPassage ) 
		&& IsValid( pDBObject->pModels[0] ) && IsValid( pDBObject->pModels[0]->pModel ) )
	{
		// passage objects
		CPtr<IPassageObject> pPassageObject = 0;
		NDb::CContainerModel *pCont = pDBObject->pModels[0];
		if ( IsValid( pCont->pModel->pSkeleton ) )
		{
			// anim passage object
			pPassageObject = CreateAnimPassageObject( this, pos, mapElement.bLightmap, pDBObject, pRPGObject, 
				GetTime(), mapElement.nPassageZoneID, mapElement.nPassageObjectID, mapElement.nAPRadius, mapElement.flags );
		}
		else
		{
			// "simple" passage object
			pPassageObject = CreatePassageObject( this, pos, mapElement.bLightmap, pDBObject, pRPGObject, 
					mapElement.nPassageZoneID, mapElement.nPassageObjectID, mapElement.nAPRadius, mapElement.flags );
		}
		CDynamicCast<CObjectServerBase> pPassageObjectOS(pPassageObject);
		if (pPassageObjectOS)
		{
			objects.push_back( pPassageObjectOS.GetPtr() );
			CDynamicCast<CAnimObjectServerBase> pAnimPassageObjectOS(pPassageObject);
			if (pAnimPassageObjectOS)
				miscObjects.push_back( pAnimPassageObjectOS.GetPtr() );
			if ( pPassageObjectOS->NeedSegment() )
				segmentObjects.push_back( pPassageObjectOS.GetPtr() );
			pResult = pPassageObjectOS;
		}
	}
	else
	{
		NDb::CContainerModel *pCont = pDBObject->pModels[0];
		ASSERT(pCont);
		if ( !pCont )
			return 0;
		CObjectServerBase *pBase;
		if ( pCont->pModel && pCont->pModel->pSkeleton )
			pBase = *objects.insert( objects.end(), new CAnimObjectServer( this, pos, mapElement.bLightmap, pDBObject, pRPGObject, GetTime(), mapElement.flags ) );
		else
			pBase = *objects.insert( objects.end(), new CObjectServer( this, pos, mapElement.bLightmap, pDBObject, pRPGObject, mapElement.flags, mapElement.bBorder ) );
		if ( pBase->NeedSegment() )
			segmentObjects.push_back( pBase );
		pResult = pBase;
	}
	//
	if ( IsValid( pResult ) && !mapElement.szName.empty() )
	{
		string szUpperName;
		MakeUpperName( mapElement.szName, &szUpperName );
		nameToObj[ szUpperName ] = pResult.GetBarePtr();
	}
	// retail CWorld::AddObject @0x365ee0 (LAB_0076617e): arm the object's self-detonation grenade so any LATER
	// damaging ProcessAttack (path A @0x785242, AddGrenadeExplosion via pWorld vtbl+0x124) spawns it -- this is the
	// "explodable objects explode from any damage" behaviour. Retail does AttachExplosion( mapElement.doorParams.pGrenade )
	// for EVERY object kind, and the map builder (CMapBuilder::AddSimpleElements @0x2747c0) sets that descriptor grenade
	// straight from the CREATED object's own grenade: after `this_04 = CTRndObject::CreateObject(...)` it does
	// `doorParams.pGrenade = this_04->pGrenade` (NDb::CObject::pGrenade @0x40, DB col "RPGGrenade"). So gas tanks / fuel
	// barrels carry their explosion on the OBJECT record -- verified in the Steam game.db: all 18 RPGGrenade objects have
	// container AttachedGrenade==0, and their map placements carry CFinalElement.pGrenade==0 / bArmed==false (the
	// per-placement grenade+bArmed pair is the separate door/mine booby-trap path). The recon instead sources
	// mapElement.pGrenade from the per-placement CFinalElement.pGrenade (used for those traps), so prefer it to preserve
	// trap behaviour and fall back to the object-def grenade -- reproducing retail's object-sourced arming for explodables.
	if ( IsValid( pResult ) )
	{
		if ( IsValid( mapElement.pGrenade ) )
			pResult->AttachExplosion( mapElement.pGrenade );
		else if ( IsValid( pDBObject ) && IsValid( pDBObject->pGrenade ) )
			pResult->AttachExplosion( pDBObject->pGrenade );
	}
	return pResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::AddUnit( const NAI::SPathPlace &aiPos,	NRPG::IUnitMission *_pRPG, CPlayer *pPlayer, const string &szName )
{
	CUnitServer *pUS = 0;
	NAI::SUnitPosition p;
	p.pos.SetNetwork( pPathNetwork );
	p.pos.p = aiPos;
	p.bRun = false;
	if ( !p.IsValid() )
		return 0;
	//
	pUS = AddUnit( new CUnitServer( this, _pRPG, _pRPG->GetModel(), pPlayer, p ) );
	if ( !IsValid( pUS ) )
		return 0;
	//
	if ( !pUS->IsEmptyPK() )
	{
		// register unit`s name
		string szUpperName;
		MakeUpperName( szName, &szUpperName );
		if ( szUpperName.empty() )
			MakeUpperName( pUS->GetUnitRPG()->GetRPGUnit()->GetPers()->szUserName, &szUpperName );
		if ( !szUpperName.empty() )
			nameToObj[ szUpperName ] = CastToObjectBase( pUS );
		// wear PK if unit has one
		NRPG::CUnit *pUnit = pUS->GetUnitRPG()->GetRPGUnit();
		if ( pUnit->pPanzerklein )
		{
			CPtr<CUnitServer> pPKServer = AddUnit( pUS->GetPosition().pos.p, NRPG::CreateUnit( pUnit->pPanzerklein ), 0 );
			if ( pPKServer->WearAsPK( true ) )
				pUS->FlipPanzerklein( pPKServer, false );
		}
	}
	return pUS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::AddUnit( CUnitServer *pUS )
{
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pUS ) )
		return 0;
	//
	units.push_back( pUS );
	if ( !pUS->IsEmptyPK() )
	{
		OnUnitAdded( pUS );
		pGlobalAck->AddAck( pUS );
		pUS->GetTBSPlayer()->AddUnit( pUS );
		MakeUnitActive( pUS, pUS->GetTBSPlayer() );
	}
	else
		pUS->SetState( new CUnitStateDeath( pUS ) );
	return pUS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::AddUnitInGame( const NAI::SPathPlace &aiPos, NRPG::IUnitMission *_pRPG, CPlayer *pPlayer, const string &szName )
{
	CUnitServer *pUS = AddUnit( aiPos, _pRPG, pPlayer, szName );
	pUS->PlaceOnPassablePlace();
	UpdateVisible();
	return pUS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemoveUnit( CUnitServer *pUnit )
{
	if ( !pUnit->IsEmptyPK() )
	{
		pUnit->Die( true );
		GetPathNetwork()->Unlock( pUnit );
		CPtr<CPlayer> pPlayer = pUnit->GetTBSPlayer();
		RemoveUnitFromAI( pUnit );
		if ( IsValid( pPlayer ) )
			pPlayer->RemoveUnit( pUnit );
	}
	units.remove( pUnit );
	UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddBuilding( const SMapBuilding &info )
{
	CBuilding *pBuilding = new CBuilding( pShow, info, this );
	buildings.push_back( pBuilding );
	miscObjects.push_back( pBuilding );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AttachMiscObject( CTimedObject *p )
{
	p->Attach( pShow, this );
	miscObjects.push_back( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateParticle( const CVec3 &ptPos, const CQuat &rot, NDb::CEffect *pEffect, int nFloor )
{
  CFBMatrixStack<4> m;
  m.Init();
	m.Push( ptPos, rot );
	NGScene::CCFBTransform *pPlace = new NGScene::CCFBTransform( m.Get() );
	AttachMiscObject( CreateDParticles( pPlace, pEffect, nFloor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddHitLocator( CHitLocator* pLocator )
{
	eventHits.push_back( pLocator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddUICommand( CUICmd *pCmd )
{
	uiCmdsList.push_back( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateFakeTerrainInfo( STerrainInfo *pTerrain )
{
	STerrainInfo &terrain = *pTerrain;
	
	terrain.nWidth = 128;
	terrain.nHeight = 128;
	terrain.typeMap.SetSizes( terrain.nWidth + 1, terrain.nHeight + 1 );
	terrain.heightMap.SetSizes( terrain.nWidth + 1, terrain.nHeight + 1 );
	terrain.color.SetSizes( terrain.nWidth + 1, terrain.nHeight + 1 );
	terrain.color.FillEvery( 0xffffffff );

	CDBTable<NDb::CTerrainTile> *pTileTable = NDatabase::GetTable<NDb::CTerrainTile>();
	CDBIterator<NDb::CTerrainTile> iTempTile( *pTileTable );
	if ( iTempTile.MoveNext() )
		terrain.typeMap.FillEvery( iTempTile.Get()->GetRecordID() );
	else
	{
		ASSERT(0 && "Hudozhniki suki ubili vse tiles" );
		terrain.typeMap.FillZero();
	}

	terrain.heightMap.FillZero();
/*	
	for ( int nTempY = 0; nTempY <= terrain.nHeight; nTempY++ )
	{
		for ( int nTempX = 0; nTempX <= terrain.nWidth; nTempX++ )
		{
#define PARAM( ValX, ValY ) FP_4PI * ( (float)ValX / terrain.nWidth + (float)ValY / terrain.nWidth ) + FP_4PI * cos ( ( (float)ValX / terrain.nWidth ) * ( (float)ValX / terrain.nWidth ) ) + sin ( ( (float)ValY / terrain.nHeight ) * ( (float)ValY / terrain.nHeight ) )
			terrain.heightMap[nTempY][nTempX] = 4 * ( 32 * sin( PARAM( nTempX, nTempY )  ) + 16 * sin( 2.0f * PARAM( nTempX, nTempY ) ) + 8 * sin( 4.0f * PARAM( nTempX, nTempY ) ) );
			
			terrain.typeMap[nTempY][nTempX] = 6;
			if ( terrain.heightMap[nTempY][nTempX] > 32 )
				terrain.typeMap[nTempY][nTempX] = 8;
		}
	}
	
	/*STerrainHole tHole;
	tHole.bVisible = true;
	tHole.nHeight = 20;
	tHole.vPolygon.push_back( CVec2( 10 + 0, 10 + 0 ) );
	tHole.vPolygon.push_back( CVec2( 10 + 3, 10 + 0 ) );
	tHole.vPolygon.push_back( CVec2( 10 + 8, 10 + 5 ) );
	tHole.vPolygon.push_back( CVec2( 10 + 5, 10 + 5 ) );
	tHole.vPolygon.push_back( CVec2( 10 + 5, 10 + 8 ) );
	tHole.vPolygon.push_back( CVec2( 10 + 0, 10 + 3 ) );
	terrain.holes.push_back( tHole );
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCompareInterruptStrength
{
	bool operator()( const SInterruptInfo::SNotice &a, const SInterruptInfo::SNotice &b ) const
	{
		return a.fStrength < b.fStrength;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CheckInterrupt( SInterruptInfo *info )
{
	if ( IsForcedRealTime() )
		return;
	if ( info->events.empty() )
		return;
	// detect if interrupts are mutual or not
	for ( list<SInterruptInfo::SNotice>::iterator i = info->events.begin(); i != info->events.end(); ++i )
	{
		SInterruptInfo::SNotice &n = *i;
		for ( list<SInterruptInfo::SNotice>::iterator k = i; k != info->events.end(); ++k )
		{
			if ( k->pWho == n.pWhom && k->pWhom == n.pWho )
			{
				k->bIsMutual = true;
				n.bIsMutual = true;
				break;
			}
		}
	}
	CPlayer *pWhoseTurn = GetTBSCurrentPlayer();
	// determine interrupts strength
	CPtr<IPlayer> pWhoPlayer = info->events.front().pWho->GetPlayer(); // �������� ����������
	list< CPtr<CUnitServer> > unitsToCancelAction; // �������� ����������
	bool bCanCancel = ( IsRealTime() || pWhoPlayer.GetPtr() == pWhoseTurn )
			&& !CDynamicCast<NAI::CAICommander>( pWhoPlayer->GetCommander() ); // �������� ����������
	for ( list<SInterruptInfo::SNotice>::iterator i = info->events.begin(); i != info->events.end(); )
	{
		if ( bCanCancel && !i->bWasShot && !i->bIsMutual && i->pWho->GetPlayer() == pWhoPlayer )
		{
			unitsToCancelAction.push_back( i->pWho );
			i = info->events.erase( i );
		}
		else
		{
			int nInteruptStr = i->pWho->GetUnitRPG()->CheckInterrupt( i->pWhom->GetUnitRPG(), i->bIsMutual, i->bWasShot );
			if ( nInteruptStr < 0 || i->pWho->WasInterrupted( i->pWhom ) || !i->pWho->CanSpendAP( i->pWho->GetActionAP( NRPG::AC_MOVE_SIDE ) ) )
				i = info->events.erase( i );
			else
			{
				i->fStrength = nInteruptStr;
				++i;
			}
		}
	}
	if ( info->events.empty() )
	{
		if ( !unitsToCancelAction.empty() )
		{
			// �������� ����������
			csSystem << CC_RED << "No interrupt, all commands were canceled" << endl;
			for ( list< CPtr<CUnitServer> >::iterator i = unitsToCancelAction.begin(); i != unitsToCancelAction.end(); ++i )
				(*i)->OnTBSEvent( TBS_CANCEL_ACTION );
			//	(*i)->Do( new NWorld::CCmdCancel( *i ) );
		}
		// �������� ���������� �� ����
		vector<CPtr<CPlayer> > players;
		GetPlayersList( &players );
		for ( int k = 0; k < players.size(); ++k )
		{
			CPlayer *pPlayer = players[k];
			if ( pPlayer->CanSeeNewTraps() )
				pPlayer->OnTBSEvent( TBS_CANCEL_ACTION );
		}
		return;
	}
	// sort them out
	info->events.sort( SCompareInterruptStrength() );
	list<CUnitServer*> res;
	CUnitServer *pWhom = 0;
	CUnitServer *pWho = 0;
	CPlayer *pTestPlayer = info->events.back().pWho->GetTBSPlayer();
	while ( !info->events.empty() )
	{
		const SInterruptInfo::SNotice &e = info->events.back();
		pWho = e.pWho;
		if ( pWho->GetTBSPlayer() != pTestPlayer )
			break;
		if ( find( res.begin(), res.end(), pWho ) == res.end() )
			res.push_back( pWho );
		if ( !pWhom )
			pWhom = e.pWhom;
		pWho->MarkInterrupted( e.pWhom );
		info->events.pop_back();
		AttachMiscObject( Create3DSound( pWho->GetPosition().GetEyePosition(), NDb::GetSound(11) ) ); // CRAP multiple sounds in one place, direct ID specified
	}
	//
	if ( IsValid( pWho ) )
		GetGlobalAck()->OnInterrupt( pWho );
	AddInterrupt( res );
	if ( pWhom )
		AddUICommand( new CUICmdUnit( pWhom ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::MergeFriendlyPlayersVisibleSets()
{
	vector<CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for (;;)
	{
		bool bWasMerge = false;
		for ( int k = 0; k < players.size(); ++k )
		{
			int nKID = players[k]->GetScenarioPlayerID();
			for ( int m = 0; m < players.size(); ++m )
			{
				// players[m] - target
				if ( CDynamicCast<NAI::CAICommander>( players[m]->GetCommander() ) )
					continue;
				int nMID = players[m]->GetScenarioPlayerID();
				if ( pDiplomacy->GetDiplomacyState( nKID, nMID ) == NDb::DS_ALLY )
					bWasMerge |= players[m]->MergeVisibility( players[k] );
			}
		}
		if ( !bWasMerge )
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::UpdateVisible()
{
	SInterruptInfo info;
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
		(*i)->UpdateVisible( &info );
	vector<CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( int k = 0; k < players.size(); ++k )
		players[k]->UpdateVisible();
	MergeFriendlyPlayersVisibleSets();
	CheckInterrupt( &info );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ProcessAISignals()
{
	vector<CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( int k = 0; k < players.size(); ++k )
		players[k]->GetCommander()->ProcessAISignals();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnUnitAdded( CUnitServer *pUnit ) 
{
	vector<CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( int k = 0; k < players.size(); ++k )
		players[k]->GetCommander()->OnUnitAdded( pUnit );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetAllUnits( vector< CPtr<NWorld::CUnit> > *pUnits )
{
	pUnits->clear();
	for ( list<CObj<CUnitServer> >::const_iterator i = units.begin(); i != units.end(); ++i )
		pUnits->push_back( i->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetAllUnits( list<CPtr<CUnitServer> > *pRes )
{
	pRes->clear();
	for ( list<CObj<CUnitServer> >::const_iterator k = units.begin(); k != units.end(); ++k )
		pRes->push_back( k->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetUnitsNear( const CVec3 &pos, list<CPtr<CUnitServer> > *pRes, float fRadius )
{
	pRes->clear();
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		CVec3 ptDest = (*i)->GetPosition().GetCenter();
		if ( fabs( ptDest - pos ) <  fRadius )
			pRes->push_back( (*i).GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetCannons( vector<CCannon*> *pRes )
{
	pRes->clear();
	for ( list< CObj<CObjectServerBase> >::const_iterator i = objects.begin(); i != objects.end(); ++i )
	{
		CCannon *pCannon = dynamic_cast<CCannon*>( (*i).GetPtr() );
		if ( pCannon != 0 )
			pRes->push_back( pCannon );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddMine( IMine *pMine )
{
	if ( find( trappedObjects.begin(), trappedObjects.end(), pMine ) != trappedObjects.end() )
		return;
	trappedObjects.push_back( pMine );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemoveMine( IMine *pMine )
{
	trappedObjects.remove( pMine );
	GlobalSituationHasChanged();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetMinesNear( const CVec3 &pos, list<CPtr<IMine> > *pRes, float fRadius )
{
	pRes->clear();
	for ( list< CPtr<IMine> >::iterator i = trappedObjects.begin(); i != trappedObjects.end(); )
	{
		IMine *pMine = *i;
		if ( IsValid(pMine) )
		{
			if ( pMine->IsMineSet() && fabs2( pos - pMine->GetMinePos() ) < sqr(fRadius) )
				pRes->push_back( pMine );
			++i;
		}
		else
			i = trappedObjects.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ClickOfDeath( const CRay &ray, int nMaxFloor )
{
	CObjectBase *pUserData;
	int nUserID;
	CVec3 ptPoint;
	if ( TraceRay( this, ray, nMaxFloor, &pUserData, &nUserID, &ptPoint ) )
		miscObjects.push_back( CreateClickOfDeath( GetActiveCounter(), pUserData, nUserID, ray ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit* CWorld::GetUnit( const NAI::SUnitPosition &pos )
{
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetPosition().pos == pos.pos && !(*i)->IsDead() && !(*i)->IsUnconscious() )
			return (*i);
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit* CWorld::GetUnitInTile( const NAI::SUnitPosition &pos )
{
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !(*i)->IsUnconscious() && !(*i)->IsDead() && ( (*i)->GetPosition().GetCP() == pos.GetCP() ) )
			return (*i);
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTRect<float>& CWorld::GetMapSafeZone() const
{
	return sMapSafeZone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::IsPersonSlotUsed( int nUnitID, 
	const ClueToSlot &clueToSlot, int *pPersID, string *pClueName )
{
	for ( ClueToSlot::const_iterator i = clueToSlot.begin(); i != clueToSlot.end(); ++i )
		if ( i->second.nUnitID == nUnitID )
		{
			CDBPtr<NDb::CDBScenarioClue> pClue = i->first->GetDBClue();
			*pPersID = pClue->nPersID;
			*pClueName = pClue->sSmallDescription;
			return true;
		}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::DistributeClues( const SMapInfo &mapInfo,
	const list< CPtr<NScenario::CScenarioClue> > &clues,
	ClueToSlot *personClueToSlot,	ClueToSlot *itemClueToSlot )
{
	ASSERT( personClueToSlot != 0 );
	ASSERT( itemClueToSlot != 0 );
	//
	personClueToSlot->clear();
	itemClueToSlot->clear();
	//
	vector<SClueSlot> personSlots;
	vector<SClueSlot> itemSlots;
	// ��������� person clues
	for ( vector<SClueSlot>::const_iterator slot = mapInfo.slots.begin();
		slot != mapInfo.slots.end(); ++slot )
		if ( slot->bPersSlot )
			personSlots.push_back( *slot );
	int nPersonSlots = personSlots.size();
	//
	for ( list< CPtr<NScenario::CScenarioClue> >::const_iterator clue = clues.begin();
		clue != clues.end(); ++clue )
			if ( !personSlots.empty() && (*clue)->GetDBClue()->clueType == NDb::CT_PERSON &&
				(*clue)->GetDBClue()->nPersID > 0 )
			{
				int n = random.Get( 0, personSlots.size() );
				SClueSlot &slot = personSlots[n];
				(*personClueToSlot)[ *clue ] = slot;
				if ( slot.bInventorySlot )
					itemSlots.push_back( slot );
				personSlots.erase( personSlots.begin() + n );
			}
	// ��������� item clues
	for ( vector<SClueSlot>::const_iterator slot = mapInfo.slots.begin();
		slot != mapInfo.slots.end(); ++slot )
		if ( !slot->bPersSlot )
			itemSlots.push_back( *slot );
	int nItemSlots = itemSlots.size();
	//
	for ( list< CPtr<NScenario::CScenarioClue> >::const_iterator clue = clues.begin();
		clue != clues.end(); ++clue )
			if ( !itemSlots.empty() && (*clue)->GetDBClue()->clueType == NDb::CT_ITEM && 
				(*clue)->GetDBClue()->nItemID > 0 )
			{
				int n = random.Get( 0, itemSlots.size() );
				(*itemClueToSlot)[ *clue ] = itemSlots[ n ];
				itemSlots.erase( itemSlots.begin() + n );
			}
	//
	char szStr[128];
	sprintf( szStr, "[SCENARIO TRACKER] %d person slots, %d item slots\n", nPersonSlots, nItemSlots );
	OutputDebugString( szStr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::PlaceItemSlotsToMap( const ClueToSlot &clueToSlot )
{
	for ( ClueToSlot::const_iterator i = clueToSlot.begin();
		i != clueToSlot.end(); ++i )
	{
		if ( !i->second.bInventorySlot )
		{
			CPtr<NRPG::IInventoryItem> pItem = NRPG::CreateClueItem( NDb::GetRPGItem( i->first->GetDBClue()->nItemID ) );
			if ( IsValid( pItem ) )
			{
				// �������� �� �����
				CQuat rot( ToRadian( i->second.pos.fRotation ), CVec3(0,0,1) );
				CDFrozenItem *pFrozenItem = AddFrozenItem( i->second.pos.ptPos, rot, pItem, i->second.pos.nFloor );
				if ( IsValid( pFrozenItem ) )
				{
					string szUpperName;
					MakeUpperName( i->first->GetDBClue()->sSmallDescription, &szUpperName );
					nameToObj[szUpperName] = CastToObjectBase( pFrozenItem );
					DebugTrace( "[SCENARIO TRACKER] item %d was placed on the map\n", i->first->GetDBClue()->nItemID );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::PlaceItemSlotsToInventory( const ClueToSlot &clueToSlot )
{
	char szStr[128];
	//
	for ( ClueToSlot::const_iterator i = clueToSlot.begin();
		i != clueToSlot.end(); ++i )
	{
		if ( i->second.bInventorySlot )
		{
			CPtr<NRPG::IInventoryItem> pItem = NRPG::CreateItem( NDb::GetRPGItem( i->first->GetDBClue()->nItemID )->pSuccessor );
			if ( IsValid( pItem ) )
			{
				// �������� � inventory
				CPtr<NWorld::CUnitServer> pUnitServer = GetUnitServerByPersID( i->second.pPers->nRPGPersID );
				if ( IsValid( pUnitServer ) )
				{
					CPtr<NRPG::IInventory> pInventory = pUnitServer->GetUnitRPG()->GetInventory();
					CTPoint<int> position;
					pInventory->FindPlace( pItem, &position );
					pInventory->Place( position, pItem );
					sprintf( szStr, "[SCENARIO TRACKER] item %d was placed in an inventory\n", i->first->GetDBClue()->nItemID );
					OutputDebugString( szStr );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddWaypoint( CMapWaypoint *pWaypoint )
{
	string szLowerName = pWaypoint->pName->szName;
	NStr::ToLower( szLowerName );
	ASSERT( !IsValid( waypoints[ szLowerName ] ) ); // ��� waypoint-� � ����� ������
	if ( !IsValid( waypoints[ szLowerName ] ) )
	{
		waypoints[ szLowerName ] = new NAI::CAIRouteWaypoint( GetPathNetwork(), pWaypoint );
	}
	else
	{
		NScript::ScriptWarning( string( "There are more then one waypoint with name " ) + string( pWaypoint->pName->szName ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::LoadWaypoints( const list< CObj<CMapWaypoint> > &_waypoints )
{
	for ( list< CObj<CMapWaypoint> >::const_iterator 
		i = _waypoints.begin(); i != _waypoints.end(); ++i )
	{
		if ( (*i)->bExists )
			AddWaypoint( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddAIPlayer( const wstring &wsName, int nScenarioPlayerID )
{
	CPlayer *pPlayer = new CPlayer( L"AI Player", pGlobalGame, 0, nScenarioPlayerID );
	RegisterPlayer( pPlayer );
	CPtr<NAI::CAICommander> pAICommander = new NAI::CAICommander( this, pPlayer );
	pPlayer->SetCommander( pAICommander );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateAIUnits( const SMapInfo &mapInfo, const ClueToSlot &personClueToSlot, 
	int nMobsLevel, unordered_map< int, CPtr<CUnitServer> > *pIDToUnit, CVec3 ptDeltaPos )
{
	//return; // uncomment this if you wanna have real "noai"! without any interrupts, turnbased mode etc.
	nAIUnitsCreated = 0;
	//
	vector<SMapUnit> unitsToCreate;
	// ������� ������ unit-�� ������� ���� �������
	for ( list<SMapUnit>::const_iterator i = mapInfo.units.begin(); i != mapInfo.units.end(); ++i )
	{
		int nPersID;
		string szClueName;
		if ( !i->bSlot || ( i->bSlot && IsPersonSlotUsed( i->nUnitID, personClueToSlot, &nPersID, &szClueName ) ) )
		{
			SMapUnit unit = *i;
			NStr::TrimBoth( unit.szName );
			if ( i->bSlot )
			{
				unit.pPers = NDb::GetPers( nPersID );
				if ( unit.szName.empty() )
					unit.szName = szClueName;
			}
			//
			if ( unit.nDiplomacy < 0 )
				unit.nDiplomacy = pDiplomacy->GetPlayerDiplomacy( i->nScenarioPlayer ).GetDiplomacy();
			//
			ASSERT( IsValid( unit.pPers ) );
			if ( IsValid( unit.pPers ) )
			{
				unit.pos.ptPos = unit.pos.ptPos + ptDeltaPos;
				unitsToCreate.push_back( unit );
			}
		}
	}
	// ������� AI Player-��
	for ( vector<SMapUnit>::const_iterator i = unitsToCreate.begin(); i != unitsToCreate.end(); ++i )
	{
		if ( !IsValid( GetPlayerByID( i->nScenarioPlayer ) ) )
		{
			AddAIPlayer( L"AI player", i->nScenarioPlayer );
			csSystem << "AI Player number " << CC_RED << i->nScenarioPlayer << CC_WHITE << " was created" << endl; // DEBUG
			CDynamicCast<NAI::CAICommander> pAICommander( GetPlayerByID( i->nScenarioPlayer )->GetCommander() );
			if ( pAICommander )
				UpdateAICommander( pAICommander );
		}
	}
	// ������� AI Unit-��
	for ( int nUnitToCreate = 0; nUnitToCreate < unitsToCreate.size(); ++nUnitToCreate )
	{
		SMapUnit *i = &unitsToCreate[ nUnitToCreate ];
		CPtr<NRPG::IUnitMission> pRPG = NRPG::CreateUnit( i->pPers );
		int nLevel = Max( 0, nMobsLevel + i->nRelativeLevel + pGlobalGame->pDifficulty->nAIUnitsLevel );
		if ( !IsValid( i->pPers->pPanzerklein ) )
		{
			pRPG->GetRPGUnit()->Skills(NDb::ST_VP).Multiply( pGlobalGame->pDifficulty->fVPCoeff );
			pRPG->GetRPGUnit()->Skills(NDb::ST_AP).Multiply( pGlobalGame->pDifficulty->fAPCoeff );
			pRPG->GetRPGUnit()->SetXPLevel( nLevel );
			pRPG->SetDiplomacy( i->nDiplomacy );
		}
		//
		NAI::SPosition pos;
		int nFloor = i->pos.nFloor;
		pPathNetwork->SetOnFloor( &pos, nFloor, i->pos.ptPos );
		pos.p.SetDirection( pPathNetwork->GetClosestDir( pos.p.GetLayer(), ToRadian( i->pos.fRotation ) ) );
		pos.p.SetPose( ( unsigned short )i->eInitialPose );
		CPtr<CPlayer> pPlayer = GetPlayerByID( i->nScenarioPlayer );
		//
		ASSERT( IsValid( pPlayer ) );
		if ( !IsValid( pPlayer ) )
			continue;
		//
		CPtr<CUnitServer> pUnitServer = AddUnit( pos.p, pRPG, pPlayer, i->szName );
		(*pIDToUnit)[ i->nUnitID ] = pUnitServer;

		ASSERT( IsValid( pUnitServer ) );
		// ������� route-�
		if ( IsValid( pUnitServer ) && !pUnitServer->IsEmptyPK() )
		{
			CDynamicCast<NAI::CAICommander> pAICommander( pPlayer->GetCommander() );
			ASSERT( IsValid( pAICommander ) );
			pAICommander->GetAITaskCommander()->CreateRoute( pUnitServer, *i );
		}
		++nAIUnitsCreated;
	}
	PlaceAllUnits();
	UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::UpdateAICommander( NAI::CAICommander *pAICommander )
{
	ASSERT( IsValid( pAICommander ) );
	if ( !IsValid( pAICommander ) )
		return;
	//
	for ( list< CObj<CUnitServer> >::iterator u = units.begin(); u != units.end(); ++u )
		if ( (*u)->CanFight() )
			pAICommander->OnUnitAdded( u->GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateUnitGroups( const SMapInfo &mapInfo, 
	unordered_map< int, CPtr<CUnitServer> > *pIDToUnit )
{
	for ( unordered_map< int, SUnitGroup >::const_iterator 
		i = mapInfo.groups.begin(); i != mapInfo.groups.end(); ++i )
	{
		CPtr<CUnitGroup> pUnitGroup = GetUnitGroup( i->first );
		if ( !IsValid( pUnitGroup ) )
			pUnitGroup = CreateUnitGroup( i->first );
		ASSERT( pUnitGroup->GetID() == i->first );
		for ( vector<int>::const_iterator u = i->second.units.begin(); u != i->second.units.end(); ++u )
			pUnitGroup->units.Add( (*pIDToUnit)[ *u ] );
		//
		CObj<NAI::CAIRoute> pRoute = new NAI::CAIRoute( this, i->second.route );
		if ( !pRoute->IsEmpty() )
			NAI::SetGroupRoute( pUnitGroup, pRoute, true, NAI::AIM_AI );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateObjects( const SMapInfo &mapInfo, CPostWorldCreateInfo *pPostInfo, CVec3 ptDeltaPos, bool bCreateBorder	)
{
	for ( list<SMapElement>::const_iterator i = mapInfo.items.begin(); i != mapInfo.items.end(); ++i )
	{
		if ( bCreateBorder || !i->bBorder  )
		{
			CPtr<NRPG::IObject> pRPGObject = NRPG::CreateObject( i->pObject, i->nObjectPhase );
			if ( !IsValid( pRPGObject ) )
				continue;
			SObjectPlace place;
			CalcTransform( &place, i->pos );
			place.ptPos = place.ptPos + ptDeltaPos;
			AddObject( place, pRPGObject, *i, pPostInfo );
		}
	}
	for ( list<SMapRPGElement>::const_iterator i = mapInfo.rpgitems.begin(); i != mapInfo.rpgitems.end(); ++i )
	{
		if ( IsValid( i->pItem ) && IsValid( i->pItem->pSuccessor ) )
		{
			CDynamicCast<NDb::CRPGMine> pM(i->pItem->pSuccessor);
			if (pM)
			{
				if ( i->bArmed )
				{
					CPtr<CMine> pMine = new CMine( this, i->pos.ptPos, pM, i->nDC, i->pos.nFloor );
					continue;
				}
			}
			CQuat rot( ToRadian( i->pos.fRotation ), CVec3(0,0,1) );
			CVec3 ptPos = i->pos.ptPos + ptDeltaPos;
			CDFrozenItem *pFrozenItem = AddFrozenItem( ptPos, rot, NRPG::CreateItem( i->pItem->pSuccessor ), i->pos.nFloor );
			if ( IsValid( pFrozenItem ) )
			{
				string szUpperName;
				MakeUpperName( i->szName, &szUpperName );
				nameToObj[szUpperName] = CastToObjectBase( pFrozenItem );
			}
		}
		else
			ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::PlaceTemplate( int nTemplateID, CVec3 ptPos )
{
	CPtr<NDb::CTemplate> pTemplate = NDb::GetTemplate( nTemplateID );
	if ( !IsValid( pTemplate ) )
		return false;
	//
	int nVariantID = NDb::GetTemplVariant( pTemplate, vector<int>(), -1, &SRand() )->GetRecordID();
	//
	SMapInfo mapInfo;
	if ( !BuildMap( nVariantID, vector<string>(), GetPathNetwork(), &mapInfo ) )
		return false;
	//
	CreateObjects( mapInfo, 0, ptPos, false );
	//
	for ( list< CObj<CMapWaypoint> >::iterator i = mapInfo.waypoints.begin(); i != mapInfo.waypoints.end(); ++i )
	{
		(*i)->pos.ptPos = (*i)->pos.ptPos + ptPos;
		AddWaypoint( *i );
	}
	//
	unordered_map< int, CPtr<NWorld::CUnitServer> > idToUnit;
	int nLevel = GetGlobalGame()->pDifficulty->nAIUnitsLevel;
	CreateAIUnits( mapInfo, ClueToSlot(), nLevel, &idToUnit, ptPos );
	CreateUnitGroups( mapInfo, &idToUnit );
	//
	if ( IsValid( pScript ) )
	{
		for ( list<CDBPtr<NDb::CScript> >::const_iterator i = mapInfo.scripts.begin(); i != mapInfo.scripts.end(); ++i )
			pScript->DoString( (*i)->strCode.c_str() );
	}
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateRandom( int nVariantID, const vector<string> &params,
	bool bBuildingStability, const list< CPtr<NScenario::CScenarioClue> > &clues, 
	int nMobsLevel, CObj<CPostWorldCreateInfo> *pPostInfo, SRandomSeed sSeed, bool _bLeanAndMean )
{
	CFWContext world( &pCurrentWorld, this );
	CDGPtr<CFuncBase<STime> > pRefreshTime(pTime);
	pRefreshTime.Refresh();
	//
	bLeanAndMean = _bLeanAndMean;
	pAIMap = NAI::CreateAIMap( this );
	pPathNetwork = NAI::CreateNodesNetwork( pAIMap, pAIJobManager );
	pRPGGame = NRPG::CreateGame( pAIMap, pPathNetwork );
	ConvertFlags( &createFlags, params );

	SMapInfo mapInfo;
	if ( !BuildMap( nVariantID, params, pPathNetwork, &mapInfo, -1, sSeed ) )
	{
		CreateDefault();
		return;
	}
	pDiplomacy->LoadDiplomacy( nVariantID );
	nRootLayersGroup = 0;
	pDefaultLight = mapInfo.pDefaultLight;
	sMapSafeZone = mapInfo.sMapSafeZone;

	if ( mapInfo.bShowTerrain )
	{
		pTerrainInfo = new CTerrainInfoHolder( mapInfo.terrain );
		pTerrain = new NWorld::CTerrain( pShow, pTerrainInfo, pAIMap, 
			pTime, mapInfo.nBaseTerrainFloor, mapInfo.holesList, mapInfo.wallsList );
	}
	//
	if ( pPostInfo )
	{
		*pPostInfo = new CPostWorldCreateInfo;
		(*pPostInfo)->scripts = mapInfo.scripts;
	}
	//
	CreateObjects( mapInfo, pPostInfo ? *pPostInfo : 0 );
	//
	vector<SMapBuilding>::const_iterator ib;
	for ( ib = mapInfo.buildings.begin(); ib != mapInfo.buildings.end(); ++ib )
	{
		if ( !bBuildingStability )
			ib->pGrid->ToggleStability();
		AddBuilding( *ib );
	}

	if ( !bLeanAndMean )
	{
		ClueToSlot personClueToSlot, itemClueToSlot;
		DistributeClues( mapInfo, clues, &personClueToSlot, &itemClueToSlot );
		PlaceItemSlotsToMap( itemClueToSlot );
		//CreateFakeTerrainInfo( &terrain );
		//
		// ����� ����� ������� �� ������� �������, �������� �� ������������
		pAIMap->Sync();
		// deploy units
		// put some enemies
		//
		vector<NAI::SPathPlace> empty;
		pPathNetwork->UpdateColouring( empty );
		LoadWaypoints( mapInfo.waypoints );
		// �� ����� ������� �� ������� ������
		pDeployedDeadUnitsPlayer = new CPlayer( L"Deployed dead units fake player", pGlobalGame, 0, -1 );
		pDeployedDeadUnitsPlayer->SetCommander( new NWorld::CCommander );
		//
		unordered_map< int, CPtr<CUnitServer> > idToUnit;
		CreateAIUnits( mapInfo, personClueToSlot, nMobsLevel, &idToUnit );
		CreateUnitGroups( mapInfo, &idToUnit );
		PlaceItemSlotsToInventory( itemClueToSlot );
		//
		nPartiesAdded = 0;
		// copy deploy spots
		for ( int k = 0; k < mapInfo.deploySpots.size(); ++k )
		{
			SDeploySpot &s = mapInfo.deploySpots[k];
			vector<NAI::SPathPlace> res;
			float fR = 0.63f;
			while ( res.empty() && fR < 5 )
			{
				SSphere t( s.pos.ptPos, fR );
				pPathNetwork->GetNearPlaces( t, &res );
				fR *=2;
			}
			if ( res.empty() )
			{
				ASSERT( 0 && "no path network near deploy spot" );
				continue;
			}
			NAI::SPathPlace p = res[0];
			int nMinFloor = pPathNetwork->GetFloor( p.GetLayer() );
			for ( int i = 0; i < res.size(); ++i )
			{
				if ( pPathNetwork->GetFloor( res[i].GetLayer() ) < nMinFloor )
				{
					nMinFloor = pPathNetwork->GetFloor( res[i].GetLayer() );
					p = res[i];
				}
			}
			char buf[128];
			sprintf( buf, "Deploy spot at floor %d\n", nMinFloor );
			OutputDebugString( buf );
			p.SetDirection( pPathNetwork->GetClosestDir( p.GetLayer(), ToRadian( s.pos.fRotation ) ) );
			p.SetPose( NAI::CM_STAND );
			deploySpots.push_back( SWorldDeploySpot( p, 0, k ) );
		}
		//
		CheckStability();
	}

	StartGame();
	StartFirstSegments();   // retail warm-up + freeze-driven start control (c_StartGameEx / c_DelayGameStartEx).
	                        // Fresh start only -- NOT on CreateRestored (a loaded game is already mid-state).
	if ( IsValid( pOwnScript ) )
	{
		pScript = pOwnScript;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RunPostInit( CPostWorldCreateInfo *pPostInfo )
{
	if ( !pPostInfo )
	{
		ASSERT(0);
		return;
	}
	// retail RunPostInit: re-arm the OnEnterZone one-shot for this freshly (re)loaded scenario/zone.
	bFirstSegment = true;
	if ( IsValid( pScript ) )
	{
		for ( list<CDBPtr<NDb::CScript> >::const_iterator is = pPostInfo->scripts.begin(); is != pPostInfo->scripts.end(); ++is )
		{
			int nRet = pScript->DoString( (*is)->strCode.c_str() );
		}
	}
	for ( list<SDoorTrap>::const_iterator it = pPostInfo->traps.begin(); it != pPostInfo->traps.end(); ++it )
	{
		if ( IsValid( it->pDoor ) )
			it->pDoor->SetTrap( it->pGrenade, it->nDC );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateDefault()
{
	CFWContext world( &pCurrentWorld, this );
	CDGPtr<CFuncBase<STime> > pRefreshTime(pTime);
	pRefreshTime.Refresh();
	//
	pAIMap = NAI::CreateAIMap( this );
	pPathNetwork = NAI::CreateNodesNetwork( pAIMap, pAIJobManager );
	pRPGGame = NRPG::CreateGame( pAIMap, pPathNetwork );
	
	nRootLayersGroup = pPathNetwork->CreateLayersGroup( 16, 16, CVec2(0,0), 0, 0 );

	STerrainInfo sInfo;
	CreateFakeTerrainInfo( &sInfo );
	pTerrainInfo = new CTerrainInfoHolder( sInfo );
	list<SMapHole> holes;
	list<SMapWall> walls;
	pTerrain = new NWorld::CTerrain( pShow, pTerrainInfo, pAIMap, pTime, nRootLayersGroup, holes, walls );

	pAIMap->Sync();

	vector<NAI::SPathPlace> empty;
	pPathNetwork->UpdateColouring( empty );

	StartGame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateRestored()
{
	nPartiesAdded = 0;

	StartGame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool GetDeployWithNumber( int nPlayer, int nID, const vector<CWorld::SWorldDeploySpot> &deploySpots, 
	NAI::IPathNetwork *pPathNetwork, NAI::SPathPlace *pRes )
{
	for ( int i = 0; i < deploySpots.size(); ++i )
	{
		if ( deploySpots[i].nID == nID && deploySpots[i].nPlayer == nPlayer )
		{
			*pRes = deploySpots[i].p;
			return true;
		}
	}
	CVec3 ptCenter = CVec3( 1 * FP_GRID_STEP, 8 * FP_GRID_STEP, 0 );
	NAI::SPosition pp;
	pPathNetwork->SetOnLayer( &pp, 0, ptCenter );
	*pRes = pp.p;
	pRes->SetDirection( 0 );
	pRes->SetPose( NAI::CM_STAND );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IPlayer* CWorld::AddPlayer( const wstring &wsName, NRPG::CGlobalPlayer *pGlobalPlayer, CCommander *_pCommander,
	bool bAddOnManyDeploySpots )
{
	CFWContext world( &pCurrentWorld, this );
  //  
	CPlayer *pRes = new CPlayer( wsName, pGlobalGame, pGlobalPlayer, 0 );
	csSystem << "User player was created" << endl; // DEBUG
	RegisterPlayer( pRes );
	pRes->SetCommander( _pCommander );

	if ( nRootLayersGroup < 0 )
		return pRes;
	if ( bLeanAndMean )
		return pRes;
	//
	list< CPtr<IPassageObject> > passageObjects;
	GetPassageObjects( pGlobalPlayer->deployData.nPassageZoneID, &passageObjects );
	//
	bool bSetDeploySpot = true;
	NAI::SPathPlace fakePlace; // set something for case when player has no personages
	GetDeployWithNumber( 100, -1, deploySpots, pPathNetwork, &fakePlace );
	pRes->SetDeploySpot( fakePlace );

	int nPers = pGlobalPlayer->mercs.size();
  for ( int i = 0; i < nPers; i++ )
	{
		if ( pGlobalPlayer->mercs[i]->IsDead() )
			continue;
		//
		int nShift;
		NAI::SPathPlace placeForUnit;
		if ( bAddOnManyDeploySpots )
		{
			GetDeployWithNumber( nPartiesAdded, i + 1, deploySpots, pPathNetwork, &placeForUnit );
			nShift = 0;
		}
		else
		{
			GetDeployWithNumber( nPartiesAdded, 0, deploySpots, pPathNetwork, &placeForUnit );
			nShift = i - nPers / 2;
		}
		placeForUnit = pPathNetwork->GetDeployPlace( placeForUnit, nShift );

		if ( pGlobalPlayer->deployData.bPassage )
		{
			// �������
			CPtr<IPassageObject> pPassageObject = 0;
			for ( list< CPtr<IPassageObject> >::iterator j = passageObjects.begin(); j != passageObjects.end(); ++j )
			{
				if ( (*j)->GetPassageObjectID() == 
					pGlobalPlayer->deployData.unitsDeployData[ pGlobalPlayer->mercs[i].GetPtr() ].nPassageObjectID )
				{
					pPassageObject = *j;
					break;
				}
			}
			//
			ASSERT( IsValid( pPassageObject ) );
			if ( !IsValid( pPassageObject ) )
				continue;

			vector<NAI::SPathPlace> approaches;
			pPassageObject->GetObjectApproaches( &approaches );
			if ( !approaches.empty() )
				placeForUnit = approaches[0];
		}
		if ( bSetDeploySpot )
		{
			pRes->SetDeploySpot( placeForUnit );
			bSetDeploySpot = false;
		}

		NRPG::CUnit *pUnit = pGlobalPlayer->mercs[i];
 		CPtr<CUnitServer> pUS = AddUnit( placeForUnit, NRPG::CreateUnit( pUnit ), pRes );
		ASSERT( IsValid( pUS ) );
		if ( !IsValid( pUS ) )
			continue;
		//
		pUS->GetUnitRPG()->GetRPGUnit()->CalcDeathVP( GetGlobalGame()->pDifficulty->fDeathCoeff );
	}
	//
	InitPlayerCorpseCarrying( pRes );	
	pGlobalPlayer->deployData.bPassage = false;
	++nPartiesAdded;
	pRes->OnTBSEvent( TBS_START_NEW_TURN );
	OnNewPlayerTurn( pRes );
	StartGame();
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemovePlayer( IPlayer *_pPlayer )
{
	CDynamicCast<CPlayer> pPlayer( _pPlayer );
	UnregisterPlayer( pPlayer );

	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( IsValid(*i) )
			++i;
		else
			i = units.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetActiveUnits( IPlayer *_pPlayer, list<CUnit*> *pRes )
{
	CDynamicCast<CPlayer> pPlayer( _pPlayer );
	CPlayer::CUnitSet units;
	pPlayer->GetUnits( &units );
	pRes->clear();
	for ( CPlayer::CUnitSet::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( IsUnitActive( *i ) )
			pRes->push_back( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// for debug purposes
static void VisualizeSoundRadius( CVec3 ptCenter, float fRadius )
{
	sphereParticles.clear();
	for ( int nPhi = 0; nPhi < 360; nPhi += 2 )
	{
		float fPhi = 360.0 / 3.1415 * nPhi;
		CVec3 ptSphereCenter = ptCenter;
		ptSphereCenter.x += fRadius * cos( fPhi );
		ptSphereCenter.y += fRadius * sin( fPhi );
		ptSphereCenter.z += 0.3f;
		sphereParticles.push_back( SSphere( ptSphereCenter, 0.05f ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateSoundStuff( vector<CObj<CTimedObject> > *stuff, CVec3 ptPos )
{
	// create particle to show user where enemy is
	CTimedObject *pParticles = CreateDParticles( ptPos,
		CQuat(0,CVec3(0,0,1)), NDb::GetEffect(N_SOUND_PARTICLE_ID) );
	pParticles->Attach( pShowUnits, this );
	stuff->push_back( pParticles );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::MakeAISound( NDb::CAISound *pAISound, CDumbUnitServer *_pWho, int nSoundType, NDb::CSound *pSound )
{
	CDynamicCast<CUnitServer> pWho( _pWho );
	if ( !IsValid( pWho ) )
		return;
	if ( !pWho->CanFight() )
		return;

	CVec3 ptFrom = pWho->GetPosition().GetCP();

	vector<CObj<CTimedObject> > stuff;
	//CTimedObject *pSound = _pSound;
	if ( pSound == 0 )
		pSound = pAISound->pSound;
	if ( pSound )
	{
		CTimedObject *p = Create3DSound( ptFrom, pSound );
		p->Attach( pShowUnits, this );
		stuff.push_back( p );
		pWho->AttachMiscObject( p );
	}
	CreateSoundStuff( &stuff, pWho->GetPosition().GetCP() );
	// DEBUG{
	//float fTmpRadius = pAISound->GetRadiusFromAISoundType( pWho->GetAISoundType() ) * FP_GRID_STEP;
	//VisualizeSoundRadius( pWho->GetPosition().GetCP(), fTmpRadius );
	// EODEBUG}

	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		CUnitServer *pTarget = *i;
		if ( !pTarget->CanFight() )
			continue;
		if ( pTarget != pWho )
		{
			float fDistance = fabs( pTarget->GetPosition().GetCP() - pWho->GetPosition().GetCP() ) / FP_GRID_STEP;
			// � �� ����� �� ����� �� ������� ���������� ����������?
			if ( fDistance > pTarget->GetUnitRPG()->GetAISoundConstants()->nExitRadius )
				pTarget->SetAudible( pWho, false );
			// � �� ������ �� �� ���� ����?
			if ( pTarget->CanHearSound( pWho->GetPosition().GetCP(), pAISound, nSoundType, pWho ) )
			{
				pTarget->HearSound( stuff, pWho, pWho->GetPosition().pos.p );
				pTarget->SetAudible( pWho, true );
				//
				bool bDiplomacyEnemy = pTarget->GetDiplomacyState( pWho ) == NDb::DS_ENEMY;
				if ( bDiplomacyEnemy )
				{
					// retail CWorld::MakeAISound: the hearer learns of the heard hostile (OnHearEnemy -> possibleEnemy).
					NGlobal::ThrowEvent( NWorld::CEventOnHearEnemy( pTarget, pWho ) );
					GetAISignalManager()->Add( NAI::CreateAIUnitSoundSignal( pWho, pTarget, pAISound->fRadius ) );
				}
				else if ( pAISound->fRadius > 30.0f )
					NGlobal::ThrowEvent( NWorld::CEventOnHearAlly( pTarget, pWho ) );   // loud (>30) friendly sound -> ally-needs-help
				if ( bDiplomacyEnemy && pTarget->GetPlayer() != pWho->GetPlayer() && 
					pWho->GetUnitRPG()->IsHiding() && GetGame()->CheckVisibility( pTarget, pWho ) )
				{
					pWho->Hide( false );
					UpdateVisible();
				}
			}
			else
				pTarget->ClearSound( pWho );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::MakeSound( const CVec3 &ptCenter, NDb::CSound *pSound )
{
	if ( pSound )
		AttachMiscObject( Create3DSound( ptCenter, pSound ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::PlaceAllUnits()
{
	for ( list<CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
		(*i)->PlaceOnPassablePlace();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::StartGame()
{
	pAIMap->Sync();
	PlaceAllUnits();
	pAIMap->Sync();
	UpdateVisible();
	NGlobal::ThrowEvent( NWorld::CEventOnStartGame() );   // retail CWorld::StartGame @0x36bb50: one-shot begin refresh of every AI tracker
	StartTBSGame();
	csSystem << CC_RED << "Start game" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail IWorld vtbl+0x1d8 -- a script controls the game start: DelayGameStart(1) FREEZES it
// (c_DelayGameStartEx), DelayGameStart(0) lets it proceed (c_StartGameEx). Read by StartFirstSegments.
void CWorld::DelayGameStart( int bDelay )
{
	bFreezeStart = bDelay != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x36c4c0 -- the initial warm-up segments at fresh level start. Each Segment() ticks the world
// AND runs the own-script threads (wMain.cpp:1921), so while a script has FROZEN the start (bFreezeStart)
// we keep segmenting -- advancing the world clock by 50ms/segment (accumulated as hidden time so the
// mission's elapsed-time accounting is unaffected) -- until a script calls c_StartGameEx -> DelayGameStart(0);
// then 2 base segments finish the warm-up. A 10000-iteration cap guards against a never-unfreezing script.
// ELISION: the retail's separate up-front pAimTime tick is dropped (Segment refreshes pTime; pAimTime is
// aim-only and start-irrelevant).
void CWorld::StartFirstSegments()
{
	const STime nStep = 0x32;			// +50ms per extra segment
	const int   nFreezeCap = 0x2710;	// 10000-iteration safety cap
	//
	Segment();							// the up-front segment
	int nBase = 2;
	int nFreeze = nFreezeCap;
	while ( ( bFreezeStart && --nFreeze > 0 ) || ( --nBase > 0 ) )
	{
		tHiddenDelta += nStep;
		GetTime()->Set( GetTime()->GetValue() + nStep );
		Segment();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x361bb0 -- time of day is a single 6(DAY)/5(NIGHT) sentinel in createFlags (or neither = ANYTIME).
ETimeOfDay CWorld::GetTimeOfDay()
{
	for ( int i = 0; i < createFlags.size(); ++i )
		if ( createFlags[ i ] == TOD_DAY )
			return TOD_DAY;
	for ( int i = 0; i < createFlags.size(); ++i )
		if ( createFlags[ i ] == TOD_NIGHT )
			return TOD_NIGHT;
	return TOD_ANYTIME;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x362b90 -- rewrite the createFlags TOD sentinel (drop any existing day then night, append the new
// one unless ANYTIME), then re-light every live object (so the renderer re-evaluates them for the new TOD)
// and force an UpdateVisible. The SetTimeOfDay lua binding follows this with a CUICmdSetAmbient recompute.
void CWorld::SetTimeOfDay( ETimeOfDay tod )
{
	for ( vector<int>::iterator it = createFlags.begin(); it != createFlags.end(); ++it )
		if ( *it == TOD_DAY ) { createFlags.erase( it ); break; }
	for ( vector<int>::iterator it = createFlags.begin(); it != createFlags.end(); ++it )
		if ( *it == TOD_NIGHT ) { createFlags.erase( it ); break; }
	if ( tod != TOD_ANYTIME )
		createFlags.push_back( (int)tod );
	//
	for ( list< CObj<CObjectServerBase> >::iterator i = objects.begin(); i != objects.end(); ++i )
		if ( IsValid( *i ) )
			(*i)->UpdateLight();
	UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x361c00: a script asks for turn-based mode. Record the wish (saved state), then -- only while
// the world is currently real-time and at least one player exists -- request a switch to turn-based for
// the LAST player in the list (the CTBSWorld base queues an interrupt for it).
void CWorld::ScriptWantTurnBased( bool bWant )
{
	bScriptWantTurnBased = bWant;
	if ( !bWant )
		return;
	if ( !IsRealTime() )
		return;
	vector< CPtr<CPlayer> > pls;
	GetPlayersList( &pls );
	if ( pls.empty() )
		return;
	WantTurnBased( pls.back() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// LUA convergence (hint machinery): retail NWorld::CWorld::AddNextUIHint @0x368140 (IWorld vtbl+0x118).
// Walk the CUIHint table and pick the not-yet-shown hint with the smallest nSequenceID strictly greater
// than the global game's cursor (nHintSequenceID); advance the cursor onto it and remember it in hintsSet.
// When bSilent is false it also pops the hint modal -- but the sole caller (lua AddHints) passes true, so the
// sequence advances silently. When fewer than two unshown hints remained, the sequence is exhausted: set
// bNoMoreHints and refresh. (Retail also RemoveAllHintItems()s the in-world hint pickups here; this
// predecessor fork has no in-world hint-item subsystem, so there is nothing to remove.)
void CWorld::AddNextUIHint( bool bSilent )
{
	NRPG::CGlobalGame *pGlobalGame = GetGlobalGame();
	if ( !IsValid( pGlobalGame ) )
		return;
	CDBTable<NDb::CUIHint> *pTable = NDatabase::GetTable<NDb::CUIHint>();
	if ( !pTable )
		return;
	//
	NDb::CUIHint *pNext = 0;
	int nUnshown = 0;
	for ( CDBIterator<NDb::CUIHint> it( *pTable ); it.MoveNext(); )
	{
		NDb::CUIHint *pHint = it.Get();
		if ( !IsValid( pHint ) )
			continue;
		// only hints ahead of the cursor are candidates
		if ( pGlobalGame->nHintSequenceID >= pHint->nSequenceID )
			continue;
		// already shown? -> skip
		bool bAlreadyShown = false;
		for ( vector< CDBPtr<NDb::CUIHint> >::iterator i = pGlobalGame->hintsSet.begin(); i != pGlobalGame->hintsSet.end(); ++i )
			if ( (*i).GetPtr() == pHint ) { bAlreadyShown = true; break; }
		if ( bAlreadyShown )
			continue;
		//
		++nUnshown;
		if ( !IsValid( pNext ) || pHint->nSequenceID < pNext->nSequenceID )
			pNext = pHint;
	}
	//
	if ( IsValid( pNext ) )
	{
		pGlobalGame->nHintSequenceID = pNext->nSequenceID;
		pGlobalGame->hintsSet.push_back( pNext );
		if ( !bSilent )
			AddUICommand( new CUICmdShowHint( pNext ) );
	}
	if ( nUnshown < 2 )
	{
		pGlobalGame->bNoMoreHints = true;
		UpdateVisible();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ExecuteCommand( CCommand *_pCmd )
{
	CObj<CCommand> pHold(_pCmd);
	CDynamicCast<CCmdInterfaceEvent> pEvent(_pCmd);
	if (pEvent)
	{
		// release id-queue: a finished UI command posts its id; drop it from the own-script's queue so the
		// lua WaitForUI(id) that queued it unblocks. (Replaces the dev per-type OnInterfaceEvent routing.)
		if ( IsValid( pOwnScript ) )
			pOwnScript->RemoveUIActionID( pEvent->nID );
	}
	else {
		CDynamicCast<CCmdCallScriptFunction> pCall(_pCmd);
		if (pCall)
			NScript::luaCallFunction(pCall->szFuncName, pCall->params);
		else {
			CDynamicCast<CCmdAddUnit> pAddUnit(_pCmd);
			if (pAddUnit)
			{
				CDynamicCast<CPlayer> pPlayer(pAddUnit->pPlayer);
				AddUnitInGame(pAddUnit->sPos.p, NRPG::CreateUnit(pAddUnit->pMerc), pPlayer);
			}
			else {
				CDynamicCast<CCmdRemoveUnit> pRemoveUnit(_pCmd);
				if (pRemoveUnit)
				{
					CDynamicCast<CUnitServer> pUnit(pRemoveUnit->pUnit);
					RemoveUnit(pUnit);
				}
				else {
					CDynamicCast<CCmdUnit> pUnitCmd(_pCmd);
					if (pUnitCmd)
					{
						CDynamicCast<CUnitServer> pUS(pUnitCmd->pUnit);
						ASSERT(pUS);
						ASSERT(IsUnitActive(pUS));
						pUS->Do(_pCmd);
					}
					else
						ASSERT(0);
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CHitLocator* CWorld::GetHitEvent()
{
	if ( eventHits.empty() )
		return 0;
	CHitLocator* pPart = eventHits.front().Extract();
	eventHits.pop_front();
	return pPart;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmd* CWorld::GetUICommand()
{
	if ( uiCmdsList.empty() )
		return 0;

	CUICmd* pCmd = uiCmdsList.front().Extract();
	uiCmdsList.pop_front();
	return pCmd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CheckForAcks()
{
	if ( IsForcedRealTime() )
		return;
	//
	CPtr<NDb::CDBAckSequence> pSeq = pGlobalAck->GetSequence( 0 );
	if ( !IsValid( pSeq ) )
		return;
	//
	int nCount = 0;
	while ( pSeq->pDBAckInfo[nCount] ) nCount++;
	//
	vector< CPtr<NWorld::CAckEvent> > phrases;
	phrases.resize( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		int nPersID = pSeq->pDBAckInfo[i]->nRPGPersID;
		CDynamicCast<NWorld::CUnit> pUnit( GetUnitServerByPersID( nPersID ) );
		ASSERT( pUnit );
		phrases[i] = new CAckEvent( pSeq->nPriority, pUnit.GetPtr(), pSeq->pDBAckInfo[i] );
	}
	//
	AddUICommand( new NWorld::CUICmdPlayAck( phrases ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::Explode( const CVec3 &ptEpicentre, int nPower )
{
	AttachMiscObject( CreateDGrassEvent( ptEpicentre ) );
	CreateParticle( ptEpicentre, CQuat(random.GetFloat(0,10000),CVec3(0,0,1)), NDb::GetEffect( 51 ) );
	//
	MakeSound( ptEpicentre, NDb::GetSound(63) );
	//
	list< CObj<CBuilding> >::const_iterator it;
	for ( it = buildings.begin(); it != buildings.end(); ++it )
		(*it)->Explode( ptEpicentre, nPower );
	SSphere sphere;
	sphere.ptCenter = ptEpicentre;
	sphere.fRadius = 15;
	ActivateDebris( sphere, GetAIMap(), pTime );
	//
	CheckStability();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GenerateDebris( NDb::CDebrisMaterial *pDebrisMaterial, const CVec3 &ptCenter, const CVec3 &ptDir, int nDebris )
{
	if ( !pDebrisMaterial )
		return;
	for ( int i=0; i<nDebris; ++i )
	{
		NDb::CDebris *pDebris = pDebrisMaterial->GetDebris();
		if ( !pDebris )
			continue;
		SRand rand;
		CPtr<NDb::CModel> pModel = pDebris->pModel->CreateModel( &rand );
		CVec3 randVel;
		randVel.x = random.GetFloat(-1,1);
		randVel.y = random.GetFloat(-1,1);
		randVel.z = random.GetFloat(1,3);
		AddDebris( pModel.GetPtr(), pAIMap, ptCenter + CVec3(0,0,1), QNULL, randVel, pTime );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CheckStability()
{
	bool bNeedRecalc = true;
	while ( bNeedRecalc )
	{
		pAIMap->Sync();
		bNeedRecalc = false;
		list< CObj<CObjectServerBase> >::const_iterator io;
		for ( io = objects.begin(); io != objects.end(); )
		{
			CObjectServerBase *pTest = *io++;
			if ( !pTest->CheckStability() )
				bNeedRecalc = true;
		}
	}
	list< CObj<CUnitServer> >::const_iterator iu;
	for ( iu = units.begin(); iu != units.end(); ++iu )
	{
		CUnitServer *pTest = *iu;
		pTest->CheckStability();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::Segment()
{
	TTBSWorld::Segment();
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); )
	{
		if ( IsValid(*i) )
		{
			(*i)->Segment();
			++i;
		}
		else
			i = units.erase( i );
	}
	CallSegment( &miscObjects );
	for ( list< CPtr<CObjectServerBase> >::iterator i = segmentObjects.begin(); i != segmentObjects.end(); )
	{
		if ( !IsValid(*i) || (*i)->Segment() )
			i = segmentObjects.erase( i );
		else
			++i;
	}

	SSphere changed;
	bool bChanged = CDebrisController::Segment( &changed );

	MarkNewDGFrame();
	CDGPtr<CFuncBase<STime> > pRefreshTime( pTime );
	pRefreshTime.Refresh();
	pAIMap->Sync( IsAction() ? NAI::IAIMap::ST_FAST : NAI::IAIMap::ST_NORMAL );

	pGlobalAck->OnSegment();
	NGlobal::ThrowEvent( CEventOnSegment() );   // per-segment broadcast (script-logic units count segments)
	pAIJobManager->Segment();
	pAISignalManager->Segment();
	CheckForAcks();

	pScript = pOwnScript;
	pOwnScript->ExecuteThreads();
	CheckRealTimeTurn();
	// retail CWorld::Segment (wMain.c:11549): on the first segment of a freshly (re)started scenario, fire the
	// global lua OnEnterZone() hook once, then latch off (re-armed by RunPostInit on the next zone load).
	if ( bFirstSegment )
	{
		NScript::luaCallFunction( "OnEnterZone", "" );
		bFirstSegment = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ExecuteOwnScript -- the menu interfaces tick pOwnScript directly (outside Segment) to advance the
// menu scenario script every frame.  A bare pOwnScript->ExecuteThreads() crashes: the lua bindings read
// NScript::GetScript() (the global `pScript`), which is only valid while it points at the running script.
// Segment sets `pScript = pOwnScript` before ticking (above); reproduce that here so the direct tick has
// a valid script context.  (Without it, after a pushed interface's world is destroyed the global pScript
// is invalid -> GetScript()==0 -> a coroutine's ObjectIsAction dereferences null -> crash.)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ExecuteOwnScript()
{
	if ( !IsValid( pOwnScript ) )
		return;
	pScript = pOwnScript;
	pOwnScript->ExecuteThreads();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CheckRealTimeTurn()
{
	STime curTime = GetTime()->GetValue();
	if ( !IsForcedRealTime() && IsRealTime() )
	{
		if ( curTime - prevTurnTime > N_REALTIME_TURN )
		{
			OnNewTurn();
			prevTurnTime = curTime;
			NGlobal::ThrowEvent( CEventOnNewPlayerTurnOrTime() );
		}
		if ( curTime - prevFastTurnTime > N_REALTIME_FAST_TURN )
		{
			prevFastTurnTime = curTime;
			NGlobal::ThrowEvent( CEventOnNewPlayerFastTurnOrTime() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::CanSeeAction( IPlayer *_pPlayer )
{
	CDynamicCast<CPlayer> pPlayer( _pPlayer );
	return IsRealTime() || CanPlayerSeeAction( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::UpdateWorld( STime tScene, IPlayer *pPlayer )
{
	CFWContext world( &pCurrentWorld, this );
	ASSERT( IsValid( pTime ) );
	STime tCurrent = pTime->GetValue();
	int nSkipped = 0;
	tScene += tHiddenDelta;
	while ( tScene >= tCurrent ) // CRAP - time wrap problem
	{
		tCurrent += DW_SEGMENT_TIME;
		pTime->Set( tCurrent );
		MarkNewDGFrame();
		CDGPtr<CFuncBase<STime> > pRefreshTime(pTime);
		pRefreshTime.Refresh();
		pAimTime->Set( tCurrent - tHiddenDelta );
		CDGPtr<CFuncBase<STime> > pRefreshAimTime( pAimTime );
		pRefreshAimTime.Refresh();
		Segment();

		if ( IsValid(pPlayer) && CanSkip( CDynamicCast<CPlayer>(pPlayer) ) && nSkipped < 5 )
		{
			tHiddenDelta += DW_SEGMENT_TIME;
			tScene += DW_SEGMENT_TIME;
			nSkipped++;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::KillObject( CObjectServerBase *pOS )
{
	objects.remove( pOS );
	CDynamicCast<NWorld::IDynamicObject> pDyn(pOS);
	if (pDyn)
	{
		CObj<NWorld::IDynamicObject> pObj(pDyn);
		miscObjects.remove( pObj );
	}
/*	CWObject *pO = pOS;
	showObjects.Remove( pO );
	GenerateDebris( pOS->GetDebrisMaterial(), pOS->GetPosition(), ptDir, 5 );
	objects.remove( pOS );
	ActivateDebris( SSphere( pOS->GetPosition(), 5 ), GetAIMap(), pTime );*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::PerformRangedAttack( const NRPG::CAttackPortion &ap, const CRay &ray, const vector<NRPG::IAttackable*> &ignores, STime sCast, NDb::CModel *pTrailModel, float fTrailSpeed )
{
	vector<NRPG::STrailPoint> trail;
	pRPGGame->ProcessRangedAttackPortion( ap, ray, ignores, &trail );

	miscObjects.push_back( CreateBulletServer( this, trail, sCast, pTrailModel, fTrailSpeed ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ThrowGrenade( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, 
	float fTFly, NDb::CModel *pModel, NDb::CRPGGrenade *pRPGGrenade, CUnitServer *pUnitServer )
{
	if ( !pRPGGrenade )
		return;
	miscObjects.push_back( CreateGrenadeServer( this, vFrom, vSpeed, tThrow, 
		fTFly, pModel, pRPGGrenade, pUnitServer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ThrowKnife( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fDistance,
	NDb::CModel *pModel, NRPG::CAttackPortion &attack, NRPG::IInventoryItem *pIItem, CUnitServer *pUnitServer )
{
	miscObjects.push_back( CreateKnifeServer( this, vFrom, vSpeed, tThrow, 
		fDistance, pModel, attack, pIItem, pUnitServer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::LaunchRocket( const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &attack, 
		NRPG::IClipItem *pRocket, CUnitServer *pIgnored, NDb::CEffect *pEffect   )
{
	miscObjects.push_back( CreateRocketServer( this, vFrom, vSpeed, tThrow, 
		fDistance, pModel, attack, pRocket, pIgnored, pEffect ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_UNIT_REACH_DISTANCE = 20.0f;
const float F_UNIT_REACH_COLLIDER_RADIUS = 0.3f;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::FindCloseGroundItems( CUnit *pU, vector<SItem> *pRes )
{
	CVec3 tracePos = pU->GetPosition().GetCenter();

	list< CObj<CDFrozenItem> >::iterator it;
	for ( it = showFrozenItems.begin(); it != showFrozenItems.end(); ++it )
	{
		if ( !(*it)->GetInvItem() )
			continue;
		CVec3 from = (*it)->GetPos();
		if ( fabs2( from - tracePos ) > sqr(F_UNIT_REACH_DISTANCE) )
			continue;
		SItem &item = *pRes->insert( pRes->end(), SItem());
		item.eType = SItem::GROUND;
		item.pItem = (*it)->GetInvItem();
		item.pWorldItem = (*it);
		item.pUnit = 0;
		item.nSlot = -1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::IsWinnerPlayer( IPlayer *pPlayer )
{
	CDynamicCast<CPlayer> p( pPlayer );
	//if ( nAIUnitsCreated == 0 && nPartiesAdded <= 1 )
	//	return false;
	return !HasEnemies( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::MakeExplosion( const CRay &ray, int nMaxFloor )
{
	CPtr<CUnitServer> pThrower = 0;
	if ( !units.empty() )
		pThrower = units.front();
	//
	if ( IsValid( pThrower ) )
	{
		int nUserID;
		CVec3 ptPoint;
		CObjectBase *pUserData;
		if ( TraceRay( this, ray, nMaxFloor, &pUserData, &nUserID, &ptPoint ) )
			AddGrenadeExplosion( ptPoint, NDb::GetRPGGrenade( 21 ), pThrower );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::AddGrenadeExplosion( const CVec3 &vStartPosition, NDb::CRPGGrenade *pRPGGrenade,
	CUnitServer *pUnitServer, CObjectBase *pIgnitionObject )
{
	ASSERT( IsValid(pRPGGrenade) );
	if ( !IsValid(pRPGGrenade) )
		return;
	int nEffectType = 0, nSoundType = 0;
	NDb::CRPGArmor *pArmor = GetArmor( vStartPosition );
	if ( pArmor )
	{
		nEffectType = Clamp( pArmor->nGrenadeExplostionType - 1, 0, NDb::N_MAX_GRENADE_EXPLOSION_TYPE - 1 );
		nSoundType = Clamp( pArmor->nGrenadeSoundType - 1, 0, NDb::N_MAX_GRENADE_SOUND_TYPE - 1 );
	}
	SRand rnd;
	if ( pRPGGrenade->pEffect.p[ nEffectType ] )
		CreateParticle( vStartPosition, CQuat(random.GetFloat(0,10000),CVec3(0,0,1)), pRPGGrenade->pEffect.p[ nEffectType ]->GetEffect( &rnd ) );
	//AttachMiscObject( new CDFlash( vStartPosition + CVec3(0,0,5), CVec3(1,1,1), 10, 3 ) );
	if ( pRPGGrenade->pSound.p[ nSoundType ] )
		MakeSound( vStartPosition, pRPGGrenade->pSound.p[ nSoundType ]->GetSound( &rnd )->pSound );
	GetAISignalManager()->Add( NAI::CreateAIGrenadeSoundSignal( vStartPosition ) );
	// the thrower's explosive-perk modifiers are seeded inside the CVoxelExplTracker ctor (before ExplodeFragments).
	miscObjects.push_back( new CVoxelExplTracker( vStartPosition, pRPGGrenade, pUnitServer, this, pIgnitionObject ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::GetUnitServer( string szName )
{
	string szUpperName;
	MakeUpperName( szName, &szUpperName );
	return CDynamicCast<CUnitServer>( nameToObj[ szUpperName ] ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer *CWorld::GetUnitServer( NRPG::IUnitMissionInfo *pUnitMission )
{
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
		if ( (*i)->GetUnitRPG() == pUnitMission )
			return *i;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer *CWorld::GetUnitServer( NRPG::CUnit *pRPGUnit )
{
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
		if ( (*i)->GetUnitRPG()->GetRPGUnit() == pRPGUnit )
			return *i;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer *CWorld::GetUnitServerByPersID( int nPersID ) const
{
	for ( list< CObj<CUnitServer> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( (*i)->GetUnitRPG()->GetRPGPersID() == nPersID )
			return (*i).GetPtr();
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::UsePassageObject( CUnitServer *pUS, int nPassageZoneID )
{
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pUS ) )
		return false;
	//
	CPtr<NScenario::CScenarioZone> pZone = GetGlobalGame()->pCurrentZone;
	if ( !IsValid( pZone ) )
		return false;
	//
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = pUS->GetPlayer()->GetGlobalPlayer();
	if ( !IsValid( pGlobalPlayer ) )
		return false;
	//
	list< CPtr<IPassageObject> > passageObjects;
	GetPassageObjects( nPassageZoneID, &passageObjects );
	// ��������� ��� �� ����� ����� � ���������� ��������� ��������
	bool bCanPass = true;
	unordered_map< CPtr<CUnitServer>, CPtr<IPassageObject>, SPtrHash > passagesForUnits;
	for ( list< CObj<CUnitServer> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !(*i)->IsUnconscious() && 
			!(*i)->GetUnitRPG()->IsDead() && (*i)->GetPlayer() == pUS->GetPlayer() )
		{
			bool bCanUnitPass = false;
			for (	list< CPtr<IPassageObject> >::iterator j = passageObjects.begin(); j != passageObjects.end(); ++j )
				if ( (*j)->CanPass( *i ) )
				{
					bCanUnitPass = true;
					passagesForUnits[ (*i).GetPtr() ] = *j;
					break;
				}	
			//
			bCanPass &= bCanUnitPass;
		}
	}
	//
	if ( !bCanPass )
		return false;
	//
	if ( nPassageZoneID <= 0 )
	{
		// ������� �� chapter map
		AddUICommand( new NWorld::CUICmdContinueChapter() );
		return true;
	}
	// ���� � ����� template ���������
	vector<int> templatesIDs;
	pZone->GetTemplatesIDs( &templatesIDs );
	for ( vector<int>::iterator i = templatesIDs.begin();	i != templatesIDs.end(); ++i )
	{
		if ( *i != GetGlobalGame()->nCurrentTemplateID )
		{
			// ���� passage objects
			SMapInfo info;
			vector<string> strParams;
			int nVariantID = pZone->GetVariantIDForTemplate( *i );
			SRandomSeed seed = pZone->GetRandomSeedForTemplate( *i );
			if ( !BuildMap( nVariantID, strParams, 0, &info, -1, seed ) )
				continue;
			//
			for ( list<SMapElement>::const_iterator j = info.items.begin(); j != info.items.end(); ++j )
			{
				if ( IsValid( (*j).pObject->pPassage ) && (*j).nPassageZoneID == nPassageZoneID )
				{
					// ��������� ������ � ��������
					NRPG::SDeployData &deployData = pGlobalPlayer->deployData;
					deployData.bPassage = true;
					deployData.nPassageZoneID = nPassageZoneID;
					//
					unordered_map< CPtr<CUnitServer>, CPtr<IPassageObject>, SPtrHash >::iterator u;
					for ( u = passagesForUnits.begin(); u != passagesForUnits.end(); ++u )
						deployData.unitsDeployData[ u->first->GetUnitRPG()->GetRPGUnit() ].nPassageObjectID =
							u->second->GetPassageObjectID();
					// ���������
					AddUICommand( new NWorld::CUICmdLoadTemplate( pZone, *i ) );
					return true;
				}
			}
		}
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnFrozenItemDestroyed( int nItemID )
{
	GetGlobalGame()->pScenarioTracker->OnScenarioClueDestroyed( nItemID, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CheckSpot( const vector< CPtr<CPlayer> > &players )
{
	bool bUpdate = false;
	//
	for ( list< CObj<CUnitServer> >::iterator u = units.begin(); u != units.end(); ++u )
		if ( (*u)->GetUnitRPG()->IsHiding() )
		{
			for ( vector< CPtr<CPlayer> >::const_iterator i = players.begin(); i != players.end(); ++i )
			{
				if ( (*i) == (*u)->GetPlayer() )
					continue;
				//
				vector< CPtr<CUnit> > enemies;
				(*i)->GetUnits( &enemies );
				for ( vector< CPtr<CUnit> >::iterator e = enemies.begin(); e != enemies.end(); ++e )
				{
					CDynamicCast<CUnitServer> pEnemyUS( e->GetPtr() );
					if ( IsValid( pEnemyUS ) && pEnemyUS->GetDiplomacyState( (*u) ) == NDb::DS_ENEMY &&
						pEnemyUS->CanFight() && GetGame()->CheckVisibility( pEnemyUS, *u ) )
					{
						float fDistance = fabs( pEnemyUS->GetPosition().GetCP() - (*u)->GetPosition().GetCP() ) / FP_GRID_STEP;
						int nProbability = pEnemyUS->GetUnitRPG()->GetUnhideProbability( (*u)->GetUnitRPG(), fDistance );
						int nCheck = random.Get( 0,100 );
						if ( nCheck < nProbability )
						{
							csSystem << CC_RED << 
								"Hiding unit detected: probability = " << nProbability << ", check = " << nCheck << endl;
							(*u)->Hide( false );
							bUpdate = true;
						}
					}
					//
					if ( !(*u)->GetUnitRPG()->IsHiding() )
						break;
				}
				//
				if ( !(*u)->GetUnitRPG()->IsHiding() )
					break;
			}
		}
	//
	if ( bUpdate )
		UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnNewPlayerTurn( CPlayer *pPlayer )
{
	NGlobal::ThrowEvent( CEventOnNewPlayerTurnOrTime( pPlayer ) );
	NGlobal::ThrowEvent( CEventOnNewPlayerFastTurnOrTime( pPlayer ) );
	NGlobal::ThrowEvent( CEventOnNewPlayerTurn( pPlayer ) );
	//
	GetGlobalAck()->OnNewTurnStarted( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnNewPlayerFastTurnOrTime( const CEventOnNewPlayerFastTurnOrTime &event )
{
	vector< CPtr<CPlayer> > players;
	if ( !IsValid( event.pPlayer ) )
		GetPlayersList( &players );
	else
		players.push_back( event.pPlayer );
	CheckSpot( players );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnRealTimeStarted()
{
	GetGlobalAck()->OnRealTimeStarted();
	prevTurnTime = GetTime()->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnNewTurn()
{
	++nTurnID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWorld::GetEnemyWatchers( IPlayer *pPlayer ) const
{
	return TTBSWorld::GetEnemyWatchers( dynamic_cast<CPlayer*>( pPlayer ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::IsTBSRealTimeModePossible() const 
{ 
	if ( nPartiesAdded > 1 )
		return false;
	vector< CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( int k = 0; k < players.size(); ++k )
	{
		CPlayer *pPlayer = players[k];
		if ( !pPlayer->HasAlivePeople() )
			continue;
		int nID = pPlayer->GetScenarioPlayerID();
		if ( pPlayer->HasLostFromSightAliveUnits() )
		{
			csSystem << CC_GREEN << "Real time is impossible, player " << nID << " has lost units from sight this turn" << endl;
			return false;
		}
		if ( !pPlayer->IsTurnDone() )
		{
			csSystem << CC_GREEN << "Real time is impossible, player " << nID << " has not finished his turn" << endl;
			return false;
		}
	}
	for ( int k = 0; k < players.size(); ++k )
	{
		CPlayer *pPlayer = players[k];
		if ( !pPlayer->HasAlivePeople() )
			continue;
		int nKID = pPlayer->GetScenarioPlayerID();
		const list<CPtr<CUnitServer> > &visible = pPlayer->GetTBSVisible();
		for ( list<CPtr<CUnitServer> >::const_iterator i = visible.begin(); i != visible.end(); ++i )
		{
			CUnitServer *pUnit = *i;
			if ( !IsValid(pUnit) || !IsValid( pUnit->GetPlayer() ) )
				continue;
			if ( !pUnit->CanFight() )
				continue;
			int nUnitPlayerID = pUnit->GetPlayer()->GetScenarioPlayerID();
			if ( pDiplomacy->GetDiplomacyState( nKID, nUnitPlayerID ) == NDb::DS_ENEMY )
			{
				string szUnitName("?");
				GetUnitName( pUnit, &szUnitName );
				csSystem << CC_GREEN << "Real time is impossible, player " << nKID << " sees not ally unit " << szUnitName << " of player " << nUnitPlayerID << endl;
				return false;
			}
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::OnAction( bool bStartAction )
{
	for ( list<CObj<CUnitServer> >::iterator u = units.begin(); u != units.end(); ++u )
	{
		CUnitServer *pUS = *u;
		if ( pUS->GetState() != CUnit::ST_HEALER )
			pUS->animator.IdleBan( NAnimation::E_NO_IDLE_ON_ACTION, bStartAction );
	}
	if ( !bStartAction )
		CheckStability();
	pPathNetwork->Freeze( bStartAction );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::GetPassageObjects( int nPassageZoneID, list< CPtr<IPassageObject> > *pPassageObjects )
{
	pPassageObjects->clear();
	// ���� ������� �������� ���� ���� ��������
	for (list< CObj<CObjectServerBase> >::iterator i = objects.begin(); i != objects.end(); ++i)
	{
		CDynamicCast<IPassageObject> pPassage(*i);
		if (pPassage)
			if (pPassage->GetPassageZoneID() == nPassageZoneID)
				pPassageObjects->push_back(pPassage.GetPtr());
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitServer* CWorld::GetDeployedDeadUnit( const NAI::SPathPlace &aiPos, NRPG::CUnit *pRPGUnit )
{
	CUnitServer *pUS = 0;
	pUS = GetUnitServer( pRPGUnit );
	if ( pUS == 0 )
	{
		NAI::SUnitPosition p;
		p.pos.SetNetwork( pPathNetwork );
		p.pos.p = aiPos;
		CPtr<NRPG::IUnitMission> pRPG = NRPG::CreateUnit( pRPGUnit );
		pUS = new CUnitServer( this, pRPG, pRPG->GetModel(), pDeployedDeadUnitsPlayer, p );
		units.push_back( pUS );
		pDeployedDeadUnitsPlayer->AddUnit( pUS );
	}
	return pUS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::InitPlayerCorpseCarrying( CPlayer *pPlayer )
{
	ASSERT( IsValid( pPlayer ) );
	if ( !IsValid( pPlayer ) )
		return;
	//
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = pPlayer->GetGlobalPlayer();
	if ( !IsValid( pGlobalPlayer ) )
		return;
	//
	vector< CPtr<CUnit> > units;
	pPlayer->GetUnits( &units );
	for ( vector< CPtr<CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		CDynamicCast<CUnitServer> pUS(*i);
		if (pUS)
		{
			NRPG::SUnitDeployData &deployData = pGlobalPlayer->deployData.unitsDeployData[ pUS->GetUnitRPG()->GetRPGUnit() ];
			NRPG::CUnit *pCorpse =	deployData.pCorpse;
			if ( IsValid( pCorpse ) )
			{
				CPtr<CUnitServer> pCorpseUS = GetDeployedDeadUnit( pUS->GetPosition().pos.p, pCorpse );
				if ( IsValid( pCorpseUS ) )
				{
					CCmd *pTake = new CCmdTakeCorpseOnDeploy( pUS, pCorpseUS, !deployData.bCorpseAlive );
					pUS->Do( new CCmdSetCommand( pUS.GetPtr(), pTake ) );
					pUS->Do( new CCmdSetCommand( pUS.GetPtr(), new CCmdContinue() ) );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayer *CWorld::GetPlayerByID( int nScenarioPlayerID, int nIndex )
{
	// retail @0x365530: among the players whose scenario id matches, return the nIndex-th (0-based);
	// nIndex defaults to 0 so the single-arg callers get the first match (the dev predecessor behavior).
	vector< CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( vector< CPtr<CPlayer> >::iterator i = players.begin(); i != players.end(); ++i )
		if ( (*i)->GetScenarioPlayerID() == nScenarioPlayerID )
		{
			if ( nIndex == 0 )
				return *i;
			--nIndex;
		}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CWorld::OnInterfaceEvent removed -- the release replaced the per-type interface-action routing with the
//  CScript id-queue; ExecuteCommand now drops the finished command's id via pOwnScript->RemoveUIActionID.)
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CAIRouteWaypoint* CWorld::GetWaypoint( string szName )
{
	string szLowerName = szName;
	NStr::ToLower( szLowerName );
	NAI::CAIRouteWaypoint *pRes = waypoints[ szLowerName ];
	if ( !IsValid( pRes ) )
		NScript::ScriptWarning( string( "waypoint \"" ) + string( szName )+ string( "\" not found " ) );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitGroup* CWorld::GetUnitGroup( int nGroupID )
{
	for ( int i = 0; i < unitGroups.size(); ++i )
		if ( unitGroups[i]->GetID() == nGroupID )
			return unitGroups[i].GetPtr();
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitGroup* CWorld::CreateUnitGroup( int nGroupID )
{
	int nID = nGroupID;
	if ( nID < 0 )
	{
		for ( int i = 0; i < unitGroups.size(); ++i )
			nID = Max( nID, unitGroups[i]->GetID() );
		++nID;
	}
	CPtr<CUnitGroup> pUnitGroup = new CUnitGroup( nID );
	unitGroups.push_back( pUnitGroup.GetPtr() );
	return pUnitGroup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemoveUnitGroup( CUnitGroup* pUnitGroup )
{
	ASSERT( IsValid( pUnitGroup ) );
	if ( IsValid( pUnitGroup ) )
		unitGroups.erase( remove( unitGroups.begin(), unitGroups.end(), pUnitGroup ), unitGroups.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGArmor* CWorld::GetArmor( const CVec3 &_point )
{
	CVec3 point( _point );
	vector<SSphere> spheres;
	vector<CVec3> vels;
	vector<NAI::SCollisionPoint> ress;
	spheres.push_back( SSphere( point + CVec3(0,0,0.11f), 0.1f ) );
	vels.push_back( CVec3(0,0,-1.5f) );
	NAI::PhysCollideInfo( GetAIMap(), spheres, vels, &ress, TS_PASS_BLOCKER );
	NAI::SCollisionPoint &res = ress[0];
	if ( res.fDist != NAI::FP_NO_COLLISION )
	{
		if ( res.pSrc->pUserData )
		{
			// armor
			return res.pSrc->pArmor;
		}
		else
		{
			// terrain
			CDGPtr<CFuncBase<STerrainInfo> > pInfo = GetTerrainInfo();
			pInfo.Refresh();
			const STerrainInfo &info = pInfo->GetValue();
			point /= FP_GRID_STEP;
			int nX = Float2Int( point.x );
			int nY = Float2Int( point.y );
			return info.GetStepSound( nX, nY ).pArmor;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 GetRandomSpherePoint()
{
	float fpX, fpY, fpZ, fpLeng;
	for ( int nSafe = 10; nSafe > 0; nSafe-- )
	{
		fpX = random.GetFloat( -1, 1 );
		fpY = random.GetFloat( -1, 1 );
		fpZ = random.GetFloat( -1, 1 );
		fpLeng = sqr( fpX ) + sqr( fpY ) + sqr( fpZ );
		if ( fpLeng < 1 )
			break;
	}
	return CVec3( fpX, fpY, fpZ ) / sqrt( fpLeng );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::CreateBloodyMess( const CVec3 &_vCenter, const CVec3 &vDirection, CObjectBase *pIgnore, int nParts )
{
	NAI::CPhysCollider collider;
	SBound bv;
	SRand rnd;
	bv.SphereInit( _vCenter, 2 );
	collider.AddIgnoredUserData( pIgnore );
	pAIMap->PrepareCollider( &collider, bv, 2, TS_FRAGMENTED );
	for ( int i = 0; i < nParts; ++i )
	{
		NAI::SCollisionPoint p;
		CVec3 vel;
		float fRadiusKoef = 1;
		if ( fabs2( vDirection ) > 0 )
		{
			vel = vDirection;
			Normalize( &vel );
			vel.x += random.GetFloat( -0.2f, 0.2f );
			vel.y += random.GetFloat( -0.2f, 0.2f );
			vel.z += random.GetFloat( -0.2f, 0.2f );
			vel *= 1.5f;
		}
		else
		{
			fRadiusKoef = 2;
			vel = GetRandomSpherePoint() * 2;
		}
		collider.CollideInfo( SSphere( _vCenter, 0.1f ), vel, &p );
		if ( p.fDist != NAI::FP_NO_COLLISION )
		{
			CVec3 vDir = p.pt - _vCenter;
			Normalize( &vDir );
			NDb::CRPGArmor *pArmor = NDb::GetArmor( 1 );
			CObjectBase *pUserData = p.pSrc->pUserData ? p.pSrc->pUserData : GetTerrainInfo();
			CDynamicCast<NWorld::IBuilding> pBuilding(p.pSrc->pUserData);
			if (pBuilding)
				pUserData = pBuilding->GetSceneHandle();
			float fRadius = pArmor->fShotRadius;
			fRadius *= random.GetFloat( 0.9f, fRadiusKoef + 0.1f );
			new CDecal( this, p.pt, vDir, fRadius, pArmor->pShotMaterial->GetMaterial(&rnd), pUserData );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ForceRealTime( bool _bForceRealTime )
{
	bool bRealTime = IsRealTime();
	bForcedRealTime = _bForceRealTime;
	//
	vector<CPtr<NWorld::CPlayer> > players;
	GetPlayersList( &players );
	for ( vector< CPtr<NWorld::CPlayer> >::const_iterator i = players.begin(); i != players.end(); ++i )
		(*i)->GetCommander()->Do( new NWorld::CCmdEndOfTurn() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CWorld::IsForcedRealTime() const
{
	return bForcedRealTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool GetNameFromHash( const unordered_map< string, CPtr<CObjectBase> > &hash, CObjectBase *pObject, string *pName )
{
	ASSERT( pName != 0 && pObject != 0 );
	if ( pName == 0 || pObject == 0 )
		return false;
	//
	for ( unordered_map< string, CPtr<CObjectBase> >::const_iterator i = hash.begin(); i != hash.end(); ++i )
	{
		if ( i->second == pObject )
		{
			*pName = i->first;
			return true;
		}
	}
	//
	*pName = "";
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::GetObjectName( CObjectServerBase *pObject, string *pName  ) const
{
	return GetNameFromHash( nameToObj, CastToObjectBase( pObject ), pName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::GetUnitName( CUnitServer *pUnit, string *pName ) const
{
	return GetNameFromHash( nameToObj, CastToObjectBase( pUnit ), pName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::GetItemName( CDFrozenItem *pItem, string *pName ) const
{
	return GetNameFromHash( nameToObj, CastToObjectBase( pItem ), pName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectServerBase* CWorld::GetObjectByName( const string &szName )
{
	string szUpperName;
	MakeUpperName( szName, &szUpperName );
	return CDynamicCast<CObjectServerBase>( nameToObj[ szUpperName ] ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDFrozenItem* CWorld::GetItemByName( const string &szName )
{
	string szUpperName;
	MakeUpperName( szName, &szUpperName );
	return CDynamicCast<CDFrozenItem>( nameToObj[ szUpperName ] ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalDiplomacy* CWorld::GetDiplomacy() const
{
	return pDiplomacy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CWorld::GetDiplomacyState( CUnit *pUnit, IPlayer *pPlayer ) const
{
	if ( !IsValid(pPlayer) )
		return NDb::DS_ENEMY;
	CDynamicCast<NRPG::IUnitMission> pUM(pUnit->GetRPG());
	if (pUM)
		return pUM->GetDiplomacy().GetDiplomacyState( pPlayer->GetScenarioPlayerID() );
	else
		return NDb::DS_ENEMY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CWorld::GetDiplomacyState( IPlayer *pPlayer1, IPlayer *pPlayer2 ) const
{
	return GetDiplomacy()->GetDiplomacyState( pPlayer1->GetScenarioPlayerID(), pPlayer2->GetScenarioPlayerID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemoveUnitFromAI( CUnitServer *pUS )
{
	vector<CPtr<CPlayer> > players;
	GetPlayersList( &players );
	for ( vector<CPtr<CPlayer> >::const_iterator i = players.begin(); i != players.end(); ++i )
	{
		CDynamicCast<NAI::CAICommander> pCommander((*i)->GetCommander());
		if (pCommander)
			pCommander->RemoveUnit( pUS );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::ChangeUnitPlayer( CUnitServer *pUnit, CPlayer *pPlayer )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pPlayer ) );
	if ( !IsValid( pUnit ) || !IsValid( pPlayer ) || pUnit->GetPlayer() == pPlayer )
		return;
	//
	CDynamicCast<CPlayer> pOldPlayer( pUnit->GetPlayer() );
	if ( !IsValid( pOldPlayer ) )
		return;
	//
	pUnit->Do( new CCmdCancel() );
	//
	MakeUnitInactive( pUnit );
	RemoveUnitFromAI( pUnit );
	// ������� ������ �����, ����� pUnit ������ �� IsValid
	pPlayer->AddUnit( pUnit );
	pOldPlayer->RemoveUnit( pUnit );
	//
	pUnit->SetPlayer( pPlayer );
	pUnit->GetUnitRPG()->SetDiplomacy( GetDiplomacy()->GetPlayerDiplomacy( pPlayer->GetScenarioPlayerID() ) );
	OnUnitAdded( pUnit );
	MakeUnitActive( pUnit, pPlayer );
	UpdateVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::PlaceUnitInPocket( CUnitServer *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( !IsUnitInPocket( pUnit ) );
	if ( IsValid( pUnit ) && !IsUnitInPocket( pUnit ) )
		pocket.push_back( CWorld::SUnitPtrHolder( pUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWorld::RemoveUnitFromPocket( CUnitServer *pUnit )
{
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsUnitInPocket( pUnit ) );
	for ( vector<CWorld::SUnitPtrHolder>::iterator i = pocket.begin(); i != pocket.end(); )
	{
		if ( (*i).pCObjHolder.GetPtr() == pUnit )
			i = pocket.erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWorld::IsUnitInPocket( CUnitServer *pUnit ) const
{
	ASSERT( IsValid( pUnit ) );
	for ( vector<CWorld::SUnitPtrHolder>::const_iterator i = pocket.begin(); i != pocket.end(); ++i )
	{
		if ( (*i).pCObjHolder.GetPtr() == pUnit )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NWorld

REGISTER_SAVELOAD_CLASS_NM( 0x0251101b, CWorld, NWorld );
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x02731131, CPlayer )
BASIC_REGISTER_CLASS( CPostWorldCreateInfo )
