#include "StdAfx.h"
#include "G2DView.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "Sound.h"   // NSound::CreateSoundScene (retail CInterface own-scene fallback @0x31dbd0)
#include "UIWrap.h"
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "Console.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// UITexture 292 = "Normal" (the arrow cursor). The predecessor's 202 is UITexture "HitLocation - Head"
// (the attack_head body-part cursor) -> the default cursor showed the body silhouette everywhere. The release
// reaches the normal cursor via GetUICursor(2), whose UITexture is 292; we keep the GetUITexture path + the
// correct id. (A fuller GetUICursor migration -- UICursor carries the hotspot center -- is a separate leg.)
const int N_DEFAULT_CURSOR = 292;
////////////////////////////////////////////////////////////////////////////////////////////////////
wstring GetDBString( int nID )
{
	if ( nID == -1 )
		return L"";

	CPtr<NDb::CString> pString = NDb::GetString( nID );
	if ( !IsValid( pString ) )
		return L"";

	return pString->szStr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
wstring GetDBString( NDb::CString *pString )
{
	if ( !IsValid( pString ) )
		return L"";

	return pString->szStr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMouseCaptureHandler
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMouseCaptureHandler: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CMouseCaptureHandler)
private:
	ZDATA
	bool bMouseCover;
	CPtr<CWindow> pWindow;
	CPtr<CInterface> pInterface;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bMouseCover); f.Add(3,&pWindow); f.Add(4,&pInterface); return 0; }

public:
	CMouseCaptureHandler() {}
	CMouseCaptureHandler( CInterface *pInterface, CWindow *pWindow );
	~CMouseCaptureHandler();

	CWindow* GetWindow() const;

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMouseCaptureHandler::CMouseCaptureHandler( CInterface *_pInterface, CWindow *_pWindow ): 
	pInterface( _pInterface ), pWindow( _pWindow ), bMouseCover( true )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMouseCaptureHandler::~CMouseCaptureHandler()
{
	if ( IsValid( pInterface ) && IsValid( pWindow ) )
		pWindow->ProcessMessage( SEvent( EVENT_MOUSECAPTURELOSE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CMouseCaptureHandler::GetWindow() const
{
	return pWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMouseCaptureHandler::ProcessMessage( const SEvent &sEvent )
{
	ASSERT( IsValid( pWindow ) );

	switch( sEvent.nEvent )
	{
		case EVENT_MOUSEMOVE:
		{
			if ( pWindow->HitTest( sEvent.nX, sEvent.nY ) )
			{
				if ( !bMouseCover )
				{
					bMouseCover = true;
					pWindow->ProcessMessage( SEvent( EVENT_MOUSEENTER, sEvent.nX, sEvent.nY ) );
				}
			}
			else
			{
				if ( bMouseCover )
				{
					bMouseCover = false;
					pWindow->ProcessMessage( SEvent( EVENT_MOUSEEXIT, sEvent.nX, sEvent.nY ) );
				}
			}
			pWindow->ProcessMessage( sEvent );
			break;
		}
		case EVENT_LBUTTONUP:
		case EVENT_LBUTTONDOWN:
		case EVENT_LBUTTONDBLCLK:
		case EVENT_RBUTTONUP:
		case EVENT_RBUTTONDOWN:
		case EVENT_RBUTTONDBLCLK:
			if ( pWindow->ProcessMessage( sEvent ) )
				return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SControlsSort
{
	bool operator()( NDb::CUIControl* pC1, NDb::CUIControl* pC2 ) const 
	{ 
		return ( pC1->nDepth > pC2->nDepth ); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoader::Load( CWindow* _pParent, NDb::CUIContainer *pTemplate )
{
	pParent = _pParent;

	if ( IsValid( pTemplate ) )
	{
		pParent->SetSize( SPoint( pTemplate->nWidth, pTemplate->nHeight ) );

		vector< CPtr<NDb::CUIControl> > controls( pTemplate->controls );
		sort( controls.begin(), controls.end(), SControlsSort() );

		windowsSet.resize( controls.size() );
		for ( int nTemp = 0; nTemp < controls.size(); nTemp++ )
		{
			CPtr<NDb::CUIControl> pControl = controls[nTemp];

			int nStyle = STYLE_ENABLED;
			if ( pControl->bVisible )
				nStyle |= STYLE_VISIBLE;
			if ( pControl->bTransparent )
				nStyle |= STYLE_TRANSPARENT;
			if ( pControl->bTopmost && !pControl->bBottommost )
				nStyle |= STYLE_TOPMOST;
			if ( pControl->bBottommost && !pControl->bTopmost )
				nStyle |= STYLE_BOTTOMMOST;

			SWindow sWindow;
			sWindow.sInfo = SWindowInfo( pParent, SPoint( pControl->rect.x1, pControl->rect.y1 ), SPoint( pControl->rect.Width(), pControl->rect.Height() ), pControl->szID, nStyle );
			sWindow.pControl = pControl;
			windowsSet[nTemp] = TTemplateWindow( 0, sWindow );
		}
	}

	pParent->ProcessMessage( SEvent( EVENT_TEMPLATELOAD, this, pTemplate ) );

	list<CPtr<CWindow> > childrenList;
	pParent->GetChildrenList( &childrenList );

	CPtr<CWindow> pDefaultWindow;
	for ( int nTemp = 0; nTemp < windowsSet.size(); nTemp++ )
	{
		for( list<CPtr<CWindow> >::iterator iTemp = childrenList.begin(); iTemp != childrenList.end(); iTemp++ )
		{
			if ( windowsSet[nTemp].second.sInfo.szID.empty() )
				continue;
			if ( (*iTemp)->GetWindowID() != windowsSet[nTemp].second.sInfo.szID )
				continue;

			windowsSet[nTemp].first = (*iTemp);
			break;
		}

		CPtr<CWindow> pWindow = windowsSet[nTemp].first;
		const SWindowInfo &sInfo = windowsSet[nTemp].second.sInfo;
		CPtr<NDb::CUIControl> pControl = windowsSet[nTemp].second.pControl;

		if ( !IsValid( pWindow ) )
		{
			switch ( pControl->type )
			{
			case NDb::UI_WINDOW:
				pWindow = new CWindow( sInfo );
				break;
			case NDb::UI_CONTAINER:
				pWindow = new CWindow( sInfo );
				break;
			case NDb::UI_TEXT:
				pWindow = new CText( sInfo );
				break;
			case NDb::UI_IMAGE:
				pWindow = new CImage( sInfo );
				break;
			case NDb::UI_EDIT:
				pWindow = new CEdit( sInfo );
				break;
			case NDb::UI_BUTTON:
				pWindow = new CButton( sInfo );
				break;
			case NDb::UI_PUSHBUTTON:
				pWindow = new CPushButton( sInfo );
				break;
			case NDb::UI_CHECKBUTTON:
				pWindow = new CCheckButton( sInfo );
				break;
			case NDb::UI_RADIOBUTTON:
				ASSERT( 0 );
				pWindow = new CPushButton( sInfo );
				break;
			case NDb::UI_SLIDER:
				pWindow = new CSlider( sInfo );
				break;
			case NDb::UI_SCROLL:
				pWindow = new CScroll( sInfo );
				break;
			case NDb::UI_IMAGELIST:
				pWindow = new CListView( sInfo );
				break;
			case NDb::UI_COMBOBOX:
				pWindow = new CComboBox( sInfo );
				break;
			case NDb::UI_PROGRESSBAR:
				pWindow = new CProgressBar( sInfo );
				break;
			default:
				ASSERT( 0 );
				pWindow = new CWindow( sInfo );
				break;
			}

			windowsSet[nTemp].first = pWindow;
		}

		if ( IsValid( pControl ) && pControl->bDefault )
			pDefaultWindow = pWindow;
	}

	for ( int nTemp = 0; nTemp < windowsSet.size(); nTemp++ )
	{
		CPtr<CWindow> pWindow = windowsSet[nTemp].first;
		CPtr<NDb::CUIControl> pControl = windowsSet[nTemp].second.pControl;

		CPtr<NDb::CUIContainer> pNestedTemplate;
		if ( !IsValid( pControl ) )
		{
			pControl = new NDb::CUIControl;
			pNestedTemplate = 0;
		}
		else
			pNestedTemplate = pControl->pNestedUIContainer;

		pParent->SendMessage( pWindow, SEvent( EVENT_TEMPLATECREATE, this, pControl ) );

		NUI::LoadTemplate( pWindow, pNestedTemplate );
	}

	pParent->ProcessMessage( SEvent( EVENT_TEMPLATELOADCOMPLETE, this, pTemplate ) );

	if ( IsValid( pDefaultWindow ) && pDefaultWindow->GetStyle( STYLE_VISIBLE ) )
		pDefaultWindow->ShowWindow( SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SWindowInfo& CLoader::GetControl( const string &szID )
{
	for ( int nTemp = 0; nTemp < windowsSet.size(); nTemp++ )
	{
		if ( windowsSet[nTemp].second.sInfo.szID != szID )
			continue;

		return windowsSet[nTemp].second.sInfo;
	}

	csSystem << "UI-ERROR: UI Container not complete, control " << szID << " in container " << pParent->GetWindowID() << " not found" << endl;
	TTemplateWindow &sWindow = *windowsSet.insert( windowsSet.end(), TTemplateWindow());
	sWindow.second.sInfo = SWindowInfo( pParent, SPoint( 0, 0 ) , SPoint( 0, 0 ), szID, 0 );
	return sWindow.second.sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadTemplate( CWindow *pWindow, NDb::CUIContainer *pTemplate )
{
	CObj<CLoader> pLoader = new CLoader;
	pLoader->Load( pWindow, pTemplate );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
CInterface::CInterface(): 
	cmdLButtonDown( "leftbutton_down" ), cmdLButtonUp( "leftbutton_up" ), cmdRButtonDown( "rightbutton_down" ), cmdRButtonUp( "rightbutton_up" ), 
	cmdConsole( "console" ), cmdFPSShow( "showfps" ), bindScroll( "scroll" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CInterface::CInterface( ICursor* _pCursor, NSound::ISoundScene *_pSound ): 
	CWindow( SWindowInfo( 0, SPoint( 0, 0 ), SPoint( 1024, 768 ), "desktop", STYLE_VISIBLE | STYLE_ENABLED ) ), bShowFPSStats( false ), 
	cmdLButtonDown( "leftbutton_down" ), cmdLButtonUp( "leftbutton_up" ), cmdRButtonDown( "rightbutton_down" ), cmdRButtonUp( "rightbutton_up" ), 
	cmdConsole( "console" ), cmdFPSShow( "showfps" ), bindScroll( "scroll" ),
	sDoubleClickTime( 0 ), sLastLButtonDownTime( 0 ), sLastRButtonDownTime( 0 ), sCursorPoint( 0, 0 ), sLastTime( 0 )
{
	SetInterface( this );

	pTimer = sTimer.GetTime();
	pView = NGScene::CreateNew2DView();

	// retail CInterface ctor @0x31dbd0 (decomp-verified): when NO sound scene is handed in (or it is
	// dead), the interface CREATES ITS OWN (bOwnSoundScene=true -> NSound::CreateSoundScene) so
	// CWindow::PlaySound (@0x327380: pInterface->GetSound()->Add2DSound) always has a live target.
	// The dev ctor left pSound null for every pure-menu screen (TeamMng recruit click, menu button
	// sounds ...) -> CWindow::PlaySound silently dropped every sample there, while screens hosted on
	// the mission/render-base interfaces (FaceGen) played fine. Add2DSound starts the buffer
	// immediately (Sound.cpp: NFMSound::PlaySound inside the call), so an own 2D-only scene needs no
	// per-frame pump; the CPtr releases it at interface teardown.
	pSound = _pSound;
	if ( !IsValid( pSound ) )
		pSound = NSound::CreateSoundScene( 0 );
	pCursor = _pCursor;
	pConsole = new CConsole( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "console", STYLE_ENABLED | STYLE_TOPMOST ) );
	NUI::LoadTemplate( pConsole, NDb::GetUIContainer( 42 ) );
	sDoubleClickTime = GetDoubleClickTime();

	sDefaultCursor = SCursorInfo( NDb::GetUITexture( N_DEFAULT_CURSOR ) );

	pFPSText = new CTextDraw( SPoint( 0, 32 ), SPoint( 1024, 768 ), L"Counting..." );
	pNonPublicDemo = new CTextDraw( SPoint( 0, 32 ), SPoint( 1024, 768 ), L"<font size=18pt face=Courier><right>Work in progress" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CInterface::GetCursorPos() const
{
	return sCursorPoint;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SCursorInfo& CInterface::GetCursorInfo() const
{
	return pCursor->GetCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SCursorInfo& CInterface::GetDefaultCursorInfo() const
{
	return sDefaultCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::SetCursorInfo( const SCursorInfo &sInfo )
{
	if ( sInfo.pTexture )
		sCursor = sInfo;
	else
		sCursor = sDefaultCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::SetToolTipOwner( CWindow *pOwner )
{
	if ( pToolTipOwner == pOwner )
		return;

	if ( IsValid( pToolTip ) )
		pToolTip->SetStyle( STYLE_VISIBLE, false );

	pToolTip = 0;
	pToolTipOwner = pOwner;
	if ( IsValid( pToolTipOwner ) )
	{
		pToolTip = pToolTipOwner->GetToolTip();
		if ( IsValid( pToolTip ) )
		{
			SRect sWindow;
			SPoint sPosition;
			pOwner->ClientToScreen( &sPosition, &sWindow );

			pToolTip->SetStyle( STYLE_VISIBLE, true );
			pToolTip->SetPosition( SPoint( sCursorPoint.x , sPosition.y ) );
		}

//	DebugTrace( "%s\n", pToolTipOwner->GetWindowID().c_str() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CInterface::CreateMouseCapture( CWindow *pWindow )
{
	pMouseCapture = new CMouseCaptureHandler( this, pWindow );
	return pMouseCapture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NSound::ISoundScene* CInterface::GetSound()
{
	return pSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::I2DGameView* CInterface::GetView()
{
	return pView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterface::ProcessEvent( const NInput::SEvent &eEvent )
{
	int nVirtualKey;
	bool bRet = false;

	if ( cmdConsole.ProcessEvent( eEvent ) )
	{
		pConsole->SetConsoleState( !pConsole->GetStyle( NUI::STYLE_VISIBLE ) );
		return true;
	}
	if ( pConsole->ProcessEvent( eEvent ) )
		return true;

	SPoint sPoint( pCursor->GetPos().x * 1024 / pView->GetViewportSize().x, pCursor->GetPos().y * 768 / pView->GetViewportSize().y  );
	if ( cmdLButtonUp.ProcessEvent( eEvent ) )
	{
		bRet |= ProcessMessage( SEvent( EVENT_LBUTTONUP, sPoint.x, sPoint.y ) );

		if ( GetTickCount() - sLastLButtonDownTime < sDoubleClickTime )
			bRet |= ProcessMessage( SEvent( EVENT_LBUTTONDBLCLK, sPoint.x, sPoint.y ) );

		sLastLButtonDownTime = GetTickCount();
	}
	if ( cmdLButtonDown.ProcessEvent( eEvent ) )
		bRet |= ProcessMessage( SEvent( EVENT_LBUTTONDOWN, sPoint.x, sPoint.y ) );
	if ( cmdRButtonUp.ProcessEvent( eEvent ) )
		bRet |= ProcessMessage( SEvent( EVENT_RBUTTONUP, sPoint.x, sPoint.y ) );
	if ( cmdRButtonDown.ProcessEvent( eEvent ) )
		bRet |= ProcessMessage( SEvent( EVENT_RBUTTONDOWN, sPoint.x, sPoint.y ) );
	if ( bindScroll.ProcessEvent( eEvent ) )
		bRet |= ProcessMessage( SEvent( EVENT_SCROLL, sPoint.x, sPoint.y, -bindScroll.GetDelta() ) );
	if ( NInput::GetKeyForMessage( eEvent.mMessage, &nVirtualKey ) )
		bRet |= ProcessMessage( SEvent( EVENT_CHAR, nVirtualKey ) );

	if ( cmdFPSShow.ProcessEvent( eEvent ) )
	{
		bShowFPSStats = !bShowFPSStats;
		bRet = true;
	}

	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterface::ProcessMessage( const SEvent &sEvent )
{
	if ( IsValid( pMouseCapture ) )
		if ( pMouseCapture->ProcessMessage( sEvent ) )
			return true;

	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			NInput::PostEvent( sEvent.szID );
			return true;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::Step( const STime &sTime )
{
	sLastTime = sTime;		// keep the UI ms clock current (read by NScript::luaGetUITime)

	SPoint sPoint( pCursor->GetPos().x * 1024 / pView->GetViewportSize().x, pCursor->GetPos().y * 768 / pView->GetViewportSize().y  );
	sCursorPoint = sPoint;

	sCursor = sDefaultCursor;
	ProcessMessage( SEvent( EVENT_MOUSEMOVE, sPoint.x, sPoint.y ) );
	pCursor->SetCursor( sCursor );

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::UpdateCursor()
{
	pCursor->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::Draw( const STime &sTime )
{
	pView->StartNewFrame();
	CWindow::Draw( sTime, pView );

	pNonPublicDemo->Draw( this, sTime, pView );
	if( bShowFPSStats )
		pFPSText->Draw( this, sTime, pView );

	pCursor->Draw( sTime, pView );
	pView->Flush();
	UpdateFPSText();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterface::UpdateFPSText()
{
	if( bShowFPSStats )
	{
		NGScene::SRenderStats sStats;
		NGScene::GetRenderStats( &sStats );

		float fFPS = 1 / sStats.fFrameTime;
		WCHAR wszBuf[1024];
		if ( sStats.bGeometryThrashing )
			swprintf( wszBuf, L"\n\n<font face=Courier size=16><left>FPS = %4.1f\n<color=red>Scene tris = %d<color=white>\nVertices = %d\nTris = %d\nParticles = %d(%d)\nTexMem = %.1f MB", 
				fFPS, sStats.nSceneTris, sStats.nVertices, sStats.nTris, sStats.nParticles, sStats.nLitParticles, NGScene::CalcTouchedTextureSize() / 1000000.0f );
		else
			swprintf( wszBuf, L"\n\n<font face=Courier size=16><left>FPS = %4.1f\nScene tris = %d\nVertices = %d\nTris = %d\nParticles = %d(%d)\nTexMem = %.1f MB", 
				fFPS, sStats.nSceneTris, sStats.nVertices, sStats.nTris, sStats.nParticles, sStats.nLitParticles, NGScene::CalcTouchedTextureSize() / 1000000.0f );
		if ( sStats.b2DTexturesThrashing )
			wcscat( wszBuf, L"\n<color=red>2D texture cache thrashing<color=white>" );
		if ( sStats.bTransparentThrashing )
			wcscat( wszBuf, L"\n<color=red>transparent texture cache thrashing<color=white>" );
		if ( sStats.bStaticShadowDepthRendered )
			wcscat( wszBuf, L"\n<color=red>static shadow depth recalc<color=white>" );
		if ( sStats.bLightmapThrashing )
			wcscat( wszBuf, L"\n<color=red>lightmap buffer is thrashing<color=white>" );
 		pFPSText->SetText( wszBuf );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB2841122, CMouseCaptureHandler );
REGISTER_SAVELOAD_CLASS( 0xB2841123, CInterface );
