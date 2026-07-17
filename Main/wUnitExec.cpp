#include "StdAfx.h"
#include "wUnitExec.h"
#include "wUnitMove.h"
#include "wUnitAttack.h"
#include "wUnitServer.h"
#include "wMain.h"
#include "wMainPath.h"
#include "wMisc.h"
#include "RPGItem.h"
#include "RPGPerk.h"
#include "RPGUnitMission.h"
#include "rpgCheatConstants.h"
#include "wObject.h"
#include "wAckBase.h"
#include "..\misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataPerk.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAI.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
#include "wUnitAttackExec.h"
#include "rpgCheatConstants.h"
#include "wUnitStates.h"
#include "rpgUnit.h"
#include "wUnitQueue.h"
namespace NWorld
{
/*
// TEST{ for testing purposes only - do not remove
const int N_TEST_MODELS = 8;
//static int nTestModelIDs[N_TEST_MODELS] = { 433, 467, 466 };
//static int nTestModelIDs[N_TEST_MODELS] = { 94, 95, 63, 468, 469, 470 };
//static int nTestModelIDs[N_TEST_MODELS] = { 64, 67, 65 };
static int nTestModelIDs[N_TEST_MODELS] = { 467, 433, 963, 958, 981, 982, 64, 67 };
//static int nTestModelIDs[N_TEST_MODELS] = { 433 };
//static int nTestModelIDs[N_TEST_MODELS] = { 963, 958 };
static int nTestModel = 0;
// TEST}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecReload
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecReload: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecReload);
private:
	ZDATA_(CCommandExecute)
	CPtr<NRPG::IInventoryItem> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pItem); return 0; }

public:
	CExecReload( CUnitServer *_pUS = 0, int nSlot = -1 ):
		CCommandExecute(_pUS)
	{
		if ( !IsValid( pUS ) )
			return;   // saveload path: operator& restores pItem
		// retail CExecReloadWeapon ctor @0x394710: -1 = active weapon, else that slot's item
		if ( nSlot == -1 )
			pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
		else
			pItem = pUS->GetUnitRPG()->GetInventory()->Get( NDb::ESlot( nSlot ) );
	}
	int GetStartAP() const { return pUS->GetActionAP( NRPG::AC_RELOAD ); }
	int GetActionAP() const { return pUS->GetActionAP( NRPG::AC_RELOAD ); }
	virtual void Run()
	{
		/*
		// TEST{ for testing purposes only - do not remove
		for ( int i = 0; i < 16; ++i )
		{
			pUS->GetWorld()->AddDebris( NDb::GetModel( nTestModelIDs[nTestModel] ), pUS->GetWorld()->GetAIMap(),
				pUS->GetPosition().GetCP() + CVec3(0,0,1), QNULL, CVec3(1,0,20), pUS->GetWorld()->GetTime(), false, 0, 0, -2 );
			nTestModel = (nTestModel + 1) % N_TEST_MODELS;
		}
		// TEST}
		return;
		*/
		pUS->DoAction( NRPG::AC_RELOAD );
		pUS->animator.Reload( pUS->GetPosition() );
		StartAction( pUS->GetWorld(), SKIPPABLE );
	}
	virtual bool TimeLabelReached()
	{
		//return false;
		pUS->GetUnitRPG()->Reload();
		CDynamicCast<NRPG::IWeaponItem> pW(pItem);
		if (pW)
		{
			NDb::CSound *pSound = pW->GetDBWeapon()->pSoundReload;
			NDb::SAISound sound = { NDb::GetAISound( 26 ), 0, 1.0f };   // retail @0x394b70: no silencer on reload
			pUS->GetWorld()->MakeAISound( sound, pUS, pSound );
		}
		pUS->Update();
		return false;
	}
	EUnitCommandResult CanDoIt()
	{
		CDynamicCast<NRPG::IWeaponItem> pW(pItem);
		if (pW)
		{
			if ( !pW->CanReload( pUS->GetUnitRPG()->GetInventory() ) )
				return UCR_NO_EQUIPMENT;

			return UCR_OK;
		}

		// retail CExecReloadWeapon::CanDoIt @0x394970: no weapon resolved -> NO_EQUIPMENT
		return UCR_NO_EQUIPMENT;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecCriticalLostWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecCriticalLostWeapon: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecCriticalLostWeapon);
	ZDATA_(CCommandExecute)
	bool bOnlyTwoHanded;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bOnlyTwoHanded); return 0; }
public:
	CExecCriticalLostWeapon() {}
	CExecCriticalLostWeapon( CUnitServer *_pUS, bool _bOnlyTwoHanded ): CCommandExecute(_pUS), bOnlyTwoHanded(_bOnlyTwoHanded) {}
	virtual void Run()
	{
		Finished();
		if ( pUS->animator.GetCannon() )
		{
			ASSERT( 0 ); // this should never happen, state cannon should be dropped on this critical
		}
		else
		{
			NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
			NRPG::IInventory *pInventory = pRPG->GetInventory();
			bool bTwoHanded = false;
			CDynamicCast<NRPG::IWeaponItem> pW(pInventory->GetActive());
			if (pW)
				bTwoHanded = pW->GetDBWeapon()->pWeaponType->bTwoHanded;
			if ( bOnlyTwoHanded && !bTwoHanded )
				return;
			CUnitServer::SResItem item;
			CVec3 velocity = VNULL3;
			if ( pUS->TearOffItem( &item, (NDb::ESlot)pInventory->GetActiveSlot() ) )
			{
				if ( !bTwoHanded )
				{
					CVec3 v( 0, 0, 1 );
					item.q.Rotate( &v, v );
					velocity = 2 * v;
				}
				// retail @0x7b5dfc: bFallFromBody=true (critical knocks the weapon off the body),
				// the unit as visibility parent, the unit's floor (GetFloor @0x7b5df5)
				LaunchItem( pUS->GetWorld(), item, velocity, true, (CObjectBase*)pUS, pUS->GetFloor() );
				pUS->animator.SetWeaponAnimation( NDb::WT_DEFAULT );
				pUS->animator.SetActiveItem( false );
				pUS->animator.PlaceUnit( pUS->GetPosition() );
				pUS->Update();
			}
		}
	}
};
CCommandExecute* CreateLostWeapon( CUnitServer *pUS, bool bOnlyTwoHanded )
{
	return new CExecCriticalLostWeapon( pUS, bOnlyTwoHanded );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecAccidentalShot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecAccidentalShot: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecAccidentalShot);
public:
	CExecAccidentalShot( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
		Finished();
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		if ( pUS->animator.GetCannon() )
		{
			ASSERT( 0 ); // this should never happen, state cannon should be dropped on this critical
		}
		else
		{
			CRay ray;
			if ( !pUS->GetBarrelDir( &ray ) )
				return;
			//
			vector<NRPG::CAttackPortion> attack;
			pRPG->CreateAttack( &attack, true );
			if ( attack.empty() )
				return;
			//
			vector<NRPG::IAttackable *> ignores;
			CPtr<CCannon> pCannon = pUS->animator.GetCannon();
			if ( pCannon )
				ignores.push_back( pCannon );
			else
				ignores.push_back( pUS );
			//
			CPtr<NRPG::IWeaponItem> pWeaponItem = pUS->GetUnitRPG()->GetWeaponItem();
			if ( !IsValid( pWeaponItem ) )
				return;
			CDBPtr<NDb::CRPGWeapon> pWeapon = pWeaponItem->GetDBWeapon();
			//
			CPtr<NDb::CModel> pTrailEffect = 0;
			if ( pWeapon->pTrailEffect )
			{
				SRand sRand;
				pTrailEffect = pWeapon->pTrailEffect->CreateModel( &sRand );
			}
			//
			ray.ptOrigin += ray.ptDir * pUS->GetMinClearDistance();
			for ( vector<NRPG::CAttackPortion>::const_iterator i = attack.begin(); i != attack.end(); ++i )
				pUS->GetWorld()->PerformRangedAttack( *i, ray, ignores, 
					pUS->GetWorld()->GetTime()->GetValue(), pTrailEffect, pWeapon->fTrailSpeed );
			pUS->CreateFlash( false, false );   // @0x3b6d10 accidental shot -> right barrel, single sound
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateAccidentalShot( CUnitServer *pUS )
{
	CRay ray;
	if ( !pUS->GetBarrelDir( &ray ) )
		return 0;
	//
	if ( pUS->GetUnitRPG()->GetWeaponType() == NDb::WT_RLAUNCHER )
		return new CExecLaunchRocket( pUS, CExecLaunchRocket::ACCIDENTAL );
	else
		return new CExecAccidentalShot( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecStartCombat
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecStartCombat: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecStartCombat);
public:
	CExecStartCombat( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsExecStartCombat( CCommandExecute* pExec )
{
	CDynamicCast<CExecStartCombat> pCombat(pExec);
	if (pCombat)
		return true;
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::IsCancelableExec @0x392fe0 (oracle s2_createactionqueue.h, disasm-verified):
//   for (;;) {
//       if ( dynamic_cast<CExecCannon*>(pExec) ) return false;   // a cannon shot is not cancelable
//       CSimpleExecQueue *q = dynamic_cast<CSimpleExecQueue*>(pExec);
//       if ( !q ) return true;                                   // any non-queue non-cannon: cancelable
//       pExec = q->front-or-null;                                // descend into the queue head
//   }
// (a null head -- empty queue -- makes the next casts null and falls through to "true").
bool IsCancelableExec( CCommandExecute* pExec )
{
	for (;;)
	{
		CDynamicCast<CExecCannon> pCannon( pExec );
		if ( pCannon )
			return false;
		CDynamicCast<CSimpleExecQueue> pQueue( pExec );
		if ( !pQueue )
			return true;
		pExec = pQueue->GetFrontExecutor();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecExplode
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecExplode: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecExplode);
public:
	CExecExplode( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
		StartAction( pUS->GetWorld(), NORMAL );
		pUS->GetWorld()->Explode( pUS->GetPosition().GetCP(), 100 );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecShootMode
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecShootMode: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecShootMode);
private:
	ZDATA_(CCommandExecute)
	CObj<CCmdShootMode> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecShootMode() {}
	CExecShootMode( CUnitServer *_pUS, CCmdShootMode *_pCmd ): CCommandExecute(_pUS), pCmd( _pCmd ) {}
	virtual void Run()
	{
		CPtr<NRPG::IInventoryItem> pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
		ASSERT( IsValid( pItem ) );
		if ( IsValid( pItem ) )
		{
			CDynamicCast<NRPG::IWeaponItem> pWeapon(pItem);
			if (pWeapon)
				pWeapon->SetShootMode( pCmd->eMode );
		}

		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecGrenadeMode
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecGrenadeMode: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecGrenadeMode);
private:
	ZDATA_(CCommandExecute)
	CObj<CCmdGrenadeMode> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecGrenadeMode() {}
	CExecGrenadeMode( CUnitServer *_pUS, CCmdGrenadeMode *_pCmd ): CCommandExecute(_pUS), pCmd( _pCmd ) {}
	virtual void Run()
	{
		CPtr<NRPG::IInventoryItem> pItem = pUS->GetUnitRPG()->GetInventory()->GetActive();
		ASSERT( IsValid( pItem ) );
		if ( IsValid( pItem ) )
		{
			CDynamicCast<NRPG::IGrenadeItem> pGrenade(pItem);
			if (pGrenade)
				pGrenade->SetMode( pCmd->eMode );
		}

		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTeleport
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecTeleport: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecTeleport);
	ZDATA_(CCommandExecute)
	CObj<CCmdTeleport> pCmd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pCmd); return 0; }
public:
	CExecTeleport() {}
	CExecTeleport( CUnitServer *_pUS, CCmdTeleport *_p ): CCommandExecute(_pUS), pCmd(_p) {}
	virtual void Run()
	{
		CObj<CExecTeleport> pHold(this);
		bool bHasCheat = true;//pUS->IsCheatEnabled( NRPG::CHEAT_TELEPORT );
		ASSERT( bHasCheat );
		NAI::SUnitPosition pos = pCmd->pos;
		pos.pos.p.SetPose( NAI::CM_STAND );
		if ( bHasCheat && pos.pos.GetNetwork()->IsPassable( pos.pos.p ) )
		{
			pUS->animator.PlaceUnit( pos );
			pUS->SetPosition( pos );
		}
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecFly -- runs a CCmdFly: animate one fly-move from the unit's current position to dst, then on
// the move's completion snap the unit to dst. Retail NWorld::CExecFly (Run @0x3b2be0, AnimationFinished
// @0x3b5970). dst is a 3D/fly position (NAI::MakeFlyPos); the animation is SKIPPABLE.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecFly: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecFly);
	ZDATA_(CCommandExecute)
	NAI::SUnitPosition dst;
	// retail @0x3b58d0 serializes ONLY tag 2 dst -- NO CCommandExecute base chunk (dev-extra tag 1 removed, W3)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&dst); return 0; }
public:
	CExecFly() {}
	CExecFly( CUnitServer *_pUS, const NAI::SUnitPosition &_dst ): CCommandExecute(_pUS), dst(_dst)
	{
		// retail @0x3b33f0 tail: face the flight -- overwrite dst's direction with the 8-way
		// direction from the unit's current place toward the destination
		dst.pos.p.SetDirection( pUS->GetWorld()->GetPathNetwork()->GetDir( pUS->GetPosition().pos.p, dst.pos.p ) );
	}
	virtual void Run()
	{
		pUS->animator.Fly( pUS->GetPosition(), dst );
		StartAction( pUS->GetWorld(), SKIPPABLE );
	}
	virtual void AnimationFinished()
	{
		pUS->SetPosition( dst );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CExecLeaveInventory REMOVED -- dead: never constructed, Run() was fully commented out, and its
//  id 0x01122131 sits in a GAP of retail's exec block; retail has no leave-inventory exec)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetActiveItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSetActiveItem: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecSetActiveItem);
	ZDATA_(CCommandExecute)
	NDb::ESlot slot;
	int nStage;
	bool bTwoHeavy;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&slot); f.Add(3,&nStage); f.Add(4,&bTwoHeavy); return 0; }
public:
	CExecSetActiveItem() {}
	CExecSetActiveItem( CUnitServer *_pUS, NDb::ESlot _slot ): CCommandExecute(_pUS), slot(_slot) {}
	virtual void Run()
	{
		NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
		NDb::ESlot oldSlot = (NDb::ESlot)pInventory->GetActiveSlot();

		NDb::EItemSubType subType = NDb::SUBTYPE_NONE;
		if ( pInventory->Get(oldSlot) )
			subType = pInventory->Get(oldSlot)->GetDBItem()->subType;
		NDb::EItemSubType subTypeNext = NDb::SUBTYPE_NONE;
		if ( pInventory->Get(slot) )
			subTypeNext = pInventory->Get(slot)->GetDBItem()->subType;

		pInventory->Activate( slot );

		bool bActive = pUS->animator.IsActiveItem();
		if ( slot == oldSlot && bActive )
		{
			Finished();
			return;
		}
		if ( !pUS->animator.CanActivateItem() ) // on ladder, for example
		{
			NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
			pUS->animator.SetWeaponAnimation( bActive ? pRPG->GetWeaponType() : NDb::WT_DEFAULT );
			pUS->SetUndrawItem( !bActive );
			Finished();
			return;
		}
		StartAction( pUS->GetWorld(), NOBLOCK );

		bTwoHeavy = IsTwoHeavy( subType, subTypeNext );
		nStage = 1;
		if ( !BeginDeactivatingItem( pUS, subType ) )
			AnimationFinished();
	}
	virtual bool TimeLabelReached()
	{
		if ( !nStage )
			return false;
		pUS->SetUndrawItem( nStage == 1, bTwoHeavy && nStage == 1 );
		return nStage == 1;
	}
	virtual void AnimationFinished()
	{
		if ( !nStage )
			return;
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		NRPG::IInventory *pInventory = pRPG->GetInventory();
		if ( nStage == 2 || !pInventory->Get(slot) )
		{
			Finished();
			return;
		}
		nStage = 2;
		NDb::EItemSubType subType = pInventory->Get(slot)->GetDBItem()->subType;
		if ( subType == NDb::SUBTYPE_HEAVY || subType == NDb::SUBTYPE_MINE_DETECTOR )
			pUS->animator.ActivateItem( pUS->GetPosition(), true, bTwoHeavy, NDb::BELT_M1, pRPG->GetWeaponType() );
		else
		{
			int nPlace = pInventory->GetPlaceBySubType( subType );
			pUS->animator.ActivateItem( pUS->GetPosition(), false, nPlace == -1, (NDb::EItemPlace)nPlace, pRPG->GetWeaponType() );
		}
	}
	virtual void Cancel()
	{
		NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
		NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
		bool bActive = pInventory->GetActive() != 0;
		pUS->SetUndrawItem( !bActive );
		pUS->animator.SetWeaponAnimation( bActive ? pRPG->GetWeaponType() : NDb::WT_DEFAULT );
		pUS->animator.SetActiveItem( bActive );
		pUS->animator.AlignTime( 50 );
		pUS->animator.PlaceUnit( pUS->GetPosition() );
		nStage = 0;
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecLoadWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecLoadWeapon: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecLoadWeapon);
	ZDATA_(CCommandExecute)
	SItem sClip;
	CPtr<NRPG::IWeaponItemInfo> pWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&sClip); f.Add(3,&pWeapon); return 0; }
public:
	CExecLoadWeapon() {}
	CExecLoadWeapon( CUnitServer *_pUS, NRPG::IWeaponItemInfo* _pWeapon, const SItem &_sClip ): CCommandExecute(_pUS), pWeapon( _pWeapon ), sClip( _sClip ) {}
	int GetStartAP() const { return pUS->GetActionAP( NRPG::AC_RELOAD ); }
	int GetActionAP() const { return pUS->GetActionAP( NRPG::AC_RELOAD ); }
	virtual void Run()
	{
		pUS->DoAction( NRPG::AC_RELOAD );
		pUS->animator.Reload( pUS->GetPosition() );
		StartAction( pUS->GetWorld(), SKIPPABLE );
	}
	virtual bool TimeLabelReached()
	{
		CDynamicCast<NRPG::IClipItem> pClipItem( sClip.pItem );
		if ( IsValid( pClipItem ) && pUS->GetUnitRPG()->LoadWeapon( pWeapon, pClipItem ) && ( pClipItem->GetQuantity() == 0 ) )
		{
			NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
			switch( sClip.eType )
			{
			case SItem::SLOT:
				pInventory->TakeOff( NDb::ESlot( sClip.nSlot ) );
				break;
			case SItem::HAND:
				{
					// retail CExecLoadWeapon::LoadClip @0x3949a0: the consumed in-hand clip is cleared
					// by calling CUnitServer::SetHandItem (@0x387b30) DIRECTLY on the unit with an
					// SItem whose only written field is eType = VACUUM (all five smart pointers are
					// zeroed by the default ctor). Not the inventory -- retail's CInventory has no
					// hand member -- and not through the player.
					SItem sItem;
					sItem.eType = SItem::VACUUM;
					pUS->SetHandItem( sItem );
				}
				break;
			case SItem::BACKPACK:
				pInventory->Take( sClip.pItem );
				break;
			}
		}

		NDb::CSound *pSound = pWeapon->GetDBWeapon()->pSoundReload;
		NDb::SAISound sound = { NDb::GetAISound( 26 ), 0, 1.0f };   // retail @0x394df0: no silencer on load
		pUS->GetWorld()->MakeAISound( sound, pUS, pSound );

		pUS->Update();
		return false;
	}
	EUnitCommandResult CanDoIt()
	{
		if ( sClip.pUnit.GetPtr() != pUS.GetPtr() )
			return UCR_GENERAL_FAILURE;

		CDynamicCast<NRPG::IClipItem> pClipItem( sClip.pItem );
		if ( !IsValid( pClipItem ) )
			return UCR_GENERAL_FAILURE;

		if ( !pWeapon->CanLoad( pClipItem ) )
			return UCR_GENERAL_FAILURE;

		return UCR_OK;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecUnloadWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecUnloadWeapon: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecUnloadWeapon);
	ZDATA_(CCommandExecute)
	CPtr<NRPG::IWeaponItemInfo> pWeapon;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pWeapon); return 0; }
public:
	CExecUnloadWeapon() {}
	CExecUnloadWeapon( CUnitServer *_pUS, NRPG::IWeaponItemInfo* _pWeapon ): CCommandExecute(_pUS), pWeapon( _pWeapon ) {}
	virtual void Run()
	{
		ASSERT( pWeapon );
		pUS->GetUnitRPG()->UnloadWeapon( pWeapon );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetWishPose
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSetStrafe: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecSetStrafe);
	ZDATA_(CCommandExecute)
	bool bStrafe;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&bStrafe); return 0; }
public:
	CExecSetStrafe() {}
	CExecSetStrafe( CUnitServer *_pUS, bool _bStrafe ): CCommandExecute(_pUS), bStrafe(_bStrafe) {}
	EUnitCommandResult CanDoIt()
	{
		if ( pUS->IsWearingPK() )
			return UCR_GENERAL_FAILURE;

		return UCR_OK;
	}
	virtual void Run()
	{
		pUS->SetStrafe( bStrafe );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSetWishPose
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSetWishPose: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecSetWishPose);
	ZDATA_(CCommandExecute)
	NAI::EPose pose;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&pose); return 0; }
public:
	CExecSetWishPose() {}
	CExecSetWishPose( CUnitServer *_pUS, NAI::EPose _pose ): CCommandExecute(_pUS), pose(_pose) {}
	int GetActionAP() const
	{
		switch( pose )
		{
		case NAI::RUN:
			return pUS->GetActionAP( NRPG::AC_POSE_RUN );
		case NAI::WALK:
			return pUS->GetActionAP( NRPG::AC_POSE_WALK );
		case NAI::CROUCH:
			return pUS->GetActionAP( NRPG::AC_POSE_CROUCH );
		case NAI::CRAWL:
			return pUS->GetActionAP( NRPG::AC_POSE_CRAWL );
		}

		ASSERT( 0 );
		return 0;
	}
	virtual void Run()
	{
		pUS->SetWishPose( pose );
		pUS->SetRunning( pose == NAI::RUN );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (dev CExecNeedReload 0x52062175 REMOVED -- W5: dead pair with CCmdNeedReload, retail-absent; the
// OnLastPieceOfAmmo ack rides the live CWorld::PlayAck path instead, see wUnitCommands.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecWeaponJammed
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecWeaponJammed: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecWeaponJammed);
	ZDATA_(CCommandExecute)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); return 0; }
public:
	CExecWeaponJammed( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
		pUS->GetWorld()->GetGlobalAck()->OnWeaponJammed( pUS );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecOrderConfirmation
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecOrderConfirmation: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecOrderConfirmation);
	ZDATA_(CCommandExecute)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); return 0; }
public:
	CExecOrderConfirmation( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
		pUS->GetWorld()->GetGlobalAck()->OnOrderConfirmation( pUS );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecImpossibleToPerformAction
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecImpossibleToPerformAction: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecImpossibleToPerformAction);
	ZDATA_(CCommandExecute)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); return 0; }
public:
	CExecImpossibleToPerformAction( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	virtual void Run()
	{
		pUS->GetWorld()->GetGlobalAck()->OnImpossibleToPerformAction( pUS );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecSpendAP
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecSpendAPAndRegister: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecSpendAPAndRegister);
	ZDATA_(CCommandExecute)
	NRPG::EAction action;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&action); return 0; }
public:
	CExecSpendAPAndRegister( CUnitServer *_pUS = 0, 
		NRPG::EAction _action = NRPG::AC_NONE ): CCommandExecute(_pUS), action(_action) {}
	virtual int GetStartAP() const 
	{
		return pUS->GetActionAP( action );
	}
	virtual void Run()
	{
		pUS->DoAction( action );
		Finished();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecHide
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecHide: public CCommandExecute
{
	OBJECT_BASIC_METHODS( CExecHide );
	ZDATA
	ZPARENT( CCommandExecute );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CCommandExecute *)this); return 0; }
	//
public:
	CExecHide( CUnitServer *_pUS = 0 ): CCommandExecute(_pUS) {}
	//
	virtual int GetStartAP() const;
	virtual int GetActionAP() const;
	EUnitCommandResult CanDoIt();
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecHide::GetStartAP() const
{
	if ( pUS->GetUnitRPG()->IsHiding() )
		return 0;

	return pUS->GetActionAP( NRPG::AC_HIDE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CExecHide::GetActionAP() const
{
	return GetStartAP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecHide::CanDoIt()
{
	if ( pUS->IsWearingPK() )
		return UCR_GENERAL_FAILURE;

	if ( pUS->GetUnitRPG()->IsHiding() )
		return UCR_OK;
	//
	vector< CPtr<CPlayer> > players;
	pUS->GetWorld()->GetPlayersList( &players );
	for ( vector< CPtr<CPlayer> >::iterator i = players.begin(); i != players.end(); ++i )
	{
		if ( pUS->GetPlayer() == *i )
			continue;
		//
		const vector< CMObj<CUnitServer> > &units = (*i)->GetPlayerUnits();
		for ( vector< CMObj<CUnitServer> >::const_iterator u = units.begin(); u != units.end(); ++u )
		{
			if ( (*u)->GetDiplomacyState( pUS ) == NDb::DS_ENEMY && (*u)->IsUnitVisible( pUS ) )
				return UCR_GENERAL_FAILURE;
		}
	}
	//
	return UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecHide::Run() 
{ 
	if ( !pUS->GetUnitRPG()->IsHiding() )
	{
		pUS->DoAction( NRPG::AC_HIDE );
		pUS->Hide( true, false );
	}
	else
		pUS->Hide( false, false );   // retail CExecHide @0x3b2b5b: the voluntary toggle passes bThrowEvent=false

	Finished();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExecTakePerk
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExecTakePerk: public CCommandExecute
{
	OBJECT_BASIC_METHODS(CExecTakePerk);
	ZDATA_(CCommandExecute)
	int nID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCommandExecute*)this); f.Add(2,&nID); return 0; }
	//
public:
	CExecTakePerk() {}
	CExecTakePerk( CUnitServer *_pUS, int _nID ): CCommandExecute(_pUS), nID( _nID ) {}
	//
	EUnitCommandResult CanDoIt();
	virtual void Run();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
EUnitCommandResult CExecTakePerk::CanDoIt()
{
	NRPG::CPerksTree *pTree = pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree();

	vector<CPtr<NRPG::CPerk> > perksSet;
	pTree->GetAvailablePerks( &perksSet );

	for ( int nTemp = 0; nTemp < perksSet.size(); nTemp++)
	{
		if ( perksSet[nTemp]->GetDBPerk()->GetRecordID() == nID )
			return UCR_OK;
	}

	return UCR_GENERAL_FAILURE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExecTakePerk::Run() 
{ 
	NRPG::CPerksTree *pTree = pUS->GetUnitRPG()->GetRPGUnit()->GetPerksTree();
	pTree->TakePerk( nID );

	Finished(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TExec>
CCommandExecute* CreateSimpleExec( TExec *pExec, EUnitCommandResult *pError )
{
	CPtr<TExec> p( pExec );

	*pError = pExec->CanDoIt();
	if ( *pError != UCR_OK )
		return 0;

	return p.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateExecutor( CUnitServer *pUS, CCmd *pCmd, EUnitCommandResult *pError )
{
	CPtr<CCmd> pHold( pCmd );
	CWorld *pWorld = pUS->GetWorld();
	CUnitAnimator &animator = pUS->animator;
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	const NAI::SUnitPosition &position = pUS->GetPosition();

	*pError = UCR_OK;
	// retail CreateExecutor @0x3b37b0: a non-hero "wants to talk" command becomes a one-shot executor
	// that throws CEventOnNotHeroWantsToTalk (drives the CAckNPCInteraction bark).
	CDynamicCast<CCmdNotHeroWantsToTalk> pNotHeroTalk(pCmd);
	if (pNotHeroTalk)
		return new CExecNotHeroWantsToTalk(pUS);
	CDynamicCast<CCmdFly> pCmdFly(pCmd);
	if (pCmdFly)
		return new CExecFly(pUS, pCmdFly->dst);
	CDynamicCast<CCmdPath> pCmdPath(pCmd);
	if (pCmdPath)
	{
		vector<NAI::SPathPlace> dst;
		dst.push_back(pCmdPath->ptDst.p);
		CPtr<NAI::CPath> pPath = FindPath(pWorld->GetPathNetwork(), pUS,
			position.pos.p, dst, 0, true, pCmdPath->eParams, pUS->IsStrafing(), true);
		if (IsValid(pPath))
			return CreateMoveExecutor(pUS, pPath, pCmdPath->eParams, pCmdPath->needActiveItem, pError);
		return 0;
	}
	else {
		CDynamicCast<CCmdLook> pCmdLook(pCmd);
		if (pCmdLook)
		{
			vector<NAI::SPathPlace> dst;
			dst.push_back(pCmdLook->ptDst.p);
			CPtr<NAI::CPath> pPath = FindPath(pWorld->GetPathNetwork(), pUS, position.pos.p,
				dst, 0, true, NAI::PF_USE_DIR);
			if (IsValid(pPath))
			{
				CCommandExecute* pRet = CreateMoveExecutor(pUS, pPath, NAI::PF_USE_DIR, ITEM_NO_MATTER, pError, false);
				CDynamicCast<CExecQueue> pQueue(pRet);
				if (pQueue)
					pQueue->AddFrontExecutor(new CExecSpendAPAndRegister(pUS, NRPG::AC_ROTATE));
				return pRet;
			}
			return 0;
		}
		else {
			CDynamicCast<CCmdStartCombat> pCmdStartCombat(pCmd);
			if (pCmdStartCombat)
				return new CExecStartCombat(pUS);
			else {
				CDynamicCast<CCmdExplode> pExplode(pCmd);
				if (pExplode)
					return new CExecExplode(pUS);
				else {
					CDynamicCast<CCmdShootMode> pShootMode(pCmd);
					if (pShootMode)
						return new CExecShootMode(pUS, pShootMode);
					else {
						CDynamicCast<CCmdGrenadeMode> pGrenadeMode(pCmd);
						if (pGrenadeMode)
							return new CExecGrenadeMode(pUS, pGrenadeMode);
						else {
							CDynamicCast<CCmdArrangeInventory> pArrangeInventory(pCmd);
							if (pArrangeInventory)
								return new CExecArrangeInventory(pUS);
							else {
								CDynamicCast<CCmdSetActiveItem> pSetActiveItem(pCmd);
								if (pSetActiveItem)
									return new CExecSetActiveItem(pUS, (NDb::ESlot)pSetActiveItem->nSlot);
								else {
									CDynamicCast<CCmdStrafe> pStrafe(pCmd);
									if (pStrafe)
										return CreateSimpleExec(new CExecSetStrafe(pUS, pStrafe->bState), pError);
									else {
										CDynamicCast<CCmdWishPose> p(pCmd);
										if (p)
											return new CExecSetWishPose(pUS, p->pose);
										else {
											CDynamicCast<CCmdReload> pReload(pCmd);
											if (pReload)
												return CreateSimpleExec(new CExecReload(pUS, pReload->nSlot), pError);
											else {
												CDynamicCast<CCmdLoadWeapon> pLoadWeapon(pCmd);
												if (pLoadWeapon)
													return CreateSimpleExec(new CExecLoadWeapon(pUS, pLoadWeapon->GetWeapon(), pLoadWeapon->GetClip()), pError);
												else {
													CDynamicCast<CCmdUnloadWeapon> pUnloadWeapon(pCmd);
													if (pUnloadWeapon)
														return new CExecUnloadWeapon(pUS, pUnloadWeapon->GetWeapon());
													else {
														CDynamicCast<CCmdTeleport> pTeleport(pCmd);
														if (pTeleport)
															return new CExecTeleport(pUS, pTeleport);
														else {
															// (CCmdNeedReload -> CExecNeedReload dispatch REMOVED -- W5: dead retail-absent pair)
															{
																CDynamicCast<CCmdWeaponJammed> pWeaponJammed(pCmd);
																if (pWeaponJammed)
																	return new CExecWeaponJammed(pUS);
																else {
																	CDynamicCast<CCmdOrderConfirmation> pOrderConfirmation(pCmd);
																	if (pOrderConfirmation)
																		return new CExecOrderConfirmation(pUS);
																	else {
																		CDynamicCast<CCmdImpossibleToPerformAction> pImpossibleToPerformAction(pCmd);
																		if (pImpossibleToPerformAction)
																			return new CExecImpossibleToPerformAction(pUS);
																		else {
																			CDynamicCast<CCmdHide> pHide(pCmd);
																			if (pHide)
																				return CreateSimpleExec(new CExecHide(pUS), pError);
																			else {
																				CDynamicCast<CCmdTakePerk> pTakePerk(pCmd);
																				if (pTakePerk)
																					return CreateSimpleExec(new CExecTakePerk(pUS, pTakePerk->GetID()), pError);
																				else {
																					// retail CreateExecutor @0x3b37b0: create+activate runs as a QUEUE --
																					// [optional stash move (bNeedMove: moveSource->moveTarget)] ->
																					// create-and-slot -> set-active-item(nSlot).
																					CDynamicCast<CCmdCreateAndActivateInventoryItem> pCreateActivate(pCmd);
																					if (pCreateActivate)
																					{
																						CExecQueue *pQueue = new CExecQueue(pUS);
																						if (pCreateActivate->GetNeedMove())
																							pQueue->AddExecutor(new CExecMoveInventoryItem(pUS,
																								new CCmdMoveInventoryItem(pCreateActivate->GetMoveSource(), pCreateActivate->GetMoveTarget())));
																						pQueue->AddExecutor(new CExecCreateAndActivateInventoryItem(pUS, pCreateActivate));
																						pQueue->AddExecutor(new CExecSetActiveItem(pUS, (NDb::ESlot)pCreateActivate->GetSlot()));
																						return pQueue;
																					}
																					// retail @0x3b37b0 exchange arm: a QUEUE of the two symmetric moves + optional set-active.
																					// (No dev-only CExecExchangeInventoryItems -- retail composes it from registered execs, so a
																					// mid-exchange save serializes cleanly.) Each move's destination falls back to GROUND when
																					// it is a backpack with no room (IInventoryInfo::FindPlace, vtbl+0x14 in the decomp).
																					CDynamicCast<CCmdExchangeInventoryItems> pExchange(pCmd);
																					if (pExchange)
																					{
																						NRPG::IInventory *pInventory = pUS->GetUnitRPG()->GetInventory();
																						CExecQueue *pQueue = new CExecQueue(pUS);
																						CTPoint<int> ptPlace;
																						// 1) the displaced (in-hand) item -> the source's place (skipped when the hand is empty)
																						if (IsValid(pExchange->GetTarget().pItem))
																						{
																							SItem sTo = pExchange->GetSource();
																							if (sTo.eType == SItem::BACKPACK && !pInventory->FindPlace(pExchange->GetTarget().pItem, &ptPlace))
																								sTo.eType = SItem::GROUND;
																							pQueue->AddExecutor(new CExecMoveInventoryItem(pUS,
																								new CCmdMoveInventoryItem(pExchange->GetTarget(), sTo)));
																						}
																						// 2) the source item -> the target's place (the hand slot)
																						SItem sTo = pExchange->GetTarget();
																						if (sTo.eType == SItem::BACKPACK && !pInventory->FindPlace(pExchange->GetSource().pItem, &ptPlace))
																							sTo.eType = SItem::GROUND;
																						pQueue->AddExecutor(new CExecMoveInventoryItem(pUS,
																							new CCmdMoveInventoryItem(pExchange->GetSource(), sTo)));
																						if (pExchange->GetActivate())
																							pQueue->AddExecutor(new CExecSetActiveItem(pUS, (NDb::ESlot)pExchange->GetSlot()));
																						return pQueue;
																					}
																					else if (CCommandExecute* pExec = CreateActionExecutor(pUS, pCmd, pError))
																						return pExec;
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
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x00122170, CExecReload )
REGISTER_SAVELOAD_CLASS( 0x00722170, CExecCriticalLostWeapon )
REGISTER_SAVELOAD_CLASS( 0x00722171, CExecAccidentalShot )
REGISTER_SAVELOAD_CLASS( 0x01122130, CExecExplode )
REGISTER_SAVELOAD_CLASS( 0x01122133, CExecSetActiveItem )
REGISTER_SAVELOAD_CLASS( 0x01122134, CExecSetWishPose )
REGISTER_SAVELOAD_CLASS( 0x01122135, CExecTeleport )
REGISTER_SAVELOAD_CLASS( 0x71723161, CExecFly )
// (0x52062175 CExecNeedReload REMOVED -- W5: dead pair with CCmdNeedReload, retail-absent)
REGISTER_SAVELOAD_CLASS( 0x52062176, CExecWeaponJammed )
REGISTER_SAVELOAD_CLASS( 0x52062177, CExecOrderConfirmation )
REGISTER_SAVELOAD_CLASS( 0x52062178, CExecImpossibleToPerformAction )
REGISTER_SAVELOAD_CLASS( 0xB2062179, CExecStartCombat )
REGISTER_SAVELOAD_CLASS( 0xB206217A, CExecLoadWeapon )
REGISTER_SAVELOAD_CLASS( 0xB206217B, CExecUnloadWeapon )
REGISTER_SAVELOAD_CLASS( 0x71912110, CExecSpendAPAndRegister )
REGISTER_SAVELOAD_CLASS( 0xB1912111, CExecTakePerk )
REGISTER_SAVELOAD_CLASS( 0xB1912112, CExecGrenadeMode )
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
REGISTER_SAVELOAD_CLASS( 0xB3717160, CExecSetStrafe )
