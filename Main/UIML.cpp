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
// CMLTextObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLTextObject: public IMLObject
{
	OBJECT_BASIC_METHODS(CMLTextObject)
private:
	struct SFontInfo
	{
		CVec2 scale;
		CPtr<CFontFormatInfo> pInfo;
		CPtr<NGScene::CFontInfo> pFont;
		int operator&( CStructureSaver &f ) { ASSERT( 0 ); return 0; }
	};

	ZDATA
	wstring wsText;
	////
	SState sState;
	SPoint sSize;
	SPoint sPosition;
	////
	list<float> edges;
	CRectLayout sNormal;
	CRectLayout sOutline;
	CObj<CPtrFuncBase<NGfx::CTexture> > pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsText); f.Add(3,&sState); f.Add(4,&sSize); f.Add(5,&sPosition); f.Add(6,&edges); f.Add(7,&sNormal); f.Add(8,&sOutline); f.Add(9,&pTexture); return 0; }

protected:
	void GetFontFormatInfo( NGScene::I2DGameView *pView, const NGScene::SFont &sFont, SFontInfo *pFontInfo );

public:
	CMLTextObject() {}
	CMLTextObject( const wstring &wsText );

	void Generate( NGScene::I2DGameView *pView );

	const SPoint& GetSize() const;

	const SState& GetState() const;
	void SetState( const SState &sState );

	const SPoint& GetPosition() const;
	void SetPosition( const SPoint &sPosition );

	void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow );
	void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IMLObject* CreateIMLTextObject( const wstring &wsText )
{
	return new CMLTextObject( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMLTextObject::CMLTextObject( const wstring &_wsText ):
	wsText( _wsText )
{
	sSize = SPoint( 20, 20 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::Generate( NGScene::I2DGameView *pView )
{
	SFontInfo sFontInfo;
	GetFontFormatInfo( pView, sState.sFont, &sFontInfo );

	sSize.y = sFontInfo.pInfo->GetLineSpace() * sFontInfo.scale.y;
	pTexture = sFontInfo.pFont->GetTexture();

	sNormal.scale.x = sFontInfo.scale.x;
	sNormal.scale.y = sFontInfo.scale.y;
	sOutline.scale = sNormal.scale;

	int nCount = 0;
	float fX = 0;
	WCHAR wcLastChar = 0;
	for ( wstring::const_iterator iChar = wsText.begin(); iChar != wsText.end(); iChar++ )
	{
		const int &nS = sState.nOutlineBorder;
		const STFCharacter &sCharacter = sFontInfo.pInfo->GetChar( *iChar );

		fX += nS;
		fX += ( sCharacter.nA + sFontInfo.pInfo->GetKern( *iChar, wcLastChar ) ) * sFontInfo.scale.x;
		sNormal.AddRect( fX, 0, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sColor );

 		if ( sState.nOutlineBorder )
		{
			sOutline.AddRect( fX, +nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX, -nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX + nS, 0, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX - nS, 0, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX + nS, +nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX + nS, -nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX - nS, +nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
			sOutline.AddRect( fX - nS, -nS, CTRect<float>( sCharacter.x1, sCharacter.y1, sCharacter.x2, sCharacter.y2 ), sState.sOutlineColor );
		}

		fX += sCharacter.nBC * sFontInfo.scale.x;
		fX += nS;

		edges.push_back( fX );

		wcLastChar = *iChar;
	}

	sSize.x = fX;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CMLTextObject::GetSize() const
{
	return sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SState& CMLTextObject::GetState() const
{
	return sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::SetState( const SState &_sState )
{
	sState = _sState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CMLTextObject::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::SetPosition( const SPoint &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	SPoint sPos = sGlobalPosition + sPosition;

	float fLastX = 0;
	for ( list<float>::const_iterator iTemp = edges.begin(); iTemp != edges.end(); iTemp++ )
	{
		pRender->push_back( SRect( fLastX + sPos.x, sPos.y, *iTemp + sPos.x, sSize.y + sPos.y ) );
		fLastX = *iTemp;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::Render( NGScene::I2DGameView *pView, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	if ( !sOutline.rects.empty() )
		pView->CreateDynamicRects( pTexture, sOutline, sGlobalPosition + sPosition, sWindow );

	pView->CreateDynamicRects( pTexture, sNormal, sGlobalPosition + sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLTextObject::GetFontFormatInfo( NGScene::I2DGameView *pView, const NGScene::SFont &sFont, SFontInfo *pFontInfo )
{
	CVec2 vScreen = pView->GetViewportSize();
	NGScene::SFont sSearch( sFont );

	sSearch.nSize = sFont.nSize & FONT_SIZE_MASK;
	if ( sFont.nSize & FONT_SIZE_POINTS )
		sSearch.nSize = (float)( sFont.nSize & FONT_SIZE_MASK ) * vScreen.x / 1024.0f;
	else if ( sFont.nSize & FONT_SIZE_PIXELS )
		sSearch.nSize = sFont.nSize & FONT_SIZE_MASK;
	else
		ASSERT( 0 );

	CPtr<NGScene::CFontInfo> pFont = pView->GetLocaleInfo()->GetFont( sSearch );
	CDGPtr< CPtrFuncBase<CFontFormatInfo> > pInfo( pFont->GetFormatInfo() );
	pInfo.Refresh();
	pFontInfo->pFont = pFont;
	pFontInfo->pInfo = pInfo->GetValue();

	float fScale = (float)sSearch.nSize / pFontInfo->pInfo->GetLineSpace();
	pFontInfo->scale.x = fScale;
	pFontInfo->scale.y = fScale;

	return;
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
	SPoint sSize;
	SPoint sPosition;
	////
	CRectLayout sLayout;
	CDBPtr<NDb::CTexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBorder); f.Add(3,&nWidth); f.Add(4,&nHeight); f.Add(5,&eAlign); f.Add(6,&pUITexture); f.Add(7,&sState); f.Add(8,&sSize); f.Add(9,&sPosition); f.Add(10,&sLayout); f.Add(11,&pTexture); return 0; }

public:
	CMLImageObject() {}
	CMLImageObject( NDb::CUITexture *pUITexture, SState::EHORAlign eAlign, int nBorder, int nWidth, int nHeight );

	void Generate( NGScene::I2DGameView *pView );

	const SPoint& GetSize() const;

	const SState& GetState() const;
	void SetState( const SState &sState );

	const SPoint& GetPosition() const;
	void SetPosition( const SPoint &sPosition );

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
			sLayout.scale.x = vScale.x;
			sLayout.scale.y = vScale.y;
			sLayout.AddRect( 0, 0, sTexRect );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CMLImageObject::GetSize() const
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
const SPoint& CMLImageObject::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::SetPosition( const SPoint &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	pRender->push_back( SRect( sGlobalPosition.x, sGlobalPosition.y, sSize.x + sGlobalPosition.x, sSize.y + sGlobalPosition.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLImageObject::Render( NGScene::I2DGameView *pView, const SPoint &sGlobalPosition, const SRect &sWindow )
{
	pView->CreateDynamicRects( pTexture, sLayout, sGlobalPosition + sPosition, sWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLLayout
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLLayout: public IMLLayout
{
	OBJECT_BASIC_METHODS(CMLLayout)
private:
	struct SRange
	{
		ZDATA
		int nValue;
		int nHeight;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nValue); f.Add(3,&nHeight); return 0; }

		SRange() {}
		SRange( int _nValue, int _nHeight ): nValue( _nValue ), nHeight( _nHeight ) {}
	};
	struct SReflowInfo
	{
		ZDATA
		int nY;
		int nMaxX;
		int nLineWidth, nLineHeight, nLastLineHeight;
		SRange sLeft, sRight;
		list<CPtr<IMLObject> > line, leftWraped, rightWraped;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nY); f.Add(3,&nMaxX); f.Add(4,&nLineWidth); f.Add(5,&nLineHeight); f.Add(6,&nLastLineHeight); f.Add(7,&sLeft); f.Add(8,&sRight); f.Add(9,&line); f.Add(10,&leftWraped); f.Add(11,&rightWraped); return 0; }
	};
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
	SPoint sSize;
	SState sState;
	list<SCmdPair> itemsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSize); f.Add(3,&sState); f.Add(4,&itemsList); return 0; }

protected:
	void CreateLine( SReflowInfo *pInfo, int nWidth );
	void AssembleLine( SReflowInfo *pInfo );
	void ProcessWraped( SReflowInfo *pInfo );

public:
	CMLLayout();

	void AddObject( IMLObject *pObject );
	void AddCommand( ECommand eCommand, IMLObject *pObject = 0 );

	const SPoint& GetSize() const;

	const SState& GetState();
	void SetState( const SState &sState );

	void Generate( NGScene::I2DGameView *pView, int nWidth );

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
const SPoint& CMLLayout::GetSize() const
{
	return sSize;
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
void CMLLayout::Generate( NGScene::I2DGameView *pView, int nWidth )
{
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CPtr<IMLObject> pObject = iTemp->pObject;

		if ( IsValid( pObject ) )
			pObject->Generate( pView );
	}

	SReflowInfo sInfo;
	sInfo.nY = 0;
	sInfo.nMaxX = 0;
	sInfo.nLineWidth = sInfo.nLineHeight = 0;
	sInfo.nLastLineHeight = 0;
	sInfo.sLeft = SRange( 0, -1 );
	sInfo.sRight = SRange( nWidth, -1 );
	for( list<SCmdPair>::iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		ECommand eCommand = iTemp->eCmd;
		if ( eCommand == CMD_BREAKLINE )
			CreateLine( &sInfo, nWidth );

		CPtr<IMLObject> pObject = iTemp->pObject;
		if ( IsValid( pObject ) )
		{
			const SPoint &sSize = pObject->GetSize();
			const SState &sState = pObject->GetState();

			switch( sState.eHAlign )
			{
			case SState::HORALIGN_DEFAULT:
			case SState::HORALIGN_LEFT:
			case SState::HORALIGN_RIGHT:
			case SState::HORALIGN_CENTER:
			case SState::HORALIGN_JUSTIFY:
				if ( ( sInfo.nLineWidth + sSize.x ) > ( sInfo.sRight.nValue - sInfo.sLeft.nValue ) )
					CreateLine( &sInfo, nWidth );

				if ( ( eCommand == CMD_SPACE ) && sInfo.line.empty() )
					break;

				sInfo.line.push_back( pObject );
				sInfo.nLineWidth += sSize.x;
				sInfo.nLineHeight = max( sSize.y, sInfo.nLineHeight );
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

	sSize.x = sInfo.nMaxX;
	sSize.y = sInfo.nY;
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
void CMLLayout::CreateLine( SReflowInfo *pInfo, int nWidth )
{
	AssembleLine( pInfo );

	pInfo->line.clear();

	if ( pInfo->nLineHeight != 0 )
	{
		pInfo->nY += pInfo->nLineHeight;
		pInfo->nLastLineHeight = pInfo->nLineHeight;
	}
	else
		pInfo->nY += pInfo->nLastLineHeight;

	pInfo->nLineWidth = 0;
	pInfo->nLineHeight = 0;

	ProcessWraped( pInfo );
	if ( pInfo->nY > pInfo->sLeft.nHeight )
		pInfo->sLeft.nValue = 0;
	if ( pInfo->nY > pInfo->sRight.nHeight )
		pInfo->sRight.nValue = nWidth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::AssembleLine( SReflowInfo *pInfo )
{
	if ( pInfo->line.empty() )
		return;

	float fX = pInfo->sLeft.nValue;
	float fSpace = 0;
	SState::EHORAlign eHAlign = pInfo->line.front()->GetState().eHAlign;
	switch( eHAlign )
	{
	case SState::HORALIGN_RIGHT:
		{
			int nTotalWidth = pInfo->sRight.nValue - pInfo->sLeft.nValue;
			fX = pInfo->sLeft.nValue + nTotalWidth - pInfo->nLineWidth;
			fSpace = 0;
		}
		break;
	case SState::HORALIGN_CENTER:
		{
			int nTotalWidth = pInfo->sRight.nValue - pInfo->sLeft.nValue;
			fX = pInfo->sLeft.nValue + ( nTotalWidth - pInfo->nLineWidth ) / 2;
			fSpace = 0;
			break;
		}
	case SState::HORALIGN_JUSTIFY:
		{
			int nTotalWidth = pInfo->sRight.nValue - pInfo->sLeft.nValue;
			fX = pInfo->sLeft.nValue;
			fSpace = float( nTotalWidth - pInfo->nLineWidth ) / pInfo->line.size();
			break;
		}
	}

	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->line.begin(); iTemp != pInfo->line.end(); iTemp++ )
	{
		const SPoint &sSize = (*iTemp)->GetSize();
		SState::EVERTAlign eVAlign = (*iTemp)->GetState().eVAlign;

		switch( eVAlign )
		{
		case SState::VERTALIGN_TOP:
			(*iTemp)->SetPosition( SPoint( fX, pInfo->nY ) );
			break;
		case SState::VERTALIGN_BOTTOM:
			(*iTemp)->SetPosition( SPoint( fX, pInfo->nY + pInfo->nLineHeight - sSize.y ) );
			break;
		default:
			(*iTemp)->SetPosition( SPoint( fX, pInfo->nY + ( pInfo->nLineHeight - sSize.y ) / 2 ) );
			break;
		}

		fX += (*iTemp)->GetSize().x;
		pInfo->nMaxX = Max( pInfo->nMaxX, int( fX ) );
		fX += fSpace;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMLLayout::ProcessWraped( SReflowInfo *pInfo )
{
	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->leftWraped.begin(); iTemp != pInfo->leftWraped.end(); iTemp++ )
	{
		const SPoint &sSize = (*iTemp)->GetSize();

		(*iTemp)->SetPosition( SPoint( pInfo->sLeft.nValue, pInfo->nY ) );
		pInfo->nMaxX = Max( pInfo->nMaxX, pInfo->sLeft.nValue + sSize.x );

		pInfo->sLeft.nValue += sSize.x;
		pInfo->sLeft.nHeight = max( pInfo->sLeft.nHeight, pInfo->nY + sSize.y );
	}
	for( list<CPtr<IMLObject> >::iterator iTemp = pInfo->rightWraped.begin(); iTemp != pInfo->rightWraped.end(); iTemp++ )
	{
		const SPoint &sSize = (*iTemp)->GetSize();

		(*iTemp)->SetPosition( SPoint( pInfo->sRight.nValue - sSize.x, pInfo->nY ) );
		pInfo->nMaxX = Max( pInfo->nMaxX, pInfo->sRight.nValue );

		pInfo->sRight.nValue -= sSize.x;
		pInfo->sRight.nHeight = max( pInfo->sRight.nHeight, pInfo->nY + sSize.y );
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
	wstring wsText;
	CObj<CMLLayout> pLayout;
	unordered_map<wstring,CObj<IMLHandler> > tagsMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsText); f.Add(3,&pLayout); f.Add(4,&tagsMap); return 0; }

public:
	CML();

	void SetText( const wstring &wsText, int nFlags );
	void SetHandler( const wstring &wsTAG, IMLHandler *pHandler );

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
	tagsMap[L"justify"] = new CJUSTIFYHandler;
	tagsMap[L"wrapleft"] = new CWRAPLEFTHandler;
	tagsMap[L"wrapright"] = new CWRAPRIGHTHandler;
	tagsMap[L"top"] = new CTOPHandler;
	tagsMap[L"bottom"] = new CBOTTOMHandler;
	tagsMap[L"middle"] = new CMIDDLEHandler;

	tagsMap[L"font"] = new CFONTHandler;
	tagsMap[L"color"] = new CCOLORHandler;
	tagsMap[L"image"] = new CIMAGEHandler;
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
const SPoint& CML::GetSize() const
{
	return pLayout->GetSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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

	int nTemp = 0, nWordBegin = 0;
	bool bTAG = false, bBracketsBlock = false;
	ECharType eThisChar = CHAR_NULL, eLastChar = CHAR_NULL;
	vector<wstring> paramsSet;
	while( nTemp <= wsText.length() )
	{
		WCHAR wcChar = wsText.c_str()[nTemp];

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
			if ( !bTAG && ( ( eLastChar == CHAR_ALNUM ) || ( eLastChar == CHAR_PUNCTUATION ) ) )
				pLayout->AddObject( new CMLTextObject( wsText.substr( nWordBegin, nTemp - nWordBegin ) ) );
			else if ( !bTAG && ( eLastChar == CHAR_SPACE ) )
				pLayout->AddCommand( CMD_SPACE, new CMLTextObject( wsText.substr( nWordBegin, nTemp - nWordBegin ) ) );
			else if ( !bTAG && ( eLastChar == CHAR_EOL ) )
				pLayout->AddCommand( CMD_SPACE, new CMLTextObject( L" " ) );
			else if ( bTAG && ( ( eLastChar == CHAR_ALNUM ) || ( eLastChar == CHAR_PUNCTUATION ) ) )
				paramsSet.push_back( wsText.substr( nWordBegin, nTemp - nWordBegin ) );


			if ( eLastChar == CHAR_TAG_END )
			{
				bTAG = false;

				if ( !paramsSet.empty() )
				{
					unordered_map<wstring,CObj<IMLHandler> >::const_iterator iTemp = tagsMap.find( paramsSet.front() );
					if ( ( iTemp != tagsMap.end() ) && ( iTemp->second != 0 ) )
						iTemp->second->Exec( pLayout, paramsSet );
				}
			}
			else if ( eLastChar == CHAR_TAG_BEGIN )
			{
				bTAG = true;
				paramsSet.clear();
				paramsSet.reserve( 8 );
			}

			nWordBegin = nTemp;
		}

		nTemp++;
		eLastChar = eThisChar;
	};

	pLayout->AddCommand( CMD_BREAKLINE );
	pLayout->Generate( pView, nWidth );
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
