#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"			// csSystem (the "Added N XP points for hint" log)
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataMisc.h"			// NDb::CUIHint (nSequenceID / pTitle / pString)
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iShowHint.h"
#include "RPGGlobal.h"						// NRPG::CGlobalGame::AddXPToAllUnits
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed 1:1 on the WORKING dev sibling iShowClue.cpp (the clue popup is structurally identical to
// the hint popup -- both load the in-game-menu container 322 with a "view"/"cancel"/"scroll" widget set).
// Only the data source (CUIHint vs CScenarioClue), the close-event post (the hint releases its WaitForUI id),
// and the XP award on show (CICShowHint::Exec) differ. Retail RVAs: CShowHintView::ProcessMessage @0x23acc0,
// CShowHintUI::ProcessMessage @0x23abd0, CShowHintInterface::Initialize @0x23a6e0, CICShowHint::Exec @0x23ab00.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowHintView -- the inner widget window. On EVENT_TEMPLATELOAD it wraps the template's "cancel" close
// button + the "view" scrollable text (CScrollWindow<CText>) and pushes the hint body (prefixed with the
// "Clue&Hint Format" markup string 0x4F22) into it; on EVENT_TEMPLATELOADCOMPLETE it binds the "scroll" bar.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowHintView: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowHintView);
private:
	ZDATA_(CWindow)
	CDBPtr<NDb::CUIHint> pHint;
	////
	CObj<CText> pDescription;
	CObj<CScrollWindow<CText> > pDescriptionView;
	////
	CObj<CFlashButton> pCloseButton;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pHint); f.Add(3,&pDescription); f.Add(4,&pDescriptionView); f.Add(5,&pCloseButton); return 0; }

public:
	CShowHintView() {}
	CShowHintView( const SWindowInfo &sInfo, NDb::CUIHint *_pHint ): CWindow( sInfo ), pHint( _pHint ) {}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowHintView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pDescriptionView = new CScrollWindow<CText>( sEvent.pLoader->GetControl( "view" ) );
			pDescription = pDescriptionView->GetClientWindow();

			// retail @0x23acc0: text = DB string 0x4F22 ("Clue&Hint Format" markup prefix,
			// <font face=Courier size=16pt><color=0xFF5A5959>) + body only. pTitle is NOT rendered here --
			// the body string carries its own title line.
			if ( IsValid( pHint ) )
				pDescription->SetText( GetDBString( 0x4F22 ) + GetDBString( pHint->pString ), true );
			else
				pDescription->SetText( L"<color=red>[ERROR]Hint not set" );

			// retail @0x23acc0: fit the text to its content height, keeping the template width
			SPoint sRealSize;
			pDescription->GetRealSize( &sRealSize );
			sRealSize.x = pDescription->GetSize().x;
			pDescription->SetSize( sRealSize );
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
// CShowHintUI -- the hint modal's top window. On EVENT_TEMPLATELOAD it wraps the "view" sub-template as a
// CShowHintView, forwarding the hint record so the view can render it.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowHintUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowHintUI);
private:
	ZDATA_(CWindow)
	CDBPtr<NDb::CUIHint> pHint;
	////
	CObj<CShowHintView> pView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pHint); f.Add(3,&pView); return 0; }

public:
	CShowHintUI() {}
	CShowHintUI( const SWindowInfo &sInfo, NDb::CUIHint *_pHint ): CWindow( sInfo ), pHint( _pHint ) {}

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowHintUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pView = new CShowHintView( sEvent.pLoader->GetControl( "view" ), pHint );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowHintInterface -- the modal "show hint" screen. Modelled on CShowClueInterface; the close bind ("cancel")
// additionally posts a CCmdInterfaceEvent(nEventID) to the mission (unblocking the lua WaitForUI(id) that queued
// the hint -- the hint binding uses AddUICommandWithID, unlike ClueShow) before popping the modal.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowHintInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CShowHintInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	CPtr<IMission> pMission;
	int nEventID;
	CDBPtr<NDb::CUIHint> pHint;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CWindow> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMission); f.Add(3,&nEventID); f.Add(4,&pHint); f.Add(5,&pCursor); f.Add(6,&pInterface); f.Add(7,&pUI); f.Add(8,&pScreenShot); return 0; }

public:
	CShowHintInterface();

	void Initialize( IMission *pMission, int nEventID, NDb::CUIHint *pHint, NGScene::CScreenshotTexture *pScreenShotTexture );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowHintInterface::CShowHintInterface():
	bindClose( "cancel" ), nEventID( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowHintInterface::Initialize( IMission *_pMission, int _nEventID, NDb::CUIHint *_pHint,
	NGScene::CScreenshotTexture *pScreenShotTexture )
{
	pMission = _pMission;
	nEventID = _nEventID;
	pHint = _pHint;

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

	pUI = new NUI::CShowHintUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "ingamemenu", NUI::STYLE_ENABLED ), pHint );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 322 ) );	// the in-game popup container (id 0x142, shared with the clue screen)
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowHintInterface::Step()
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
void CShowHintInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowHintInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		// release @0x23a4a0: post the interface-event (carries nEventID) to the mission -- unblocks the lua
		// WaitForUI(id) that queued this hint -- then tear down the modal.
		if ( IsValid( pMission ) )
			pMission->DoEvent( new NWorld::CCmdInterfaceEvent( nEventID ) );
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowHintInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowHint
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowHint::CICShowHint( IMission *_pMission, int _nEventID, NDb::CUIHint *_pHint, NRPG::CGlobalGame *_pGlobalGame,
	NGScene::CScreenshotTexture *_pScreenShotTexture ):
	pMission( _pMission ), nEventID( _nEventID ), pHint( _pHint ), pGlobalGame( _pGlobalGame ),
	pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICShowHint::Exec()
{
	CShowHintInterface *pRes = new CShowHintInterface();
	pRes->Initialize( pMission, nEventID, pHint, pScreenShotTexture );
	// release @0x23ab00: the hint's +0x10 slot (nSequenceID) doubles as its XP reward -- log it + award it to
	// the whole party as the hint is shown.
	if ( IsValid( pHint ) )
	{
		csSystem << "Added " << pHint->nSequenceID << " XP points for hint" << endl;
		if ( IsValid( pGlobalGame ) )
			pGlobalGame->AddXPToAllUnits( ( float )pHint->nSequenceID );
	}
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0x1423130, CShowHintInterface );	// LUA convergence (release id; hint machinery)
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0x1423131, CShowHintUI );			// LUA convergence (release id)
REGISTER_SAVELOAD_CLASS( 0x1423132, CShowHintView );		// LUA convergence (release id)
