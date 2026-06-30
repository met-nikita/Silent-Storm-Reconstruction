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
CAIFireArmsWeapon::CAIFireArmsWeapon( IAIUnit *_pOwner, NRPG::CWeaponItem *_pWeaponItem ):
	pOwner( _pOwner ), pWeaponItem( _pWeaponItem ), pCurrentClip( 0 )
{
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
NRPG::IInventoryItem* CAIFireArmsWeapon::GetInventoryItem() const
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
int CAIFireArmsWeapon::GetClipCount() const 
{
	return clips.size(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CAIFireArmsWeapon::GetCurrentClip() const 
{ 
	return pCurrentClip; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeapon::SetCurrentClip( CAIFireArmsWeaponClip *pClip ) 
{ 
	pCurrentClip = pClip; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIFireArmsWeaponClip* CAIFireArmsWeapon::GetNextClip() const
{
	return clips.empty() ? 0 : clips.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeapon::RemoveClip( CAIFireArmsWeaponClip *pClip )
{
	ASSERT( IsValid( pClip ) );
	ASSERT( find( clips.begin(), clips.end(), pClip ) != clips.end() );
	clips.erase( remove( clips.begin(), clips.end(), pClip ), clips.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFireArmsWeapon::AddClip( CAIFireArmsWeaponClip *pClip )
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
bool CAIFireArmsWeapon::IsSuitableClip( CAIFireArmsWeaponClip *pClip ) const
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
	// ���������� ���-�� �������� � ������ ������
	list<int> AmmoCount;
	AmmoCount.push_back( GetCurrentClip()->GetAmmoCount() );
	for ( vector< CObj<CAIFireArmsWeaponClip> >::const_iterator i = clips.begin(); i != clips.end(); ++i )
		AmmoCount.push_back( (*i)->GetAmmoCount() );
	int nAmmoCount = AmmoCount.front();
	// ������� ������� �������� ����� ����������
	while ( nAP > 0 )
	{
		// ��������
		if ( nAmmoCount > 0 )
		{
			// ��������� ������-�� AP �� �������
			if ( nAP < nMinAPToShoot ) 
			{
				nAP = 0;
				break;
			}
			// ������������ �������
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
		// �����������
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
	int nTmpExtraAP = 0;
	if ( eShootMode == NDb::SM_Careful )
	{
		nExtraAP = max( 0, nAP - GetShotAP() );
		nTmpExtraAP = max( 0, pOwner->GetMaxAP() - GetShotAP() );
	}
	//
	CPtr<NRPG::CAIUnitToHitCalcer> pTmpToHit = new NRPG::CAIUnitToHitCalcer( pOwner, 
		pos, pTarget, nHitCover, NAI::HL_ANY, 0, GetItem(), nTmpExtraAP );
	*nMaxToHit = pTmpToHit->GetToHit();
	//
	for ( int n = 0; n < nAmmoCount; ++n )
	{
		CPtr<NRPG::CAIUnitToHitCalcer> pToHit = new NRPG::CAIUnitToHitCalcer( pOwner, 
			pos, pTarget, nHitCover, NAI::HL_ANY, n % GetAmmoCountPerShot( nAP ), GetItem(), nExtraAP );
		nRes += pToHit->GetToHit() / 100.f * nDamage;
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
// CAIMeleeWeapon / CAIThrowingWeapon
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IInventoryItem* CAIMeleeWeapon::GetInventoryItem() const
{
	return CDynamicCast<NRPG::IInventoryItem>( GetItem() ).GetPtr();
}
NRPG::IInventoryItem* CAIThrowingWeapon::GetInventoryItem() const
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
CAIFireArmsWeapon* CreateAIFireArmsWeapon( IAIUnit *pOwner, NRPG::CWeaponItem *pItem )
{
	ASSERT( IsValid( pItem ) );
	ASSERT( IsValid( pOwner ) );
	return new CAIFireArmsWeapon( pOwner, pItem );
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
REGISTER_SAVELOAD_CLASS( 0x52642100, CAIFireArmsWeapon );
REGISTER_SAVELOAD_CLASS( 0x52942120, CAIFireArmsWeaponClip );
REGISTER_SAVELOAD_CLASS( 0x52942121, CAIGrenadeWeapon );
REGISTER_SAVELOAD_CLASS( 0x52942122, CAIMeleeWeapon );
REGISTER_SAVELOAD_CLASS( 0x52942123, CAIThrowingWeapon );
REGISTER_SAVELOAD_CLASS( 0x52942124, CAIFirstAid );