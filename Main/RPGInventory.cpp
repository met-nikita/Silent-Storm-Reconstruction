#include "StdAfx.h"
#include "InterfaceConst.h"
#include "RPGItemSet.h"
#include "RPGUnit.h"
#include "..\Misc\2DArray.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� �������� ���������
class CInventory: public IInventory
{
	OBJECT_BASIC_METHODS(CInventory);
	ZDATA
	int nActiveSlot;
	CPtr<CUnit> pOwner;
	CArray2D<bool> backpackMap;
	CObj<IInventoryItem> pHandItem;
	vector<SBackPackItem> items;
	vector< CObj<IInventoryItem> > slots;
	CDBPtr<NDb::CPanzerklein> pPK;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nActiveSlot); f.Add(3,&pOwner); f.Add(4,&backpackMap); f.Add(5,&pHandItem); f.Add(6,&items); f.Add(7,&slots); f.Add(8,&pPK); return 0; }

public:
	CInventory( CUnit *pOwner = 0 );
	
	virtual void Take( IInventoryItem *pItem );
	virtual bool CanPlace( const CTPoint<int> &sPos, const IInventoryItem *pItem, bool bCheckSpace = true ) const;
	virtual bool FindPlace( const IInventoryItem *pItem, CTPoint<int> *pPos ) const;
	virtual bool Place( const CTPoint<int> &sPos, IInventoryItem *pItem );
	virtual void ArrangeItems();
	virtual const vector<SBackPackItem>& GetItems() const { return items; };

	virtual bool CanEquip( NDb::ESlot where, const IInventoryItem *pWhat ) const;
	virtual bool Equip( NDb::ESlot where, IInventoryItem *pWhat );
	virtual IInventoryItem *TakeOff( NDb::ESlot where );
	virtual IInventoryItem *Get( NDb::ESlot where ) const;
	virtual bool Activate( NDb::ESlot where );
	virtual IInventoryItem *GetActive() const;
	virtual int GetActiveSlot() const;

	virtual IInventoryItem *GetHandItem() const;
	virtual void SetHandItem( IInventoryItem *pWhat );

	virtual void SetPanzerklein( NDb::CPanzerklein *_pPK, IInventory *pPKInventory ); 

	virtual int GetPlaceBySubType( NDb::EItemSubType subType ) const;
	virtual NDb::CRPGUniform* GetUniform() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInventory::CInventory( CUnit *_pOwner ): 
	pOwner(_pOwner), backpackMap( N_BACKPACK_WIDTH, N_BACKPACK_HEIGHT )
{
	slots.resize(NDb::N_SLOTS);
	backpackMap.FillZero();
	nActiveSlot = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInventory::Take( IInventoryItem *pItem )
{
	for ( vector<SBackPackItem>::iterator iTemp = items.begin(); iTemp != items.end(); iTemp++ )
	{
		if ( iTemp->pItem == pItem )
		{
			for( int nTempY = 0; nTempY < pItem->GetSize().y; nTempY++ )
			{
				for( int nTempX = 0; nTempX < pItem->GetSize().x; nTempX++ )
				{
					backpackMap[iTemp->sPos.y + nTempY][iTemp->sPos.x + nTempX] = false;
				}
			}

			items.erase( iTemp );

			return;
		}
	}

	ASSERT( 0 && "Item doesn't exist " );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::CanPlace( const CTPoint<int> &sPos, const IInventoryItem *pItem, bool bCheckSpace ) const
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + pItem->GetSize().x > backpackMap.GetXSize() ) || ( sPos.y + pItem->GetSize().y > backpackMap.GetYSize() ) )
		return false;

	if ( !bCheckSpace )
		return true;

	const CTPoint<int> &sSize = pItem->GetSize();
	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
	{
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
		{
			if ( backpackMap[sPos.y + nTempY][sPos.x + nTempX] )
				return false;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::FindPlace( const IInventoryItem *pItem, CTPoint<int> *pPos ) const
{
	for( int nTempY = 0; nTempY < backpackMap.GetYSize(); nTempY++ )
	{
		for( int nTempX = 0; nTempX < backpackMap.GetXSize(); nTempX++ )
		{
			if ( CanPlace( CTPoint<int>( nTempX, nTempY ), pItem ) )
			{
				*pPos = CTPoint<int>( nTempX, nTempY );
				return true;
			}
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::Place( const CTPoint<int> &_sPos, IInventoryItem *pItem )
{
	CTPoint<int> sPos( _sPos );

	if ( ( sPos.x == -1 ) && ( sPos.y == -1 ) && !FindPlace( pItem, &sPos ) )
		return false;

	const CTPoint<int> &sSize = pItem->GetSize();
	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
	{
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
		{
			backpackMap[sPos.y + nTempY][sPos.x + nTempX] = true;
		}
	}

	items.push_back( SBackPackItem( sPos, pItem ) );
	
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SItemsSort
{
	bool operator()( const SBackPackItem &sI1, const SBackPackItem &sI2 ) const 
	{
		const CTPoint<int> &sSize1 = sI1.pItem->GetSize();
		const CTPoint<int> &sSize2 = sI2.pItem->GetSize();
		int nW1 = Max( sSize1.x, sSize1.y ) + sSize1.x * sSize1.y;
		int nW2 = Max( sSize2.x, sSize2.y ) + sSize2.x * sSize2.y;

		return nW1 > nW2; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInventory::ArrangeItems()
{
	vector<SBackPackItem> tempItems( items );
	sort( tempItems.begin(), tempItems.end(), SItemsSort() );

	for ( int nTemp = 0; nTemp < tempItems.size(); nTemp++ )
	{
		Take( tempItems[nTemp].pItem );
	}

	backpackMap.FillZero();
	for ( int nTemp = 0; nTemp < tempItems.size(); nTemp++ )
	{
		SBackPackItem sItem = tempItems[nTemp];

		CTPoint<int> sPos;
		if ( FindPlace( sItem.pItem, &sPos ) )
			Place( sPos, sItem.pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::CanEquip( NDb::ESlot where, const IInventoryItem *pWhat ) const
{
	int nPKType = 0;
	CDynamicCast<IToolItem> pTool(pWhat);
	if (pTool)
	{
		if ( !pTool->CanBeUsed( pOwner ) )
			return false;
	}
	CDynamicCast<IMeleeWeaponItem> pMeleeWeapon(pWhat);
	if (pMeleeWeapon)
	{
		NDb::CRPGMeleeWeapon *pDBWeap = pMeleeWeapon->GetDBMeleeWeapon();
		if ( IsValid( pDBWeap ) )
			nPKType = pDBWeap->nPanzerkleinType;
	}
	CDynamicCast<IWeaponItemInfo> pWeapon(pWhat);
	if (pWeapon)
	{
		NDb::CRPGWeapon *pDBWeap = pWeapon->GetDBWeapon();
		if ( IsValid( pDBWeap ) )
			nPKType = pDBWeap->nPanzerkleinType;
	}
	CDynamicCast<IGrenadeItem> pGrenade(pWhat);
	if (pGrenade)
	{
		// A grenade item's DB record can legitimately be null; the release CanEquip @0x0069d660 guards this
		// (reads nPanzerkleinWeapon only if GetDBGrenade() is valid) - the dev reconstruction had dropped the
		// guard, so a grenade with a null pDBGrenade crashed here on the AI's equip command. (The release
		// leaves the melee/weapon DBs unguarded; guarded here too defensively - a no-op when they're valid.)
		NDb::CRPGGrenade *pDBWeap = pGrenade->GetDBGrenade();
		if ( IsValid( pDBWeap ) )
			nPKType = pDBWeap->nPanzerkleinWeapon;
	}
	if ( nPKType )
	{
		if ( !pPK )
			return false;
		return pPK->bAllowWeaponType[ nPKType - 1 ];
	}
	else
		return ( !pPK );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::Equip( NDb::ESlot where, IInventoryItem *pWhat )
{
	if ( !CanEquip( where, pWhat ) )
		return false;
	slots[where] = pWhat;
	dynamic_cast<CInventoryItem*>(pWhat)->OnEquip( pOwner );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CInventory::TakeOff( NDb::ESlot where )
{
	CObj<IInventoryItem> pWhat = slots[where];
	if ( !pWhat )
		return 0;
	dynamic_cast<CInventoryItem*>(pWhat.GetPtr())->OnUnEquip( pOwner );
	slots[where] = 0;
	return pWhat.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventory::Activate( NDb::ESlot where )
{
	nActiveSlot = where;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CInventory::GetActive() const
{
	ASSERT( nActiveSlot >= 0 && nActiveSlot < NDb::N_SLOTS );
	if ( nActiveSlot < 0 || nActiveSlot >= NDb::N_SLOTS ) 
		return 0; 
	return slots[nActiveSlot];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem* CInventory::Get( NDb::ESlot where ) const 
{ 
	ASSERT( where >= 0 && where < NDb::N_SLOTS );
	if ( where < 0 || where >= NDb::N_SLOTS ) 
		return 0; 
	return slots[where]; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventoryItem *CInventory::GetHandItem() const
{
	return pHandItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInventory::SetHandItem( IInventoryItem *pWhat )
{
	pHandItem = pWhat;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CInventory::GetActiveSlot() const
{
	return nActiveSlot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CInventory::GetPlaceBySubType( NDb::EItemSubType subType ) const
{
	NDb::CRPGUniform *pDBUniform = GetUniform();
	for ( int i = 0; i < NDb::N_ITEM_PLACES; ++i )
	{
		if ( pDBUniform->subTypes[i] == subType )
			return i;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGUniform* CInventory::GetUniform() const
{
	if ( !IsValid( pOwner ) )
		return 0;

	return pOwner->GetPers()->pUniform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInventory::SetPanzerklein( NDb::CPanzerklein *_pPK, IInventory *pPKInventory )
{ 
	pPK = _pPK; 
	if ( pPKInventory )
	{
		if ( pPK ) // ������� ��
		{
			for ( int where = NDb::SLOT_1; where < NDb::N_SLOTS; ++where )
			{
				IInventoryItem *pWhat = pPKInventory->TakeOff( (NDb::ESlot)where );
				Equip( (NDb::ESlot)where, pWhat );
			}
			Activate( NDb::SLOT_1 );
		}
		else // ������� ��
		{
			for ( int where = NDb::SLOT_1; where < NDb::N_SLOTS; ++where )
			{
				IInventoryItem *pWhat = TakeOff( (NDb::ESlot)where );
				pPKInventory->Equip( (NDb::ESlot)where, pWhat );
			}
			pPKInventory->Activate( NDb::SLOT_1 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IInventory *CreateInventory( CUnit *pOwner )
{
	return new CInventory(pOwner);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
REGISTER_SAVELOAD_CLASS_NM( 0xE0591410, CInventory, NRPG );
using namespace NRPG;
BASIC_REGISTER_CLASS(IInventoryItem)
