#include "StdAfx.h"
//
#include "aiUnit.h"
#include "aiAction.h"
#include "aiAttackAction.h"
#include "aiMoveAction.h"
#include "aiState.h"
#include "aiJob.h"
#include "aiLog.h"
#include "aiInventory.h"
#include "aiWeapon.h"
#include "aiDecision.h"
#include "wMain.h"
#include "rpgUnitMission.h"
//
#include "aiCompoundAction.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogic
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::DoJob()
{
	if ( nCurrentJob < jobs.size() )
	{
		CPtr<CAIJob> pJob = jobs[ nCurrentJob ];
		ASSERT( IsValid( pJob ) );
		if ( IsValid( pJob ) )
		{
			OnPrepare( pJob );
			CPtr<IAIJobManager> pManager = GetState()->GetWorld()->GetAIJobManager();
			ASSERT( IsValid( pManager ) );
			if ( IsValid( pManager ) )
			{
				pManager->Add( pJob );
				pManager->WaitForJob( this, pJob );		
			}
		}
		++nCurrentJob;
	}
	else
	{
		MakeDecision();
		Finish();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogic::Think()
{
	if ( CanSkip() )
		return false;
	//
	CPtr<IAIJobManager> pManager = GetState()->GetWorld()->GetAIJobManager();
	ASSERT( IsValid( pManager ) );
	if ( IsValid( pManager ) )
	{
		nCurrentJob = 0;
		pManager->Add( this );
		return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogic::DoAction( CAIAction *pAction )
{
	SPlaceWithAP place;
	if ( GetPlaceForAction( pAction, &place )	)
	{
		CPtr<IAIUnit> pUnit = GetState()->GetCurrentAIUnit();		
		GetLog()->Add( new CAILogPosition( pUnit, pUnit->GetPosition(), place.place.pos ), true );
		GetLog()->Add( new CAILogSpendAP( pUnit, Max( 0 , pUnit->GetAP() - place.nUnitAP ) ), true );
	}
	pAction->Do( GetLog() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILogicAttack
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILogicAttack: public CAILogic
{
	OBJECT_BASIC_METHODS( CAILogicAttack );
	ZDATA
	ZPARENT( CAILogic );
	CObj<CAIFindGoodPlacesJob> pFindPlaces;
	CObj<CAIChoosePlaceForActionsJob> pChoosePlace;
	CObj<CAIShootAction> pShoot;
	CObj<CAIThrowGrenadeAction> pThrowGrenade;
	CObj<CAILaunchRocketAction> pLaunchRocket;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAILogic *)this); f.Add(3,&pFindPlaces); f.Add(4,&pChoosePlace); f.Add(5,&pShoot); f.Add(6,&pThrowGrenade); return 0; }
	//
	void ReloadWeapon( CAIFireArmsWeapon *pWeapon );
	void MoveToEnemy();
	//
public:
	CAILogicAttack() {}
	CAILogicAttack( IAIState *_pState, IAILogContainer *_pLog, CAIJob *_pParentJob );
	//
	virtual void MakeDecision();
	virtual bool CanSkip() const;
	virtual bool GetPlaceForAction( CAIAction *pAction, SPlaceWithAP *pPlace );
	virtual void OnPrepare( CAIJob *pJob );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogicAttack::CAILogicAttack( IAIState *_pState, IAILogContainer *_pLog, CAIJob *_pParentJob ): 
	CAILogic( _pState, _pLog, _pParentJob )
{
	const int N_AP_TO_MOVE = 12;
	CPtr<IAIUnit> pUnit = GetState()->GetCurrentAIUnit();
	CPtr<IAIUnit> pEnemy = GetState()->GetCurrentAIEnemy();
	ASSERT( IsValid( pUnit ) );
	if ( IsValid( pUnit ) )
	{
		pShoot = new CAIShootAction( GetState() );
		pThrowGrenade = new CAIThrowGrenadeAction( GetState() );
		pLaunchRocket = new CAILaunchRocketAction( GetState() );
		pFindPlaces = new CAIFindGoodPlacesJob( pUnit, pEnemy, N_AP_TO_MOVE, this );
		vector< CPtr<CAIAction> > actions;
		actions.push_back( pShoot.GetPtr() );
		actions.push_back( pThrowGrenade.GetPtr() );
		actions.push_back( pLaunchRocket.GetPtr() );
		pChoosePlace = new CAIChoosePlaceForActionsJob( actions, this );
		jobs.push_back( pFindPlaces.GetPtr() );
		jobs.push_back( pChoosePlace.GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogicAttack::OnPrepare( CAIJob *pJob )
{
	if ( pJob == pChoosePlace )
	{
		pChoosePlace->SetPlaces( pFindPlaces->GetPlaces() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogicAttack::CanSkip() const
{
	const int N_NO_AP = 20;
	CPtr<IAIUnit> pUnit = GetState()->GetCurrentAIUnit();
	CPtr<IAIUnit> pEnemy = GetState()->GetCurrentAIEnemy();
	return !IsValid( pUnit ) || !IsValid( pEnemy ) || pUnit->GetAP() <= N_NO_AP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILogicAttack::GetPlaceForAction( CAIAction *pAction, SPlaceWithAP *pPlace )
{
	if ( !pChoosePlace->IsPlaceChosen( pAction ) )
		return false;
	pChoosePlace->GetPlace( pAction, pPlace );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogicAttack::ReloadWeapon( CAIFireArmsWeapon *pWeapon )
{
	CPtr<IAIUnit> pUnit = GetState()->GetCurrentAIUnit();
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pWeapon ) );
	if ( IsValid( pUnit ) && IsValid( pWeapon ) )
	{
		if ( !pUnit->GetAIInventory()->IsCurrentItem( pWeapon ) )
			GetLog()->Add( new CAILogChangeWeapon( pUnit, pWeapon ), true );
		GetLog()->Add( new CAILogReloadWeapon( pUnit, pWeapon ), true );
		int nReloadAP = pUnit->GetUnitMission()->GetActionAP( NAI::WALK, NRPG::AC_RELOAD );
		GetLog()->Add( new CAILogSpendAP( pUnit, nReloadAP ), true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogicAttack::MoveToEnemy()
{
	ASSERT( IsValid( GetState() ) );
	if ( IsValid( GetState() ) )
	{
		CPtr<IAIUnit> pUnit = GetState()->GetCurrentAIUnit();
		CPtr<IAIUnit> pEnemy = GetState()->GetCurrentAIEnemy();
		if ( IsValid( pUnit ) && IsValid( pEnemy ) && pUnit->GetAIInventory()->HasAnyWeapon() )
		{
			GetLog()->Add( new CAILogPosition( pUnit, pUnit->GetPosition(), pEnemy->GetPosition(), NAI::CROUCH ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILogicAttack::MakeDecision()
{
	CAIShootAction::SAIShootActionInfo shootInfo;
	CAIThrowGrenadeAction::SAIThrowGrenadeActionInfo grenadeInfo;
	CAILaunchRocketAction::SAILaunchRocketActionInfo rocketInfo;
	SPlaceWithAP place;
	if ( pChoosePlace->IsPlaceChosen( pShoot ) )
	{
		pChoosePlace->GetPlace( pShoot, &place );
		pShoot->GetInfo( place, &shootInfo );
	}
	if ( pChoosePlace->IsPlaceChosen( pThrowGrenade ) )
	{
		pChoosePlace->GetPlace( pThrowGrenade, &place );
		pThrowGrenade->GetInfo( place, &grenadeInfo );
	}
	if ( pChoosePlace->IsPlaceChosen( pLaunchRocket ) )
	{
		pChoosePlace->GetPlace( pLaunchRocket, &place );
		pLaunchRocket->GetInfo( place, &rocketInfo );
	}
	//
	// Reload special-cases: in the release these are folded into the decision as CAIReloadAction;
	// the predecessor handles them ahead of the action choice, so we keep them as early outs.
	if ( !grenadeInfo.bCanDo && shootInfo.bCanDo && shootInfo.bNeedReload )
	{
		ReloadWeapon( shootInfo.pWeapon );
		return;
	}
	if ( !grenadeInfo.bCanDo && rocketInfo.bNeedReload )
	{
		ReloadWeapon( rocketInfo.pWeapon );
		return;
	}
	//
	// The release replaced the hand-coded priority cascade with the CDecision<CAIAction*> fuzzy
	// engine (CAIAttackLogic::MakeDecision @0x00421890). We drive the three predecessor actions
	// through that same engine: rule order encodes priority (rocket > grenade > shoot) and
	// GetBestAction commits to the first strongly-true high-priority rule. This reproduces the
	// predecessor's decision exactly while using the release decision mechanism. The full release
	// rule set spans 19 actions (knife/melee/heal/loot/snipe/HG/PK) - see ai-architecture.md.
	bool bGrenadePreferred = !shootInfo.bKillTargetCertainly || grenadeInfo.bBadGroupHealth;
	//
	CPtr< CDecision<CAIAction*> > pDecision = new CDecision<CAIAction*>;
	{
		vector< CPtr<ISign> > required, regular;
		required.push_back( new CSign<bool>( &rocketInfo.bCanDo, true, true, true ) );
		pDecision->AddRule( new CRule<CAIAction*>( pLaunchRocket.GetPtr(), required, regular ) );
	}
	{
		vector< CPtr<ISign> > required, regular;
		required.push_back( new CSign<bool>( &grenadeInfo.bCanDo, true, true, true ) );
		required.push_back( new CSign<bool>( &bGrenadePreferred, true, true, true ) );
		pDecision->AddRule( new CRule<CAIAction*>( pThrowGrenade.GetPtr(), required, regular ) );
	}
	{
		vector< CPtr<ISign> > required, regular;
		required.push_back( new CSign<bool>( &shootInfo.bCanDo, true, true, true ) );
		pDecision->AddRule( new CRule<CAIAction*>( pShoot.GetPtr(), required, regular ) );
	}
	//
	CAIAction *pBest = pDecision->GetBestAction();
	if ( pBest )
		DoAction( pBest );
	else
		MoveToEnemy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogic* CreateAttackAILogic( IAIState *pState, IAILogContainer *pLog, CAIJob *pParentJob )
{
	ASSERT( IsValid( pState ) );
	ASSERT( IsValid( pLog ) );
	CPtr<IAIUnit> pUnit = pState->GetCurrentAIUnit();
	CPtr<IAIUnit> pEnemy = pState->GetCurrentAIEnemy();
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pEnemy ) );
	if ( IsValid( pUnit ) && IsValid( pEnemy ) && IsValid( pState ) && IsValid( pLog ) )
		return new CAILogicAttack( pState, pLog, pParentJob );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILogic* CreateDefendAILogic( IAIState *pState, IAILogContainer *pLog, CAIJob *pParentJob )
{
	ASSERT( 0 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51013110, CAILogicAttack );