#include "StdAfx.h"
#include "Gfx.h"
#include "GInit.h"
#include "GAutoDetect.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\MiscDll\Commands.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iSaveManager.h"
#include "iOptionsMenu.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_SLIDER_STEPS = 100;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Config <-> checkbox helpers (the release free fns @0x2299d0 / @0x229a50).  Shared by the gameplay
// and controls option screens: write a checkbox's state into a config var (1/0) and read it back.
// Guarded against a missing control (our 32MB game.db may not ship every retail checkbox).
////////////////////////////////////////////////////////////////////////////////////////////////////
// WIDESCREEN: combo item id packs both dimensions -- Sentinels @0x4eba9f ((w&0xfff)<<12)|(h&0xfff).
static int EncodeVideoModeID( int nW, int nH )
{
	return ( ( nW & 0xfff ) << 12 ) | ( nH & 0xfff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateConfig( CCheckButton *pButton, const string &szVar )
{
	if ( IsValid( pButton ) )
		NGlobal::SetVar( szVar, NGlobal::CValue( pButton->IsChecked() ? 1.0f : 0.0f ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateUIElement( CCheckButton *pButton, const string &szVar )
{
	// v1.2 @0x229a50: integer truth test via the new CValue::GetInt (FISTP round-to-nearest) --
	// any var value rounding to a nonzero int now checks the box (was GetFloat() == 1.0f exactly).
	if ( IsValid( pButton ) )
		pButton->SetChecked( NGlobal::GetVar( szVar ).GetInt() != 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexSlider  --  the INNER slider widget (the retail "slider" control inside a CComplexScroll).
// Reshaped to match the release (.\release\iOptionsMenu.obj): now derives CSlider (was the dev's
// CComplexTextSlider:CSlider) and merges the progress bar + the per-frame text update that the dev did
// in the outer widget's UpdateControls. reg id 0xB0815155 (the release's CComplexSlider id).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexSlider: public CSlider
{
	OBJECT_BASIC_METHODS(CComplexSlider)
private:
	ZDATA_(CSlider)
	CPtr<CText> pText;
	CPtr<CProgressBar> pProgress;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CSlider*)this); f.Add(2,&pText); f.Add(3,&pProgress); return 0; }

public:
	CComplexSlider() {}
	CComplexSlider( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // @0x221870 (release-added)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CComplexSlider::CComplexSlider( const SWindowInfo &sInfo ):
	CSlider( sInfo )
{
	SetStyle( 0x100, true );   // the higher STYLE flag the release SetStyle(0x100,true) sets (above the named enum)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComplexSlider::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		pText = new CText( sEvent.pLoader->GetControl( "slider" ) );
		break;
	case EVENT_TEMPLATELOADCOMPLETE:
		pProgress = GetUIWindow<CProgressBar>( this, "progress" );
		break;
	}

	return CSlider::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw @0x221870 -- show the value as text + drive the progress bar (the dev's UpdateText + UpdateControls fused).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComplexSlider::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pText ) )
	{
		WCHAR wsBuffer[256];
		swprintf( wsBuffer, L"<font face=Courier size=18pt><center>%d", GetValue() );
		pText->SetText( GetDBString( 4404 ) + wsBuffer );
	}
	if ( IsValid( pProgress ) && GetMaxValue() != 0 )
		pProgress->SetValue( ( float( GetValue() ) / GetMaxValue() ) * 0.9f + 0.1f );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexScroll  --  the OUTER scroll widget (the retail "sensitivity"/"volume" control: a row of
// plus/minus buttons around an inner CComplexSlider).  Reshaped to match the release: derives CScroll
// (was the dev's CComplexSlider:CWindow); the +/- buttons live here, the slider+progress+text moved into
// the inner CComplexSlider.  reg id 0xB0815156 (the release's CComplexScroll id; was dev CComplexTextSlider).
// Keeps a float 0..1 ratio GetValue/SetValue (hiding CScroll's int API) so the option panels need only a
// type rename -- the inner CSlider value maps to/from the ratio via CScroll's int getters.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexScroll: public CScroll
{
	OBJECT_BASIC_METHODS(CComplexScroll)
private:
	ZDATA_(CScroll)
	CObj<CHoverButton> pPlus;
	CObj<CHoverButton> pMinus;
	CObj<CComplexSlider> pSlider;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CScroll*)this); f.Add(2,&pPlus); f.Add(3,&pMinus); f.Add(4,&pSlider); return 0; }

public:
	CComplexScroll() {}
	CComplexScroll( const SWindowInfo &sInfo );

	float GetValue();           // 0..1 ratio (hides CScroll::GetValue int)
	void SetValue( float fVal );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CComplexScroll::CComplexScroll( const SWindowInfo &sInfo ):
	CScroll( sInfo )
{
	SetStyle( 0x100, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CComplexScroll::GetValue()
{
	int nMax = CScroll::GetMaxValue();
	return ( nMax != 0 ) ? float( CScroll::GetValue() ) / nMax : 0.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComplexScroll::SetValue( float fVal )
{
	CScroll::SetValue( Float2Int( CScroll::GetMaxValue() * fVal ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComplexScroll::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			// The inner slider is created as the "slider" child so CScroll::ProcessMessage's
			// GetUIWindow<CSlider>(this,"slider") binds to it (CComplexSlider IS-A CSlider); the +/-
			// buttons are wrapped from this scroll's nested template controls.
			pPlus = new CHoverButton( sEvent.pLoader->GetControl( "plus" ) );
			pMinus = new CHoverButton( sEvent.pLoader->GetControl( "minus" ) );
			pSlider = new CComplexSlider( sEvent.pLoader->GetControl( "slider" ) );

			pPlus->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4418 ) );
			pPlus->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4418 ) );
			pMinus->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4419 ) );
			pMinus->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4419 ) );
			break;
		}
	}

	return CScroll::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexComboBox
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexComboBox: public CComboBox
{
	OBJECT_NOCOPY_METHODS(CComplexComboBox)
private:
	ZDATA_(CComboBox)
	CObj<CHoverButton> pDropDown;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CComboBox*)this); f.Add(2,&pDropDown); return 0; }

public:
	CComplexComboBox() {}
	CComplexComboBox( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CComplexComboBox::CComplexComboBox( const SWindowInfo &sInfo ):
	CComboBox( sInfo )
{
	// retail ctor @0x1c2f10 seeds the four state fonts itself (note DISABLED = 21059, a greyed font)
	SetStateInfo( STATE_NORMAL, SInfo( NUI::GetDBString( 4402 ) ) );
	SetStateInfo( STATE_HILIGHTED, SInfo( NUI::GetDBString( 4401 ) ) );
	SetStateInfo( STATE_SELECTED, SInfo( NUI::GetDBString( 4402 ) ) );
	SetStateInfo( STATE_DISABLED, SInfo( NUI::GetDBString( 21059 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComplexComboBox::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pDropDown = new CHoverButton( sEvent.pLoader->GetControl( "drop_list" ) );

			pDropDown->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4429 ) );
			pDropDown->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4429 ) );
			break;
		}
	}

	return CComboBox::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// COptionsUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionsUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(COptionsUI)
private:
	ZDATA_(CWindow)
	NGame::EOptionsScreen eScreen;
	// The release DROPPED the dev pClose (the close cross moved out to the CEmptyOptionsUI shell, a5dll
	// 2c70b6a) and ADDED a 5th "profiles" tab. operator& tags shift: video/audio are now 3/4, profiles 5.
	CObj<CHoverButton> pVideoOptions;
	CObj<CHoverButton> pAudioOptions;
	CObj<CHoverButton> pProfileOptions;
	CObj<CHoverButton> pGamePlayOptions;
	CObj<CHoverButton> pControlsOptions;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&eScreen); f.Add(3,&pVideoOptions); f.Add(4,&pAudioOptions); f.Add(5,&pProfileOptions); f.Add(6,&pGamePlayOptions); f.Add(7,&pControlsOptions); return 0; }

public:
	COptionsUI() {}
	COptionsUI( const SWindowInfo &sInfo, NGame::EOptionsScreen eScreen = NGame::OS_PROFILE );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
COptionsUI::COptionsUI( const SWindowInfo &sInfo, NGame::EOptionsScreen _eScreen ):
	CWindow( sInfo ), eScreen( _eScreen )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool COptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pVideoOptions = new CHoverButton( sEvent.pLoader->GetControl( "video" ) );
			pAudioOptions = new CHoverButton( sEvent.pLoader->GetControl( "audio" ) );
			pProfileOptions = new CHoverButton( sEvent.pLoader->GetControl( "profiles" ) );
			pGamePlayOptions = new CHoverButton( sEvent.pLoader->GetControl( "gameplay" ) );
			pControlsOptions = new CHoverButton( sEvent.pLoader->GetControl( "controls" ) );

			pVideoOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4382 ) );
			pVideoOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4382 ) );
			pAudioOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4383 ) );
			pAudioOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4383 ) );
			pProfileOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 19824 ) );
			pProfileOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 19824 ) );
			pGamePlayOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4384 ) );
			pGamePlayOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4384 ) );
			pControlsOptions->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4385 ) );
			pControlsOptions->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4385 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			switch( eScreen )
			{
			case NGame::OS_PROFILE:
				GetUIWindow<CImage>( this, "profilesmark" )->SetStyle( STYLE_VISIBLE, true );
				break;
			case NGame::OS_VIDEO:
				GetUIWindow<CImage>( this, "videomark" )->SetStyle( STYLE_VISIBLE, true );
				break;
			case NGame::OS_AUDIO:
				GetUIWindow<CImage>( this, "audiomark" )->SetStyle( STYLE_VISIBLE, true );
				break;
			case NGame::OS_GAMEPLAY:
				GetUIWindow<CImage>( this, "gameplaymark" )->SetStyle( STYLE_VISIBLE, true );
				break;
			case NGame::OS_CONTROLS:
				GetUIWindow<CImage>( this, "controlsmark" )->SetStyle( STYLE_VISIBLE, true );
				break;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEmptyOptionsUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEmptyOptionsUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CEmptyOptionsUI)
private:
	ZDATA_(CWindow)
	SCursorInfo sCursor;
	CObj<COptionsUI> pBase;
	NGame::EOptionsScreen eScreen;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&sCursor); f.Add(3,&pBase); f.Add(4,&pCloseButton); f.Add(5,&eScreen); return 0; }
	CObj<CFlashButton> pCloseButton;

public:
	CEmptyOptionsUI() {}
	CEmptyOptionsUI( const SWindowInfo &sInfo, NGame::EOptionsScreen _eScreen = NGame::OS_PROFILE );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEmptyOptionsUI::CEmptyOptionsUI( const SWindowInfo &sInfo, NGame::EOptionsScreen _eScreen ):
	CWindow( sInfo ), eScreen( _eScreen )
{
	// The same "Normal" arrow the rest of the menu UI uses: UICursors row 2 "normal" (UITexture 292);
	// cf. Interface.cpp / UIInterface.cpp.
	sCursor = SCursorInfo( NDb::GetUICursor( 2 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEmptyOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( sCursor );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pBase = new COptionsUI( sEvent.pLoader->GetControl( "base" ), eScreen );
			// The close cross lives on the OUTER options shell (this container), NOT inside "base":
			// the dev COptionsUI built pClose from GetControl("cancel") on "base", where the retail
			// container has no such control ("control cancel in container base not found") -> dead cross.
			// The release builds it here as a CFlashButton from this shell's "cancel" control (where
			// the retail container actually ships it); its "cancel"-id action fires bindClose -> CICExitModal.
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Resolution <-> config glue (retail @0x220d30/@0x220e30).  Retail's id is width|(gfx_16bit_mode
// <<16); dev uses the widescreen WxH-packed id and the "WxH" string var (16-bit modes not plumbed).
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetCurrentResolution()
{
	int nModeX = 1024, nModeY = 768;
	NGScene::GetConfiguredVideoMode( &nModeX, &nModeY );
	return EncodeVideoModeID( nModeX, nModeY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetCurrentResolution( int nMode )
{
	WCHAR wsMode[64];
	swprintf( wsMode, L"%dx%d", ( nMode >> 12 ) & 0xfff, nMode & 0xfff );
	NGlobal::SetVar( "gfx_resolution", wstring( wsMode ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The shared 4-preset item block every quality combo gets (retail inlines it per combo,
// @0x625d54/@0x626150/@0x626540/@0x626f73) + the hidden CUSTOM row (selectable, never listed).
// bAscending: the speed axis lists best-visuals id 0 first; the other axes best id 3 first.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddQualityItems( CComplexComboBox *pCombo, bool bAscending )
{
	const int nFirst = bAscending ? 0 : 3, nStep = bAscending ? 1 : -1;
	pCombo->AddItem( nFirst, CComplexComboBox::SInfo( NUI::GetDBString( 17267 ) ), 171 );
	pCombo->AddItem( nFirst + nStep, CComplexComboBox::SInfo( NUI::GetDBString( 4412 ) ), 172 );
	pCombo->AddItem( nFirst + 2 * nStep, CComplexComboBox::SInfo( NUI::GetDBString( 17266 ) ), 172 );
	pCombo->AddItem( nFirst + 3 * nStep, CComplexComboBox::SInfo( NUI::GetDBString( 4411 ) ), 173 );
	pCombo->AddHiddenItem( NGScene::CV_CUSTOM, CComplexComboBox::SInfo( NUI::GetDBString( 17423 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVideoOptionsUI -- retail shape (ProcessMessage @0x2254d0, UpdateFromConfig @0x220f60, operator&
// @0x22bee0): six IMMEDIATE-apply combos + gamma scroll + 2 checkboxes; no apply button.
// NOTE the control names are retail's own: "smoothness" = FSAA, "fsaa_level" = lighting quality.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVideoOptionsUI: public CEmptyOptionsUI
{
	OBJECT_NOCOPY_METHODS(CVideoOptionsUI)
private:
	ZDATA_(CEmptyOptionsUI)
	bool bIgnoreNotify;
	CObj<CHoverButton> pDefault;
	CPtr<CCheckButton> pHWCursor;
	CPtr<CCheckButton> pShowCrown;
	CObj<CComplexScroll> pGammaScroll;
	CObj<CComplexComboBox> pQuality;
	CObj<CComplexComboBox> pFSAALevel;
	CObj<CComplexComboBox> pLightingQuality;
	CObj<CComplexComboBox> pResolution;
	CObj<CComplexComboBox> pTextureQuality;
	CObj<CComplexComboBox> pAnisotropicLevel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CEmptyOptionsUI*)this); f.Add(2,&bIgnoreNotify); f.Add(3,&pDefault); f.Add(4,&pHWCursor); f.Add(5,&pShowCrown); f.Add(6,&pGammaScroll); f.Add(7,&pQuality); f.Add(8,&pFSAALevel); f.Add(9,&pLightingQuality); f.Add(10,&pResolution); f.Add(11,&pTextureQuality); f.Add(12,&pAnisotropicLevel); return 0; }

	void UpdateFromConfig();

public:
	CVideoOptionsUI(): bIgnoreNotify( false ) {}
	CVideoOptionsUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVideoOptionsUI::CVideoOptionsUI( const SWindowInfo &sInfo ):
	CEmptyOptionsUI( sInfo, NGame::OS_VIDEO ), bIgnoreNotify( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x220f60: push the live config back into every control (guarded against notify feedback)
void CVideoOptionsUI::UpdateFromConfig()
{
	bIgnoreNotify = true;
	UpdateUIElement( pHWCursor, "ui_hwcursor" );
	UpdateUIElement( pShowCrown, "gfx_particles" );
	if ( IsValid( pGammaScroll ) )
	{
		// the INT scroll API (retail CScroll::GetValue/SetValue): 0..100 <-> gamma 0..2 (x50 / x0.02)
		pGammaScroll->CScroll::SetMaxValue( 100 );
		pGammaScroll->CScroll::SetValue( Float2Int( NGlobal::GetVar( "gfx_gamma", 1024 ).GetFloat() * 50.0f ) );
	}
	if ( IsValid( pQuality ) )
		pQuality->SetSelectedItem( NGScene::GetSpeedMode() );
	if ( IsValid( pFSAALevel ) )
		pFSAALevel->SetSelectedItem( NGScene::GetFSAAMode() );
	if ( IsValid( pTextureQuality ) )
		pTextureQuality->SetSelectedItem( NGScene::GetTextureMode() );
	if ( IsValid( pLightingQuality ) )
		pLightingQuality->SetSelectedItem( NGScene::GetLightingQualityMode() );
	if ( IsValid( pAnisotropicLevel ) )
		pAnisotropicLevel->SetSelectedItem( Float2Int( NGlobal::GetVar( "gfx_anisotropic_filter", 1 ).GetFloat() ) );
	if ( IsValid( pResolution ) )
		pResolution->SetSelectedItem( GetCurrentResolution() );
	if ( IsValid( pQuality ) )
		pQuality->SetStyle( STYLE_ENABLED, true );
	if ( IsValid( pLightingQuality ) )
		pLightingQuality->SetStyle( STYLE_ENABLED, true );
	// retail @0x62117d: 16-bit mode pins quality to fastest; lighting quality is only offered
	// on the two best speed presets
	bool bDisableLighting = false;
	if ( NGfx::Is16BitMode() )
	{
		if ( IsValid( pQuality ) )
		{
			pQuality->SetStyle( STYLE_ENABLED, false );
			pQuality->SetSelectedItem( 3 );
		}
		bDisableLighting = true;
	}
	else if ( NGScene::GetSpeedMode() > 1 )
		bDisableLighting = true;
	if ( bDisableLighting && IsValid( pLightingQuality ) )
	{
		pLightingQuality->SetStyle( STYLE_ENABLED, false );
		pLightingQuality->SetSelectedItem( 0 );
	}
	bIgnoreNotify = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVideoOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			// retail @0x625520: swallow the notifies our own SetSelectedItem calls emit
			if ( bIgnoreNotify )
				return true;
			bIgnoreNotify = true;
			bool bChanged = false;
			if ( sEvent.szID == "default" )
			{
				NGlobal::ResetVar( "gfx_gamma" );
				NGlobal::ResetVar( "ui_hwcursor" );
				NGlobal::ResetVar( "gfx_particles" );
				// retail @0x62560d also runs NGScene::AutoDetectVideoConfig() -- deferred (GAutoDetect.h)
				UpdateFromConfig();
				bChanged = true;   // retail recreates unconditionally on default
			}
			else if ( IsValid( pResolution ) && sEvent.szID == pResolution->GetWindowID() )
			{
				bChanged = pResolution->GetSelectedItem() != GetCurrentResolution();
				SetCurrentResolution( pResolution->GetSelectedItem() );
			}
			else if ( IsValid( pQuality ) && sEvent.szID == pQuality->GetWindowID() )
			{
				bChanged = pQuality->GetSelectedItem() != NGScene::GetSpeedMode();
				NGScene::SetSpeedMode( (NGScene::EConfigValue)pQuality->GetSelectedItem() );
			}
			else if ( IsValid( pTextureQuality ) && sEvent.szID == pTextureQuality->GetWindowID() )
			{
				bChanged = pTextureQuality->GetSelectedItem() != NGScene::GetTextureMode();
				NGScene::SetTextureMode( (NGScene::EConfigValue)pTextureQuality->GetSelectedItem() );
			}
			else if ( IsValid( pFSAALevel ) && sEvent.szID == pFSAALevel->GetWindowID() )
			{
				bChanged = pFSAALevel->GetSelectedItem() != NGScene::GetFSAAMode();
				NGScene::SetFSAAMode( (NGScene::EConfigValue)pFSAALevel->GetSelectedItem() );
			}
			else
			{
				// any other control (checkboxes/gamma/lighting/aniso) writes ALL its vars -- retail @0x625799
				UpdateConfig( pHWCursor, "ui_hwcursor" );
				UpdateConfig( pShowCrown, "gfx_particles" );
				if ( IsValid( pLightingQuality ) )
					NGScene::SetLightingQualityMode( (NGScene::EConfigValue)pLightingQuality->GetSelectedItem() );
				if ( IsValid( pGammaScroll ) )
					NGlobal::SetVar( "gfx_gamma", NGlobal::CValue( pGammaScroll->CScroll::GetValue() * 0.02f ) );
				if ( IsValid( pAnisotropicLevel ) )
					NGlobal::SetVar( "gfx_anisotropic_filter", NGlobal::CValue( (float)pAnisotropicLevel->GetSelectedItem() ) );
			}
			if ( bChanged )
				NGlobal::ProcessCommand( L"gfx_recreate" );   // retail @0x62568d: apply IMMEDIATELY
			bIgnoreNotify = false;
			UpdateFromConfig();
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pDefault = new CHoverButton( sEvent.pLoader->GetControl( "default" ) );
			pDefault->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4377 ) );
			pDefault->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4377 ) );
			pGammaScroll = new CComplexScroll( sEvent.pLoader->GetControl( "gamma_slider" ) );

			pQuality = new CComplexComboBox( sEvent.pLoader->GetControl( "quality" ) );
			AddQualityItems( pQuality, true );

			pFSAALevel = new CComplexComboBox( sEvent.pLoader->GetControl( "smoothness" ) );
			AddQualityItems( pFSAALevel, false );

			pTextureQuality = new CComplexComboBox( sEvent.pLoader->GetControl( "texture_quality" ) );
			AddQualityItems( pTextureQuality, false );

			pAnisotropicLevel = new CComplexComboBox( sEvent.pLoader->GetControl( "anisotropic_level" ) );
			pAnisotropicLevel->AddItem( 1, CComplexComboBox::SInfo( NUI::GetDBString( 17434 ) ), 171 );
			// retail @0x6269e0: one row per power-of-two level up to the device cap
			for ( int nLevel = 2; nLevel <= NGfx::GetMaxAnisotropicLevel(); ++nLevel )
			{
				if ( GetNextPow2( nLevel ) != nLevel )
					continue;
				WCHAR wsBuffer[64];
				swprintf( wsBuffer, L"<right>x%d", nLevel );
				pAnisotropicLevel->AddItem( nLevel, CComplexComboBox::SInfo( wsBuffer ), ( nLevel == NGfx::GetMaxAnisotropicLevel() ) ? 173 : 172 );
			}

			pResolution = new CComplexComboBox( sEvent.pLoader->GetControl( "resolution" ) );
			list<NGfx::SVideoMode> modesList;
			NGfx::GetModesList( &modesList );
			// retail @0x626c03 keeps w>=800 AND aspect==4:3, plus an 800..1024 16-bit list
			// ("<right>%dx%dx16", id w|0x10000). WIDESCREEN port: every aspect stays (Sentinels'
			// builder @0x4eba70 has no aspect check either); dev has no 16-bit plumbing.
			for( list<NGfx::SVideoMode>::iterator iTemp = modesList.begin(); iTemp != modesList.end(); )
			{
				if ( iTemp->nXSize < 800 )
					iTemp = modesList.erase( iTemp );
				else
					iTemp++;
			}
			for( list<NGfx::SVideoMode>::iterator iTemp = modesList.begin(); iTemp != modesList.end(); iTemp++ )
			{
				int nTemplate = 172;
				list<NGfx::SVideoMode>::iterator iNext = iTemp; iNext++;
				if ( iTemp == modesList.begin() )
					nTemplate = 171;
				if ( iNext == modesList.end() )
					nTemplate = 173;

				WCHAR wsBuffer[1024];
				swprintf( wsBuffer, L"<right>%dx%dx32", iTemp->nXSize, iTemp->nYSize );
				pResolution->AddItem( EncodeVideoModeID( iTemp->nXSize, iTemp->nYSize ), NUI::CComplexComboBox::SInfo( wsBuffer ), nTemplate );
			}

			// retail's control naming: the "fsaa_level" row is the LIGHTING QUALITY combo
			pLightingQuality = new CComplexComboBox( sEvent.pLoader->GetControl( "fsaa_level" ) );
			AddQualityItems( pLightingQuality, false );

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// retail @0x62594c
			pHWCursor = GetUIWindow<CCheckButton>( this, "hw_cursor" );
			pShowCrown = GetUIWindow<CCheckButton>( this, "tree_crown" );
			UpdateFromConfig();
			break;
		}
	}

	return CEmptyOptionsUI::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAudioOptionsUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAudioOptionsUI: public CEmptyOptionsUI
{
	OBJECT_NOCOPY_METHODS(CAudioOptionsUI)
private:
	ZDATA_(CEmptyOptionsUI)
	CObj<CHoverButton> pApply;
	CObj<CHoverButton> pDefault;
	CObj<CComplexScroll> pSoundVolume;
	CObj<CComplexScroll> pMusicVolume;
	CObj<CComplexComboBox> pOutputType;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CEmptyOptionsUI*)this); f.Add(2,&pApply); f.Add(3,&pDefault); f.Add(4,&pSoundVolume); f.Add(5,&pMusicVolume); f.Add(6,&pOutputType); return 0; }

public:
	CAudioOptionsUI() {}
	CAudioOptionsUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAudioOptionsUI::CAudioOptionsUI( const SWindowInfo &sInfo ):
	CEmptyOptionsUI( sInfo, NGame::OS_AUDIO )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAudioOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "apply" )
			{
				NGlobal::SetVar( "sound_sfxvolume", pSoundVolume->GetValue() );
				NGlobal::SetVar( "sound_musicvolume", pMusicVolume->GetValue() );
			}
			else if ( sEvent.szID == "default" )
			{
				pSoundVolume->SetValue( 1.0f );
				pMusicVolume->SetValue( 0.5f );
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pApply = new CHoverButton( sEvent.pLoader->GetControl( "apply" ) );
			pDefault = new CHoverButton( sEvent.pLoader->GetControl( "default" ) );
			pSoundVolume = new CComplexScroll( sEvent.pLoader->GetControl( "sound_volume" ) );
			pMusicVolume = new CComplexScroll( sEvent.pLoader->GetControl( "music_volume" ) );

			pApply->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4376 ) );
			pApply->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4376 ) );
			pDefault->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4377 ) );
			pDefault->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4377 ) );

			pOutputType = new CComplexComboBox( sEvent.pLoader->GetControl( "outputtype" ) );
			pOutputType->SetStateInfo( CComplexComboBox::STATE_NORMAL, CComplexComboBox::SInfo( NUI::GetDBString( 4402 ) ) );
			pOutputType->SetStateInfo( CComplexComboBox::STATE_HILIGHTED, CComplexComboBox::SInfo( NUI::GetDBString( 4401 ) ) );
			pOutputType->SetStateInfo( CComplexComboBox::STATE_SELECTED, CComplexComboBox::SInfo( NUI::GetDBString( 4402 ) ) );
			pOutputType->SetStateInfo( CComplexComboBox::STATE_DISABLED, CComplexComboBox::SInfo( NUI::GetDBString( 4402 ) ) );
			pOutputType->AddItem( 0, CComplexComboBox::SInfo( NUI::GetDBString( 4420 ) ), 171 );
			pOutputType->AddItem( 1, CComplexComboBox::SInfo( NUI::GetDBString( 4421 ) ), 172 );
			pOutputType->AddItem( 3, CComplexComboBox::SInfo( NUI::GetDBString( 4422 ) ), 172 );
			pOutputType->AddItem( 4, CComplexComboBox::SInfo( NUI::GetDBString( 4423 ) ), 172 );
			pOutputType->AddItem( 5, CComplexComboBox::SInfo( NUI::GetDBString( 4424 ) ), 172 );
			pOutputType->AddItem( 6, CComplexComboBox::SInfo( NUI::GetDBString( 4425 ) ), 173 );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pSoundVolume->SetValue( NGlobal::GetVar( "sound_sfxvolume", 0 ).GetFloat() );
			pMusicVolume->SetValue( NGlobal::GetVar( "sound_musicvolume", 0 ).GetFloat() );
			break;
		}
	}

	return CEmptyOptionsUI::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGamePlayOptionsUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGamePlayOptionsUI: public CEmptyOptionsUI
{
	OBJECT_NOCOPY_METHODS(CGamePlayOptionsUI)
private:
	ZDATA_(CEmptyOptionsUI)
	CObj<CHoverButton> pDefault;
	CObj<CComplexScroll> pTooltipDelay;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CEmptyOptionsUI*)this); f.Add(2,&pDefault); f.Add(3,&pTooltipDelay); return 0; }

	// Non-owning refs to the auto-instantiated checkbox controls + the blood-warning text (re-fetched
	// at EVENT_TEMPLATELOADCOMPLETE; transient -> not serialized).  The release DROPPED the dev pApply +
	// difficulty combo: the gameplay options now apply live on every change.
	CPtr<CText> pShowBloodText;
	CPtr<CCheckButton> pShowBlood;
	CPtr<CCheckButton> pShowIcons;
	CPtr<CCheckButton> pShowHints;
	CPtr<CCheckButton> pAutosaves;
	CPtr<CCheckButton> pShowPathInRT;
	CPtr<CCheckButton> pForceTurnBased;
	CPtr<CCheckButton> pDblClkMoveInRT;

	void UpdateFromConfig();    // @0x2211f0

public:
	CGamePlayOptionsUI() {}
	CGamePlayOptionsUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGamePlayOptionsUI::CGamePlayOptionsUI( const SWindowInfo &sInfo ):
	CEmptyOptionsUI( sInfo, NGame::OS_GAMEPLAY )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGamePlayOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pDefault = new CHoverButton( sEvent.pLoader->GetControl( "default" ) );
			pDefault->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4377 ) );
			pDefault->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4377 ) );

			// Retail gameplay container (163) ships a tooltip-delay scroll the dev never built -> build
			// it as a CComplexScroll so its nested plus/minus/slider template binds.
			pTooltipDelay = new CComplexScroll( sEvent.pLoader->GetControl( "tooltip_delay" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// Grab the auto-instantiated checkbox controls + the blood-warning text, then sync the UI
			// to the current config.
			pShowBloodText = GetUIWindow<CText>( this, "show_blood_text" );
			pShowBlood = GetUIWindow<CCheckButton>( this, "show_blood" );
			pShowIcons = GetUIWindow<CCheckButton>( this, "show_icons" );
			pShowHints = GetUIWindow<CCheckButton>( this, "show_hints" );
			pAutosaves = GetUIWindow<CCheckButton>( this, "autosaves" );
			pShowPathInRT = GetUIWindow<CCheckButton>( this, "rt_showpath" );
			pForceTurnBased = GetUIWindow<CCheckButton>( this, "force_turnbased" );
			pDblClkMoveInRT = GetUIWindow<CCheckButton>( this, "rt_dblclk_move" );
			UpdateFromConfig();
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "default" )
			{
				NGlobal::ResetVar( "ui_showicons" );
				NGlobal::ResetVar( "ui_showhints" );
				NGlobal::ResetVar( "cheat_blood" );
				NGlobal::ResetVar( "game_autosaves" );
				NGlobal::ResetVar( "game_pathinrealtime" );
				NGlobal::ResetVar( "game_forceturnbased" );
				NGlobal::ResetVar( "game_dblclkmoveinrealtime" );
				NGlobal::ResetVar( "ui_tooltipdelay" );
				UpdateFromConfig();
			}
			else
			{
				// Any other notification = a control changed -> write every var back live.
				UpdateConfig( pShowIcons, "ui_showicons" );
				UpdateConfig( pShowHints, "ui_showhints" );
				UpdateConfig( pShowBlood, "cheat_blood" );
				UpdateConfig( pAutosaves, "game_autosaves" );
				UpdateConfig( pShowPathInRT, "game_pathinrealtime" );
				UpdateConfig( pForceTurnBased, "game_forceturnbased" );
				UpdateConfig( pDblClkMoveInRT, "game_dblclkmoveinrealtime" );
				NGlobal::SetVar( "ui_tooltipdelay", NGlobal::CValue( pTooltipDelay->GetValue() ) );
			}
			break;
		}
	}

	return CEmptyOptionsUI::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateFromConfig @0x2211f0 -- pull every gameplay config var into its control.  ui_germanversion
// (the censored-build flag) hides the blood option; the tooltip scroll holds the 0..1 ratio.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGamePlayOptionsUI::UpdateFromConfig()
{
	bool bShowBlood = ( NGlobal::GetVar( "ui_germanversion" ).GetFloat() == 0.0f );
	if ( IsValid( pShowBlood ) )
		pShowBlood->SetStyle( STYLE_VISIBLE, bShowBlood );
	if ( IsValid( pShowBloodText ) )
		pShowBloodText->SetStyle( STYLE_VISIBLE, bShowBlood );

	UpdateUIElement( pShowIcons, "ui_showicons" );
	UpdateUIElement( pShowHints, "ui_showhints" );
	UpdateUIElement( pShowBlood, "cheat_blood" );
	UpdateUIElement( pAutosaves, "game_autosaves" );
	UpdateUIElement( pShowPathInRT, "game_pathinrealtime" );
	UpdateUIElement( pForceTurnBased, "game_forceturnbased" );
	UpdateUIElement( pDblClkMoveInRT, "game_dblclkmoveinrealtime" );

	pTooltipDelay->SetValue( NGlobal::GetVar( "ui_tooltipdelay" ).GetFloat() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CControlsOptionsUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CControlsOptionsUI: public CEmptyOptionsUI
{
	OBJECT_NOCOPY_METHODS(CControlsOptionsUI)
private:
	ZDATA_(CEmptyOptionsUI)
	CObj<CHoverButton> pDefault;
	CObj<CComplexScroll> pCameraSensivity;
	CObj<CComplexScroll> pScrollSensivity;
	CObj<CComplexScroll> pSelectionFrameSensivity;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CEmptyOptionsUI*)this); f.Add(2,&pDefault); f.Add(3,&pCameraSensivity); f.Add(4,&pScrollSensivity); f.Add(5,&pSelectionFrameSensivity); return 0; }

	// Non-owning refs to the auto-instantiated invert checkboxes (re-fetched at TEMPLATELOADCOMPLETE;
	// transient -> not serialized).  The release DROPPED the dev pApply + mouse-sensitivity scroll:
	// the controls options apply live, and the "reduce_lag" checkbox drives gfx_block_buffering.
	CPtr<CCheckButton> pInvTurnX;
	CPtr<CCheckButton> pInvTurnY;
	CPtr<CCheckButton> pInvScrollX;
	CPtr<CCheckButton> pInvScrollY;
	CPtr<CCheckButton> pReduceMouseLag;

	void UpdateFromConfig();    // @0x2214a0

public:
	CControlsOptionsUI() {}
	CControlsOptionsUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CControlsOptionsUI::CControlsOptionsUI( const SWindowInfo &sInfo ):
	CEmptyOptionsUI( sInfo, NGame::OS_CONTROLS )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sensitivity scrolls store a 0..100 int that maps to/from the config var via the release's affine
// curve: var = (int*2.7 + 30)/100  (range 0.30..3.0, default 1.0 <=> int 26).  The raw CScroll int
// API is used (not CComplexScroll's 0..1 ratio) so the stored var matches what the camera consumes.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void WriteSensVar( CComplexScroll *pScroll, const string &szVar )
{
	if ( IsValid( pScroll ) )
		NGlobal::SetVar( szVar, NGlobal::CValue( ( ( (CScroll*)pScroll )->GetValue() * 2.7f + 30.0f ) * 0.01f ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ReadSensVar( CComplexScroll *pScroll, const string &szVar )
{
	if ( IsValid( pScroll ) )
	{
		float fVal = NGlobal::GetVar( szVar, NGlobal::CValue( 1.0f ) ).GetFloat();
		( (CScroll*)pScroll )->SetValue( Float2Int( ( fVal * 100.0f - 30.0f ) / 2.7f ) );
		( (CScroll*)pScroll )->SetMaxValue( 100 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CControlsOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pDefault = new CHoverButton( sEvent.pLoader->GetControl( "default" ) );
			// Retail container 164 ships scroll/camera/selframe sensitivity scrolls (the release dropped
			// the dev mouse-sensitivity scroll). Build each as a CComplexScroll so its nested plus/minus/
			// slider template binds (matches the retail nesting).
			pScrollSensivity = new CComplexScroll( sEvent.pLoader->GetControl( "scroll_sensivity" ) );
			pCameraSensivity = new CComplexScroll( sEvent.pLoader->GetControl( "camera_sensivity" ) );
			pSelectionFrameSensivity = new CComplexScroll( sEvent.pLoader->GetControl( "selframe_sensivity" ) );

			pDefault->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4377 ) );
			pDefault->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4377 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pInvTurnX = GetUIWindow<CCheckButton>( this, "invert_turnx" );
			pInvTurnY = GetUIWindow<CCheckButton>( this, "invert_turny" );
			pInvScrollX = GetUIWindow<CCheckButton>( this, "invert_scrollx" );
			pInvScrollY = GetUIWindow<CCheckButton>( this, "invert_scrolly" );
			pReduceMouseLag = GetUIWindow<CCheckButton>( this, "reduce_lag" );
			UpdateFromConfig();
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "default" )
			{
				NGlobal::ResetVar( "game_invertturnx" );
				NGlobal::ResetVar( "game_invertturny" );
				NGlobal::ResetVar( "game_invertscrollx" );
				NGlobal::ResetVar( "game_invertscrolly" );
				NGlobal::ResetVar( "game_camerasensivity" );
				NGlobal::ResetVar( "game_scrollsensivity" );
				NGlobal::ResetVar( "game_selectionsensivity" );
				NGlobal::ResetVar( "gfx_block_buffering" );
				UpdateFromConfig();
				NGlobal::ProcessCommand( L"camera_update" );
			}
			else
			{
				UpdateConfig( pInvTurnX, "game_invertturnx" );
				UpdateConfig( pInvTurnY, "game_invertturny" );
				UpdateConfig( pInvScrollX, "game_invertscrollx" );
				UpdateConfig( pInvScrollY, "game_invertscrolly" );
				UpdateConfig( pReduceMouseLag, "gfx_block_buffering" );
				WriteSensVar( pCameraSensivity, "game_camerasensivity" );
				WriteSensVar( pScrollSensivity, "game_scrollsensivity" );
				WriteSensVar( pSelectionFrameSensivity, "game_selectionsensivity" );
				NGlobal::ProcessCommand( L"camera_update" );   // BUG 4: push new sensitivity/invert into the camera coeffs (the reset branch above already does)
			}
			break;
		}
	}

	return CEmptyOptionsUI::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateFromConfig @0x2214a0 -- pull the invert flags + the three sensitivity vars into the controls.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CControlsOptionsUI::UpdateFromConfig()
{
	UpdateUIElement( pInvTurnX, "game_invertturnx" );
	UpdateUIElement( pInvTurnY, "game_invertturny" );
	UpdateUIElement( pInvScrollX, "game_invertscrollx" );
	UpdateUIElement( pInvScrollY, "game_invertscrolly" );
	UpdateUIElement( pReduceMouseLag, "gfx_block_buffering" );

	ReadSensVar( pCameraSensivity, "game_camerasensivity" );
	ReadSensVar( pScrollSensivity, "game_scrollsensivity" );
	ReadSensVar( pSelectionFrameSensivity, "game_selectionsensivity" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CProfileDeleteDlg  --  release-new delete-confirm dialog (a top-level window loaded from its own
// container 420).  Shown by the profile screen; on OK it deletes the profile + notifies the parent.
// reg id 0xB36E2740.  operator& @0x22b980 (base, wsProfile, pNotify, pText, pOK, pCancel).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CProfileDeleteDlg: public CWindow
{
	OBJECT_BASIC_METHODS(CProfileDeleteDlg)
private:
	ZDATA_(CWindow)
	wstring wsProfile;
	CPtr<CWindow> pNotify;
	CPtr<CText> pText;
	CObj<CHoverButton> pOK;
	CObj<CHoverButton> pCancel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&wsProfile); f.Add(3,&pNotify); f.Add(4,&pText); f.Add(5,&pOK); f.Add(6,&pCancel); return 0; }

public:
	CProfileDeleteDlg() {}
	CProfileDeleteDlg( const SWindowInfo &sInfo );

	void SetProfile( const wstring &wsName ) { wsProfile = wsName; }   // @0x229ca0
	void SetNotify( CWindow *pWindow ) { pNotify = pWindow; }          // pNotify member @+0x8c

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CProfileDeleteDlg::CProfileDeleteDlg( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CProfileDeleteDlg::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pOK = new CHoverButton( sEvent.pLoader->GetControl( "ok" ) );
			pCancel = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );

			pOK->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11197 ) + GetDBString( 19055 ) );
			pOK->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11198 ) + GetDBString( 19055 ) );
			pCancel->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11197 ) + GetDBString( 19056 ) );
			pCancel->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11198 ) + GetDBString( 19056 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pText = GetUIWindow<CText>( this, "text" );
			if ( IsValid( pText ) )
				pText->SetText( GetDBString( 20989 ) + wsProfile );
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "ok" )
			{
				NMainLoop::DeleteProfile( NStr::ToAscii( wsProfile ) );
				SendMessage( pNotify, SEvent( EVENT_NOTIFY, GetWindowID() ) );
			}
			SetStyle( STYLE_VISIBLE, false );   // both OK and Cancel close the dialog
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CProfileOptionsUI  --  release-new profile-management landing screen (OS_PROFILE, container 436).
// Lists the on-disk profiles, lets you create one (CEdit) / delete one (via CProfileDeleteDlg), and
// shows the active one selected.  reg id 0xB35051A0.  operator& @0x22bd70.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CProfileOptionsUI: public CEmptyOptionsUI
{
	OBJECT_NOCOPY_METHODS(CProfileOptionsUI)
private:
	ZDATA_(CEmptyOptionsUI)
	CPtr<CEdit> pEdit;
	CObj<CHoverButton> pDefault;
	CObj<CHoverButton> pNewProfile;
	CObj<CHoverButton> pDeleteProfile;
	CObj<CComplexComboBox> pProfiles;
	CObj<CProfileDeleteDlg> pDeleteDlg;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CEmptyOptionsUI*)this); f.Add(2,&pEdit); f.Add(3,&pDefault); f.Add(4,&pNewProfile); f.Add(5,&pDeleteProfile); f.Add(6,&pProfiles); f.Add(7,&pDeleteDlg); return 0; }

	void UpdateFromConfig();        // @0x222580
	wstring GetSelectedProfile();

public:
	CProfileOptionsUI() {}
	CProfileOptionsUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CProfileOptionsUI::CProfileOptionsUI( const SWindowInfo &sInfo ):
	CEmptyOptionsUI( sInfo, NGame::OS_PROFILE )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The profile name (wstring) currently selected in the combo, or empty.
////////////////////////////////////////////////////////////////////////////////////////////////////
wstring CProfileOptionsUI::GetSelectedProfile()
{
	int nSel = pProfiles->GetSelectedItem();
	if ( nSel == -1 )
		return wstring();
	CComplexComboBox::SInfo sInfo;
	pProfiles->GetItem( nSel, &sInfo );
	return sInfo.wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateFromConfig @0x222580 -- (re)fill the combo from the on-disk profiles + select the active one;
// disable "new profile" at the 20-profile cap.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CProfileOptionsUI::UpdateFromConfig()
{
	NMainLoop::MakeDefaultProfile();

	list<string> profilesList;
	NMainLoop::GetProfilesList( &profilesList );
	string szActive = NMainLoop::GetActiveProfile();

	int nIndex = 0;
	for ( list<string>::iterator iProfile = profilesList.begin(); iProfile != profilesList.end(); iProfile++, nIndex++ )
	{
		int nTemplate = 172;
		if ( iProfile == profilesList.begin() )
			nTemplate = 171;
		list<string>::iterator iNext = iProfile; iNext++;
		if ( iNext == profilesList.end() )
			nTemplate = 173;

		pProfiles->AddItem( nIndex, CComplexComboBox::SInfo( NStr::ToUnicode( *iProfile ) ), nTemplate );
		if ( *iProfile == szActive )
			pProfiles->SetSelectedItem( nIndex );
	}

	pNewProfile->SetStyle( STYLE_ENABLED, int( profilesList.size() ) < 20 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Find the first CEdit anywhere under pWindow (depth-first), regardless of its control name.
////////////////////////////////////////////////////////////////////////////////////////////////////
static CEdit* FindEditChild( CWindow *pWindow )
{
	list< CPtr<CWindow> > childrenList;
	pWindow->GetChildrenList( &childrenList );
	for ( list< CPtr<CWindow> >::iterator iChild = childrenList.begin(); iChild != childrenList.end(); iChild++ )
	{
		CEdit *pFound = dynamic_cast<CEdit*>( iChild->GetPtr() );
		if ( pFound != 0 )
			return pFound;
		pFound = FindEditChild( *iChild );
		if ( pFound != 0 )
			return pFound;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CProfileOptionsUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pDefault = new CHoverButton( sEvent.pLoader->GetControl( "default" ) );
			pDefault->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 4377 ) );
			pDefault->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 4377 ) );

			pNewProfile = new CHoverButton( sEvent.pLoader->GetControl( "newprofile" ) );
			pNewProfile->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 19819 ) );
			pNewProfile->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 19819 ) );
			pNewProfile->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 4402 ) + GetDBString( 19819 ) );

			pDeleteProfile = new CHoverButton( sEvent.pLoader->GetControl( "deleteprofile" ) );
			pDeleteProfile->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 4404 ) + GetDBString( 19818 ) );
			pDeleteProfile->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 4405 ) + GetDBString( 19818 ) );

			pProfiles = new CComplexComboBox( sEvent.pLoader->GetControl( "currentprofile" ) );
			pProfiles->SetStateInfo( CComplexComboBox::STATE_NORMAL, CComplexComboBox::SInfo( GetDBString( 4402 ) ) );
			pProfiles->SetStateInfo( CComplexComboBox::STATE_HILIGHTED, CComplexComboBox::SInfo( GetDBString( 4401 ) ) );
			pProfiles->SetStateInfo( CComplexComboBox::STATE_SELECTED, CComplexComboBox::SInfo( GetDBString( 4402 ) ) );
			pProfiles->SetStateInfo( CComplexComboBox::STATE_DISABLED, CComplexComboBox::SInfo( GetDBString( 4402 ) ) );

			// The delete-confirm dialog is its own top-level window (container 420), notified back to us.
			pDeleteDlg = new CProfileDeleteDlg( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 1024, 768 ), "deletedlg", STYLE_ENABLED | STYLE_TOPMOST ) );
			LoadTemplate( pDeleteDlg, NDb::GetUIContainer( 420 ) );
			pDeleteDlg->SetNotify( this );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// Our 32MB game.db's container 436 names the edit control differently from the retail
			// "profilename" (GetUIWindow-by-name would log "control not found" + spawn a 0x0 phantom),
			// so bind it BY TYPE: the first CEdit anywhere under this screen.  Format DBString 19906 is
			// the release's edit font (4404 is the oversized menu-button tag).
			pEdit = FindEditChild( this );
			if ( IsValid( pEdit ) )
			{
				pEdit->SetMode( CEdit::FILENAME );
				pEdit->SetEditSize( 23 );
				pEdit->SetTextFormat( GetDBString( 19906 ) );
			}
			else
				csSystem << "OPTIONS: profile name CEdit not found in container 436 -> create disabled" << endl;

			// Fill the combo + select the active profile here, AFTER the combo's nested selected-view is
			// built -- doing it in EVENT_TEMPLATELOAD left the closed view blank on first open.
			UpdateFromConfig();
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "default" )
			{
				NGlobal::ResetVar( "game_profile" );
				NMainLoop::MakeDefaultProfile();
				pProfiles->RemoveAllItems();
				UpdateFromConfig();
			}
			else if ( sEvent.szID == "newprofile" )
			{
				list<string> profilesList;
				NMainLoop::GetProfilesList( &profilesList );
				if ( int( profilesList.size() ) < 20 && IsValid( pEdit ) && !pEdit->GetText().empty() )
				{
					string szNew = NStr::ToAscii( pEdit->GetText() );
					NMainLoop::CreateProfile( szNew );
					NMainLoop::SetActiveProfile( szNew );
					pEdit->SetText( L"" );
					pProfiles->RemoveAllItems();
					UpdateFromConfig();
				}
			}
			else if ( sEvent.szID == "deleteprofile" )
			{
				wstring wsSel = GetSelectedProfile();
				if ( !wsSel.empty() )
				{
					pDeleteDlg->SetProfile( wsSel );
					pDeleteDlg->SetStyle( STYLE_VISIBLE, true );
				}
			}
			else if ( sEvent.szID == "currentprofile" )
			{
				// Selecting a profile in the combo makes it the active one.
				wstring wsSel = GetSelectedProfile();
				if ( !wsSel.empty() )
					NMainLoop::SetActiveProfile( NStr::ToAscii( wsSel ) );
			}
			else if ( sEvent.szID == "deletedlg" )
			{
				// The delete dialog confirmed (it already removed the profile dir) -> refresh the list.
				pProfiles->RemoveAllItems();
				UpdateFromConfig();
			}
			break;
		}
	}

	return CEmptyOptionsUI::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// COptionsInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionsInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(COptionsInterface);
private:
	NInput::CBind bindClose;
	NInput::CBind bindVideoOptions, bindAudioOptions, bindProfileOptions, bindGamePlayOptions, bindControlsOptions;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	CObj<NUI::CScreenShot> pScreenShot;
	////
	CObj<NUI::CWindow> pOptionsUI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMission); f.Add(3,&pCursor); f.Add(4,&pInterface); f.Add(5,&pScreenShot); f.Add(6,&pOptionsUI); return 0; }
	CPtr<IMission> pMission;

public:
	COptionsInterface();

	void Initialize( EOptionsScreen eScreen, NGScene::CScreenshotTexture *pScreenShotTexture );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
COptionsInterface::COptionsInterface():
	bindClose( "cancel" ), bindVideoOptions( "video" ), bindAudioOptions( "audio" ), bindProfileOptions( "profiles" ), bindGamePlayOptions( "gameplay" ), bindControlsOptions( "controls" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsInterface::Initialize( EOptionsScreen eScreen, NGScene::CScreenshotTexture *pScreenShotTexture )
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

	switch( eScreen )
	{
	case OS_PROFILE:
		pOptionsUI = new NUI::CProfileOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 436 ) );
		break;
	case OS_VIDEO:
		pOptionsUI = new NUI::CVideoOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 161 ) );
		break;
	case OS_AUDIO:
		pOptionsUI = new NUI::CAudioOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 162 ) );
		break;
	case OS_GAMEPLAY:
		pOptionsUI = new NUI::CGamePlayOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 163 ) );
		break;
	case OS_CONTROLS:
		pOptionsUI = new NUI::CControlsOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 164 ) );
		break;
	default:
		pOptionsUI = new NUI::CEmptyOptionsUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "options", NUI::STYLE_ENABLED ) );
		NUI::LoadTemplate( pOptionsUI, NDb::GetUIContainer( 170 ) );
		break;
	}

	pOptionsUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsInterface::Step()
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
void COptionsInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool COptionsInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
	else if ( bindVideoOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_VIDEO, pScreenShot->GetTexture() ) );
	}
	else if ( bindAudioOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_AUDIO, pScreenShot->GetTexture() ) );
	}
	else if ( bindProfileOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_PROFILE, pScreenShot->GetTexture() ) );
	}
	else if ( bindGamePlayOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_GAMEPLAY, pScreenShot->GetTexture() ) );
	}
	else if ( bindControlsOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_CONTROLS, pScreenShot->GetTexture() ) );
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COptionsInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );

	pInterface->Draw( GetTime() );

	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICOptions::CICOptions( EOptionsScreen _eScreen, NGScene::CScreenshotTexture *_pScreenShotTexture ):
	eScreen( _eScreen ), pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICOptions::Exec()
{
	COptionsInterface *pRes = new COptionsInterface();
	pRes->Initialize( eScreen, pScreenShotTexture );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0815150, COptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB0815151, CVideoOptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB0815152, CAudioOptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB0815153, CGamePlayOptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB0815154, CControlsOptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB0815155, CComplexSlider );
REGISTER_SAVELOAD_CLASS( 0xB0815156, CComplexScroll );	// was dev CComplexTextSlider (dropped; merged into CComplexSlider) -- release reused this id for CComplexScroll
REGISTER_SAVELOAD_CLASS( 0xB3721130, CComplexComboBox );
REGISTER_SAVELOAD_CLASS( 0xB0815158, CEmptyOptionsUI );
REGISTER_SAVELOAD_CLASS( 0xB3527160, CProfileOptionsUI );	// release-new profile-management tab
REGISTER_SAVELOAD_CLASS( 0xB3728180, CProfileDeleteDlg );	// release-new delete-confirm dialog
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB081515A, COptionsInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Register the gameplay + controls option vars with their retail defaults so the screens open with the
// right state and the "Default" button restores sensible values.  In the release these are owned by
// scattered consumer subsystems (iMainInit/wMainInit/iMissionUIInit/...) that are absent or unwired in
// this tree; here they carry no handler -- they persist to config and drive the Options UI only.
// cheat_blood is already registered (wDumbUnit.cpp) with its real BloodHandler, so it is NOT re-listed.
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iOptionsMenu)
	REGISTER_VAR( "ui_showicons",              0, 1.0f, true )
	REGISTER_VAR( "ui_showhints",              0, 1.0f, true )
	// retail iMissionUIInit @0x215b90: bShowAcks / bShowAcksSubtitles, both default 1.0 (on) --
	// gate the ack face-mirror (CMissionUI::PlayAckEvent @0x211930) and the ack subtitle band
	// (CAckView::Set @0x210e10).
	REGISTER_VAR( "ui_charresponses",          0, 1.0f, true )
	REGISTER_VAR( "ui_charresponsessubtitles", 0, 1.0f, true )
	REGISTER_VAR( "game_autosaves",            0, 1.0f, true )
	REGISTER_VAR( "game_pathinrealtime",       0, 1.0f, true )
	REGISTER_VAR( "game_forceturnbased",       0, 0.0f, true )
	REGISTER_VAR( "game_dblclkmoveinrealtime", 0, 0.0f, true )
	REGISTER_VAR( "ui_tooltipdelay",           0, 0.1f, true )
	REGISTER_VAR( "ui_showtooltips",           0, 1.0f, true )   // retail UIInterfaceInit @0x71e630 registers both tooltip vars
	REGISTER_VAR( "game_invertturnx",          0, 0.0f, true )
	REGISTER_VAR( "game_invertturny",          0, 0.0f, true )
	REGISTER_VAR( "game_invertscrollx",        0, 0.0f, true )
	REGISTER_VAR( "game_invertscrolly",        0, 0.0f, true )
	// gfx_block_buffering moved to GSceneInternal.cpp (retail registers it there, VarBoolHandler-bound)
	REGISTER_VAR( "game_camerasensivity",      0, 1.0f, true )
	REGISTER_VAR( "game_scrollsensivity",      0, 1.0f, true )
	REGISTER_VAR( "game_selectionsensivity",   0, 1.0f, true )
	REGISTER_VAR( "game_profile",              0, NGlobal::CValue( L"default" ), true )   // active profile name (release stores it here)
FINISH_REGISTER
