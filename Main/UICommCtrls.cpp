#include "StdAfx.h"
#include "Gfx.h"
#include "GfxBuffers.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "UIWrap.h"
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "UIML.h"
#include "ScreenShot.h"
#include "GBinkPlayer.h"   // NGScene::IVideoPlayer + NGScene::CreateVideoPlayer (CVideoPlayer)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_LIST_SCROLL_STEP = 16,
	N_TOOLTIP_DEFAULT_WIDTH = 400,
	N_TOOLTIP_BORDER_SIZE = 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEdit
////////////////////////////////////////////////////////////////////////////////////////////////////
CEdit::CEdit( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nSize( 255 ), nCursor( 0 ), eMode( NORMAL ), bActiveState( false ), bCursorVisible( false ), sFlashTime( 0 )
{
	pSize = new NGScene::CCTPoint;
	pTextString = new NGScene::CCWString;

	pSize->Set( GetSize() );
	pTextString->Set( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CEdit::EMode CEdit::GetMode() const
{
	return eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::SetMode( EMode _eMode )
{
	eMode = _eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CEdit::GetText() const
{
	return wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::SetText( const wstring &_wsText )
{
	wsText = _wsText;
	SetCursorPosition( nCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::SetEditSize( int _nSize )
{
	nSize = _nSize;
	if ( wsText.length() > nSize )
		wsText.resize( nSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::SetTextFormat( const wstring &_wsFormat )
{
	wsFormat = _wsFormat;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::SetCursorPosition( int nPos )
{
	nCursor = Min( (int)wsText.length(), nPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEdit::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_CHAR )
	{
		if ( !IsActive() )
			return false;

		switch( sEvent.nVal )
		{
		case VK_END:
			nCursor = wsText.length();
			return true;
		case VK_HOME:
			nCursor = 0;
			return true;
		case VK_LEFT:
			if ( nCursor > 0 ) nCursor--;
			return true;
		case VK_RIGHT:
			if ( nCursor < wsText.length() ) nCursor++;
			return true;

		case VK_BACK:
			if ( nCursor > 0 )
			{
				wstring wsTempString;
				wsTempString.append( wsText.substr( 0, nCursor - 1 ) );
				wsTempString.append( wsText.substr( nCursor, wsText.length() ) );
				wsText = wsTempString;
				nCursor--;
			}
			return true;
		case VK_DELETE:
			if ( nCursor < wsText.length() )
			{
				wstring wsTempString;
				wsTempString.append( wsText.substr( 0, nCursor ) );
				wsTempString.append( wsText.substr( nCursor + 1, wsText.length() ) );
				wsText = wsTempString;
			}
			return true;
		case VK_RETURN:
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;

		default:
			{
				WCHAR wcChar = 0;
				if ( !NInput::GetCharForKey( sEvent.nVal, &wcChar ) )
					break;
				if ( ( eMode == NUMERIC ) && ( !iswdigit( wcChar ) ) )
					break;
				if ( ( eMode == FILENAME ) && ( ( wcChar < 40 ) || ( wcschr( L".<>\\/|\"*^:?", wcChar ) != NULL ) ) )
					break;
				if ( !iswprint( wcChar ) )
					break;

				if ( wsText.length() < nSize )
				{
					wstring wsTempString;
					wsTempString.append( wsText.substr( 0, nCursor ) );
					wsTempString.append( 1, wcChar );
					wsTempString.append( wsText.substr( nCursor, wsText.length() ) );
					wsText = wsTempString;
					nCursor++;
				}

				return true;
			}
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEdit::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	wstring wsTemp( wsFormat + wsText );

	if ( GetSize() != pSize->GetValue() )
		pSize->Set( GetSize() );
	if ( wsTemp != pTextString->GetValue() )
		pTextString->Set( wsTemp );

	if ( GetTickCount() - sFlashTime > 500 )
	{
		sFlashTime = GetTickCount();
		bCursorVisible = !bCursorVisible;
	}
	if ( !IsActive() )
		bCursorVisible = false;

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	VirtualToScreen( &sPosition, &sWindow );

	if ( !pText ) pText = pView->CreateText( pTextString, pSize );

	pText.Refresh();
	pView->CreateDynamicRects( pText, sPosition, sWindow );

	if ( bCursorVisible )
	{
		CRectLayout sLayout;
		const NGScene::SText &sText = pText->GetValue();
		if ( nCursor < sText.charsSet.size() )
		{
			const CTRect<int> &sRect = sText.charsSet[nCursor].sRect;
			sLayout.AddRect( sRect.x1, sRect.y1, CRectLayout::STextureCoord( CTRect<float>( 0, 0, 2, sRect.Height() ) ) );
		}
		else
		{
			CTRect<int> sRect( 0, 0, 0, 24 );
			if ( !sText.charsSet.empty() )
				sRect = sText.charsSet.back().sRect;

			sLayout.AddRect( sRect.x2, sRect.y1, CRectLayout::STextureCoord( CTRect<float>( 0, 0, 2, sRect.Height() ) ) );
		}
		pView->CreateDynamicRects( (NDb::CTexture*)0, sLayout, sPosition, sWindow );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPushButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CButton::CButton( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nState( 0 ), bPushed( false ), bMouseEnter( false ), sColor( 0xFF, 0xFF, 0xFF, 0xFF )
{
	pGlow = new CImage( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TRANSPARENT ) );
	pMountUp = new CImage( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TRANSPARENT ) );
	pMountDown = new CImage( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TRANSPARENT ) );
	pMountDisabled = new CImage( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TRANSPARENT ) );
	BringWindowToTop( pGlow ); // Glow -> on top of button
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::SetNotifyID( const string &_szID )
{
	szID = _szID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const NGfx::SPixel8888& CButton::GetColor() const
{
	return sColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::SetColor( const NGfx::SPixel8888 &_sColor )
{
	sColor = _sColor;

	pGlow->SetColor( sColor );
	pMountUp->SetColor( sColor );
	pMountDown->SetColor( sColor );
	pMountDisabled->SetColor( sColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CButton::AddState( int nID )
{
	CWindow* pState = new CWindow( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
	statesMap[nID] = pState;
	return pState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::RemoveState( int nID )
{
	statesMap[nID] = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CButton::GetState( int nID ) const
{
	unordered_map<int,CObj<CWindow> >::const_iterator iTemp = statesMap.find( nID );
	if ( iTemp != statesMap.end() )
		return iTemp->second;

	ASSERT( 0 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CButton::AddTextState( int nID, const wstring &wsText )
{
	CWindow* pState = AddState( nID );
	CText* pText = new CText( SWindowInfo( pState, SPoint( 0, 0 ), pState->GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );
	pText->SetText( wsText );
	return pState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CButton::AddImageState( int nID, NDb::CUITexture *pTexture, const NGfx::SPixel8888 &sColor )
{
	CWindow* pState = AddState( nID );
	CImage* pImage = new CImage( SWindowInfo( pState, SPoint( 0, 0 ), pState->GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );
	pImage->SetImage( pTexture );
	pImage->SetColor( sColor );
	return pState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CButton::GetActiveState()
{
	return nState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::SetActiveState( int nID )
{
	nState = nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CButton::IsPushed() const
{
	return bPushed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CButton::IsMouseCover() const
{
	return bMouseEnter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_ACTIVATE:
		{
			if ( sEvent.nVal & EAF_DEACTIVATE )
			{
				bPushed = false;
				bMouseEnter = false;
				pMouseCaptture = 0;
			}

			break;
		}
	case EVENT_MOUSEENTER:
		{
			bMouseEnter = true;
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			bMouseEnter = false;
			break;
		}
	case EVENT_LBUTTONDOWN:
		{
			bPushed = true;
			bMouseEnter = true;
			pMouseCaptture = GetInterface()->CreateMouseCapture( this );
			return true;
		}
	case EVENT_LBUTTONUP:
		{
			if ( !bPushed )
				break;

			bPushed = false;
			pMouseCaptture = 0;
			if ( GetStyle( STYLE_ENABLED ) && HitTest( sEvent.nX, sEvent.nY ) )
				OnAction();
			return true;
		}
	case EVENT_MOUSECAPTURELOSE:
		{
			bPushed = false;
			pMouseCaptture = 0;
			return true;
		}
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONUP:
	case EVENT_RBUTTONDOWN:
	case EVENT_RBUTTONDBLCLK:
		return true;
	case EVENT_TEMPLATECREATE:
		{
			pClickSound = sEvent.pControl->pSounds[0];

			if ( IsValid( sEvent.pControl->pTextures[0] ) )
			{
				pGlow->SetSize( GetSize() );
				pGlow->SetImage( sEvent.pControl->pTextures[0] );
			}

			if ( IsValid( sEvent.pControl->pTextures[1] ) )
			{
				pMountUp->SetSize( GetSize() );
				pMountUp->SetImage( sEvent.pControl->pTextures[1] );
			}

			if ( IsValid( sEvent.pControl->pTextures[2] ) )
			{
				pMountDown->SetSize( GetSize() );
				pMountDown->SetImage( sEvent.pControl->pTextures[2] );
			}
			else if ( IsValid( sEvent.pControl->pTextures[1] ) )
			{
				pMountDown->SetSize( GetSize() );
				pMountDown->SetImage( sEvent.pControl->pTextures[1], SRect( sEvent.pControl->pTextures[1]->nWidth, 0, 0, sEvent.pControl->pTextures[1]->nHeight ) );
			}

			if ( IsValid( sEvent.pControl->pTextures[3] ) )
			{
				pMountDisabled->SetSize( GetSize() );
				pMountDisabled->SetImage( sEvent.pControl->pTextures[3] );
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !GetStyle( STYLE_VISIBLE ) )
	{
		bPushed = false;
		bMouseEnter = false;
		pMouseCaptture = 0;
	}

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	for ( unordered_map<int,CObj<CWindow> >::iterator iTemp = statesMap.begin(); iTemp != statesMap.end(); iTemp++ )
	{
		if ( iTemp->first == nState )
			iTemp->second->SetStyle( STYLE_VISIBLE, true );
		else
			iTemp->second->SetStyle( STYLE_VISIBLE, false );
	}

	if ( !GetStyle( STYLE_ENABLED ) )
	{
		pGlow->SetStyle( STYLE_VISIBLE, false );
		pMountUp->SetStyle( STYLE_VISIBLE, false );
		pMountDown->SetStyle( STYLE_VISIBLE, false );
		pMountDisabled->SetStyle( STYLE_VISIBLE, true );
	}
	else
	{
		pGlow->SetStyle( STYLE_VISIBLE, IsActive() );
		pMountDisabled->SetStyle( STYLE_VISIBLE, false );

		if ( bPushed && bMouseEnter )
		{
			pMountUp->SetStyle( STYLE_VISIBLE, false );
			pMountDown->SetStyle( STYLE_VISIBLE, true );
		}
		else
		{
			pMountUp->SetStyle( STYLE_VISIBLE, true );
			pMountDown->SetStyle( STYLE_VISIBLE, false );
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CButton::OnAction()
{
	PlaySound( pClickSound );

	if ( !szID.empty() )
		SendMessage( GetParent(), SEvent( EVENT_NOTIFY, szID ) );
	else
		SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPushButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CPushButton::CPushButton( const SWindowInfo &sInfo ):
	CButton( sInfo )
{
	pText = new CTextDraw();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CPushButton::GetText() const
{
	return wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPushButton::SetText( const wstring &_wsText )
{
	wsText = _wsText;
	pText->SetText( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPushButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATECREATE:
		{
			if ( sEvent.pControl->pString )
				SetText( sEvent.pControl->pString->szStr );
			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPushButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CButton::Draw( sTime, pView );

	const SPoint &sSize = GetSize();
	const SPoint &sTextSize = pText->GetSize( pView );

	pText->SetPosition( SPoint( ( sSize.x - sTextSize.x ) / 2, ( sSize.y - sTextSize.y ) / 2 ) );
	pText->Draw( this, sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCheckButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CCheckButton::CCheckButton( const SWindowInfo &sInfo ):
	CPushButton( sInfo ), bChecked( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCheckButton::IsChecked() const
{
	return bChecked;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCheckButton::SetChecked( bool bState )
{
	bChecked = bState;

	if ( bChecked )
		SetActiveState( 1 );
	else
		SetActiveState( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCheckButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATECREATE:
		{
			AddState( 0 );
			AddImageState( 1, sEvent.pControl->pTextures[4] );
			break;
		}
	}

	return CPushButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCheckButton::OnAction()
{
	SetChecked( !bChecked );
	CButton::OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToolTip
////////////////////////////////////////////////////////////////////////////////////////////////////
static CImageDraw* CreateImage( NDb::CUITexture *pTexture )
{
	SRect sWindow( 0, 0, 0, 0 );
	if ( IsValid( pTexture ) )
	{
		sWindow.x2 = pTexture->nWidth;
		sWindow.y2 = pTexture->nHeight;
	}

	return new CImageDraw( sWindow, pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CToolTip::CToolTip( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
	pText = new CMLText( SWindowInfo( this, SPoint( 0, 0 ), SPoint( N_TOOLTIP_DEFAULT_WIDTH, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );

	pBackgroundUp = CreateImage( NDb::GetUITexture( 655 ) );
	pBackgroundDown = CreateImage( NDb::GetUITexture( 661 ) );
	pBackgroundLeft = CreateImage( NDb::GetUITexture( 657 ) );
	pBackgroundRight = CreateImage( NDb::GetUITexture( 659 ) );
	pBackgroundMiddle = CreateImage( NDb::GetUITexture( 658 ) );
	pBackgroundUpLeft = CreateImage( NDb::GetUITexture( 654 ) );
	pBackgroundUpRight = CreateImage( NDb::GetUITexture( 656 ) );
	pBackgroundDownLeft = CreateImage( NDb::GetUITexture( 660 ) );
	pBackgroundDownRight = CreateImage( NDb::GetUITexture( 662 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::SetVal( const wstring &szID, int nVal )
{
	pText->SetVal( szID, nVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::SetVal( const wstring &szID, float fVal )
{
	pText->SetVal( szID, fVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::SetVal( const wstring &szID, const wstring &wsVal )
{
	pText->SetVal( szID, wsVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::SetText( const wstring &wsText )
{
	pText->SetText( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::SetPosition( const SPoint &_sPosition )
{
	CWindow::SetSize( SPoint( 0, 0 ) );
	CWindow::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SPoint sRealSize;
	pText->SetSize( SPoint( N_TOOLTIP_DEFAULT_WIDTH, 0 ) );
	pText->GetRealSize( &sRealSize );

	sRealSize.x += N_TOOLTIP_BORDER_SIZE * 2;
	sRealSize.y += N_TOOLTIP_BORDER_SIZE * 2;

	SPoint sPosition = GetPosition();
	sPosition.y = sPosition.y + GetSize().y - sRealSize.y;

	if ( sPosition.x < 0 )
		sPosition.x = 0;
	if ( sPosition.y < 0 )
		sPosition.y = 0;
	if ( sPosition.x + sRealSize.x > 1024 )
		sPosition.x = 1024 - sRealSize.x;
	if ( sPosition.y + sRealSize.y > 768 )
		sPosition.y = 768 - sRealSize.y;

	CWindow::SetSize( sRealSize );
	CWindow::SetPosition( sPosition );
	pText->SetSize( SPoint( N_TOOLTIP_DEFAULT_WIDTH, sRealSize.y ) );
	pText->SetPosition( SPoint( N_TOOLTIP_BORDER_SIZE, N_TOOLTIP_BORDER_SIZE ) );

	DrawBackground( sTime, pView );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToolTip::DrawBackground( const STime &sTime, NGScene::I2DGameView *pView )
{
	SRect sRect( 0, 0, GetSize().x, GetSize().y );

	sRect.y1 += pBackgroundUp->GetWindow().Height();
	sRect.y2 -= pBackgroundDown->GetWindow().Height();
	sRect.x1 += pBackgroundLeft->GetWindow().Width();
	sRect.x2 -= pBackgroundRight->GetWindow().Width();

	pBackgroundUp->SetWindow( SRect( sRect.x1, 0, sRect.x2, sRect.y1 ) );
	pBackgroundDown->SetWindow( SRect( sRect.x1, sRect.y2, sRect.x2, GetSize().y ) );
	pBackgroundLeft->SetWindow( SRect( 0, sRect.y1, sRect.x1, sRect.y2 ) );
	pBackgroundRight->SetWindow( SRect( sRect.x2, sRect.y1, GetSize().x, sRect.y2 ) );
	pBackgroundUpLeft->SetWindow( SRect( 0, 0, sRect.x1, sRect.y1 ) );
	pBackgroundUpRight->SetWindow( SRect( sRect.x2, 0, GetSize().x, sRect.y1 ) );
	pBackgroundDownLeft->SetWindow( SRect( 0, sRect.y2, sRect.x1, GetSize().y ) );
	pBackgroundDownRight->SetWindow( SRect( sRect.x2, sRect.y2, GetSize().x, GetSize().y ) );

	pBackgroundMiddle->SetWindow( sRect );

	pBackgroundUp->Draw( this, sTime, pView );
	pBackgroundDown->Draw( this, sTime, pView );
	pBackgroundLeft->Draw( this, sTime, pView );
	pBackgroundRight->Draw( this, sTime, pView );
	pBackgroundUpLeft->Draw( this, sTime, pView );
	pBackgroundUpRight->Draw( this, sTime, pView );
	pBackgroundDownLeft->Draw( this, sTime, pView );
	pBackgroundDownRight->Draw( this, sTime, pView );
	pBackgroundMiddle->Draw( this, sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlider
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlider::CSlider( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nValue( 0 ), nMaxValue( 100 ), nPageStep( 10 ), bSlide( false )
{
	if ( !GetStyle( SLDRSTYLE_HORZ ) && !GetStyle( SLDRSTYLE_VERT ) )
		SetStyle( SLDRSTYLE_VERT, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSlider::GetValue()
{
	return nValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::SetValue( int _nValue )
{
	nValue = Max( Min( _nValue, nMaxValue ), 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSlider::GetMaxValue() const
{
	return nMaxValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::SetMaxValue( int _nMaxValue )
{
	nMaxValue = _nMaxValue;

	SetValue( nValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::SetPageStep( int _nPageStep )
{
	nPageStep = _nPageStep;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlider::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	case EVENT_MOUSECAPTURELOSE:
		{
			bSlide = false;
			pMouseCapture = 0;
			break;
		}
	case EVENT_LBUTTONDOWN:
		{
			if ( !IsValid( pSlider ) )		// missing "slider" child (release-data resilience)
				break;
			SRect sWindow;
			SPoint sPosition;
			pSlider->ClientToScreen( &sPosition, &sWindow );
			if ( ( sEvent.nX > sWindow.x1 ) && ( sEvent.nY > sWindow.y1 ) && ( sEvent.nX < sWindow.x2 ) && ( sEvent.nY < sWindow.y2 ) )
			{
				bSlide = true;
				pMouseCapture = GetInterface()->CreateMouseCapture( this );
			}
			else
				PageSlide( sEvent.nX, sEvent.nY );

			break;
		}
	case EVENT_MOUSEMOVE:
		{
			if ( bSlide )
				Slide( sEvent.nX, sEvent.nY );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pSlider = GetUIWindow<CWindow>( this, "slider" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::Slide( int nX, int nY )
{
	if ( !IsValid( pSlider ) )		// missing "slider" child -> nothing to drag (release-data resilience)
		return;
	SRect sWindow;
	SPoint sPosition;
	ClientToScreen( &sPosition, &sWindow );

	const SPoint &sSize = GetSize();
	const SPoint &sSliderSize = pSlider->GetSize();

	int nValue = 0;
	if ( GetStyle( SLDRSTYLE_HORZ ) )
		nValue = Float2Int( float( nX - sPosition.x - sSliderSize.x / 2 ) * nMaxValue / float( sSize.x - sSliderSize.x ) );
	else
		nValue = Float2Int( float( nY - sPosition.y - sSliderSize.x / 2 ) * nMaxValue / float( sSize.y - sSliderSize.y ) );

	SetValue( nValue );
	OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::PageSlide( int nX, int nY )
{
	if ( !IsValid( pSlider ) )		// missing "slider" child (release-data resilience)
		return;
	const SPoint &sSliderPosition = pSlider->GetPosition();

	SPoint sCursorPosition;
	ScreenToClient( SPoint( nX, nY ), &sCursorPosition );

	if ( GetStyle( SLDRSTYLE_HORZ ) )
	{
		if ( sCursorPosition.x < sSliderPosition.x )
			SetValue( nValue - nPageStep );
		else
			SetValue( nValue + nPageStep );
	}
	else
	{
		if ( sCursorPosition.y < sSliderPosition.y )
			SetValue( nValue - nPageStep );
		else
			SetValue( nValue + nPageStep );
	}

	OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	// RELEASE-DATA RESILIENCE: pSlider is the "slider" child looked up at TEMPLATELOADCOMPLETE; a UI
	// container that ships no such child leaves it null. The shipped game.db (32MB) differs from the
	// retail layout the panels assume, so a missing child must degrade gracefully, not null-deref/crash.
	if ( !IsValid( pSlider ) )
	{
		CWindow::Update( sTime, pView );
		return;
	}
	const SPoint &sPadSize = GetSize();
	const SPoint &sSliderSize = pSlider->GetSize();
	const SPoint &sSliderPosition = pSlider->GetPosition();

	if ( GetStyle( SLDRSTYLE_HORZ ) )
		pSlider->SetPosition( SPoint( float( nValue ) * ( sPadSize.x - sSliderSize.x ) / nMaxValue, sSliderPosition.y ) );
	else
		pSlider->SetPosition( SPoint( sSliderPosition.x, float( nValue ) * ( sPadSize.y - sSliderSize.y ) / nMaxValue ) );

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlider::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScroll
////////////////////////////////////////////////////////////////////////////////////////////////////
CScroll::CScroll( const SWindowInfo &sInfo ):
	CWindow( sInfo ), pNotify( sInfo.pParent )
{
	if ( !GetStyle( SCRLSTYLE_HORZ ) && !GetStyle( SCRLSTYLE_VERT ) )
		SetStyle( SCRLSTYLE_VERT, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScroll::pSlider is the "slider" child looked up at TEMPLATELOADCOMPLETE; guard every deref so a
// container missing that child degrades gracefully instead of null-deref/crash (release-data resilience).
int CScroll::GetValue()
{
	return IsValid( pSlider ) ? pSlider->GetValue() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScroll::SetValue( int nValue )
{
	if ( IsValid( pSlider ) )
		pSlider->SetValue( nValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScroll::GetMaxValue() const
{
	return IsValid( pSlider ) ? pSlider->GetMaxValue() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScroll::SetMaxValue( int nMaxValue )
{
	if ( IsValid( pSlider ) )
		pSlider->SetMaxValue( nMaxValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScroll::SetPageStep( int nPageStep )
{
	if ( IsValid( pSlider ) )
		pSlider->SetPageStep( nPageStep );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScroll::SetNotifyWindow( CWindow *pWindow )
{
	pNotify = pWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScroll::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( !IsValid( pSlider ) )		// missing "slider" child (release-data resilience)
				return true;
			if ( sEvent.szID == "minus" )
			{
				pSlider->SetValue( pSlider->GetValue() - 1 );
				OnAction();
			}
			else if ( sEvent.szID == "plus" )
			{
				pSlider->SetValue( pSlider->GetValue() + 1 );
				OnAction();
			}
			else if ( sEvent.szID == "slider" )
				OnAction();
			return true;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pSlider = GetUIWindow<CSlider>( this, "slider" );
			if ( IsValid( pSlider ) )		// guard: container may ship no "slider" child
			{
				if ( GetStyle( SCRLSTYLE_HORZ ) )
					pSlider->SetStyle( SLDRSTYLE_HORZ, true );
				else if ( GetStyle( SCRLSTYLE_VERT ) )
					pSlider->SetStyle( SLDRSTYLE_VERT, true );
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScroll::OnAction()
{
	SendMessage( pNotify, SEvent( EVENT_NOTIFY, GetWindowID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CListView
////////////////////////////////////////////////////////////////////////////////////////////////////
CListView::CListView( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nSelectedID( -1 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::AddItem( int nID, CWindow *pWindow )
{
	itemsList.push_back( SItem( nID, pWindow ) );

	if ( ( nSelectedID == -1 ) && GetStyle( LVSTYLE_SHOWSELALWAYS ) )
		SetSelectedItem( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::RemoveItem( int nID )
{
	if ( nID == nSelectedID )
		nSelectedID = -1;

	for ( list<SItem>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		if ( iTemp->nID == nID )
		{
			itemsList.erase( iTemp );
			return;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::RemoveAllItems()
{
	nSelectedID = -1;
	itemsList.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CListView::GetItemsCount() const
{
	return itemsList.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CListView::GetItem( int nID ) const
{
	SItem sItem;
	if ( !FindItemByID( nID, &sItem ) )
	{
		ASSERT( 0 && "Item dosn't exist" );
		return 0;
	}

	return sItem.pWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::GetItemsList( list<CPtr<CWindow> > *pList ) const
{
	for ( list<SItem>::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
		pList->push_back( iTemp->pWindow.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CListView::GetSelectedItem() const
{
	return nSelectedID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::SetSelectedItem( int nID )
{
	SItem sSelectedItem;

	if ( ( nSelectedID != -1 ) && FindItemByID( nSelectedID, &sSelectedItem ) )
		SendMessage( sSelectedItem.pWindow, SEvent( EVENT_LISTVIEW_ITEMSELECTED, ELV_ITEMSSELECTED_FALSE ) );

	nSelectedID = nID;

	if ( ( nSelectedID != -1 ) && FindItemByID( nSelectedID, &sSelectedItem ) )
		SendMessage( sSelectedItem.pWindow, SEvent( EVENT_LISTVIEW_ITEMSELECTED, ELV_ITEMSSELECTED_TRUE ) );

	OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CListView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_CHAR:
		{
			switch( sEvent.nVal )
			{
			case VK_UP:
				if ( !itemsList.empty() )
				{
					if ( nSelectedID != -1 )
					{
						int nPrevID = nSelectedID;
						for ( list<SItem>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
						{
							if ( iTemp->nID == nSelectedID )
								SetSelectedItem( nPrevID );
							nPrevID = iTemp->nID;
						}
					}
					else
						SetSelectedItem( itemsList.front().nID );
				}
				return true;
			case VK_DOWN:
				if ( !itemsList.empty() )
				{
					if ( nSelectedID != -1 )
					{
						bool bSelectNext = false;
						for ( list<SItem>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
						{
							if ( bSelectNext )
							{
								SetSelectedItem( iTemp->nID );
								break;
							}
							if ( iTemp->nID == nSelectedID )
								bSelectNext = true;
						}
					}
					else
						SetSelectedItem( itemsList.front().nID );
				}
				return true;
			case VK_HOME:
				if ( !itemsList.empty() )
					SetSelectedItem( itemsList.front().nID );
				return true;
			case VK_END:
				if ( !itemsList.empty() )
					SetSelectedItem( itemsList.back().nID );
				return true;
			}

			break;
		}
	case EVENT_LISTVIEW_ITEMSELECTED:
		{
			SItem sItem;
			if ( IsValid( sEvent.pWindow.GetPtr() ) && FindItemByWindow( sEvent.pWindow.GetPtr(), &sItem ) )
				SetSelectedItem( sItem.nID );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	int nY = 0, nX = 0;
	for ( list<SItem>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		iTemp->pWindow->SetPosition( SPoint( iTemp->pWindow->GetPosition().x, nY ) );
		nY += iTemp->pWindow->GetSize().y;
	}

	SetSize( SPoint( GetSize().x, nY ) );

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CListView::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CListView::FindItemByID( int nID, SItem *pItem ) const
{
	for ( list<SItem>::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		if ( iTemp->nID != nID )
			continue;

		*pItem = (*iTemp);
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CListView::FindItemByWindow( CWindow *pWindow, SItem *pItem ) const
{
	for ( list<SItem>::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		if ( iTemp->pWindow != pWindow )
			continue;

		*pItem = (*iTemp);
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComboBoxItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComboBoxItem: public CButton
{
	OBJECT_NOCOPY_METHODS(CComboBoxItem)
private:
	enum ECBState
	{
		STATE_NORMAL		= 0,
		STATE_HILIGHTED	= 1,
		STATE_SELECTED	= 2,
		STATE_DISABLED	= 3,
		STATE_MAXVALUE	= 4
	};
	ZDATA_(CButton)
	int nTemplate;
	bool bUpdated;
	bool bSelected;
	CComboBox::SInfo sInfo;
	vector<CPtr<CWindow> > statesSet;
	vector<CComboBox::SInfo> comboStatesSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&nTemplate); f.Add(3,&bUpdated); f.Add(4,&bSelected); f.Add(5,&sInfo); f.Add(6,&statesSet); f.Add(7,&comboStatesSet); return 0; }

protected:
	virtual void OnAction();
	void SetState( ECBState eState );
	void CreateState( NGScene::I2DGameView *pView, CWindow *pState, const CComboBox::SInfo &sInfo, const CComboBox::SInfo &sState );

public:
	CComboBoxItem() {}
	CComboBoxItem( const SWindowInfo &sInfo, int nTemplate = -1 );

	void SetInfo( const CComboBox::SInfo &sInfo, const vector<CComboBox::SInfo> &statesSet );
	const CComboBox::SInfo& GetInfo() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CComboBoxItem::CComboBoxItem( const SWindowInfo &sInfo, int _nTemplate ):
	CButton( sInfo ), nTemplate( _nTemplate ), bUpdated( false ), bSelected( false ), statesSet( STATE_MAXVALUE )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxItem::SetInfo( const CComboBox::SInfo &_sInfo, const vector<CComboBox::SInfo> &_comboStatesSet )
{
	sInfo = _sInfo;
	comboStatesSet = _comboStatesSet;
	bUpdated = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CComboBox::SInfo& CComboBoxItem::GetInfo() const
{
	return sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComboBoxItem::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LISTVIEW_ITEMSELECTED:
		{
			if ( sEvent.nVal == ELV_ITEMSSELECTED_TRUE )
				bSelected = true;
			else
				bSelected = false;

			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxItem::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( bUpdated )
	{
		bUpdated = false;

		statesSet[STATE_NORMAL] = AddState( STATE_NORMAL );
		CreateState( pView, statesSet[STATE_NORMAL], sInfo, comboStatesSet[CComboBox::STATE_NORMAL] );
		statesSet[STATE_HILIGHTED] = AddState( STATE_HILIGHTED );
		CreateState( pView, statesSet[STATE_HILIGHTED], sInfo, comboStatesSet[CComboBox::STATE_HILIGHTED] );
		statesSet[STATE_SELECTED] = AddState( STATE_SELECTED );
		CreateState( pView, statesSet[STATE_SELECTED], sInfo, comboStatesSet[CComboBox::STATE_SELECTED] );
		statesSet[STATE_DISABLED] = AddState( STATE_DISABLED );
		CreateState( pView, statesSet[STATE_DISABLED], sInfo, comboStatesSet[CComboBox::STATE_DISABLED] );
	}

	if ( !GetStyle( STYLE_ENABLED ) )
	{
		SetState( STATE_DISABLED );
		CButton::Draw( sTime, pView );
	}

	if ( bSelected )
		SetState( STATE_SELECTED );
	else if ( IsMouseCover() )
		SetState( STATE_HILIGHTED );
	else
		SetState( STATE_NORMAL );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxItem::SetState( ECBState eState )
{
	SetSize( statesSet[eState]->GetSize() );
	CButton::SetActiveState( eState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxItem::CreateState( NGScene::I2DGameView *pView, CWindow *pState, const CComboBox::SInfo &sInfo, const CComboBox::SInfo &sState )
{
	wstring wsText = sState.wsText + sInfo.wsText;

	CPtr<CWindow> pStateView;
	if ( nTemplate != -1 )
	{
		LoadTemplate( pState, NDb::GetUIContainer( nTemplate ) );
		pStateView = GetUIWindow<CWindow>( pState, "view" );
	}
	else if ( !wsText.empty() )
	{
		CObj<CTextDraw> pTextCalc = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );
		SPoint sTextSize = pTextCalc->GetSize( pView );

		pState->SetSize( SPoint( GetParent()->GetSize().x, sTextSize.y ) );
		pStateView = new CWindow( SWindowInfo( pState, SPoint( 0, 0 ), pState->GetSize(), "view", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TOPMOST ) );
	}
	else if ( IsValid( sState.pImage ) )
	{
		pState->SetSize( SPoint( GetParent()->GetSize().x, sInfo.pImage->nHeight ) );
		pStateView = new CWindow( SWindowInfo( pState, SPoint( 0, 0 ), pState->GetSize(), "view", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TOPMOST ) );
	}

	const SPoint &sSize = pStateView->GetSize();

	CPtr<CText> pText = new CText( SWindowInfo( pStateView, SPoint( 0, 0 ), sSize, "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
	pText->SetText( wsText );

	CPtr<CImage> pImage = new CImage( SWindowInfo( pStateView, SPoint( 0, 0 ), sSize, "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
	pImage->SetColor( sInfo.sColor );
	pImage->SetImage( sInfo.pImage );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComboBoxList
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComboBoxList: public CListView
{
	OBJECT_NOCOPY_METHODS(CComboBoxList)
private:
	ZDATA_(CListView)
	CPtr<CWindow> pOwner;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CListView*)this); f.Add(2,&pOwner); return 0; }

protected:
	virtual void OnAction();

public:
	CComboBoxList() {}
	CComboBoxList( const SWindowInfo &sInfo );

	void SetOwner( CWindow *pOwner );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CComboBoxList::CComboBoxList( const SWindowInfo &sInfo ):
	CListView( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxList::SetOwner( CWindow *_pOwner )
{
	pOwner = _pOwner;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBoxList::OnAction()
{
	SendMessage( pOwner, SEvent( EVENT_NOTIFY, GetWindowID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComboBox
////////////////////////////////////////////////////////////////////////////////////////////////////
CComboBox::CComboBox( const SWindowInfo &sInfo ):
	CWindow( sInfo ), statesSet( STATE_LAST )
{
	CPtr<CComboBoxList> pComboBoxList = new CComboBoxList( SWindowInfo( GetParent(), SPoint( GetPosition().x, GetPosition().y + GetSize().y ), SPoint( GetSize().x, 0 ), "list", STYLE_ENABLED | STYLE_TOPMOST ) );
	pComboBoxList->SetOwner( this );
	pList = pComboBoxList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBox::AddItem( int nID, const SInfo &sItem, int nTemplate )
{
	CPtr<CComboBoxItem> pItem = new CComboBoxItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), nTemplate );
	pItem->SetInfo( sItem, statesSet );

	int nSelected = pList->GetSelectedItem();
	pList->AddItem( nID, pItem );
	if ( pList->GetSelectedItem() != nSelected )
		SetSelectedItem( pList->GetSelectedItem() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBox::RemoveAllItems()
{
	pList->RemoveAllItems();
	pSelected = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComboBox::GetItem( int nID, SInfo *pInfo )
{
	CPtr<CWindow> pItem = pList->GetItem( nID );
	CDynamicCast<CComboBoxItem> pComboBoxItem(pItem);
	if (pComboBoxItem)
	{
		*pInfo = pComboBoxItem->GetInfo();
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBox::SetStateInfo( EState eState, const SInfo &sInfo )
{
	statesSet[eState] = sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CComboBox::GetSelectedItem() const
{
	return pList->GetSelectedItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComboBox::SetSelectedItem( int nID )
{
	CPtr<CWindow> pItem = pList->GetItem( nID );
	CDynamicCast<CComboBoxItem> pComboBoxItem(pItem);
	if (pComboBoxItem)
	{
		if ( pList->GetSelectedItem() != nID )
			pList->SetSelectedItem( nID );

		CPtr<CComboBoxItem> pSelectedItem = new CComboBoxItem( SWindowInfo( pSelectedView, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
		pSelectedItem->SetInfo( pComboBoxItem->GetInfo(), statesSet );
		pSelected = pSelectedItem;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CComboBox::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "list" )
			{
				pMouseCapture = 0;
				pList->SetStyle( STYLE_VISIBLE, false );
				SetSelectedItem( pList->GetSelectedItem() );
				return true;
			}
			else if ( sEvent.szID == "drop_list" )
			{
				pList->SetStyle( STYLE_VISIBLE, true );
				pMouseCapture = GetInterface()->CreateMouseCapture( this );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pSelectedView = GetUIWindow<CWindow>( this, "view" );
			break;
		}
	}

	if ( IsValid( pMouseCapture ) )
	{
		switch ( sEvent.nEvent )
		{
		case EVENT_LBUTTONUP:
		case EVENT_LBUTTONDOWN:
		case EVENT_LBUTTONDBLCLK:
			{
				if ( pList->HitTest( sEvent.nX, sEvent.nY ) )
					break;

				pMouseCapture = 0;
				pList->SetStyle( STYLE_VISIBLE, false );
				return true;
			}
		case EVENT_MOUSECAPTURELOSE:
			{
				pMouseCapture = 0;
				pList->SetStyle( STYLE_VISIBLE, false );
				return true;
			}
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CProgressBar
////////////////////////////////////////////////////////////////////////////////////////////////////
CProgressBar::CProgressBar( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nImageWidth( 1 ), fValue( 1 )
{
	pImage = new CImageDraw( SRect( 0, 0, GetSize().x, GetSize().y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CProgressBar::GetValue()
{
	return fValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressBar::SetValue( float _fValue )
{
	fValue = min( max( _fValue, 0 ), 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CProgressBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATECREATE:
		{
			nImageWidth = 1;
			if ( IsValid( sEvent.pControl->pTextures[0] ) )
			{
				pImage->SetImage( sEvent.pControl->pTextures[0] );
				nImageWidth = sEvent.pControl->pTextures[0]->nWidth;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CProgressBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( GetStyle( PBARSTYLE_VERT ) )
	{
		if ( !GetStyle( PBARSTYLE_CENTERED ) )
			pImage->SetWindow( SRect( 0, 0, GetSize().x, GetSize().y * fValue ) );
		else
			pImage->SetWindow( SRect( 0, GetSize().y * ( 1.0f - fValue ) / 2.0f, GetSize().x, GetSize().y * ( 1.0f + fValue ) / 2.0f ) );

		if ( GetStyle( PBARSTYLE_SCALE ) )
			pImage->SetScale( CVec2( 1, float( GetSize().x * fValue ) / nImageWidth ) );
	}
	else
	{
		if ( !GetStyle( PBARSTYLE_CENTERED ) )
			pImage->SetWindow( SRect( 0, 0, GetSize().x * fValue, GetSize().y ) );
		else
			pImage->SetWindow( SRect( GetSize().x * ( 1.0f - fValue ) / 2.0f, 0, GetSize().x * ( 1.0f + fValue ) / 2.0f, GetSize().y ) );

		if ( GetStyle( PBARSTYLE_SCALE ) )
			pImage->SetScale( CVec2( float( GetSize().x * fValue ) / nImageWidth, 1 ) );
	}

	pImage->Draw( this, sTime, pView );
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScreenShot
////////////////////////////////////////////////////////////////////////////////////////////////////
CScreenShot::CScreenShot( const SWindowInfo &sInfo ):
	CWindow( sInfo ), eMode( COLOR ), vCoeff( 1, 1, 1, 1 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenShot::Set( const CArray2D<NGfx::SPixel8888> &sScreenShot )
{
	pTexture = new NGScene::CScreenshotTexture();
	pTexture->Set( sScreenShot );

	SetMode( eMode, vCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenShot::Generate()
{
	pTexture = new NGScene::CScreenshotTexture();
	pTexture->Generate();

	SetMode( eMode, vCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::CScreenshotTexture* CScreenShot::GetTexture() const
{
	return pTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenShot::SetTexture( NGScene::CScreenshotTexture* _pTexture )
{
	pTexture = _pTexture;

	SetMode( eMode, vCoeff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenShot::SetMode( EMode _eMode, const CVec4 &_vCoeff )
{
	eMode = _eMode;
	vCoeff = _vCoeff;

	if ( IsValid( pTexture ) )
	{
		switch( eMode )
		{
		case COLOR:
			pTexture->SetMode( NGScene::CScreenshotTexture::COLOR, vCoeff );
			break;
		case BLACKANDWHITE:
			pTexture->SetMode( NGScene::CScreenshotTexture::BLACKANDWHITE, vCoeff );
			break;
		default:
			ASSERT( 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenShot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pTexture ) )
	{
		SRect sScrWindow;
		SPoint sScrPosition;
		if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
			return;
		VirtualToScreen( &sScrPosition, &sScrWindow );

		SRect sDummyRect( sScrWindow );
		SPoint sRealSize( GetSize() );
		VirtualToScreen( &sRealSize, &sDummyRect );

		SPoint sSTSize;
		pTexture->GetSize( &sSTSize );

		CRectLayout sLayout;
		sLayout.scale = CTPoint<float>( float( sRealSize.x ) / sSTSize.x, float( sRealSize.y ) / sSTSize.y );
		sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sSTSize.x, sSTSize.y ) );
		pView->CreateDynamicRects( pTexture, sLayout, sScrPosition, sScrWindow );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVideoPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CVideoPlayer::CVideoPlayer( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Set @0x316ed0 -- build a fresh concrete player and adopt it.  The PLAY_* control
// flags are remapped to the player's internal flags by a one-bit left shift of bits 0..3.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVideoPlayer::Set( const string &szName, int nFlags )
{
	int nPlayFlags = 0;
	if ( nFlags & PLAY_LOOPED )         nPlayFlags |= 0x02;
	if ( nFlags & PLAY_FROM_MEMORY )    nPlayFlags |= 0x04;
	if ( nFlags & PLAY_WITH_SOUND )     nPlayFlags |= 0x08;
	if ( nFlags & PLAY_NO_TIME_UPDATE ) nPlayFlags |= 0x10;

	// CDGPtr assignment performs the AddRef-new / Release-old / version=0 that the
	// disasm open-codes.
	pTexture = NGScene::CreateVideoPlayer( szName, nPlayFlags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Play @0x314560 / Stop @0x314580 / IsPlaying @0x3145a0 -- forward only when live.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVideoPlayer::Play( bool bRestart )
{
	if ( IsValid( pTexture ) )
		pTexture->Play( bRestart );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVideoPlayer::Stop()
{
	if ( IsValid( pTexture ) )
		pTexture->Stop();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CVideoPlayer::IsPlaying()
{
	if ( IsValid( pTexture ) )
		return pTexture->IsPlaying();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetFrame @0x3142f0 / GetLength @0x314300 / GetFrameCount @0x3142e0
// ORIGINAL BUG (confirmed via disasm @0x3142f0: mov eax,[ecx+0x80]; jmp [eax+0x2c],
// no null test): these three forward UNCONDITIONALLY -- no IsValid guard, unlike
// Play/Stop/IsPlaying -- so a control whose player was never Set() dereferences a
// null node.  Reproduced faithfully; not reached on the boot path.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVideoPlayer::SetFrame( int nFrame )
{
	pTexture->SetCurrentFrame( nFrame );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CVideoPlayer::GetLength()
{
	return pTexture->GetLength();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CVideoPlayer::GetFrameCount()
{
	return pTexture->GetNumFrames();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw @0x315df0 -- render the current movie frame into the control's screen rect,
// then ALWAYS the base CWindow::Draw.  Modeled on the CScreenShot::Draw twin (the
// dev CRectLayout uses a scale + texel-units texcoord; CreateDynamicRects clips to
// the screen window).  The release additionally letterboxes with explicit black
// bars (CreateDynamicClearRects); for the 4:3 fullscreen intro videos the
// scale-to-fit is visually identical, so those bars are omitted.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVideoPlayer::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pTexture ) )
	{
		SRect sScrWindow;
		SPoint sScrPosition;
		if ( ClientToScreen( &sScrPosition, &sScrWindow ) )
		{
			VirtualToScreen( &sScrPosition, &sScrWindow );

			SRect sDummyRect( sScrWindow );
			SPoint sRealSize( GetSize() );
			VirtualToScreen( &sRealSize, &sDummyRect );

			SPoint sVidSize;
			pTexture->GetSize( &sVidSize );             // the movie's pixel size

			if ( sVidSize.x > 0 && sVidSize.y > 0 )
			{
				CRectLayout sLayout;
				sLayout.scale = CTPoint<float>( float( sRealSize.x ) / sVidSize.x, float( sRealSize.y ) / sVidSize.y );
				sLayout.AddRect( 0, 0, CTRect<float>( 0, 0, sVidSize.x, sVidSize.y ) );
				pView->CreateDynamicRects( pTexture, sLayout, sScrPosition, sScrWindow );
			}
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
REGISTER_SAVELOAD_CLASS_NM( 0xB0241960, CEdit, NUI );
REGISTER_SAVELOAD_CLASS_NM( 0xB0241961, CButton, NUI );
REGISTER_SAVELOAD_CLASS_NM( 0xB024196A, CComboBox, NUI );
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241962, CPushButton );
REGISTER_SAVELOAD_CLASS( 0xB0241963, CCheckButton );
REGISTER_SAVELOAD_CLASS( 0xB0241964, CToolTip );
REGISTER_SAVELOAD_CLASS( 0xB0241965, CSlider );
REGISTER_SAVELOAD_CLASS( 0xB0241966, CScroll );
REGISTER_SAVELOAD_CLASS( 0xB0241967, CListView );
REGISTER_SAVELOAD_CLASS( 0xB0241968, CComboBoxItem );
REGISTER_SAVELOAD_CLASS( 0xB0241969, CComboBoxList );
REGISTER_SAVELOAD_CLASS( 0xB024196B, CProgressBar );
REGISTER_SAVELOAD_CLASS( 0xB024196C, CScreenShot );
REGISTER_SAVELOAD_CLASS( 0xB3630160, CVideoPlayer );
