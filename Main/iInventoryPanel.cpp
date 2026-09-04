#include "StdAfx.h"
#include "GSceneUtils.h"
#include "InterfaceConst.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataCamera.h"   // NDb::GetDBCamera -- retail CUnitModelShow::Set @0x1edfe0 uses DataCamera 1
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iInventoryPanel.h"
#include "iGameStates.h"
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitShow
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitModelShow: public CActionDecorator<CInteractiveUnitView>
{
	OBJECT_BASIC_METHODS(CUnitModelShow);
private:
	// retail NUI::CUnitModelShow (PDB, 264 bytes) adds ONLY pUnit@0x104 -- there is no own pMission
	// (the mission link lives in the CActionDecorator base). operator& @0x1f0540 = {1 base, 2 pUnit};
	// byte-walk-confirmed (retail save carries NO tag 3). dev previously serialized a dead, never-set
	// pMission at tag 2, which silently swallowed the save's pUnit ref and then read a nonexistent
	// tag 3 into pUnit.
	ZDATA_(TBaseClass)
	CPtr<NWorld::CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pUnit); return 0; }

public:
	CUnitModelShow() {}
	CUnitModelShow( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void Set( NWorld::CUnit *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitModelShow::CUnitModelShow( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	TBaseClass( sInfo, _pMission )
{
	// retail @0x1ee9e0 tail: adopt the mission's render game (otherwise the CUnitView tag-9
	// pRenderGame stays null on the wire and the doll's head controller is a private orphan).
	SetRenderGame( _pMission->GetRenderGame() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1edf60: while the user is spinning the doll (bButtonDown, the CInteractiveUnitView drag),
// the decorator must not re-route events into the mission state; otherwise the doll is a valid target
// for ANY state that is not a movement order (item drags, first aid, ... -- everything but CStateMove).
// (dev previously accepted only CStateDragItem and ignored the spin gate.)
bool CUnitModelShow::CanHandleState( NGame::IState *pState ) const
{
	if ( bButtonDown )
		return false;

	CDynamicCast<NGame::CStateMove> pMove(pState);
	if (pMove)
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitModelShow::GetTarget()
{
	return pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1edfe0: change-GATED -- CInventoryPanel::Draw @0x1eeac0 calls this every frame, so a
// same-unit re-Set must NOT rebuild the show unit (post-load the deserialized doll graph is the one
// that keeps rendering; dev's ungated version destroyed it on the first frame and then rebuilt the
// whole CShowWorldUnit every frame). Rebuild camera = global DBCamera 5013 (disasm @0x5ee02e:
// `mov ecx,0x1395` -- Ghidra's "GetDBCamera(1)" was the dropped register arg) with
// (bItems=true, bShowCap=true, bPlayIdle=false) -- the @0x1c03d0 inventory-doll argument set.
void CUnitModelShow::Set( NWorld::CUnit *_pUnit )
{
	if ( pUnit == _pUnit )
		return;

	pUnit = _pUnit;
	CUnitView::SetUnit( pUnit, NDb::GetDBCamera( 5013 ), true, true, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBackPackSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBackPackSlot: public CSlot
{
	OBJECT_BASIC_METHODS(CBackPackSlot);
private:
	ZDATA_( CSlot )
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NGame::IUnitTracker> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,( CSlot *)this); f.Add(2,&pMission); f.Add(3,&pUnit); return 0; }

public:
	CBackPackSlot() {}
	CBackPackSlot( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NGame::IUnitTracker *pUnit );

	NWorld::CUnit* GetUnit();      // retail @0x1edfc0
	CObjectBase* GetTarget();      // retail @0x1ee050 (CActionDecorator pure virtual)

	void Take( int nX, int nY );
	void Take( int nX, int nY, bool bSmart );
	void Place( int nX, int nY, const NWorld::SItem &sItem );
	bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 );
	void GetItemsList( vector<SItem> *pItemsSet );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBackPackSlot::CBackPackSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CSlot( sInfo, _pMission, N_BACKPACK_WIDTH, N_BACKPACK_HEIGHT, NDb::CAMERA_NORMAL, true ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackPackSlot::Set( NGame::IUnitTracker *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1edfc0
NWorld::CUnit* CBackPackSlot::GetUnit()
{
	if ( !IsValid( pUnit ) )
		return 0;

	return pUnit->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1ee050: publish the backpack as a drag target (CSlotInfo{BACKPACK, unit}) while hovered.
CObjectBase* CBackPackSlot::GetTarget()
{
	if ( !IsValid( pUnit ) )
		return 0;

	return new CSlotInfo( CSlotInfo::BACKPACK, 0, pUnit->GetUnit() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackPackSlot::Take( int nX, int nY )
{
	Take( nX, nY, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x1ee140: smart-take drops normally, but sells directly while the store panel is open.
void CBackPackSlot::Take( int nX, int nY, bool bSmart )
{
	SPoint sPos;
	GetInSlotPos( nX, nY, &sPos );

	const vector<NRPG::SBackPackItem> &itemsSet = pUnit->GetUnit()->GetRPG()->GetInventoryInfo()->GetItems();
	for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
	{
		const NRPG::SBackPackItem &sItem = itemsSet[nTemp];

		const SPoint &sItemPos = itemsSet[nTemp].sPos;
		const SPoint &sItemSize = itemsSet[nTemp].pItem->GetSize();

		if ( ( sItemPos.x <= sPos.x ) && ( sItemPos.x + sItemSize.x > sPos.x  ) && ( sItemPos.y <= sPos.y ) && ( sItemPos.y + sItemSize.y > sPos.y  ) )
		{
			NWorld::SItem sInvItem;
			sInvItem.eType = NWorld::SItem::BACKPACK;
			sInvItem.pItem = sItem.pItem;
			sInvItem.pUnit = pUnit->GetUnit();
			sInvItem.sPosition = sItem.sPos;

			NWorld::SItem sTarget;
			if ( !bSmart )
				sTarget = NWorld::SItem( pUnit->GetUnit(), NWorld::SItem::HAND );
			else if ( pMission->GetPanelState( NGame::PANEL_STORE ) )
			{
				sTarget.eType = NWorld::SItem::STORAGE;
				sTarget.pPlayer = GetGame()->GetActivePlayer()->GetPlayer();
				sTarget.sPosition = CTPoint<int>( -1, -1 );
			}
			else
				sTarget = NWorld::SItem( pUnit->GetUnit(), NWorld::SItem::GROUND );

			pMission->CommandState( new NGame::CStateMoveItem( pUnit->GetUnit(), sInvItem, sTarget ) );
			return;
		}
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackPackSlot::Place( int nX, int nY, const NWorld::SItem &sItem )
{
	const CTPoint<int> &sSize = sItem.pItem->GetSize();

	SPoint sPos;
	GetItemInSlotPos( nX, nY, sSize, &sPos );

	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::BACKPACK;
	sTarget.pUnit = pUnit->GetUnit();
	sTarget.sPosition = sPos;

	pMission->Command( pUnit->GetUnit(), new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBackPackSlot::CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP )
{
	const CTPoint<int> &sSize = sItem.pItem->GetSize();

	SPoint sPos;
	GetItemInSlotPos( nX, nY, sSize, &sPos );

	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::BACKPACK;
	sTarget.pUnit = pUnit->GetUnit();
	sTarget.sPosition = sPos;

	NWorld::EUnitCommandResult eRes = pUnit->GetUnit()->CanDo( new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
	if ( eRes != NWorld::UCR_OK )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBackPackSlot::GetItemsList( vector<SItem> *pItemsSet )
{
	const vector<NRPG::SBackPackItem> &itemsSet = pUnit->GetUnit()->GetRPG()->GetInventoryInfo()->GetItems();
	pItemsSet->resize( itemsSet.size() );
	for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
	{
		SItem &sItem = (*pItemsSet)[nTemp];
		sItem.sPos = itemsSet[nTemp].sPos;
		sItem.pItem = itemsSet[nTemp].pItem;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBackPackSlot::ProcessMessage( const SEvent &sEvent )
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
CInventoryPanel::CInventoryPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInventoryPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pBackPack = new CBackPackSlot( sEvent.pLoader->GetControl( "backpack" ), pMission );
			pUnitModelShow = new CUnitModelShow( sEvent.pLoader->GetControl( "unitview" ), pMission );

			// retail @0x1eeda0 builds ONLY backpack/unitview/unload -- no "repair" button exists in the binary
			pUnload = new CComplexButton( sEvent.pLoader->GetControl( "unload" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pUnload->Set( NDb::GetUITexture( 397 ), NDb::GetUITexture( 423 ), CComplexButton::UNCHECKED, "unload" );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// retail @0x1eeda0: resolve the "name" CText header first, then close/arrange
			pName = GetUIWindow<CText>( this, "name" );

			pClose = GetUIWindow<CButton>( this, "inventory" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );

			pArrange = GetUIWindow<CButton>( this, "arrange" );
			pArrange->AddImageState( 0, NDb::GetUITexture( 380 ) );
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
void CInventoryPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() == 1 )
	{
		// retail @0x1eeac0: unload toggle = "is the current state a CStateUnloadItem"
		bool bUnload = false;
		CDynamicCast<NGame::CStateUnloadItem> pUnloadItem( pMission->GetState() );
		if ( pUnloadItem )
			bUnload = true;
		pUnload->SetChecked( bUnload );

		// retail @0x1eeac0: header = DB string 19325 (0x4b7d) + selected unit's RPG name
		pName->SetText( GetDBString( 19325 ) + unitsSet[0]->GetUnit()->GetRPG()->GetName(), true );

		pBackPack->Set( unitsSet[0] );
		pUnitModelShow->Set( unitsSet[0]->GetUnit() );

		// ORIGINAL BUG (confirmed in Game.exe @0x1eeac0): the CWindow::Draw recurse is INSIDE the
		// size()==1 branch -- with 0 or >1 units selected the panel's children are not drawn.
		CWindow::Draw( sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521141, CUnitModelShow );
REGISTER_SAVELOAD_CLASS( 0xB0521142, CBackPackSlot );
REGISTER_SAVELOAD_CLASS( 0xB0521143, CInventoryPanel );
