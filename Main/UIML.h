#ifndef __A5_UI_ML_H__
#define __A5_UI_ML_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GLocale.h"
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IML;
class IMLObject;
class IMLLayout;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommand
{
	CMD_NULL,
	CMD_TAB,
	CMD_SPACE,
	CMD_BREAKLINE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	FONT_SIZE_MASK			= 0x00FFFFFF,
	FONT_SIZE_PIXELS		= 0x10000000,
	FONT_SIZE_POINTS		= 0x20000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SState
{
	enum EHORAlign
	{
		HORALIGN_DEFAULT,
		HORALIGN_LEFT,
		HORALIGN_RIGHT,
		HORALIGN_CENTER,
		HORALIGN_JUSTIFY,
		HORALIGN_WRAP_LEFT,
		HORALIGN_WRAP_RIGHT
	};
	enum EVERTAlign
	{
		VERTALIGN_TOP,
		VERTALIGN_BOTTOM,
		VERTALIGN_MIDDLE
	};

	ZDATA
	//// reflow
	EHORAlign eHAlign;
	EVERTAlign eVAlign;
	//// font
	NGScene::SFont sFont;
	NGfx::SPixel8888 sColor;
	//// outline font
	int nOutlineBorder;
	NGfx::SPixel8888 sOutlineColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eHAlign); f.Add(3,&eVAlign); f.Add(4,&sFont); f.Add(5,&sColor); f.Add(6,&nOutlineBorder); f.Add(7,&sOutlineColor); f.Add(8,&bForceFontSize); f.Add(9,&nMinFontSize); return 0; }
	bool bForceFontSize = false;
	int nMinFontSize = 0;

	SState(): eHAlign( HORALIGN_DEFAULT ), eVAlign( VERTALIGN_MIDDLE ), sFont( 16, "System" ), sColor( 0xFF, 0xFF, 0xFF, 0xFF ), nOutlineBorder( 0 ), sOutlineColor( 0xFF, 0xFF, 0xFF, 0xFF ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLObject: public CObjectBase
{
public:
	virtual void Generate( NGScene::I2DGameView *pView ) = 0;

	virtual const SPoint& GetSize() const = 0;

	virtual const SState& GetState() const = 0;
	virtual void SetState( const SState &sState ) = 0;

	virtual const SPoint& GetPosition() const = 0;
	virtual void SetPosition( const SPoint &sPosition ) = 0;

	virtual void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IMLObject* CreateIMLTextObject( const wstring &wsText );
IMLObject* CreateIMLImageObject( NDb::CUITexture *pUITexture, SState::EHORAlign eAlign, int nBorder, int nWidth, int nHeight );
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLLayout
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLLayout: public CObjectBase
{
public:
	virtual void AddObject( IMLObject *pObject ) = 0;
	virtual void AddCommand( ECommand eCommand, IMLObject *pObject = 0 ) = 0;

	virtual const SPoint& GetSize() const = 0;

	virtual const SState& GetState() = 0;
	virtual void SetState( const SState &sState ) = 0;

	virtual void Generate( NGScene::I2DGameView *pView, int nWidth ) = 0;

	virtual void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLHandler
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLHandler: public CObjectBase
{
public:
	virtual void Exec( IMLLayout *pLayout, const vector<wstring> &paramsSet ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IML
////////////////////////////////////////////////////////////////////////////////////////////////////
class IML: public CObjectBase
{
public:
	virtual void SetText( const wstring &wsText, int nFlags ) = 0;
	virtual void SetHandler( const wstring &wsTAG, IMLHandler *pHandler ) = 0;

	virtual const SPoint& GetSize() const = 0;

	virtual void Generate( NGScene::I2DGameView *pView, int nWidth ) = 0;

	virtual void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IML* CreateML();
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif