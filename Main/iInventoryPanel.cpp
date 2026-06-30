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
	ZDATA_(TBaseClass)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NWorld::CUnit> pUnit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMission); f.Add(3,&pUnit); return 0; }

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
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitModelShow::CanHandleState( NGame::IState *pState ) const
{
	CDynamicCast<NGame::CStateDragItem> pDrag(pState);
	if (pDrag)
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitModelShow::GetTarget()
{
	return pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitModelShow::Set( NWorld::CUnit *_pUnit )
{
	pUnit = _pUnit;
	SetUnit( pUnit );
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

	void Take( int nX, int nY );
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
void CBackPackSlot::Take( int nX, int nY )
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
			pMission->CommandState( new NGame::CStateMoveItem( pUnit->GetUnit(), sInvItem, NWorld::SItem( pUnit->GetUnit(), NWorld::SItem::HAND ) ) );
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

			pUnload = new CComplexButton( sEvent.pLoader->GetControl( "unload" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pUnload->Set( NDb::GetUITexture( 397 ), NDb::GetUITexture( 423 ), CComplexButton::UNCHECKED, "unload" );

			pRepair = new CComplexButton( sEvent.pLoader->GetControl( "repair" ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 352 ), NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pRepair->Set( NDb::GetUITexture( 423 ), NDb::GetUITexture( 423 ), CComplexButton::UNCHECKED, "repair" );
			pRepair->SetStyle( STYLE_ENABLED, false );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
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
		NGame::IState *pState = pMission->GetState();

		bool bUnload = false, bRepair = false;
		CDynamicCast<NGame::CStateUnloadItem> pUnloadItem(pState);
		if (pState)
			bUnload = true;
	//	else if ( CDynamicCast<NGame::CStateRepairItem> pRepairItem( pState ) )
	//		bRepair = true;

		pUnload->SetChecked( bUnload );
		pRepair->SetChecked( bRepair );
		if (pState)
			pUnload->SetChecked( true );
		else
			pUnload->SetChecked( false );


		pBackPack->Set( unitsSet[0] );
		pUnitModelShow->Set( unitsSet[0]->GetUnit() );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521141, CUnitModelShow );
REGISTER_SAVELOAD_CLASS( 0xB0521142, CBackPackSlot );
REGISTER_SAVELOAD_CLASS( 0xB0521143, CInventoryPanel );
