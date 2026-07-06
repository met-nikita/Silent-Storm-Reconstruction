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
#include "RPGPerk.h"					// NRPG::CPerk/CPerksTree (CUnitPerksPanel rows)
#include "..\DBFormat\DataPerk.h"		// NDb::CDBPerk (perk row icon/tooltip)
#include "..\DBFormat\DataMisc.h"		// NDb::CMedal (CUnitMedalsPanel rows)
#include "Interface.h"
#include "GSceneUtils.h"	// NGScene::CCFBTransform (UIWrap.h dependency)
#include "UIWrap.h"			// NUI::CImageDraw -- the recruit-card placeholder background
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
// retail EPanel (CTeamMngUI::Set @0x245c80 switch constants / CButtonsPanel::SetMode @0x2441b0
// cmp 2..6): the release recruit menu has FIVE tabs -- perks/medals/character/inventory/biography.
enum EPanel
{
	PANEL_DEFAULT,		// 0 -- "keep the current tab"
	PANEL_NONE,			// 1 -- unset (Set() coerces it to PANEL_BIOGRAPHY, the retail default tab)
	PANEL_PERKS,		// 2
	PANEL_MEDALS,		// 3
	PANEL_CHARACTER,	// 4
	PANEL_INVENTORY,	// 5
	PANEL_BIOGRAPHY		// 6
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPortraitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPortraitView: public CUnitView
{
	OBJECT_NOCOPY_METHODS(CUnitPortraitView)
private:
	ZDATA_(CUnitView)
	// retail @0x244470: the card-frame placeholder is a plain CImageDraw drawn FIRST as a
	// background (retail Draw @0x244130), NOT a child CImage window -- a child window renders
	// LAST inside CUnitView::Draw's CWindow::Draw tail, covering the 3D head every frame (the
	// recruitment cards showed only the placeholder "smoke" texture).
	CObj<CImageDraw> pImage;
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
	pImage = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ), NDb::GetUITexture( 535 ) );

	if ( IsValid( pMerc ) )
		SetUnit( pMerc );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPortraitView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// retail @0x244130: placeholder background FIRST, then the 3D head over it (CUnitView::Draw
	// does its own clear-rect depth punch and paints any child windows itself).
	if ( IsValid( pImage ) )
		pImage->Draw( this, sTime, pView );
	CUnitView::Draw( sTime, pView );
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
// CUnitPerksPanelItem -- one taken-perk row (icon + description text) of the recruit menu's perks
// tab. Retail ctor @0x244f80 / ProcessMessage @0x2492b0: TEMPLATELOADCOMPLETE wires the "text"
// child to GetDBString(pDBPerk->pToolTip) and the "icon" child to pDBPerk->pIcon.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPerksPanelItem: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitPerksPanelItem)
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CPerk> pPerk;
	////
	CPtr<CText> pText;
	CPtr<CImage> pImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pPerk); f.Add(3,&pText); f.Add(4,&pImage); return 0; }

public:
	CUnitPerksPanelItem() {}
	CUnitPerksPanelItem( const SWindowInfo &sInfo, NRPG::CPerk *_pPerk ):
		CWindow( sInfo ), pPerk( _pPerk )
	{
	}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPerksPanelItem::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOADCOMPLETE )
	{
		pText = GetUIWindow<CText>( this, "text" );
		pImage = GetUIWindow<CImage>( this, "icon" );

		NDb::CDBPerk *pDBPerk = IsValid( pPerk ) ? pPerk->GetDBPerk() : 0;
		if ( IsValid( pDBPerk ) )
		{
			pText->SetText( GetDBString( pDBPerk->pToolTip ) );
			pImage->SetImage( pDBPerk->pIcon );
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPerksPanel -- the recruit menu's "perks" tab: a scroll list of the merc's TAKEN perks.
// Retail ctor @0x244fc0 / ProcessMessage @0x249410 / Generate @0x2464c0.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitPerksPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitPerksPanel)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	////
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pList); f.Add(4,&pListView); return 0; }

protected:
	void Generate();

public:
	CUnitPerksPanel() {}
	CUnitPerksPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
		CWindow( sInfo ), pMerc( _pMerc )
	{
	}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPerksPanel::Generate()
{
	// retail @0x2464c0: walk ALL perks of the merc's tree, keep the TAKEN ones (first-match scan),
	// append one templated row per taken perk AT ITS allPerks INDEX, each with a description tooltip.
	if ( !IsValid( pMerc ) || !IsValid( pList ) )
		return;
	NRPG::CPerksTree *pPerksTree = pMerc->GetPerksTree();
	if ( !IsValid( pPerksTree ) )
		return;

	vector< CPtr<NRPG::CPerk> > allPerks, takenPerks;
	pPerksTree->GetAllPerks( &allPerks );
	pPerksTree->GetTakenPerks( &takenPerks );

	for ( int nTemp = 0; nTemp < allPerks.size(); nTemp++ )
	{
		if ( find( takenPerks.begin(), takenPerks.end(), allPerks[nTemp] ) == takenPerks.end() )
			continue;

		NRPG::CPerk *pPerk = allPerks[nTemp];
		CPtr<CUnitPerksPanelItem> pItem = new CUnitPerksPanelItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pPerk );
		LoadTemplate( pItem, NDb::GetUIContainer( 394 ) );	// retail 0x18a -- the perk-row template

		// retail attaches a perk-description CToolTip (GetDBString(name)+SetPerkParam substitutions);
		// the dev CDBPerk carries only pToolTip, so use it -- same idiom as the converged CPerkButton.
		NDb::CDBPerk *pDBPerk = pPerk->GetDBPerk();
		if ( IsValid( pDBPerk ) )
		{
			CPtr<CToolTip> pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
			pToolTip->SetText( GetDBString( pDBPerk->pToolTip ) );
			pItem->SetToolTip( pToolTip );
		}

		pList->AddItem( nTemp, pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPerksPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();
			Generate();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pListView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitMedalsPanelItem -- one earned-medal row (icon + name) of the recruit menu's medals tab.
// Retail ctor @0x245000 / ProcessMessage @0x249580: "text" <- GetDBString(pMedal->pName),
// "icon" <- pMedal->pImage.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitMedalsPanelItem: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitMedalsPanelItem)
private:
	ZDATA_(CWindow)
	CDBPtr<NDb::CMedal> pMedal;
	////
	CPtr<CText> pText;
	CPtr<CImage> pImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMedal); f.Add(3,&pText); f.Add(4,&pImage); return 0; }

public:
	CUnitMedalsPanelItem() {}
	CUnitMedalsPanelItem( const SWindowInfo &sInfo, NDb::CMedal *_pMedal ):
		CWindow( sInfo ), pMedal( _pMedal )
	{
	}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMedalsPanelItem::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOADCOMPLETE )
	{
		pText = GetUIWindow<CText>( this, "text" );
		pImage = GetUIWindow<CImage>( this, "icon" );

		if ( IsValid( pMedal ) )
		{
			pText->SetText( GetDBString( pMedal->pName ) );
			pImage->SetImage( pMedal->pImage );
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitMedalsPanel -- the recruit menu's "medals" tab: the merc's earned medals as a scroll list.
// Retail ctor @0x245040 / ProcessMessage @0x2496e0 / Generate @0x246ab0. NOTE: the row source
// CMedalsGainer::GetGainedMedals is still the documented dev STUB (empty until NDb::CSide::medals
// lands) -- the tab lights up and shows the retail "no awards" empty list until then.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitMedalsPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitMedalsPanel)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	////
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pList); f.Add(4,&pListView); return 0; }

protected:
	void Generate();

public:
	CUnitMedalsPanel() {}
	CUnitMedalsPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
		CWindow( sInfo ), pMerc( _pMerc )
	{
	}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitMedalsPanel::Generate()
{
	// retail @0x246ab0: one templated row per gained medal, appended at its gained-list index.
	if ( !IsValid( pMerc ) || !IsValid( pList ) )
		return;

	vector< CDBPtr<NDb::CMedal> > gainedMedals;
	pMerc->GetGainedMedals( &gainedMedals );

	for ( int nTemp = 0; nTemp < gainedMedals.size(); nTemp++ )
	{
		CPtr<CUnitMedalsPanelItem> pItem = new CUnitMedalsPanelItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), gainedMedals[nTemp] );
		LoadTemplate( pItem, NDb::GetUIContainer( 416 ) );	// retail 0x1a0 -- the medal-row template
		pList->AddItem( nTemp, pItem );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitMedalsPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();
			Generate();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pListView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitBiographyPanel -- the recruit menu's "biography" tab (the retail DEFAULT tab): the persona
// photo, the scrollable bio prose and the "characteristics" block. Retail ctor @0x244f30 /
// ProcessMessage @0x248e90: both text blocks are GetDBString(0x4f22 markup header) + the persona's
// record string; the photo is pPers->pPhoto. (v1.2 appends three award widgets to this panel --
// not ported yet.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitBiographyPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CUnitBiographyPanel)
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	////
	CObj<CImage> pPhoto;
	CObj<CMLText> pText;
	CObj<CMLText> pCharacteristics;
	CObj<CScrollWindow<CMLText> > pTextView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pPhoto); f.Add(4,&pText); f.Add(5,&pCharacteristics); f.Add(6,&pTextView); return 0; }

public:
	CUnitBiographyPanel() {}
	CUnitBiographyPanel( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
		CWindow( sInfo ), pMerc( _pMerc )
	{
	}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitBiographyPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			NDb::CRPGPers *pPers = IsValid( pMerc ) ? pMerc->GetPers() : 0;

			pTextView = new CScrollWindow<CMLText>( sEvent.pLoader->GetControl( "view" ) );
			pText = pTextView->GetClientWindow();
			if ( IsValid( pText ) && IsValid( pPers ) )
			{
				// retail: GetDBString(0x4f22) markup header + the persona's biography string,
				// then grow the client to its real text height (width preserved) for the scroll.
				pText->SetText( GetDBString( 0x4F22 ) + GetDBString( pPers->pBiography ), true );
				SPoint sRealSize;
				pText->GetRealSize( &sRealSize );
				pText->SetSize( SPoint( pText->GetSize().x, sRealSize.y ) );
			}

			pCharacteristics = new CMLText( sEvent.pLoader->GetControl( "characteristics" ) );
			if ( IsValid( pPers ) )
				pCharacteristics->SetText( GetDBString( 0x4F22 ) + GetDBString( pPers->pCharacteristics ), true );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pPhoto = GetUIWindow<CImage>( this, "image" );
			NDb::CRPGPers *pPers = IsValid( pMerc ) ? pMerc->GetPers() : 0;
			if ( IsValid( pPers ) && IsValid( pPers->pPhoto ) )
				pPhoto->SetImage( pPers->pPhoto );

			pTextView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CKIAPanel -- the "killed in action" name plate shown over a dead merc's page. Retail ctor
// @0x2450d0 / Set @0x244440 / ProcessMessage @0x249850; its template is the SIDE record's
// pKIAPaper container (CTeamMngUI::ProcessMessage reads side+0x70).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CKIAPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CKIAPanel)
private:
	ZDATA_(CWindow)
	CObj<CText> pName;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pName); return 0; }

public:
	CKIAPanel() {}
	CKIAPanel( const SWindowInfo &sInfo ): CWindow( sInfo ) {}

	void Set( NRPG::CUnit *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CKIAPanel::Set( NRPG::CUnit *pUnit )
{
	// retail @0x244440: print the merc's full name into the plate's "text" label.
	if ( !IsValid( pUnit ) || !IsValid( pName ) )
		return;
	pName->SetText( pUnit->wsFullName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CKIAPanel::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOADCOMPLETE )
		pName = GetUIWindow<CText>( this, "text" );

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTeamMngUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CButtonsPanel (iTeamMngMenu.obj): SEVEN CComplexButtons -- Hire/Fire plus the five tab
// buttons -- built on EVENT_TEMPLATELOAD from the loader controls (ctor @0x245080, ProcessMessage
// @0x245450); SetMode @0x2441b0 lights exactly one tab via SetChecked; SetHireMode @0x244220 flips
// the Hire/Fire pair. The old dev panel used bare CButtons with the DISABLED art (418 medals /
// 429 biography) as the NORMAL image -- the "always gray" buttons; the retail icons are
// medals=400, biography=381 (and perks=430, character=383, inventory=395), on the standard
// 566/408 unchecked/checked plates (disasm-verified GetUITexture literals @0x6454c1..0x645c1e).
class CButtonsPanel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CButtonsPanel)
private:
	ZDATA_(CWindow)
	CPtr<CComplexButton> pHire;
	CPtr<CComplexButton> pFire;
	CPtr<CComplexButton> pPerks;
	CPtr<CComplexButton> pMedals;
	CPtr<CComplexButton> pCharacter;
	CPtr<CComplexButton> pInventory;
	CPtr<CComplexButton> pBiography;
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
	// retail @0x2441b0: exactly one of the five tab buttons is checked (cmp 2..6).
	pPerks->SetChecked( ePanel == PANEL_PERKS );
	pMedals->SetChecked( ePanel == PANEL_MEDALS );
	pCharacter->SetChecked( ePanel == PANEL_CHARACTER );
	pInventory->SetChecked( ePanel == PANEL_INVENTORY );
	pBiography->SetChecked( ePanel == PANEL_BIOGRAPHY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButtonsPanel::SetHireMode( bool bHire, bool bEnabled )
{
	// retail @0x244220: show Hire (enabled per bEnabled) XOR show Fire. (Jan03's extra
	// SetActiveState line is absent from the release body.)
	pHire->SetStyle( STYLE_VISIBLE, bHire );
	pHire->SetStyle( STYLE_ENABLED, bEnabled );

	pFire->SetStyle( STYLE_VISIBLE, !bHire );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CButtonsPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			// retail @0x245450: hire/fire NORMAL, the five tabs UNCHECKED; all on the shared
			// 566 (unchecked plate) / 408 (checked plate) base pair.
			pHire = new CComplexButton( sEvent.pLoader->GetControl( "hire" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pHire->Set( NDb::GetUITexture( 618 ), NDb::GetUITexture( 619 ), CComplexButton::NORMAL );

			pFire = new CComplexButton( sEvent.pLoader->GetControl( "fire" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pFire->Set( NDb::GetUITexture( 437 ), 0, CComplexButton::NORMAL );

			pPerks = new CComplexButton( sEvent.pLoader->GetControl( "perks" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pPerks->Set( NDb::GetUITexture( 430 ), 0, CComplexButton::UNCHECKED );

			pMedals = new CComplexButton( sEvent.pLoader->GetControl( "medals" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pMedals->Set( NDb::GetUITexture( 400 ), 0, CComplexButton::UNCHECKED );

			pCharacter = new CComplexButton( sEvent.pLoader->GetControl( "character" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pCharacter->Set( NDb::GetUITexture( 383 ), 0, CComplexButton::UNCHECKED );

			pInventory = new CComplexButton( sEvent.pLoader->GetControl( "inventory" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pInventory->Set( NDb::GetUITexture( 395 ), 0, CComplexButton::UNCHECKED );

			pBiography = new CComplexButton( sEvent.pLoader->GetControl( "biography" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pBiography->Set( NDb::GetUITexture( 381 ), 0, CComplexButton::UNCHECKED );
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
	CObj<CKIAPanel> pKIAPanel;			// retail +0x94 (save tag 7) -- the dead-merc name plate
	CObj<CFlashButton> pCloseButton;
	CObj<CButtonsPanel> pButtonsPanel;
	CPtr<CUnitPortraitView> pSelected;
	vector<CObj<CUnitPortraitView> > unitsViewSet;
	vector<CObj<CUnitPortraitState> > unitsStateSet;
	// retail CTeamMngUI::operator& @0x24c6f0 tag map (disasm): 2 pGlobalPlayer, 3 ePanel, 4 pMerc,
	// 5 pPanel, 6 pPanelBase, 7 pKIAPanel, 8 pCloseButton, 9 pButtonsPanel, 10 pSelected,
	// 11 unitsViewSet, 12 unitsStateSet. (Dev had 7..11 without the KIA plate -- off by one vs retail saves.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGlobalPlayer); f.Add(3,&ePanel); f.Add(4,&pMerc); f.Add(5,&pPanel); f.Add(6,&pPanelBase); f.Add(7,&pKIAPanel); f.Add(8,&pCloseButton); f.Add(9,&pButtonsPanel); f.Add(10,&pSelected); f.Add(11,&unitsViewSet); f.Add(12,&unitsStateSet); return 0; }

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
	// retail @0x245c80: selecting a DIFFERENT merc plays their on-select greeting ack
	// (NUI::GetPersAck @0x2452c0 -> CWindow::PlaySound, this->vtbl+0x20). PlaySound null-guards.
	if ( IsValid( _pMerc ) )
	{
		if ( pMerc != _pMerc )
			PlaySound( GetPersVoiceAck( _pMerc ) );
		pMerc = _pMerc;
	}
	if ( _ePanel != PANEL_DEFAULT )
		ePanel = _ePanel;
	if ( ePanel == PANEL_NONE )
		ePanel = PANEL_BIOGRAPHY;		// retail: the default tab is the biography

	pPanel = 0;
	switch( ePanel )
	{
	// retail panel switch (container ids are the disasm literals @0x645e35..0x646220):
	case PANEL_PERKS:
		{
			pPanel = new CUnitPerksPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "perks", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 319 ) );	// 0x13f
			break;
		}
	case PANEL_MEDALS:
		{
			pPanel = new CUnitMedalsPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "medals", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 319 ) );	// 0x13f (shared list template)
			break;
		}
	case PANEL_CHARACTER:
		{
			pPanel = new CUnitCharacterPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "character", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 318 ) );	// 0x13e
			break;
		}
	case PANEL_INVENTORY:
		{
			pPanel = new CUnitInventoryPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "inventory", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 393 ) );	// 0x189 (retail; Jan03's 319 became the perks/medals list)
			break;
		}
	case PANEL_BIOGRAPHY:
		{
			pPanel = new CUnitBiographyPanel( SWindowInfo( pPanelBase, SPoint( 0, 0 ), pPanelBase->GetSize(), "biography", STYLE_ENABLED | STYLE_VISIBLE ), pMerc );
			LoadTemplate( pPanel, NDb::GetUIContainer( 382 ) );	// 0x17e
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

	// retail: a dead merc raises the KIA name plate and grays (transparents) the buttons row.
	bool bDead = IsValid( pMerc ) && pMerc->IsDead();
	if ( IsValid( pMerc ) )
		pKIAPanel->Set( pMerc );
	pKIAPanel->SetStyle( STYLE_VISIBLE, bDead );
	pButtonsPanel->SetStyle( STYLE_TRANSPARENT, bDead );	// retail SetStyle(0x20, bDead)
	if ( bDead )
		pKIAPanel->ShowWindow( SWTYPE_SHOW );

	vector<CObj<NRPG::CUnit> >::iterator iTemp = find( pGlobalPlayer->mercs.begin(), pGlobalPlayer->mercs.end(), pMerc );
	if ( iTemp == pGlobalPlayer->mercs.end() )
		pButtonsPanel->SetHireMode( true, ( pGlobalPlayer->mercs.size() <= N_MAX_PLAYER_UNITS ) && !bDead );
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
			// retail @0x2498f0: the KIA name plate is built FIRST, from the "kia_panel" control,
			// and skinned with the SIDE record's pKIAPaper template (side+0x70) when the player
			// has a valid side. (pKIAPaper comes from the retail game.db chunk stream, tag 18.)
			pKIAPanel = new CKIAPanel( sEvent.pLoader->GetControl( "kia_panel" ) );
			if ( IsValid( pGlobalPlayer->pSide ) && IsValid( pGlobalPlayer->pSide->pKIAPaper ) )
				LoadTemplate( pKIAPanel, pGlobalPlayer->pSide->pKIAPaper );

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

			// retail @0x2498f0: auto-select the FIRST roster entry (dead or not -- a dead one
			// shows the KIA plate) on the BIOGRAPHY tab, the release default.
			if ( !pGlobalPlayer->totalMercs.empty() )
				Set( pGlobalPlayer->totalMercs[0], PANEL_BIOGRAPHY );
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
	// retail CTeamMngMenuInterface ctor @0x245100 builds SIX binds: cancel, perks, medals,
	// character, inventory, biography (the tab buttons reach these via CInterface's
	// EVENT_NOTIFY -> NInput::PostEvent(szID) bridge, so the binds serve both mouse + hotkeys).
	NInput::CBind bindClose;
	NInput::CBind bindPerks, bindMedals;
	NInput::CBind bindCharacter, bindInventory, bindBiography;

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
	bindClose( "cancel" ), bindPerks( "perks" ), bindMedals( "medals" ),
	bindCharacter( "character" ), bindInventory( "inventory" ), bindBiography( "biography" )
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

	// retail ProcessEvent @0x247510: perks, medals, character, inventory, biography (in order).
	if ( bindPerks.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_PERKS );
		return true;
	}
	else if ( bindMedals.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_MEDALS );
		return true;
	}
	else if ( bindCharacter.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_CHARACTER );
		return true;
	}
	else if ( bindInventory.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_INVENTORY );
		return true;
	}
	else if ( bindBiography.ProcessEvent( sEvent ) )
	{
		pMenuUI->Set( 0, NUI::PANEL_BIOGRAPHY );
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
// retail iTeamMngMenu saveload ids (gen/classreg.json):
REGISTER_SAVELOAD_CLASS( 0xB0925148, CUnitBiographyPanel );
REGISTER_SAVELOAD_CLASS( 0xB0925149, CUnitPerksPanel );
REGISTER_SAVELOAD_CLASS( 0xB092514A, CUnitPerksPanelItem );
REGISTER_SAVELOAD_CLASS( 0xB092514B, CUnitMedalsPanel );
REGISTER_SAVELOAD_CLASS( 0xB092514C, CUnitMedalsPanelItem );
REGISTER_SAVELOAD_CLASS( 0xB3516170, CKIAPanel );
