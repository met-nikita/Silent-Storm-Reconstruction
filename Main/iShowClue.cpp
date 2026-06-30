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
#include "iShowClue.h"
#include "RPGGlobal.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowClueView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowClueView: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowClueView);
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalGame> pGame;
	CPtr<NScenario::CScenarioClue> pClue;
	////
	CObj<CMLText> pDescription;
	CObj<CScrollWindow<CMLText> > pDescriptionView;
	////
	CObj<CFlashButton> pCloseButton;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGame); f.Add(3,&pPlayer); f.Add(4,&pClue); f.Add(5,&pDescription); f.Add(6,&pDescriptionView); f.Add(7,&pBackground); f.Add(8,&pCloseButton); return 0; }
	CPtr<NRPG::CGlobalPlayer> pPlayer;
	CPtr<NUI::CImage> pBackground;

public:
	CShowClueView() {}
	CShowClueView( const SWindowInfo &sInfo, NRPG::CGlobalGame *pGame, NScenario::CScenarioClue *pClue );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowClueView::CShowClueView( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame, NScenario::CScenarioClue *_pClue ):
	CWindow( sInfo ), pGame( _pGame ), pClue( _pClue )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowClueView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pDescriptionView = new CScrollWindow<CMLText>( sEvent.pLoader->GetControl( "view" ) );
			pDescription = pDescriptionView->GetClientWindow();

			CPtr<NDb::CString> pClueDescription = pGame->pScenarioTracker->GetClueDescriptionFromObjective( pClue );
			if ( IsValid( pClueDescription ) )
				pDescription->SetText( pClueDescription->szStr );
			else
				pDescription->SetText( L"<color=red>[ERROR]Description not set" );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pDescriptionView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowClueUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowClueUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowClueUI);
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalGame> pGame;
	CPtr<NScenario::CScenarioClue> pClue;
	////
	CObj<CShowClueView> pView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGame); f.Add(3,&pPlayer); f.Add(4,&pClue); f.Add(5,&pView); return 0; }
	CPtr<NRPG::CGlobalPlayer> pPlayer;

public:
	CShowClueUI() {}
	CShowClueUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *pGame, NScenario::CScenarioClue *pClue );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowClueUI::CShowClueUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame, NScenario::CScenarioClue *_pClue ):
	CWindow( sInfo ), pGame( _pGame ), pClue( _pClue )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowClueUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pView = new CShowClueView( sEvent.pLoader->GetControl( "view" ), pGame, pClue );
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
class CShowClueInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CShowClueInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	CPtr<NRPG::CGlobalGame> pGame;
	CPtr<NScenario::CScenarioClue> pClue;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CWindow> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nEventID); f.Add(3,&pMission); f.Add(4,&pGame); f.Add(5,&pPlayer); f.Add(6,&pClue); f.Add(7,&pCursor); f.Add(8,&pInterface); f.Add(9,&pUI); f.Add(10,&pScreenShot); return 0; }
	int nEventID = 0;
	CPtr<NGame::IMission> pMission;
	CPtr<NRPG::CGlobalPlayer> pPlayer;

public:
	CShowClueInterface();

	void Initialize( NRPG::CGlobalGame *pGame, NScenario::CScenarioClue *pClue, NUI::CScreenShot *pScreenShot );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowClueInterface::CShowClueInterface():
	bindClose( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowClueInterface::Initialize( NRPG::CGlobalGame *_pGame, NScenario::CScenarioClue *_pClue, NUI::CScreenShot *_pScreenShot )
{
	pGame = _pGame;
	pClue = _pClue;

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	if ( !IsValid( _pScreenShot ) )
	{
		pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
		pScreenShot->Generate();
	}
	else
		pScreenShot->SetTexture( _pScreenShot->GetTexture() );

	pUI = new NUI::CShowClueUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "ingamemenu", NUI::STYLE_ENABLED ), pGame, pClue );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 322 ) );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowClueInterface::Step()
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
void CShowClueInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowClueInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowClueInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowClue
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowClue::CICShowClue( NRPG::CGlobalGame *_pGame, NScenario::CScenarioClue *_pClue, NUI::CScreenShot *_pScreenShot ):
	pGame( _pGame ), pClue( _pClue ), pScreenShot( _pScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICShowClue::Exec()
{
	CShowClueInterface *pRes = new CShowClueInterface();
	pRes->Initialize( pGame, pClue, pScreenShot );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1001180, CShowClueInterface );
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1001181, CShowClueUI );
REGISTER_SAVELOAD_CLASS( 0xB1001182, CShowClueView );
