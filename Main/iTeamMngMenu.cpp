#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "GView.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "RPGMerc.h"
#include "RPGUnit.h"
#include "RPGUnitInfo.h"
#include "RPGGlobal.h"
#include "RPGItemInfo.h"
#include "RPGItem.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iTeamMngMenu.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_MAX_UNITS = 20,
	N_MAX_PLAYER_UNITS = 5,
	N_MAXVISIBLE_INVENTORYITEMS = 8;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPanel
{
	PANEL_DEFAULT,
	PANEL_NONE,
	PANEL_CHARACTER,
	PANEL_INVENTORY
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPortraitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPortraitView: public CUnitView
{
	OBJECT_NOCOPY_METHODS(CUnitPortraitView)
private:
	ZDATA_(CUnitView)
	CObj<CImage> pImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitView*)this); f.Add(2,&pImage); return 0; }

public:
	CUnitPortraitView() {}
	CUnitPortraitView( const SWindowInfo &sInfo, NRPG::CUnit *pMerc );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitPortraitView::CUnitPortraitView( const SWindowInfo &sInfo, NRPG::CUnit *pMerc ):
	CUnitView( sInfo, 0 )
{
	pImage = new CImage( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ) );
	pImage->SetImage( NDb::GetUITexture( 535 ) );

	if ( IsValid( pMerc ) )
		SetUnit( pMerc );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPortraitView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SRect sScrWindow;
	SPoint sScrPosition;
	if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
		return;

	VirtualToScreen( &sScrPosition, &sScrWindow );

	SRect sDummyRect;
	SPoint sSize( GetSize() );
	VirtualToScreen( &sSize, &sDummyRect );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sSize.x, sSize.y ) );

	pView->CreateDynamicClearRects( sLayout, sScrPosition, sScrWindow, 1 );
	CUnitView::Draw( sTime, pView );
	pView->CreateDynamicClearRects( sLayout, sScrPosition, sScrWindow, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPortraitState
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPortraitState: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitPortraitState)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	////
	CObj<CImage> pState;
	CObj<CImage> pSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pPlayer); f.Add(4,&pState); f.Add(5,&pSelection); return 0; }

public:
	CUnitPortraitState() {}
	CUnitPortraitState( const SWindowInfo &sInfo, NRPG::CGlobalPlayer *pPlayer, NRPG::CUnit *pMerc );

	void SetSelected( bool bState );

	NRPG::CUnit* GetMerc() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitPortraitState::CUnitPortraitState( const SWindowInfo &sInfo, NRPG::CGlobalPlayer *_pPlayer, NRPG::CUnit *_pMerc ):
	CWindow( sInfo ), pPlayer( _pPlayer ), pMerc( _pMerc )
{
	pSelection = new CImage( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "", STYLE_ENABLED ) );
	pSelection->SetImage( NDb::GetUITexture( 537 ) );
	pSelection->SetSizeFromImage( NDb::GetUITexture( 537 ) );

	pState = new CImage( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TOPMOST ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPortraitState::SetSelected( bool bState )
{
	pSelection->SetStyle( STYLE_VISIBLE, bState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CUnit* CUnitPortraitState::GetMerc() const
{
	return pMerc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPortraitState::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
		{
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;
		}
	case EVENT_LBUTTONUP:
		return true;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPortraitState::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pMerc ) )
	{
		pState->SetStyle( STYLE_VISIBLE, false );
		if ( pMerc->IsDead() )
		{
			pState->SetStyle( STYLE_VISIBLE, true );
			pState->SetImage( NDb::GetUITexture( 538 ) );
			pState->SetSizeFromImage( NDb::GetUITexture( 538 ) );
		}
		else
		{
			for ( int nTemp = 0; nTemp < pPlayer->mercs.size(); nTemp++ )
			{
				if ( !IsValid( pPlayer->mercs[nTemp] ) )
					continue;

				if ( pPlayer->mercs[nTemp] == pMerc )
				{
					pState->SetStyle( STYLE_VISIBLE, true );
					pState->SetImage( NDb::GetUITexture( 536 ) );
					pState->SetSizeFromImage( NDb::GetUITexture( 536 ) );
					break;
				}
			}
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitCharacterPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitCharacterPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitCharacterPanel)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	////
	CPtr<CText> pName;
	CPtr<CText> pLevel;
	CPtr<CImage> pClass;
	CPtr<CProgressBar> pLevelBar;
	//// Primary
	CPtr<CText> pEvasion;
	CPtr<CText> pStrength;
	CPtr<CText> pDexterity;
	CPtr<CText> pIntelligence;
	CPtr<CText> pActionPoints;
	CPtr<CText> pVitalityPoints;
	CPtr<CProgressBar> pEvasionBar;
	CPtr<CProgressBar> pStrengthBar;
	CPtr<CProgressBar> pDexterityBar;
	CPtr<CProgressBar> pIntelligenceBar;
	CPtr<CProgressBar> pActionPointsBar;
	CPtr<CProgressBar> pVitalityPointsBar;
	//// Secondary
	CPtr<CText> pHide;
	CPtr<CText> pSpot;
	CPtr<CText> pBurst;
	CPtr<CText> pMelee;
	CPtr<CText> pSnipe;
	CPtr<CText> pMedicine;
	CPtr<CText> pShooting;
	CPtr<CText> pThrowing;
	CPtr<CText> pInterrupt;
	CPtr<CText> pEngineering;
	CPtr<CProgressBar> pHideBar;
	CPtr<CProgressBar> pSpotBar;
	CPtr<CProgressBar> pBurstBar;
	CPtr<CProgressBar> pMeleeBar;
	CPtr<CProgressBar> pSnipeBar;
	CPtr<CProgressBar> pMedicineBar;
	CPtr<CProgressBar> pShootingBar;
	CPtr<CProgressBar> pThrowingBar;
	CPtr<CProgressBar> pInterruptBar;
	CPtr<CProgressBar> pEngineeringBar;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pName); f.Add(4,&pLevel); f.Add(5,&pClass); f.Add(6,&pLevelBar); f.Add(7,&pEvasion); f.Add(8,&pStrength); f.Add(9,&pDexterity); f.Add(10,&pIntelligence); f.Add(11,&pActionPoints); f.Add(12,&pVitalityPoints); f.Add(13,&pEvasionBar); f.Add(14,&pStrengthBar); f.Add(15,&pDexterityBar); f.Add(16,&pIntelligenceBar); f.Add(17,&pActionPointsBar); f.Add(18,&pVitalityPointsBar); f.Add(19,&pHide); f.Add(20,&pSpot); f.Add(21,&pBurst); f.Add(22,&pMelee); f.Add(23,&pSnipe); f.Add(24,&pMedicine); f.Add(25,&pShooting); f.Add(26,&pThrowing); f.Add(27,&pInterrupt); f.Add(28,&pEngineering); f.Add(29,&pHideBar); f.Add(30,&pSpotBar); f.Add(31,&pBurstBar); f.Add(32,&pMeleeBar); f.Add(33,&pSnipeBar); f.Add(34,&pMedicineBar); f.Add(35,&pShootingBar); f.Add(36,&pThrowingBar); f.Add(37,&pInterruptBar); f.Add(38,&pEngineeringBar); return 0; }

protected:
	void Generate();

public:
	CUnitCharacterPanel() {}
	CUnitCharacterPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitCharacterPanel::CUnitCharacterPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
	CWindow( sInfo ), pMerc( _pMerc )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitCharacterPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pName = GetUIWindow<CText>( this, "name" );
			pLevel = GetUIWindow<CText>( this, "level" );
			pClass = GetUIWindow<CImage>( this, "class" );
			pLevelBar = GetUIWindow<CProgressBar>( this, "level_bar" );

			pEvasion = GetUIWindow<CText>( this, "evasion" );
			pStrength = GetUIWindow<CText>( this, "strength" );
			pDexterity = GetUIWindow<CText>( this, "dexterity" );
			pIntelligence = GetUIWindow<CText>( this, "intelligence" );
			pActionPoints = GetUIWindow<CText>( this, "ap" );
			pVitalityPoints = GetUIWindow<CText>( this, "vp" );
			pEvasionBar = GetUIWindow<CProgressBar>( this, "evasion_bar" );
			pStrengthBar = GetUIWindow<CProgressBar>( this, "strength_bar" );
			pDexterityBar = GetUIWindow<CProgressBar>( this, "dexterity_bar" );
			pIntelligenceBar = GetUIWindow<CProgressBar>( this, "intelligence_bar" );
			pActionPointsBar = GetUIWindow<CProgressBar>( this, "ap_bar" );
			pVitalityPointsBar = GetUIWindow<CProgressBar>( this, "vp_bar" );

			pHide = GetUIWindow<CText>( this, "hide" );
			pSpot = GetUIWindow<CText>( this, "spot" );
			pBurst = GetUIWindow<CText>( this, "burst" );
			pMelee = GetUIWindow<CText>( this, "melee" );
			pSnipe = GetUIWindow<CText>( this, "snipe" );
			pMedicine = GetUIWindow<CText>( this, "medicine" );
			pShooting = GetUIWindow<CText>( this, "shooting" );
			pThrowing = GetUIWindow<CText>( this, "throwing" );
			pInterrupt = GetUIWindow<CText>( this, "interrupt" );
			pEngineering = GetUIWindow<CText>( this, "engineering" );
			pHideBar = GetUIWindow<CProgressBar>( this, "hide_bar" );
			pSpotBar = GetUIWindow<CProgressBar>( this, "spot_bar" );
			pBurstBar = GetUIWindow<CProgressBar>( this, "burst_bar" );
			pMeleeBar = GetUIWindow<CProgressBar>( this, "melee_bar" );
			pSnipeBar = GetUIWindow<CProgressBar>( this, "snipe_bar" );
			pMedicineBar = GetUIWindow<CProgressBar>( this, "medicine_bar" );
			pShootingBar = GetUIWindow<CProgressBar>( this, "shooting_bar" );
			pThrowingBar = GetUIWindow<CProgressBar>( this, "throwing_bar" );
			pInterruptBar = GetUIWindow<CProgressBar>( this, "interrupt_bar" );
			pEngineeringBar = GetUIWindow<CProgressBar>( this, "engineering_bar" );

			Generate();
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitCharacterPanel::Generate()
{
	if ( !IsValid( pMerc ) )
		return;

	WCHAR wsText[256];
	NRPG::CUnit *pUnit = pMerc;

	swprintf( wsText, L"<font face=Courier size=16pt><center>%s", pUnit->GetName().c_str() );
	pName->SetText( wsText );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_LEVEL ) );
	pLevel->SetText( wsText );
	pLevelBar->SetValue( pUnit->Skills( NDb::ST_LEVEL ).GetProgress() );
	if ( pUnit->GetPers()->pClass )
		pClass->SetImage( pUnit->GetPers()->pClass->pIcon );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_STR ) );
	pStrength->SetText( wsText );
	pStrengthBar->SetValue( pUnit->Skills( NDb::ST_STR ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_DEX ) );
	pDexterity->SetText( wsText );
	pDexterityBar->SetValue( pUnit->Skills( NDb::ST_DEX ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_INT ) );
	pIntelligence->SetText( wsText );
	pIntelligenceBar->SetValue( pUnit->Skills( NDb::ST_INT ).GetProgress() );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_IC ) );
	pEvasion->SetText( wsText );
	pEvasionBar->SetValue( pUnit->Skills( NDb::ST_IC ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_AP ) );
	pActionPoints->SetText( wsText );
	pActionPointsBar->SetValue( pUnit->Skills( NDb::ST_AP ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_VP ) );
	pVitalityPoints->SetText( wsText );
	pVitalityPointsBar->SetValue( pUnit->Skills( NDb::ST_VP ).GetProgress() );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_STEALTH ) );
	pHide->SetText( wsText );
	pHideBar->SetValue( pUnit->Skills( NDb::ST_STEALTH ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SPOT ) );
	pSpot->SetText( wsText );
	pSpotBar->SetValue( pUnit->Skills( NDb::ST_SPOT ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_BURST ) );
	pBurst->SetText( wsText );
	pBurstBar->SetValue( pUnit->Skills( NDb::ST_BURST ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_MELEE ) );
	pMelee->SetText( wsText );
	pMeleeBar->SetValue( pUnit->Skills( NDb::ST_MELEE ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SNIPE ) );
	pSnipe->SetText( wsText );
	pSnipeBar->SetValue( pUnit->Skills( NDb::ST_SNIPE ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_MEDICINE ) );
	pMedicine->SetText( wsText );
	pMedicineBar->SetValue( pUnit->Skills( NDb::ST_MEDICINE ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SHOOTING ) );
	pShooting->SetText( wsText );
	pShootingBar->SetValue( pUnit->Skills( NDb::ST_SHOOTING ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_THROWING ) );
	pThrowing->SetText( wsText );
	pThrowingBar->SetValue( pUnit->Skills( NDb::ST_THROWING ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_INTERRUPT ) );
	pInterrupt->SetText( wsText );
	pInterruptBar->SetValue( pUnit->Skills( NDb::ST_INTERRUPT ).GetProgress() );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_ENGINEERING ) );
	pEngineering->SetText( wsText );
	pEngineeringBar->SetValue( pUnit->Skills( NDb::ST_ENGINEERING ).GetProgress() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitInventoryPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitInventoryPanelItem: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitInventoryPanelItem)
private:
	ZDATA_(CWindow)
	int nCount;
	CPtr<NRPG::IInventoryItem> pItem;
	////
	CPtr<CText> pText;
	CPtr<CShowItemModel> pItemModel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nCount); f.Add(3,&pItem); f.Add(4,&pText); f.Add(5,&pItemModel); return 0; }

public:
	CUnitInventoryPanelItem() {}
	CUnitInventoryPanelItem( const SWindowInfo &sInfo, NRPG::IInventoryItem *pItem, int nCount );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitInventoryPanelItem::CUnitInventoryPanelItem( const SWindowInfo &sInfo, NRPG::IInventoryItem *_pItem, int _nCount ):
	CWindow( sInfo ), pItem( _pItem ), nCount( _nCount )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitInventoryPanelItem::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pItemModel = new CShowItemModel( sEvent.pLoader->GetControl( "view" ) );
			pItemModel->Set( pItem, NDb::CAMERA_SLOT );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pText = GetUIWindow<CText>( this, "text" );
			WCHAR wsBuffer[256];
			swprintf( wsBuffer, L"x%d", nCount );
			pText->SetText( wsBuffer );

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitInventoryPanelItem::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SRect sScrWindow;
	SPoint sScrPosition;
	if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
		return;

	VirtualToScreen( &sScrPosition, &sScrWindow );

	SRect sDummyRect;
	SPoint sSize( GetSize() );
	VirtualToScreen( &sSize, &sDummyRect );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sSize.x, sSize.y ) );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitInventoryPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitInventoryPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitInventoryPanel)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	////
	CObj<CListView> pList1;
	CObj<CListView> pList2;
	CObj<CScrollWindow<CListView> > pListView1;
	CObj<CScrollWindow<CListView> > pListView2;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pList1); f.Add(4,&pList2); f.Add(5,&pListView1); f.Add(6,&pListView2); return 0; }

protected:
	void Generate();

public:
	CUnitInventoryPanel() {}
	CUnitInventoryPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitInventoryPanel::CUnitInventoryPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
	CWindow( sInfo ), pMerc( _pMerc )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitInventoryPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pListView1 = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view_1" ) );
			pList1 = pListView1->GetClientWindow();

			pListView2 = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view_2" ) );
			pList2 = pListView2->GetClientWindow();

			Generate();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pListView1->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			pListView2->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitInventoryPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SRect sScrWindow;
	SPoint sScrPosition;
	if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
		return;

	VirtualToScreen( &sScrPosition, &sScrWindow );

	SRect sDummyRect;
	SPoint sSize( GetSize() );
	VirtualToScreen( &sSize, &sDummyRect );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sSize.x, sSize.y ) );

	pView->CreateDynamicClearRects( sLayout, sScrPosition, sScrWindow, 0 );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInventoryItem
{
	int nCount;
	CPtr<NRPG::IInventoryItem> pItem;
};
static void AddItem( list<SInventoryItem> *pList, NRPG::IInventoryItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;

	for ( list<SInventoryItem>::iterator iTemp = pList->begin(); iTemp != pList->end(); iTemp++ )
	{
		if ( iTemp->pItem->GetDBItem() != pItem->GetDBItem() )
			continue;

		iTemp->nCount++;
		return;
	}

	SInventoryItem &sItem = *pList->insert( pList->end(), SInventoryItem());
	sItem.pItem = pItem;
	sItem.nCount = 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitInventoryPanel::Generate()
{
	CPtr<NRPG::IInventory> pInventory = pMerc->GetInventory();


	list<SInventoryItem> itemsList;
	for ( int nTemp = 0; nTemp < NDb::N_SLOTS; nTemp++ )
		AddItem( &itemsList, pInventory->Get( NDb::ESlot( nTemp ) ) );

	const vector<NRPG::SBackPackItem> &itemsSet = pInventory->GetItems();
	for( vector<NRPG::SBackPackItem>::const_iterator iTemp = itemsSet.begin(); iTemp != itemsSet.end(); iTemp++ )
		AddItem( &itemsList, iTemp->pItem );

	int nCount = 0;
	for ( list<SInventoryItem>::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CPtr<CListView> pList;
		if ( nCount & 1 )
			pList = pList2;
		else
			pList = pList1;

		CPtr<CUnitInventoryPanelItem> pShowItem = new CUnitInventoryPanelItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), iTemp->pItem, iTemp->nCount );
		LoadTemplate( pShowItem, NDb::GetUIContainer( 323 ) );

		pList->AddItem( nCount, pShowItem );
		nCount++;
	}
	for ( ; ( nCount < N_MAXVISIBLE_INVENTORYITEMS ); nCount++ )
	{
		CPtr<CListView> pList;
		if ( nCount & 1 )
			pList = pList2;
		else
			pList = pList1;

		CPtr<CWindow> pShowItem = new CWindow( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
		LoadTemplate( pShowItem, NDb::GetUIContainer( 323 ) );
		pList->AddItem( nCount, pShowItem );
	}
	for ( int nTemp = pList2->GetItemsCount(); nTemp < pList1->GetItemsCount(); nTemp++ )
	{
		CPtr<CWindow> pShowItem = new CWindow( SWindowInfo( pList2, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
		LoadTemplate( pShowItem, NDb::GetUIContainer( 323 ) );
		pList2->AddItem( nCount, pShowItem );

		nCount++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTeamMngUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CButtonsPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CButtonsPanel)
private:
	enum EState
	{
		STATE_NORMAL,
		STATE_CHECKED,
		STATE_DISABLED
	};

	ZDATA_(CWindow)
	CPtr<CButton> pHire;
	CPtr<CButton> pFire;
	CPtr<CButton> pPerks;
	CPtr<CButton> pMedals;
	CPtr<CButton> pCharacter;
	CPtr<CButton> pInventory;
	CPtr<CButton> pBiography;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pHire); f.Add(3,&pFire); f.Add(4,&pPerks); f.Add(5,&pMedals); f.Add(6,&pCharacter); f.Add(7,&pInventory); f.Add(8,&pBiography); return 0; }

public:
	CButtonsPanel() {}
	CButtonsPanel( const SWindowInfo &sInfo );

	void SetMode( EPanel ePanel );
	void SetHireMode( bool bHire, bool bEnabled );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CButtonsPanel::CButtonsPanel( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButtonsPanel::SetMode( EPanel ePanel )
{
	bool bCharacter = false, bInventory = false;
	switch( ePanel )
	{
	case PANEL_CHARACTER:
		bCharacter = true;
		break;
	case PANEL_INVENTORY:
		bInventory = true;
		break;
	}

	pCharacter->SetActiveState( !bCharacter ? STATE_NORMAL : STATE_CHECKED );
	pInventory->SetActiveState( !bInventory ? STATE_NORMAL : STATE_CHECKED );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButtonsPanel::SetHireMode( bool bHire, bool bEnabled )
{
	pHire->SetStyle( STYLE_VISIBLE, bHire );
	pHire->SetStyle( STYLE_ENABLED, bEnabled );
	pHire->SetActiveState( bEnabled ? STATE_NORMAL : STATE_DISABLED );

	pFire->SetStyle( STYLE_VISIBLE, !bHire );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CButtonsPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pHire = GetUIWindow<CButton>( this, "hire" );
			pHire->AddImageState( STATE_NORMAL, NDb::GetUITexture( 618 ) );
			pHire->AddImageState( STATE_DISABLED, NDb::GetUITexture( 619 ) );

			pFire = GetUIWindow<CButton>( this, "fire" );
			pFire->AddImageState( STATE_NORMAL, NDb::GetUITexture( 437 ) );

			pPerks = GetUIWindow<CButton>( this, "perks" );
			pPerks->AddImageState( STATE_NORMAL, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( STATE_CHECKED, NDb::GetUITexture( 430 ), NGfx::SPixel8888( 0x7F, 0x7F, 0x7F, 0x7F ) );

			pMedals = GetUIWindow<CButton>( this, "medals" );
			pMedals->AddImageState( STATE_NORMAL, NDb::GetUITexture( 418 ) );
			pMedals->AddImageState( STATE_CHECKED, NDb::GetUITexture( 418 ), NGfx::SPixel8888( 0x7F, 0x7F, 0x7F, 0x7F ) );

			pCharacter = GetUIWindow<CButton>( this, "character" );
			pCharacter->AddImageState( STATE_NORMAL, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( STATE_CHECKED, NDb::GetUITexture( 383 ), NGfx::SPixel8888( 0x7F, 0x7F, 0x7F, 0x7F ) );

			pInventory = GetUIWindow<CButton>( this, "inventory" );
			pInventory->AddImageState( STATE_NORMAL, NDb::GetUITexture( 395 ) );
			pInventory->AddImageState( STATE_CHECKED, NDb::GetUITexture( 395 ), NGfx::SPixel8888( 0x7F, 0x7F, 0x7F, 0x7F ) );

			pBiography = GetUIWindow<CButton>( this, "biography" );
			pBiography->AddImageState( STATE_NORMAL, NDb::GetUITexture( 429 ) );
			pBiography->AddImageState( STATE_CHECKED, NDb::GetUITexture( 429 ), NGfx::SPixel8888( 0x7F, 0x7F, 0x7F, 0x7F ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTeamMngUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTeamMngUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CTeamMngUI)
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	////
	EPanel ePanel;
	CObj<NRPG::CUnit> pMerc;
	////
	CObj<CWindow> pPanel;
	CPtr<CWindow> pPanelBase;
	////
	CObj<CFlashButton> pCloseButton;
	CObj<CButtonsPanel> pButtonsPanel;
	CPtr<CUnitPortraitView> pSelected;
	vector<CObj<CUnitPortraitView> > unitsViewSet;
	vector<CObj<CUnitPortraitState> > unitsStateSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGlobalPlayer); f.Add(3,&ePanel); f.Add(4,&pMerc); f.Add(5,&pPanel); f.Add(6,&pPanelBase); f.Add(7,&pCloseButton); f.Add(8,&pButtonsPanel); f.Add(9,&pSelected); f.Add(10,&unitsViewSet); f.Add(11,&unitsStateSet); return 0; }

public:
	CTeamMngUI() {}
	CTeamMngUI( const SWindowInfo &sInfo, NRPG::CGlobalPlayer *_pGlobalPlayer );

	void Set( NRPG::CUnit *pMerc, EPanel ePanel = PANEL_DEFAULT );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTeamMngUI::CTeamMngUI( const SWindowInfo &sInfo,  NRPG::CGlobalPlayer *_pGlobalPlayer ):
	CWindow( sInfo ), pGlobalPlayer( _pGlobalPlayer ), unitsViewSet( N_MAX_UNITS ), unitsStateSet( N_MAX_UNITS ), ePanel( PANEL_NONE )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngUI::Set( NRPG::CUnit *_pMerc, EPanel _ePanel )
{
	if ( IsValid( _pMerc ) )
		pMerc = _pMerc;
	if ( _ePanel != PANEL_DEFAULT )
		ePanel = _ePanel;

	pPanel = 0;
	switch( ePanel )
	{
	case PANEL_NONE:
	case PANEL_CHARACTER:
		{
			ePanel = PANEL_CHARACTER;
			pPanel = new CUnitCharacterPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 318 ) );
			break;
		}
	case PANEL_INVENTORY:
		{
			ePanel = PANEL_INVENTORY;
			pPanel = new CUnitInventoryPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 319 ) );
			break;
		}
	}

	for( int nTemp = 0; nTemp < unitsStateSet.size(); nTemp++ )
	{
		CPtr<CUnitPortraitState> pState = unitsStateSet[nTemp];

		if( pState->GetMerc() == pMerc )
			pState->SetSelected( true );
		else
			pState->SetSelected( false );
	}

	pButtonsPanel->SetMode( ePanel );

	vector<CObj<NRPG::CUnit> >::iterator iTemp = find( pGlobalPlayer->mercs.begin(), pGlobalPlayer->mercs.end(), pMerc );
	if ( iTemp == pGlobalPlayer->mercs.end() )
		pButtonsPanel->SetHireMode( true, ( pGlobalPlayer->mercs.size() <= N_MAX_PLAYER_UNITS ) && !pMerc->IsDead() );
	else
		pButtonsPanel->SetHireMode( false, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTeamMngUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "hire" )
			{
				if ( IsValid( pMerc ) && pGlobalPlayer->mercs.size() <= N_MAX_PLAYER_UNITS )
				{
					vector<CObj<NRPG::CUnit> >::iterator iTemp = find( pGlobalPlayer->mercs.begin(), pGlobalPlayer->mercs.end(), pMerc );
					if ( ( iTemp == pGlobalPlayer->mercs.end() ) && !pMerc->IsDead() && !pMerc->IsUnconscious() )
						pGlobalPlayer->Hire( pMerc );
				}

				Set( pMerc );
				return true;
			}
			else if ( sEvent.szID == "fire" )
			{
				if ( IsValid( pMerc ) )
				{
					vector<CObj<NRPG::CUnit> >::iterator iTemp = find( pGlobalPlayer->mercs.begin(), pGlobalPlayer->mercs.end(), pMerc );
					if ( iTemp != pGlobalPlayer->mercs.end() )
						pGlobalPlayer->Fire( pMerc );
				}

				Set( pMerc );
				return true;
			}

			for( int nTemp = 0; nTemp < unitsStateSet.size(); nTemp++ )
			{
				CPtr<CUnitPortraitState> pState = unitsStateSet[nTemp];

				if ( pState->GetWindowID() != sEvent.szID )
					continue;

				Set( pState->GetMerc() );
				return true;
			}
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );
			pButtonsPanel = new CButtonsPanel( sEvent.pLoader->GetControl( "buttons_panel" ) );

			for ( int nTemp = 0; nTemp < N_MAX_UNITS; nTemp++ )
			{
				NRPG::CUnit *pMerc = 0;
				if ( ( nTemp < pGlobalPlayer->totalMercs.size() ) && IsValid( pGlobalPlayer->totalMercs[nTemp] ) )
					pMerc = pGlobalPlayer->totalMercs[nTemp];

				unitsViewSet[nTemp] = new CUnitPortraitView( sEvent.pLoader->GetControl( NStr::Format( "unit_view_%d", nTemp ) ), pMerc );
				unitsStateSet[nTemp] = new CUnitPortraitState( sEvent.pLoader->GetControl( NStr::Format( "unit_%d", nTemp ) ), pGlobalPlayer, pMerc );
			}
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pPanelBase = GetUIWindow<CWindow>( this, "panel" );

			for ( int nTemp = 0; nTemp < pGlobalPlayer->totalMercs.size(); nTemp++ )
			{
				NRPG::CUnit *pMerc = pGlobalPlayer->totalMercs[nTemp];

				if ( pMerc->IsDead() )
					continue;

				Set( pMerc, PANEL_CHARACTER );
				break;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SRect sScrWindow;
	SPoint sScrPosition;
	if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
		return;

	VirtualToScreen( &sScrPosition, &sScrWindow );

	SRect sDummyRect;
	SPoint sSize( GetSize() );
	VirtualToScreen( &sSize, &sDummyRect );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sSize.x, sSize.y ) );

	pView->CreateDynamicClearRects( sLayout, sScrPosition, sScrWindow, 0 );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInGameMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTeamMngMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CTeamMngMenuInterface);
private:
	NInput::CBind bindClose;
	NInput::CBind bindCharacter, bindInventory;

	ZDATA
	CPtr<IMission> pMission;
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CTeamMngUI> pMenuUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); f.Add(3,&pMission); f.Add(4,&pGlobalPlayer); f.Add(5,&pCursor); f.Add(6,&pInterface); f.Add(7,&pMenuUI); f.Add(8,&pScreenShot); return 0; }
	int nID = 0;

public:
	CTeamMngMenuInterface();

	void Initialize( NRPG::CGlobalPlayer *pGlobalPlayer, IMission *pMission );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTeamMngMenuInterface::CTeamMngMenuInterface():
	bindClose( "cancel" ), bindCharacter( "character" ), bindInventory( "inventory" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngMenuInterface::Initialize( NRPG::CGlobalPlayer *_pGlobalPlayer, IMission *_pMission )
{
	pMission = _pMission;
	pGlobalPlayer = _pGlobalPlayer;

	for ( vector<CObj<NRPG::CUnit> >::iterator iTemp = pGlobalPlayer->mercs.begin(); iTemp != pGlobalPlayer->mercs.end(); )
	{
		if ( !(*iTemp)->IsDead() )
			iTemp++;
		else
			iTemp = pGlobalPlayer->mercs.erase( iTemp );
	}

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	pScreenShot->Generate();

	pMenuUI = new NUI::CTeamMngUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "ingamemenu", NUI::STYLE_ENABLED ), pGlobalPlayer );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 316 ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngMenuInterface::Step()
{
	MarkNewDGFrame();
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		RenderFrame();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTeamMngMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = pMission->GetActivePlayer()->GetGlobalPlayer();
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		pMission->GetUnits( &unitsSet );

		for ( int nTemp = 0; nTemp < pGlobalPlayer->mercs.size(); nTemp++ )
		{
			bool bFound = false;
			NRPG::CUnit *pMerc = pGlobalPlayer->mercs[nTemp];

			for ( int nUnit = 0; nUnit < unitsSet.size(); nUnit++ )
			{
				if ( unitsSet[nUnit]->GetUnit()->GetRPG()->GetRPGUnit() == pMerc )
					bFound = true;
			}

			if ( !bFound )
			{
				csSystem << "Unit created!" << endl;
				pMission->GetActivePlayer()->AddUnit( pMerc );
			}
		}
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		{
			bool bFound = false;
			IUnitTracker *pUnit = unitsSet[nTemp];

			for ( int nMerc = 0; nMerc < pGlobalPlayer->mercs.size(); nMerc++ )
			{
				if ( pUnit->GetUnit()->GetRPG()->GetRPGUnit() == pGlobalPlayer->mercs[nMerc] )
					bFound = true;
			}

			if ( !bFound )
				pMission->GetActivePlayer()->RemoveUnit( pUnit );
		}

		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	if ( bindCharacter.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_CHARACTER );
		return true;
	}
	else if ( bindInventory.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_INVENTORY );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTeamMngMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICTeamMngMenu::CICTeamMngMenu( NRPG::CGlobalPlayer *_pGlobalPlayer, IMission *_pMission ):
	pGlobalPlayer( _pGlobalPlayer ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICTeamMngMenu::Exec()
{
	CTeamMngMenuInterface *pRes = new CTeamMngMenuInterface();
	pRes->Initialize( pGlobalPlayer, pMission );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB0925140, CTeamMngMenuInterface );
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0925141, CTeamMngUI );
REGISTER_SAVELOAD_CLASS( 0xB0925142, CUnitCharacterPanel );
REGISTER_SAVELOAD_CLASS( 0xB0925143, CUnitPortraitView );
REGISTER_SAVELOAD_CLASS( 0xB0925144, CUnitPortraitState );
REGISTER_SAVELOAD_CLASS( 0xB0925145, CUnitInventoryPanel );
REGISTER_SAVELOAD_CLASS( 0xB0925146, CUnitInventoryPanelItem );
REGISTER_SAVELOAD_CLASS( 0xB0925147, CButtonsPanel );
