#include "StdAfx.h"
#include "iLoading.h"
#include "..\DBFormat\DataFormat.h"   // NDb::GetUITexture / NDb::GetUIContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed release module iLoading.obj:
//   NUI::CLoadingUI (a CWindow splash/progress window) + the NGame loading-screen lifecycle free fns
//   over three file-scope UI globals (pLoadingUI @0x9c6574 / pInterface @0x9c6570 / pCursor @0x9c656c).
// All cross-subsystem reaches bind directly to the real engine types (no pfn hook structs).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadingUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// ctor @0x1f1eb0 -- CWindow base from the SWindowInfo, then the added members.  nImageID = -1 (no
// splash yet); pBackground/pProgress default-construct to null.  nProgress is intentionally NOT written
// (the release ctor leaves it; SetProgress is what fills it).
CLoadingUI::CLoadingUI( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
	nImageID = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetImage @0x1f1e50 -- adopt a live UITexture record's id as the splash id (dead-bit gate ==
// IsValid; record id == CDBRecord::GetRecordID, the +0xc field the disasm reads).
void CLoadingUI::SetImage( NDb::CUITexture *pTexture )
{
	if ( IsValid( pTexture ) )
		nImageID = pTexture->GetRecordID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetProgress @0x1f1d40 -- record the percent and map it across the clip's interior frames:
//   frame = (GetFrameCount()-2) * nProgress / 100 + 2   (first/last frame are the 0%/100% end-caps;
//   signed integer divide truncates toward zero).  Retail dereferences pProgress unconditionally
//   (operator new never returns null); the IsValid guard is a harmless safety net.
void CLoadingUI::SetProgress( int nValue )
{
	nProgress = nValue;
	if ( IsValid( pProgress ) )
	{
		int nCount = pProgress->GetFrameCount();
		pProgress->SetFrame( ( nCount - 2 ) * nProgress / 100 + 2 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw @0x1f1d90 -- look up the splash texture by nImageID, blit it into the background image with an
// all-zero rect, then the base CWindow::Draw recurse.  (The decompiler attributed the rect pointer to
// GetUITexture; the disasm passes nImageID (mov ecx,[esi+0x80]) and the separate zeroed rect to
// CImage::SetImage -- confirmed by disasm @0x5f1d99..0x5f1dc2.)
void CLoadingUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	NDb::CUITexture *pTexture = NDb::GetUITexture( nImageID );
	if ( IsValid( pBackground ) )
		pBackground->SetImage( pTexture, SRect( 0, 0, 0, 0 ) );
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ProcessMessage @0x1f2190 -- on EVENT_TEMPLATELOAD build the progress clip from the loader's "video"
// control and start it; on EVENT_TEMPLATELOADCOMPLETE bind the "background" splash child.  Always
// chains to CWindow::ProcessMessage and returns its result.
bool CLoadingUI::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOAD )
	{
		pProgress = new CVideoPlayer( sEvent.pLoader->GetControl( "video" ) );
		pProgress->Set( "./res/video/loading.bik", 8 );   // release-faithful raw play flags (disasm push 8)
		pProgress->Play( false );
	}
	else if ( sEvent.nEvent == EVENT_TEMPLATELOADCOMPLETE )
	{
		pBackground = GetUIWindow<CImage>( this, "background" );
	}
	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// The three loading-screen UI globals (release file-scope statics @0x9c6574 / 0x9c6570 / 0x9c656c)
// plus the ShowLoadingScreen throttle stamp.  Held as the real engine smart pointers, so clearing a
// slot runs the engine's AddRef-new / Release-old discipline (CObj -> ReleaseObj, CPtr -> ReleaseRef).
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<NUI::CLoadingUI> pLoadingUI;   // @0x9c6574
static CObj<NUI::CInterface> pInterface;   // @0x9c6570
static CPtr<NUI::ICursor>    pCursor;      // @0x9c656c
static DWORD dwPrevLoadingScreen = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
// InitLoadingScreen @0x1f1ef0 -- create the empty-hotspot cursor, the loading interface, and the
// loading window (CLoadingUI, style VISIBLE|ENABLED == 6, sized to the interface), load its template
// (UI container 0x1a3), show it, and point the splash at the default texture (0x373).  The three
// global swaps reproduce the disasm's AddRef-new / Release-old sequences via CObj/CPtr assignment.
// (Immediates recovered from disasm @0x5f1f30..0x5f210c, lost by the decompiler as register noise.)
void InitLoadingScreen()
{
	pCursor    = NUI::ICursor::Create( false, CVec2( -1, -1 ) );
	pInterface = new NUI::CInterface( pCursor, 0 );
	pLoadingUI = new NUI::CLoadingUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), pInterface->GetSize(), "loading", NUI::STYLE_VISIBLE | NUI::STYLE_ENABLED ) );

	NUI::LoadTemplate( pLoadingUI, NDb::GetUIContainer( 0x1a3 ) );
	pLoadingUI->ShowWindow( NUI::SWTYPE_SHOW );
	pLoadingUI->SetImage( NDb::GetUITexture( 0x373 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// TermLoadingScreen @0x1f2130 -- release the three globals (CObj/CPtr clear == capture/clear/release).
void TermLoadingScreen()
{
	pLoadingUI = 0;
	pInterface = 0;
	pCursor    = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetLoadingImage @0x1f1e70 -- point the splash at pTexture if it is a live record, else fall back to
// the default loading texture (0x373).  Both writes route through CLoadingUI::SetImage (which re-applies
// the live-record gate), exactly reproducing the release's two-branch behaviour.
void SetLoadingImage( NDb::CUITexture *pTexture )
{
	if ( IsValid( pLoadingUI ) )
	{
		if ( IsValid( pTexture ) )
			pLoadingUI->SetImage( pTexture );
		else
			pLoadingUI->SetImage( NDb::GetUITexture( 0x373 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowLoadingScreen @0x1f1de0 -- throttle to ~50ms (abs tick delta > 0x31), then bump the DG frame,
// push the progress to the loading window, step + draw the interface (time 0), and flip.
void ShowLoadingScreen( int nProgress )
{
	DWORD dwNow = GetTickCount();
	int nDelta = (int)( dwNow - dwPrevLoadingScreen );
	if ( nDelta < 0 )
		nDelta = -nDelta;
	if ( nDelta > 0x31 )
	{
		MarkNewDGFrame();
		if ( IsValid( pLoadingUI ) )
			pLoadingUI->SetProgress( nProgress );
		dwPrevLoadingScreen = dwNow;
		if ( IsValid( pInterface ) )
		{
			pInterface->Step( STime( 0 ) );
			pInterface->Draw( STime( 0 ) );
		}
		NGScene::Flip();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
