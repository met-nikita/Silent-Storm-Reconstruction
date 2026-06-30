#include "StdAfx.h"
#include "RPGItem.h"
#include "RPGItemSet.h"
#include "aiPosition.h"
#include "RPGAttackMech.h"
#include "..\DBFormat\DataRPG.h"
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

	float fUW = pInnerClip->GetDBAmmo()->fUnitWeight;
	int nK = (int)( fUW * pDBWeapon->nInitialVelocity );
	pRes->push_back( CAttackPortion( nK, pInnerClip->GetDBAmmo()->nBulletType, info.nDmgMin, info.nDmgMax,
	info.nArmorPiercingAbility, 0 ) ); // вероятность и сложность critical вычисляется в RPGUnitMission
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
	// если патроны в рожке не закончились, то туда можно сыпать только тот же тип патронов
	bool bSameColor = bCheckSameColor || pInnerClip->GetIncQuantity() > 0;
	// Ищем обойму такого же типа как сейчас в оружии
	// ищем в slot-ах
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
	// ищем в рюкзаке
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
// CMeleeWeaponItem
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMeleeWeaponItem::CreateNewAttackPortion( vector<CAttackPortion> *pRes )
{
	pRes->push_back( CAttackPortion( 
		110, 2, 
		pDBMelee->nDmgMin, pDBMelee->nDmgMax, 
		0, 0 ) ); // piercing ability(+str*10), вероятность и сложность critical вычисляется в RPGUnitMission
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
CFirstAidItem::CFirstAidItem( NDb::CRPGFirstAid *_pDBFirstAid )
	:CInventoryItem( _pDBFirstAid->pItem ), pDBFirstAid(_pDBFirstAid) 
{
	pClip = new CPotionContainer();
	CPotionItem *pI = new CPotionItem();
	pI->nQuantity = _pDBFirstAid->nQuantity;
	pClip->Load( pI );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFirstAidItem::SpendPotion()
{
	ASSERT( !IsEmpty() );
	if ( !IsEmpty() )
		return;
	CObj<IJoinSplit> pSpent = pClip->SplitItem( 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFirstAidItem::IsEmpty()
{
	if ( !IsValid( pClip ) )
	{
		ASSERT(0);
		return true;
	}
	return pClip->GetIncQuantity() <= 0;
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
		// ищем подходящие патроны
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
		// В базе данных нет подходящего clip-а
		// Ошибка дизайнеров
		ASSERT( 0 );
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CClueItem: public CInventoryItem
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
REGISTER_SAVELOAD_CLASS( 0xA1112153, CFirstAidItem )
REGISTER_SAVELOAD_CLASS( 0xA1512132, CMeleeWeaponItem )
REGISTER_SAVELOAD_CLASS( 0xA2312140, CPotionItem )
REGISTER_SAVELOAD_CLASS( 0xA2312141, CPotionContainer )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x11462170, CMineDetectorItem, CSomeItem )
REGISTER_SAVELOAD_CLASS( 0x51012110, CClueItem )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x018c2110, CMineItem, CSomeItem )
REGISTER_SAVELOAD_TEMPL_CLASS( 0x024c2141, CKeyItem, CSomeItem )
REGISTER_SAVELOAD_CLASS( 0x024c2140, CToolItem )