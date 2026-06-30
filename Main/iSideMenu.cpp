#include "StdAfx.h"
#include "Transform.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "wInterface.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iMain.h"
#include "iRenderWorld.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iSideMenu.h"
#include "iHeroMenu.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataCamera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_SIDE_AXIS = 1,
	N_SIDE_ALLIES = 2,
	N_SIDEMENU_CAMERA = 47,
	N_SIDEMENU_TEMPLATE = 2387;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScriptHoverButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptHoverButton: public CHoverButton
{
	OBJECT_BASIC_METHODS(CScriptHoverButton);
private:
	ZDATA_(CHoverButton)
	CPtr<NGame::CRenderBaseInterface> pInterface;
	////
	bool bHoverNotify;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&pInterface); f.Add(3,&bChecked); f.Add(4,&bHoverNotify); return 0; }
	bool bChecked = false;

protected:
	void OnAction();

public:
	CScriptHoverButton() {}
	CScriptHoverButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *pInterface );

	void SetChecked( bool bState ) { bChecked = bState; }   // selected/highlighted (retail: the menu script sets it)
	bool IsChecked() const { return bChecked; }

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScriptHoverButton::CScriptHoverButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *_pInterface ):
	CHoverButton( sInfo ), pInterface( _pInterface ), bHoverNotify( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptHoverButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsMouseCover() != bHoverNotify )
	{
		NWorld::CCommand *pCmd = 
			new NWorld::CCmdCallScriptFunction( "OnScriptNotify", "si", GetWindowID().c_str(), IsMouseCover() ? 1 : 0 );
		pInterface->Command( pCmd );
	}

	bHoverNotify = IsMouseCover();

	// Keep the selected button visually highlighted: force the bracketed state-3 "selected" art while
	// checked (the release does exactly this @0x1bec00 -- ForceState(bChecked, 3)).
	ForceState( bChecked, 3 );

	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptHoverButton::OnAction()
{
	NWorld::CCommand *pCmd = new NWorld::CCmdCallScriptFunction( "OnScriptNotify", "si", GetWindowID().c_str(), 2 );
	pInterface->Command( pCmd );

	CHoverButton::OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSideMenuUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// The retail side-menu container (353) ships no static cancel/next/axis/allies controls (the dev
// looked them up by name -> dead buttons). The release builds them PROGRAMMATICALLY in two CButtonsLine
// over the "line_1"/"line_2" template controls (the main-menu precedent) + adds a difficulty row.
// Base is CDesktopWindow (the release's script-UI host). operator& 7 -> 15 tags (@0x23ed60).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSideMenuUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CSideMenuUI);
public:
	enum ESide
	{
		SIDE_NONE,
		SIDE_AXIS,
		SIDE_ALLIES
	};
	enum EDifficulty
	{
		DIF_EASY,
		DIF_HARD,
		DIF_NORMAL
	};

private:
	ZDATA_(CWindow)
	CPtr<NGame::CRenderBaseInterface> pInterface;
	////
	ESide eSide;
	EDifficulty eDifficulty;
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pNext;
	CObj<CScriptHoverButton> pAxis;
	CObj<CScriptHoverButton> pAllies;
	CObj<CScriptHoverButton> pEasy;
	CObj<CScriptHoverButton> pHard;
	CObj<CScriptHoverButton> pNormal;
	CObj<CScriptHoverButton> pAxisOnView;
	CObj<CScriptHoverButton> pAlliesOnView;
	CObj<CButtonsLine> pButtonsLine1;
	CObj<CButtonsLine> pButtonsLine2;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pInterface); f.Add(3,&eSide); f.Add(4,&eDifficulty); f.Add(5,&pBack); f.Add(6,&pNext); f.Add(7,&pAxis); f.Add(8,&pAllies); f.Add(9,&pEasy); f.Add(10,&pHard); f.Add(11,&pNormal); f.Add(12,&pAxisOnView); f.Add(13,&pAlliesOnView); f.Add(14,&pButtonsLine1); f.Add(15,&pButtonsLine2); return 0; }

	// Build a script-notifying button into a CButtonsLine (the release's AddScriptHoverButton @0x1c5ba0:
	// a CScriptHoverButton over the generic CButtonsLine::AddButton, NORMAL/HOVER/disabled captions).
	CScriptHoverButton* AddScriptButton( CButtonsLine *pLine, const string &szID, int nCaption )
	{
		CScriptHoverButton *pButton = new CScriptHoverButton( SWindowInfo( pLine, SPoint( 0, 0 ), SPoint( 0, 0 ), szID, STYLE_ENABLED | STYLE_VISIBLE ), pInterface );
		// State 3 is the release's "selected" art: the caption bracketed `[ ... ]`. The font/colour tag
		// (GetDBString 11129) must lead so the brackets share the caption's font (else `[ ` renders in
		// the default small font).
		pLine->AddButton( pButton, -1,
			GetDBString( 11129 ) + GetDBString( nCaption ),                 // normal
			GetDBString( 11130 ) + GetDBString( nCaption ),                 // hover
			GetDBString( 11129 ) + L"[ " + GetDBString( nCaption ) + L" ]", // state 3 = selected
			GetDBString( 17338 ) + GetDBString( nCaption ) );               // disabled
		return pButton;
	}

	// Highlight the selected side + difficulty (retail drives this from the menu script; we set it in C++).
	void UpdateChecked()
	{
		if ( IsValid( pAxis ) )         pAxis->SetChecked( eSide == SIDE_AXIS );
		if ( IsValid( pAllies ) )       pAllies->SetChecked( eSide == SIDE_ALLIES );
		if ( IsValid( pAxisOnView ) )   pAxisOnView->SetChecked( eSide == SIDE_AXIS );
		if ( IsValid( pAlliesOnView ) ) pAlliesOnView->SetChecked( eSide == SIDE_ALLIES );
		if ( IsValid( pEasy ) )         pEasy->SetChecked( eDifficulty == DIF_EASY );
		if ( IsValid( pHard ) )         pHard->SetChecked( eDifficulty == DIF_HARD );
		if ( IsValid( pNormal ) )       pNormal->SetChecked( eDifficulty == DIF_NORMAL );
	}

public:
	CSideMenuUI() {}
	CSideMenuUI( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *pInterface );

	ESide GetSide() const { return eSide; }
	EDifficulty GetDifficulty() const { return eDifficulty; }

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSideMenuUI::CSideMenuUI( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *_pInterface ):
	CDesktopWindow( sInfo ), pInterface( _pInterface ), eSide( SIDE_NONE ), eDifficulty( DIF_EASY )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSideMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_NOTIFY:
			{
				if ( sEvent.szID == "axis" )        { eSide = SIDE_AXIS; UpdateChecked(); return true; }
				else if ( sEvent.szID == "allies" ) { eSide = SIDE_ALLIES; UpdateChecked(); return true; }
				else if ( sEvent.szID == "easy" )   { eDifficulty = DIF_EASY; UpdateChecked(); return true; }
				else if ( sEvent.szID == "hard" )   { eDifficulty = DIF_HARD; UpdateChecked(); return true; }
				else if ( sEvent.szID == "normal" ) { eDifficulty = DIF_NORMAL; UpdateChecked(); return true; }

				break;
			}
		case EVENT_TEMPLATELOAD:
			{
				// The in-3D-view "axis"/"allies" side clickables (notify the menu script on hover/click).
				// Orphan-safe if our container 353 lacks these controls.
				pAxisOnView = new CScriptHoverButton( sEvent.pLoader->GetControl( "axis" ), pInterface );
				pAxisOnView->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 11131 ) );
				pAxisOnView->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 11131 ) );

				pAlliesOnView = new CScriptHoverButton( sEvent.pLoader->GetControl( "allies" ), pInterface );
				pAlliesOnView->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 11132 ) );
				pAlliesOnView->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 11132 ) );

				// The navigable buttons live in two CButtonsLine over the line_1/line_2 template controls.
				pButtonsLine1 = new CButtonsLine( sEvent.pLoader->GetControl( "line_1" ) );
				pButtonsLine2 = new CButtonsLine( sEvent.pLoader->GetControl( "line_2" ) );

				pBack = pButtonsLine1->AddHoverButton( "cancel", 19250,
					GetDBString( 11129 ) + GetDBString( 11174 ),
					GetDBString( 11130 ) + GetDBString( 11174 ),
					GetDBString( 17338 ) + GetDBString( 11174 ) );
				pAxis   = AddScriptButton( pButtonsLine1, "axis",   11131 );
				pAllies = AddScriptButton( pButtonsLine1, "allies", 11132 );
				pNext = pButtonsLine1->AddHoverButton( "next", -1,
					GetDBString( 11129 ) + GetDBString( 16820 ),
					GetDBString( 11130 ) + GetDBString( 16820 ),
					GetDBString( 17339 ) + GetDBString( 16820 ) );

				pEasy   = AddScriptButton( pButtonsLine2, "easy",   17329 );
				pNormal = AddScriptButton( pButtonsLine2, "normal", 17330 );
				pHard   = AddScriptButton( pButtonsLine2, "hard",   17331 );

				UpdateChecked();   // initial highlight (default difficulty)
				break;
			}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSideMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSideMenuInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CSideMenuInterface);
private:
	NInput::CBind bindClose, bindNext;

	ZDATA_(CRenderBaseInterface)
	CObj<NUI::CSideMenuUI> pSideMenuUI;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&pSideMenuUI); return 0; }

public:
	CSideMenuInterface();

	void Initialize();

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSideMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
CSideMenuInterface::CSideMenuInterface():
	bindClose( "cancel" ), bindNext( "next" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSideMenuInterface::Initialize()
{
	CRenderBaseInterface::Initialize( N_SIDEMENU_TEMPLATE );

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_SIDEMENU_CAMERA );
	ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	GetCamera()->SetPlacement( sCameraPos );

	pSideMenuUI = new NUI::CSideMenuUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "mainmenuUI" ), this );
	NUI::LoadTemplate( pSideMenuUI, NDb::GetUIContainer( 353 ) );
	pSideMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSideMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() ); 
		return true;
	}
	else if ( bindNext.ProcessEvent( sEvent ) )
	{
		// The release (@0x23e330) threads the chosen difficulty as the 2nd CICHeroMenu arg, with the
		// enum value passed DIRECTLY as the DB id: GetDBSide(eSide) (SIDE_AXIS=1/SIDE_ALLIES=2 = the
		// N_SIDE_* ids) + GetDBDifficulty(eDifficulty) (DIF_EASY=0/DIF_HARD=1/DIF_NORMAL=2). We guard the
		// null-side case (Next before a side is picked) — retail script-gates Next; our build doesn't.
		NDb::CSide *pDBSide = ( pSideMenuUI->GetSide() != NUI::CSideMenuUI::SIDE_NONE )
			? NDb::GetDBSide( pSideMenuUI->GetSide() ) : 0;
		NDb::CDBDifficulty *pDBDifficulty = NDb::GetDBDifficulty( pSideMenuUI->GetDifficulty() );
		if ( IsValid( pDBSide ) )
			NMainLoop::Command( new NGame::CICHeroMenu( pDBSide, pDBDifficulty ) );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSideMenuInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		// Render the 3D world full-screen, exactly like the main menu (iMainMenu.cpp). The predecessor
		// derived a sub-rect from a "clientview" UI control, but the retail side-menu container (353)
		// ships no such control -> GetUIWindow fell back to a zero-size window -> zero camera screen-rect
		// -> CRenderBaseInterface::RenderFrame skipped pScene->Draw -> no 3D backdrop (uncleared backbuffer)
		// + "control clientview not found" spam. The release renders the menu world full-screen.
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );

		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMainMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICSideMenu::CICSideMenu()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSideMenu::Exec()
{
	CSideMenuInterface *pRes = new CSideMenuInterface;
	pRes->Initialize();
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1112180, CSideMenuUI );
REGISTER_SAVELOAD_CLASS( 0xB1112181, CScriptHoverButton );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB111218A, CSideMenuInterface );
