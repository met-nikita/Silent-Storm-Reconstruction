#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "GView.h"     // NGScene::IGameView complete -- the CMissionBase base op& serializes CObj<IGameView> (cast helpers instantiate here)
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iMission.h"
#include "iMissionExec.h"		// complete CUICmdExec/CUICmdLocatorExec for the CMission member smart-ptrs
#include "iMissionInternal.h"	// NGame::CMission -- retail base of CMultiPlayerInterface (W4.2)
#include "iMultiPlayerMenu.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Module iMultiPlayerMenu -- the multiplayer / hot-seat pre-game setup screen. Reconstructed from the
// answer-key decode (decomp src/s2_multiplayermenu.h, s2_cmultiplayerinterface.h, s2_cplayerline.h,
// s2_cicmultiplayer.h, s2_imultiplayermenuinit.h, s2_nui_imultiplayermenu.h; reg 0xb3721110/11/12),
// translated from the answer-key free-fn-over-mirror+hook style to the real engine types,
// following the menu idiom of iCharGen.cpp / iInGameMenu.cpp / iAdvFaceGen.cpp.
//
// FAITHFUL ADAPTATIONS (answer-key release form -> dev idiom):
//  - W4.2 serialization-convergence UPDATE: CMultiPlayerInterface now derives the concrete
//    NGame::CMission exactly like the release (PDB size 1468; operator& @0x21d720 = ObjSer<CMission>
//    base + pMenuUI), and its cursor/interface/global-game ride the CMissionBase member slots. The
//    lobby data model is unchanged: an NRPG::CGlobalGame with four NRPG::CGlobalPlayer slots
//    (Initialize); CMultiPlayerUI/CPlayerLine still read the players from the global game (the only
//    player objects that exist outside a running mission) instead of IPlayerTracker virtuals --
//    behaviour-equivalent at the menu level (the earlier lightweight-IInterfaceBase adaptation note
//    is superseded by this reparenting).
//  - Release ReturnToMenu / GetPanelState are CMission desktop-stack overrides; they have no place in
//    the IInterfaceBase pattern (the menu UI is always the shown child of pInterface), so they are
//    dropped. "cancel" exits the modal (CICExitModal), exactly as the sibling menus do.
//  - The sides combo uses the header-available NUI::CComboBox (the release CComplexComboBox is a
//    file-local class in iOptionsMenu.cpp and not includable).
//  - The "weapons" action posts a CCmdUpdateStore to the first player; that command class is ABSENT
//    from this build (grep UpdateStore in a5dll/Main = 0 hits, as the prior decode noted). The bind and
//    its branch are present but consume-only (parity-neutral) until a store-update path is wired.
//  - GetUIContainer id 449 (release 0x1c1) is the decoded skin id; CLoader::GetControl is null-safe
//    (logs a UI-ERROR + returns an empty placeholder for a missing control), so the screen is crash-safe
//    even where this build's content DB lacks the container.
//
// CARRIED-OVER RELEASE BEHAVIOUR: tech level steps by +/-1 clamped to [0,20]; the "next" button is
// enabled only when the map field is non-empty AND there is at least one populated player slot; "next"
// begins the mission template parsed from the map field via CICBeginMission over the global game.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetUIContainer id for the multiplayer setup screen (release 0x1c1).
const int N_MULTIPLAYER_CONTAINER = 449;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayerLine -- one row of the multiplayer lobby player list: a name edit, team-manage / weapons
// buttons, an AI checkbox and a side combo, bound to one NRPG::CGlobalPlayer slot of the lobby game.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlayerLine: public CWindow
{
	OBJECT_BASIC_METHODS(CPlayerLine);
private:
	ZDATA_(CWindow)
	wstring wsDefName;
	bool bAI = false;
	int nSide = 0;
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	////
	CPtr<CEdit> pName;
	CObj<CHoverButton> pTeamMng;
	CObj<CHoverButton> pWeapons;
	CPtr<CCheckButton> pAIPlayer;
	CObj<CComboBox> pSides;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&wsDefName); f.Add(3,&bAI); f.Add(4,&nSide); f.Add(5,&pPlayer); f.Add(6,&pName); f.Add(7,&pTeamMng); f.Add(8,&pWeapons); f.Add(9,&pAIPlayer); f.Add(10,&pSides); return 0; }

public:
	CPlayerLine() {}
	CPlayerLine( const SWindowInfo &sInfo, const wstring &wsDefName, NRPG::CGlobalPlayer *pPlayer );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayerLine::CPlayerLine( const SWindowInfo &sInfo, const wstring &_wsDefName, NRPG::CGlobalPlayer *_pPlayer ):
	CWindow( sInfo ), wsDefName( _wsDefName ), pPlayer( _pPlayer )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerLine::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pTeamMng = new CHoverButton( sEvent.pLoader->GetControl( "teammng" ) );
			pTeamMng->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) );
			pTeamMng->AddTextState( CHoverButton::STATE_HOVER,  GetDBString( 11130 ) );

			pWeapons = new CHoverButton( sEvent.pLoader->GetControl( "weapons" ) );
			pWeapons->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) );
			pWeapons->AddTextState( CHoverButton::STATE_HOVER,  GetDBString( 11130 ) );

			pSides = new CComboBox( sEvent.pLoader->GetControl( "side" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pName = GetUIWindow<CEdit>( this, "name" );
			if ( IsValid( pName ) )
				pName->SetText( wsDefName );

			pAIPlayer = GetUIWindow<CCheckButton>( this, "aiplayer" );

			if ( IsValid( pSides ) )
			{
				pSides->AddItem( 0, CComboBox::SInfo( L"-" ) );
				pSides->AddItem( 1, CComboBox::SInfo( L"Allies" ) );
				pSides->AddItem( 2, CComboBox::SInfo( L"Axis" ) );
				pSides->SetSelectedItem( 0 );
			}
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "aiplayer" )
			{
				// release writes the AI flag into the player's active group (+0x54); the lobby has no
				// live group here, so latch it on the row instead.
				bAI = IsValid( pAIPlayer ) && pAIPlayer->IsChecked();
				return true;
			}
			else if ( sEvent.szID == "side" )
			{
				// 0 = clear, 1/2 = a chosen side. Applying it to the live unit group needs the running
				// multiplayer mission (absent); latch the selection (parity-neutral).
				if ( IsValid( pSides ) )
					nSide = pSides->GetSelectedItem();
				return true;
			}
			else if ( sEvent.szID == "teammng" || sEvent.szID == "weapons" )
			{
				// team-management / per-player weapons screens drive the live mission's IPlayerTracker
				// slot system (absent in this build) -- consume the event (parity-neutral).
				return true;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerLine::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// release: team-manage enabled by the active group's state, weapons enabled when the player owns
	// members. Both reduce here to "this slot has mercs".
	bool bHasMembers = IsValid( pPlayer ) && !pPlayer->mercs.empty();
	if ( IsValid( pTeamMng ) ) pTeamMng->SetStyle( STYLE_ENABLED, bHasMembers );
	if ( IsValid( pWeapons ) ) pWeapons->SetStyle( STYLE_ENABLED, bHasMembers );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMultiPlayerUI -- the multiplayer setup desktop: the map/template field, the +/- tech-level stepper,
// the play/cancel buttons and one CPlayerLine per lobby player slot.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMultiPlayerUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CMultiPlayerUI);
private:
	ZDATA_(CDesktopWindow)
	int nTechLevel = 0;
	CPtr<NRPG::CGlobalGame> pGame;
	////
	CPtr<CEdit> pMapName;
	CPtr<CText> pTechLevel;
	CObj<CButtonsLine> pButtonsLine;
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pNext;
	CObj<CComplexButton> pTechLevelUp;
	CObj<CComplexButton> pTechLevelDown;
	vector<CObj<CPlayerLine> > playerLines;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&nTechLevel); f.Add(3,&pGame); f.Add(4,&pMapName); f.Add(5,&pTechLevel); f.Add(6,&pButtonsLine); f.Add(7,&pBack); f.Add(8,&pNext); f.Add(9,&pTechLevelUp); f.Add(10,&pTechLevelDown); f.Add(11,&playerLines); return 0; }

protected:
	void ChangeTechLevel( int nDelta );

public:
	CMultiPlayerUI() {}
	CMultiPlayerUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *pGame );

	// the map field doubles as the base-10 mission-template index (release CMultiPlayerUI::GetTemplate).
	int GetTemplate() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMultiPlayerUI::CMultiPlayerUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame ):
	CDesktopWindow( sInfo ), nTechLevel( 0 ), pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CMultiPlayerUI::ChangeTechLevel @0x21a600: step nTechLevel by the signed delta, clamp to
// [0,20] (the binary caps the high bound first, then the low), then refresh the markup label.
void CMultiPlayerUI::ChangeTechLevel( int nDelta )
{
	nTechLevel += nDelta;
	if ( nTechLevel > 20 ) nTechLevel = 20;
	if ( nTechLevel < 0 )  nTechLevel = 0;

	if ( IsValid( pTechLevel ) )
	{
		WCHAR wsText[256];
		swprintf( wsText, L"<font face=CourierBold size=20pt><color=white><center>%d", nTechLevel );
		pTechLevel->SetText( wsText );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultiPlayerUI::GetTemplate() const
{
	if ( !IsValid( pMapName ) )
		return 0;
	return (int)wcstol( pMapName->GetText().c_str(), 0, 10 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultiPlayerUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			// play/cancel row (the main-menu / char-gen CButtonsLine pattern; the szID is the notify id
			// the interface binds match on). Re-uses the verified gold-Impact button markup strings.
			pButtonsLine = new CButtonsLine( sEvent.pLoader->GetControl( "line" ) );
			pBack = pButtonsLine->AddHoverButton( "cancel", -1,
				GetDBString( 11129 ) + GetDBString( 10895 ),
				GetDBString( 11130 ) + GetDBString( 10895 ),
				GetDBString( 17339 ) + GetDBString( 10895 ) );
			pNext = pButtonsLine->AddHoverButton( "next", -1,
				GetDBString( 11129 ) + GetDBString( 10896 ),
				GetDBString( 11130 ) + GetDBString( 10896 ),
				GetDBString( 17339 ) + GetDBString( 10896 ) );

			pTechLevelUp   = new CComplexButton( sEvent.pLoader->GetControl( "techlevel_up" ), 0, 0, 0, 0 );
			pTechLevelDown = new CComplexButton( sEvent.pLoader->GetControl( "techlevel_down" ), 0, 0, 0, 0 );
			pTechLevelUp->Set( 0, 0, CComplexButton::NORMAL, "techlevel_up" );
			pTechLevelDown->Set( 0, 0, CComplexButton::NORMAL, "techlevel_down" );

			// one CPlayerLine row per lobby player slot (release enumerates the mission's players).
			playerLines.clear();
			if ( IsValid( pGame ) )
			{
				for ( int i = 0; i < pGame->players.size(); i++ )
				{
					char szID[16] = "player0";
					szID[6] = (char)( '0' + i );           // four slots -> single digit
					WCHAR wsName[64];
					swprintf( wsName, L"Player #%d", i + 1 );
					CObj<CPlayerLine> pLine = new CPlayerLine( sEvent.pLoader->GetControl( szID ), wsName, pGame->players[i] );
					playerLines.push_back( pLine );
				}
			}
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pMapName = GetUIWindow<CEdit>( this, "map" );
			if ( IsValid( pMapName ) )
			{
				pMapName->SetMode( CEdit::NUMERIC );
				pMapName->SetTextFormat( GetDBString( 10899 ) );
			}
			pTechLevel = GetUIWindow<CText>( this, "techlevel" );
			ChangeTechLevel( 0 );                          // clamp + refresh label
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "techlevel_up" )
			{
				ChangeTechLevel( +1 );
				return true;
			}
			else if ( sEvent.szID == "techlevel_down" )
			{
				ChangeTechLevel( -1 );
				return true;
			}
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CMultiPlayerUI::Draw @0x21a8a0: the "next" (start) button is enabled only when the map field
// is non-empty AND the player list is non-empty AND at least one slot is populated.
void CMultiPlayerUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	bool bMapNonEmpty = IsValid( pMapName ) && !pMapName->GetText().empty();

	bool bPlayerListNonEmpty = IsValid( pGame ) && !pGame->players.empty();
	bool bAnyPlayerReady = false;
	if ( IsValid( pGame ) )
	{
		for ( int i = 0; i < pGame->players.size(); i++ )
		{
			if ( IsValid( pGame->players[i] ) && !pGame->players[i]->mercs.empty() )
			{
				bAnyPlayerReady = true;
				break;
			}
		}
	}

	bool bEnable = bMapNonEmpty && bPlayerListNonEmpty && bAnyPlayerReady;
	if ( IsValid( pNext ) )
		pNext->SetStyle( STYLE_ENABLED, bEnable );

	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMultiPlayerInterface -- the multiplayer pre-game setup interface. Serialization-convergence W4.2:
// reparented onto CMission like retail (PDB: CMultiPlayerInterface : NGame::CMission + 5 binds +
// pMenuUI; operator& @0x21d720 = tag 1 ObjSer<NGame::CMission> base + tag 2 pMenuUI). The old dev
// members pCursor(2)/pInterface(3)/pGlobalGame(4) now ride the CMissionBase slots (base tags 15/16/2).
// NOTE (retail-exact ownership): the base pGlobalGame is a WEAK CPtr -- retail has no owning ref to
// the lobby game either (the fresh CreateGlobalGame object lives with refcount 0 until something
// adopts it; the whole lobby chain -- CMultiPlayerUI::pGame, CICBeginMission -- is CPtr in both trees).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMultiPlayerInterface: public CMission
{
	OBJECT_BASIC_METHODS(CMultiPlayerInterface);
private:
	NInput::CBind bindClose, bindPlay, bindWeapons, bindStore, bindInventory;

	ZDATA_(CMission)
	CObj<NUI::CMultiPlayerUI> pMenuUI;
	// retail NGame::CMultiPlayerInterface::operator& @0x21d720
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CMission*)this); f.Add(2,&pMenuUI); return 0; }

public:
	CMultiPlayerInterface();

	void Initialize();

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMultiPlayerInterface::CMultiPlayerInterface():
	bindClose( "cancel" ), bindPlay( "next" ), bindWeapons( "weapons" ), bindStore( "store" ), bindInventory( "inventory" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CMultiPlayerInterface::Initialize @0x21af20: create the lobby global game with exactly four
// players, then build + skin + show the multiplayer setup desktop.
void CMultiPlayerInterface::Initialize()
{
	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pGlobalGame = NRPG::CreateGlobalGame();
	for ( int i = 0; i < 4; i++ )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );

	pMenuUI = new NUI::CMultiPlayerUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "multiPlayerUI", NUI::STYLE_ENABLED ), pGlobalGame );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( NUI::N_MULTIPLAYER_CONTAINER ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerInterface::Step()
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
void CMultiPlayerInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CMultiPlayerInterface::ProcessEvent @0x21aa20: the bind dispatch tree. cancel -> exit modal;
// inventory/store -> return to the lobby (the menu is always shown here, so consume); next -> begin the
// mission; weapons -> store update (CCmdUpdateStore absent in this build -> consume).
bool CMultiPlayerInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}
	else if ( bindInventory.ProcessEvent( sEvent ) || bindStore.ProcessEvent( sEvent ) )
	{
		return true;
	}
	else if ( bindPlay.ProcessEvent( sEvent ) )
	{
		int nTemplateID = IsValid( pMenuUI ) ? pMenuUI->GetTemplate() : 0;
		vector<string> params;
		NMainLoop::Command( new CICBeginMission( nTemplateID, -1, params, pGlobalGame ) );
		return true;
	}
	else if ( bindWeapons.ProcessEvent( sEvent ) )
	{
		// release issues a CCmdUpdateStore to the first player here; that command is absent from this
		// build (see file note) -- consume only.
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultiPlayerInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMultiPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CICMultiPlayer::Exec @0x21b2d0: build + Initialize a CMultiPlayerInterface and push it.
void CICMultiPlayer::Exec()
{
	CMultiPlayerInterface *pRes = new CMultiPlayerInterface();
	pRes->Initialize();
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3721110, CMultiPlayerUI );      // release class id @ s2_multiplayermenu.h
REGISTER_SAVELOAD_CLASS( 0xB3721112, CPlayerLine );         // release class id @ s2_cplayerline.h
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3721111, CMultiPlayerInterface );// release class id @ s2_cmultiplayerinterface.h
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CommandStartMultiplayer @0x21a5c0 + iMultiPlayerMenuInit @0x21c7f0: register the "multiplayer" console
// command that posts CICMultiPlayer -> opens the MP / hot-seat setup screen. The whole screen was reconstructed but
// NOTHING posted the command, so it was unreachable. Mirrors the iGlobalMap.cpp "global" registrar idiom.
static void CommandStartMultiplayer( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	NMainLoop::Command( new CICMultiPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMultiPlayerMenu)
	REGISTER_CMD( "multiplayer", CommandStartMultiplayer )
FINISH_REGISTER
