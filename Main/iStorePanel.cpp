#include "StdAfx.h"
#include "GSceneUtils.h"
#include "InterfaceConst.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "wMain.h"
#include "RPGItem.h"
#include "RPGStore.h"
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
	// retail CStoreSlot::operator& = {1 CSlot, 2 pMission, 3 pScrollBase(CPtr<CScrollWindowBase>), 4 sCellSize,
	// 5 eFilter}. The release slot is only a view over CStore's active CItemsMap; it owns no item/map copy.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CSlot *)this); f.Add(2,&pMission); f.Add(3,&pScrollBase); f.Add(4,&sCellSize); f.Add(5,&eFilter); return 0; }

public:
	CStoreSlot() {}
	CStoreSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, CScrollWindowBase *pScrollBase );

	EFilter GetFilter() const { return eFilter; }
	void SetFilter( EFilter eFilter );

	CObjectBase* GetTarget();      // retail @0x241220 (CActionDecorator pure virtual)

	void Take( int nX, int nY );
	void Take( int nX, int nY, bool bToUnit );
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
	sCellSize = SPoint( GetSize().x / N_STORESLOT_DEFWIDTH, GetSize().y / N_STORESLOT_DEFHEIGHT );
	// Retail @0x241310 pushes its initial pistol filter through the mission's store container too.
	SetFilter( FLT_PISTOLS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::SetFilter( EFilter _eFilter )
{
	eFilter = _eFilter;
	NGame::IPlayerTracker *pTracker = IsValid( pMission ) ? pMission->GetActivePlayer() : 0;
	NRPG::CGlobalPlayer *pGlobalPlayer = IsValid( pTracker ) ? pTracker->GetGlobalPlayer() : 0;
	if ( IsValid( pGlobalPlayer ) && IsValid( pGlobalPlayer->pStore ) )
		pGlobalPlayer->pStore->SetFilter( (NRPG::EStoreFilter)eFilter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x241220: the store grid is a STORAGE drop target (no owning unit).
CObjectBase* CStoreSlot::GetTarget()
{
	return new CSlotInfo( CSlotInfo::STORAGE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::Take( int nX, int nY )
{
	Take( nX, nY, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStoreSlot::Take( int nX, int nY, bool bToUnit )
{
	SPoint sPos;
	GetInSlotPos( nX, nY, &sPos );

	CPtr<NGame::IPlayerTracker> pPlayer = GetGame()->GetActivePlayer();
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pPlayer->GetUnits( &unitsSet );
	if ( unitsSet.empty() )
		return;

	CPtr<NGame::IUnitTracker> pUnit = unitsSet.front();

	NRPG::CGlobalPlayer *pGlobalPlayer = pPlayer->GetGlobalPlayer();
	if ( !IsValid( pGlobalPlayer ) || !IsValid( pGlobalPlayer->pStore ) )
		return;
	pGlobalPlayer->pStore->SetFilter( (NRPG::EStoreFilter)eFilter );
	vector<NRPG::SMapItem> *pItems = pGlobalPlayer->pStore->GetItems();
	for ( int nTemp = 0; nTemp < (int)pItems->size(); nTemp++ )
	{
		const NRPG::SMapItem &sItem = (*pItems)[nTemp];
		const SPoint &sItemPos = sItem.sPos;
		const SPoint &sItemSize = sItem.pItem->GetSize();

		if ( ( sItemPos.x <= sPos.x ) && ( sItemPos.x + sItemSize.x > sPos.x  ) && ( sItemPos.y <= sPos.y ) && ( sItemPos.y + sItemSize.y > sPos.y  ) )
		{
			NWorld::SItem sInvItem;
			sInvItem.eType = NWorld::SItem::STORAGE;
			sInvItem.pItem = sItem.pItem;
			sInvItem.pPlayer = pPlayer->GetPlayer();
			NWorld::SItem sTarget;
			if ( bToUnit )
			{
				sTarget.eType = NWorld::SItem::UNIT_ANYPLACE;
				sTarget.pUnit = pUnit->GetUnit();
			}
			else
				sTarget = NWorld::SItem( 0, NWorld::SItem::HAND );
			pMission->CommandState( new NGame::CStateMoveItem( pUnit->GetUnit(), sInvItem, sTarget ) );
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
	sTarget.sPosition = sPos;

	// Retail routes the command through the unit that owns the dragged hand item. Falling back to
	// the selected unit is only for an ownerless hand; using the selected unit unconditionally made
	// moves from another squad member fail the executor's source-hand validation.
	NWorld::CUnit *pCommandUnit = IsValid( sItem.pUnit ) ? sItem.pUnit.GetPtr() : pUnit->GetUnit();
	pMission->Command( pCommandUnit, new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStoreSlot::CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP )
{
	// Retail @0x240e40 accepts every dragged item. CStore::Place classifies it and either honours
	// the requested cell or auto-places it in the correct category; rejecting against the visible
	// category map here prevented selling items whose footprint did not fit at the cursor.
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x241d00: copy the already-positioned active CStore map; no transient reclassification or
// second occupancy grid exists in CStoreSlot.
void CStoreSlot::GetItemsList( vector<SItem> *pItemsSet )
{
	pItemsSet->clear();
	NGame::IPlayerTracker *pTracker = IsValid( pMission ) ? pMission->GetActivePlayer() : 0;
	NRPG::CGlobalPlayer *pGlobalPlayer = IsValid( pTracker ) ? pTracker->GetGlobalPlayer() : 0;
	if ( !IsValid( pGlobalPlayer ) || !IsValid( pGlobalPlayer->pStore ) )
		return;

	NRPG::CStore *pStore = pGlobalPlayer->pStore;
	// The stock is normally refreshed by CCmdUpdateStore before the panel is used. A save produced
	// before that command was restored can still contain old stock rows while all release category maps
	// are empty, so probe the maps themselves and refresh lazily once the live player is available.
	if ( !pStore->HasMappedItems() )
	{
		NWorld::CPlayer *pPlayer = dynamic_cast<NWorld::CPlayer*>( pTracker->GetPlayer() );
		if ( pPlayer )
			pPlayer->UpdateStore();
		pStore = pGlobalPlayer->pStore;
	}
	pStore->SetFilter( (NRPG::EStoreFilter)eFilter );
	vector<NRPG::SMapItem> *pStoreItems = pStore->GetItems();
	pItemsSet->reserve( pStoreItems->size() );
	for ( int i = 0; i < (int)pStoreItems->size(); ++i )
	{
		SItem sItem;
		sItem.sPos = (*pStoreItems)[i].sPos;
		sItem.pItem = (*pStoreItems)[i].pItem;
		pItemsSet->push_back( sItem );
	}

	CTPoint<int> sMapSize = pStore->GetSize();
	SetSize( SPoint( sMapSize.x * sCellSize.x, sMapSize.y * sCellSize.y ) );
	SetSlotSize( sMapSize.x, sMapSize.y );
	pScrollBase->GetVScroll()->SetMaxValue( Max( 0, sMapSize.y - N_STORESLOT_DEFHEIGHT ) );
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
			NWorld::SItem sInfo;
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
	case EVENT_LBUTTONDBLCLK:
		Take( sEvent.nX, sEvent.nY, true );
		return true;
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
			// Retail @0x241fb0: "store" is the close button and is left to CWindow.  The separate
			// "arrange" button posts the one-shot store-refresh command through the first mission unit.
			if ( sEvent.szID == "store" )
				break;
			if ( sEvent.szID == "arrange" )
			{
				vector< CPtr<NGame::IUnitTracker> > unitsSet;
				pMission->GetUnits( &unitsSet );
				if ( !unitsSet.empty() && IsValid( unitsSet.front() ) )
					pMission->Command( unitsSet.front()->GetUnit(), new NWorld::CCmdUpdateStore(), true );
				return true;
			}

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

			pSMG = new CComplexButtonFlash( sEvent.pLoader->GetControl( "submachinegun" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pSMG->Set( NDb::GetUITexture( 624 ), NDb::GetUITexture( 625 ), CComplexButton::UNCHECKED );
			pOthers = new CComplexButtonFlash( sEvent.pLoader->GetControl( "others" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pOthers->Set( NDb::GetUITexture( 632 ), NDb::GetUITexture( 633 ), CComplexButton::UNCHECKED );
			pRifles = new CComplexButtonFlash( sEvent.pLoader->GetControl( "rifles" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pRifles->Set( NDb::GetUITexture( 622 ), NDb::GetUITexture( 623 ), CComplexButton::UNCHECKED );
			pPistols = new CComplexButtonFlash( sEvent.pLoader->GetControl( "pistols" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pPistols->Set( NDb::GetUITexture( 620 ), NDb::GetUITexture( 621 ), CComplexButton::UNCHECKED );
			pGrenades = new CComplexButtonFlash( sEvent.pLoader->GetControl( "grenades" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pGrenades->Set( NDb::GetUITexture( 630 ), NDb::GetUITexture( 631 ), CComplexButton::UNCHECKED );
			pColdSteel = new CComplexButtonFlash( sEvent.pLoader->GetControl( "coldsteel" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pColdSteel->Set( NDb::GetUITexture( 628 ), NDb::GetUITexture( 629 ), CComplexButton::UNCHECKED );
			pPKWeapons = new CComplexButtonFlash( sEvent.pLoader->GetControl( "pkweapons" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pPKWeapons->Set( NDb::GetUITexture( 634 ), NDb::GetUITexture( 635 ), CComplexButton::UNCHECKED );
			pHeavyWeapon = new CComplexButtonFlash( sEvent.pLoader->GetControl( "heavyweapon" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pHeavyWeapon->Set( NDb::GetUITexture( 626 ), NDb::GetUITexture( 627 ), CComplexButton::UNCHECKED );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pClose = GetUIWindow<CButton>( this, "store" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );
			pArrange = GetUIWindow<CButton>( this, "arrange" );
			pArrange->AddImageState( 0, NDb::GetUITexture( 380 ) );

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
// Retail @0x241100: consume the store's category-dirty flags and arm the matching flash buttons.
void CStorePanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<bool> flagsSet;
	NGame::IPlayerTracker *pTracker = IsValid( pMission ) ? pMission->GetActivePlayer() : 0;
	NWorld::IPlayer *pPlayer = IsValid( pTracker ) ? pTracker->GetPlayer() : 0;
	if ( IsValid( pPlayer ) )
		pPlayer->GetStoreUpdateFlags( &flagsSet );

	if ( flagsSet.size() >= NRPG::FLT_MAXVALUE )
	{
		pSMG->SetShowFlash( flagsSet[CStoreSlot::FLT_SUBMACHINEGUN] );
		pOthers->SetShowFlash( flagsSet[CStoreSlot::FLT_OTHERS] );
		pRifles->SetShowFlash( flagsSet[CStoreSlot::FLT_RIFLES] );
		pPistols->SetShowFlash( flagsSet[CStoreSlot::FLT_PISTOLS] );
		pGrenades->SetShowFlash( flagsSet[CStoreSlot::FLT_GRENADES] );
		pColdSteel->SetShowFlash( flagsSet[CStoreSlot::FLT_COLDSTEEL] );
		pPKWeapons->SetShowFlash( flagsSet[CStoreSlot::FLT_PKWEAPONS] );
		pHeavyWeapon->SetShowFlash( flagsSet[CStoreSlot::FLT_HEAVYWEAPON] );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1110180, CStoreSlot );
REGISTER_SAVELOAD_CLASS( 0xB1110181, CStorePanel );
REGISTER_SAVELOAD_CLASS( 0xB1110182, CSlotScroll );
