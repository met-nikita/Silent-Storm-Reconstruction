#include "StdAfx.h"
#include "Transform.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "wInterface.h"        // NWorld::IWorld / IPlayer / CCommander (iRenderWorld.h members)
#include "Sound.h"
#include "RWGame.h"            // NRender::IRenderGame (iRenderWorld.h member)
#include "RWSound.h"           // NRender::IRenderSound (iRenderWorld.h member)
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iMain.h"
#include "iRenderWorld.h"      // NGame::CRenderBaseInterface
#include "iMainMenu.h"         // NGame::CICMainMenu (final-credits -> rebuild the main menu)
#include "iCreditsScreen.h"
#include "UIInterface.h"       // NUI::CInterface / GetDBString / LoadTemplate
#include "UIBaseCtrls.h"       // NUI::CMLText (the markup credits text)
#include "UICommCtrls.h"       // NUI::CVideoPlayer (the final-credits .bik)
#include "iDesktopWindow.h"    // NUI::CDesktopWindow (CCreditsUI base)
#include "Camera.h"            // ICamera::SCameraPos
#include "..\Input\Bind.h"     // NInput::CBind / SEvent / SetSection
#include "..\MiscDll\Commands.h"   // NGlobal::RegisterCmd
#include "..\MiscDll\LogStream.h"  // csSystem / endl
#include "..\DBFormat\DataCamera.h"    // NDb::GetDBCamera / CDBCamera
#include "..\DBFormat\DataFormat.h"    // NDb::GetUIContainer / CUIContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  iCreditsScreen  --  the credits roll.  Reconstructed from the release module
//  .\release\iCreditsScreen.obj (absent from this predecessor tree; the dev main-menu
//  "Credits" button was a dead no-op).  CCredits is a CRenderBaseInterface twin of
//  CMainMenuInterface (3D city backdrop, full-screen), with a CDesktopWindow that scrolls
//  the credits text up through a clip rect.  VA = RVA + 0x400000.
//
//  Release-base note: the release CCredits derives from CRenderBaseInterface : CMissionBase
//  (its Step/desktop machinery come from CMissionBase).  This predecessor's CRenderBaseInterface
//  has no CMissionBase, so -- exactly like CMainMenuInterface -- we reproduce the behaviour with a
//  Step() that sets the full-screen camera rect + RenderFrame (the release relies on CMissionBase
//  ::Step), and parent the UI to GetInterface() instead of CMissionBase::PushDesktop.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_CREDITS_TEMPLATE  = 2425,   // @Initialize push 0x979 -- reuses the main-menu 3D world template
	N_CREDITS_CAMERA    = 26,     // @Initialize mov ecx,0x1a -- reuses the main-menu DBCamera
	N_CREDITS_CONTAINER = 444;    // @Initialize mov ecx,0x1bc -- the credits UI container (GetUIContainer)
// The four DB strings (markup-tagged) concatenated into the scrolling roll (ctor @0x1cc3b8..).
const int
	N_CREDITS_STRING_0 = 0x4f0a,
	N_CREDITS_STRING_1 = 0x4f0b,
	N_CREDITS_STRING_2 = 0x4f0c,
	N_CREDITS_STRING_3 = 0x4f0d;
const float F_CREDITS_SCROLL_SPEED = 0.02f;   // @Draw 0x3ca3d70a -- pixels per millisecond
////////////////////////////////////////////////////////////////////////////////////////////////////
// "\n"/"\r"/0x85 -> markup, exactly as the dev iMissionUI.cpp file-local helper (kept file-local
// here too -- the release inlines the same conversion before CMLText::SetText).
////////////////////////////////////////////////////////////////////////////////////////////////////
static wstring ConvertLineBreaks( const wstring &szStr )
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
			case 133: // symbol L'…'
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
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCreditsUI  --  the scrolling credits desktop (saveload id 0xB3410171).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCreditsUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CCreditsUI);
private:
	ZDATA_(CDesktopWindow)
	float            fScroll;        // current scroll offset (px); starts -clipHeight so the roll enters from below
	STime            sLastTime;      // last Draw time, for the per-frame delta
	CObj<CMLText>    pText;          // the markup credits text (release CObj<CText>; this tree's markup text == CMLText)
	CObj<CWindow>    pClip;          // the clip rect the text scrolls through
	CObj<CVideoPlayer> pVideoPlayer; // the looping Credits.bik (final credits only)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&fScroll); f.Add(3,&sLastTime); f.Add(4,&pText); f.Add(5,&pClip); f.Add(6,&pVideoPlayer); return 0; }

public:
	CCreditsUI() {}
	CCreditsUI( const SWindowInfo &sInfo, bool bFinalCredits );   // @0x1cbff0

	bool IsComplete();                                            // @0x1cbcd0
	void Draw( const STime &sTime, NGScene::I2DGameView *pView ); // @0x1cbd20
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCreditsUI::CCreditsUI( const SWindowInfo &sInfo, bool bFinalCredits ):
	CDesktopWindow( sInfo ), fScroll( 0 ), sLastTime( 0 )
{
	// The clip rect (rows 128..640) masks the scroll; the text fills it and slides up through it.
	pClip = new CWindow( SWindowInfo( this, SPoint( 0, 128 ), SPoint( 1024, 512 ), "text",
	                     STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST | STYLE_TRANSPARENT ) );
	pText = new CMLText( SWindowInfo( pClip, SPoint( 0, 0 ), pClip->GetSize(), "text",
	                     STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST | STYLE_TRANSPARENT ) );
	pVideoPlayer = new CVideoPlayer( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "video",
	                     STYLE_ENABLED | STYLE_BOTTOMMOST | STYLE_TRANSPARENT ) );

	// Start the text just below the clip so the roll scrolls up into view.
	fScroll = float( -pClip->GetSize().y );

	// The roll body = the four credits DB strings (each already carrying <font Impact> markup) concatenated,
	// with literal line breaks converted to <br>.  SetText(...,true) processes the markup tags.
	wstring wsText = ConvertLineBreaks( GetDBString( N_CREDITS_STRING_0 ) + GetDBString( N_CREDITS_STRING_1 ) +
	                                    GetDBString( N_CREDITS_STRING_2 ) + GetDBString( N_CREDITS_STRING_3 ) );
	pText->SetText( wsText, true );

	// Size the text window to its content height (at least full-screen), so the whole roll can scroll past.
	SPoint sRealSize;
	pText->GetRealSize( &sRealSize );
	if ( sRealSize.y <= GetSize().y )
		sRealSize.y = GetSize().y;
	pText->SetSize( SPoint( pText->GetSize().x, sRealSize.y ) );

	// The looping Credits.bik plays (with sound) only for the end-game (final) credits.
	pVideoPlayer->SetStyle( STYLE_VISIBLE, bFinalCredits );
	if ( bFinalCredits )
	{
		pVideoPlayer->Set( ".\\res\\video\\Credits.bik", CVideoPlayer::PLAY_WITH_SOUND );
		pVideoPlayer->Play( true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsComplete @0x1cbcd0 -- true once the roll has scrolled past (textHeight - clipHeight).
// (No live caller in the release -- the roll loops by the cancel bind; kept for parity.)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCreditsUI::IsComplete()
{
	return float( pText->GetSize().y - pClip->GetSize().y ) < fScroll;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw @0x1cbd20 -- advance the scroll by the real-time delta and slide the text up.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreditsUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( sLastTime == 0 )
		sLastTime = sTime;
	fScroll += float( sTime - sLastTime ) * F_CREDITS_SCROLL_SPEED;
	pText->SetPosition( SPoint( 0, -Float2Int( fScroll ) ) );
	sLastTime = sTime;

	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCredits  --  the credits interface (3D backdrop + scrolling text).  saveload id 0xB3410170.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCredits: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CCredits);
private:
	NInput::CBind bindClose;                  // "cancel" -> leave the credits

	// Runtime-only: the release owns the UI via CMissionBase::PushDesktop; this tree keeps it as a member
	// (parented to GetInterface(), which renders it) and does NOT serialize it (matches CCredits::operator&).
	CObj<NUI::CCreditsUI> pCreditsUI;

	ZDATA_(CRenderBaseInterface)
	bool bFinalCredits;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&bFinalCredits); return 0; }

public:
	CCredits();

	void Initialize( bool bFinalCredits );    // @0x1cc5e0

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );   // @0x1cbdc0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCredits::CCredits():
	bindClose( "cancel" ), bFinalCredits( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCredits::Initialize( bool _bFinalCredits )
{
	bFinalCredits = _bFinalCredits;
	CRenderBaseInterface::Initialize( N_CREDITS_TEMPLATE );   // builds the 3D world (reuses the main-menu template)

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_CREDITS_CAMERA );
	ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	GetCamera()->SetPlacement( sCameraPos );

	pCreditsUI = new NUI::CCreditsUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "", NUI::STYLE_VISIBLE | NUI::STYLE_ENABLED ), bFinalCredits );
	NUI::LoadTemplate( pCreditsUI, NDb::GetUIContainer( N_CREDITS_CONTAINER ) );
	pCreditsUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Step -- render the 3D backdrop full-screen (the release relies on CMissionBase::Step; this tree
// reproduces the CMainMenuInterface::Step pattern: the dev CRenderBaseInterface::RenderFrame gates
// pScene->Draw on a non-zero camera screen-rect, so set it 0,0,1,1 each rendered frame).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCredits::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );
		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ProcessEvent @0x1cbdc0 -- the cancel bind leaves the credits: final credits replace the stack
// (so rebuild the main menu), the from-menu roll was pushed (so pop back to the menu).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCredits::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		if ( bFinalCredits )
			NMainLoop::Command( new CICMainMenu() );
		else
			NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICCreditsScreen
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICCreditsScreen::Exec()
{
	CCredits *pRes = new CCredits;
	pRes->Initialize( bFinalCredits );
	if ( bFinalCredits )
		SetInterface( pRes );      // final credits REPLACE the stack
	else
		PushInterface( pRes );     // from-the-menu roll is PUSHED on top
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CmdCreditsScreen -- the "credits [final]" console command handler (@0x1cbf30).
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CmdCreditsScreen( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	csSystem << "Credits..." << endl;
	bool bFinal = ( !paramsSet.empty() && paramsSet[0] == L"final" );
	NMainLoop::Command( new NGame::CICCreditsScreen( bFinal ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3410171, CCreditsUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3410170, CCredits );
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iCreditsScreen)
	REGISTER_CMD( "credits", CmdCreditsScreen )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
