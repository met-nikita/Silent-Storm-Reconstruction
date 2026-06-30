#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "wInterface.h"
#include "RPGItemInfo.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataRPG.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionExec.h"
#include "iLogPanel.h"
#include "iTopPanel.h"
#include "iUnitPanel.h"
#include "iPerksPanel.h"
#include "iStorePanel.h"
#include "iInventoryPanel.h"
#include "iCharacterPanel.h"
#include "iActionDecorator.h"
#include "UIWrap.h"
#include "rpgUnitInfo.h"
#include "RPGUnit.h"        // NRPG::CUnit complete type (GetRPGUnit()->GetVoice() for the in-game ack voice)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_ASK_TTL = 3000;
const int
	N_LOGPANEL_PAD = 20; // pad-zone in points
const int
	N_SCROLL_STEP				= 4,
	N_SCROLL_GUARDBAND	= 4;
const int
	N_HITPTRACKER_TTL		= 2000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckIcon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckIcon: public CWindow
{
	OBJECT_NOCOPY_METHODS(CAckIcon);
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CAckEvent> pEvent;
	////
	CPtr<CText> pText;
	CPtr<CUnitHead> pHead;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pEvent); f.Add(4,&pText); f.Add(5,&pHead); return 0; }

public:
	CAckIcon() {}
	CAckIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission ): CWindow( sInfo ), pMission( _pMission ) {}

	void Set( CAckEvent *pEvent );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline wstring ConvertLineBreaks( const wstring &szStr )
{
	wstring szRet;
	for ( wstring::const_iterator i = szStr.begin(); i != szStr.end(); )
	{
		switch ( wchar_t(*i) )
		{
			case L'\n':
				szRet += L"<br>";
				break;
			case L'\r':
				szRet += L"<br>";
				++i;
				if ( i != szStr.end() && *i == L'\n' )
					++i;
				continue;
			case 133: // symbol L'�'
				szRet += L"...";
				break;
			default:
				szRet += *i;
				break;
		}
		++i;
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckIcon::Set( CAckEvent *_pEvent )
{
	pEvent = _pEvent;

	NWorld::CAckEvent *pAckEvent = pEvent->GetAckEvent();
	if ( !IsValid( pAckEvent->pAckInfo ) )
		return;

	if ( pAckEvent->pUnit )
	{
		// Use the UNIT's chosen voice (GetRPGUnit()->GetVoice() == NRPG::CUnit::nVoice, set by SetVoice in the
		// FaceGen editor), NOT the persona's preset nVoice -- so the player's voice choice is heard in-game.
		// (retail reads *(GetRPGUnit()+0xa4); 0xa4 == NRPG::CUnit::nVoice, vs GetRPGPers()->nVoice = the preset.)
		const NDb::SAckVoice &voice = pAckEvent->pAckInfo->GetVoice( pAckEvent->pUnit->GetRPG()->GetRPGUnit()->GetVoice() );
		pHead->SetUnit( pAckEvent->pUnit );
		pHead->SetSequence( voice.pSequence );
		PlaySound( voice.pSound );
	}

	pText->SetText( ConvertLineBreaks( GetDBString( pAckEvent->pAckInfo->pText ) ) );

	SetStyle( STYLE_VISIBLE, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAckIcon::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_LBUTTONDOWN:
			return true;
		case EVENT_LBUTTONUP:
			pEvent->Cancel();
			return true;
		case EVENT_MOUSEMOVE:
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		case EVENT_TEMPLATELOAD:
			pHead = new CUnitHead( sEvent.pLoader->GetControl( "face" ), pMission->GetRenderGame(), 1.8f );
			break;
		case EVENT_TEMPLATELOADCOMPLETE:
			pText = GetUIWindow<CText>( this, "text" );
			break;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SetStyle( STYLE_VISIBLE, IsValid( pEvent ) );
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemText: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CItemText)
private:
	ZDATA_(TBaseClass)
	CPtr<CMissionUI> pMissionUI;
	CPtr<NGame::IMission> pMission;
	////
	NWorld::SItem sItem;
	CObj<CTextDraw> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMissionUI); f.Add(3,&pMission); f.Add(4,&sItem); f.Add(5,&pText); return 0; }

public:
	CItemText() {}
	CItemText( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI, const NWorld::SItem &sItem );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	const NWorld::SItem& GetItem() const { return sItem; }
	const SPoint& GetRealSize( NGScene::I2DGameView *pView ) { return pText->GetSize( pView ); }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemText::CItemText( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI, const NWorld::SItem &_sItem ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission ), pMissionUI( _pMissionUI ), sItem( _sItem )
{
	wstring wsText( L"<font face=Courier size=16pt><color=white>[UNKNOWN]" );
	if ( sItem.pItem->GetDBItem()->pName )
		wsText = L"<font face=Courier size=16pt><color=white>" + sItem.pItem->GetDBItem()->pName->szStr;

	pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );
	SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CItemText::GetTarget()
{
	return CDynamicCast<CObjectBase>( sItem.pWorldItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_MOUSEENTER:
		{
			SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0xDF, 0xFF ) );
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemText::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SetSize( pText->GetSize( pView ) );

	TBaseClass::Draw( sTime, pView );

	pText->Draw( this, sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEnemyIcon: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CEnemyIcon)
private:
	ZDATA_(TBaseClass)
	CPtr<CMissionUI> pMissionUI;
	CPtr<NGame::IMission> pMission;
	////
	bool bOwner;
	float fAngle;
	CPtr<CImage> pImage;
	CPtr<NWorld::CUnit> pEnemy;
	CDBPtr<NDb::CUITexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMissionUI); f.Add(3,&pMission); f.Add(4,&bOwner); f.Add(5,&fAngle); f.Add(6,&pImage); f.Add(7,&pEnemy); f.Add(8,&pTexture); return 0; }

public:
	CEnemyIcon() {}
	CEnemyIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetPosition( const SPoint &_sPosition );

	void Set( NWorld::CUnit *pEnemy, bool bOwner, float _fAngle );
	NWorld::CUnit* GetUnit() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEnemyIcon::CEnemyIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission ), pMissionUI( _pMissionUI )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEnemyIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CEnemyIcon::GetTarget()
{
	return pEnemy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::Set( NWorld::CUnit *_pEnemy, bool _bOwner, float _fAngle )
{
	int pNormalIcons[8] = { 446, 447, 448, 449, 450, 451, 452, 453 };
	int pDisabledIcons[8] = { 454, 455, 456, 457, 458, 459, 460, 461 };

	fAngle = _fAngle;
	bOwner = _bOwner;
	pEnemy = _pEnemy;

	int nID = min( max( Float2Int( fAngle / 45 ), 0 ), 7 );

	if ( bOwner )
	{
		if ( fAngle == -1 )
			pTexture = NDb::GetUITexture( 464 );
		else
			pTexture = NDb::GetUITexture( pNormalIcons[nID] );
	}
	else
	{
		if ( fAngle == -1 )
			pTexture = NDb::GetUITexture( 463 );
		else
			pTexture = NDb::GetUITexture( pDisabledIcons[nID] );
	}

	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEnemyIcon::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
		pMission->FocusCameraOnUnit( pEnemy );
		return true;
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	const SPoint &sParentSize = pMissionUI->GetClientWindow()->GetSize();
	const SPoint &sParentPosition = pMissionUI->GetClientWindow()->GetPosition();
	SRect sViewRect( sParentPosition.x, sParentPosition.y, sParentPosition.x + sParentSize.x, sParentPosition.y + sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
//////////////////////////////////////////////////////////////////////////////////////
class CHitTracker: public CText
{
	OBJECT_BASIC_METHODS(CHitTracker);
private:
	ZDATA_(CText)
	CPtr<CWindow> pClientWindow;
	CPtr<NGame::IMission> pMission;
	////
	int nHitValue;
	bool bComplete;
	CVec3 vBegPoint;
	STime sBegTime;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CText*)this); f.Add(2,&pClientWindow); f.Add(3,&pMission); f.Add(4,&nHitValue); f.Add(5,&bComplete); f.Add(6,&vBegPoint); f.Add(7,&sBegTime); return 0; }

public:
	CHitTracker() {}
	CHitTracker( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::CHitLocator *pLocator, const STime &sTime );

	bool IsComplete() const;

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHitTracker::CHitTracker( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::CHitLocator *pLocator, const STime &sTime ):
	CText( sInfo ), pMission( _pMission ), sBegTime( sTime ), nHitValue( pLocator->nHitValue ), vBegPoint( pLocator->vPosition ), bComplete( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHitTracker::IsComplete() const
{
	return bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHitTracker::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( bComplete )
		return;

	bComplete = true;

	CVec2 vScreenPoint;
	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();
	if ( !TestRayInFrustrum( vBegPoint, &sTS, vScreenRect, &vScreenPoint ) )
		return;

	vScreenPoint.x = vScreenPoint.x * 1024 / vScreenRect.x;
	vScreenPoint.y = vScreenPoint.y * 768 / vScreenRect.y;

	SPoint sPosition;
	GetParent()->ScreenToClient( SPoint( vScreenPoint.x, vScreenPoint.y ), &sPosition );

	float fWeight = float( sTime - sBegTime ) / N_HITPTRACKER_TTL;
	if ( fWeight > 1 )
		return;

	sPosition.x += 40 * fWeight;
	sPosition.y += -40 * fWeight;

	WCHAR wsText[256];
	int nAlpha = ( 1 - fWeight ) * 0xFF;
	swprintf( wsText, L"<color=%.2xff0000>%d", nAlpha, nHitValue );

	SetPosition( sPosition );
	SetText( wsText );

	bComplete = false;

	CText::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionUI::CMissionUI():
	bindCancel( "cancel" ),	bindShowItems( "showitems" ), 
	bindPerks( "perks" ), bindStore( "store" ), bindInventory( "inventory" ), bindCharacter( "character" ),
	bindPoseSubMenu( "submenu_poseselect" ), bindWeaponModeSubMenu( "submenu_weaponmode" ), bindGrenadeModeSubMenu( "submenu_grenademode" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionUI::CMissionUI( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CDesktopWindow( sInfo ), pMission( _pMission ), sCameraScrollUpdate( 0 ),
	bindCancel( "cancel" ),	bindShowItems( "showitems" ), 
	bindPerks( "perks" ), bindStore( "store" ), bindInventory( "inventory" ), bindCharacter( "character" ), 
	bindPoseSubMenu( "submenu_poseselect" ), bindWeaponModeSubMenu( "submenu_weaponmode" ), bindGrenadeModeSubMenu( "submenu_grenademode" )
	
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CUICmdExec* CMissionUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( !pMission->IsReady() )
		return false;

	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		NGame::EActionIconsSet eIconsSet = pMission->GetActionIconsSet();
		if ( ( eIconsSet == NGame::AIS_POSES ) || ( eIconsSet == NGame::AIS_WEAPONMODES ) || ( eIconsSet == NGame::AIS_GRENADEMODES ) )
		{
			pMission->SetActionIconsSet( NGame::AIS_MAIN );
			return true;
		}
		else if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER ) != 0 )
		{
			pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER, false );
			return true;
		}
	}

	if ( bindStore.ProcessEvent( sEvent ) )
	{
		pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_INVENTORY, false );
		return true;
	}
	if ( bindPerks.ProcessEvent( sEvent ) )
	{
		pMission->SetPanelState( NGame::PANEL_PERKS, pMission->GetPanelState( NGame::PANEL_PERKS ) == 0 );
		return true;
	}
	else if ( bindInventory.ProcessEvent( sEvent ) )
	{
		// retail CMissionUI::ProcessEvent (iMissionUI.c:542): capture prior inventory-panel state, toggle, then
		// fire the matching engine->script hook so the campaign lua can react to the inventory open/close.
		bool bWasOpen = ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 );
		pMission->SetPanelState( NGame::PANEL_INVENTORY, !bWasOpen );
		pMission->Command( new NWorld::CCmdCallScriptFunction( bWasOpen ? "OnCloseInventory" : "OnOpenInventory", "" ) );
		return true;
	}
	else if ( bindCharacter.ProcessEvent( sEvent ) )
	{
		pMission->SetPanelState( NGame::PANEL_CHARACTER, pMission->GetPanelState( NGame::PANEL_CHARACTER ) == 0 );
		return true;
	}

	if ( bindPoseSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_POSES );
	else if ( bindWeaponModeSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_WEAPONMODES );
	else if ( bindGrenadeModeSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_GRENADEMODES );

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( pMission->GetState()->GetCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pLogPanel = new CLogPanel( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "logpanel", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TOPMOST | STYLE_TRANSPARENT ), STREAM_GAME );

			pAck = new CAckIcon( sEvent.pLoader->GetControl( "ack" ), pMission );

			pTopBar = new CTopBar( sEvent.pLoader->GetControl( "topbar" ), pMission );
			pUnitPanel = new CUnitPanel( sEvent.pLoader->GetControl( "unitpanel" ), pMission );
			pPerksPanel = new CPerksPanel( sEvent.pLoader->GetControl( "perkspanel" ), pMission );
			pStorePanel = new CStorePanel( sEvent.pLoader->GetControl( "storepanel" ), pMission );
			pInventoryPanel = new CInventoryPanel( sEvent.pLoader->GetControl( "inventorypanel" ), pMission );
			pCharacterPanel = new CCharacterPanel( sEvent.pLoader->GetControl( "characterpanel" ), pMission );

			pInventory = new CHoverButton( sEvent.pLoader->GetControl( "inventory" ) );
			pInventory->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 395 ) );
			pInventory->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 395 ) );
			pInventory->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 647 ) );
			pInventory->SetCursorInfo( GetInterface()->GetDefaultCursorInfo() );

			pCharacter = new CHoverButton( sEvent.pLoader->GetControl( "character" ) );
			pCharacter->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 428 ) );
			pCharacter->SetCursorInfo( GetInterface()->GetDefaultCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pPause = GetUIWindow<CText>( this, "pause" );   // "pause" is a UI_TEXT control -> CText, not CImage
			break;
		}
	}

	bool bRet = CDesktopWindow::ProcessMessage( sEvent );

	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			CPtr<NGame::IState> pState = pMission->GetState();
			if ( ( pState->GetType() == NGame::IState::FORCED ) || ( pState->GetType() == NGame::IState::TEMPORARY ) )
			{
				SCursorInfo sStateCursor = pState->GetCursorInfo();
				if ( sStateCursor.pTexture != GetInterface()->GetCursorInfo().pTexture )
				{
					sStateCursor.wsText = L"";
					GetInterface()->SetCursorInfo( sStateCursor );
				}
			}
			break;
		}
	}

	if ( bRet )
		return true;

	return pMission->GetState()->ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( pMission->CountSelected() != 1 )
		pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER, false );
	if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_INVENTORY ) == NGame::PANEL_STORE )
		pMission->SetPanelState( NGame::PANEL_STORE, false );
	if ( pMission->GetPanelState( NGame::PANEL_PERKS | NGame::PANEL_CHARACTER ) == NGame::PANEL_PERKS )
		pMission->SetPanelState( NGame::PANEL_PERKS, false );

	const SPoint &sSize = GetClientWindow()->GetSize();
	const SPoint &sPosition = GetClientWindow()->GetPosition();
	SRect sClientRect( 0, 0, 1024, 768 );
	if ( !pMission->IsInterfaceHidden() )
	{
		sClientRect = SRect( 0, 32, 1024, 596 );
		if ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 )
			sClientRect.x2 = 512;
		if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER ) != 0 )
			sClientRect.x1 = 512;
	}

	GetClientWindow()->SetSize( SPoint( sClientRect.Width(), sClientRect.Height() ) );
	GetClientWindow()->SetPosition( SPoint( sClientRect.x1, sClientRect.y1 ) );

	pPause->SetStyle( STYLE_VISIBLE, pMission->IsGamePaused() );

	pInventory->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) == 0 ) );
	pInventory->SetStyle( STYLE_ENABLED, pMission->IsReady() );
	pCharacter->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER ) == 0 ) );
	pCharacter->SetStyle( STYLE_ENABLED, pMission->IsReady() );

	pPerksPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_PERKS ) != 0 ) );
	pStorePanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS ) == NGame::PANEL_STORE ) );
	pInventoryPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 ) );
	pCharacterPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_CHARACTER | NGame::PANEL_STORE | NGame::PANEL_PERKS ) == NGame::PANEL_CHARACTER ) );

	SRect sLogRect( sClientRect );
	sLogRect.x1 += N_LOGPANEL_PAD;
	sLogRect.y1 += N_LOGPANEL_PAD;
	sLogRect.x2 -= N_LOGPANEL_PAD;
	sLogRect.y2 -= N_LOGPANEL_PAD;
	sLogRect.y2 = sLogRect.y1 + sLogRect.Height() / 2;
	pLogPanel->SetSize( SPoint( sLogRect.Width(), sLogRect.Height() ) );
	pLogPanel->SetPosition( SPoint( sLogRect.x1, sLogRect.y1 ) );

	CDesktopWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	UpdateHits( sTime );
	UpdateItems( pView );
	UpdateEnemies();
	UpdateCameraScroll( sTime );

	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInvItemSort
{
	bool operator()( CItemText *p1, CItemText *p2 ) const 
	{
		const SPoint &sSize1 = p1->GetItem().pItem->GetSize();
		const SPoint &sSize2 = p2->GetItem().pItem->GetSize();

		int nW1 = Max( sSize1.x, sSize1.y ) + sSize1.x * sSize1.y;
		int nW2 = Max( sSize2.x, sSize2.y ) + sSize2.x * sSize2.y;

		return nW1 > nW2; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanPlace( const CArray2D<bool> &sMap, const SPoint &sPos, const SPoint &sSize )
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + sSize.x > sMap.GetXSize() ) || ( sPos.y + sSize.y > sMap.GetYSize() ) )
		return false;

	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
	{
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
		{
			if ( sMap[sPos.y + nTempY][sPos.x + nTempX] )
				return false;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Place( CArray2D<bool> *pMap, const SPoint &sPos, const SPoint &sSize )
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + sSize.x > pMap->GetXSize() ) || ( sPos.y + sSize.y > pMap->GetYSize() ) )
	{
		ASSERT( 0 );
		return;
	}

	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			(*pMap)[sPos.y + nTempY][sPos.x + nTempX] = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateItems( NGScene::I2DGameView *pView )
{
	if ( bindShowItems.IsActive() )
	{
		list<CObj<CItemText> > newItemTextsList;

		unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CItemText>, SPtrHash> itemsMap;
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pItemText = *iTemp;
			itemsMap[ pItemText->GetItem().pItem ] = pItemText;
		}

		vector<CPtr<NGame::IUnitTracker> > unitsSet;
		pMission->GetSelectedUnits( &unitsSet );
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		{
			vector<NWorld::SItem> tempItems;
			pMission->GetWorld()->FindCloseGroundItems( unitsSet[nTemp]->GetUnit(), &tempItems );

			for ( int nTemp = 0; nTemp < tempItems.size(); nTemp++ )
			{
				const NWorld::SItem &sItem = tempItems[nTemp];

				if ( !IsValid( sItem.pItem ) )
					continue;
				if ( !IsValid( sItem.pWorldItem ) )
					continue;

				unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CItemText>, SPtrHash>::iterator iFindRes = itemsMap.find( sItem.pItem );
				if ( iFindRes == itemsMap.end() )
					newItemTextsList.push_back( new CItemText( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this, sItem ) );
				else
					newItemTextsList.push_back( iFindRes->second.GetPtr() );
			}
		}

		itemTextsList = newItemTextsList;

		const int 
			N_X_STEP = 4,
			N_Y_STEP = 16,
			N_X_SIZE = 1024 / N_X_STEP,
			N_Y_SIZE = 768 / N_Y_STEP;

		CArray2D<bool> sMap( N_X_SIZE, N_Y_SIZE );
		sMap.FillEvery( false );

		itemTextsList.sort( SInvItemSort() );

		CVec2 vScreenRect = pView->GetViewportSize();
		CTransformStack sTS = pMission->GetCameraTransform();
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pItemText = *iTemp;

			CVec2 vRes;
			if ( !TestRayInFrustrum( pItemText->GetItem().pWorldItem->GetPos(), &sTS, vScreenRect, &vRes ) )
			{
				pItemText->SetStyle( STYLE_VISIBLE, false );
				continue;
			}

			SPoint sRealSize = pItemText->GetRealSize( pView );
			sRealSize.x += N_X_STEP;

			SPoint sItemPos( vRes.x * N_X_SIZE / vScreenRect.x, vRes.y * N_Y_SIZE / vScreenRect.y );
			SPoint sItemSize( float( Max( sRealSize.x, N_X_STEP ) ) / N_X_STEP, float( Max( sRealSize.y, N_Y_STEP ) ) / N_Y_STEP );

			bool bComplete = false;
			for ( int nTempY = 0; nTempY < N_Y_SIZE / 4; nTempY++ )
			{
				for ( int nTempX = 0; nTempX < N_X_SIZE / 4; nTempX++ )
				{
					SPoint sTestPos;

					sTestPos = SPoint( sItemPos.x + nTempX, sItemPos.y + nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x - nTempX, sItemPos.y + nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x + nTempX, sItemPos.y - nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x - nTempX, sItemPos.y - nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}
				}

				if ( bComplete )
					break;
			}

			pItemText->SetStyle( STYLE_VISIBLE, bComplete );
			pItemText->SetPosition( SPoint( sItemPos.x * N_X_STEP, sItemPos.y * N_Y_STEP ) );
		}
	}
	else
		itemTextsList.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateHits( const STime &sTime )
{
	CPtr<NWorld::CHitLocator> pTempLocator;
	while( pTempLocator = pMission->GetWorld()->GetHitEvent() )
		hitsList.push_back( new CHitTracker( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 120, 20 ), "hit", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST | STYLE_VISIBLE ), pMission, pTempLocator, sTime ) );

	for ( list<CObj<CHitTracker> >::iterator iTemp = hitsList.begin(); iTemp != hitsList.end(); )
	{
		if ( !(*iTemp)->IsComplete() )
			iTemp++;
		else
			iTemp = hitsList.erase( iTemp );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateEnemies()
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );

	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();

	SRect sViewRect;
	SPoint sViewPosition;
	GetClientWindow()->ClientToScreen( &sViewPosition, &sViewRect );

	if ( sViewRect.Width() == 0 )
	{
		enemyIconsList.clear();
		return;
	}

	unordered_map<CPtr<NWorld::CUnit>,bool,SPtrHash> enemySet;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		list<CPtr<NWorld::CUnit> > visibleUnits;
		unitsSet[nTemp]->GetVisibleEnemiesList( &visibleUnits );

		for ( list<CPtr<NWorld::CUnit> >::const_iterator iEnemy = visibleUnits.begin(); iEnemy != visibleUnits.end(); iEnemy++ )
		{
			bool &bValue = enemySet[*iEnemy];
			if ( unitsSet[nTemp]->IsSelected() )
				bValue = true;
		}
	}

	list<CObj<CEnemyIcon> > newEnemyIconsList;
	list<CObj<CEnemyIcon> >::iterator iOldIcons = enemyIconsList.begin();
	for (unordered_map<CPtr<NWorld::CUnit>,bool,SPtrHash>::iterator iTemp = enemySet.begin(); iTemp != enemySet.end(); iTemp++ )
	{
		bool bVisible = iTemp->second;
		CPtr<NWorld::CUnit> pEnemy = iTemp->first;

		if ( pEnemy->IsDead() || pEnemy->IsUnconscious() )
			continue;

		CEnemyIcon *pIcon;
		if ( iOldIcons != enemyIconsList.end() )
		{
			pIcon = (*iOldIcons);
			iOldIcons++;
		}
		else
		{
			pIcon = new CEnemyIcon( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this );
		}

		CVec2 vScreenPos;
		CVec3 vEyePosition( pEnemy->GetPosition().GetEyePosition() );
		vEyePosition += CVec3( 0, 0, 0.6f );
		TestRayInFrustrum( vEyePosition, &sTS, vScreenRect, &vScreenPos );
		vScreenPos.x = vScreenPos.x * 1024 / vScreenRect.x;
		vScreenPos.y = vScreenPos.y * 768 / vScreenRect.y;

		bool bRet = false;
		if ( ( sViewRect.x1 < vScreenPos.x ) && ( sViewRect.x2 > vScreenPos.x ) && ( sViewRect.y1 < vScreenPos.y ) && ( sViewRect.y2 > vScreenPos.y ) )
			bRet = true;

		float fAngle = -1;
		if ( !bRet )
		{
			fAngle = ToDegree( atan2( vScreenPos.x - ( sViewRect.x2 - sViewRect.x1 ) / 2, -( vScreenPos.y - ( sViewRect.y2 - sViewRect.y1 ) / 2 ) ) );
			if ( fAngle < 0 )
				fAngle += 360;
		}

		const NUI::SPoint &sSize = pIcon->GetSize();
		vScreenPos.x = max( min( vScreenPos.x, sViewRect.x2 ), sViewRect.x1 );
		vScreenPos.y = max( min( vScreenPos.y, sViewRect.y2 ), sViewRect.y1 );

		pIcon->Set( pEnemy, bVisible, fAngle );
		pIcon->SetPosition( SPoint( vScreenPos.x, vScreenPos.y ) );
		newEnemyIconsList.push_back( pIcon );
	}

	enemyIconsList = newEnemyIconsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateCameraScroll( const STime &sTime )
{
	STime sDelta = sTime - sCameraScrollUpdate;
	sDelta = Min( sDelta, (STime)100 );
	sCameraScrollUpdate = sTime;

	CPtr<ICamera> pCamera = pMission->GetCamera();
	CPtr<NUI::ICursor> pCursor = pMission->GetCursor();
	CPtr<NGScene::IGameView> pView = pMission->GetScene();

	CVec2 vScreenRect = pView->GetScreenRect();
	const CVec2 &vPos = pCursor->GetPos();

	ICamera::SCameraPos sCameraPos;
	pCamera->GetPlacement( &sCameraPos );

	CVec3 vStrafeDir( pCamera->GetStrafeDir() ), vForwardDir( pCamera->GetForwardDir() );
	vStrafeDir.z = 0;
	vForwardDir.z = 0;
	Normalize( &vStrafeDir );
	Normalize( &vForwardDir );

	int nMask = 0;
	float fStep = (float)( N_SCROLL_STEP * sDelta ) / 1000.0f;
	if ( vPos.x < N_SCROLL_GUARDBAND )
	{
		nMask |= 1;
		sCameraPos.ptAnchor -= vStrafeDir * fStep;
	}
	if ( vPos.x > vScreenRect.x - N_SCROLL_GUARDBAND )
	{
		nMask |= 2;
		sCameraPos.ptAnchor += vStrafeDir * fStep;
	}
	if ( vPos.y < N_SCROLL_GUARDBAND )
	{
		nMask |= 4;
		sCameraPos.ptAnchor += vForwardDir * fStep;
	}
	if ( vPos.y > vScreenRect.y - N_SCROLL_GUARDBAND )
	{
		nMask |= 8;
		sCameraPos.ptAnchor -= vForwardDir * fStep;
	}

	pCamera->SetPlacement( sCameraPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckEvent* CMissionUI::PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent )
{
	CAckEvent *pAckEvent = new CAckEvent;
	pAckEvent->Set( sTime, pEvent );

	pAck->Set( pAckEvent );

	return pAckEvent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241940, CAckIcon );
REGISTER_SAVELOAD_CLASS( 0xB0241942, CMissionUI );
REGISTER_SAVELOAD_CLASS( 0xB0241947, CItemText );
REGISTER_SAVELOAD_CLASS( 0xB0241948, CEnemyIcon );
REGISTER_SAVELOAD_CLASS( 0xB0241949, CHitTracker );
