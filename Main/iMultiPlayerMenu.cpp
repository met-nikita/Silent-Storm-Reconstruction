#include "StdAfx.h"
#include "iMain.h"
#include "GView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "RPGGlobal.h"
#include "RPGUnit.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iMission.h"
#include "iMissionExec.h"
#include "iMissionInternal.h"
#include "iMultiPlayerMenu.h"
#include "iTeamMngMenu.h"
#include "iSaveManager.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "wInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
// Retail 0x21b340: rows operate on live player trackers, not detached global-player copies.
class CPlayerLine: public CWindow
{
	OBJECT_BASIC_METHODS(CPlayerLine);
	ZDATA_(CWindow)
	wstring wsDefName;
	CPtr<NGame::IMission> pMission;
	CPtr<NGame::IPlayerTracker> pPlayer;
	CPtr<CEdit> pName;
	CObj<CHoverButton> pTeamMng;
	CObj<CHoverButton> pWeapons;
	CPtr<CCheckButton> pAIPlayer;
	CObj<CComplexComboBox> pSides;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&wsDefName); f.Add(3,&pMission); f.Add(4,&pPlayer); f.Add(5,&pName); f.Add(6,&pTeamMng); f.Add(7,&pWeapons); f.Add(8,&pAIPlayer); f.Add(9,&pSides); return 0; }
public:
	CPlayerLine() {}
	CPlayerLine( const SWindowInfo &sInfo, const wstring &wsName, NGame::IMission *pMission, NGame::IPlayerTracker *pPlayer );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayerLine::CPlayerLine( const SWindowInfo &sInfo, const wstring &wsName, NGame::IMission *_pMission, NGame::IPlayerTracker *_pPlayer ):
	CWindow( sInfo ), wsDefName( wsName ), pMission( _pMission ), pPlayer( _pPlayer )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerLine::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		pTeamMng = new CHoverButton( sEvent.pLoader->GetControl( "teammng" ) );
		pTeamMng->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 20965 ) + GetDBString( 20959 ) );
		pTeamMng->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 20964 ) + GetDBString( 20959 ) );
		pTeamMng->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 20980 ) + GetDBString( 20959 ) );
		pWeapons = new CHoverButton( sEvent.pLoader->GetControl( "weapons" ) );
		pWeapons->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 20965 ) + GetDBString( 20960 ) );
		pWeapons->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 20964 ) + GetDBString( 20960 ) );
		pWeapons->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 20980 ) + GetDBString( 20960 ) );
		pSides = new CComplexComboBox( sEvent.pLoader->GetControl( "side" ) );
		break;
	case EVENT_TEMPLATELOADCOMPLETE:
		pName = GetUIWindow<CEdit>( this, "name" );
		pName->SetText( wsDefName );
		pName->SetTextFormat( GetDBString( 20957 ) );
		pAIPlayer = GetUIWindow<CCheckButton>( this, "aiplayer" );
		pSides->AddItem( 0, CComboBox::SInfo( GetDBString( 20961 ) ), 171 );
		pSides->AddItem( 1, CComboBox::SInfo( GetDBString( 20962 ) ), 172 );
		pSides->AddItem( 2, CComboBox::SInfo( GetDBString( 20963 ) ), 173 );
		pSides->SetSelectedItem( 0 );
		break;
	case EVENT_NOTIFY:
		if ( sEvent.szID == "teammng" )
		{
			pMission->SetActivePlayer( pPlayer );
			if ( IsValid( pPlayer->GetGlobalPlayer()->pSide ) )
				NMainLoop::Command( new NGame::CICTeamMngMenu( pPlayer->GetGlobalPlayer(), pMission, -1 ) );
			return true;
		}
		if ( sEvent.szID == "side" )
		{
			NRPG::CGlobalPlayer *pGlobalPlayer = pPlayer->GetGlobalPlayer();
			CPtr<NDb::CSide> pOldSide = pGlobalPlayer->pSide;
			int nSide = pSides->GetSelectedItem();
			pGlobalPlayer->pSide = nSide == 1 || nSide == 2 ? NDb::GetDBSide( nSide ) : 0;
			if ( pOldSide != pGlobalPlayer->pSide )
			{
				pGlobalPlayer->mercs.clear();
				pGlobalPlayer->totalMercs.clear();
				vector< CPtr<NGame::IUnitTracker> > units;
				pPlayer->GetUnits( &units );
				for ( int i = 0; i < units.size(); ++i )
					pPlayer->RemoveUnit( units[i] );
				NRPG::AddTeamMngPerses( pGlobalPlayer, true );
			}
			return true;
		}
		if ( sEvent.szID == "aiplayer" )
		{
			pPlayer->GetGlobalPlayer()->bAIPlayer = pAIPlayer->IsChecked();
			return true;
		}
		if ( sEvent.szID == "weapons" )
			pMission->SetActivePlayer( pPlayer ); // propagate to the interface's weapons bind
		break;
	}
	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerLine::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	pTeamMng->SetStyle( STYLE_ENABLED, IsValid( pPlayer->GetGlobalPlayer()->pSide ) );
	vector< CPtr<NGame::IUnitTracker> > units;
	pPlayer->GetUnits( &units );
	pWeapons->SetStyle( STYLE_ENABLED, !units.empty() );
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMultiPlayerUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CMultiPlayerUI);
	ZDATA_(CDesktopWindow)
	CPtr<NGame::IMission> pMission;
	int nTechLevel = 0;
	CPtr<CEdit> pMapName;
	CPtr<CText> pTechLevel;
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pNext;
	CObj<CButtonsLine> pButtonsLine;
	CObj<CComplexButton> pTechLevelUp;
	CObj<CComplexButton> pTechLevelDown;
	vector<CObj<CPlayerLine> > playerLines;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pMission); f.Add(3,&nTechLevel); f.Add(4,&pMapName); f.Add(5,&pTechLevel); f.Add(6,&pBack); f.Add(7,&pNext); f.Add(8,&pButtonsLine); f.Add(9,&pTechLevelUp); f.Add(10,&pTechLevelDown); f.Add(11,&playerLines); return 0; }
	void ChangeTechLevel( int nDelta );
public:
	CMultiPlayerUI() {}
	CMultiPlayerUI( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
		CDesktopWindow( sInfo ), pMission( _pMission ) {}
	int GetTemplate() const { return _wtoi( pMapName->GetText().c_str() ); }
	int GetTechLevel() const { return nTechLevel; }
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerUI::ChangeTechLevel( int nDelta )
{
	nTechLevel = Max( 0, Min( 20, nTechLevel + nDelta ) );
	pTechLevel->SetText( NStr::Format( L"<font face=CourierBold size=20pt><color=white><center>%d", nTechLevel ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultiPlayerUI::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pButtonsLine = new CButtonsLine( sEvent.pLoader->GetControl( "line" ) );
			pBack = pButtonsLine->AddHoverButton( "cancel", 0x4b32,
				GetDBString( 11129 ) + GetDBString( 0x2ba6 ),
				GetDBString( 11130 ) + GetDBString( 0x2ba6 ), L"" );
			pNext = pButtonsLine->AddHoverButton( "next", 0x4b31,
				GetDBString( 11129 ) + GetDBString( 0x41b4 ),
				GetDBString( 11130 ) + GetDBString( 0x41b4 ),
				GetDBString( 17339 ) + GetDBString( 0x41b4 ) );
			pTechLevelUp = new CComplexButton( sEvent.pLoader->GetControl( "techlevel_up" ), 0, 0, 0, 0 );
			pTechLevelDown = new CComplexButton( sEvent.pLoader->GetControl( "techlevel_down" ), 0, 0, 0, 0 );
			pTechLevelUp->Set( NDb::GetUITexture( 399 ), 0, CComplexButton::NORMAL, "" );
			pTechLevelDown->Set( NDb::GetUITexture( 398 ), 0, CComplexButton::NORMAL, "" );
			vector< CPtr<NGame::IPlayerTracker> > players;
			pMission->GetPlayers( &players );
			for ( int i = 0; i < players.size(); ++i )
				playerLines.push_back( new CPlayerLine( sEvent.pLoader->GetControl( NStr::Format( "player%d", i ) ),
					NStr::Format( L"Player #%d", i ), pMission, players[i] ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		pMapName = GetUIWindow<CEdit>( this, "map" );
		pMapName->SetMode( CEdit::NUMERIC );
		pMapName->SetTextFormat( GetDBString( 20958 ) );
		pTechLevel = GetUIWindow<CText>( this, "techlevel" );
		ChangeTechLevel( 0 );
		break;
	case EVENT_NOTIFY:
		if ( sEvent.szID == "techlevel_up" || sEvent.szID == "techlevel_down" )
		{
			ChangeTechLevel( sEvent.szID == "techlevel_up" ? 1 : -1 );
			return true;
		}
		break;
	}
	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	bool bHasUnits = false;
	vector< CPtr<NGame::IPlayerTracker> > players;
	pMission->GetPlayers( &players );
	for ( int i = 0; i < players.size(); ++i )
	{
		vector< CPtr<NGame::IUnitTracker> > units;
		players[i]->GetUnits( &units );
		if ( !units.empty() ) bHasUnits = true;
	}
	pNext->SetStyle( STYLE_ENABLED, !pMapName->GetText().empty() && bHasUnits );
	CDesktopWindow::Draw( sTime, pView );
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class CMultiPlayerInterface: public CMission
{
	OBJECT_BASIC_METHODS(CMultiPlayerInterface);
	NInput::CBind bindClose, bindPlay, bindWeapons, bindStore, bindInventory;
	ZDATA_(CMission)
	CObj<NUI::CMultiPlayerUI> pMenuUI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CMission*)this); f.Add(2,&pMenuUI); return 0; }
	void ReturnToMenu();
public:
	CMultiPlayerInterface(): bindClose( "cancel" ), bindPlay( "next" ), bindWeapons( "weapons" ),
		bindStore( "store" ), bindInventory( "inventory" ) {}
	void Initialize();
	bool IsReady() const { return true; }
	bool IsRealTime() const { return true; }
	bool IsSequence() const { return true; }
	bool IsPlayerTurn() const { return true; }
	bool IsActionExecuted() const { return false; }
	bool IsSetupMode() const { return true; }
	int GetPanelState( int nMask ) const { return nMask & ( PANEL_INVENTORY | PANEL_STORE ); }
	void SetPanelState( int nMask, bool bState ) {}
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerInterface::Initialize()
{
	pGlobalGame = NRPG::CreateGlobalGame();
	vector<int> emptyRoster;
	for ( int i = 0; i < 4; ++i )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer( emptyRoster ) );
	bLoseSignalSended = true;
	CMission::Initialize( -1, -1, 0, vector<string>(), pGlobalGame, 0 );
	pMenuUI = new NUI::CMultiPlayerUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ),
		NUI::SPoint( 1024, 768 ), "multiPlayerUI", NUI::STYLE_ENABLED ), this );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 449 ) );
	PushDesktop( pMenuUI );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
	CDynamicCast<NWorld::CWorld> pSetupWorld( pWorld.GetPtr() );
	pSetupWorld->StartSequence();
	nSequence = 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerInterface::ReturnToMenu()
{
	if ( GetDesktop() != pMenuUI )
	{
		PushDesktop( pMenuUI );
		pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultiPlayerInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "game" );
	if ( bindClose.ProcessEvent( sEvent ) )
	{
		if ( GetDesktop() != pMenuUI ) ReturnToMenu();
		else NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}
	if ( bindInventory.ProcessEvent( sEvent ) || bindStore.ProcessEvent( sEvent ) )
	{
		ReturnToMenu();
		return true;
	}
	if ( bindPlay.ProcessEvent( sEvent ) )
	{
		NMainLoop::GetSaveManager()->ClearSlot( NMainLoop::S_SLOT_ACTIVE );
		for ( int i = 0; i < pGlobalGame->players.size(); )
		{
			if ( pGlobalGame->players[i]->mercs.empty() )
				pGlobalGame->players.erase( pGlobalGame->players.begin() + i );
			else ++i;
		}
		for ( int i = 0; i < pGlobalGame->players.size(); ++i )
			for ( int j = 0; j < pGlobalGame->players[i]->mercs.size(); ++j )
				pGlobalGame->players[i]->mercs[j]->SetXPLevel( pMenuUI->GetTechLevel() );
		if ( !pGlobalGame->players.empty() )
			NMainLoop::Command( new CICBeginMission( pMenuUI->GetTemplate(), -1, vector<string>(), pGlobalGame, 0 ) );
		return true;
	}
	if ( bindWeapons.ProcessEvent( sEvent ) )
	{
		vector< CPtr<IUnitTracker> > units;
		GetUnits( &units );
		if ( !units.empty() )
		{
			pGlobalGame->nHotSeatTechLevel = pMenuUI->GetTechLevel();
			Command( units.front()->GetUnit(), new NWorld::CCmdUpdateStore(), true );
			if ( GetDesktop() == pMenuUI )
			{
				pMenuUI->ShowWindow( NUI::SWTYPE_HIDE );
				PopDesktop( pMenuUI );
			}
		}
		return true;
	}
	return CMission::ProcessEvent( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICMultiPlayer::Exec()
{
	CMultiPlayerInterface *pRes = new CMultiPlayerInterface();
	pRes->Initialize();
	PushInterface( pRes );
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3721110, CMultiPlayerUI );
REGISTER_SAVELOAD_CLASS( 0xB3721112, CPlayerLine );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3721111, CMultiPlayerInterface );
static void CommandStartMultiplayer( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	NMainLoop::Command( new CICMultiPlayer() );
}
START_REGISTER(iMultiPlayerMenu)
	REGISTER_CMD( "multiplayer", CommandStartMultiplayer )
FINISH_REGISTER
