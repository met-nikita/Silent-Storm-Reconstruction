#include "stdafx.h"
//
#include "..\DBFormat\DataRPG.h"
//
#include "aiWeapon.h"
#include "aiUnit.h"
//
#include "rpgUnitMission.h"
#include "rpgItemSet.h"
#include "rpgToHit.h"
#include "rpgUnit.h"
//
#include "wUnitServer.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_MAX_AP = 0xFFFF;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFireArmsWeaponClip
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip::CAIFireArmsWeaponClip( NRPG::CClipItem *_pClipItem ): 
	pClipItem( _pClipItem ), nAmmoCount( 0 )
{
	ASSERT( IsValid( pClipItem ) );
	if ( IsValid( pClipItem ) )
		nAmmoCount = pClipItem->GetIncQuantity();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeaponClip::GetAmmoCount() const 
{ 
	return nAmmoCount; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeaponClip::SetAmmoCount( int nCount ) 
{ 
	ASSERT( nCount >= 0 ); 
	nAmmoCount = Max( 0, nCount ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeaponClip::SpendAmmo( int nCount ) 
{ 
	nAmmoCount = max( 0, nAmmoCount - nCount ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeaponClip::IsEmpty() const 
{ 
	return nAmmoCount <= 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CClipItem* CAIFireArmsWeaponClip::GetItem() const 
{ 
	return pClipItem; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFireArmsWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeapon::CAIFireArmsWeapon( IAIUnit *_pOwner, NRPG::CWeaponItem *_pWeaponItem )
{
	pOwner = _pOwner;
	pWeaponItem = _pWeaponItem;
	pCurrentClip = 0;
	ASSERT( IsValid( pWeaponItem ) );
	ASSERT( IsValid( pOwner ) );
	if ( IsValid( pWeaponItem ) )
	{
		CDynamicCast<NRPG::CClipItem> pClipItem(pWeaponItem->GetInnerClip());
		if (pClipItem)
			SetCurrentClip( CreateAIFireArmsWeaponClip( pClipItem ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IInventoryItem* CAIFireArmsWeaponBase::GetInventoryItem() const
{
	return CDynamicCast<NRPG::IInventoryItem>( GetItem() ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFireArmsWeapon::GetAnimType -- the weapon's animation hold-type (the [model+0x6c]->[+0x38] chain the
// assassin gate reads): GetItem()->GetDBWeapon()->pAnimWeaponType->type. Returns -1 when any link is missing.
int CAIFireArmsWeapon::GetAnimType() const
{
	NRPG::CWeaponItem *pItem = GetItem();
	if ( pItem == 0 )
		return -1;
	NDb::CRPGWeapon *pDB = pItem->GetDBWeapon();
	if ( pDB == 0 || !IsValid( pDB->pAnimWeaponType ) )
		return -1;
	return (int)pDB->pAnimWeaponType->type;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeaponBase::GetClipCount() const
{
	return clips.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CAIFireArmsWeaponBase::GetCurrentClip() const
{
	return pCurrentClip;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeaponBase::IsCurrentClipFull() const
{
	if ( !IsValid( pCurrentClip ) || !IsValid( pCurrentClip->GetItem() ) )
		return false;
	// Retail v1.1 0x4b5b80 / v1.2 0x4b5e00 calls IItemContainerInfo::GetMaxIncQuantity.
	// The loaded clip's capacity can differ from the DB ammo pack (Nagant: 7 vs 21).
	return pCurrentClip->GetAmmoCount() == pCurrentClip->GetItem()->GetMaxIncQuantity();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeaponBase::SetCurrentClip( CAIFireArmsWeaponClip *pClip )
{
	pCurrentClip = pClip;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CAIFireArmsWeaponBase::GetNextClip() const
{
	return clips.empty() ? 0 : clips.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeaponBase::RemoveClip( CAIFireArmsWeaponClip *pClip )
{
	ASSERT( IsValid( pClip ) );
	ASSERT( find( clips.begin(), clips.end(), pClip ) != clips.end() );
	clips.erase( remove( clips.begin(), clips.end(), pClip ), clips.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeaponBase::AddClip( CAIFireArmsWeaponClip *pClip )
{
	ASSERT( IsValid( pClip ) );
	if ( IsValid( pClip ) )
	{
		bool bIsSuitable = IsSuitableClip( pClip );
		ASSERT( bIsSuitable );
		if ( bIsSuitable )
			clips.push_back( pClip );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeaponBase::IsSuitableClip( CAIFireArmsWeaponClip *pClip ) const
{
	ASSERT( IsValid( pClip ) );
	if ( IsValid( pClip ) )
	{
		CDynamicCast<NRPG::CClipItem> pInnerClipItem(pWeaponItem->GetInnerClip());
		if (pInnerClipItem)
			return pInnerClipItem->IsCompatible( pClip->GetItem(), false );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb5a60: compare by the DB AIRating ("+0xa8"), NOT nQuality. A null/dead candidate makes the
// held weapon read as worse (retail returns true on the guard path).
bool CAIFireArmsWeaponBase::IsWorseThen( NRPG::CWeaponItem *pCandidate ) const
{
	if ( !IsValid( pCandidate ) )
		return true;
	return pWeaponItem->GetDBWeapon()->nAIRating < pCandidate->GetDBWeapon()->nAIRating;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#define GET_WEAPON_OPERATION_AP( Name, RPGName )									\
int CAIFireArmsWeapon::##Name() const															\
{																																	\
	ASSERT( IsValid( pOwner ) );																		\
	if ( IsValid( pOwner ) )																				\
		return pOwner->GetRPGUnit()->##RPGName( GetItem() );					\
	else																														\
		return N_MAX_AP;																							\
}
//
GET_WEAPON_OPERATION_AP( GetShotAP, GetWeaponAP );
GET_WEAPON_OPERATION_AP( GetBurstAP, GetWeaponBurstAP );
GET_WEAPON_OPERATION_AP( GetReloadAP, GetWeaponReloadAP );
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetMinAPToShoot( int nUnitAP ) const
{
	int nRes = 0;
	//
	nRes = GetShotAP();
	NAI::EPose pose = pOwner->GetUnitPosition().GetPose();
	nRes += pOwner->GetUnitMission()->GetActionAP( pose, NRPG::AC_PREPARE );
	//
	if ( pWeaponItem->GetShootMode() == NDb::SM_Careful )
		nRes = max( nUnitAP, GetShotAP() );
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeapon::IsSameWeapon( CAIFireArmsWeapon *pWeapon ) const
{
	ASSERT( IsValid( pWeapon ) );
	if ( IsValid( pWeapon ) )
	{
		int nID = pWeapon->GetItem()->GetDBWeapon()->GetRecordID();
		int nThisID = GetItem()->GetDBWeapon()->GetRecordID();
		if ( nID == nThisID )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeapon::IsBurstMode( NDb::EShootMode eShotMode ) const
{
	return ( eShotMode == NDb::SM_ShortBurst ) || ( eShotMode == NDb::SM_LongBurst );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetMeanDamage() const
{
	NRPG::SWeaponInfo info;
	GetItem()->GetInfo( &info );
	return ( info.nDmgMax + info.nDmgMin ) / 2.f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetLongBurstAmmoCountPerShot( int nUnitAP ) const
{
	int nRes = 0;
	int nAP = nUnitAP;
	nAP -= GetMinAPToShoot( nUnitAP );
	while ( nAP >= 0 )
	{
		++nRes;
		nAP -= GetBurstAP();
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetAmmoCountPerShot( int nUnitAP ) const
{
	switch( GetItem()->GetShootMode() )
	{
		case NDb::SM_Snap:
		case NDb::SM_Aimed:
		case NDb::SM_Careful:
		case NDb::SM_Snipe:
			return 1;
		case NDb::SM_ShortBurst:
			return GetItem()->GetDBWeapon()->nRoF / 6;
		case NDb::SM_LongBurst:
			return GetLongBurstAmmoCountPerShot( nUnitAP );
	}
	//
	ASSERT( 0 && " Unknown shoot mode " );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetAmmoCountPerAP( int _nAP ) const
{
	int nRes = 0;
	int nAP = _nAP;
	int nMinAPToShoot = GetMinAPToShoot( nAP );
	// remember the number of rounds in each magazine
	list<int> AmmoCount;
	AmmoCount.push_back( GetCurrentClip()->GetAmmoCount() );
	for ( vector< CObj<CAIFireArmsWeaponClip> >::const_iterator i = clips.begin(); i != clips.end(); ++i )
		AmmoCount.push_back( (*i)->GetAmmoCount() );
	int nAmmoCount = AmmoCount.front();
	// count how many rounds we can fire
	while ( nAP > 0 )
	{
		// shots
		if ( nAmmoCount > 0 )
		{
			// check whether there are enough AP for a burst
			if ( nAP < nMinAPToShoot ) 
			{
				nAP = 0;
				break;
			}
			// fire off the rounds
			int nAmmoToShoot = min( nAmmoCount, GetAmmoCountPerShot( _nAP ) );
			for ( int k = 0; k < nAmmoToShoot; ++k )
			{
				if ( k == 0 )
					nAP -= nMinAPToShoot;
				else
					nAP -= GetBurstAP();
				//
				if ( nAP < 0 )
					break;
				//
				++nRes;
				--nAmmoCount;				
			}
		}
		// reload
		if ( nAmmoCount <= 0 )
		{
			nAP -= GetReloadAP();
			if ( nAP <= 0 )
				break;
			//
			AmmoCount.pop_front();
			if ( AmmoCount.empty() )
				break;
			nAmmoCount += AmmoCount.front();
		}
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeapon::GetShotParameters( const NAI::SUnitPosition &pos, IAIUnit *pTarget, 
	int nHitCover, int nAvailableAP, int *nAP, int *nAmmo, int *nDamage, bool *bNeedReload ) const
{
	*nAP = 0;
	*nAmmo = 0;
	*nDamage = 0;
	*bNeedReload = false;
	//
	if ( GetCurrentClip()->GetAmmoCount() <= 0 && !IsValid( GetNextClip() ) || !IsValid( pTarget ) )
		return;
	//
	int nAmmoInClip = GetCurrentClip()->GetAmmoCount();
	if ( nAmmoInClip <= 0 )
	{
		*nAP += GetReloadAP();
		nAmmoInClip = GetNextClip()->GetAmmoCount();
		*bNeedReload = true;
	}
	*nAmmo = min( nAmmoInClip, GetAmmoCountPerShot( nAvailableAP - *nAP ) );
	//
	int nBulletDamage = GetMeanDamage();
	int nExtraAP = 0;
	if ( pWeaponItem->GetShootMode() == NDb::SM_Careful )
		nExtraAP = nAvailableAP - GetShotAP()- *nAP;
	//
	for ( int n = 0; n < *nAmmo; ++n )
	{
		if ( n == 0 )
			*nAP += GetMinAPToShoot( nAvailableAP - *nAP );
		else
			*nAP += GetBurstAP();
		//
		CPtr<NRPG::CAIUnitToHitCalcer> pToHit = new NRPG::CAIUnitToHitCalcer( pOwner, 
			pos, pTarget, nHitCover, NAI::HL_ANY, n, GetItem(), nExtraAP );
		*nDamage += pToHit->GetToHit() / 100.f * nBulletDamage;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIFireArmsWeapon::GetDamage( const NAI::SUnitPosition &pos, IAIUnit *pTarget,
	int nHitCover, NAI::EPose ePose, int nAP, NDb::EShootMode eShootMode, int *nMaxToHit ) const
{
	int nRes = 0;
	NDb::EShootMode eTmpShootMode = pWeaponItem->GetShootMode();
	pWeaponItem->SetShootMode( eShootMode );
	int nDamage = GetMeanDamage();
	int nAmmoCount = GetAmmoCountPerAP( nAP );
	//
	int nExtraAP = 0;
	if ( eShootMode == NDb::SM_Careful )
		nExtraAP = max( 0, nAP - GetShotAP() );
	//
	// ‼️ RETAIL AFFORDABILITY GATE (disasm-proven @0xb5fd0, 2026-07-10): the out to-hit is ZEROED at entry
	// and accumulated (max) ONLY inside the per-bullet loop, which is bounded by GetAmmoCountPerAP(nAP) --
	// a candidate place whose leftover AP cannot fund a single shot reports nMaxToHit = 0 (and damage 0),
	// so CAIShootAction::GetInfoInner's nToHit>0 gate marks it un-shootable. The dev predecessor computed
	// an UNCONDITIONAL pre-loop to-hit (a calcer seeded from GetMaxAP-derived extra AP -- no such code
	// exists in retail): every LOS place looked shootable regardless of budget, so the place choice
	// degenerated into a to-hit hill-climb toward the enemy -- the "one step per think until AP dies"
	// creep -- while retail units hold position once no candidate can afford a shot.
	*nMaxToHit = 0;
	for ( int n = 0; n < nAmmoCount; ++n )
	{
		CPtr<NRPG::CAIUnitToHitCalcer> pToHit = new NRPG::CAIUnitToHitCalcer( pOwner,
			pos, pTarget, nHitCover, NAI::HL_ANY, n % GetAmmoCountPerShot( nAP ), GetItem(), nExtraAP );
		const int nToHit = pToHit->GetToHit();
		*nMaxToHit = max( *nMaxToHit, nToHit );
		nRes += nToHit / 100.f * nDamage;
	}
	pWeaponItem->SetShootMode( eTmpShootMode );
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIFireArmsWeapon::IsRocketLauncher() const
{
	if ( !IsValid( GetItem() ) )
		return false;
	return GetItem()->GetDBWeapon()->bBazookaLogic;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGrenadeWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IInventoryItem* CAIGrenadeWeapon::GetInventoryItem() const
{
	return CDynamicCast<NRPG::IInventoryItem>( GetItem() ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIMeleeWeaponBase (CAIMeleeWeapon / CAIThrowingWeapon)
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IInventoryItem* CAIMeleeWeaponBase::GetInventoryItem() const  // @0xb5940
{
	return CDynamicCast<NRPG::IInventoryItem>( GetItem() ).GetPtr();
}
NRPG::IInventoryItem* CAIFirstAid::GetInventoryItem() const
{
	return CDynamicCast<NRPG::IInventoryItem>( GetItem() ).GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CreateAIFireArmsWeaponClip( NRPG::CClipItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	return new CAIFireArmsWeaponClip( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb67d0: item + owner must be live and the weapon must NOT be bazooka-logic (the factories are
// symmetric -- retail returns 0 for a bazooka here and dispatches it to the rocket-launcher leaf).
CAIFireArmsWeapon* CreateAIFireArmsWeapon( IAIUnit *pOwner, NRPG::CWeaponItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	ASSERT( IsValid( pOwner ) );
	if ( !IsValid( pItem ) || !IsValid( pOwner ) )
		return 0;
	if ( pItem->GetDBWeapon()->bBazookaLogic )
		return 0;
	return new CAIFireArmsWeapon( pOwner, pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb6880: item + owner must be live and the weapon MUST be bazooka-logic.
CAIRocketLauncherWeapon* CreateAIRocketLauncherWeapon( IAIUnit *pOwner, NRPG::CWeaponItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	ASSERT( IsValid( pOwner ) );
	if ( !IsValid( pItem ) || !IsValid( pOwner ) )
		return 0;
	if ( !pItem->GetDBWeapon()->bBazookaLogic )
		return 0;
	return new CAIRocketLauncherWeapon( pOwner, pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeWeapon* CreateAIGrenadeWeapon( NRPG::CGrenadeItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	return new CAIGrenadeWeapon( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIMeleeWeapon* CreateAIMeleeWeapon( NRPG::CMeleeWeaponItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	return new CAIMeleeWeapon( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIThrowingWeapon* CreateAIThrowingWeapon( NRPG::CMeleeWeaponItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	return new CAIThrowingWeapon( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFirstAid* CreateAIFirstAid( NRPG::CFirstAidItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	return new CAIFirstAid( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52642100, CAIFireArmsWeaponBase );
REGISTER_SAVELOAD_CLASS( 0x53133163, CAIFireArmsWeapon );
REGISTER_SAVELOAD_CLASS( 0x53133162, CAIRocketLauncherWeapon );
REGISTER_SAVELOAD_CLASS( 0x52942120, CAIFireArmsWeaponClip );
REGISTER_SAVELOAD_CLASS( 0x52942121, CAIGrenadeWeapon );
REGISTER_SAVELOAD_CLASS( 0x53133160, CAIMeleeWeaponBase );
REGISTER_SAVELOAD_CLASS( 0x52533160, CAIMeleeWeapon );
REGISTER_SAVELOAD_CLASS( 0x53133161, CAIThrowingWeapon );
REGISTER_SAVELOAD_CLASS( 0x53133150, CAIFirstAid );
