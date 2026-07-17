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
#include "iExitMenu.h"
#include "iSaveLoad.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExitMenuUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExitMenuUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CExitMenuUI);
private:
	ZDATA_(CWindow)
	CObj<CHoverButton> pExit;
	CObj<CHoverButton> pSave;
	CObj<CHoverButton> pCancel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bAllowSave); f.Add(3,&pExit); f.Add(4,&pSave); f.Add(5,&pCancel); return 0; }
	bool bAllowSave = false;

public:
	CExitMenuUI() {}
	CExitMenuUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CExitMenuUI::CExitMenuUI( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExitMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pExit = new CHoverButton( sEvent.pLoader->GetControl( "exitgame" ) );
			pExit->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 11199 ) );
			pExit->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 11199 ) );

			pSave = new CHoverButton( sEvent.pLoader->GetControl( "save" ) );
			pSave->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 11200 ) );
			pSave->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 11200 ) );

			pCancel = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pCancel->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 11201 ) );
			pCancel->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 11201 ) );
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
// CExitMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExitMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CExitMenuInterface);
private:
	NInput::CBind bindClose, bindSaveGame, bindExitGame;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CExitMenuUI> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bAllowSave); f.Add(3,&pCursor); f.Add(4,&pInterface); f.Add(5,&pUI); f.Add(6,&pScreenShot); return 0; }
	bool bAllowSave = true;   // retail threads a CICExitMenu ctor arg; dev's only opener (iMain, the global Alt+F4 modal) allows saving

public:
	CExitMenuInterface();

	void Initialize( NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CExitMenuInterface::CExitMenuInterface():
	bindClose( "cancel" ), bindSaveGame( "save" ), bindExitGame( "exitgame" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExitMenuInterface::Initialize( NGScene::CScreenshotTexture *pScreenShotTexture )
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

	pUI = new NUI::CExitMenuUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "exitmenu", NUI::STYLE_ENABLED ) );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 357 ) );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExitMenuInterface::Step()
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
void CExitMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExitMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
	else if ( bindSaveGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICSaveLoadMenu( SAVE, pScreenShot->GetTexture(), bAllowSave ) );   // retail @0x1d1310 forwards the screen's own gate
		return true;
	}
	else if ( bindExitGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( 0 );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExitMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICExitMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICExitMenu::CICExitMenu( NGScene::CScreenshotTexture *_pScreenShotTexture ):
	pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICExitMenu::Exec()
{
	CExitMenuInterface *pRes = new CExitMenuInterface();
	pRes->Initialize( pScreenShotTexture );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1203151, CExitMenuUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1203150, CExitMenuInterface );
