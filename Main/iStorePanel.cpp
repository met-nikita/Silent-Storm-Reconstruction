#include "StdAfx.h"
#include "GSceneUtils.h"
#include "InterfaceConst.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "RPGItem.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iStorePanel.h"
#include "iGameStates.h"
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStoreSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStoreSlot: public CSlot
{
	OBJECT_BASIC_METHODS(CStoreSlot);
public:
	enum EFilter
	{
		FLT_OTHERS,
		FLT_RIFLES,
		FLT_PISTOLS,
		FLT_GRENADES,
		FLT_COLDSTEEL,
		FLT_PKWEAPONS,
		FLT_HEAVYWEAPON,
		FLT_SUBMACHINEGUN
	};

private:
	ZDATA_( CSlot )
	CPtr<NGame::IMission> pMission;
	CPtr<CScrollWindowBase> pScrollBase;
	////
	SPoint sCellSize;
	EFilter eFilter;
	vector<SItem> itemsSet;
	CArray2D<bool> placeMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CSlot *)this); f.Add(2,&pMission); f.Add(3,&sCellSize); f.Add(4,&eFilter); f.Add(5,&itemsSet); f.Add(6,&placeMap); return 0; }

protected:
	void SetPlaceMapSize( int nWidth, int nHeight );
	void ArrangeTake( const CTPoint<int> &sPos, const NRPG::IInventoryItem *pItem );
	bool ArrangePlace( const CTPoint<int> &sPos, const NRPG::IInventoryItem *pItem );
	bool ArrangeCanPlace( const CTPoint<int> &sPos, const NRPG::IInventoryItem *pItem ) const;
	bool ArrangeFindPlace( const NRPG::IInventoryItem *pItem, CTPoint<int> *pPos );

public:
	CStoreSlot() {}
	CStoreSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, CScrollWindowBase *pScrollBase );

	EFilter GetFilter() const { return eFilter; }
	void SetFilter( EFilter eFilter );

	void Take( int nX, int nY );
	void Place( int nX, int nY, const NWorld::SItem &sItem );
	bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 );
	void GetItemsList( vector<SItem> *pItemsSet );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CStoreSlot::CStoreSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission, CScrollWindowBase *_pScrollBase ):
	CSlot( sInfo, _pMission, N_STORESLOT_DEFWIDTH, N_STORESLOT_DEFHEIGHT, NDb::CAMERA_NORMAL, true ), 
	pMission( _pMission ), pScrollBase( _pScrollBase ), eFilter( FLT_PISTOLS )
{
	placeMap.SetSizes( N_STORESLOT_DEFWIDTH, N_STORESLOT_DEFHEIGHT );
	placeMap.FillEvery( false );
	sCellSize = SPoint( GetSize().x / N_STORESLOT_DEFWIDTH, GetSize().y / N_STORESLOT_DEFHEIGHT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::SetFilter( EFilter _eFilter )
{
	eFilter = _eFilter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::Take( int nX, int nY )
{
	SPoint sPos;
	GetInSlotPos( nX, nY, &sPos );

	CPtr<NGame::IPlayerTracker> pPlayer = GetGame()->GetActivePlayer();
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pPlayer->GetUnits( &unitsSet );
	if ( unitsSet.empty() )
		return;

	CPtr<NGame::IUnitTracker> pUnit = unitsSet.front();

	for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
	{
		const SItem &sItem = itemsSet[nTemp];
		const SPoint &sItemPos = sItem.sPos;
		const SPoint &sItemSize = sItem.pItem->GetSize();

		if ( ( sItemPos.x <= sPos.x ) && ( sItemPos.x + sItemSize.x > sPos.x  ) && ( sItemPos.y <= sPos.y ) && ( sItemPos.y + sItemSize.y > sPos.y  ) )
		{
			NWorld::SItem sInvItem;
			sInvItem.eType = NWorld::SItem::STORAGE;
			sInvItem.pItem = sItem.pItem;
			sInvItem.pPlayer = pPlayer->GetPlayer();
			pMission->CommandState( new NGame::CStateMoveItem( pUnit->GetUnit(), sInvItem, NWorld::SItem( 0, NWorld::SItem::HAND ) ) );
			return;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::Place( int nX, int nY, const NWorld::SItem &sItem )
{
	CPtr<NGame::IPlayerTracker> pPlayer = GetGame()->GetActivePlayer();
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pPlayer->GetUnits( &unitsSet );
	if ( unitsSet.empty() )
		return;

	CPtr<NGame::IUnitTracker> pUnit = unitsSet.front();

	const CTPoint<int> &sSize = sItem.pItem->GetSize();

	SPoint sPos;
	GetItemInSlotPos( nX, nY, sSize, &sPos );

	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::STORAGE;
	sTarget.pPlayer = GetGame()->GetActivePlayer()->GetPlayer();

	pMission->Command( pUnit->GetUnit(), new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::GetItemsList( vector<SItem> *pItemsSet )
{
	*pItemsSet = itemsSet;

	list<CPtr<NRPG::IInventoryItem> > itemsList;
	pMission->GetActivePlayer()->GetPlayer()->GetStoreItems( &itemsList );

	for ( list<CPtr<NRPG::IInventoryItem> >::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); )
	{
		EFilter eType = FLT_OTHERS;

		CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( *iTemp );
		if ( IsValid( pWeapon ) && IsValid( pWeapon->GetDBWeapon()->pWeaponType ) )
		{
			switch( pWeapon->GetDBWeapon()->pWeaponType->eStoreWeaponType )
			{
			case NDb::SWT_PISTOL:
				eType = FLT_PISTOLS;
				break;
			case NDb::SWT_RIFLE:
				eType = FLT_RIFLES;
				break;
			case NDb::SWT_SUB_MACHINE_GUN:
				eType = FLT_SUBMACHINEGUN;
				break;
			case NDb::SWT_HEAVY_WEAPON:
				eType = FLT_HEAVYWEAPON;
				break;
			case NDb::SWT_COLD_STEEL:
				eType = FLT_COLDSTEEL;
				break;
			case NDb::SWT_OTHER:
				eType = FLT_OTHERS;
				break;
			case NDb::SWT_GRENADE:
				eType = FLT_GRENADES;
				break;
			case NDb::SWT_PK_WEAPON:
				eType = FLT_PKWEAPONS;
				break;
			}
		}
		else {
			CDynamicCast<NRPG::IGrenadeItem> pGrenade(*iTemp);
			if (pGrenade)
			{
				eType = FLT_GRENADES;
			}
			else
			{
				switch ((*iTemp)->GetWeaponType())
				{
				case NDb::SUBTYPE_PISTOL:
				case NDb::SUBTYPE_AMMO_PISTOL:
					eType = FLT_PISTOLS;
					break;
				case NDb::SUBTYPE_AMMO_RIFLE:
					eType = FLT_RIFLES;
					break;
				case NDb::SUBTYPE_AMMO_SMG:
					eType = FLT_SUBMACHINEGUN;
					break;
				case NDb::SUBTYPE_HEAVY:
				case NDb::SUBTYPE_AMMO_MG:
				case NDb::SUBTYPE_AMMO_HEAVY:
					eType = FLT_HEAVYWEAPON;
					break;
				case NDb::SUBTYPE_KNIFE:
				case NDb::SUBTYPE_THROWING_KNIFE:
					eType = FLT_COLDSTEEL;
					break;
				case NDb::SUBTYPE_GRENADE_SMALL:
				case NDb::SUBTYPE_GRENADE_LARGE:
					eType = FLT_GRENADES;
					break;
				case NDb::SUBTYPE_FIRST_AID:
				case NDb::SUBTYPE_ENGINEERING:
				case NDb::SUBTYPE_MINE_DETECTOR:
				case NDb::SUBTYPE_NONE:
					eType = FLT_OTHERS;
					break;
				}
			}
		}

		if ( eType != eFilter )
			iTemp = itemsList.erase( iTemp );
		else
			iTemp++;
	}

	int nMaxY = N_STORESLOT_DEFHEIGHT;
	vector<SItem> newItemsSet;
	newItemsSet.reserve( itemsSet.size() );
	for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
	{
		list<CPtr<NRPG::IInventoryItem> >::const_iterator iItem = find( itemsList.begin(), itemsList.end(), itemsSet[nTemp].pItem );
		if ( iItem == itemsList.end() )
		{
			ArrangeTake( itemsSet[nTemp].sPos, itemsSet[nTemp].pItem );
			continue;
		}

		nMaxY = Max( nMaxY, itemsSet[nTemp].sPos.y + itemsSet[nTemp].pItem->GetSize().y );
		newItemsSet.push_back( itemsSet[nTemp] );
	}
	itemsSet = newItemsSet;

	if ( placeMap.GetYSize() != nMaxY )
		SetPlaceMapSize( placeMap.GetXSize(), nMaxY );

	for ( list<CPtr<NRPG::IInventoryItem> >::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		bool bFound = false;
		for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
		{
			if ( itemsSet[nTemp].pItem != *iTemp )
				continue;

			bFound = true;
			break;
		}

		if ( bFound )
			continue;

		SPoint sPos;
		if ( ArrangeFindPlace( *iTemp, &sPos ) )
		{
			ArrangePlace( sPos, *iTemp );

			SItem &sItem = *itemsSet.insert( itemsSet.end(), SItem());
			sItem.sPos = sPos;
			sItem.pItem = *iTemp;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::ProcessMessage( const SEvent &sEvent )
{
	if ( CSlot::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		{
			NWorld::IPlayer::SItemInfo sInfo;
			if ( GetDragItem( &sInfo ) )
			{
				NWorld::SItem sSource( sInfo.pUnit, NWorld::SItem::HAND, sInfo.pItem );
				if ( CanPlace( sEvent.nX, sEvent.nY, sSource ) )
					Place( sEvent.nX, sEvent.nY, sSource );
				else
					PlaySound( NDb::GetSound( NGame::N_SOUND_ERROR ) );
			}
			else
				Take( sEvent.nX, sEvent.nY );

			return true;
		}
	case EVENT_LBUTTONUP:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::SetPlaceMapSize( int nWidth, int nHeight )
{
	CArray2D<bool> oldPlaceMap( placeMap );
	placeMap.SetSizes( nWidth, nHeight );
	placeMap.FillEvery( false );

	for( int nTempY = 0; nTempY < Min( nHeight, oldPlaceMap.GetYSize() ); nTempY++ )
		for( int nTempX = 0; nTempX < Min( nWidth, oldPlaceMap.GetXSize() ); nTempX++ )
			placeMap[nTempY][nTempX] = oldPlaceMap[nTempY][nTempX];

	SetSize( SPoint( placeMap.GetXSize() * sCellSize.x, placeMap.GetYSize() * sCellSize.y ) );
	SetSlotSize( placeMap.GetXSize(), placeMap.GetYSize() );

	pScrollBase->GetVScroll()->SetMaxValue( nHeight - N_STORESLOT_DEFHEIGHT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::ArrangeTake( const CTPoint<int> &sPos, const NRPG::IInventoryItem *pItem )
{
	const CTPoint<int> &sSize = pItem->GetSize();
	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			placeMap[sPos.y + nTempY][sPos.x + nTempX] = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::ArrangePlace( const CTPoint<int> &_sPos, const NRPG::IInventoryItem *pItem )
{
	CTPoint<int> sPos( _sPos );

	if ( ( sPos.x == -1 ) && ( sPos.y == -1 ) && !ArrangeFindPlace( pItem, &sPos ) )
		return false;

	const CTPoint<int> &sSize = pItem->GetSize();
	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			placeMap[sPos.y + nTempY][sPos.x + nTempX] = true;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::ArrangeCanPlace( const CTPoint<int> &sPos, const NRPG::IInventoryItem *pItem ) const
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + pItem->GetSize().x > placeMap.GetXSize() ) || ( sPos.y + pItem->GetSize().y > placeMap.GetYSize() ) )
		return false;

	const CTPoint<int> &sSize = pItem->GetSize();
	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
	{
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
		{
			if ( placeMap[sPos.y + nTempY][sPos.x + nTempX] )
				return false;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::ArrangeFindPlace( const NRPG::IInventoryItem *pItem, CTPoint<int> *pPos )
{
	for ( int nTemp = 0; nTemp <= pItem->GetSize().y; nTemp++ )
	{
		for( int nTempY = 0; nTempY < placeMap.GetYSize(); nTempY++ )
		{
			for( int nTempX = 0; nTempX < placeMap.GetXSize(); nTempX++ )
			{
				if ( ArrangeCanPlace( CTPoint<int>( nTempX, nTempY ), pItem ) )
				{
					*pPos = CTPoint<int>( nTempX, nTempY );
					return true;
				}
			}
		}

		SetPlaceMapSize( placeMap.GetXSize(), placeMap.GetYSize() + 1 );
	}


	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlotScroll
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlotScroll: public CScrollWindowBase
{
	OBJECT_BASIC_METHODS(CSlotScroll);
private:
	ZDATA_(CScrollWindowBase)
	CPtr<NGame::IMission> pMission;
	////
	CObj<CStoreSlot> pScrollWindow;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CScrollWindowBase*)this); f.Add(2,&pMission); f.Add(3,&pScrollWindow); return 0; }

public:
	CSlotScroll() {}
	CSlotScroll( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	CStoreSlot* GetClientWindow() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlotScroll::CSlotScroll( const SWindowInfo &sInfo, NGame::IMission *_pMission ): 
	CScrollWindowBase( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlotScroll::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pScrollWindow = new CStoreSlot( sEvent.pLoader->GetControl( "view" ), pMission, this );
			SetClient( pScrollWindow );
			break;
		}
	}

	return CScrollWindowBase::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CStoreSlot* CSlotScroll::GetClientWindow() const
{
	return pScrollWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStorePanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CStorePanel::CStorePanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStorePanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "store" )
				break;

			if ( sEvent.szID == "pistols" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_PISTOLS );
			else if ( sEvent.szID == "rifles" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_RIFLES );
			else if ( sEvent.szID == "submachinegun" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_SUBMACHINEGUN );
			else if ( sEvent.szID == "heavyweapon" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_HEAVYWEAPON );
			else if ( sEvent.szID == "coldsteel" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_COLDSTEEL );
			else if ( sEvent.szID == "grenades" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_GRENADES );
			else if ( sEvent.szID == "others" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_OTHERS );
			else if ( sEvent.szID == "pkweapons" )
				pStoreSlot->SetFilter( CStoreSlot::FLT_PKWEAPONS );

			UpdateButtons();
			return true;
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pStoreSlotView = new CSlotScroll( sEvent.pLoader->GetControl( "slot" ), pMission );

			pSMG = new CComplexButton( sEvent.pLoader->GetControl( "submachinegun" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pSMG->Set( NDb::GetUITexture( 624 ), NDb::GetUITexture( 625 ), CComplexButton::UNCHECKED );
			pOthers = new CComplexButton( sEvent.pLoader->GetControl( "others" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pOthers->Set( NDb::GetUITexture( 632 ), NDb::GetUITexture( 633 ), CComplexButton::UNCHECKED );
			pRifles = new CComplexButton( sEvent.pLoader->GetControl( "rifles" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pRifles->Set( NDb::GetUITexture( 622 ), NDb::GetUITexture( 623 ), CComplexButton::UNCHECKED );
			pPistols = new CComplexButton( sEvent.pLoader->GetControl( "pistols" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pPistols->Set( NDb::GetUITexture( 620 ), NDb::GetUITexture( 621 ), CComplexButton::UNCHECKED );
			pGrenades = new CComplexButton( sEvent.pLoader->GetControl( "grenades" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pGrenades->Set( NDb::GetUITexture( 630 ), NDb::GetUITexture( 631 ), CComplexButton::UNCHECKED );
			pColdSteel = new CComplexButton( sEvent.pLoader->GetControl( "coldsteel" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pColdSteel->Set( NDb::GetUITexture( 628 ), NDb::GetUITexture( 629 ), CComplexButton::UNCHECKED );
			pPKWeapons = new CComplexButton( sEvent.pLoader->GetControl( "pkweapons" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pPKWeapons->Set( NDb::GetUITexture( 634 ), NDb::GetUITexture( 635 ), CComplexButton::UNCHECKED );
			pHeavyWeapon = new CComplexButton( sEvent.pLoader->GetControl( "heavyweapon" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pHeavyWeapon->Set( NDb::GetUITexture( 626 ), NDb::GetUITexture( 627 ), CComplexButton::UNCHECKED );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pClose = GetUIWindow<CButton>( this, "store" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );

			pStoreSlot = pStoreSlotView->GetClientWindow();
			pStoreSlotView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			UpdateButtons();
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	case EVENT_LBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONUP:
	case EVENT_RBUTTONDOWN:
	case EVENT_RBUTTONDBLCLK:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStorePanel::UpdateButtons()
{
	CStoreSlot::EFilter eFilter = pStoreSlot->GetFilter();
	pSMG->SetChecked( eFilter == CStoreSlot::FLT_SUBMACHINEGUN );
	pOthers->SetChecked( eFilter == CStoreSlot::FLT_OTHERS );
	pRifles->SetChecked( eFilter == CStoreSlot::FLT_RIFLES );
	pPistols->SetChecked( eFilter == CStoreSlot::FLT_PISTOLS );
	pGrenades->SetChecked( eFilter == CStoreSlot::FLT_GRENADES );
	pColdSteel->SetChecked( eFilter == CStoreSlot::FLT_COLDSTEEL );
	pPKWeapons->SetChecked( eFilter == CStoreSlot::FLT_PKWEAPONS );
	pHeavyWeapon->SetChecked( eFilter == CStoreSlot::FLT_HEAVYWEAPON );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1110180, CStoreSlot );
REGISTER_SAVELOAD_CLASS( 0xB1110181, CStorePanel );
REGISTER_SAVELOAD_CLASS( 0xB1110182, CSlotScroll );
