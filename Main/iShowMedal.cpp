#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataRPG.h"		// NDb::CSide (release-new pMedalPaperContainer, save tag 14)
#include "..\DBFormat\DataMisc.h"		// NDb::CMedal (pName)
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iShowMedal.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release iShowMedal.obj convergence -- the "show awarded medal" modal. Reconstructed 1:1 on the working
// dev siblings iShowClue.cpp / iShowHint.cpp (identical CIC -> IInterfaceBase -> CWindow modal lifecycle),
// the data source swapped to a side + medal DB record + a wide player name. Unlike the clue/hint popups the
// medal window is a single CShowMedalUI (no nested view): it binds a "name" and a "medal" CText label plus a
// "cancel" close button. Retail RVAs (VA = RVA + 0x400000):
//   NUI::CShowMedalUI::ProcessMessage       @0x23c180
//   NGame::CShowMedalInterface::Initialize  @0x23bcf0   (LoadTemplate arg = pSide->pMedalPaperContainer, @0x23c035)
//   NGame::CICShowMedal::Exec               @0x23c100   (guards IsValid(pMedal))
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowMedalUI -- the medal popup window. On EVENT_TEMPLATELOAD it wraps the template's "cancel" close
// button; on EVENT_TEMPLATELOADCOMPLETE it binds the "name"/"medal" CText children and fills them with the
// localized player-name line and medal-name line.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowMedalUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowMedalUI);
private:
	ZDATA_(CWindow)
	wstring wsName;
	CDBPtr<NDb::CMedal> pMedal;
	////
	CPtr<CText> pName;
	CPtr<CText> pMedalName;
	////
	CObj<CFlashButton> pCloseButton;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&wsName); f.Add(3,&pMedal); f.Add(4,&pName); f.Add(5,&pMedalName); f.Add(6,&pCloseButton); return 0; }

public:
	CShowMedalUI() {}
	CShowMedalUI( const SWindowInfo &sInfo, const wstring &wsName, NDb::CMedal *pMedal );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowMedalUI::CShowMedalUI( const SWindowInfo &sInfo, const wstring &_wsName, NDb::CMedal *_pMedal ):
	CWindow( sInfo ), wsName( _wsName ), pMedal( _pMedal )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowMedalUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pName      = GetUIWindow<CText>( this, "name" );
			pMedalName = GetUIWindow<CText>( this, "medal" );
			// release @0x23c266 / @0x23c2d9: name line = GetDBString(0x494b) + player name; medal line =
			// GetDBString(medal->pName) + GetDBString(0x494a). (dev NUI::CText::SetText takes a single wstring --
			// the release's bProcessTAGs=true argument is not present in the dev signature and is dropped.)
			pName->SetText( GetDBString( 0x494b ) + wsName );
			pMedalName->SetText( GetDBString( pMedal->pName ) + GetDBString( 0x494a ) );
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
// CShowMedalInterface -- the modal "show medal" screen. Modelled on CShowClueInterface / CShowHintInterface;
// the close bind ( "cancel" ) just pops the modal (no event post). Initialize freezes the current frame as a
// B&W backdrop and shows the medal window loaded from the side's medal-paper UI container.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowMedalInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CShowMedalInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	wstring wsName;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CMedal> pMedal;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CWindow> pUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsName); f.Add(3,&pSide); f.Add(4,&pMedal); f.Add(5,&pCursor); f.Add(6,&pInterface); f.Add(7,&pUI); f.Add(8,&pScreenShot); return 0; }

public:
	CShowMedalInterface();

	void Initialize( NDb::CSide *pSide, const wstring &wsName, NDb::CMedal *pMedal,
		NGScene::CScreenshotTexture *pScreenShotTexture );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowMedalInterface::CShowMedalInterface():
	bindClose( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowMedalInterface::Initialize( NDb::CSide *_pSide, const wstring &_wsName, NDb::CMedal *_pMedal,
	NGScene::CScreenshotTexture *_pScreenShotTexture )
{
	pSide  = _pSide;
	wsName = _wsName;
	pMedal = _pMedal;

	pCursor    = NUI::ICursor::Create( false, CVec2( -1, -1 ) );
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	// release @0x23bcf0: the medal screen unconditionally tints the captured frame B&W and binds it (no
	// IsValid/Generate branch -- unlike clue/hint). V_SCREENSHOT_MUL_COLOR (absent global) is inlined as the
	// twins' B&W coefficient (0.5,0.5,0.5,1).
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	pScreenShot->SetTexture( _pScreenShotTexture );

	pUI = new NUI::CShowMedalUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "ingamemenu", NUI::STYLE_ENABLED ), wsName, pMedal );
	// release @0x23c035 (mov edx,[ecx+0x60]): LoadTemplate template arg = pSide->pMedalPaperContainer (CSide +0x60),
	// NOT NDb::GetUIContainer(322) used by the clue/hint twins.
	NUI::LoadTemplate( pUI, pSide->pMedalPaperContainer );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowMedalInterface::Step()
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
void CShowMedalInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowMedalInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
void CShowMedalInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0, 0, 0 ) );		// release ClearScreenZBuffer() (absent) -> black clear
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowMedal
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowMedal::CICShowMedal( NDb::CSide *_pSide, const wstring &_wsName, NDb::CMedal *_pMedal,
	NGScene::CScreenshotTexture *_pScreenShotTexture ):
	wsName( _wsName ), pSide( _pSide ), pMedal( _pMedal ), pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICShowMedal::Exec()
{
	// release @0x23c119: do nothing unless the medal record is live (non-null + not flagged destroyed).
	if ( !IsValid( pMedal ) )
		return;

	CShowMedalInterface *pRes = new CShowMedalInterface();
	pRes->Initialize( pSide, wsName, pMedal, pScreenShotTexture );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3423160, CShowMedalInterface );
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3423161, CShowMedalUI );
