#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iLoseFake.h"
#include "iMainMenu.h"
#include "iSaveLoad.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoseMenuUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoseMenuUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CLoseMenuUI);
private:
	ZDATA_(CWindow)
	CObj<CHoverButton> pExit;
	CObj<CHoverButton> pLoad;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pExit); f.Add(3,&pLoad); f.Add(4,&pRestart); f.Add(5,&pText); f.Add(6,&pString); return 0; }
	CObj<CHoverButton> pRestart;
	CPtr<CText> pText;
	CDBPtr<NDb::CString> pString;

public:
	CLoseMenuUI() {}
	CLoseMenuUI( const SWindowInfo &sInfo, NDb::CString *pReason = 0 );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLoseMenuUI::CLoseMenuUI( const SWindowInfo &sInfo, NDb::CString *pReason ):
	CWindow( sInfo ), pString( pReason )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLoseMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pExit = new CHoverButton( sEvent.pLoader->GetControl( "exitgame" ) );
			pExit->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 11235 ) );
			pExit->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 11235 ) );

			pLoad = new CHoverButton( sEvent.pLoader->GetControl( "loadgame" ) );   // retail control name; the dev's "load" doesn't exist in container 371 -> the button was invisible
			pLoad->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 11236 ) );
			pLoad->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 11236 ) );

			// release CLoseMenuUI::ProcessMessage @0x1f3fc0: also wrap the "restart" control (label string 18970)
			// -- the dev never built it, so Restart was invisible.
			pRestart = new CHoverButton( sEvent.pLoader->GetControl( "restart" ) );
			pRestart->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 18970 ) );
			pRestart->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 18970 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// release CLoseMenuUI::ProcessMessage @0x1f3fc0 fills the reason text via GetUIWindow<CText>("text").
			// But container 371's text control (2028) has a BLANK window-id in this DB, so GetUIWindow fabricates
			// an invisible 0x0 control -- which is why NO text (not even "Game Over!") showed. Build the CText
			// directly at control 2028's geometry, with the "Game Over!" header (11234) + the campaign lose reason
			// (pString, e.g. "Main Hero died!"; the squad-wipe -1 path has none -> just the header).
			pText = new CText( SWindowInfo( this, SPoint( 400, 247 ), SPoint( 231, 14 ), "text", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
			pText->SetText( IsValid( pString ) ? GetDBString( 11234 ) + GetDBString( pString ) : GetDBString( 11234 ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLeaveZoneMenuUI -- release-new (iLeaveZone): the "leave zone" modal's widget window. Modelled on
// CExitMenuUI: on EVENT_TEMPLATELOAD it wraps the template's "cancel" / "endmission" controls in
// CHoverButtons (so they render + hover) -- the interface's binds catch their clicks.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLeaveZoneMenuUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CLeaveZoneMenuUI);
private:
	ZDATA_(CWindow)
	CObj<CHoverButton> pCancel;
	CObj<CHoverButton> pEndMission;
	CDBPtr<NDb::CString> pString;		// the prompt title carried from the command
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pCancel); f.Add(3,&pEndMission); f.Add(4,&pString); return 0; }
	CPtr<CText> pText;					// prompt label control, rebuilt on EVENT_TEMPLATELOAD (transient; not saved)

public:
	CLeaveZoneMenuUI() {}
	CLeaveZoneMenuUI( const SWindowInfo &sInfo, NDb::CString *pTitle ): CWindow( sInfo ), pString( pTitle ) {}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLeaveZoneMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			// retail NUI::CLeaveZoneMenuUI::ProcessMessage @0x1f0ca0: fill the prompt label from the command's
			// title string (else the default 16837), and label both buttons (hover/normal). Without this the
			// modal renders blank -- no prompt text and invisible text-only buttons.
			pText = new CText( sEvent.pLoader->GetControl( "text" ) );
			pText->SetText( IsValid( pString ) ? GetDBString( pString ) : GetDBString( 16837 ) );

			pCancel = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pCancel->AddTextState( CHoverButton::STATE_HOVER,  GetDBString( 16840 ) + GetDBString( 11144 ) );
			pCancel->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 16840 ) + GetDBString( 11143 ) );

			pEndMission = new CHoverButton( sEvent.pLoader->GetControl( "endmission" ) );
			pEndMission->AddTextState( CHoverButton::STATE_HOVER,  GetDBString( 16839 ) + GetDBString( 11144 ) );
			pEndMission->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 16839 ) + GetDBString( 11143 ) );
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
// CLoseMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoseMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CLoseMenuInterface);
private:
	NInput::CBind bindLoadGame, bindExitGame, bindRestartGame;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CLoseMenuUI> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pUI); f.Add(5,&pScreenShot); return 0; }

public:
	CLoseMenuInterface();

	void Initialize( int nStringID, NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLoseMenuInterface::CLoseMenuInterface():
	bindLoadGame( "loadgame" ), bindExitGame( "exitgame" ), bindRestartGame( "restart" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoseMenuInterface::Initialize( int nStringID, NGScene::CScreenshotTexture *pScreenShotTexture )
{
	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	if ( !IsValid( pScreenShotTexture ) )
	{
		pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
		pScreenShot->Generate();
	}
	else
		pScreenShot->SetTexture( pScreenShotTexture );

	pUI = new NUI::CLoseMenuUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "exitmenu", NUI::STYLE_ENABLED ), NDb::GetString( nStringID ) );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 371 ) );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoseMenuInterface::Step()
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
void CLoseMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLoseMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindLoadGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICSaveLoadMenu( LOAD, pScreenShot->GetTexture() ) );
		return true;
	}
	else if ( bindRestartGame.ProcessEvent( sEvent ) )
	{
		// retail @0x1f3880: restart = reload the mission-start save (CICSaveRestartMission writes "restart.sav"
		// at mission start). CICLoad::Exec is try-guarded, so this is a safe no-op until that mission-start
		// autosave is wired (separate follow-up in CMission::Initialize).
		NMainLoop::Command( new NMainLoop::CICLoad( string( "restart.sav" ) ) );
		return true;
	}
	else if ( bindExitGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICMainMenu() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoseMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICLoseMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICLoseMenu::CICLoseMenu( int _nStringID, NGScene::CScreenshotTexture *_pScreenShotTexture ):
	nStringID( _nStringID ), pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICLoseMenu::Exec()
{
	CLoseMenuInterface *pRes = new CLoseMenuInterface();
	pRes->Initialize( nStringID, pScreenShotTexture );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLeaveZoneMenuInterface -- release-new (iLeaveZone): the in-game "leave zone" modal screen. Modelled
// on CLoseMenuInterface; the cancel bind pops the modal, confirm/endmission close it and end the mission.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLeaveZoneMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CLeaveZoneMenuInterface);
private:
	NInput::CBind bindClose, bindConfirm, bindEndMission;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CLeaveZoneMenuUI> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	CPtr<IMission> pMission;
	bool bAutoSave;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pUI); f.Add(5,&pScreenShot); f.Add(6,&pMission); f.Add(7,&bAutoSave); return 0; }

public:
	CLeaveZoneMenuInterface();

	void Initialize( IMission *pMission, NDb::CString *pTitle, bool bAutoSave, NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLeaveZoneMenuInterface::CLeaveZoneMenuInterface():
	bindClose( "cancel" ), bindConfirm( "confirm" ), bindEndMission( "endmission" ), bAutoSave( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLeaveZoneMenuInterface::Initialize( IMission *_pMission, NDb::CString *pTitle, bool _bAutoSave,
	NGScene::CScreenshotTexture *pScreenShotTexture )
{
	pMission = _pMission;
	bAutoSave = _bAutoSave;

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	if ( !IsValid( pScreenShotTexture ) )
	{
		pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
		pScreenShot->Generate();
	}
	else
		pScreenShot->SetTexture( pScreenShotTexture );

	pUI = new NUI::CLeaveZoneMenuUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "exitmenu", NUI::STYLE_ENABLED ), pTitle );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 379 ) );	// the leave-zone container (id 0x17b, from CLeaveZoneMenuInterface::Initialize @0x1f0b56)
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLeaveZoneMenuInterface::Step()
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
void CLeaveZoneMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLeaveZoneMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
	else if ( bindConfirm.ProcessEvent( sEvent ) || bindEndMission.ProcessEvent( sEvent ) )
	{
		// confirm / end -> close the modal and end the mission. (The retail also inserts an autosave
		// CICSave when bAutoSave; its slot-name DB string id isn't recovered, so it is omitted here.)
		vector<CPtr<NMainLoop::CInterfaceCommand> > cmds;
		cmds.push_back( new NMainLoop::CICExitModal() );
		cmds.push_back( new CICEndMission( pMission ) );
		NMainLoop::Command( new NMainLoop::CICContainer( cmds ) );
		return false;	// ORIGINAL BUG (confirmed): returns false on confirm/endmission
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLeaveZoneMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICLeaveZoneMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICLeaveZoneMenu::CICLeaveZoneMenu( IMission *_pMission, NDb::CString *_pString, bool _bAutoSave,
	NGScene::CScreenshotTexture *_pScreenShotTexture ):
	pMission( _pMission ), pString( _pString ), bAutoSave( _bAutoSave ), pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICLeaveZoneMenu::Exec()
{
	CLeaveZoneMenuInterface *pRes = new CLeaveZoneMenuInterface();
	pRes->Initialize( pMission, pString, bAutoSave, pScreenShotTexture );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1217151, CLoseMenuUI );
REGISTER_SAVELOAD_CLASS( 0xB3130121, CLeaveZoneMenuUI );			// LUA convergence PART B (fresh id)
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1217150, CLoseMenuInterface );
REGISTER_SAVELOAD_CLASS( 0xB3130122, CLeaveZoneMenuInterface );	// LUA convergence PART B (fresh id)
