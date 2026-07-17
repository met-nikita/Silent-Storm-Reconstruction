#include "StdAfx.h"
#include "RectLayout.h"
#include "G2DView.h"
#include "FontFormat.h"
#include "GFont.h"
#include "GLocale.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
///
#include "GSceneUtils.h"
#include "Transform.h"
#include "DiscretePos.h"
//
#include "Interface.h"
#include "UIWrap.h"
#include "UIML.h"
#include "UIMLHandlers.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SFontInfo + GetFontFormatInfo -- retail NUI::GetFontFormatInfo @0x321200 (a FREE function in the
// retail binary, shared by the ML text leaves): resolve the viewport-scaled font.
// Retail deltas vs the old dev member version: the point-size is ROUNDED (not truncated), there is
// no ASSERT arm for a flag-less size, and the resolved size is clamped up to nMinFontSize
// (`if (size <= nMinFontSize) size = nMinFontSize` -- the SState "minfontsize" support).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFontInfo
{
	CVec2 scale;
	CPtr<CFontFormatInfo> pInfo;
	CPtr<NGScene::CFontInfo> pFont;
	int operator&( CStructureSaver &f ) { ASSERT( 0 ); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetFontFormatInfo( NGScene::I2DGameView *pView, const NGScene::SFont &sFont, int nMinFontSize, SFontInfo *pFontInfo )
{
	CVec2 vScreen = pView->GetViewportSize();
	NGScene::SFont sSearch( sFont );

	sSearch.nSize = sFont.nSize & FONT_SIZE_MASK;
	if ( sFont.nSize & FONT_SIZE_POINTS )
		sSearch.nSize = Float2Int( float( sFont.nSize & FONT_SIZE_MASK ) * vScreen.x / 1024.0f );   // retail ROUND @0x72123c
	// FONT_SIZE_PIXELS (and a flag-less size): the raw masked value -- retail has no ASSERT arm.
	if ( sSearch.nSize <= nMinFontSize )   // retail clamp to the SState minimum @0x721250
		sSearch.nSize = nMinFontSize;

	CPtr<NGScene::CFontInfo> pFont = pView->GetLocaleInfo()->GetFont( sSearch );
	CDGPtr< CPtrFuncBase<CFontFormatInfo> > pInfo( pFont->GetFormatInfo() );
	pInfo.Refresh();
	pFontInfo->pFont = pFont;
	pFontInfo->pInfo = pInfo->GetValue();

	float fScale = (float)sSearch.nSize / pFontInfo->pInfo->GetLineSpace();
	pFontInfo->scale.x = fScale;
	pFontInfo->scale.y = fScale;
	// WIDESCREEN (Sentinels @0x44cbcc-0x44cbdc): scale.y = fScale * (vp.y/vp.x) * 4/3, so glyph
	// height tracks vp.y/768 while width keeps vp.x/1024. Gated wider-than-4:3 => 4:3/5:4 bit-identical.
	if ( vScreen.x * 3.0f > vScreen.y * 4.0f )
		pFontInfo->scale.y = fScale * ( vScreen.y * 1024.0f ) / ( vScreen.x * 768.0f );

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLTextObject -- retail @UIML.obj, saveload id 0xB0829160. STREAM-BASED in retail: the text run is
// the slice [nStrStart, nStrStart+nStrSize) of the owning CML's CMLStream (weak CPtr back-ref); the
// old dev inline `wstring wsText` copy is gone. operator& @0x326040: 2=nStrSize, 3=nStrStart,
// 4=pStream, 5=sState, 6=sSize, 7=sPosition, 8=edges, 9=sNormal, 10=sOutline, 11=pTexture.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLTextObject: public IMLObject
{
	OBJECT_BASIC_METHODS(CMLTextObject)
private:
	ZDATA
	int nStrSize;
	int nStrStart;
	CPtr<CMLStream> pStream;
	////
	SState sState;
	CTPoint<float> sSize;       // retail PDB +0x44: CTPoint<float> (was wrongly SPoint/int in dev --
	CTPoint<float> sPosition;   // +0x4c; the save's 8-byte tag-6/7 chunks are float on the wire)
	////
	list<float> edges;
	CRectLayout sNormal;
	CRectLayout sOutline;
	CObj<CPtrFuncBase<NGfx::CTexture> > pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nStrSize); f.Add(3,&nStrStart); f.Add(4,&pStream); f.Add(5,&sState); f.Add(6,&sSize); f.Add(7,&sPosition); f.Add(8,&edges); f.Add(9,&sNormal); f.Add(10,&sOutline); f.Add(11,&pTexture); return 0; }

public:
	// retail default ctor @0x324c60 leaves the slice indices uninitialized (only the save/load path
	// builds one and operator& fills them); zero-init here, observably identical.
	CMLTextObject(): nStrSize( 0 ), nStrStart( 0 ) {}
	// retail streaming ctor @0x320d60: store the slice + the (AddRef'd) source stream; sSize seeds
	// to the 20x20 default exactly like the old wstring ctor.
	CMLTextObject( CMLStream *pStream, int nStrStart, int nStrSize );

	void Generate( NGScene::I2DGameView *pView );

	const CTPoint<float>& GetSize() const;

	const SState& GetState() const;
	void SetState( const SState &sState );

	const CTPoint<float>& GetPosition() const;
	void SetPosition( const CTPoint<float> &sPosition );

	void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow );
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMLTextObject::CMLTextObject( CMLStream *_pStream, int _nStrStart, int _nStrSize ):
	pStream( _pStream ), nStrStart( _nStrStart ), nStrSize( _nStrSize )
{
	sSize = CTPoint<float>( 20, 20 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMLTextObject::Generate @0x321430: stream the slice into glyph rects. Retail deltas vs the
// old dev body: text comes from pStream at its cursor (seeded to nStrStart), the outline border is
// point->screen scaled (nOutlineBorder * viewportW / 1024) instead of the raw int, the lead/trail
// borders are suppressed under bForceFontSize (the outline ring keeps the full offset), and a TAB
// (wchar 9) snaps the pen to the next multiple-of-4 tab stop of the ave-char cell width.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::Generate( NGScene::I2DGameView *pView )
{
	SFontInfo sFontInfo;
	GetFontFormatInfo( pView, sState.sFont, sState.nMinFontSize, &sFontInfo );

	sSize.y = sFontInfo.pInfo->GetLineSpace() * sFontInfo.scale.y;   // float store (retail @0x7214a4)
	pTexture = sFontInfo.pFont->GetTexture();

	CMLStream *pStr = pStream;
	pStr->nPos = nStrStart;   // retail seeds the stream cursor unconditionally

	// outline border in screen pixels: nOutlineBorder * viewportW / 1024 (retail @0x7218e0)
	const CVec2 vScreen = pView->GetViewportSize();
	const float fS = float( sState.nOutlineBorder ) * vScreen.x / 1024.0f;
	// WIDESCREEN (Sentinels @0x44cd34-0x44cd3e): the ring's vertical offset scales by vp.y/768
	const float fSY = ( vScreen.x * 3.0f > vScreen.y * 4.0f )
		? float( sState.nOutlineBorder ) * vScreen.y / 768.0f : fS;

	float fX = 0;
	WCHAR wcLastChar = 0;
	for ( int nTemp = 0; nTemp < nStrSize; nTemp++ )
	{
		WCHAR wcChar = ( pStr->nPos < (int)pStr->wsText.size() ) ? pStr->wsText[pStr->nPos] : 0;
		const STFCharacter &sCharacter = sFontInfo.pInfo->GetChar( wcChar );   // queried even for a tab (retail)

		bool bEmitted = false;
		if ( wcChar == 9 )
		{
			// TAB: snap the pen to the next multiple-of-4 stop of the tab cell width
			// (the two borders unless bForceFontSize, plus the ave-char advance).
			float fTabBase = ( sState.bForceFontSize ? 0.0f : fS + fS )
				+ float( sFontInfo.pInfo->GetAveCharWidth() ) * sFontInfo.scale.x;
			if ( fTabBase != 0 )
			{
				int q = Float2Int( fX / fTabBase );
				fX = float( ( q / 4 + 1 ) * 4 ) * fTabBase;
				bEmitted = true;
			}
			// ORIGINAL BUG (confirmed retail @0x721943): a zero tab cell width neither advances the
			// stream cursor nor emits an edge -- the remaining iterations re-read the same tab.
		}
		else
		{
			fX += sState.bForceFontSize ? 0.0f : fS;   // leading border (suppressed under bForceFontSize)
			fX += ( sCharacter.nA + sFontInfo.pInfo->GetKern( wcChar, wcLastChar ) ) * sFontInfo.scale.x;
			// retail @0x721430 bakes the glyph quad size into each rect: (x2-x1)*scale.x, (y2-y1)*scale.y
			const CTRect<float> sTexRect( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 );
			const float fSizeX = ( sTexRect.x2 - sTexRect.x1 ) * sFontInfo.scale.x;
			const float fSizeY = ( sTexRect.y2 - sTexRect.y1 ) * sFontInfo.scale.y;
			sNormal.AddRect( fX, 0, fSizeX, fSizeY, sTexRect, sState.sColor );

			if ( sState.nOutlineBorder )
			{
				// 8 ring copies at +-fS/+-fSY (the offsets keep the full scaled border even under bForceFontSize -- retail)
				sOutline.AddRect( fX, +fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX, -fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX + fS, 0, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX - fS, 0, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX + fS, +fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX + fS, -fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX - fS, +fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
				sOutline.AddRect( fX - fS, -fSY, fSizeX, fSizeY, sTexRect, sState.sOutlineColor );
			}

			fX += sCharacter.nBC * sFontInfo.scale.x;
			fX += sState.bForceFontSize ? 0.0f : fS;   // trailing border
			bEmitted = true;
		}

		if ( bEmitted )
		{
			edges.push_back( fX );
			pStr->nPos = pStr->nPos + 1;
			wcLastChar = wcChar;
		}
	}

	sSize.x = fX;   // float store (retail @0x721967)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<float>& CMLTextObject::GetSize() const
{
	return sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SState& CMLTextObject::GetState() const
{
	return sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<float>& CMLTextObject::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::SetState( const SState &_sState )
{
	sState = _sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::SetPosition( const CTPoint<float> &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x320ac0 builds float rects (list<CTRect<float>>); dev's pRender list is int SRect (see
// the UIML.h boundary note) -- the float sums narrow only at the push_back.
void CMLTextObject::Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	const float fPosX = sGlobalPosition.x + sPosition.x;
	const float fPosY = sGlobalPosition.y + sPosition.y;

	float fLastX = 0;
	for ( list<float>::const_iterator iTemp = edges.begin(); iTemp != edges.end(); iTemp++ )
	{
		pRender->push_back( SRect( fLastX + fPosX, fPosY, *iTemp + fPosX, sSize.y + fPosY ) );
		fLastX = *iTemp;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3207c0 passes the float position straight into CreateDynamicRects (float in retail);
// dev's CreateDynamicRects is int -- narrow with the engine's fistp idiom at the boundary.
void CMLTextObject::Render( NGScene::I2DGameView *pView, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	SPoint sPos( Float2Int( sGlobalPosition.x + sPosition.x ), Float2Int( sGlobalPosition.y + sPosition.y ) );
	if ( !sOutline.rects.empty() )
		pView->CreateDynamicRects( pTexture, sOutline, sPos, sWindow );

	pView->CreateDynamicRects( pTexture, sNormal, sPos, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLImageObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLImageObject: public IMLObject
{
	OBJECT_BASIC_METHODS(CMLImageObject)
private:
	ZDATA
	int nBorder;
	int nWidth;
	int nHeight;
	SState::EHORAlign eAlign;
	CDBPtr<NDb::CUITexture> pUITexture;
	////
	SState sState;
	CTPoint<float> sSize;       // retail PDB +0x4c: CTPoint<float> (float on the wire, tags 8/9)
	CTPoint<float> sPosition;   // +0x54
	////
	CRectLayout sLayout;
	CDBPtr<NDb::CTexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBorder); f.Add(3,&nWidth); f.Add(4,&nHeight); f.Add(5,&eAlign); f.Add(6,&pUITexture); f.Add(7,&sState); f.Add(8,&sSize); f.Add(9,&sPosition); f.Add(10,&sLayout); f.Add(11,&pTexture); return 0; }

public:
	CMLImageObject() {}
	CMLImageObject( NDb::CUITexture *pUITexture, SState::EHORAlign eAlign, int nBorder, int nWidth, int nHeight );

	void Generate( NGScene::I2DGameView *pView );

	const CTPoint<float>& GetSize() const;

	const SState& GetState() const;
	void SetState( const SState &sState );

	const CTPoint<float>& GetPosition() const;
	void SetPosition( const CTPoint<float> &sPosition );

	void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow );
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IMLObject* CreateIMLImageObject( NDb::CUITexture *pUITexture, SState::EHORAlign eAlign, int nBorder, int nWidth, int nHeight )
{
	return new CMLImageObject( pUITexture, eAlign, nBorder, nWidth, nHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMLImageObject::CMLImageObject( NDb::CUITexture *_pUITexture, SState::EHORAlign _eAlign, int _nBorder, int _nWidth, int _nHeight ):
	pUITexture( _pUITexture ), eAlign( _eAlign ), nBorder( _nBorder ), nWidth( _nWidth ), nHeight( _nHeight ), sSize( 0, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::Generate( NGScene::I2DGameView *pView )
{
	if ( IsValid( pUITexture ) )
	{
		sSize.x = pUITexture->nWidth;
		sSize.y = pUITexture->nHeight;

		CVec2 vImageMode( 1024, 768 );
		NDb::EUIMode eMode = NDb::UIM_1024x768;
		switch( Float2Int( pView->GetViewportSize().x ) )
		{
			case 1600:
				eMode = NDb::UIM_1600x1200;
				break;
			case 1280:
				eMode = NDb::UIM_1280x1024;
				break;
			case 1024:
				eMode = NDb::UIM_1024x768;
				break;
			case 800:
				eMode = NDb::UIM_800x600;
				break;
		}

		CVec2 vScale( 1.0f, 1.0f );
		if ( !IsValid( pUITexture->pTextures[eMode] ) )
		{
			eMode = NDb::UIM_1024x768;
			vScale.x = pView->GetViewportSize().x / 1024.0f;
			vScale.y = pView->GetViewportSize().y / 768.0f;
		}

		pTexture = pUITexture->pTextures[eMode];
		if ( IsValid( pTexture ) )
		{
			if ( ( pTexture->nWidth == 0 ) || ( pTexture->nHeight == 0 ) )
				return;

			if ( nWidth != -1 )
			{
				sSize.x = nWidth;
				vScale.x = vScale.x * nWidth / pUITexture->nWidth;
			}
			if ( nHeight != -1 )
			{
				sSize.y = nHeight;
				vScale.y = vScale.y * nHeight / pUITexture->nHeight;
			}

			CTRect<float> sTexRect;
			sTexRect.x1 = 0;
			sTexRect.x2 = pTexture->nWidth;
			sTexRect.y1 = pTexture->nHeight;
			sTexRect.y2 = 0;
			// retail @0x720ec0 (call @0x721093): quad size = texture dims * vScale, baked per rect;
			// colour = opaque white (0xFFFFFFFF at [esp+0x30] in the retail frame)
			sLayout.AddRect( 0, 0, pTexture->nWidth * vScale.x, pTexture->nHeight * vScale.y, sTexRect );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<float>& CMLImageObject::GetSize() const
{
	return sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SState& CMLImageObject::GetState() const
{
	return sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::SetState( const SState &_sState )
{
	sState = _sState;
	if ( eAlign != SState::HORALIGN_DEFAULT )
		sState.eHAlign = eAlign;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<float>& CMLImageObject::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::SetPosition( const CTPoint<float> &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x320b50 (float rect list in retail; narrows at the int push_back boundary here)
void CMLImageObject::Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	pRender->push_back( SRect( sGlobalPosition.x, sGlobalPosition.y, sSize.x + sGlobalPosition.x, sSize.y + sGlobalPosition.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x320840 (float position in retail; fistp-narrowed at the dev int CreateDynamicRects boundary)
void CMLImageObject::Render( NGScene::I2DGameView *pView, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	SPoint sPos( Float2Int( sGlobalPosition.x + sPosition.x ), Float2Int( sGlobalPosition.y + sPosition.y ) );
	pView->CreateDynamicRects( pTexture, sLayout, sPos, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLTabObject -- retail NUI::CMLTabObject (saveload id 0xB3716160; ctor @0x3208f0, operator&
// @0x3254c0, DynamicGenerate @0x321a60): the <tab> reflow object CTABHandler mints. Sized per
// reflow pass against the LIVE pen: it spans from the current line width to the next multiple-of-4
// stop of the ave-char tab cell.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLTabObject: public IMLObject
{
	OBJECT_BASIC_METHODS(CMLTabObject)
private:
	ZDATA
	SState sState;
	CTPoint<float> sSize;       // retail PDB +0x38: CTPoint<float>
	CTPoint<float> sPosition;   // +0x40
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sState); f.Add(3,&sSize); f.Add(4,&sPosition); return 0; }

public:
	CMLTabObject(): sSize( 0, 0 ) {}

	void Generate( NGScene::I2DGameView *pView ) {}   // retail: no static sizing -- DynamicGenerate does it all

	// retail DynamicGenerate @0x321a60: tab cell = 4 * ave-char width (the same +0x3c field the
	// literal-tab arm of CMLTextObject::Generate reads), scaled by the font's y scale (retail
	// fmul [esp+8] = scale.y HERE, scale.x in the text-object arm -- kept faithful); the object
	// spans the gap from the current pen to the next multiple-of-4 stop. Height = the previous
	// line's height. nStop TRUNCATES (retail or-ah-0xc fistp @0x721acd -- unlike the text-object
	// tab arm's round-to-nearest fistp), and unlike that arm there is NO fStep==0 guard in retail
	// (@0x721a60 is straight-line); sSize.x/y are plain float stores now.
	void DynamicGenerate( NGScene::I2DGameView *pView, const SReflowInfo &sInfo )
	{
		SFontInfo sFontInfo;
		GetFontFormatInfo( pView, sState.sFont, sState.nMinFontSize, &sFontInfo );

		float fStep = float( sFontInfo.pInfo->GetAveCharWidth() * 4 ) * sFontInfo.scale.y;
		sSize.y = sInfo.fLastLineHeight;
		int nStop = int( sInfo.fLineWidth / fStep ) + 1;
		sSize.x = float( nStop ) * fStep - sInfo.fLineWidth;
	}

	const CTPoint<float>& GetSize() const { return sSize; }

	const SState& GetState() const { return sState; }
	void SetState( const SState &_sState ) { sState = _sState; }

	const CTPoint<float>& GetPosition() const { return sPosition; }
	void SetPosition( const CTPoint<float> &_sPosition ) { sPosition = _sPosition; }

	void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow ) {}
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IMLObject* CreateIMLTabObject()
{
	return new CMLTabObject;   // retail NUI::CreateIMLTabObject @0x320ba0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLLayout
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLLayout: public IMLLayout
{
	OBJECT_BASIC_METHODS(CMLLayout)
private:
	// (SRange/SReflowInfo hoisted to namespace scope in UIML.h -- retail NUI::SReflowInfo is
	// namespace-scope and IMLObject::DynamicGenerate takes it.)
	struct SCmdPair
	{
		ZDATA
		ECommand eCmd;
		CPtr<IMLObject> pObject;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eCmd); f.Add(3,&pObject); return 0; }

		SCmdPair() {}
		SCmdPair( ECommand _eCmd, IMLObject *_pObject ): eCmd( _eCmd ), pObject( _pObject ) {}
	};
	ZDATA
	CTPoint<float> sSize;   // retail PDB +0x38: CTPoint<float> (float on the wire, tag 3)
	SState sState;
	list<SCmdPair> itemsList;
	// retail CMLLayout::operator& @0x326e40 = {2 sState(SState,55B), 3 sSize(8B), 4 itemsList}. dev had
	// sState/sSize SWAPPED, so sState (55B) read from the save's 8-byte sSize chunk -> over-read 47 bytes of
	// the adjacent chunk into sState's CObj/CPtr members -> ref-less wild ptrs -> double-free (~CImage) on
	// the deserialize object-table teardown (CStructureSaver::Finish objects.clear).
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sState); f.Add(3,&sSize); f.Add(4,&itemsList); return 0; }

protected:
	void CreateLine( SReflowInfo *pInfo, float fWidth );
	void AssembleLine( SReflowInfo *pInfo );
	void ProcessWraped( SReflowInfo *pInfo );

public:
	CMLLayout();

	void AddObject( IMLObject *pObject );
	void AddCommand( ECommand eCommand, IMLObject *pObject = 0 );

	const CTPoint<float>& GetSize() const;

	const SState& GetState();
	void SetState( const SState &sState );

	void Generate( NGScene::I2DGameView *pView, float fWidth );

	void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow );
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMLLayout::CMLLayout():
	sSize( 0, 0 )
{
	sState.sFont = NGScene::SFont( 16 | FONT_SIZE_POINTS, "System" );
	sState.sColor = NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF );
	sState.eHAlign = SState::HORALIGN_LEFT;
	sState.eVAlign = SState::VERTALIGN_MIDDLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::AddObject( IMLObject *pObject )
{
	itemsList.push_back( SCmdPair( CMD_NULL, pObject ) );
	pObject->SetState( sState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::AddCommand( ECommand eCommand, IMLObject *pObject )
{
	itemsList.push_back( SCmdPair( eCommand, pObject ) );
	if ( IsValid( pObject ) )
		pObject->SetState( sState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTPoint<float>& CMLLayout::GetSize() const
{
	return sSize;   // retail @0x320350: the raw float size; CML::GetSize does the pixel rounding
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SState& CMLLayout::GetState()
{
	return sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::SetState( const SState &_sState )
{
	sState = _sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x321fe0 -- FLOAT width param (int->float conversion lives in CML::Generate @0x322320);
// the whole reflow cursor is float, sLeft/sRight heights seed to -1.0f, and the final sSize is a
// plain float store of (fMaxX, fY) -- no rounding anywhere in the reflow.
void CMLLayout::Generate( NGScene::I2DGameView *pView, float fWidth )
{
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CPtr<IMLObject> pObject = iTemp->pObject;

		if ( IsValid( pObject ) )
			pObject->Generate( pView );
	}

	SReflowInfo sInfo;
	sInfo.fY = 0;
	sInfo.fMaxX = 0;
	sInfo.fLineWidth = sInfo.fLineHeight = 0;
	sInfo.fLastLineHeight = 0;
	sInfo.sLeft = SRange( 0, -1.0f );
	sInfo.sRight = SRange( fWidth, -1.0f );
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		ECommand eCommand = iTemp->eCmd;
		if ( eCommand == CMD_BREAKLINE )
			CreateLine( &sInfo, fWidth );

		CPtr<IMLObject> pObject = iTemp->pObject;
		if ( IsValid( pObject ) )
		{
			// retail @0x321fe0: size every object against the LIVE reflow cursor before reading it
			// (only CMLTabObject reacts -- the pen-relative tab-stop gap).
			pObject->DynamicGenerate( pView, sInfo );

			const CTPoint<float> &sSize = pObject->GetSize();
			const SState &sState = pObject->GetState();

			switch( sState.eHAlign )
			{
			case SState::HORALIGN_DEFAULT:
			case SState::HORALIGN_LEFT:
			case SState::HORALIGN_RIGHT:
			case SState::HORALIGN_CENTER:
			case SState::HORALIGN_JUSTIFY:
				if ( ( sInfo.fLineWidth + sSize.x ) > ( sInfo.sRight.fValue - sInfo.sLeft.fValue ) )
					CreateLine( &sInfo, fWidth );

				if ( ( eCommand == CMD_SPACE ) && sInfo.line.empty() )
					break;

				sInfo.line.push_back( pObject );
				sInfo.fLineWidth += sSize.x;
				sInfo.fLineHeight = max( sSize.y, sInfo.fLineHeight );
				break;
			case SState::HORALIGN_NOWRAP:
				// retail @0x321fe0: appended like LEFT/DEFAULT but WITHOUT the overflow pre-break --
				// a nowrap line never auto-wraps (AssembleLine has no NOWRAP case either; it falls
				// to the left-aligned arm exactly like the dev unmatched-case behaviour).
				if ( ( eCommand == CMD_SPACE ) && sInfo.line.empty() )
					break;

				sInfo.line.push_back( pObject );
				sInfo.fLineWidth += sSize.x;
				sInfo.fLineHeight = max( sSize.y, sInfo.fLineHeight );
				break;
			case SState::HORALIGN_WRAP_LEFT:
				sInfo.leftWraped.push_back( pObject );
				break;
			case SState::HORALIGN_WRAP_RIGHT:
				sInfo.rightWraped.push_back( pObject );
				break;
			default:
				ASSERT( 0 );
				break;
			}
		}
	}

	sSize.x = sInfo.fMaxX;
	sSize.y = sInfo.fY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow )
{
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CPtr<IMLObject> pObject = iTemp->pObject;

		if ( IsValid( pObject ) )
			pObject->Render( pRender, sPosition, sWindow );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow )
{
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CPtr<IMLObject> pObject = iTemp->pObject;

		if ( IsValid( pObject ) )
			pObject->Render( pView, sPosition, sWindow );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x321c90 (float throughout; the fLineHeight != 0 test is the retail NAN-idiom compare)
void CMLLayout::CreateLine( SReflowInfo *pInfo, float fWidth )
{
	AssembleLine( pInfo );

	pInfo->line.clear();

	if ( pInfo->fLineHeight != 0 )
	{
		pInfo->fY += pInfo->fLineHeight;
		pInfo->fLastLineHeight = pInfo->fLineHeight;
	}
	else
		pInfo->fY += pInfo->fLastLineHeight;

	pInfo->fLineWidth = 0;
	pInfo->fLineHeight = 0;

	ProcessWraped( pInfo );
	if ( pInfo->fY > pInfo->sLeft.fHeight )
		pInfo->sLeft.fValue = 0;
	if ( pInfo->fY > pInfo->sRight.fHeight )
		pInfo->sRight.fValue = fWidth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x320950 -- all-float line assembly: CENTER/MIDDLE use *0.5f (no int halving), positions
// are stored as raw floats via SetPosition(CTPoint<float>), and fMaxX tracks the float pen.
void CMLLayout::AssembleLine( SReflowInfo *pInfo )
{
	if ( pInfo->line.empty() )
		return;

	float fX = pInfo->sLeft.fValue;
	float fSpace = 0;
	SState::EHORAlign eHAlign = pInfo->line.front()->GetState().eHAlign;
	switch( eHAlign )
	{
	case SState::HORALIGN_RIGHT:
		// retail operand order: ((right - left) + left) - lineWidth
		fX = ( ( pInfo->sRight.fValue - pInfo->sLeft.fValue ) + pInfo->sLeft.fValue ) - pInfo->fLineWidth;
		fSpace = 0;
		break;
	case SState::HORALIGN_CENTER:
		fX = ( ( pInfo->sRight.fValue - pInfo->sLeft.fValue ) - pInfo->fLineWidth ) * 0.5f + pInfo->sLeft.fValue;
		fSpace = 0;
		break;
	case SState::HORALIGN_JUSTIFY:
		fX = pInfo->sLeft.fValue;
		fSpace = ( ( pInfo->sRight.fValue - pInfo->sLeft.fValue ) - pInfo->fLineWidth ) / float( pInfo->line.size() );
		break;
	}

	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->line.begin(); iTemp != pInfo->line.end(); iTemp++ )
	{
		const CTPoint<float> &sSize = (*iTemp)->GetSize();
		SState::EVERTAlign eVAlign = (*iTemp)->GetState().eVAlign;

		switch( eVAlign )
		{
		case SState::VERTALIGN_TOP:
			(*iTemp)->SetPosition( CTPoint<float>( fX, pInfo->fY ) );
			break;
		case SState::VERTALIGN_BOTTOM:
			(*iTemp)->SetPosition( CTPoint<float>( fX, ( pInfo->fLineHeight + pInfo->fY ) - sSize.y ) );
			break;
		default:
			(*iTemp)->SetPosition( CTPoint<float>( fX, ( pInfo->fLineHeight - sSize.y ) * 0.5f + pInfo->fY ) );
			break;
		}

		fX += (*iTemp)->GetSize().x;
		pInfo->fMaxX = Max( pInfo->fMaxX, fX );
		fX += fSpace;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x321b50 (float throughout)
void CMLLayout::ProcessWraped( SReflowInfo *pInfo )
{
	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->leftWraped.begin(); iTemp != pInfo->leftWraped.end(); iTemp++ )
	{
		const CTPoint<float> &sSize = (*iTemp)->GetSize();

		(*iTemp)->SetPosition( CTPoint<float>( pInfo->sLeft.fValue, pInfo->fY ) );
		pInfo->fMaxX = Max( pInfo->fMaxX, sSize.x + pInfo->sLeft.fValue );

		pInfo->sLeft.fValue += sSize.x;
		pInfo->sLeft.fHeight = max( pInfo->sLeft.fHeight, sSize.y + pInfo->fY );
	}
	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->rightWraped.begin(); iTemp != pInfo->rightWraped.end(); iTemp++ )
	{
		const CTPoint<float> &sSize = (*iTemp)->GetSize();

		(*iTemp)->SetPosition( CTPoint<float>( pInfo->sRight.fValue - sSize.x, pInfo->fY ) );
		pInfo->fMaxX = Max( pInfo->fMaxX, pInfo->sRight.fValue );

		pInfo->sRight.fValue -= sSize.x;
		pInfo->sRight.fHeight = max( pInfo->sRight.fHeight, sSize.y + pInfo->fY );
	}

	pInfo->leftWraped.clear();
	pInfo->rightWraped.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CML
////////////////////////////////////////////////////////////////////////////////////////////////////
class CML: public IML
{
	OBJECT_BASIC_METHODS(CML)
private:
	ZDATA
	// retail operator& @0x326970: 2=sSize (DataChunk), 3=wsText (string chunk), 4=pLayout,
	// 5=pStream, 6=tagsMap (convergence W4: dev was 2=wsText/3=pLayout/4=tagsMap).
	mutable SPoint sSize;   // retail CML caches its rounded pixel size here (GetSize @0x320880 writes it)
	wstring wsText;
	CObj<CMLLayout> pLayout;
	CObj<CMLStream> pStream;   // the parse stream (rebuilt by Generate, seeded from wsText)
	unordered_map<wstring,CObj<IMLHandler> > tagsMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSize); f.Add(3,&wsText); f.Add(4,&pLayout); f.Add(5,&pStream); f.Add(6,&tagsMap); return 0; }

public:
	CML();

	void SetText( const wstring &wsText, int nFlags );
	void SetHandler( const wstring &wsTAG, IMLHandler *pHandler );

	CMLStream* GetStream() { return pStream; }   // retail IML vtbl+0x18

	const SPoint& GetSize() const;

	void Generate( NGScene::I2DGameView *pView, int nWidth );

	void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow );
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CML::CML()
{
	pLayout = new CMLLayout();

	tagsMap[L"br"] = new CBRHandler;

	tagsMap[L"left"] = new CLEFTHandler;
	tagsMap[L"right"] = new CRIGHTHandler;
	tagsMap[L"center"] = new CCENTERHandler;
	tagsMap[L"nowrap"] = new CNOWRAPHandler;		// retail CML::CML @0x3226b0 installs 16 handlers; these
	tagsMap[L"justify"] = new CJUSTIFYHandler;
	tagsMap[L"wrapleft"] = new CWRAPLEFTHandler;
	tagsMap[L"wrapright"] = new CWRAPRIGHTHandler;
	tagsMap[L"top"] = new CTOPHandler;
	tagsMap[L"bottom"] = new CBOTTOMHandler;
	tagsMap[L"middle"] = new CMIDDLEHandler;
	tagsMap[L"tab"] = new CTABHandler;				// ... three were the dev-missing ones (nowrap/tab/minfontsize)

	tagsMap[L"font"] = new CFONTHandler;
	tagsMap[L"color"] = new CCOLORHandler;
	tagsMap[L"image"] = new CIMAGEHandler;
	tagsMap[L"minfontsize"] = new CMINFONTSIZEHandler;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CML::SetText( const wstring &_wsText, int nFlags )
{
	wsText = _wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CML::SetHandler( const wstring &wsTAG, IMLHandler *pHandler )
{
	tagsMap[wsTAG] = pHandler;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CML::GetSize @0x320880 -- THE single float->int boundary of the ML pipeline: refresh the
// cached int sSize (PDB CTPoint<int> +0xc, the serialized tag-2 value) from the layout's float size.
// Rounding proof (disasm @0x720890-0x7208be): fadd 0.5f, fstp to a SINGLE-precision temp, then a
// bare fistp (round-to-nearest, NO control-word change) -- i.e. Float2Int( x + 0.5f ), not a
// truncating cast.
const SPoint& CML::GetSize() const
{
	const CTPoint<float> &sLayoutSize = pLayout->GetSize();
	sSize.x = Float2Int( sLayoutSize.x + 0.5f );
	sSize.y = Float2Int( sLayoutSize.y + 0.5f );
	return sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CML::Generate @0x322320 -- STREAM-BASED in retail: a fresh CMLLayout AND a fresh CMLStream
// are made, the stream is seeded with wsText, and the tokenizer walks the STREAM at its caret
// (pStream->nPos); word/space runs become CMLTextObject slices over the stream, in-tag runs are
// pulled out via CMLStream::GetString. Handlers get `this` (retail Exec's leading IML*) so e.g.
// CVALUEHandler can InsertString at the caret and have the substitution parsed inline.
// Retail also DROPPED the Jan03/dev `CHAR_EOL -> AddCommand(CMD_SPACE, " ")` flush arm, and toggles
// the tag state / runs the handler keyed on the CURRENT char class after the caret advance.
void CML::Generate( NGScene::I2DGameView *pView, int nWidth )
{
	enum ECharType
	{
		CHAR_NULL,
		CHAR_SPACE,
		CHAR_ALNUM,
		CHAR_PUNCTUATION,
		CHAR_EOL,
		////
		CHAR_TAG_END,
		CHAR_TAG_BEGIN
	};

	pLayout = new CMLLayout();
	pStream = new CMLStream();
	pStream->nPos = 0;
	pStream->InsertString( wsText );

	int nWordBegin = 0;
	bool bTAG = false, bBracketsBlock = false;
	ECharType eThisChar = CHAR_NULL, eLastChar = CHAR_NULL;
	vector<wstring> paramsSet;
	bool bNotEnd = true;
	do
	{
		WCHAR wcChar = 0;
		if ( pStream->nPos < pStream->nSize )
			wcChar = pStream->wsText[pStream->nPos];
		else
			bNotEnd = false;   // the terminal iteration parses the implicit '\0' (CHAR_EOL)

		eThisChar = CHAR_NULL;
		if ( wcChar == '>' )
			eThisChar = CHAR_TAG_END;
		else if ( wcChar == '<' )
			eThisChar = CHAR_TAG_BEGIN;
		else if ( bTAG && ( wcChar == '\"' ) )
			bBracketsBlock = !bBracketsBlock;
		else if ( iswalnum( wcChar ) != 0 )
			eThisChar = CHAR_ALNUM;
		else if ( iswpunct( wcChar ) )
			eThisChar = bTAG ? CHAR_PUNCTUATION : CHAR_ALNUM; //// CRAP: Tag need separate '='
		else if  ( !bBracketsBlock && ( wcChar == L' ' ) )
			eThisChar = CHAR_SPACE;
		else if  ( bBracketsBlock && ( wcChar == L' ' ) )
			eThisChar = eLastChar;
		else if ( ( wcChar == L'\0' ) || ( wcChar == L'\n' ) )
			eThisChar = CHAR_EOL;

		if ( eLastChar != eThisChar )
		{
			if ( bTAG )
			{
				if ( ( eLastChar == CHAR_ALNUM ) || ( eLastChar == CHAR_PUNCTUATION ) )
				{
					wstring wsParam;
					pStream->GetString( nWordBegin, pStream->nPos - nWordBegin, &wsParam );
					paramsSet.push_back( wsParam );
				}
			}
			else if ( ( eLastChar == CHAR_ALNUM ) || ( eLastChar == CHAR_PUNCTUATION ) )
				pLayout->AddObject( new CMLTextObject( pStream, nWordBegin, pStream->nPos - nWordBegin ) );
			else if ( eLastChar == CHAR_SPACE )
				pLayout->AddCommand( CMD_SPACE, new CMLTextObject( pStream, nWordBegin, pStream->nPos - nWordBegin ) );
			// (retail dropped the Jan03 CHAR_EOL -> CMD_SPACE " " arm)

			nWordBegin = pStream->nPos;
		}

		pStream->nPos = pStream->nPos + 1;

		// tag open/close keyed on the CURRENT class, after the caret advance (retail order): a '>'
		// runs the handler with the caret past it, so an InsertString lands right after the tag.
		if ( eThisChar == CHAR_TAG_END )
		{
			bTAG = false;
			if ( !paramsSet.empty() )
			{
				unordered_map<wstring,CObj<IMLHandler> >::const_iterator iTemp = tagsMap.find( paramsSet.front() );
				if ( ( iTemp != tagsMap.end() ) && ( iTemp->second != 0 ) )
					iTemp->second->Exec( this, pLayout, paramsSet );
			}
		}
		else if ( eThisChar == CHAR_TAG_BEGIN )
		{
			bTAG = true;
			paramsSet.clear();
			paramsSet.reserve( 8 );
		}

		eLastChar = eThisChar;
	} while ( bNotEnd );

	pLayout->AddCommand( CMD_BREAKLINE );
	pLayout->Generate( pView, float( nWidth ) );   // retail @0x322320: int width -> FLOAT layout width
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CML::Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow )
{
	pLayout->Render( pRender, sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CML::Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow )
{
	pLayout->Render( pView, sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateML
////////////////////////////////////////////////////////////////////////////////////////////////////
IML* CreateML()
{
	return new CML;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
BASIC_REGISTER_CLASS( IML );
REGISTER_SAVELOAD_CLASS( 0xB0829160, CMLTextObject );
REGISTER_SAVELOAD_CLASS( 0xB0829161, CMLImageObject );
REGISTER_SAVELOAD_CLASS( 0xB0829162, CMLLayout );
REGISTER_SAVELOAD_CLASS( 0xB0829163, CML );
REGISTER_SAVELOAD_CLASS( 0xB100323D, CMLStream );   // retail NUI::CMLStream (gen/classreg.json)
////
REGISTER_SAVELOAD_CLASS( 0xB1003230, CLEFTHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003231, CRIGHTHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003232, CCENTERHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003233, CJUSTIFYHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003234, CWRAPLEFTHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003235, CWRAPRIGHTHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003236, CTOPHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003237, CMIDDLEHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003238, CBOTTOMHandler );
REGISTER_SAVELOAD_CLASS( 0xB1003239, CBRHandler );
REGISTER_SAVELOAD_CLASS( 0xB100323A, CCOLORHandler );
REGISTER_SAVELOAD_CLASS( 0xB100323B, CFONTHandler );
REGISTER_SAVELOAD_CLASS( 0xB100323C, CIMAGEHandler );
REGISTER_SAVELOAD_CLASS( 0x52353000, CNOWRAPHandler );      // retail ids (gen/classreg.json) -- these three
REGISTER_SAVELOAD_CLASS( 0xB3716161, CTABHandler );         // handlers + the tab object were the last UIML
REGISTER_SAVELOAD_CLASS( 0xB3714190, CMINFONTSIZEHandler ); // classes missing vs the retail registry
REGISTER_SAVELOAD_CLASS( 0xB3716160, CMLTabObject );
