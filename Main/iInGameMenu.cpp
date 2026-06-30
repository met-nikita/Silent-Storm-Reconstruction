#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iMainMenu.h"
#include "iSaveLoad.h"
#include "iInGameMenu.h"
#include "iOptionsMenu.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowClueUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInGameMenuUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CInGameMenuUI);
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	////
	CPtr<CImage> pBackground;
	CObj<CHoverButton> pOptions;
	CObj<CHoverButton> pLoadGame;
	CObj<CHoverButton> pSaveGame;
	CObj<CHoverButton> pReturnToGame;
	CObj<CHoverButton> pExitToWindows;
	CObj<CHoverButton> pExitToMainMenu;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bAllowSave); f.Add(3,&bAllowRestart); f.Add(4,&pPlayer); f.Add(5,&pBackground); f.Add(6,&pOptions); f.Add(7,&pLoadGame); f.Add(8,&pSaveGame); f.Add(9,&pReturnToGame); f.Add(10,&pExitToWindows); f.Add(11,&pExitToMainMenu); f.Add(12,&pRestartMission); return 0; }
	bool bAllowSave = false;
	bool bAllowRestart = false;
	CObj<CHoverButton> pRestartMission;

public:
	CInGameMenuUI() {}
	CInGameMenuUI( const SWindowInfo &sInfo, NRPG::CGlobalPlayer *pPlayer );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInGameMenuUI::CInGameMenuUI( const SWindowInfo &sInfo, NRPG::CGlobalPlayer *_pPlayer ):
	CWindow( sInfo ), pPlayer( _pPlayer )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInGameMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pOptions = new CHoverButton( sEvent.pLoader->GetControl( "options" ) );
			pOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11146 ) );
			pOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11146 ) );
			pOptions->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11146 ) );

			pLoadGame = new CHoverButton( sEvent.pLoader->GetControl( "loadgame" ) );
			pLoadGame->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11147 ) );
			pLoadGame->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11147 ) );
			pLoadGame->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11147 ) );

			pSaveGame = new CHoverButton( sEvent.pLoader->GetControl( "savegame" ) );
			pSaveGame->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11148 ) );
			pSaveGame->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11148 ) );
			pSaveGame->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11148 ) );

			pReturnToGame = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pReturnToGame->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11151 ) );
			pReturnToGame->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11151 ) );
			pReturnToGame->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11151 ) );

			pExitToWindows = new CHoverButton( sEvent.pLoader->GetControl( "exitgame" ) );
			pExitToWindows->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11150 ) );
			pExitToWindows->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11150 ) );
			pExitToWindows->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11150 ) );

			pExitToMainMenu = new CHoverButton( sEvent.pLoader->GetControl( "exittomainmenu" ) );
			pExitToMainMenu->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11144 ) + GetDBString( 11149 ) );
			pExitToMainMenu->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11143 ) + GetDBString( 11149 ) );
			pExitToMainMenu->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11145 ) + GetDBString( 11149 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pBackground = GetUIWindow<CImage>( this, "background" );
			if ( IsValid( pPlayer->pSide ) )
				pBackground->SetImage( pPlayer->pSide->pESCMenuBackground );

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInGameMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInGameMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CInGameMenuInterface);
private:
	NInput::CBind bindClose;
	NInput::CBind bindOptions, bindLoadGame, bindSaveGame, bindExitToMainMenu;

	ZDATA
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CScreenShot> pScreenShot;
	CObj<NUI::CInGameMenuUI> pMenuUI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bAllowSave); f.Add(3,&bAllowRestart); f.Add(4,&pMission); f.Add(5,&pPlayer); f.Add(6,&pCursor); f.Add(7,&pInterface); f.Add(8,&pScreenShot); f.Add(9,&pMenuUI); return 0; }
	bool bAllowSave = false;
	bool bAllowRestart = false;
	CPtr<NGame::IMission> pMission;

public:
	CInGameMenuInterface();

	void Initialize( NRPG::CGlobalPlayer *pPlayer );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInGameMenuInterface::CInGameMenuInterface():
	bindClose( "cancel" ), bindOptions( "options" ), bindLoadGame( "loadgame" ), bindSaveGame( "savegame" ), bindExitToMainMenu( "exittomainmenu" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInGameMenuInterface::Initialize( NRPG::CGlobalPlayer *_pPlayer )
{
	pPlayer = _pPlayer;

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	pScreenShot->Generate();

	pMenuUI = new NUI::CInGameMenuUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "ingamemenu", NUI::STYLE_ENABLED ), pPlayer );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 158 ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInGameMenuInterface::Step()
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
void CInGameMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInGameMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
	else if ( bindOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICOptions( OS_PROFILE, pScreenShot->GetTexture() ) );
		return true;
	}
	else if ( bindSaveGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( SAVE, pScreenShot->GetTexture() ) );
		return true;
	}
	else if ( bindLoadGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( LOAD, pScreenShot->GetTexture() ) );
		return true;
	}
	else if ( bindExitToMainMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICMainMenu() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInGameMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICInGameMenu::CICInGameMenu( NRPG::CGlobalPlayer *_pGlobalPlayer ):
	pGlobalPlayer( _pGlobalPlayer )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICInGameMenu::Exec()
{
	CInGameMenuInterface *pRes = new CInGameMenuInterface();
	pRes->Initialize( pGlobalPlayer );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0905131, CInGameMenuUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB0905130, CInGameMenuInterface );
