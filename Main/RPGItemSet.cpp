#include "StdAfx.h"
#include "RPGItem.h"
#include "RPGItemSet.h"
#include "aiPosition.h"
#include "RPGAttackMech.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMisc.h"   // NDb::CRPGPicklock (CPicklockItem ctor + the CreateItem cascade)
#include "..\DBFormat\DataPerk.h"
#include "rpgUnit.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItem
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItem::Join( IJoinSplit *_pItem )
{
	if ( typeid(this) != typeid(_pItem) )
		return false;
	CDynamicCast<CItem> pItem(_pItem);
	ASSERT( pItem );
	if ( pItem )
		nQuantity += pItem->GetQuantity();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IJoinSplit *CItem::Split( int nQuantityToGet )
{
	if ( nQuantityToGet > nQuantity - 1 )
		nQuantityToGet = nQuantity - 1;
	nQuantity -= nQuantityToGet;
	CItem *pNewItem = dynamic_cast<CItem*>(MakeCopy());
	pNewItem->nQuantity = nQuantityToGet;
	return pNewItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInventoryItem
////////////////////////////////////////////////////////////////////////////////////////////////////
int CInventoryItem::GetWeight() const 
{ 
	return GetDBItem()->nWeight; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<int>& CInventoryItem::GetSize() const 
{ 
	return GetDBItem()->sSize; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CInventoryItem::GetWeaponType() const
{
	return NDb::WT_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClipItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CClipItem::CClipItem( NDb::CRPGClip *pClip ): 
	CItemContainer<CAmmoItem>( pClip->pItem ), pDBClip( pClip ), nMaxAmmoQuantity( 0 )
{
	ASSERT( IsValid( pClip ) );
	if ( IsValid( pClip ) )
		SetMaxIncQuantity( pDBClip->nQuantity );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CClipItem::GetDBClipID() const 
{ 
	if ( IsValid( pDBClip ) ) 
		return pDBClip->GetRecordID(); 
	return -1; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGAmmo* CClipItem::GetDBAmmo() const 
{ 
	return GetItem()->GetDBAmmo(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CClipItem::GetMaxIncQuantity() const
{
	return nMaxAmmoQuantity;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipItem::SetMaxIncQuantity( int _nMaxAmmoQuantity )
{
	nMaxAmmoQuantity = _nMaxAmmoQuantity;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClipItem::IsCompatible( CClipItem *pClip, bool bCheckSameColor )
{
	return ( GetDBClip()->nAmmoGroup == pClip->GetDBClip()->nAmmoGroup &&
		( !bCheckSameColor || GetDBAmmo()->color == pClip->GetDBAmmo()->color ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipItem::LoadAmmoFromClip( CClipItem *pClip )
{
	ASSERT( IsValid( pClip ) );
	if ( !IsValid( pClip ) )
		return;
	ASSERT( IsCompatible( pClip, false ) );
	if ( !IsCompatible( pClip, false ) )
		return;
	//
	if ( pClip->GetIncQuantity() <= 0 )
		return;
	//
	if ( GetDBAmmo()->color != pClip->GetDBAmmo()->color )
	{
		ASSERT( GetIncQuantity() == 0 );
		CPtr<CAmmoItem> pNewAmmoItem = new CAmmoItem( pClip->GetDBAmmo() );
		Load( pNewAmmoItem );
	}
	int nNeed = GetMaxIncQuantity() - GetIncQuantity();
	int nHave = pClip->GetIncQuantity();
	CPtr<IJoinSplit> pGet = pClip->SplitItem( min( nNeed, nHave ) );
	if ( IsValid( pGet ) )
		JoinItem( pGet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWeaponItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CWeaponItem::CWeaponItem( NDb::CRPGWeapon *_pWeapon )
	: CInventoryItem( _pWeapon->pItem ), pDBWeapon( _pWeapon ), bWorking(true)
{
	for ( int i = 0; i < NDb::SM_MAXVALUE; i++ )
	{
		if ( IsShootModeSupported( NDb::EShootMode( i ) ) )
		{
			eShootMode = NDb::EShootMode( i );
			break;
		}
	}
	//
	int nAmmoQuantity = pDBWeapon->pInnerClip->nQuantity;
	if ( pDBWeapon->nInnerClipAmmoQuantity > 0 )
		nAmmoQuantity = pDBWeapon->nInnerClipAmmoQuantity;
	CDynamicCast<CClipItem> pTmpClip(CreateClipItem(pDBWeapon->pInnerClip, 0, nAmmoQuantity));
	if (pTmpClip)
	{
		pInnerClip = pTmpClip;
		pInnerClip->SetMaxIncQuantity( nAmmoQuantity );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::SetShootMode( NDb::EShootMode _eShootMode )
{
	if ( !IsShootModeSupported( _eShootMode ) )
		return false;

	eShootMode = _eShootMode;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::IsShootModeSupported( NDb::EShootMode eMode ) const
{
	return pDBWeapon->shootModes[eMode];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWeaponItem::GetInfo( SWeaponInfo *pInfo ) const
{
	ASSERT( pInfo );
	if ( pInfo == 0 )
		return;
	//
	CPtr<NDb::CRPGAmmo> pAmmo = GetInnerClip()->GetDBAmmo();
	int nDmgMod = pDBWeapon->nDamageMod;
	pInfo->nRoF = Float2Int( float( pDBWeapon->nRoF ) / 6 );
	pInfo->nDmgMin = ( pAmmo->nDmgMin * pDBWeapon->nDamageMod ) / 100;
	pInfo->nDmgMax = ( pAmmo->nDmgMax * pDBWeapon->nDamageMod ) / 100;
	pInfo->nArmorPiercingAbility = int( float(pDBWeapon->nInitialVelocity) * pAmmo->fUnitWeight );
	pInfo->nShotAP = pDBWeapon->nShotAP;
	pInfo->nTargetingAP = pDBWeapon->nTargetingAP;
	pInfo->nQuality = pDBWeapon->nQuality;
	pInfo->nMinRange = pDBWeapon->nMinRange;
	pInfo->nMaxRange = pDBWeapon->nMaxRange;
	pInfo->nRecoil = pDBWeapon->nRecoil;
	pInfo->fScopeFactor = (pDBWeapon->bScope) ? 100.f : 20.f;
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::ESkillType CWeaponItem::GetSkillIndex() const
{
	return (NDb::ESkillType)(NDb::ST_MELEE + pDBWeapon->pWeaponType->nSkillIndex);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWeaponItem::GetShootAP() const
{
	return pDBWeapon->nShotAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWeaponItem::GetReloadAP() const
{
	return pDBWeapon->nReloadAP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWeaponItem::GetAmmoQuantity() const
{
	ASSERT( IsValid( pInnerClip ) );
	if ( IsValid( pInnerClip ) )
		return pInnerClip->GetIncQuantity();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::HasAmmo() const
{
	return GetAmmoQuantity() > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWeaponItem::CreateNewAttackPortion( vector<CAttackPortion> *pRes, bool bSpendAmmo )
{
	if ( !IsWorking() )
		return;
	SWeaponInfo info;
	GetInfo( &info );

	if ( bSpendAmmo )
	{
		ASSERT( IsValid( pInnerClip ) );
		if ( pInnerClip->GetIncQuantity() == 0 )
			return;

		CObj<IJoinSplit> pSpent = pInnerClip->SplitItem( 1 );
	}

	NDb::CRPGAmmo *pAmmo = pInnerClip->GetDBAmmo();
	float fUW = pAmmo->fUnitWeight;
	int nK = (int)( fUW * pDBWeapon->nInitialVelocity );
	// retail @0x2a09e0 (disasm 0x6a0add..0x6a0b12): the corpse-push coefficient is the bullet's
	// momentum-like product calibr^2 * PI * muzzle velocity * bullet weight * 2.5e-7 (0x348637bd).
	// Firearms are the ONLY retail attack source with a non-zero fPushCoeff -- this is what makes
	// a sniper/MG kill fling the corpse while a pistol kill just slumps it.
	float fPushCoeff = pAmmo->fCalibr * pAmmo->fCalibr * FP_PI *
		( (float)pDBWeapon->nInitialVelocity * pAmmo->fWeight ) * 2.5e-7f;
	pRes->push_back( CAttackPortion( nK, pAmmo->nBulletType, fPushCoeff, info.nDmgMin, info.nDmgMax,
	info.nArmorPiercingAbility, 0 ) ); // ����������� � ��������� critical ����������� � RPGUnitMission
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWeaponItem::GetClipType() const
{
	if ( !IsValid( pInnerClip )  )
		return -1;
	return pInnerClip->GetDBClipID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFindClipResult
{
	enum ESource
	{
		SLOT,
		BACKPACK
	};

	ESource eSource;
	NDb::ESlot eSlot;
	CPtr<CClipItem> pItem;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void EraseFindResultItem( IInventory *pInventory, const SFindClipResult &sResult )
{
	switch( sResult.eSource )
	{
	case SFindClipResult::SLOT:
		{
			pInventory->TakeOff( sResult.eSlot );
			break;
		}
	case SFindClipResult::BACKPACK:
		{
			pInventory->Take( sResult.pItem );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::FindProperClip( IInventoryInfo *pInventory, 
	SFindClipResult *pResult, bool bCheckSameColor ) const
{
	ASSERT( pResult );
	// ���� ������� � ����� �� �����������, �� ���� ����� ������ ������ ��� �� ��� ��������
	bool bSameColor = bCheckSameColor || pInnerClip->GetIncQuantity() > 0;
	// ���� ������ ������ �� ���� ��� ������ � ������
	// ���� � slot-��
	for ( int i = 0; i < NDb::N_SLOTS; ++i )
	{
		CDynamicCast<CClipItem> pClip(pInventory->Get(NDb::ESlot(i)));
		if (pClip)
		{
			if ( pInnerClip->IsCompatible( pClip, bSameColor ) )
			{
				pResult->eSource = SFindClipResult::SLOT;
				pResult->eSlot = NDb::ESlot(i);
				pResult->pItem = pClip;
				return true;
			}
		}
	}
	// ���� � �������
	const vector<SBackPackItem> &items = pInventory->GetItems();
	for ( int i = 0; i < items.size(); ++i )
	{
		CDynamicCast<CClipItem> pClip(items[i].pItem);
		if (pClip)
		{
			if ( pInnerClip->IsCompatible( pClip, bSameColor ) )
			{
				pResult->eSource = SFindClipResult::BACKPACK;
				pResult->pItem = pClip;
				return true;
			}
		}
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::CanLoad( IClipItem *pClip ) const
{
	CDynamicCast<NRPG::CClipItem> pClipItem( pClip );
	if ( !IsValid( pClipItem ) )
		return false;

	if ( !pInnerClip->IsCompatible( pClipItem, false ) )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::CanReload( IInventoryInfo *pInventory ) const
{
	SFindClipResult sResult;
	if ( GetInnerClip()->GetIncQuantity() > 0 )
		return FindProperClip( pInventory, &sResult, true );
	else
		return FindProperClip( pInventory, &sResult, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::Load( IClipItem *pClip )
{
	CDynamicCast<NRPG::CClipItem> pClipItem( pClip );
	if ( !IsValid( pClipItem ) )
		return false;

	if ( !pInnerClip->IsCompatible( pClipItem, false ) )
		return false;

	pInnerClip->LoadAmmoFromClip( pClipItem );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::Reload( IInventory *pInventory )
{
	SFindClipResult sResult;
	if ( !FindProperClip( pInventory, &sResult, false ) )
		return false;
	//
	bool bSameColor = FindProperClip( pInventory, &sResult, true );
	while ( pInnerClip->GetIncQuantity() < pInnerClip->GetMaxIncQuantity() && 
		FindProperClip( pInventory, &sResult, bSameColor ) )
	{
		pInnerClip->LoadAmmoFromClip( sResult.pItem );
		if ( sResult.pItem->GetIncQuantity() <= 0 )
			EraseFindResultItem( pInventory, sResult );
		bSameColor = true;
	}
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWeaponItem::Unload( IInventory *pInventory )
{
	ASSERT( IsValid( pInventory ) );
	if ( !IsValid( pInventory ) || pInnerClip->GetIncQuantity() <= 0 )
		return false;
	//
	while ( pInnerClip->GetIncQuantity() > 0 )
	{
		CDynamicCast<CClipItem> pUnloadedClip(CreateClipItem(pInnerClip->GetDBClip(),
			pInnerClip->GetDBAmmo(), 0));
		if (pUnloadedClip)
		{
			int nUnload = min( pInnerClip->GetIncQuantity(), pInnerClip->GetDBClip()->nQuantity );
			CPtr<IJoinSplit> pGet = pInnerClip->SplitItem( nUnload );
			if ( IsValid( pGet ) )
				pUnloadedClip->JoinItem( pGet );
			pInventory->Place( CTPoint<int>( -1, -1 ), pUnloadedClip );
		}
		return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CWeaponItem::GetWeaponType() const
{
	NDb::EWeaponType eType = pDBWeapon->eWeaponType;
	if ( ( eType == NDb::WT_DEFAULT ) && IsValid( pDBWeapon->pAnimWeaponType ) )
		return pDBWeapon->pAnimWeaponType->type;

	return eType;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrenadeItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrenadeItem::CGrenadeItem( NDb::CRPGGrenade *_pDBGrenade )
	: CInventoryItem( _pDBGrenade->pItem ), pDBGrenade(_pDBGrenade), eMode( GM_THROW )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2a11d0: the engineer-grenade flavour -- same CInventoryItem base off the record's
// pItem, eMode=GM_THROW, pDBGrenade stays null, the record lands in pDBEngGrenade (tag 4).
CGrenadeItem::CGrenadeItem( NDb::CRPGEngGrenade *_pDBEngGrenade )
	: CInventoryItem( _pDBEngGrenade->pItem ), pDBGrenade(0), eMode( GM_THROW ), pDBEngGrenade(_pDBEngGrenade)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMeleeWeaponItem
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMeleeWeaponItem::CreateNewAttackPortion( vector<CAttackPortion> *pRes )
{
	pRes->push_back( CAttackPortion(
		110, 2, 0.0f, // retail @0x2a0850 passes fPushCoeff=0 -- melee kills never push the corpse
		pDBMelee->nDmgMin, pDBMelee->nDmgMax,
		0, 0 ) ); // piercing ability(+str*10), ����������� � ��������� critical ����������� � RPGUnitMission
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CMeleeWeaponItem::GetWeaponType() const
{
	NDb::EWeaponType eType = pDBMelee->eWeaponType;
	if ( ( eType == NDb::WT_DEFAULT ) && IsValid( pDBMelee->pAnimWeaponType ) )
		return pDBMelee->pAnimWeaponType->type;

	return eType;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2a17a0: chain the container base on the shared CRPGItem, then load ONE CSimpleCharge
// seeded with the record's full quantity (NOT a loop of N charges).
CFirstAidItem::CFirstAidItem( NDb::CRPGFirstAid *_pDBFirstAid )
	: TChargeContainer( _pDBFirstAid->pItem ), pDBFirstAid( _pDBFirstAid )
{
	CSimpleCharge *pI = new CSimpleCharge();
	pI->nQuantity = _pDBFirstAid->nQuantity;
	Load( pI );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2a0740 (IItemContainerInfo slot 1): the charge capacity is the record's nQuantity
int CFirstAidItem::GetMaxIncQuantity() const
{
	return pDBFirstAid->nQuantity;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMeleeWeaponItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CMeleeWeaponItem::CMeleeWeaponItem( NDb::CRPGMeleeWeapon *_pDBMelee )
	: CInventoryItem( _pDBMelee->pItem ), pDBMelee(_pDBMelee) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CreateClipItem( NDb::CRPGClip *pDBClip, NDb::CRPGAmmo *pDBAmmo, int nAmmoQuantity )
{
	ASSERT( IsValid( pDBClip ) );
	if ( !IsValid( pDBClip ) )
		return 0;
	//
	CDBPtr<NDb::CRPGAmmo> pTmpDBAmmo = pDBAmmo;
	if ( !IsValid( pTmpDBAmmo ) )
	{
		// ���� ���������� �������
		CDBTable<NDb::CRPGAmmo> *pAmmoTable = NDatabase::GetTable<NDb::CRPGAmmo>();
		CDBIterator<NDb::CRPGAmmo> i(*pAmmoTable);
		while ( pAmmoTable && i.MoveNext() )
		{
			CDBPtr<NDb::CRPGAmmo> pTableRecord = i.Get();
			if ( pTableRecord->nAmmoGroup == pDBClip->nAmmoGroup )
			{
				pTmpDBAmmo = pTableRecord;
				break;
			}
		}
	}
	if ( IsValid( pTmpDBAmmo ) )
	{
		CClipItem *pClip = new CClipItem( pDBClip );
		if ( IsValid( pClip ) )
		{
			CPtr<NRPG::CAmmoItem> pAmmo = new CAmmoItem( pTmpDBAmmo );
			pAmmo->nQuantity = nAmmoQuantity < 0 ? pClip->GetMaxIncQuantity() : nAmmoQuantity;
			pClip->Load( pAmmo );
		}
		return pClip;
	}
	else
	{
		// � ���� ������ ��� ����������� clip-�
		// ������ ����������
		ASSERT( 0 );
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::CSimpleItem<NRPG::IClueItem> (ctors @0x2a58e0/@0x2a4b90/@0x2a8800): the IClueItem
// marker base is what CMissionUI::UpdateVisibleItems @0x2130c0 RTDynamicCast-detects to raise the
// in-world CClueIcon marker over a discovered clue pickup. No data of its own beyond CInventoryItem.
// (Save id 0x51012110 is this fork's established one -- retail registers the class as 0xB3212130,
// register thunk @0x8a3a40 -- kept for save compatibility with existing dev saves.)
class CClueItem: public CInventoryItem, public IClueItem
{
	OBJECT_BASIC_METHODS( CClueItem );
	ZDATA
	ZPARENT( CInventoryItem );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CInventoryItem *)this); return 0; }
	//
public:
	CClueItem() {}
	CClueItem( NDb::CRPGItem *_pItem ): CInventoryItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::CSimpleItem<NRPG::IHintItem> (ctors @0x2a5a20/@0x2a4d40/@0x2a8e20): the in-world
// "hint" pickup item. Identical shape to the clue item; the IHintItem marker base is what
// UpdateVisibleItems @0x2130c0 detects to raise a CHintIcon (gated by "ui_showhints").
class CHintItem: public CInventoryItem, public IHintItem
{
	OBJECT_BASIC_METHODS( CHintItem );
	ZDATA
	ZPARENT( CInventoryItem );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CInventoryItem *)this); return 0; }
	//
public:
	CHintItem() {}
	CHintItem( NDb::CRPGItem *_pItem ): CInventoryItem( _pItem ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToolItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CToolItem::CToolItem( NDb::CRPGTool *_pDBTool ): 
	CInventoryItem( _pDBTool->pItem ), pDBTool( _pDBTool ) 
{ 
	nQuantity = pDBTool->nCharges;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CToolItem::CanBeUsed( NRPG::CUnit *pUnit ) const
{
	CDBPtr<NDb::CRPGTool> pTool = GetDBItemInfo();
	ASSERT( IsValid( pTool ) );
	ASSERT( IsValid( pUnit ) );
	if ( IsValid( pTool ) && IsValid( pUnit ) )
	{
		bool bHasPerk = true;
		if ( IsValid( pTool->pNeededPerk ) && !pUnit->HasPerk( pTool->pNeededPerk->GetRecordID() ) )
			bHasPerk = false;
		bool bHasSkill = pUnit->Skills( NDb::ST_ENGINEERING ) >= pTool->nNeededEngSkill;
		return bHasPerk && bHasSkill && GetQuantity() > 0;
	}
	else
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CToolItem::GetSkillModifForMineCleaning() const
{
	CDBPtr<NDb::CRPGTool> pTool = GetDBItemInfo();
	ASSERT( IsValid( pTool ) );
	if ( IsValid( pTool ) && pTool->bCanUseForMineCleaning )
		return pTool->nSkillModifForMineCleaning;
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPicklockItem
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2a1c80: container base off the record's pItem, the record in pDBPicklock, and ONE
// CSimpleCharge loaded with the record's nQuantity as its charge count.
CPicklockItem::CPicklockItem( NDb::CRPGPicklock *_pDBPicklock ):
	TChargeContainer( _pDBPicklock->pItem ), pDBPicklock( _pDBPicklock )
{
	CSimpleCharge *pCharge = new CSimpleCharge();
	pCharge->nQuantity = _pDBPicklock->nQuantity;
	Load( pCharge );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPicklockItem::GetMaxIncQuantity() const   // retail @0x2a0750
{
	return pDBPicklock->nQuantity;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPicklockItem::operator&( CStructureSaver &f )   // retail @0x2abaa0
{
	f.Add( 2, (TChargeContainer*)this );
	f.Add( 3, &pDBPicklock );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CreateClueItem( NDb::CRPGItem *pDBItem )
{
	ASSERT( IsValid( pDBItem ) );
	if ( !IsValid( pDBItem ) )
		return 0;
	//
	return new CClueItem( pDBItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NRPG::CreateHintItem @0x2a21a0 (disasm 0x6a21d3: mov ecx,0x1b6; push 1 -> GetRPGItem(0x1b6,1)):
// every in-world hint pickup wraps the FIXED db RPG item 0x1b6 in a CSimpleItem<IHintItem>.
// (The null guard mirrors the dev CreateClueItem shape; retail's GetRPGItem second arg 1 is its
// assert-on-missing flag.)
IInventoryItem *CreateHintItem()
{
	NDb::CRPGItem *pDBItem = NDb::GetRPGItem( 0x1b6 );
	ASSERT( IsValid( pDBItem ) );
	if ( !IsValid( pDBItem ) )
		return 0;
	//
	return new CHintItem( pDBItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IWeaponItem *CreateWeaponItem( NDb::CRPGWeapon *pDBWeapon )
{
	ASSERT( IsValid( pDBWeapon ) );
	if ( !IsValid( pDBWeapon ) )
		return 0;
	//
	return new CWeaponItem( pDBWeapon );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static IInventoryItem *CreateGrenadeItem( NDb::CRPGGrenade *pDBGrenade )
{
	CGrenadeItem *pGrenade = new CGrenadeItem(pDBGrenade);
	return pGrenade;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2a1540: the engineer-grenade overload (CGrenadeItem with pDBEngGrenade set)
static IInventoryItem *CreateGrenadeItem( NDb::CRPGEngGrenade *pDBEngGrenade )
{
	CGrenadeItem *pGrenade = new CGrenadeItem(pDBEngGrenade);
	return pGrenade;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Which-record grenade field accessors (see RPGItem.h): retail reads these fields through a
// GetDBGrenade()-else-GetDBEngGrenade() branch at every consumer.
NDb::CRPGWeaponType *GetGrenadeRecWeaponType( IGrenadeItemInfo *pGrenade )
{
	if ( NDb::CRPGGrenade *pDB = pGrenade->GetDBGrenade() )
		return pDB->pWeaponType;
	return pGrenade->GetDBEngGrenade()->pWeaponType;
}
int GetGrenadeRecQuality( IGrenadeItemInfo *pGrenade )
{
	if ( NDb::CRPGGrenade *pDB = pGrenade->GetDBGrenade() )
		return pDB->nQuality;
	return pGrenade->GetDBEngGrenade()->nQuality;
}
int GetGrenadeRecMaxDelay( IGrenadeItemInfo *pGrenade )
{
	if ( NDb::CRPGGrenade *pDB = pGrenade->GetDBGrenade() )
		return pDB->nMaxDelay;
	return pGrenade->GetDBEngGrenade()->nMaxDelay;
}
NDb::CRPGItem *GetGrenadeRecItem( IGrenadeItemInfo *pGrenade )
{
	if ( NDb::CRPGGrenade *pDB = pGrenade->GetDBGrenade() )
		return pDB->pItem;
	return pGrenade->GetDBEngGrenade()->pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static IInventoryItem *CreateUniformItem( NDb::CRPGUniform *pDBUniform )
{
	//CUniformItem *pUniform = new CUniformItem(pDBUniform);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static IInventoryItem* CreateFirstAidItem( NDb::CRPGFirstAid *pDBFirstAid )
{
	CFirstAidItem *pFA = new CFirstAidItem(pDBFirstAid);
	return pFA;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem* CreateMeleeWeaponItem( NDb::CRPGMeleeWeapon *pDBMelee )
{
	CMeleeWeaponItem *pMW = new CMeleeWeaponItem(pDBMelee);
	return pMW;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem* CreateItem( CDBRecord *pItem )
{
	IInventoryItem *pIItem = 0;
	CDynamicCast<NDb::CRPGWeapon> pWeapon(pItem);
	if(pWeapon)
		pIItem = CreateWeaponItem( pWeapon );
	else {
		CDynamicCast<NDb::CRPGClip> pClip(pItem);
		if (pClip)
			pIItem = CreateClipItem(pClip);
		else {
			CDynamicCast<NDb::CRPGGrenade> pGrenade(pItem);
			if (pGrenade)
				pIItem = CreateGrenadeItem(pGrenade);
			else {
				// retail CreateItem @0x2a25f0: the engineer-grenade branch sits right after the
				// plain grenade one. Its absence made getitem on an EngGrenade id (e.g. 431)
				// return NULL -> CanPlace(NULL) crash.
				CDynamicCast<NDb::CRPGEngGrenade> pEngGrenade(pItem);
				if (pEngGrenade) {
					pIItem = CreateGrenadeItem(pEngGrenade);
					return pIItem;
				}
				CDynamicCast<NDb::CRPGFirstAid> pFirstAid(pItem);
				if (pFirstAid)
					pIItem = CreateFirstAidItem(pFirstAid);
				else {
					CDynamicCast<NDb::CRPGMeleeWeapon> pMelee(pItem);
					if (pMelee)
						pIItem = CreateMeleeWeaponItem(pMelee);
					else {
						CDynamicCast<NDb::CRPGMineDetector> pMD(pItem);
						if (pMD)
							pIItem = new CMineDetectorItem(pMD);
						else {
							CDynamicCast<NDb::CRPGMine> pDB(pItem);
							if (pDB)
								pIItem = new CMineItem(pDB);
							else {
								CDynamicCast<NDb::CRPGTool> pDB(pItem);
								if (pDB)
									pIItem = new CToolItem(pDB);
								else {
									CDynamicCast<NDb::CRPGKey> pDB(pItem);
									if (pDB)
										pIItem = new CKeyItem(pDB);
									else {
										// retail CreateItem @0x2a25f0: the last cascade rung is
										// the picklock (new CPicklockItem, 0x68 bytes); anything
										// else returns NULL in retail too.
										CDynamicCast<NDb::CRPGPicklock> pPicklock(pItem);
										if (pPicklock)
											pIItem = new CPicklockItem(pPicklock);
										else
											ASSERT(0);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return pIItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NRPG;
BASIC_REGISTER_CLASS(IClipItem)
BASIC_REGISTER_CLASS(IWeaponItem)
BASIC_REGISTER_CLASS(IWeaponItemInfo)
REGISTER_SAVELOAD_CLASS( 0xE10A1140, CWeaponItem )
REGISTER_SAVELOAD_CLASS( 0xE10A1141, CClipItem )
REGISTER_SAVELOAD_CLASS( 0xE10A1142, CAmmoItem )
REGISTER_SAVELOAD_CLASS( 0x101B1170, CGrenadeItem )
REGISTER_SAVELOAD_CLASS( 0xA1112153, CFirstAidItem )    // retail id (registrar $E35 @0x4a37a0)
REGISTER_SAVELOAD_CLASS( 0xA1512132, CMeleeWeaponItem )
REGISTER_SAVELOAD_CLASS( 0xA2312140, CSimpleCharge )    // retail id (reg thunk @0x4a3870; Jan03 name CPotionItem)
// 0xA2312141 CPotionContainer DELETED with the first-aid container refactor (dev-only id, absent in retail)
REGISTER_SAVELOAD_CLASS( 0xA0523091, CPicklockItem )    // retail id (reg thunk @0x4a3a00)
REGISTER_SAVELOAD_TEMPL_CLASS( 0x11462170, CMineDetectorItem, CSomeItem )
REGISTER_SAVELOAD_CLASS( 0x51012110, CClueItem )
REGISTER_SAVELOAD_CLASS( 0xB3212131, CHintItem )	// retail CSimpleItem<IHintItem> id (register thunk @0x8a3a70)
REGISTER_SAVELOAD_TEMPL_CLASS( 0x018c2110, CMineItem, CSomeItem )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x024c2141, CKeyItem, CSomeItem )
REGISTER_SAVELOAD_CLASS( 0x024c2140, CToolItem )