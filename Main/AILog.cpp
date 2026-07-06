#include "StdAfx.h"

#include "aiLog.h"
#include "aiUnit.h"
#include "aiWeapon.h"
#include "aiInventory.h"

#include "wInterface.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"

#include "RPGUnitMission.h"
#include "RPGItemInfo.h"
#include "RPGItem.h"
#include "RPGItemSet.h"

#include "..\DBFormat\DataConst.h"
#include "..\DBFormat\DataRPG.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CAILogContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogContainer: public IAILogContainer
{
	OBJECT_BASIC_METHODS(CAILogContainer)
	ZDATA
	list< CObj<IAILogRecord> > Records;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&Records); return 0; }
public:
	CAILogContainer() {}
	// IAILogContainer
	virtual void Add( IAILogRecord *pAILogRecord, bool bCommit = false );
	virtual void Add( IAILogContainer *pAILogContainer );
	virtual list< CObj<IAILogRecord> > *GetLogRecords();
	// IAILogRecord
	virtual void RollBack();
	virtual void Commit();
	virtual void GetCommands( list< CPtr<NWorld::CCommand> > *Commands );
	virtual void Clear();
	virtual bool IsEmpty();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::Add( IAILogRecord *pAILogRecord, bool bCommit )
{
	Records.push_back(pAILogRecord); 
	if ( bCommit )
		pAILogRecord->Commit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::Add( IAILogContainer *pAILogContainer )
{
	list< CObj<IAILogRecord> > *pRecords = pAILogContainer->GetLogRecords();
	for ( list< CObj<IAILogRecord> >::iterator i = pRecords->begin(); i != pRecords->end(); ++i )
		Add( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
list< CObj<IAILogRecord> > *CAILogContainer::GetLogRecords()
{
	return &Records;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::RollBack() 
{  
	for ( list< CObj<IAILogRecord> >::reverse_iterator i = Records.rbegin(); i != Records.rend(); ++i )
		(*i)->RollBack();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::Commit()
{ 
	for ( list< CObj<IAILogRecord> >::iterator i = Records.begin(); i != Records.end(); ++i ) 
		(*i)->Commit(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	for ( list< CObj<IAILogRecord> >::iterator i = Records.begin(); i != Records.end(); ++i ) 
		(*i)->GetCommands( Commands ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogContainer::Clear()
{
	Records.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogContainer::IsEmpty() 
{ 
	return Records.empty(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogPosition::CAILogPosition( IAIUnit *_pAIUnit, SPosition _pSourcePosition, SPosition _pTargetPosition, NAI::EPose _pose ):
	CAILogRecord(_pAIUnit), pSourcePosition(_pSourcePosition), pTargetPosition(_pTargetPosition), pose( _pose )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPosition::RollBack() 
{ 
	pAIUnit->SetPosition( pSourcePosition ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPosition::Commit()
{
	pAIUnit->SetPosition( pTargetPosition ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPosition::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{ 
	NWorld::CCmd *pCmd = new NWorld::CCmdWishPose( pose );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
	pCmd = new NWorld::CCmdPath( pTargetPosition, NAI::PF_USE_POSE );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogShot
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogShot::CAILogShot(	IAIUnit *_pAIUnit, IAIUnit *_pTarget, NAI::EHitLocation _eHitLocation ) 
	: CAILogRecord(_pAIUnit), pTarget(_pTarget), eHitLocation(_eHitLocation) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogShot::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	NWorld::CCmd *pCmd = new NWorld::CCmdShootObject( pTarget->GetUnitServer(), 0, eHitLocation );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogMelee (release-new; retail id 0x52533166). Mirror of CAILogShot carrying the melee weapon for save
// fidelity; GetCommands @0x460d20 emits the identical CCmdShootObject against the enemy (no HL_ANY pin, no
// validity guard -- exactly the CAILogShot shape, over pEnemy instead of pTarget).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogMelee::CAILogMelee(	IAIUnit *_pAIUnit, IAIUnit *_pEnemy, CAIMeleeWeapon *_pWeapon, NAI::EHitLocation _eHitLocation )
	: CAILogRecord(_pAIUnit), pEnemy(_pEnemy), pWeapon(_pWeapon), eHitLocation(_eHitLocation)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogMelee::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	NWorld::CCmd *pCmd = new NWorld::CCmdShootObject( pEnemy->GetUnitServer(), 0, eHitLocation );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogShotPoint
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogShotPoint::CAILogShotPoint( IAIUnit *_pAIUnit, CVec3 _ptTarget ):
	CAILogRecord( _pAIUnit ), ptTarget( _ptTarget )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogShotPoint::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	NWorld::CCmd *pCmd = new NWorld::CCmdShootTile( ptTarget );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogHeal
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogHeal::CAILogHeal( IAIUnit *_pAIUnit, IAIUnit *_pPatient ):
	CAILogRecord( _pAIUnit ), pPatient( _pPatient )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogHeal::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pPatient ) )
		return;
	NWorld::CCmd *pCmd = new NWorld::CCmdHeal( pPatient->GetUnitServer() );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogLeavePK - emit a CCmdExitPK on the unit server, but only when the recorded unit is still
// validly seated in a live panzerklein at replay time (decode guard chain @0x61400: unit alive ->
// owned unit-server alive -> GetWearingDBPK record alive). Faithful to s2_cailogleavepk.h, expressed
// in the in-tree record idiom (the inner CCmd is wrapped in a CCmdSetCommand on the unit server).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogLeavePK::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) )
		return;
	NWorld::CUnitServer *pUS = pAIUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	if ( !IsValid( pUS->GetWearingDBPK() ) )      // still wearing a live PK?
		return;
	Commands->push_back( new NWorld::CCmdSetCommand( pUS, new NWorld::CCmdExitPK() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogWearPK - replay as a CCmdTakeCorpse aimed at the chosen suit's embedded CUnit ("mount this
// body"); the suit's CUnit subobject is the corpse (CUnitServer IS-A NWorld::CUnit). Faithful to
// s2_cailogwearpk.h (GetWorldCommands @0x61110), in the in-tree CCmdSetCommand idiom.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogWearPK::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) || !IsValid( pPK ) )
		return;
	NWorld::CCmd *pCmd = new NWorld::CCmdTakeCorpse( (NWorld::CUnit*)pPK.GetPtr() );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogUseCannon - man the cannon: normalise the pose to RUN, then issue the cannon (enter) command.
void CAILogUseCannon::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) || !IsValid( pCannon ) )
		return;
	CDynamicCast<NWorld::IObject> pObject( pCannon.GetPtr() );
	if ( !pObject )
		return;
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), new NWorld::CCmdWishPose( NAI::RUN ) ) );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), new NWorld::CCmdCannon( pObject ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogExitCannon - abandon the cannon.
void CAILogExitCannon::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) || !IsValid( pCannon ) )
		return;
	CDynamicCast<NWorld::IObject> pObject( pCannon.GetPtr() );
	if ( !pObject )
		return;
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), new NWorld::CCmdExitCannon( pObject ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogReloadWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogReloadWeapon::CAILogReloadWeapon(	IAIUnit *_pAIUnit, CAIFireArmsWeapon *_pWeapon ):
	CAILogRecord( _pAIUnit ), pWeapon( _pWeapon )
{
	ASSERT( IsValid( pWeapon ) );
	if (IsValid( pWeapon ) )
	{
		pNewClip = pWeapon->GetNextClip();
		pOldClip = pWeapon->GetCurrentClip();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogReloadWeapon::RollBack()
{
	pWeapon->SetCurrentClip( pOldClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogReloadWeapon::Commit()
{
	pWeapon->RemoveClip( pNewClip );
	pWeapon->SetCurrentClip( pNewClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogReloadWeapon::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(),
		new NWorld::CCmdReload() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendAmmo
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogSpendAmmo::CAILogSpendAmmo(	CAIFireArmsWeaponClip *_pClip, int nSpendAmmo ):
	CAILogRecord( 0 ), pClip( _pClip )
{
	nOldAmmo = pClip->GetAmmoCount();
	nNewAmmo = max( 0, nOldAmmo - nSpendAmmo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAmmo::RollBack()
{
	pClip->SetAmmoCount( nOldAmmo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAmmo::Commit()
{
	pClip->SetAmmoCount( nNewAmmo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAmmo::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendAP
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogSpendAP::CAILogSpendAP(	IAIUnit *_pAIUnit, int nSpendAP ): CAILogRecord(_pAIUnit)
{
	pAIUnit->GetAP( &nOldAP, &nMaxAP );
	nNewAP = max( 0, nOldAP - nSpendAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAP::RollBack()
{
	pAIUnit->SetAP( nOldAP, nMaxAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAP::Commit()
{
	pAIUnit->SetAP( nNewAP, nMaxAP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendAP::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogSpendHP
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogSpendHP::CAILogSpendHP(	IAIUnit *_pAIUnit, int nSpendHP ): CAILogRecord(_pAIUnit)
{
	pAIUnit->GetHP( &nOldHP, &nMaxHP );
	nNewHP = nOldHP - nSpendHP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendHP::RollBack()
{
	pAIUnit->SetHP( nOldHP, nMaxHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendHP::Commit()
{
	pAIUnit->SetHP( nNewHP, nMaxHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogSpendHP::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogHurt
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogHurt::CAILogHurt(	IAIUnit *_pAIUnit, int nHurtHP ): CAILogRecord(_pAIUnit)
{
	nOldHurtHP = pAIUnit->GetHurtHP();
	nNewHurtHP = nOldHurtHP + nHurtHP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogHurt::RollBack()
{
	pAIUnit->SetHurtHP( nOldHurtHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogHurt::Commit()
{
	pAIUnit->SetHurtHP( nNewHurtHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogHurt::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogChangeWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogChangeWeapon::CAILogChangeWeapon(	IAIUnit *_pAIUnit, IAIInventoryItem *_pNewWeapon ):
	CAILogRecord( _pAIUnit ), pNewWeapon( _pNewWeapon )
{
	ASSERT( IsValid( pNewWeapon ) );
	pOldWeapon = pAIUnit->GetAIInventory()->GetCurrentItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeWeapon::RollBack()
{
	pAIUnit->GetAIInventory()->SetCurrentItem( pOldWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeWeapon::Commit()
{
	pAIUnit->GetAIInventory()->SetCurrentItem( pNewWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeWeapon::GetItemPosition( NRPG::IInventoryItem *pItem, CTPoint<int> *Position )
{
	const vector<NRPG::SBackPackItem> &Items = pAIUnit->GetUnitServer()->GetUnitRPG()->GetInventory()->GetItems();
	for ( vector<NRPG::SBackPackItem>::const_iterator i = Items.begin(); i != Items.end(); ++i )	
		if ( (*i).pItem == pItem )
		{
			*Position = (*i).sPos;
			return;
		}
	//
	ASSERT( 0 ); // � inventory �� ��������� �������� item-�
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeWeapon::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	NWorld::CUnitServer *pUnitServer = pAIUnit->GetUnitServer();
	NRPG::IInventory *pInventory = pUnitServer->GetUnitRPG()->GetInventory();
	// ������� ������ � BackPack
	if ( IsValid( pOldWeapon ) )
	{
		CPtr<NRPG::IInventoryItem> pOldItem = pOldWeapon->GetInventoryItem();
		ASSERT( IsValid( pOldItem ) );
		if ( IsValid( pOldItem ) )
		{
			NWorld::SItem From( pUnitServer, NWorld::SItem::SLOT, NDb::SLOT_1, pOldItem );
			NWorld::SItem To( pUnitServer, NWorld::SItem::BACKPACK, CTPoint<int>( -1, -1 ), pOldItem );
			Commands->push_back( new NWorld::CCmdSetCommand( pUnitServer,
				new NWorld::CCmdMoveInventoryItem( From, To ) ) );
		}
	}
	// ���������� ����� ������ � Slot
	if ( IsValid( pNewWeapon ) )
	{
		CPtr<NRPG::IInventoryItem> pNewItem = pNewWeapon->GetInventoryItem();
		// retail CAILogChangeWeapon::GetWorldCommands @0x5f010 gates on alive(newItem) -- the UNDERLYING NWorld item,
		// NOT the AI weapon wrapper. The wrapper (pNewWeapon) can stay valid while its item is stale/freed (e.g. a
		// just-thrown grenade the wrapper still references after TearOffItem). The prior IsValid(pNewWeapon) here was a
		// copy/paste from the old-weapon block above (which correctly checks pOldItem); it equipped that DB-less item
		// into SLOT_1, crashing CExecMoveInventoryItem::AnimationFinished (wUnitAttackExec.cpp:2688) on its null
		// GetDBItem()->subType. Guard the ITEM -> skip the draw when it isn't alive, exactly as retail does.
		if ( IsValid( pNewItem ) )
		{
			Commands->push_back( new NWorld::CCmdSetCommand( pUnitServer,
				new NWorld::CCmdArrangeInventory() ) );
			NWorld::SItem From( pUnitServer, NWorld::SItem::BACKPACK, CTPoint<int>( -1, -1 ), pNewItem );
			NWorld::SItem To( pUnitServer, NWorld::SItem::SLOT, NDb::SLOT_1, pNewItem );
			Commands->push_back( new NWorld::CCmdSetCommand( pUnitServer,
				new NWorld::CCmdMoveInventoryItem( From, To ) ) );
			Commands->push_back( new NWorld::CCmdSetCommand( pUnitServer,
				new NWorld::CCmdSetActiveItem( NDb::SLOT_1 ) ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogAddWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogAddWeapon::CAILogAddWeapon(	IAIUnit *_pAIUnit, CAIFireArmsWeapon *_pWeapon ):
	CAILogRecord( _pAIUnit ), pWeapon( _pWeapon )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeapon::RollBack()
{
	//pAIUnit->GetAIInventory()->RemoveWeapon( pWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeapon::Commit()
{
	//pAIUnit->GetAIInventory()->AddWeapon( pWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeapon::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogAddWeaponClip
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogAddWeaponClip::CAILogAddWeaponClip(	IAIUnit *_pAIUnit, 
	CAIFireArmsWeapon *_pWeapon, CAIFireArmsWeaponClip *_pClip ): CAILogRecord( _pAIUnit ), pWeapon( _pWeapon ), pClip( _pClip )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeaponClip::RollBack()
{
	pWeapon->RemoveClip( pClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeaponClip::Commit()
{
	pWeapon->AddClip( pClip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogAddWeaponClip::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogPickUpItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogPickUpItem::CAILogPickUpItem(	IAIUnit *_pAIUnit, NRPG::IInventoryItem *_pItem ): 
	CAILogRecord( _pAIUnit ), pItem( _pItem )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPickUpItem::RollBack() {}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPickUpItem::Commit() {}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogPickUpItem::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	CPtr<NWorld::CUnitServer> pUnitServer = pAIUnit->GetUnitServer();
	NRPG::IInventory *pInventory = pUnitServer->GetUnitRPG()->GetInventory();
	NWorld::SItem From( pUnitServer, NWorld::SItem::GROUND );
	From.pItem = pItem;
	From.pUnit = pUnitServer;
	CTPoint<int> Position;
	pInventory->FindPlace( pItem, &Position );
	NWorld::SItem To( pUnitServer, NWorld::SItem::BACKPACK, Position, pItem );
	To.pUnit = pUnitServer;
	//
	Commands->push_back( new NWorld::CCmdSetCommand( pUnitServer,
		new NWorld::CCmdMoveInventoryItem( From, To ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogDropItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogDropItem::CAILogDropItem(	IAIUnit *_pAIUnit, NRPG::IInventoryItem *_pItem ): 
	CAILogRecord( _pAIUnit ), pItem( _pItem )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogDropItem::RollBack() {}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogDropItem::Commit() {}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogDropItem::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogThrowGrenade
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogThrowGrenade::CAILogThrowGrenade(	IAIUnit *_pUnit, CVec3 _ptTarget, CAIGrenadeWeapon *_pGrenade ) 
	: CAILogRecord(_pUnit), ptTarget( _ptTarget ), pGrenade( _pGrenade )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowGrenade::RollBack() 
{
	CPtr<CAIInventory> pInventory = pAIUnit->GetAIInventory();
	pInventory->AddGrenade( pGrenade );
	pInventory->SetCurrentItem( pGrenade );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowGrenade::Commit() 
{
	CPtr<CAIInventory> pInventory = pAIUnit->GetAIInventory();
	pInventory->RemoveGrenade( pGrenade );
	pInventory->SetCurrentItem( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowGrenade::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	NWorld::CCmd *pCmd = new NWorld::CCmdShootTile( ptTarget );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogThrowKnife - retail NAI::CAILogThrowKnife (release-new). Value ctor @0x45c770; Commit == retail
// ModifyState @0x45c7f0 (knife leaves inventory, hand empties); GetCommands == GetWorldCommands @0x460c30
// (CCmdShootObject vs the enemy, eHL pinned HL_ANY). RollBack is the a5dll-only inverse of Commit (mirrors
// CAILogThrowGrenade::RollBack) so the planner's look-ahead can undo a speculative commit.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogThrowKnife::CAILogThrowKnife( IAIUnit *_pAIUnit, IAIUnit *_pEnemy, CAIThrowingWeapon *_pWeapon, NAI::EHitLocation _eHitLocation )
	: CAILogRecord( _pAIUnit ), pEnemy( _pEnemy ), pWeapon( _pWeapon ), eHitLocation( _eHitLocation )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowKnife::RollBack()
{
	CPtr<CAIInventory> pInventory = pAIUnit->GetAIInventory();
	pInventory->AddThrowingWeapon( pWeapon );
	pInventory->SetCurrentItem( pWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowKnife::Commit()
{
	// retail ModifyState @0x45c7f0: RemoveItem(pWeapon) + SetCurrentItem(0). The a5dll CAIInventory keeps the
	// knife in its typed throwingWeapons vector, so the generic retail RemoveItem maps to RemoveThrowingWeapon
	// (mirrors CAILogThrowGrenade::Commit -> RemoveGrenade). The record's own pWeapon CPtr keeps the knife alive.
	CPtr<CAIInventory> pInventory = pAIUnit->GetAIInventory();
	pInventory->RemoveThrowingWeapon( pWeapon );
	pInventory->SetCurrentItem( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogThrowKnife::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	// @0x460c30 -- CCmdShootObject aimed at the enemy, eHL PINNED to HL_ANY (-1): a knife throw is not aimed at
	// a body zone (the decode stores -1 outright, ignoring the recorded eHitLocation, which is kept only for
	// save/load fidelity). Same shape as CAILogShot / CAILogBeginSnipe.
	NWorld::CCmd *pCmd = new NWorld::CCmdShootObject( pEnemy->GetUnitServer(), 0, HL_ANY );
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogExpediency
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogExpediency::CAILogExpediency(	IAIUnit *_pAIUnit, int nExpediency ):
	CAILogRecord( _pAIUnit )
{
	nOldExpediency = pAIUnit->GetAdditionalExpediency();
	nNewExpediency = nOldExpediency + nExpediency;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogExpediency::RollBack()
{
	pAIUnit->SetAdditionalExpediency( nOldExpediency );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogExpediency::Commit()
{
	pAIUnit->SetAdditionalExpediency( nNewExpediency );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogExpediency::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogChangeShootMode
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogChangeShootMode::CAILogChangeShootMode( IAIUnit *_pAIUnit, CAIFireArmsWeapon *pWeapon, NDb::EShootMode _eShootMode ):
	CAILogRecord( _pAIUnit )
{
	ASSERT( IsValid( pWeapon ) );
	if ( IsValid( pWeapon ) )
		pWeaponItem = pWeapon->GetItem();
	ASSERT( IsValid( pWeaponItem ) );
	if ( IsValid( pWeaponItem ) )
	{
		eNewShootMode = _eShootMode;
		eOldShootMode = pWeaponItem->GetShootMode();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeShootMode::RollBack()
{
	pWeaponItem->SetShootMode( eOldShootMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeShootMode::Commit()
{
	pWeaponItem->SetShootMode( eNewShootMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeShootMode::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(),
		new NWorld::CCmdShootMode( eNewShootMode ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogHide
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogHide::CAILogHide( IAIUnit *_pAIUnit ): CAILogRecord( _pAIUnit )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogHide::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	Commands->push_back( 
		new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), new NWorld::CCmdHide() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogChangeMaxToHit
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogChangeMaxToHit::CAILogChangeMaxToHit( IAIUnit *_pAIUnit, int nMaxToHit ): 
	CAILogRecord( _pAIUnit )
{
	nNewToHit = nMaxToHit;
	nOldToHit = pAIUnit->GetMaxToHit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeMaxToHit::RollBack()
{
	pAIUnit->SetMaxToHit( nOldToHit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeMaxToHit::Commit()
{
	pAIUnit->SetMaxToHit( nNewToHit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogChangeMaxToHit::GetCommands( list< CPtr<NWorld::CCommand> > *Commands ) {}
////////////////////////////////////////////////////////////////////////////////////////////////////

/*
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogUseCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogUseCannon::CAILogUseCannon(	IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon ) 
	:	CAILogRecord(_pAIUnit), pCannon(_pCannon) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogUseCannon::Commit( list< CPtr<NWorld::CCommand> > *Commands )
{
	CDynamicCast<NWorld::IObject> pObject(pCannon);
	if ( pObject )
	{
		NWorld::CCmd *pCmd = new NWorld::CCmdWishPose( NAI::RUN );
		if ( pCmd )
			Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );

		pCmd = new NWorld::CCmdCannon( pObject );
		if ( pCmd )
			Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogExitCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogExitCannon::CAILogExitCannon(	IAIUnit *_pAIUnit, NWorld::CCannon *_pCannon ) 
	: CAILogRecord(_pAIUnit), pCannon(_pCannon) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogExitCannon::Commit( list< CPtr<NWorld::CCommand> > *Commands )
{
	CDynamicCast<NWorld::IObject> pObject(pCannon);
	if ( pObject )
	{
		NWorld::CCmd *pCmd = new NWorld::CCmdExitCannon( pObject );
		if ( pCmd )
			Commands->push_back( new NWorld::CCmdSetCommand( pAIUnit->GetUnitServer(), pCmd ) );
	}
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// Snipe-state-machine plan records (release-new). See AILog.h for the per-record notes; faithful to the
// matched-release decode (decomp/src/s2_cailogmelee.h, s2_cailogcollectsnipeap.h, s2_cailogcancelaction.h).
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogBeginSnipe::CAILogBeginSnipe(	IAIUnit *_pAIUnit, IAIUnit *_pEnemy, NAI::EHitLocation _eHitLocation )
	: CAILogRecord( _pAIUnit ), pEnemy( _pEnemy ), eHitLocation( _eHitLocation )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogBeginSnipe::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) || !IsValid( pEnemy ) )
		return;
	NWorld::CUnitServer *pUS = pAIUnit->GetUnitServer();
	if ( !IsValid( pUS ) || !IsValid( pEnemy->GetUnitServer() ) )
		return;
	NWorld::CCmd *pCmd = new NWorld::CCmdShootObject( pEnemy->GetUnitServer(), 0, eHitLocation );
	Commands->push_back( new NWorld::CCmdSetCommand( pUS, pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogCollectSnipeAP::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) )
		return;
	NWorld::CUnitServer *pUS = pAIUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// Release emits CCmdCollectSnipeAP{ CSAP_PRECISE, nAP } (an exact AP amount); the dev command carries only
	// the ECollectSnipeAP mode (no nAP / no CSAP_PRECISE), so the closest faithful mode is CSAP_ALL (collect all
	// currently available AP). The exact-nAP refinement is a documented release divergence; the computed nAP is
	// still carried on this record for save/load fidelity.
	NWorld::CCmd *pCmd = new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_ALL );
	Commands->push_back( new NWorld::CCmdSetCommand( pUS, pCmd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogCancelAction::GetCommands( list< CPtr<NWorld::CCommand> > *Commands )
{
	if ( !IsValid( pAIUnit ) )
		return;
	NWorld::CUnitServer *pUS = pAIUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return;
	// CCmdCancel is itself a top-level interface command (CCmdUnit), so it is pushed directly -- not wrapped in
	// CCmdSetCommand like the unit-server CCmd records above. (Release names this command CCmdUnitCancelAction.)
	Commands->push_back( new NWorld::CCmdCancel( pUS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAILogContainer *CreateAILogContainer()
{
	return new CAILogContainer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822121, CAILogContainer );
REGISTER_SAVELOAD_CLASS( 0x52822127, CAILogShot );
REGISTER_SAVELOAD_CLASS( 0x52533166, CAILogMelee );
REGISTER_SAVELOAD_CLASS( 0x52822122, CAILogPosition );
REGISTER_SAVELOAD_CLASS( 0x52822125, CAILogSpendAP );
REGISTER_SAVELOAD_CLASS( 0x52822126, CAILogSpendHP );
REGISTER_SAVELOAD_CLASS( 0x50732171, CAILogChangeWeapon );
REGISTER_SAVELOAD_CLASS( 0x50732170, CAILogReloadWeapon );
REGISTER_SAVELOAD_CLASS( 0x51362142, CAILogAddWeapon );
REGISTER_SAVELOAD_CLASS( 0x51362143, CAILogAddWeaponClip );
REGISTER_SAVELOAD_CLASS( 0x51362144, CAILogPickUpItem );
REGISTER_SAVELOAD_CLASS( 0x51362145, CAILogDropItem );
REGISTER_SAVELOAD_CLASS( 0x51362146, CAILogSpendAmmo );
REGISTER_SAVELOAD_CLASS( 0x51362147, CAILogHurt );
REGISTER_SAVELOAD_CLASS( 0x50732172, CAILogThrowGrenade );
REGISTER_SAVELOAD_CLASS( 0x52533165, CAILogThrowKnife );
REGISTER_SAVELOAD_CLASS( 0x50172150, CAILogExpediency );
REGISTER_SAVELOAD_CLASS( 0x50872131, CAILogChangeShootMode );
REGISTER_SAVELOAD_CLASS( 0x50972130, CAILogChangeMaxToHit );
REGISTER_SAVELOAD_CLASS( 0x52612110, CAILogHide );
REGISTER_SAVELOAD_CLASS( 0x50442130, CAILogUseCannon );
REGISTER_SAVELOAD_CLASS( 0x50442131, CAILogExitCannon );
REGISTER_SAVELOAD_CLASS( 0x51413190, CAILogShotPoint );
REGISTER_SAVELOAD_CLASS( 0x51413191, CAILogHeal );
REGISTER_SAVELOAD_CLASS( 0x23072480, CAILogLeavePK );
REGISTER_SAVELOAD_CLASS( 0x23069ac1, CAILogWearPK );
REGISTER_SAVELOAD_CLASS( 0x52253090, CAILogBeginSnipe );
REGISTER_SAVELOAD_CLASS( 0x52253091, CAILogCollectSnipeAP );
REGISTER_SAVELOAD_CLASS( 0x52353090, CAILogCancelAction );