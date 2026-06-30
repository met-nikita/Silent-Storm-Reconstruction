#include "StdAfx.h"
//
#include "..\DBFormat\DataRPG.h"
#include "aiPosition.h"
#include "aiUnit.h"
#include "aiInventory.h"
#include "aiWeapon.h"
#include "aiLog.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "wUnitCommands.h"
#include "rpgItem.h"
#include "aiState.h"
#include "rpgUnitMission.h"
#include "rpgUnitInfo.h"
//
#include "aiAttackAction.h"
//
namespace NWorld
{
	EUnitCommandResult CanUnitThrowGrenade( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IGrenadeItem *pGrenade );
	EUnitCommandResult CanUnitLaunchRocket( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, int nExtraAP, NRPG::IWeaponItem *pBazooka );
}
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIShootAction
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIShootAction::GetInfo( const SPlaceWithAP &place, SAIShootActionInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	ASSERT( IsValid( GetUnit() ) );
	ASSERT( IsValid( GetEnemy() ) );
	if ( !IsValid( GetUnit() ) || !IsValid( GetEnemy() ) )
		return;
	//
	pInfo->pWeapon = 
		GetUnit()->GetAIInventory()->GetBestFireArms( place.place, GetEnemy(), place.nUnitAP, &pInfo->nCover, &pInfo->nDamage, &pInfo->shootMode, &pInfo->nToHit );
	if ( IsValid( pInfo->pWeapon ) )
	{
		pInfo->bCanDo = true;
		pInfo->bNeedReload = pInfo->pWeapon->GetCurrentClip()->GetAmmoCount() <= 0;
		if ( pInfo->bNeedReload && !IsValid( pInfo->pWeapon->GetNextClip() ) )
			pInfo->bCanDo = false;
		pInfo->fDistance = fabs( GetEnemy()->GetPosition().GetCP() - place.place.GetCP() );
		pInfo->bKillTargetCertainly = GetEnemy()->GetHP() * 2.5f < pInfo->nDamage;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIShootAction::CanDo( const SPlaceWithAP &place ) const
{
	SAIShootActionInfo info;
	GetInfo( place, &info );
	return info.bCanDo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIShootAction::Do( IAILogContainer *pLog ) const
{
	ASSERT( IsValid( pLog ) );
	if ( !IsValid( pLog ) )
		return;
	//
	SAIShootActionInfo info;
	CPtr<IAIUnit> pUnit = GetUnit();
	CPtr<IAIUnit> pEnemy = GetEnemy();
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pEnemy ) );
	GetInfo( GetCurrentPlace(), &info );
	ASSERT( info.bCanDo );
	if ( info.bCanDo && IsValid( pUnit ) && IsValid( pEnemy ) && IsValid( info.pWeapon ) && info.nToHit > 0  )
	{
		while ( 1 )
		{
			int nAP = pUnit->GetAP();
			int nAmmoCount = info.pWeapon->GetCurrentClip()->GetAmmoCount();
			if ( nAP > 0 && nAmmoCount > 0 )
			{
				if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
					pLog->Add( new CAILogChangeWeapon( pUnit, info.pWeapon ), true );
				pLog->Add( new CAILogChangeShootMode( pUnit, info.pWeapon, info.shootMode ), true );
				//
				bool bNeedReload;
				int nAmmo, nShotHP, nShotAP;
				info.pWeapon->GetShotParameters( pUnit->GetUnitPosition(), pEnemy, info.nCover, pUnit->GetAP(), &nShotAP, &nAmmo, &nShotHP, &bNeedReload );
				if ( nAmmo > 0 && nShotAP <= pUnit->GetAP() )
				{
					if ( bNeedReload )
						pLog->Add( new CAILogReloadWeapon( pUnit, info.pWeapon ), true );
					pLog->Add( new CAILogShot( pUnit, pEnemy, HL_ANY ), true );
					pLog->Add( new CAILogSpendAP( pUnit, nShotAP ), true );
					pLog->Add( new CAILogSpendAmmo( info.pWeapon->GetCurrentClip(), nAmmo ), true );
				}
				else
					break;
			}
			else
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIShootAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	const int N_GOOD_TOHIT = 20;
	//
	SAIShootActionInfo info1, info2;
	GetInfo( p1, &info1 );
	GetInfo( p2, &info2 );
	//
	if ( !info1.bCanDo )
		return false;
	if ( !info2.bCanDo )
		return true;
	if ( info2.nToHit >= N_GOOD_TOHIT )	
		return false;
	ECheckMove pose = ( ECheckMove )( p1.place.pos.p.GetPose() );
	if ( pose == CM_INACTIVE || pose == CM_LAY )
		return false;
	if ( info1.nToHit < info2.nToHit )
		return false;
	if ( info1.nToHit > 0 && info1.nToHit > info2.nToHit )
		return true;
	else
		return info1.fDistance < info2.fDistance;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIThrowGrenadeAction
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsGoodGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, CAIFireArmsWeapon *pLauncher, const SAIUnitGroup &group, CVec3 *pTarget )
{
	ASSERT( IsValid( pLauncher ) );
	if ( !IsValid( pLauncher ) )
		return false;
	//
	if ( !group.allies.empty() || group.enemies.empty() )
		return false;
	//
	CDynamicCast<NRPG::IWeaponItem> pItem( pLauncher->GetItem() );
	*pTarget = group.ptCenter;
	if ( NWorld::CanUnitLaunchRocket( pUS, pos, *pTarget, 0, pItem.GetPtr() ) != NWorld::UCR_OK )
	{
		*pTarget = group.ptCenter + CVec3( 0, 0, 0.6f );
		CVec3 ptDir = pos.GetCP() - *pTarget;
		Normalize( &ptDir );
		*pTarget += ptDir * 1.7f;
		if ( NWorld::CanUnitLaunchRocket( pUS, pos, *pTarget, 0, pItem.GetPtr() ) != NWorld::UCR_OK )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsGoodGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, CAIGrenadeWeapon *pGrenade, const SAIUnitGroup &group, CVec3 *pTarget )
{
	ASSERT( IsValid( pGrenade ) );
	if ( !IsValid( pGrenade ) )
		return false;
	//
	if ( !group.allies.empty() || group.enemies.empty() )
		return false;
	// can throw
	CDynamicCast<NRPG::IGrenadeItem> pItem( pGrenade->GetItem() );
	NWorld::EUnitCommandResult res = NWorld::CanUnitThrowGrenade( pUS, pos, group.ptCenter, pItem.GetPtr() );	
	if ( res != NWorld::UCR_OK )
		return false;
	//
	*pTarget = group.ptCenter;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static int GetNearestGroup( NWorld::CUnitServer *pUS, const SUnitPosition &pos, T *pWeapon, const vector<SAIUnitGroup> &groups, CVec3 *pTarget )
{
	int nBestGroup = -1;
	int nGroup = 0;
	while ( nGroup < groups.size() )
	{
		const SAIUnitGroup &group = groups[ nGroup ];
		CVec3 ptTarget;
		if ( IsGoodGroup( pUS, pos, pWeapon, group, &ptTarget ) )
		{
			bool bBestGroup = true;
			if ( nBestGroup >= 0 )
			{
				const SAIUnitGroup &bestGroup = groups[ nBestGroup ];
				CVec3 unitPos = pos.GetCP();
				float fDistance = fabs2( group.ptCenter - unitPos );
				float fBestDistance = fabs2( bestGroup.ptCenter - unitPos );
				if ( !( fDistance < fBestDistance || ( fDistance == fBestDistance && group.enemies.size() > bestGroup.enemies.size() ) ) )
					bBestGroup = false;
			}
			if ( bBestGroup )
			{
				nBestGroup = nGroup;
				*pTarget = ptTarget;
			}
		}
		++nGroup;
	}
	return nBestGroup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsBadGroupHealth( const SAIUnitGroup &group )
{
	const int N_BAD_HEALTH = 15;
	vector< CPtr<IAIUnit> >::const_iterator i;
	int nHealth = 0;
	for ( i = group.enemies.begin(); i != group.enemies.end(); ++i )
		nHealth = Max( nHealth, (*i)->GetAP() );
	return nHealth <= N_BAD_HEALTH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIThrowGrenadeAction::GetInfo( const SPlaceWithAP &place, SAIThrowGrenadeActionInfo *pInfo ) const
{
	pInfo->bCanDo = false;
	ASSERT( IsValid( GetUnit() ) );
	if ( !IsValid( GetUnit() ) )
		return;
	pInfo->pGrenade = GetUnit()->GetAIInventory()->GetBestGrenade( VNULL3 );
	if ( !IsValid( pInfo->pGrenade ) )
		return;
	//
	CVec3 ptTarget;
	const vector<SAIUnitGroup> &groups = GetState()->GetEnemyGroups();
	pInfo->nTargetGroup = GetNearestGroup( GetUnit()->GetUnitServer(), place.place, pInfo->pGrenade.GetPtr(), groups, &ptTarget );
	if ( pInfo->nTargetGroup >= 0 )
	{
		pInfo->bCanDo = true;
		const SAIUnitGroup &group = groups[ pInfo->nTargetGroup ];
		pInfo->fDistance = fabs( group.ptCenter - place.place.GetCP() );
		pInfo->nTargetSize = group.enemies.size();
		pInfo->bBadGroupHealth = IsBadGroupHealth( group );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIThrowGrenadeAction::CanDo( const SPlaceWithAP &place ) const
{
	SAIThrowGrenadeActionInfo info;
	GetInfo( place, &info );
	return info.bCanDo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIThrowGrenadeAction::Do( IAILogContainer *pLog ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	ASSERT( IsValid( pUnit ) );
	ASSERT( IsValid( pLog ) );
	if ( !IsValid( pLog ) )
		return;
	//
	SAIThrowGrenadeActionInfo info;
	GetInfo( GetCurrentPlace(), &info );
	ASSERT( info.bCanDo );
	if ( info.bCanDo )
	{
		CPtr<CAIGrenadeWeapon> pGrenade = info.pGrenade;
		NAI::EPose pose = pUnit->GetUnitPosition().GetPose();
		int nGrenadeThrowAP = pUnit->GetUnitMission()->GetActionAP( pose, NRPG::AC_THROW_GRENADE );
		while ( IsValid( pGrenade ) && pUnit->GetAP() >= nGrenadeThrowAP )
		{
			const vector<SAIUnitGroup> &groups = GetState()->GetEnemyGroups();
			if ( !pUnit->GetAIInventory()->IsCurrentItem( pGrenade ) )
				pLog->Add( new CAILogChangeWeapon( pUnit, pGrenade ), true );
			pLog->Add( new CAILogThrowGrenade( pUnit, groups[ info.nTargetGroup ].ptCenter, pGrenade ), true );
			pLog->Add( new CAILogSpendAP( pUnit, nGrenadeThrowAP ), true );
			pGrenade = pUnit->GetAIInventory()->GetBestGrenade( VNULL3 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIThrowGrenadeAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	SAIThrowGrenadeActionInfo info1, info2;
	GetInfo( p1, &info1 );
	GetInfo( p2, &info2 );
	//
	if ( !info1.bCanDo )
	{
		if ( info2.bCanDo )
			return false;
		else
			return info1.fDistance < info2.fDistance;
	}
	if ( !info2.bCanDo )
		return true;
	return p1.nUnitAP > p2.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILaunchRocketAction
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILaunchRocketAction::GetInfo( const SPlaceWithAP &place, SAILaunchRocketActionInfo *pInfo ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	pInfo->bCanDo = false;
	ASSERT( IsValid( pUnit ) );
	if ( !IsValid( pUnit ) )
		return;
	//
	pInfo->pWeapon = pUnit->GetAIInventory()->GetBestRocketLaunchers();
	if ( !IsValid( pInfo->pWeapon ) )
		return;
	if ( pInfo->pWeapon->GetCurrentClip()->GetAmmoCount() <= 0 )
	{
		if ( IsValid( pInfo->pWeapon->GetNextClip() ) )
		{
			pInfo->bNeedReload = true;
			pInfo->bCanDo = true;
		}
		return;
	}
	//
	const vector<SAIUnitGroup> &groups = GetState()->GetEnemyGroups();
	int nGroup = GetNearestGroup( pUnit->GetUnitServer(), place.place, pInfo->pWeapon.GetPtr(), groups, &pInfo->ptTarget );
	if ( nGroup >= 0 )
	{
		const SAIUnitGroup &group = groups[ nGroup ];
		pInfo->bCanDo = true;
		pInfo->nTargetSize = group.enemies.size();
		pInfo->fDistance = fabs( group.ptCenter - place.place.GetCP() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILaunchRocketAction::CanDo( const SPlaceWithAP &place ) const
{
	SAILaunchRocketActionInfo info;
	GetInfo( place, &info );
	return info.bCanDo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAILaunchRocketAction::Do( IAILogContainer *pLog ) const
{
	CPtr<IAIUnit> pUnit = GetUnit();
	SAILaunchRocketActionInfo info;
	GetInfo( GetCurrentPlace(), &info );
	if ( !pUnit->GetAIInventory()->IsCurrentItem( info.pWeapon ) )
		pLog->Add( new CAILogChangeWeapon( pUnit, info.pWeapon ), true );
	pLog->Add( new CAILogChangeShootMode( pUnit, info.pWeapon, NDb::SM_Snap ), true );
	pLog->Add( new CAILogShotPoint( pUnit, info.ptTarget ), true );
	pLog->Add( new CAILogSpendAP( pUnit, info.pWeapon->GetShotAP() ), true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAILaunchRocketAction::ComparePlaces( const SPlaceWithAP &p1, const SPlaceWithAP &p2 ) const
{
	SAILaunchRocketActionInfo info1, info2;
	GetInfo( p1, &info1 );
	GetInfo( p2, &info2 );
	//
	if ( !info1.bCanDo )
	{
		if ( info2.bCanDo )
			return false;
		else
			return info1.fDistance < info2.fDistance;
	}
	if ( !info2.bCanDo )
		return true;
	return p1.nUnitAP > p2.nUnitAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////








////////////////////////////////////////////////////////////////////////////////////////////////////
/*
bool CAITacticalCommander::CannonCanDamageEnemy( NWorld::CCannon *pCannon )
{
	vector< CPtr<IAIUnit> > *vEnemyUnits = pAIState->GetEnemyAIPlayer()->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = vEnemyUnits->begin(); i != vEnemyUnits->end(); ++i )
	{
		if ( (*i)->GetUnitServer()->CanDo( new NWorld::CCmdCannon( pCannon ) ) == NWorld::UCR_OK )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCannon *CAITacticalCommander::FindNearestCannon( IAIUnit *pUnit )
{
	NWorld::CCannon *pRes = 0;
	list< CObj<NWorld::IDynamicObject> > *miscObjects = GetWorld()->GetMiscObjects();
	float fMinDistance = 10; // максимальный радиус поиска пулемета в метрах 
	for ( list< CObj<NWorld::IDynamicObject> >::iterator i = miscObjects->begin(); i != miscObjects->end(); ++i )
	{
		if ( CDynamicCast<NWorld::CCannon> pCannon( *i ) )
		{
			if ( pCannon->GetItem()->HasAmmo() && !pCannon->IsBroken() && !pCannon->IsOccupied() )
			{
				// не вполне корректно, т.к. надо-бы проверять кол-во AP необходимое, чтобы дойти до cannon
				float fDistance = fabs( pCannon->GetPosition() - pUnit->GetUnitServer()->GetPosition().GetCP() );

				if ( !pAICommander->IsObjectLocked( pCannon ) && fDistance < fMinDistance )
				{
					NWorld::EUnitCommandResult eResult;
					CPtr<NWorld::CCmdCannon> pCmd = new NWorld::CCmdCannon( pCannon );
					CPtr<NWorld::CCommandExecute> pExec = NWorld::CreateActionExecutor( pUnit->GetUnitServer(), pCmd, &eResult );

					bool bCannonCanDamageEnemy = CannonCanDamageEnemy( pCannon );

					if ( pExec && CannonCanDamageEnemy( pCannon ) )
					// можем использовать cannon и есть в кого стрелять
					{
						pRes = pCannon;
						fMinDistance = fDistance;
					}
				}
			}
		}
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITacticalCommander::UseCannon( IAIUnit *pAIUnit )
{
	if ( !IsValid( pAIUnit ) )
		return;

	NWorld::CCannon *pCannon = pAIUnit->GetCannon();
	if ( IsValid( pCannon ) )
	{
		pAILog->Clear();
		// проверяем, что cannon в рабочем состоянии
		if ( !( pCannon->GetItem()->HasAmmo() && !pCannon->IsBroken() && CannonCanDamageEnemy( pCannon ) ) )
		{
			// отвязываемся от cannon
			pAIUnit->SetCannon( 0 );
			pAILog->Add( new CAILogExitCannon( pAIUnit, pCannon ) );
			pAICommander->UnLockObject( pCannon );
		}
	}
	else		
	{
		// ищем cannon, который имеет смысл использовать
		CPtr<NWorld::CCannon> pTmpCannon = FindNearestCannon( pAIUnit );
		if ( IsValid( pTmpCannon ) )
		{
			pAILog->Clear();
			pAIUnit->SetCannon( pTmpCannon );
			pAILog->Add( new CAILogUseCannon( pAIUnit, pTmpCannon ) );
			pAICommander->LockObject( pTmpCannon );
		}
	}
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51313110, CAIShootAction )
REGISTER_SAVELOAD_CLASS( 0x51313120, CAIThrowGrenadeAction )
REGISTER_SAVELOAD_CLASS( 0x51413180, CAILaunchRocketAction )